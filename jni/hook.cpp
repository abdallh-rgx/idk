// ============================================================
// hook — Android JNI (v53: SpawnActor on GameThread)
// ============================================================
#include <jni.h>
#include <cstdint>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cmath>
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

// ============================================================
// Addresses (from IDA)
// ============================================================
#define ADDR_StaticFindObject             0x762A664
#define ADDR_GetGameState                 0x55B8AB0
#define ADDR_PlaylistHandler              0x5519710
#define ADDR_StartAircraftPhase           0x550A1F4
#define ADDR_PostLogin                    0x550C610

// Discovered targets
#define ADDR_SpawnActor                   0x947F4FC   // UWorld::SpawnActor
#define ADDR_GetAllActorsOfClass          0x93A4744   // UGameplayStatics::GetAllActorsOfClass impl

// ============================================================
// Field offsets
// ============================================================
#define OFF_UOBJ_OUTERPRIVATE             0x20

#define OFF_PC_PlayerState                0x278
#define OFF_PC_Pawn                       0x2B8
#define OFF_PC_AckPawn                    0x2C0

#define OFF_Pawn_Controller               0x2A8
#define OFF_Pawn_Owner                    0x2A0
#define OFF_Pawn_PlayerState              0x2D0

#define OFF_GM_GameState                  0x2D0
#define OFF_GS_Playlist2298               0x2298
#define OFF_GS_Playlist2290               0x2290

// PlayerState spectator bit (from RestartPlayerAtPlayerStart IDA)
#define OFF_PS_SpectatorByte              0x27A
#define BIT_PS_Spectator                  0x08   // bit 3

// ============================================================
// Logger
// ============================================================
static FILE* g_logfile = nullptr;
static std::mutex& GetMutex() { static std::mutex* m = new std::mutex(); return *m; }
static void InitLogFile() {
    const char* p[] = {
        "/storage/emulated/0/Android/data/com.epicgames.fortnite/files/hook.log",
        "/sdcard/Android/data/com.epicgames.fortnite/files/hook.log",
        "/data/local/tmp/hook.log",
    };
    for (auto x : p) { FILE* f = fopen(x, "w"); if (f) { g_logfile = f; return; } }
}
static void LOGF(const char* fmt, ...) {
    char buf[4096]; va_list ap; va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap); va_end(ap);
    __android_log_print(ANDROID_LOG_INFO, LOG_TAG, "%s", buf);
    if (g_logfile) {
        std::lock_guard<std::mutex> lock(GetMutex());
        fprintf(g_logfile, "%s\n", buf); fflush(g_logfile);
    }
}

// ============================================================
// Safe memory
// ============================================================
static bool SafeReadPtr(void* addr, void** out) {
    *out = nullptr;
    if (!addr) return false;
    struct iovec l = { out, sizeof(void*) }, r = { addr, sizeof(void*) };
    return process_vm_readv(getpid(), &l, 1, &r, 1, 0) == (ssize_t)sizeof(void*);
}
static bool SafeReadBytes(void* addr, void* out, size_t n) {
    if (!addr || !out || n == 0 || n > 4096) return false;
    struct iovec l = { out, n }, r = { addr, n };
    return process_vm_readv(getpid(), &l, 1, &r, 1, 0) == (ssize_t)n;
}
static bool SafeWritePtr(void* addr, void* v) {
    if (!addr) return false;
    struct iovec l = { &v, sizeof(void*) }, r = { addr, sizeof(void*) };
    return process_vm_writev(getpid(), &l, 1, &r, 1, 0) == (ssize_t)sizeof(void*);
}
static bool SafeWriteU8(void* addr, uint8_t v) {
    struct iovec l = { &v, 1 }, r = { addr, 1 };
    return process_vm_writev(getpid(), &l, 1, &r, 1, 0) == 1;
}
static bool SafeReadU8(void* addr, uint8_t* v) {
    struct iovec l = { v, 1 }, r = { addr, 1 };
    return process_vm_readv(getpid(), &l, 1, &r, 1, 0) == 1;
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
            o->text_end = o->text_start + p.p_memsz;
            break;
        }
        return 1;
    }, &info);
    return info;
}

// ============================================================
// Globals
// ============================================================
static uintptr_t g_base = 0;
static std::mutex g_inject_mutex;
static std::set<void*> g_injected_gms;
static void* g_cached_playlist = nullptr;
static void* g_last_gm = nullptr;
static void* g_last_pc = nullptr;

typedef void* (*StaticFindObject_t)(void*, void*, const char16_t*, bool);
static StaticFindObject_t SFO = nullptr;

static bool IsValidPtr(void* p) {
    uintptr_t v = (uintptr_t)p;
    return (v >= 0x1000000000ULL && v <= 0x8000000000ULL);
}

static void* SFO2(const char16_t* name) {
    if (!SFO) return nullptr;
    void* a = SFO(nullptr, nullptr, name, false);
    if (IsValidPtr(a)) return a;
    void* b = SFO(nullptr, (void*)-1, name, false);
    if (IsValidPtr(b)) return b;
    return nullptr;
}

// ============================================================
// Walk outer chain
// ============================================================
static void* WalkOuter(void* start, int hops) {
    void* cur = start;
    for (int i = 0; i < hops; i++) {
        if (!IsValidPtr(cur)) return nullptr;
        void* outer = nullptr;
        if (!SafeReadPtr((char*)cur + OFF_UOBJ_OUTERPRIVATE, &outer)) return nullptr;
        if (!IsValidPtr(outer)) return nullptr;
        cur = outer;
    }
    return cur;
}

// ============================================================
// FTransform (UE5 single-precision, 48 bytes)
// ============================================================
struct FTransform {
    float qx, qy, qz, qw;
    float tx, ty, tz, _p1;
    float sx, sy, sz, _p2;
};

static void MakeXform(float x, float y, float z, FTransform* t) {
    memset(t, 0, sizeof(*t));
    t->qw = 1.0f;
    t->tx = x; t->ty = y; t->tz = z;
    t->sx = t->sy = t->sz = 1.0f;
}

struct TArrayN { void* Data; int32_t Num; int32_t Max; int32_t pad; };

// ============================================================
// Call GetAllActorsOfClass impl
// ============================================================
typedef void (*GetAllActorsFn_t)(void* context, void* cls, void* outTArray);

static int CallGetAllActors(void* ctx, void* cls, void* out[128]) {
    if (!IsValidPtr(ctx) || !IsValidPtr(cls)) return 0;

    GetAllActorsFn_t fn = (GetAllActorsFn_t)(g_base + ADDR_GetAllActorsOfClass);
    TArrayN outArr = { nullptr, 0, 0, 0 };

    fn(ctx, cls, &outArr);

    LOGF("[GetAll] ctx=%p cls=%p -> Data=%p Num=%d",
         ctx, cls, outArr.Data, outArr.Num);

    int n = outArr.Num;
    if (n < 0) n = 0;
    if (n > 128) n = 128;
    if (n > 0 && IsValidPtr(outArr.Data)) {
        for (int i = 0; i < n; i++) {
            void* a = nullptr;
            SafeReadPtr((char*)outArr.Data + i * 8, &a);
            out[i] = a;
            LOGF("  actor[%d] = %p", i, a);
        }
    }
    return n;
}

// ============================================================
// Read actor location
// ============================================================
static bool ReadActorLocation(void* actor, float out[3]) {
    out[0] = out[1] = out[2] = 0;
    if (!IsValidPtr(actor)) return false;

    static const int rootOffs[] = { 0x130, 0x198, 0x1A0, 0x1B0, 0x220, 0x240, 0x250, 0x280 };
    static const int locOffs[]  = { 0x128, 0x1F0, 0x1E0, 0x1D0, 0x250, 0x130, 0x100, 0x110,
                                    0x118, 0x1A8, 0x1B0, 0x1B8 };

    for (int ro : rootOffs) {
        void* root = nullptr;
        if (!SafeReadPtr((char*)actor + ro, &root) || !IsValidPtr(root)) continue;
        for (int lo : locOffs) {
            float xyz[3];
            if (!SafeReadBytes((char*)root + lo, xyz, 12)) continue;
            if (!std::isfinite(xyz[0]) || !std::isfinite(xyz[1]) || !std::isfinite(xyz[2])) continue;
            if (fabsf(xyz[0]) > 500000 || fabsf(xyz[1]) > 500000) continue;
            if (xyz[2] < -2000 || xyz[2] > 20000) continue;
            if (xyz[0] == 0 && xyz[1] == 0 && xyz[2] == 0) continue;
            out[0] = xyz[0]; out[1] = xyz[1]; out[2] = xyz[2];
            LOGF("  [loc] root@0x%X loc@0x%X = (%.0f, %.0f, %.0f)", ro, lo, out[0], out[1], out[2]);
            return true;
        }
    }
    return false;
}

// ============================================================
// Clear spectator bit
// ============================================================
static void ClearSpectatorBit(void* ps) {
    if (!IsValidPtr(ps)) return;
    uint8_t b = 0;
    if (!SafeReadU8((char*)ps + OFF_PS_SpectatorByte, &b)) return;
    uint8_t nb = b & ~BIT_PS_Spectator;
    if (nb != b) {
        SafeWriteU8((char*)ps + OFF_PS_SpectatorByte, nb);
        LOGF("[Spec] PS+0x%X: 0x%02X -> 0x%02X", OFF_PS_SpectatorByte, b, nb);
    } else {
        LOGF("[Spec] PS+0x%X: 0x%02X (already clear)", OFF_PS_SpectatorByte, b);
    }
}

// ============================================================
// SpawnActor wrapper
// ============================================================
typedef void* (*SpawnActor_t)(void* world, void* cls, void* xform, void* params);

static void* DoSpawnActor(void* world, void* cls, FTransform* xform) {
    if (!IsValidPtr(world) || !IsValidPtr(cls)) return nullptr;

    // Zeroed params buffer (required — SpawnActor dereferences it)
    uint8_t params[0x100];
    memset(params, 0, sizeof(params));

    SpawnActor_t fn = (SpawnActor_t)(g_base + ADDR_SpawnActor);
    LOGF("[Spawn] SpawnActor(world=%p cls=%p loc=(%.0f,%.0f,%.0f))",
         world, cls, xform->tx, xform->ty, xform->tz);

    void* result = fn(world, cls, xform, params);
    LOGF("[Spawn] returned %p %s", result, IsValidPtr(result) ? "OK" : "NULL");
    return result;
}

// ============================================================
// Attach Pawn to PC
// ============================================================
static void AttachPawnToPC(void* pc, void* pawn) {
    if (!IsValidPtr(pc) || !IsValidPtr(pawn)) return;
    LOGF("[Possess] attach pawn=%p -> pc=%p", pawn, pc);
    SafeWritePtr((char*)pc + OFF_PC_Pawn, pawn);
    SafeWritePtr((char*)pc + OFF_PC_AckPawn, pawn);
    SafeWritePtr((char*)pawn + OFF_Pawn_Controller, pc);
    SafeWritePtr((char*)pawn + OFF_Pawn_Owner, pc);
    void* ps = nullptr;
    if (SafeReadPtr((char*)pc + OFF_PC_PlayerState, &ps) && IsValidPtr(ps)) {
        SafeWritePtr((char*)pawn + OFF_Pawn_PlayerState, ps);
    }
    LOGF("[Possess] done");
}

// ============================================================
// Playlist
// ============================================================
static void* FindPlaylist() {
    if (!SFO) return nullptr;
    void* r = SFO(nullptr, nullptr,
        u"/Game/Athena/Playlists/Playlist_DefaultSolo.Playlist_DefaultSolo", false);
    return IsValidPtr(r) ? r : nullptr;
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
    if (!SafeReadPtr((char*)gm + OFF_GM_GameState, &gs) || !gs) return false;
    *(void**)((char*)gs + OFF_GS_Playlist2298) = g_cached_playlist;
    *(void**)((char*)gs + OFF_GS_Playlist2290) = g_cached_playlist;
    LOGF("[Inject] gs+2298/2290 = %p", g_cached_playlist);
    g_injected_gms.insert(gm);
    return true;
}

// ============================================================
// Find pawn class
// ============================================================
static void* FindPawnClass() {
    static const char16_t* names[] = {
        u"FortPlayerPawnAthena",
        u"FortPlayerPawnAthena_C",
        u"FortPlayerPawn",
        u"Athena_PlayerPawn_C",
        u"BP_PlayerPawn_Athena_C",
        u"PlayerPawn_Athena_C",
    };
    for (auto n : names) {
        void* c = SFO2(n);
        if (IsValidPtr(c)) {
            LOGF("[PawnClass] found: %p", c);
            return c;
        }
    }
    LOGF("[PawnClass] none found");
    return nullptr;
}

// ============================================================
// Main spawn (called from GameThread hook)
// ============================================================
static void DoSpawnOnGameThread(void* gm, void* pc) {
    LOGF("");
    LOGF("============================================================");
    LOGF("[Spawn] GameThread spawn attempt GM=%p PC=%p", gm, pc);
    LOGF("============================================================");

    if (!IsValidPtr(gm) || !IsValidPtr(pc)) return;

    // 1) Get PlayerState
    void* ps = nullptr;
    SafeReadPtr((char*)pc + OFF_PC_PlayerState, &ps);
    LOGF("[Spawn] PlayerState = %p", ps);

    // 2) Clear spectator bit
    if (IsValidPtr(ps)) ClearSpectatorBit(ps);

    // 3) Check existing pawn
    void* existing = nullptr;
    SafeReadPtr((char*)pc + OFF_PC_Pawn, &existing);
    if (IsValidPtr(existing)) {
        LOGF("[Spawn] Pawn already exists: %p", existing);
        AttachPawnToPC(pc, existing);
        return;
    }

    // 4) Find World
    void* gs = nullptr;
    SafeReadPtr((char*)gm + OFF_GM_GameState, &gs);
    void* world = nullptr;
    if (IsValidPtr(gs)) world = WalkOuter(gs, 2);
    if (!world && IsValidPtr(pc)) world = WalkOuter(pc, 2);
    if (!world && IsValidPtr(gm)) world = WalkOuter(gm, 2);
    LOGF("[Spawn] World = %p", world);
    if (!IsValidPtr(world)) {
        LOGF("[Spawn] World not found, abort");
        return;
    }

    // 5) Find PlayerStart location
    float loc[3] = { 0.0f, 0.0f, 5000.0f };
    bool gotLoc = false;

    void* fpsCls = SFO2(u"FortPlayerStartWarmup");
    void* psCls  = SFO2(u"PlayerStart");

    void* actors[128] = {0};
    int n = 0;
    if (IsValidPtr(fpsCls)) {
        n = CallGetAllActors(pc, fpsCls, actors);
        LOGF("[Spawn] FPW count = %d", n);
    }
    if (n == 0 && IsValidPtr(psCls)) {
        n = CallGetAllActors(pc, psCls, actors);
        LOGF("[Spawn] PS count = %d", n);
    }
    for (int i = 0; i < n && i < 128; i++) {
        if (!IsValidPtr(actors[i])) continue;
        if (ReadActorLocation(actors[i], loc)) {
            gotLoc = true;
            LOGF("[Spawn] using loc from actor[%d] = (%.0f, %.0f, %.0f)",
                 i, loc[0], loc[1], loc[2]);
            break;
        }
    }
    if (!gotLoc) LOGF("[Spawn] fallback loc (%.0f, %.0f, %.0f)", loc[0], loc[1], loc[2]);

    // 6) Find Pawn Class
    void* pawnCls = FindPawnClass();
    if (!IsValidPtr(pawnCls)) {
        LOGF("[Spawn] PawnClass not found, abort");
        return;
    }

    // 7) Build transform and spawn
    FTransform xform;
    MakeXform(loc[0], loc[1], loc[2] + 200.0f, &xform);

    void* pawn = DoSpawnActor(world, pawnCls, &xform);
    if (!IsValidPtr(pawn)) {
        LOGF("[Spawn] SpawnActor failed");
        return;
    }

    // 8) Attach
    AttachPawnToPC(pc, pawn);
    LOGF("[Spawn] ===== SUCCESS =====");
}

// ============================================================
// Hooks
// ============================================================
typedef void (*PostLogin_t)(void*, void*);
static PostLogin_t o_PostLogin = nullptr;
static uint64_t g_postlogin_calls = 0;

static void Hook_PostLogin(void* gm, void* pc) {
    g_postlogin_calls++;
    LOGF("");
    LOGF("========== [PostLogin #%llu] GM=%p PC=%p ==========",
         (unsigned long long)g_postlogin_calls, gm, pc);

    g_last_gm = gm;
    g_last_pc = pc;

    InjectPlaylist(gm);

    if (o_PostLogin) o_PostLogin(gm, pc);
}

typedef bool (*StartAircraftPhase_t)(void*, char);
static StartAircraftPhase_t o_SAP = nullptr;
static bool g_aircraft_done = false;

static bool Hook_StartAircraftPhase(void* gm, char a2) {
    LOGF("");
    LOGF("========================================");
    LOGF("[Aircraft] this=%p a2=%d", gm, (int)a2);

    // ⭐ We are on GameThread — safe to call SpawnActor!
    if (!g_aircraft_done && IsValidPtr(g_last_pc)) {
        g_aircraft_done = true;
        DoSpawnOnGameThread(gm, g_last_pc);
    }

    bool result = o_SAP ? o_SAP(gm, a2) : false;
    LOGF("[Aircraft] returned: %d %s", (int)result, result ? "OK" : "FAIL");
    LOGF("========================================");
    return result;
}

typedef int (*PlaylistHandler_t)(void*);
static PlaylistHandler_t o_PH = nullptr;
static int Hook_PlaylistHandler(void* a1) {
    if (a1) InjectPlaylist(a1);
    return o_PH ? o_PH(a1) : 0;
}

typedef void* (*GetGameState_t)(void*);
static GetGameState_t o_GGS = nullptr;
static void* Hook_GGS(void* gm) { return o_GGS ? o_GGS(gm) : nullptr; }

// ============================================================
// Main
// ============================================================
static void RunHooks() {
    LibInfo i{0,0,0};
    for (int k = 0; k < 60; k++) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        i = GetLibInfo(); if (i.base) break;
    }
    if (!i.base) { LOGF("[ERR] libUnreal not found"); return; }
    g_base = i.base;

    LOGF("========================================");
    LOGF(" hook — v53 (SpawnActor GameThread)");
    LOGF(" base=0x%lx", g_base);
    LOGF("========================================");

    DobbyHook((void*)(g_base + ADDR_GetGameState),
              (void*)Hook_GGS, (void**)&o_GGS);
    DobbyHook((void*)(g_base + ADDR_PlaylistHandler),
              (void*)Hook_PlaylistHandler, (void**)&o_PH);
    DobbyHook((void*)(g_base + ADDR_StartAircraftPhase),
              (void*)Hook_StartAircraftPhase, (void**)&o_SAP);
    DobbyHook((void*)(g_base + ADDR_PostLogin),
              (void*)Hook_PostLogin, (void**)&o_PostLogin);

    SFO = (StaticFindObject_t)(g_base + ADDR_StaticFindObject);
    LOGF("StaticFindObject: %p", (void*)SFO);
    LOGF(" All Hooks Installed.");
}

extern "C" JNIEXPORT jint JNICALL
JNI_OnLoad(JavaVM* vm, void* reserved) {
    InitLogFile();
    LOGF("========================================");
    LOGF(" hook — %s %s", __DATE__, __TIME__);
    LOGF(" PID: %d", getpid());
    LOGF("========================================");
    std::thread([]{ try { RunHooks(); } catch (...) { LOGF("[ERR]"); } }).detach();
    return JNI_VERSION_1_6;
}
