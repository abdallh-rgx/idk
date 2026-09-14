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

#define STATIC_VFT_RVA                   0xBDB8BC0
#define VFT_IDX_SpawnDefaultPawnAtTransform 0xDE

#define OFF_PC_PlayerState               0x278
#define OFF_GM_GameState                 0x2D0
#define OFF_GS_Playlist2298              0x2298
#define OFF_GS_Playlist2290              0x2290
#define OFF_PS_SpectatorByte             0x27A

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

static bool IVP(void* p) {
    uintptr_t v = (uintptr_t)p;
    return (v >= 0x1000000000ULL && v <= 0x8000000000ULL);
}

typedef void* (*SpawnDefaultPawnAtTransform_t)(void*, void*, void*);
static SpawnDefaultPawnAtTransform_t SpawnDefaultPawnAtTransform = nullptr;

struct FTransform { float qx,qy,qz,qw; float tx,ty,tz,_p1; float sx,sy,sz,_p2; };

static void DumpUFunction(void* fn) {
    if (!IVP(fn)) { LOGF("[Dump] UFunction invalid"); return; }

    LOGF("========== UFunction Dump %p ==========", fn);

    for (size_t off = 0x00; off < 0x200; off += 8) {
        void* v = nullptr;
        if (!RP((char*)fn + off, &v)) {
            LOGF("  +0x%03zX = <unreadable>", off);
            continue;
        }

        if (!IVP(v)) {
            if ((uintptr_t)v < 0x1000) {
                LOGF("  +0x%03zX = 0x%lX", off, (unsigned long)(uintptr_t)v);
            } else {
                LOGF("  +0x%03zX = %p", off, v);
            }
            continue;
        }

        uintptr_t rva = (uintptr_t)v - g_base;
        if (rva < 0x20000000ULL) {
            LOGF("  +0x%03zX = %p   (RVA=0x%lX)   <=== FUNCTION CANDIDATE",
                 off, v, (unsigned long)rva);
        } else {
            LOGF("  +0x%03zX = %p   (RVA too large)", off, v);
        }
    }

    LOGF("========== End UFunction Dump ==========");
}

static void* FindSpawnDefaultPawnForFunc() {
    if (!SFO) return nullptr;

    const char16_t* paths[] = {
        u"/Script/FortniteGame.FortGameMode.SpawnDefaultPawnFor",
        u"/Script/FortniteGame.FortGameModeAthena.SpawnDefaultPawnFor",
        u"/Script/FortniteGame.FortGameModeBase.SpawnDefaultPawnFor",
        u"/Script/Engine.GameModeBase.SpawnDefaultPawnFor",
    };

    for (auto p : paths) {
        void* fn = SFO(nullptr, nullptr, p, false);
        if (IVP(fn)) {
            LOGF("[Find] FOUND '%ls' -> %p", p, fn);
            return fn;
        }
        LOGF("[Find] miss '%ls' -> %p", p, fn);
    }
    return nullptr;
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

static void ReadSpawnDefaultPawnAtTransform() {
    void** vft = (void**)(g_base + STATIC_VFT_RVA);
    if (!IVP(vft)) { LOGF("[VFT] invalid"); return; }

    void* at = nullptr;
    if (RP((char*)vft + VFT_IDX_SpawnDefaultPawnAtTransform * sizeof(void*), &at) && IVP(at)) {
        SpawnDefaultPawnAtTransform = (SpawnDefaultPawnAtTransform_t)at;
        LOGF("[VFT] SpawnDefaultPawnAtTransform = %p (RVA=0x%lX)",
             at, (unsigned long)((uintptr_t)at - g_base));
    } else {
        LOGF("[VFT] SpawnDefaultPawnAtTransform read FAILED");
    }
}

typedef void (*PL_t)(void*, void*);
static PL_t o_PL = nullptr;
static uint64_t g_calls = 0;

static void Hook_PL(void* gm, void* pc) {
    g_calls++;
    LOGF("========== [PostLogin #%llu] GM=%p PC=%p ==========",
         (unsigned long long)g_calls, gm, pc);

    InjectPlaylist(gm);
    ClearSpectator(pc);

    if (o_PL) o_PL(gm, pc);

    ClearSpectator(pc);

    // محاولة Spawn يدوية عبر SpawnDefaultPawnAtTransform
    if (IVP(gm) && IVP(pc) && SpawnDefaultPawnAtTransform) {
        FTransform sky;
        memset(&sky, 0, sizeof(sky));
        sky.qw = 1.0f;
        sky.tz = 500.0f;
        sky.sx = sky.sy = sky.sz = 1.0f;

        LOGF("[Spawn] Attempting manual spawn...");
        void* pawn = SpawnDefaultPawnAtTransform(gm, pc, &sky);
        LOGF("[Spawn] pawn = %p", pawn);
    }

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
    LOGF(" hook v13 - UFunction dump only");
    LOGF(" base = 0x%lx", g_base);
    LOGF("========================================");

    SFO = (StaticFindObject_t)(g_base + ADDR_StaticFindObject);
    LOGF(" SFO = %p (RVA=0x%X)", (void*)SFO, ADDR_StaticFindObject);

    ReadSpawnDefaultPawnAtTransform();

    // فقط Dump UFunction - بدون أي Hook عليه
    void* fn = FindSpawnDefaultPawnForFunc();
    LOGF(" SpawnDefaultPawnFor UFunction = %p", fn);
    if (IVP(fn)) {
        DumpUFunction(fn);
    } else {
        LOGF("[WARN] UFunction not found - check paths");
    }

    // فقط الـ hooks الآمنة (كانت تشتغل قبل)
    DobbyHook((void*)(g_base + ADDR_PlaylistHandler), (void*)Hook_PH, (void**)&o_PH);
    DobbyHook((void*)(g_base + ADDR_StartAircraftPhase), (void*)Hook_SAP, (void**)&o_SAP);
    DobbyHook((void*)(g_base + ADDR_PostLogin), (void*)Hook_PL, (void**)&o_PL);
}

extern "C" JNIEXPORT jint JNICALL
JNI_OnLoad(JavaVM* vm, void* reserved) {
    InitLogFile();
    LOGF(" hook loaded %s %s", __DATE__, __TIME__);
    std::thread([]{ try { Run(); } catch (...) { LOGF("[ERR] exception in Run"); } }).detach();
    return JNI_VERSION_1_6;
}
