// ============================================================
// hook — Android JNI (v24)
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
#define ADDR_SpawnDefaultPawnFor_Direct 0x551A7C4

#define OFFSET_GameMode_GameState       0x2D0
#define OFFSET_GameState_Playlist2298   0x2298
#define OFFSET_GameState_Playlist2290   0x2290
#define OFFSET_478_2D8                  0x2D8
#define OFFSET_2D8_298                  0x298
#define OFFSET_PC_PlayerState           0x278
#define OFFSET_PS_2A8                   0x2A8
#define OFFSET_PS_3F2                   0x3F2
#define OFFSET_PS_120C                  0x120C
#define OFFSET_Actor_Location           0x128

#define VFT_NAME_ISVALID_OFF            0x30
#define VFT_NAME_GETNAME_OFF            0x38

#define VFT_INDEX_HandleMatchHasStarted 0x11C

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
static bool SafeReadI16(void* addr, int16_t* out) {
    *out = 0;
    if (!addr) return false;
    struct iovec local  = { out, sizeof(int16_t) };
    struct iovec remote = { addr, sizeof(int16_t) };
    return process_vm_readv(getpid(), &local, 1, &remote, 1, 0) == (ssize_t)sizeof(int16_t);
}
static bool SafeReadU8(void* addr, uint8_t* out) {
    *out = 0;
    if (!addr) return false;
    struct iovec local  = { out, sizeof(uint8_t) };
    struct iovec remote = { addr, sizeof(uint8_t) };
    return process_vm_readv(getpid(), &local, 1, &remote, 1, 0) == (ssize_t)sizeof(uint8_t);
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
static bool VFTPatch(void** vtable, size_t idx, void* hook_fn, void** orig_out) {
    if (!vtable) return false;
    void** slot = &vtable[idx];
    *orig_out = *slot;

    const size_t ps = (size_t)sysconf(_SC_PAGESIZE);
    uintptr_t addr = (uintptr_t)slot;
    uintptr_t pg = addr & ~(ps - 1);
    size_t span = ps;
    if ((addr + sizeof(void*)) > (pg + ps)) span += ps;

    if (mprotect((void*)pg, span, PROT_READ | PROT_WRITE) != 0) return false;
    *slot = hook_fn;
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
static void* (*g_orig_SDPF)(void*, void*, void*) = nullptr;

static void Hook_HandleMatchHasStarted(void*);
static void* Hook_SpawnDefaultPawnFor(void*, void*, void*);

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
// GameMode vtable dump
// ------------------------------------------------------------
static void DumpGameModeVtable(void* gameMode, const char* tag) {
    if (!IsValidHeapPtr(gameMode)) return;
    void* vft = nullptr;
    if (!SafeReadPtr(gameMode, &vft)) return;
    if (!IsValidPtr(vft)) return;

    LOGF("[vtdump:%s] GameMode=%p vtable=%p", tag, gameMode, vft);

    static const int idxs[] = {
        0x0D8, 0x0DC, 0x0DD, 0x0E0, 0x0E4,
        0x100, 0x108, 0x110, 0x114, 0x118, 0x11C, 0x120,
    };
    for (int idx : idxs) {
        void* fn = nullptr;
        if (!SafeReadPtr((char*)vft + idx, &fn)) continue;
        uintptr_t off = (uintptr_t)fn - g_base;
        LOGF("  [vft+0x%X] = %p  (offset +0x%lx)  %s",
             idx, fn, (unsigned long)off,
             IsTextPtr(fn) ? "TEXT" : "");
    }
}

// ------------------------------------------------------------
// Scan GameMode for GameSession (wide)
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
        LOGF("[Scan] +0x%X -> %p -> %p -> %p  (%s)",
             offset, ptr, ptr2, ptr3,
             !ptr3 ? "NULL" :
             (IsValidHeapPtr(ptr3) ? "set-heap" : "other"));

        if (ptr3 == nullptr) {
            LOGF("[Scan] Found candidate @ +0x%X (ptr3 NULL)", offset);
            return ptr;
        }
        if (!best) best = ptr;
    }

    LOGF("[Scan] done. candidates=%d best=%p", candidates, best);
    return best;
}

// ------------------------------------------------------------
// Playlist lookup
// ------------------------------------------------------------
static void* FindPlaylistByName(const char16_t* fullPath) {
    if (!g_StaticFindObject) return nullptr;
    LOGF("");
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

// ------------------------------------------------------------
// Playlist injection
// ------------------------------------------------------------
static bool InjectPlaylist(void* gameMode) {
    if (!gameMode) return false;

    if (!g_cached_playlist) {
        g_cached_playlist = FindPlaylistByName(
            u"/Game/Athena/Playlists/Playlist_DefaultSolo.Playlist_DefaultSolo"
        );
        if (!g_cached_playlist) { LOGF("[Inject] Cannot find Playlist"); return false; }
    }

    bool injected = false;

    void* gameState = nullptr;
    if (SafeReadPtr((char*)gameMode + OFFSET_GameMode_GameState, &gameState) && gameState) {
        void* p2298 = nullptr;
        SafeReadPtr((char*)gameState + OFFSET_GameState_Playlist2298, &p2298);
        if (!p2298) {
            *(void**)((char*)gameState + OFFSET_GameState_Playlist2298) = g_cached_playlist;
            LOGF("[Inject] [Path1a] SET gameState+0x2298 = %p", g_cached_playlist);
            injected = true;
        } else {
            LOGF("[Inject] [Path1a] gameState+0x2298 already = %p", p2298);
        }

        void* p2290 = nullptr;
        SafeReadPtr((char*)gameState + OFFSET_GameState_Playlist2290, &p2290);
        if (!p2290) {
            *(void**)((char*)gameState + OFFSET_GameState_Playlist2290) = g_cached_playlist;
            LOGF("[Inject] [Path1b] SET gameState+0x2290 = %p", g_cached_playlist);
            injected = true;
        } else {
            LOGF("[Inject] [Path1b] gameState+0x2290 already = %p", p2290);
        }
    } else {
        LOGF("[Inject] gameState not valid!");
    }

    if (!g_cached_game_session || !IsValidHeapPtr(g_cached_game_session)) {
        g_cached_game_session = nullptr;

        void* ptr478 = nullptr;
        if (SafeReadPtr((char*)gameMode + 0x478, &ptr478) && IsValidHeapPtr(ptr478) && HasValidVtable(ptr478)) {
            void* ptr2D8 = nullptr;
            if (SafeReadPtr((char*)ptr478 + OFFSET_478_2D8, &ptr2D8) && IsValidHeapPtr(ptr2D8)) {
                g_cached_game_session = ptr478;
                LOGF("[Inject] [Path2] GameSession @+0x478 = %p", ptr478);
            }
        }
        if (!g_cached_game_session) {
            LOGF("[Inject] [Path2] Standard 0x478 failed. Wide-scanning...");
            g_cached_game_session = ScanGameModeForGameSession(gameMode);
        }
    }

    if (g_cached_game_session) {
        void* ptr2D8 = nullptr;
        if (SafeReadPtr((char*)g_cached_game_session + OFFSET_478_2D8, &ptr2D8) && IsValidHeapPtr(ptr2D8)) {
            void* ptr298 = nullptr;
            SafeReadPtr((char*)ptr2D8 + OFFSET_2D8_298, &ptr298);
            LOGF("[Inject] [Path2] current ptr2D8+0x298 = %p", ptr298);

            if (ptr298 != g_cached_playlist) {
                *(void**)((char*)ptr2D8 + OFFSET_2D8_298) = g_cached_playlist;
                LOGF("[Inject] [Path2] SET ptr2D8+0x298 = %p (was %p)", g_cached_playlist, ptr298);
                injected = true;
            } else {
                LOGF("[Inject] [Path2] already correct");
            }
        }
    } else {
        LOGF("[Inject] [Path2] GameSession not found");
    }
    return injected;
}

// ------------------------------------------------------------
// VFT install on GameMode
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
    bool ok1 = VFTPatch(vft, VFT_INDEX_HandleMatchHasStarted,
                        (void*)Hook_HandleMatchHasStarted, (void**)&g_orig_HMHS);
    LOGF("[VFT] Hook 0x11C: %s", ok1 ? "OK" : "FAIL");
    g_known_vfts.push_back(real_vft);
    return true;
}

// ------------------------------------------------------------
// Hooks
// ------------------------------------------------------------
static void Hook_HandleMatchHasStarted(void* this_) {
    LOGF("[0x11C] HandleMatchHasStarted this=%p", this_);
    if (g_orig_HMHS) g_orig_HMHS(this_);
}

static void* Hook_SpawnDefaultPawnFor(void* this_, void* newPlayer, void* startSpot) {
    LOGF("[0xDD VFT] SpawnDefaultPawnFor this=%p newPlayer=%p startSpot=%p",
         this_, newPlayer, startSpot);
    void* r = g_orig_SDPF ? g_orig_SDPF(this_, newPlayer, startSpot) : nullptr;
    return r;
}

typedef int (*PlaylistHandler_t)(void*);
static PlaylistHandler_t g_orig_Playlist = nullptr;

static int Hook_PlaylistHandler(void* a1) {
    static int cc = 0;
    cc++;
    LOGF("[Playlist #%d] a1=%p", cc, a1);
    if (a1) InjectPlaylist(a1);
    int result = g_orig_Playlist ? g_orig_Playlist(a1) : 0;
    LOGF("  -> returned: %d %s", result, result ? "OK" : "FAIL");
    return result;
}

typedef bool (*StartAircraftPhase_t)(void*, char);
static StartAircraftPhase_t g_orig_SAP = nullptr;

static bool Hook_StartAircraftPhase(void* this_, char a2) {
    LOGF("");
    LOGF("========================================");
    LOGF("[Aircraft] StartAircraftPhase this=%p a2=%d", this_, (int)a2);

    DumpGameModeVtable(this_, "aircraft");

    InstallVFTOnReal(this_);
    LOGF("[Aircraft] InjectPlaylist (pre-call)...");
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

// ------------------------------------------------------------
// Direct hook on SpawnDefaultPawnFor (legacy offset — may not fire)
// ------------------------------------------------------------
typedef void* (*SpawnDefaultPawnFor_Direct_t)(void*, void*, void*);
static SpawnDefaultPawnFor_Direct_t g_orig_SDPF_Direct = nullptr;

static void* Hook_SpawnDefaultPawnFor_Direct(void* this_, void* newPlayer, void* startSpot) {
    LOGF("[DIRECT 0x551A7C4] SpawnDefaultPawnFor this=%p newPlayer=%p startSpot=%p",
         this_, newPlayer, startSpot);
    void* r = g_orig_SDPF_Direct ? g_orig_SDPF_Direct(this_, newPlayer, startSpot) : nullptr;
    if (IsValidPtr(r)) {
        float* loc = (float*)((char*)r + OFFSET_Actor_Location);
        if (IsValidPtr(loc)) LOGF("  Pawn Loc ~ (%.1f, %.1f, %.1f)", loc[0], loc[1], loc[2]);
    }
    return r;
}

// ------------------------------------------------------------
// PostLogin
// ------------------------------------------------------------
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
    LOGF("  this_=%p  newPlayer=%p", this_, newPlayer);

    if (IsValidPtr(newPlayer)) {
        void* playerState = nullptr;
        if (SafeReadPtr((char*)newPlayer + OFFSET_PC_PlayerState, &playerState))
            LOGF("  PlayerState (+0x278) = %p", playerState);

        if (IsValidPtr(playerState)) {
            int16_t f3F2 = 0;
            uint8_t f120C = 0;
            SafeReadI16((char*)playerState + OFFSET_PS_3F2, &f3F2);
            SafeReadU8 ((char*)playerState + OFFSET_PS_120C, &f120C);
            LOGF("  +0x3F2=%d  +0x120C=%u", (int)f3F2, (unsigned)f120C);
            std::string name = TryReadPlayerName(playerState);
            LOGF("  PLAYER NAME = \"%s\"", name.c_str());
        }
    }

    if (g_orig_PostLogin) g_orig_PostLogin(this_, newPlayer);
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
    LOGF(" hook — build");
    LOGF(" base       = 0x%lx", g_base);
    LOGF(" text_start = 0x%lx", g_text_start);
    LOGF(" text_end   = 0x%lx", g_text_end);
    LOGF("========================================");

    DobbyHook((void*)(g_base + ADDR_GetGameState),
              (void*)Hook_GetGameState,       (void**)&g_orig_GGS);
    DobbyHook((void*)(g_base + ADDR_PlaylistHandler),
              (void*)Hook_PlaylistHandler,    (void**)&g_orig_Playlist);
    DobbyHook((void*)(g_base + ADDR_StartAircraftPhase),
              (void*)Hook_StartAircraftPhase, (void**)&g_orig_SAP);
    DobbyHook((void*)(g_base + ADDR_PostLogin),
              (void*)Hook_PostLogin,          (void**)&g_orig_PostLogin);

    void* pSDPF = (void*)(g_base + ADDR_SpawnDefaultPawnFor_Direct);
    int rc = DobbyHook(pSDPF, (void*)Hook_SpawnDefaultPawnFor_Direct,
                       (void**)&g_orig_SDPF_Direct);
    LOGF("Direct Hook @ 0x%lx rc=%d", (unsigned long)(g_base + ADDR_SpawnDefaultPawnFor_Direct), rc);

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
