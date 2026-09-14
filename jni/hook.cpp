// ============================================================
// hook — Android JNI (v50: clear spectator + force pawn class)
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

#define OFF_UOBJ_CLASSPRIVATE            0x10
#define OFF_UOBJ_OUTERPRIVATE            0x20

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
static std::mutex g_inject_mutex;
static std::set<void*> g_injected_gms;
static void* g_cached_playlist = nullptr;
static void* g_pawn_class = nullptr;

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

// ------------------------------------------------------------
// Try to find valid Pawn Class
// ------------------------------------------------------------
static void FindPawnClassExhaustive() {
    if (VPtr(g_pawn_class)) return;
    LOGF("");
    LOGF("========== Find Pawn Class ==========");

    static const char16_t* names[] = {
        u"FortPlayerPawnAthena",
        u"FortPlayerPawnAthena_C",
        u"FortPlayerPawn",
        u"FortPlayerPawn_C",
        u"Athena_PlayerPawn_C",
        u"BP_PlayerPawn_Athena_C",
        u"PlayerPawn_Athena_C",
        u"DefaultPawn",
        u"DefaultPawn_C",
        u"Pawn",
    };

    for (auto n : names) {
        void* a = SFO(nullptr, nullptr, n, false);
        void* b = SFO(nullptr, (void*)-1, n, false);
        void* c = VPtr(a) ? a : (VPtr(b) ? b : nullptr);
        LOGF("  %-32s -> %p", (n ? "name" : ""), c);
        if (c && !g_pawn_class) {
            g_pawn_class = c;
        }
    }

    LOGF("========== chose: %p ==========", g_pawn_class);
}

// ------------------------------------------------------------
// Find GameMode's DefaultPawnClass field (scan pointers whose target is UClass)
// ------------------------------------------------------------
static void ScanGameModeForPawnClassField(void* gm) {
    if (!VPtr(gm)) return;
    LOGF("");
    LOGF("--- GM DefaultPawnClass scan [0x280..0x480] ---");
    for (int off = 0x280; off < 0x480; off += 8) {
        void* v = nullptr;
        if (!RP((char*)gm + off, &v)) continue;
        if (!VPtr(v)) continue;

        // Read target's class (is it UClass?)
        void* tgt_cls = nullptr;
        RP((char*)v + OFF_UOBJ_CLASSPRIVATE, &tgt_cls);

        // Get name of target's class
        void* clsname_obj = nullptr;
        if (VPtr(tgt_cls)) RP((char*)tgt_cls + OFF_UOBJ_OUTERPRIVATE, &clsname_obj);

        LOGF("  GM+0x%X = %p  (tgt_cls=%p)",
             off, v, tgt_cls);

        // If GM+off is a UClass, this might be DefaultPawnClass
        // Compare to known g_pawn_class
        if (g_pawn_class && v == g_pawn_class) {
            LOGF("    ^^^ MATCHES g_pawn_class! This IS DefaultPawnClass @ +0x%X", off);
        }
    }
}

// ------------------------------------------------------------
// Clear PlayerState spectator flags (scan bytes)
// ------------------------------------------------------------
static void ClearSpectatorFlags(void* ps, const char* tag) {
    if (!VPtr(ps)) return;
    LOGF("");
    LOGF("--- Clear Spectator Flags PS=%p [%s] ---", ps, tag);

    // Dump bytes 0x280..0x3A0
    int cleared = 0;
    for (int off = 0x280; off < 0x3A0; off++) {
        uint8_t b = 0;
        if (!RU8((char*)ps + off, &b)) continue;

        // Spectator bits pattern: only low 4 bits matter
        // We look for bytes with bit1 (0x02) or bit2 (0x04) set
        // but NOT high bits (0xF0) to avoid false positives
        bool lowOnly = (b & 0xF0) == 0;
        bool hasSpecBit = (b & 0x06) != 0;
        bool notTooMany = (b & 0xF9) == 0; // only bits 1,2 possibly set

        if (lowOnly && hasSpecBit && notTooMany && b != 0) {
            LOGF("  PS+0x%X = 0x%02X  ->  clear bits 1,2", off, b);
            uint8_t nb = b & ~0x06;
            WU8((char*)ps + off, nb);
            cleared++;
        }
    }

    if (cleared == 0) LOGF("  no candidate bytes found");
    LOGF("  cleared=%d", cleared);
}

// ------------------------------------------------------------
// Playlist
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

// ------------------------------------------------------------
// Attach Pawn
// ------------------------------------------------------------
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
// Spawn worker
// ------------------------------------------------------------
static void SpawnWorker(void* gm, void* pc) {
    LOGF("");
    LOGF("============================================================");
    LOGF("[W] worker gm=%p pc=%p", gm, pc);
    LOGF("============================================================");

    std::this_thread::sleep_for(std::chrono::milliseconds(3000));

    // 1) Find pawn class
    FindPawnClassExhaustive();

    // 2) Dump GameMode fields
    ScanGameModeForPawnClassField(gm);

    // 3) Clear PS spectator flags
    void* ps = nullptr;
    if (RP((char*)pc + OFF_PC_PlayerState, &ps) && VPtr(ps)) {
        ClearSpectatorFlags(ps, "playerstate");
    }

    // 4) Try spawn
    typedef void* (*SDPAT_t)(void*, void*, void*);
    SDPAT_t fn = (SDPAT_t)(g_base + ADDR_SpawnDefaultPawnAtTransform);

    for (int i = 0; i < 8; i++) {
        void* ex = ReadPawn(pc);
        if (VPtr(ex)) {
            LOGF("[W] pawn exists: %p", ex);
            Attach(pc, ex);
            return;
        }

        // Re-clear spectator flags each attempt (in case game set them again)
        if (VPtr(ps)) ClearSpectatorFlags(ps, "retry");

        LOGF("[W] attempt %d", i + 1);
        FTransform t;
        MakeXform(0, 0, 5000 + i * 200.0f, &t);
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
    LOGF(" hook — v50 (clear spectator + force class)");
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
