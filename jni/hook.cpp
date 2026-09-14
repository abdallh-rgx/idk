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
#define VFT_IDX_ProcessEvent             0x42

#define OFF_PC_PlayerState               0x278
#define OFF_GM_GameState                 0x2D0
#define OFF_GS_Playlist2298              0x2298
#define OFF_GS_Playlist2290              0x2290
#define OFF_PS_SpectatorByte             0x27A

#define OFF_UFUNCTION_FUNC               0xE0

#define SKY_Z                            500.0f

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

typedef void* (*StaticFindObject_t)(void*, void*, const char16_t*, bool);
static StaticFindObject_t SFO = nullptr;

static bool IVP(void* p) {
    uintptr_t v = (uintptr_t)p;
    return (v >= 0x1000000000ULL && v <= 0x8000000000ULL);
}

static bool VFTHook(void* vft, size_t vft_index, void* hook_fn, void** orig_out) {
    if (!vft || !hook_fn) return false;
    void** slot = nullptr;
    if (!RP((char*)vft + vft_index * sizeof(void*), (void**)&slot)) return false;
    void* current = nullptr;
    if (!RP(slot, &current)) return false;
    if (orig_out) *orig_out = current;

    const size_t page_size = static_cast<size_t>(sysconf(_SC_PAGESIZE));
    uintptr_t addr = reinterpret_cast<uintptr_t>(slot);
    uintptr_t page_start = addr & ~(page_size - 1);
    size_t span = page_size;
    if ((addr + sizeof(void*)) > (page_start + page_size)) span += page_size;

    if (mprotect(reinterpret_cast<void*>(page_start), span, PROT_READ | PROT_WRITE | PROT_EXEC) != 0) return false;
    *slot = hook_fn;
    mprotect(reinterpret_cast<void*>(page_start), span, PROT_READ | PROT_EXEC);
    __builtin___clear_cache(reinterpret_cast<char*>(page_start), reinterpret_cast<char*>(page_start + span));
    return true;
}

struct FTransform { float qx,qy,qz,qw; float tx,ty,tz,_p1; float sx,sy,sz,_p2; };

static void MakeSkyXform(FTransform* t) {
    memset(t, 0, sizeof(*t));
    t->qw = 1.0f;
    t->tz = SKY_Z;
    t->sx = t->sy = t->sz = 1.0f;
}

typedef void* (*SpawnDefaultPawnAtTransform_t)(void*, void*, void*);
static SpawnDefaultPawnAtTransform_t SpawnDefaultPawnAtTransform = nullptr;

typedef void (*ProcessEvent_t)(void*, void*, void*);
static ProcessEvent_t ProcessEvent = nullptr;

typedef void (*UFunctionHook_t)(void*, void*, void*);
static UFunctionHook_t o_SpawnDefaultPawnForFunc = nullptr;

static void* g_cached_gm = nullptr;
static void* g_cached_pc = nullptr;
static void* g_spawn_func = nullptr;

// ============================================================
// UFunction hook signature:
//   void execXXX(UObject* Context, FFrame& Stack, void* Result)
// ============================================================
static void Hook_SpawnDefaultPawnForFunc(void* Context, void* Stack, void* Result) {
    LOGF("[SpawnDefaultPawnFor] CALLED! ctx=%p stack=%p result=%p", Context, Stack, Result);

    if (!IVP(g_cached_pc) || !SpawnDefaultPawnAtTransform) {
        LOGF("[SpawnDefaultPawnFor] no cached pc, fallback to original");
        if (o_SpawnDefaultPawnForFunc) o_SpawnDefaultPawnForFunc(Context, Stack, Result);
        return;
    }

    FTransform sky;
    MakeSkyXform(&sky);
    void* pawn = SpawnDefaultPawnAtTransform(Context, g_cached_pc, &sky);
    LOGF("[SpawnDefaultPawnFor] custom spawn pawn=%p", pawn);

    if (IVP(pawn)) {
        if (Result) *(void**)Result = pawn;
        return;
    }

    LOGF("[SpawnDefaultPawnFor] custom failed, call original");
    if (o_SpawnDefaultPawnForFunc) o_SpawnDefaultPawnForFunc(Context, Stack, Result);
}

static void DumpUFunction(void* fn) {
    LOGF("========== UFunction Dump %p ==========", fn);
    for (size_t off = 0x80; off < 0x140; off += 8) {
        void* v = nullptr;
        if (!RP((char*)fn + off, &v)) continue;
        if (!IVP(v)) {
            LOGF("  +0x%zX = %p", off, v);
            continue;
        }
        uintptr_t rva = (uintptr_t)v - g_base;
        if (rva < 0x20000000ULL) {
            LOGF("  +0x%zX = %p   (RVA=0x%lX)  <-- candidate Func",
                 off, v, (unsigned long)rva);
        } else {
            LOGF("  +0x%zX = %p", off, v);
        }
    }
}

static void* FindSpawnDefaultPawnForFunc() {
    if (!SFO) return nullptr;

    const char16_t* paths[] = {
        u"/Script/FortniteGame.FortGameMode.SpawnDefaultPawnFor",
        u"/Script/FortniteGame.FortGameModeBase.SpawnDefaultPawnFor",
        u"/Script/Engine.GameModeBase.SpawnDefaultPawnFor",
        u"/Game/Athena/FortGameMode.FortGameMode_C.SpawnDefaultPawnFor",
    };

    for (auto p : paths) {
        void* fn = SFO(nullptr, nullptr, p, false);
        LOGF("[Find] '%ls' -> %p", p, fn);
        if (IVP(fn)) return fn;
    }
    return nullptr;
}

static bool HookUFunction(void* ufunc, void* hook, void** orig_out) {
    if (!IVP(ufunc) || !hook) return false;

    void* current = nullptr;
    if (!RP((char*)ufunc + OFF_UFUNCTION_FUNC, &current)) {
        LOGF("[UFunctionHook] cannot read Func at +0x%X", OFF_UFUNCTION_FUNC);
        return false;
    }

    uintptr_t rva = (uintptr_t)current - g_base;
    LOGF("[UFunctionHook] Func @+0x%X = %p (RVA=0x%lX)",
         OFF_UFUNCTION_FUNC, current, (unsigned long)rva);

    if (orig_out) *orig_out = current;

    if (!WP((char*)ufunc + OFF_UFUNCTION_FUNC, hook)) {
        LOGF("[UFunctionHook] write failed");
        return false;
    }
    LOGF("[UFunctionHook] replaced Func with %p", hook);
    return true;
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

static bool g_vftHooked = false;

static void ApplyVftHooks() {
    void** vft = (void**)(g_base + STATIC_VFT_RVA);

    if (!IVP(vft)) {
        LOGF("[VFT] Static VFT pointer invalid: %p", vft);
        return;
    }
    LOGF("[VFT] Static VFT = %p (RVA=0x%X)", vft, STATIC_VFT_RVA);

    void* at = nullptr;
    if (RP((char*)vft + VFT_IDX_SpawnDefaultPawnAtTransform * sizeof(void*), &at) && IVP(at)) {
        SpawnDefaultPawnAtTransform = (SpawnDefaultPawnAtTransform_t)at;
        LOGF("[VFT] SpawnDefaultPawnAtTransform = %p (RVA=0x%lX)",
             at, (unsigned long)((uintptr_t)at - g_base));
    } else {
        LOGF("[VFT] SpawnDefaultPawnAtTransform read FAILED");
    }

    void* pe = nullptr;
    if (RP((char*)vft + VFT_IDX_ProcessEvent * sizeof(void*), &pe) && IVP(pe)) {
        ProcessEvent = (ProcessEvent_t)pe;
        LOGF("[VFT] ProcessEvent = %p (RVA=0x%lX)",
             pe, (unsigned long)((uintptr_t)pe - g_base));
    } else {
        LOGF("[VFT] ProcessEvent read FAILED");
    }
}

static void CallSpawnDefaultPawnForViaProcessEvent(void* gm, void* pc) {
    if (!IVP(gm) || !IVP(pc)) return;
    if (!IVP(g_spawn_func) || !ProcessEvent) {
        LOGF("[Call] cannot call, g_spawn_func=%p ProcessEvent=%p",
             g_spawn_func, (void*)ProcessEvent);
        return;
    }

    uint8_t params[0x40];
    memset(params, 0, sizeof(params));

    *(void**)(params + 0x00) = pc;
    *(void**)(params + 0x08) = nullptr;

    LOGF("[Call] ProcessEvent(gm=%p, func=%p, params=%p)", gm, g_spawn_func, params);
    ProcessEvent(gm, g_spawn_func, params);

    void* result = *(void**)(params + 0x10);
    LOGF("[Call] result (params+0x10) = %p", result);

    if (!IVP(result)) {
        result = *(void**)(params + 0x00);
        LOGF("[Call] try params+0x00 = %p", result);
    }
}

typedef void (*PL_t)(void*, void*);
static PL_t o_PL = nullptr;
static uint64_t g_calls = 0;

static void Hook_PL(void* gm, void* pc) {
    g_calls++;
    LOGF("========== [PostLogin #%llu] GM=%p PC=%p ==========",
         (unsigned long long)g_calls, gm, pc);

    g_cached_gm = gm;
    g_cached_pc = pc;

    if (!g_vftHooked) {
        ApplyVftHooks();
        g_vftHooked = true;
    }

    InjectPlaylist(gm);
    ClearSpectator(pc);

    if (o_PL) o_PL(gm, pc);

    ClearSpectator(pc);

    if (IVP(gm) && IVP(pc)) {
        CallSpawnDefaultPawnForViaProcessEvent(gm, pc);
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
    LOGF(" hook v12 - UFunction SpawnDefaultPawnFor");
    LOGF(" base = 0x%lx", g_base);
    LOGF("========================================");

    SFO = (StaticFindObject_t)(g_base + ADDR_StaticFindObject);
    LOGF(" SFO = %p (RVA=0x%X)", (void*)SFO, ADDR_StaticFindObject);

    g_spawn_func = FindSpawnDefaultPawnForFunc();
    LOGF(" SpawnDefaultPawnFor UFunction = %p", g_spawn_func);

    if (IVP(g_spawn_func)) {
        DumpUFunction(g_spawn_func);
        HookUFunction(g_spawn_func, (void*)Hook_SpawnDefaultPawnForFunc,
                      (void**)&o_SpawnDefaultPawnForFunc);
    }

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
