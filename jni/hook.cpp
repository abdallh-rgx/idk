#include <jni.h>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cstdarg>
#include <cerrno>
#include <string>
#include <thread>
#include <chrono>
#include <mutex>
#include <dlfcn.h>
#include <link.h>
#include <android/log.h>
#include <sys/uio.h>
#include <unistd.h>
#include "dobby.h"

#define LOG_TAG "FNamePool"
#define ADDR_FNamePool_LogStats   0x74915C8

// ==================== Globals ====================
static FILE* g_logfile = nullptr;
static std::mutex g_mutex;
static uintptr_t g_base = 0;
static uintptr_t g_FNamePool = 0;

// ==================== Logger (نفس الكود الشغال) ====================
static void InitLogFile() {
    const char* paths[] = {
        "/storage/emulated/0/Android/data/com.epicgames.fortnite/files/FNamePool.txt",
        "/sdcard/Android/data/com.epicgames.fortnite/files/FNamePool.txt",
        "/data/data/com.epicgames.fortnite/files/FNamePool.txt",
        "/data/local/tmp/FNamePool.txt",
    };
    for (auto p : paths) {
        FILE* f = fopen(p, "w");
        if (f) { 
            g_logfile = f;
            __android_log_print(ANDROID_LOG_INFO, LOG_TAG, "[FILE] opened: %s", p);
            return;
        }
        __android_log_print(ANDROID_LOG_WARN, LOG_TAG, "[FILE] failed: %s (errno=%d)", p, errno);
    }
    __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "[FILE] all failed — logcat only");
}

static void LOGF(const char* fmt, ...) {
    char buf[4096];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    
    __android_log_print(ANDROID_LOG_INFO, LOG_TAG, "%s", buf);
    
    if (g_logfile) {
        std::lock_guard<std::mutex> lock(g_mutex);
        fprintf(g_logfile, "%s\n", buf);
        fflush(g_logfile);
    }
}

// ==================== GetLibInfo (نفس كودك الأصلي) ====================
struct LibInfo { uintptr_t base, text_start, text_end; };

static LibInfo GetLibInfo() {
    LibInfo info{0, 0, 0};
    dl_iterate_phdr([](struct dl_phdr_info* ph, size_t, void* d) -> int {
        auto* o = (LibInfo*)d;
        if (!ph->dlpi_name || !*ph->dlpi_name) return 0;
        if (!strstr(ph->dlpi_name, "libUnreal")) return 0;
        o->base = ph->dlpi_addr;
        for (int i = 0; i < ph->dlpi_phnum; i++) {
            const auto& p = ph->dlpi_phdr[i];
            if (p.p_type != PT_LOAD || !(p.p_flags & PF_X)) continue;
            o->text_start = ph->dlpi_addr + p.p_vaddr;
            o->text_end   = o->text_start + p.p_memsz;
            break;
        }
        return 1;
    }, &info);
    return info;
}

// ==================== FName Resolver ====================
static std::string ResolveFName(uint32_t nameId) {
    if (!g_FNamePool) return "<no pool>";
    
    uint32_t blockIdx = nameId >> 16;
    uint32_t offsetInBlock = nameId & 0xFFFF;
    if (blockIdx >= 8192) return "<oob>";
    
    uint64_t blockPtr = 0;
    {
        struct iovec l = { &blockPtr, 8 };
        struct iovec r = { (void*)(g_FNamePool + 0x7C + blockIdx * 8), 8 };
        if (process_vm_readv(getpid(), &l, 1, &r, 1, 0) != 8) return "<rdfail>";
    }
    if (!blockPtr) return "<null>";
    
    uintptr_t entryPtr = blockPtr + offsetInBlock * 2;
    
    uint16_t header = 0;
    {
        struct iovec l = { &header, 2 };
        struct iovec r = { (void*)entryPtr, 2 };
        if (process_vm_readv(getpid(), &l, 1, &r, 1, 0) != 2) return "<rd>";
    }
    
    uint32_t len = (header >> 6) & 0x3FF;
    bool isWide = (header & 0x8000) != 0;
    if (len == 0 || len > 1023) return "<badlen>";
    
    char buf[1024] = {};
    if (isWide) {
        char16_t wide[512] = {};
        uint32_t nBytes = len * 2;
        if (nBytes > sizeof(wide) - 2) nBytes = sizeof(wide) - 2;
        struct iovec l = { wide, nBytes };
        struct iovec r = { (void*)(entryPtr + 2), nBytes };
        if (process_vm_readv(getpid(), &l, 1, &r, 1, 0) != (ssize_t)nBytes) return "<rd>";
        for (uint32_t i = 0; i < len; i++) buf[i] = (char)(wide[i] & 0x7F);
    } else {
        uint32_t nBytes = len;
        if (nBytes > sizeof(buf) - 1) nBytes = sizeof(buf) - 1;
        struct iovec l = { buf, nBytes };
        struct iovec r = { (void*)(entryPtr + 2), nBytes };
        if (process_vm_readv(getpid(), &l, 1, &r, 1, 0) != (ssize_t)nBytes) return "<rd>";
    }
    return std::string(buf, len);
}

// ==================== FNamePool Hook ====================
typedef void (*LogStats_t)(void*, void*);
static LogStats_t o_LogStats = nullptr;

static void Hook_LogStats(void* a1, void* a2) {
    static bool once = false;
    if (!once && a1) {
        once = true;
        g_FNamePool = (uintptr_t)a1;
        
        LOGF("========================================");
        LOGF("FNamePool @ 0x%lx", g_FNamePool);
        LOGF("libBase   @ 0x%lx", g_base);
        LOGF("Offset    = 0x%lx  <- SAVE THIS", g_FNamePool - g_base);
        LOGF("========================================");
        
        // اختبر أول 20 FName
        LOGF("--- First 20 FNames ---");
        for (uint32_t i = 0; i < 20; i++) {
            LOGF("[%2u] = %s", i, ResolveFName(i).c_str());
        }
    }
    
    if (o_LogStats) o_LogStats(a1, a2);
}

// ==================== Main ====================
static void Run() {
    // 1) انتظر libUnreal.so
    LibInfo info{0,0,0};
    for (int i = 0; i < 60; i++) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        info = GetLibInfo();
        if (info.base) break;
    }
    if (!info.base) { 
        LOGF("[ERR] libUnreal not found after 60s"); 
        return; 
    }
    g_base = info.base;
    
    LOGF("========================================");
    LOGF("FNamePool Dumper — Fortnite 21.30");
    LOGF("libBase = 0x%lx", g_base);
    LOGF("========================================");
    
    // 2) ثبّت hook على FNamePool::LogStats
    void* target = (void*)(g_base + ADDR_FNamePool_LogStats);
    int rc = DobbyHook(target, (void*)Hook_LogStats, (void**)&o_LogStats);
    
    LOGF("DobbyHook(%p) = %d", target, rc);
    LOGF("Original ptr   = %p", (void*)o_LogStats);
    
    if (rc != 0) {
        LOGF("!!! HOOK FAILED !!!");
        return;
    }
    
    LOGF("Hook installed. Waiting for LogStats call...");
}

// ==================== JNI ====================
extern "C" JNIEXPORT jint JNICALL
JNI_OnLoad(JavaVM* vm, void* reserved) {
    InitLogFile();       // ← أول شي
    LOGF("========================================");
    LOGF("FNamePool Dumper %s %s", __DATE__, __TIME__);
    LOGF("PID: %d", getpid());
    LOGF("========================================");
    
    std::thread([]{
        try { Run(); }
        catch (...) { LOGF("[ERR] exception in Run"); }
    }).detach();
    
    return JNI_VERSION_1_6;
}
