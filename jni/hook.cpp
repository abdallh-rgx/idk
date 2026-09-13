// ============================================================
// Erbium: Capture Real VFT via sub_55B8AB0 (GetGameState)
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
#include "dobby.h"   // ← الآن متوفر

#define LOG_TAG "ErbiumRealVFT"
#define UNREAL_LIB_NAME "libUnreal.so"

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
        "/storage/emulated/0/Android/data/com.epicgames.fortnite/files/realvft.log",
        "/sdcard/Android/data/com.epicgames.fortnite/files/realvft.log",
        "/data/local/tmp/realvft.log",
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
    LibInfo info{0,0,0};
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
// VFT patch
// ============================================================
static bool VFTPatch(void** vtable, size_t idx, void* hook_fn, void** orig_out) {
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
// الخطوة 1: هوك sub_55B8AB0 (GetGameState) لالتقاط VFT الحقيقي
// ============================================================
typedef void* (*GetGameState_t)(void*);
static GetGameState_t g_orig_GetGameState = nullptr;
static bool g_real_vft_captured = false;
static uint64_t g_call_count = 0;

// VFT indices نهوكها على VFT الحقيقي
static void* g_orig_real_11C = nullptr;
static void* g_orig_real_DD  = nullptr;

// دالة Hook لـ sub_55B98F4 (Tick Handler) — نستخدمها كـ trigger
typedef void (*TickFunc_t)(void*);
static TickFunc_t g_orig_TickFunc = nullptr;
static bool g_vft_hook_installed = false;

// Hook لـ 0x11C على VFT الحقيقي (HandleMatchHasStarted)
typedef void (*HandleStart_t)(void*);
static HandleStart_t g_orig_HandleStart = nullptr;

static void Hook_RealHandleStart(void* this_) {
    LOGF("");
    LOGF("========================================");
    LOGF("[REAL VFT] HandleMatchHasStarted (0x11C) called!");
    LOGF("  this = %p", this_);
    if (this_) {
        void* vft = *(void**)this_;
        void* gameState = *(void**)((char*)this_ + 0x278);
        LOGF("  VFT  = %p", vft);
        LOGF("  GameState = %p", gameState);
        if (gameState) {
            uint8_t ms = *(uint8_t*)((char*)gameState + 0x2150);
            LOGF("  MatchState = %d", ms);
        }
    }
    LOGF("========================================");

    if (g_orig_HandleStart) g_orig_HandleStart(this_);
}

// Hook لـ 0xDD على VFT الحقيقي (SpawnDefaultPawnFor)
typedef void* (*SpawnPawn_t)(void*, void*, void*);
static SpawnPawn_t g_orig_SpawnPawn = nullptr;

static void* Hook_RealSpawnPawn(void* a, void* b, void* c) {
    LOGF("[REAL VFT] SpawnDefaultPawnFor (0xDD) called! a=%p b=%p c=%p", a, b, c);
    if (g_orig_SpawnPawn) return g_orig_SpawnPawn(a, b, c);
    return nullptr;
}

// دالة Hook لـ sub_55B8AB0
static void* Hook_GetGameState(void* gameMode) {
    g_call_count++;

    // احصل على VFT الحقيقي
    void* real_vft = nullptr;
    if (gameMode) {
        real_vft = *(void**)gameMode;   // أول 8 بايت من الكائن = VFT pointer
    }

    if (!g_real_vft_captured && real_vft && g_call_count > 5) {
        g_real_vft_captured = true;

        LOGF("");
        LOGF("========================================");
        LOGF(" REAL VFT CAPTURED!");
        LOGF("========================================");
        LOGF("GameMode instance = %p", gameMode);
        LOGF("Real VFT          = %p", real_vft);

        // اقرأ بعض المدخلات للمقارنة
        void** rv = (void**)real_vft;
        LOGF("");
        LOGF("Real VFT entries:");
        for (int i : {0xB0, 0xD4, 0xDD, 0x11C}) {
            LOGF("  [0x%03X] = %p", i, rv[i]);
        }

        // ثبّت الهوك على VFT الحقيقي!
        if (!g_vft_hook_installed) {
            bool ok1 = VFTPatch(rv, 0x11C,
                                (void*)Hook_RealHandleStart,
                                (void**)&g_orig_HandleStart);
            bool ok2 = VFTPatch(rv, 0xDD,
                                (void*)Hook_RealSpawnPawn,
                                (void**)&g_orig_SpawnPawn);
            g_vft_hook_installed = true;
            LOGF("");
            LOGF("Install on REAL VFT:");
            LOGF("  0x11C: %s (orig=%p)",
                 ok1 ? "OK" : "FAIL", g_orig_HandleStart);
            LOGF("  0xDD:  %s (orig=%p)",
                 ok2 ? "OK" : "FAIL", g_orig_SpawnPawn);
            LOGF("========================================");
        }
    }

    if (g_orig_GetGameState)
        return g_orig_GetGameState(gameMode);
    return nullptr;
}

// ============================================================
// التثبيت
// ============================================================
static void RunHooks() {
    LibInfo info{0,0,0};
    LOGF("[Wait] libUnreal...");

    for (int i = 0; i < 60; i++) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        info = GetLibInfo();
        if (info.base) break;
    }
    if (!info.base) { LOGF("libUnreal not found"); return; }

    LOGF("base = 0x%lx", info.base);

    // هوك sub_55B8AB0
    uintptr_t getGameState = info.base + 0x55B8AB0;
    LOGF("Target sub_55B8AB0 = 0x%lx", getGameState);

    int ret = DobbyHook((void*)getGameState,
                        (void*)Hook_GetGameState,
                        (void**)&g_orig_GetGameState);

    LOGF("");
    LOGF("DobbyHook ret = %d %s", ret, ret == 0 ? "(OK)" : "(FAIL)");
    LOGF("");
    LOGF("========================================");
    LOGF(" Waiting for GetGameState to be called...");
    LOGF("========================================");
}

extern "C" JNIEXPORT jint JNICALL
JNI_OnLoad(JavaVM* vm, void* reserved) {
    InitLogFile();
    std::thread([]{ try { RunHooks(); } catch (...) {} }).detach();
    return JNI_VERSION_1_6;
}
