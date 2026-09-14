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

#define OFF_GM_GLOBAL                    0xBDB8BC0
#define VFT_IDX_SpawnDefaultPawnFor      0xDD
#define VFT_IDX_SpawnDefaultPawnAtTransform 0xDE

#define OFF_PC_PlayerState               0x278
#define OFF_GM_GameState                 0x2D0
#define OFF_GS_Playlist2298              0x2298
#define OFF_GS_Playlist2290              0x2290
#define OFF_PS_SpectatorByte             0x27A

#define SKY_Z                            10000.0f

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
static void* g_gmObj = nullptr;

typedef void* (*StaticFindObject_t)(void*, void*, const char16_t*, bool);
static StaticFindObject_t SFO = nullptr;

static bool IVP(void* p) {
    uintptr_t v = (uintptr_t)p;
    return (v >= 0x1000000000ULL && v <= 0x8000000000ULL);
}

static bool VFTHook(void* obj, size_t vft_index, void* hook_fn, void** orig_out) {
    if (!obj || !hook_fn) return false;

    void** vtable = *reinterpret_cast<void***>(obj);
    if (!vtable) return false;

    void** slot = &vtable[vft_index];

    if (orig_out) *orig_out = *slot;

    const size_t page_size = static_cast<size_t>(sysconf(_SC_PAGESIZE));
    uintptr_t addr = reinterpret_cast<uintptr_t>(slot);
    uintptr_t page_start = addr & ~(page_size - 1);

    size_t span = page_size;
    if ((addr + sizeof(void*)) > (page_start + page_size)) span += page_size;

    if (mprotect(reinterpret_cast<void*>(page_start), span,
                 PROT_READ | PROT_WRITE | PROT_EXEC) != 0) {
        return false;
    }

    *slot = hook_fn;

    mprotect(reinterpret_cast<void*>(page_start), span, PROT_READ | PROT_EXEC);

    __builtin___clear_cache(reinterpret_cast<char*>(page_start),
                            reinterpret_cast<char*>(page_start + span));
    return true;
}

struct FTransform { float qx,qy,qz,qw; float tx,ty,tz,_p1; float sx,sy,sz,_p2; };

static void MakeSkyXform(FTransform* t) {
    memset(t, 0, sizeof(*t));
    t->qw = 1.0f;
    t->tx = 0.0f;
    t->ty = 0.0f;
    t->tz = SKY_Z;
    t->sx = t->sy = t->sz = 1.0f;
}

typedef void* (*SpawnDefaultPawnFor_t)(void*, void*, void*);
static SpawnDefaultPawnFor_t o_SpawnDefaultPawnFor = nullptr;

typedef void* (*SpawnDefaultPawnAtTransform_t)(void*, void*, void*);
static SpawnDefaultPawnAtTransform_t SpawnDefaultPawnAtTransform = nullptr;

static void* Hook_SpawnDefaultPawnFor(void* gm, void* controller, void* startSpot) {
    LOGF("");
    LOGF("========================================");
    LOGF("[SpawnDefaultPawnFor] gm=%p ctrl=%p spot=%p", gm, controller, startSpot);

    if (IVP(gm) && IVP(controller) && SpawnDefaultPawnAtTransform) {
        FTransform sky;
        MakeSkyXform(&sky);
        LOGF("[Sky] Forcing spawn at Z=%.1f", sky.tz);

        void* pawn = SpawnDefaultPawnAtTransform(gm, controller, &sky);
        if (IVP(pawn)) {
            LOGF("[Sky] SUCCESS Pawn=%p", pawn);
            LOGF("========================================");
            return pawn;
        }
        LOGF("[Sky] FAILED -> fallback to original");
    }

    void* res = o_SpawnDefaultPawnFor ? o_SpawnDefaultPawnFor(gm, controller, startSpot) : nullptr;
    LOGF("[SpawnDefaultPawnFor] fallback Pawn=%p", res);
    LOGF("========================================");
    return res;
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
        LOGF("[Spectator] 0x%02X -> 0x00 (PS=%p)", b, ps);
    }
}

typedef void (*PL_t)(void*, void*);
static PL_t o_PL = nullptr;
static uint64_t g_calls = 0;

static void Hook_PL(void* gm, void* pc) {
    g_calls++;
    LOGF("");
    LOGF("========== [PostLogin #%llu] GM=%p PC=%p ==========",
         (unsigned long long)g_calls, gm, pc);
    InjectPlaylist(gm);
    ClearSpectator(pc);
    if (o_PL) o_PL(gm, pc);
    ClearSpectator(pc);
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

static void* GetGameMode() {
    void* p = nullptr;
    if (RP((void*)(g_base + OFF_GM_GLOBAL), &p) && IVP(p)) {
        LOGF("[GM] pointer-read -> %p", p);
        return p;
    }
    typedef void* (*Getter_t)();
    Getter_t getter = (Getter_t)(g_base + OFF_GM_GLOBAL);
    void* gm = getter();
    if (IVP(gm)) {
        LOGF("[GM] getter-call -> %p", gm);
        return gm;
    }
    return nullptr;
}

static void ApplyVftHooks() {
    if (!IVP(g_gmObj)) {
        LOGF("[VFT] GameMode invalid, aborting");
        return;
    }

    void** vft = *reinterpret_cast<void***>(g_gmObj);
    if (!vft) {
        LOGF("[VFT] vft NULL");
        return;
    }
    LOGF("[VFT] vft=%p", (void*)vft);

    SpawnDefaultPawnAtTransform = (SpawnDefaultPawnAtTransform_t)vft[VFT_IDX_SpawnDefaultPawnAtTransform];
    LOGF("[VFT] SpawnDefaultPawnAtTransform [0x%X] = %p",
         VFT_IDX_SpawnDefaultPawnAtTransform, (void*)SpawnDefaultPawnAtTransform);

    bool ok = VFTHook(g_gmObj, VFT_IDX_SpawnDefaultPawnFor,
                      (void*)Hook_SpawnDefaultPawnFor,
                      (void**)&o_SpawnDefaultPawnFor);
    LOGF("[VFT] Hook SpawnDefaultPawnFor [0x%X]: %s (orig=%p)",
         VFT_IDX_SpawnDefaultPawnFor, ok ? "OK" : "FAIL",
         (void*)o_SpawnDefaultPawnFor);
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
    LOGF(" hook vft sky spawn");
    LOGF(" base = 0x%lx", g_base);
    LOGF("========================================");

    SFO = (StaticFindObject_t)(g_base + ADDR_StaticFindObject);
    LOGF(" SFO = %p", (void*)SFO);

    DobbyHook((void*)(g_base + ADDR_PlaylistHandler), (void*)Hook_PH, (void**)&o_PH);
    DobbyHook((void*)(g_base + ADDR_StartAircraftPhase), (void*)Hook_SAP, (void**)&o_SAP);
    DobbyHook((void*)(g_base + ADDR_PostLogin), (void*)Hook_PL, (void**)&o_PL);

    for (int k = 0; k < 300; k++) {
        void* gm = GetGameMode();
        if (IVP(gm)) {
            g_gmObj = gm;
            LOGF("[GM] resolved at attempt %d", k);
            ApplyVftHooks();
            return;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
    LOGF("[GM] TIMEOUT waiting for GameMode instance");
}

extern "C" JNIEXPORT jint JNICALL
JNI_OnLoad(JavaVM* vm, void* reserved) {
    InitLogFile();
    LOGF(" hook loaded %s %s", __DATE__, __TIME__);
    std::thread([]{ try { Run(); } catch (...) { LOGF("[ERR] exception in Run"); } }).detach();
    return JNI_VERSION_1_6;
}
