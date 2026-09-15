#include <jni.h>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <thread>
#include <chrono>
#include <mutex>
#include <android/log.h>
#include <sys/uio.h>
#include <unistd.h>
#include "dobby.h"

#define LOG_TAG "FNamePool"
#define ADDR_FNamePool_LogStats   0x74915C8

// مسار الحفظ المطلوب
#define OUTPUT_PATH "/storage/emulated/0/Android/data/com.epicgames.fortnite/files/FNamePool.txt"

uintptr_t g_libBase          = 0;
uintptr_t g_FNamePool        = 0;
uintptr_t g_FNamePoolOffset  = 0;
uint32_t  g_entryStride      = 2;    // جرّب 2 أول، لو ما نفع 4
uint32_t  g_blocksOffset     = 0x7C; // الأولوية (لو ما نفع جرّب 0x40 / 0x80 / 0x100)

static std::mutex g_file_mutex;
static FILE* g_out = nullptr;

// ==================== File helpers ====================

static bool OpenOutputFile() {
    std::lock_guard<std::mutex> lock(g_file_mutex);
    if (g_out) return true;
    
    g_out = fopen(OUTPUT_PATH, "w");
    if (!g_out) {
        __android_log_print(ANDROID_LOG_ERROR, LOG_TAG,
            "Cannot open %s (errno=%d)", OUTPUT_PATH, errno);
        return false;
    }
    return true;
}

static void WriteLine(const char* fmt, ...) {
    char buf[2048];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    
    // Android log
    __android_log_print(ANDROID_LOG_INFO, LOG_TAG, "%s", buf);
    
    // ملف
    std::lock_guard<std::mutex> lock(g_file_mutex);
    if (g_out) {
        fprintf(g_out, "%s\n", buf);
        fflush(g_out);   // ← ضروري عشان النتيجة تظهر فوراً
    }
}

// ==================== Read helpers ====================

static bool R32(uintptr_t addr, uint32_t* out) {
    struct iovec local  = { out, 4 };
    struct iovec remote = { (void*)addr, 4 };
    return process_vm_readv(getpid(), &local, 1, &remote, 1, 0) == 4;
}

static bool RP64(uintptr_t addr, uint64_t* out) {
    struct iovec local  = { out, 8 };
    struct iovec remote = { (void*)addr, 8 };
    return process_vm_readv(getpid(), &local, 1, &remote, 1, 0) == 8;
}

// ==================== FName resolver ====================

static std::string ResolveFName(uint32_t nameId) {
    if (!g_FNamePool) return "<no pool>";
    
    uint32_t blockIdx      = nameId >> 16;
    uint32_t offsetInBlock = nameId & 0xFFFF;
    
    if (blockIdx >= 8192) return "<oob>";
    
    // اقرأ pointer البلوك
    uint64_t blockPtr = 0;
    if (!RP64(g_FNamePool + g_blocksOffset + blockIdx * 8, &blockPtr))
        return "<rdfail>";
    if (!blockPtr)
        return "<null>";
    
    uintptr_t entryPtr = blockPtr + offsetInBlock * g_entryStride;
    
    // Header (uint16)
    uint16_t header = 0;
    struct iovec local  = { &header, 2 };
    struct iovec remote = { (void*)entryPtr, 2 };
    if (process_vm_readv(getpid(), &local, 1, &remote, 1, 0) != 2)
        return "<rd>";
    
    uint32_t len    = (header >> 6) & 0x3FF;
    bool     isWide = (header & 0x8000) != 0;
    
    if (len == 0 || len > 1023) return "<badlen>";
    
    char buf[1024] = {};
    if (isWide) {
        char16_t wide[512] = {};
        uint32_t nBytes = len * 2;
        if (nBytes > sizeof(wide) - 2) nBytes = sizeof(wide) - 2;
        
        local  = { wide, nBytes };
        remote = { (void*)(entryPtr + 2), nBytes };
        if (process_vm_readv(getpid(), &local, 1, &remote, 1, 0) != (ssize_t)nBytes)
            return "<rd>";
        for (uint32_t i = 0; i < len; i++) buf[i] = (char)(wide[i] & 0x7F);
    } else {
        uint32_t nBytes = len;
        if (nBytes > sizeof(buf) - 1) nBytes = sizeof(buf) - 1;
        local  = { buf, nBytes };
        remote = { (void*)(entryPtr + 2), nBytes };
        if (process_vm_readv(getpid(), &local, 1, &remote, 1, 0) != (ssize_t)nBytes)
            return "<rd>";
    }
    
    return std::string(buf, len);
}

// ==================== Test ====================

static void TestFNamePool() {
    WriteLine("==========================================");
    WriteLine("FNamePool Test");
    WriteLine("==========================================");
    WriteLine("libBase           = 0x%016lx", g_libBase);
    WriteLine("FNamePool         = 0x%016lx", g_FNamePool);
    WriteLine("FNamePool offset  = 0x%lx       <- SAVE THIS", g_FNamePoolOffset);
    WriteLine("Blocks offset     = 0x%lx", g_blocksOffset);
    WriteLine("Entry stride      = %u", g_entryStride);
    WriteLine("");
    WriteLine("--- First 20 FNames ---");
    
    bool ok = false;
    for (uint32_t i = 0; i < 20; i++) {
        std::string s = ResolveFName(i);
        WriteLine("[%2u] = %s", i, s.c_str());
        if (i == 0 && s == "None") ok = true;
        if (i == 1 && s == "ByteProperty") ok = true;
    }
    
    if (ok) {
        WriteLine("");
        WriteLine("##########################################");
        WriteLine("###     ★★★ FNamePool WORKS! ★★★     ###");
        WriteLine("##########################################");
        WriteLine("Saved offset: 0x%lx", g_FNamePoolOffset);
        WriteLine("Use it: g_FNamePool = libBase + 0x%lx;", g_FNamePoolOffset);
    } else {
        WriteLine("");
        WriteLine("!!! FAILED with stride=%u, blocksOffset=0x%lx", g_entryStride, g_blocksOffset);
        
        // جرّب stride بديل
        if (g_entryStride == 2) {
            WriteLine("Trying stride = 4...");
            g_entryStride = 4;
            TestFNamePool();
        } else {
            WriteLine("Trying blocksOffset = 0x40...");
            g_blocksOffset = 0x40;
            g_entryStride  = 2;
            TestFNamePool();
        }
    }
}

// ==================== Hook ====================

typedef void (*LogStats_t)(void* a1, void* a2);
static LogStats_t o_LogStats = nullptr;

static void Hook_LogStats(void* a1, void* a2) {
    static bool once = false;
    if (!once && a1) {
        once = true;
        
        g_FNamePool       = (uintptr_t)a1;
        g_FNamePoolOffset = g_FNamePool - g_libBase;
        
        WriteLine("==========================================");
        WriteLine("FNamePool Detected via LogStats hook");
        WriteLine("==========================================");
        WriteLine("FNamePool ptr     = 0x%016lx", g_FNamePool);
        WriteLine("libBase           = 0x%016lx", g_libBase);
        WriteLine("Offset from base  = 0x%lx", g_FNamePoolOffset);
        WriteLine("");
        
        // اختبر مباشرة
        TestFNamePool();
        
        WriteLine("");
        WriteLine("=== End of dump ===");
    }
    
    if (o_LogStats) o_LogStats(a1, a2);
}

// ==================== Install ====================

static void InstallFNamePoolHook(uintptr_t libBase) {
    g_libBase = libBase;
    
    // افتح الملف
    if (!OpenOutputFile()) {
        __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "Cannot open output file");
        return;
    }
    
    WriteLine("==========================================");
    WriteLine("FNamePool Dumper - Fortnite 21.30");
    WriteLine("==========================================");
    WriteLine("libBase           = 0x%016lx", libBase);
    WriteLine("LogStats addr     = 0x%016lx", libBase + ADDR_FNamePool_LogStats);
    WriteLine("");
    WriteLine("Waiting for LogStats call...");
    WriteLine("");
    
    void* target = (void*)(libBase + ADDR_FNamePool_LogStats);
    int rc = DobbyHook(target, (void*)Hook_LogStats, (void**)&o_LogStats);
    
    WriteLine("DobbyHook result  = %d", rc);
    WriteLine("original func ptr = %p", (void*)o_LogStats);
    WriteLine("");
    
    if (rc != 0) {
        WriteLine("!!! Hook FAILED !!!");
    } else {
        WriteLine("Hook installed. Waiting for game to call LogStats.");
    }
}
