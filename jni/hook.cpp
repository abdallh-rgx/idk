// ============================================================
// FNamePool Scanner — Fortnite 21.30 (UE 5.0.1)
// Sig Scan للذاكرة — لا يحتاج hooks
// ============================================================
#include <jni.h>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cstdarg>
#include <cerrno>
#include <string>
#include <vector>
#include <thread>
#include <chrono>
#include <mutex>
#include <algorithm>
#include <dlfcn.h>
#include <link.h>
#include <android/log.h>
#include <sys/uio.h>
#include <unistd.h>
#include "dobby.h"

#define LOG_TAG "FNamePool"

// ============================================================
// Globals
// ============================================================
static FILE* g_logfile = nullptr;
static std::mutex g_mutex;
static uintptr_t g_base = 0;

// ============================================================
// Logger
// ============================================================
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
    __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "[FILE] all paths failed — logcat only");
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

// ============================================================
// LibInfo
// ============================================================
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

// ============================================================
// Segment Scanner  (مصحح: flags نوعه uint32_t)
// ============================================================
struct Segment {
    uintptr_t addr;
    size_t    size;
    uint32_t  flags;
};

static std::vector<Segment> GetWritableSegments() {
    std::vector<Segment> segs;
    dl_iterate_phdr([](struct dl_phdr_info* ph, size_t, void* data) -> int {
        auto* out = (std::vector<Segment>*)data;
        if (!ph->dlpi_name || !*ph->dlpi_name) return 0;
        if (!strstr(ph->dlpi_name, "libUnreal")) return 0;

        for (int i = 0; i < ph->dlpi_phnum; i++) {
            const auto& p = ph->dlpi_phdr[i];
            if (p.p_type != PT_LOAD) continue;

            // writable + readable
            if ((p.p_flags & (PF_W | PF_R)) == (PF_W | PF_R)) {
                Segment s;
                s.addr  = ph->dlpi_addr + p.p_vaddr;
                s.size  = p.p_memsz;
                s.flags = (uint32_t)p.p_flags;
                out->push_back(s);
            }
        }
        return 1;
    }, &segs);
    return segs;
}

// ============================================================
// Safe Read Helpers
// ============================================================
static bool IsValidHeapPtr(uint64_t p) {
    return p >= 0x1000000000ULL && p <= 0x8000000000ULL;
}

static bool SafeRead(uintptr_t addr, void* buf, size_t n) {
    struct iovec l = { buf, n };
    struct iovec r = { (void*)addr, n };
    return process_vm_readv(getpid(), &l, 1, &r, 1, 0) == (ssize_t)n;
}

// ============================================================
// FName Reader (لـ التحقق من صحة FNamePool)
// ============================================================
static std::string ReadFNameFromPool(uintptr_t pool, uint32_t nameId) {
    uint32_t blockIdx      = nameId >> 16;
    uint32_t offsetInBlock = nameId & 0xFFFF;
    if (blockIdx >= 8192) return "<oob>";

    uint64_t blockPtr = 0;
    if (!SafeRead(pool + 0x7C + blockIdx * 8, &blockPtr, 8)) return "<rdfail>";
    if (!IsValidHeapPtr(blockPtr)) return "<null>";

    uintptr_t entryPtr = blockPtr + offsetInBlock * 2;

    uint16_t header = 0;
    if (!SafeRead(entryPtr, &header, 2)) return "<rd>";

    uint32_t len    = (header >> 6) & 0x3FF;
    bool     isWide = (header & 0x8000) != 0;
    if (len == 0 || len > 1023) return "<badlen>";

    char buf[1024] = {};
    if (isWide) {
        char16_t wide[512] = {};
        uint32_t nBytes = len * 2;
        if (nBytes > sizeof(wide) - 2) nBytes = sizeof(wide) - 2;
        if (!SafeRead(entryPtr + 2, wide, nBytes)) return "<rd>";
        for (uint32_t i = 0; i < len; i++) buf[i] = (char)(wide[i] & 0x7F);
    } else {
        uint32_t nBytes = len;
        if (nBytes > sizeof(buf) - 1) nBytes = sizeof(buf) - 1;
        if (!SafeRead(entryPtr + 2, buf, nBytes)) return "<rd>";
    }
    return std::string(buf, len);
}

// ============================================================
// Validator: هل هذا FNamePool حقيقي؟
// ============================================================
static bool ValidateFNamePool(uintptr_t candidate) {
    // 1) CurrentBlock منطقي؟
    uint32_t cb = 0;
    if (!SafeRead(candidate + 0x38, &cb, 4)) return false;
    if (cb > 200) return false;

    // 2) البلوك الأول صالح؟
    uint64_t block0 = 0;
    if (!SafeRead(candidate + 0x7C, &block0, 8)) return false;
    if (!IsValidHeapPtr(block0)) return false;

    // 3) أول 5 entries يجب أن تكون معروفة
    static const char* expected[] = {
        "None", "ByteProperty", "IntProperty", "BoolProperty", "FloatProperty"
    };

    for (int i = 0; i < 5; i++) {
        std::string name = ReadFNameFromPool(candidate, i);
        if (name != expected[i]) return false;
    }

    return true;
}

// ============================================================
// Scanner الرئيسي
// ============================================================
static uintptr_t ScanForFNamePool() {
    LOGF("");
    LOGF("========================================");
    LOGF("Starting memory scan for FNamePool...");
    LOGF("========================================");

    auto segs = GetWritableSegments();
    LOGF("Found %zu writable segments", segs.size());

    const size_t CHUNK = 0x100000;  // 1MB
    std::vector<uint8_t> buf(CHUNK);

    size_t totalScanned = 0;
    auto startTime = std::chrono::steady_clock::now();

    for (auto& seg : segs) {
        LOGF("");
        LOGF("Segment: addr=0x%lx size=0x%lx flags=0x%x",
             seg.addr, seg.size, seg.flags);

        for (size_t off = 0; off < seg.size; off += CHUNK) {
            size_t toRead = std::min(CHUNK, seg.size - off);

            struct iovec l = { buf.data(), toRead };
            struct iovec r = { (void*)(seg.addr + off), toRead };
            ssize_t got = process_vm_readv(getpid(), &l, 1, &r, 1, 0);

            if (got < 0x84) continue;
            totalScanned += got;

            // امسح كل 8 بايت
            for (size_t i = 0; i + 0x84 <= (size_t)got; i += 8) {
                uintptr_t candidate = seg.addr + off + i;

                // فحص سريع 1: CurrentBlock @ +0x38
                uint32_t cb = *(uint32_t*)(buf.data() + i + 0x38);
                if (cb > 200) continue;

                // فحص سريع 2: block ptr @ +0x7C
                uint64_t blockPtr = *(uint64_t*)(buf.data() + i + 0x7C);
                if (!IsValidHeapPtr(blockPtr)) continue;

                // فحص سريع 3: أول entry = "None"?
                uint8_t entry[16];
                if (!SafeRead(blockPtr, entry, 16)) continue;

                uint16_t header = *(uint16_t*)entry;
                uint32_t len    = (header >> 6) & 0x3FF;
                bool isWide     = (header & 0x8000) != 0;

                if (isWide || len != 4) continue;
                if (entry[2] != 'N' || entry[3] != 'o' ||
                    entry[4] != 'n' || entry[5] != 'e') continue;

                // 🎯 مرشح قوي — افحص بعمق
                LOGF("");
                LOGF("Candidate @ 0x%lx (cb=%u block0=0x%lx)",
                     candidate, cb, blockPtr);

                if (ValidateFNamePool(candidate)) {
                    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
                        std::chrono::steady_clock::now() - startTime).count();

                    LOGF("");
                    LOGF("##########################################");
                    LOGF("###  *** FNamePool FOUND! ***         ###");
                    LOGF("##########################################");
                    LOGF("FNamePool    @ 0x%lx", candidate);
                    LOGF("libBase      @ 0x%lx", g_base);
                    LOGF("Offset       = 0x%lx   <- SAVE THIS!", candidate - g_base);
                    LOGF("CurrentBlock = %u", cb);
                    LOGF("Scanned      = %.1f MB in %lds",
                         totalScanned / (1024.0 * 1024.0), (long)elapsed);
                    LOGF("##########################################");
                    LOGF("");

                    // اطبع أول 30 entry للتأكد
                    LOGF("--- First 30 FNames ---");
                    for (uint32_t j = 0; j < 30; j++) {
                        LOGF("[%2u] = %s", j, ReadFNameFromPool(candidate, j).c_str());
                    }

                    return candidate;
                }
            }
        }
    }

    LOGF("");
    LOGF("!!! FNamePool NOT FOUND in writable segments !!!");
    LOGF("Scanned: %.1f MB", totalScanned / (1024.0 * 1024.0));
    return 0;
}

// ============================================================
// Main
// ============================================================
static void Run() {
    // انتظر libUnreal.so
    LibInfo info{0,0,0};
    for (int i = 0; i < 60; i++) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        info = GetLibInfo();
        if (info.base) break;
    }
    if (!info.base) {
        LOGF("[ERR] libUnreal not found");
        return;
    }
    g_base = info.base;

    LOGF("========================================");
    LOGF("FNamePool Scanner — Fortnite 21.30");
    LOGF("libBase = 0x%lx", g_base);
    LOGF("========================================");

    // انتظر شوي عشان اللعبة تحمّل كل شي
    LOGF("Waiting 15s for game to fully load...");
    std::this_thread::sleep_for(std::chrono::seconds(15));

    // ابدأ الـ scan
    uintptr_t pool = ScanForFNamePool();

    if (pool) {
        LOGF("");
        LOGF("=== DONE ===");
        LOGF("Use: g_FNamePool = libBase + 0x%lx;", pool - g_base);
    } else {
        LOGF("");
        LOGF("=== FAILED ===");
        LOGF("Try:");
        LOGF("  1. Increase wait time");
        LOGF("  2. Scan more segments");
        LOGF("  3. Use different validation");
    }
}

// ============================================================
// JNI
// ============================================================
extern "C" JNIEXPORT jint JNICALL
JNI_OnLoad(JavaVM* vm, void* reserved) {
    InitLogFile();
    LOGF("========================================");
    LOGF("FNamePool Scanner %s %s", __DATE__, __TIME__);
    LOGF("PID: %d", getpid());
    LOGF("========================================");

    std::thread([]{
        try { Run(); }
        catch (...) { LOGF("[ERR] exception in Run"); }
    }).detach();

    return JNI_VERSION_1_6;
}
