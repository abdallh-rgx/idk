// ============================================================
// Erbium Android Hook — DIAGNOSTIC
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

#define LOG_TAG "ErbiumDiag"
#define UNREAL_LIB_NAME  "libUnreal.so"
#define VFT_PTR_OFFSET   0xBDB8BC0
#define VFT_INDEX        0xDD

static FILE* g_logfile = nullptr;

static std::mutex& GetMutex() {
    static std::mutex* m = new std::mutex();
    return *m;
}

static void InitLogFile() {
    const char* paths[] = {
        "/storage/emulated/0/Android/data/com.epicgames.fortnite/files/diag.log",
        "/sdcard/Android/data/com.epicgames.fortnite/files/diag.log",
        "/data/local/tmp/diag.log",
    };
    for (auto p : paths) {
        FILE* f = fopen(p, "w");
        if (f) {
            g_logfile = f;
            __android_log_print(ANDROID_LOG_INFO, LOG_TAG, "Logging to: %s", p);
            return;
        }
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

struct LibInfo { uintptr_t base, text_start, text_end; };

static LibInfo GetLibInfo(const char* name) {
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

static bool IsInRange(uintptr_t a, uintptr_t s, uintptr_t e) {
    return a >= s && a < e;
}

static void RunDiagnostic() {
    LibInfo info{0,0,0};
    LOGF("[Wait] Searching for %s...", UNREAL_LIB_NAME);

    for (int i = 0; i < 60; i++) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        info = GetLibInfo(UNREAL_LIB_NAME);
        if (info.base) { LOGF("[Wait] Found after %d sec", i+1); break; }
    }

    if (!info.base) { LOGF("[ERROR] libUnreal never loaded"); return; }

    LOGF("");
    LOGF("base       = 0x%lx", info.base);
    LOGF("text_start = 0x%lx", info.text_start);
    LOGF("text_end   = 0x%lx", info.text_end);

    uintptr_t loc = info.base + VFT_PTR_OFFSET;
    LOGF("");
    LOGF("VFT_PTR address  = 0x%lx", loc);
    LOGF("Value at address = 0x%lx", *(uintptr_t*)loc);

    void** vt_a = *(void***)loc;
    LOGF("");
    LOGF("--- Interp A (deref) ---");
    LOGF("VFT ptr (A) = %p", vt_a);
    if (vt_a) {
        uintptr_t ea = (uintptr_t)vt_a[VFT_INDEX];
        LOGF("A[0x%X] = 0x%lx (%s)", VFT_INDEX, ea,
             IsInRange(ea, info.text_start, info.text_end) ? "TEXT" : "----");
    }

    void** vt_b = (void**)loc;
    uintptr_t eb = (uintptr_t)vt_b[VFT_INDEX];
    LOGF("");
    LOGF("--- Interp B (direct) ---");
    LOGF("VFT ptr (B) = %p", vt_b);
    LOGF("B[0x%X] = 0x%lx (%s)", VFT_INDEX, eb,
         IsInRange(eb, info.text_start, info.text_end) ? "TEXT" : "----");

    LOGF("");
    LOGF("--- Context around 0x%X ---", VFT_INDEX);
    for (int i = -3; i <= 3; i++) {
        int idx = VFT_INDEX + i;
        uintptr_t va = vt_a ? (uintptr_t)vt_a[idx] : 0;
        uintptr_t vb = (uintptr_t)vt_b[idx];
        LOGF("idx 0x%X | A=0x%lx (%s) | B=0x%lx (%s)",
             idx,
             va, IsInRange(va, info.text_start, info.text_end) ? "TEXT" : "----",
             vb, IsInRange(vb, info.text_start, info.text_end) ? "TEXT" : "----");
    }

    LOGF("");
    LOGF("DIAGNOSTIC COMPLETE");
}

extern "C" JNIEXPORT jint JNICALL
JNI_OnLoad(JavaVM* vm, void* reserved) {
    __android_log_print(ANDROID_LOG_INFO, LOG_TAG, "=== JNI_OnLoad ===");
    InitLogFile();
    std::thread([]{
        try { RunDiagnostic(); } catch (...) {
            LOGF("[ERROR] exception in thread");
        }
    }).detach();
    return JNI_VERSION_1_6;
}
