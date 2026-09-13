// ============================================================
// hook — Android JNI (v39: Razer's SpawnDefaultPawnFor hook)
// ============================================================
#include <jni.h>
#include <cstdint>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cerrno>
#include <cstdarg>
#include <string>
#include <dlfcn.h>
#include <link.h>
#include <thread>
#include <chrono>
#include <mutex>
#include <set>
#include <android/log.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/uio.h>
#include "dobby.h"

#define LOG_TAG "hook"

#define ADDR_StaticFindObject            0x762A664
#define ADDR_GetGameState                0x55B8AB0
#define ADDR_PlaylistHandler             0x5519710
#define ADDR_StartAircraftPhase          0x550A1F4
#define ADDR_PostLogin                   0x550C610
#define ADDR_SpawnDefaultPawnFor         0x551A7C4

// GameMode vtable slot for SpawnDefaultPawnAtTransform (from v24 dump)
#define VFT_SLOT_SpawnDefaultPawnAtTransform  0xDE

#define OFFSET_GameMode_GameState        0x2D0
#define OFFSET_GameState_Playlist2298    0x2298
#define OFFSET_GameState_Playlist2290    0x2290
#define OFFSET_PC_PlayerState            0x278
#define OFFSET_PC_Pawn                   0x2B8

// ------------------------------------------------------------
// Logger
// ------------------------------------------------------------
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
    char buf[4096];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    __android_log_print(ANDROID_LOG_INFO, LOG_TAG, "%s", buf);
    if (g_logfile) {
        std::lock_guard<std::mutex> lock(GetMutex());
        fprintf(g_logfile, "%s\n", buf);
        fflush(g_logfile);
    }
}

// ------------------------------------------------------------
// Safe memory
// ------------------------------------------------------------
static bool SafeReadPtr(void* addr, void** out) {
    *out = nullptr;
    if (!addr) return false;
    struct iovec local  = { out, sizeof(void*) };
    struct iovec remote = { addr, sizeof(void*) };
    return process_vm_readv(getpid(), &local, 1, &remote, 1, 0) == (ssize_t)sizeof(void*);
}
static bool SafeReadBytes(void* addr, void* out, size_t n) {
    if (!addr || !out || n == 0 || n > 256) return false;
    struct iovec local  = { out, n };
    struct iovec remote = { addr, n };
    return process_vm_readv(getpid(), &local, 1, &remote, 1, 0) == (ssize_t)n;
}

// ------------------------------------------------------------
// LibInfo
// ------------------------------------------------------------
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

// ------------------------------------------------------------
// Globals
// ------------------------------------------------------------
static uintptr_t g_base = 0;
static std::mutex g_inject_mutex;
static std::set<void*> g_injected_gms;
static void* g_cached_playlist = nullptr;

typedef void* (*StaticFindObject_t)(void*, void*, const char16_t*, bool);
static StaticFindObject_t g_StaticFindObject = nullptr;

static bool IsValidPtr(void* ptr) {
    uintptr_t p = (uintptr_t)ptr;
    return (p >= 0x1000000000ULL && p <= 0x8000000000ULL);
}

// ------------------------------------------------------------
// Playlist
// ------------------------------------------------------------
static void* FindPlaylist() {
    if (!g_StaticFindObject) return nullptr;
    const char16_t* full = u"/Game/Athena/Playlists/Playlist_DefaultSolo.Playlist_DefaultSolo";
    return g_StaticFindObject(nullptr, nullptr, full, false);
}

static bool InjectPlaylist(void* gm) {
    if (!gm) return false;
    std::lock_guard<std::mutex> lock(g_inject_mutex);
    if (g_injected_gms.count(gm)) return false;
    if (!g_cached_playlist) {
        g_cached_playlist = FindPlaylist();
        if (!g_cached_playlist) return false;
    }
    void* gs = nullptr;
    if (!SafeReadPtr((char*)gm + OFFSET_GameMode_GameState, &gs) || !gs) return false;

    *(void**)((char*)gs + OFFSET_GameState_Playlist2298) = g_cached_playlist;
    *(void**)((char*)gs + OFFSET_GameState_Playlist2290) = g_cached_playlist;
    LOGF("[Inject] gs+2298/2290 = %p", g_cached_playlist);
    g_injected_gms.insert(gm);
    return true;
}

// ------------------------------------------------------------
// FTransform (UE5 single-precision)
// ------------------------------------------------------------
struct FTransform {
    float qx, qy, qz, qw;  // FQuat rotation
    float tx, ty, tz, _p1; // FVector translation
    float sx, sy, sz, _p2; // FVector scale
};

static void MakeIdentityTransform(FTransform* t) {
    memset(t, 0, sizeof(*t));
    t->qw = 1.0f;
    t->sx = t->sy = t->sz = 1.0f;
}

// ------------------------------------------------------------
// Read transform from an AActor (via RootComponent::ComponentToWorld)
// Falls back to a default transform if unavailable
// ------------------------------------------------------------
static void ReadActorTransform(void* actor, FTransform* out) {
    MakeIdentityTransform(out);
    if (!IsValidPtr(actor)) return;

    // Try common RootComponent offsets for UE5
    static const int kRootOffsets[] = { 0x198, 0x1A0, 0x130, 0x1B0 };
    for (int ro : kRootOffsets) {
        void* root = nullptr;
        if (!SafeReadPtr((char*)actor + ro, &root)) continue;
        if (!IsValidPtr(root)) continue;

        // Try common ComponentToWorld offsets
        static const int kCompOffsets[] = { 0x1F0, 0x1E0, 0x1D0, 0x250 };
        for (int co : kCompOffsets) {
            FTransform tmp;
            if (!SafeReadBytes((char*)root + co, &tmp, sizeof(tmp))) continue;

            // Sanity: quat magnitude ~1, scale != 0
            float qmag = tmp.qx*tmp.qx + tmp.qy*tmp.qy + tmp.qz*tmp.qz + tmp.qw*tmp.qw;
            if (qmag > 0.5f && qmag < 2.0f &&
                tmp.sx > 0.001f && tmp.sx < 1000.0f) {
                *out = tmp;
                LOGF("  [xform] actor=%p rootOff=0x%X compOff=0x%X loc=(%.0f,%.0f,%.0f)",
                     actor, ro, co, tmp.tx, tmp.ty, tmp.tz);
                return;
            }
        }
    }
    LOGF("  [xform] actor=%p -> fallback (identity)", actor);
}

// ------------------------------------------------------------
// SpawnDefaultPawnAtTransform via vtable
// ------------------------------------------------------------
typedef void* (*SpawnDefaultPawnAtTransform_t)(void*, void*, void*);
typedef void* (*SpawnDefaultPawnFor_orig_t)(void*, void*, void*);

static SpawnDefaultPawnFor_orig_t g_orig_SDPF = nullptr;

static void* Hook_SpawnDefaultPawnFor(void* gm, void* pc, void* startSpot) {
    LOGF("");
    LOGF("[SDPF-HOOK] gm=%p pc=%p startSpot=%p", gm, pc, startSpot);

    // If pawn already exists, don't spawn
    void* existing = nullptr;
    if (IsValidPtr(pc)) SafeReadPtr((char*)pc + OFFSET_PC_Pawn, &existing);
    if (IsValidPtr(existing)) {
        LOGF("[SDPF-HOOK] pawn already = %p", existing);
        return existing;
    }

    // Determine actual StartSpot (fallback to GameMode)
    void* actualStartSpot = startSpot;
    if (!IsValidPtr(actualStartSpot)) {
        LOGF("[SDPF-HOOK] startSpot null -> using GameMode as fallback");
        actualStartSpot = gm;
    }

    // Read transform from startSpot
    FTransform xform;
    ReadActorTransform(actualStartSpot, &xform);
    LOGF("[SDPF-HOOK] transform loc=(%.0f, %.0f, %.0f)",
         xform.tx, xform.ty, xform.tz);

    // Get SpawnDefaultPawnAtTransform from GameMode vtable
    void** vft = nullptr;
    if (!SafeReadPtr(gm, (void**)&vft) || !IsValidPtr(vft)) {
        LOGF("[SDPF-HOOK] gm vtable invalid");
        return g_orig_SDPF ? g_orig_SDPF(gm, pc, startSpot) : nullptr;
    }

    void* fnPtr = nullptr;
    if (!SafeReadPtr((char*)vft + (size_t)VFT_SLOT_SpawnDefaultPawnAtTransform * 8, &fnPtr)
        || !IsValidPtr(fnPtr)) {
        LOGF("[SDPF-HOOK] SpawnDefaultPawnAtTransform not found in vtable");
        return g_orig_SDPF ? g_orig_SDPF(gm, pc, startSpot) : nullptr;
    }

    LOGF("[SDPF-HOOK] calling SpawnDefaultPawnAtTransform=%p(gm, pc, &xform)", fnPtr);
    SpawnDefaultPawnAtTransform_t fn = (SpawnDefaultPawnAtTransform_t)fnPtr;
    void* pawn = fn(gm, pc, &xform);
    LOGF("[SDPF-HOOK] -> pawn = %p  %s", pawn, IsValidPtr(pawn) ? "✅" : "❌");
    return pawn;
}

// ------------------------------------------------------------
// PostLogin
// ------------------------------------------------------------
typedef void (*PostLogin_t)(void*, void*);
static PostLogin_t g_orig_PostLogin = nullptr;

static void Hook_PostLogin(void* this_, void* newPlayer) {
    LOGF("");
    LOGF("========== [PostLogin] GM=%p PC=%p ==========", this_, newPlayer);

    InjectPlaylist(this_);

    if (g_orig_PostLogin) g_orig_PostLogin(this_, newPlayer);

    LOGF("  PostLogin done");
}

// ------------------------------------------------------------
// Other hooks
// ------------------------------------------------------------
typedef int (*PlaylistHandler_t)(void*);
static PlaylistHandler_t g_orig_Playlist = nullptr;
static int Hook_PlaylistHandler(void* a1) {
    if (a1) InjectPlaylist(a1);
    return g_orig_Playlist ? g_orig_Playlist(a1) : 0;
}

typedef bool (*StartAircraftPhase_t)(void*, char);
static StartAircraftPhase_t g_orig_SAP = nullptr;
static bool Hook_StartAircraftPhase(void* this_, char a2) {
    LOGF("[Aircraft] this=%p a2=%d", this_, (int)a2);
    InjectPlaylist(this_);
    return g_orig_SAP ? g_orig_SAP(this_, a2) : false;
}

typedef void* (*GetGameState_t)(void*);
static GetGameState_t g_orig_GGS = nullptr;
static void* Hook_GetGameState(void* gm) { return g_orig_GGS ? g_orig_GGS(gm) : nullptr; }

// ------------------------------------------------------------
// Main
// ------------------------------------------------------------
static void RunHooks() {
    LibInfo info{0, 0, 0};
    for (int i = 0; i < 60; i++) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        info = GetLibInfo();
        if (info.base) break;
    }
    if (!info.base) { LOGF("[ERROR] libUnreal not found"); return; }

    g_base = info.base;
    LOGF("========================================");
    LOGF(" hook — v39 (Razer's SpawnDefaultPawnFor)");
    LOGF(" base=0x%lx", g_base);
    LOGF("========================================");

    // Hook the SpawnDefaultPawnFor with our custom implementation
    int rc = DobbyHook((void*)(g_base + ADDR_SpawnDefaultPawnFor),
                       (void*)Hook_SpawnDefaultPawnFor,
                       (void**)&g_orig_SDPF);
    LOGF("SpawnDefaultPawnFor hook rc=%d", rc);

    DobbyHook((void*)(g_base + ADDR_GetGameState),
              (void*)Hook_GetGameState,       (void**)&g_orig_GGS);
    DobbyHook((void*)(g_base + ADDR_PlaylistHandler),
              (void*)Hook_PlaylistHandler,    (void**)&g_orig_Playlist);
    DobbyHook((void*)(g_base + ADDR_StartAircraftPhase),
              (void*)Hook_StartAircraftPhase, (void**)&g_orig_SAP);
    DobbyHook((void*)(g_base + ADDR_PostLogin),
              (void*)Hook_PostLogin,          (void**)&g_orig_PostLogin);

    g_StaticFindObject = (StaticFindObject_t)(g_base + ADDR_StaticFindObject);
    LOGF("StaticFindObject: %p", (void*)g_StaticFindObject);
    LOGF(" All Hooks Installed.");
}

extern "C" JNIEXPORT jint JNICALL
JNI_OnLoad(JavaVM* vm, void* reserved) {
    InitLogFile();
    LOGF("========================================");
    LOGF(" hook — %s %s", __DATE__, __TIME__);
    LOGF(" PID: %d", getpid());
    LOGF("========================================");

    std::thread([]{
        try { RunHooks(); }
        catch (...) { LOGF("[ERROR] Exception in RunHooks"); }
    }).detach();

    return JNI_VERSION_1_6;
}
