// ============================================================
// hook — Android JNI (v59: spawn camera manager)
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

#define ADDR_StaticFindObject             0x762A664
#define ADDR_GetGameState                 0x55B8AB0
#define ADDR_PlaylistHandler              0x5519710
#define ADDR_StartAircraftPhase           0x550A1F4
#define ADDR_PostLogin                    0x550C610
#define ADDR_SpawnActor                   0x947F4FC
#define ADDR_GetAllActorsOfClass          0x93A4744
#define ADDR_SpawnPlayerCameraManager     0x968BDDC   // ⭐ من IDA

// ============================================================
// CORRECT offsets (from IDA)
// ============================================================
#define OFF_UOBJ_OUTERPRIVATE             0x20

#define OFF_PC_PlayerState                0x278
#define OFF_PC_Pawn                       0x2B8
#define OFF_PC_AckPawn                    0x2C0
#define OFF_PC_CameraManager              0x328   // ⭐
#define OFF_PC_CameraManagerClass         0x330   // ⭐
#define OFF_PC_ViewTarget_Target          0x2E0   // FViewTarget.Target (guess)

#define OFF_Pawn_Controller               0x2A8
#define OFF_Pawn_Owner                    0x2A0
#define OFF_Pawn_PlayerState              0x2D0

#define OFF_GM_GameState                  0x2D0
#define OFF_GS_Playlist2298               0x2298
#define OFF_GS_Playlist2290               0x2290
#define OFF_PS_SpectatorByte              0x27A
#define BIT_PS_Spectator                  0x08

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

static bool RP(void* a, void** o) {
    *o = nullptr; if (!a) return false;
    struct iovec l = { o, sizeof(void*) }, r = { a, sizeof(void*) };
    return process_vm_readv(getpid(), &l, 1, &r, 1, 0) == (ssize_t)sizeof(void*);
}
static bool RB(void* a, void* o, size_t n) {
    if (!a || !o || n == 0 || n > 4096) return false;
    struct iovec l = { o, n }, r = { a, n };
    return process_vm_readv(getpid(), &l, 1, &r, 1, 0) == (ssize_t)n;
}
static bool WP(void* a, void* v) {
    if (!a) return false;
    struct iovec l = { &v, sizeof(void*) }, r = { a, sizeof(void*) };
    return process_vm_writev(getpid(), &l, 1, &r, 1, 0) == (ssize_t)sizeof(void*);
}
static bool WU8(void* a, uint8_t v) {
    struct iovec l = { &v, 1 }, r = { a, 1 };
    return process_vm_writev(getpid(), &l, 1, &r, 1, 0) == 1;
}
static bool RU8(void* a, uint8_t* v) {
    struct iovec l = { v, 1 }, r = { a, 1 };
    return process_vm_readv(getpid(), &l, 1, &r, 1, 0) == 1;
}

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

static uintptr_t g_base = 0;
static uintptr_t g_text_start = 0, g_text_end = 0;
static std::mutex g_inject_mutex;
static std::set<void*> g_injected_gms;
static void* g_cached_playlist = nullptr;
static void* g_last_gm = nullptr;
static void* g_last_pc = nullptr;

typedef void* (*StaticFindObject_t)(void*, void*, const char16_t*, bool);
static StaticFindObject_t SFO = nullptr;

static bool IVP(void* p) {
    uintptr_t v = (uintptr_t)p;
    return (v >= 0x1000000000ULL && v <= 0x8000000000ULL);
}
static bool ITP(void* p) {
    uintptr_t v = (uintptr_t)p;
    return (g_text_start && v >= g_text_start && v < g_text_end);
}
static void* SFO2(const char16_t* name) {
    if (!SFO) return nullptr;
    void* a = SFO(nullptr, nullptr, name, false);
    if (IVP(a)) return a;
    void* b = SFO(nullptr, (void*)-1, name, false);
    if (IVP(b)) return b;
    return nullptr;
}
static void* WalkOuter(void* start, int hops) {
    void* cur = start;
    for (int i = 0; i < hops; i++) {
        if (!IVP(cur)) return nullptr;
        void* outer = nullptr;
        if (!RP((char*)cur + OFF_UOBJ_OUTERPRIVATE, &outer)) return nullptr;
        if (!IVP(outer)) return nullptr;
        cur = outer;
    }
    return cur;
}

struct FTransform { float qx,qy,qz,qw; float tx,ty,tz,_p1; float sx,sy,sz,_p2; };
static void MakeXform(float x, float y, float z, FTransform* t) {
    memset(t, 0, sizeof(*t));
    t->qw = 1.0f; t->tx = x; t->ty = y; t->tz = z;
    t->sx = t->sy = t->sz = 1.0f;
}
struct TArrayN { void* Data; int32_t Num; int32_t Max; int32_t pad; };

static bool FindTransformInActor(void* actor, float out[3]) {
    if (!IVP(actor)) return false;
    for (int off = 0x100; off < 0x800; off += 4) {
        float q[4];
        if (!RB((char*)actor + off, q, 16)) continue;
        float qm = q[0]*q[0] + q[1]*q[1] + q[2]*q[2] + q[3]*q[3];
        if (qm < 0.9f || qm > 1.1f) continue;
        float t[3];
        if (!RB((char*)actor + off + 16, t, 12)) continue;
        if (!std::isfinite(t[0]) || !std::isfinite(t[1]) || !std::isfinite(t[2])) continue;
        float magXY = fabsf(t[0]) + fabsf(t[1]);
        if (magXY < 5000.0f) continue;
        if (fabsf(t[0]) > 200000.0f || fabsf(t[1]) > 200000.0f) continue;
        if (t[2] < 50.0f || t[2] > 5000.0f) continue;
        out[0] = t[0]; out[1] = t[1]; out[2] = t[2];
        return true;
    }
    return false;
}

static void ClearSpecBit(void* ps) {
    if (!IVP(ps)) return;
    uint8_t b = 0;
    if (!RU8((char*)ps + OFF_PS_SpectatorByte, &b)) return;
    uint8_t nb = b & ~BIT_PS_Spectator;
    if (nb != b) {
        WU8((char*)ps + OFF_PS_SpectatorByte, nb);
        LOGF("[Spec] 0x%02X -> 0x%02X", b, nb);
    }
}

typedef void (*GA_t)(void*, void*, void*);
static int CallGetAllActors(void* ctx, void* cls, void* out[128]) {
    if (!IVP(ctx) || !IVP(cls)) return 0;
    GA_t fn = (GA_t)(g_base + ADDR_GetAllActorsOfClass);
    TArrayN outArr = { nullptr, 0, 0, 0 };
    fn(ctx, cls, &outArr);
    int n = outArr.Num;
    if (n < 0) n = 0;
    if (n > 128) n = 128;
    if (n > 0 && IVP(outArr.Data)) {
        for (int i = 0; i < n; i++) {
            void* a = nullptr;
            RP((char*)outArr.Data + i * 8, &a);
            out[i] = a;
        }
    }
    return n;
}

typedef void* (*SAT_t)(void*, void*, void*, void*);
static void* DoSpawnActor(void* world, void* cls, FTransform* xform) {
    if (!IVP(world) || !IVP(cls)) return nullptr;
    uint8_t params[0x100];
    memset(params, 0, sizeof(params));
    SAT_t fn = (SAT_t)(g_base + ADDR_SpawnActor);
    return fn(world, cls, xform, params);
}

static void* FindPlaylist() {
    if (!SFO) return nullptr;
    return SFO(nullptr, nullptr,
        u"/Game/Athena/Playlists/Playlist_DefaultSolo.Playlist_DefaultSolo", false);
}
static bool InjectPlaylist(void* gm) {
    if (!gm) return false;
    std::lock_guard<std::mutex> lock(g_inject_mutex);
    if (g_injected_gms.count(gm)) return false;
    if (!g_cached_playlist) {
        g_cached_playlist = FindPlaylist();
        if (!IVP(g_cached_playlist)) return false;
    }
    void* gs = nullptr;
    if (!RP((char*)gm + OFF_GM_GameState, &gs) || !gs) return false;
    *(void**)((char*)gs + OFF_GS_Playlist2298) = g_cached_playlist;
    *(void**)((char*)gs + OFF_GS_Playlist2290) = g_cached_playlist;
    g_injected_gms.insert(gm);
    return true;
}

static void* FindPawnClass() {
    static const char16_t* names[] = {
        u"FortPlayerPawnAthena", u"FortPlayerPawnAthena_C",
        u"Athena_PlayerPawn_C", u"FortPlayerPawn",
    };
    for (auto n : names) {
        void* c = SFO2(n);
        if (IVP(c)) return c;
    }
    return nullptr;
}

static void* FindCameraManagerClass() {
    static const char16_t* names[] = {
        u"FortPlayerCameraManager",
        u"FortPlayerCameraManager_C",
        u"AthenaPlayerCameraManager",
        u"PlayerCameraManager",
    };
    for (auto n : names) {
        void* c = SFO2(n);
        if (IVP(c)) { LOGF("[CamClass] found: %p", c); return c; }
    }
    LOGF("[CamClass] none found via SFO");
    return nullptr;
}

// ============================================================
// MAIN SPAWN
// ============================================================
static void DoSpawnOnGameThread(void* gm, void* pc) {
    LOGF("");
    LOGF("============================================================");
    LOGF("[Spawn] GM=%p PC=%p", gm, pc);
    LOGF("============================================================");

    if (!IVP(gm) || !IVP(pc)) return;

    // --- Clear spectator ---
    void* ps = nullptr;
    RP((char*)pc + OFF_PC_PlayerState, &ps);
    if (IVP(ps)) ClearSpecBit(ps);

    // --- Existing pawn check ---
    void* pawn = nullptr;
    RP((char*)pc + OFF_PC_Pawn, &pawn);

    void* world = nullptr;
    void* gs = nullptr;
    RP((char*)gm + OFF_GM_GameState, &gs);
    if (IVP(gs)) world = WalkOuter(gs, 2);
    if (!world && IVP(pc)) world = WalkOuter(pc, 2);
    if (!world && IVP(gm)) world = WalkOuter(gm, 2);
    LOGF("[Spawn] World = %p", world);

    // --- Spawn Pawn if needed ---
    if (!IVP(pawn)) {
        float loc[3] = { 0.0f, 0.0f, 50000.0f };
        bool gotLoc = false;

        void* fpsCls = SFO2(u"FortPlayerStartWarmup");
        if (!IVP(fpsCls)) fpsCls = SFO2(u"PlayerStart");
        void* actors[128] = {0};
        int n = CallGetAllActors(pc, fpsCls, actors);
        LOGF("[Spawn] candidates=%d", n);

        float bestZ = -1e9f;
        for (int i = 0; i < n && i < 128; i++) {
            if (!IVP(actors[i])) continue;
            float tmp[3];
            if (!FindTransformInActor(actors[i], tmp)) continue;
            if (tmp[2] > bestZ) {
                bestZ = tmp[2];
                loc[0] = tmp[0]; loc[1] = tmp[1]; loc[2] = tmp[2];
                gotLoc = true;
                LOGF("[Spawn] actor[%d] = (%.0f,%.0f,%.0f)", i, tmp[0], tmp[1], tmp[2]);
            }
        }
        if (!gotLoc) LOGF("[Spawn] no good loc, using fallback");

        void* pawnCls = FindPawnClass();
        if (!IVP(pawnCls)) return;

        FTransform xform;
        MakeXform(loc[0], loc[1], loc[2], &xform);

        pawn = DoSpawnActor(world, pawnCls, &xform);
        LOGF("[Spawn] pawn = %p", pawn);
        if (!IVP(pawn)) return;
    }

    // --- Write Pawn to PC (BEFORE spawning camera!) ---
    WP((char*)pc + OFF_PC_Pawn, pawn);
    WP((char*)pc + OFF_PC_AckPawn, pawn);
    WP((char*)pawn + OFF_Pawn_Controller, pc);
    WP((char*)pawn + OFF_Pawn_Owner, pc);
    if (IVP(ps)) WP((char*)pawn + OFF_Pawn_PlayerState, ps);

    // Also write ViewTarget.Target
    WP((char*)pc + OFF_PC_ViewTarget_Target, pawn);
    LOGF("[Attach] PC->Pawn = %p", pawn);

    // ============================================================
    // Camera Manager
    // ============================================================
    void* cm = nullptr;
    void* cmCls = nullptr;
    RP((char*)pc + OFF_PC_CameraManager, &cm);
    RP((char*)pc + OFF_PC_CameraManagerClass, &cmCls);

    LOGF("[Cam] BEFORE: PC->CamMgr = %p, PC->CamMgrClass = %p", cm, cmCls);

    // If class null, try to set it
    if (!IVP(cmCls)) {
        // 1) Try GM's CameraManagerClass (scan GM memory for UClass*)
        void* foundCls = nullptr;

        // Try known offsets in GM
        for (int off = 0x2A0; off < 0x500; off += 8) {
            void* v = nullptr;
            if (!RP((char*)gm + off, &v)) continue;
            if (!IVP(v)) continue;
            // Check if v is a UClass — try reading its name
            // (skip for now, just log candidates)
        }

        // 2) Try SFO
        foundCls = FindCameraManagerClass();

        if (IVP(foundCls)) {
            WP((char*)pc + OFF_PC_CameraManagerClass, foundCls);
            LOGF("[Cam] SET PC->CamMgrClass = %p", foundCls);
        } else {
            LOGF("[Cam] WARN: couldn't find CameraManagerClass");
        }
    }

    // ============================================================
    // Call SpawnPlayerCameraManager (0x968BDDC)
    // ============================================================
    if (!IVP(cm)) {
        LOGF("[Cam] calling SpawnPlayerCameraManager(pc)...");
        typedef void (*SpawnCam_t)(void*);
        SpawnCam_t fn = (SpawnCam_t)(g_base + ADDR_SpawnPlayerCameraManager);
        fn(pc);
        RP((char*)pc + OFF_PC_CameraManager, &cm);
        LOGF("[Cam] AFTER: PC->CamMgr = %p", cm);
    }

    // ============================================================
    // If CamMgr exists, try to find its ViewTarget.Target and set it to pawn
    // ============================================================
    if (IVP(cm)) {
        // Scan PlayerCameraManager memory for a pointer == pawn (it might already be set)
        LOGF("[Cam] scanning CamMgr for view target...");
        int found = 0;
        for (int off = 0x100; off < 0x600 && found < 5; off += 8) {
            void* v = nullptr;
            if (!RP((char*)cm + off, &v)) continue;
            if (!IVP(v)) continue;
            if (v == pawn) {
                LOGF("[Cam] CamMgr+0x%X = pawn", off);
                found++;
            }
        }

        // Write ViewTarget.Target on PC too
        WP((char*)pc + OFF_PC_ViewTarget_Target, pawn);
    }

    // ============================================================
    // Final dump
    // ============================================================
    void* pc_pawn = nullptr; void* pc_cm = nullptr; void* pc_vt = nullptr;
    RP((char*)pc + OFF_PC_Pawn, &pc_pawn);
    RP((char*)pc + OFF_PC_CameraManager, &pc_cm);
    RP((char*)pc + OFF_PC_ViewTarget_Target, &pc_vt);
    LOGF("[Final] Pawn=%p CamMgr=%p VT.Target=%p", pc_pawn, pc_cm, pc_vt);
    LOGF("[Spawn] ===== DONE =====");
}

// ============================================================
// Hooks
// ============================================================
typedef void (*PL_t)(void*, void*);
static PL_t o_PL = nullptr;
static uint64_t g_calls = 0;
static void Hook_PL(void* gm, void* pc) {
    g_calls++;
    LOGF("");
    LOGF("========== [PostLogin #%llu] GM=%p PC=%p ==========",
         (unsigned long long)g_calls, gm, pc);
    g_last_gm = gm;
    g_last_pc = pc;
    InjectPlaylist(gm);
    if (o_PL) o_PL(gm, pc);
}

typedef bool (*SAP_t)(void*, char);
static SAP_t o_SAP = nullptr;
static bool g_spawned = false;
static bool Hook_SAP(void* gm, char a2) {
    LOGF("");
    LOGF("========================================");
    LOGF("[Aircraft] this=%p a2=%d", gm, (int)a2);
    if (!g_spawned && IVP(g_last_pc)) {
        g_spawned = true;
        DoSpawnOnGameThread(gm, g_last_pc);
    }
    bool result = o_SAP ? o_SAP(gm, a2) : false;
    LOGF("[Aircraft] returned: %d", (int)result);
    LOGF("========================================");
    return result;
}

typedef int (*PH_t)(void*);
static PH_t o_PH = nullptr;
static int Hook_PH(void* a1) {
    if (a1) InjectPlaylist(a1);
    return o_PH ? o_PH(a1) : 0;
}
typedef void* (*GGS_t)(void*);
static GGS_t o_GGS = nullptr;
static void* Hook_GGS(void* gm) { return o_GGS ? o_GGS(gm) : nullptr; }

static void Run() {
    LibInfo i{0,0,0};
    for (int k = 0; k < 60; k++) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        i = GetLibInfo(); if (i.base) break;
    }
    if (!i.base) { LOGF("[ERR]"); return; }
    g_base = i.base;
    g_text_start = i.text_start;
    g_text_end = i.text_end;

    LOGF("========================================");
    LOGF(" hook — v59 (SpawnPlayerCameraManager)");
    LOGF(" base=0x%lx", g_base);
    LOGF("========================================");

    DobbyHook((void*)(g_base + ADDR_GetGameState), (void*)Hook_GGS, (void**)&o_GGS);
    DobbyHook((void*)(g_base + ADDR_PlaylistHandler), (void*)Hook_PH, (void**)&o_PH);
    DobbyHook((void*)(g_base + ADDR_StartAircraftPhase), (void*)Hook_SAP, (void**)&o_SAP);
    DobbyHook((void*)(g_base + ADDR_PostLogin), (void*)Hook_PL, (void**)&o_PL);

    SFO = (StaticFindObject_t)(g_base + ADDR_StaticFindObject);
    LOGF("SFO: %p", (void*)SFO);
}

extern "C" JNIEXPORT jint JNICALL
JNI_OnLoad(JavaVM* vm, void* reserved) {
    InitLogFile();
    LOGF(" hook — %s %s", __DATE__, __TIME__);
    std::thread([]{ try { Run(); } catch (...) { LOGF("[ERR]"); } }).detach();
    return JNI_VERSION_1_6;
}
