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
#include "dobby.h"

#define LOG_TAG "hook"

#define ADDR_StaticFindObject   0x762A664
#define ADDR_GetGameState       0x55B8AB0
#define ADDR_PlaylistHandler    0x5519710
#define ADDR_StartAircraftPhase 0x550A1F4
#define ADDR_PostLogin          0x550C610

#define OFFSET_GameMode_GameState      0x2D0
#define OFFSET_GameState_Playlist2298  0x2298
#define OFFSET_GameState_Playlist2290  0x2290

#define OFFSET_478_2D8                 0x2D8
#define OFFSET_2D8_298                 0x298

#define VFT_INDEX_HandleMatchHasStarted  0x11C
#define VFT_INDEX_SpawnDefaultPawnFor    0x07

static uint64_t g_post_login_addr = 0;
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
    while (*p) {
        if (*p == u'.') lastDot = p;
        p++;
    }
    return lastDot ? lastDot + 1 : full_path;
}

static bool IsValidPtr(void* ptr) {
    uintptr_t p = (uintptr_t)ptr;
    return (p >= 0x1000000000ULL && p <= 0x8000000000ULL);
}

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

static uintptr_t g_base = 0;
static std::mutex g_vft_mutex;
static std::vector<void*> g_known_vfts;
static void* g_cached_playlist = nullptr;
static void* g_cached_game_session = nullptr;

static void (*g_orig_HMHS)(void*) = nullptr;
static void* (*g_orig_SDPF)(void*, void*, void*) = nullptr;

static void Hook_HandleMatchHasStarted(void*);
static void* Hook_SpawnDefaultPawnFor(void*, void*, void*);

typedef void* (*StaticFindObject_t)(
    void* Class,
    void* InOuter,
    const char16_t* Name,
    bool  ExactClass
);
static StaticFindObject_t g_StaticFindObject = nullptr;

static void* ScanGameModeForGameSession(void* gameMode) {
    if (!gameMode) return nullptr;

    LOGF("[Scan] Scanning GameMode=%p for GameSession...", gameMode);

    for (int offset = 0x400; offset < 0x800; offset += 8) {
        void* ptr = *(void**)((char*)gameMode + offset);
        if (!IsValidPtr(ptr)) continue;

        void* ptr2 = *(void**)((char*)ptr + OFFSET_478_2D8);
        if (!IsValidPtr(ptr2)) continue;

        void* ptr3 = *(void**)((char*)ptr2 + OFFSET_2D8_298);

        LOGF("[Scan] GameMode+0x%X -> %p -> %p -> %p", offset, ptr, ptr2, ptr3);
        
        if (IsValidPtr(ptr3)) {
            return ptr;
        }
    }

    LOGF("[Scan] Done.");
    return nullptr;
}

static void* FindPlaylistByName(const char16_t* fullPath) {
    if (!g_StaticFindObject) return nullptr;

    LOGF("");
    LOGF("========== Playlist Lookup ==========");
    LOGF("Full Path: %s", u16_to_u8(fullPath).c_str());

    void* r1 = g_StaticFindObject(nullptr, nullptr, fullPath, false);
    LOGF("  [nullptr + FullPath] -> %p", r1);
    if (r1) return r1;

    const char16_t* shortName = GetShortName(fullPath);
    void* r2 = g_StaticFindObject(nullptr, (void*)-1, shortName, false);
    LOGF("  [(void*)-1 + ShortName] -> %p", r2);
    if (r2) return r2;

    return nullptr;
}

static bool InjectPlaylist(void* gameMode) {
    if (!gameMode) return false;

    if (!g_cached_playlist) {
        g_cached_playlist = FindPlaylistByName(
            u"/Game/Athena/Playlists/Playlist_DefaultSolo.Playlist_DefaultSolo"
        );
        if (!g_cached_playlist) return false;
    }

    bool injected = false;

    void* gameState = *(void**)((char*)gameMode + OFFSET_GameMode_GameState);
    if (gameState) {
        void* p2298 = *(void**)((char*)gameState + OFFSET_GameState_Playlist2298);
        if (!p2298) {
            *(void**)((char*)gameState + OFFSET_GameState_Playlist2298) = g_cached_playlist;
            LOGF("[Inject] [Path1] SET gameState+0x2298 = %p", g_cached_playlist);
            injected = true;
        }
    }

    if (!g_cached_game_session) {
        void* ptr478 = *(void**)((char*)gameMode + 0x478);
        if (IsValidPtr(ptr478)) {
            void* ptr2D8 = *(void**)((char*)ptr478 + OFFSET_478_2D8);
            if (IsValidPtr(ptr2D8)) {
                g_cached_game_session = ptr478;
            }
        }
        if (!g_cached_game_session) {
            g_cached_game_session = ScanGameModeForGameSession(gameMode);
        }
    }

    if (g_cached_game_session) {
        void* ptr2D8 = *(void**)((char*)g_cached_game_session + OFFSET_478_2D8);
        if (IsValidPtr(ptr2D8)) {
            void* ptr298 = *(void**)((char*)ptr2D8 + OFFSET_2D8_298);
            if (!IsValidPtr(ptr298)) {
                *(void**)((char*)ptr2D8 + OFFSET_2D8_298) = g_cached_playlist;
                LOGF("[Inject] [Path2] SET ptr2D8+0x298 = %p", g_cached_playlist);
                injected = true;
            }
        }
    }

    return injected;
}

static bool InstallVFTOnReal(void* gameMode) {
    if (!gameMode) return false;
    void* real_vft = *(void**)gameMode;
    if (!real_vft) return false;

    std::lock_guard<std::mutex> lock(g_vft_mutex);
    for (auto* v : g_known_vfts) {
        if (v == real_vft) return false;
    }

    LOGF("[VFT] New VFT: %p (GameMode=%p)", real_vft, gameMode);
    void** vft = (void**)real_vft;
    bool ok1 = VFTPatch(vft, VFT_INDEX_HandleMatchHasStarted, (void*)Hook_HandleMatchHasStarted, (void**)&g_orig_HMHS);
    bool ok2 = VFTPatch(vft, VFT_INDEX_SpawnDefaultPawnFor, (void*)Hook_SpawnDefaultPawnFor, (void**)&g_orig_SDPF);
    LOGF("[VFT] Hook 0x11C: %s | 0x07: %s", ok1 ? "OK" : "FAIL", ok2 ? "OK" : "FAIL");
    g_known_vfts.push_back(real_vft);
    return true;
}

static void Hook_HandleMatchHasStarted(void* this_) {
    LOGF("[0x11C] HandleMatchHasStarted this=%p", this_);
    if (g_orig_HMHS) g_orig_HMHS(this_);
}

static void* Hook_SpawnDefaultPawnFor(void* this_, void* newPlayer, void* startSpot) {
    LOGF("[0x07] SpawnDefaultPawnFor this=%p newPlayer=%p startSpot=%p", this_, newPlayer, startSpot);
    if (startSpot == nullptr) LOGF("  [!] startSpot is NULL! Under the map.");
    void* r = g_orig_SDPF ? g_orig_SDPF(this_, newPlayer, startSpot) : nullptr;
    LOGF("  Return = %p", r);
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
    LOGF("  -> PlaylistHandler returned: %d", result);
    return result;
}

typedef bool (*StartAircraftPhase_t)(void*, char);
static StartAircraftPhase_t g_orig_SAP = nullptr;

static bool Hook_StartAircraftPhase(void* this_, char a2) {
    LOGF("");
    LOGF("========================================");
    LOGF("[Aircraft] StartAircraftPhase this=%p a2=%d", this_, (int)a2);
    InstallVFTOnReal(this_);
    LOGF("[Aircraft] Injecting Playlist before original call...");
    InjectPlaylist(this_);
    bool result = g_orig_SAP ? g_orig_SAP(this_, a2) : false;
    LOGF("[Aircraft] Original StartAircraftPhase returned: %d", (int)result);
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

typedef void (*PostLogin_t)(void*, void*);
static PostLogin_t g_orig_PostLogin = nullptr;

static void Hook_PostLogin(void* this_, void* newPlayer) {
    LOGF("");
    LOGF("========================================");
    LOGF("[PostLogin] this_=%p newPlayer=%p", this_, newPlayer);
    
    if (this_) {
        LOGF("[PostLogin] Injecting Playlist...");
        InjectPlaylist(this_);
        
        void* gameState = *(void**)((char*)this_ + OFFSET_GameMode_GameState);
        LOGF("[PostLogin] GameState (at +0x2D0) = %p", gameState);
        
        if (!IsValidPtr(gameState)) {
            LOGF("[PostLogin] GameState is INVALID at 0x2D0! Trying 0x2D8...");
            gameState = *(void**)((char*)this_ + 0x2D8);
            LOGF("[PostLogin] GameState (at +0x2D8) = %p", gameState);
        }

        if (IsValidPtr(gameState)) {
            void* p2298 = *(void**)((char*)gameState + OFFSET_GameState_Playlist2298);
            LOGF("[PostLogin] GameState+0x2298 (Playlist) = %p", p2298);
            if (!IsValidPtr(p2298)) {
                LOGF("[PostLogin] Playlist is NULL! Injecting...");
                InjectPlaylist(this_);
            } else {
                LOGF("[PostLogin] Playlist already exists.");
            }
        }
        
        void* ptr478 = *(void**)((char*)this_ + 0x478);
        LOGF("[PostLogin] GameMode+0x478 (GameSession)=%p", ptr478);
        
        if (IsValidPtr(ptr478)) {
            void* ptr2D8 = *(void**)((char*)ptr478 + OFFSET_478_2D8);
            LOGF("[PostLogin] GameSession+0x2D8=%p", ptr2D8);
            if (IsValidPtr(ptr2D8)) {
                void* ptr298 = *(void**)((char*)ptr2D8 + OFFSET_2D8_298);
                LOGF("[PostLogin] GameSession->0x2D8->0x298 (Playlist)=%p", ptr298);
            }
        }
    }
    
    if (g_orig_PostLogin) {
        g_orig_PostLogin(this_, newPlayer);
    }
    
    LOGF("[PostLogin] Original returned.");
    LOGF("========================================");
}

static void RunHooks() {
    LibInfo info{0,0,0};
    for (int i = 0; i < 60; i++) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        info = GetLibInfo();
        if (info.base) break;
    }
    if (!info.base) { LOGF("[ERROR] libUnreal not found"); return; }

    g_base = info.base;
    LOGF("========================================");
    LOGF(" GameHook - Build %s %s", __DATE__, __TIME__);
    LOGF(" base = 0x%lx", g_base);
    LOGF("========================================");

    DobbyHook((void*)(g_base + ADDR_GetGameState), (void*)Hook_GetGameState, (void**)&g_orig_GGS);
    DobbyHook((void*)(g_base + ADDR_PlaylistHandler), (void*)Hook_PlaylistHandler, (void**)&g_orig_Playlist);
    DobbyHook((void*)(g_base + ADDR_StartAircraftPhase), (void*)Hook_StartAircraftPhase, (void**)&g_orig_SAP);

    g_post_login_addr = g_base + ADDR_PostLogin;
    int r = DobbyHook((void*)g_post_login_addr, (void*)Hook_PostLogin, (void**)&g_orig_PostLogin);
    LOGF("Hook PostLogin: %s (addr=0x%lx)", r == 0 ? "OK" : "FAIL", g_post_login_addr);

    g_StaticFindObject = (StaticFindObject_t)(g_base + ADDR_StaticFindObject);
    LOGF("StaticFindObject: %p", (void*)g_StaticFindObject);

    LOGF(" All Hooks Installed. Waiting for map load...");
}

extern "C" JNIEXPORT jint JNICALL
JNI_OnLoad(JavaVM* vm, void* reserved) {
    InitLogFile();
    LOGF("========================================");
    LOGF(" GameHook - Build %s %s", __DATE__, __TIME__);
    LOGF(" PID: %d", getpid());
    LOGF("========================================");

    std::thread([]{
        try { RunHooks(); }
        catch (...) { LOGF("[ERROR] Exception in RunHooks"); }
    }).detach();

    return JNI_VERSION_1_6;
}
