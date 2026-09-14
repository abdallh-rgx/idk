// ============================================================
// hook — Android JNI (v46: enumerate PlayerStart actors)
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

#define OFF_PC_PlayerState               0x278
#define OFF_PC_Pawn                      0x2B8
#define OFF_PC_AckPawn                   0x2C0
#define OFF_Pawn_Controller              0x2A8
#define OFF_Pawn_Owner                   0x2A0
#define OFF_Pawn_PlayerState             0x2D0
#define OFF_GM_GameState                 0x2D0
#define OFF_GS_Playlist2298              0x2298
#define OFF_GS_Playlist2290              0x2290

// UObject::OuterPrivate offset (UE5)
#define OFF_UOBJ_OUTER                   0x20
#define OFF_UOBJ_CLASS                   0x10
#define OFF_UOBJ_NAME                    0x18

// Common World offsets to try
static const int kWorldLevelsOffsets[]  = { 0x1D0, 0x1C0, 0x1E0, 0x2C0, 0x2A0, 0x250, 0x130, 0x290, 0x2B0, 0x1F0 };
// Common Level->Actors offsets
static const int kLevelActorsOffsets[]  = { 0x98, 0xA0, 0x90, 0xA8, 0xB0, 0xC0, 0x88 };

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

// Safe memory
static bool RP(void* a, void** o) {
    *o = nullptr; if (!a) return false;
    struct iovec l = { o, sizeof(void*) }, r = { a, sizeof(void*) };
    return process_vm_readv(getpid(), &l, 1, &r, 1, 0) == (ssize_t)sizeof(void*);
}
static bool RB(void* a, void* o, size_t n) {
    if (!a || !o || n == 0 || n > 1024) return false;
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
static void* g_playerstart_class = nullptr;

typedef void* (*StaticFindObject_t)(void*, void*, const char16_t*, bool);
static StaticFindObject_t SFO = nullptr;

static bool VPtr(void* p) {
    uintptr_t v = (uintptr_t)p;
    return (v >= 0x1000000000ULL && v <= 0x8000000000ULL);
}

// GetName
struct FString16 { char16_t* Data; int32_t Num; int32_t Max; ~FString16() {} };
typedef FString16 (*GetName_t)(void*);

static std::string ObjName(void* obj) {
    if (!VPtr(obj)) return "<bad>";
    void** vft = nullptr;
    if (!RP(obj, (void**)&vft)) return "<no vft>";
    if (!VPtr(vft)) return "<bad vft>";
    void* fn = nullptr;
    if (!RP((char*)vft + 0x38, &fn)) return "<no fn>";
    if (!VPtr(fn)) return "<bad fn>";
    GetName_t gn = (GetName_t)fn;
    FString16 out = gn(obj);
    std::string r;
    if (out.Data && out.Num > 0) {
        int n = out.Num > 128 ? 128 : out.Num;
        for (int i = 0; i < n; i++) {
            uint16_t c = out.Data[i];
            if (!c) break;
            r += (c < 0x80) ? (char)c : '?';
        }
    }
    return r;
}

// Find World by walking outer chain
static void* FindWorldFrom(void* obj) {
    LOGF("");
    LOGF("========== Walk outer chain from %p ==========", obj);
    void* cur = obj;
    for (int i = 0; i < 8; i++) {
        if (!VPtr(cur)) break;
        std::string nm = ObjName(cur);
        LOGF("  outer[%d] %p  %s", i, cur, nm.c_str());

        if (nm.find("World") != std::string::npos ||
            nm.find("Terrain") != std::string::npos ||
            nm.find("Artemis") != std::string::npos) {
            LOGF("  ✅ Found World: %p (%s)", cur, nm.c_str());
            return cur;
        }

        void* outer = nullptr;
        if (!RP((char*)cur + OFF_UOBJ_OUTER, &outer)) break;
        if (!VPtr(outer)) break;
        cur = outer;
    }
    LOGF("  ❌ World not found in outer chain");
    return nullptr;
}

// Read TArray (UE5: TArray64 is different, classic TArray is 16 bytes)
struct TArrayNative {
    void* Data;
    int32_t Num;
    int32_t Max;
};

// Enumerate actors of a class
static void* FindActorOfClass(void* world, void* targetClass, const char* label) {
    if (!VPtr(world) || !VPtr(targetClass)) return nullptr;

    LOGF("");
    LOGF("========== FindActorOfClass(%s) world=%p class=%p ==========",
         label, world, targetClass);

    for (int lvOff : kWorldLevelsOffsets) {
        TArrayNative levels;
        if (!RB((char*)world + lvOff, &levels, sizeof(levels))) continue;
        if (levels.Num <= 0 || levels.Num > 200) continue;
        if (!VPtr(levels.Data)) continue;

        LOGF("  [try Levels@0x%X] Data=%p Num=%d Max=%d",
             lvOff, levels.Data, levels.Num, levels.Max);

        int levelsN = levels.Num > 50 ? 50 : levels.Num;
        for (int li = 0; li < levelsN; li++) {
            void* lvl = nullptr;
            if (!RP((char*)levels.Data + li * 8, &lvl)) continue;
            if (!VPtr(lvl)) continue;

            for (int acOff : kLevelActorsOffsets) {
                TArrayNative actors;
                if (!RB((char*)lvl + acOff, &actors, sizeof(actors))) continue;
                if (actors.Num <= 0 || actors.Num > 500000) continue;
                if (!VPtr(actors.Data)) continue;

                LOGF("    [L%d] Actors@0x%X Data=%p Num=%d",
                     li, acOff, actors.Data, actors.Num);

                int actorsN = actors.Num > 20000 ? 20000 : actors.Num;
                int found = 0;
                for (int ai = 0; ai < actorsN; ai++) {
                    void* actor = nullptr;
                    if (!RP((char*)actors.Data + ai * 8, &actor)) continue;
                    if (!VPtr(actor)) continue;

                    void* actorClass = nullptr;
                    if (!RP((char*)actor + OFF_UOBJ_CLASS, &actorClass)) continue;
                    if (actorClass == targetClass) {
                        LOGF("      ✅ MATCH actor=%p", actor);
                        return actor;
                    }
                    // Also check via IsA? For now compare direct + log first few
                    if (ai < 3) {
                        std::string cls = ObjName(actorClass);
                        std::string nm = ObjName(actor);
                        LOGF("        [%d] actor=%p class=%p(%s) name=%s",
                             ai, actor, actorClass, cls.c_str(), nm.c_str());
                    }
                    found = 0;
                }
                (void)found;
            }
        }
    }
    LOGF("  ❌ no actor of target class");
    return nullptr;
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

struct FTransform { float qx,qy,qz,qw; float tx,ty,tz,_1; float sx,sy,sz,_2; };

static void MakeXform(float x, float y, float z, FTransform* t) {
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

        // 1) Find PlayerStart UClass
        void* psClass = SFO(nullptr, nullptr, u"PlayerStart", false);
        if (!VPtr(psClass)) psClass = SFO(nullptr, (void*) -1, u"PlayerStart", false);
        LOGF("[spawn] PlayerStart class = %p", psClass);
        g_playerstart_class = psClass;

        // 2) Find World by walking outer chain from PC then from GameState
        void* world = FindWorldFrom(pc);
        if (!world) {
            void* gs = nullptr;
            if (RP((char*)gm + OFF_GM_GameState, &gs) && VPtr(gs)) {
                world = FindWorldFrom(gs);
            }
        }
        if (!world) {
            void* gs = nullptr;
            if (RP((char*)gm + OFF_GM_GameState, &gs) && VPtr(gs)) {
                world = FindWorldFrom(gs);
            }
        }
        LOGF("[spawn] World = %p", world);

        // 3) Find a PlayerStart actor
        void* ps_actor = nullptr;
        if (world && VPtr(psClass)) {
            ps_actor = FindActorOfClass(world, psClass, "PlayerStart");
        }
        LOGF("[spawn] PlayerStart actor = %p", ps_actor);

        // 4) Read location
        float spawnX = 0, spawnY = 0, spawnZ = 5000;
        if (VPtr(ps_actor)) {
            static const int rootOffs[] = { 0x130, 0x198, 0x1A0, 0x1B0, 0x220, 0x240, 0x250 };
            static const int locOffs[]  = { 0x128, 0x1F0, 0x1E0, 0x1D0, 0x250, 0x130, 0x100, 0x110 };
            bool got = false;
            for (int ro : rootOffs) {
                void* root = nullptr;
                if (!RP((char*)ps_actor + ro, &root) || !VPtr(root)) continue;
                for (int lo : locOffs) {
                    float xyz[3];
                    if (!RB((char*)root + lo, xyz, 12)) continue;
                    if (!std::isfinite(xyz[0]) || !std::isfinite(xyz[1]) || !std::isfinite(xyz[2])) continue;
                    if (fabsf(xyz[0]) > 500000 || fabsf(xyz[1]) > 500000) continue;
                    if (xyz[2] < -2000 || xyz[2] > 20000) continue;
                    if (xyz[0] == 0 && xyz[1] == 0 && xyz[2] == 0) continue;
                    spawnX = xyz[0]; spawnY = xyz[1]; spawnZ = xyz[2] + 100;
                    LOGF("[spawn] loc from root@0x%X loc@0x%X = (%.0f, %.0f, %.0f)",
                         ro, lo, spawnX, spawnY, spawnZ);
                    got = true;
                    break;
                }
                if (got) break;
            }
            if (!got) LOGF("[spawn] could not read PlayerStart location");
        }

        LOGF("[spawn] using loc = (%.0f, %.0f, %.0f)", spawnX, spawnY, spawnZ);

        // 5) Try spawn
        typedef void* (*SDPAT_t)(void*, void*, void*);
        SDPAT_t fn = (SDPAT_t)(g_base + ADDR_SpawnDefaultPawnAtTransform);

        for (int i = 0; i < 8; i++) {
            void* ex = ReadPawn(pc);
            if (VPtr(ex)) { LOGF("[W] pawn exists: %p", ex); Attach(pc, ex); return; }

            LOGF("[W] attempt %d", i + 1);
            FTransform t;
            MakeXform(spawnX, spawnY, spawnZ + i * 200.0f, &t);
            void* pawn = fn(gm, pc, &t);
            LOGF("[W] spawn -> %p", pawn);
            if (VPtr(pawn)) { Attach(pc, pawn); return; }
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
    LOGF(" hook — v46 (enumerate PlayerStart)");
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
