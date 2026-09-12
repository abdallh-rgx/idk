// ============================================================
// Erbium Android Hook — MULTI VFT PROBE
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

// ---------- Generic probe hook ----------
// كل hook يسجل فقط: هذا مؤشر دوال عام
struct ProbeEntry {
    int          vft_index;
    const char*  name;
    void*        original;
    uint32_t     call_count;
};

static ProbeEntry g_probes[] = {
    { 0xD4, "VFT[0xD4]", nullptr, 0 },
    { 0xD5, "VFT[0xD5]", nullptr, 0 },
    { 0xD6, "VFT[0xD6]", nullptr, 0 },
    { 0xD7, "VFT[0xD7]", nullptr, 0 },
    { 0xD8, "VFT[0xD8]", nullptr, 0 },
    { 0xD9, "VFT[0xD9]", nullptr, 0 },
    { 0xDA, "VFT[0xDA]", nullptr, 0 },
    { 0xDB, "VFT[0xDB]", nullptr, 0 },
    { 0xDC, "VFT[0xDC]", nullptr, 0 },
    { 0xDD, "VFT[0xDD]", nullptr, 0 },   // SpawnDefaultPawnFor
    { 0xDE, "VFT[0xDE]", nullptr, 0 },
    { 0xDF, "VFT[0xDF]", nullptr, 0 },
    { 0xE0, "VFT[0xE0]", nullptr, 0 },
};
static constexpr size_t g_probes_count = sizeof(g_probes) / sizeof(g_probes[0]);

// hook generic: ياخد 3 args (this, arg1, arg2) ويعيد void*
// هذا لا يعمل لكل الدوال لكن يكفي للكشف
typedef void* (*GenericFunc_t)(void*, void*, void*);

// دالة hook عامة — نحتاج 13 نسخة مختلفة (واحدة لكل مدخل)
// لذا سنستخدم macro
#define DEFINE_PROBE_HOOK(N) \
    static void* Hook_Probe_##N(void* a, void* b, void* c) { \
        auto& p = g_probes[N]; \
        p.call_count++; \
        if (p.call_count <= 3) { \
            LOGF("[PROBE] %s (#%u) called | this=%p arg1=%p arg2=%p", \
                 p.name, p.call_count, a, b, c); \
        } \
        auto orig = (GenericFunc_t)p.original; \
        if (!orig) return nullptr; \
        return orig(a, b, c); \
    }

DEFINE_PROBE_HOOK(0)
DEFINE_PROBE_HOOK(1)
DEFINE_PROBE_HOOK(2)
DEFINE_PROBE_HOOK(3)
DEFINE_PROBE_HOOK(4)
DEFINE_PROBE_HOOK(5)
DEFINE_PROBE_HOOK(6)
DEFINE_PROBE_HOOK(7)
DEFINE_PROBE_HOOK(8)
DEFINE_PROBE_HOOK(9)
DEFINE_PROBE_HOOK(10)
DEFINE_PROBE_HOOK(11)
DEFINE_PROBE_HOOK(12)

static void* g_hook_fns[] = {
    (void*)Hook_Probe_0,
    (void*)Hook_Probe_1,
    (void*)Hook_Probe_2,
    (void*)Hook_Probe_3,
    (void*)Hook_Probe_4,
    (void*)Hook_Probe_5,
    (void*)Hook_Probe_6,
    (void*)Hook_Probe_7,
    (void*)Hook_Probe_8,
    (void*)Hook_Probe_9,
    (void*)Hook_Probe_10,
    (void*)Hook_Probe_11,
    (void*)Hook_Probe_12,
};

// ---------- VFT patch ----------
static bool VFTPatch(void** vtable, size_t idx, void* hook_fn, void** orig_out) {
    void** slot = &vtable[idx];
    *orig_out = *slot;

    const size_t page_size = (size_t)sysconf(_SC_PAGESIZE);
    uintptr_t addr       = (uintptr_t)slot;
    uintptr_t page_start = addr & ~(page_size - 1);

    size_t span = page_size;
    if ((addr + sizeof(void*)) > (page_start + page_size))
        span += page_size;

    if (mprotect((void*)page_start, span, PROT_READ | PROT_WRITE) != 0) {
        return false;
    }

    *slot = hook_fn;

    mprotect((void*)page_start, span, PROT_READ | PROT_EXEC);
    __builtin___clear_cache((char*)page_start, (char*)page_start + span);

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
    LOGF("text = 0x%lx .. 0x%lx", info.text_start, info.text_end);

    void** vtable = (void**)(info.base + VFT_PTR_OFFSET);
    LOGF("VFT = %p", vtable);

    // اطبع كل VFT entries أولاً
    LOGF("");
    LOGF("--- VFT entries we will hook ---");
    for (size_t i = 0; i < g_probes_count; i++) {
        uintptr_t fn = (uintptr_t)vtable[g_probes[i].vft_index];
        bool in_text = (fn >= info.text_start && fn < info.text_end);
        LOGF("  [0x%X] = 0x%lx %s",
             g_probes[i].vft_index, fn, in_text ? "(TEXT ✓)" : "(NOT TEXT ✗)");
    }

    // ثبّت الـ hooks على المدخلات الصالحة فقط
    LOGF("");
    LOGF("--- Installing probes ---");
    int installed = 0;
    for (size_t i = 0; i < g_probes_count; i++) {
        uintptr_t fn = (uintptr_t)vtable[g_probes[i].vft_index];
        // ثبّت فقط إذا كانت القيمة في .text (أي دالة حقيقية)
        if (fn < info.text_start || fn >= info.text_end) {
            LOGF("  Skip 0x%X (not in text)", g_probes[i].vft_index);
            continue;
        }

        bool ok = VFTPatch(vtable, g_probes[i].vft_index,
                           g_hook_fns[i], &g_probes[i].original);
        if (ok) {
            LOGF("  ✓ 0x%X hooked (orig=%p)", g_probes[i].vft_index, g_probes[i].original);
            installed++;
        } else {
            LOGF("  ✗ 0x%X failed", g_probes[i].vft_index);
        }
    }

    LOGF("");
    LOGF("==========================================");
    LOGF(" %d probes installed", installed);
    LOGF(" Waiting for any of them to be called...");
    LOGF("==========================================");
}

extern "C" JNIEXPORT jint JNICALL
JNI_OnLoad(JavaVM* vm, void* reserved) {
    InitLogFile();
    std::thread([]{
        try { RunHook(); } catch (...) {}
    }).detach();
    return JNI_VERSION_1_6;
}
