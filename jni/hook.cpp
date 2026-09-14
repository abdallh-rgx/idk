// ============================================================
// hook — Android JNI (v49: proven path + deep actor dump)
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
#include <vector>
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
#define ADDR_SpawnDefaultPawnAtTransform 0x5511618

#define OFF_UOBJ_CLASSPRIVATE            0x10
#define OFF_UOBJ_NAMEPRIVATE             0x18
#define OFF_UOBJ_OUTERPRIVATE            0x20

static const int kSuperOffs[] = { 0x40, 0x48, 0x38, 0x30, 0x50 };

#define OFF_PC_PlayerState               0x278
#define OFF_PC_Pawn                      0x2B8
#define OFF_PC_AckPawn                   0x2C0
#define OFF_Pawn_Controller              0x2A8
#define OFF_Pawn_Owner                   0x2A0
#define OFF_Pawn_PlayerState             0x2D0
#define OFF_GM_GameState                 0x2D0
#define OFF_GS_Playlist2298              0x2298
#define OFF_GS_Playlist2290              0x2290

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
    if (!a || !o || n == 0 || n > 2048) return false;
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
static std::mutex g_inject_mutex;
static std::set<void*> g_injected_gms;
static void* g_cached_playlist = nullptr;

typedef void* (*StaticFindObject_t)(void*, void*, const char16_t*, bool);
static StaticFindObject_t SFO = nullptr;

static bool VPtr(void* p) {
    uintptr_t v = (uintptr_t)p;
    return (v >= 0x1000000000ULL && v <= 0x8000000000ULL);
}

static void* SFO2(const char16_t* name) {
    if (!SFO) return nullptr;
    void* a = SFO(nullptr, nullptr, name, false);
    if (VPtr(a)) return a;
    void* b = SFO(nullptr, (void*)-1, name, false);
    if (VPtr(b)) return b;
    return nullptr;
}

// Resolve FName to string via GNames? We'll just hex-dump the FName.
static uint64_t ReadFName(void* obj) {
    uint64_t n = 0;
    if (VPtr(obj)) RB((char*)obj + OFF_UOBJ_NAMEPRIVATE, &n, 8);
    return n;
}

// IsSubclassOf via SuperStruct
static bool IsSubclassOf(void* cls, void* target) {
    if (!VPtr(cls) || !VPtr(target)) return false;
    if (cls == target) return true;

    void* cur = cls;
    for (int i = 0; i < 25; i++) {
        void* super = nullptr;
        bool found = false;
        for (int so : kSuperOffs) {
            void* s = nullptr;
            if (RP((char*)cur + so, &s) && VPtr(s)) { super = s; found = true; break; }
        }
        if (!found || !VPtr(super)) break;
        if (super == target) return true;
        cur = super;
    }
    return false;
}

// Read actor location
static bool ReadActorLocation(void* actor, float out[3]) {
    out[0] = out[1] = out[2] = 0;
    if (!VPtr(actor)) return false;
    static const int rootOffs[] = { 0x130, 0x198, 0x1A0, 0x1B0, 0x220, 0x240, 0x250, 0x280 };
    static const int locOffs[]  = { 0x128, 0x1F0, 0x1E0, 0x1D0, 0x250, 0x130, 0x100, 0x110,
                                    0x118, 0x1A8, 0x1B0, 0x1B8 };
    for (int ro : rootOffs) {
        void* root = nullptr;
        if (!RP((char*)actor + ro, &root) || !VPtr(root)) continue;
        for (int lo : locOffs) {
            float xyz[3];
            if (!RB((char*)root + lo, xyz, 12)) continue;
            if (!std::isfinite(xyz[0]) || !std::isfinite(xyz[1]) || !std::isfinite(xyz[2])) continue;
            if (fabsf(xyz[0]) > 500000 || fabsf(xyz[1]) > 500000) continue;
            if (xyz[2] < -2000 || xyz[2] > 20000) continue;
            if (xyz[0] == 0 && xyz[1] == 0 && xyz[2] == 0) continue;
            out[0] = xyz[0]; out[1] = xyz[1]; out[2] = xyz[2];
            return true;
        }
    }
    return false;
}

// ------------------------------------------------------------
// Walk outer chain
// ------------------------------------------------------------
static void* WalkOuter(void* start, int hops) {
    void* cur = start;
    for (int i = 0; i < hops; i++) {
        if (!VPtr(cur)) return nullptr;
        void* outer = nullptr;
        if (!RP((char*)cur + OFF_UOBJ_OUTERPRIVATE, &outer)) return nullptr;
        if (!VPtr(outer)) return nullptr;
        cur = outer;
    }
    return cur;
}

// ------------------------------------------------------------
// Iterate actors in a Level
// ------------------------------------------------------------
static void* IterateLevelActors(void* level, void* targetCls, const char* label, bool dumpMode) {
    if (!VPtr(level)) return nullptr;

    static const int actorsOffs[] = {
        0x98, 0xA0, 0x90, 0xA8, 0xB0, 0xC0, 0x88, 0x150, 0x38, 0x40, 0x48, 0x50
    };

    for (int acOff : actorsOffs) {
        void* data = nullptr;
        int32_t num = 0, max = 0;
        if (!RB((char*)level + acOff, &data, 8)) continue;
        if (!RB((char*)level + acOff + 8, &num, 4)) continue;
        if (!RB((char*)level + acOff + 12, &max, 4)) continue;
        if (!VPtr(data) || num <= 0 || num > 500000) continue;

        LOGF("    [Actors@0x%X] Data=%p Num=%d Max=%d", acOff, data, num, max);

        int lim = num > 50000 ? 50000 : num;
        int found = 0;

        for (int ai = 0; ai < lim; ai++) {
            void* actor = nullptr;
            if (!RP((char*)data + ai * 8, &actor)) continue;
            if (!VPtr(actor)) continue;

            void* ac = nullptr;
            if (!RP((char*)actor + OFF_UOBJ_CLASSPRIVATE, &ac)) continue;
            uint64_t nm = ReadFName(actor);

            if (dumpMode) {
                // Print every actor's class pointer
                LOGF("      A[%d] actor=%p cls=%p name=%llx",
                     ai, actor, ac, (unsigned long long)nm);
                found++;
                if (found >= 200) { LOGF("      ... truncated"); return nullptr; }
            } else if (VPtr(targetCls) && IsSubclassOf(ac, targetCls)) {
                LOGF("      ✅ MATCH %s at [%d] = %p (cls=%p)", label, ai, actor, ac);
                return actor;
            }
        }
    }
    return nullptr;
}

// ------------------------------------------------------------
static void* FindPlaylist() {
    return SFO2(u"/Game/Athena/Playlists/Playlist_DefaultSolo.Playlist_DefaultSolo");
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
    if (!RP((char*)gm + OFF_GM_GameState, &gs) || !gs) return false;
    *(void**)((char*)gs + OFF_GS_Playlist2298) = g_cached_playlist;
    *(void**)((char*)gs + OFF_GS_Playlist2290) = g_cached_playlist;
    LOGF("[Inject] gs+2298/2290 = %p", g_cached_playlist);
    g_injected_gms.insert(gm);
    return true;
}

static void Attach(void* pc, void* pawn) {
    if (!VPtr(pc) || !VPtr(pawn)) return;
    LOGF("[Possess] attach pawn=%p -> pc=%p", pawn, pc);
    WP((char*)pc + OFF_PC_Pawn, pawn);
    WP((char*)pc + OFF_PC_AckPawn, pawn);
    WP((char*)pawn + OFF_Pawn_Controller, pc);
    WP((char*)pawn + OFF_Pawn_Owner, pc);
    void* ps = nullptr;
    if (RP((char*)pc + OFF_PC_PlayerState, &ps) && VPtr(ps)) {
        WP((char*)pawn + OFF_Pawn_PlayerState, ps);
        for (int off = 0x2B8; off <= 0x320; off++) {
            uint8_t b = 0;
            if (RB((char*)ps + off, &b, 1) && b == 0x01) {
                WU8((char*)ps + off, 0x00);
                LOGF("[Possess] cleared PS+0x%X", off);
            }
        }
    }
    LOGF("[Possess] done");
}

static void* ReadPawn(void* pc) {
    void* p = nullptr;
    if (VPtr(pc)) RP((char*)pc + OFF_PC_Pawn, &p);
    return p;
}

struct FTransform { float qx,qy,qz,qw; float tx,ty,tz,_1; float sx,sy,sz,_2; };
static void MakeXform(float x, float y, float z, FTransform* t) {
    memset(t, 0, sizeof(*t));
    t->qw = 1.0f;
    t->tx = x; t->ty = y; t->tz = z;
    t->sx = t->sy = t->sz = 1.0f;
}

// ------------------------------------------------------------
static void SpawnWorker(void* gm, void* pc) {
    LOGF("");
    LOGF("============================================================");
    LOGF("[W] worker started gm=%p pc=%p", gm, pc);
    LOGF("============================================================");

    std::this_thread::sleep_for(std::chrono::milliseconds(3000));

    // ==== 1) Classes ====
    void* worldCls = SFO2(u"World");
    void* levelCls = SFO2(u"Level");
    void* psCls    = SFO2(u"PlayerStart");
    void* fpsCls   = SFO2(u"FortPlayerStartWarmup");
    void* fpCls    = SFO2(u"FortPlayerStart");
    LOGF("[W] classes: World=%p Level=%p PlayerStart=%p FPW=%p FP=%p",
         worldCls, levelCls, psCls, fpsCls, fpCls);

    // ==== 2) Level from PC outer (PROVEN) ====
    void* level = WalkOuter(pc, 1);
    void* world = WalkOuter(pc, 2);
    LOGF("[W] PC->Outer(1)=%p  Outer(2)=%p", level, world);

    // Verify class of level & world
    void* lvlCls = nullptr; if (VPtr(level)) RP((char*)level + OFF_UOBJ_CLASSPRIVATE, &lvlCls);
    void* wrldCls = nullptr; if (VPtr(world)) RP((char*)world + OFF_UOBJ_CLASSPRIVATE, &wrldCls);
    LOGF("[W] level cls=%p (want %p)  world cls=%p (want %p)",
         lvlCls, levelCls, wrldCls, worldCls);

    // ==== 3) Dump actors in this level (find PlayerStart by hierarchy) ====
    LOGF("");
    LOGF("===== Dump Actors in PC's level =====");
    if (VPtr(level)) {
        IterateLevelActors(level, nullptr, "dump", true);
    }

    // ==== 4) Search for PlayerStart in PC's level ====
    void* psActor = nullptr;
    LOGF("");
    LOGF("===== Search PlayerStart =====");
    if (VPtr(level)) {
        if (VPtr(psCls))  psActor = IterateLevelActors(level, psCls, "PlayerStart", false);
        if (!psActor && VPtr(fpsCls)) psActor = IterateLevelActors(level, fpsCls, "FPW", false);
        if (!psActor && VPtr(fpCls))  psActor = IterateLevelActors(level, fpCls,  "FP", false);
    }

    // ==== 5) Also try World->Levels ====
    if (!psActor && VPtr(world)) {
        LOGF("");
        LOGF("===== Try World->Levels =====");
        static const int levelsOffs[] = { 0x1D0, 0x1C0, 0x1E0, 0x1F0, 0x2C0, 0x2A0 };
        for (int lvOff : levelsOffs) {
            void* data = nullptr; int32_t num = 0, max = 0;
            if (!RB((char*)world + lvOff, &data, 8)) continue;
            if (!RB((char*)world + lvOff + 8, &num, 4)) continue;
            if (!RB((char*)world + lvOff + 12, &max, 4)) continue;
            if (!VPtr(data) || num <= 0 || num > 500) continue;
            LOGF("  [World->Levels@0x%X] Num=%d", lvOff, num);
            for (int li = 0; li < num && li < 100; li++) {
                void* l = nullptr;
                if (!RP((char*)data + li * 8, &l)) continue;
                if (!VPtr(l)) continue;
                LOGF("    L[%d] = %p", li, l);
                if (VPtr(psCls))  psActor = IterateLevelActors(l, psCls, "PlayerStart", false);
                if (!psActor && VPtr(fpsCls)) psActor = IterateLevelActors(l, fpsCls, "FPW", false);
                if (!psActor && VPtr(fpCls))  psActor = IterateLevelActors(l, fpCls,  "FP", false);
                if (psActor) break;
            }
            if (psActor) break;
        }
    }

    LOGF("");
    LOGF("[W] PlayerStart actor = %p", psActor);

    // ==== 6) Location ====
    float spawnX = 0, spawnY = 0, spawnZ = 5000;
    if (VPtr(psActor)) {
        float loc[3];
        if (ReadActorLocation(psActor, loc)) {
            spawnX = loc[0]; spawnY = loc[1]; spawnZ = loc[2] + 200;
            LOGF("[W] spawn from PlayerStart: (%.0f, %.0f, %.0f)", spawnX, spawnY, spawnZ);
        }
    } else {
        LOGF("[W] using fallback loc (%.0f, %.0f, %.0f)", spawnX, spawnY, spawnZ);
    }

    // ==== 7) Try spawn ====
    typedef void* (*SDPAT_t)(void*, void*, void*);
    SDPAT_t fn = (SDPAT_t)(g_base + ADDR_SpawnDefaultPawnAtTransform);

    for (int i = 0; i < 8; i++) {
        void* ex = ReadPawn(pc);
        if (VPtr(ex)) { LOGF("[W] pawn exists: %p", ex); Attach(pc, ex); return; }

        LOGF("[W] attempt %d loc=(%.0f, %.0f, %.0f)", i + 1, spawnX, spawnY, spawnZ + i * 200.0f);
        FTransform t;
        MakeXform(spawnX, spawnY, spawnZ + i * 200.0f, &t);
        void* pawn = fn(gm, pc, &t);
        LOGF("[W] spawn -> %p", pawn);
        if (VPtr(pawn)) { Attach(pc, pawn); return; }
        std::this_thread::sleep_for(std::chrono::milliseconds(700));
    }
    LOGF("[W] gave up");
}

// ------------------------------------------------------------
typedef void (*PostLogin_t)(void*, void*);
static PostLogin_t o_PostLogin = nullptr;
static uint64_t g_calls = 0;

static void Hook_PostLogin(void* gm, void* pc) {
    g_calls++;
    LOGF("");
    LOGF("========== [PostLogin #%llu] GM=%p PC=%p ==========",
         (unsigned long long)g_calls, gm, pc);
    InjectPlaylist(gm);
    if (o_PostLogin) o_PostLogin(gm, pc);
    if (!VPtr(gm) || !VPtr(pc)) return;
    if (g_calls == 1) std::thread(SpawnWorker, gm, pc).detach();
}

typedef int (*PH_t)(void*);
static PH_t o_PH = nullptr;
static int Hook_PH(void* a1) { if (a1) InjectPlaylist(a1); return o_PH ? o_PH(a1) : 0; }

typedef bool (*SAP_t)(void*, char);
static SAP_t o_SAP = nullptr;
static bool Hook_SAP(void* t, char a2) {
    LOGF("[Aircraft] this=%p a2=%d", t, (int)a2);
    InjectPlaylist(t);
    return o_SAP ? o_SAP(t, a2) : false;
}

typedef void* (*GGS_t)(void*);
static GGS_t o_GGS = nullptr;
static void* Hook_GGS(void* gm) { return o_GGS ? o_GGS(gm) : nullptr; }

// ------------------------------------------------------------
static void Run() {
    LibInfo i{0,0,0};
    for (int k = 0; k < 60; k++) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        i = GetLibInfo(); if (i.base) break;
    }
    if (!i.base) { LOGF("[ERR] libUnreal not found"); return; }
    g_base = i.base;
    LOGF("========================================");
    LOGF(" hook — v49 (proven path + deep dump)");
    LOGF(" base=0x%lx", g_base);
    LOGF("========================================");

    DobbyHook((void*)(g_base + ADDR_GetGameState), (void*)Hook_GGS, (void**)&o_GGS);
    DobbyHook((void*)(g_base + ADDR_PlaylistHandler), (void*)Hook_PH, (void**)&o_PH);
    DobbyHook((void*)(g_base + ADDR_StartAircraftPhase), (void*)Hook_SAP, (void**)&o_SAP);
    DobbyHook((void*)(g_base + ADDR_PostLogin), (void*)Hook_PostLogin, (void**)&o_PostLogin);

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
    std::thread([]{ try { Run(); } catch (...) { LOGF("[ERR]"); } }).detach();
    return JNI_VERSION_1_6;
}
