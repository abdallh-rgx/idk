// ============================================================
// Erbium Android Hook — LOG-ONLY version
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

#define LOG_TAG "ErbiumHook"
#define UNREAL_LIB_NAME  "libUnreal.so"
#define VFT_PTR_OFFSET   0xBDB8BC0
#define VFT_INDEX        0xDD

// ---------- Logger ----------
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

// ---------- Lib info ----------
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

// ---------- دالة الاستبدال (Log Only) ----------
typedef void* (*SpawnDefaultPawnFor_t)(void*, void*, void*);
static SpawnDefaultPawnFor_t g_orig_SpawnDefaultPawnFor = nullptr;
static uint64_t g_call_count = 0;

static void* Hook_SpawnDefaultPawnFor(void* this_, void* new_player, void* start_spot) {
    g_call_count++;
    LOGF("");
    LOGF("========================================");
    LOGF("[Hook #%llu] SpawnDefaultPawnFor", (unsigned long long)g_call_count);
    LOGF("  this_      = %p", this_);
    LOGF("  NewPlayer  = %p", new_player);
    LOGF("  StartSpot  = %p %s", start_spot, start_spot == nullptr ? "← NULL!" : "");

    // استدعاء الدالة الأصلية — بدون أي تعديل
    void* pawn = nullptr;
    if (g_orig_SpawnDefaultPawnFor)
        pawn = g_orig_SpawnDefaultPawnFor(this_, new_player, start_spot);

    LOGF("  Return     = %p %s", pawn, pawn == nullptr ? "← NULL!" : "✓");
    LOGF("========================================");
    return pawn;
}

// ---------- تركيب الـ Hook ----------
static bool VFTHook(void** vtable, size_t idx, void* hook_fn, void** orig_out) {
    void** slot = &vtable[idx];
    if (orig_out) *orig_out = *slot;

    const size_t page_size = (size_t)sysconf(_SC_PAGESIZE);
    uintptr_t addr       = (uintptr_t)slot;
    uintptr_t page_start = addr & ~(page_size - 1);

    size_t span = page_size;
    if ((addr + sizeof(void*)) > (page_start + page_size))
        span += page_size;

    if (mprotect((void*)page_start, span, PROT_READ | PROT_WRITE) != 0) {
        LOGF("[VFTHook] mprotect RW failed: %s", strerror(errno));
        return false;
    }

    *slot = hook_fn;

    mprotect((void*)page_start, span, PROT_READ | PROT_EXEC);
    __builtin___clear_cache((char*)page_start, (char*)page_start + span);

    LOGF("[VFTHook] ✓ slot=%p orig=%p hook=%p", slot, *orig_out, hook_fn);
    return true;
}

// ---------- العمل الفعلي ----------
static void RunHook() {
    LibInfo info{0,0,0};
    LOGF("[Wait] Searching for %s...", UNREAL_LIB_NAME);

    for (int i = 0; i < 60; i++) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        info = GetLibInfo(UNREAL_LIB_NAME);
        if (info.base) { LOGF("[Wait] Found after %d sec", i+1); break; }
    }

    if (!info.base) { LOGF("[ERROR] libUnreal never loaded"); return; }

    LOGF("base = 0x%lx", info.base);

    // Interpretation B: العنوان نفسه هو VFT
    void** vtable = (void**)(info.base + VFT_PTR_OFFSET);
    LOGF("VFT (Interp B) = %p", vtable);
    LOGF("VFT[0xDD] = %p", vtable[VFT_INDEX]);

    // ثبّت الـ Hook
    bool ok = VFTHook(vtable, VFT_INDEX,
                      (void*)Hook_SpawnDefaultPawnFor,
                      (void**)&g_orig_SpawnDefaultPawnFor);

    LOGF(ok ? "[✓] Hook installed successfully" : "[✗] Hook FAILED");

    if (ok) {
        LOGF("");
        LOGF("==========================================");
        LOGF(" Waiting for SpawnDefaultPawnFor to be called...");
        LOGF("==========================================");
    }
}

extern "C" JNIEXPORT jint JNICALL
JNI_OnLoad(JavaVM* vm, void* reserved) {
    __android_log_print(ANDROID_LOG_INFO, LOG_TAG, "=== JNI_OnLoad ===");
    InitLogFile();
    std::thread([]{
        try { RunHook(); } catch (...) {
            LOGF("[ERROR] exception in thread");
        }
    }).detach();
    return JNI_VERSION_1_6;
}
