// ============================================================
// Erbium Android Hook — Complete Solution
// - Captures real VFT via sub_55B8AB0 (GetGameState)
// - Hooks VFT[0x11C] HandleMatchHasStarted
// - Hooks VFT[0xDD]  SpawnDefaultPawnFor
// - Hooks sub_5519710 Playlist Handler (diagnostic + fix)
// - Hooks sub_550A1F4 StartAircraftPhase (fix GamePhase on fail)
// ============================================================
#include <jni.h>
#include <cstdint>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <cerrno>
#include <cstdarg>
#include <dlfcn.h>
#include <link.h>
#include <thread>
#include <chrono>
#include <mutex>
#include <android/log.h>
#include <unistd.h>
#include <sys/mman.h>
#include "dobby.h"

#define LOG_TAG "ErbiumHook"
#define UNREAL_LIB_NAME  "libUnreal.so"

// ============================================================
// Offsets (مؤكدة من IDA)
// ============================================================
#define OFFSET_GameMode_GameState      0x2D0    // 720 - مؤكد من sub_550A1F4
#define OFFSET_GameState_GamePhase     0x2150   // 8528 - مؤكد
#define OFFSET_GameMode_V4             0x478    // 1144 - مؤكد من sub_5519710
#define OFFSET_V4_V5                   0x2D8    // 728
#define OFFSET_V5_V6                   0x298    // 664

// ============================================================
// عناوين الدوال (من IDA)
// ============================================================
#define ADDR_GetGameState       0x55B8AB0    // GetGameState helper
#define ADDR_PlaylistHandler    0x5519710    // Playlist check
#define ADDR_StartAircraftPhase 0x550A1F4    // StartAircraftPhase

// ============================================================
// Logger
// ============================================================
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
    char buf[2048];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);

    __android_log_print(ANDROID_LOG_INFO, LOG_TAG, "%s", buf);

    if (g_logfile) {
        std::lock_guard<std::mutex> lk(GetMutex());
        fprintf(g_logfile, "%s\n", buf);
        fflush(g_logfile);
    }
}

// ============================================================
// LibInfo
// ============================================================
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

// ============================================================
// VFT Patch
// ============================================================
static bool VFTPatch(void** vtable, size_t idx, void* hook_fn, void** orig_out) {
    void** slot = &vtable[idx];
    *orig_out = *slot;

    const size_t page_size = (size_t)sysconf(_SC_PAGESIZE);
    uintptr_t addr       = (uintptr_t)slot;
    uintptr_t page_start = addr & ~(page_size - 1);

    size_t span = page_size;
    if ((addr + sizeof(void*)) > (page_start + page_size))
        span += page_size;

    if (mprotect((void*)page_start, span, PROT_READ | PROT_WRITE) != 0)
        return false;

    *slot = hook_fn;

    mprotect((void*)page_start, span, PROT_READ | PROT_EXEC);
    __builtin___clear_cache((char*)page_start, (char*)page_start + span);
    return true;
}

// ============================================================
// Global state
// ============================================================
static void*  g_gameMode       = nullptr;
static void*  g_realVFT        = nullptr;
static bool   g_hooks_installed = false;

// ============================================================
// Hook: HandleMatchHasStarted (VFT[0x11C])
// ============================================================
typedef void (*HandleMatchHasStarted_t)(void*);
static HandleMatchHasStarted_t g_orig_HandleMatchHasStarted = nullptr;

static void Hook_HandleMatchHasStarted(void* this_) {
    LOGF("");
    LOGF("========================================");
    LOGF("[Hook 0x11C] HandleMatchHasStarted");
    LOGF("  this = %p", this_);

    if (this_) {
        // اقرأ GameState (offset 0x2D0)
        void* gameState = *(void**)((char*)this_ + OFFSET_GameMode_GameState);
        LOGF("  GameState = %p", gameState);

        if (gameState) {
            uint8_t phase = *(uint8_t*)((char*)gameState + OFFSET_GameState_GamePhase);
            LOGF("  GamePhase = %d", phase);
        }

        // اقرأ السلسلة v4 → v5 → v6
        void* v4 = *(void**)((char*)this_ + OFFSET_GameMode_V4);
        LOGF("  v4 (this+0x%X) = %p", OFFSET_GameMode_V4, v4);

        if (v4) {
            void* v5 = *(void**)((char*)v4 + OFFSET_V4_V5);
            LOGF("  v5 (v4+0x%X) = %p", OFFSET_V4_V5, v5);

            if (v5) {
                void* v6 = *(void**)((char*)v5 + OFFSET_V5_V6);
                LOGF("  v6 (v5+0x%X) = %p", OFFSET_V5_V6, v6);
            }
        }
    }

    LOGF("========================================");

    if (g_orig_HandleMatchHasStarted)
        g_orig_HandleMatchHasStarted(this_);
}

// ============================================================
// Hook: SpawnDefaultPawnFor (VFT[0xDD])
// ============================================================
typedef void* (*SpawnDefaultPawnFor_t)(void*, void*, void*);
static SpawnDefaultPawnFor_t g_orig_SpawnDefaultPawnFor = nullptr;

static void* Hook_SpawnDefaultPawnFor(void* a, void* b, void* c) {
    LOGF("");
    LOGF("========================================");
    LOGF("[Hook 0xDD] SpawnDefaultPawnFor");
    LOGF("  this=%p newPlayer=%p startSpot=%p", a, b, c);
    if (c == nullptr) {
        LOGF("  [!] startSpot = NULL");
    }
    LOGF("========================================");

    void* result = nullptr;
    if (g_orig_SpawnDefaultPawnFor)
        result = g_orig_SpawnDefaultPawnFor(a, b, c);

    LOGF("  Return = %p", result);
    return result;
}

// ============================================================
// Hook: Playlist Handler (sub_5519710) — تشخيصي
// ============================================================
typedef int (*PlaylistHandler_t)(void*);
static PlaylistHandler_t g_orig_PlaylistHandler = nullptr;

static int Hook_PlaylistHandler(void* a1) {
    static int call_count = 0;
    call_count++;

    // اطبع فقط أول 5 مرات + كل 100
    bool shouldLog = (call_count <= 5) || (call_count % 100 == 0);
    if (!shouldLog) {
        if (g_orig_PlaylistHandler)
            return g_orig_PlaylistHandler(a1);
        return 0;
    }

    LOGF("");
    LOGF("========================================");
    LOGF("[Hook Playlist] sub_5519710 #%d | a1=%p", call_count, a1);

    if (a1) {
        // v4
        void* v4 = *(void**)((char*)a1 + OFFSET_GameMode_V4);
        LOGF("  v4 (0x%X) = %p", OFFSET_GameMode_V4, v4);

        if (v4) {
            // v5
            void* v5 = *(void**)((char*)v4 + OFFSET_V4_V5);
            LOGF("  v5 (0x%X) = %p", OFFSET_V4_V5, v5);

            if (v5) {
                // v6
                void* v6 = *(void**)((char*)v5 + OFFSET_V5_V6);
                LOGF("  v6 (0x%X) = %p", OFFSET_V5_V6, v6);

                if (v6 == nullptr) {
                    LOGF("  [!] v6 is NULL → هذا سبب فشل Playlist!");
                }
            } else {
                LOGF("  [!] v5 is NULL!");
            }
        } else {
            LOGF("  [!] v4 is NULL!");
        }
    }

    int result = 0;
    if (g_orig_PlaylistHandler)
        result = g_orig_PlaylistHandler(a1);

    LOGF("  → Return = %d %s", result, result == 0 ? "(FAIL)" : "(OK)");
    LOGF("========================================");
    return result;
}

// ============================================================
// Hook: StartAircraftPhase (sub_550A1F4) — مع إصلاح GamePhase
// ============================================================
typedef bool (*StartAircraftPhase_t)(void*, char);
static StartAircraftPhase_t g_orig_StartAircraftPhase = nullptr;

static bool Hook_StartAircraftPhase(void* this_, char a2) {
    LOGF("");
    LOGF("========================================");
    LOGF("[Hook Aircraft] StartAircraftPhase");
    LOGF("  this=%p a2=%d", this_, (int)a2);

    if (this_) {
        void* gameState = *(void**)((char*)this_ + OFFSET_GameMode_GameState);
        LOGF("  GameState = %p", gameState);

        if (gameState) {
            uint8_t phase = *(uint8_t*)((char*)gameState + OFFSET_GameState_GamePhase);
            LOGF("  GamePhase BEFORE = %d", phase);
        }
    }

    bool result = false;
    if (g_orig_StartAircraftPhase)
        result = g_orig_StartAircraftPhase(this_, a2);

    LOGF("  Original return = %d", (int)result);

    // ============================================================
    // 🎯 إصلاح: إذا فشلت، نجبر GamePhase = Aircraft (3)
    // ============================================================
    if (!result && this_) {
        LOGF("  [!] StartAircraftPhase FAILED → forcing GamePhase");
        void* gameState = *(void**)((char*)this_ + OFFSET_GameMode_GameState);

        if (gameState) {
            uint8_t* phase = (uint8_t*)((char*)gameState + OFFSET_GameState_GamePhase);
            LOGF("  Old GamePhase = %d", *phase);
            *phase = 3;  // Aircraft
            LOGF("  [✓] Forced GamePhase = %d", *phase);
            result = true;
        }
    }

    LOGF("  Final return = %d", (int)result);
    LOGF("========================================");
    return result;
}

// ============================================================
// Hook: GetGameState (sub_55B8AB0) — لالتقاط real VFT
// ============================================================
typedef void* (*GetGameState_t)(void*);
static GetGameState_t g_orig_GetGameState = nullptr;
static uint64_t g_call_count = 0;

static void* Hook_GetGameState(void* gameMode) {
    g_call_count++;

    void* result = nullptr;
    if (g_orig_GetGameState)
        result = g_orig_GetGameState(gameMode);

    // التقط real VFT
    if (!g_hooks_installed && gameMode && g_call_count > 5) {
        void* real_vft = *(void**)gameMode;

        if (real_vft) {
            g_gameMode = gameMode;
            g_realVFT = real_vft;
            g_hooks_installed = true;

            LOGF("");
            LOGF("========================================");
            LOGF(" REAL VFT CAPTURED");
            LOGF("========================================");
            LOGF("GameMode = %p", gameMode);
            LOGF("Real VFT = %p", real_vft);

            void** vft = (void**)real_vft;
            LOGF("  [0x0B0] = %p", vft[0x0B0]);
            LOGF("  [0x0D4] = %p", vft[0x0D4]);
            LOGF("  [0x0DD] = %p", vft[0x0DD]);
            LOGF("  [0x11C] = %p", vft[0x11C]);

            // ثبّت Hooks على real VFT
            bool ok1 = VFTPatch(vft, 0x11C,
                                (void*)Hook_HandleMatchHasStarted,
                                (void**)&g_orig_HandleMatchHasStarted);
            bool ok2 = VFTPatch(vft, 0x0DD,
                                (void*)Hook_SpawnDefaultPawnFor,
                                (void**)&g_orig_SpawnDefaultPawnFor);

            LOGF("  Hook 0x11C: %s (orig=%p)", ok1 ? "OK" : "FAIL",
                 g_orig_HandleMatchHasStarted);
            LOGF("  Hook 0x0DD: %s (orig=%p)", ok2 ? "OK" : "FAIL",
                 g_orig_SpawnDefaultPawnFor);
            LOGF("========================================");
        }
    }

    return result;
}

// ============================================================
// تثبيت كل الـ Hooks
// ============================================================
static void RunHooks() {
    LibInfo info{0,0,0};
    LOGF("[Wait] Searching for %s...", UNREAL_LIB_NAME);

    for (int i = 0; i < 60; i++) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        info = GetLibInfo();
        if (info.base) {
            LOGF("[Wait] Found after %d sec", i+1);
            break;
        }
    }

    if (!info.base) {
        LOGF("[ERROR] libUnreal never loaded");
        return;
    }

    LOGF("");
    LOGF("========================================");
    LOGF(" libUnreal base = 0x%lx", info.base);
    LOGF(" text = 0x%lx .. 0x%lx", info.text_start, info.text_end);
    LOGF("========================================");

    // ============================================================
    // 1. Hook GetGameState → لالتقاط Real VFT
    // ============================================================
    uintptr_t addr_getGameState = info.base + ADDR_GetGameState;
    LOGF("Target GetGameState (0x%X) = 0x%lx",
         ADDR_GetGameState, addr_getGameState);

    int ret1 = DobbyHook((void*)addr_getGameState,
                         (void*)Hook_GetGameState,
                         (void**)&g_orig_GetGameState);
    LOGF("DobbyHook GetGameState ret = %d %s",
         ret1, ret1 == 0 ? "(OK)" : "(FAIL)");

    // ============================================================
    // 2. Hook Playlist Handler (sub_5519710)
    // ============================================================
    uintptr_t addr_playlist = info.base + ADDR_PlaylistHandler;
    LOGF("Target Playlist (0x%X) = 0x%lx",
         ADDR_PlaylistHandler, addr_playlist);

    int ret2 = DobbyHook((void*)addr_playlist,
                         (void*)Hook_PlaylistHandler,
                         (void**)&g_orig_PlaylistHandler);
    LOGF("DobbyHook Playlist ret = %d %s",
         ret2, ret2 == 0 ? "(OK)" : "(FAIL)");

    // ============================================================
    // 3. Hook StartAircraftPhase (sub_550A1F4)
    // ============================================================
    uintptr_t addr_aircraft = info.base + ADDR_StartAircraftPhase;
    LOGF("Target StartAircraftPhase (0x%X) = 0x%lx",
         ADDR_StartAircraftPhase, addr_aircraft);

    int ret3 = DobbyHook((void*)addr_aircraft,
                         (void*)Hook_StartAircraftPhase,
                         (void**)&g_orig_StartAircraftPhase);
    LOGF("DobbyHook StartAircraftPhase ret = %d %s",
         ret3, ret3 == 0 ? "(OK)" : "(FAIL)");

    LOGF("");
    LOGF("========================================");
    LOGF(" All Hooks Installed");
    LOGF("  GetGameState:        %s", ret1 == 0 ? "OK" : "FAIL");
    LOGF("  PlaylistHandler:     %s", ret2 == 0 ? "OK" : "FAIL");
    LOGF("  StartAircraftPhase:  %s", ret3 == 0 ? "OK" : "FAIL");
    LOGF("========================================");
    LOGF(" Waiting for events...");
    LOGF("========================================");
}

// ============================================================
// نقطة الدخول
// ============================================================
extern "C" JNIEXPORT jint JNICALL
JNI_OnLoad(JavaVM* vm, void* reserved) {
    InitLogFile();

    LOGF("========================================");
    LOGF(" Erbium Android Hook — Complete");
    LOGF(" Build: %s %s", __DATE__, __TIME__);
    LOGF(" PID: %d", getpid());
    LOGF("========================================");

    std::thread([]{
        try {
            RunHooks();
        } catch (...) {
            LOGF("[ERROR] Exception in RunHooks");
        }
    }).detach();

    return JNI_VERSION_1_6;
}
