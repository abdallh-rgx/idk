// ============================================================
// hook — Android JNI (v45: PlayerStart-based spawn)
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

#define ADDR_StaticFindObject            0x762A664
#define ADDR_GetGameState                0x55B8AB0
#define ADDR_PlaylistHandler             0x5519710
#define ADDR_StartAircraftPhase          0x550A1F4
#define ADDR_PostLogin                   0x550C610
#define ADDR_SpawnDefaultPawnAtTransform 0x5511618

#define OFF_PC_PlayerState               0x278
#define OFF_PC_Pawn                      0x2B8
#define OFF_PC_AckPawn                   0x2C0
#define OFF_Pawn_Controller              0x2A8
#define OFF_Pawn_Owner                   0x2A0
#define OFF_Pawn_PlayerState             0x2D0
#define OFF_GM_GameState                 0x2D0
#define OFF_GS_Playlist2298              0x2298
#define OFF_GS_Playlist2290              0x2290

// Fallback spawn Z for Artemis map (well above terrain)
static const float kFallbackX = 0.0f;
static const float kFallbackY = 0.0f;
static const float kFallbackZ = 5000.0f;

// ------------------------------------------------------------
static std::string u16(const char16_t* s) {
    if (!s) return "";
    std::string r;
    while (*s) { uint16_t c = *s++; r += (c < 0x80) ? (char)c : '?'; }
    return r;
}

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
    if (!a || !o || n == 0 || n > 512) return false;
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

// ------------------------------------------------------------
// Try to find PlayerStart actor
// ------------------------------------------------------------
static void* FindAnyPlayerStart() {
    LOGF("");
    LOGF("========== Find PlayerStart ==========");

    static const char16_t* names[] = {
        u"PlayerStart_0",
        u"PlayerStart_1",
        u"PlayerStart_2",
        u"PlayerStart_3",
        u"PlayerStart_4",
        u"PlayerStart_5",
        u"PlayerStart_6",
        u"PlayerStart_7",
        u"Athena_PlayerStart_C_0",
        u"Athena_PlayerStart_C_1",
        u"Athena_PlayerStart_0",
        u"Athena_PlayerStart_1",
        u"FortPlayerStart_Athena_0",
        u"FortPlayerStart_Athena_1",
        u"PlayerStart_C_0",
        u"PlayerStart",
        u"PlayerStart_C",
        u"Athena_PlayerStart",
        u"Athena_PlayerStart_C",
    };

    for (auto n : names) {
        void* a = SFO(nullptr, nullptr, n, false);
        void* b = SFO(nullptr, (void*)-1, n, false);
        if (VPtr(a) || VPtr(b)) {
            void* result = VPtr(a) ? a : b;
            LOGF("  ✅ %s -> %p", u16(n).c_str(), result);
            return result;
        }
        LOGF("  ❌ %s", u16(n).c_str());
    }

    LOGF("========== no PlayerStart found ==========");
    return nullptr;
}

// ------------------------------------------------------------
// Read actor location (try many offsets)
// ------------------------------------------------------------
static bool ReadActorLocation(void* actor, float out[3]) {
    out[0] = out[1] = out[2] = 0;
    if (!VPtr(actor)) return false;

    // RootComponent offsets (common UE5)
    static const int rootOffs[] = {
        0x130, 0x198, 0x1A0, 0x1B0, 0x220, 0x240, 0x250
    };

    // ComponentToWorld.Translation offsets (usually +0x10 inside ComponentToWorld)
    // But the transform offset inside USceneComponent varies
    static const int locOffs[] = {
        0x128, 0x1F0, 0x1E0, 0x1D0, 0x250, 0x130, 0x100, 0x110
    };

    for (int ro : rootOffs) {
        void* root = nullptr;
        if (!RP((char*)actor + ro, &root)) continue;
        if (!VPtr(root)) continue;

        for (int lo : locOffs) {
            float xyz[3];
            if (!RB((char*)root + lo, xyz, 12)) continue;

            // Sanity checks for Artemis map coordinates
            // Island spans roughly -100000..100000 X/Y, Z roughly -500..5000
            bool ok = true;
            if (!std::isfinite(xyz[0]) || !std::isfinite(xyz[1]) || !std::isfinite(xyz[2])) ok = false;
            if (fabsf(xyz[0]) > 500000.0f) ok = false;
            if (fabsf(xyz[1]) > 500000.0f) ok = false;
            if (xyz[2] < -2000.0f || xyz[2] > 20000.0f) ok = false;
            if (xyz[0] == 0 && xyz[1] == 0 && xyz[2] == 0) ok = false;

            if (ok) {
                out[0] = xyz[0]; out[1] = xyz[1]; out[2] = xyz[2];
                LOGF("  [loc] root@0x%X loc@0x%X = (%.0f, %.0f, %.0f)",
                     ro, lo, out[0], out[1], out[2]);
                return true;
            }
        }
    }

    LOGF("  [loc] no valid location for actor %p", actor);
    return false;
}

// ------------------------------------------------------------
static void* FindPlaylist() {
    if (!SFO) return nullptr;
    return SFO(nullptr, nullptr, u"/Game/Athena/Playlists/Playlist_DefaultSolo.Playlist_DefaultSolo", false);
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

// ------------------------------------------------------------
// FTransform
// ------------------------------------------------------------
struct FTransform {
    float qx, qy, qz, qw;
    float tx, ty, tz, _p1;
    float sx, sy, sz, _p2;
};

static void MakeIdentityAt(float x, float y, float z, FTransform* t) {
    memset(t, 0, sizeof(*t));
    t->qw = 1.0f;
    t->tx = x; t->ty = y; t->tz = z;
    t->sx = t->sy = t->sz = 1.0f;
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

    std::thread([gm, pc] {
        std::this_thread::sleep_for(std::chrono::milliseconds(3000));

        // 1) Find PlayerStart
        void* ps_actor = FindAnyPlayerStart();

        float spawnX = kFallbackX, spawnY = kFallbackY, spawnZ = kFallbackZ;
        bool gotLoc = false;

        if (VPtr(ps_actor)) {
            float loc[3];
            if (ReadActorLocation(ps_actor, loc)) {
                spawnX = loc[0]; spawnY = loc[1]; spawnZ = loc[2] + 100.0f;
                gotLoc = true;
                LOGF("[spawn] using PlayerStart loc (%.0f, %.0f, %.0f)",
                     spawnX, spawnY, spawnZ);
            }
        }

        if (!gotLoc) {
            LOGF("[spawn] fallback to default (%.0f, %.0f, %.0f)",
                 spawnX, spawnY, spawnZ);
        }

        // 2) Try spawn
        typedef void* (*SDPAT_t)(void*, void*, void*);
        SDPAT_t fn = (SDPAT_t)(g_base + ADDR_SpawnDefaultPawnAtTransform);

        for (int i = 0; i < 8; i++) {
            void* ex = ReadPawn(pc);
            if (VPtr(ex)) {
                LOGF("[W] pawn exists: %p", ex);
                Attach(pc, ex);
                return;
            }

            // Slightly vary Z each attempt
            float z = spawnZ + (i * 200.0f);
            LOGF("[W] attempt %d loc=(%.0f, %.0f, %.0f)", i + 1, spawnX, spawnY, z);

            FTransform t;
            MakeIdentityAt(spawnX, spawnY, z, &t);

            void* pawn = fn(gm, pc, &t);
            LOGF("[W] spawn -> %p", pawn);
            if (VPtr(pawn)) {
                Attach(pc, pawn);
                return;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(700));
        }
        LOGF("[W] gave up");
    }).detach();
}

// ------------------------------------------------------------
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
    LOGF(" hook — v45 (PlayerStart-based spawn)");
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
