// ============================================================
// FNamePool Finder — Fortnite 21.30 (UE 5.0.1)
// يبحث عن بصمة "None" + "ByteProperty" بدون null terminators
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

#define LOG_TAG "FNamePool"

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
        if (f) { g_logfile = f; return; }
    }
}

static void LOGF(const char* fmt, ...) {
    char buf[4096];
    va_list ap; va_start(ap, fmt);
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
// Segments (libUnreal.so فقط)
// ============================================================
struct Segment {
    uintptr_t addr;
    size_t    size;
    uint32_t  flags;
};

static std::vector<Segment> GetAllWritableSegments() {
    std::vector<Segment> segs;
    dl_iterate_phdr([](struct dl_phdr_info* ph, size_t, void* data) -> int {
        auto* out = (std::vector<Segment>*)data;
        if (!ph->dlpi_name || !*ph->dlpi_name) return 0;
        if (!strstr(ph->dlpi_name, "libUnreal")) return 0;

        for (int i = 0; i < ph->dlpi_phnum; i++) {
            const auto& p = ph->dlpi_phdr[i];
            if (p.p_type != PT_LOAD) continue;
            if ((p.p_flags & (PF_W | PF_R)) != (PF_W | PF_R)) continue;

            Segment s;
            s.addr  = ph->dlpi_addr + p.p_vaddr;
            s.size  = p.p_memsz;
            s.flags = (uint32_t)p.p_flags;
            out->push_back(s);
        }
        return 1;
    }, &segs);
    return segs;
}

// ============================================================
// Safe read
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
// فحص: هل هذا FNameEntry صالح؟
// يقرأ header ويستخرج الاسم بدون null terminator
// ============================================================
static std::string ReadFNameEntry(uintptr_t entryAddr, bool* ok) {
    *ok = false;
    uint16_t hdr = 0;
    if (!SafeRead(entryAddr, &hdr, 2)) return "";

    // UE5.0 FNameEntry header:
    //   bits 0-5:  unused
    //   bits 6-14: length
    //   bit 15:    bIsWide
    // أو صيغة أخرى:
    //   bit 0: bIsWide, bits 1-15: length

    // جرب الصيغة الأولى
    uint32_t len1 = (hdr >> 6) & 0x1FF;
    bool wide1 = (hdr & 0x8000) != 0;

    // جرب الصيغة الثانية
    uint32_t len2 = (hdr >> 1) & 0x7FFF;
    bool wide2 = (hdr & 1) != 0;

    // اختر واحدة معقولة
    uint32_t len;
    bool wide;
    if (len1 >= 1 && len1 <= 200 && !wide1) {
        len = len1;
        wide = false;
    } else if (len2 >= 1 && len2 <= 200 && !wide2) {
        len = len2;
        wide = false;
    } else {
        return "";
    }

    char buf[256] = {};
    if (!SafeRead(entryAddr + 2, buf, std::min<uint32_t>(len, 250))) return "";

    // تحقق: كل الحروف ASCII قابلة للطباعة
    for (uint32_t i = 0; i < len; i++) {
        char c = buf[i];
        if (c < 0x20 || c > 0x7E) return "";
    }

    *ok = true;
    return std::string(buf, len);
}

// ============================================================
// STEP 1: ابحث عن block0
// بحث عن: "None" ثم "ByteProperty" في نطاق 24 بايت
// ============================================================
struct Block0Candidate {
    uintptr_t addr;
    std::string first;   // "None"
    std::string second;  // "ByteProperty"
};

static std::vector<Block0Candidate> FindBlock0Candidates(
        const std::vector<Segment>& segs) {

    std::vector<Block0Candidate> out;
    const size_t CHUNK = 0x400000; // 4MB
    std::vector<uint8_t> buf(CHUNK + 64);

    // "None" = 4E 6F 6E 65
    // "ByteProperty" = 42 79 74 65 50 72 6F 70 65 72 74 79
    const uint8_t noneBytes[]    = { 0x4E, 0x6F, 0x6E, 0x65 };
    const uint8_t bytePropBytes[] = {
        0x42, 0x79, 0x74, 0x65, 0x50, 0x72, 0x6F, 0x70, 0x65, 0x72, 0x74, 0x79
    };

    for (auto& seg : segs) {
        LOGF("[Find] Scanning segment 0x%lx size=0x%lx",
             seg.addr, seg.size);

        for (size_t off = 0; off < seg.size; off += CHUNK) {
            size_t toRead = std::min(CHUNK + 64, seg.size - off);

            struct iovec l = { buf.data(), toRead };
            struct iovec r = { (void*)(seg.addr + off), toRead };
            ssize_t got = process_vm_readv(getpid(), &l, 1, &r, 1, 0);
            if (got < 32) continue;

            for (ssize_t i = 0; i + 24 < got; i++) {
                // "None" موجود؟
                if (memcmp(buf.data() + i, noneBytes, 4) != 0) continue;

                // "ByteProperty" خلال 24 بايت بعدها؟
                for (ssize_t j = i + 4; j < i + 24 && j + 12 <= got; j++) {
                    if (memcmp(buf.data() + j, bytePropBytes, 12) != 0)
                        continue;

                    // 🎯 وجدنا بصمة block0!
                    // الموضع i = نهاية "None"، إذن block0 يبدأ قبله:
                    // header(2 bytes) + name(4 bytes) → header عند i - 2
                    uintptr_t block0Addr = seg.addr + off + i - 2;

                    Block0Candidate c;
                    c.addr   = block0Addr;
                    c.first  = "None";
                    c.second = "ByteProperty";
                    out.push_back(c);

                    LOGF("[Find] Candidate @ 0x%lx (offset in block0: +0x%lx)",
                         block0Addr, (uintptr_t)i - 2);
                    break;
                }
            }
        }
    }

    return out;
}

// ============================================================
// STEP 2: ابحث عن pointer إلى block0
// ============================================================
struct PoolCandidate {
    uintptr_t poolAddr;
    uintptr_t ptrLoc;
    uintptr_t blocksOffset;
};

static std::vector<PoolCandidate> FindPoolFromBlock0(
        const std::vector<Segment>& segs,
        uintptr_t block0Addr) {

    std::vector<PoolCandidate> out;
    const size_t CHUNK = 0x400000;
    std::vector<uint8_t> buf(CHUNK + 8);

    // جرّب offsets محتملة لـ Blocks[] داخل FNamePool
    const uint32_t possibleBlocksOffsets[] = {
        0x40,    // بعد CurrentBlock مباشرة
        0x50,    // محاذاة 16
        0x7C,    // من تحليل assembly الأول
        0x80,    // محاذاة 128
        0x100,   // محاذاة 256
        0x1000,
        0x10000,
        0x10040, // 0x40 + 8192*8
        0x10080, // 0x80 + 8192*8
    };

    for (auto& seg : segs) {
        for (size_t off = 0; off < seg.size; off += CHUNK) {
            size_t toRead = std::min(CHUNK + 8, seg.size - off);

            struct iovec l = { buf.data(), toRead };
            struct iovec r = { (void*)(seg.addr + off), toRead };
            ssize_t got = process_vm_readv(getpid(), &l, 1, &r, 1, 0);
            if (got < 8) continue;

            for (ssize_t i = 0; i + 8 <= got; i += 8) {
                uint64_t val = *(uint64_t*)(buf.data() + i);
                if (val != block0Addr) continue;

                uintptr_t ptrLoc = seg.addr + off + i;

                for (uint32_t blkOff : possibleBlocksOffsets) {
                    if (ptrLoc < blkOff) continue;
                    uintptr_t pool = ptrLoc - blkOff;

                    // تحقق: CurrentBlock @ +0x38
                    uint32_t cb = 0;
                    if (!SafeRead(pool + 0x38, &cb, 4)) continue;
                    if (cb > 200 || cb == 0) continue;

                    // تحقق: block1 @ pool + blkOff + 8
                    uint64_t block1 = 0;
                    if (!SafeRead(pool + blkOff + 8, &block1, 8)) continue;
                    if (!IsValidHeapPtr(block1)) continue;

                    // block1 يجب يحتوي أسماء صالحة
                    bool ok = false;
                    std::string name1 = ReadFNameEntry(block1, &ok);
                    if (!ok || name1.empty()) continue;

                    PoolCandidate pc;
                    pc.poolAddr     = pool;
                    pc.ptrLoc       = ptrLoc;
                    pc.blocksOffset = blkOff;
                    out.push_back(pc);

                    LOGF("[Find] Pool candidate @ 0x%lx "
                         "(ptr @ 0x%lx, blocks_off=0x%x, cb=%u, block1_name=\"%s\")",
                         pool, ptrLoc, blkOff, cb, name1.c_str());
                }
            }
        }
    }

    return out;
}

// ============================================================
// STEP 3: eقراءة أسماء
// ============================================================
static std::string ReadFName(uintptr_t pool, uint32_t blocksOff, uint32_t id) {
    uint32_t blockIdx      = id >> 16;
    uint32_t offsetInBlock = id & 0xFFFF;

    uint64_t blockPtr = 0;
    if (!SafeRead(pool + blocksOff + blockIdx * 8, &blockPtr, 8)) return "<?>";
    if (!IsValidHeapPtr(blockPtr)) return "<?>";

    // stride = 2 (UE5.0.1)
    uintptr_t entry = blockPtr + offsetInBlock * 2;

    bool ok = false;
    std::string s = ReadFNameEntry(entry, &ok);
    return ok ? s : "<?>";
}

// ============================================================
// Main
// ============================================================
static void Run() {
    // انتظر libUnreal
    LibInfo:
    uintptr_t base = 0;
    for (int i = 0; i < 60; i++) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        dl_iterate_phdr([](struct dl_phdr_info* ph, size_t, void* d) -> int {
            auto* out = (uintptr_t*)d;
            if (!ph->dlpi_name || !*ph->dlpi_name) return 0;
            if (!strstr(ph->dlpi_name, "libUnreal")) return 0;
            *out = ph->dlpi_addr;
            return 1;
        }, &base);
        if (base) break;
    }
    if (!base) { LOGF("[ERR] libUnreal not found"); return; }
    g_base = base;

    LOGF("========================================");
    LOGF("FNamePool Finder — Fortnite 21.30");
    LOGF("libBase = 0x%lx", g_base);
    LOGF("========================================");

    LOGF("Waiting 20s for game to load...");
    std::this_thread::sleep_for(std::chrono::seconds(20));

    auto segs = GetAllWritableSegments();
    LOGF("Writable segments: %zu", segs.size());
    for (auto& s : segs)
        LOGF("  addr=0x%lx size=0x%lx flags=0x%x", s.addr, s.size, s.flags);
    LOGF("");

    // STEP 1
    LOGF("STEP 1: Search for block0 signature (\"None\"+\"ByteProperty\")...");
    auto block0s = FindBlock0Candidates(segs);
    if (block0s.empty()) {
        LOGF("!!! No block0 candidates found !!!");
        return;
    }
    LOGF("Found %zu block0 candidates.", block0s.size());
    for (auto& b : block0s)
        LOGF("  block0 @ 0x%lx", b.addr);
    LOGF("");

    // STEP 2
    LOGF("STEP 2: Search for FNamePool (pointer to block0)...");
    for (auto& b : block0s) {
        LOGF("  Trying block0 = 0x%lx", b.addr);
        auto pools = FindPoolFromBlock0(segs, b.addr);

        if (pools.empty()) {
            LOGF("    No pool pointer found for this block0.");
            continue;
        }

        LOGF("");
        LOGF("##########################################");
        LOGF("###     *** FNamePool FOUND! ***      ###");
        LOGF("##########################################");

        for (auto& p : pools) {
            LOGF("FNamePool    @ 0x%lx", p.poolAddr);
            LOGF("  Offset     = 0x%lx   <- SAVE THIS!",
                 p.poolAddr - g_base);
            LOGF("  Blocks[]   = +0x%x", p.blocksOffset);
            LOGF("  Pointer at = 0x%lx", p.ptrLoc);

            LOGF("");
            LOGF("--- First 30 FNames (using blocks_off=0x%x) ---",
                 p.blocksOffset);
            for (uint32_t i = 0; i < 30; i++) {
                LOGF("[%2u] = %s",
                     i, ReadFName(p.poolAddr, p.blocksOffset, i).c_str());
            }
            LOGF("");
        }
    }

    LOGF("=== DONE ===");
}

// ============================================================
// JNI
// ============================================================
extern "C" JNIEXPORT jint JNICALL
JNI_OnLoad(JavaVM* vm, void* reserved) {
    InitLogFile();
    LOGF("========================================");
    LOGF("FNamePool Finder %s %s", __DATE__, __TIME__);
    LOGF("PID: %d", getpid());
    LOGF("========================================");

    std::thread([]{
        try { Run(); }
        catch (...) { LOGF("[ERR] exception in Run"); }
    }).detach();

    return JNI_VERSION_1_6;
}
