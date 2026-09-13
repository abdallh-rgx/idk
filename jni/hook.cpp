// ============================================================
// Erbium Android Hook — Final v2
// الإصلاحات:
// 1. InstallVFTOnReal الآن يُهوّك فعلياً على VFT الجديد
// 2. إعادة المحاولة بعد فرض GamePhase
// 3. تسجيل تفصيلي لكل VFT
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
#include <vector>
#include <android/log.h>
#include <unistd.h>
#include <sys/mman.h>
#include "dobby.h"

#define LOG_TAG "ErbiumHook"
#define UNREAL_LIB_NAME  "libUnreal.so"

// ============================================================
// Offsets (مؤكدة من IDA)
// ============================================================
#define OFFSET_GameMode_GameState      0x2D0
#define OFFSET_GameState_GamePhase     0x2150
#define OFFSET_GameMode_V4             0x478
#define OFFSET_V4_V5                   0x2D8
#define OFFSET_V5_V6                   0x298

#define VFT_INDEX_HandleMatchHasStarted  0x11C
#define VFT_INDEX_SpawnDefaultPawnFor    0x0DD

#define ADDR_GetGameState       0x55B8AB0
#define ADDR_PlaylistHandler    0x5519710
#define ADDR_StartAircraftPhase 0x550A1F4

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

// ============================================================
// Global state
// ============================================================
struct VFTRecord {
    void* vft_addr;
    void* game_mode;
    bool  hooked;
};
static std::mutex g_vft_mutex;
static std::vector<VFTRecord> g_known_vfts;

// Prototypes
static void Hook_HandleMatchHasStarted(void*);
static void* Hook_SpawnDefaultPawnFor(void*, void*, void*);

// المعالجات الأصلية
static void (*g_orig_HMHS)(void*) = nullptr;
static void* (*g_orig_SDPF)(void*, void*, void*) = nullptr;

// ============================================================
// تثبيت Hooks على VFT فعلياً
// ============================================================
static bool InstallVFTOnReal(void* game_mode) {
    if (!game_mode) return false;

    void* real_vft = *(void**)game_mode;
    if (!real_vft) return false;

    std::lock_guard<std::mutex> lk(g_vft_mutex);

    // تحقق: هل هوكنا هذا VFT؟
    for (auto& rec : g_known_vfts) {
        if (rec.vft_addr == real_vft) {
            return false;  // معروف مسبقاً
        }
    }

    LOGF("");
    LOGF("========================================");
    LOGF(" NEW VFT DETECTED — INSTALLING HOOKS");
    LOGF("========================================");
    LOGF(" GameMode = %p", game_mode);
    LOGF(" VFT      = %p", real_vft);

    void** vft = (void**)real_vft;

    LOGF(" VFT entries BEFORE:");
    LOGF("   [0x0DD] = %p", vft[VFT_INDEX_SpawnDefaultPawnFor]);
    LOGF("   [0x11C] = %p", vft[VFT_INDEX_HandleMatchHasStarted]);

    // ✅ الهوك الفعلي
    bool ok1 = VFTPatch(vft, VFT_INDEX_HandleMatchHasStarted,
                        (void*)Hook_HandleMatchHasStarted,
                        (void**)&g_orig_HMHS);
    bool ok2 = VFTPatch(vft, VFT_INDEX_SpawnDefaultPawnFor,
                        (void*)Hook_SpawnDefaultPawnFor,
                        (void**)&g_orig_SDPF);

    LOGF(" Hook 0x11C: %s (orig=%p)", ok1 ? "OK" : "FAIL", g_orig_HMHS);
    LOGF(" Hook 0x0DD: %s (orig=%p)", ok2 ? "OK" : "FAIL", g_orig_SDPF);

    g_known_vfts.push_back({real_vft, game_mode, ok1 && ok2});
    LOGF("========================================");
    return true;
}

// ============================================================
// Hook 0x11C: HandleMatchHasStarted
// ============================================================
static void Hook_HandleMatchHasStarted(void* this_) {
    LOGF("");
    LOGF("========================================");
    LOGF("[Hook 0x11C] HandleMatchHasStarted");
    LOGF("  this = %p", this_);

    if (this_) {
        void* gameState = *(void**)((char*)this_ + OFFSET_GameMode_GameState);
        LOGF("  GameState = %p", gameState);
        if (gameState) {
            uint8_t phase = *(uint8_t*)((char*)gameState + OFFSET_GameState_GamePhase);
            LOGF("  GamePhase = %d", phase);
        }
    }
    LOGF("========================================");

    if (g_orig_HMHS) g_orig_HMHS(this_);
}

// ============================================================
// Hook 0xDD: SpawnDefaultPawnFor
// ============================================================
static void* Hook_SpawnDefaultPawnFor(void* a, void* b, void* c) {
    LOGF("");
    LOGF("========================================");
    LOGF("[Hook 0xDD] SpawnDefaultPawnFor");
    LOGF("  this=%p newPlayer=%p startSpot=%p", a, b, c);
    if (c == nullptr) LOGF("  [!] startSpot = NULL");
    LOGF("========================================");

    void* result = nullptr;
    if (g_orig_SDPF) result = g_orig_SDPF(a, b, c);
    LOGF("  Return = %p", result);
    return result;
}

// ============================================================
// Hook Playlist Handler
// ============================================================
typedef int (*PlaylistHandler_t)(void*);
static PlaylistHandler_t g_orig_Playlist = nullptr;

static int Hook_PlaylistHandler(void* a1) {
    static int cc = 0;
    cc++;
    bool shouldLog = (cc <= 5) || (cc % 50 == 0);

    if (!shouldLog) {
        return g_orig_Playlist ? g_orig_Playlist(a1) : 0;
    }

    LOGF("");
    LOGF("========================================");
    LOGF("[Hook Playlist] #%d | a1=%p", cc, a1);
    if (a1) {
        void* v4 = *(void**)((char*)a1 + OFFSET_GameMode_V4);
        LOGF("  v4 (0x%X) = %p", OFFSET_GameMode_V4, v4);
        if (v4) {
            void* v5 = *(void**)((char*)v4 + OFFSET_V4_V5);
            LOGF("  v5 (0x%X) = %p", OFFSET_V4_V5, v5);
            if (v5) {
                void* v6 = *(void**)((char*)v5 + OFFSET_V5_V6);
                LOGF("  v6 (0x%X) = %p", OFFSET_V5_V6, v6);
            } else {
                LOGF("  [!] v5 NULL!");
            }
        } else {
            LOGF("  [!] v4 NULL!");
        }
    }
    int result = g_orig_Playlist ? g_orig_Playlist(a1) : 0;
    LOGF("  → Return = %d %s", result, result ? "(OK)" : "(FAIL)");
    LOGF("========================================");
    return result;
}

// ============================================================
// Hook StartAircraftPhase
// ============================================================
typedef bool (*StartAircraftPhase_t)(void*, char);
static StartAircraftPhase_t g_orig_SAP = nullptr;
static int g_sap_attempts = 0;

static bool Hook_StartAircraftPhase(void* this_, char a2) {
    g_sap_attempts++;

    LOGF("");
    LOGF("========================================");
    LOGF("[Hook Aircraft] StartAircraftPhase #%d", g_sap_attempts);
    LOGF("  this=%p a2=%d", this_, (int)a2);

    // ✅ ثبّت Hooks على VFT هذا GameMode
    InstallVFTOnReal(this_);

    void* gameState = nullptr;
    uint8_t* phase_ptr = nullptr;

    if (this_) {
        gameState = *(void**)((char*)this_ + OFFSET_GameMode_GameState);
        LOGF("  GameState = %p", gameState);

        if (gameState) {
            phase_ptr = (uint8_t*)((char*)gameState + OFFSET_GameState_GamePhase);
            LOGF("  GamePhase BEFORE = %d", *phase_ptr);
        }
    }

    // ✅ استدعِ الأصلية
    bool result = false;
    if (g_orig_SAP)
        result = g_orig_SAP(this_, a2);

    LOGF("  Original return = %d", (int)result);

    // ============================================================
    // 🎯 إذا فشلت: أجبر GamePhase = 3
    // ============================================================
    if (!result && this_ && phase_ptr) {
        LOGF("  [!] FAILED → forcing GamePhase = 3");
        uint8_t old = *phase_ptr;
        *phase_ptr = 3;
        LOGF("  GamePhase: %d → %d", old, *phase_ptr);
        result = true;

        // استدعِ HandleMatchHasStarted يدوياً إن لم يُستدعى
        if (g_orig_HMHS) {
            LOGF("  [Manual] Calling HandleMatchHasStarted...");
            g_orig_HMHS(this_);
            LOGF("  [✓] Manual call done");
        }
    }

    LOGF("  Final return = %d", (int)result);
    LOGF("========================================");
    return result;
}

// ============================================================
// Hook GetGameState
// ============================================================
typedef void* (*GetGameState_t)(void*);
static GetGameState_t g_orig_GGS = nullptr;
static uint64_t g_ggs_calls = 0;

static void* Hook_GetGameState(void* gameMode) {
    g_ggs_calls++;
    void* result = g_orig_GGS ? g_orig_GGS(gameMode) : nullptr;
    if (gameMode && g_ggs_calls > 3) {
        InstallVFTOnReal(gameMode);
    }
    return result;
}

// ============================================================
// Main
// ============================================================
static void RunHooks() {
    LibInfo info{0,0,0};
    LOGF("[Wait] Searching for %s...", UNREAL_LIB_NAME);
    for (int i = 0; i < 60; i++) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        info = GetLibInfo();
        if (info.base) { LOGF("[Wait] Found after %d sec", i+1); break; }
    }
    if (!info.base) { LOGF("[ERROR] libUnreal not found"); return; }

    LOGF("");
    LOGF("========================================");
    LOGF(" base = 0x%lx", info.base);
    LOGF(" text = 0x%lx .. 0x%lx", info.text_start, info.text_end);
    LOGF("========================================");

    // Hook GetGameState
    uintptr_t a1 = info.base + ADDR_GetGameState;
    int r1 = DobbyHook((void*)a1, (void*)Hook_GetGameState, (void**)&g_orig_GGS);
    LOGF("Hook GetGameState: %s", r1 == 0 ? "OK" : "FAIL");

    // Hook Playlist
    uintptr_t a2 = info.base + ADDR_PlaylistHandler;
    int r2 = DobbyHook((void*)a2, (void*)Hook_PlaylistHandler, (void**)&g_orig_Playlist);
    LOGF("Hook Playlist: %s", r2 == 0 ? "OK" : "FAIL");

    // Hook StartAircraftPhase
    uintptr_t a3 = info.base + ADDR_StartAircraftPhase;
    int r3 = DobbyHook((void*)a3, (void*)Hook_StartAircraftPhase, (void**)&g_orig_SAP);
    LOGF("Hook StartAircraftPhase: %s", r3 == 0 ? "OK" : "FAIL");

    LOGF("");
    LOGF("========================================");
    LOGF(" All Hooks Installed");
    LOGF("========================================");
}

extern "C" JNIEXPORT jint JNICALL
JNI_OnLoad(JavaVM* vm, void* reserved) {
    InitLogFile();
    LOGF("========================================");
    LOGF(" Erbium Hook — Final v2");
    LOGF(" Build: %s %s", __DATE__, __TIME__);
    LOGF("========================================");
    std::thread([]{ try { RunHooks(); } catch (...) {} }).detach();
    return JNI_VERSION_1_6;
}
