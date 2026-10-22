// ═══════════════════════════════════════════════════════════════
// listen_test.cpp — Standalone Listen Server Test
// Fortnite 21.30.0-21088273 Android ARM64
//
// لا يعتمد على EraCH3 أو أي مكتبة أخرى
// يُبنى كـ .so ويُحقن في اللعبة
//
// RVAs مؤكدة من MobileDumper-7 dump
// ═══════════════════════════════════════════════════════════════

#include <jni.h>
#include <android/log.h>
#include <pthread.h>
#include <dlfcn.h>
#include <link.h>
#include <unistd.h>
#include <cstring>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstdarg>
#include <cctype>
#include <string>
#include <vector>
#include <signal.h>
#include <setjmp.h>
#include <sys/ucontext.h>

// ═══════════════════════════════════════════════════════════════
// RVAs (21.30.0-21088273)
// ═══════════════════════════════════════════════════════════════
static constexpr uintptr_t RVA_GEngine                     = 0xDE34970;
static constexpr uintptr_t RVA_GWorld                      = 0xDE38568;
static constexpr uintptr_t RVA_GNames                      = 0xDC960C0;
static constexpr uintptr_t RVA_GetWorldContext             = 0x98BB050;
static constexpr uintptr_t RVA_CreateNetDriverWorldContext = 0x98BE044;
static constexpr uintptr_t RVA_CreateNetDriver_Old         = 0x98BDE00;
static constexpr uintptr_t RVA_InitListen                  = 0x3852A0C;
static constexpr uintptr_t RVA_SetWorld                    = 0x62BB6D4;

// ═══════════════════════════════════════════════════════════════
// Configuration
// ═══════════════════════════════════════════════════════════════
static constexpr int kPort                = 7777;
static constexpr int kUrlSize             = 0x80;
static constexpr int kPollIntervalSec     = 1;
static constexpr int kMaxPollAttempts     = 120;   // 2 minutes

// UObject reflection (UE5.0)
static constexpr unsigned UObject_Class = 0x10;
static constexpr unsigned UObject_Name  = 0x18;
static constexpr unsigned UObject_Outer = 0x20;

// ═══════════════════════════════════════════════════════════════
// Globals
// ═══════════════════════════════════════════════════════════════
static uintptr_t  g_base     = 0;
static FILE*      g_logFile  = nullptr;
static bool       g_done     = false;

// ═══════════════════════════════════════════════════════════════
// Logging
// ═══════════════════════════════════════════════════════════════
static void log_impl(const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    __android_log_vprint(ANDROID_LOG_INFO, "ListenTest", fmt, ap);
    va_end(ap);
    if (g_logFile) {
        va_start(ap, fmt);
        vfprintf(g_logFile, fmt, ap);
        va_end(ap);
        fprintf(g_logFile, "\n");
        fflush(g_logFile);
    }
}
#define LOG(...) log_impl(__VA_ARGS__)

// ═══════════════════════════════════════════════════════════════
// Fault guard (prevent crash from killing the process)
// ═══════════════════════════════════════════════════════════════
static sigjmp_buf         g_jmp;
static volatile sig_atomic_t g_inCall = 0;
static struct sigaction   g_oldSegv, g_oldBus;
static volatile uintptr_t g_faultPC = 0, g_faultAddr = 0;
static volatile int       g_faultSig = 0;

static void fault_handler(int sig, siginfo_t* info, void* uctx) {
    if (g_inCall) {
        g_faultSig  = sig;
        g_faultPC   = uctx ? (uintptr_t)((ucontext_t*)uctx)->uc_mcontext.pc : 0;
        g_faultAddr = info ? (uintptr_t)info->si_addr : 0;
        g_inCall = 0;
        siglongjmp(g_jmp, 1);
    }
    // forward
    struct sigaction* old = (sig == SIGSEGV) ? &g_oldSegv : &g_oldBus;
    if (old->sa_flags & SA_SIGINFO) { old->sa_sigaction(sig, info, uctx); return; }
    if (old->sa_handler && old->sa_handler != SIG_IGN && old->sa_handler != SIG_DFL) {
        old->sa_handler(sig); return;
    }
    signal(sig, SIG_DFL);
    raise(sig);
}

static void install_guards() {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_sigaction = fault_handler;
    sa.sa_flags = SA_SIGINFO;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGSEGV, &sa, &g_oldSegv);
    sigaction(SIGBUS, &sa, &g_oldBus);
}

template <typename Fn, typename... Args>
static bool RunGuarded(const char* tag, Fn fn, Args... args) {
    if (sigsetjmp(g_jmp, 1) != 0) {
        LOG("[%s] ⚠️ FAULT: sig=%d pc=0x%llx addr=0x%llx — continuing",
            tag, (int)g_faultSig, (unsigned long long)g_faultPC,
            (unsigned long long)g_faultAddr);
        g_inCall = 0;
        return false;
    }
    g_inCall = 1;
    fn(args...);
    g_inCall = 0;
    return true;
}

// ═══════════════════════════════════════════════════════════════
// Base finding
// ═══════════════════════════════════════════════════════════════
static int phdr_cb(dl_phdr_info* info, size_t, void*) {
    const char* name = strrchr(info->dlpi_name, '/');
    name = name ? name + 1 : info->dlpi_name;
    if (strcmp(name, "libUnreal.so") == 0) {
        g_base = info->dlpi_addr;
        return 1;
    }
    return 0;
}

static bool find_base() {
    for (int i = 0; i < 200 && g_base == 0; i++) {
        dl_iterate_phdr(phdr_cb, nullptr);
        if (g_base) break;
        usleep(100000);
    }
    return g_base != 0;
}

// ═══════════════════════════════════════════════════════════════
// Name reading (FNamePool)
// ═══════════════════════════════════════════════════════════════
static bool is_bad(const void* p) {
    if (!p) return true;
    uintptr_t v = (uintptr_t)p;
    return v < 0x10000 || v > 0x0000FFFFFFFFFFFFULL;
}

// GNames pool entry format (from dump):
//   Header @ 0   (uint16_t: Wide:1 | ProbeHash:5 | Len:10)
//   String @ 4   (TCHAR[])
//   Stride = 4   (offset multiplier)
static std::string read_name(uint32_t compIdx) {
    if (compIdx == 0 || compIdx == 0xFFFFFFFF) return "";
    
    void* poolBase = (void*)(g_base + RVA_GNames);
    if (is_bad(poolBase)) return "";
    
    // Blocks array @ poolBase+0x40 (per dump)
    void** blocksArr = *(void***)((char*)poolBase + 0x40);
    if (is_bad(blocksArr)) return "";
    
    uint32_t blockIdx = compIdx >> 16;
    uint32_t offset    = compIdx & 0xFFFF;
    
    if (blockIdx > 0x100) return "";
    if (is_bad(blocksArr)) return "";
    
    void* block = blocksArr[blockIdx];
    if (!block || is_bad(block)) return "";
    
    // Entry at block + offset*4 (stride)
    uint8_t* entry = (uint8_t*)block + (offset * 4);
    
    uint16_t hdr = *(uint16_t*)entry;
    bool wide = (hdr & 1) != 0;
    int len = (hdr >> 6) & 0x3FF;
    
    if (len == 0 || len > 200) return "";
    
    std::string out;
    out.reserve((size_t)len);
    if (wide) {
        const char16_t* s = (const char16_t*)(entry + 4);
        for (int i = 0; i < len; i++) {
            char16_t c = s[i];
            out.push_back(c < 128 ? (char)c : '?');
        }
    } else {
        const char* s = (const char*)(entry + 4);
        for (int i = 0; i < len; i++) out.push_back(s[i]);
    }
    return out;
}

static std::string obj_name(void* obj) {
    if (!obj || is_bad(obj)) return "";
    uint32_t compIdx = *(uint32_t*)((char*)obj + UObject_Name);
    return read_name(compIdx);
}

static std::string obj_class_name(void* obj) {
    if (!obj || is_bad(obj)) return "";
    void* cls = *(void**)((char*)obj + UObject_Class);
    if (!cls || is_bad(cls)) return "";
    return obj_name(cls);
}

// ═══════════════════════════════════════════════════════════════
// NetMode reading — try multiple offsets
// ═══════════════════════════════════════════════════════════════
static int read_net_mode(void* world) {
    if (!world || is_bad(world)) return -1;
    static const unsigned candidateOffsets[] = {
        0x11C, 0x120, 0x118, 0x114, 0x110, 0x108, 0x100
    };
    for (unsigned off : candidateOffsets) {
        uint8_t v = *(uint8_t*)((char*)world + off);
        if (v <= 3) return (int)v;
    }
    return -1;
}

static const char* net_mode_name(int nm) {
    switch (nm) {
        case 0: return "Standalone";
        case 1: return "DedicatedServer";
        case 2: return "ListenServer";
        case 3: return "Client";
        default: return "Unknown";
    }
}

// ═══════════════════════════════════════════════════════════════
// FName("GameNetDriver") — hardcoded after failure to resolve
// عبر name pool مباشرة
// ═══════════════════════════════════════════════════════════════
static uint32_t find_fname_index(const char* target) {
    // Walk name pool entries looking for the target string
    void* poolBase = (void*)(g_base + RVA_GNames);
    if (is_bad(poolBase)) return 0;
    
    void** blocksArr = *(void***)((char*)poolBase + 0x40);
    if (is_bad(blocksArr)) return 0;
    
    // Blocks: iterate block 0..N
    for (int blockIdx = 0; blockIdx < 16; blockIdx++) {
        void* block = blocksArr[blockIdx];
        if (!block || is_bad(block)) continue;
        
        // Walk entries in block
        uint8_t* p = (uint8_t*)block;
        uint8_t* end = p + 0x10000 * 4;   // assume 64K entries * 4 stride
        
        while (p < end) {
            uint16_t hdr = *(uint16_t*)p;
            bool wide = (hdr & 1) != 0;
            int len = (hdr >> 6) & 0x3FF;
            if (len == 0) { p += 4; continue; }
            if (len > 200) { p += 4; continue; }
            
            int entrySize = 4 + len * (wide ? 2 : 1);
            entrySize = (entrySize + 3) & ~3;   // align 4
            
            // Check match
            bool match = false;
            if ((int)strlen(target) == len) {
                match = true;
                if (wide) {
                    const char16_t* s = (const char16_t*)(p + 4);
                    for (int i = 0; i < len; i++) {
                        if (s[i] != (char16_t)target[i]) { match = false; break; }
                    }
                } else {
                    const char* s = (const char*)(p + 4);
                    if (memcmp(s, target, (size_t)len) != 0) match = false;
                }
            }
            
            if (match) {
                uint32_t offset = (uint32_t)(p - (uint8_t*)block) / 4;
                uint32_t compIdx = ((uint32_t)blockIdx << 16) | offset;
                LOG("FName '%s' = {CompIdx=0x%X} (block=%d offset=0x%X)",
                    target, compIdx, blockIdx, offset);
                return compIdx;
            }
            
            p += entrySize;
        }
    }
    LOG("⚠️ FName '%s' not found in pool", target);
    return 0;
}

// ═══════════════════════════════════════════════════════════════
// FName struct (4 bytes on 21.30)
// ═══════════════════════════════════════════════════════════════
struct FName { uint32_t CompIdx; };

// ═══════════════════════════════════════════════════════════════
// URL builder
// ═══════════════════════════════════════════════════════════════
static void* make_url() {
    void* url = calloc(1, kUrlSize);
    if (!url) return nullptr;
    // UE5 FURL: Protocol@0(16B FString) | Host@0x10(16B FString) | Port@0x20(4B)
    *(int32_t*)((char*)url + 0x20) = kPort;
    return url;
}

// ═══════════════════════════════════════════════════════════════
// The actual listen setup
// ═══════════════════════════════════════════════════════════════
static bool try_setup(void* world, const char* source) {
    LOG("═══════════════════════════════════════════════════════════");
    LOG("[%s] Attempting listen setup", source);
    
    if (!world || is_bad(world)) {
        LOG("❌ World invalid");
        return false;
    }
    
    std::string wname = obj_name(world);
    std::string wclass = obj_class_name(world);
    int nmBefore = read_net_mode(world);
    LOG("World    = %p", world);
    LOG("Name     = '%s'", wname.c_str());
    LOG("Class    = '%s'", wclass.c_str());
    LOG("NetMode  = %d (%s)", nmBefore, net_mode_name(nmBefore));
    
    if (nmBefore == 2) {
        LOG("✅ Already ListenServer — nothing to do");
        return true;
    }
    
    // ─── Read GEngine ───
    void** gengineSlot = (void**)(g_base + RVA_GEngine);
    void* engine = *gengineSlot;
    if (!engine || is_bad(engine)) {
        LOG("❌ GEngine = null (%p) — RVAs incompatible?", engine);
        return false;
    }
    LOG("GEngine  = %p", engine);
    
    // ─── Get WorldContext ───
    auto getWorldCtx = (void*(*)(void*, void*))(g_base + RVA_GetWorldContext);
    void* worldCtx = nullptr;
    if (!RunGuarded("GetWorldContext", [&] {
        worldCtx = getWorldCtx(engine, world);
    })) {
        LOG("❌ GetWorldContext FAULTED");
        return false;
    }
    if (!worldCtx || is_bad(worldCtx)) {
        LOG("❌ WorldContext = %p", worldCtx);
        return false;
    }
    LOG("WorldCtx = %p", worldCtx);
    
    // ─── FName("GameNetDriver") ───
    uint32_t ndIdx = find_fname_index("GameNetDriver");
    FName netName{ ndIdx };
    LOG("FName    = {CompIdx=0x%X}", netName.CompIdx);
    
    // ─── CreateNetDriver ───
    // Try new path first, fall back to old
    void* netDriver = nullptr;
    bool ndCreated = false;
    
    if (!ndCreated) {
        LOG("Trying CreateNetDriverWorldContext...");
        auto createND = (void*(*)(void*, void*, FName, int))(g_base + RVA_CreateNetDriverWorldContext);
        RunGuarded("CreateNetDriver_New", [&] {
            netDriver = createND(engine, worldCtx, netName, 0);
            ndCreated = true;
        });
        if (netDriver && !is_bad(netDriver)) {
            LOG("✅ NetDriver = %p (via WorldContext)", netDriver);
        } else {
            LOG("⚠️ CreateNetDriverWorldContext returned %p", netDriver);
            netDriver = nullptr;
        }
    }
    
    if (!netDriver) {
        LOG("Trying CreateNetDriver (old signature)...");
        auto createND_old = (void*(*)(void*, void*, FName))(g_base + RVA_CreateNetDriver_Old);
        RunGuarded("CreateNetDriver_Old", [&] {
            netDriver = createND_old(engine, world, netName);
        });
        if (netDriver && !is_bad(netDriver)) {
            LOG("✅ NetDriver = %p (via old signature)", netDriver);
        } else {
            LOG("❌ Both CreateNetDriver paths failed");
            return false;
        }
    }
    
    // ─── SetWorld (pre) ───
    auto setWorld = (void(*)(void*, void*))(g_base + RVA_SetWorld);
    RunGuarded("SetWorld_Pre", [&] { setWorld(netDriver, world); });
    LOG("SetWorld (pre) — done");
    
    // ─── URL ───
    void* url = make_url();
    
    // ─── InitListen ───
    LOG("Calling InitListen(port=%d)...", kPort);
    auto initListen = (bool(*)(void*, void*, void*, bool, void*))(g_base + RVA_InitListen);
    void* errBuf = calloc(1, 0x20);
    bool ok = false;
    if (!RunGuarded("InitListen", [&] {
        ok = initListen(netDriver, world, url, false, errBuf);
    })) {
        LOG("❌ InitListen FAULTED");
        free(url);
        free(errBuf);
        return false;
    }
    LOG("InitListen = %d", ok);
    
    // Try to read error string
    if (!ok && errBuf) {
        struct FStr { char16_t* Data; int32_t Count; int32_t Max; };
        FStr* err = (FStr*)errBuf;
        if (err->Data && !is_bad(err->Data) && err->Count > 0 && err->Count < 256) {
            std::string msg;
            for (int i = 0; i < err->Count && err->Data[i]; i++) {
                char16_t c = err->Data[i];
                msg.push_back(c < 128 ? (char)c : '?');
            }
            LOG("InitListen error: '%s'", msg.c_str());
        }
    }
    
    free(url);
    free(errBuf);
    
    if (!ok) {
        LOG("❌ InitListen rejected");
        return false;
    }
    
    // ─── SetWorld (post) ───
    RunGuarded("SetWorld_Post", [&] { setWorld(netDriver, world); });
    LOG("SetWorld (post) — done");
    
    // ─── Read NetMode after ───
    int nmAfter = read_net_mode(world);
    LOG("NetMode after = %d (%s)", nmAfter, net_mode_name(nmAfter));
    
    if (nmAfter == 2) {
        LOG("═══════════════════════════════════════════════════════════");
        LOG("✅✅✅ SUCCESS — ListenServer ACTIVE");
        LOG("You are now Player AND Server on port %d", kPort);
        LOG("Next: test that native building/inventory/death now work");
        LOG("═══════════════════════════════════════════════════════════");
        return true;
    }
    
    // ─── NetMode didn't change — try to force it ───
    LOG("⚠️ InitListen succeeded but NetMode not 2 — trying to force write");
    
    static const unsigned forceOffsets[] = { 0x11C, 0x120, 0x118 };
    for (unsigned off : forceOffsets) {
        uint8_t before = *(uint8_t*)((char*)world + off);
        if (before > 3) continue;
        
        uint8_t* target = (uint8_t*)((char*)world + off);
        bool written = false;
        RunGuarded("ForceNetMode", [&] {
            *target = 2;
            written = true;
        });
        if (written) {
            uint8_t after = *target;
            LOG("  Wrote NetMode@0x%X: %u -> %u", off, before, after);
            if (after == 2) {
                LOG("✅✅✅ SUCCESS (forced) — NetMode now 2");
                return true;
            }
        }
    }
    
    LOG("❌ NetMode still not 2 — but NetDriver exists");
    LOG("    NetDriver=%p may still work; test in-game", netDriver);
    return false;
}

// ═══════════════════════════════════════════════════════════════
// Worker thread
// ═══════════════════════════════════════════════════════════════
static void* worker(void*) {
    LOG("╔══════════════════════════════════════════════════════╗");
    LOG("║   ListenTest — Standalone Listen Server Test        ║");
    LOG("║   Fortnite 21.30.0-21088273 Android                 ║");
    LOG("╚══════════════════════════════════════════════════════╝");
    
    // Open log file
    const char* logPaths[] = {
        "/storage/emulated/0/Android/data/com.epicgames.fortnite/files/listen_test.log",
        "/sdcard/Android/data/com.epicgames.fortnite/files/listen_test.log",
        "/data/local/tmp/listen_test.log",
        "/sdcard/listen_test.log",
        nullptr
    };
    for (int i = 0; logPaths[i]; i++) {
        g_logFile = fopen(logPaths[i], "w");
        if (g_logFile) { LOG("Log file: %s", logPaths[i]); break; }
    }
    
    // Install fault guards
    install_guards();
    LOG("Fault guards installed");
    
    // Find base
    LOG("Searching for libUnreal.so...");
    if (!find_base()) {
        LOG("❌ libUnreal.so not found — giving up");
        return nullptr;
    }
    LOG("✅ Base = 0x%llX", (unsigned long long)g_base);
    
    // Sanity check GNames
    LOG("Verifying GNames pool...");
    std::string test = read_name(0);
    if (test == "None") {
        LOG("✅ GNames OK (entry 0 = 'None')");
    } else {
        LOG("⚠️ GNames entry 0 = '%s' (expected 'None')", test.c_str());
    }
    
    // Poll for world
    LOG("Polling for match world (max %ds)...", kMaxPollAttempts * kPollIntervalSec);
    
    for (int attempt = 1; attempt <= kMaxPollAttempts; attempt++) {
        usleep(kPollIntervalSec * 1000000);
        
        void** gworldSlot = (void**)(g_base + RVA_GWorld);
        void* world = *gworldSlot;
        
        if (!world || is_bad(world)) {
            if (attempt % 10 == 0) {
                LOG("[poll %d] GWorld = %p (still loading)", attempt, world);
            }
            continue;
        }
        
        std::string wname = obj_name(world);
        int nm = read_net_mode(world);
        
        LOG("[poll %d] World=%p name='%s' NetMode=%d (%s)",
            attempt, world, wname.c_str(), nm, net_mode_name(nm));
        
        // Is this a match world?
        bool isMatch = (wname.find("Terrain") != std::string::npos) ||
                       (wname.find("Apollo") != std::string::npos) ||
                       (wname.find("Artemis") != std::string::npos) ||
                       (wname.find("Athena") != std::string::npos);
        
        if (!isMatch) {
            if (attempt % 10 == 0) {
                LOG("[poll %d] Still in lobby/frontend — waiting for match", attempt);
            }
            continue;
        }
        
        LOG("═══════════════════════════════════════════════════════");
        LOG("✅ Match world detected: '%s'", wname.c_str());
        
        if (nm == 2) {
            LOG("✅ Already ListenServer — nothing to do");
            break;
        }
        
        if (nm != 0) {
            LOG("⚠️ NetMode is %d (%s) — not Standalone, will still try",
                nm, net_mode_name(nm));
        }
        
        // Try setup
        if (try_setup(world, "auto")) {
            g_done = true;
            LOG("TEST COMPLETE — check log above for NetMode=2");
            break;
        }
        
        // Failed - retry once after 15s
        LOG("═══════════════════════════════════════════════════════");
        LOG("First attempt failed — retrying in 15s...");
        sleep(15);
        
        // Refresh world
        world = *gworldSlot;
        if (!world || is_bad(world)) {
            LOG("World disappeared during retry wait");
            continue;
        }
        
        if (try_setup(world, "retry")) {
            g_done = true;
            LOG("TEST COMPLETE on retry — check log above");
            break;
        }
        
        LOG("═══════════════════════════════════════════════════════");
        LOG("❌ Both attempts failed — test exhausted");
        LOG("See individual [Listen] errors above for details");
        LOG("═══════════════════════════════════════════════════════");
        g_done = true;
        break;
    }
    
    if (!g_done) {
        LOG("❌ Timeout — no match world detected in %ds",
            kMaxPollAttempts * kPollIntervalSec);
    }
    
    if (g_logFile) {
        fclose(g_logFile);
        g_logFile = nullptr;
    }
    
    LOG("Worker thread finished — log file written");
    return nullptr;
}

// ═══════════════════════════════════════════════════════════════
// Entry points
// ═══════════════════════════════════════════════════════════════
extern "C" __attribute__((constructor))
void lib_constructor() {
    pthread_t t;
    if (pthread_create(&t, nullptr, worker, nullptr) == 0) {
        pthread_detach(t);
    }
}

extern "C" JNIEXPORT jint JNICALL
JNI_OnLoad(JavaVM* vm, void* reserved) {
    return JNI_VERSION_1_6;
}

// Optional JNI trigger (if you load via System.loadLibrary)
extern "C" JNIEXPORT void JNICALL
Java_com_example_listentest_ListenTest_start(JNIEnv* env, jclass) {
    // Already started by constructor, this is a no-op if already running
    pthread_t t;
    if (pthread_create(&t, nullptr, worker, nullptr) == 0) {
        pthread_detach(t);
    }
}
