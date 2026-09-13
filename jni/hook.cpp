// ============================================================
// hook — Android JNI (v29: crash-safe, RestartPlayer probes)
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
#include <set>
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
#define ADDR_RestartPlayerCand_A        0x550DB58   // slot 0x11A
#define ADDR_RestartPlayerCand_B        0x551EB8C   // slot 0x11D

#define OFFSET_GameMode_GameState       0x2D0
#define OFFSET_GameState_Playlist2298   0x2298
#define OFFSET_GameState_Playlist2290   0x2290
#define OFFSET_PC_PlayerState           0x278
#define OFFSET_PS_2A8                   0x2A8
#define OFFSET_PS_3F2                   0x3F2
#define OFFSET_PS_120C                  0x120C

#define VFT_NAME_ISVALID_OFF            0x30
#define VFT_NAME_GETNAME_OFF            0x38

// Set to 1 to enable (risky)
#define ENABLE_PATH1B 0
#define ENABLE_PATH2  0

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
// Globals
// ------------------------------------------------------------
static uintptr_t g_base = 0;
static uintptr_t g_text_start = 0;
static uintptr_t g_text_end = 0;
static std::mutex g_inject_mutex;
static std::set<void*> g_injected_gms;
static void* g_cached_playlist = nullptr;

typedef void* (*StaticFindObject_t)(void*, void*, const char16_t*, bool);
static StaticFindObject_t g_StaticFindObject = nullptr;

// ------------------------------------------------------------
// Validation
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

// ------------------------------------------------------------
// Playlist lookup (direct only, no scan)
// ------------------------------------------------------------
static void* FindPlaylist() {
    if (!g_StaticFindObject) return nullptr;
    LOGF("========== Playlist Lookup ==========");
    const char16_t* full = u"/Game/Athena/Playlists/Playlist_DefaultSolo.Playlist_DefaultSolo";
    void* r = g_StaticFindObject(nullptr, nullptr, full, false);
    LOGF("  FullPath -> %p %s", r, r ? "OK" : "");
    return r;
}

// ------------------------------------------------------------
// Playlist injection (SAFE: Path1a only)
// ------------------------------------------------------------
static bool InjectPlaylist(void* gameMode) {
    if (!gameMode) return false;

    std::lock_guard<std::mutex> lock(g_inject_mutex);
    if (g_injected_gms.count(gameMode)) return false;

    if (!g_cached_playlist) {
        g_cached_playlist = FindPlaylist();
        if (!g_cached_playlist) { LOGF("[Inject] Playlist not found"); return false; }
    }

    bool injected = false;

    void* gameState = nullptr;
    if (SafeReadPtr((char*)gameMode + OFFSET_GameMode_GameState, &gameState) && gameState) {
        // Path1a — gameState + 0x2298
        void* p2298 = nullptr;
        SafeReadPtr((char*)gameState + OFFSET_GameState_Playlist2298, &p2298);
        if (!p2298) {
            *(void**)((char*)gameState + OFFSET_GameState_Playlist2298) = g_cached_playlist;
            LOGF("[Inject] [Path1a] SET gameState+0x2298 = %p", g_cached_playlist);
            injected = true;
        } else {
            LOGF("[Inject] [Path1a] already = %p", p2298);
        }

#if ENABLE_PATH1B
        // Path1b — gameState + 0x2290
        void* p2290 = nullptr;
        SafeReadPtr((char*)gameState + OFFSET_GameState_Playlist2290, &p2290);
        if (!p2290) {
            *(void**)((char*)gameState + OFFSET_GameState_Playlist2290) = g_cached_playlist;
            LOGF("[Inject] [Path1b] SET gameState+0x2290 = %p", g_cached_playlist);
            injected = true;
        }
#endif
    } else {
        LOGF("[Inject] gameState invalid");
    }

#if ENABLE_PATH2
    // Path2 — disabled by default (suspect of crash)
    LOGF("[Inject] [Path2] disabled");
#endif

    if (injected) g_injected_gms.insert(gameMode);
    return injected;
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
    LOGF("========== [PostLogin #%llu] GM=%p PC=%p ==========",
         (unsigned long long)g_postlogin_calls, this_, newPlayer);

    if (IsValidPtr(newPlayer)) {
        void* pawnB = nullptr;
        SafeReadPtr((char*)newPlayer + 0x2B8, &pawnB);

        void* playerState = nullptr;
        SafeReadPtr((char*)newPlayer + OFFSET_PC_PlayerState, &playerState);

        LOGF("  PC+0x2B8 BEFORE=%p  PS=%p", pawnB, playerState);

        if (IsValidPtr(playerState)) {
            int16_t f3F2 = 0; uint8_t f120C = 0;
            SafeReadI16((char*)playerState + OFFSET_PS_3F2, &f3F2);
            SafeReadU8 ((char*)playerState + OFFSET_PS_120C, &f120C);
            std::string name = TryReadPlayerName(playerState);
            LOGF("  NAME=\"%s\"  +3F2=%d +120C=%u",
                 name.c_str(), (int)f3F2, (unsigned)f120C);
        }
    }

    if (g_orig_PostLogin) g_orig_PostLogin(this_, newPlayer);

    if (IsValidPtr(newPlayer)) {
        void* pawnA = nullptr;
        SafeReadPtr((char*)newPlayer + 0x2B8, &pawnA);
        LOGF("  PC+0x2B8 AFTER=%p", pawnA);
    }
}

// ------------------------------------------------------------
// PlaylistHandler
// ------------------------------------------------------------
typedef int (*PlaylistHandler_t)(void*);
static PlaylistHandler_t g_orig_Playlist = nullptr;

static int Hook_PlaylistHandler(void* a1) {
    LOGF("[Playlist] a1=%p", a1);
    if (a1) InjectPlaylist(a1);
    int result = g_orig_Playlist ? g_orig_Playlist(a1) : 0;
    LOGF("  -> returned: %d", result);
    return result;
}

// ------------------------------------------------------------
// StartAircraftPhase
// ------------------------------------------------------------
typedef bool (*StartAircraftPhase_t)(void*, char);
static StartAircraftPhase_t g_orig_SAP = nullptr;

static bool Hook_StartAircraftPhase(void* this_, char a2) {
    LOGF("");
    LOGF("========================================");
    LOGF("[Aircraft] StartAircraftPhase this=%p a2=%d", this_, (int)a2);
    InjectPlaylist(this_);
    bool result = g_orig_SAP ? g_orig_SAP(this_, a2) : false;
    LOGF("[Aircraft] returned: %d %s", (int)result, result ? "SUCCESS" : "FAIL");
    LOGF("========================================");
    return result;
}

// ------------------------------------------------------------
// GetGameState (read-only, no VFT install)
// ------------------------------------------------------------
typedef void* (*GetGameState_t)(void*);
static GetGameState_t g_orig_GGS = nullptr;

static void* Hook_GetGameState(void* gm) {
    void* r = g_orig_GGS ? g_orig_GGS(gm) : nullptr;
    return r;
}

// ------------------------------------------------------------
// RestartPlayer candidates — probe only
// ------------------------------------------------------------
typedef void (*RestartPlayerCandA_t)(void*, void*);
static RestartPlayerCandA_t g_orig_RPC_A = nullptr;

static void Hook_RestartPlayerCandA(void* this_, void* newPlayer) {
    LOGF(">>> [CAND-A 0x550DB58] this=%p newPlayer=%p", this_, newPlayer);
    if (g_orig_RPC_A) g_orig_RPC_A(this_, newPlayer);
    LOGF("<<< [CAND-A] done");
}

typedef void (*RestartPlayerCandB_t)(void*, void*);
static RestartPlayerCandB_t g_orig_RPC_B = nullptr;

static void Hook_RestartPlayerCandB(void* this_, void* newPlayer) {
    LOGF(">>> [CAND-B 0x551EB8C] this=%p newPlayer=%p", this_, newPlayer);
    if (g_orig_RPC_B) g_orig_RPC_B(this_, newPlayer);
    LOGF("<<< [CAND-B] done");
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
    LOGF(" hook — v29");
    LOGF(" base=0x%lx text=0x%lx..0x%lx", g_base, g_text_start, g_text_end);
    LOGF("========================================");

    DobbyHook((void*)(g_base + ADDR_GetGameState),
              (void*)Hook_GetGameState,       (void**)&g_orig_GGS);
    DobbyHook((void*)(g_base + ADDR_PlaylistHandler),
              (void*)Hook_PlaylistHandler,    (void**)&g_orig_Playlist);
    DobbyHook((void*)(g_base + ADDR_StartAircraftPhase),
              (void*)Hook_StartAircraftPhase, (void**)&g_orig_SAP);
    DobbyHook((void*)(g_base + ADDR_PostLogin),
              (void*)Hook_PostLogin,          (void**)&g_orig_PostLogin);

    int rcA = DobbyHook((void*)(g_base + ADDR_RestartPlayerCand_A),
                        (void*)Hook_RestartPlayerCandA, (void**)&g_orig_RPC_A);
    LOGF("CAND-A hook rc=%d", rcA);

    int rcB = DobbyHook((void*)(g_base + ADDR_RestartPlayerCand_B),
                        (void*)Hook_RestartPlayerCandB, (void**)&g_orig_RPC_B);
    LOGF("CAND-B hook rc=%d", rcB);

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
