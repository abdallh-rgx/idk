// ============================================================================
// EraCH3 — Fortnite 21.30 CL-21088273 Android  (FIXED & COMPLETED)
// ============================================================================
// Build ID of target libUnreal.so:
//   5b371871c152651ff2b038c01c9504c1a82de1f8
//
// This is the corrected version of hook.cpp. All engine-class offsets and
// function signatures below were verified against the 21.30 MobileDumper-7
// SDK (GObjects dump + ClassesInfo/StructsInfo/FunctionsInfo/EnumsInfo).
//
// FIXES/PORTS (see CHANGES.md for the full list):
//  [F01] FQuickBarSlot.bEnabled lives at +0x10 bit0 (was writing +0x18 =
//        UsedByItemGuid, corrupting the GUID and never enabling the slot).
//  [F02] Reflection paths "FortQuickBar.Slots"/"FortQuickBarSlot.Items" do
//        not exist — the reflected struct names are "QuickBar"/"QuickBarSlot".
//  [F03] "Engine.Actor.SetMirrored" does not exist — use
//        BuildingSMActor.SetMirrored (bIsMirrored).
//  [F04] FortPlayerController.ServerAttemptAircraftJump does not exist in
//        21.30 — it lives on UFortControllerComponent_Aircraft.
//  [F05] JumpFromHudStart is dispatched on UAthenaHUDContext, not on the
//        controller — the old PE filter never matched (touch jump dead).
//  [F06] AFortPickup has no ItemDefinition/Count members in 21.30 — the
//        entry lives in PrimaryPickupItemEntry (FFortItemEntry).
//  [F07] FCreateBuildingActorData: BuildingClass ptr at 0x40 (was 0x48),
//        bMirrored at 0x38, param size 0x50.
//  [F08] ServerEditBuildingActor params: BuildingActorToEdit(ptr),
//        NewBuildingClass(ptr), RotationIterations(uint8), bMirrored(bool)
//        — old struct read bMirrored from the wrong offset.
//  [F09] GetAnimationHardReference in 21.30 takes BodyType/Gender/
//        PawnContext (old "Type/Animation" params are gone) — emotes fixed.
//  [F10] Edit tool: EditActor + OnRep_EditActor live on FortWeap_EditingTool,
//        not FortWeapon.
//  [F11] ServerOnExitVehicle ExitForceBehavior is a uint8 enum (old code
//        bound int32 → bind failed).
//  [F12] Removed dead OnRep calls (FortQuickBars OnRep_Primary/Secondary,
//        PC.OnRep_QuickBar, PlayerState.OnRep_CharacterParts,
//        Pawn.OnRep_CustomizationLoadout) — none exist in 21.30.
//  [F13] Removed K2_GiveAbility/GiveAbility grant paths — no such UFUNCTIONs
//        in 21.30; abilities flow through EquipFortAbilitySet only.
//  [F14] GC / SavePackage guards in WorldTick (offsets existed, never used).
//  [F15] IsStandalone gating before any session work (was never called).
//  [F16] Call: Blueprint event override resolution + receiver validation
//        + named/positional param binding (ported from the 10.40 Era core).
//  [F17] Inventory: native NewGuid + reconciliation verification + duplicate
//        protection (ported from Era's give()).
//  [F18] Pickups: stacking, max-stack transfer, weapon slot selection
//        (ported from Era's local_pickup).
//  [F19] Two-bar quickbar layout: weapons on Primary(0), build pieces on
//        Secondary(1), binding through EquipItemToQuickBarSlot with
//        GetItemInQuickbarSlot read-back verification.
//  [F20] Local loot rolling for containers with rollback (replaces the
//        dead BP-only path; exported-table style from the Era core).
//  [F21] Building through the local preview (LastBuildPreviewGridSnapLoc/
//        Rot + CurrentBuildableClass) with dedup + touch-build polling.
//  [F22] Locker cosmetics via MCP (CosmeticLoadoutPC + GetLoadoutForPlayer)
//        with hero specializations + backpack fallback.
//  [F23] SetMaxHealth/SetHealth(100) after possession; bBuildFree for free
//        building; bReadyToStartMatch direct bit fallback (0xA28 bit5).
//  [F24] Lobby return interception (ServerReturnToMainMenu → open Frontend
//        after tick) + fly/walk/ghost/god/teleport + era.* console commands.
//  [F25] Prologue verification for every hooked RVA before DobbyHook.
// ============================================================================
#include <jni.h>
#include <pthread.h>
#include <dlfcn.h>
#include <link.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/mman.h>
#include <cstring>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <ctime>
#include <cstdarg>
#include <cstddef>
#include <atomic>
#include <string>
#include <vector>
#include <array>
#include <map>
#include <set>
#include <algorithm>
#include <android/log.h>

static FILE* g_logFile = nullptr;
static void InitLog() {
    const char* p[] = {
        "/storage/emulated/0/Android/data/com.epicgames.fortnite/files/era_log.txt",
        "/sdcard/Android/data/com.epicgames.fortnite/files/era_log.txt",
        "/data/local/tmp/era_log.txt",
        "/sdcard/era_log.txt",
        nullptr};
    for (int i = 0; p[i]; ++i) { g_logFile = fopen(p[i], "w"); if (g_logFile) break; }
}
static void LogImpl(const char* fmt, ...) {
    char line[1024]; va_list ap; va_start(ap, fmt);
    vsnprintf(line, sizeof(line), fmt, ap); va_end(ap);
    __android_log_write(ANDROID_LOG_INFO, "EraCH3", line);
    if (g_logFile) { fprintf(g_logFile, "%s\n", line); fflush(g_logFile); }
}
#define LOG(...) LogImpl(__VA_ARGS__)

using int8=int8_t; using int16=int16_t; using int32=int32_t; using int64=int64_t;
using uint8=uint8_t; using uint16=uint16_t; using uint32=uint32_t; using uint64=uint64_t;
using TCHAR=char16_t;

// ============================================================================
// PROFILE — All offsets for Fortnite 21.30 (libUnreal.so CL-21088273).
// Code RVAs byte-verified against the shipped library; class offsets
// verified against the 21.30 MobileDumper-7 dump.
// ============================================================================
namespace profile {
// Core engine (prologue-verified: valid ARM64 prologues at every RVA)
constexpr uintptr_t ProcessEvent         = 0x07611B5C;
constexpr uintptr_t StaticFindObject     = 0x0762A664;
constexpr uintptr_t StaticLoadObject     = 0x0761C568;
constexpr uintptr_t StaticLoadClass      = 0x0762CFB8;
constexpr uintptr_t NameAppendString     = 0x074937CC;
constexpr uintptr_t Free                 = 0x07355370;
constexpr uintptr_t WorldTick            = 0x0949685C;
constexpr uintptr_t EngineExec           = 0x098BB160;
constexpr uintptr_t IsStandalone         = 0x0945F840;
constexpr uintptr_t GetPlayerController  = 0x093A1D48;
constexpr uintptr_t BeginSpawn           = 0x093A37E4;
constexpr uintptr_t FinishSpawn          = 0x0939F1B8;
constexpr uintptr_t SpawnObject          = 0x093A3388;
constexpr uintptr_t GenericArrayAdd      = 0x0944A668;
constexpr uintptr_t GenericArrayRemove   = 0x0944B2DC;
constexpr uintptr_t IsGarbageCollecting  = 0x0756E138;
constexpr uintptr_t IsSavingPackage      = 0x0760A59C;
constexpr uintptr_t GObjects             = 0x0DCD6E08;
constexpr uintptr_t Realloc              = 0x0735E520;
constexpr uintptr_t CopyScriptStruct     = 0x07535848;   // UScriptStruct::CopyScriptStruct
constexpr uintptr_t GetInterfaceAddress  = 0x0761AA88;

// Object layout (UE5.0, Dumper-7 preset — matches MobileDumper-7)
constexpr unsigned ObjectClass  = 0x10;
constexpr unsigned ObjectName   = 0x18;
constexpr unsigned ObjectOuter  = 0x20;
constexpr unsigned FieldNext    = 0x28;
constexpr unsigned StructSuper  = 0x40;
constexpr unsigned StructChildren   = 0x48;
constexpr unsigned StructChildProps = 0x50;
constexpr unsigned FunctionParamSize = 0xB6;
constexpr unsigned PropFlags       = 0x38;
constexpr unsigned PropOffset      = 0x44;
constexpr unsigned PropElementSize = 0x34;
constexpr unsigned BoolSize        = 0x80;
constexpr unsigned BoolByteOffset  = 0x81;
constexpr unsigned BoolFieldMask   = 0x82;
constexpr unsigned FFieldName = 0x28;
constexpr unsigned FFieldNext = 0x20;

// [F28-F33] Streaming / vehicles / wildlife / minimap (SDK 21.30 verified)
constexpr unsigned World_StreamingLevels        = 0x0088;   // UWorld.StreamingLevels TArray<ULevelStreaming*>
constexpr unsigned VehicleSpawner_CachedItemDef = 0x02B8;   // AFortAthenaVehicleSpawner.CachedFortVehicleItemDef (696)
constexpr unsigned VehicleItemDef_ActorClass    = 0x0A38;   // UFortVehicleItemDefinition.VehicleActorClass (2616)

// CPF flags
constexpr uint64_t CPF_Parm       = 0x00000080ULL;
constexpr uint64_t CPF_OutParm    = 0x00000100ULL;
constexpr uint64_t CPF_ConstParm  = 0x00000200ULL;
constexpr uint64_t CPF_ReturnParm = 0x00000400ULL;

// FFortItemEntry (21.30 struct layout — verified)
constexpr unsigned ItemEntry_Count          = 0x0C;
constexpr unsigned ItemEntry_ItemDefinition = 0x18;
constexpr unsigned ItemEntry_Durability     = 0x24;
constexpr unsigned ItemEntry_LoadedAmmo     = 0x2C;
constexpr unsigned ItemEntry_PhantomAmmo    = 0x30;
constexpr unsigned ItemEntry_ItemGuid       = 0x68;
constexpr unsigned ItemEntry_TrackerGuid    = 0x78;
constexpr unsigned ItemEntry_StateValues    = 0xD0;

// Fortnite classes (verified)
constexpr unsigned FortPlayerState_HeroType            = 0x0430;
constexpr unsigned FortPlayerState_CharacterData       = 0x0580;
constexpr unsigned FortPlayerState_AbilitySystemComp   = 0x09F0;
constexpr unsigned FortPlayerController_MyPlayerInfo   = 0x2088;
constexpr unsigned FortPlayerController_ClientQuickBars= 0x1A60;
constexpr unsigned FortPlayerController_QuickBarReady  = 0x1A68;   // byte after ClientQuickBars, checked by native GetItemInQuickbarSlot gate
constexpr unsigned FortPlayerController_ReadyByte      = 0x0A28;   // bReadyToStartMatch bitfield byte
constexpr unsigned FortPlayerController_ReadyBit       = 5;
constexpr unsigned RegisteredPlayerInfo_HeroInstance   = 0x2D0;    // TempAthenaMenuHeroInstance
constexpr unsigned FortHero_CharacterParts             = 0x238;
constexpr unsigned FortHero_Specializations            = 0x218;
constexpr unsigned FortPlayerPawn_bIsSkydiving         = 0x1A1D;
constexpr unsigned FortPlayerPawn_bIsParachuteOpen     = 0x1A1E;
constexpr unsigned FortPlayerPawn_CosmeticLoadout      = 0x2A08;
constexpr unsigned FortPawn_CurrentWeapon              = 0x07F0;
constexpr unsigned FortPawn_AbilitySystemComponent     = 0x1008;
constexpr unsigned BuildingWall_bDoorOpen_Byte         = 0x0FDE;   // plain bool byte (0/1)
constexpr unsigned PlayerPawn_Athena_Speed_Sprint      = 0x53EC;

// ⭐ Movement offsets (from SDK)
constexpr unsigned FortPawn_bWantsToSlide            = 0x1030;
constexpr unsigned FortPlayerPawn_bIsSliding_Byte    = 0x06DB;
constexpr unsigned FortPlayerPawn_bIsSliding_Bit     = 3;
constexpr unsigned FortPlayerPawn_bIsSurfaceSwim_Byte= 0x0770;
constexpr unsigned FortPlayerPawn_bIsSurfaceSwim_Bit = 4;
constexpr unsigned CharacterMovement_MovementMode    = 0x019C;
constexpr unsigned CharacterMovement_CustomMode      = 0x019D;

// TacticalSprint (UFortPlayerControllerComponent_TacticalSprint, verified)
constexpr unsigned TacticalSprint_Flags              = 0xC0;
constexpr uint8_t TacticalSprint_Bit_Enabled   = 0x01;
constexpr uint8_t TacticalSprint_Bit_WantsSprint = 0x02;
constexpr uint8_t TacticalSprint_Bit_IsSprinting = 0x08;

// ⭐ QuickBars (21.30 — libUnreal.so)
constexpr uintptr_t InitializeAthenaQuickBars    = 0x056607ec;
constexpr unsigned  QuickBars_VtableSlot         = 0x738;
constexpr unsigned  QuickBars_PrimaryOffset      = 0x278;   // FQuickBar.PrimaryQuickBar
constexpr unsigned  QuickBars_SecondaryOffset    = 0x308;   // FQuickBar.SecondaryQuickBar
constexpr unsigned  QuickBars_CreativeOffset     = 0x398;   // FQuickBar.CreativeQuickBar
constexpr unsigned  QuickBar_SlotsOffset         = 0x10;    // FQuickBar.Slots
constexpr unsigned  QuickBarSlot_ItemsOffset     = 0x00;    // FQuickBarSlot.Items
constexpr unsigned  QuickBarSlot_EnabledOffset   = 0x10;    // FQuickBarSlot.bEnabled (bit0)
constexpr unsigned  QuickBarSlot_EnabledBit      = 0x01;
constexpr unsigned  QuickBarSlot_OccupiedBit     = 0x08;    // bIsOccupied (bit3, same byte)
constexpr unsigned  QuickBarSlot_SlotSize        = 0x28;
constexpr unsigned  Controller_ClientQuickBars   = 0x1a60;
constexpr unsigned  Controller_QuickBarReadyByte = 0x1a68;
constexpr uintptr_t GetItemInQuickbarSlot_Gate   = 0x06673524;
constexpr unsigned  QuickBars_VtableSlot_GetItemInSlot = 0x6f0;

// ⭐ Local building / reticle / touch (AFortPlayerController, verified)
constexpr unsigned Controller_CurrentBuildableClass = 0x1898;
constexpr unsigned Controller_BuildPreviewRotIter   = 0x1858;
constexpr unsigned Controller_BuildPreviewMirrored  = 0x1859;
constexpr unsigned Controller_bBuildFree            = 0x185B;
constexpr unsigned Controller_TouchPrimaryFire      = 0x0BB8;
constexpr unsigned Controller_PreviewSnapLoc        = 0x19B0;   // FVector (LWC)
constexpr unsigned Controller_PreviewSnapRot        = 0x19C8;   // FRotator (LWC)
constexpr unsigned Controller_LocationUnderReticle  = 0x24C0;
constexpr unsigned Controller_InteractionComponent  = 0x28D8;
constexpr unsigned Controller_CosmeticLoadoutPC     = 0x1E70;   // FFortAthenaLoadout
constexpr unsigned AFortPickup_PrimaryEntry         = 0x02F8;
constexpr unsigned AFortPickup_SourceFlags          = 0x0540;
constexpr unsigned AFortPickup_SpawnSource          = 0x0541;
constexpr unsigned AFortGameState_GamePhase         = 0x2150;
constexpr unsigned AFortGameState_SafeZoneIndicator = 0x19A8;
constexpr unsigned AFortGameState_PlaylistInfo      = 0x2170;   // FPlaylistPropertyArray
constexpr unsigned EditingTool_EditActor            = 0x1020;
constexpr unsigned EditingTool_bEditConfirmed       = 0x1028;   // [F49] AFortWeap_EditingTool.bEditConfirmed
constexpr unsigned Pickaxe_WeaponDefinition         = 0x0790;
}

extern "C" int DobbyHook(void* target, void* replace, void** origin);

// ============================================================================
// MEMORY HELPERS
// ============================================================================
static inline bool IsBadPtr(const void* p) {
    if (!p) return true;
    uintptr_t v = (uintptr_t)p;
    if (v < 0x10000) return true;
    if (v > 0x0000FFFFFFFFFFFFULL) return true;
    return false;
}
template<class T> static T Read(const void* p, size_t off = 0) {
    T r{}; if (p) memcpy(&r, (const char*)p + off, sizeof(T)); return r;
}
template<class T> static void Write(void* p, size_t off, const T& v) {
    if (p) memcpy((char*)p + off, &v, sizeof(T));
}

static uintptr_t g_base = 0;
static std::atomic<int> g_status{0};
static std::atomic<int> g_gameTid{0};
static thread_local bool g_internal = false;

// [F34] Crash logger — SIGSEGV/SIGBUS/SIGILL/SIGABRT mini-backtrace with
// libUnreal.so module offsets, written async-signal-safe to
// /data/local/tmp/era_crash.txt. Converts an opaque "game crashed" into a
// fixable function offset on the next run.
static void EraCrashHandler(int sig, siginfo_t* info, void* ctx) {
    char buf[2048]; int n = 0;
    n += snprintf(buf + n, sizeof(buf) - (size_t)n,
                  "CRASH sig=%d addr=%p base=%p\n", sig,
                  info ? info->si_addr : nullptr, (void*)g_base);
    ucontext_t* uc = (ucontext_t*)ctx;
    if (uc) {
#if defined(__aarch64__) && defined(__BIONIC__)
        uintptr_t pc = (uintptr_t)uc->uc_mcontext.pc;
        uintptr_t sp = (uintptr_t)uc->uc_mcontext.sp;
        n += snprintf(buf + n, sizeof(buf) - (size_t)n, "PC=0x%lx (lib+0x%lx)\n",
                      (unsigned long)pc, g_base ? (unsigned long)(pc - g_base) : 0ul);
        if (sp) {
            n += snprintf(buf + n, sizeof(buf) - (size_t)n, "ret scan (libUnreal):\n");
            for (uintptr_t p = sp & ~(uintptr_t)0xF; p < sp + 0x600 && n < (int)sizeof(buf) - 64; p += 8) {
                uintptr_t v = *(uintptr_t*)p;
                if (g_base && v > g_base && v < g_base + 0xE000000)
                    n += snprintf(buf + n, sizeof(buf) - (size_t)n, "  lib+0x%lx\n",
                                  (unsigned long)(v - g_base));
            }
        }
#else
        (void)uc;
#endif
    }
    int fd = open("/data/local/tmp/era_crash.txt", O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (fd >= 0) { ssize_t w = write(fd, buf, (size_t)n); (void)w; close(fd); }
    signal(sig, SIG_DFL);
    raise(sig);
}
static void InstallCrashLogger() {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_sigaction = EraCrashHandler;
    sa.sa_flags = SA_SIGINFO;
    sigaction(SIGSEGV, &sa, nullptr);
    sigaction(SIGBUS,  &sa, nullptr);
    sigaction(SIGILL,  &sa, nullptr);
    sigaction(SIGABRT, &sa, nullptr);
}

struct Guard { bool s = g_internal; Guard() { g_internal = true; } ~Guard() { g_internal = s; } };
template<class F> static F Fn(uintptr_t rva) { return reinterpret_cast<F>(g_base + rva); }

// ============================================================================
// UE5 STRUCTS (LWC — 21.30 uses double-precision FVector/FRotator)
// ============================================================================
struct FStringNative { TCHAR* Data; int32 Count; int32 Max; };
struct FName         { uint32 CompIdx; uint32 DispIdx; };
struct Array         { void* Data; int32 Count; int32 Max; };
struct Guid          { uint32 A, B, C, D; };
struct Vector        { double X, Y, Z; };
struct Rotator       { double Pitch, Yaw, Roll; };

struct alignas(16) Transform {
    double Rotation[4]{0, 0, 0, 1};
    double Location[3]{0, 0, 0};
    uint8_t _padL[8]{};
    double Scale[3]{1, 1, 1};
    uint8_t _padS[8]{};
};
static_assert(sizeof(Vector) == 24, "Vector LWC");
static_assert(sizeof(Transform) == 96, "Transform LWC");

struct FUObjectItem { void* Object; int32 Flags; int32 Cluster; int32 Serial; };
struct GObjects_t {
    uint8 _pad0[0x10];
    void** Objects;
    uint8 _pad1[0x8];
    int32 MaxElements;
    int32 NumElements;
    int32 MaxChunks;
    int32 NumChunks;
    void* GetById(uint32 id) {
        if (!Objects) return nullptr;
        uint32 ci = id / 0x10000, wi = id % 0x10000;
        if ((int32)ci >= NumChunks) return nullptr;
        auto* chunk = (FUObjectItem*)Objects[ci];
        if (!chunk) return nullptr;
        return chunk[wi].Object;
    }
};
struct TScriptInterface { void* Object; void* Interface; };

// ============================================================================
// FORWARD DECLS
// ============================================================================
static void* FindObj(const std::string& path);
static std::string ObjName(void* obj);
static bool IsA(void* obj, void* type);
static bool SubclassOfFast(void* klass, void* targetClass);
static void* GetProp(const char* path);
static void* GetField(void* obj, void* prop);
static void SetBool(void* obj, const char* path, bool enabled);
static std::u16string ToWide(const std::string& s);
static std::string Narrow(const char16_t* s, int max);
static void* GetObjField(void* obj, const char* path);
static void* LoadAsset(const std::string& path, const char* type_path);
static void* LoadClass(const char* path, const char* base_path);
static void* GetPC(void* world, int index);
static void* incoming(void* function, void* data, const char* key, int expected_size = 0);
static GObjects_t* GObjectArray();
static void RotatorToQuat(const Rotator& r, double out[4]);   // [F28-F33] forward decl
static bool IsCDO(void* obj);                                 // [F39] forward decl
static uint32_t BRRand();                                     // [F75-c] forward decl (wildlife)
static float BRRand01();                                      // [F75-c] forward decl (wildlife)
// static void ZoneMonitor(); // [F35] removed — see [F73] note near BRTick
static double Monotonic();                                    // [F54] forward decl (GObjectArray probe)

// ============================================================================
// TYPE CHECKS
// ============================================================================
static bool SubclassOfFast(void* klass, void* targetClass) {
    if (!klass || !targetClass || IsBadPtr(klass) || IsBadPtr(targetClass)) return false;
    for (int d = 0; d < 64; ++d) {
        if (klass == targetClass) return true;
        void* sup = Read<void*>(klass, profile::StructSuper);
        if (!sup || sup == klass || IsBadPtr(sup)) break;
        klass = sup;
    }
    return false;
}
static bool IsA(void* obj, void* type) {
    if (!obj || !type || IsBadPtr(obj) || IsBadPtr(type)) return false;
    void* klass = Read<void*>(obj, profile::ObjectClass);
    if (!klass || IsBadPtr(klass)) return false;
    return SubclassOfFast(klass, type);
}
static GObjects_t* GObjectArray() {
    // [F54] ROOT-CAUSE FIX — every GObjects scanner (containers, spawners,
    // wildlife, ability classes, quickbar widgets, door/harvest caches) was
    // silently dead on device: "[Loot] Total 0 containers", "[Veh] 0 spawner
    // actors", "[Wild] 0 animal classes", ability classes "not resident yet"
    // forever, build-over-map-buildings, no harvest resources. The old probe
    // only accepted "the address holds a POINTER to FUObjectArray" — when the
    // build stores the array directly there, the probe returned null and every
    // scan collapsed to zero results with no log line. Probe both layouts and
    // validate the chunked-array structure before trusting either.
    static GObjects_t* cached = nullptr;
    static bool attempted = false;
    static double lastAttempt = 0;
    if (attempted && cached) return cached;
    double now = Monotonic();
    if (attempted && now - lastAttempt < 10.0) return cached;   // allow re-probe after failure
    attempted = true; lastAttempt = now;
    if (!g_base) return nullptr;
    auto valid = [](GObjects_t* go) -> bool {
        if (!go || IsBadPtr(go)) return false;
        int32 num = go->NumElements, chunks = go->NumChunks;
        if (num < 1000 || num > 4000000) return false;      // 21.30 dump: 306793 in lobby
        if (chunks < 1 || chunks > 512) return false;
        if (IsBadPtr(go->Objects)) return false;
        void* chunk0 = Read<void*>(go->Objects, 0);
        if (IsBadPtr(chunk0)) return false;
        void* first = Read<void*>(chunk0, 0);               // FUObjectItem.Object[0]
        if (IsBadPtr(first)) return false;
        if (IsBadPtr(Read<void*>(first, profile::ObjectClass))) return false;
        return true;
    };
    GObjects_t* indirect = Read<GObjects_t*>((void*)(g_base + profile::GObjects));
    if (valid(indirect)) {
        cached = indirect;
        LOG("[GObj] resolved (pointer) elements=%d chunks=%d", cached->NumElements, cached->NumChunks);
        return cached;
    }
    GObjects_t* direct = reinterpret_cast<GObjects_t*>(g_base + profile::GObjects);
    if (valid(direct)) {
        cached = direct;
        LOG("[GObj] resolved (direct) elements=%d chunks=%d", cached->NumElements, cached->NumChunks);
        return cached;
    }
    LOG("[GObj] UNRESOLVED at RVA 0x%llX — all GObjects scans stay disabled",
        (unsigned long long)profile::GObjects);
    return nullptr;
}

// ============================================================================
// STRING HELPERS
// ============================================================================
static std::u16string ToWide(const std::string& s) { return std::u16string(s.begin(), s.end()); }
static std::string Narrow(const char16_t* s, int max) {
    std::string out;
    if (s) for (int i = 0; i < max && s[i]; ++i) out.push_back(s[i] < 128 ? (char)s[i] : '?');
    return out;
}
static void* FindObj(const std::string& path) {
    auto w = ToWide(path);
    return Fn<void*(*)(void*, void*, const char16_t*, bool)>(profile::StaticFindObject)(nullptr, nullptr, w.c_str(), false);
}
static bool SafeFieldName(void* field, uint32 nameOff, std::string& out) {
    if (!field || IsBadPtr(field)) return false;
    uint32 compIdx = Read<uint32>(field, nameOff + 0);
    uint32 dispIdx = Read<uint32>(field, nameOff + 4);
    if (compIdx == 0xFFFFFFFF || compIdx > 0x8000000) return false;
    if (dispIdx == 0xFFFFFFFF || dispIdx > 0x8000000) return false;
    FStringNative ns{};
    Fn<void(*)(const void*, FStringNative*)>(profile::NameAppendString)((char*)field + nameOff, &ns);
    out = Narrow(ns.Data, 128);
    if (ns.Data) Fn<void(*)(void*)>(profile::Free)(ns.Data);
    return true;
}
static std::string ObjName(void* obj) {
    if (!obj || IsBadPtr(obj)) return {};
    uint32 compIdx = Read<uint32>(obj, profile::ObjectName + 0);
    uint32 dispIdx = Read<uint32>(obj, profile::ObjectName + 4);
    if (compIdx == 0xFFFFFFFF || compIdx > 0x8000000) return {};
    if (dispIdx == 0xFFFFFFFF || dispIdx > 0x8000000) return {};
    static pthread_mutex_t m = PTHREAD_MUTEX_INITIALIZER;
    static std::map<void*, std::string> cache;
    pthread_mutex_lock(&m);
    auto it = cache.find(obj);
    if (it != cache.end()) { std::string r = it->second; pthread_mutex_unlock(&m); return r; }
    pthread_mutex_unlock(&m);
    FStringNative s{};
    Fn<void(*)(const void*, FStringNative*)>(profile::NameAppendString)((char*)obj + profile::ObjectName, &s);
    std::string out = Narrow(s.Data, std::min(std::max(s.Count, 0), 512));
    if (s.Data) Fn<void(*)(void*)>(profile::Free)(s.Data);
    pthread_mutex_lock(&m);
    if (cache.size() < 200000) cache[obj] = out;
    pthread_mutex_unlock(&m);
    return out;
}
// [F17/F20] Find an item definition by its short name through GObjects
// (ported from the Era core by_short_name()).
// [F54] memoized — auto-looting dozens of containers used to run a full
// GObjects pass per loot row (~0.3 s each), freezing the game thread.
static void* ByShortName(const std::string& wanted) {
    static pthread_mutex_t m = PTHREAD_MUTEX_INITIALIZER;
    static std::map<std::string, void*> memo;
    {
        pthread_mutex_lock(&m);
        auto it = memo.find(wanted);
        void* hit = it != memo.end() ? it->second : nullptr;
        pthread_mutex_unlock(&m);
        if (hit) return hit;
    }
    GObjects_t* go = GObjectArray();
    if (!go) return nullptr;
    void* definition_type = FindObj("/Script/FortniteGame.FortItemDefinition");
    if (!definition_type) return nullptr;
    for (int32 i = 0; i < go->NumElements; i += 1) {
        void* object = go->GetById((uint32)i);
        if (object && !IsBadPtr(object) && IsA(object, definition_type) && ObjName(object) == wanted) {
            pthread_mutex_lock(&m);
            if (memo.size() < 4096) memo[wanted] = object;
            pthread_mutex_unlock(&m);
            return object;
        }
    }
    return nullptr;
}

// [F55] UE-allocator allocation — the manual QuickBar slot arrays and GUID
// buffers were malloc'ed while the engine reallocs/frees them through
// FMemory on slot updates (mismatched allocator = the classic source of the
// random build-mode/inventory crashes).
static void* FMalloc(size_t size) {
    using Realloc_t = void* (*)(void*, size_t, uint32_t);
    return Fn<Realloc_t>(profile::Realloc)(nullptr, size, 0);
}

// [F54] Generic class-by-name scan (classes need IsA(Class) + SubclassOfFast,
// the IsA-on-an-object form never reaches the class hierarchy — same rule as
// FindAbilityClassNamed, generalized for markers/saplings/anything).
static void* FindClassNamedLoose(void* baseClass, const std::string& exactName) {
    GObjects_t* go = GObjectArray();
    void* classClass = FindObj("/Script/CoreUObject.Class");
    if (!go || !baseClass || !classClass) return nullptr;
    for (int32 i = 0; i < go->NumElements; ++i) {
        void* object = go->GetById((uint32)i);
        if (!object || IsBadPtr(object)) continue;
        if (!IsA(object, classClass)) continue;
        if (!SubclassOfFast(object, baseClass)) continue;
        if (ObjName(object) == exactName) return object;
    }
    return nullptr;
}

// [F29/F30] Enumerate live instances of a class through GObjects (Erbium
// Utils::GetAll port). One-shot scans; caller caps the result size.
static std::vector<void*> FindAllInstances(void* baseClass, int limit = 64) {
    std::vector<void*> out;
    GObjects_t* go = GObjectArray();
    if (!go || !baseClass) return out;
    out.reserve(16);
    for (int32 i = 0; i < go->NumElements && (int)out.size() < limit; ++i) {
        void* object = go->GetById((uint32)i);
        if (!object || IsBadPtr(object)) continue;
        if (IsCDO(object)) continue;              // [F39] class defaults are not live actors
        if (IsA(object, baseClass)) out.push_back(object);
    }
    return out;
}

// [F30] Enumerate loaded *classes* deriving from baseClass whose name matches
// one of the lowercase keywords (nullptr-terminated). Skips the base itself.
static std::vector<void*> FindAllClassesNamed(void* baseClass, const char* const* keywords, int limit = 32) {
    std::vector<void*> out;
    GObjects_t* go = GObjectArray();
    void* classClass = FindObj("/Script/CoreUObject.Class");
    if (!go || !baseClass || !classClass) return out;
    for (int32 i = 0; i < go->NumElements && (int)out.size() < limit; ++i) {
        void* object = go->GetById((uint32)i);
        if (!object || IsBadPtr(object) || object == baseClass) continue;
        if (!IsA(object, classClass)) continue;
        if (!SubclassOfFast(object, baseClass)) continue;
        std::string n = ObjName(object);
        if (n.size() < 3) continue;
        for (int k = 0; keywords[k]; ++k)
            if (n.find(keywords[k]) != std::string::npos) { out.push_back(object); break; }
    }
    return out;
}

// [F41] Find a loaded object by its exact UObject name AND type (one-shot
// GObjects scan). StaticFindObject needs a full mount path, but cosmetics/AI
// BPs are already resident after the map loads — a name+type scan finds them
// without knowing the /Game/ prefix.
static void* FindLoadedObjectOfType(const char* type_path, const std::string& wanted) {
    GObjects_t* go = GObjectArray();
    void* type = FindObj(type_path);
    if (!go || !type) return nullptr;
    for (int32 i = 0; i < go->NumElements; ++i) {
        void* object = go->GetById((uint32)i);
        if (!object || IsBadPtr(object)) continue;
        if (!IsA(object, type)) continue;
        if (ObjName(object) == wanted) return object;
    }
    return nullptr;
}

// [F50] Enumerate UFunctions by name across all packages — BP widget events
// (HandleQuickbarContentChanged) live under widget BP classes whose mount
// paths are unknown, so the only way in is a name scan. The receiver check
// inside Call::run() keeps a wrong-package match from executing.
static std::vector<void*> FindAllFunctionsNamed(const char* fname, int limit = 8) {
    std::vector<void*> out;
    GObjects_t* go = GObjectArray();
    void* funcType = FindObj("/Script/CoreUObject.Function");
    if (!go || !funcType) return out;
    for (int32 i = 0; i < go->NumElements && (int)out.size() < limit; ++i) {
        void* object = go->GetById((uint32)i);
        if (!object || IsBadPtr(object)) continue;
        if (!IsA(object, funcType)) continue;
        if (ObjName(object) == fname) out.push_back(object);
    }
    return out;
}

// [F52] Find a GameplayAbility CLASS by exact name (classes must be matched
// with SubclassOfFast — IsA() on a UClass object walks the meta-class chain
// and never reaches the ability hierarchy).
static void* FindAbilityClassNamed(const std::string& wanted) {
    GObjects_t* go = GObjectArray();
    void* gaCls = FindObj("/Script/GameplayAbilities.GameplayAbility");
    void* classClass = FindObj("/Script/CoreUObject.Class");
    if (!go || !gaCls || !classClass) return nullptr;
    for (int32 i = 0; i < go->NumElements; ++i) {
        void* object = go->GetById((uint32)i);
        if (!object || IsBadPtr(object)) continue;
        if (!IsA(object, classClass)) continue;
        if (!SubclassOfFast(object, gaCls)) continue;
        if (ObjName(object) == wanted) return object;
    }
    return nullptr;
}

// [F39] CDO filter — class default objects live in GObjects and would pass
// IsA(instance) checks; they must never be treated as live map actors.
static bool IsCDO(void* obj) {
    if (!obj || IsBadPtr(obj)) return true;
    return (Read<uint32_t>(obj, 0x08) & 0x10u) != 0u;  // RF_ClassDefaultObject
}

// ============================================================================
// PROPERTY ACCESS
// ============================================================================
static void* GetProp(const char* path) {
    if (!path) return nullptr;
    std::string s(path);
    if (void* direct = FindObj(s)) return direct;
    auto pos = s.rfind('.');
    if (pos == std::string::npos) return nullptr;
    std::string classPath = s.substr(0, pos);
    std::string propName  = s.substr(pos + 1);
    if (void* alt = FindObj(classPath + ":" + propName)) return alt;
    void* klass = FindObj(classPath);
    if (!klass || IsBadPtr(klass)) return nullptr;
    for (int depth = 0; depth < 32 && klass && !IsBadPtr(klass); ++depth) {
        void* f = Read<void*>(klass, profile::StructChildProps);
        for (int i = 0; f && !IsBadPtr(f) && i < 1024; ++i) {
            std::string n;
            if (SafeFieldName(f, profile::FFieldName, n) && n == propName) return f;
            f = Read<void*>(f, profile::FFieldNext);
        }
        void* c = Read<void*>(klass, profile::StructChildren);
        for (int i = 0; c && !IsBadPtr(c) && i < 1024; ++i) {
            std::string n;
            if (SafeFieldName(c, profile::ObjectName, n) && n == propName) return c;
            c = Read<void*>(c, profile::FieldNext);
        }
        klass = Read<void*>(klass, profile::StructSuper);
    }
    return nullptr;
}
static void* GetField(void* obj, void* prop) {
    if (!obj || !prop) return nullptr;
    return (char*)obj + Read<int>(prop, profile::PropOffset);
}
static void SetBool(void* obj, const char* path, bool enabled) {
    void* p = GetProp(path);
    if (!p || !obj) return;
    uint8_t size = Read<uint8_t>(p, profile::BoolSize);
    uint8_t byte = Read<uint8_t>(p, profile::BoolByteOffset);
    uint8_t mask = Read<uint8_t>(p, profile::BoolFieldMask);
    if (!size || size > 8 || byte >= size || !mask) return;
    auto dst = (uint8_t*)GetField(obj, p) + byte;
    *dst = enabled ? (*dst | mask) : (*dst & ~mask);
}
static bool GetBool(void* obj, const char* path) {
    void* p = GetProp(path);
    if (!p || !obj) return false;
    uint8_t size = Read<uint8_t>(p, profile::BoolSize);
    uint8_t byte = Read<uint8_t>(p, profile::BoolByteOffset);
    uint8_t mask = Read<uint8_t>(p, profile::BoolFieldMask);
    if (!size || size > 8 || byte >= size || !mask) return false;
    return (Read<uint8_t>(GetField(obj, p), byte) & mask) != 0;
}
static void* GetObjField(void* obj, const char* path) {
    void* p = GetProp(path);
    if (!p || !obj) return nullptr;
    if (Read<int>(p, profile::PropElementSize) != 8) return nullptr;
    return Read<void*>(GetField(obj, p));
}
template<class T> static bool GetFieldValue(void* obj, const char* path, T& out) {
    void* p = GetProp(path);
    if (!p || !obj || Read<int>(p, profile::PropElementSize) != sizeof(T)) return false;
    out = Read<T>(GetField(obj, p));
    return true;
}

// ============================================================================
// HOOK TYPEDEFS
// ============================================================================
using PE_t   = void(*)(void*, void*, void*);
using Tick_t = void(*)(void*, int, float);
using Exec_t = bool(*)(void*, void*, const char16_t*, void*);

static PE_t   orig_PE = nullptr;
static Tick_t orig_Tick = nullptr;
static Exec_t orig_Exec = nullptr;

// ============================================================================
// CALL SYSTEM  — [F16] upgraded:
//   • Blueprint event override resolution (event_receiver) so BP events such
//     as BuildingActor.BlueprintOnInteract run the receiver's override
//     instead of the empty base event (ported from the Era core).
//   • Receiver validation in run(): refuses to execute a function on an
//     object that is not a subclass of the function's outer class.
//   • set_any(): bind by trying several reflection names (dumper renames
//     duplicate params like "Class" → "Class_0").
//   • set_at(): positional binding for nameless native params
//     (e.g. AFortPawn.SetMaxHealth(float)).
// ============================================================================
class Call {
    void* m_func = nullptr;
    struct Param { std::string name; int offset, size; uint64_t flags; };
    std::vector<Param> m_params;
    alignas(16) std::array<uint8_t, 8192> m_data{};
    bool m_valid = false;

    void add_param(void* p, uint64_t flags, int total, bool ffield) {
        if (!(flags & profile::CPF_Parm)) return;
        int off = Read<int>(p, profile::PropOffset);
        int sz  = Read<int>(p, profile::PropElementSize);
        if (off < 0 || sz < 1 || off + sz > total) return;
        std::string pname;
        int nameOff = ffield ? profile::FFieldName : profile::ObjectName;
        if (!SafeFieldName(p, nameOff, pname)) return;
        m_params.push_back({pname, off, sz, flags});
    }
public:
    explicit Call(const char* path, void* event_receiver = nullptr) {
        m_func = FindObj(path);
        if (!m_func) return;
        // [F16] Blueprint override resolution: walk the receiver's super
        // chain from most-derived to base and prefer the matching Function
        // found there over the (possibly empty) base event we looked up.
        if (event_receiver && !IsBadPtr(event_receiver)) {
            std::string ev = ObjName(m_func);
            void* funcType = FindObj("/Script/CoreUObject.Function");
            void* klass = Read<void*>(event_receiver, profile::ObjectClass);
            void* derived = nullptr;
            for (int d = 0; d < 64 && klass && !IsBadPtr(klass) && funcType; ++d) {
                void* f = Read<void*>(klass, profile::StructChildProps);
                for (int i = 0; f && !IsBadPtr(f) && i < 1024; ++i) {
                    std::string n;
                    if (SafeFieldName(f, profile::FFieldName, n) && n == ev && IsA(f, funcType)) { derived = f; break; }
                    f = Read<void*>(f, profile::FFieldNext);
                }
                if (!derived) {
                    void* c = Read<void*>(klass, profile::StructChildren);
                    for (int i = 0; c && !IsBadPtr(c) && i < 1024; ++i) {
                        std::string n;
                        if (SafeFieldName(c, profile::ObjectName, n) && n == ev && IsA(c, funcType)) { derived = c; break; }
                        c = Read<void*>(c, profile::FieldNext);
                    }
                }
                if (derived) break;
                klass = Read<void*>(klass, profile::StructSuper);
            }
            if (derived) m_func = derived;
        }
        init();
    }
    // [F50] Direct UFunction construction — for BP events that cannot be
    // reached by path (widget BPs whose package mount is unknown).
    explicit Call(void* funcPtr) { m_func = funcPtr; init(); }
private:
    void init() {
        if (!m_func || IsBadPtr(m_func)) { m_func = nullptr; return; }
        int total = Read<uint16_t>(m_func, profile::FunctionParamSize);
        if (total < 0 || total > (int)m_data.size()) { m_valid = false; return; }
        void* p = Read<void*>(m_func, profile::StructChildren);
        for (int i = 0; p && !IsBadPtr(p) && i < 128; ++i) {
            add_param(p, Read<uint64_t>(p, profile::PropFlags), total, false);
            p = Read<void*>(p, profile::FieldNext);
        }
        void* fp = Read<void*>(m_func, profile::StructChildProps);
        for (int i = 0; fp && !IsBadPtr(fp) && i < 128; ++i) {
            add_param(fp, Read<uint64_t>(fp, profile::PropFlags), total, true);
            fp = Read<void*>(fp, profile::FFieldNext);
        }
        m_valid = (total == 0) || !m_params.empty();
    }
public:
    bool ok() const { return m_valid; }
    void* func() const { return m_func; }
    void dump(const char* label) const {
        LOG("[Call %s] valid=%d func=%p params=%zu", label, (int)m_valid, m_func, m_params.size());
        for (auto& p : m_params)
            LOG("   param '%s' off=%d size=%d flags=%llx", p.name.c_str(), p.offset, p.size,
                (unsigned long long)p.flags);
    }
    template<class T> bool set(const char* key, const T& value) {
        for (auto& p : m_params)
            if (p.name == key && p.size == (int)sizeof(T)) {
                memcpy(m_data.data() + p.offset, &value, sizeof(T));
                return true;
            }
        return false;
    }
    // [F16] Try several reflection names (GetContext's "Class" is emitted as
    // "Class_0" by the dumper, and other renames exist across the SDK).
    template<class T> bool set_any(const T& value, const char* k1, const char* k2 = nullptr,
                                   const char* k3 = nullptr, const char* k4 = nullptr) {
        const char* keys[] = {k1, k2, k3, k4};
        for (int i = 0; i < 4; ++i)
            if (keys[i] && set(keys[i], value)) return true;
        return false;
    }
    // [F16] Positional binding for native functions with nameless params.
    template<class T> bool set_at(int index, const T& value) {
        int seen = 0;
        for (auto& p : m_params)
            if (!(p.flags & profile::CPF_ReturnParm) && p.size == (int)sizeof(T))
                if (seen++ == index) { memcpy(m_data.data() + p.offset, &value, sizeof(T)); return true; }
        return false;
    }
    bool set_bytes(const char* key, const void* src, int size) {
        for (auto& p : m_params)
            if (p.name == key && p.size == size && src) {
                memcpy(m_data.data() + p.offset, src, size);
                return true;
            }
        return false;
    }
    template<class T> T result() const {
        for (auto& p : m_params)
            if ((p.flags & profile::CPF_ReturnParm) && p.size == (int)sizeof(T))
                return Read<T>(m_data.data(), p.offset);
        return {};
    }
    void* result_data(int size) {
        for (auto& p : m_params)
            if ((p.flags & profile::CPF_ReturnParm) && p.size == size)
                return m_data.data() + p.offset;
        return nullptr;
    }
    template<class T> T get(const char* key) const {
        for (auto& p : m_params)
            if (p.name == key && p.size == (int)sizeof(T))
                return Read<T>(m_data.data(), p.offset);
        return {};
    }
    // [F64] Out-parameter read (CPF_OutParm). GetAllActorsOfClass-style
    // functions deliver their payload through a reference param — result<T>()
    // only matched CPF_ReturnParm, so every GetAll*OfClass call silently read
    // an empty array (this alone killed the quickbar widget poke:
    // "no quickbar widgets resident" was a false negative).
    template<class T> T get_out(const char* key) const {
        for (auto& p : m_params)
            if (p.name == key && (p.flags & profile::CPF_OutParm) && p.size == (int)sizeof(T))
                return Read<T>(m_data.data(), p.offset);
        return {};
    }
    bool run(void* object) {
        if (!m_valid || !object || !orig_PE) return false;
        // [F16] Receiver validation — refuse cross-class execution.
        void* owner = Read<void*>(m_func, profile::ObjectOuter);
        if (owner && !IsBadPtr(owner) && !IsA(object, owner)) {
            // [F77-e] widget-layer reflection quirk: the Bacchus quickbar
            // widgets report a broken inheritance chain against QuickbarBase_C,
            // so every HandleQuickbarContentChanged poke was rejected and the
            // quickbar UI stayed dead. Poke targets are collected BY class —
            // the receiver is right by construction, so allowlist this one.
            if (ObjName(m_func) != "HandleQuickbarContentChanged") {
                LOG("[Call] rejected %s: receiver=%s owner=%s", ObjName(m_func).c_str(),
                    ObjName(Read<void*>(object, profile::ObjectClass)).c_str(), ObjName(owner).c_str());
                return false;
            }
        }
        Guard g;
        orig_PE(object, m_func, m_data.data());
        return true;
    }
};

// [F16] Extract a named incoming PE parameter by reflection metadata
// (ported from the Era core incoming()).
static void* incoming(void* function, void* data, const char* key, int expected_size) {
    if (!function || !data || IsBadPtr(function)) return nullptr;
    int total = Read<uint16_t>(function, profile::FunctionParamSize);
    void* p = Read<void*>(function, profile::StructChildren);
    for (int i = 0; p && !IsBadPtr(p) && i < 128; ++i, p = Read<void*>(p, profile::FieldNext)) {
        if (!(Read<uint64_t>(p, profile::PropFlags) & profile::CPF_Parm)) continue;
        std::string n;
        if (!SafeFieldName(p, profile::ObjectName, n) || n != key) continue;
        int offset = Read<int>(p, profile::PropOffset);
        int size   = Read<int>(p, profile::PropElementSize);
        if (offset < 0 || size < 1 || offset + size > total || (expected_size && size != expected_size))
            return nullptr;
        return (char*)data + offset;
    }
    p = Read<void*>(function, profile::StructChildProps);
    for (int i = 0; p && !IsBadPtr(p) && i < 128; ++i, p = Read<void*>(p, profile::FFieldNext)) {
        if (!(Read<uint64_t>(p, profile::PropFlags) & profile::CPF_Parm)) continue;
        std::string n;
        if (!SafeFieldName(p, profile::FFieldName, n) || n != key) continue;
        int offset = Read<int>(p, profile::PropOffset);
        int size   = Read<int>(p, profile::PropElementSize);
        if (offset < 0 || size < 1 || offset + size > total || (expected_size && size != expected_size))
            return nullptr;
        return (char*)data + offset;
    }
    return nullptr;
}

// [F76-a] Force a bool-returning ProcessEvent verdict to false after the
// native ran. The frame layout is not "params then return": a zero-param
// bool function (Engine.GameMode.ReadyToEndMatch) keeps its return value at
// offset 0, so the return property is located by reflection instead of
// guessing an offset.
namespace Athena { void ForceReturnFalse(void* func, void* params); }
void Athena::ForceReturnFalse(void* func, void* params) {
    if (!func || !params || IsBadPtr(func)) return;
    int total = Read<uint16_t>(func, profile::FunctionParamSize);
    if (total <= 0 || total > 8192) return;
    void* p = Read<void*>(func, profile::StructChildren);
    for (int i = 0; p && !IsBadPtr(p) && i < 128; ++i, p = Read<void*>(p, profile::FieldNext)) {
        if (!(Read<uint64_t>(p, profile::PropFlags) & profile::CPF_ReturnParm)) continue;
        int off = Read<int>(p, profile::PropOffset);
        if (off >= 0 && off < total) Write<uint8_t>(params, (size_t)off, 0);
        return;
    }
    void* fp = Read<void*>(func, profile::StructChildProps);
    for (int i = 0; fp && !IsBadPtr(fp) && i < 128; ++i, fp = Read<void*>(fp, profile::FFieldNext)) {
        if (!(Read<uint64_t>(fp, profile::PropFlags) & profile::CPF_ReturnParm)) continue;
        int off = Read<int>(fp, profile::PropOffset);
        if (off >= 0 && off < total) Write<uint8_t>(params, (size_t)off, 0);
        return;
    }
    Write<uint8_t>(params, 0, 0);   // no reflected return — zero-param bool frame
}

// ============================================================================
// ASSET LOADING / SPAWN
// ============================================================================
static void* LoadAsset(const std::string& path,
                       const char* type_path = "/Script/FortniteGame.FortItemDefinition") {
    if (void* r = FindObj(path)) return r;
    auto s = ToWide(path);
    void* type = FindObj(type_path);
    if (!type) return nullptr;
    return Fn<void*(*)(void*, void*, const char16_t*, const char16_t*, uint32_t, void*, bool, void*)>(
        profile::StaticLoadObject)(type, nullptr, s.c_str(), nullptr, 0, nullptr, true, nullptr);
}
static void* LoadClass(const char* path, const char* base_path = "/Script/Engine.Actor") {
    if (void* r = FindObj(path)) return r;
    auto s = ToWide(path);
    void* parent = FindObj(base_path);
    if (!parent) return nullptr;
    return Fn<void*(*)(void*, void*, const char16_t*, const char16_t*, uint32_t, void*)>(
        profile::StaticLoadClass)(parent, nullptr, s.c_str(), nullptr, 0, nullptr);
}
static void* GetPC(void* world, int index = 0) {
    if (!world) return nullptr;
    return Fn<void*(*)(void*, int)>(profile::GetPlayerController)(world, index);
}
static void* GetPawn(void* controller) {
    if (!controller) return nullptr;
    static void* pawnProp = nullptr;
    static bool resolved = false;
    if (!resolved) { pawnProp = GetProp("/Script/Engine.Controller.Pawn"); resolved = true; }
    if (!pawnProp) return nullptr;
    return Read<void*>(GetField(controller, pawnProp));
}
static void Possess(void* controller, void* pawn) {
    if (!controller || !pawn) return;
    Call c("/Script/Engine.Controller.Possess");
    if (!c.ok()) { LOG("Possess: binding failed"); return; }
    if (!c.set("InPawn", pawn)) { c.dump("Possess"); return; }
    c.run(controller);
}
static void* SpawnActor(void* world, void* klass, void* owner, const Transform& t) {
    if (!world || !klass) return nullptr;
    void* actor = Fn<void*(*)(void*, void*, const Transform*, uint8_t, void*)>(
        profile::BeginSpawn)(world, klass, &t, 1, owner);
    return actor ? Fn<void*(*)(void*, const Transform*)>(profile::FinishSpawn)(actor, &t) : nullptr;
}
// [F31] Erbium building order: BeginSpawn → InitializeKismetSpawnedBuildingActor
// → FinishSpawn. Initializing only AFTER FinishSpawn re-runs component state
// on a live actor (crash on build placement).
static void* SpawnActorUnfinished(void* world, void* klass, void* owner, const Transform& t) {
    if (!world || !klass) return nullptr;
    return Fn<void*(*)(void*, void*, const Transform*, uint8_t, void*)>(
        profile::BeginSpawn)(world, klass, &t, 1, owner);
}
static void FinishSpawnActor(void* actor, const Transform& t) {
    if (!actor) return;
    Fn<void*(*)(void*, const Transform*)>(profile::FinishSpawn)(actor, &t);
}
static void* ResolveInterface(void* object, void* ifaceClass) {
    if (!object || !ifaceClass) return nullptr;
    using Fn_t = void*(*)(void*, void*);
    return ((Fn_t)(g_base + profile::GetInterfaceAddress))(object, ifaceClass);
}
// GUID generation through the native Kismet library (verified in 21.30).
static Guid MakeGuid() {
    Guid g{};
    void* klib = FindObj("/Script/Engine.Default__KismetGuidLibrary");
    if (klib) { Call c("/Script/Engine.KismetGuidLibrary.NewGuid"); if (c.ok() && c.run(klib)) g = c.result<Guid>(); }
    return g;
}

// ============================================================================
// SESSION
// ============================================================================
struct Session {
    void* world = nullptr;
    int   world_index = -1;
    void* controller = nullptr;
    void* mode = nullptr;
    void* state = nullptr;
    void* pawn = nullptr;
    void* inventory = nullptr;
    void* quickbars = nullptr;
    void* tactical_sprint = nullptr;
    bool  initialized = false;
    bool  inventory_ready = false;
    bool  aircraft_jumped = false;
    bool  attempted = false;
    bool  finalized = false;
    bool  cheats_ready = false;
    bool  safezone_spawned = false;
    bool  sprint_applied = false;
    bool  ready_called = false;
    bool  visibility_forced = false;
    bool  abilities_granted = false;
    bool  melee_granted = false;
    bool  stamina_initialized = false;
    bool  tactical_sprint_setup = false;
    bool  quickbars_initialized = false;
    bool  movement_enabled = false;
    bool  lobby_requested = false;      // [F24]
    bool  vehicles_spawned = false;     // [F29]
    bool  wildlife_spawned = false;     // [F30]
    bool  maps_forced = false;          // [F33]
    int   last_phase = -1;              // [F24] GamePhase tracking
    double finalized_at = 0;            // [F29/F30] auto-spawn timers
    // [F21] local build dedup + touch polling
    void*  held_build_class = nullptr;
    void*  last_build_class = nullptr;
    double last_build_time = 0;
    double held_build_time = 0;
    std::array<double,6> held_build_pose{};
    std::array<double,6> last_build_pose{};
    void*  locker_pickaxe = nullptr;      // [F22] resolved by SyncLockerLoadout
    double observed_at = 0;
    double retry_at = 0;
    double finalize_at = 0;
    double next_build_poll = 0;
    std::array<void*, 6> slots{};           // Primary quickbar (weapons)
    std::array<void*, 6> secondary_slots{}; // Secondary quickbar (build pieces) [F19]
    std::vector<void*> character_parts;
    std::vector<void*> granted_ability_sets;
    void* edit_tool = nullptr;
    void* wall = nullptr;
    void* floor = nullptr;
    void* stairs = nullptr;
    void* roof = nullptr;
    // [F35-F45] world systems v2 (zone/players/harvest/wildlife/doors/body/climb)
    bool  zone_driven = false;          // [F35] InitializeWithMapDefaultPhases + StartPhase(0)
    bool  players_set = false;          // [F36] PlayersLeft counter pushed to HUD
    bool  harvest_enabled = true;       // [F37] pickaxe -> wood/stone/metal
    bool  doors_hold = true;            // [F40] keep opened doors open
    bool  climb_enabled = true;         // [F42] hold-jump wall climb boost
    bool  body_fixed = false;           // [F41] pawn mesh + anim instance set
    bool  maps_logged_missing = false;  // [F43] one-shot map manager log
    int   vehicles_attempts = 0;        // [F39] spawner retry budget
    int   loot_attempts = 0;            // [F45] floor loot retry budget
    int   loot_found_total = -1;        // [F45]
    double last_harvest_scan = 0;       // [F37]
    double last_door_audit = 0;         // [F40]
    double last_climb_check = 0;        // [F42]
    double last_zone_monitor = 0;       // [F35]
    int   last_zone_phase = -1;         // [F35]
    std::vector<void*> harvest_cache;   // [F37] BuildingSMActor within 300 m
    std::map<void*, float> harvest_hp;  // [F37] last observed health
    std::vector<void*> door_cache;      // [F40] walls that own a door
    std::set<void*>    doors_ever_open; // [F40] keep these doors open
    void* vehicle_cache[32] = {};       // [F39] resolved vehicle BP classes
    int   vehicle_cache_n = 0;          // [F39]
    // [F46-F53] interaction / world-content / edit / NPC round
    std::vector<void*> containers;      // [F46] discovered map containers
    std::vector<Vector> container_locs; // [F46] cached locations (no per-tick reflection)
    std::vector<uint8_t> container_done;// [F46] already-searched flags
    std::vector<void*> vehicles;        // [F47] live vehicle actors (ours + map)
    void* interact_comp = nullptr;      // [F47] FortControllerComponent_Interaction
    void* nearest_interact = nullptr;   // [F47] current interaction candidate
    int   nearest_kind = 0;             // [F47] 1=container 2=vehicle
    bool  auto_interact = true;         // [F47] proximity auto-search / auto-enter
    std::map<void*, double> auto_interact_at; // [F47] per-actor cooldown
    double last_interact_scan = 0;      // [F47]
    bool  map_scan_done = false;        // [F46] one-time world-content scan
    bool  world_abilities_granted = false; // [F52] slide + enter-vehicle GAS
    bool  qb_widgets_poked = false;     // [F50] HandleQuickbarContentChanged poke
    bool  npcs_spawned = false;         // [F52] ambient NPC bots
    // [F54-F63] auto-world round 3 (root-cause scan fix + real spawner paths)
    int   wildlife_attempts = 0;        // [F54] Irwin animals surface late — retry
    int   qb_pokes = 0;                 // [F57] periodic quickbar widget refresh
    int   scan_retries = 0;             // [F54] re-run ScanMapContent while empty
    bool  reality_done = false;         // [F62] auto reality-sapling planting
    std::vector<void*> npcs;            // [F63] live ambient NPCs (top-up)
    double last_edit_check = 0;         // [F49] edit-tool watcher
    uint8_t last_edit_confirmed = 0;    // [F49] bEditConfirmed edge detect
    void* last_edit_actor = nullptr;    // [F49] tool-bound edit target
    std::set<uint64_t> build_cells;     // [F49] occupied grid cells
    std::vector<void*> placed_builds;   // [F49] pieces we placed (occupancy)
    // [F64-F71] Erbium 1:1 round — live nearby scan + verified vehicles +
    // part-based body + set-delivered abilities
    double last_near_refresh = 0;            // [F65] GetAllActorsOfClass cadence
    std::vector<void*> near_containers;      // [F65] live containers (streaming-aware)
    std::vector<Vector> near_container_locs; // [F65]
    std::vector<void*> near_pickups;         // [F65] live ground loot pickups
    std::vector<Vector> near_pickup_locs;    // [F65]
    std::map<void*, double> pickup_last_at;  // [F65] per-pickup collect cooldown
    int   hud_phase_kick = 0;                // [F69] ClientOnPawnSpawned re-kick
    Guid  last_equip_guid{};                 // [F75] dedupe — skip re-equip of the held item
    void* bot_inventory_cls = nullptr;       // [F75] AFortInventory for bot WorldInventory
    std::set<void*> interact_logged;         // [F75-d] one-shot per interact candidate
} session;

static double Monotonic() {
    timespec ts{}; clock_gettime(CLOCK_MONOTONIC, &ts);
    return double(ts.tv_sec) + double(ts.tv_nsec) * 1e-9;
}
// [F15] Standalone gating — never touch an online session.
static bool WorldIsStandalone(void* world) {
    if (!g_base || !world) return false;
    using IsStandalone_t = bool(*)(void*);
    return Fn<IsStandalone_t>(profile::IsStandalone)(world);
}
static bool ActorLocation(void* actor, Vector& out) {
    Call c("/Script/Engine.Actor.K2_GetActorLocation");
    if (!c.ok() || !c.run(actor)) return false;
    out = c.result<Vector>();
    return std::isfinite(out.X) && std::isfinite(out.Y) && std::isfinite(out.Z);
}
static bool NearPlayer(const Vector& point, float limit) {
    Vector origin{};
    if (!session.pawn || !ActorLocation(session.pawn, origin)) return false;
    double dx = point.X - origin.X, dy = point.Y - origin.Y, dz = point.Z - origin.Z;
    return dx*dx + dy*dy + dz*dz <= double(limit) * double(limit);
}

// ============================================================================
// [F73] ERBIUM BATTLE ROYALE LIFECYCLE — forward decls (definitions below,
// after SetPlayersLeft). 1:1 port of Erbium's BattleRoyaleGamePhaseLogic +
// FortGameMode safe-zone/aircraft/bot flow, driven from our world tick.
// ============================================================================
static void BRStartAircraftPhase();   // Erbium StartAircraftPhase
static void BRZoneTables();           // Erbium SafeZoneDefinition -> tables
static void BRPickZoneCenters();      // Erbium InitializeSafeZoneLocations
static void BRSetupSafeZoneIndicator(); // Erbium SetupSafeZoneIndicator
static void BRStartNewSafeZonePhase(int p, bool bInitial); // Erbium StartNewSafeZonePhase
static void BRStormTick();            // Erbium zone flags + storm damage
static float BRWorldTime();           // UGameplayStatics::GetTimeSeconds
static void* BRGetComponent();        // BattleRoyaleGamePhaseLogic component

// [F73] BR machine state (mirrors Erbium's component statics + locals)
struct BRCtx {
    bool   started = false;           // HandleMatchHasStarted port ran
    uint8_t phase = 0;                // EAthenaGamePhase (None..EndGame)
    uint8_t step = 0;                 // EAthenaGamePhaseStep
    void*  component = nullptr;       // FortGameStateComponent_BattleRoyaleGamePhaseLogic
    void*  mapinfo = nullptr;         // AFortAthenaMapInfo
    void*  indicator = nullptr;       // AFortSafeZoneIndicator (ours)
    void*  aircraft = nullptr;        // AFortAthenaAircraft (the bus)
    float  warmup_end = -1;           // WarmupCountdownEndTime (world seconds)
    float  safezones_start = -1;      // SafeZonesStartTime
    float  drop_start = -1, drop_end = -1, flight_end = -1; // aircraft windows
    bool   bus_unlocked = false;      // BusFlying step fired
    bool   drop_forced = false;       // OnAircraftExitedDropZone port fired
    bool   flight_done = false;       // aircraft destroyed
    bool   zone_formed = false;       // indicator + phases built
    bool   zone_shrinking = false;    // state 3 fired for current phase
    int    zone_count = 9;            // phases built
    std::vector<float> radius, wait, shrink, damage; // per-phase zone tables
    std::vector<Vector> zone_centers; // per-phase centers
    int    bots_target = 99;          // Erbium full BR roster
    int    bots_spawned = 0;          // ErbiumBot(#) counters
    std::vector<void*> bot_pawns;     // [F74-e] live bot pawns (movement driver)
    double next_bot_wave = 0;         // monotonic schedule
    double last_storm_tick = 0;
    int    last_logged_zone = -1;
    bool   alive_added = false;       // local PC in GameMode->AlivePlayers
    bool   bot_skins_logged = false;  // [F76-b] sample skin line printed
};
static BRCtx br;

// [F74-e/F75-b/F76-c] bot + wildlife movement motor state. F75 drove the
// CharacterMovementComponent Velocity directly (works but dumb: straight
// lines into walls). F76 upgrades the driver to the game's own navmesh
// pathing — UAIBlueprintHelperLibrary.SimpleMoveToLocation (native, static,
// verified 0x70fb715070) creates a PathFollowingComponent on ANY controller
// and walks real paths around obstacles. Velocity writes stay as the
// fallback for pawns whose stuck-check shows the navmesh never moved them
// (2.2s window, <40 units displacement -> velocity window with a new
// heading). Wildlife keeps the same hybrid at walk speed.
struct BRBotMotor {
    void*  pawn;
    float  yaw;
    double next_heading;      // next retarget (move request or heading roll)
    float  lastX = 0, lastY = 0;   // stuck detection anchor
    double last_check = 0;         // last stuck-check time
    uint8_t vel_mode = 0;          // 0 = try navmesh move, 1 = velocity drive
    // [F77-c] engagement — face a nearby enemy and hold ground for a beat
    void*  face = nullptr;
    double face_until = 0;
};
static std::vector<BRBotMotor> g_bot_motors;
static std::vector<BRBotMotor> g_wild_motors;   // [F75-c] animals roam too

// ============================================================================
// ATHENA NAMESPACE
// ============================================================================
namespace Athena {
    void SetupWorld(void* world);
    void Tick();
    void BRTick();                     // [F73] Erbium BR phase machine
    void BRJump();                     // [F73] jump from bus (HUD + auto-drop)
    bool TryInitializePlayer(void* world, void* mode, void* controller);
    void SetupCheats();
    void FinalizePlayer();
    void ShowSkin();
    void SyncLockerLoadout();          // [F22] MCP locker sync
    void LoadProfileCosmetics();
    void LoadDefaultCosmetics();
    void ForcePlayerVisible();         // [F73] skipped while riding the bus
    void BeginPlayerSkydiving();
    void EquipSlot(int slot);
    void DestroyLods();
    void DropLoadingScreen();
    void FixBuildingFoundations();
    void FixBus();
    void InitializeInventory();
    bool GiveItem(void* def, int count, int slot, uint8_t bar, bool require_new); // [F17]
    void AddToInventory(void* def, int count, int slot);
    void InventoryUpdate();
    void GrantDefaultAbilities();
    void GrantAbility(void* klass);
    void GrantAllAbilitySets();
    void InitStaminaAttributes();
    void RefreshHUD();
    void ApplySprintSpeed();
    void SetupTacticalSprint();
    void SetTacticalSprinting(bool sprinting);
    void SpawnSafeZoneIndicator();
    void SpawnPickup(void* def, int count, Vector loc, uint8_t sourceFlags, uint8_t spawnSource, bool toss);
    void DropInventoryItem(Guid guid, int count);
    void OnServerExecuteInventoryItem(Guid guid);
    void OnServerExecuteInventoryWeapon(void* weapon);
    void HandleJumpFromHudStart();
    void SpawnBattleBus();
    void OnAircraftJump();
    void TeleportToSpawnIsland();
    void RemoveNetDebugUI();
    void SpawnBuildPreviews();
    bool LocalPickup(void* pickup);    // [F18]
    void Loot(void* actor);
    void ForceReturnFalse(void* func, void* params);  // [F76-a] defang native bool verdicts
    void HandleDoor(void* door);
    void OnServerCreateBuildingActor(void* params);
    bool LocalBuild();                 // [F21]
    void PollTouchBuild();             // [F21]
    void OnBeginEditActor(void* piece);
    void OnFinishEditActor(void* actor, void* newClass, int rotIter, bool mirrored);
    void PlayEmoteItem(void* item);
    void OnExitVehicle();
    void ReturnToLobby(void* world);   // [F24]
    void TeleportToReticle();          // [F24]
    void ApplyCheats(const std::string& cmd); // [F24] fly/walk/ghost/god
    void CheatScript(const char* script);

    // QuickBars [F19]
    void InitializeQuickBarsNative();
    void SetQuickBarReady();
    bool BindSlot(void* item, int slot, uint8_t bar);
    void ActivateQuickBarSlot(uint8_t bar, int slot);
    void ForceUpdateQuickBars();
    void BuildQuickBarsManually();

    // Loot [F20]
    void SpawnFloorLoot();
    bool SearchContainer(void* actor, const Vector& location);

    // [F18] pickups / [movement]
    void ServerHandlePickup(void* pickup);
    void EnableMovementFeatures();

    // [F28-F33] Erbium-parity world systems
    void ForceLevelStreaming();        // [F28] stream in unloaded map regions
    int  SpawnVehiclesNear(int max);   // [F29] cars/bikes from map spawners
    int  SpawnWildlifeNear(int max);   // [F30] boars/chickens/wolves/…
    int  SpawnWildlifeScattered(int max); // [F75-c] animals across the whole map
    void ForceMapsVisible();           // [F33] minimap + full map

    // [F35-F45] world systems v2
    // [F35] DriveSafeZone removed — see [F73] (BR lifecycle owns zones)
    void SetPlayersLeft(int n);        // [F36] HUD player counter
    bool StackAdd(void* def, int count); // [F37] stack resources onto existing entry
    void HarvestTick();                // [F37] pickaxe -> wood/stone/metal
    void VehicleRetry();               // [F39] periodic spawner scan + static-load fallback
    void DoorAudit();                  // [F40] keep opened doors open, unstick them
    void FixBodyAndAnimations();       // [F41] pawn body + locomotion anims
    void ClimbTick();                  // [F42] hold-jump wall climb boost
    void SpawnFloorLootRetry();        // [F45] floor loot once levels stream in

    // [F46-F53] interaction / world-content / edit / NPC round
    void ScanMapContent();             // [F46] discover containers/spawners/sharks/saplings
    void NearbyScanRefresh();          // [F65] live containers/pickups (GetAllActorsOfClass)
    void InteractTick();               // [F47] candidate scan + HUD prompt + auto-interact
    bool DoInteract(void* actor);      // [F47] route chest/vehicle/door/prop interact
    void* GetInteractionComp();        // [F47] FortControllerComponent_Interaction
    bool EnterVehicle(void* veh);      // [F48] ForceSeatSinglePawn entry
    void PokeQuickbarWidgets();        // [F50] force-refresh QuickbarBase widgets
    bool StackRemove(void* def, int count); // [F49] build resource cost
    void SpendBuildResources(void* klass);  // [F49] -10 wood/stone/metal on place
    bool BuildCellOccupied(uint64_t key, const Vector& loc); // [F49] no build-overlap
    void EditAssist();                 // [F49] EditingPlayer + bEditConfirmed watcher
    void GrantWorldAbilities();        // [F52] slide + GA_AthenaEnterVehicle
    int  SpawnNPCs(int max);           // [F52] ambient NPC pawns
    void HarvestGrantFor(void* actor, float damage); // [F49] shared harvest grant
    int  SpawnRealitySaplings(int max); // [F62] Ch3 S3 reality-tree saplings
    void SpawnLocalMarker();           // [F60] minimap marker actor (standalone)

    // [F73] Erbium BR lifecycle — BRTick/BRJump defined in the BR section
}

// ============================================================================
// ABILITY SYSTEM
// ============================================================================
static void* GetASC(void* actor) {
    if (!actor) return nullptr;
    void* asc = GetObjField(actor, "/Script/FortniteGame.FortPawn.AbilitySystemComponent");
    if (asc) return asc;
    asc = GetObjField(actor, "/Script/FortniteGame.FortPlayerState.AbilitySystemComponent");
    if (asc) return asc;
    asc = GetObjField(actor, "/Script/GameplayAbilities.AbilitySystemComponent");
    return asc;
}

void Athena::GrantAbility(void* klass) {
    if (!klass || !session.pawn) return;
    void* asc = GetASC(session.pawn);
    if (!asc) { LOG("[Ability] no ASC"); return; }
    // [F13] TryActivateAbilityByClass is the only reflected activation entry
    // point in 21.30 (GiveAbility/K2_GiveAbility do not exist).
    Call c("/Script/GameplayAbilities.AbilitySystemComponent.TryActivateAbilityByClass");
    if (!c.ok()) return;
    bool bAllow = true;
    if (c.set("InAbilityToActivate", klass) && c.set("bAllowRemoteActivation", bAllow)) {
        c.run(asc);
        LOG("[Ability] Activated: %s", ObjName(klass).c_str());
    }
}

// FFortAbilitySetHandle (0x38): TWeakObjectPtr + GrantedAbilityHandles array
// + AppliedEffectHandles array + ItemGuidsForAdditionalItems array. The
// Android wrapper leaves the two handle arrays allocated for us — free them
// so the GAS bookkeeping does not leak (Era core behaviour).
struct FortAbilitySetHandle {
    void*   TargetAbilitySystemComponent;
    Array   GrantedAbilityHandles;
    Array   AppliedEffectHandles;
    Array   ItemGuidsForAdditionalItems;
};
static_assert(sizeof(FortAbilitySetHandle) == 0x38, "FFortAbilitySetHandle 21.30");

static bool ApplyAbilitySet(void* pawn, void* abilitySet) {
    if (!pawn || !abilitySet || IsBadPtr(abilitySet)) return false;
    void* klib = FindObj("/Script/FortniteGame.Default__FortKismetLibrary");
    if (!klib) { LOG("[Abilities] KismetLibrary missing"); return false; }
    Call c("/Script/FortniteGame.FortKismetLibrary.EquipFortAbilitySet");
    if (!c.ok()) { LOG("[Abilities] EquipFortAbilitySet binding FAILED"); c.dump("EquipFortAbilitySet"); return false; }
    void* ifaceType = FindObj("/Script/GameplayAbilities.AbilitySystemInterface");
    if (!ifaceType) { LOG("[Abilities] AbilitySystemInterface class missing"); return false; }
    void* ifaceAddr = ResolveInterface(pawn, ifaceType);
    if (!ifaceAddr) {
        void* state = GetObjField(pawn, "/Script/Engine.Controller.PlayerState");
        if (!state) {
            void* pc = GetObjField(pawn, "/Script/Engine.Pawn.Controller");
            if (pc) state = GetObjField(pc, "/Script/Engine.Controller.PlayerState");
        }
        if (state) {
            ifaceAddr = ResolveInterface(state, ifaceType);
            LOG("[Abilities] Resolved on PlayerState=%p -> %p", state, ifaceAddr);
        }
    }
    LOG("[Abilities] ResolveInterface(pawn=%p) -> iface=%p", pawn, ifaceAddr);
    TScriptInterface iface{pawn, ifaceAddr};
    bool s1 = c.set("AbilitySystemInterfaceActor", iface);
    bool s2 = c.set("AbilitySet", abilitySet);
    bool s3 = c.set("OverrideSourceObject", pawn);
    if (!s1 || !s2 || !s3) { LOG("[Abilities] param bind failed"); c.dump("EquipFortAbilitySet"); return false; }
    if (!c.run(klib)) return false;
    // Free returned handle arrays (Era core: the wrapper destroys them anyway,
    // freeing keeps the GAS bookkeeping consistent).
    FortAbilitySetHandle handle = c.result<FortAbilitySetHandle>();
    if (handle.GrantedAbilityHandles.Data) Fn<void(*)(void*)>(profile::Free)(handle.GrantedAbilityHandles.Data);
    if (handle.AppliedEffectHandles.Data) Fn<void(*)(void*)>(profile::Free)(handle.AppliedEffectHandles.Data);
    LOG("[Abilities] ✅ Applied set: %s (abilities=%d effects=%d)", ObjName(abilitySet).c_str(),
        handle.GrantedAbilityHandles.Count, handle.AppliedEffectHandles.Count);
    return true;
}

void Athena::InitStaminaAttributes() {
    if (!session.pawn) return;
    void* asc = GetASC(session.pawn);
    if (!asc) { LOG("[Stamina] no ASC"); return; }
    void* attrClass = FindObj("/Script/FortniteGame.FortPlayerAttributeSet");
    if (!attrClass) attrClass = FindObj("/Script/GameplayAbilities.AttributeSet");
    if (!attrClass) { LOG("[Stamina] AttributeSet missing"); return; }
    void* dataTable = FindObj("/Game/Athena/Items/Quests/AthenaObjectiveStatXPTable.AthenaObjectiveStatXPTable");
    Call c("/Script/GameplayAbilities.AbilitySystemComponent.K2_InitStats");
    if (!c.ok()) { LOG("[Stamina] K2_InitStats binding FAILED"); return; }
    if (!c.set("Attributes", attrClass)) { LOG("[Stamina] Attributes bind failed"); return; }
    if (dataTable) c.set("DataTable", dataTable);
    if (!c.run(asc)) return;
    session.stamina_initialized = true;
    LOG("[Stamina] Initialized FortPlayerAttributeSet");
}

void Athena::GrantAllAbilitySets() {
    if (!session.pawn || session.abilities_granted) return;
    LOG("[Abilities] Setting up...");
    void* asc = GetASC(session.pawn);
    if (!asc) { LOG("[Abilities] no ASC"); return; }
    if (!session.stamina_initialized) InitStaminaAttributes();

    void* coreSet = LoadAsset(
        "/Game/Abilities/Player/Generic/Traits/DefaultPlayer/GAS_AthenaPlayer.GAS_AthenaPlayer",
        "/Script/FortniteGame.FortAbilitySet");
    if (!coreSet) { LOG("[Abilities] GAS_AthenaPlayer missing"); session.abilities_granted = true; return; }

    // Add the mobile interaction/harvest/sprint abilities into the set before
    // EquipFortAbilitySet so the native delivery path initializes their
    // replicated specs (Era core behaviour; offsets verified: GameplayAbilities
    // @0x38 on UFortAbilitySet).
    void* abilities_prop = GetProp("/Script/FortniteGame.FortAbilitySet.GameplayAbilities");
    if (abilities_prop) {
        int off = Read<int>(abilities_prop, profile::PropOffset);
        LOG("[Abilities] GameplayAbilities offset = 0x%X (expect 0x38)", off);
        Array* abilities = reinterpret_cast<Array*>(GetField(coreSet, abilities_prop));
        if (abilities && abilities->Count >= 0 && abilities->Count < 256) {
            const char* extra[] = {
                "/Script/FortniteGame.FortGameplayAbility_Sprint",
                "/Script/FortniteGame.FortGameplayAbility_Jump",
                "/Game/Abilities/Player/Generic/Traits/DefaultPlayer/GA_DefaultPlayer_InteractSearch.GA_DefaultPlayer_InteractSearch_C",
                "/Game/Abilities/Player/Generic/Traits/DefaultPlayer/GA_DefaultPlayer_InteractUse.GA_DefaultPlayer_InteractUse_C",
                "/Game/Abilities/Player/Generic/Traits/DefaultPlayer/GA_DefaultPlayer_Harvest.GA_DefaultPlayer_Harvest_C",
                nullptr
            };
            for (int i = 0; extra[i]; ++i) {
                void* klass = LoadClass(extra[i], "/Script/GameplayAbilities.GameplayAbility");
                if (!klass) continue;
                bool present = false;
                for (int j = 0; j < abilities->Count; ++j)
                    if (Read<void*>(abilities->Data, j * 8) == klass) { present = true; break; }
                if (!present) {
                    Fn<int(*)(void*,void*,const void*)>(profile::GenericArrayAdd)(abilities, abilities_prop, &klass);
                    LOG("[Abilities] ✅ Added: %s", extra[i]);
                }
            }
            LOG("[Abilities] Total abilities in set: %d", abilities->Count);
        }
    }

    if (ApplyAbilitySet(session.pawn, coreSet)) session.granted_ability_sets.push_back(coreSet);

    // [F67] Erbium-pure: NO manual TryActivateAbilityByClass calls. Erbium
    // delivers every ability through EquipFortAbilitySet only — manual
    // activation fires the ability once OUTSIDE the spec system (no input
    // binding, no replication bookkeeping) and left every granted ability
    // dead on device. Worse, the old list force-activated
    // GA_DefaultPlayer_Death_C and GameplayAbility_CharacterJump at spawn —
    // a live Death spec + a pre-occupied jump ability is the "player is
    // unplayable / can't jump" report. Instead, add the slide class into the
    // same set whenever it becomes resident so the delivery path
    // initialises its spec properly.
    if (abilities_prop) {
        if (void* slide = FindAbilityClassNamed("GA_Athena_Player_Slide_C")) {
            Array* abilities = reinterpret_cast<Array*>(GetField(coreSet, abilities_prop));
            bool present = false;
            for (int j = 0; abilities && j < abilities->Count; ++j)
                if (Read<void*>(abilities->Data, (size_t)j * 8) == slide) { present = true; break; }
            if (!present && abilities && abilities->Count >= 0 && abilities->Count < 256) {
                Fn<int(*)(void*, void*, const void*)>(profile::GenericArrayAdd)(abilities, abilities_prop, &slide);
                LOG("[Abilities] slide class added into GAS_AthenaPlayer");
            }
        }
    }
    session.abilities_granted = true;
    session.melee_granted = true; // [F13] melee flows with the pickaxe weapon intrinsics; no separate grant API exists
    LOG("[Abilities] Done — sets=%zu (set delivery only, no manual activations)",
        session.granted_ability_sets.size());
}

void Athena::GrantDefaultAbilities() { GrantAllAbilitySets(); }

void Athena::RefreshHUD() {
    if (!session.controller) return;
    void* asc = GetASC(session.pawn);
    if (asc) {
        Call c("/Script/GameplayAbilities.AbilitySystemComponent.OnRep_ActivateAbilities");
        if (c.ok()) c.run(asc);
    }
}

// ============================================================================
// QUICKBARS  — [F01/F02/F19] fixed layout + two-bar binding with read-back
// ============================================================================
void Athena::InitializeQuickBarsNative() {
    if (!session.quickbars || IsBadPtr(session.quickbars)) { LOG("[QB] No quickbar"); return; }
    const uint32_t expected[] = {0xa9bd57f6, 0xa9014ff4, 0xa9027bfd, 0x910083fd};
    if (memcmp(reinterpret_cast<void*>(g_base + profile::InitializeAthenaQuickBars), expected, sizeof(expected)) != 0)
        LOG("[QB] ⚠️ prologue mismatch at 0x%llX", (unsigned long long)(g_base + profile::InitializeAthenaQuickBars));
    void* vtable = Read<void*>(session.quickbars);
    if (!vtable || IsBadPtr(vtable)) { LOG("[QB] vtable invalid"); return; }
    uintptr_t init_slot = Read<uintptr_t>(vtable, profile::QuickBars_VtableSlot);
    uintptr_t expected_fn = g_base + profile::InitializeAthenaQuickBars;
    LOG("[QB] vtable=%p slot+0x%X=0x%llX expected=0x%llX", vtable, profile::QuickBars_VtableSlot, (unsigned long long)init_slot, (unsigned long long)expected_fn);
    if (init_slot != expected_fn) { LOG("[QB] ❌ vtable mismatch — SKIPPING native call"); return; }
    LOG("[QB] ✅ vtable match — calling native init");
    void* primary = (char*)session.quickbars + profile::QuickBars_PrimaryOffset;
    void* secondary = (char*)session.quickbars + profile::QuickBars_SecondaryOffset;
    Array p_before = Read<Array>(primary, profile::QuickBar_SlotsOffset);
    Array s_before = Read<Array>(secondary, profile::QuickBar_SlotsOffset);
    LOG("[QB] Before: Primary=%d Secondary=%d", p_before.Count, s_before.Count);
    { Guard g; reinterpret_cast<void(*)(void*)>(init_slot)(session.quickbars); }
    Array p_after = Read<Array>(primary, profile::QuickBar_SlotsOffset);
    Array s_after = Read<Array>(secondary, profile::QuickBar_SlotsOffset);
    LOG("[QB] After: Primary=%d Secondary=%d", p_after.Count, s_after.Count);
    if (p_after.Count >= 1 && s_after.Count >= 1) { LOG("[QB] ✅ Init succeeded"); session.quickbars_initialized = true; }
    else LOG("[QB] ⚠️ Init may have failed");
}

// [F01] Manual QuickBar construction — the enable byte is at slot+0x10
// (bEnabled bit0 | bIsDirty bit1 | bIsReserved bit2 | bIsOccupied bit3);
// the old code wrote slot+0x18 which is UsedByItemGuid.
void Athena::BuildQuickBarsManually() {
    if (!session.quickbars) { LOG("[QB] No quickbars"); return; }
    // [F44] Dedupe: if slot 0 of the primary bar already holds a GUID we are
    // already filled — re-allocating would leak arrays and re-log noise.
    {
        void* barChk = GetProp("/Script/FortniteGame.FortQuickBars.PrimaryQuickBar");
        if (barChk) {
            void* bar = GetField(session.quickbars, barChk);
            Array arr = Read<Array>((char*)bar + profile::QuickBar_SlotsOffset);
            if (arr.Count > 0 && arr.Data && !IsBadPtr(arr.Data)) {
                Guid g0 = Read<Guid>((char*)arr.Data + 0x18, 0);   // UsedByItemGuid
                if (g0.A | g0.B | g0.C | g0.D) {
                    session.quickbars_initialized = true;
                    return;   // already filled
                }
            }
        }
    }
    LOG("[QB] Building QuickBars manually...");

    struct SlotEntry { int idx; Guid guid; uint8_t bar; };
    std::vector<SlotEntry> entries;

    void* invProp = GetProp("/Script/FortniteGame.FortInventory.Inventory");
    void* instProp = GetProp("/Script/FortniteGame.FortItemList.ItemInstances");
    if (!invProp || !instProp) return;

    auto collect = [&](std::array<void*,6>& src, uint8_t bar) {
        for (int i = 0; i < 6; ++i) {
            void* inst = src[i];
            if (!inst) continue;
            Call g("/Script/FortniteGame.FortItem.GetItemGuid");
            if (g.ok() && g.run(inst)) entries.push_back({i, g.result<Guid>(), bar});
        }
    };
    collect(session.slots, 0);
    collect(session.secondary_slots, 1);

    LOG("[QB] Collected %zu item GUIDs", entries.size());

    void* barProp = GetProp("/Script/FortniteGame.FortQuickBars.PrimaryQuickBar");
    void* secProp = GetProp("/Script/FortniteGame.FortQuickBars.SecondaryQuickBar");
    void* slotsProp = GetProp("/Script/FortniteGame.QuickBar.Slots");   // [F02] struct name is QuickBar
    if (!barProp || !secProp || !slotsProp) { LOG("[QB] QuickBar reflection missing"); return; }

    auto ensure_slots = [&](void* bar, int want) -> Array {
        void* slotsField = (char*)bar + profile::QuickBar_SlotsOffset;
        Array arr = Read<Array>(slotsField);
        if (arr.Count < want) {
            void* newData = FMalloc(profile::QuickBarSlot_SlotSize * want);   // [F55] UE allocator
            if (newData) {
                memset(newData, 0, profile::QuickBarSlot_SlotSize * want);
                Write<void*>(slotsField, 0, newData);
                Write<int32>(slotsField, 8, want);
                Write<int32>(slotsField, 12, want);
                arr = Read<Array>(slotsField);
                LOG("[QB] Allocated %d slots at %p", want, newData);
            }
        }
        return arr;
    };

    Array primArr = ensure_slots(GetField(session.quickbars, barProp), 6);
    Array secArr  = ensure_slots(GetField(session.quickbars, secProp), 5);

    for (auto& e : entries) {
        Array& arr = e.bar == 0 ? primArr : secArr;
        if (e.idx < 0 || e.idx >= arr.Count || !arr.Data) continue;
        void* slot = (char*)arr.Data + (size_t)e.idx * profile::QuickBarSlot_SlotSize;
        // Items array (TArray<FGuid>) at slot+0x00
        void* itemsField = (char*)slot + profile::QuickBarSlot_ItemsOffset;
        Guid* guidData = (Guid*)FMalloc(16);   // [F55] UE allocator (engine may realloc/free)
        if (!guidData) continue;
        *guidData = e.guid;
        Write<void*>(itemsField, 0, guidData);
        Write<int32>(itemsField, 8, 1);
        Write<int32>(itemsField, 12, 1);
        // [F01] bEnabled bit0 + bIsOccupied bit3 on the flags byte at +0x10
        uint8_t* flags = (uint8_t*)((char*)slot + profile::QuickBarSlot_EnabledOffset);
        *flags |= profile::QuickBarSlot_EnabledBit | profile::QuickBarSlot_OccupiedBit;
        LOG("[QB] Bar%u slot %d -> GUID %08X:%08X", e.bar, e.idx, e.guid.A, e.guid.B);
    }

    session.quickbars_initialized = true;
    LOG("[QB] ✅ Manual initialization done");
}

void Athena::SetQuickBarReady() {
    if (!session.controller) return;
    void* qp = GetProp("/Script/FortniteGame.FortPlayerController.ClientQuickBars");
    if (!qp) { LOG("[QB] ClientQuickBars property not found"); return; }
    int propOff = Read<int>(qp, profile::PropOffset);
    LOG("[QB] ClientQuickBars offset = 0x%X (expect 0x%X)", propOff, profile::Controller_ClientQuickBars);
    if (propOff != (int)profile::Controller_ClientQuickBars) { LOG("[QB] ❌ offset mismatch — SKIPPING"); return; }
    if (!session.quickbars_initialized) { LOG("[QB] ❌ QuickBars not initialized — SKIPPING"); return; }
    uint8_t before = Read<uint8_t>(session.controller, profile::Controller_QuickBarReadyByte);
    if (before > 1) { LOG("[QB] Unexpected byte=%u", before); return; }
    Write<uint8_t>(session.controller, profile::Controller_QuickBarReadyByte, 1);
    LOG("[QB] ✅ Readiness byte: %u -> 1", before);
}

// [F19] Bind through the native Kismet helper, then verify by reading the
// slot back. EquipItemToQuickBarSlot(WorldContextObject, ItemDefinition,
// ItemVariantGuid, QuickBarType, SlotIndex) exists in 21.30.
bool Athena::BindSlot(void* item, int slot, uint8_t bar) {
    if (!item || !session.controller || bar > 1 || slot < 0) return false;
    void* definition = nullptr;
    { Call d("/Script/FortniteGame.FortItem.GetItemDefinitionBP"); if (d.ok() && d.run(item)) definition = d.result<void*>(); }
    if (!definition) return false;
    Guid guid{};
    { Call g("/Script/FortniteGame.FortItem.GetItemGuid"); if (g.ok() && g.run(item)) guid = g.result<Guid>(); }
    if (!(guid.A | guid.B | guid.C | guid.D)) return false;

    Call bind("/Script/FortniteGame.FortKismetLibrary.EquipItemToQuickBarSlot");
    if (bind.ok() && bind.set("WorldContextObject", session.world) &&
        bind.set("ItemDefinition", definition) && bind.set("ItemVariantGuid", guid) &&
        bind.set("QuickBarType", bar) && bind.set("SlotIndex", slot))
        bind.run(FindObj("/Script/FortniteGame.Default__FortKismetLibrary"));

    // Verify through reflection layout checks before trusting anything.
    // (21.30: FQuickBarSlot stride 0x28, Items is TArray<FGuid> — the
    // definitive verification is the GetItemInQuickbarSlot read-back below.)
    void* barProp = GetProp(bar == 0 ? "/Script/FortniteGame.FortQuickBars.PrimaryQuickBar"
                                     : "/Script/FortniteGame.FortQuickBars.SecondaryQuickBar");
    void* slotsProp = GetProp("/Script/FortniteGame.QuickBar.Slots");       // [F02]
    void* itemsProp = GetProp("/Script/FortniteGame.QuickBarSlot.Items");   // [F02]
    if (!barProp || !slotsProp || !itemsProp) return false;
    void* quickbar = GetObjField(session.controller, "/Script/FortniteGame.FortPlayerController.ClientQuickBars");
    if (!quickbar) return false;
    void* barField = GetField(quickbar, barProp);
    Array slots = Read<Array>(GetField(barField, slotsProp));
    if (!slots.Data || slots.Count < 1 || slot >= slots.Count) return false;
    void* target = (char*)slots.Data + (size_t)slot * profile::QuickBarSlot_SlotSize;
    Array values = Read<Array>(GetField(target, itemsProp));
    bool matched = false;
    for (int i = 0; i < values.Count; ++i)
        if (!memcmp((char*)values.Data + (size_t)i * sizeof(Guid), &guid, sizeof(Guid))) matched = true;
    if (!matched && values.Count == 1) {
        Write<Guid>(values.Data, 0, guid);   // stale temporary GUID must not survive reconciliation
        matched = true;
    }
    if (!matched && values.Count == 0) {
        void* itemsField = GetField(target, itemsProp);
        Fn<int(*)(void*,void*,const void*)>(profile::GenericArrayAdd)(itemsField, itemsProp, &guid);
        values = Read<Array>(itemsField);
        matched = values.Count >= 1 && values.Data && !memcmp(values.Data, &guid, sizeof(Guid));
        if (matched) {
            uint8_t* flags = (uint8_t*)((char*)target + profile::QuickBarSlot_EnabledOffset);
            *flags |= profile::QuickBarSlot_EnabledBit | profile::QuickBarSlot_OccupiedBit;
        }
        LOG("[QB] Standalone slot repair bar=%u slot=%d guid_verified=%d", bar, slot, (int)matched);
    }
    // [F19] Read back through the controller API for a definitive answer.
    Call query("/Script/FortniteGame.FortPlayerController.GetItemInQuickbarSlot");
    void* actual = nullptr;
    if (query.ok() && query.set("QuickBarType", bar) && query.set("SlotIndex", slot) && query.run(session.controller))
        actual = query.result<void*>();
    if (actual) matched = matched || (actual == item);
    LOG("[QB] Slot bar=%u slot=%d verified=%d", bar, slot, (int)matched);
    return matched;
}

void Athena::ActivateQuickBarSlot(uint8_t bar, int slot) {
    if (!session.controller) return;
    Call act("/Script/FortniteGame.FortPlayerController.ClientActivateSlot");
    if (!act.ok()) { LOG("[QB] ClientActivateSlot binding failed"); return; }
    float delay = 0.f; bool bUpdatePrev = true; bool bForce = true;
    if (!act.set("InQuickBar", bar) || !act.set("Slot", slot) || !act.set("ActivateDelay", delay) ||
        !act.set("bUpdatePreviousFocusedSlot", bUpdatePrev) || !act.set("bForceExecution", bForce)) {
        LOG("[QB] ClientActivateSlot param binding failed"); return;
    }
    if (act.run(session.controller)) LOG("[QB] ✅ ClientActivateSlot bar=%u slot=%d", bar, slot);
}

void Athena::ForceUpdateQuickBars() {
    if (!session.controller) return;
    for (uint8_t bar : {(uint8_t)0, (uint8_t)1}) {
        Call c("/Script/FortniteGame.FortPlayerController.ClientForceUpdateQuickbar");
        if (c.ok() && c.set("QuickbarToRefresh", bar)) c.run(session.controller);
    }
}

// [F50/F57] Widget-level quickbar refresh — the HUD quickbar slots only
// rebuild when its content-changed BP event fires. On this offline setup the
// event never arrives after our manual slot fill, so we poke every resident
// quickbar widget directly. 21.30's HUD uses the Bacchus family:
//   Quickbar.Quickbar_C : UBacchusQuickbar, QuickbarBase.QuickbarBase_C :
//   UFortQuickbarBase (+ BacchusQuickbarPrimary/Secondary) — the old probe
//   only asked for /Script/FortniteUI.FortQuickbarBase and never matched.
void Athena::PokeQuickbarWidgets() {
    if (!session.world) return;
    void* wb = FindObj("/Script/UMG.Default__WidgetBlueprintLibrary");
    if (!wb) { LOG("[QBW] WidgetBlueprintLibrary missing"); return; }
    Call gac("/Script/UMG.WidgetBlueprintLibrary.GetAllWidgetsOfClass");
    if (!gac.ok()) { LOG("[QBW] GetAllWidgetsOfClass missing"); return; }

    static const char* const bases[] = {
        "/Script/FortniteUI.FortQuickbarBase",
        "/Script/FortniteUI.BacchusQuickbarBase",
        "/Script/FortniteUI.BacchusQuickbar",
        "/Script/UMG.UserWidget",          // last resort — receiver check gates it
        nullptr
    };
    std::vector<void*> widgets;
    for (int b = 0; bases[b]; ++b) {
        void* base = FindObj(bases[b]);
        if (!base) continue;
        gac.set("WorldContextObject", session.world);
        gac.set("WidgetClass", base);
        if (!gac.run(wb)) continue;
        // [F64] FoundWidgets is an out param (CPF_OutParm) — result<Array>()
        // only matched the return slot and always read an empty array.
        Array got = gac.get_out<Array>("FoundWidgets");
        if (!got.Data) got = gac.get<Array>("FoundWidgets");   // flag-less fallback
        if (!got.Data || got.Count < 0 || got.Count > 256) continue;
        for (int i = 0; i < got.Count && widgets.size() < 16; ++i) {
            void* w = Read<void*>(got.Data, (size_t)i * 8);
            if (w && !IsBadPtr(w) &&
                std::find(widgets.begin(), widgets.end(), w) == widgets.end())
                widgets.push_back(w);
        }
    }
    if (widgets.empty()) { LOG("[QBW] no quickbar widgets resident (Bacchus family absent)"); return; }

    // [F57] event-name variants across widget BPs (Bacchus may rename the
    // FortniteUI classic HandleQuickbarContentChanged).
    static const char* const events[] = {
        "HandleQuickbarContentChanged", "OnQuickbarContentChanged",
        "HandleQuickBarContentChanged", "UpdateQuickbar", "RefreshQuickbar",
        nullptr
    };
    std::vector<void*> funcs;
    for (int e = 0; events[e] && funcs.empty(); ++e)
        funcs = FindAllFunctionsNamed(events[e], 8);
    if (funcs.empty()) { LOG("[QBW] no quickbar content-changed function found"); return; }

    int32 slots[6] = {0, 1, 2, 3, 4, 5};
    Array changed{ slots, 6, 6 };
    // [F76-f] the BP event's enum param is spelled differently across widget
    // generations — try the known spellings per receiver instead of failing
    // the whole widget on one name.
    static const char* const barNames[] = {"QuickBarType", "QuickbarType", "QuickBarIndex", "QuickBar", nullptr};
    int poked = 0;
    for (void* w : widgets) {
        bool done = false;
        for (void* f : funcs) {
            Call c(f);
            if (!c.ok()) continue;
            bool ran = true;
            for (uint8_t bar : {(uint8_t)0, (uint8_t)1}) {
                bool setOk = false;
                for (int bn = 0; barNames[bn] && !setOk; ++bn)
                    setOk = c.set(barNames[bn], bar);
                if (!setOk || !c.set_bytes("ChangedSlots", &changed, sizeof(Array)) || !c.run(w)) { ran = false; break; }
            }
            if (ran) { done = true; break; }
        }
        if (done) ++poked;
    }
    LOG("[QBW] poked %d/%d quickbar widgets (functions=%zu)", poked, (int)widgets.size(), funcs.size());
}

// ============================================================================
// MOVEMENT FEATURES
// ============================================================================
void Athena::EnableMovementFeatures() {
    if (!session.pawn) return;
    LOG("[Movement] Enabling slide & swim...");

    Call canSlide("/Script/FortniteGame.FortPlayerPawn.SetCanSlide");
    if (canSlide.ok()) {
        bool bTrue = true;
        if (canSlide.set("bCanSlide", bTrue)) {
            if (canSlide.run(session.pawn)) LOG("[Movement] ✅ SetCanSlide(true)");
            else LOG("[Movement] SetCanSlide run failed");
        }
    } else LOG("[Movement] SetCanSlide not found");

    // [F52] bWantsToSlide is game-driven (sprint + crouch) — forcing it on
    // permanently caused broken slide behaviour. Only SetCanSlide stays.
    void* movement = GetObjField(session.pawn, "/Script/Engine.Character.CharacterMovement");
    if (movement && !IsBadPtr(movement)) {
        const char* swimPaths[] = {
            "/Script/Engine.NavMovementComponent.bCanSwim",
            "/Script/Engine.MovementComponent.bCanSwim",
            "/Script/Engine.CharacterMovementComponent.bCanSwim",
            nullptr
        };
        for (int i = 0; swimPaths[i]; ++i) {
            void* p = GetProp(swimPaths[i]);
            if (p) { SetBool(movement, swimPaths[i], true); LOG("[Movement] ✅ Set %s", swimPaths[i]); break; }
        }
    }

    // [F13/F52] Slide ability rides in the ability set delivery path — the
    // 21.30 class is GA_Athena_Player_Slide (name scan; the old hardcoded
    // mount path was wrong and never loaded).
    void* slideAbility = FindAbilityClassNamed("GA_Athena_Player_Slide_C");
    if (slideAbility) {
        LOG("[Movement] Found slide ability (GA_Athena_Player_Slide_C) — activating through ASC");
        GrantAbility(slideAbility);
    } else {
        LOG("[Movement] Slide ability class not resident yet (GrantWorldAbilities retries)");
    }

    session.movement_enabled = true;
    LOG("[Movement] Done");
}

// ============================================================================
// TACTICAL SPRINT
// ============================================================================
void Athena::SetupTacticalSprint() {
    if (!session.controller) return;
    if (session.tactical_sprint_setup && session.tactical_sprint) return;
    LOG("[TacticalSprint] Setting up component...");
    void* compClass = FindObj("/Script/TacticalSprintRuntime.FortPlayerControllerComponent_TacticalSprint");
    if (!compClass) { LOG("[TacticalSprint] Component class not found"); session.tactical_sprint_setup = true; return; }
    void* comp = nullptr;
    Call get("/Script/Engine.Actor.GetComponentByClass");
    if (get.ok() && get.set("ComponentClass", compClass) && get.run(session.controller)) comp = get.result<void*>();
    if (!comp || IsBadPtr(comp)) {
        LOG("[TacticalSprint] Adding component...");
        Call add("/Script/Engine.Actor.AddComponentByClass");
        if (add.ok()) {
            bool bManual = false; Transform t{}; bool bDeferred = false;
            add.set("Class_0", compClass) || add.set("Class", compClass);
            add.set("bManualAttachment", bManual);
            add.set("RelativeTransform", t); add.set("bDeferredFinish", bDeferred);
            if (add.run(session.controller)) { comp = add.result<void*>(); LOG("[TacticalSprint] Component added: %p", comp); }
        }
    }
    if (!comp || IsBadPtr(comp)) { LOG("[TacticalSprint] Failed to create component"); session.tactical_sprint_setup = true; return; }
    session.tactical_sprint = comp;
    uint8_t* flags = (uint8_t*)comp + profile::TacticalSprint_Flags;
    *flags |= profile::TacticalSprint_Bit_Enabled;
    LOG("[TacticalSprint] Enabled, flags=0x%02X", *flags);
    Call set("/Script/TacticalSprintRuntime.FortPlayerControllerComponent_TacticalSprint.SetIsSprinting");
    if (set.ok()) {
        bool bSprint = true;
        if (set.set("bNewIsSprinting", bSprint)) set.run(comp);
    }
    session.tactical_sprint_setup = true;
    LOG("[TacticalSprint] ✅ Setup complete");
}
void Athena::SetTacticalSprinting(bool sprinting) {
    if (!session.tactical_sprint) return;
    Call set("/Script/TacticalSprintRuntime.FortPlayerControllerComponent_TacticalSprint.SetIsSprinting");
    if (set.ok() && set.set("bNewIsSprinting", sprinting)) set.run(session.tactical_sprint);
}

void Athena::ApplySprintSpeed() {
    if (!session.pawn || IsBadPtr(session.pawn)) return;
    void* pawnCls = FindObj("/Game/Athena/PlayerPawn_Athena.PlayerPawn_Athena_C");
    if (pawnCls && IsA(session.pawn, pawnCls)) {
        float* sprintSpeed = (float*)((char*)session.pawn + profile::PlayerPawn_Athena_Speed_Sprint);
        if (!IsBadPtr(sprintSpeed)) {
            float old = *sprintSpeed;
            if (std::isfinite(old) && old > 0.0f && old < 5000.0f) {
                if (fabsf(old - 1300.0f) > 1.0f) { *sprintSpeed = 1300.0f; LOG("[Sprint] Speed_Sprint: %.0f -> 1300", old); }
            }
        }
    }
    void* cmc = GetObjField(session.pawn, "/Script/Engine.Character.CharacterMovement");
    if (cmc && !IsBadPtr(cmc)) {
        void* prop = GetProp("/Script/Engine.CharacterMovementComponent.MaxWalkSpeed");
        if (prop) { float* val = (float*)GetField(cmc, prop); if (val && !IsBadPtr(val)) { if (fabsf(*val - 600.f) > 1.0f) *val = 600.f; } }
        void* flyProp = GetProp("/Script/Engine.CharacterMovementComponent.MaxFlySpeed");
        if (flyProp) { float* val = (float*)GetField(cmc, flyProp); if (val && !IsBadPtr(val)) { if (fabsf(*val - 1200.f) > 1.0f) *val = 1200.f; } }
    }
    session.sprint_applied = true;
}

// ============================================================================
// BATTLE BUS / SKYDIVING — [F04] aircraft jump via the 21.30 component
// ============================================================================
static void* GetAircraftComponent(void* controller) {
    if (!controller) return nullptr;
    void* cls = FindObj("/Script/FortniteGame.FortControllerComponent_Aircraft");
    if (!cls) return nullptr;
    Call get("/Script/Engine.Actor.GetComponentByClass");
    if (!get.ok() || !get.set("ComponentClass", cls)) return nullptr;
    if (!get.run(controller)) return nullptr;
    return get.result<void*>();
}

void Athena::SpawnBattleBus() {
    // [F73] cheat path — run the real Erbium aircraft phase when the BR
    // machine is live; otherwise keep the legacy phase-3 poke so the command
    // never dead-ends.
    if (br.started && br.phase < 3) { BRStartAircraftPhase(); return; }
    if (!session.mode || !session.state) return;
    LOG("[Bus] legacy aircraft poke (BR machine idle)");
    void* gp = GetProp("/Script/FortniteGame.FortGameStateAthena.GamePhase");
    if (gp) {
        uint8_t phase = 3;
        Write<uint8_t>(GetField(session.state, gp), 0, phase);
        Call n("/Script/FortniteGame.FortGameStateAthena.OnRep_GamePhase");
        if (n.ok() && n.set("OldGamePhase", (uint8_t)2)) n.run(session.state);
    }
    if (session.pawn) {
        Vector loc{0.0, 0.0, 50000.0}; Rotator rot{0.0, 0.0, 0.0};
        Call sl("/Script/Engine.Actor.K2_SetActorLocationAndRotation");
        if (sl.ok()) {
            bool bSweep = false, bTeleport = true;
            sl.set("NewLocation", loc); sl.set("NewRotation", rot);
            sl.set("bSweep", bSweep); sl.set("bTeleport", bTeleport);
            sl.run(session.pawn);
        }
        BeginPlayerSkydiving();
    }
    LOG("[Bus] Aircraft phase + skydive dispatched");
}

// [F05] The touch jump button raises UAthenaHUDContext.JumpFromHudStart; the
// standalone build has no authoritative server for the aircraft RPC, so run
// the local skydive fallback and restore the pawn as the view target (Era
// core behaviour, ported with the 21.30 component path added).
void Athena::HandleJumpFromHudStart() {
    if (!session.controller || !session.pawn) return;
    if (session.aircraft_jumped) return;
    Guard g;
    LOG("[Bus] JumpFromHudStart intercepted");
    Vector rot{0, 0, 0};
    void* rotProp = GetProp("/Script/Engine.Controller.ControlRotation");
    if (rotProp && Read<int>(rotProp, profile::PropElementSize) == 24) {
        void* addr = GetField(session.controller, rotProp);
        if (addr) rot = Read<Vector>(addr);
    }
    // [F04] ServerAttemptAircraftJump lives on UFortControllerComponent_Aircraft.
    void* aircraft = GetAircraftComponent(session.controller);
    if (aircraft) {
        Call ac("/Script/FortniteGame.FortControllerComponent_Aircraft.ServerAttemptAircraftJump");
        if (ac.ok()) {
            Rotator r{rot.X, rot.Y, rot.Z};
            if (ac.set("ClientRotation", r)) ac.run(aircraft);
        }
    } else {
        LOG("[Bus] Aircraft component unavailable — local fallback only");
    }
    Call sky("/Script/FortniteGame.FortPlayerPawn.BeginSkydiving");
    if (sky.ok()) { bool bFromBus = true; if (sky.set("bFromBus", bFromBus)) sky.run(session.pawn); }
    Write<uint8_t>(session.pawn, profile::FortPlayerPawn_bIsSkydiving, 1);
    Call cam("/Script/Engine.PlayerController.SetViewTargetWithBlend");
    if (cam.ok()) { cam.set("NewViewTarget", session.pawn); float blend = 0.0f; cam.set("BlendTime", blend); cam.run(session.controller); }
    Call rep("/Script/FortniteGame.FortPlayerPawn.OnRep_IsSkydiving");
    if (rep.ok()) rep.run(session.pawn);
    session.aircraft_jumped = true;
    LOG("[Bus] Skydiving started");
}
void Athena::OnAircraftJump() { BRJump(); } // [F73] bus-position teleport + skydive
void Athena::BeginPlayerSkydiving() {
    if (!session.pawn || IsBadPtr(session.pawn)) return;
    Call bsd("/Script/FortniteGame.FortPlayerPawn.BeginSkydiving");
    if (bsd.ok()) { bool bFromBus = true; if (bsd.set("bFromBus", bFromBus)) { bsd.run(session.pawn); LOG("[Skydive] BeginSkydiving(true)"); } }
    Write<uint8_t>(session.pawn, profile::FortPlayerPawn_bIsSkydiving, 1);
}

void Athena::SetupCheats() {
    void* pc = session.controller; if (!pc) return;
    void* p = GetProp("/Script/Engine.PlayerController.CheatManager");
    void* klass = FindObj("/Script/FortniteGame.FortCheatManager");
    if (!klass) klass = FindObj("/Script/Engine.CheatManager");
    if (!p || !klass || Read<int>(p, profile::PropElementSize) != 8) return;
    void* cheat = Read<void*>(GetField(pc, p));
    if (!cheat) {
        cheat = Fn<void*(*)(void*, void*)>(profile::SpawnObject)(klass, pc);
        if (cheat) Write<void*>(GetField(pc, p), 0, cheat);
    }
    if (cheat) {
        Call init("/Script/Engine.CheatManager.ReceiveInitCheatManager");
        if (init.ok()) init.run(cheat);
        session.cheats_ready = true;
    }
}

// ============================================================================
// DOORS — [F70] Erbium-pure: BlueprintOnInteract drives open AND close
// ============================================================================
void Athena::HandleDoor(void* door) {
    if (!door || IsBadPtr(door)) return;
    static void* wallCls = nullptr;
    if (!wallCls) wallCls = FindObj("/Script/FortniteGame.BuildingWall");
    if (!wallCls || !IsA(door, wallCls)) return;

    // [F40] 0x0FDE bit layout (ABuildingWall): bit0 bSwingingDoor, bit1
    // bSlidingDoor, bit2 bAutomaticSlidingDoor, bit3 bDoubleDoor, bit4
    // bCreateDoorLink, bit5 bDoorOpen, bit6 bLocalDoorOpen, bit7
    // bDoorCollisionDisabled. Read-only here — the BP owns the state now.
    constexpr uint8_t kOpenBit = 0x20;
    uint8_t* byte = (uint8_t*)door + profile::BuildingWall_bDoorOpen_Byte;
    uint8_t cur = *byte;
    bool wasOpen = (cur & kOpenBit) != 0;

    // [F70] Erbium's entire door system is ONE call: BlueprintOnInteract with
    // the receiver's BP override — the door toggles itself (animation, sound,
    // both directions). The old manual bit toggle could only open (the close
    // branch fought the door BP state machine — "doors only open").
    bool handled = false;
    if (session.pawn) {
        Call interact("/Script/FortniteGame.BuildingActor.BlueprintOnInteract", door);
        if (interact.ok() && interact.set("InteractingPawn", session.pawn) && interact.run(door)) handled = true;
    }
    if (!handled) {
        // Fallback: manual bit toggle (both directions, config bits intact).
        constexpr uint8_t kStateMask = 0x60;   // open bit + local bit
        uint8_t next = (uint8_t)(cur & ~kStateMask);
        if (!wasOpen) next |= kStateMask;      // open: bDoorOpen + bLocalDoorOpen
        else          next |= kOpenBit;        // close: server closed, local follows
        *byte = next;
        Call r("/Script/FortniteGame.BuildingWall.OnRep_bDoorOpen");
        if (r.ok()) r.run(door);
    }
    if (!wasOpen) session.doors_ever_open.insert(door);   // [F40] remember player-opened doors
    else          session.doors_ever_open.erase(door);    // deliberate close wins

    Call netup("/Script/Engine.Actor.ForceNetUpdate");
    if (netup.ok()) netup.run(door);

    LOG("[Door] %s via %s (state=0x%02X)", wasOpen ? "CLOSE" : "OPEN",
        handled ? "BlueprintOnInteract" : "bit-toggle", (unsigned)*byte);
}

// ============================================================================
// COSMETICS — [F22] locker sync through the MCP context, with the menu-hero
// parts fallback from the original hook (Era core setup_cosmetics ported).
// ============================================================================
static std::vector<void*> SoftAssetArray(void* owner, const char* path) {
    std::vector<void*> result;
    void* p = GetProp(path);
    if (!p || !owner || IsBadPtr(owner)) return result;
    // UE5-safe: the element stride is the property's own ElementSize
    // (TSoftObjectPtr in 21.30 is 24 bytes); no 0x80 inner probing needed.
    int stride = Read<int>(p, profile::PropElementSize);
    Array values = Read<Array>(GetField(owner, p));
    if (stride < 8 || stride > 128 || values.Count < 0 || values.Count > 64 || (values.Count && !values.Data)) return result;
    for (int i = 0; i < values.Count; ++i) {
        Call load("/Script/Engine.KismetSystemLibrary.LoadAsset_Blocking");
        if (load.ok() && load.set_bytes("Asset", (char*)values.Data + (size_t)i * stride, stride) &&
            load.run(FindObj("/Script/Engine.Default__KismetSystemLibrary")))
            if (void* value = load.result<void*>()) result.push_back(value);
    }
    return result;
}

// FFortAthenaLoadout shadow (21.30 size 0x110 — verified: last member
// CachedProfileSource @0x108 + 8).
struct FFortAthenaLoadoutShadow { uint8_t Bytes[0x110]; };

void Athena::SyncLockerLoadout() {
    if (!session.controller || !session.pawn) return;
    // [F22] AFortPlayerController.CosmeticLoadoutPC @0x1E70 (FFortAthenaLoadout).
    void* loadoutProp = GetProp("/Script/FortniteGame.FortPlayerController.CosmeticLoadoutPC");
    if (!loadoutProp) { LOG("[Locker] CosmeticLoadoutPC not present"); return; }
    void* loadout = GetField(session.controller, loadoutProp);

    void* character = GetObjField(loadout, "/Script/FortniteGame.FortAthenaLoadout.Character");
    void* pickaxe   = GetObjField(loadout, "/Script/FortniteGame.FortAthenaLoadout.Pickaxe");
    void* backpack  = GetObjField(loadout, "/Script/FortniteGame.FortAthenaLoadout.Backpack");

    // MCP route: GetContext(FortMcpContext).GetLoadoutForPlayer(PlayerID)
    void* info = GetObjField(session.controller, "/Script/FortniteGame.FortPlayerController.MyPlayerInfo");
    void* idProp = GetProp("/Script/FortniteGame.FortRegisteredPlayerInfo.PlayerID");
    if (info && idProp && !character) {
        void* ctxType = FindObj("/Script/FortniteGame.FortMcpContext");
        Call context("/Script/BlueprintContext.BlueprintContextLibrary.GetContext");
        if (ctxType && context.ok() &&
            context.set("ContextObject", session.controller) &&
            (context.set("Class", ctxType) || context.set("Class_0", ctxType)) &&
            context.run(FindObj("/Script/BlueprintContext.Default__BlueprintContextLibrary"))) {
            void* mcp = context.result<void*>();
            int idSize = Read<int>(idProp, profile::PropElementSize);
            Call get("/Script/FortniteGame.FortMcpContext.GetLoadoutForPlayer");
            if (mcp && get.ok() && get.set_bytes("MemberUniqueId", GetField(info, idProp), idSize) && get.run(mcp)) {
                void* value = get.result_data(Read<int>(loadoutProp, profile::PropElementSize));
                if (value) {
                    int size = Read<int>(loadoutProp, profile::PropElementSize);
                    memcpy(loadout, value, (size_t)std::min(size, (int)sizeof(FFortAthenaLoadoutShadow)));
                    character = GetObjField(loadout, "/Script/FortniteGame.FortAthenaLoadout.Character");
                    pickaxe   = GetObjField(loadout, "/Script/FortniteGame.FortAthenaLoadout.Pickaxe");
                    LOG("[Locker] loadout resolved through MCP");
                }
            }
        }
    }
    LOG("[Locker] character=%s pickaxe=%s", ObjName(character).c_str(), ObjName(pickaxe).c_str());
    session.locker_pickaxe = pickaxe;

    // Hero definition → hero type on the player state.
    void* state = GetObjField(session.controller, "/Script/Engine.Controller.PlayerState");
    void* heroTypeProp = GetProp("/Script/FortniteGame.FortPlayerState.HeroType");
    void* heroType = nullptr;
    if (character) {
        void* heroDef = GetObjField(character, "/Script/FortniteGame.AthenaCharacterItemDefinition.HeroDefinition");
        if (heroDef) {
            // [F72] skip the FortHero::GetHeroTypeBP call when heroDef is
            // already a FortHeroType — the receiver check rejected it every
            // run and the log line looked like a failure (it never was).
            std::string hdCls = ObjName(Read<void*>(heroDef, profile::ObjectClass));
            if (hdCls != "FortHeroType") {
                Call ght("/Script/FortniteGame.FortHero.GetHeroTypeBP");
                if (ght.ok() && ght.run(heroDef)) heroType = ght.result<void*>();
            }
            // [F58] on 21.30 the HeroDefinition object IS a FortHeroType
            // (the old GetHeroTypeBP call was rejected: receiver=FortHeroType
            // owner=FortHero) — use it directly.
            if (!heroType) heroType = heroDef;
        }
    }
    if (heroType && state && heroTypeProp && Read<int>(heroTypeProp, profile::PropElementSize) == 8) {
        Write<void*>(GetField(state, heroTypeProp), 0, heroType);
        Call r1("/Script/FortniteGame.FortPlayerState.OnRep_HeroType");
        if (r1.ok()) r1.run(state);
    }

    // Mirror the loadout onto the pawn (CosmeticLoadout @0x2A08).
    void* pawnLoadoutProp = GetProp("/Script/FortniteGame.FortPlayerPawn.CosmeticLoadout");
    if (pawnLoadoutProp && Read<int>(pawnLoadoutProp, profile::PropElementSize) == Read<int>(loadoutProp, profile::PropElementSize)) {
        int size = Read<int>(pawnLoadoutProp, profile::PropElementSize);
        memcpy(GetField(session.pawn, pawnLoadoutProp), loadout, (size_t)size);
        LOG("[Locker] pawn CosmeticLoadout mirrored (%d bytes)", size);
    }

    // Character parts: menu hero first, then hero specializations, then backpack.
    std::vector<void*> equipped_parts;
    void* menu = info ? Read<void*>(info, profile::RegisteredPlayerInfo_HeroInstance) : nullptr;
    if (menu && !IsBadPtr(menu)) {
        Array parts = Read<Array>(menu, profile::FortHero_CharacterParts);
        if (parts.Data && parts.Count > 0 && parts.Count <= 16)
            for (int i = 0; i < parts.Count; ++i) equipped_parts.push_back(Read<void*>(parts.Data, (size_t)i * 8));
    }
    void* heroDef = character ? GetObjField(character, "/Script/FortniteGame.AthenaCharacterItemDefinition.HeroDefinition") : nullptr;
    if (heroDef && !IsBadPtr(heroDef)) {
        for (void* spec : SoftAssetArray(heroDef, "/Script/FortniteGame.FortHeroType.Specializations"))
            for (void* part : SoftAssetArray(spec, "/Script/FortniteGame.FortHeroSpecialization.CharacterParts"))
                equipped_parts.push_back(part);
    }
    if (backpack && !IsBadPtr(backpack)) {
        void* bp = GetProp("/Script/FortniteGame.AthenaCharacterPartItemDefinition.CharacterParts");
        if (bp) {
            Array back_parts = Read<Array>(GetField(backpack, bp));
            if (back_parts.Data && back_parts.Count >= 0 && back_parts.Count <= 16)
                for (int i = 0; i < back_parts.Count; ++i) equipped_parts.push_back(Read<void*>(back_parts.Data, (size_t)i * 8));
        }
    }
    for (void* part : equipped_parts) {
        uint8_t type = 255;
        void* partCls = FindObj("/Script/FortniteGame.CustomCharacterPart");
        if (!part || !partCls || !IsA(part, partCls)) continue;
        if (!GetFieldValue<uint8_t>(part, "/Script/FortniteGame.CustomCharacterPart.CharacterPartType", type) || type > 6) continue;
        Call choose("/Script/FortniteGame.FortPlayerPawn.ServerChoosePart");
        if (choose.ok() && choose.set("Part", type) && choose.set("ChosenCharacterPart", part)) {
            choose.run(session.pawn);
            if (std::find(session.character_parts.begin(), session.character_parts.end(), part) == session.character_parts.end())
                session.character_parts.push_back(part);
        }
    }
    if (!equipped_parts.empty() && state) {
        Array parts{session.character_parts.data(), (int32)session.character_parts.size(), (int32)session.character_parts.size()};
        Call apply("/Script/FortniteGame.FortKismetLibrary.ApplyCharacterCosmetics");
        if (apply.ok() && apply.set("WorldContextObject", session.world) && apply.set("CharacterParts", parts) && apply.set("PlayerState", state))
            apply.run(FindObj("/Script/FortniteGame.Default__FortKismetLibrary"));
    }
    if (state) {
        Call r3("/Script/FortniteGame.FortPlayerState.OnRep_CharacterData"); if (r3.ok()) r3.run(state);
        void* klib = FindObj("/Script/FortniteGame.Default__FortKismetLibrary");
        if (klib) {
            Call refresh("/Script/FortniteGame.FortKismetLibrary.UpdatePlayerCustomCharacterPartsVisualization");
            if (refresh.ok() && refresh.set("PlayerState", state)) refresh.run(klib);
        }
    }
}

void Athena::LoadProfileCosmetics() {
    if (!session.pawn || !session.controller) return;
    LOG("[Profile] Loading cosmetics...");
    SyncLockerLoadout();
    if (!session.character_parts.empty()) { ShowSkin(); return; }
    LoadDefaultCosmetics();
}

void Athena::LoadDefaultCosmetics() {
    if (!session.pawn || !session.controller) return;
    LOG("[Cosmetics] Loading hardcoded defaults...");
    const char* partPaths[] = {
        "/Game/Athena/Heroes/Meshes/Heads/F_Med_Head1_ATH.F_Med_Head1_ATH",
        "/Game/Athena/Heroes/Meshes/Bodies/CP_001_Athena_Body.CP_001_Athena_Body",
    };
    void* loaded[2]{};
    for (int i = 0; i < 2; ++i) {
        void* asset = LoadAsset(partPaths[i], "/Script/FortniteGame.CustomCharacterPart");
        if (!asset) continue;
        void* partCls = FindObj("/Script/FortniteGame.CustomCharacterPart");
        if (!partCls || !IsA(asset, partCls)) continue;
        loaded[i] = asset;
        Call choose("/Script/FortniteGame.FortPlayerPawn.ServerChoosePart");
        if (choose.ok() && choose.set("Part", (uint8_t)i) && choose.set("ChosenCharacterPart", asset)) {
            choose.run(session.pawn);
            if (std::find(session.character_parts.begin(), session.character_parts.end(), asset) == session.character_parts.end())
                session.character_parts.push_back(asset);
        }
    }
    void* state = GetObjField(session.controller, "/Script/Engine.Controller.PlayerState");
    if (!state || IsBadPtr(state)) return;
    if (loaded[0] && loaded[1]) {
        Array partsArr{loaded, 2, 2};
        Call apply("/Script/FortniteGame.FortKismetLibrary.ApplyCharacterCosmetics");
        if (apply.ok() && apply.set("WorldContextObject", session.world) && apply.set("CharacterParts", partsArr) && apply.set("PlayerState", state)) {
            void* klib = FindObj("/Script/FortniteGame.Default__FortKismetLibrary");
            if (klib) apply.run(klib);
        }
        void* hero = LoadAsset("/Game/Athena/Heroes/HID_001_Athena_Commando_F.HID_001_Athena_Commando_F", "/Script/FortniteGame.FortHeroType");
        if (!hero) hero = LoadAsset("/Game/Athena/Heroes/HID_001_Athena_Commando_M.HID_001_Athena_Commando_M", "/Script/FortniteGame.FortHeroType");
        void* heroTypeProp = GetProp("/Script/FortniteGame.FortPlayerState.HeroType");
        if (hero && heroTypeProp) Write<void*>(GetField(state, heroTypeProp), 0, hero);
    }
    Call r1("/Script/FortniteGame.FortPlayerState.OnRep_HeroType"); if (r1.ok()) r1.run(state);
    Call r3("/Script/FortniteGame.FortPlayerState.OnRep_CharacterData"); if (r3.ok()) r3.run(state);
    void* klib = FindObj("/Script/FortniteGame.Default__FortKismetLibrary");
    if (klib) {
        Call upd("/Script/FortniteGame.FortKismetLibrary.UpdatePlayerCustomCharacterPartsVisualization");
        if (upd.ok() && upd.set("PlayerState", state)) upd.run(klib);
    }
}
void Athena::ShowSkin() { LoadDefaultCosmetics(); }

void Athena::ForcePlayerVisible() {
    if (!session.pawn || IsBadPtr(session.pawn)) return;
    // [F73] while riding the bus the pawn is deliberately hidden — do not
    // fight the boarding state (retry happens after the jump).
    if (br.started && br.phase == 3 && !session.aircraft_jumped) return;
    LOG("[Visibility] Forcing player body visible...");
    SetBool(session.pawn, "/Script/FortniteGame.FortPlayerPawn.bIsSkydiving", false);
    SetBool(session.pawn, "/Script/FortniteGame.FortPlayerPawn.bIsParachuteOpen", false);
    SetBool(session.pawn, "/Script/FortniteGame.FortPlayerPawn.bIsParachuteAvailable", true);
    Call sd("/Script/FortniteGame.FortPlayerPawn.OnRep_IsSkydiving"); if (sd.ok()) sd.run(session.pawn);
    Call po("/Script/FortniteGame.FortPlayerPawn.OnRep_IsParachuteOpen"); if (po.ok()) po.run(session.pawn);
    Call body("/Script/FortniteGame.FortPlayerPawnAthena.SetCharacterBodyVisibilityForPossession");
    if (body.ok()) { bool bShow = true; if (body.set("bVisible", bShow)) { body.run(session.pawn); bool ret = body.result<bool>(); LOG("[Visibility] SetCharacterBodyVisibilityForPossession -> %d", (int)ret); } }
    Call hid("/Script/Engine.Actor.SetActorHiddenInGame");
    if (hid.ok()) { bool bHidden = false; if (hid.set("bNewHidden", bHidden)) hid.run(session.pawn); }
    SetBool(session.pawn, "/Script/Engine.Actor.bHidden", false);
    LoadProfileCosmetics();
    session.visibility_forced = true;
}

// ============================================================================
// PLAYER INITIALIZATION / FINALIZATION
// ============================================================================
static void SetPawnHealth(float value) {
    if (!session.pawn) return;
    // [F23] SetMaxHealth/SetHealth take a single nameless float — positional
    // binding handles them.
    for (const char* method : {"/Script/FortniteGame.FortPawn.SetMaxHealth", "/Script/FortniteGame.FortPawn.SetHealth"}) {
        Call c(method);
        if (c.ok() && c.set_at(0, value)) c.run(session.pawn);
    }
}

bool Athena::TryInitializePlayer(void* world, void* mode, void* controller) {
    // [F15] Standalone gate.
    if (!WorldIsStandalone(world)) { LOG("[Init] World is not standalone — stopping setup"); session.attempted = true; return false; }
    void* pawnCls = LoadClass("/Game/Athena/PlayerPawn_Athena.PlayerPawn_Athena_C", "/Script/Engine.Pawn");
    if (!pawnCls) { LOG("[Init] PlayerPawn_Athena_C missing"); return false; }
    void* existing = GetPawn(controller);
    if (existing && !IsBadPtr(existing) && IsA(existing, pawnCls)) { session.pawn = existing; LOG("[Init] Using existing pawn"); return true; }
    Transform t{};
    t.Location[0] = 0.0; t.Location[1] = 0.0; t.Location[2] = 500.0;
    bool haveSpawn = false;
    Call choose("/Script/Engine.GameModeBase.ChoosePlayerStart");
    if (choose.ok() && choose.set("Player", controller) && choose.run(mode)) {
        void* start = choose.result<void*>();
        if (start && !IsBadPtr(start)) {
            Call loc("/Script/Engine.Actor.K2_GetActorLocation");
            if (loc.ok() && loc.run(start)) {
                Vector v = loc.result<Vector>();
                if (std::isfinite(v.X) && std::isfinite(v.Y) && std::isfinite(v.Z)) {
                    t.Location[0] = v.X; t.Location[1] = v.Y; t.Location[2] = v.Z + 100.0;
                    haveSpawn = true;
                    LOG("[Init] PlayerStart at (%.0f,%.0f,%.0f)", v.X, v.Y, v.Z);
                }
            }
        }
    }
    if (!haveSpawn) { t.Location[0] = 0.0; t.Location[1] = 0.0; t.Location[2] = 3100.0; }
    session.pawn = SpawnActor(world, pawnCls, controller, t);
    if (!session.pawn) { LOG("[Init] Spawn failed"); return false; }
    LOG("[Init] Spawned: %p", session.pawn);
    SetBool(session.pawn, "/Script/FortniteGame.FortPlayerPawn.bIsSkydiving", false);
    SetBool(session.pawn, "/Script/FortniteGame.FortPlayerPawn.bIsParachuteOpen", false);
    Possess(controller, session.pawn);
    if (GetPawn(controller) != session.pawn) { LOG("[Init] Possession did not stick"); return false; }
    LOG("[Init] Possessed OK");
    SetupCheats();
    // [F23] Full health on spawn.
    SetPawnHealth(100.0f);
    Call sd("/Script/FortniteGame.FortPlayerPawn.OnRep_IsSkydiving"); if (sd.ok()) sd.run(session.pawn);
    Call po("/Script/FortniteGame.FortPlayerPawn.OnRep_IsParachuteOpen"); if (po.ok()) po.run(session.pawn);
    Call restart("/Script/Engine.PlayerController.ClientRestart");
    if (restart.ok() && restart.set("NewPawn", session.pawn)) restart.run(controller);
    ForcePlayerVisible();
    return true;
}

void Athena::SetupWorld(void* world) {
    session.world = world;
    session.controller = GetPC(world, 0);
    if (!session.controller) { LOG("SetupWorld: no controller"); return; }
    void* baseCtrl = FindObj("/Script/FortniteGame.FortPlayerController");
    if (!baseCtrl || !IsA(session.controller, baseCtrl)) { LOG("SetupWorld: not FortPlayerController"); return; }
    void* modeProp  = GetProp("/Script/Engine.World.AuthorityGameMode");
    void* stateProp = GetProp("/Script/Engine.World.GameState");
    session.mode  = modeProp  ? Read<void*>(GetField(world, modeProp))  : nullptr;
    session.state = stateProp ? Read<void*>(GetField(world, stateProp)) : nullptr;
    if (!session.mode || !session.state) { LOG("SetupWorld: mode/state missing"); return; }
    LOG("SetupWorld: PC=%s GM=%s GS=%s", ObjName(session.controller).c_str(), ObjName(session.mode).c_str(), ObjName(session.state).c_str());
    if (!TryInitializePlayer(world, session.mode, session.controller)) { LOG("SetupWorld: prerequisites not ready"); return; }
    LOG("SetupWorld: GrantAllAbilitySets"); GrantAllAbilitySets();
    LOG("SetupWorld: SetupTacticalSprint"); SetupTacticalSprint();
    LOG("SetupWorld: EnableMovementFeatures"); EnableMovementFeatures();
    LOG("SetupWorld: ShowSkin"); ShowSkin();
    LOG("SetupWorld: DestroyLods"); DestroyLods();
    LOG("SetupWorld: InitializeInventory"); InitializeInventory();
    LOG("SetupWorld: SpawnFloorLoot"); SpawnFloorLoot();
    LOG("SetupWorld: ApplySprintSpeed"); ApplySprintSpeed();
    // [F73] Zone/bus removed from the init pass — the Erbium BR lifecycle
    // (BRTick) owns warmup -> aircraft -> safe zones from finalize+2s. The
    // old SpawnSafeZoneIndicator here spawned an EMPTY indicator (no phases)
    // and the old SpawnBattleBus faked phase 3 + teleported to (0,0,50000):
    // both fought the new machine.
    LOG("SetupWorld: SpawnBuildPreviews"); SpawnBuildPreviews();
    LOG("SetupWorld: DropLoadingScreen"); DropLoadingScreen();
    LOG("SetupWorld: RemoveNetDebugUI"); RemoveNetDebugUI();
    // [F23] Free building + direct readiness bit (standalone-safe fallbacks).
    SetBool(session.controller, "/Script/FortniteGame.FortPlayerController.bBuildFree", true);
    { uint8_t* ready = (uint8_t*)session.controller + profile::FortPlayerController_ReadyByte;
      *ready |= (uint8_t)(1u << profile::FortPlayerController_ReadyBit); }
    RefreshHUD();
    session.finalize_at = Monotonic() + 2.0;
    session.attempted   = true;
    session.initialized = true;
    LOG("SetupWorld: initial pass done");
}

void Athena::FinalizePlayer() {
    if (session.finalized) return;
    session.finalized = true;
    LOG("=== FinalizePlayer ===");
    void* pc = GetPC(session.world, 0);
    if (pc != session.controller) { LOG("Finalize: controller changed"); return; }
    void* pawnCls = FindObj("/Script/FortniteGame.FortPlayerPawnAthena");
    void* pawn = GetPawn(pc);
    if (!pawn || !pawnCls || !IsA(pawn, pawnCls)) { LOG("Finalize: pawn is not FortPlayerPawnAthena"); return; }
    session.pawn = pawn;
    // Readiness can change possession — refresh cached pawn-dependent state.
    Call ps("/Script/Engine.Pawn.OnRep_PlayerState"); if (ps.ok()) ps.run(pawn);
    if (!session.abilities_granted) GrantAllAbilitySets();
    if (!session.tactical_sprint_setup) SetupTacticalSprint();
    if (!session.movement_enabled) EnableMovementFeatures();
    if (!session.quickbars_initialized) {
        InitializeQuickBarsNative();
        if (!session.quickbars_initialized) BuildQuickBarsManually();
        SetQuickBarReady();
        ForceUpdateQuickBars();
        ActivateQuickBarSlot(0, 0);
    }
    if (!session.ready_called) {
        Call srt("/Script/FortniteGame.FortPlayerController.ServerReadyToStartMatch"); if (srt.ok()) srt.run(pc);
        Call cps("/Script/FortniteGame.FortPlayerControllerZone.ClientOnPawnSpawned"); if (cps.ok()) cps.run(pc);
        session.ready_called = true;
    }
    Call restart("/Script/Engine.PlayerController.ClientRestart");
    if (restart.ok() && restart.set("NewPawn", pawn)) restart.run(pc);
    Call cam("/Script/Engine.PlayerController.SetViewTargetWithBlend");
    if (cam.ok() && cam.set("NewViewTarget", pawn)) cam.run(pc);
    session.visibility_forced = false;
    ForcePlayerVisible();
    SetPawnHealth(100.0f);
    RefreshHUD();
    session.finalized_at = Monotonic();
    ForceMapsVisible();       // [F33] minimap + full map
    ForceLevelStreaming();    // [F28] first stream-in pass
    // [F69] HUD diagnosis — the whole quickbar/interact widget chain starts at
    // the HUD actor. If its class is not a Fort HUD the widget layers never
    // build (Erbium logs the same pair after readiness).
    {
        Call hud("/Script/Engine.PlayerController.GetHUD");
        if (hud.ok() && hud.run(session.controller)) {
            void* h = hud.result<void*>();
            LOG("[HUD] hud=%p class=%s", h,
                (h && !IsBadPtr(h)) ? ObjName(Read<void*>(h, profile::ObjectClass)).c_str() : "null");
        }
    }
    LOG("Finalize: complete");
}

void Athena::Tick() {
    if (!session.world) return;
    double now = Monotonic();
    if (!session.attempted) {
        if (now - session.retry_at < 1.0) return;
        session.retry_at = now;
        SetupWorld(session.world);
        return;
    }
    if (session.finalize_at > 0 && !session.finalized && now >= session.finalize_at) { FinalizePlayer(); return; }
    static double lastVisibility = 0;
    static double lastSpeedReapply = 0;
    static double lastAbilityCheck = 0;
    static double lastSprintCheck = 0;
    static double lastQBCheck = 0;
    static double lastMovementCheck = 0;
    static double lastStreamForce = 0;   // [F28]
    static double lastVehicleRetry = 0;  // [F39]
    static double lastLootRetry = 0;     // [F45]
    static double lastBodyCheck = 0;     // [F41]
    static int qbRetries = 0;

    // [F28] Keep forcing streaming levels while the match is young — the
    // engine re-evaluates streaming on its own schedule and may re-hide.
    if (session.finalized && now - lastStreamForce > 8.0 && now - session.finalized_at < 150.0) {
        lastStreamForce = now;
        ForceLevelStreaming();
    }
    // [F29/F39] Vehicle passes — retried every 5s: streaming levels load
    // async, so spawner actors and vehicle BPs surface over ~2 minutes.
    if (session.finalized && now - lastVehicleRetry > 5.0 && now - session.finalized_at > 12.0) {
        lastVehicleRetry = now;
        if (session.vehicles_attempts < 30) VehicleRetry();
    }
    // [F30/F38/F54] Wildlife pass — the Irwin animal BPs stream in late, so
    // retry every 20s (max 10) instead of the one-shot that fired too early.
    // [F75-c] once the near ring lands, also scatter animals across the map.
    if (session.finalized && !session.wildlife_spawned && session.wildlife_attempts < 10 &&
        now - session.finalized_at > 16.0) {
        static double lastWild = 0;
        if (now - lastWild > 20.0) {
            lastWild = now;
            ++session.wildlife_attempts;
            if (SpawnWildlifeNear(8) > 0) {
                session.wildlife_spawned = true;
                SpawnWildlifeScattered(12);
            }
        }
    }
    // [F33] Retry the map flags a couple of times — the manager may spawn late.
    if (session.finalized && !session.maps_forced && now - session.finalized_at > 20.0) ForceMapsVisible();
    // [F45] Floor loot — retry every 20s until containers surface (max 8).
    if (session.finalized && session.loot_found_total <= 0 && session.loot_attempts < 8 && now - lastLootRetry > 20.0) {
        lastLootRetry = now;
        SpawnFloorLootRetry();
    }
    // [F73] ERBIUM BR LIFECYCLE — the full BattleRoyaleGamePhaseLogic machine
    // (warmup countdown -> aircraft/bus -> safe zones -> storm) replaces the
    // old F35 one-shot zone poke and the F36 static counter. Runs every tick.
    if (session.finalized) BRTick();
    // [F37] Harvest — health-delta watch grants wood/stone/metal on hits.
    if (session.finalized && session.harvest_enabled) HarvestTick();
    // [F40] Doors — keep player-opened doors open, unstick jammed ones.
    if (session.finalized && session.doors_hold && now - session.last_door_audit > 2.0) {
        session.last_door_audit = now;
        DoorAudit();
    }
    // [F41] Body + locomotion animations.
    if (session.finalized && !session.body_fixed && now - lastBodyCheck > 5.0) {
        lastBodyCheck = now;
        FixBodyAndAnimations();
    }
    // [F42] Climb boost (21.30 has no mantle — local substitute).
    if (session.finalized) ClimbTick();
    if (session.finalized && now - lastVisibility > 8.0) { lastVisibility = now; if (!session.visibility_forced) ForcePlayerVisible(); }
    if (session.finalized && session.pawn && now - lastSpeedReapply > 2.0) { lastSpeedReapply = now; ApplySprintSpeed(); }
    if (session.finalized && session.pawn && now - lastAbilityCheck > 10.0) {
        lastAbilityCheck = now;
        if (!session.abilities_granted) { GrantAllAbilitySets(); RefreshHUD(); }
    }
    if (session.finalized && session.controller && now - lastSprintCheck > 5.0) {
        lastSprintCheck = now;
        if (!session.tactical_sprint_setup || !session.tactical_sprint) { session.tactical_sprint_setup = false; SetupTacticalSprint(); }
    }
    if (session.finalized && session.controller && now - lastQBCheck > 6.0) {
        lastQBCheck = now;
        if (!session.quickbars_initialized && qbRetries++ < 5) {
            LOG("Tick: QuickBars retry #%d", qbRetries);
            InitializeQuickBarsNative();
            if (!session.quickbars_initialized) BuildQuickBarsManually();
            if (session.quickbars_initialized) SetQuickBarReady();
        }
    }
    if (session.finalized && session.pawn && now - lastMovementCheck > 8.0) {
        lastMovementCheck = now;
        if (!session.movement_enabled) EnableMovementFeatures();
    }
    // [F52] Slide + enter-vehicle abilities (name-scan, retried until resident).
    static double lastWorldAbilities = 0;
    if (session.finalized && !session.world_abilities_granted && now - lastWorldAbilities > 8.0) {
        lastWorldAbilities = now;
        GrantWorldAbilities();
    }
    // [F46/F54] World-content discovery (chests/floorloot/spawners/sharks/
    // saplings). First pass at 22s, then re-scan every 60s while no containers
    // surfaced (streaming levels land progressively) — max 5 re-runs.
    if (session.finalized && !session.map_scan_done && now - session.finalized_at > 22.0) {
        session.map_scan_done = true;
        ScanMapContent();
    } else if (session.finalized && session.map_scan_done && session.containers.empty() &&
               session.scan_retries < 5) {
        static double lastRescan = 0;
        if (now - lastRescan > 60.0) { lastRescan = now; ++session.scan_retries; ScanMapContent(); }
    }
    // [F47] Interaction candidate + auto-interact (chest/vehicle proximity).
    if (session.finalized) InteractTick();
    // [F49] Edit tool watcher (EditingPlayer keep-alive + confirm edge).
    if (session.finalized) EditAssist();
    // [F50/F57] Quickbar widget poke — periodic for the first ~3 minutes (the
    // HUD widgets construct after our manual slot fill; one early poke was
    // not enough once the widgets appeared later).
    if (session.finalized && session.qb_pokes < 8 && now - session.finalized_at > 18.0) {
        static double lastQBW = 0;
        if (now - lastQBW > 20.0) { lastQBW = now; ++session.qb_pokes; PokeQuickbarWidgets(); }
    }
    // [F69] The mobile HUD builds the quickbar/interact buttons on
    // ClientOnPawnSpawned — re-kick after each game-phase change (warmup ->
    // aircraft -> safezones). The first call fired before the phase machine
    // even started, which is one reason the QuickBars stayed dead.
    if (session.finalized && session.state && session.controller) {
        void* gp = GetProp("/Script/FortniteGame.FortGameStateAthena.GamePhase");
        if (gp && Read<int>(gp, profile::PropElementSize) == 1) {
            uint8_t ph = Read<uint8_t>(GetField(session.state, gp), 0);
            if ((int)ph > session.hud_phase_kick) {
                session.hud_phase_kick = (int)ph;
                Call cps("/Script/FortniteGame.FortPlayerControllerZone.ClientOnPawnSpawned");
                if (cps.ok()) cps.run(session.controller);
                ForceUpdateQuickBars();
                session.qb_pokes = 0;   // let the widget poke loop run again
                LOG("[HUD] phase %u — ClientOnPawnSpawned re-kick", (unsigned)ph);
            }
        }
    }
    // [F52/F63] Ambient NPCs — first batch at 35s, then keep at least 2 alive.
    if (session.finalized && !session.npcs_spawned && now - session.finalized_at > 35.0) {
        session.npcs_spawned = true;
        SpawnNPCs(4);
    }
    if (session.finalized && session.npcs_spawned && !session.npcs.empty()) {
        static double lastNpcCheck = 0;
        if (now - lastNpcCheck > 60.0) {
            lastNpcCheck = now;
            session.npcs.erase(std::remove_if(session.npcs.begin(), session.npcs.end(),
                [](void* p) { return !p || IsBadPtr(p); }), session.npcs.end());
            if (session.npcs.size() < 2) SpawnNPCs(1);
        }
    }
    // [F62] Reality-tree saplings (Ch3 S3) — planted once the map settles.
    if (session.finalized && !session.reality_done && now - session.finalized_at > 45.0) {
        session.reality_done = true;
        SpawnRealitySaplings(2);
    }
    if (session.finalized) PollTouchBuild();
}

void Athena::DestroyLods() {
    void* cm = GetObjField(session.controller, "/Script/Engine.PlayerController.CheatManager");
    if (!cm) { SetupCheats(); cm = GetObjField(session.controller, "/Script/Engine.PlayerController.CheatManager"); }
    if (!cm) return;
    void* cls = FindObj("/Script/FortniteGame.FortHLODSMActor");
    if (!cls) return;
    Call c("/Script/Engine.CheatManager.DestroyAll");
    if (c.ok() && c.set("aClass", cls)) c.run(cm);
    LOG("DestroyLods: done");
}

void Athena::DropLoadingScreen() {
    if (!session.state || !session.controller) return;
    void* gp = GetProp("/Script/FortniteGame.FortGameStateAthena.GamePhase");
    if (gp && Read<int>(gp, profile::PropElementSize) == 1) {
        uint8_t old = Read<uint8_t>(GetField(session.state, gp));
        if (old < 2) {
            uint8_t phase = 2;
            Write<uint8_t>(GetField(session.state, gp), 0, phase);
            Call n("/Script/FortniteGame.FortGameStateAthena.OnRep_GamePhase");
            if (n.ok() && n.set("OldGamePhase", old)) n.run(session.state);
        }
    }
    SetBool(session.controller, "/Script/FortniteGame.FortPlayerController.bHasServerFinishedLoading", true);
    SetBool(session.controller, "/Script/FortniteGame.FortPlayerController.bHasClientFinishedLoading", true);
    void* playlist = FindObj("/Game/Athena/Playlists/Playlist_DefaultSolo.Playlist_DefaultSolo");
    if (!playlist) playlist = FindObj("/Game/Athena/Playlists/Playground/Playlist_Playground.Playlist_Playground");
    if (playlist) {
        void* infoProp = GetProp("/Script/FortniteGame.FortGameStateAthena.CurrentPlaylistInfo");
        void* bp = GetProp("/Script/FortniteGame.PlaylistPropertyArray.BasePlaylist");
        void* op = GetProp("/Script/FortniteGame.PlaylistPropertyArray.OverridePlaylist");
        if (infoProp && bp) Write<void*>(GetField(GetField(session.state, infoProp), bp), 0, playlist);
        if (infoProp && op) Write<void*>(GetField(GetField(session.state, infoProp), op), 0, playlist);
        Call a("/Script/FortniteGame.FortGameStateAthena.OnRep_CurrentPlaylistInfo"); if (a.ok()) a.run(session.state);
    }
    if (!session.ready_called) {
        Call srt("/Script/FortniteGame.FortPlayerController.ServerReadyToStartMatch"); if (srt.ok()) srt.run(session.controller);
        Call cps("/Script/FortniteGame.FortPlayerControllerZone.ClientOnPawnSpawned"); if (cps.ok()) cps.run(session.controller);
        session.ready_called = true;
    }
    LOG("Loading screen dropped");
}

void Athena::InventoryUpdate() {
    if (session.inventory) { Call a("/Script/FortniteGame.FortInventory.HandleInventoryLocalUpdate"); if (a.ok()) a.run(session.inventory); }
    { Call b("/Script/FortniteGame.FortPlayerController.HandleWorldInventoryLocalUpdate"); if (b.ok()) b.run(session.controller); }
    { Call f("/Script/FortniteGame.FortPlayerController.ClientForceWorldInventoryUpdate"); if (f.ok()) f.run(session.controller); }
    // [F12] FortQuickBars OnRep_* and PC.OnRep_QuickBar do not exist in 21.30;
    // the ClientForceUpdateQuickbar calls below are the supported refresh path.
    for (uint8_t bar : {uint8_t(0), uint8_t(1)}) {
        Call c("/Script/FortniteGame.FortPlayerController.ClientForceUpdateQuickbar");
        if (c.ok() && c.set("QuickbarToRefresh", bar)) c.run(session.controller);
    }
}

// ============================================================================
// INVENTORY — [F17/F18/F19] native GUID + reconciliation + stacking + 2 bars
// ============================================================================
static void* ItemDefinitionOf(void* item) {
    Call c("/Script/FortniteGame.FortItem.GetItemDefinitionBP");
    return c.ok() && item && c.run(item) ? c.result<void*>() : nullptr;
}
static Guid ItemGuidOf(void* item) {
    Call c("/Script/FortniteGame.FortItem.GetItemGuid");
    return c.ok() && item && c.run(item) ? c.result<Guid>() : Guid{};
}

// [F17] Era-core give(): create the temporary instance, assign a native GUID
// BEFORE the replicated-entry copy, insert both arrays, then verify the
// surviving instance by GUID after a local inventory refresh.
bool Athena::GiveItem(void* def, int count, int slot, uint8_t bar, bool require_new) {
    if (!def || !session.inventory || count < 1 || count > 9999) return false;
    if (!IsA(def, FindObj("/Script/FortniteGame.FortItemDefinition"))) return false;
    void* invProp   = GetProp("/Script/FortniteGame.FortInventory.Inventory");
    void* instProp  = GetProp("/Script/FortniteGame.FortItemList.ItemInstances");
    void* entryProp = GetProp("/Script/FortniteGame.FortItemList.ReplicatedEntries");
    void* itemEntryProp = GetProp("/Script/FortniteGame.FortWorldItem.ItemEntry");
    if (!invProp || !instProp || !entryProp || !itemEntryProp) return false;
    void* list = GetField(session.inventory, invProp);
    void* instArr  = GetField(list, instProp);
    void* entryArr = GetField(list, entryProp);
    Array current = Read<Array>(instArr);
    Array replicated = Read<Array>(entryArr);
    if (current.Count < 0 || current.Count > 512 || current.Count > current.Max ||
        replicated.Count < 0 || replicated.Count > 512 || replicated.Count > replicated.Max) return false;

    // Duplicate protection — callers can give a different item instead.
    for (int i = 0; i < current.Count; ++i) {
        void* existing = Read<void*>(current.Data, (size_t)i * 8);
        if (existing && ItemDefinitionOf(existing) == def) {
            if (require_new) return false;
            if (slot >= 0 && slot < 6) (bar == 0 ? session.slots : session.secondary_slots)[(size_t)slot] = existing;
            LOG("Already present: %s", ObjName(def).c_str());
            return true;
        }
    }

    // CreateTemporaryItemInstanceBP(Count, Level) — single call, capture result.
    void* inst = nullptr;
    {
        Call c("/Script/FortniteGame.FortItemDefinition.CreateTemporaryItemInstanceBP");
        int lvl = 1;
        if (c.ok() && c.set("Count", count) && c.set("Level", lvl) && c.run(def)) inst = c.result<void*>();
    }
    if (!inst || !IsA(inst, FindObj("/Script/FortniteGame.FortWorldItem"))) return false;
    { Call c("/Script/FortniteGame.FortItem.SetOwningControllerForTemporaryItem");
      if (!c.ok() || !c.set("InController", session.controller) || !c.run(inst)) return false; }

    void* entrySrc = GetField(inst, itemEntryProp);
    if (!entrySrc) return false;
    void* guidProp = GetProp("/Script/FortniteGame.FortItemEntry.ItemGuid");
    if (!guidProp || Read<int>(guidProp, profile::PropElementSize) != sizeof(Guid)) return false;

    // Assign the native GUID before the deep copy so the replicated entry
    // and the instance share identity (Era core ordering).
    Guid guid = MakeGuid();
    if (!(guid.A | guid.B | guid.C | guid.D)) return false;
    Write<Guid>(GetField(entrySrc, guidProp), 0, guid);
    Guid reported = ItemGuidOf(inst);
    if (memcmp(&reported, &guid, sizeof(guid)) != 0) {
        LOG("Temporary item GUID accessor mismatch; insertion refused");
        return false;
    }

    Write<float>(entrySrc, profile::ItemEntry_Durability, 1.0f);
    if (IsA(def, FindObj("/Script/FortniteGame.FortWeaponItemDefinition"))) {
        Write<int32>(entrySrc, profile::ItemEntry_LoadedAmmo, 30);
        if (GetBool(def, "/Script/FortniteGame.FortWeaponItemDefinition.bUsesPhantomReserveAmmo"))
            Write<int32>(entrySrc, profile::ItemEntry_PhantomAmmo, 300);
    }

    auto add = Fn<int(*)(void*,void*,const void*)>(profile::GenericArrayAdd);
    int idx = add(instArr, instProp, &inst);
    if (idx < 0) return false;
    int entryIdx = add(entryArr, entryProp, entrySrc);
    if (entryIdx < 0) { LOG("Entry copy failed after instance insertion; stop granting items"); return false; }

    InventoryUpdate();

    // Reconciliation: the refresh may rebuild cached state; resolve the
    // canonical instance by GUID and refuse silent loss (Era core).
    void* canonical = nullptr;
    Array after = Read<Array>(instArr);
    if (after.Count >= 0 && after.Count <= 512 && after.Data)
        for (int i = 0; i < after.Count; ++i) {
            void* candidate = Read<void*>(after.Data, (size_t)i * 8);
            if (candidate && !IsBadPtr(candidate)) {
                Guid id = ItemGuidOf(candidate);
                if (!memcmp(&id, &guid, sizeof(guid))) { canonical = candidate; break; }
            }
        }
    if (!canonical || ItemDefinitionOf(canonical) != def) {
        LOG("Inventory reconciliation lost %s GUID=%08x:%08x:%08x:%08x",
            ObjName(def).c_str(), guid.A, guid.B, guid.C, guid.D);
        return false;
    }
    inst = canonical;
    LOG("Inventory identity verified %s GUID=%08x:%08x:%08x:%08x",
        ObjName(def).c_str(), guid.A, guid.B, guid.C, guid.D);

    if (slot >= 0 && slot < 6) {
        (bar == 0 ? session.slots : session.secondary_slots)[(size_t)slot] = inst;
        BindSlot(inst, slot, bar);
        ForceUpdateQuickBars();   // [F32] refresh the bar on every pickup/give
    }
    LOG("Inventory inserted %s count=%d slot=%d bar=%u indices=%d/%d",
        ObjName(def).c_str(), count, slot, bar, idx, entryIdx);
    return true;
}

// Legacy direct-add kept for ammo/resources (no quickbar binding).
void Athena::AddToInventory(void* def, int count, int slot) {
    if (IsBadPtr(def) || !session.inventory) return;
    GiveItem(def, count, (slot >= 0 && slot < 6) ? slot : -1, 0, false);
}

// [F19] Two-bar starter loadout: pickaxe + weapons on Primary(0), building
// pieces on Secondary(1), ammo/resources unbound. Pickaxe comes from the
// locker when available (AthenaPickaxeItemDefinition.WeaponDefinition).
static void* ResolveWeaponDefForEquip(void* def) {
    if (!def) return nullptr;
    void* pickCls = FindObj("/Script/FortniteGame.AthenaPickaxeItemDefinition");
    if (pickCls && IsA(def, pickCls)) {
        void* weapon = GetObjField(def, "/Script/FortniteGame.AthenaPickaxeItemDefinition.WeaponDefinition");
        if (weapon) return weapon;
    }
    return def;
}

void Athena::InitializeInventory() {
    if (!session.controller) return;
    void* invProp = GetProp("/Script/FortniteGame.FortPlayerController.WorldInventory");
    if (!invProp) return;
    session.inventory = Read<void*>(GetField(session.controller, invProp));
    if (!session.inventory) {
        void* klass = FindObj("/Script/FortniteGame.FortInventory");
        Transform t{};
        session.inventory = SpawnActor(session.world, klass, session.controller, t);
        if (session.inventory) Write<void*>(GetField(session.controller, invProp), 0, session.inventory);
    }
    if (!session.inventory) { LOG("[Inv] No inventory"); return; }
    { Call so("/Script/FortniteGame.FortInventory.SetOwner"); if (so.ok()) so.run(session.inventory); }

    void* qbProp = GetProp("/Script/FortniteGame.FortPlayerController.ClientQuickBars");
    if (qbProp) {
        session.quickbars = Read<void*>(GetField(session.controller, qbProp));
        if (!session.quickbars) {
            LOG("[QB] Spawning FortQuickBarsAthena...");
            void* klass = FindObj("/Script/FortniteGame.FortQuickBarsAthena");
            if (!klass) { LOG("[QB] FortQuickBarsAthena not found, fallback"); klass = FindObj("/Script/FortniteGame.FortQuickBars"); }
            Transform t{};
            session.quickbars = SpawnActor(session.world, klass, session.controller, t);
            if (session.quickbars) {
                Write<void*>(GetField(session.controller, qbProp), 0, session.quickbars);
                LOG("[QB] Spawned: %p class=%s", session.quickbars, ObjName(Read<void*>(session.quickbars, profile::ObjectClass)).c_str());
            }
        }
        if (session.quickbars) {
            Write<void*>((char*)session.quickbars + 0x138, 0, session.controller);
            Call soQB("/Script/FortniteGame.FortQuickBars.SetOwner");
            if (soQB.ok()) soQB.run(session.quickbars);
            LOG("[QB] Running native initialize...");
            InitializeQuickBarsNative();
        }
    }

    LOG("Inventory ready: inv=%p qb=%p", session.inventory, session.quickbars);

    // Pickaxe: locker first, hardcoded fallback second.
    void* pickaxeDef = session.locker_pickaxe;
    if (!pickaxeDef) pickaxeDef = LoadAsset("/Game/Athena/Items/Weapons/WID_Harvest_Pickaxe_Athena_C_T01.WID_Harvest_Pickaxe_Athena_C_T01");
    if (!pickaxeDef) pickaxeDef = ByShortName("WID_Harvest_Pickaxe_Athena_C_T01");
    if (pickaxeDef) GiveItem(pickaxeDef, 1, 0, 0, false);

    // Starter weapon (slot 1 keeps 2..5 free for pickups).
    { void* def = ByShortName("WID_Assault_Auto_Athena_C_Ore_T03");
      if (!def) def = LoadAsset("/Game/Athena/Items/Weapons/WID_Assault_Auto_Athena_C_Ore_T03.WID_Assault_Auto_Athena_C_Ore_T03");
      if (def) GiveItem(def, 1, 1, 0, false); }

    // [F19] Build pieces live on the Secondary quickbar (bar 1).
    struct ToolSlot { const char* path; const char* shortName; int slot; };
    ToolSlot tools[] = {
        {"/Game/Items/Weapons/BuildingTools/BuildingItemData_Wall.BuildingItemData_Wall",        "BuildingItemData_Wall",     0},
        {"/Game/Items/Weapons/BuildingTools/BuildingItemData_Floor.BuildingItemData_Floor",      "BuildingItemData_Floor",    1},
        {"/Game/Items/Weapons/BuildingTools/BuildingItemData_Stair_W.BuildingItemData_Stair_W",  "BuildingItemData_Stair_W",  2},
        {"/Game/Items/Weapons/BuildingTools/BuildingItemData_RoofS.BuildingItemData_RoofS",      "BuildingItemData_RoofS",    3},
    };
    for (auto& t : tools) {
        void* def = LoadAsset(t.path);
        if (!def) def = ByShortName(t.shortName);
        if (def) GiveItem(def, 1, t.slot, 1, false);
    }
    session.edit_tool = LoadAsset("/Game/Items/Weapons/BuildingTools/EditTool.EditTool");
    if (session.edit_tool) GiveItem(session.edit_tool, 1, 4, 1, false);

    const char* ammo[] = {
        "/Game/Athena/Items/Ammo/AthenaAmmoDataBulletsLight.AthenaAmmoDataBulletsLight",
        "/Game/Athena/Items/Ammo/AthenaAmmoDataBulletsMedium.AthenaAmmoDataBulletsMedium",
        "/Game/Athena/Items/Ammo/AthenaAmmoDataBulletsHeavy.AthenaAmmoDataBulletsHeavy",
        "/Game/Athena/Items/Ammo/AthenaAmmoDataShells.AthenaAmmoDataShells",
        "/Game/Athena/Items/Ammo/AmmoDataRockets.AmmoDataRockets", nullptr};
    for (int i = 0; ammo[i]; ++i) { void* def = LoadAsset(ammo[i]); if (def) AddToInventory(def, 999, -1); }
    const char* resources[] = {
        "/Game/Items/ResourcePickups/WoodItemData.WoodItemData",
        "/Game/Items/ResourcePickups/StoneItemData.StoneItemData",
        "/Game/Items/ResourcePickups/MetalItemData.MetalItemData", nullptr};
    for (int i = 0; resources[i]; ++i) { void* def = LoadAsset(resources[i]); if (def) AddToInventory(def, 500, -1); }

    InventoryUpdate();
    ForceUpdateQuickBars();
    ActivateQuickBarSlot(0, 0);
    // [F44] Fill the quickbar slots NOW — the HUD widgets may be constructed
    // right after possession; slots present before construction render on
    // first draw (the old flow only filled them in FinalizePlayer, after the
    // HUD had already rendered an empty bar).
    BuildQuickBarsManually();
    SetQuickBarReady();
    session.inventory_ready = true;
    EquipSlot(0);
}

// [F19] EquipSlot resolves pickaxe WeaponDefinition and equips by GUID.
void Athena::EquipSlot(int slot) {
    if (slot < 0 || slot >= 6 || !session.slots[(size_t)slot]) return;
    void* item = session.slots[(size_t)slot];
    void* def = ItemDefinitionOf(item);
    if (!def) return;
    Guid guid = ItemGuidOf(item);
    if (!(guid.A | guid.B | guid.C | guid.D)) return;
    void* weaponDef = ResolveWeaponDefForEquip(def);
    if (!weaponDef) return;

    void* itemEntryProp = GetProp("/Script/FortniteGame.FortWorldItem.ItemEntry");
    Guid trackerGuid{};
    if (itemEntryProp) {
        void* entrySrc = GetField(item, itemEntryProp);
        void* trackerProp = GetProp("/Script/FortniteGame.FortItemEntry.TrackerGuid");
        if (entrySrc && trackerProp) trackerGuid = Read<Guid>(GetField(entrySrc, trackerProp));
    }
    Call eq("/Script/FortniteGame.FortPawn.EquipWeaponDefinition");
    if (!eq.ok()) return;
    bool bDisableAnim = false;
    eq.set("WeaponData", weaponDef); eq.set("ItemEntryGuid", guid);
    eq.set("TrackerGuid", trackerGuid); eq.set("bDisableEquipAnimation", bDisableAnim);
    if (eq.run(session.pawn)) {
        void* weapon = eq.result<void*>();
        LOG("Equip %s -> %p", ObjName(weaponDef).c_str(), weapon);
        session.last_equip_guid = guid;   // [F75] dedupe marker
    }
}

void Athena::OnServerExecuteInventoryItem(Guid guid) {
    if (!session.inventory || !session.pawn) return;
    // [F75-a] dedupe — the native warmup-end transition re-fires the pickaxe
    // equip at T-0 while the pickaxe is ALREADY the held weapon. Re-running
    // EquipWeaponDefinition on the held actor mid-phase-change is a crash
    // window on this build. Only the truly-held item is skipped; switching
    // back to it from another slot (weapon not in hand) still equips.
    if (void* curW = Read<void*>(session.pawn, profile::FortPawn_CurrentWeapon)) {
        if (!IsBadPtr(curW)) {
            if (void* gProp = GetProp("/Script/FortniteGame.FortWeapon.ItemEntryGuid")) {
                Guid held = Read<Guid>(GetField(curW, gProp));
                if (!memcmp(&held, &guid, sizeof(Guid))) {
                    static bool once = false;
                    if (!once) { once = true; LOG("[Equip] duplicate equip skipped (item already in hand)"); }
                    return;
                }
            }
        }
    }
    void* invProp = GetProp("/Script/FortniteGame.FortInventory.Inventory");
    void* instProp = GetProp("/Script/FortniteGame.FortItemList.ItemInstances");
    if (!instProp || !invProp) return;
    void* list = GetField(session.inventory, invProp);
    Array arr = Read<Array>(GetField(list, instProp));
    for (int i = 0; i < arr.Count; ++i) {
        void* inst = Read<void*>(arr.Data, (size_t)i * 8);
        if (!inst) continue;
        Guid cand = ItemGuidOf(inst);
        if (!memcmp(&cand, &guid, sizeof(Guid))) {
            for (int s = 0; s < 6; ++s) if (session.slots[(size_t)s] == inst) { EquipSlot(s); return; }
            // Not bound to a slot — still allow equipping weapons.
            void* def = ItemDefinitionOf(inst);
            void* weaponDef = ResolveWeaponDefForEquip(def);
            if (weaponDef) {
                Guid tracker{};
                Call eq("/Script/FortniteGame.FortPawn.EquipWeaponDefinition");
                bool bDisableAnim = false;
                if (eq.ok() && eq.set("WeaponData", weaponDef) && eq.set("ItemEntryGuid", cand) &&
                    eq.set("TrackerGuid", tracker) && eq.set("bDisableEquipAnimation", bDisableAnim))
                    eq.run(session.pawn);
            }
            return;
        }
    }
}
void Athena::OnServerExecuteInventoryWeapon(void* weapon) {
    if (!weapon || !session.pawn) return;
    void* p = GetProp("/Script/FortniteGame.FortWeapon.ItemEntryGuid");
    if (!p) return;
    Guid guid = Read<Guid>(GetField(weapon, p));
    OnServerExecuteInventoryItem(guid);
}

// ============================================================================
// PICKUPS — [F06/F18] entry-based access + stacking (Era core local_pickup)
// ============================================================================
static void* PickupEntry(void* pickup) {
    void* entryProp = GetProp("/Script/FortniteGame.FortPickup.PrimaryPickupItemEntry");
    return pickup && entryProp ? GetField(pickup, entryProp) : nullptr;
}
static void* EntryDefinition(void* entry) {
    void* p = entry ? GetProp("/Script/FortniteGame.FortItemEntry.ItemDefinition") : nullptr;
    return p ? Read<void*>(GetField(entry, p)) : nullptr;
}
static int EntryCount(void* entry) {
    void* p = entry ? GetProp("/Script/FortniteGame.FortItemEntry.Count") : nullptr;
    return p ? Read<int>(GetField(entry, p)) : 0;
}

// [F18] Local pickup with stack transfer and weapon slot assignment.
bool Athena::LocalPickup(void* pickup) {
    if (!pickup || !IsA(pickup, FindObj("/Script/FortniteGame.FortPickup"))) return false;
    Vector location{};
    if (!ActorLocation(pickup, location) || !NearPlayer(location, 600.0f)) return false;
    void* entry = PickupEntry(pickup);
    void* def = EntryDefinition(entry);
    int count = EntryCount(entry);
    if (!def || count < 1 || count > 9999) return true;

    // Stackable: transfer into existing stacks first.
    int maximum = 0;
    void* maxProp = GetProp("/Script/FortniteGame.FortItemDefinition.MaxStackSize");
    if (maxProp && Read<int>(maxProp, profile::PropElementSize) == 8) {
        double v = Read<double>(GetField(def, maxProp));
        if (v > 1.0 && v <= 9999.0) maximum = (int)v;
    }
    if (maximum > 1) {
        void* invProp   = GetProp("/Script/FortniteGame.FortInventory.Inventory");
        void* instProp  = GetProp("/Script/FortniteGame.FortItemList.ItemInstances");
        void* entryProp = GetProp("/Script/FortniteGame.FortItemList.ReplicatedEntries");
        void* itemEntryProp = GetProp("/Script/FortniteGame.FortWorldItem.ItemEntry");
        void* countProp = GetProp("/Script/FortniteGame.FortItemEntry.Count");
        void* guidProp  = GetProp("/Script/FortniteGame.FortItemEntry.ItemGuid");
        if (invProp && instProp && entryProp && itemEntryProp && countProp && guidProp) {
            void* list = GetField(session.inventory, invProp);
            void* instArr = GetField(list, instProp);
            void* entryArr = GetField(list, entryProp);
            Array instances = Read<Array>(instArr);
            Array replicated = Read<Array>(entryArr);
            int stride = Read<int>(itemEntryProp, profile::PropElementSize);
            for (int i = 0; i < instances.Count && count > 0; ++i) {
                void* item = Read<void*>(instances.Data, (size_t)i * 8);
                if (!item || ItemDefinitionOf(item) != def) continue;
                void* itemEntry = GetField(item, itemEntryProp);
                int have = Read<int>(GetField(itemEntry, countProp));
                int amount = std::min(count, std::max(0, maximum - have));
                Guid id = Read<Guid>(GetField(itemEntry, guidProp));
                void* matching = nullptr;
                for (int j = 0; j < replicated.Count; ++j) {
                    void* candidate = (char*)replicated.Data + (size_t)j * stride;
                    Guid other = Read<Guid>(GetField(candidate, guidProp));
                    if (!memcmp(&id, &other, sizeof(id))) { matching = candidate; break; }
                }
                if (!matching || amount < 1) break;
                int updated = have + amount;
                Write<int>(GetField(itemEntry, countProp), 0, updated);
                Write<int>(GetField(matching, countProp), 0, updated);
                count -= amount;
            }
            if (count < EntryCount(entry)) {
                if (count > 0) { Write<int>(GetField(entry, countProp), 0, count); }
                else { Call d("/Script/Engine.Actor.K2_DestroyActor"); if (d.ok()) d.run(pickup); }
                InventoryUpdate();
                LOG("Pickup stacked %s remainder=%d", ObjName(def).c_str(), count);
                return true;
            }
            if (count == 0) return true;
        }
    }

    // Weapon/item pickup: first free primary slot (0 is the pickaxe).
    int slot = -1;
    if (IsA(def, FindObj("/Script/FortniteGame.FortWeaponItemDefinition"))) {
        for (int i = 1; i < 6; ++i)
            if (!session.slots[(size_t)i]) { slot = i; break; }
        if (slot < 0) { LOG("Pickup retained: primary quickbar full"); return true; }
    }
    if (!GiveItem(def, count, slot, 0, true)) {
        LOG("Pickup retained: duplicate, full stack, or insertion failed (%s)", ObjName(def).c_str());
        return true;
    }
    Call d("/Script/Engine.Actor.K2_DestroyActor");
    if (d.ok()) d.run(pickup);
    InventoryUpdate();
    LOG("Local pickup transferred %s count=%d slot=%d", ObjName(def).c_str(), count, slot);
    return true;
}

void Athena::ServerHandlePickup(void* pickup) { LocalPickup(pickup); }

// [F06] SpawnPickup writes the FFortPickup entry (ItemDefinition/Count no
// longer exist on AFortPickup in 21.30) and tags source flags so the pickup
// widget/despawn behave like native loot.
void Athena::SpawnPickup(void* def, int count, Vector loc, uint8_t sourceFlags, uint8_t spawnSource, bool toss) {
    if (!def || !session.world) return;
    void* klass = FindObj("/Script/FortniteGame.FortPickupAthena");
    if (!klass) return;
    Transform t{};
    t.Location[0] = loc.X; t.Location[1] = loc.Y; t.Location[2] = loc.Z;
    void* pickup = SpawnActor(session.world, klass, session.pawn, t);
    if (!pickup) return;
    void* entry = PickupEntry(pickup);
    void* defProp = GetProp("/Script/FortniteGame.FortItemEntry.ItemDefinition");
    void* cntProp = GetProp("/Script/FortniteGame.FortItemEntry.Count");
    if (entry && defProp) Write<void*>(GetField(entry, defProp), 0, def);
    if (entry && cntProp) Write<int>(GetField(entry, cntProp), 0, count);
    Write<uint8_t>(pickup, profile::AFortPickup_SourceFlags, sourceFlags);
    Write<uint8_t>(pickup, profile::AFortPickup_SpawnSource, spawnSource);
    Call rep("/Script/FortniteGame.FortPickup.OnRep_PrimaryPickupItemEntry"); if (rep.ok()) rep.run(pickup);
    if (toss) {
        Call c("/Script/FortniteGame.FortPickup.TossPickup");
        if (c.ok()) {
            c.set("FinalLocation", loc);
            c.set("ItemOwner", session.pawn);
            int overrideMax = -1; c.set("OverrideMaxStackCount", overrideMax);
            bool bToss = true; c.set("bToss", bToss);
            bool combine = true; c.set("bShouldCombinePickupsWhenTossCompletes", combine);
            // EEFortPickupSourceTypeFlag (uint8) / EEFortPickupSpawnSource (uint8)
            c.set_any((uint8_t)sourceFlags, "InPickupSourceTypeFlags");
            c.set_any((uint8_t)spawnSource, "InPickupSpawnSource");
            c.run(pickup);
        }
    }
}

// ============================================================================
// WORLD SYSTEMS — [F28-F33] Erbium parity (streaming / vehicles / wildlife /
// minimap). All offsets and function paths verified against the 21.30 SDK.
// ============================================================================

// [F28] Standalone clients never receive the server's level-streaming
// visibility updates, so distant map regions stay unloaded. Force every
// UWorld.StreamingLevels (@0x88) entry to load+show. Reflected in 21.30:
// ULevelStreaming.SetShouldBeLoaded(bInShouldBeLoaded) / SetShouldBeVisible.
void Athena::ForceLevelStreaming() {
    if (!session.world) return;
    Array levels = Read<Array>((char*)session.world + profile::World_StreamingLevels);
    if (levels.Count < 0 || levels.Count > 4096 || !levels.Data) return;
    Call load("/Script/Engine.LevelStreaming.SetShouldBeLoaded");
    Call vis ("/Script/Engine.LevelStreaming.SetShouldBeVisible");
    int forced = 0;
    for (int i = 0; i < levels.Count; ++i) {
        void* level = Read<void*>(levels.Data, (size_t)i * 8);
        if (!level || IsBadPtr(level)) continue;
        bool okL = false, okV = false;
        if (load.ok()) { bool b = true; okL = load.set("bInShouldBeLoaded", b) && load.run(level); }
        if (vis.ok())  { bool b = true; okV = vis.set("bInShouldBeVisible", b) && vis.run(level); }
        if (okL || okV) ++forced;
    }
    LOG("[Stream] forced %d/%d streaming levels (load+visible)", forced, levels.Count);
}

// [F29/F39] Vehicles — Erbium port + field fixes. The map's own
// AFortAthenaVehicleSpawner actors carry CachedFortVehicleItemDef (@0x2B8)
// whose VehicleActorClass (@0xA38) is the exact 21.30 vehicle class. Two
// field fixes from the first device run:
//   1. the one-shot 12s scan ran while streaming levels were still loading
//      async -> 0 spawners. VehicleRetry() now re-scans every 5s for up to
//      30 attempts (~150s).
//   2. fallback now StaticLoadClass'es the exact Ch3 vehicle BP mounts
//      (Dirtbike/Sportbike/Valet/ModdedBasicCar — Erbium's s21 tag table),
//      so spawning works even if no spawner ever surfaces. Dagwood vehicles
//      get SetFuel(100) like Erbium does.
static void* LoadVehicleClassByName(const char* mount_path) {
    // Try the plugin mount as-is ("/Dirtbike/..."), then a /Game/ variant.
    if (void* c = LoadClass(mount_path, "/Script/FortniteGame.FortAthenaVehicle")) return c;
    std::string withGame = std::string("/Game") + mount_path;
    return LoadClass(withGame.c_str(), "/Script/FortniteGame.FortAthenaVehicle");
}

static const char* const kVehicleMounts[] = {
    "/Dirtbike/Vehicle/Motorcycle_DirtBike_Vehicle.Motorcycle_DirtBike_Vehicle_C",
    "/Sportbike/Vehicle/Motorcycle_Sport_Vehicle.Motorcycle_Sport_Vehicle_C",
    "/Valet/TaxiCab/Valet_TaxiCab_Vehicle.Valet_TaxiCab_Vehicle_C",
    "/ModdedBasicCar/Vehicle/Valet_BasicCar_Vehicle_SuperSedan.Valet_BasicCar_Vehicle_SuperSedan_C",
    "/Valet/BasicCar/Valet_BasicCar_Vehicle_Upgrade.Valet_BasicCar_Vehicle_Upgrade_C",
    "/Valet/BasicTruck/Valet_BasicTruck_Vehicle_Upgrade.Valet_BasicTruck_Vehicle_Upgrade_C",
    "/Valet/BigRig/Valet_BigRig_Vehicle_Upgrade.Valet_BigRig_Vehicle_Upgrade_C",
    "/Valet/SportsCar/Valet_SportsCar_Vehicle_Upgrade.Valet_SportsCar_Vehicle_Upgrade_C",
    nullptr
};

// [F55] Native 21.30 vehicle classes — always resident (no mount path
// guessing). [F72] CRASH FIX: only FortAthenaSimpleCar (physics branch)
// survives a raw UWorld::SpawnActor. Every SK-branch class
// (Antelope/Goat/Doghouse/Ferret/Jackal : AFortAthenaSKVehicle) is a
// skeletal-mesh vehicle whose wheel/skeleton/anim config lives in BP —
// spawning the bare native class crashes the game (device log:
// "spawned FortAthenaSimpleCar" OK -> Antelope -> dead, no further log).
static const char* const kNativeVehicles[] = {
    "/Script/FortniteGame.FortAthenaSimpleCar",      // physics branch — raw-spawn safe
    nullptr
};

// [F72] Raw-spawn quarantine for the native SK bases. BP vehicle classes
// (name ends "_C") carry their own config and are always allowed; the map's
// own spawner classes are handled through SpawnVehicleWithConstruction.
static bool UnsafeRawVehicleClass(void* vclass) {
    if (!vclass || IsBadPtr(vclass)) return true;
    std::string n = ObjName(vclass);
    if (n.size() > 2 && n.compare(n.size() - 2, 2, "_C") == 0) return false;  // BP class — configured
    return n == "FortAthenaAntelopeVehicle" || n == "FortAthenaGoatVehicle" ||
           n == "FortAthenaDoghouseVehicle" || n == "FortAthenaFerretVehicle" ||
           n == "FortAthenaJackalVehicle"   || n == "FortAthenaSKMotorVehicle" ||
           n == "FortAthenaSKVehicle";
}

// [F55] Ask the map's own spawner to do its job — AFortAthenaVehicleSpawner
// carries SpawnVehicle()/SpawnVehicleWithConstruction() UFunctions (SDK
// GObjects 36154-36158); this is the same call the warmup game mode makes,
// so class/fuel/transform all come from the map data.
static bool SpawnerNativeSpawn(void* spawner) {
    if (!spawner || IsBadPtr(spawner)) return false;
    {
        Call sv("/Script/FortniteGame.FortAthenaVehicleSpawner.SpawnVehicle");
        if (sv.ok() && sv.run(spawner)) return true;
    }
    {
        Call sv2("/Script/FortniteGame.FortAthenaVehicleSpawner.SpawnVehicleWithConstruction");
        if (sv2.ok() && sv2.run(spawner)) return true;
    }
    return false;
}

// [F72] The map's OWN vehicle class — GetVehicleClass() is a BlueprintPure
// native returning exactly what this spawner is configured to spawn (same
// class the game itself uses). Fallback: CachedFortVehicleItemDef's
// VehicleActorClass.
static void* SpawnerVehicleClass(void* spawner) {
    if (!spawner || IsBadPtr(spawner)) return nullptr;
    {
        Call gvc("/Script/FortniteGame.FortAthenaVehicleSpawner.GetVehicleClass");
        if (gvc.ok() && gvc.run(spawner)) {
            void* c = gvc.result<void*>();
            if (c && !IsBadPtr(c)) return c;
        }
    }
    void* itemDef = Read<void*>(spawner, profile::VehicleSpawner_CachedItemDef);
    if (itemDef && !IsBadPtr(itemDef)) {
        void* c = Read<void*>(itemDef, profile::VehicleItemDef_ActorClass);
        if (c && !IsBadPtr(c)) return c;
    }
    return nullptr;
}

int Athena::SpawnVehiclesNear(int max) {
    if (!session.world || max < 1) return 0;
    void* spawnerCls = FindObj("/Script/FortniteGame.FortAthenaVehicleSpawner");
    void* vehicleCls = FindObj("/Script/FortniteGame.FortAthenaVehicle");
    if (!vehicleCls) return 0;
    Call loc("/Script/Engine.Actor.K2_GetActorLocation");
    Rotator identity{};
    int spawned = 0;

    auto register_vehicle = [&](void* veh) {
        if (!veh || IsBadPtr(veh)) return;
        if (std::find(session.vehicles.begin(), session.vehicles.end(), veh) == session.vehicles.end() &&
            session.vehicles.size() < 32)
            session.vehicles.push_back(veh);
        // Erbium: Dagwood (valet) vehicles spawn with empty tanks — fill the
        // fuel component (SetFuel lives on UFortVehicleFuelComponent, with
        // the vehicle holding CachedFuelComponent @0x1058).
        void* fuelComp = Read<void*>(veh, 0x1058);
        if (fuelComp && !IsBadPtr(fuelComp)) {
            Call fuel("/Script/FortniteGame.FortVehicleFuelComponent.SetFuel");
            if (fuel.ok()) { float full = 100.0f; if (fuel.set("NewFuel", full)) fuel.run(fuelComp); }
        }
    };

    auto spawn_at = [&](void* vclass, const Vector& at) -> bool {
        if (!vclass || IsBadPtr(vclass) || !SubclassOfFast(vclass, vehicleCls)) return false;
        if (UnsafeRawVehicleClass(vclass)) return false;   // [F72] SK native bases crash
        Transform t{};
        t.Location[0] = at.X; t.Location[1] = at.Y; t.Location[2] = at.Z + 150.0;
        RotatorToQuat(identity, t.Rotation);
        LOG("[Veh] raw spawn attempt: %s", ObjName(vclass).c_str());  // [F72] names the culprit if we die
        void* veh = SpawnActor(session.world, vclass, session.controller, t);
        if (!veh) return false;
        // [F47] register for the interaction/entry system
        register_vehicle(veh);
        LOG("[Veh] spawned %s", ObjName(vclass).c_str());
        return true;
    };

    // 1) Authentic path — the map's own spawners (retried by VehicleRetry).
    //    [F72] Erbium-aligned, three-tier per spawner:
    //      a) SpawnVehicleWithConstruction(GetVehicleClass(), transform) — the
    //         game's OWN construction entry (fuel/seats/mods wired) with the
    //         map's OWN class. SpawnVehicle() refuses offline (def/phase
    //         gating — the count never rose), but this one takes explicit
    //         args and is BlueprintCallable from map BPs.
    //      b) raw SpawnActor of the map class (Erbium's exact move) — gated
    //         by UnsafeRawVehicleClass.
    //      c) legacy SpawnVehicle() probe + cache ring fallback.
    //    Every success is verified by the returned pointer or a live count
    //    rise (F66) — no blind trust.
    if (spawnerCls) {
        auto spawners = FindAllInstances(spawnerCls, 96);
        LOG("[Veh] %zu spawner actors on map (attempt %d)", spawners.size(), session.vehicles_attempts);
        auto countLive = [&]() -> int {
            if (!vehicleCls) return 0;
            int n = 0;
            for (void* v : FindAllInstances(vehicleCls, 64)) if (v && !IsBadPtr(v)) ++n;
            return n;
        };
        int before = countLive();
        bool nativeTrusted = false;
        int constructed = 0;   // [F72] per-pass cap on the construction path
        Call svc("/Script/FortniteGame.FortAthenaVehicleSpawner.SpawnVehicleWithConstruction");
        for (void* sp : spawners) {
            if (spawned >= max) break;
            if (!sp || IsBadPtr(sp)) continue;
            Vector at{};
            if (!loc.ok() || !loc.run(sp)) continue;
            at = loc.result<Vector>();
            if (!std::isfinite(at.X) || !std::isfinite(at.Y) || !std::isfinite(at.Z)) continue;

            void* mapClass = SpawnerVehicleClass(sp);
            bool done = false;

            // (a) the game's own construction call with the map's own class
            if (mapClass && svc.ok() && constructed < 4) {
                Transform t{};
                t.Location[0] = at.X; t.Location[1] = at.Y; t.Location[2] = at.Z;
                RotatorToQuat(identity, t.Rotation);
                if (svc.set_any(mapClass, "Class_0", "Class") &&
                    svc.set_bytes("Transform", &t, (int)sizeof(Transform)) && svc.run(sp)) {
                    void* v = svc.result<void*>();
                    if (v && !IsBadPtr(v) && SubclassOfFast(Read<void*>(v, profile::ObjectClass), vehicleCls)) {
                        register_vehicle(v);
                        ++spawned; ++constructed; done = true;
                        LOG("[Veh] constructed %s at spawner (%.0f %.0f %.0f)",
                            ObjName(mapClass).c_str(), at.X, at.Y, at.Z);
                    }
                }
            }
            // (b) Erbium's raw move with the map class (BP classes only)
            if (!done && mapClass && spawn_at(mapClass, at)) { ++spawned; done = true; }

            // (c) legacy native probe — trusted only after the count rises
            if (!done && !nativeTrusted && SpawnerNativeSpawn(sp)) {
                int after = countLive();
                if (after > before) {
                    before = after;
                    ++spawned; nativeTrusted = true; done = true;
                    LOG("[Veh] native spawner -> vehicle at %.0f %.0f %.0f", at.X, at.Y, at.Z);
                } else {
                    LOG("[Veh] native spawner refused (no new vehicle) — fallback");
                    nativeTrusted = true;   // do not re-probe on every spawner (full GObjects scan)
                }
            }
            if (!done && session.vehicle_cache_n > 0)
                if (spawn_at(session.vehicle_cache[spawned % session.vehicle_cache_n], at)) ++spawned;
        }
        // [F66] merge ALL live vehicles into the tracked set (every pass —
        // spawner-produced, map-preplaced and ours). session.vehicles drives
        // the interaction/auto-enter loop; empty = "cars not enterable".
        if (vehicleCls) {
            for (void* v : FindAllInstances(vehicleCls, 64)) {
                if (v && !IsBadPtr(v) && session.vehicles.size() < 64 &&
                    std::find(session.vehicles.begin(), session.vehicles.end(), v) == session.vehicles.end())
                    session.vehicles.push_back(v);
            }
            LOG("[Veh] tracked %zu live vehicles (spawned this pass=%d)", session.vehicles.size(), spawned);
        }
    } else {
        LOG("[Veh] FortAthenaVehicleSpawner class not loaded");
    }

    // 1b) [F46] Generic spawner scan — 21.30 may place vehicles through BP
    // spawner classes whose names differ from the native one (the native
    // scan above found 0). Any actor class named *VehicleSpawn*/*BoatLaunch*
    // counts; each instance gets the cached fallback vehicle class.
    if (spawned < max && session.vehicle_cache_n > 0) {
        void* actorCls = FindObj("/Script/Engine.Actor");
        if (actorCls) {
            static const char* const kw[] = {"vehiclespawner", "vehiclespawn", "boatlaunch", "boatspawn", nullptr};
            std::vector<void*> spawnerClasses;
            GObjects_t* go = GObjectArray();
            void* classClass = FindObj("/Script/CoreUObject.Class");
            if (go && classClass) {
                for (int32 i = 0; i < go->NumElements && spawnerClasses.size() < 8; ++i) {
                    void* o = go->GetById((uint32)i);
                    if (!o || IsBadPtr(o) || !IsA(o, classClass)) continue;
                    if (!SubclassOfFast(o, actorCls) || SubclassOfFast(o, vehicleCls)) continue;
                    std::string n = ObjName(o);
                    std::transform(n.begin(), n.end(), n.begin(), ::tolower);
                    if (n.find("_c") == std::string::npos) continue;
                    for (int k = 0; kw[k]; ++k)
                        if (n.find(kw[k]) != std::string::npos) { spawnerClasses.push_back(o); break; }
                }
            }
            if (!spawnerClasses.empty()) {
                LOG("[Veh] generic spawner classes found: %zu", spawnerClasses.size());
                for (void* k : spawnerClasses) {
                    if (spawned >= max) break;
                    auto inst = FindAllInstances(k, 24);
                    for (void* s : inst) {
                        if (spawned >= max) break;
                        Vector at{};
                        if (!ActorLocation(s, at)) continue;
                        if (!std::isfinite(at.X) || !std::isfinite(at.Y) || !std::isfinite(at.Z)) continue;
                        if (spawn_at(session.vehicle_cache[spawned % session.vehicle_cache_n], at)) ++spawned;
                    }
                }
                LOG("[Veh] spawned %d from generic spawner points", spawned);
            }
        }
    }

    // 2) Static-load fallback — [F72] BP mounts FIRST (they carry their own
    //    vehicle config), then the single raw-spawn-safe native class
    //    (SimpleCar). The SK native bases are banned (crash). Cached on
    //    first success. Waits for attempt 3 (~15s) so the map's own
    //    spawners get first shot, then only re-probes every 5th attempt
    //    (StaticLoadClass hits disk).
    if (spawned < max && session.vehicle_cache_n == 0 && session.vehicles_attempts >= 3 &&
        ((session.vehicles_attempts % 5) == 0 || session.vehicles_attempts == 3)) {
        for (int i = 0; kVehicleMounts[i] && session.vehicle_cache_n < 32; ++i) {
            if (void* c = LoadVehicleClassByName(kVehicleMounts[i]))
                session.vehicle_cache[session.vehicle_cache_n++] = c;
        }
        for (int i = 0; kNativeVehicles[i] && session.vehicle_cache_n < 32; ++i) {
            if (void* c = FindObj(kNativeVehicles[i]))
                session.vehicle_cache[session.vehicle_cache_n++] = c;
        }
        LOG("[Veh] fallback resolved %d vehicle classes (BP first, SK natives banned)", session.vehicle_cache_n);
    }
    if (spawned < max && session.vehicle_cache_n > 0) {
        Vector origin{};
        if (ActorLocation(session.pawn ? session.pawn : session.controller, origin) || ActorLocation(session.world, origin)) {
            static const double offs[8][2] = {{700,0},{-700,0},{0,700},{0,-700},{700,700},{-700,-700},{700,-700},{-700,700}};
            int ring = 0;
            // [F72] ONE ring spawn per pass (retry every 5s), rotating
            // through the cache — a bad class can no longer take down 11
            // spawns in a single frame, and the "raw spawn attempt" log
            // line names the exact class.
            int start = session.vehicles_attempts % session.vehicle_cache_n;
            for (int k = 0; k < session.vehicle_cache_n && spawned < max && ring < 1; ++k) {
                int i = (start + k) % session.vehicle_cache_n;
                Vector at = origin;
                at.X += offs[ring % 8][0]; at.Y += offs[ring % 8][1]; at.Z += 150.0;
                ++ring;
                if (spawn_at(session.vehicle_cache[i], at)) ++spawned;
            }
        }
    }
    LOG("[Veh] total spawned=%d", spawned);
    return spawned;
}

// [F39] Vehicle spawner retry — streaming levels load async, so the spawner
// actors (and the vehicle BPs they reference) surface over the first ~2 min.
// [F72] with 1-spawn-per-pass rate limiting, "spawned>0" can no longer stop
// the retry loop — keep going until we track a usable fleet or run out.
void Athena::VehicleRetry() {
    if (session.vehicles_attempts >= 30) return;
    if (session.vehicles.size() >= 8) { session.vehicles_attempts = 30; return; }
    ++session.vehicles_attempts;
    double now = Monotonic();
    (void)now;
    int spawned = SpawnVehiclesNear(session.vehicle_cache_n ? 2 : 12);
    (void)spawned;
}

// [F75-c] shared wildlife class resolution — loaded FortAIPawn subclasses
// (Irwin modules first) + the static-load fallback.
static void WildClasses(std::vector<void*>& classes) {
    void* aiPawn = FindObj("/Script/FortniteGame.FortAIPawn");
    if (!aiPawn) { LOG("[Wild] FortAIPawn missing"); return; }
    {
        static const char* const prefer[] = {"irwin", "burt", "nug", "smackie", "wildlife", nullptr};
        static const char* const skip[]   = {"parent", "base", "controller", "loot", "spawner", nullptr};
        GObjects_t* go = GObjectArray();
        void* classClass = FindObj("/Script/CoreUObject.Class");
        if (go && classClass) {
            for (int32 i = 0; i < go->NumElements && (int)classes.size() < 24; ++i) {
                void* object = go->GetById((uint32)i);
                if (!object || IsBadPtr(object) || object == aiPawn) continue;
                if (!IsA(object, classClass)) continue;
                if (!SubclassOfFast(object, aiPawn)) continue;
                std::string n = ObjName(object);
                if (n.find("_C") == std::string::npos) continue;
                bool bad = false;
                for (int k = 0; skip[k]; ++k) if (n.find(skip[k]) != std::string::npos) { bad = true; break; }
                if (bad) continue;
                bool good = false;
                for (int k = 0; prefer[k]; ++k) if (n.find(prefer[k]) != std::string::npos) { good = true; break; }
                if (!good && classes.size() >= 8) continue;   // unknown names only as filler
                classes.push_back(object);
            }
        }
    }
    // Static-load fallback — Irwin modules (paths unknown, try common mounts).
    if (classes.empty()) {
        static const char* const pkgs[] = {
            "NPC_Pawn_Irwin_Prey_Burt.NPC_Pawn_Irwin_Prey_Burt_C",
            "NPC_Pawn_Irwin_Prey_Nug.NPC_Pawn_Irwin_Prey_Nug_C",
            "NPC_Pawn_Irwin_Simple_Smackie.NPC_Pawn_Irwin_Simple_Smackie_C",
            nullptr
        };
        static const char* const mounts[] = {
            "/Game/Athena/Environments/Blueprints/AI/",
            "/Game/Athena/Environments/Blueprints/AI/Irwin/",
            "/Game/Athena/Environments/Blueprints/AI/Wildlife/",
            "/Game/NPC/",
            "",
            nullptr
        };
        for (int p = 0; pkgs[p]; ++p) {
            for (int m = 0; mounts[m]; ++m) {
                std::string full = std::string(mounts[m]) + pkgs[p];
                if (void* c = LoadClass(full.c_str(), "/Script/FortniteGame.FortAIPawn")) { classes.push_back(c); break; }
            }
        }
        LOG("[Wild] static-load fallback resolved %zu classes", classes.size());
    }
}

// [F75-c] spawn + prepare one animal (AI controller, survival flags, motor).
static void WildSpawnOne(void* cls, const Vector& at) {
    Transform t{};
    t.Location[0] = at.X; t.Location[1] = at.Y; t.Location[2] = at.Z;
    Rotator identity{};
    RotatorToQuat(identity, t.Rotation);
    void* animal = SpawnActor(session.world, cls, session.controller, t);
    if (!animal) return;
    // Give the animal its AI controller so BP wander/agro logic runs.
    {
        void* aiCls = GetObjField(animal, "/Script/Engine.Pawn.AIControllerClass");
        if (aiCls && !IsBadPtr(aiCls) && SubclassOfFast(aiCls, FindObj("/Script/Engine.AIController"))) {
            Transform ct{}; ct.Location[0] = at.X; ct.Location[1] = at.Y; ct.Location[2] = at.Z;
            RotatorToQuat(identity, ct.Rotation);
            if (void* aic = SpawnActor(session.world, aiCls, animal, ct)) Possess(aic, animal);
        }
    }
    // [F71] wildlife must survive: the field log showed spawned animals
    // dying (spawn/fall damage). Disable damage, full health, force shown.
    SetBool(animal, "/Script/Engine.Actor.bCanBeDamaged", false);
    { Call hp("/Script/FortniteGame.FortPawn.SetMaxHealth"); if (hp.ok()) { float v = 100.0f; if (hp.set_at(0, v)) hp.run(animal); } }
    { Call hp("/Script/FortniteGame.FortPawn.SetHealth");     if (hp.ok()) { float v = 100.0f; if (hp.set_at(0, v)) hp.run(animal); } }
    { Call sh("/Script/Engine.Actor.SetActorHiddenInGame");   if (sh.ok()) { bool h = false; if (sh.set("bNewHidden", h)) sh.run(animal); } }
    // [F75-c] roam motor — animals drift around their spawn area (the AI
    // controllers alone do not drive them on this build).
    g_wild_motors.push_back({animal, (float)(BRRand01() * 360.0), 0.0});
}

int Athena::SpawnWildlifeNear(int max) {
    if (!session.world || !session.pawn || max < 1) return 0;
    std::vector<void*> classes;
    WildClasses(classes);
    LOG("[Wild] %zu animal classes available", classes.size());
    if (classes.empty()) return 0;
    Vector origin{};
    if (!ActorLocation(session.pawn, origin)) return 0;
    static const double offs[12][2] = {{600,0},{-600,0},{0,600},{0,-600},{600,600},{-600,-600},
                                       {600,-600},{-600,600},{900,0},{-900,0},{0,900},{0,-900}};
    int spawned = 0;
    for (int i = 0; i < max; ++i) {
        void* cls = classes[(size_t)i % classes.size()];
        Vector at{ origin.X + offs[i % 12][0], origin.Y + offs[i % 12][1], origin.Z + 100.0 };
        WildSpawnOne(cls, at);
        ++spawned;
        LOG("[Wild] spawned %s", ObjName(cls).c_str());
    }
    LOG("[Wild] total spawned=%d", spawned);
    return spawned;
}

// [F75-c/F76-e] map-wide wildlife — animals "in their places". First scan
// GObjects for the map's own loaded wildlife/NPC spawner ACTORS (class-name
// contains wildlife/animal/npc AND spawner/spawn — e.g. BGA_Athena_Wildlife-
// Spawn, the flopper-style world spawners that stream with the terrain) and
// drop the animals exactly on those points — that is where the game's own
// lifecycle puts them. Fallback: the uniform playable-bounds scatter (the
// native spawners may not be streamed yet at match start).
int Athena::SpawnWildlifeScattered(int max) {
    if (!session.world || !session.pawn || max < 1) return 0;
    std::vector<void*> classes;
    WildClasses(classes);
    if (classes.empty()) { LOG("[Wild] scattered — no classes"); return 0; }
    int spawned = 0;
    // [F76-e] natural spawner positions — loaded level actors first.
    {
        GObjects_t* go = GObjectArray();
        void* actorCls = FindObj("/Script/Engine.Actor");
        if (go && actorCls) {
            std::vector<Vector> spots;
            for (int32 i = 0; i < go->NumElements && spots.size() < 24; ++i) {
                void* object = go->GetById((uint32)i);
                if (!object || IsBadPtr(object) || IsCDO(object)) continue;
                void* oc = Read<void*>(object, profile::ObjectClass);
                if (!oc || IsBadPtr(oc)) continue;
                if (!SubclassOfFast(oc, actorCls)) continue;    // world actor only
                std::string cn = ObjName(oc);
                std::transform(cn.begin(), cn.end(), cn.begin(), ::tolower);
                if (cn.find("wildlife") == std::string::npos &&
                    cn.find("npcspawn") == std::string::npos &&
                    cn.find("animalspawn") == std::string::npos) continue;
                if (cn.find("spawn") == std::string::npos &&
                    cn.find("spawner") == std::string::npos) continue;
                Vector l{};
                if (!ActorLocation(object, l)) continue;
                if (std::fabs(l.X) < 1.0 && std::fabs(l.Y) < 1.0) continue;  // origin stub
                spots.push_back(l);
            }
            if (!spots.empty()) {
                LOG("[Wild] ✅ %zu natural spawner positions found — spawning there", spots.size());
                for (int i = 0; i < max; ++i) {
                    const Vector& s = spots[(size_t)i % spots.size()];
                    Vector at{ s.X + (BRRand01() - 0.5f) * 300.0f,
                               s.Y + (BRRand01() - 0.5f) * 300.0f, s.Z + 120.0f };
                    void* cls = classes[(size_t)(BRRand() % classes.size())];
                    WildSpawnOne(cls, at);
                    ++spawned;
                }
                LOG("[Wild] natural pass: %d animals at spawner positions", spawned);
                return spawned;
            }
            LOG("[Wild] no natural spawner actors streamed yet — uniform scatter");
        }
    }
    Vector center{0, 0, 0};
    if (!br.zone_centers.empty()) center = br.zone_centers[0];
    for (int i = 0; i < max; ++i) {
        float ang = BRRand01() * 6.2831853f;
        float dist = 12000.0f + BRRand01() * 68000.0f;   // 120m..800m from center
        Vector at{ center.X + std::cos(ang) * dist,
                   center.Y + std::sin(ang) * dist,
                   600.0 };
        void* cls = classes[(size_t)(BRRand() % classes.size())];
        WildSpawnOne(cls, at);
        ++spawned;
    }
    LOG("[Wild] scattered %d animals across the map (motors=%zu)", spawned, g_wild_motors.size());
    return spawned;
}

// ============================================================================
// [F46] WORLD-CONTENT DISCOVERY — the 21.30 SDK dump was taken in the lobby,
// so map BP classes (chests, floor loot, vehicle spawners, sharks, saplings)
// never appear in the headers: their real names only exist once the terrain
// has streamed in. This one-shot scans GObjects for classes matching the
// keywords, logs what it finds (diagnosis for future rounds) and drives the
// systems that previously depended on hardcoded class paths.
void Athena::ScanMapContent() {
    if (!session.world || !session.pawn) return;
    GObjects_t* go = GObjectArray();
    void* classClass = FindObj("/Script/CoreUObject.Class");
    void* bcCls   = FindObj("/Script/FortniteGame.BuildingContainer");
    void* actorCls = FindObj("/Script/Engine.Actor");
    void* aiPawn  = FindObj("/Script/FortniteGame.FortAIPawn");
    if (!go || !classClass || !bcCls || !actorCls) return;

    std::vector<void*> containerClasses, spawnerClasses, sharkClasses, saplingClasses;
    static const char* const contKw[]  = {"tiered", "chest", "floorloot", "ammobox", nullptr};
    static const char* const spawnKw[] = {"vehiclespawner", "vehiclespawn", "boatlaunch", "boatspawn", nullptr};
    static const char* const skipKw[]  = {"component", "parent", "data", "definition", "default", "hud", "ui", nullptr};
    for (int32 i = 0; i < go->NumElements; ++i) {
        void* o = go->GetById((uint32)i);
        if (!o || IsBadPtr(o) || o == bcCls) continue;
        if (!IsA(o, classClass)) continue;
        std::string n = ObjName(o);
        if (n.find("_C") == std::string::npos) continue;
        std::string low = n;
        std::transform(low.begin(), low.end(), low.begin(), ::tolower);
        bool skip = false;
        for (int k = 0; skipKw[k]; ++k) if (low.find(skipKw[k]) != std::string::npos) { skip = true; break; }
        if (SubclassOfFast(o, bcCls)) {
            for (int k = 0; contKw[k]; ++k)
                if (low.find(contKw[k]) != std::string::npos) { containerClasses.push_back(o); break; }
        } else if (!skip && SubclassOfFast(o, actorCls)) {
            for (int k = 0; spawnKw[k]; ++k)
                if (low.find(spawnKw[k]) != std::string::npos) { spawnerClasses.push_back(o); break; }
            if (low.find("shark") != std::string::npos && (!aiPawn || SubclassOfFast(o, aiPawn)))
                sharkClasses.push_back(o);
            else if (low.find("sapling") != std::string::npos && saplingClasses.size() < 4)
                saplingClasses.push_back(o);
        }
    }
    LOG("[Scan] classes: containers=%zu spawners=%zu sharks=%zu saplings=%zu",
        containerClasses.size(), spawnerClasses.size(), sharkClasses.size(), saplingClasses.size());
    for (size_t i = 0; i < containerClasses.size() && i < 10; ++i) LOG("[Scan]   container: %s", ObjName(containerClasses[i]).c_str());
    for (size_t i = 0; i < spawnerClasses.size() && i < 10; ++i)   LOG("[Scan]   spawner: %s",  ObjName(spawnerClasses[i]).c_str());
    for (size_t i = 0; i < sharkClasses.size() && i < 6; ++i)      LOG("[Scan]   shark: %s",    ObjName(sharkClasses[i]).c_str());
    for (size_t i = 0; i < saplingClasses.size() && i < 6; ++i)    LOG("[Scan]   sapling: %s",  ObjName(saplingClasses[i]).c_str());

    // Containers + spawners + vehicles — ONE unified instance pass with a
    // per-class verdict cache (each class costs one hierarchy walk, not one
    // full GObjects scan — N full scans would freeze the game thread).
    session.containers.clear();
    session.container_locs.clear();
    session.container_done.clear();
    int autoLooted = 0;
    void* vehCls = FindObj("/Script/FortniteGame.FortAthenaVehicle");
    struct Target { void* cls; int kind; };   // 0 container | 1 spawner | 2 vehicle
    std::vector<Target> targets;
    // [F56] EVERY BuildingContainer instance is a loot container — the map's
    // chest/floorloot/ammo BP classes all derive from the native base. The
    // old keyword filter missed whole families (0 containers on device).
    // Floor-loot members are detected per-instance from their class name.
    if (bcCls) targets.push_back({bcCls, 0});
    for (void* k : containerClasses)
        if (k != bcCls) targets.push_back({k, 0});
    for (void* k : spawnerClasses) targets.push_back({k, 1});
    if (vehCls) targets.push_back({vehCls, 2});

    std::vector<std::pair<std::vector<void*>, int>> found;   // (instances, kind)
    std::map<void*, int> verdict;                            // class -> targets idx (-1 none)
    if (!targets.empty()) {
        GObjects_t* go2 = go;
        std::vector<std::vector<void*>> buckets(targets.size());
        for (int32 i = 0; i < go2->NumElements; ++i) {
            void* o = go2->GetById((uint32)i);
            if (!o || IsBadPtr(o) || IsCDO(o)) continue;
            void* oc = Read<void*>(o, profile::ObjectClass);
            if (!oc || IsBadPtr(oc)) continue;
            auto v = verdict.find(oc);
            int idx = -1;
            if (v != verdict.end()) {
                idx = v->second;
            } else {
                for (size_t t = 0; t < targets.size(); ++t)
                    if (SubclassOfFast(oc, targets[t].cls)) { idx = (int)t; break; }
                verdict[oc] = idx;
            }
            if (idx < 0) continue;
            if (buckets[idx].size() < 768) buckets[idx].push_back(o);   // [F56] full map of chests
        }
        for (size_t t = 0; t < targets.size(); ++t)
            found.push_back({std::move(buckets[t]), targets[t].kind});
    }

    // register containers (kind 0)
    int floorSeen = 0;
    for (auto& bucket : found) {
        if (bucket.second != 0) continue;
        for (void* c : bucket.first) {
            if (session.containers.size() >= 768) break;
            Vector l{};
            bool haveLoc = ActorLocation(c, l);
            session.containers.push_back(c);
            session.container_locs.push_back(haveLoc ? l : Vector{});
            session.container_done.push_back((uint8_t)(haveLoc ? GetBool(c, "/Script/FortniteGame.BuildingContainer.bAlreadySearched") : 1));
            if (!session.container_done.back()) {
                // [F56] floor-loot detection is per-INSTANCE (its class name),
                // not per-keyword-class — floor loot IS these containers.
                std::string cn = ObjName(Read<void*>(c, profile::ObjectClass));
                std::transform(cn.begin(), cn.end(), cn.begin(), ::tolower);
                bool isFloor = cn.find("floorloot") != std::string::npos;
                if (!isFloor && haveLoc) ++floorSeen;
                if (isFloor && haveLoc && autoLooted < 12) {   // cap per pass
                    bool ok = false;
                    {   // native first — the real cooked loot tables
                        Call bp("/Script/FortniteGame.BuildingContainer.BP_SpawnLoot");
                        if (bp.ok() && bp.set("PlayerPawn", session.pawn) && bp.run(c) &&
                            GetBool(c, "/Script/FortniteGame.BuildingContainer.bAlreadySearched")) ok = true;
                    }
                    if (!ok) ok = SearchContainer(c, l);
                    if (ok) { ++autoLooted; session.container_done.back() = 1; }
                }
            }
        }
    }
    LOG("[Scan] containers=%zu chests=%d floorloot-spawned=%d", session.containers.size(), floorSeen, autoLooted);
    if ((int)session.containers.size() > session.loot_found_total)
        session.loot_found_total = (int)session.containers.size();   // stop the [F45] retry loop

    // Vehicle spawners (kind 1) — spawn a vehicle at each discovered point.
    int fromSpawners = 0;
    for (auto& bucket : found) {
        if (bucket.second != 1) continue;
        for (void* s : bucket.first) {
            if (fromSpawners >= 10) break;
            Vector at{};
            if (!ActorLocation(s, at)) continue;
            if (!std::isfinite(at.X) || !std::isfinite(at.Y) || !std::isfinite(at.Z)) continue;
            void* itemDef = Read<void*>(s, profile::VehicleSpawner_CachedItemDef);
            void* vclass  = (itemDef && !IsBadPtr(itemDef)) ? Read<void*>(itemDef, profile::VehicleItemDef_ActorClass) : nullptr;
            if (!vclass && session.vehicle_cache_n > 0) vclass = session.vehicle_cache[0];
            if (!vclass || !vehCls || !SubclassOfFast(vclass, vehCls)) continue;
            Transform t{};
            t.Location[0] = at.X; t.Location[1] = at.Y; t.Location[2] = at.Z + 150.0;
            Rotator identity{};
            RotatorToQuat(identity, t.Rotation);
            if (void* veh = SpawnActor(session.world, vclass, session.controller, t)) {
                if (session.vehicles.size() < 32) session.vehicles.push_back(veh);
                ++fromSpawners;
                LOG("[Scan] vehicle spawned at spawner: %s", ObjName(vclass).c_str());
            }
        }
    }

    // Track every live vehicle (kind 2 — map-spawned or ours) for interaction.
    for (auto& bucket : found) {
        if (bucket.second != 2) continue;
        for (void* v : bucket.first)
            if (v && !IsBadPtr(v) && session.vehicles.size() < 32 &&
                std::find(session.vehicles.begin(), session.vehicles.end(), v) == session.vehicles.end())
                session.vehicles.push_back(v);
    }

    // Sharks — spawn if their classes surfaced with the map (they are
    // water-bound in the real game; here they get a spot near the player).
    int sharks = 0;
    for (void* k : sharkClasses) {
        if (sharks >= 2) break;
        Vector origin{};
        if (!ActorLocation(session.pawn, origin)) break;
        Transform t{};
        t.Location[0] = origin.X + 700.0 * (sharks ? -1.0 : 1.0);
        t.Location[1] = origin.Y + 300.0;
        t.Location[2] = origin.Z - 60.0;
        Rotator identity{};
        RotatorToQuat(identity, t.Rotation);
        if (SpawnActor(session.world, k, session.controller, t)) ++sharks;
    }
    if (sharkClasses.empty()) LOG("[Scan] no shark classes on this map (may not exist in 21.30)");
    else if (sharks > 0) LOG("[Scan] spawned %d shark(s)", sharks);

    // Reality saplings (Ch3 S3 plants with loot).
    int saplings = 0;
    for (void* k : saplingClasses) {
        if (saplings >= 3) break;
        Vector origin{};
        if (!ActorLocation(session.pawn, origin)) break;
        Transform t{};
        t.Location[0] = origin.X + 500.0 + 250.0 * saplings;
        t.Location[1] = origin.Y - 400.0;
        t.Location[2] = origin.Z + 80.0;
        Rotator identity{};
        RotatorToQuat(identity, t.Rotation);
        if (SpawnActor(session.world, k, session.controller, t)) ++saplings;
    }
    if (saplings > 0) LOG("[Scan] spawned %d reality sapling(s)", saplings);
}

// ============================================================================
// [F52/F58] WORLD ABILITIES — 21.30 ships GA_Athena_Player_Slide (tactical-
// sprint slide, anims included) and GA_AthenaEnterVehicle (EXACT spelling —
// the package has no underscore between Athena and Enter). Exit/In-vehicle
// and the emote input ability ride along when resident. The core pair must
// resolve; the rest are best-effort and never block the retry loop.
void Athena::GrantWorldAbilities() {
    if (session.world_abilities_granted || !session.pawn) return;
    static const char* const names[] = {
        "GA_Athena_Player_Slide_C",     // core
        "GA_AthenaEnterVehicle_C",      // core (exact 21.30 spelling)
        "GA_AthenaExitVehicle_C",       // best-effort
        "GA_AthenaInVehicle_C",         // best-effort
        "GAB_Emote_Generic_C",          // best-effort (emote input)
        nullptr
    };
    std::vector<void*> classes;
    for (int i = 0; names[i]; ++i) {
        if (void* k = FindAbilityClassNamed(names[i])) classes.push_back(k);
        else if (i < 2) { LOG("[World] ability class not resident yet: %s", names[i]); }
    }
    if (classes.size() < 2) return;   // wait for the core pair to stream in

    // [F67] Erbium-style delivery: build a mini FortAbilitySet and run it
    // through EquipFortAbilitySet so the ASC creates real ability specs with
    // input bindings. TryActivateAbilityByClass is NOT a grant — it fires the
    // ability once outside the spec system (activating GA_AthenaInVehicle_C
    // while on foot corrupted pawn state — "GiveAbility is dead / player
    // unplayable" on device).
    void* setCls = FindObj("/Script/FortniteGame.FortAbilitySet");
    if (!setCls) { LOG("[World] FortAbilitySet class missing"); return; }
    void* set = Fn<void*(*)(void*, void*)>(profile::SpawnObject)(setCls, session.pawn);
    if (!set || IsBadPtr(set)) { LOG("[World] mini ability set creation failed"); return; }
    void* abProp = GetProp("/Script/FortniteGame.FortAbilitySet.GameplayAbilities");
    if (!abProp) { LOG("[World] GameplayAbilities property missing"); return; }
    Array* arr = reinterpret_cast<Array*>(GetField(set, abProp));
    if (!arr || arr->Count != 0) { LOG("[World] mini set array unavailable"); return; }
    for (void* k : classes) {
        if (!k || IsBadPtr(k)) continue;
        Fn<int(*)(void*, void*, const void*)>(profile::GenericArrayAdd)(arr, abProp, &k);
    }
    if (ApplyAbilitySet(session.pawn, set)) {
        session.world_abilities_granted = true;
        LOG("[World] ✅ %zu world abilities delivered through EquipFortAbilitySet", classes.size());
    }
}

// ============================================================================
// [F52] NPC — ambient human pawns (FortPlayerPawnAthena + AIController) so
// the world is not empty. Bots keep the default body; cosmetics are applied
// where the assets are resident (same path as the local player's fallback).
int Athena::SpawnNPCs(int max) {
    if (!session.world || !session.pawn || max < 1) return 0;
    void* pawnCls = FindObj("/Game/Athena/PlayerPawn_Athena.PlayerPawn_Athena_C");
    if (!pawnCls) pawnCls = FindObj("/Script/FortniteGame.FortPlayerPawnAthena");
    if (!pawnCls) { LOG("[NPC] pawn class missing"); return 0; }
    Vector origin{};
    if (!ActorLocation(session.pawn, origin)) return 0;
    static const double offs[6][2] = {{600,300},{-600,300},{600,-300},{-600,-300},{0,800},{0,-800}};
    // [F52] resolve the fallback body/anim once (each lookup is a full
    // GObjects pass — doing it per-NPC would hitch the game thread).
    static void* npcBody   = nullptr;
    static void* npcAnimBP = nullptr;
    if (!npcBody)   npcBody   = FindLoadedObjectOfType("/Script/Engine.SkeletalMesh", "M_MED_Lancelot");
    if (!npcAnimBP) npcAnimBP = FindLoadedObjectOfType("/Script/Engine.AnimBlueprintGeneratedClass", "M_MED_Lancelot_AnimBP_C");
    Rotator identity{};
    int spawned = 0;
    for (int i = 0; i < max; ++i) {
        Transform t{};
        t.Location[0] = origin.X + offs[i % 6][0];
        t.Location[1] = origin.Y + offs[i % 6][1];
        t.Location[2] = origin.Z + 80.0;
        RotatorToQuat(identity, t.Rotation);
        void* npc = SpawnActor(session.world, pawnCls, session.controller, t);
        if (!npc) continue;
        // AI controller so the pawn is a real controller-owned actor (no
        // Fortnite bot logic — plain Engine.AIController keeps it stable).
        {
            void* aiCls = FindObj("/Script/Engine.AIController");
            if (aiCls) {
                Transform ct{};
                ct.Location[0] = t.Location[0]; ct.Location[1] = t.Location[1]; ct.Location[2] = t.Location[2];
                RotatorToQuat(identity, ct.Rotation);
                if (void* aic = SpawnActor(session.world, aiCls, npc, ct)) Possess(aic, npc);
            }
        }
        // Body so the NPC is visible (same fallback mesh as the local player).
        if (void* mesh = GetObjField(npc, "/Script/Engine.Character.Mesh")) {
            if (npcBody) {
                Call s("/Script/Engine.SkinnedMeshComponent.SetSkeletalMesh");
                if (s.ok()) { bool reinit = true; if (s.set("NewMesh", npcBody) && s.set("bReinitPose", reinit)) s.run(mesh); }
            }
            if (npcAnimBP) {
                Call a("/Script/Engine.SkeletalMeshComponent.SetAnimClass");
                if (a.ok() && a.set("NewClass", npcAnimBP)) a.run(mesh);
            }
        }
        // [F71] keep NPCs alive + visible (field log: "NPCs invisible, the far
        // ones died"). Bots have no server damage pipeline to heal them.
        SetBool(npc, "/Script/Engine.Actor.bCanBeDamaged", false);
        { Call hp("/Script/FortniteGame.FortPawn.SetMaxHealth"); if (hp.ok()) { float v = 100.0f; if (hp.set_at(0, v)) hp.run(npc); } }
        { Call hp("/Script/FortniteGame.FortPawn.SetHealth");     if (hp.ok()) { float v = 100.0f; if (hp.set_at(0, v)) hp.run(npc); } }
        { Call sh("/Script/Engine.Actor.SetActorHiddenInGame");   if (sh.ok()) { bool h = false; if (sh.set("bNewHidden", h)) sh.run(npc); } }
        ++spawned;
        if (session.npcs.size() < 16) session.npcs.push_back(npc);
        LOG("[NPC] spawned %p (%d/%d)", npc, spawned, max);
    }
    LOG("[NPC] total spawned=%d (tracked=%zu)", spawned, session.npcs.size());
    return spawned;
}

// [F62] Reality-tree saplings — Ch3 S3's signature world feature. The native
// RealitySeedGameplayRuntime.BuildingRealitySapling actor class ships in this
// build (SDK GObjects 1565); the B_RealitySapling_* controller components
// drive the full planting visuals when present.
int Athena::SpawnRealitySaplings(int max) {
    if (!session.world || !session.pawn || max < 1) return 0;
    void* sapCls = FindObj("/Script/RealitySeedGameplayRuntime.BuildingRealitySapling");
    if (!sapCls) sapCls = FindClassNamedLoose(FindObj("/Script/Engine.Actor"), "BuildingRealitySapling");
    if (!sapCls) { LOG("[Reality] sapling class not resident"); return 0; }
    Vector origin{};
    if (!ActorLocation(session.pawn, origin)) return 0;
    Rotator identity{};
    int spawned = 0;
    static const double offs[4][2] = {{500,-400},{-520,-420},{-80,-560},{760,-180}};
    for (int i = 0; i < max; ++i) {
        Transform t{};
        t.Location[0] = origin.X + offs[i % 4][0];
        t.Location[1] = origin.Y + offs[i % 4][1];
        t.Location[2] = origin.Z + 80.0;
        RotatorToQuat(identity, t.Rotation);
        if (SpawnActor(session.world, sapCls, session.controller, t)) {
            ++spawned;
            LOG("[Reality] sapling planted (%d/%d)", spawned, max);
        }
    }
    LOG("[Reality] total saplings=%d", spawned);
    return spawned;
}

// [F60] Minimap marker — ServerSpawnMark is a server RPC and a standalone
// session has no server to replicate the marker actor. Spawn the real marker
// actor locally instead (B_AthenaPlayerMarker_WithCustomization_C, resident
// per the SDK dump), falling back to the base marker BP.
void Athena::SpawnLocalMarker() {
    if (!session.world || !session.pawn) return;
    void* markerCls = FindClassNamedLoose(FindObj("/Script/Engine.Actor"), "B_AthenaPlayerMarker_WithCustomization_C");
    if (!markerCls) markerCls = FindClassNamedLoose(FindObj("/Script/Engine.Actor"), "B_AthenaMapMarkerBase_C");
    if (!markerCls) { LOG("[Mark] no marker BP resident"); return; }
    Vector loc{};
    if (!ActorLocation(session.pawn, loc)) return;
    Transform t{};
    t.Location[0] = loc.X; t.Location[1] = loc.Y; t.Location[2] = loc.Z + 100.0;
    Rotator identity{};
    RotatorToQuat(identity, t.Rotation);
    void* marker = SpawnActor(session.world, markerCls, session.controller, t);
    if (!marker) { LOG("[Mark] marker spawn failed"); return; }
    // Point the marker at our player state so the HUD renders our colour/icon.
    if (void* ps = GetObjField(session.controller, "/Script/Engine.Controller.PlayerState")) {
        const char* ownerPaths[] = {
            "/Script/FortniteGame.FortPlayerMarkerBase.TeamOwner",
            "/Script/FortniteGame.FortPlayerMarkerBase.MarkerOwner",
            nullptr
        };
        for (int i = 0; ownerPaths[i]; ++i)
            if (GetProp(ownerPaths[i])) { Write<void*>(GetField(marker, GetProp(ownerPaths[i])), 0, ps); break; }
    }
    LOG("[Mark] local marker spawned (%s)", ObjName(markerCls).c_str());
}

// [F33/F43] Minimap / full map — field fix: the first run spammed
// "[Map] InGameMapManager reflection missing" every call because the object
// paths used the C++ names ("AFortInGameMapManager") while 21.30's UObject
// names have no A-prefix ("FortInGameMapManager"), and every failure
// re-logged. Now: get the live instance via
// AFortGameStateZone.GetUIMapManager() (no path guessing), gate the log to
// one shot, and mark the session done either way.
void Athena::ForceMapsVisible() {
    if (!session.world || !session.state || session.maps_forced) return;
    Call gum("/Script/FortniteGame.FortGameStateZone.GetUIMapManager");
    void* mgr = nullptr;
    if (gum.ok() && gum.run(session.state)) mgr = gum.result<void*>();
    if (!mgr || IsBadPtr(mgr)) {
        if (!session.maps_logged_missing) {
            session.maps_logged_missing = true;
            LOG("[Map] UI map manager not available (GetUIMapManager failed) — HUD may still draw it via BP");
        }
        session.maps_forced = true;   // one attempt is enough — never spam
        return;
    }
    int shown = 0;
    Call mini("/Script/FortniteGame.FortInGameMapManager.AllowShowMiniMap");
    if (mini.ok()) { bool b = true; if (mini.set("bAllowShow", b) && mini.run(mgr)) ++shown; }
    Call full("/Script/FortniteGame.FortInGameMapManager.AllowShowMainMap");
    if (full.ok()) { bool b = true; if (full.set("bAllowShow", b) && full.run(mgr)) ++shown; }
    session.maps_forced = shown > 0;
    LOG("[Map] AllowShow calls applied=%d (mgr=%p)", shown, mgr);
}

// ============================================================================
// [F35] ZONE / STORM — AFortSafeZoneIndicator is the authentic zone driver:
// it holds the phase list, shrinks the circle and (server-side, which in
// standalone is us) damages pawns outside the safe radius. The map spawns
// its own phases via InitializeWithMapDefaultPhases(); we then start phase 0
// and monitor radius/phase for the log. era.zone <i> jumps to a phase.
// [F73] DriveSafeZone/ZoneMonitor (F35) removed — the Erbium BR lifecycle
// (BRSetupSafeZoneIndicator + StartNewSafeZonePhase + BRTick) owns the zone
// now; InitializeWithMapDefaultPhases could not create the phases array the
// game needs ("everything dead" symptom).

// ============================================================================
// [F36] PLAYER COUNTER — FortGameStateAthena.PlayersLeft @0x14B8 is what the
// HUD scoreboard reads. Without a server it stays 0; set it and fire
// OnRep_PlayersLeft so the mobile HUD shows the counter.
void Athena::SetPlayersLeft(int n) {
    if (!session.state) return;
    void* prop = GetProp("/Script/FortniteGame.FortGameStateAthena.PlayersLeft");
    if (!prop) { LOG("[Players] PlayersLeft property missing"); return; }
    int off = Read<int>(prop, profile::PropOffset);
    if (off != 0x14B8) LOG("[Players] ⚠️ offset 0x%X != expected 0x14B8", off);
    Write<int>(GetField(session.state, prop), 0, n);
    Call r("/Script/FortniteGame.FortGameStateAthena.OnRep_PlayersLeft");
    if (r.ok()) r.run(session.state);
    LOG("[Players] PlayersLeft=%d (offset 0x%X)", n, off);
}

// ============================================================================
// [F37] HARVEST RESOURCES — Erbium implements harvesting by granting the
// resource themselves inside their ABuildingSMActor::OnDamageServer hook;
// the native damage path already runs in-process, only the grant is missing.
// Hook-free equivalent: watch BuildingSMActor health deltas around the pawn
// while the pickaxe is equipped and grant resources proportional to damage.
// Yield model (Erbium): ResCount = CurveOut * Damage / MaxHealth, curve from
// BuildingResourceAmountOverride evaluated via DataTableFunctionLibrary.
// ============================================================================
// [F73] ERBIUM BATTLE ROYALE LIFECYCLE — full 1:1 port of the Erbium BR
// gameplay logic (github.com/plooshi/Erbium), the systems that make a match
// "alive" on a real server and that the standalone build was missing entirely:
//
//   BattleRoyaleGamePhaseLogic.cpp : HandleMatchHasStarted (warmup countdown),
//     Tick() phase machine (Warmup -> Aircraft -> SafeZones), StartAircraftPhase
//     (SpawnAircraft + flight windows + board players), SetupSafeZoneIndicator
//     (SafeZonePhases from MapInfo->SafeZoneDefinition), StartNewSafeZonePhase
//     (centers/radii/shrink times/damage + OnReps + OnSafeZoneStateChange),
//     zone shrink progression (state 3 -> StormShrinking -> next phase) and
//     bIsInsideSafeZone / bIsInAnyStorm updates.
//   FortGameMode.cpp               : SafeZonePhase bookkeeping
//     (HandlePostSafeZonePhaseChanged), AlivePlayers, WarmupCountdown*.
//   FortPlayerControllerAthena.cpp : the "spawnbot" routine (ErbiumBot roster:
//     PlayerPawn_Athena_C + Athena_PlayerController_C + FortPlayerStateAthena,
//     Possess + PlayerState wiring + TeamIndex/SquadId/bIsABot + ability sets
//     + PlayersLeft++ + AlivePlayers).
//
// On a normal client all of this runs on the server; the standalone device has
// no server, so the phases never advanced (everything stayed "dead"). We now
// run the exact same state machine locally, in the game's own world time.
// ============================================================================

// xorshift RNG (no libc rand() dependency)
static uint32_t BRRand() {
    static uint32_t s = 0x9E3779B9u;
    s ^= s << 13; s ^= s >> 17; s ^= s << 5;
    return s;
}
static float BRRand01() { return (BRRand() & 0xFFFFFF) / 16777216.0f; }

// UGameplayStatics::GetTimeSeconds — Erbium compares every timer against WORLD
// time (not monotonic): the native SafeZoneIndicator tick also uses these
// fields, so all BR windows must live in the same clock.
static float BRWorldTime() {
    static float cached = 0;
    static double at = 0;
    double now = Monotonic();
    if (now - at < 0.2) return cached;
    at = now;
    if (!session.world) return cached;
    void* cdo = FindObj("/Script/Engine.Default__GameplayStatics");
    Call c("/Script/Engine.GameplayStatics.GetTimeSeconds");
    if (c.ok()) {
        if (!c.set_any(session.world, "WorldContextObject", "World", "WorldContext")) return cached;
        if (c.run(cdo ? cdo : session.world)) cached = c.result<float>();
    }
    return cached;
}

// typed float read/write via reflection
static bool BRReadFloat(void* obj, const char* path, float& out) {
    void* p = obj ? GetProp(path) : nullptr;
    if (!p || Read<int>(p, profile::PropElementSize) != 4) return false;
    out = Read<float>(GetField(obj, p), 0);
    return true;
}
static bool BRWriteFloat(void* obj, const char* path, float v) {
    void* p = obj ? GetProp(path) : nullptr;
    if (!p || Read<int>(p, profile::PropElementSize) != 4) return false;
    Write<float>(GetField(obj, p), 0, v);
    return true;
}
static void BRWriteVector(void* obj, const char* path, const Vector& v) {
    void* p = obj ? GetProp(path) : nullptr;
    if (!p || Read<int>(p, profile::PropElementSize) != 24) return;
    Write<Vector>(GetField(obj, p), 0, v);
}
// [F74-c] typed object-pointer write via reflection (bus skin fields)
static void BRWritePtr(void* obj, const char* path, void* ptr) {
    void* p = obj ? GetProp(path) : nullptr;
    if (!p || Read<int>(p, profile::PropElementSize) != 8) return;
    Write<void*>(GetField(obj, p), 0, ptr);
}

// UFortGameStateComponent_BattleRoyaleGamePhaseLogic — [F74-f] does NOT exist
// in 21.30 (verified against the SDK dump: no such class in this build; the
// phase machinery lives on AFortGameStateAthena + FortGameModeAthena). The
// lookup stays for builds that ship it; on 21.30 every caller falls back to
// the GameState path — this is expected, not an error.
static void* BRGetComponent() {
    if (br.component && !IsBadPtr(br.component)) return br.component;
    if (!session.state) return nullptr;
    void* cls = FindObj("/Script/FortniteGame.FortGameStateComponent_BattleRoyaleGamePhaseLogic");
    if (!cls) { static bool once = false; if (!once) { once = true; LOG("[BR] no GamePhaseLogic component (expected on 21.30 — GameState path drives phases)"); } return nullptr; }
    Call c("/Script/Engine.Actor.GetComponentByClass");
    if (!c.ok()) return nullptr;
    if (!c.set_any(cls, "ComponentClass", "ActorClass", "Class")) { c.dump("GetComponentByClass"); return nullptr; }
    if (!c.run(session.state)) return nullptr;
    br.component = c.result<void*>();
    if (br.component && !IsBadPtr(br.component))
        LOG("[BR] GamePhaseLogic component = %p", br.component);
    return br.component;
}

// EAthenaGamePhase setter — Erbium SetGamePhase fallback path: write the
// property then fire OnRep_GamePhase(Old). We drive BOTH the component
// (game internals) and the GameState mirror (mobile HUD + [F69] HUD kick).
static void BRSetGamePhase(uint8_t p) {
    uint8_t old = br.phase;
    if (BRGetComponent()) {
        void* prop = GetProp("/Script/FortniteGame.FortGameStateComponent_BattleRoyaleGamePhaseLogic.GamePhase");
        if (prop) Write<uint8_t>(GetField(br.component, prop), 0, p);
        Call r("/Script/FortniteGame.FortGameStateComponent_BattleRoyaleGamePhaseLogic.OnRep_GamePhase");
        if (r.ok()) { if (!r.set("OldGamePhase", old)) r.set_at(0, old); r.run(br.component); }
    }
    if (session.state) {
        void* gp = GetProp("/Script/FortniteGame.FortGameStateAthena.GamePhase");
        if (gp) Write<uint8_t>(GetField(session.state, gp), 0, p);
        Call n("/Script/FortniteGame.FortGameStateAthena.OnRep_GamePhase");
        if (n.ok()) { if (!n.set("OldGamePhase", old)) n.set_at(0, old); n.run(session.state); }
    }
    br.phase = p;
    LOG("[BR] GamePhase -> %u (Erbium EAthenaGamePhase)", (unsigned)p);
}

// EAthenaGamePhaseStep setter — Erbium SetGamePhaseStep: property write +
// HandleGamePhaseStepChanged (the native that notifies HUD/music/announcer).
// [F74-f] 21.30 has NO FortGameStateComponent_BattleRoyaleGamePhaseLogic
// (verified in the SDK dump — the class doesn't exist in this build). The
// step broadcast lives on AFortGameStateAthena.GamePhaseStepChanged and its
// registered handlers: FortGameModeAthena.OnGamePhaseStepChanged +
// FortPlayerPawnAthena.GamePhaseStepChanged (both take
// {TScriptInterface<IFortSafeZoneInterface>, EEAthenaGamePhaseStep}).
// [F76-a] CRASH FIX — the F75 device log proved that calling the GameMode
// handler directly kills the process seconds later (its native continuation
// is the whole server-side aircraft transition). The set is now:
//   1) write GameState.GamePhaseStep (replicated field — every reader),
//   2) BROADCAST the AFortGameStateAthena.GamePhaseStepChanged multicast via
//      OnGamePhaseStepChanged__DelegateSignature (0x70fb023b60 — params
//      SafeZoneInterface@0x0 + GamePhaseStep@0x10): every CLIENT binding
//      (PC.HandleGamePhaseStepChanged, widgets, music) fires natively while
//      the server-side GameMode binding is swallowed in pe_hook, exactly
//      like Erbium where the client never runs the server transition,
//   3) Pawn.GamePhaseStepChanged (client pawn visuals) as before.
static void BRBroadcastStepChanged(uint8_t s) {
    if (!session.state || !orig_PE) return;
    void* sig = FindObj("/Script/FortniteGame.OnGamePhaseStepChanged__DelegateSignature");
    if (!sig) {
        static bool once = false;
        if (!once) { once = true; LOG("[BR] GamePhaseStepChanged delegate signature missing — visuals skipped"); }
        return;
    }
    uint8_t params[32];
    memset(params, 0, sizeof(params));
    uint64_t iface[2] = { (uint64_t)(uintptr_t)(br.indicator && !IsBadPtr(br.indicator) ? br.indicator : nullptr),
                          (uint64_t)(uintptr_t)(br.indicator && !IsBadPtr(br.indicator) ? br.indicator : nullptr) };
    memcpy(params, iface, sizeof(iface));
    params[0x10] = s;
    orig_PE(session.state, sig, params);
}
static void BRSetGamePhaseStep(uint8_t s) {
    br.step = s;
    if (BRGetComponent()) {
        void* prop = GetProp("/Script/FortniteGame.FortGameStateComponent_BattleRoyaleGamePhaseLogic.GamePhaseStep");
        if (prop) Write<uint8_t>(GetField(br.component, prop), 0, s);
        Call h("/Script/FortniteGame.FortGameStateComponent_BattleRoyaleGamePhaseLogic.HandleGamePhaseStepChanged");
        if (h.ok()) { if (!h.set_any(s, "NewGamePhaseStep", "InGamePhaseStep", "GamePhaseStep")) h.set_at(0, s); h.run(br.component); }
    }
    if (session.state) {
        void* prop = GetProp("/Script/FortniteGame.FortGameStateAthena.GamePhaseStep");
        if (prop) Write<uint8_t>(GetField(session.state, prop), 0, s);
    }
    BRBroadcastStepChanged(s);
    if (!IsBadPtr(session.pawn)) {
        uint64_t iface[2] = {(uint64_t)(uintptr_t)(br.indicator && !IsBadPtr(br.indicator) ? br.indicator : nullptr),
                             (uint64_t)(uintptr_t)(br.indicator && !IsBadPtr(br.indicator) ? br.indicator : nullptr)};
        Call pp("/Script/FortniteGame.FortPlayerPawnAthena.GamePhaseStepChanged");
        if (pp.ok() && session.pawn &&
            pp.set_bytes("SafeZoneInterface", iface, 16) && pp.set("GamePhaseStep", s))
            pp.run(session.pawn);
    }
    LOG("[BR] GamePhaseStep -> %u", (unsigned)s);
}

// ----------------------------------------------------------------------------
// [F73-a] Zone tables — Erbium evaluates MapInfo->SafeZoneDefinition curves
// (Radius/WaitTime/ShrinkTime ScalableFloats) + "Default.SafeZone.Damage".
// FName construction is not available in reflection-land, so the curve rows
// are replaced by the published BR pacing table; SafeZoneDefinition.Count is
// still honoured when the map ships a literal Value (>0).
// ----------------------------------------------------------------------------
static void BRZoneTables() {
    static const float kRadius[9] = {25600, 19800, 14600, 10600, 7600, 5400, 3600, 2200, 1000};
    static const float kWait[9]   = {50, 40, 35, 30, 25, 20, 20, 15, 10};
    static const float kShrink[9] = {40, 35, 32, 28, 25, 21, 17, 13, 10};
    static const float kDamage[9] = {1, 1, 1, 2, 2, 5, 6, 8, 10}; // % of max health
    int count = 9;
    if (br.mapinfo) {
        void* outer = GetProp("/Script/FortniteGame.FortAthenaMapInfo.SafeZoneDefinition");
        void* inner = GetProp("/Script/FortniteGame.FortSafeZoneDefinition.Count");
        if (outer && inner) {
            int so = Read<int>(outer, profile::PropOffset);
            int io = Read<int>(inner, profile::PropOffset);
            if (so > 0 && io > 0 && so + io < 0x8000) {
                float v = Read<float>((char*)br.mapinfo + so + io, 0);
                if (v >= 4.0f && v <= 12.0f) count = (int)v;
            }
        }
    }
    br.zone_count = count;
    br.radius.assign(kRadius, kRadius + 9);
    br.wait.assign(kWait, kWait + 9);
    br.shrink.assign(kShrink, kShrink + 9);
    br.damage.assign(kDamage, kDamage + 9);
    if (count > 9) { // pad with the last ring values
        br.radius.resize(count, kRadius[8]); br.wait.resize(count, kWait[8]);
        br.shrink.resize(count, kShrink[8]); br.damage.resize(count, kDamage[8]);
    }
    LOG("[BR] zone tables: count=%d r0=%.0f wait0=%.0f shrink0=%.0f", count, br.radius[0], br.wait[0], br.shrink[0]);
}

// ----------------------------------------------------------------------------
// [F73-b] Zone centers — Erbium InitializeSafeZoneLocations uses
// MapInfo->ConstructSafeZoneLocationParams + PickSafeZoneLocation (POI picker
// + blacklist curve). The picker needs FName/blacklist plumbing reflection
// cannot build, so we use the same shrink-in invariant the native picker
// enforces: circle i is randomly placed fully inside circle i-1 around the
// map center.
// ----------------------------------------------------------------------------
static void BRPickZoneCenters() {
    br.zone_centers.clear();
    Vector center{0, 0, 0};
    if (br.mapinfo) {
        Call c("/Script/FortniteGame.FortAthenaMapInfo.GetMapCenter");
        if (c.ok() && c.run(br.mapinfo)) center = c.result<Vector>();
    }
    if (!std::isfinite(center.X) || !std::isfinite(center.Y)) center = Vector{0, 0, 0};
    br.zone_centers.push_back(center);
    for (int i = 1; i < br.zone_count; ++i) {
        float gap = br.radius[i - 1] - br.radius[i];
        if (gap < 500) gap = 500;
        float dist = gap * (0.20f + 0.50f * BRRand01());
        float ang = BRRand01() * 6.2831853f;
        Vector c{ center.X + std::cos(ang) * dist, center.Y + std::sin(ang) * dist, center.Z };
        br.zone_centers.push_back(c);
    }
    LOG("[BR] zone centers picked (%d) around (%.0f, %.0f)", br.zone_count, center.X, center.Y);
}

// FArrayProperty -> validated inner element size (ElementSize lives on the
// inner property; the FArrayProperty header size differs between dumps, so
// probe both candidates and validate the name of what we find).
static int BRInnerSize(void* arrayProp) {
    if (!arrayProp) return 0;
    static const unsigned candidates[] = {0x50, 0x48, 0x58};
    for (unsigned off : candidates) {
        void* inner = Read<void*>(arrayProp, off);
        if (!inner || IsBadPtr(inner)) continue;
        void* innerCls = Read<void*>(inner, profile::ObjectClass);
        std::string cn = (innerCls && !IsBadPtr(innerCls)) ? ObjName(innerCls) : "";
        if (cn != "StructProperty" && cn != "Struct") continue;
        int esz = Read<int>(inner, profile::PropElementSize);
        if (esz >= 0x40 && esz <= 0x800) return esz;
    }
    return 0;
}

// ----------------------------------------------------------------------------
// [F73-c] Erbium SetupSafeZoneIndicator — spawn the REAL indicator, build the
// SafeZonePhases array (Radius/Wait/Shrink/PlayerCap/Center/DamageInfo per
// phase), fire PhaseCount/CurrentPhase OnReps and mirror it onto GameState +
// GameMode + component exactly like the server flow does.
// ----------------------------------------------------------------------------
static void BRSetupSafeZoneIndicator() {
    if (br.indicator && !IsBadPtr(br.indicator)) return;
    if (!session.state || !session.world) return;

    void* ind = GetObjField(session.state, "/Script/FortniteGame.FortGameStateAthena.SafeZoneIndicator");
    bool reuse = ind && !IsBadPtr(ind);
    if (!reuse) {
        void* cls = nullptr;
        void* clsProp = GetProp("/Script/FortniteGame.FortGameMode.SafeZoneIndicatorClass");
        if (clsProp && session.mode) {
            void* c = Read<void*>(GetField(session.mode, clsProp), 0);
            if (c && !IsBadPtr(c)) cls = c;
        }
        if (!cls) cls = FindObj("/Script/FortniteGame.FortSafeZoneIndicator");
        if (!cls) { LOG("[BR] SafeZoneIndicator class missing"); return; }
        Transform t{};
        ind = SpawnActor(session.world, cls, session.state, t);
        if (!ind) { LOG("[BR] indicator spawn FAILED"); return; }
    }
    br.indicator = ind;

    void* arrProp = GetProp("/Script/FortniteGame.FortSafeZoneIndicator.SafeZonePhases");
    int esz = BRInnerSize(arrProp);
    if (!arrProp || !esz) { LOG("[BR] SafeZonePhases array missing (prop=%p esz=%d)", arrProp, esz); return; }

    // FortSafeZonePhaseInfo field offsets
    auto fo = [](const char* n) -> int {
        void* p = GetProp((std::string("/Script/FortniteGame.FortSafeZonePhaseInfo.") + n).c_str());
        return p ? Read<int>(p, profile::PropOffset) : -1;
    };
    int oRadius = fo("Radius"), oWait = fo("WaitTime"), oShrink = fo("ShrinkTime"),
        oCap = fo("PlayerCap"), oCenter = fo("Center"), oDmg = fo("DamageInfo");
    int oDmgD = -1, oDmgP = -1;
    if (void* dp = GetProp("/Script/FortniteGame.FortSafeZoneDamageInfo.Damage"))
        oDmgD = Read<int>(dp, profile::PropOffset);
    if (void* dp = GetProp("/Script/FortniteGame.FortSafeZoneDamageInfo.bPercentageBasedDamage"))
        oDmgP = Read<int>(dp, profile::PropOffset);
    if (oRadius < 0 || oWait < 0 || oShrink < 0 || oCenter < 0) {
        LOG("[BR] phase fields missing r=%d w=%d s=%d c=%d", oRadius, oWait, oShrink, oCenter);
        return;
    }
    LOG("[BR] phase struct esz=0x%X r=0x%X w=0x%X s=0x%X ctr=0x%X dmg=0x%X(+%X/%X)",
        esz, oRadius, oWait, oShrink, oCenter, oDmg, oDmgD, oDmgP);

    std::vector<uint8_t> elem((size_t)esz, 0);
    for (int i = 0; i < br.zone_count; ++i) {
        std::fill(elem.begin(), elem.end(), (uint8_t)0);
        Write<float>(elem.data(), oRadius, br.radius[i]);
        Write<float>(elem.data(), oWait, br.wait[i]);
        Write<float>(elem.data(), oShrink, br.shrink[i]);
        if (oCap >= 0 && oCap + 4 <= esz) Write<int>(elem.data(), oCap, 100);
        if (oCenter + 24 <= esz) Write<Vector>(elem.data(), oCenter, br.zone_centers[i]);
        if (oDmg >= 0 && oDmgD >= 0 && oDmg + oDmgD + 4 <= esz)
            Write<float>(elem.data(), oDmg + oDmgD, br.damage[i]);
        if (oDmg >= 0 && oDmgP >= 0 && oDmg + oDmgP < esz)
            Write<uint8_t>(elem.data(), oDmg + oDmgP, 1); // bPercentageBasedDamage
        Fn<int(*)(void*, void*, const void*)>(profile::GenericArrayAdd)(GetField(ind, arrProp), arrProp, elem.data());
    }

    void* pcProp = GetProp("/Script/FortniteGame.FortSafeZoneIndicator.PhaseCount");
    if (pcProp) Write<int>(GetField(ind, pcProp), 0, br.zone_count);
    Call rpc("/Script/FortniteGame.FortSafeZoneIndicator.OnRep_PhaseCount");
    if (rpc.ok()) rpc.run(ind);

    // Erbium tail: phase-0 window + CurrentPhase = 0 (+ OnRep)
    float now = BRWorldTime();
    BRWriteFloat(ind, "/Script/FortniteGame.FortSafeZoneIndicator.SafeZoneStartShrinkTime", now + br.wait[0]);
    BRWriteFloat(ind, "/Script/FortniteGame.FortSafeZoneIndicator.SafeZoneFinishShrinkTime", now + br.wait[0] + br.shrink[0]);
    void* cpProp = GetProp("/Script/FortniteGame.FortSafeZoneIndicator.CurrentPhase");
    if (cpProp) Write<int>(GetField(ind, cpProp), 0, 0);
    Call rcp("/Script/FortniteGame.FortSafeZoneIndicator.OnRep_CurrentPhase");
    if (rcp.ok()) rcp.run(ind);

    if (!reuse) {
        // GameState->SafeZoneIndicator + OnRep (what the HUD reads)
        void* stProp = GetProp("/Script/FortniteGame.FortGameStateAthena.SafeZoneIndicator");
        if (stProp) Write<void*>(GetField(session.state, stProp), 0, ind);
        Call rs("/Script/FortniteGame.FortGameStateAthena.OnRep_SafeZoneIndicator");
        if (rs.ok()) rs.run(session.state);
        // GameMode->SafeZoneIndicator (native storm/zone paths read it)
        void* gmProp = GetProp("/Script/FortniteGame.FortGameMode.SafeZoneIndicator");
        if (gmProp && session.mode) Write<void*>(GetField(session.mode, gmProp), 0, ind);
        // component->SafeZoneIndicator
        if (BRGetComponent()) {
            void* coProp = GetProp("/Script/FortniteGame.FortGameStateComponent_BattleRoyaleGamePhaseLogic.SafeZoneIndicator");
            if (coProp) Write<void*>(GetField(br.component, coProp), 0, ind);
        }
        SetBool(session.mode, "/Script/FortniteGame.FortGameMode.bSafeZoneActive", true);
        SetBool(br.component, "/Script/FortniteGame.FortGameStateComponent_BattleRoyaleGamePhaseLogic.bSafeZoneActive", true);
    }
    SetBool(session.state, "/Script/FortniteGame.FortGameStateAthena.bPlaylistStoppedSafeZonePhases", false);
    LOG("[BR] SafeZoneIndicator %s phases=%d (%p)", reuse ? "reused" : "spawned", br.zone_count, ind);
}

// ----------------------------------------------------------------------------
// [F73-d] Erbium StartNewSafeZonePhase — sets Previous/Next/NextNext centers +
// radii, the shrink window, CurrentDamageInfo, CurrentPhase (+ OnReps), the
// SafeZoneState byte (offset = FutureReplicator - 4) to 2, OnSafeZoneStateChange
// and StormHolding; then the GameMode's own HandlePostSafeZonePhaseChanged
// bookkeeping (Erbium reaches it through the OnSafeZonePhaseChanged delegate
// bound to the GameMode — the direct call is the same effect).
// ----------------------------------------------------------------------------
static void BRStartNewSafeZonePhase(int p, bool bInitial) {
    if (!br.indicator || IsBadPtr(br.indicator) || p < 0 || p >= br.zone_count) return;
    float now = BRWorldTime();

    // GameMode bookkeeping FIRST (Erbium runs the OG hook before re-writing
    // the windows — same order here).
    Call hpc("/Script/FortniteGame.FortGameMode.HandlePostSafeZonePhaseChanged");
    if (hpc.ok() && session.mode && !IsBadPtr(session.mode)) {
        if (!hpc.set_any((int)-1, "NewSafeZonePhase", "InSafeZonePhase", "NewPhase")) hpc.set_at(0, -1);
        hpc.run(session.mode);
    }
    void* szp = GetProp("/Script/FortniteGame.FortGameMode.SafeZonePhase");
    if (szp && session.mode) Write<int>(GetField(session.mode, szp), 0, p);

    if (p - 1 >= 0) {
        BRWriteVector(br.indicator, "/Script/FortniteGame.FortSafeZoneIndicator.PreviousCenter", br.zone_centers[p - 1]);
        BRWriteFloat(br.indicator, "/Script/FortniteGame.FortSafeZoneIndicator.PreviousRadius", br.radius[p - 1]);
    }
    BRWriteVector(br.indicator, "/Script/FortniteGame.FortSafeZoneIndicator.NextCenter", br.zone_centers[p]);
    BRWriteFloat(br.indicator, "/Script/FortniteGame.FortSafeZoneIndicator.NextRadius", br.radius[p]);
    if (p + 1 < br.zone_count) {
        BRWriteVector(br.indicator, "/Script/FortniteGame.FortSafeZoneIndicator.NextNextCenter", br.zone_centers[p + 1]);
        BRWriteFloat(br.indicator, "/Script/FortniteGame.FortSafeZoneIndicator.NextNextRadius", br.radius[p + 1]);
        if (void* fr = GetObjField(br.indicator, "/Script/FortniteGame.FortSafeZoneIndicator.FutureReplicator");
            fr && !IsBadPtr(fr)) {
            BRWriteVector(fr, "/Script/FortniteGame.FortSafeZoneIndicatorFuture.NextNextCenter", br.zone_centers[p + 1]);
            BRWriteFloat(fr, "/Script/FortniteGame.FortSafeZoneIndicatorFuture.NextNextRadius", br.radius[p + 1]);
        }
    }
    float start = now + br.wait[p];
    BRWriteFloat(br.indicator, "/Script/FortniteGame.FortSafeZoneIndicator.SafeZoneStartShrinkTime", start);
    BRWriteFloat(br.indicator, "/Script/FortniteGame.FortSafeZoneIndicator.SafeZoneFinishShrinkTime", start + br.shrink[p]);

    // CurrentDamageInfo = { damage[p], percentage=true }
    if (void* diProp = GetProp("/Script/FortniteGame.FortSafeZoneIndicator.CurrentDamageInfo")) {
        int diOff = Read<int>(diProp, profile::PropOffset);
        int oDmgD = -1, oDmgP = -1;
        if (void* dp = GetProp("/Script/FortniteGame.FortSafeZoneDamageInfo.Damage"))
            oDmgD = Read<int>(dp, profile::PropOffset);
        if (void* dp = GetProp("/Script/FortniteGame.FortSafeZoneDamageInfo.bPercentageBasedDamage"))
            oDmgP = Read<int>(dp, profile::PropOffset);
        if (oDmgD >= 0) Write<float>((char*)br.indicator + diOff + oDmgD, 0, br.damage[p]);
        if (oDmgP >= 0) Write<uint8_t>((char*)br.indicator + diOff + oDmgP, 0, 1);
    }
    Call rdi("/Script/FortniteGame.FortSafeZoneIndicator.OnRep_CurrentDamageInfo");
    if (rdi.ok()) rdi.run(br.indicator);

    void* cpProp = GetProp("/Script/FortniteGame.FortSafeZoneIndicator.CurrentPhase");
    if (cpProp) Write<int>(GetField(br.indicator, cpProp), 0, p);
    Call rcp("/Script/FortniteGame.FortSafeZoneIndicator.OnRep_CurrentPhase");
    if (rcp.ok()) rcp.run(br.indicator);

    // SafeZoneState byte = 2 (storm formed; Erbium writes &FutureReplicator-4)
    if (void* frProp = GetProp("/Script/FortniteGame.FortSafeZoneIndicator.FutureReplicator")) {
        int fro = Read<int>(frProp, profile::PropOffset);
        if (fro > 4 && fro < 0x8000) Write<uint8_t>(br.indicator, fro - 4, 2);
    }
    Call osc("/Script/FortniteGame.FortSafeZoneIndicator.OnSafeZoneStateChange");
    if (osc.ok()) {
        if (!osc.set_at(0, (uint8_t)2)) osc.dump("OnSafeZoneStateChange");
        osc.set_at(1, (uint8_t)bInitial ? (uint8_t)1 : (uint8_t)0);
        osc.run(br.indicator);
    }
    BRSetGamePhaseStep(7); // EAthenaGamePhaseStep::StormHolding
    br.last_logged_zone = -1; // force a Zone log line next tick
    LOG("[BR] zone phase %d r=%.0f ctr=(%.0f,%.0f) wait=%.0f shrink=%.0f dmg=%.1f%%",
        p, br.radius[p], br.zone_centers[p].X, br.zone_centers[p].Y, br.wait[p], br.shrink[p], br.damage[p]);
}

// ----------------------------------------------------------------------------
// [F73-e/F74] Erbium StartAircraftPhase — 21.30 correction: SpawnAircraft is
// a STATIC on AFortAthenaAircraft (UFunction verified in the 21.30 dump:
// World + AircraftClass + FAircraftFlightInfo InFlightInfo@0x10 0x48 bytes;
// the old FortGameStateAthena.SpawnAircraft path never existed in this build).
// [F74-c] Bus skin: DefaultBattleBus (GameState) + DefaultBusSkin/ActiveSkin
// (aircraft/cosmetic actor) from BBID_DefaultBus — Erbium FortGameMode.cpp
// sets these right after the aircraft exists.
// [F74] every step logs BEFORE running so any residual crash names itself.
// ----------------------------------------------------------------------------
static void BRStartAircraftPhase() {
    if (br.phase >= 3) return;
    if (!session.world || !session.state) return;
    float now = BRWorldTime();
    LOG("[BR] StartAircraftPhase (Erbium) t=%.0f", now);

    void* acCls = br.mapinfo ? GetObjField(br.mapinfo, "/Script/FortniteGame.FortAthenaMapInfo.AircraftClass") : nullptr;
    if (!acCls || IsBadPtr(acCls)) acCls = FindObj("/Script/FortniteGame.FortAthenaAircraft");
    LOG("[BR] aircraft class=%s", acCls && !IsBadPtr(acCls) ? ObjName(acCls).c_str() : "<null>");

    // FlightInfos[0] element bytes (same struct the SpawnAircraft param takes)
    void* arrProp = br.mapinfo ? GetProp("/Script/FortniteGame.FortAthenaMapInfo.FlightInfos") : nullptr;
    Array* fi = arrProp ? (Array*)GetField(br.mapinfo, arrProp) : nullptr;
    int fiElem = BRInnerSize(arrProp);
    bool haveFI = fi && fi->Count > 0 && fi->Data && !IsBadPtr(fi->Data) && fiElem > 0;
    if (haveFI) {
        auto fif = [&](const char* n, float& out) {
            void* p = GetProp((std::string("/Script/FortniteGame.FortAthenaAircraftFlightInfo.") + n).c_str());
            if (!p) p = GetProp((std::string("/Script/FortniteGame.AthenaAircraftFlightInfo.") + n).c_str());
            if (!p) return false;
            int off = Read<int>(p, profile::PropOffset);
            if (off < 0 || off + 4 > fiElem) return false;
            out = Read<float>(fi->Data, off);
            return true;
        };
        float speed = 0;
        bool haveSpeed = fif("FlightSpeed", speed); // map flight speed (logged)
        float tfe = 0, tds = 0, tde = 0;
        fif("TimeTillFlightEnd", tfe); fif("TimeTillDropStart", tds); fif("TimeTillDropEnd", tde);
        if (tfe > 1 && tde > tds) {
            br.drop_start = now + tds;
            br.drop_end = now + tde;
            br.flight_end = now + tfe;
        }
        LOG("[BR] FlightInfos[0]: speed=%.0f (ok=%d) drop %.0f->%.0f flight %.0f",
            speed, (int)haveSpeed, tds, tde, tfe);
    }

    void* aircraft = nullptr;
    // [F75-a] adopt-first — the native warmup-end transition may have already
    // spawned the aircraft at this exact tick (it fires at the same moment we
    // do). GameState.Aircraft/Aircrafts[0] non-null means the real native bus
    // exists: adopt it (Erbium 1:1 — the real server always has exactly one)
    // instead of spawning a duplicate.
    if (void* ac1Prop = GetProp("/Script/FortniteGame.FortGameStateAthena.Aircraft")) {
        void* existing = Read<void*>(GetField(session.state, ac1Prop), 0);
        if (existing && !IsBadPtr(existing)) {
            aircraft = existing;
            LOG("[BR] native aircraft already present — adopting %p (no duplicate spawn)", existing);
        }
    }
    if (!aircraft && acCls && !IsBadPtr(acCls)) {
        // [F74-c] resolve the bus skin BEFORE the spawn — the native
        // SpawnAircraft reads GameState->DefaultBattleBus while building the
        // cosmetic bus actor (Erbium sets it in HandleMatchHasStarted, i.e.
        // before any aircraft exists).
        void* busDef = LoadAsset("/Game/Athena/Items/Cosmetics/BattleBuses/BBID_DefaultBus.BBID_DefaultBus");
        if (busDef) BRWritePtr(session.state, "/Script/FortniteGame.FortGameStateAthena.DefaultBattleBus", busDef);
        else LOG("[BR] BBID_DefaultBus not resident — bus renders default");
        if (haveFI) {
            // [F74-b] The REAL 21.30 spawn path — static UFunction on the
            // aircraft class, run on its CDO (Erbium:
            // AFortAthenaAircraft::SpawnAircraft(World, Class, FlightInfo)).
            Call sp("/Script/FortniteGame.FortAthenaAircraft.SpawnAircraft");
            if (sp.ok()) {
                LOG("[BR] SpawnAircraft (static) elem=%d", fiElem);
                bool w = sp.set_any(session.world, "World", "WorldContextObject", "InWorld");
                bool k = sp.set_any(acCls, "InAircraftClass", "AircraftClass", "Aircraft");
                bool f = sp.set_bytes("InFlightInfo", fi->Data, fiElem) ||
                         sp.set_bytes("FlightInfo", fi->Data, fiElem);
                void* cdo = FindObj("/Script/FortniteGame.Default__FortAthenaAircraft");
                if (w && k && f && sp.run(cdo ? cdo : session.state)) aircraft = sp.result<void*>();
                LOG("[BR] SpawnAircraft -> %p (w=%d k=%d f=%d)", aircraft, (int)w, (int)k, (int)f);
                if (!aircraft) sp.dump("SpawnAircraft");
            } else {
                LOG("[BR] static SpawnAircraft UFunction not found — manual fallback");
            }
        }
        if (!aircraft) {
            // Manual fallback — the real class, empty transform (its own Tick
            // interpolates FlightInfo fields we write next).
            LOG("[BR] manual aircraft spawn...");
            Transform t{};
            aircraft = SpawnActor(session.world, acCls, session.state, t);
            LOG("[BR] manual aircraft -> %p", aircraft);
            if (aircraft)
                SetBool(aircraft, "/Script/FortniteGame.FortAthenaAircraft.bWasSpawnedForAircraftPhase", true);
        }
    }
    if (!aircraft) LOG("[BR] aircraft spawn FAILED (cls=%p)", acCls);
    else {
        br.aircraft = aircraft;
        // Flight window props (Erbium sets these after SpawnAircraft)
        if (br.drop_start < 0) { br.drop_start = now + 5; br.drop_end = now + 50; br.flight_end = now + 55; }
        // Flight path data: copy MapInfo->FlightInfos[0] into the aircraft's
        // own FlightInfo struct AND the individual fields (Erbium writes the
        // field variant; 21.30 aircraft carries both) so the native aircraft
        // Tick flies the real route.
        LOG("[BR] writing flight path fields...");
        if (haveFI) {
            if (void* fiProp = GetProp("/Script/FortniteGame.FortAthenaAircraft.FlightInfo")) {
                int psz = Read<int>(fiProp, profile::PropElementSize);
                if (psz > 0 && psz <= 0x400)
                    memcpy(GetField(aircraft, fiProp), fi->Data, (size_t)(psz < fiElem ? psz : fiElem));
            }
            Vector startLoc{};
            if (void* lp = GetProp("/Script/FortniteGame.FortAthenaAircraftFlightInfo.FlightStartLocation")) {
                int off = Read<int>(lp, profile::PropOffset);
                if (off >= 0 && off + 24 <= fiElem) startLoc = Read<Vector>(fi->Data, off);
            }
            float spd = 1500, tfe = 0, tds = 0, tde = 0;
            { void* p = GetProp("/Script/FortniteGame.FortAthenaAircraftFlightInfo.FlightSpeed");
              if (p) { int off = Read<int>(p, profile::PropOffset);
                       if (off >= 0 && off + 4 <= fiElem) spd = Read<float>(fi->Data, off); } }
            { void* p = GetProp("/Script/FortniteGame.FortAthenaAircraftFlightInfo.TimeTillFlightEnd");
              if (p) { int off = Read<int>(p, profile::PropOffset);
                       if (off >= 0 && off + 4 <= fiElem) tfe = Read<float>(fi->Data, off); } }
            { void* p = GetProp("/Script/FortniteGame.FortAthenaAircraftFlightInfo.TimeTillDropStart");
              if (p) { int off = Read<int>(p, profile::PropOffset);
                       if (off >= 0 && off + 4 <= fiElem) tds = Read<float>(fi->Data, off); } }
            { void* p = GetProp("/Script/FortniteGame.FortAthenaAircraftFlightInfo.TimeTillDropEnd");
              if (p) { int off = Read<int>(p, profile::PropOffset);
                       if (off >= 0 && off + 4 <= fiElem) tde = Read<float>(fi->Data, off); } }
            BRWriteFloat(aircraft, "/Script/FortniteGame.FortAthenaAircraft.FlightSpeed", spd);
            BRWriteVector(aircraft, "/Script/FortniteGame.FortAthenaAircraft.FlightStartLocation", startLoc);
            if (tfe > 0) BRWriteFloat(aircraft, "/Script/FortniteGame.FortAthenaAircraft.TimeTillFlightEnd", tfe);
            if (tde > 0) BRWriteFloat(aircraft, "/Script/FortniteGame.FortAthenaAircraft.TimeTillDropEnd", tde);
            BRWriteFloat(aircraft, "/Script/FortniteGame.FortAthenaAircraft.TimeTillDropStart", tds);
            LOG("[BR] flight path: start=(%.0f,%.0f,%.0f) speed=%.0f", startLoc.X, startLoc.Y, startLoc.Z, spd);
        }
        BRWriteFloat(aircraft, "/Script/FortniteGame.FortAthenaAircraft.FlightElapsedTime", 0);
        BRWriteFloat(aircraft, "/Script/FortniteGame.FortAthenaAircraft.DropStartTime", br.drop_start);
        BRWriteFloat(aircraft, "/Script/FortniteGame.FortAthenaAircraft.DropEndTime", br.drop_end);
        BRWriteFloat(aircraft, "/Script/FortniteGame.FortAthenaAircraft.FlightStartTime", now);
        BRWriteFloat(aircraft, "/Script/FortniteGame.FortAthenaAircraft.FlightEndTime", br.flight_end);
        BRWriteFloat(aircraft, "/Script/FortniteGame.FortAthenaAircraft.ReplicatedFlightTimestamp", now);

        // [F74-c] BattleBus skin — post-spawn half (Erbium FortGameMode.cpp
        // :1043-1109): DefaultBusSkin on the aircraft + ActiveSkin on the
        // cosmetic bus actor (DefaultBattleBus was set pre-spawn above).
        LOG("[BR] applying bus skin...");
        void* busDef2 = LoadAsset("/Game/Athena/Items/Cosmetics/BattleBuses/BBID_DefaultBus.BBID_DefaultBus");
        if (busDef2) {
            BRWritePtr(aircraft, "/Script/FortniteGame.FortAthenaAircraft.DefaultBusSkin", busDef2);
            void* sca = GetObjField(aircraft, "/Script/FortniteGame.FortAthenaAircraft.SpawnedCosmeticActor");
            if (sca && !IsBadPtr(sca)) {
                BRWritePtr(sca, "/Script/FortniteGame.BattleBusCosmeticInstanceBase.ActiveSkin", busDef2);
                LOG("[BR] bus cosmetic actor skin set");
            }
        }

        // GameState->Aircrafts + OnRep (HUD bus panel + native aircraft logic)
        LOG("[BR] registering aircraft in GameState...");
        void* acsProp = GetProp("/Script/FortniteGame.FortGameStateAthena.Aircrafts");
        if (acsProp) {
            Fn<int(*)(void*, void*, const void*)>(profile::GenericArrayAdd)(GetField(session.state, acsProp), acsProp, &aircraft);
            Call ra("/Script/FortniteGame.FortGameStateAthena.OnRep_Aircrafts");
            if (ra.ok()) ra.run(session.state);
        }
        void* ac1 = GetProp("/Script/FortniteGame.FortGameStateAthena.Aircraft");
        if (ac1) Write<void*>(GetField(session.state, ac1), 0, aircraft);
        SetBool(session.state, "/Script/FortniteGame.FortGameStateAthena.bAircraftIsLocked", true);
        if (BRGetComponent())
            SetBool(br.component, "/Script/FortniteGame.FortGameStateComponent_BattleRoyaleGamePhaseLogic.bAircraftIsLocked", true);
        LOG("[BR] aircraft %p class=%s drop=%.0f..%.0f flightEnd=%.0f", aircraft,
            ObjName(Read<void*>(aircraft, profile::ObjectClass)).c_str(),
            br.drop_start - now, br.drop_end - now, br.flight_end - now);
    }

    // Board the player (Erbium destroys pawns + ClientGotoState Spectating;
    // standalone-safe equivalent: hide pawn + no collision + camera on bus).
    if (session.pawn && !IsBadPtr(session.pawn)) {
        LOG("[BR] boarding player...");
        Call hid("/Script/Engine.Actor.SetActorHiddenInGame");
        if (hid.ok()) { bool b = true; if (hid.set("bNewHidden", b)) hid.run(session.pawn); }
        Call col("/Script/Engine.Actor.SetActorEnableCollision");
        if (col.ok()) { bool b = false; if (col.set("bNewActorEnableCollision", b)) col.run(session.pawn); }
        if (aircraft) {
            Call cam("/Script/Engine.PlayerController.SetViewTargetWithBlend");
            if (cam.ok()) { cam.set("NewViewTarget", aircraft); float blend = 0; cam.set("BlendTime", blend); cam.run(session.controller); }
        }
        LOG("[BR] player boarded (pawn hidden, camera on bus)");
    }

    // Erbium StartAircraftPhase tail: ClientActivateSlot(0,0) re-arms the
    // pickaxe for landing. The [F74-a] swallow makes this safe standalone.
    if (session.controller) {
        Call cas("/Script/FortniteGame.FortPlayerController.ClientActivateSlot");
        if (cas.ok()) {
            uint8_t bar = 0; int slot = 0; float delay = 0;
            bool upd = true, force = true;
            if (cas.set("InQuickBar", bar) && cas.set("Slot", slot) && cas.set("ActivateDelay", delay) &&
                cas.set("bUpdatePreviousFocusedSlot", upd) && cas.set("bForceExecution", force))
                cas.run(session.controller);
        }
    }

    BRSetGamePhase(3);     // EAthenaGamePhase::Aircraft
    BRSetGamePhaseStep(4); // EAthenaGamePhaseStep::BusLocked
    LOG("[BR] StartAircraftPhase complete");
}

// ----------------------------------------------------------------------------
// [F73-f] Jump from the bus — Erbium's ServerAttemptAircraftJump respawns the
// pawn at the aircraft; standalone: teleport our pawn to the bus position,
// restore visibility and run the existing skydive chain (F05).
// ----------------------------------------------------------------------------
void Athena::BRJump() {
    if (!session.pawn) { HandleJumpFromHudStart(); return; }
    if (br.aircraft && !IsBadPtr(br.aircraft)) {
        Vector loc{};
        if (ActorLocation(br.aircraft, loc)) {
            loc.Z -= 200.0; // just under the bus so the camera exits cleanly
            Call sl("/Script/Engine.Actor.K2_SetActorLocationAndRotation");
            if (sl.ok()) {
                Rotator rot{}; Vector vloc = loc;
                sl.set("NewLocation", vloc); sl.set("NewRotation", rot);
                bool sweep = false, tp = true;
                sl.set("bSweep", sweep); sl.set("bTeleport", tp);
                sl.run(session.pawn);
            }
        }
        Call hid("/Script/Engine.Actor.SetActorHiddenInGame");
        if (hid.ok()) { bool b = false; if (hid.set("bNewHidden", b)) hid.run(session.pawn); }
        Call col("/Script/Engine.Actor.SetActorEnableCollision");
        if (col.ok()) { bool b = true; if (col.set("bNewActorEnableCollision", b)) col.run(session.pawn); }
    }
    HandleJumpFromHudStart();
}

// ----------------------------------------------------------------------------
// [F73-g/F74] Erbium spawnbot — the full roster procedure from
// FortPlayerControllerAthena.cpp: pawn + BP controller + PlayerState, Possess,
// PlayerState wiring (+ OnReps), SetMaxHealth, TeamIndex/SquadId/bIsABot,
// ability sets, cosmetics (F74-d full chain), AlivePlayers, PlayersLeft++.
// [F74-e] every bot registers a movement motor (BRBotMoveTick) — a living
// match instead of mannequins.
// ----------------------------------------------------------------------------
static void* BRBotPawnClass = nullptr;
static void* BRBotCtrlClass = nullptr;
static void* BRBotStateClass = nullptr;
static void* BRBotHeroType = nullptr;

// ----------------------------------------------------------------------------
// [F76-b] Random bot cosmetics — the 21.30 GObjects dump holds 4,853 loaded
// CustomCharacterPart instances (583 CP_Head_* + 1,157 CP_Body_*) — the whole
// cooked locker of this build. Erbium gives every bot the same F_Med_Head1
// trio (its server just needs A body); the user asked for "different random
// skins, everything random". One GObjects pass builds head/body pools, every
// bot rolls a random (head, body) pair and runs the same full F74-d chain
// (ServerChoosePart + LocalCharacterParts + HeroType + OnReps +
// ApplyCharacterCosmetics + UpdatePlayerCustomCharacterPartsVisualization).
// ----------------------------------------------------------------------------
static std::vector<void*> g_head_pool;
static std::vector<void*> g_body_pool;
// [F77-b] complete outfits — resident AthenaCharacterItemDefinition objects
// carry HeroDefinition->Specializations->CharacterParts: a full (head, body)
// pair from ONE skin, exactly how the local player's locker renders Lancelot.
struct BRCidOutfit { void* head; void* body; void* cid; };
static std::vector<BRCidOutfit> g_cid_outfits;
static double g_pool_scan_at = 0;
static void BRPartPools(bool force = false) {
    GObjects_t* go = GObjectArray();
    void* ccpCls = FindObj("/Script/FortniteGame.CustomCharacterPart");
    void* cidCls = FindObj("/Script/FortniteGame.AthenaCharacterItemDefinition");
    if (!go || (!ccpCls && !cidCls)) { LOG("[BR] cosmetic pool scan unavailable (ccp=%p)", ccpCls); return; }
    // [F77-b] parts stream in over time — the F76 one-shot scan at the first
    // bot wave saw only 12 resident parts (heads=4 bodies=8). Rescan while the
    // pools are thin, at most once per 5s.
    double nowM = Monotonic();
    bool thin = g_head_pool.size() < 24 || g_body_pool.size() < 24 || g_cid_outfits.size() < 8;
    if (!force && (!thin || nowM - g_pool_scan_at < 5.0)) return;
    g_pool_scan_at = nowM;
    size_t heads0 = g_head_pool.size(), bodies0 = g_body_pool.size(), outfits0 = g_cid_outfits.size();
    for (int32 i = 0; i < go->NumElements; ++i) {
        if (g_head_pool.size() >= 512 && g_body_pool.size() >= 512 && g_cid_outfits.size() >= 512) break;
        void* object = go->GetById((uint32)i);
        if (!object || IsBadPtr(object) || IsCDO(object)) continue;
        void* cls = Read<void*>(object, profile::ObjectClass);
        if (!cls || IsBadPtr(cls)) continue;
        if (cidCls && IsA(object, cidCls)) {
            bool dup = false;
            for (auto& o : g_cid_outfits) if (o.cid == object) { dup = true; break; }
            if (dup) continue;
            void* heroDef = GetObjField(object, "/Script/FortniteGame.AthenaCharacterItemDefinition.HeroDefinition");
            if (!heroDef || IsBadPtr(heroDef)) continue;
            void* head = nullptr; void* body = nullptr;
            for (void* spec : SoftAssetArray(heroDef, "/Script/FortniteGame.FortHeroType.Specializations")) {
                if (!spec || IsBadPtr(spec)) continue;
                for (void* part : SoftAssetArray(spec, "/Script/FortniteGame.FortHeroSpecialization.CharacterParts")) {
                    if (!part || IsBadPtr(part)) continue;
                    std::string pn = ObjName(part);
                    if (!head && pn.find("Head") != std::string::npos) head = part;
                    else if (!body && pn.find("Body") != std::string::npos) body = part;
                }
            }
            if (head && body) g_cid_outfits.push_back({head, body, object});
        } else if (ccpCls && IsA(object, ccpCls)) {
            std::string n = ObjName(object);
            if (n.find("Empty") != std::string::npos) continue;
            if (n.find("Head") != std::string::npos) {
                if (g_head_pool.size() < 512) g_head_pool.push_back(object);
            } else if (n.find("Body") != std::string::npos) {
                if (g_body_pool.size() < 512) g_body_pool.push_back(object);
            }
        }
    }
    if (g_head_pool.size() != heads0 || g_body_pool.size() != bodies0 || g_cid_outfits.size() != outfits0)
        LOG("[BR] cosmetic pools: heads=%zu(+%zu) bodies=%zu(+%zu) outfits=%zu(+%zu) (F77 — resident locker scan)",
            g_head_pool.size(), g_head_pool.size() - heads0, g_body_pool.size(), g_body_pool.size() - bodies0,
            g_cid_outfits.size(), g_cid_outfits.size() - outfits0);
}

static void BRSpawnBotBatch(int n) {
    if (!session.world || !session.mode || !session.state) return;
    if (!BRBotPawnClass) BRBotPawnClass = LoadClass("/Game/Athena/PlayerPawn_Athena.PlayerPawn_Athena_C", "/Script/Engine.Pawn");
    if (!BRBotCtrlClass) BRBotCtrlClass = LoadClass("/Game/Athena/Athena_PlayerController.Athena_PlayerController_C", "/Script/FortniteGame.FortPlayerControllerAthena");
    if (!BRBotCtrlClass) BRBotCtrlClass = FindObj("/Script/FortniteGame.FortPlayerControllerAthena");
    if (!BRBotStateClass) BRBotStateClass = FindObj("/Script/FortniteGame.FortPlayerStateAthena");
    if (!BRBotHeroType) BRBotHeroType = LoadAsset("/Game/Athena/Heroes/HID_001_Athena_Commando_F.HID_001_Athena_Commando_F", "/Script/FortniteGame.FortHeroType");
    // [F74-d] head/body parts are resolved per-batch inside the cosmetic block
    // (BRBotHead/BRBotBody statics removed — they fed ApplyCharacterCosmetics
    // only and kept the old unresolvable paths).
    if (!BRBotPawnClass || !BRBotCtrlClass || !BRBotStateClass) {
        LOG("[BR] bot classes missing p=%p c=%p s=%p — disabling roster", BRBotPawnClass, BRBotCtrlClass, BRBotStateClass);
        br.bots_target = br.bots_spawned;
        return;
    }

    // spawn points: PlayerStart instances (Erbium uses the caller pawn's spot;
    // a roster needs spread — PlayerStarts are the game's own spread points)
    static std::vector<void*> starts;
    static bool startsCached = false;
    if (!startsCached) {
        startsCached = true;
        void* psCls = FindObj("/Script/Engine.PlayerStart");
        if (psCls) starts = FindAllInstances(psCls, 128);
        LOG("[BR] PlayerStarts available: %d", (int)starts.size());
    }

    void* aliveProp = GetProp("/Script/FortniteGame.FortGameMode.AlivePlayers");
    int spawned = 0;
    // [F76-b] random cosmetic pools (583 heads / 1,157 bodies in this build).
    // The F74 statics stay as the guaranteed fallback when the pool scan
    // finds nothing (Erbium trio).
    BRPartPools();
    static void* headPart = nullptr;
    static void* bodyPart = nullptr;
    static void* backpackPart = nullptr;
    if (!headPart) headPart = LoadAsset("/Game/Athena/Heroes/Meshes/Heads/F_Med_Head1_ATH.F_Med_Head1_ATH", "/Script/FortniteGame.CustomCharacterPart");
    if (!bodyPart) bodyPart = LoadAsset("/Game/Athena/Heroes/Meshes/Bodies/CP_001_Athena_Body.CP_001_Athena_Body", "/Script/FortniteGame.CustomCharacterPart");
    if (!headPart) headPart = ByShortName("CP_Head_Med_Soldier_M_DesertOps");
    if (!bodyPart) bodyPart = ByShortName("CP_Athena_Body_M_Fallback");
    if (!backpackPart) backpackPart = ByShortName("NoBackpack");
    for (int k = 0; k < n && br.bots_spawned < br.bots_target; ++k) {
        Vector loc{0, 0, 500};
        bool haveLoc = false;
        if (!starts.empty()) {
            void* s = starts[BRRand() % starts.size()];
            if (s && !IsBadPtr(s)) { if (ActorLocation(s, loc)) { loc.Z += 150; haveLoc = true; } }
        }
        if (!haveLoc) {
            Vector c{0, 0, 0};
            if (!br.zone_centers.empty()) c = br.zone_centers[0];
            float ang = BRRand01() * 6.2831853f, dist = 2000 + BRRand01() * 60000;
            loc = Vector{c.X + std::cos(ang) * dist, c.Y + std::sin(ang) * dist, c.Z + 1000};
        }
        Transform t{};
        t.Location[0] = loc.X; t.Location[1] = loc.Y; t.Location[2] = loc.Z;

        void* pawn = SpawnActor(session.world, BRBotPawnClass, nullptr, t);
        void* ctrl = pawn ? SpawnActor(session.world, BRBotCtrlClass, nullptr, t) : nullptr;
        void* st   = ctrl ? SpawnActor(session.world, BRBotStateClass, nullptr, t) : nullptr;
        if (!pawn || !ctrl || !st) {
            LOG("[BR] bot spawn failed p=%p c=%p s=%p (spawned %d before failure)", pawn, ctrl, st, spawned);
            if (pawn) { Call d("/Script/Engine.Actor.K2_DestroyActor"); if (d.ok()) d.run(pawn); }
            if (ctrl) { Call d("/Script/Engine.Actor.K2_DestroyActor"); if (d.ok()) d.run(ctrl); }
            if (st)   { Call d("/Script/Engine.Actor.K2_DestroyActor"); if (d.ok()) d.run(st); }
            break;
        }

        // PlayerState wiring (Erbium: SetOwner + both OnRep_PlayerState paths)
        Call so("/Script/Engine.Actor.SetOwner");
        if (so.ok() && so.set("Owner", ctrl)) so.run(st);
        if (void* psProp = GetProp("/Script/Engine.Controller.PlayerState"))
            Write<void*>(GetField(ctrl, psProp), 0, st);
        Call orc("/Script/Engine.PlayerController.OnRep_PlayerState");
        if (orc.ok()) orc.run(ctrl);
        if (void* ppProp = GetProp("/Script/Engine.Pawn.PlayerState"))
            Write<void*>(GetField(pawn, ppProp), 0, st);
        Call orp("/Script/Engine.Pawn.OnRep_PlayerState");
        if (orp.ok()) orp.run(pawn);

        Possess(ctrl, pawn);
        if (void* mfp = GetProp("/Script/FortniteGame.FortPlayerController.MyFortPawn"))
            Write<void*>(GetField(ctrl, mfp), 0, pawn);
        for (const char* method : {"/Script/FortniteGame.FortPawn.SetMaxHealth", "/Script/FortniteGame.FortPawn.SetHealth"}) {
            Call mh(method);
            if (mh.ok() && mh.set_at(0, 100.0f)) mh.run(pawn);
        }

        // TeamIndex/SquadId/bIsABot (Erbium PickTeam: unique team per solo bot)
        uint8_t team = (uint8_t)(3 + br.bots_spawned);
        if (void* tiProp = GetProp("/Script/FortniteGame.FortPlayerState.TeamIndex")) {
            int esz = Read<int>(tiProp, profile::PropElementSize);
            void* addr = GetField(st, tiProp);
            if (esz == 1) Write<uint8_t>(addr, 0, team);
            else if (esz == 2) Write<uint16_t>(addr, 0, team);
            else if (esz == 4) Write<int>(addr, 0, team);
        }
        if (void* sqProp = GetProp("/Script/FortniteGame.FortPlayerState.SquadId")) {
            int esz = Read<int>(sqProp, profile::PropElementSize);
            void* addr = GetField(st, sqProp);
            int16_t sq = (int16_t)(team - 3);
            if (esz == 2) Write<int16_t>(addr, 0, sq);
            else if (esz == 4) Write<int>(addr, 0, sq);
        }
        SetBool(st, "/Script/FortniteGame.FortPlayerState.bIsABot", true);

        // [F74-d + F77-b] cosmetics — the exact chain that gave the LOCAL
        // player a working body, now with a RANDOM outfit per bot: prefer a
        // complete resident skin (CID outfit — head+body from ONE skin), then
        // independent random parts, then the Erbium trio statics.
        {
            void* botHead = nullptr; void* botBody = nullptr; void* botCid = nullptr;
            if (!g_cid_outfits.empty()) {
                const BRCidOutfit& o = g_cid_outfits[(size_t)(BRRand() % g_cid_outfits.size())];
                botHead = o.head; botBody = o.body; botCid = o.cid;
            } else {
                if (!g_head_pool.empty()) botHead = g_head_pool[(size_t)(BRRand() % g_head_pool.size())];
                if (!g_body_pool.empty()) botBody = g_body_pool[(size_t)(BRRand() % g_body_pool.size())];
            }
            if (!botHead) botHead = headPart;
            if (!botBody) botBody = bodyPart;
            if (k < 3 && !br.bot_skins_logged) {
                br.bot_skins_logged = true;
                LOG("[BR] bot skins: %s | %s | %s",
                    botCid ? ObjName(botCid).c_str() : "<parts>",
                    botHead ? ObjName(botHead).c_str() : "<null>",
                    botBody ? ObjName(botBody).c_str() : "<null>");
            }
            // [F77-b] the LOCAL player's skin works because SyncLockerLoadout
            // mirrors the whole FFortAthenaLoadout onto the pawn
            // (FortPlayerPawn.CosmeticLoadout) — every bot gets the same
            // mirror with its own random CID.
            void* pawnLoProp = GetProp("/Script/FortniteGame.FortPlayerPawn.CosmeticLoadout");
            if (pawnLoProp) {
                int loSize = Read<int>(pawnLoProp, profile::PropElementSize);
                if (loSize > 8 && loSize <= 1024) {
                    std::vector<uint8_t> shadow((size_t)loSize, 0);
                    if (botCid) {
                        void* chProp = GetProp("/Script/FortniteGame.FortAthenaLoadout.Character");
                        if (chProp) {
                            int chOff = Read<int>(chProp, profile::PropOffset);
                            if (chOff >= 0 && chOff + 8 <= loSize)
                                memcpy(shadow.data() + chOff, &botCid, 8);
                        }
                    }
                    memcpy(GetField(pawn, pawnLoProp), shadow.data(), (size_t)loSize);
                }
            }
            int partsApplied = 0;
            if (botHead) { Call choose("/Script/FortniteGame.FortPlayerPawn.ServerChoosePart");
                             if (choose.ok() && choose.set("Part", (uint8_t)0) && choose.set("ChosenCharacterPart", botHead)) { choose.run(pawn); ++partsApplied; } }
            if (botBody) { Call choose("/Script/FortniteGame.FortPlayerPawn.ServerChoosePart");
                             if (choose.ok() && choose.set("Part", (uint8_t)1) && choose.set("ChosenCharacterPart", botBody)) { choose.run(pawn); ++partsApplied; } }
            if (partsApplied) {
                Call vis("/Script/FortniteGame.FortPlayerPawnAthena.SetCharacterBodyVisibilityForPossession");
                if (vis.ok()) { bool show = true; if (vis.set("bVisible", show)) vis.run(pawn); }
            }
            // Erbium spawnbot fallback array: LocalCharacterParts[0/1/3]
            // (UCustomCharacterPart* [7] — fixed C-array on FortPlayerState).
            if (void* lcp = GetProp("/Script/FortniteGame.FortPlayerState.LocalCharacterParts")) {
                void* base = GetField(st, lcp);
                if (botHead) Write<void*>(base, 0 * 8, botHead);
                if (botBody) Write<void*>(base, 1 * 8, botBody);
                if (backpackPart) Write<void*>(base, 3 * 8, backpackPart);
            }
            if (BRBotHeroType) {
                if (void* htProp = GetProp("/Script/FortniteGame.FortPlayerState.HeroType"))
                    Write<void*>(GetField(st, htProp), 0, BRBotHeroType);
                Call rht("/Script/FortniteGame.FortPlayerState.OnRep_HeroType");
                if (rht.ok()) rht.run(st);
            }
            if (botHead && botBody) {
                void* parts[2] = {botHead, botBody};
                Array partsArr{parts, 2, 2};
                void* klib = FindObj("/Script/FortniteGame.Default__FortKismetLibrary");
                Call apply("/Script/FortniteGame.FortKismetLibrary.ApplyCharacterCosmetics");
                if (apply.ok() && klib && apply.set("WorldContextObject", session.world) &&
                    apply.set("CharacterParts", partsArr) && apply.set("PlayerState", st))
                    apply.run(klib);
            }
            Call rcd("/Script/FortniteGame.FortPlayerState.OnRep_CharacterData");
            if (rcd.ok()) rcd.run(st);
            void* klib2 = FindObj("/Script/FortniteGame.Default__FortKismetLibrary");
            if (klib2) {
                Call upd("/Script/FortniteGame.FortKismetLibrary.UpdatePlayerCustomCharacterPartsVisualization");
                if (upd.ok() && upd.set("PlayerState", st)) upd.run(klib2);
            }
        }

        // [F74-e] movement driver registration (bots wander, see BRBotMoveTick)
        br.bot_pawns.push_back(pawn);
        g_bot_motors.push_back({pawn, (float)(BRRand01() * 360.0), 0.0});

        // ability sets (Erbium: GiveAbilitySet per AbilitySets entry)
        for (void* set : session.granted_ability_sets) ApplyAbilitySet(pawn, set);

        // [F75-a] WorldInventory — the F74 crash at bus-zero died inside the
        // NATIVE warmup-end transition (evidence: the pickaxe
        // ServerExecuteInventoryItem fired by the game itself right before the
        // crash, plus no [BR] StartAircraftPhase log). That transition walks
        // every controller in AlivePlayers and dereferences WorldInventory —
        // our bots had none (Erbium's own spawnbot spawns one but it is
        // commented out there because the real server never touches it; our
        // standalone native paths do). Spawn a real empty inventory per bot.
        if (!session.bot_inventory_cls) session.bot_inventory_cls = FindObj("/Script/FortniteGame.FortInventory");
        if (session.bot_inventory_cls) {
            Transform it{};   // zero transform — inventories are non-world actors
            void* inv = SpawnActor(session.world, session.bot_inventory_cls, ctrl, it);
            if (inv) {
                if (void* wiProp = GetProp("/Script/FortniteGame.FortPlayerController.WorldInventory"))
                    Write<void*>(GetField(ctrl, wiProp), 0, inv);
                SetBool(ctrl, "/Script/FortniteGame.FortPlayerController.bHasInitializedWorldInventory", true);
                SetBool(ctrl, "/Script/FortniteGame.FortPlayerController.bIsWorldInventoryInitialized", true);
                if (void* itProp = GetProp("/Script/FortniteGame.FortInventory.InventoryType"))
                    Write<uint8_t>(GetField(inv, itProp), 0, 0);   // EFortInventoryType::World
            }
        }

        // [F75-a] GameMemberInfoArray — Erbium spawnbot registers every bot
        // (FGameMemberInfo{SquadId,TeamIndex} + array add). Native code
        // (team screens, PlayerArray walks) reads this list; leaving bots out
        // makes the roster invisible to it.
        {
            void* gma = GetObjField(session.state, "/Script/FortniteGame.FortGameStateAthena.GameMemberInfoArray");
            void* membersProp = GetProp("/Script/FortniteGame.GameMemberInfoArray.Members");
            if (gma && membersProp) {
                int elem = Read<int>(membersProp, profile::PropElementSize);
                if (elem >= 0x10 && elem <= 0x80) {
                    std::vector<uint8_t> member((size_t)elem, 0);
                    // FFastArraySerializerItem head: keys are -1 pre-replication
                    Write<int32>(member.data(), 0, -1);
                    Write<int32>(member.data(), 4, -1);
                    Write<int32>(member.data(), 8, -1);
                    void* sqProp = GetProp("/Script/FortniteGame.GameMemberInfo.SquadId");
                    void* tiProp = GetProp("/Script/FortniteGame.GameMemberInfo.TeamIndex");
                    int sqOff = sqProp ? Read<int>(sqProp, profile::PropOffset) : 0x0C;
                    int tiOff = tiProp ? Read<int>(tiProp, profile::PropOffset) : 0x0D;
                    if (sqOff >= 0 && sqOff < elem) member[(size_t)sqOff] = (uint8_t)(team - 3);
                    if (tiOff >= 0 && tiOff < elem) member[(size_t)tiOff] = team;
                    Fn<int(*)(void*, void*, const void*)>(profile::GenericArrayAdd)(
                        GetField(gma, membersProp), membersProp, member.data());
                }
            }
        }

        // AlivePlayers (storm/zone natives iterate this)
        if (aliveProp)
            Fn<int(*)(void*, void*, const void*)>(profile::GenericArrayAdd)(GetField(session.mode, aliveProp), aliveProp, &ctrl);

        br.bots_spawned++;
        spawned++;
    }

    if (spawned > 0) {
        // PlayersLeft = 1 (local) + bots, OnRep once per batch
        void* plProp = GetProp("/Script/FortniteGame.FortGameStateAthena.PlayersLeft");
        if (plProp) {
            Write<int>(GetField(session.state, plProp), 0, 1 + br.bots_spawned);
            Call rpl("/Script/FortniteGame.FortGameStateAthena.OnRep_PlayersLeft");
            if (rpl.ok()) rpl.run(session.state);
        }
        LOG("[BR] +%d bots — roster %d/%d, PlayersLeft=%d, motors=%zu", spawned, br.bots_spawned, br.bots_target, 1 + br.bots_spawned, g_bot_motors.size());
    }
}

// ----------------------------------------------------------------------------
// [F74-e/F75-b/F76-c] Movement motors — the game's own navmesh pathing.
// F75's raw Velocity writes moved the pawns but "stupidly" (user's words):
// straight lines into walls, constant slide-on-obstacle, animals fighting
// their own native AI. F76 issues UAIBlueprintHelperLibrary.SimpleMoveTo-
// Location(Controller, Goal) — the native that creates a PathFollowing-
// Component on any controller and walks real navmesh paths — on a staggered
// wander schedule. A 2.2s stuck-check measures actual displacement: a pawn
// the navmesh never moved (<40 units) flips to the velocity drive for that
// window (fresh heading + SetControlRotation so it still faces where it
// walks), then retries the navmesh on the next retarget. Dead pawns drop
// out of the lists.
// ----------------------------------------------------------------------------
static void MotorTick(std::vector<BRBotMotor>& list, float speed, const char* tag) {
    if (list.empty()) return;
    double now = Monotonic();
    static Call* smove = nullptr;       // [F76-c] navmesh move (static on AIBlueprintHelperLibrary)
    static void*  aiLib = nullptr;
    static Call* rot = nullptr;
    static Call* move = nullptr;
    static Call* getLoc = nullptr;
    static void* ctrlProp = nullptr;      // hot path — resolve once
    static void* charMoveProp = nullptr;  // [F75-b] Character.CharacterMovement
    static void* velProp = nullptr;       // [F75-b] MovementComponent.Velocity
    if (!smove) smove = new Call("/Script/AIModule.AIBlueprintHelperLibrary.SimpleMoveToLocation");
    if (!aiLib) aiLib = FindObj("/Script/AIModule.Default__AIBlueprintHelperLibrary");
    if (!rot) rot = new Call("/Script/Engine.Controller.SetControlRotation");
    if (!move) move = new Call("/Script/Engine.Pawn.AddMovementInput");
    if (!getLoc) getLoc = new Call("/Script/Engine.Actor.K2_GetActorLocation");
    if (!ctrlProp) ctrlProp = GetProp("/Script/Engine.Pawn.Controller");
    if (!charMoveProp) charMoveProp = GetProp("/Script/Engine.Character.CharacterMovement");
    if (!velProp) velProp = GetProp("/Script/Engine.MovementComponent.Velocity");
    int ctrlOff = ctrlProp ? Read<int>(ctrlProp, profile::PropOffset) : -1;
    int cmOff   = charMoveProp ? Read<int>(charMoveProp, profile::PropOffset) : -1;
    int vOff    = velProp ? Read<int>(velProp, profile::PropOffset) : -1;
    static double lastBeat = 0;
    static int navmesh_moves = 0, vel_moves = 0;
    if (now - lastBeat > 5.0) {
        lastBeat = now;
        LOG("[Motor] %s n=%zu navmesh=%d vel=%d smove=%d cmOff=%d vOff=%d", tag, list.size(),
            navmesh_moves, vel_moves, (int)(smove && smove->ok()), cmOff, vOff);
        navmesh_moves = 0; vel_moves = 0;
    }
    bool navOk = smove && smove->ok() && aiLib && !IsBadPtr(aiLib);
    bool isWild = !std::strcmp(tag, "Wild");
    // [F77-c/d] the player's position — one PE call per tick feeds both the
    // bot engagement check and the wildlife flee check.
    Vector playerAt{}; bool havePlayer = false;
    if (session.pawn && !IsBadPtr(session.pawn) && getLoc && getLoc->ok() && getLoc->run(session.pawn)) {
        playerAt = getLoc->result<Vector>(); havePlayer = true;
    }
    size_t w = 0;
    for (size_t i = 0; i < list.size(); ++i) {
        BRBotMotor& m = list[i];
        void* pawn = m.pawn;
        if (!pawn || IsBadPtr(pawn)) continue;                 // destroyed (storm/edit)
        void* cls = Read<void*>(pawn, profile::ObjectClass);
        if (!cls || IsBadPtr(cls)) continue;                   // half-freed actor
        // current location (stuck-check anchor + wander center)
        Vector at{};
        bool haveAt = false;
        if (getLoc && getLoc->ok() && getLoc->run(pawn)) { at = getLoc->result<Vector>(); haveAt = true; }
        // ---- retarget window: flee / storm discipline / engage / wander ----
        if (now >= m.next_heading) {
            m.yaw = (float)(BRRand01() * 360.0);
            m.next_heading = now + 4.0 + BRRand01() * 4.0;
            Vector goal{ at.X, at.Y, at.Z };
            bool   haveGoal = false;
            bool   engage   = false;
            if (isWild) {
                // [F77-d] prey instinct — the player within 22m scatters the
                // animal AWAY from them (wildlife that ignores you is dead).
                if (havePlayer) {
                    float ddx = at.X - playerAt.X, ddy = at.Y - playerAt.Y;
                    float pd2 = ddx * ddx + ddy * ddy;
                    if (pd2 < 2200.0f * 2200.0f && pd2 > 1.0f) {
                        float inv = 1.0f / std::sqrt(pd2);
                        goal = Vector{ at.X + ddx * inv * 1400.0f, at.Y + ddy * inv * 1400.0f, at.Z };
                        haveGoal = true;
                    }
                }
            } else {
                // [F77-c] storm discipline — outside the first safe circle the
                // bot walks back in (no more bots staring at the storm wall).
                if (br.phase >= 4 && !br.zone_centers.empty()) {
                    Vector c = br.zone_centers[0];
                    float zr = br.radius.empty() ? 25600.0f : br.radius[0];
                    float ddx = at.X - c.X, ddy = at.Y - c.Y;
                    if (std::sqrt(ddx * ddx + ddy * ddy) > zr * 0.92f) {
                        float ang = BRRand01() * 6.2831853f, rr = zr * 0.25f * BRRand01();
                        goal = Vector{ c.X + std::cos(ang) * rr, c.Y + std::sin(ang) * rr, at.Z };
                        haveGoal = true;
                    }
                }
                // [F77-c] engagement — the player within 30m: face them and
                // hold ground for 1-3s (a bot that notices you is alive).
                if (!haveGoal && havePlayer) {
                    float ddx = at.X - playerAt.X, ddy = at.Y - playerAt.Y;
                    if (ddx * ddx + ddy * ddy < 3000.0f * 3000.0f) {
                        m.face = session.pawn;
                        m.face_until = now + 1.2 + BRRand01() * 2.5;
                        m.next_heading = m.face_until + 0.5;
                        engage = true;
                    }
                }
            }
            if (engage) {
                m.lastX = haveAt ? at.X : 0; m.lastY = haveAt ? at.Y : 0;
            } else if (haveGoal || navOk) {
                if (!haveGoal) {
                    // [F76-c] primary: native navmesh wander — real pathing
                    // around obstacles instead of straight lines into walls.
                    float wander = isWild ? 500.0f + BRRand01() * 900.0f
                                          : 900.0f + BRRand01() * 1700.0f;
                    float ang = BRRand01() * 6.2831853f;
                    goal = Vector{ at.X + std::cos(ang) * wander, at.Y + std::sin(ang) * wander, at.Z };
                }
                void* ctrl = ctrlOff > 0 ? Read<void*>(pawn, (size_t)ctrlOff) : nullptr;
                if (navOk && ctrl && !IsBadPtr(ctrl) &&
                    smove->set("Controller", ctrl) && smove->set("Goal", goal) && smove->run(aiLib)) {
                    m.vel_mode = 0;                       // trust the navmesh this window
                    m.lastX = haveAt ? at.X : 0; m.lastY = haveAt ? at.Y : 0;
                    m.last_check = now + 2.2;
                    ++navmesh_moves;
                    if (navmesh_moves == 1)
                        LOG("[Motor] %s navmesh move -> (%.0f, %.0f)", tag, goal.X, goal.Y);
                } else {
                    m.vel_mode = 1;                       // request refused — velocity window
                }
            } else {
                m.vel_mode = 1;
            }
        }
        // [F77-c] engaged — hold ground and track the player every tick
        if (m.face && now < m.face_until) {
            Vector fat{};
            if (getLoc && getLoc->ok() && getLoc->run(m.face)) {
                fat = getLoc->result<Vector>();
                void* ctrl = ctrlOff > 0 ? Read<void*>(pawn, (size_t)ctrlOff) : nullptr;
                if (ctrl && !IsBadPtr(ctrl) && rot && rot->ok() && haveAt) {
                    Rotator r{}; r.Yaw = std::atan2(fat.Y - at.Y, fat.X - at.X) * 57.2957795f;
                    if (rot->set("NewRotation", r)) rot->run(ctrl);
                }
                if (cmOff > 0 && vOff > 0) {
                    void* cm = Read<void*>(pawn, (size_t)cmOff);
                    if (cm && !IsBadPtr(cm)) {
                        Vector v = Read<Vector>(cm, (size_t)vOff);
                        if (std::isfinite(v.Z)) { v.X = 0; v.Y = 0; Write<Vector>(cm, (size_t)vOff, v); }
                    }
                }
                list[w++] = m;
                continue;
            }
            m.face = nullptr;   // target gone — back to the wander machine
        }
        // ---- stuck check 2.2s after each navmesh request ----
        if (!m.vel_mode && haveAt && now >= m.last_check) {
            double dx = at.X - m.lastX, dy = at.Y - m.lastY;
            if (dx * dx + dy * dy < 40.0 * 40.0) {
                m.vel_mode = 1;                           // navmesh silent — velocity window
                m.next_heading = now + 1.0 + BRRand01();  // re-roll soon
            }
            m.last_check = now + 2.2;
            m.lastX = at.X; m.lastY = at.Y;
        }
        // ---- velocity fallback (navmesh failed or stuck) ----
        if (m.vel_mode) {
            float rad = m.yaw * 3.14159265f / 180.0f;
            Vector dir{std::cos(rad), std::sin(rad), 0};
            // [F75-b] primary drive: write CharacterMovement->Velocity (keep
            // the current Z so gravity still owns vertical motion).
            if (cmOff > 0 && vOff > 0) {
                void* cm = Read<void*>(pawn, (size_t)cmOff);
                if (cm && !IsBadPtr(cm)) {
                    Vector v = Read<Vector>(cm, (size_t)vOff);
                    if (std::isfinite(v.Z)) {
                        v.X = dir.X * speed; v.Y = dir.Y * speed;
                        Write<Vector>(cm, (size_t)vOff, v);
                    }
                }
            }
            void* ctrl = ctrlOff > 0 ? Read<void*>(pawn, (size_t)ctrlOff) : nullptr;
            if (ctrl && !IsBadPtr(ctrl) && rot && rot->ok()) {
                Rotator r{}; r.Pitch = 0; r.Yaw = m.yaw; r.Roll = 0;
                if (rot->set("NewRotation", r)) rot->run(ctrl);
            }
            if (move && move->ok()) {
                float scale = 1.0f; uint8_t force = 0;
                if (move->set("WorldDirection", dir) && move->set("ScaleValue", scale) && move->set("bForce", force))
                    move->run(pawn);
            }
            ++vel_moves;
        }
        list[w++] = m;
    }
    list.resize(w);
}
static void BRBotMoveTick() { MotorTick(g_bot_motors, 320.0f, "Bot"); }

// ----------------------------------------------------------------------------
// [F73-h] Storm pass — Erbium's GamePhaseLogic Tick tail: bIsInsideSafeZone /
// bIsInAnyStorm for every pawn + storm damage. The native indicator applies
// the GameplayEffect path when a server ticks it; standalone we mirror it with
// GameplayStatics.ApplyDamage (percentage of MaxHealth per second outside),
// causer = the indicator. Bots that reach 0 HP are destroyed and thinned out
// of PlayersLeft/AlivePlayers (real elimination bookkeeping).
// ----------------------------------------------------------------------------
static void BRStormTick() {
    if (!br.indicator || IsBadPtr(br.indicator) || !session.pawn) return;
    Vector c{}; float r = 0;
    { Call gc("/Script/FortniteGame.FortSafeZoneIndicator.GetSafeZoneCenter");
      if (gc.ok() && gc.run(br.indicator)) c = gc.result<Vector>(); }
    { Call gr("/Script/FortniteGame.FortSafeZoneIndicator.GetSafeZoneRadius");
      if (gr.ok() && gr.run(br.indicator)) r = gr.result<float>(); }
    if (r <= 0) return;
    auto outside = [&](void* pawn) -> bool {
        Vector l{};
        if (!ActorLocation(pawn, l)) return false;
        double dx = l.X - c.X, dy = l.Y - c.Y;
        return dx * dx + dy * dy > double(r) * double(r);
    };

    // local pawn flags (Erbium printf -> our LOG, once per change)
    bool in = !outside(session.pawn);
    bool cur = GetBool(session.pawn, "/Script/FortniteGame.FortPlayerPawn.bIsInsideSafeZone");
    if (cur != in) {
        SetBool(session.pawn, "/Script/FortniteGame.FortPlayerPawn.bIsInsideSafeZone", in);
        SetBool(session.pawn, "/Script/FortniteGame.FortPlayerPawn.bIsInAnyStorm", !in);
        Call ra("/Script/FortniteGame.FortPlayerPawn.OnRep_IsInsideSafeZone");
        if (ra.ok()) ra.run(session.pawn);
        Call rb("/Script/FortniteGame.FortPlayerPawn.OnRep_IsInAnyStorm");
        if (rb.ok()) rb.run(session.pawn);
        LOG("[BR] local pawn storm status: %s (phase %d r=%.0f)", in ? "safe" : "STORM", br.phase, r);
    }

    // damage bookkeeping
    float maxhp = 100;
    BRReadFloat(session.pawn, "/Script/FortniteGame.FortPawn.MaxHealth", maxhp);
    int curPhase = 0;
    if (void* cpProp = GetProp("/Script/FortniteGame.FortSafeZoneIndicator.CurrentPhase"))
        curPhase = Read<int>(GetField(br.indicator, cpProp), 0);
    if (curPhase < 0 || curPhase >= (int)br.damage.size()) curPhase = 0;
    float pct = br.damage[curPhase];
    if (!in && pct > 0) {
        float dmg = maxhp * pct / 100.0f;
        void* gsCdo = FindObj("/Script/Engine.Default__GameplayStatics");
        Call ad("/Script/Engine.GameplayStatics.ApplyDamage");
        if (ad.ok() && gsCdo) {
            if (ad.set("DamagedActor", session.pawn) && ad.set("BaseDamage", dmg) &&
                ad.set("DamageCauser", br.indicator)) {
                void* zero = nullptr;
                if (!ad.set("EventInstigator", zero)) ad.set_at(1, zero);
                ad.run(gsCdo);
                LOG("[BR] storm damage %.1f (%d%% of %.0f)", dmg, (int)pct, maxhp);
            }
        }
    }

    // bots: outside -> damage; dead -> destroy + roster bookkeeping
    void* aliveProp = GetProp("/Script/FortniteGame.FortGameMode.AlivePlayers");
    void* plProp = GetProp("/Script/FortniteGame.FortGameStateAthena.PlayersLeft");
    if (!aliveProp || !session.mode) return;
    Array* alive = (Array*)GetField(session.mode, aliveProp);
    if (!alive || alive->Count < 1 || !alive->Data) return;
    // NOTE: alive->Data/Count re-read every iteration — GenericArrayRemove can
    // realloc the storage mid-loop (backwards index keeps ordering safe).
    for (int32 i = (int32)alive->Count - 1; i >= 0; --i) {
        void* pc = Read<void*>(alive->Data, (size_t)i * 8);
        if (!pc || pc == session.controller || IsBadPtr(pc)) continue;
        void* pawn = GetPawn(pc);
        if (!pawn || IsBadPtr(pawn)) continue;
        float hp = 0;
        BRReadFloat(pawn, "/Script/FortniteGame.FortPawn.Health", hp);
        if (!outside(pawn) && hp > 0) {
            float bmax = 100; BRReadFloat(pawn, "/Script/FortniteGame.FortPawn.MaxHealth", bmax);
            float dmg = bmax * pct / 100.0f;
            void* gsCdo = FindObj("/Script/Engine.Default__GameplayStatics");
            Call ad("/Script/Engine.GameplayStatics.ApplyDamage");
            if (ad.ok() && gsCdo && ad.set("DamagedActor", pawn) && ad.set("BaseDamage", dmg) &&
                ad.set("DamageCauser", br.indicator)) {
                void* zero = nullptr;
                if (!ad.set("EventInstigator", zero)) ad.set_at(1, zero);
                ad.run(gsCdo);
            }
            continue;
        }
        if (hp <= 0) { // eliminated by the storm
            char name[64]{};
            snprintf(name, sizeof(name), "ErbiumBot(#%d)", i);
            Call d("/Script/Engine.Actor.K2_DestroyActor");
            if (d.ok()) d.run(pawn);
            // [F74-e] drop the elimination from the movement roster too
            for (auto& m : g_bot_motors)
                if (m.pawn == pawn) { m.pawn = nullptr; break; }
            if (plProp) {
                int left = Read<int>(GetField(session.state, plProp), 0);
                if (left > 1) Write<int>(GetField(session.state, plProp), 0, left - 1);
                Call rpl("/Script/FortniteGame.FortGameStateAthena.OnRep_PlayersLeft");
                if (rpl.ok()) rpl.run(session.state);
            }
            // GenericArrayRemove takes the ELEMENT INDEX (same shape as the
            // inventory usage) — resolve it before removing.
            for (int32 j = 0; j < (int32)alive->Count; ++j)
                if (Read<void*>(alive->Data, (size_t)j * 8) == pc) {
                    Fn<void(*)(void*, void*, int)>(profile::GenericArrayRemove)(GetField(session.mode, aliveProp), aliveProp, j);
                    break;
                }
            LOG("[BR] 💀 %s eliminated by the storm (left=%d)", name,
                plProp ? Read<int>(GetField(session.state, plProp), 0) : -1);
        }
    }
}

// ----------------------------------------------------------------------------
// [F73-i] Match start — Erbium HandleMatchHasStarted: warmup countdown on the
// component + GameState, GamePhase(Warmup), local PC into AlivePlayers, bot
// waves scheduled. (era.warmup N shortens the countdown on device.)
// ----------------------------------------------------------------------------
static void BRStart() {
    br.started = true;
    LOG("=== [BR] match start — Erbium HandleMatchHasStarted ===");
    BRGetComponent();
    if (!br.mapinfo || IsBadPtr(br.mapinfo))
        br.mapinfo = GetObjField(session.state, "/Script/FortniteGame.FortGameStateAthena.MapInfo");
    float now = BRWorldTime();
    float warmup = 60.0f;
    if (br.component) {
        BRWriteFloat(br.component, "/Script/FortniteGame.FortGameStateComponent_BattleRoyaleGamePhaseLogic.WarmupCountdownStartTime", now);
        BRWriteFloat(br.component, "/Script/FortniteGame.FortGameStateComponent_BattleRoyaleGamePhaseLogic.WarmupCountdownEndTime", now + warmup);
        BRWriteFloat(br.component, "/Script/FortniteGame.FortGameStateComponent_BattleRoyaleGamePhaseLogic.WarmupCountdownDuration", 10);
        BRWriteFloat(br.component, "/Script/FortniteGame.FortGameStateComponent_BattleRoyaleGamePhaseLogic.WarmupEarlyCountdownDuration", warmup - 10);
    }
    BRWriteFloat(session.state, "/Script/FortniteGame.FortGameStateAthena.WarmupCountdownStartTime", now);
    BRWriteFloat(session.state, "/Script/FortniteGame.FortGameStateAthena.WarmupCountdownEndTime", now + warmup);
    BRWriteFloat(session.mode, "/Script/FortniteGame.FortGameMode.WarmupCountdownDuration", warmup);
    BRWriteFloat(session.mode, "/Script/FortniteGame.FortGameMode.WarmupEarlyCountdownDuration", warmup - 10);
    br.warmup_end = now + warmup;

    // Erbium WarmupRequiredPlayerCount = 1 (single player starts the match)
    void* wpc = GetProp("/Script/FortniteGame.FortGameMode.WarmupRequiredPlayerCount");
    if (wpc) Write<int>(GetField(session.mode, wpc), 0, 1);
    SetBool(session.mode, "/Script/FortniteGame.FortGameMode.bWorldIsReady", true);
    SetBool(session.state, "/Script/FortniteGame.FortGameStateAthena.bPlaylistStoppedSafeZonePhases", false);

    // AlivePlayers: the local PC (HandleStartingNewPlayer normally does this)
    if (!br.alive_added) {
        void* aliveProp = GetProp("/Script/FortniteGame.FortGameMode.AlivePlayers");
        if (aliveProp && session.controller) {
            Fn<int(*)(void*, void*, const void*)>(profile::GenericArrayAdd)(GetField(session.mode, aliveProp), aliveProp, &session.controller);
            br.alive_added = true;
        }
    }
    void* plProp = GetProp("/Script/FortniteGame.FortGameStateAthena.PlayersLeft");
    if (plProp) {
        Write<int>(GetField(session.state, plProp), 0, 1);
        Call rpl("/Script/FortniteGame.FortGameStateAthena.OnRep_PlayersLeft");
        if (rpl.ok()) rpl.run(session.state);
    }

    BRSetGamePhase(2);     // EAthenaGamePhase::Warmup
    BRSetGamePhaseStep(2); // EAthenaGamePhaseStep::Warmup
    br.next_bot_wave = Monotonic() + 1.0;
    LOG("[BR] warmup %.0fs — bus at T+%.0f (era.warmup N / era.bus to control)", warmup, warmup);
}

// ----------------------------------------------------------------------------
// [F73-j] The machine — Erbium BattleRoyaleGamePhaseLogic::Tick, every world
// tick once the player is finalized.
// ----------------------------------------------------------------------------
void Athena::BRTick() {
    if (!session.finalized || !session.state || !session.mode || !session.world) return;
    if (session.lobby_requested) return;
    double mono = Monotonic();

    if (!br.started) {
        if (mono - session.finalized_at < 2.0) return;
        BRStart();
        return;
    }

    // Erbium bot roster — waves of 10 every 2s until the target is reached
    if (br.bots_spawned < br.bots_target && mono >= br.next_bot_wave) {
        br.next_bot_wave = mono + 2.0;
        BRSpawnBotBatch(10);
    }
    // [F74-e/F75-b] keep the roster alive — velocity motors (cheap: cached
    // UFunctions + direct Velocity writes), animals share the same driver
    BRBotMoveTick();
    MotorTick(g_wild_motors, 200.0f, "Wild");

    float now = BRWorldTime();

    if (br.phase <= 2) { // Warmup (None/Setup/Warmup)
        // [F77-a] STARVE THE NATIVE TRANSITION — the F76 device log proved the
        // crash at countdown zero is the NATIVE warmup-end machinery (its pawn
        // teardown ran: ReceiveDestroyed -> OnAvatarActorDestroyed ->
        // ClientForceWorldInventoryUpdate -> death). It reads GameState.
        // WarmupCountdownEndTime — so while GetReady runs we hold that value
        // permanently 6s from zero. The visible counter parks at 0:06 until
        // OUR phase broadcast flips the HUD to the bus (Erbium never lets the
        // native game mode drive phases either).
        if (br.step >= 3 && br.warmup_end >= 0) {
            BRWriteFloat(session.state, "/Script/FortniteGame.FortGameStateAthena.WarmupCountdownEndTime", now + 6.0f);
            if (br.component)
                BRWriteFloat(br.component, "/Script/FortniteGame.FortGameStateComponent_BattleRoyaleGamePhaseLogic.WarmupCountdownEndTime", now + 6.0f);
            static bool froze = false;
            if (!froze) { froze = true; LOG("[BR] native warmup countdown held at T-6 (F77 — native zero-transition starved)"); }
        }
        // [F74-f] Erbium Tick machine: WarmupEarlyCountdownDuration (end-10s)
        // flips the step to GetReady before the aircraft launches.
        if (br.step < 3 && br.warmup_end >= 0 && now >= br.warmup_end - 10.0f)
            BRSetGamePhaseStep(3); // EAthenaGamePhaseStep::GetReady
        // [F75-a] GetReady heartbeat — shows exactly how close the crash is
        if (br.step == 3 && br.warmup_end >= 0) {
            static double lastReady = 0;
            double mono2 = mono;
            if (mono2 - lastReady > 4.0 && now < br.warmup_end) {
                lastReady = mono2;
                LOG("[BR] GetReady — bus in %.0fs", (double)br.warmup_end - (double)now);
            }
        }
        if (br.warmup_end >= 0 && now >= br.warmup_end) {
            // [F75-a] named milestone — if the next log line is missing, the
            // crash happened inside the native warmup-end transition that
            // fires at this exact tick (see [PETRACE] + era_crash.txt).
            LOG("[BR] warmup elapsed (t=%.0f) — launching aircraft (native transition may also fire here)", now);
            BRStartAircraftPhase();
        }
        return;
    }

    if (br.phase == 3) { // Aircraft
        // [F77-a] keep the native countdown parked while our bus flies.
        BRWriteFloat(session.state, "/Script/FortniteGame.FortGameStateAthena.WarmupCountdownEndTime", now + 6.0f);
        if (br.component)
            BRWriteFloat(br.component, "/Script/FortniteGame.FortGameStateComponent_BattleRoyaleGamePhaseLogic.WarmupCountdownEndTime", now + 6.0f);
        void* ac = br.aircraft;
        if (ac && !IsBadPtr(ac)) {
            // [F77] bus flight heartbeat — names a dead bus immediately, and
            // drives it manually (straight line to the map centre) if the
            // native aircraft tick stays silent for two consecutive beats.
            static double lastBusBeat = 0; static Vector lastBusAt{}; static int stillBeats = 0;
            Vector bl{};
            bool haveBus = ActorLocation(ac, bl);
            if (haveBus && now - lastBusBeat > 3.0) {
                lastBusBeat = now;
                float moved = std::sqrt((bl.X - lastBusAt.X) * (bl.X - lastBusAt.X) +
                                        (bl.Y - lastBusAt.Y) * (bl.Y - lastBusAt.Y) +
                                        (bl.Z - lastBusAt.Z) * (bl.Z - lastBusAt.Z));
                LOG("[BUS] (%.0f, %.0f, %.0f) moved=%.0f", bl.X, bl.Y, bl.Z, (double)moved);
                stillBeats = moved < 30.0f ? stillBeats + 1 : 0;
                lastBusAt = bl;
            }
            if (haveBus && stillBeats >= 2) {
                static double lastDrive = 0;
                double dt = now - lastDrive;
                lastDrive = now;
                if (dt > 0.001 && dt < 0.25) {
                    Vector c{0, 0, 0};
                    if (!br.zone_centers.empty()) c = br.zone_centers[0];
                    float ddx = c.X - bl.X, ddy = c.Y - bl.Y;
                    float len = std::sqrt(ddx * ddx + ddy * ddy);
                    if (len > 1000.0f) {
                        Vector nl{ bl.X + ddx / len * 1500.0f * (float)dt,
                                   bl.Y + ddy / len * 1500.0f * (float)dt, bl.Z };
                        Call sl("/Script/Engine.Actor.K2_SetActorLocation");
                        if (sl.ok()) {
                            bool sweep = false, tp = true;
                            if (sl.set("NewLocation", nl) && sl.set("bSweep", sweep) && sl.set("bTeleport", tp))
                                sl.run(ac);
                        }
                    }
                }
            }
            if (!br.bus_unlocked && br.drop_start > 0 && now >= br.drop_start) {
                br.bus_unlocked = true;
                SetBool(session.state, "/Script/FortniteGame.FortGameStateAthena.bAircraftIsLocked", false);
                if (br.component)
                    SetBool(br.component, "/Script/FortniteGame.FortGameStateComponent_BattleRoyaleGamePhaseLogic.bAircraftIsLocked", false);
                BRSetGamePhaseStep(5); // BusFlying
                LOG("[BR] bus unlocked — jump button live (Erbium BusFlying)");
            }
            if (!br.drop_forced && br.drop_end > 0 && now >= br.drop_end && !session.aircraft_jumped) {
                br.drop_forced = true;
                LOG("[BR] drop window ended — forcing jump (Erbium OnAircraftExitedDropZone)");
                BRJump();
            }
            // [F73] Erbium: aircraft destroyed once FlightEndTime passes —
            // regardless of how the player exited (manual jump or forced).
            if (!br.flight_done && br.flight_end > 0 && now >= br.flight_end) {
                br.flight_done = true;
                Call d("/Script/Engine.Actor.K2_DestroyActor");
                if (d.ok()) d.run(ac);
                br.aircraft = nullptr;
                LOG("[BR] aircraft destroyed (flight over)");
            }
            // [F73] failsafe — flight over but the jump chain never landed:
            // restore the pawn so the player is never stuck hidden.
            if (br.flight_done && !session.aircraft_jumped && session.pawn && !IsBadPtr(session.pawn)) {
                Call hid("/Script/Engine.Actor.SetActorHiddenInGame");
                if (hid.ok()) { bool b = false; if (hid.set("bNewHidden", b)) hid.run(session.pawn); }
                Call col("/Script/Engine.Actor.SetActorEnableCollision");
                if (col.ok()) { bool b = true; if (col.set("bNewActorEnableCollision", b)) col.run(session.pawn); }
                session.aircraft_jumped = true;
                LOG("[BR] failsafe: pawn restored after flight window");
            }
        } else if (!br.flight_done) {
            // no aircraft (spawn refused) — Erbium bSkipAircraft path
            br.flight_done = true;
            br.safezones_start = now + 15.0f;
            BRSetGamePhase(4);     // SafeZones
            BRSetGamePhaseStep(6); // StormForming
            LOG("[BR] no aircraft — SafeZones in 15s (bSkipAircraft path)");
        }
        return;
    }

    if (br.phase == 4) { // SafeZones
        if (!br.zone_formed) {
            if (br.safezones_start > 0 && now >= br.safezones_start) {
                br.zone_formed = true;
                BRZoneTables();
                BRPickZoneCenters();
                BRSetupSafeZoneIndicator();
                BRStartNewSafeZonePhase(1, true);
            }
        } else if (br.indicator && !IsBadPtr(br.indicator)) {
            float startT = 0, finishT = 0;
            BRReadFloat(br.indicator, "/Script/FortniteGame.FortSafeZoneIndicator.SafeZoneStartShrinkTime", startT);
            BRReadFloat(br.indicator, "/Script/FortniteGame.FortSafeZoneIndicator.SafeZoneFinishShrinkTime", finishT);
            if (!br.zone_shrinking && startT > 0 && now >= startT && now < finishT) {
                br.zone_shrinking = true;
                if (void* frProp = GetProp("/Script/FortniteGame.FortSafeZoneIndicator.FutureReplicator")) {
                    int fro = Read<int>(frProp, profile::PropOffset);
                    if (fro > 4 && fro < 0x8000) Write<uint8_t>(br.indicator, fro - 4, 3);
                }
                Call osc("/Script/FortniteGame.FortSafeZoneIndicator.OnSafeZoneStateChange");
                if (osc.ok()) { osc.set_at(0, (uint8_t)3); osc.set_at(1, (uint8_t)0); osc.run(br.indicator); }
                BRSetGamePhaseStep(8); // StormShrinking
                LOG("[BR] zone shrinking (state 3)");
            }
            if (br.zone_shrinking && finishT > 0 && now >= finishT) {
                int cur = 0;
                if (void* cpProp = GetProp("/Script/FortniteGame.FortSafeZoneIndicator.CurrentPhase"))
                    cur = Read<int>(GetField(br.indicator, cpProp), 0);
                if (cur + 1 < br.zone_count) {
                    br.zone_shrinking = false;
                    BRStartNewSafeZonePhase(cur + 1, false);
                } else {
                    br.zone_shrinking = false; // final zone holds forever (Erbium)
                    BRWriteFloat(br.indicator, "/Script/FortniteGame.FortSafeZoneIndicator.SafeZoneFinishShrinkTime", finishT + 3600);
                }
            }
            // zone log (Erbium printf per change)
            int cur = 0;
            if (void* cpProp = GetProp("/Script/FortniteGame.FortSafeZoneIndicator.CurrentPhase"))
                cur = Read<int>(GetField(br.indicator, cpProp), 0);
            if (cur != br.last_logged_zone) {
                br.last_logged_zone = cur;
                Vector c{}; float r = 0;
                { Call gc("/Script/FortniteGame.FortSafeZoneIndicator.GetSafeZoneCenter");
                  if (gc.ok() && gc.run(br.indicator)) c = gc.result<Vector>(); }
                { Call gr("/Script/FortniteGame.FortSafeZoneIndicator.GetSafeZoneRadius");
                  if (gr.ok() && gr.run(br.indicator)) r = gr.result<float>(); }
                LOG("[Zone] phase=%d radius=%.0f center=(%.0f,%.0f,%.0f)", cur, r, c.X, c.Y, c.Z);
            }
        }
        if (mono - br.last_storm_tick >= 1.0) {
            br.last_storm_tick = mono;
            BRStormTick();
        }
        return;
    }
}

bool Athena::StackAdd(void* def, int count) {
    if (!def || !session.inventory || count < 1) return false;
    void* invProp   = GetProp("/Script/FortniteGame.FortInventory.Inventory");
    void* instProp  = GetProp("/Script/FortniteGame.FortItemList.ItemInstances");
    void* entryProp = GetProp("/Script/FortniteGame.FortItemList.ReplicatedEntries");
    void* worldItemEntryProp = GetProp("/Script/FortniteGame.FortWorldItem.ItemEntry");
    void* countProp = GetProp("/Script/FortniteGame.FortItemEntry.Count");
    if (!invProp || !instProp || !entryProp || !worldItemEntryProp || !countProp) return false;
    void* list = GetField(session.inventory, invProp);
    void* instArr  = GetField(list, instProp);
    void* entryArr = GetField(list, entryProp);
    Array insts = Read<Array>(instArr);
    Array reps  = Read<Array>(entryArr);
    int entryStride = Read<int>(entryProp, profile::PropElementSize);
    if (entryStride < 0x80 || entryStride > 0x400) return false;
    void* guidProp = GetProp("/Script/FortniteGame.FortItemEntry.ItemGuid");
    if (!guidProp) return false;

    for (int i = 0; i < insts.Count; ++i) {
        void* item = Read<void*>(insts.Data, (size_t)i * 8);
        if (!item || IsBadPtr(item) || ItemDefinitionOf(item) != def) continue;
        void* entry = GetField(item, worldItemEntryProp);
        if (!entry) return false;
        int32 cur = Read<int32>(GetField(entry, countProp), 0);
        int32 nv = cur + count;
        if (nv > 999) nv = 999;
        int added = nv - cur;
        if (added <= 0) return true;
        Write<int32>(GetField(entry, countProp), 0, nv);
        // Mirror into the replicated entry (inline structs, stride-based).
        Guid g = ItemGuidOf(item);
        for (int j = 0; j < reps.Count; ++j) {
            uint8_t* re = (uint8_t*)reps.Data + (size_t)j * entryStride;
            Guid rg = Read<Guid>(re + Read<int>(guidProp, profile::PropOffset), 0);
            if (memcmp(&rg, &g, sizeof(Guid)) == 0) {
                int32 rc = Read<int32>(re + Read<int>(countProp, profile::PropOffset), 0);
                int32 rn = rc + added; if (rn > 999) rn = 999;
                Write<int32>(re + Read<int>(countProp, profile::PropOffset), 0, rn);
                break;
            }
        }
        InventoryUpdate();
        ForceUpdateQuickBars();
        LOG("[Harvest] + %d %s (total %d)", added, ObjName(def).c_str(), nv);
        return true;
    }
    return false;
}

// [F49] Mirror of StackAdd for build resource COST (wood/stone/metal).
// Decrements the instance entry + the replicated entry, floors at 0.
bool Athena::StackRemove(void* def, int count) {
    if (!def || !session.inventory || count < 1) return false;
    void* invProp   = GetProp("/Script/FortniteGame.FortInventory.Inventory");
    void* instProp  = GetProp("/Script/FortniteGame.FortItemList.ItemInstances");
    void* entryProp = GetProp("/Script/FortniteGame.FortItemList.ReplicatedEntries");
    void* worldItemEntryProp = GetProp("/Script/FortniteGame.FortWorldItem.ItemEntry");
    void* countProp = GetProp("/Script/FortniteGame.FortItemEntry.Count");
    if (!invProp || !instProp || !entryProp || !worldItemEntryProp || !countProp) return false;
    void* list = GetField(session.inventory, invProp);
    void* instArr  = GetField(list, instProp);
    void* entryArr = GetField(list, entryProp);
    Array insts = Read<Array>(instArr);
    Array reps  = Read<Array>(entryArr);
    int entryStride = Read<int>(entryProp, profile::PropElementSize);
    if (entryStride < 0x80 || entryStride > 0x400) return false;
    void* guidProp = GetProp("/Script/FortniteGame.FortItemEntry.ItemGuid");
    if (!guidProp) return false;

    for (int i = 0; i < insts.Count; ++i) {
        void* item = Read<void*>(insts.Data, (size_t)i * 8);
        if (!item || IsBadPtr(item) || ItemDefinitionOf(item) != def) continue;
        void* entry = GetField(item, worldItemEntryProp);
        if (!entry) return false;
        int32 cur = Read<int32>(GetField(entry, countProp), 0);
        int32 nv = cur - count; if (nv < 0) nv = 0;
        int taken = cur - nv;
        if (taken <= 0) return false;
        Write<int32>(GetField(entry, countProp), 0, nv);
        Guid g = ItemGuidOf(item);
        for (int j = 0; j < reps.Count; ++j) {
            uint8_t* re = (uint8_t*)reps.Data + (size_t)j * entryStride;
            Guid rg = Read<Guid>(re + Read<int>(guidProp, profile::PropOffset), 0);
            if (memcmp(&rg, &g, sizeof(Guid)) == 0) {
                int32 rc = Read<int32>(re + Read<int>(countProp, profile::PropOffset), 0);
                int32 rn = rc - taken; if (rn < 0) rn = 0;
                Write<int32>(re + Read<int>(countProp, profile::PropOffset), 0, rn);
                break;
            }
        }
        InventoryUpdate();
        LOG("[Build] - %d %s (left %d)", taken, ObjName(def).c_str(), nv);
        return true;
    }
    return false;
}

// [F49] Shared harvest grant — resolve the actor's resource item, scale the
// yield by damage/max-health (curve override when resolvable), then stack it
// into the player's inventory. Used by both the health-delta watcher (world
// builds) and the OnDamageServer ProcessEvent interception (player builds).
void Athena::HarvestGrantFor(void* actor, float damage) {
    if (!actor || IsBadPtr(actor) || damage < 1.0f || !session.controller) return;
    uint8_t rtype = 0;
    if (void* rtProp = GetProp("/Script/FortniteGame.BuildingSMActor.ResourceType"))
        rtype = Read<uint8_t>(GetField(actor, rtProp), 0);
    void* def = nullptr;
    { Call kr("/Script/FortniteGame.FortKismetLibrary.K2_GetResourceItemDefinition");
      if (kr.ok()) { uint8_t in = rtype; if (kr.set("ResourceType", in) &&
          kr.run(FindObj("/Script/FortniteGame.Default__FortKismetLibrary"))) def = kr.result<void*>(); } }
    if (!def) return;
    // Yield per full object: curve override if resolvable, else 60.
    float fullYield = 60.0f;
    {
        void* overrideProp = GetProp("/Script/FortniteGame.BuildingSMActor.BuildingResourceAmountOverride");
        if (overrideProp) {
            uint8_t* handle = (uint8_t*)GetField(actor, overrideProp);
            void* curveTable = Read<void*>(handle, 0);
            int32_t rowIdx = Read<int32_t>(handle, 8);
            if (curveTable && !IsBadPtr(curveTable) && rowIdx != 0) {
                Call ev("/Script/Engine.DataTableFunctionLibrary.EvaluateCurveTableRow");
                if (ev.ok()) {
                    uint8_t rowName[8] = {};
                    memcpy(rowName, &rowIdx, 4);
                    ev.set("CurveTable", curveTable);
                    ev.set_bytes("RowName", rowName, 8);
                    float xy = 0.0f;
                    ev.set("InXY", xy);
                    if (ev.run(FindObj("/Script/Engine.Default__DataTableFunctionLibrary"))) {
                        float outXY = ev.get<float>("OutXY");
                        if (outXY > 0.0f && outXY < 2000.0f) fullYield = outXY;
                    }
                }
            }
        }
    }
    void* maxProp = GetProp("/Script/FortniteGame.BuildingActor.MaxHealth");
    float maxhp = maxProp ? Read<float>(GetField(actor, maxProp), 0) : 0.0f;
    if (maxhp < 1.0f) maxhp = 500.0f;
    int resCount = (int)std::lround(fullYield * damage / maxhp);
    if (resCount < 1) resCount = 1;
    if (resCount > 60) resCount = 60;
    if (!StackAdd(def, resCount))
        GiveItem(def, resCount, -1, 0, true);   // first pick: introduce into inventory
}

void Athena::HarvestTick() {
    if (!session.harvest_enabled || !session.pawn || !session.world) return;
    double now = Monotonic();
    void* smCls = FindObj("/Script/FortniteGame.BuildingSMActor");
    if (!smCls) return;

    // Rebuild the nearby-harvestable cache every 8s (GObjects scan limited
    // to instances within 300 m of the pawn).
    if (now - session.last_harvest_scan > 8.0) {
        session.last_harvest_scan = now;
        session.harvest_cache.clear();
        GObjects_t* go = GObjectArray();
        if (go) {
            Vector origin{};
            if (ActorLocation(session.pawn, origin)) {
                Call loc("/Script/Engine.Actor.K2_GetActorLocation");
                for (int32 i = 0; i < go->NumElements && session.harvest_cache.size() < 256; ++i) {
                    void* object = go->GetById((uint32)i);
                    if (!object || IsBadPtr(object) || IsCDO(object)) continue;
                    if (!IsA(object, smCls)) continue;
                    if (GetBool(object, "/Script/FortniteGame.BuildingActor.bPlayerPlaced")) continue;
                    if (loc.ok() && loc.run(object)) {
                        Vector at = loc.result<Vector>();
                        double dx = at.X - origin.X, dy = at.Y - origin.Y, dz = at.Z - origin.Z;
                        if (dx*dx + dy*dy + dz*dz > 300.0*300.0) continue;
                        session.harvest_cache.push_back(object);
                    }
                }
            }
        }
    }

    // Watch health deltas — a drop between ticks = a pickaxe (or weapon) hit.
    void* healthProp = GetProp("/Script/FortniteGame.BuildingActor.Health");
    if (!healthProp) return;
    for (void* actor : session.harvest_cache) {
        if (!actor || IsBadPtr(actor)) continue;
        float hp = Read<float>(GetField(actor, healthProp), 0);
        auto it = session.harvest_hp.find(actor);
        if (it == session.harvest_hp.end()) { session.harvest_hp[actor] = hp; continue; }
        float prev = it->second;
        it->second = hp;
        float damage = prev - hp;
        if (damage < 1.0f || damage > prev) continue;   // heal or one-shot destroy skip
        // [F49] shared grant path (resource def + curve + stack)
        HarvestGrantFor(actor, damage);
    }
    // Drop destroyed actors from the map.
    if (session.harvest_hp.size() > 1024) session.harvest_hp.clear();
}

// ============================================================================
// [F40] DOOR AUDIT — keep player-opened doors open and re-sync jammed ones.
// Native native auto-close / client desync can flip bLocalDoorOpen back; we
// re-apply both state bits and fire OnRep so the mesh animates. Also heals
// the "door stuck after sprint-push" case (SlamOpen trigger desync).
void Athena::DoorAudit() {
    if (!session.doors_hold || !session.world) return;
    double now = Monotonic();
    static void* wallCls = nullptr;
    if (!wallCls) wallCls = FindObj("/Script/FortniteGame.BuildingWall");
    if (!wallCls) return;

    // One-time cache of walls that own a door (config bits or DoorComponent);
    // refreshed every 60s so late-streamed walls get picked up.
    static double lastDoorScan = 0;
    if (session.door_cache.empty() || now - lastDoorScan > 60.0) {
        lastDoorScan = now;
        session.door_cache.clear();
        GObjects_t* go = GObjectArray();
        if (!go) return;
        void* doorCompProp = GetProp("/Script/FortniteGame.BuildingWall.DoorComponent");
        for (int32 i = 0; i < go->NumElements && session.door_cache.size() < 4096; ++i) {
            void* object = go->GetById((uint32)i);
            if (!object || IsBadPtr(object) || IsCDO(object)) continue;
            if (!IsA(object, wallCls)) continue;
            uint8_t bits = Read<uint8_t>(object, profile::BuildingWall_bDoorOpen_Byte);
            bool hasDoor = (bits & 0x13) != 0;   // swinging | sliding | double
            if (!hasDoor && doorCompProp) {
                void* dc = GetField(object, doorCompProp);
                hasDoor = dc && !IsBadPtr(dc);
            }
            if (hasDoor) session.door_cache.push_back(object);
        }
        static size_t lastLoggedDoorCache = 0;
        if (session.door_cache.size() != lastLoggedDoorCache) {
            lastLoggedDoorCache = session.door_cache.size();
            LOG("[Door] audit cache: %zu walls with doors", session.door_cache.size());
        }
    }

    constexpr uint8_t kStateMask = 0x60;
    int fixed = 0;
    for (void* wall : session.door_cache) {
        if (!wall || IsBadPtr(wall)) continue;
        uint8_t* byte = (uint8_t*)wall + profile::BuildingWall_bDoorOpen_Byte;
        uint8_t cur = *byte;
        bool serverOpen = (cur & 0x20) != 0;
        bool localOpen  = (cur & 0x40) != 0;
        bool everOpen   = session.doors_ever_open.count(wall) != 0;
        bool want = serverOpen || everOpen;   // hold player-opened doors open
        if (localOpen != want || (want && !serverOpen)) {
            uint8_t next = (uint8_t)(cur & ~kStateMask);
            if (want) next |= kStateMask;
            *byte = next;
            Call r("/Script/FortniteGame.BuildingWall.OnRep_bDoorOpen");
            if (r.ok()) r.run(wall);
            ++fixed;
        }
    }
    if (fixed > 0) LOG("[Door] audit re-synced %d doors", fixed);
}

// ============================================================================
// [F68] BODY — Erbium-pure cosmetics: NEVER swap SkeletalMesh directly. The
// previous fix replaced the assembled CustomCharacterPart body with a bare
// base skeleton (M_MED_Lancelot) — the "sick, unplayable" body on device.
// Erbium assembles the body through parts only (ServerChoosePart +
// ApplyCharacterCosmetics + visualization OnReps — already ported in
// ShowSkin/LoadDefaultCosmetics). Here we only:
//   (a) re-run the part pipeline when no parts ever landed,
//   (b) restore a MISSING AnimScriptInstance (locomotion),
//   (c) keep the body visible.
void Athena::FixBodyAndAnimations() {
    if (session.body_fixed || !session.pawn) return;
    void* mesh = GetObjField(session.pawn, "/Script/Engine.Character.Mesh");
    if (!mesh || IsBadPtr(mesh)) return;

    if (session.character_parts.empty()) {
        LoadProfileCosmetics();          // locker -> part pipeline -> defaults
        if (session.character_parts.empty()) return;   // assets not resident yet — retry next pass
    }
    void* animInst = GetObjField(mesh, "/Script/Engine.SkeletalMeshComponent.AnimScriptInstance");
    if (!animInst) {
        void* animBP = FindLoadedObjectOfType("/Script/Engine.AnimBlueprintGeneratedClass", "Fortnite_M_Avg_Player_AnimBlueprint");
        if (!animBP) animBP = FindLoadedObjectOfType("/Script/Engine.AnimBlueprintGeneratedClass", "M_MED_Lancelot_AnimBP");
        if (animBP) {
            Call a("/Script/Engine.SkeletalMeshComponent.SetAnimClass");
            if (a.ok() && a.set("NewClass", animBP) && a.run(mesh))
                LOG("[Body] AnimScriptInstance restored (locomotion)");
        }
    }
    Call body("/Script/FortniteGame.FortPlayerPawnAthena.SetCharacterBodyVisibilityForPossession");
    if (body.ok()) { bool bShow = true; if (body.set("bVisible", bShow)) body.run(session.pawn); }
    Call hid("/Script/Engine.Actor.SetActorHiddenInGame");
    if (hid.ok()) { bool h = false; if (hid.set("bNewHidden", h)) hid.run(session.pawn); }

    session.body_fixed = true;
    LOG("[Body] ✅ body via %zu character parts — no direct mesh swap (Erbium)",
        session.character_parts.size());
}

// ============================================================================
// [F42] CLIMB — 21.30 has NO mantle system (ledger climb shipped in 23.00 /
// Chapter 4; the 21.30 SDK has zero Mantle classes). Local substitute: while
// the jump button is held and the pawn faces a wall, trace forward; if the
// wall top is within reach, launch upward — a practical "climb" boost.
void Athena::ClimbTick() {
    if (!session.climb_enabled || !session.pawn) return;
    double now = Monotonic();
    if (now - session.last_climb_check < 0.18) return;
    session.last_climb_check = now;

    // Jump held? bPressedJump is BitIndex 0x02 in its byte (Dumper-7).
    void* jumpProp = GetProp("/Script/Engine.Character.bPressedJump");
    if (!jumpProp) return;
    int jumpOff = Read<int>(jumpProp, profile::PropOffset);
    if (jumpOff <= 0 || jumpOff > 0x2000) return;
    uint8_t bits = Read<uint8_t>((char*)session.pawn + jumpOff, 0);
    if (!(bits & 0x04)) return;

    // Falling (MODE_Falling = 3) — climbing happens mid-jump against a wall.
    void* cm = GetObjField(session.pawn, "/Script/Engine.Character.CharacterMovement");
    if (!cm || IsBadPtr(cm)) return;
    uint8_t mode = 0;
    if (void* modeProp = GetProp("/Script/Engine.CharacterMovementComponent.MovementMode"))
        mode = Read<uint8_t>(GetField(cm, modeProp), 0);
    if (mode != 3) return;

    Vector origin{};
    if (!ActorLocation(session.pawn, origin)) return;
    // Facing direction from control rotation (simplified: use pawn velocity
    // direction if moving, else root forward).
    double fx = 0.0, fy = 0.0;
    { Call v("/Script/Engine.Actor.GetVelocity");
      if (v.ok() && v.run(session.pawn)) {
          Vector vel = v.result<Vector>();
          double len = std::sqrt(vel.X*vel.X + vel.Y*vel.Y);
          if (len > 40.0) { fx = vel.X / len; fy = vel.Y / len; }
      } }
    if (fx == 0.0 && fy == 0.0) {   // fall back to actor forward vector
        Call fwd("/Script/Engine.Actor.GetActorForwardVector");
        if (!fwd.ok() || !fwd.run(session.pawn)) return;
        Vector f = fwd.result<Vector>();
        double len = std::sqrt(f.X*f.X + f.Y*f.Y);
        if (len < 0.01) return;
        fx = f.X / len; fy = f.Y / len;
    }

    // Trace from chest height forward 1.4 m.
    Vector start = origin; start.Z += 40.0;
    Vector end = start;    end.X += fx * 140.0; end.Y += fy * 140.0;
    bool blocking = false;
    {
        Call t("/Script/Engine.KismetSystemLibrary.LineTraceSingle");
        if (!t.ok()) return;
        Array ignore{};   // empty array (Count=0, Data=null) is safe for the native
        uint8_t channel = 1;   // TraceTypeQuery1 (Visibility)
        uint8_t debug = 0;
        bool complex = false, ignoreSelf = true;
        uint8_t hit[224] = {};
        if (!t.set("WorldContextObject", session.world)) return;
        t.set("Start", start);
        t.set("End", end);
        t.set("TraceChannel", channel);
        t.set("bTraceComplex", complex);
        t.set_bytes("ActorsToIgnore", &ignore, sizeof(Array));
        t.set("DrawDebugType", debug);
        t.set_bytes("OutHit", hit, sizeof(hit));
        t.set("bIgnoreSelf", ignoreSelf);
        if (!t.run(FindObj("/Script/Engine.Default__KismetSystemLibrary"))) return;
        blocking = hit[0] & 0x01;   // bBlockingHit bit0 of FHitResult
    }
    if (!blocking) return;

    // Upward launch — XY velocity is preserved (bXYOverride=false).
    { Call l("/Script/Engine.Character.LaunchCharacter");
      if (!l.ok()) return;
      Vector v{}; v.Z = 520.0;
      bool xy = false, z = true;
      if (l.set("LaunchVelocity", v) && l.set("bXYOverride", xy) && l.set("bZOverride", z))
          l.run(session.pawn); }
}

// ============================================================================
// [F45] FLOOR LOOT — the map's Tiered_Athena_FloorLoot containers live in
// streaming levels that load async; the first one-shot ran too early and
// found 0 actors. Retry until containers actually surface (max 8 attempts).
void Athena::SpawnFloorLootRetry() {
    if (session.loot_attempts >= 8) return;
    ++session.loot_attempts;
    SpawnFloorLoot();
}

// [F24/F58] Inventory drop — the crash on device came from (a) the native
// ServerAttemptInventoryDrop RPC running into the dead standalone server
// path and (b) removing the ReplicatedEntry by INDEX while the two arrays
// are not index-aligned. Now: the RPC is swallowed in pe_hook, the visual
// pickup spawns FIRST, and both arrays are reconciled by GUID.
void Athena::DropInventoryItem(Guid guid, int count) {
    if (!session.inventory) return;
    if (!(guid.A | guid.B | guid.C | guid.D)) return;
    void* invProp = GetProp("/Script/FortniteGame.FortInventory.Inventory");
    void* instProp = GetProp("/Script/FortniteGame.FortItemList.ItemInstances");
    void* entryProp = GetProp("/Script/FortniteGame.FortItemList.ReplicatedEntries");
    void* guidProp = GetProp("/Script/FortniteGame.FortItemEntry.ItemGuid");
    if (!invProp || !instProp || !entryProp || !guidProp) return;
    void* list = GetField(session.inventory, invProp);
    void* instArr = GetField(list, instProp);
    void* entryArr = GetField(list, entryProp);
    int entryStride = Read<int>(entryProp, profile::PropElementSize);
    if (entryStride < 0x80 || entryStride > 0x400) return;
    Array arr = Read<Array>(instArr);
    Array entries = Read<Array>(entryArr);
    for (int i = 0; i < arr.Count; ++i) {
        void* inst = Read<void*>(arr.Data, (size_t)i * 8);
        if (!inst) continue;
        Guid cand = ItemGuidOf(inst);
        if (memcmp(&cand, &guid, sizeof(Guid)) != 0) continue;
        // 1) the visual pickup goes out before anything is mutated
        void* def = ItemDefinitionOf(inst);
        Vector loc{};
        if (session.pawn) ActorLocation(session.pawn, loc);
        loc.Z += 60.0;
        int entryIdx = -1;
        for (int j = 0; j < entries.Count; ++j) {
            uint8_t* re = (uint8_t*)entries.Data + (size_t)j * entryStride;
            Guid rg = Read<Guid>(re + Read<int>(guidProp, profile::PropOffset), 0);
            if (!memcmp(&rg, &guid, sizeof(Guid))) { entryIdx = j; break; }
        }
        int currentCount = 1;
        if (void* wProp = GetProp("/Script/FortniteGame.FortWorldItem.ItemEntry"))
            if (void* entry = GetField(inst, wProp))
                if (void* cntProp = GetProp("/Script/FortniteGame.FortItemEntry.Count"))
                    currentCount = Read<int32>(GetField(entry, cntProp), 0);
        int dropped = count > 0 ? std::min(count, currentCount > 0 ? currentCount : count) : 1;
        if (def) SpawnPickup(def, dropped, loc, 16 /*Tossed*/, 10 /*TossedByPlayer*/, true);
        // 2) remove the instance and ITS OWN replicated entry (by GUID index)
        Fn<void(*)(void*,void*,int)>(profile::GenericArrayRemove)(instArr, instProp, i);
        if (entryIdx >= 0)
            Fn<void(*)(void*,void*,int)>(profile::GenericArrayRemove)(entryArr, entryProp, entryIdx);
        // 3) clear our slot bookkeeping; if it was equipped, go back to the pickaxe
        bool wasEquipped = false;
        for (int s = 0; s < 6; ++s) if (session.slots[(size_t)s] == inst) { session.slots[(size_t)s] = nullptr; wasEquipped = true; }
        for (int s = 0; s < 6; ++s) if (session.secondary_slots[(size_t)s] == inst) session.secondary_slots[(size_t)s] = nullptr;
        if (wasEquipped) ActivateQuickBarSlot(0, 0);
        InventoryUpdate();
        ForceUpdateQuickBars();
        LOG("[Drop] item dropped count=%d (entry %d)", dropped, entryIdx);
        return;
    }
    LOG("[Drop] guid not found in inventory");
}

// ============================================================================
// LOOT — [F20] local rolling (Era core search_container ported to 21.30)
// ============================================================================
namespace local_loot {
    struct Row  { const char* name; float weight; int count; };
    struct Tier { const char* group; int weaponsMin, weaponsMax, ammoMin, ammoMax, resources; float rarityBoost; };

    // Weapon short names — every row is resolved through ByShortName and
    // silently skipped when the asset is absent, so the table is safe to
    // extend for other seasons.
    static const Row weapons[] = {
        {"WID_Assault_Auto_Athena_C_Ore_T03",        30, 1},
        {"WID_Shotgun_Standard_Athena_C_Ore_T03",    25, 1},
        {"WID_Shotgun_SemiAuto_Athena_C_Ore_T03",    20, 1},
        {"WID_Sniper_Standard_Athena_C_Ore_T03",     12, 1},
        {"WID_Assault_Semi_Athena_C_Ore_T03",        12, 1},
        {"WID_SMG_Tactical_Athena_C_Ore_T03",        12, 1},
        {"WID_Launcher_Rocket_Athena_C_Ore_T03",      4, 1},
        {"WID_Minigun_Athena_C_Ore_T03",              3, 1},
    };
    static const Row ammo[] = {
        {"AthenaAmmoDataBulletsLight",   30, 60},
        {"AthenaAmmoDataBulletsMedium",  34, 60},
        {"AthenaAmmoDataBulletsHeavy",   14, 30},
        {"AthenaAmmoDataShells",         18, 24},
        {"AmmoDataRockets",               4, 4},
    };
    static const Row resources[] = {
        {"WoodItemData",  40, 50},
        {"StoneItemData", 34, 50},
        {"MetalItemData", 26, 50},
    };
    // Loot-tier-group → package mapping (21.30 SearchLootTierGroup names).
    static const Tier tiers[] = {
        {"Loot_AthenaTreasure",       2, 3, 2, 3, 1, 1.6f},
        {"Loot_Treasure",             2, 3, 2, 3, 1, 1.6f},   // legacy alias
        {"Loot_AthenaAmmoLarge",      0, 0, 3, 4, 0, 0.0f},
        {"Loot_AthenaAmmoSmall",      0, 0, 2, 3, 0, 0.0f},
        {"Loot_AthenaFloorLoot",      1, 1, 1, 2, 1, 0.0f},
    };

    static float random01() {
        static uint32_t state = 0;
        if (!state) state = (uint32_t)(Monotonic() * 1000000.0) | 1;
        state ^= state << 13; state ^= state >> 17; state ^= state << 5;
        return float(state >> 8) / 16777216.0f;
    }
    static void* resolve(const Row& row) {
        std::string shortName(row.name);
        void* def = ByShortName(shortName);
        if (def) return def;
        // Try the classic content paths before giving up.
        const char* prefixes[] = {
            "/Game/Athena/Items/Weapons/",
            "/Game/Athena/Items/Ammo/",
            "/Game/Items/ResourcePickups/",
            nullptr
        };
        for (int i = 0; prefixes[i]; ++i) {
            std::string path = std::string(prefixes[i]) + shortName + "." + shortName;
            def = LoadAsset(path);
            if (def) return def;
        }
        return nullptr;
    }
    static void* pickWeapon(float boost) {
        float total = 0;
        for (auto& w : weapons) total += w.weight * (1.0f + boost * (w.weight < 10.0f ? 2.0f : 0.0f));
        float pick = random01() * total;
        for (auto& w : weapons) {
            float weight = w.weight * (1.0f + boost * (w.weight < 10.0f ? 2.0f : 0.0f));
            pick -= weight;
            if (pick < 0) return resolve(w);
        }
        return nullptr;
    }
    static void* pickRow(const Row* table, size_t n) {
        float total = 0;
        for (size_t i = 0; i < n; ++i) total += table[i].weight;
        float pick = random01() * total;
        for (size_t i = 0; i < n; ++i) {
            pick -= table[i].weight;
            if (pick < 0) return resolve(table[i]);
        }
        return nullptr;
    }
    static int rollBetween(int lo, int hi) { return lo + (int)(random01() * float(hi - lo + 1)); }
}

bool Athena::SearchContainer(void* actor, const Vector& location) {
    void* searchedProp = GetProp("/Script/FortniteGame.BuildingContainer.bAlreadySearched");
    if (!actor || !searchedProp) return false;
    if (GetBool(actor, "/Script/FortniteGame.BuildingContainer.bAlreadySearched")) return true;

    // Read the container's loot tier group (FName).
    std::string group;
    void* groupProp = GetProp("/Script/FortniteGame.BuildingContainer.SearchLootTierGroup");
    if (groupProp) {
        FStringNative text{};
        Fn<void(*)(const void*, FStringNative*)>(profile::NameAppendString)(GetField(actor, groupProp), &text);
        group = Narrow(text.Data, 128);
        if (text.Data) Fn<void(*)(void*)>(profile::Free)(text.Data);
    }
    if (group == "Loot_Treasure") group = "Loot_AthenaTreasure";
    if (group == "Loot_Ammo") group = "Loot_AthenaAmmoLarge";

    const local_loot::Tier* tier = nullptr;
    for (auto& t : local_loot::tiers)
        if (group == t.group) { tier = &t; break; }
    // [F65] Erbium never refuses a search: an unknown 21.30 group (faction
    // chests, coolers, custom map containers…) falls back to the treasure
    // tier so EVERY container type opens. The old "group not ported" refusal
    // was one of the "no type of them responds" causes.
    if (!tier) {
        LOG("[Loot] group '%s' not tabled — treasure fallback", group.c_str());
        for (auto& t : local_loot::tiers)
            if (!strcmp(t.group, "Loot_AthenaTreasure")) { tier = &t; break; }
        if (!tier) return false;
    }

    struct Drop { void* def; int count; };
    std::vector<Drop> drops;
    int weapons = tier->weaponsMax > tier->weaponsMin ? local_loot::rollBetween(tier->weaponsMin, tier->weaponsMax) : tier->weaponsMin;
    int ammo = tier->ammoMax > tier->ammoMin ? local_loot::rollBetween(tier->ammoMin, tier->ammoMax) : tier->ammoMin;
    for (int i = 0; i < weapons; ++i)
        if (void* def = local_loot::pickWeapon(tier->rarityBoost)) drops.push_back({def, 1});
    for (int i = 0; i < ammo; ++i)
        if (void* def = local_loot::pickRow(local_loot::ammo, sizeof(local_loot::ammo) / sizeof(local_loot::Row))) {
            int cnt = 30 + (int)(local_loot::random01() * 60.0f);
            drops.push_back({def, cnt});
        }
    for (int i = 0; i < tier->resources; ++i)
        if (void* def = local_loot::pickRow(local_loot::resources, sizeof(local_loot::resources) / sizeof(local_loot::Row)))
            drops.push_back({def, 50});
    if (drops.empty()) return false;

    // Resolve/spawn every pickup first; roll back partial spawns so a retry
    // cannot duplicate loot (Era core guarantee).
    std::vector<void*> spawned;
    for (size_t i = 0; i < drops.size(); ++i) {
        Vector loc{location.X + 60.0 + double(i % 3) * 35.0,
                   location.Y + double(i / 3) * 35.0,
                   location.Z + 50.0};
        // sourceFlags: Container(4) | Player(1); spawnSource: Chest(2)/AmmoBox(4)/FloorLoot? use LootDrop(9) for generic
        uint8_t spawnSource = 9; // LootDrop
        if (group == "Loot_AthenaTreasure") spawnSource = 2; // Chest
        if (group.find("Ammo") != std::string::npos) spawnSource = 4; // AmmoBox
        void* pickup = nullptr;
        {
            void* klass = FindObj("/Script/FortniteGame.FortPickupAthena");
            Transform t{};
            t.Location[0] = loc.X; t.Location[1] = loc.Y; t.Location[2] = loc.Z;
            pickup = klass ? SpawnActor(session.world, klass, nullptr, t) : nullptr;
            if (pickup) {
                void* entry = PickupEntry(pickup);
                void* defProp = GetProp("/Script/FortniteGame.FortItemEntry.ItemDefinition");
                void* cntProp = GetProp("/Script/FortniteGame.FortItemEntry.Count");
                if (entry && defProp) Write<void*>(GetField(entry, defProp), 0, drops[i].def);
                if (entry && cntProp) Write<int>(GetField(entry, cntProp), 0, drops[i].count);
                Write<uint8_t>(pickup, profile::AFortPickup_SourceFlags, 4);
                Write<uint8_t>(pickup, profile::AFortPickup_SpawnSource, spawnSource);
                Call rep("/Script/FortniteGame.FortPickup.OnRep_PrimaryPickupItemEntry");
                if (rep.ok()) rep.run(pickup);
            }
        }
        if (!pickup) {
            for (void* previous : spawned) { Call d("/Script/Engine.Actor.K2_DestroyActor"); if (d.ok()) d.run(previous); }
            LOG("Container pickup spawn failed; search not consumed");
            return true;
        }
        spawned.push_back(pickup);
    }
    SetBool(actor, "/Script/FortniteGame.BuildingContainer.bAlreadySearched", true);
    Call r1("/Script/FortniteGame.BuildingContainer.OnRep_bAlreadySearched"); if (r1.ok()) r1.run(actor);
    Call r2("/Script/FortniteGame.BuildingContainer.BounceContainer"); if (r2.ok()) r2.run(actor);
    LOG("[Loot] ✅ Container search %s spawned=%zu", group.c_str(), spawned.size());
    return true;
}

void Athena::Loot(void* actor) {
    if (!actor || !session.pawn) return;
    LOG("[Loot] Opening %s", ObjName(Read<void*>(actor, profile::ObjectClass)).c_str());
    Vector location{};
    bool haveLoc = ActorLocation(actor, location);

    // [F56] Native BP_SpawnLoot FIRST — it rolls the real cooked loot tables
    // (correct items for the chest tier). Success is verified by the native
    // flag it sets; only when that does not land do we fall back to the
    // offline local roll.
    {
        Call spawn("/Script/FortniteGame.BuildingContainer.BP_SpawnLoot");
        if (spawn.ok() && spawn.set("PlayerPawn", session.pawn)) {
            if (spawn.run(actor)) {
                if (GetBool(actor, "/Script/FortniteGame.BuildingContainer.bAlreadySearched")) {
                    Call r2("/Script/FortniteGame.BuildingContainer.BounceContainer"); if (r2.ok()) r2.run(actor);
                    LOG("[Loot] ✅ native BP_SpawnLoot path");
                    return;
                }
                LOG("[Loot] native path ran without searching — local fallback");
            }
        }
    }
    if (haveLoc && SearchContainer(actor, location)) return;
    LOG("[Loot] ❌ container search failed (local+native)");
}

void Athena::SpawnFloorLoot() {
    LOG("[Loot] Spawning floor loot...");
    GObjects_t* go = GObjectArray();
    void* bcCls = FindObj("/Script/FortniteGame.BuildingContainer");
    if (!go || !bcCls || !session.pawn) { LOG("[Loot] scanner unavailable"); return; }
    // [F56] The hardcoded "/Game/.../Tiered_Athena_FloorLoot_01_C" names do
    // not exist in 21.30 (dumper confirms: no FloorLoot package in the FName
    // pool — mount paths differ). Find floor-loot containers by their CLASS
    // NAME among live BuildingContainer instances instead, and roll them with
    // the native BP_SpawnLoot (real cooked tables) first.
    int total = 0, seen = 0;
    for (int32 i = 0; i < go->NumElements && total < 40; ++i) {
        void* c = go->GetById((uint32)i);
        if (!c || IsBadPtr(c) || IsCDO(c)) continue;
        void* oc = Read<void*>(c, profile::ObjectClass);
        if (!oc || IsBadPtr(oc) || !SubclassOfFast(oc, bcCls)) continue;
        std::string cn = ObjName(oc);
        std::transform(cn.begin(), cn.end(), cn.begin(), ::tolower);
        if (cn.find("floorloot") == std::string::npos) continue;
        ++seen;
        if (GetBool(c, "/Script/FortniteGame.BuildingContainer.bAlreadySearched")) continue;
        Vector loc{};
        if (!ActorLocation(c, loc)) continue;
        bool ok = false;
        {   // native first
            Call bp("/Script/FortniteGame.BuildingContainer.BP_SpawnLoot");
            if (bp.ok() && bp.set("PlayerPawn", session.pawn) && bp.run(c) &&
                GetBool(c, "/Script/FortniteGame.BuildingContainer.bAlreadySearched")) ok = true;
        }
        if (!ok) ok = SearchContainer(c, loc);
        if (ok) {
            SetBool(c, "/Script/FortniteGame.BuildingContainer.bAlreadySearched", true);
            Call r1("/Script/FortniteGame.BuildingContainer.OnRep_bAlreadySearched"); if (r1.ok()) r1.run(c);
            ++total;
        }
    }
    LOG("[Loot] ✅ floor loot pass: spawned=%d floorloot-containers=%zu", total, (size_t)seen);
    // [F45] stop retrying once the map's floor-loot containers are accounted
    // for (spawned or already searched) — but keep the number honest.
    session.loot_found_total = (seen > 0) ? (total > 0 ? total : 1) : 0;
}

// ============================================================================
// [F47/F48] INTERACTION — the mobile HUD prompt only appears when the
// interaction component finds a candidate; on this offline setup it never
// does, so the character "cannot interact with anything". We now:
//   1. scan for the nearest chest/vehicle ourselves and write it into
//      FortControllerComponent_Interaction.InteractActor (HUD can show it),
//   2. auto-execute the interaction on close proximity (era.auto on|off),
//   3. route real ServerAttemptInteract RPCs through the same table.
bool Athena::DoInteract(void* actor) {
    if (!actor || IsBadPtr(actor) || !session.pawn) return false;
    void* cls = Read<void*>(actor, profile::ObjectClass);
    std::string cn = ObjName(cls);
    void* bcCls   = FindObj("/Script/FortniteGame.BuildingContainer");
    void* vehCls  = FindObj("/Script/FortniteGame.FortAthenaVehicle");
    void* wallCls = FindObj("/Script/FortniteGame.BuildingWall");
    void* bActCls = FindObj("/Script/FortniteGame.BuildingActor");
    LOG("[Interact] -> %s (%s)", cn.c_str(), ObjName(actor).c_str());
    if (bcCls && SubclassOfFast(cls, bcCls)) { Loot(actor); return true; }
    if (vehCls && SubclassOfFast(cls, vehCls)) return EnterVehicle(actor);
    if (wallCls && SubclassOfFast(cls, wallCls)) { HandleDoor(actor); return true; }
    if (bActCls && SubclassOfFast(cls, bActCls)) {
        // [F16] generic building interaction via BP override, then the native
        // internal attempt (covers props with native-only interaction).
        Call interact("/Script/FortniteGame.BuildingActor.BlueprintOnInteract", actor);
        if (interact.ok() && interact.set("InteractingPawn", session.pawn) && interact.run(actor)) return true;
        if (void* comp = GetInteractionComp()) {
            Call native("/Script/FortniteGame.FortControllerComponent_Interaction.BP_ServerAttemptInteractInternal", comp);
            if (native.ok()) {
                float dist = 200.0f;
                uint8_t itype = 1, being = 0;
                void* nullObj = nullptr;
                if (native.set("InteractingPawn", session.pawn) && native.set("InInteractDistance", dist) &&
                    native.set("ReceivingActor", actor) && native.set("InteractType", itype) &&
                    native.set("OptionalObjectData", nullObj) && native.set("InteractionBeingAttempted", being) &&
                    native.run(comp)) return true;
            }
        }
    }
    // [F76] generic BP interaction — vending/SC machines, forage plants,
    // hiding props, NPC pawns: everything that implements its OWN
    // BlueprintOnInteract outside the BuildingActor tree. The override walk
    // resolves the function on the target's own class and the receiver
    // validation rejects targets that never implemented it (silent no-op).
    {
        Call bi("/Script/FortniteGame.BuildingActor.BlueprintOnInteract", actor);
        if (bi.ok() && bi.set("InteractingPawn", session.pawn) && bi.run(actor)) {
            LOG("[Interact] ✅ BP interact on %s", cn.c_str());
            return true;
        }
    }
    return false;
}

// [F47] Resolve the controller's interaction component (add it if the
// offline-possessed controller never created one — same fallback as the
// TacticalSprint component).
void* Athena::GetInteractionComp() {
    if (session.interact_comp && !IsBadPtr(session.interact_comp)) return session.interact_comp;
    if (!session.controller) return nullptr;
    void* cls = FindObj("/Script/FortniteGame.FortControllerComponent_Interaction");
    if (!cls) return nullptr;
    Call get("/Script/Engine.Actor.GetComponentByClass");
    if (get.ok() && get.set("ComponentClass", cls) && get.run(session.controller))
        session.interact_comp = get.result<void*>();
    if (!session.interact_comp || IsBadPtr(session.interact_comp)) {
        Call add("/Script/Engine.Actor.AddComponentByClass");
        if (add.ok()) {
            bool manualAttachment = false, deferred = false;
            Transform rel{};
            void* klass = cls;
            if ((add.set("Class", klass) || add.set("Class_0", klass)) &&
                add.set("bManualAttachment", manualAttachment) && add.set("RelativeTransform", rel) &&
                add.set("bDeferredFinish", deferred) && add.run(session.controller)) {
                session.interact_comp = add.result<void*>();
                LOG("[Interact] interaction component added: %p", session.interact_comp);
            }
        }
    }
    if (session.interact_comp && !IsBadPtr(session.interact_comp)) return session.interact_comp;
    session.interact_comp = nullptr;
    return nullptr;
}

// [F48] Vehicle entry — ForceSeatSinglePawn is the authoritative seat logic
// (occupancy check, attach, OnPawnEnterVehicle). Tries seats 0..3 so cargo
// seats are used when the driver seat is taken.
bool Athena::EnterVehicle(void* veh) {
    if (!veh || IsBadPtr(veh) || !session.pawn) return false;
    void* vehCls = FindObj("/Script/FortniteGame.FortAthenaVehicle");
    if (!vehCls || !IsA(veh, vehCls)) { LOG("[Veh] not a vehicle"); return false; }
    // GA_AthenaEnterVehicle drives the native input state once granted —
    // harmless when it is not (the seat call below is authoritative).
    if (void* ga = FindAbilityClassNamed("GA_AthenaEnterVehicle_C")) GrantAbility(ga);
    for (int seat = 0; seat < 4; ++seat) {
        Call f("/Script/FortniteGame.FortAthenaVehicle.ForceSeatSinglePawn");
        if (!f.ok()) { LOG("[Veh] ForceSeatSinglePawn missing"); return false; }
        int idx = seat;
        if (!f.set("PawnToSeat", session.pawn) || !f.set("SeatIndex", idx)) break;
        if (!f.run(veh)) continue;
        if (f.result<bool>()) {
            LOG("[Veh] ✅ seated in %s (seat %d)", ObjName(Read<void*>(veh, profile::ObjectClass)).c_str(), seat);
            return true;
        }
    }
    LOG("[Veh] all seats refused (occupied?)");
    return false;
}

// [F47] Periodic candidate scan + HUD exposure + auto-interaction.
// [F65] The old candidate pool came from a GObjects-order cache whose first
// 768 BuildingContainers could all sit in one far map region (load order) —
// nothing was ever within reach of the player, which is why chest
// interaction "never responded". NearbyScanRefresh() now runs
// GetAllActorsOfClass for BuildingContainer + FortPickup every 3s —
// streaming-aware, so the lists always describe what actually exists around
// the player right now.
void Athena::NearbyScanRefresh() {
    if (!session.world || !session.pawn) return;
    void* gas = FindObj("/Script/Engine.Default__GameplayStatics");
    Call gac("/Script/Engine.GameplayStatics.GetAllActorsOfClass");
    if (!gas || !gac.ok()) return;
    auto collect = [&](const char* clsPath, std::vector<void*>& out, std::vector<Vector>& outLoc, size_t cap) {
        out.clear(); outLoc.clear();
        void* cls = FindObj(clsPath);
        if (!cls) return;
        if (!gac.set("WorldContextObject", session.world) || !gac.set("ActorClass", cls)) return;
        if (!gac.run(gas)) return;
        // [F64] OutActors is an out param (CPF_OutParm) — result<Array>() only
        // matched the return slot and read an empty array forever.
        Array got = gac.get_out<Array>("OutActors");
        if (!got.Data) got = gac.get<Array>("OutActors");
        if (!got.Data || got.Count < 0 || got.Count > 4096) return;
        for (int i = 0; i < got.Count && out.size() < cap; ++i) {
            void* a = Read<void*>(got.Data, (size_t)i * 8);
            if (!a || IsBadPtr(a)) continue;
            Vector l{};
            if (!ActorLocation(a, l)) continue;
            out.push_back(a); outLoc.push_back(l);
        }
    };
    collect("/Script/FortniteGame.BuildingContainer", session.near_containers, session.near_container_locs, 512);
    collect("/Script/FortniteGame.FortPickup",        session.near_pickups,    session.near_pickup_locs, 256);
}

void Athena::InteractTick() {
    if (!session.pawn || !session.world) return;
    double now = Monotonic();
    if (now - session.last_near_refresh > 3.0) {
        session.last_near_refresh = now;
        NearbyScanRefresh();
    }

    Vector origin{};
    if (!ActorLocation(session.pawn, origin)) return;

    // [F65] bAlreadySearched bit — resolve the property once per pass (the
    // full GetProp chain walk per candidate per tick would stutter).
    static void* searchedProp = nullptr;
    if (!searchedProp) searchedProp = GetProp("/Script/FortniteGame.BuildingContainer.bAlreadySearched");
    auto isSearched = [&](void* c) -> bool {
        if (!c || IsBadPtr(c) || !searchedProp) return false;
        uint8_t size = Read<uint8_t>(searchedProp, profile::BoolSize);
        uint8_t byte = Read<uint8_t>(searchedProp, profile::BoolByteOffset);
        uint8_t mask = Read<uint8_t>(searchedProp, profile::BoolFieldMask);
        if (!size || size > 8 || byte >= size || !mask) return false;
        return (Read<uint8_t>(GetField(c, searchedProp), byte) & mask) != 0;
    };

    void* best = nullptr; double bestD2 = 0; int bestKind = 0;
    auto consider = [&](void* actor, const Vector& at, int kind, double reach) {
        double dx = at.X - origin.X, dy = at.Y - origin.Y, dz = at.Z - origin.Z;
        double d2 = dx*dx + dy*dy + dz*dz;
        if (d2 > reach * reach) return;
        if (!best || d2 < bestD2) { best = actor; bestD2 = d2; bestKind = kind; }
    };

    // [F65] live container candidates (searched ones filtered via the cached
    // bit read); the legacy GObjects cache stays as fallback while the live
    // list is still empty.
    for (size_t i = 0; i < session.near_containers.size() && i < session.near_container_locs.size(); ++i) {
        void* c = session.near_containers[i];
        if (!c || IsBadPtr(c) || isSearched(c)) continue;
        consider(c, session.near_container_locs[i], 1, 320.0);
    }
    if (!best) {
        size_t n = session.containers.size() < session.container_locs.size()
                 ? session.containers.size() : session.container_locs.size();
        for (size_t i = 0; i < n; ++i) {
            if (session.container_done[i]) continue;
            const Vector& at = session.container_locs[i];
            if (at.X == 0.0 && at.Y == 0.0 && at.Z == 0.0) continue;
            consider(session.containers[i], at, 1, 320.0);
        }
    }
    // vehicles (live tracked set — spawner-produced + map-preplaced)
    for (void* v : session.vehicles) {
        if (!v || IsBadPtr(v)) continue;
        Vector at{};
        if (!ActorLocation(v, at)) continue;
        consider(v, at, 2, 320.0);
    }
    // [F77-e] wildlife answers proximity too — every tracked animal becomes a
    // candidate (DoInteract's per-class BlueprintOnInteract override walk;
    // classes without their own handler fail the receiver check silently).
    for (auto& wm : g_wild_motors) {
        if (!wm.pawn || IsBadPtr(wm.pawn)) continue;
        Vector l{};
        if (!ActorLocation(wm.pawn, l)) continue;
        consider(wm.pawn, l, 3, 260.0);
    }
    session.nearest_interact = best;
    session.nearest_kind = bestKind;
    // [F75-d] one-shot per candidate — proves the scan actually sees world
    // objects near the player (the user reported zero interaction response;
    // this line names every interactable the system finds, at what distance).
    if (best && session.interact_logged.insert(best).second) {
        std::string cn = ObjName(Read<void*>(best, profile::ObjectClass));
        LOG("[Interact] candidate: %s (%.1fm, kind=%d)", cn.c_str(), std::sqrt(bestD2) / 100.0, bestKind);
        if (session.interact_logged.size() > 64) session.interact_logged.clear();
    }

    if (best) {
        // Expose the candidate so the native HUD interact button can appear.
        if (void* comp = GetInteractionComp()) {
            if (void* p = GetProp("/Script/FortniteGame.FortControllerComponent_Interaction.InteractActor"))
                Write<void*>(GetField(comp, p), 0, best);
            // [F75-d/F76] the mobile interact prompt also reads the
            // component's highlighted list — mirror the candidate into it,
            // but ONLY when the native scan has not populated the list
            // itself: the F75 log proved the native interaction system IS
            // alive at phase 3 (AthenaInteractionIndicator + BlueprintCan-
            // Interact traces) and our writes would then fight it (the
            // "stupid interaction" feel). Empty native list -> we fill it;
            // native list -> we stand down.
            if (void* chProp = GetProp("/Script/FortniteGame.FortControllerComponent_Interaction.CurrentlyHighlightedActors")) {
                void* arr = GetField(comp, chProp);
                Array cur = Read<Array>(arr);
                if (cur.Count <= 0) {
                    Fn<int(*)(void*, void*, const void*)>(profile::GenericArrayAdd)(arr, chProp, &best);
                }
            }
            // once: let the component open swinging doors by itself
            static bool doorsAutoSet = false;
            if (!doorsAutoSet) {
                Call sod("/Script/FortniteGame.FortControllerComponent_Interaction.SetShouldAutoOpenSwingingDoors");
                if (sod.ok()) { bool b = true; if (sod.set("bOpenSwingingDoorsAutomatically", b) && sod.run(comp)) doorsAutoSet = true; }
            }
        }
        // [F47/F65] auto-interact on close proximity (Erbium reach: the chest
        // search radius is ~256; the old 170 left the player standing just
        // outside the trigger with "no response").
        if (session.auto_interact) {
            bool seated = false;
            if (bestKind == 2) {
                Call bp("/Script/FortniteGame.FortPlayerPawn.BP_GetVehicle");
                if (bp.ok() && bp.run(session.pawn)) {
                    void* curVeh = bp.result<void*>();
                    seated = (curVeh && !IsBadPtr(curVeh) && curVeh == best);
                }
            }
            double reach = bestKind == 1 ? 260.0 : 300.0;
            if (!seated && bestD2 <= reach * reach) {
                auto& last = session.auto_interact_at[best];
                if (now - last > 2.0) {
                    last = now;
                    if (DoInteract(best) && bestKind == 1) {
                        // drop it from the legacy cache so the fallback loop skips it
                        size_t n = session.containers.size() < session.container_done.size()
                                 ? session.containers.size() : session.container_done.size();
                        for (size_t i = 0; i < n; ++i)
                            if (session.containers[i] == best) { session.container_done[i] = 1; break; }
                    }
                }
            }
        }
    }

    // [F65/F76-d] auto pickup — Erbium's collection flow: the pickup first
    // FLIES to the player (AFortPickup.BlueprintSetPickupTarget = the native
    // fly + pickup-sound path the real client uses), and the item transfer
    // runs when it arrives (~0.6s). When the fly call is unavailable the
    // transfer runs immediately (F65 behaviour). Ground loot collects
    // itself when walked over — no dead pickups on the ground.
    if (session.auto_interact) {
        static Call* spt = nullptr;
        static void* sptCls = nullptr;
        if (!spt) spt = new Call("/Script/FortniteGame.FortPickup.BlueprintSetPickupTarget");
        if (!sptCls) sptCls = FindObj("/Script/FortniteGame.FortPickup");
        static std::map<void*, double> pending;   // pickup -> arrival time
        for (size_t i = 0; i < session.near_pickups.size() && i < session.near_pickup_locs.size(); ++i) {
            void* pk = session.near_pickups[i];
            if (!pk || IsBadPtr(pk)) continue;
            const Vector& at = session.near_pickup_locs[i];
            double dx = at.X - origin.X, dy = at.Y - origin.Y, dz = at.Z - origin.Z;
            double d2 = dx*dx + dy*dy + dz*dz;
            if (d2 > 300.0 * 300.0) continue;
            auto pend = pending.find(pk);
            if (pend != pending.end()) {
                if (now < pend->second) continue;         // still flying
                pending.erase(pend);
                LocalPickup(pk);                          // arrived — transfer
                continue;
            }
            auto& last = session.pickup_last_at[pk];
            if (now - last < 1.5) continue;
            last = now;
            bool flew = false;
            if (spt && spt->ok() && sptCls && IsA(pk, sptCls)) {
                flew = spt->set("PickupTarget", session.pawn) &&
                       spt->set("bPlayPickupSound", true) && spt->run(pk);
                if (flew) {
                    pending[pk] = now + 0.6;
                    static bool once = false;
                    if (!once) { once = true; LOG("[Loot] ✅ pickup fly-collect live (BlueprintSetPickupTarget)"); }
                    continue;
                }
            }
            LocalPickup(pk);   // stacks into inventory / destroys on success
        }
    }
}

// ============================================================================
// BUILDING — [F07/F21] local preview build (Era core) + fixed RPC parse
// ============================================================================
// Unreal FRotator (Pitch/Yaw/Roll degrees) → FQuat for the 21.30 scalar
// double-precision FTransform (Era core conversion, in doubles).
static void RotatorToQuat(const Rotator& r, double out[4]) {
    const double h = 0.00872664626; // pi / 360
    double sp = std::sin(r.Pitch * h), cp = std::cos(r.Pitch * h);
    double sy = std::sin(r.Yaw * h),   cy = std::cos(r.Yaw * h);
    double sr = std::sin(r.Roll * h),  cr = std::cos(r.Roll * h);
    out[0] = cr * sp * sy - sr * cp * cy;
    out[1] = -cr * sp * cy - sr * cp * sy;
    out[2] = cr * cp * sy - sr * sp * cy;
    out[3] = cr * cp * cy + sr * sp * sy;
}

// [F31] Shared building placement (Erbium FortPlayerControllerAthena order):
// BeginSpawn → InitializeKismetSpawnedBuildingActor → FinishSpawn → flags.
static void* PlaceBuildingActor(void* klass, const Transform& t, bool mirrored) {
    if (!session.world || !klass) return nullptr;
    void* actor = SpawnActorUnfinished(session.world, klass, session.controller, t);
    if (!actor) return nullptr;
    Call init("/Script/FortniteGame.BuildingActor.InitializeKismetSpawnedBuildingActor");
    bool anims = true;
    if (init.ok() && init.set("BuildingOwner", actor) && init.set("SpawningController", session.controller) &&
        init.set("bUsePlayerBuildAnimations", anims))
        init.run(actor);
    FinishSpawnActor(actor, t);
    SetBool(actor, "/Script/FortniteGame.BuildingActor.bPlayerPlaced", true);
    Call mirror("/Script/FortniteGame.BuildingSMActor.SetMirrored");
    if (mirror.ok()) { bool m = mirrored; if (mirror.set("bIsMirrored", m)) mirror.run(actor); }
    return actor;
}

// [F49] Grid-cell key for the piece the player is placing. Fortnite snaps
// builds to 512u horizontal / 384u vertical; walls sit on cell edges (256u
// offsets), so 256u/384u quantization + yaw octant + piece kind is enough to
// detect "same spot already built".
static uint64_t BuildCellKey(void* klass, const Vector& loc, const Rotator& rot) {
    int64_t cx = (int64_t)std::llround(loc.X / 256.0);
    int64_t cy = (int64_t)std::llround(loc.Y / 256.0);
    int64_t cz = (int64_t)std::llround(loc.Z / 384.0);
    int64_t yawOct = (int64_t)(std::lround(rot.Yaw / 90.0) & 3);
    std::string n = ObjName(klass);
    int64_t kind = 0;   // 0 wall | 1 floor | 2 stairs | 3 roof
    if (n.find("Floor") != std::string::npos) kind = 1;
    else if (n.find("Stair") != std::string::npos) kind = 2;
    else if (n.find("Roof") != std::string::npos) kind = 3;
    return (uint64_t)(cx & 0xFFFFF)
         | ((uint64_t)(cy & 0xFFFFF) << 20)
         | ((uint64_t)(cz & 0x3FFFF) << 40)
         | ((uint64_t)(yawOct & 3)  << 58)
         | ((uint64_t)(kind & 3)    << 60);
}

// [F49] A cell is occupied when we placed something there, or when a nearby
// world structure (harvest cache — refreshed every 8s) sits on the spot.
bool Athena::BuildCellOccupied(uint64_t key, const Vector& loc) {
    if (session.build_cells.count(key)) return true;
    if (session.harvest_cache.empty()) return false;
    Call locCall("/Script/Engine.Actor.K2_GetActorLocation");
    if (!locCall.ok()) return false;
    int checked = 0;
    for (void* b : session.harvest_cache) {
        if (!b || IsBadPtr(b) || checked >= 64) break;
        if (!locCall.run(b)) continue;
        Vector at = locCall.result<Vector>();
        if (!std::isfinite(at.X) || !std::isfinite(at.Y) || !std::isfinite(at.Z)) continue;
        double dx = at.X - loc.X, dy = at.Y - loc.Y, dz = at.Z - loc.Z;
        if (dx*dx + dy*dy + dz*dz < 70.0 * 70.0) return true;
        ++checked;
    }
    return false;
}

// [F49] Building costs resources (10/piece in BR). The material follows the
// buildable class: _M1 → metal, _S/_Stone → stone, everything else → wood.
void Athena::SpendBuildResources(void* klass) {
    if (!klass) return;
    std::string n = ObjName(klass);
    const char* resName = "WoodItemData";
    if (n.find("Metal") != std::string::npos || n.find("_M1") != std::string::npos || n.find("_M_") != std::string::npos)
        resName = "MetalItemData";
    else if (n.find("Stone") != std::string::npos || n.find("_S1") != std::string::npos || n.find("_S_") != std::string::npos)
        resName = "StoneItemData";
    if (void* def = ByShortName(resName)) StackRemove(def, 10);
}

// [F21] Build from the controller's local preview state — the mobile client
// keeps LastBuildPreviewGridSnapLoc/Rot + CurrentBuildableClass updated while
// the build marker is visible, so no server payload is required.
bool Athena::LocalBuild() {
    if (!session.controller || !session.world) return false;
    void* klass = nullptr;
    Vector loc{};
    Rotator rot{};
    if (!GetFieldValue<Vector>(session.controller, "/Script/FortniteGame.FortPlayerController.LastBuildPreviewGridSnapLoc", loc) ||
        !GetFieldValue<Rotator>(session.controller, "/Script/FortniteGame.FortPlayerController.LastBuildPreviewGridSnapRot", rot)) {
        LOG("[Build] local preview properties unavailable");
        return true;
    }
    void* classProp = GetProp("/Script/FortniteGame.FortPlayerController.CurrentBuildableClass");
    if (classProp) klass = Read<void*>(GetField(session.controller, classProp));
    void* smCls = FindObj("/Script/FortniteGame.BuildingSMActor");
    if (!klass || !smCls || !SubclassOfFast(klass, smCls) || !NearPlayer(loc, 2000.0f) ||
        !std::isfinite(rot.Pitch) || !std::isfinite(rot.Yaw) || !std::isfinite(rot.Roll)) {
        LOG("[Build] invalid selected class or preview transform");
        return true;
    }
    // [F49] No building over an existing piece / map structure.
    uint64_t cell = BuildCellKey(klass, loc, rot);
    if (BuildCellOccupied(cell, loc)) {
        LOG("[Build] spot already built (%s at %.0f %.0f %.0f) — skipped",
            ObjName(klass).c_str(), loc.X, loc.Y, loc.Z);
        return true;
    }
    // Mirroring from the controller state (bBuildPreviewMirrored @0x1859).
    bool mirrored = false;
    { uint8_t flag = Read<uint8_t>(session.controller, profile::Controller_BuildPreviewMirrored);
      mirrored = flag != 0; }

    // Dedup: identical class+pose within 0.25s (touch repeat protection).
    double pose[6] = {loc.X, loc.Y, loc.Z, rot.Pitch, rot.Yaw, rot.Roll};
    double now = Monotonic();
    if (klass == session.last_build_class && now - session.last_build_time < 0.25) {
        bool same = true;
        for (int i = 0; i < 6; ++i) if (session.last_build_pose[(size_t)i] != pose[i]) { same = false; break; }
        if (same) return true;
    }
    LOG("[Build] preview %s at %.0f %.0f %.0f", ObjName(klass).c_str(), loc.X, loc.Y, loc.Z);

    Transform t{};
    t.Location[0] = loc.X; t.Location[1] = loc.Y; t.Location[2] = loc.Z;
    RotatorToQuat(rot, t.Rotation);
    void* actor = PlaceBuildingActor(klass, t, mirrored);   // [F31] Erbium order
    if (!actor) { LOG("[Build] spawn returned null"); return true; }
    session.last_build_class = klass;
    session.last_build_time = now;
    for (int i = 0; i < 6; ++i) session.last_build_pose[(size_t)i] = pose[i];
    session.build_cells.insert(cell);                       // [F49] mark occupied
    if (session.placed_builds.size() < 2048) session.placed_builds.push_back(actor);
    SpendBuildResources(klass);                             // [F49] -10 material
    LOG("[Build] ✅ local build placed %s", ObjName(klass).c_str());
    return true;
}

// [F21] Mobile touch placement: poll the touch-primary-fire hold while in
// build mode and place when the preview pose changes (Era core behaviour).
void Athena::PollTouchBuild() {
    if (!session.controller || !session.world) return;
    double now = Monotonic();
    if (now < session.next_build_poll) return;
    session.next_build_poll = now + 0.05;
    bool held = false;
    if (!GetFieldValue<bool>(session.controller, "/Script/FortniteGame.FortPlayerController.bHoldingPrimaryFireFromTouch", held)) return;
    if (!held) return;
    Call mode("/Script/FortniteGame.FortPlayerController.IsInBuildMode");
    if (!mode.ok() || !mode.run(session.controller) || !mode.result<bool>()) return;
    Vector loc{};
    Rotator rot{};
    if (!GetFieldValue<Vector>(session.controller, "/Script/FortniteGame.FortPlayerController.LastBuildPreviewGridSnapLoc", loc) ||
        !GetFieldValue<Rotator>(session.controller, "/Script/FortniteGame.FortPlayerController.LastBuildPreviewGridSnapRot", rot)) return;
    void* classProp = GetProp("/Script/FortniteGame.FortPlayerController.CurrentBuildableClass");
    void* klass = classProp ? Read<void*>(GetField(session.controller, classProp)) : nullptr;
    double pose[6] = {loc.X, loc.Y, loc.Z, rot.Pitch, rot.Yaw, rot.Roll};
    if (session.held_build_class == klass && now - session.held_build_time < 0.25) {
        bool same = true;
        for (int i = 0; i < 6; ++i) if (session.held_build_pose[(size_t)i] != pose[i]) { same = false; break; }
        if (same) return;
    }
    session.held_build_class = klass;
    session.held_build_time = now;
    for (int i = 0; i < 6; ++i) session.held_build_pose[(size_t)i] = pose[i];
    LOG("[Build] touch placement confirm observed");
    LocalBuild();
}

// [F07] ServerCreateBuildingActor parse — FCreateBuildingActorData (0x50):
//   BuildingClassHandle u32 @0x00, BuildLoc FVector @0x08, BuildRot FRotator
//   @0x20, bMirrored @0x38, SyncKey f32 @0x3C, BuildingClassData @0x40
//   (BuildingClass UClass* @0x40).
void Athena::OnServerCreateBuildingActor(void* params) {
    if (!params) return;
    void* fn = FindObj("/Script/FortniteGame.FortPlayerController.ServerCreateBuildingActor");
    uint16_t total = fn ? Read<uint16_t>(fn, profile::FunctionParamSize) : 0;
    if (total == 0x50) {
        void* cls = Read<void*>(params, 0x40);
        Vector loc = Read<Vector>(params, 0x08);
        Rotator rot = Read<Rotator>(params, 0x20);
        bool mirrored = Read<uint8_t>(params, 0x38) != 0;
        if (!cls) { LOG("[Build] RPC class handle-only placement — using local preview"); LocalBuild(); return; }
        // [F49] same occupancy rule as the local path
        uint64_t cell = BuildCellKey(cls, loc, rot);
        if (BuildCellOccupied(cell, loc)) {
            LOG("[Build] RPC spot already built — skipped");
            return;
        }
        Transform t{};
        t.Location[0] = loc.X; t.Location[1] = loc.Y; t.Location[2] = loc.Z;
        RotatorToQuat(rot, t.Rotation);
        void* actor = PlaceBuildingActor(cls, t, mirrored);   // [F31] Erbium order
        if (!actor) return;
        session.build_cells.insert(cell);                     // [F49]
        if (session.placed_builds.size() < 2048) session.placed_builds.push_back(actor);
        SpendBuildResources(cls);                             // [F49]
        LOG("[Build] RPC build placed %s", ObjName(cls).c_str());
    } else {
        LOG("[Build] unexpected ServerCreateBuildingActor size %u — using local preview", total);
        LocalBuild();
    }
}

// [F10] Edit tool — EditActor and OnRep_EditActor live on FortWeap_EditingTool.
void Athena::OnBeginEditActor(void* piece) {
    if (!session.edit_tool || !session.pawn || !piece) return;
    void* inst = nullptr;
    {
        Call c("/Script/FortniteGame.FortItemDefinition.CreateTemporaryItemInstanceBP");
        int cnt = 1, lvl = 1;
        if (c.ok() && c.set("Count", cnt) && c.set("Level", lvl) && c.run(session.edit_tool)) inst = c.result<void*>();
    }
    if (!inst) return;
    Guid guid{};
    { Call g("/Script/FortniteGame.FortItem.GetItemGuid"); if (g.ok() && g.run(inst)) guid = g.result<Guid>(); }
    Call eq("/Script/FortniteGame.FortPawn.EquipWeaponDefinition");
    if (!eq.ok()) return;
    bool bd = false; Guid tg{};
    eq.set("WeaponData", session.edit_tool); eq.set("ItemEntryGuid", guid);
    eq.set("TrackerGuid", tg); eq.set("bDisableEquipAnimation", bd);
    if (!eq.run(session.pawn)) return;
    void* editActor = eq.result<void*>();
    if (!editActor) return;
    void* p = GetProp("/Script/FortniteGame.FortWeap_EditingTool.EditActor");   // [F10]
    if (p) Write<void*>(GetField(editActor, p), 0, piece);
    else Write<void*>(editActor, profile::EditingTool_EditActor, piece);
    Call r("/Script/FortniteGame.FortWeap_EditingTool.OnRep_EditActor");        // [F10]
    if (r.ok()) r.run(editActor);
    // [F49] The mobile edit grid UI only opens when the building's
    // EditingPlayer is set (OnRep_EditingPlayer) — the old code skipped this
    // so the edit UI never appeared and the finish RPC never fired.
    if (void* ps = GetObjField(session.controller, "/Script/Engine.PlayerController.PlayerState")) {
        if (void* epProp = GetProp("/Script/FortniteGame.BuildingSMActor.EditingPlayer")) {
            Write<void*>(GetField(piece, epProp), 0, ps);
            Call rep("/Script/FortniteGame.BuildingSMActor.OnRep_EditingPlayer");
            if (rep.ok()) rep.run(piece);
        }
    }
    if (void* ebp = GetProp("/Script/FortniteGame.FortPlayerController.EditBuildingActor"))
        Write<void*>(GetField(session.controller, ebp), 0, piece);
    session.last_edit_actor = piece;
    LOG("[Edit] editing tool bound to %s (EditingPlayer set)", ObjName(piece).c_str());
}

// [F49] Edit watcher — keeps EditingPlayer/EditBuildingActor fresh while the
// tool is out and detects the bEditConfirmed rising edge (the mobile confirm
// gesture). If the game's own finish RPC does not fire, era.edit applies the
// replacement manually.
void Athena::EditAssist() {
    if (!session.pawn || !session.controller) return;
    double now = Monotonic();
    if (now - session.last_edit_check < 0.3) return;
    session.last_edit_check = now;
    void* weapon = Read<void*>(session.pawn, profile::FortPawn_CurrentWeapon);
    if (!weapon || IsBadPtr(weapon)) { session.last_edit_confirmed = 0; return; }
    void* toolCls = FindObj("/Script/FortniteGame.FortWeap_EditingTool");
    if (!toolCls || !IsA(weapon, toolCls)) { session.last_edit_confirmed = 0; return; }
    void* editActor = nullptr;
    if (void* p = GetProp("/Script/FortniteGame.FortWeap_EditingTool.EditActor"))
        editActor = Read<void*>(GetField(weapon, p));
    if (!editActor) editActor = Read<void*>(weapon, profile::EditingTool_EditActor);
    if (editActor && !IsBadPtr(editActor)) {
        session.last_edit_actor = editActor;
        // keep the binding alive on both sides (building + controller)
        if (void* ps = GetObjField(session.controller, "/Script/Engine.PlayerController.PlayerState")) {
            if (void* epProp = GetProp("/Script/FortniteGame.BuildingSMActor.EditingPlayer")) {
                void* cur = Read<void*>(GetField(editActor, epProp));
                if (cur != ps) {
                    Write<void*>(GetField(editActor, epProp), 0, ps);
                    Call rep("/Script/FortniteGame.BuildingSMActor.OnRep_EditingPlayer");
                    if (rep.ok()) rep.run(editActor);
                }
            }
        }
        if (void* ebp = GetProp("/Script/FortniteGame.FortPlayerController.EditBuildingActor")) {
            void* curCtl = Read<void*>(GetField(session.controller, ebp));
            if (curCtl != editActor) Write<void*>(GetField(session.controller, ebp), 0, editActor);
        }
    }
    uint8_t confirmed = Read<uint8_t>(weapon, profile::EditingTool_bEditConfirmed);
    if (confirmed && !session.last_edit_confirmed)
        LOG("[Edit] bEditConfirmed rising (target=%s) — if the game UI does not finish, use era.edit wall|floor|stairs|roof",
            editActor && !IsBadPtr(editActor) ? ObjName(editActor).c_str() : "?");
    session.last_edit_confirmed = confirmed;
}

void Athena::OnFinishEditActor(void* actor, void* newClass, int rotIter, bool mirrored) {
    (void)rotIter;   // rotation iteration is preserved inside keepRot (F31)
    if (!actor) return;
    // [F31] Preserve the original piece transform for the edited replacement
    // (the old code respawned at world origin).
    Vector keepLoc{};
    Rotator keepRot{};
    { Call l("/Script/Engine.Actor.K2_GetActorLocation");
      if (l.ok() && l.run(actor)) keepLoc = l.result<Vector>(); }
    { Call r("/Script/Engine.Actor.K2_GetActorRotation");
      if (r.ok() && r.run(actor)) keepRot = r.result<Rotator>(); }
    SetBool(actor, "/Script/FortniteGame.BuildingActor.bPlayDestructionEffects", false);
    Vector zero{0.0, 0.0, 0.0};
    Call s("/Script/Engine.Actor.SetActorScale3D");
    if (s.ok() && s.set("NewScale3D", zero)) s.run(actor);
    Call d("/Script/Engine.Actor.K2_DestroyActor");
    if (d.ok()) d.run(actor);
    if (!newClass) return;
    Transform t{};
    t.Location[0] = std::isfinite(keepLoc.X) ? keepLoc.X : 0.0;
    t.Location[1] = std::isfinite(keepLoc.Y) ? keepLoc.Y : 0.0;
    t.Location[2] = std::isfinite(keepLoc.Z) ? keepLoc.Z : 0.0;
    RotatorToQuat(keepRot, t.Rotation);
    void* edited = PlaceBuildingActor(newClass, t, mirrored);   // [F31]
    if (!edited) return;
    Call m("/Script/FortniteGame.BuildingSMActor.SetMirrored");                 // [F03]
    if (m.ok()) { bool mm = mirrored != 0; if (m.set("bIsMirrored", mm)) m.run(edited); }
    LOG("[Edit] replaced with %s", ObjName(newClass).c_str());
}

void Athena::SpawnBuildPreviews() {
    void* previewCls = FindObj("BuildingPlayerPrimitivePreview");
    if (!previewCls || !session.controller) return;
    auto build = [&](const char* meshPath) -> void* {
        Transform t{};
        void* actor = SpawnActor(session.world, previewCls, session.controller, t);
        if (!actor) return nullptr;
        void* mesh = LoadAsset(meshPath, nullptr);
        void* mc = nullptr;
        Call g("/Script/FortniteGame.BuildingActor.GetBuildingMeshComponent");
        if (g.ok() && g.run(actor)) mc = g.result<void*>();
        if (mc && mesh) { Call s("/Script/Engine.StaticMeshComponent.SetStaticMesh"); if (s.ok() && s.set("NewMesh", mesh)) s.run(mc); }
        Call hide("/Script/Engine.Actor.SetActorHiddenInGame");
        bool h = true;
        if (hide.ok() && hide.set("bNewHidden", h)) hide.run(actor);
        return actor;
    };
    session.wall   = build("/Game/Building/ActorBlueprints/Player/Wood/L1/PBWA_W1_Solid.PBWA_W1_Solid");
    session.floor  = build("/Game/Building/ActorBlueprints/Player/Wood/L1/PBWA_W1_Floor.PBWA_W1_Floor");
    session.stairs = build("/Game/Building/ActorBlueprints/Player/Wood/L1/PBWA_W1_StairW.PBWA_W1_StairW");
    session.roof   = build("/Game/Building/ActorBlueprints/Player/Wood/L1/PBWA_W1_RoofC.PBWA_W1_RoofC");
}

// ============================================================================
// SAFE ZONE / MISC
// ============================================================================
void Athena::SpawnSafeZoneIndicator() {
    if (session.safezone_spawned || !session.state || !session.world) return;
    void* existing = GetObjField(session.state, "/Script/FortniteGame.FortGameStateAthena.SafeZoneIndicator");
    if (existing && !IsBadPtr(existing)) { session.safezone_spawned = true; return; }
    void* cls = FindObj("/Script/FortniteGame.FortSafeZoneIndicator");
    if (!cls) return;
    Transform t{};
    void* indicator = SpawnActor(session.world, cls, session.state, t);
    if (!indicator) return;
    void* prop = GetProp("/Script/FortniteGame.FortGameStateAthena.SafeZoneIndicator");
    if (prop) Write<void*>(GetField(session.state, prop), 0, indicator);
    Call n("/Script/FortniteGame.FortGameStateAthena.OnRep_SafeZoneIndicator");
    if (n.ok()) n.run(session.state);
    session.safezone_spawned = true;
}

void Athena::FixBuildingFoundations() {
    void* cls = FindObj("/Script/FortniteGame.BuildingFoundation");
    if (!cls || !session.world) return;
    GObjects_t* go = GObjectArray();
    if (!go) return;
    void* p = GetProp("/Script/FortniteGame.BuildingFoundation.DynamicFoundationType");
    if (!p) return;
    int propOff = Read<int>(p, profile::PropOffset);
    if (propOff < 0 || propOff > 0x20000) return;
    int fixed = 0;
    for (int32 i = 0; i < go->NumElements; ++i) {
        void* o = go->GetById((uint32)i);
        if (!o || IsBadPtr(o)) continue;
        if (Read<void*>(o, profile::ObjectClass) != cls) continue;
        Write<char>((char*)o + propOff, 0, 0);
        ++fixed;
    }
    LOG("[Foundations] Static type applied to %d foundations", fixed);
}

void Athena::TeleportToSpawnIsland() {
    if (!session.pawn) return;
    void* cls = FindObj("/Script/FortniteGame.FortPlayerStartWarmup");
    if (!cls) return;
    GObjects_t* go = GObjectArray();
    if (!go) return;
    std::vector<void*> starts;
    for (int32 i = 0; i < go->NumElements; ++i) {
        void* o = go->GetById((uint32)i);
        if (o && !IsBadPtr(o)) {
            void* k = Read<void*>(o, profile::ObjectClass);
            if (k == cls) starts.push_back(o);
        }
    }
    if (starts.empty()) return;
    void* start = starts[rand() % starts.size()];
    Call c("/Script/Engine.Actor.K2_GetActorLocation");
    Vector loc{0.0, 0.0, 1000.0};
    if (c.ok() && c.run(start)) loc = c.result<Vector>();
    Rotator rot{0.0, 0.0, 0.0};
    Call sl("/Script/Engine.Actor.K2_SetActorLocationAndRotation");
    if (sl.ok()) { bool bSweep = false, bTeleport = true; sl.set("NewLocation", loc); sl.set("NewRotation", rot); sl.set("bSweep", bSweep); sl.set("bTeleport", bTeleport); sl.run(session.pawn); }
}

void Athena::RemoveNetDebugUI() {
    void* wb = FindObj("/Script/UMG.Default__WidgetBlueprintLibrary");
    if (!wb) return;
    void* cls = FindObj("/Game/Athena/HUD/NetDebugUI.NetDebugUI_C");
    if (!cls) return;
    Call c("/Script/UMG.WidgetBlueprintLibrary.GetAllWidgetsOfClass");
    if (!c.ok() || !c.set("WorldContextObject", session.world) || !c.set("WidgetClass", cls)) return;
    if (!c.run(wb)) return;
    auto arr = c.result<Array>();
    for (int i = 0; i < arr.Count; ++i) {
        void* w = Read<void*>(arr.Data, (size_t)i * 8);
        if (w) { Call r("/Script/UMG.UserWidget.RemoveFromViewport"); if (r.ok()) r.run(w); }
    }
}

// ============================================================================
// EMOTES — [F09] 21.30 signature: BodyType/Gender/PawnContext
// ============================================================================
void Athena::PlayEmoteItem(void* item) {
    if (!item || !session.pawn) return;
    void* mesh = GetObjField(session.pawn, "/Script/Engine.Character.Mesh");
    if (!mesh) return;
    Call anim("/Script/Engine.SkeletalMeshComponent.GetAnimInstance");
    if (!anim.ok() || !anim.run(mesh)) return;
    void* ai = anim.result<void*>();
    if (!ai) return;
    Call ref("/Script/FortniteGame.FortMontageItemDefinitionBase.GetAnimationHardReference");
    void* montage = nullptr;
    if (ref.ok()) {
        // EEFortCustomBodyType / EEFortCustomGender are uint8 enums; try the
        // female/male combinations and fall back gracefully.
        struct Combo { uint8_t body; uint8_t gender; };
        const Combo combos[] = {{1, 1}, {0, 0}, {1, 0}, {0, 1}};
        for (auto& cb : combos) {
            if (ref.set("BodyType", cb.body) && ref.set("Gender", cb.gender) &&
                ref.set("PawnContext", session.pawn) && ref.run(item)) {
                montage = ref.result<void*>();
                if (montage) break;
            }
        }
    }
    if (!montage) { LOG("[Emote] no montage hard reference resolved"); return; }
    Call play("/Script/Engine.AnimInstance.Montage_Play");
    if (play.ok()) {
        play.set("MontageToPlay", montage);
        float rate = 1.0f; play.set("InPlayRate", rate);
        float start = 0.0f; play.set("InTimeToStartMontageAt", start);
        bool bStop = true; play.set("bStopAllMontages", bStop);
        play.run(ai);
        LOG("[Emote] playing %s", ObjName(montage).c_str());
    }
}

// [F11] ExitForceBehavior is EETryExitVehicleBehavior (uint8).
void Athena::OnExitVehicle() {
    if (!session.pawn) return;
    Call v("/Script/FortniteGame.FortPlayerPawn.ServerOnExitVehicle");
    if (v.ok()) {
        uint8_t ExitForce = 0; bool bDestroy = false;
        if (v.set("ExitForceBehavior", ExitForce) && v.set("bDestroyVehicleWhenForced", bDestroy)) v.run(session.pawn);
    }
}

void Athena::CheatScript(const char* script) {
    if (!script || !session.pawn) return;
    std::string s = script;
    std::transform(s.begin(), s.end(), s.begin(), ::tolower);
    LOG("CheatScript: %s", s.c_str());
}

// ============================================================================
// LOBBY / CHEAT COMMANDS — [F24]
// ============================================================================
static void* g_depart_world = nullptr;

void Athena::ReturnToLobby(void* world) {
    // Travel after WorldTick, never from inside a controller return-menu RPC
    // (Era core return_to_lobby ported).
    void* controller = session.controller;
    g_depart_world = world;
    session = Session{};
    auto text = ToWide("open Frontend");
    FStringNative command{(TCHAR*)text.data(), (int32)text.size() + 1, (int32)text.size() + 1};
    Call travel("/Script/Engine.KismetSystemLibrary.ExecuteConsoleCommand");
    bool dispatched = travel.ok() && travel.set("WorldContextObject", world) &&
                      travel.set("Command", command) && travel.set("SpecificPlayer", controller) &&
                      travel.run(FindObj("/Script/Engine.Default__KismetSystemLibrary"));
    LOG("Lobby travel dispatched=%d; old session cleared", (int)dispatched);
    if (!dispatched) { g_depart_world = nullptr; }
}

void Athena::TeleportToReticle() {
    if (!session.controller) return;
    Vector target{};
    if (!GetFieldValue<Vector>(session.controller, "/Script/FortniteGame.FortPlayerController.LocationUnderReticle", target)) {
        LOG("Teleport: no reticle location"); return;
    }
    Call teleport("/Script/FortniteGame.FortPlayerController.ServerTeleportToReticle");
    if (teleport.ok() && teleport.set("TeleportLocation", target) && teleport.run(session.controller))
        LOG("Teleport-to-reticle dispatched to %.0f %.0f %.0f", target.X, target.Y, target.Z);
}

// fly/walk/ghost/god — Era core exec handlers ported.
void Athena::ApplyCheats(const std::string& cmd) {
    // [F73] BR lifecycle commands — available before the pawn exists too.
    if (cmd.rfind("era.bots", 0) == 0) {
        int n = -1; sscanf(cmd.c_str(), "era.bots %d", &n);
        if (n < 0) n = 0;
        br.bots_target = n > 120 ? 120 : n;
        br.next_bot_wave = 0;
        LOG("[BR] bot roster target = %d (spawned %d)", br.bots_target, br.bots_spawned);
        return;
    }
    if (cmd.rfind("era.warmup", 0) == 0) {
        float n = 10; sscanf(cmd.c_str(), "era.warmup %f", &n);
        if (!br.started) { LOG("[BR] machine not started yet"); return; }
        br.warmup_end = BRWorldTime() + n;
        LOG("[BR] warmup ends in %.0fs", n);
        return;
    }
    if (cmd == "era.bus") {
        if (!br.started) { LOG("[BR] machine not started — starting match first"); br.started = true; }
        LOG("[BR] era.bus — launching aircraft phase now");
        br.warmup_end = BRWorldTime();
        SpawnBattleBus();
        return;
    }
    if (!session.pawn || !session.controller) return;
    if (cmd == "god") {
        bool current = GetBool(session.pawn, "/Script/Engine.Actor.bCanBeDamaged");
        SetBool(session.pawn, "/Script/Engine.Actor.bCanBeDamaged", !current);
        LOG("God mode=%d", (int)current);
        return;
    }
    void* movement = GetObjField(session.pawn, "/Script/Engine.Character.CharacterMovement");
    Call mode("/Script/Engine.CharacterMovementComponent.SetMovementMode");
    uint8_t movement_mode = cmd == "walk" ? 1 : 5, custom = 0;
    if (movement && mode.ok() && mode.set("NewMovementMode", movement_mode) && mode.set("NewCustomMode", custom))
        mode.run(movement);
    Call collision("/Script/Engine.Actor.SetActorEnableCollision");
    bool enabled = cmd != "ghost";
    if (collision.ok() && collision.set("bNewActorEnableCollision", enabled)) collision.run(session.pawn);
    LOG("Local movement cheat %s", cmd.c_str());
}

// ============================================================================
// HOOKS
// ============================================================================
static void* g_interactionClass = nullptr;
static void* g_hudContextClass = nullptr;
static void* g_aircraftClass = nullptr;

// [F25] Prologue verification before hooking (Era core pattern) — refuses to
// patch when the bytes at an RVA do not match the profiled build (protects
// against wrong offsets or another mod's detour).
static bool PrologueMatch(const char* label, uintptr_t rva, const uint32_t* words, size_t count) {
    for (size_t i = 0; i < count; ++i) {
        uint32_t actual = Read<uint32_t>((const void*)(g_base + rva), 4 * i);
        if (actual != words[i]) {
            LOG("PROLOGUE MISMATCH %s RVA=%llx word=%zu expected=%08x actual=%08x",
                label, (unsigned long long)rva, i, words[i], actual);
            return false;
        }
    }
    LOG("Prologue verified %s RVA=%llx", label, (unsigned long long)rva);
    return true;
}

// [F26] ProcessEvent prologue varies across builds in its first instruction:
//   libUE4.so  (CL-8936006, reference): STR X28,[SP,#-0x60]!  = 0xf81a0ffc
//   libUnreal.so (CL-21088273 / 21.30): STP X28,X27,[SP,#-0x60]! = 0xa9ba6ffc
// Words 1..3 are identical in both builds (STP X26,X25 / X24,X23 / X22,X21).
// Accept either save-X28 form, then verify the shared tail.
static bool PrologueMatchPE(const char* label, uintptr_t rva) {
    static const uint32_t word0_variants[] = {0xa9ba6ffc, 0xf81a0ffc};
    static const uint32_t tail[]           = {0xa90167fa, 0xa9025ff8, 0xa90357f6};
    uint32_t actual0 = Read<uint32_t>((const void*)(g_base + rva), 0);
    bool ok0 = false;
    for (size_t i = 0; i < sizeof(word0_variants) / sizeof(word0_variants[0]); ++i)
        if (actual0 == word0_variants[i]) { ok0 = true; break; }
    if (!ok0) {
        LOG("PROLOGUE MISMATCH %s RVA=%llx word=0 expected=a9ba6ffc|f81a0ffc actual=%08x",
            label, (unsigned long long)rva, actual0);
        return false;
    }
    for (size_t i = 0; i < sizeof(tail) / sizeof(tail[0]); ++i) {
        uint32_t actual = Read<uint32_t>((const void*)(g_base + rva), 4 * (i + 1));
        if (actual != tail[i]) {
            LOG("PROLOGUE MISMATCH %s RVA=%llx word=%zu expected=%08x actual=%08x",
                label, (unsigned long long)rva, i + 1, tail[i], actual);
            return false;
        }
    }
    LOG("Prologue verified %s RVA=%llx word0=%08x", label, (unsigned long long)rva, actual0);
    return true;
}

static void pe_hook(void* obj, void* func, void* params) {
    if (!orig_PE) return;
    if (!g_internal && func && g_gameTid.load() == (int)gettid()) {
        if (session.lobby_requested) { orig_PE(obj, func, params); return; }

        std::string fn = ObjName(func);
        // [F75-a/F76] flight recorder — the bus-zero crash happens inside the
        // NATIVE transition paths, between our log lines. Record the first
        // occurrence of each external ProcessEvent name from GetReady until
        // the aircraft phase completes (F76 extended the window through
        // phase 3: the native warmup-end machinery runs at T-0 even after
        // the swallows — if it still kills the process the last PETRACE
        // entry names the killer). Capped at 120 unique names.
        if (br.started && br.step >= 3 && br.phase <= 3) {
            static std::set<std::string> seen;
            static int logged = 0;
            if (logged < 120 && seen.insert(fn).second) {
                ++logged;
                std::string rc = obj ? ObjName(Read<void*>(obj, profile::ObjectClass)) : std::string("<null>");
                LOG("[PETRACE] %s on %s", fn.c_str(), rc.c_str());
            }
        }
        bool isPC   = (obj == session.controller);
        bool isPawn = (obj == session.pawn);
        void* currentWeapon = nullptr;
        if (session.pawn) currentWeapon = Read<void*>(session.pawn, profile::FortPawn_CurrentWeapon);
        bool isWeapon = (obj == currentWeapon && obj);

        // [F05] JumpFromHudStart comes through UAthenaHUDContext (touch HUD).
        if (!g_hudContextClass) g_hudContextClass = FindObj("/Script/FortniteGame.AthenaHUDContext");
        if (g_hudContextClass && obj && fn == "JumpFromHudStart") {
            void* cls = Read<void*>(obj, profile::ObjectClass);
            if (SubclassOfFast(cls, g_hudContextClass)) { Athena::HandleJumpFromHudStart(); return orig_PE(obj, func, params); }
        }
        // Direct receiver match (original behaviour retained for safety).
        if (isPC && fn == "JumpFromHudStart") { Athena::HandleJumpFromHudStart(); return orig_PE(obj, func, params); }

        // [F24] Lobby return interception (standalone: travel locally).
        if (isPC && (fn == "ReturnToMainMenu" || fn == "ServerReturnToMainMenu" ||
                     fn == "ClientReturnToMainMenu" || fn == "ClientReturnToMainMenuWithTextReason")) {
            session.lobby_requested = true;
            LOG("Lobby return queued: %s", fn.c_str());
            return orig_PE(obj, func, params);
        }

        // [F04] Aircraft jump RPC arrives on the aircraft component.
        if (!g_aircraftClass) g_aircraftClass = FindObj("/Script/FortniteGame.FortControllerComponent_Aircraft");
        if (g_aircraftClass && obj && fn == "ServerAttemptAircraftJump") {
            void* cls = Read<void*>(obj, profile::ObjectClass);
            if (SubclassOfFast(cls, g_aircraftClass)) { Athena::HandleJumpFromHudStart(); return orig_PE(obj, func, params); }
        }

        if (!g_interactionClass) g_interactionClass = FindObj("/Script/FortniteGame.FortControllerComponent_Interaction");
        bool isInteraction = false;
        if (g_interactionClass && obj) {
            void* cls = Read<void*>(obj, profile::ObjectClass);
            isInteraction = SubclassOfFast(cls, g_interactionClass);
        }
        // [F49/F59] harvest on player-placed builds: the OnDamageServer receiver is
        // the building itself, not the pawn/controller — extend the gate.
        // [F59] container receivers ride the same gate: hitting a chest must
        // pop its loot (standalone never runs the native destroy->loot chain).
        bool isBuildSM = false;
        bool isContainer = false;
        if (fn == "OnDamageServer" && obj) {
            void* cls = Read<void*>(obj, profile::ObjectClass);
            void* smCls = FindObj("/Script/FortniteGame.BuildingSMActor");
            if (smCls) isBuildSM = SubclassOfFast(cls, smCls);
            if (!isBuildSM) {
                void* bcC = FindObj("/Script/FortniteGame.BuildingContainer");
                if (bcC) isContainer = SubclassOfFast(cls, bcC);
            }
        }
        // [F74-a/F75-a] DEAD-SERVER RPCs — SWALLOW FOR ANY RECEIVER. These run the
        // server-side implementation locally in standalone, which crashes
        // (the F58 ServerAttemptInventoryDrop class). At warmup countdown
        // zero the NATIVE warmup-end transition fires the pickaxe equip chain
        // (ClientActivateSlot -> ServerExecuteInventoryItem) — the F74 log
        // proved the equip is game-originated (it fired without any of our
        // calls) and the crash followed it. F75 additionally swallows
        // ClientActivateSlot for external callers: it is a server->client
        // notify (the only declaration in 21.30 is on FortPlayerController),
        // player taps never route through it, and our own calls bypass this
        // hook entirely (Call::run invokes orig_PE directly). Erbium never
        // calls the originals either: it implements these server RPCs itself
        // because there is no upstream server.
        // [F76-a] NATIVE PHASE MACHINERY — SWALLOW. The F75 device log proved
        // the bus-zero crash: our own BRSetGamePhaseStep called
        // FortGameModeAthena.OnGamePhaseStepChanged (the native SERVER
        // handler) at warmup_end-10s and the process died seconds later,
        // mid PETRACE, before zero. That handler runs the whole native
        // aircraft transition (StartAircraftPhase machinery + walking every
        // controller) which needs the server infra the standalone lacks.
        // Erbium never lets the client game mode drive phases either — its
        // GamePhaseLogic component owns them. On 21.30 WE are the phase
        // engine (F73): BRSetGamePhaseStep writes the replicated field and
        // broadcasts the AFortGameStateAthena.GamePhaseStepChanged multicast
        // (client visuals: HUD/music/pawn) — the GameMode binding inside that
        // broadcast lands HERE and dies quietly. ReadyToEndMatch is defanged
        // too: the native match-state poll fired during GetReady ([PETRACE])
        // and a stray true would end the match natively.
        if (fn == "OnGamePhaseStepChanged") {
            static bool once = false;
            if (!once) { once = true; LOG("[BR] native OnGamePhaseStepChanged swallowed (phase engine is ours, F76)"); }
            return;
        }
        if (fn == "ReadyToEndMatch") {
            orig_PE(obj, func, params);   // let the native run, then force the verdict false
            Athena::ForceReturnFalse(func, params);
            return;
        }
        if (fn == "ClientActivateSlot") {
            Guard g;
            static bool once = false;
            if (!once) { once = true; LOG("[Equip] external ClientActivateSlot swallowed (native phase-change equip)"); }
            return;   // the native continuation of this chain crashes standalone
        }
        // [F77-a] GetReady->Bus seatbelt — the F76 crash tail (the native
        // warmup-end transition) ran exactly these calls on its way to death:
        // ServerClientPawnLoaded, the pawn teardown cascade and
        // ClientForceWorldInventoryUpdate. The countdown freeze (BRTick F77-a)
        // is the primary fix — these swallows are the seatbelt in case any
        // other trigger ever fires the transition. ClientGotoState
        // ("Spectating") would strand the hidden-pawn bus design. Everything
        // reopens at phase 4 (normal gameplay inventory refresh).
        if (br.started && br.phase <= 3 &&
            (fn == "ServerClientPawnLoaded" || fn == "ClientGotoState" ||
             fn == "ClientForceWorldInventoryUpdate")) {
            Guard g;
            static bool once77 = false;
            if (!once77) { once77 = true; LOG("[BR] phase-window seatbelt: %s swallowed (F77-a)", fn.c_str()); }
            return;
        }
        // [F77-a] the other warmup-zero trigger — the native match-start
        // poll. Defang exactly like ReadyToEndMatch (F76-a3): let the native
        // run, then force the verdict false so the native GameMode can never
        // StartMatch behind our phase engine's back.
        if (fn == "ReadyToStartMatch") {
            orig_PE(obj, func, params);
            Athena::ForceReturnFalse(func, params);
            return;
        }
        if (fn == "ServerExecuteInventoryItem" || fn == "ServerExecuteInventoryWeapon") {
            Guard g;
            if (isPC) {
                if (fn == "ServerExecuteInventoryItem") {
                    if (Read<uint16_t>(func, profile::FunctionParamSize) >= (int)sizeof(Guid))
                        Athena::OnServerExecuteInventoryItem(Read<Guid>(params));
                } else {
                    Athena::OnServerExecuteInventoryWeapon(Read<void*>(params));
                }
            }
            return;   // [F74-a] local equip done — never reach the crashing native path
        }
        if (isPC || isPawn || isWeapon || isInteraction || isBuildSM || isContainer) {
            Guard g;
            // [F06] ServerHandlePickup(PickUp, InFlyTime, InStartDirection,
            // bPlayPickupSound) and ServerHandlePickupInfo(PickUp, Info) —
            // the pickup pointer is the first parameter in both.
            if ((fn == "ServerHandlePickupInfo" || fn == "ServerHandlePickup") && isPawn) {
                void* arg = incoming(func, params, "PickUp", 8);
                if (!arg) arg = incoming(func, params, "Pickup", 8);
                if (!arg) arg = params;   // PickUp is the first parameter
                if (arg) Athena::ServerHandlePickup(Read<void*>(arg));
            }
            else if (fn == "ServerAttemptInteract" && params && isInteraction) {
                void* target = Read<void*>(params, 0);
                if (target && !IsBadPtr(target)) {
                    // [F47] unified routing: chest / vehicle / door / prop
                    if (!Athena::DoInteract(target)) {
                        // last resort: hand it to the native server path
                        LOG("[Interact] no local route for target — native path continues");
                    }
                }
            }
            else if (fn == "ServerAttemptAircraftJump" && !session.aircraft_jumped) Athena::HandleJumpFromHudStart();
            else if (fn == "ServerCreateBuildingActor" && params && isPC) Athena::OnServerCreateBuildingActor(params);
            // [F21] Local preview build for any ServerCreateBuilding variant.
            else if (fn.rfind("ServerCreateBuilding", 0) == 0 && params && isPC) { if (!Athena::LocalBuild()) Athena::OnServerCreateBuildingActor(params); }
            else if (fn == "ServerBeginEditingBuildingActor" && params) Athena::OnBeginEditActor(Read<void*>(params, 0));
            else if (fn == "ServerEditBuildingActor" && params) {
                // [F08] Named extraction: BuildingActorToEdit(ptr),
                // NewBuildingClass(ptr), RotationIterations(u8), bMirrored(bool).
                void* ba = incoming(func, params, "BuildingActorToEdit", 8);
                void* nc = incoming(func, params, "NewBuildingClass", 8);
                void* ri = incoming(func, params, "RotationIterations", 1);
                void* mi = incoming(func, params, "bMirrored", 1);
                if (ba && nc && ri && mi)
                    Athena::OnFinishEditActor(Read<void*>(ba), Read<void*>(nc), Read<uint8_t>(ri), Read<uint8_t>(mi) != 0);
                else {
                    struct P { void* BA; void* NC; uint8_t RI; uint8_t M; } *p = (P*)params;
                    Athena::OnFinishEditActor(p->BA, p->NC, p->RI, p->M != 0);
                }
            }
            else if (fn == "ServerPlayEmoteItem" && isPC) Athena::PlayEmoteItem(Read<void*>(params));
            else if (fn == "ServerAttemptExitVehicle") Athena::OnExitVehicle();
            // [F49] Harvest grant on player-placed builds (the health-delta
            // watcher covers world structures; this covers our own walls).
            else if (isBuildSM && fn == "OnDamageServer" && params) {
                void* dmgP  = incoming(func, params, "Damage", 4);
                void* instP = incoming(func, params, "InstigatedBy", 8);
                float dmg = dmgP ? Read<float>(dmgP, 0) : 0.0f;
                void* inst = instP ? Read<void*>(instP, 0) : nullptr;
                if (dmg >= 1.0f && inst == session.controller &&
                    GetBool(obj, "/Script/FortniteGame.BuildingActor.bPlayerPlaced"))
                    Athena::HarvestGrantFor(obj, dmg);
            }
            // [F59] chest break -> loot pops (Erbium never needed this because
            // its server sim handled it; standalone must do it locally).
            else if (isContainer && fn == "OnDamageServer" && params) {
                void* instP = incoming(func, params, "InstigatedBy", 8);
                void* inst = instP ? Read<void*>(instP, 0) : nullptr;
                if (inst == session.controller &&
                    !GetBool(obj, "/Script/FortniteGame.BuildingContainer.bAlreadySearched")) {
                    LOG("[Loot] container damaged by player — popping loot");
                    Athena::Loot(obj);
                }
            }
            // [F52] minimap marker diagnosis (era.mark + the map UI path).
            else if (fn == "ServerSpawnMark" && isPC) {
                LOG("[Mark] ServerSpawnMark arrived (map ping path works)");
            }
            // [F58] ServerAttemptInventoryDrop(ItemGuid, Count, bTrash) —
            // SWALLOWED: the native RPC runs into the dead standalone server
            // path (the device crash on every inventory drop). The full local
            // drop (pickup + by-GUID array reconciliation) already ran above.
            else if (fn == "ServerAttemptInventoryDrop" && isPC && params) {
                Guid gg = Read<Guid>(params);
                int cnt = Read<int>(params, sizeof(Guid));
                Athena::DropInventoryItem(gg, cnt);
                return;   // never reach the crashing native path
            }
            // [F58→F69] ServerHandlingBuildMode is the mobile build toggle —
            // swallowing it left the player permanently stuck in build mode
            // ("can't exit build mode"). The call runs its client-side state
            // change locally in standalone; the dead server path inside it is
            // harmless. One-shot diag stays for the crash logger.
            else if (fn == "ServerHandlingBuildMode" && isPC) {
                static bool logged = false;
                if (!logged) { logged = true; LOG("[BuildCall] ServerHandlingBuildMode passes through (build mode toggles natively)"); }
            }
            // [F58] one-shot per-name diagnosis of every other Build-related
            // controller call — the next device log names the exact exit path.
            else if (isPC && fn.find("Build") != std::string::npos) {
                static std::set<std::string> seenBuildCalls;
                if (seenBuildCalls.insert(fn).second)
                    LOG("[BuildCall] %s (diag)", fn.c_str());
            }
        }
    }
    orig_PE(obj, func, params);
}

static const char* PhaseName(int phase) {
    switch (phase) {
        case 0: return "None"; case 1: return "Setup"; case 2: return "Warmup";
        case 3: return "Aircraft"; case 4: return "SafeZones"; case 5: return "EndGame";
        case 6: return "Count"; default: return "Unknown";
    }
}

static void tick_hook(void* world, int type, float delta) {
    if (orig_Tick) orig_Tick(world, type, delta);
    if (g_internal || !world) return;
    int expected = 0;
    if (g_gameTid.compare_exchange_strong(expected, (int)gettid())) LOG("WorldTick first: tid=%d", (int)gettid());
    if (g_gameTid.load() != (int)gettid()) return;
    // [F27] The old [F14] guard read libUnreal.so CODE bytes at
    // profile::IsSavingPackage (0x0760A59C is an EXEC RVA in 21.30 — it was
    // defined-but-never-used in the original hook.cpp). First byte there is
    // 0xFD, so the check was always true and tick_hook returned before ANY
    // world detection ran (no controller binding, no spawn). game.cpp's
    // reference used a real .bss bool there, but this build has no known data
    // address for it — both probes are dropped (the original hook.cpp never
    // used them either). Re-entrancy is still guarded below.
    Guard g;

    // [F27] One-shot diagnostics: proves world pointer + name resolution work
    // on-device (appears once, right after the first game-thread tick).
    static bool diag_logged = false;
    if (!diag_logged) {
        diag_logged = true;
        void* wcls = Read<void*>(world, profile::ObjectClass);
        LOG("DIAG world=%p class=%p ('%s') name='%s'",
            world, wcls, ObjName(wcls).c_str(), ObjName(world).c_str());
    }

    // [F24] Lobby travel was dispatched inside ReturnToLobby; while it is in
    // flight, ignore the departing world so the session does not resurrect.
    if (g_depart_world) {
        if (world == g_depart_world) return;
        g_depart_world = nullptr;
    }
    if (session.lobby_requested && session.world == world) { Athena::ReturnToLobby(world); return; }

    std::string wn = ObjName(world);
    static std::string lastWorld;
    if (wn != lastWorld) { LOG(">>> WorldTick world: '%s'", wn.c_str()); lastWorld = wn; }
    if (wn.find("Frontend") != std::string::npos) {
        if (session.attempted && !session.finalized && session.finalize_at > 0) {
            double now = Monotonic();
            if (now >= session.finalize_at) Athena::FinalizePlayer();
        }
        return;
    }
    if (wn.find("Artemis_Terrain") == std::string::npos && wn.find("Athena_Terrain") == std::string::npos) return;
    int index = Read<int>(world, 0xC);
    if (world != session.world || index != session.world_index) {
        session = Session{};
        session.world = world; session.world_index = index;
        session.observed_at = Monotonic();
        LOG(">>> Terrain detected: %s", wn.c_str());
    }
    double now = Monotonic();
    // [F24] GamePhase tracking for diagnostics (reflection cached).
    if (session.state) {
        static void* phaseProp = nullptr;
        static bool phaseResolved = false;
        if (!phaseResolved) { phaseProp = GetProp("/Script/FortniteGame.FortGameStateAthena.GamePhase"); phaseResolved = true; }
        if (phaseProp && Read<int>(phaseProp, profile::PropElementSize) == 1) {
            uint8_t phase = Read<uint8_t>(GetField(session.state, phaseProp));
            if ((int)phase != session.last_phase) {
                LOG("GamePhase observed=%u (%s) elapsed=%.1f", phase, PhaseName(phase), now - session.observed_at);
                session.last_phase = (int)phase;
            }
        }
    }
    if (now - session.observed_at < 5.0) return;
    Athena::Tick();
}

static bool exec_hook(void* engine, void* world, const char16_t* command, void* output) {
    if (!orig_Exec) return false;
    std::string cmd = Narrow(command, 256);
    std::string normalized = cmd;
    std::transform(normalized.begin(), normalized.end(), normalized.begin(), ::tolower);
    while (!normalized.empty() && (normalized.back() == ' ' || normalized.back() == '\t')) normalized.pop_back();
    if (!cmd.empty()) LOG("Exec: %s", cmd.c_str());
    bool onGameThread = (g_gameTid.load() == (int)gettid());

    if (normalized == "open athena_terrain" || normalized.rfind("open athena_terrain", 0) == 0) {
        LOG("Intercepting Athena travel");
        session = Session{};
        session.attempted = false;
        return orig_Exec(engine, world, u"open Athena_Terrain", output);
    }
    if (onGameThread && session.world && (normalized == "era.lobby" || normalized == "athena.lobby")) {
        session.lobby_requested = true;
        return true;
    }
    if (onGameThread && session.world && normalized == "teleport") {
        if (session.finalized) Athena::TeleportToReticle();
        return true;
    }
    // [F24] Movement cheats (Era core parity).
    if (onGameThread && session.world && session.finalized &&
        (normalized == "fly" || normalized == "walk" || normalized == "ghost" || normalized == "god")) {
        Guard g;
        Athena::ApplyCheats(normalized);
        return true;
    }
    // [F24] Diagnostics/cheat console (era.* mirror of the Era core athena.*).
    if (onGameThread && normalized.rfind("era.", 0) == 0) {
        Guard g;
        if (normalized == "era.status") {
            LOG("STATUS world=%p controller=%p pawn=%p inv=%p qb=%p ready=%d phase=%d",
                session.world, session.controller, session.pawn, session.inventory,
                session.quickbars, (int)session.finalized, session.last_phase);
        } else if (normalized == "era.inventory" && session.inventory) {
            for (size_t i = 0; i < session.slots.size(); ++i)
                LOG("SLOT %zu %s", i, ObjName(ItemDefinitionOf(session.slots[i])).c_str());
            for (size_t i = 0; i < session.secondary_slots.size(); ++i)
                LOG("BUILD %zu %s", i, ObjName(ItemDefinitionOf(session.secondary_slots[i])).c_str());
        } else if (normalized.rfind("era.equip ", 0) == 0) {
            int slot = -1;
            if (sscanf(normalized.c_str(), "era.equip %d", &slot) == 1 && slot >= 0 && slot < 6)
                Athena::EquipSlot(slot);
        } else if (normalized.rfind("era.give ", 0) == 0 && session.inventory_ready) {
            char asset[512]{}; int count = 1, slot = -1;
            if (sscanf(cmd.c_str(), "%*s %511s %d %d", asset, &count, &slot) >= 1) {
                void* def = asset[0] == '/' ? LoadAsset(asset) : ByShortName(asset);
                if (!def) LOG("Give: asset not found: %s", asset);
                else LOG("Give result=%d", (int)Athena::GiveItem(def, count, slot, slot >= 0 && slot < 6 ? 0 : (uint8_t)-1, false));
            }
        }
        // [F28-F33] world systems
        else if (normalized.rfind("era.vehicles", 0) == 0) {
            int n = 12; sscanf(normalized.c_str(), "era.vehicles %d", &n);
            session.vehicles_attempts = 3;   // arm the static-load fallback too
            LOG("[Veh] command -> spawned %d", Athena::SpawnVehiclesNear(n < 1 ? 1 : (n > 40 ? 40 : n)));
        }
        else if (normalized.rfind("era.animals", 0) == 0) {
            int n = 8; sscanf(normalized.c_str(), "era.animals %d", &n);
            LOG("[Wild] command -> spawned %d", Athena::SpawnWildlifeNear(n < 1 ? 1 : (n > 24 ? 24 : n)));
        }
        else if (normalized == "era.stream") Athena::ForceLevelStreaming();
        else if (normalized == "era.hud") { Athena::ForceMapsVisible(); Athena::RefreshHUD(); }
        // [F35-F45] world systems v2
        else if (normalized.rfind("era.zone", 0) == 0) {
            int idx = -1; sscanf(normalized.c_str(), "era.zone %d", &idx);
            if (idx >= 0 && br.started) {
                // [F73] direct phase jump through the Erbium machine
                br.zone_formed = true;
                br.zone_shrinking = false;
                br.safezones_start = 0;
                BRZoneTables();
                if (br.zone_centers.empty()) BRPickZoneCenters();
                BRSetupSafeZoneIndicator();
                BRStartNewSafeZonePhase(idx > 0 && idx < br.zone_count ? idx : 1, false);
            } else {
                // start the SafeZones phase immediately (Erbium bSkipAircraft)
                if (br.started) {
                    br.flight_done = true;
                    br.safezones_start = BRWorldTime();
                    if (br.phase < 4) { BRSetGamePhase(4); BRSetGamePhaseStep(6); }
                }
                LOG("[Zone] BR machine %s — safezones triggered", br.started ? "running" : "idle (no match yet)");
            }
        }
        else if (normalized.rfind("era.players", 0) == 0) {
            int n = 100; sscanf(normalized.c_str(), "era.players %d", &n);
            Athena::SetPlayersLeft(n < 1 ? 1 : (n > 200 ? 200 : n));
        }
        else if (normalized.rfind("era.harvest", 0) == 0) {
            session.harvest_enabled = normalized.find("off") == std::string::npos;
            LOG("[Harvest] enabled=%d", (int)session.harvest_enabled);
        }
        else if (normalized.rfind("era.doors", 0) == 0) {
            session.doors_hold = normalized.find("off") == std::string::npos;
            LOG("[Door] hold-open audit enabled=%d", (int)session.doors_hold);
        }
        else if (normalized.rfind("era.climb", 0) == 0) {
            session.climb_enabled = normalized.find("off") == std::string::npos;
            LOG("[Climb] enabled=%d (hold jump against a wall)", (int)session.climb_enabled);
        }
        else if (normalized == "era.loot") { session.loot_attempts = 0; session.loot_found_total = -1; Athena::SpawnFloorLoot(); }
        else if (normalized == "era.body") { session.body_fixed = false; Athena::FixBodyAndAnimations(); }
        // [F46-F53] interaction / world-content / edit / NPC round
        else if (normalized == "era.scan") { session.map_scan_done = false; Athena::ScanMapContent(); session.map_scan_done = true; }
        else if (normalized.rfind("era.auto", 0) == 0) {
            session.auto_interact = normalized.find("off") == std::string::npos;
            LOG("[Interact] auto-interact enabled=%d", (int)session.auto_interact);
        }
        else if (normalized == "era.interact") {
            void* t = session.nearest_interact;
            if (t) LOG("[Interact] command -> %d", (int)Athena::DoInteract(t));
            else   LOG("[Interact] no candidate nearby (walk closer)");
        }
        else if (normalized.rfind("era.enter", 0) == 0) {
            void* best = session.nearest_kind == 2 ? session.nearest_interact : nullptr;
            if (!best && !session.vehicles.empty()) best = session.vehicles.front();
            if (best) LOG("[Veh] command -> %d", (int)Athena::EnterVehicle(best));
            else LOG("[Veh] no vehicle nearby");
        }
        else if (normalized == "era.exit") { Athena::OnExitVehicle(); }
        else if (normalized.rfind("era.edit", 0) == 0) {
            char what[64] = {}; sscanf(cmd.c_str(), "%*s %63s", what);
            std::string w = what;
            std::transform(w.begin(), w.end(), w.begin(), ::tolower);
            void* target = session.last_edit_actor;
            if (void* ebp = GetProp("/Script/FortniteGame.FortPlayerController.EditBuildingActor"))
                target = Read<void*>(GetField(session.controller, ebp));
            if (!target || IsBadPtr(target)) { LOG("[Edit] no edit target — select a build with the edit tool first"); }
            else {
                const char* piece = "PBWA_W1_Solid";
                if (w == "floor")              piece = "PBWA_W1_Floor";
                else if (w == "stairs" || w == "ramp") piece = "PBWA_W1_StairW";
                else if (w == "roof")          piece = "PBWA_W1_RoofC";
                std::string path = std::string("/Game/Building/ActorBlueprints/Player/Wood/L1/") + piece + "." + piece;
                void* nc = LoadAsset(path);
                if (nc) {
                    uint8_t rotIter = 0;
                    if (void* rip = GetProp("/Script/FortniteGame.FortPlayerController.BuildingRotIterations")) {
                        // field is uint8 at prop offset — read through reflection
                        rotIter = Read<uint8_t>(GetField(session.controller, rip), 0);
                    }
                    Athena::OnFinishEditActor(target, nc, rotIter, false);
                    session.last_edit_actor = nullptr;
                } else LOG("[Edit] piece class not found: %s", path.c_str());
            }
        }
        else if (normalized == "era.qb") { Athena::PokeQuickbarWidgets(); Athena::ForceUpdateQuickBars(); }
        else if (normalized.rfind("era.npc", 0) == 0) {
            int n = 2; sscanf(normalized.c_str(), "era.npc %d", &n);
            LOG("[NPC] command -> spawned %d", Athena::SpawnNPCs(n < 1 ? 1 : (n > 12 ? 12 : n)));
        }
        else if (normalized == "era.sharks") { Athena::ScanMapContent(); }
        else if (normalized == "era.reality" || normalized.rfind("era.reality ", 0) == 0) {
            int n = 2; sscanf(normalized.c_str(), "era.reality %d", &n);
            LOG("[Reality] command -> planted %d", Athena::SpawnRealitySaplings(n < 1 ? 1 : (n > 8 ? 8 : n)));
        }
        else if (normalized == "era.mark") {
            if (session.controller && session.pawn) {
                Vector loc{};
                if (ActorLocation(session.pawn, loc)) {
                    // [F60] standalone has no server — spawn the real marker
                    // actor locally (this is what puts the icon on the map).
                    Athena::SpawnLocalMarker();
                    Call m("/Script/FortniteGame.FortPlayerController.ServerSpawnMark");
                    if (m.ok()) {
                        void* nullActor = nullptr;
                        // PreapprovedText stays zeroed (empty FText).
                        if (m.set("ActorToAttachTo", nullActor) && m.set("AttachOffset", loc) && m.run(session.controller))
                            LOG("[Mark] ServerSpawnMark sent at %.0f %.0f %.0f", loc.X, loc.Y, loc.Z);
                        else LOG("[Mark] ServerSpawnMark bind/run failed");
                    } else LOG("[Mark] ServerSpawnMark not found in 21.30");
                }
            }
        }
        else LOG("Commands: era.status | era.inventory | era.equip 0..5 | era.give Name [count] [slot] | era.vehicles [n] | era.animals [n] | era.stream | era.hud | era.zone [i] | era.players [n] | era.harvest on|off | era.doors on|off | era.climb on|off | era.loot | era.body | era.scan | era.auto on|off | era.interact | era.enter | era.exit | era.edit wall|floor|stairs|roof | era.qb | era.npc [n] | era.sharks | era.reality [n] | era.mark | fly | walk | ghost | god | teleport | era.lobby");
        return true;
    }
    return orig_Exec(engine, world, command, output);
}

static void InstallHooks() {
    LOG("=== InstallHooks ===");
    // [F25/F26] Verify every hooked RVA belongs to this build before patching.
    // Each hook installs independently — one prologue mismatch must never
    // disable the remaining hooks (the old all-or-nothing return left the
    // game with zero hooks: no controller binding, no respawn).
    static const uint32_t tick_words[]  = {0xd10503ff, 0x6d0d23e9, 0xa90e6ffc, 0xa90f67fa};
    static const uint32_t exec_words[]  = {0xd10183ff, 0xa9025ff8, 0xa90357f6, 0xa9044ff4};
    static const uint32_t qb_words[]    = {0xa9bd57f6, 0xa9014ff4, 0xa9027bfd, 0x910083fd};
    bool pe_valid   = PrologueMatchPE("ProcessEvent",    profile::ProcessEvent);
    bool tick_valid = PrologueMatch("WorldTick",       profile::WorldTick,       tick_words, 4);
    bool exec_valid = PrologueMatch("EngineExec",      profile::EngineExec,      exec_words, 4);
    bool qb_valid   = PrologueMatch("AthenaQuickBars", profile::InitializeAthenaQuickBars, qb_words, 4);

    int installed = 0, failed = 0;
    if (pe_valid) {
        if (DobbyHook((void*)(g_base + profile::ProcessEvent), (void*)pe_hook, (void**)&orig_PE) == 0 && orig_PE) { LOG("ProcessEvent hooked"); ++installed; }
        else { LOG("ProcessEvent FAILED"); ++failed; }
    } else { LOG("ProcessEvent skipped (prologue)"); ++failed; }
    if (tick_valid) {
        if (DobbyHook((void*)(g_base + profile::WorldTick), (void*)tick_hook, (void**)&orig_Tick) == 0 && orig_Tick) { LOG("WorldTick hooked"); ++installed; }
        else { LOG("WorldTick FAILED"); ++failed; }
    } else { LOG("WorldTick skipped (prologue)"); ++failed; }
    if (exec_valid) {
        if (DobbyHook((void*)(g_base + profile::EngineExec), (void*)exec_hook, (void**)&orig_Exec) == 0 && orig_Exec) { LOG("EngineExec hooked"); ++installed; }
        else { LOG("EngineExec FAILED"); ++failed; }
    } else { LOG("EngineExec skipped (prologue)"); ++failed; }
    if (!qb_valid) LOG("[QB] initializer prologue mismatch — native init disabled, manual path only");
    LOG("InstallHooks done: installed=%d failed=%d", installed, failed);
}

static int InspectCb(dl_phdr_info* info, size_t, void*) {
    const char* name = strrchr(info->dlpi_name, '/');
    name = name ? name + 1 : info->dlpi_name;
    if (strcmp(name, "libUnreal.so")) return 0;
    g_base = info->dlpi_addr;
    g_status.store(1);
    LOG("libUnreal.so base=%p", (void*)g_base);
    return 1;
}

static void* Worker(void*) {
    LOG("=== EraCH3 (fixed) starting — target CL-21088273 build 5b371871c152651ff2b038c01c9504c1a82de1f8 ===");
    LOG("sizeof(Vector)=%zu sizeof(Rotator)=%zu sizeof(Transform)=%zu", sizeof(Vector), sizeof(Rotator), sizeof(Transform));
    for (int i = 0; i < 600 && g_status.load() == 0; ++i) {
        dl_iterate_phdr(InspectCb, nullptr);
        if (g_status.load() == 0) usleep(100000);
    }
    if (g_status.load() != 1) { LOG("Timeout"); return nullptr; }
    InstallCrashLogger();   // [F34] capture crash offsets from now on
    usleep(300000);
    InstallHooks();
    LOG("=== Ready ===");
    return nullptr;
}

extern "C" __attribute__((constructor))
void LibMain() {
    InitLog();
    pthread_t t;
    if (pthread_create(&t, nullptr, Worker, nullptr) == 0) pthread_detach(t);
}

extern "C" JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM*, void*) { return JNI_VERSION_1_6; }
