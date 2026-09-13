// ============================================================
// hook — Android JNI (v27: PostLogin + manual RestartPlayer)
// ============================================================
#include <jni.h>
#include <cstdint>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cerrno>
#include <cstdarg>
#include <cwchar>
#include <string>
#include <dlfcn.h>
#include <link.h>
#include <thread>
#include <chrono>
#include <mutex>
#include <vector>
#include <android/log.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/uio.h>
#include "dobby.h"

#define LOG_TAG "hook"

#define ADDR_StaticFindObject           0x762A664
#define ADDR_GetGameState               0x55B8AB0
#define ADDR_PlaylistHandler            0x5519710
#define ADDR_StartAircraftPhase         0x550A1F4
#define ADDR_PostLogin                  0x550C610

#define OFFSET_GameMode_GameState       0x2D0
#define OFFSET_GameState_Playlist2298   0x2298
#define OFFSET_GameState_Playlist2290   0x2290
#define OFFSET_478_2D8                  0x2D8
#define OFFSET_2D8_298                  0x298
#define OFFSET_PC_PlayerState           0x278
#define OFFSET_PC_Pawn                  0x2B8
#define OFFSET_PS_2A8                   0x2A8
#define OFFSET_PS_3F2                   0x3F2
#define OFFSET_PS_120C                  0x120C

#define VFT_NAME_ISVALID_OFF            0x30
#define VFT_NAME_GETNAME_OFF            0x38

#define VFT_SLOT_HandleMatchHasStarted  0x11C

#define SCAN_START  0x100
#define SCAN_END    0x2000
#define SCAN_STEP   8

// ------------------------------------------------------------
// Logger
// ------------------------------------------------------------
static FILE* g_logfile = nullptr;
static std::mutex& GetMutex() {
    static std::mutex* m = new std::mutex();
    return *m;
}
static void InitLogFile() {
    const char* paths[] = {
        "/storage/emulated/0/Android/data/com.epicgames.fortnite/files/hook.log",
        "/sdcard/Android/data/com.epicgames.fortnite/files/hook.log",
        "/data/local/tmp/hook.log",
    };
    for (auto p : paths) {
        FILE* f = fopen(p, "w");
        if (f) { g_logfile = f; return; }
    }
}
static void LOGF(const char* fmt, ...) {
    char buf[4096];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    __android_log_print(ANDROID_LOG_INFO, LOG_TAG, "%s", buf);
    if (g_logfile) {
        std::lock_guard<std::mutex> lock(GetMutex());
        fprintf(g_logfile, "%s\n", buf);
        fflush(g_logfile);
    }
}

// ------------------------------------------------------------
// UTF-16 -> UTF-8
// ------------------------------------------------------------
static std::string u16_to_u8(const char16_t* str) {
    if (!str) return "nullptr";
    std::string res;
    while (*str) {
        uint16_t c = *str++;
        if (c < 0x80) res += (char)c;
        else if (c < 0x800) {
            res += (char)(0xC0 | (c >> 6));
            res += (char)(0x80 | (c & 0x3F));
        } else {
            res += (char)(0xE0 | (c >> 12));
            res += (char)(0x80 | ((c >> 6) & 0x3F));
            res += (char)(0x80 | (c & 0x3F));
        }
    }
    return res;
}
static const char16_t* GetShortName(const char16_t* full_path) {
    if (!full_path) return nullptr;
    const char16_t* lastDot = nullptr;
    const char16_t* p = full_path;
    while (*p) { if (*p == u'.') lastDot = p; p++; }
    return lastDot ? lastDot + 1 : full_path;
}

// ------------------------------------------------------------
// Safe memory
// ------------------------------------------------------------
static bool SafeReadPtr(void* addr, void** out) {
    *out = nullptr;
    if (!addr) return false;
    struct iovec local  = { out, sizeof(void*) };
    struct iovec remote = { addr, sizeof(void*) };
    return process_vm_readv(getpid(), &local, 1, &remote, 1, 0) == (ssize_t)sizeof(void*);
}
static bool SafeReadBytes(void* addr, void* out, size_t n) {
    if (!addr || !out || n == 0 || n > 64) return false;
    struct iovec local  = { out, n };
    struct iovec remote = { addr, n };
    return process_vm_readv(getpid(), &local, 1, &remote, 1, 0) == (ssize_t)n;
}
static bool SafeReadI16(void* addr, int16_t* out) {
    return SafeReadBytes(addr, out, sizeof(int16_t));
}
static bool SafeReadU8(void* addr, uint8_t* out) {
    return SafeReadBytes(addr, out, sizeof(uint8_t));
}

// ------------------------------------------------------------
// LibInfo
// ------------------------------------------------------------
struct LibInfo { uintptr_t base, text_start, text_end; };
static LibInfo GetLibInfo() {
    LibInfo info{0, 0, 0};
    dl_iterate_phdr([](struct dl_phdr_info* ph, size_t, void* data) -> int {
        auto* out = (LibInfo*)data;
        if (!ph->dlpi_name || !*ph->dlpi_name) return 0;
        if (!strstr(ph->dlpi_name, "libUnreal")) return 0;
        out->base = ph->dlpi_addr;
        for (int i = 0; i < ph->dlpi_phnum; i++) {
            const auto& p = ph->dlpi_phdr[i];
            if (p.p_type != PT_LOAD || !(p.p_flags & PF_X)) continue;
            out->text_start = ph->dlpi_addr + p.p_vaddr;
            out->text_end   = out->text_start + p.p_memsz;
            break;
        }
        return 1;
    }, &info);
    return info;
}

// ------------------------------------------------------------
// VFT patch
// ------------------------------------------------------------
static bool VFTPatch(void** vtable, size_t slot, void* hook_fn, void** orig_out) {
    if (!vtable) return false;
    void** ptr_slot = &vtable[slot];
    *orig_out = *ptr_slot;

    const size_t ps = (size_t)sysconf(_SC_PAGESIZE);
    uintptr_t addr = (uintptr_t)ptr_slot;
    uintptr_t pg = addr & ~(ps - 1);
    size_t span = ps;
    if ((addr + sizeof(void*)) > (pg + ps)) span += ps;

    if (mprotect((void*)pg, span, PROT_READ | PROT_WRITE) != 0) return false;
    *ptr_slot = hook_fn;
    mprotect((void*)pg, span, PROT_READ | PROT_EXEC);
    __builtin___clear_cache((char*)pg, (char*)pg + span);
    return true;
}

// ------------------------------------------------------------
// Globals
// ------------------------------------------------------------
static uintptr_t g_base = 0;
static uintptr_t g_text_start = 0;
static uintptr_t g_text_end = 0;
static std::mutex g_vft_mutex;
static std::vector<void*> g_known_vfts;
static void* g_cached_playlist = nullptr;
static void* g_cached_game_session = nullptr;

static void (*g_orig_HMHS)(void*) = nullptr;
static void Hook_HandleMatchHasStarted(void*);

typedef void* (*StaticFindObject_t)(void*, void*, const char16_t*, bool);
static StaticFindObject_t g_StaticFindObject = nullptr;

// ------------------------------------------------------------
// Pointer validation
// ------------------------------------------------------------
static bool IsValidPtr(void* ptr) {
    uintptr_t p = (uintptr_t)ptr;
    return (p >= 0x1000000000ULL && p <= 0x8000000000ULL);
}
static bool IsValidHeapPtr(void* ptr) {
    uintptr_t p = (uintptr_t)ptr;
    if (p < 0x1000000000ULL || p > 0x8000000000ULL) return false;
    if (g_base && p >= g_base && p < (g_base + 0x10000000ULL)) return false;
    return true;
}
static bool IsTextPtr(void* ptr) {
    uintptr_t p = (uintptr_t)ptr;
    return (g_text_start && p >= g_text_start && p < g_text_end);
}
static bool HasValidVtable(void* obj) {
    if (!IsValidHeapPtr(obj)) return false;
    void* vft = nullptr;
    if (!SafeReadPtr(obj, &vft)) return false;
    if (!IsValidPtr(vft)) return false;
    uintptr_t v = (uintptr_t)vft;
    if (g_base && v >= g_base && v < (g_base + 0x10000000ULL)) return true;
    return IsValidHeapPtr(vft);
}

// ------------------------------------------------------------
// Memory dump
// ------------------------------------------------------------
static void DumpMem(const char* tag, void* base, int start, int end) {
    if (!IsValidPtr(base)) return;
    LOGF("  --- %s dump [%p + 0x%X .. 0x%X] ---", tag, base, start, end);
    for (int off = start; off < end; off += 16) {
        uint8_t buf[16];
        if (!SafeReadBytes((char*)base + off, buf, 16)) {
            LOGF("    +0x%X: <unreadable>", off);
            continue;
        }
        LOGF("    +0x%X: %02X %02X %02X %02X  %02X %02X %02X %02X  "
             "%02X %02X %02X %02X  %02X %02X %02X %02X",
             off,
             buf[0], buf[1], buf[2], buf[3], buf[4], buf[5], buf[6], buf[7],
             buf[8], buf[9], buf[10], buf[11], buf[12], buf[13], buf[14], buf[15]);
    }
}

// ------------------------------------------------------------
// VTable dump
// ------------------------------------------------------------
static void DumpGameModeVtable(void* gameMode, const char* tag) {
    if (!IsValidHeapPtr(gameMode)) return;
    void* vft = nullptr;
    if (!SafeReadPtr(gameMode, &vft)) return;
    if (!IsValidPtr(vft)) return;

    LOGF("[vtdump:%s] GameMode=%p vtable=%p", tag, gameMode, vft);

    static const int slots[] = {
        0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F, 0x20, 0x21,
        0xD0, 0xD1, 0xD2, 0xD3, 0xD4, 0xD5, 0xD6, 0xD7, 0xD8, 0xD9,
        0xDA, 0xDB, 0xDC, 0xDD, 0xDE, 0xDF, 0xE0, 0xE1,
        0x11A, 0x11B, 0x11C, 0x11D, 0x11E, 0x11F, 0x120,
    };
    for (int s : slots) {
        void* fn = nullptr;
        if (!SafeReadPtr((char*)vft + (size_t)s * 8, &fn)) continue;
        uintptr_t off = (uintptr_t)fn - g_base;
        LOGF("  [slot 0x%X] = %p  (offset +0x%lx)  %s",
             s, fn, (unsigned long)off, IsTextPtr(fn) ? "TEXT" : "");
    }
}

// ------------------------------------------------------------
// Scan
// ------------------------------------------------------------
static void* ScanGameModeForGameSession(void* gameMode) {
    if (!gameMode) return nullptr;
    LOGF("[Scan] Wide-scan GameMode=%p range=0x%X..0x%X",
         gameMode, SCAN_START, SCAN_END);

    void* best = nullptr;
    int candidates = 0;
    for (int offset = SCAN_START; offset < SCAN_END; offset += SCAN_STEP) {
        void* ptr = nullptr;
        if (!SafeReadPtr((char*)gameMode + offset, &ptr)) continue;
        if (!IsValidHeapPtr(ptr)) continue;
        if (!HasValidVtable(ptr)) continue;

        void* ptr2 = nullptr;
        if (!SafeReadPtr((char*)ptr + OFFSET_478_2D8, &ptr2)) continue;
        if (!IsValidHeapPtr(ptr2)) continue;

        void* ptr3 = nullptr;
        SafeReadPtr((char*)ptr2 + OFFSET_2D8_298, &ptr3);
        candidates++;

        if (ptr3 == nullptr) {
            LOGF("[Scan] Found @ +0x%X (ptr3 NULL)", offset);
            return ptr;
        }
        if (!best) best = ptr;
    }
    LOGF("[Scan] candidates=%d best=%p", candidates, best);
    return best;
}

// ------------------------------------------------------------
// Playlist
// ------------------------------------------------------------
static void* FindPlaylistByName(const char16_t* fullPath) {
    if (!g_StaticFindObject) return nullptr;
    LOGF("========== Playlist Lookup ==========");
    LOGF("Full Path: %s", u16_to_u8(fullPath).c_str());

    void* r1 = g_StaticFindObject(nullptr, nullptr, fullPath, false);
    LOGF("  [nullptr + FullPath] -> %p %s", r1, r1 ? "OK" : "");
    if (r1) return r1;

    const char16_t* shortName = GetShortName(fullPath);
    void* r2 = g_StaticFindObject(nullptr, (void*)-1, shortName, false);
    LOGF("  [(void*)-1 + ShortName] -> %p %s", r2, r2 ? "OK" : "");
    if (r2) return r2;
    return nullptr;
}

static bool InjectPlaylist(void* gameMode) {
    if (!gameMode) return false;
    if (!g_cached_playlist) {
        g_cached_playlist = FindPlaylistByName(
            u"/Game/Athena/Playlists/Playlist_DefaultSolo.Playlist_DefaultSolo"
        );
        if (!g_cached_playlist) { LOGF("[Inject] Playlist not found"); return false; }
    }

    bool injected = false;
    void* gameState = nullptr;
    if (SafeReadPtr((char*)gameMode + OFFSET_GameMode_GameState, &gameState) && gameState) {
        void* p2298 = nullptr;
        SafeReadPtr((char*)gameState + OFFSET_GameState_Playlist2298, &p2298);
        if (!p2298) {
            *(void**)((char*)gameState + OFFSET_GameState_Playlist2298) = g_cached_playlist;
            LOGF("[Inject] [Path1a] SET gameState+0x2298");
            injected = true;
        }
        void* p2290 = nullptr;
        SafeReadPtr((char*)gameState + OFFSET_GameState_Playlist2290, &p2290);
        if (!p2290) {
            *(void**)((char*)gameState + OFFSET_GameState_Playlist2290) = g_cached_playlist;
            LOGF("[Inject] [Path1b] SET gameState+0x2290");
            injected = true;
        }
    }

    if (!g_cached_game_session || !IsValidHeapPtr(g_cached_game_session)) {
        g_cached_game_session = ScanGameModeForGameSession(gameMode);
    }
    if (g_cached_game_session) {
        void* ptr2D8 = nullptr;
        if (SafeReadPtr((char*)g_cached_game_session + OFFSET_478_2D8, &ptr2D8) && IsValidHeapPtr(ptr2D8)) {
            void* ptr298 = nullptr;
            SafeReadPtr((char*)ptr2D8 + OFFSET_2D8_298, &ptr298);
            if (ptr298 != g_cached_playlist) {
                *(void**)((char*)ptr2D8 + OFFSET_2D8_298) = g_cached_playlist;
                LOGF("[Inject] [Path2] SET ptr2D8+0x298");
                injected = true;
            }
        }
    }
    return injected;
}

// ------------------------------------------------------------
// VFT install
// ------------------------------------------------------------
static bool InstallVFTOnReal(void* gameMode) {
    if (!gameMode) return false;
    void* real_vft = nullptr;
    if (!SafeReadPtr(gameMode, &real_vft)) return false;
    if (!IsValidPtr(real_vft)) return false;

    std::lock_guard<std::mutex> lock(g_vft_mutex);
    for (auto* v : g_known_vfts) if (v == real_vft) return false;

    LOGF("[VFT] New VFT: %p (GameMode=%p)", real_vft, gameMode);
    void** vft = (void**)real_vft;
    bool ok1 = VFTPatch(vft, VFT_SLOT_HandleMatchHasStarted,
                        (void*)Hook_HandleMatchHasStarted, (void**)&g_orig_HMHS);
    LOGF("[VFT] Hook slot 0x%X: %s", VFT_SLOT_HandleMatchHasStarted, ok1 ? "OK" : "FAIL");
    g_known_vfts.push_back(real_vft);
    return true;
}

// ------------------------------------------------------------
// Hooks
// ------------------------------------------------------------
static void Hook_HandleMatchHasStarted(void* this_) {
    LOGF("[slot 0x11C] HandleMatchHasStarted this=%p", this_);
    if (g_orig_HMHS) g_orig_HMHS(this_);
}

typedef int (*PlaylistHandler_t)(void*);
static PlaylistHandler_t g_orig_Playlist = nullptr;

static int Hook_PlaylistHandler(void* a1) {
    LOGF("[Playlist] a1=%p", a1);
    if (a1) InjectPlaylist(a1);
    int result = g_orig_Playlist ? g_orig_Playlist(a1) : 0;
    LOGF("  -> returned: %d", result);
    return result;
}

typedef bool (*StartAircraftPhase_t)(void*, char);
static StartAircraftPhase_t g_orig_SAP = nullptr;

static bool Hook_StartAircraftPhase(void* this_, char a2) {
    LOGF("========================================");
    LOGF("[Aircraft] StartAircraftPhase this=%p a2=%d", this_, (int)a2);
    DumpGameModeVtable(this_, "aircraft");
    InstallVFTOnReal(this_);
    InjectPlaylist(this_);
    bool result = g_orig_SAP ? g_orig_SAP(this_, a2) : false;
    LOGF("[Aircraft] returned: %d %s", (int)result, result ? "SUCCESS" : "FAIL");
    LOGF("========================================");
    return result;
}

typedef void* (*GetGameState_t)(void*);
static GetGameState_t g_orig_GGS = nullptr;
static uint64_t g_ggs_calls = 0;

static void* Hook_GetGameState(void* gm) {
    g_ggs_calls++;
    void* r = g_orig_GGS ? g_orig_GGS(gm) : nullptr;
    if (gm && g_ggs_calls > 3) InstallVFTOnReal(gm);
    return r;
}

// ============================================================
// RestartPlayer — find UFunction and call it manually
// ============================================================
typedef void (*ProcessEvent_t)(void*, void*, void*);

static void* g_restartPlayerFn = nullptr;   // UFunction* for "RestartPlayer"
static void* g_gameModeBaseClass = nullptr;
static void* g_gameModeBaseCDO = nullptr;

// Buffer large enough for a UFunction call frame (params).
// RestartPlayer(APlayerController* NewPlayer) → 1 pointer param.
struct RestartPlayer_Params {
    void* NewPlayer;
};

static void* FindObject(const char16_t* name, void* outer = nullptr) {
    if (!g_StaticFindObject) return nullptr;
    return g_StaticFindObject(nullptr, outer, name, false);
}

// Find UClass* for a given class name
static void* FindClass(const char16_t* className) {
    if (!g_StaticFindObject) return nullptr;
    // Class objects live under /Script/<Module>.<ClassName>
    // Try various outers first
    void* c = g_StaticFindObject(nullptr, nullptr, className, false);
    if (c) return c;
    // Try with -1 outer
    c = g_StaticFindObject(nullptr, (void*)-1, className, false);
    return c;
}

// Discover RestartPlayer UFunction via GameModeBase CDO
static void LocateRestartPlayer() {
    LOGF("");
    LOGF("========== Locate RestartPlayer ==========");

    // Find GameModeBase class
    g_gameModeBaseClass = FindClass(u"GameModeBase");
    LOGF("  GameModeBase class = %p", g_gameModeBaseClass);

    if (!IsValidPtr(g_gameModeBaseClass)) {
        LOGF("  cannot locate GameModeBase; skipping");
        return;
    }

    // Try to find the CDO (ClassDefaultObject) via StaticFindObject
    g_gameModeBaseCDO = FindObject(u"Default__GameModeBase");
    LOGF("  Default__GameModeBase = %p", g_gameModeBaseCDO);

    // Try to find the UFunction directly
    g_restartPlayerFn = FindObject(u"RestartPlayer");
    LOGF("  RestartPlayer (global) = %p", g_restartPlayerFn);

    // Also try common alternatives
    void* rpAtStart = FindObject(u"RestartPlayerAtPlayerStart");
    LOGF("  RestartPlayerAtPlayerStart = %p", rpAtStart);

    void* chooseStart = FindObject(u"ChoosePlayerStart");
    LOGF("  ChoosePlayerStart = %p", chooseStart);

    void* findStart = FindObject(u"FindPlayerStart");
    LOGF("  FindPlayerStart = %p", findStart);

    LOGF("==========================================");
}

// Call a UFunction via ProcessEvent
static void InvokeUFunction(void* obj, void* ufunc, void* params, int paramSize) {
    if (!IsValidPtr(obj) || !IsValidPtr(ufunc)) return;

    // ProcessEvent is a virtual method. Its index differs by UE version.
    // Common index on Fortnite (UE 5.x): vtable[0x44] (slot 68).
    void** vft = nullptr;
    if (!SafeReadPtr(obj, (void**)&vft)) return;
    if (!IsValidPtr(vft)) return;

    // Try several likely slots for ProcessEvent
    // UObject::ProcessEvent UFunction* Function, void* Parms
    for (int slot : {0x40, 0x44, 0x48, 0x4C}) {
        void* fn = nullptr;
        if (!SafeReadPtr((char*)vft + (size_t)slot * 8, &fn)) continue;
        if (!IsTextPtr(fn)) continue;

        LOGF("  [ProcessEvent?] trying slot 0x%X fn=%p", slot, fn);
        // We don't blindly call; log only
    }

    (void)params;
    (void)paramSize;
}

// ============================================================
// PostLogin — dump + manual RestartPlayer attempt
// ============================================================
struct FString16 { char16_t* Data; int32_t Num; int32_t Max; ~FString16() {} };
typedef bool       (*IsValidFn_t)(void*);
typedef FString16  (*GetNameFn_t)(void*);

static std::string TryReadPlayerName(void* playerState) {
    if (!IsValidPtr(playerState)) return "<no-PS>";
    void* subObj = nullptr;
    if (!SafeReadPtr((char*)playerState + OFFSET_PS_2A8, &subObj)) return "<2A8-unread>";
    if (!IsValidPtr(subObj)) return "<no-subObj>";

    void** vft = nullptr;
    if (!SafeReadPtr(subObj, (void**)&vft)) return "<no-vft>";
    if (!IsValidPtr(vft)) return "<bad-vft>";

    void* isValidPtr = nullptr;
    void* getNamePtr = nullptr;
    SafeReadPtr((char*)vft + VFT_NAME_ISVALID_OFF, &isValidPtr);
    SafeReadPtr((char*)vft + VFT_NAME_GETNAME_OFF, &getNamePtr);
    if (!IsValidPtr(isValidPtr) || !IsValidPtr(getNamePtr)) return "<fn-invalid>";

    IsValidFn_t isValid = (IsValidFn_t)isValidPtr;
    if (!isValid(subObj)) return "<invalid>";

    GetNameFn_t getName = (GetNameFn_t)getNamePtr;
    FString16 out = getName(subObj);

    std::string res;
    if (out.Data && out.Num > 0) {
        int32_t n = out.Num > 256 ? 256 : out.Num;
        for (int32_t i = 0; i < n; i++) {
            uint16_t c = (uint16_t)out.Data[i];
            if (c == 0) break;
            if (c < 0x80) res += (char)c;
            else if (c < 0x800) {
                res += (char)(0xC0 | (c >> 6));
                res += (char)(0x80 | (c & 0x3F));
            } else {
                res += (char)(0xE0 | (c >> 12));
                res += (char)(0x80 | ((c >> 6) & 0x3F));
                res += (char)(0x80 | (c & 0x3F));
            }
        }
    }
    return res.empty() ? "<empty>" : res;
}

typedef void (*PostLogin_t)(void*, void*);
static PostLogin_t g_orig_PostLogin = nullptr;
static uint64_t g_postlogin_calls = 0;

static void Hook_PostLogin(void* this_, void* newPlayer) {
    g_postlogin_calls++;
    LOGF("");
    LOGF("========== [PostLogin #%llu] ==========",
         (unsigned long long)g_postlogin_calls);
    LOGF("  this_(GM)=%p  newPlayer(PC)=%p", this_, newPlayer);

    // Discover RestartPlayer once
    if (!g_restartPlayerFn) LocateRestartPlayer();

    if (IsValidPtr(newPlayer)) {
        // Pawn state BEFORE original PostLogin
        void* pawnBefore = nullptr;
        if (SafeReadPtr((char*)newPlayer + OFFSET_PC_Pawn, &pawnBefore))
            LOGF("  PC->Pawn BEFORE = %p", pawnBefore);

        // PlayerState info
        void* playerState = nullptr;
        if (SafeReadPtr((char*)newPlayer + OFFSET_PC_PlayerState, &playerState))
            LOGF("  PlayerState = %p", playerState);

        if (IsValidPtr(playerState)) {
            int16_t f3F2 = 0;
            uint8_t f120C = 0;
            SafeReadI16((char*)playerState + OFFSET_PS_3F2, &f3F2);
            SafeReadU8 ((char*)playerState + OFFSET_PS_120C, &f120C);
            LOGF("  PS+0x3F2=%d  PS+0x120C=%u", (int)f3F2, (unsigned)f120C);
            std::string name = TryReadPlayerName(playerState);
            LOGF("  PLAYER NAME = \"%s\"", name.c_str());

            // Spectator flags region
            DumpMem("PS[0x2B0..0x320]", playerState, 0x2B0, 0x320);
        }

        // PC region with Pawn pointer
        DumpMem("PC[0x270..0x310]", newPlayer, 0x270, 0x310);
    }

    // Call original PostLogin
    if (g_orig_PostLogin) g_orig_PostLogin(this_, newPlayer);

    // Pawn state AFTER original PostLogin
    if (IsValidPtr(newPlayer)) {
        void* pawnAfter = nullptr;
        if (SafeReadPtr((char*)newPlayer + OFFSET_PC_Pawn, &pawnAfter))
            LOGF("  PC->Pawn AFTER = %p", pawnAfter);

        // Try manual RestartPlayer call via ProcessEvent if we found it
        if (g_restartPlayerFn && IsValidPtr(this_) && g_postlogin_calls <= 2) {
            LOGF("  attempting manual RestartPlayer via ProcessEvent...");
            RestartPlayer_Params p{ newPlayer };
            InvokeUFunction(this_, g_restartPlayerFn, &p, sizeof(p));

            std::this_thread::sleep_for(std::chrono::milliseconds(100));

            void* pawnRetry = nullptr;
            if (SafeReadPtr((char*)newPlayer + OFFSET_PC_Pawn, &pawnRetry))
                LOGF("  PC->Pawn AFTER manual call = %p", pawnRetry);
        }
    }

    LOGF("========== [PostLogin END] ==========");
}

// ------------------------------------------------------------
// Main
// ------------------------------------------------------------
static void RunHooks() {
    LibInfo info{0, 0, 0};
    for (int i = 0; i < 60; i++) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        info = GetLibInfo();
        if (info.base) break;
    }
    if (!info.base) { LOGF("[ERROR] libUnreal not found"); return; }

    g_base = info.base;
    g_text_start = info.text_start;
    g_text_end = info.text_end;
    LOGF("========================================");
    LOGF(" hook — v27");
    LOGF(" base=0x%lx text=0x%lx..0x%lx",
         g_base, g_text_start, g_text_end);
    LOGF("========================================");

    DobbyHook((void*)(g_base + ADDR_GetGameState),
              (void*)Hook_GetGameState,       (void**)&g_orig_GGS);
    DobbyHook((void*)(g_base + ADDR_PlaylistHandler),
              (void*)Hook_PlaylistHandler,    (void**)&g_orig_Playlist);
    DobbyHook((void*)(g_base + ADDR_StartAircraftPhase),
              (void*)Hook_StartAircraftPhase, (void**)&g_orig_SAP);
    DobbyHook((void*)(g_base + ADDR_PostLogin),
              (void*)Hook_PostLogin,          (void**)&g_orig_PostLogin);

    g_StaticFindObject = (StaticFindObject_t)(g_base + ADDR_StaticFindObject);
    LOGF("StaticFindObject: %p", (void*)g_StaticFindObject);
    LOGF(" All Hooks Installed.");
}

extern "C" JNIEXPORT jint JNICALL
JNI_OnLoad(JavaVM* vm, void* reserved) {
    InitLogFile();
    LOGF("========================================");
    LOGF(" hook — %s %s", __DATE__, __TIME__);
    LOGF(" PID: %d", getpid());
    LOGF("========================================");

    std::thread([]{
        try { RunHooks(); }
        catch (...) { LOGF("[ERROR] Exception in RunHooks"); }
    }).detach();

    return JNI_VERSION_1_6;
}
