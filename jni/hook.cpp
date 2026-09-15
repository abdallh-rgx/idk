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
#define ADDR_PlaylistHandler             0x5519710
#define ADDR_StartAircraftPhase          0x550A1F4
#define ADDR_PostLogin                   0x550C610
#define ADDR_sub_92E318C                 0x92E318C

#define VFT_IDX_SpawnDefaultPawnAtTransform 0xDC
#define VFT_IDX_SpawnDefaultPawnFor         0xDE

#define OFF_PC_PlayerState               0x278
#define OFF_GM_GameState                 0x2D0
#define OFF_GS_Playlist2298              0x2298
#define OFF_GS_Playlist2290              0x2290
#define OFF_PS_SpectatorByte             0x27A
#define OFF_GM_DefaultPawnClass          0x330

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

static bool WP(void* a, void* v) {
    struct iovec l = { &v, sizeof(void*) }, r = { a, sizeof(void*) };
    return process_vm_writev(getpid(), &l, 1, &r, 1, 0) == (ssize_t)sizeof(void*);
}

static bool RU8(void* a, uint8_t* v) {
    struct iovec l = { v, 1 }, r = { a, 1 };
    return process_vm_readv(getpid(), &l, 1, &r, 1, 0) == 1;
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
            o->text_end = o->text_start + p.p_vaddr;
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

static bool IVP(void* p) {
    uintptr_t v = (uintptr_t)p;
    return (v >= 0x1000000000ULL && v <= 0x8000000000ULL);
}

typedef void* (*SpawnDefaultPawnAtTransform_t)(void*, void*, void*, void*);

struct FTransform { float qx,qy,qz,qw; float tx,ty,tz,_p1; float sx,sy,sz,_p2; };

static void* GetPawnClass(void* gm) {
    void* pawnClass = nullptr;
    RP((char*)gm + OFF_GM_DefaultPawnClass, &pawnClass);
    if (!IVP(pawnClass) && SFO) {
        pawnClass = SFO(nullptr, nullptr, u"/Script/FortniteGame.FortPlayerPawnAthena", false);
    }
    return pawnClass;
}

static void DirectSpawn(void* gm, void* pc) {
    if (!IVP(gm) || !IVP(pc)) return;

    void** vft = *(void***)gm;
    if (!vft) return;

    auto SpawnAtTransform = (SpawnDefaultPawnAtTransform_t)vft[VFT_IDX_SpawnDefaultPawnAtTransform];
    if (!SpawnAtTransform) return;

    void* pawnClass = GetPawnClass(gm);

    FTransform sky;
    memset(&sky, 0, sizeof(sky));
    sky.qw = 1.0f;
    sky.tx = 0.0f;
    sky.ty = 0.0f;
    sky.tz = 4000.0f;
    sky.sx = sky.sy = sky.sz = 1.0f;

    void* newPawn = SpawnAtTransform(gm, pc, &sky, pawnClass);
    LOGF("[Spawn] Result Pawn = %p", newPawn);
}

typedef void (*sub_92E318C_t)(void* this_ptr, void* new_pawn);
static sub_92E318C_t o_sub_92E318C = nullptr;

static void Hook_sub_92E318C(void* this_ptr, void* new_pawn) {
    void* this_vtable = nullptr;
    void* pawn_vtable = nullptr;
    void* vt_234_func = nullptr;
    void* vt_237_func = nullptr;
    void* ptr_at_2b0   = nullptr;

    if (IVP(this_ptr)) {
        if (RP(this_ptr, &this_vtable) && IVP(this_vtable)) {
            void* slot_234_addr = (char*)this_vtable + 0x750;
            RP(slot_234_addr, &vt_234_func);
        }
        void* field_2b0_addr = (char*)this_ptr + 0x2B0;
        RP(field_2b0_addr, &ptr_at_2b0);
    }

    if (IVP(new_pawn)) {
        if (RP(new_pawn, &pawn_vtable) && IVP(pawn_vtable)) {
            void* slot_237_addr = (char*)pawn_vtable + 0x768;
            RP(slot_237_addr, &vt_237_func);
        }
    }

    LOGF("[PAWN_TRACE] this=%p NewPawn=%p", this_ptr, new_pawn);
    LOGF("[PAWN_TRACE] this_VTable=%p", this_vtable);
    LOGF("[PAWN_TRACE] newPawn_VTable=%p", pawn_vtable);
    LOGF("[PAWN_TRACE] this_VT_234=%p", vt_234_func);
    LOGF("[PAWN_TRACE] pawn_VT_237=%p", vt_237_func);
    LOGF("[PAWN_TRACE] this+0x2B0=%p", ptr_at_2b0);

    if (o_sub_92E318C) {
        o_sub_92E318C(this_ptr, new_pawn);
    }
}

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
        if (!IVP(g_cached_playlist)) return false;
    }
    void* gs = nullptr;
    if (!RP((char*)gm + OFF_GM_GameState, &gs) || !gs) return false;
    *(void**)((char*)gs + OFF_GS_Playlist2298) = g_cached_playlist;
    *(void**)((char*)gs + OFF_GS_Playlist2290) = g_cached_playlist;
    g_injected_gms.insert(gm);
    LOGF("[Inject] Playlist set on GS=%p", gs);
    return true;
}

static void ClearSpectator(void* pc) {
    if (!IVP(pc)) return;
    void* ps = nullptr;
    RP((char*)pc + OFF_PC_PlayerState, &ps);
    if (!IVP(ps)) return;
    uint8_t b = 0;
    if (RU8((char*)ps + OFF_PS_SpectatorByte, &b) && b != 0) {
        WU8((char*)ps + OFF_PS_SpectatorByte, 0x00);
        LOGF("[Spectator] 0x%02X -> 0x00", b);
    }
}

typedef void (*PL_t)(void*, void*);
static PL_t o_PL = nullptr;
static uint64_t g_calls = 0;

static void Hook_PL(void* gm, void* pc) {
    g_calls++;
    LOGF("========== [PostLogin #%llu] GM=%p PC=%p ==========", (unsigned long long)g_calls, gm, pc);

    InjectPlaylist(gm);
    ClearSpectator(pc);

    if (o_PL) o_PL(gm, pc);

    ClearSpectator(pc);

    std::thread([gm, pc]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(1500));
        ClearSpectator(pc);
        DirectSpawn(gm, pc);
    }).detach();

    LOGF("[PostLogin] done");
}

typedef bool (*SAP_t)(void*, char);
static SAP_t o_SAP = nullptr;

static bool Hook_SAP(void* gm, char a2) {
    LOGF("[Aircraft] phase=%d", (int)a2);
    return o_SAP ? o_SAP(gm, a2) : false;
}

typedef int (*PH_t)(void*);
static PH_t o_PH = nullptr;

static int Hook_PH(void* a1) {
    if (a1) InjectPlaylist(a1);
    return o_PH ? o_PH(a1) : 0;
}

static void Run() {
    LibInfo i{0,0,0};
    for (int k = 0; k < 60; k++) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        i = GetLibInfo(); if (i.base) break;
    }
    if (!i.base) { LOGF("[ERR] libUnreal not found"); return; }
    g_base = i.base;

    LOGF("========================================");
    LOGF(" hook v17 - Direct Spawn + Pawn Trace");
    LOGF(" base = 0x%lx", g_base);
    LOGF("========================================");

    SFO = (StaticFindObject_t)(g_base + ADDR_StaticFindObject);
    LOGF(" SFO = %p (RVA=0x%X)", (void*)SFO, ADDR_StaticFindObject);

    DobbyHook((void*)(g_base + ADDR_PlaylistHandler), (void*)Hook_PH, (void**)&o_PH);
    DobbyHook((void*)(g_base + ADDR_StartAircraftPhase), (void*)Hook_SAP, (void**)&o_SAP);
    DobbyHook((void*)(g_base + ADDR_PostLogin), (void*)Hook_PL, (void**)&o_PL);
    DobbyHook((void*)(g_base + ADDR_sub_92E318C), (void*)Hook_sub_92E318C, (void**)&o_sub_92E318C);

    LOGF("[Run] All hooks installed. Waiting for PostLogin...");
}

extern "C" JNIEXPORT jint JNICALL
JNI_OnLoad(JavaVM* vm, void* reserved) {
    InitLogFile();
    LOGF(" hook loaded %s %s", __DATE__, __TIME__);
    std::thread([]{ try { Run(); } catch (...) { LOGF("[ERR] exception in Run"); } }).detach();
    return JNI_VERSION_1_6;
}
