// ============================================================
// Erbium Android Hook — Final v5 (مع StaticFindObject الحقيقي!)
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
// ⭐ العناوين الصحيحة
// ============================================================
#define ADDR_StaticFindObject   0x762A664   // ← ✅ من تحليلك!

// ============================================================
// Offsets (مؤكدة)
// ============================================================
#define OFFSET_GameMode_GameState      0x2D0
#define OFFSET_GameState_GamePhase     0x2150
#define OFFSET_GameMode_V4             0x478
#define OFFSET_V4_V5                   0x2D8

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
// Globals
// ============================================================
static uintptr_t g_base = 0;
static std::mutex g_vft_mutex;
static std::vector<void*> g_known_vfts;
static void* g_cached_playlist = nullptr;

static void (*g_orig_HMHS)(void*) = nullptr;
static void* (*g_orig_SDPF)(void*, void*, void*) = nullptr;

static void Hook_HandleMatchHasStarted(void*);
static void* Hook_SpawnDefaultPawnFor(void*, void*, void*);

// ============================================================
// StaticFindObject — التوقيع الحقيقي
// ============================================================
typedef void* (*StaticFindObject_t)(
    void* Class,        // x0 - UClass*
    void* InOuter,      // x1 - UObject* (يمرر -1 كـ nullptr)
    const wchar_t* Name,// x2 - wchar_t*
    bool  ExactClass    // w3 - bool
);
static StaticFindObject_t g_StaticFindObject = nullptr;

static void* FindPlaylistByName(const wchar_t* name) {
    if (!g_StaticFindObject) {
        LOGF("[Playlist] StaticFindObject NOT set!");
        return nullptr;
    }

    LOGF("[Playlist] StaticFindObject(\"%ls\")", name);

    // ⚠️ مرر -1 (وليس nullptr) للـ Outer (كما يفعل الكود الأصلي)
    void* result = g_StaticFindObject(
        nullptr,        // UClass*
        (void*)-1,      // InOuter = -1 (يعني nullptr)
        name,
        false           // bExactClass
    );

    LOGF("[Playlist] → Result = %p", result);
    return result;
}

// ============================================================
// حقن Playlist
// ============================================================
static bool InjectPlaylist(void* gameMode) {
    if (!gameMode) return false;

    void* v4 = *(void**)((char*)gameMode + OFFSET_GameMode_V4);
    LOGF("[Inject] v4 = %p", v4);
    if (!v4) return false;

    void* current_v5 = *(void**)((char*)v4 + OFFSET_V4_V5);
    LOGF("[Inject] current v5 = %p", current_v5);

    if (current_v5 != nullptr) {
        LOGF("[Inject] v5 already set");
        return true;
    }

    if (!g_cached_playlist) {
        g_cached_playlist = FindPlaylistByName(
            L"/Game/Athena/Playlists/Playlist_DefaultSolo.Playlist_DefaultSolo"
        );
        if (!g_cached_playlist) {
            LOGF("[Inject] Playlist NOT found!");
            return false;
        }
    }

    *(void**)((char*)v4 + OFFSET_V4_V5) = g_cached_playlist;
    LOGF("[Inject] ✓ Set v5 = %p", g_cached_playlist);
    return true;
}

// ============================================================
// VFT hooks install
// ============================================================
static bool InstallVFTOnReal(void* gameMode) {
    if (!gameMode) return false;
    void* real_vft = *(void**)gameMode;
    if (!real_vft) return false;

    std::lock_guard<std::mutex> lk(g_vft_mutex);
    for (auto* v : g_known_vfts) if (v == real_vft) return false;

    LOGF("[VFT] New VFT: %p (GM=%p)", real_vft, gameMode);

    void** vft = (void**)real_vft;
    bool ok1 = VFTPatch(vft, VFT_INDEX_HandleMatchHasStarted,
                        (void*)Hook_HandleMatchHasStarted, (void**)&g_orig_HMHS);
    bool ok2 = VFTPatch(vft, VFT_INDEX_SpawnDefaultPawnFor,
                        (void*)Hook_SpawnDefaultPawnFor, (void**)&g_orig_SDPF);
    LOGF("[VFT] Hook 0x11C: %s | 0x0DD: %s", ok1?"OK":"FAIL", ok2?"OK":"FAIL");

    g_known_vfts.push_back(real_vft);
    return true;
}

static void Hook_HandleMatchHasStarted(void* this_) {
    LOGF("[0x11C] HandleMatchHasStarted this=%p", this_);
    if (g_orig_HMHS) g_orig_HMHS(this_);
}

static void* Hook_SpawnDefaultPawnFor(void* a, void* b, void* c) {
    LOGF("[0xDD] SpawnDefaultPawnFor a=%p b=%p c=%p", a, b, c);
    void* r = g_orig_SDPF ? g_orig_SDPF(a, b, c) : nullptr;
    LOGF("[0xDD] Return = %p", r);
    return r;
}

// ============================================================
// Playlist Handler hook (تشخيصي)
// ============================================================
typedef int (*PlaylistHandler_t)(void*);
static PlaylistHandler_t g_orig_Playlist = nullptr;

static int Hook_PlaylistHandler(void* a1) {
    static int cc = 0;
    cc++;
    bool log = (cc <= 3) || (cc % 100 == 0);

    if (log) {
        LOGF("[Playlist #%d] a1=%p", cc, a1);
        if (a1) {
            void* v4 = *(void**)((char*)a1 + OFFSET_GameMode_V4);
            if (v4) {
                void* v5 = *(void**)((char*)v4 + OFFSET_V4_V5);
                LOGF("  v4=%p v5=%p %s", v4, v5, v5?"":"(NULL)");
            }
        }
    }

    int result = g_orig_Playlist ? g_orig_Playlist(a1) : 0;
    if (log) LOGF("  → %d %s", result, result?"OK":"FAIL");
    return result;
}

// ============================================================
// StartAircraftPhase hook — الحل الأساسي
// ============================================================
typedef bool (*StartAircraftPhase_t)(void*, char);
static StartAircraftPhase_t g_orig_SAP = nullptr;

static bool Hook_StartAircraftPhase(void* this_, char a2) {
    LOGF("");
    LOGF("========================================");
    LOGF("[Aircraft] StartAircraftPhase this=%p a2=%d", this_, (int)a2);

    InstallVFTOnReal(this_);

    void* gs = nullptr;
    uint8_t* phase = nullptr;
    if (this_) {
        gs = *(void**)((char*)this_ + OFFSET_GameMode_GameState);
        if (gs) {
            phase = (uint8_t*)((char*)gs + OFFSET_GameState_GamePhase);
            LOGF("  GameState=%p GamePhase=%d", gs, *phase);
        }
    }

    // محاولة أولى
    bool result = g_orig_SAP ? g_orig_SAP(this_, a2) : false;
    LOGF("  Attempt 1 = %d", (int)result);

    if (result) {
        LOGF("  [✓] Success!");
        LOGF("========================================");
        return true;
    }

    // محاولة حقن Playlist
    LOGF("  [!] Failed → Injecting Playlist...");
    if (InjectPlaylist(this_)) {
        if (phase && *phase != 2) *phase = 2;

        result = g_orig_SAP ? g_orig_SAP(this_, a2) : false;
        LOGF("  Attempt 2 = %d", (int)result);

        if (result) {
            LOGF("  [✓✓] SUCCESS after Playlist injection!");
            LOGF("========================================");
            return true;
        }
    }

    // Fallback: Force GamePhase
    LOGF("  [Fallback] Force GamePhase = 3");
    if (phase) { uint8_t old = *phase; *phase = 3; LOGF("  %d → %d", old, *phase); }
    if (g_orig_HMHS) g_orig_HMHS(this_);

    LOGF("  Final = true (forced)");
    LOGF("========================================");
    return true;
}

// ============================================================
// GetGameState hook
// ============================================================
typedef void* (*GetGameState_t)(void*);
static GetGameState_t g_orig_GGS = nullptr;
static uint64_t g_ggs_calls = 0;

static void* Hook_GetGameState(void* gm) {
    g_ggs_calls++;
    void* r = g_orig_GGS ? g_orig_GGS(gm) : nullptr;
    if (gm && g_ggs_calls > 3) InstallVFTOnReal(gm);
    return r;
}

// ============================================================
// Main
// ============================================================
static void RunHooks() {
    LibInfo info{0,0,0};
    for (int i = 0; i < 60; i++) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        info = GetLibInfo();
        if (info.base) break;
    }
    if (!info.base) { LOGF("[ERROR] libUnreal not found"); return; }

    g_base = info.base;
    LOGF("========================================");
    LOGF(" base = 0x%lx", g_base);
    LOGF("========================================");

    int r1 = DobbyHook((void*)(g_base + ADDR_GetGameState),
                       (void*)Hook_GetGameState, (void**)&g_orig_GGS);
    LOGF("Hook GetGameState: %s", r1 == 0 ? "OK" : "FAIL");

    int r2 = DobbyHook((void*)(g_base + ADDR_PlaylistHandler),
                       (void*)Hook_PlaylistHandler, (void**)&g_orig_Playlist);
    LOGF("Hook Playlist: %s", r2 == 0 ? "OK" : "FAIL");

    int r3 = DobbyHook((void*)(g_base + ADDR_StartAircraftPhase),
                       (void*)Hook_StartAircraftPhase, (void**)&g_orig_SAP);
    LOGF("Hook StartAircraftPhase: %s", r3 == 0 ? "OK" : "FAIL");

    // ⭐ StaticFindObject
    g_StaticFindObject = (StaticFindObject_t)(g_base + ADDR_StaticFindObject);
    LOGF("StaticFindObject @ %p", (void*)g_StaticFindObject);

    // اختبره الآن
    LOGF("Testing StaticFindObject...");
    void* pl = FindPlaylistByName(
        L"/Game/Athena/Playlists/Playlist_DefaultSolo.Playlist_DefaultSolo"
    );
    if (pl) {
        LOGF("[✓] Playlist = %p", pl);
        g_cached_playlist = pl;
    } else {
        LOGF("[!] Playlist NOT found");
    }

    LOGF("========================================");
    LOGF(" All Hooks Installed");
    LOGF("========================================");
}

extern "C" JNIEXPORT jint JNICALL
JNI_OnLoad(JavaVM* vm, void* reserved) {
    InitLogFile();
    LOGF("========================================");
    LOGF(" Erbium Hook — Final v5");
    LOGF("========================================");
    std::thread([]{ try { RunHooks(); } catch (...) {} }).detach();
    return JNI_VERSION_1_6;
}
