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
//  [F26] ProcessEvent prologue variants (21.30 + 10.40 reference build).
//  [F27] WorldTick re-entrancy fix (dead .bss probe removed).
//  [F28] VEHICLES (Erbium 1:1): AFortAthenaVehicleSpawner spawning with the
//        VehicleMin/MaxSpawnPercent roll + bForceSpawnAlways, SetFuel(100),
//        seat-weapon grant/removal through WeaponSeatDefinitions + PlayerSlots
//        (EquipVehicleWeapon / ServerOnExitVehicle parity).
//  [F29] Shared build dedupe between the ServerCreateBuildingActor RPC path
//        and the touch-poll path (no double placement).
//  [F30] BUILDING (Erbium 1:1): BuildingClass resolved from CurrentBuildable-
//        Class when the client only sends the handle, Erbium spawn order
//        (SpawnActorUnfinished → InitializeKismetSpawnedBuildingActor →
//        FinishSpawnActor), CurrentBuildingLevel + OnRep + Team bookkeeping.
//        EDITS now replace at the OLD actor transform (RotationIterations ×
//        90° yaw) instead of the world origin.
//  [F31] PICKUPS: LoadedAmmo/Level carry-over from the pickup entry, ClipSize
//        read from WeaponStatHandle rows, hotbar swap when full (Erbium
//        GiveOrSwap), consumables take primary slots too.
//  [F32] LOOT (Erbium 1:1): real DataTable roll — AthenaLootTierData_Client /
//        AthenaLootPackages_Client through GetDataTableRowNames/Row with the
//        full ChooseLootForContainer algorithm (weighted tier → DropCount →
//        category min/max bumping → package recursion → ammo companion via
//        FortWeaponPickupSpawnAmmoData → stack merge). The curated table is
//        now only a fallback. GetAllActorsOfClass out-param fixed.
//  [F33] Pickup placement along the container axes (LootSpawnLocation_Athena)
//        + guarded TossPickup (refuses to run with an unbound location).
//  [F34] Standalone cache fix: stale bAlreadySearched reset + floor-loot
//        spawners force-rolled and destroyed (Erbium ≥11 behaviour).
//  [F35] PICKUP PIPELINE (Erbium 1:1) — fixes "loot appears but cannot be
//        picked up / looks weird":
//          • SetPickupTarget emulation (PickupLocationData + OnRep_Pickup-
//            LocationData + bPickedUp/OnRep + IncomingPickups) → the native
//            fly-to-player animation plays.
//          • InternalPickup port (stacking / GiveOrSwapStack / GiveOrSwap /
//            FinalLoc re-toss) with a scheduled transfer after FlyTime.
//          • OnCapsuleBeginOverlap hook on the pawn (the live-server pickup
//            trigger) + ServerHandlePickup* RPCs fully replaced (native half
//            does not exist in this build — loot used to vanish).
//          • Container loot now TossPickups out of the chest (was hovering
//            mid-air), PawnWhoDroppedPickup set on every spawn.
//  [F36] QUICKBARS/INVENTORY (Erbium 1:1) — fixes "QuickBars disabled":
//          • FortQuickBars.ServerAddItemInternal / ServerActivateSlotInternal
//            / ServerRemoveItemInternal / EmptySlot native binding with the
//            manual path as fallback.
//          • GiveItem: OwnerInventory + ParentInventory + TrackerGuid +
//            bIsReplicatedCopy (inventory UI orphan fixes).
//          • UE-allocator growth for QuickBar slot arrays (mixed malloc/UE
//            heap was a latent corruption/crash).
//  [F37] INTERACTION — ServerOnAttemptInteract is also dispatched ON the
//        container (Erbium v3 vft hook parity); the old receiver filter
//        dropped those calls, so chests and most world objects ignored the
//        interact button. Chests/doors/BP-interact now resolve from either
//        receiver shape.
//  [F38] BUILD MODE EXIT CRASH — ServerCreateBuildingActor* native path is
//        fully replaced (Erbium parity) and the OG call skipped: the
//        standalone BuildingClassMap is empty, and the exit-build payload
//        crashed the native implementation. Hidden hand-rolled preview
//        actors (SpawnBuildPreviews) removed — they collided with the native
//        preview teardown when build mode ended.
//  [F39] HARVEST RESOURCES (Erbium ABuildingSMActor::OnDamageServer 1:1) —
//        pickaxe damage on trees/rocks grants the mapped resource
//        (K2_GetResourceItemDefinition + AthenaResourceRates curve +
//        BuildingResourceAmountOverride), stack-clamped with overflow
//        re-toss, + ClientReportDamagedResourceBuilding HUD feedback.
//  [F40] ALIVE PLAYERS — GameMode.AlivePlayers.Add(PC) + GameState.PlayersLeft
//        = 1 + OnRep_PlayersLeft (the HUD counter now shows a number).
//  [F41] VEHICLES retry fix — the spawn pass no longer marks itself done
//        before the map's spawners stream in (12 attempts × 15 s).
//  [F42] era.fix (re-run every pass) + era.debug (dump slots/inventory/state)
//        console commands.
// ============================================================================
#include <jni.h>
#include <pthread.h>
#include <dlfcn.h>
#include <link.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <cstring>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <ctime>
#include <cstdarg>
#include <cstddef>
#include <cerrno>
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
constexpr unsigned StructPropertiesSize = 0x54;   // UStruct::PropertiesSize — [F86] was 0xA8 (UE5.1-era, wrong). Derivation: runtime-verified StructSuper=0x40 anchors the UE5.0 UStruct layout (local UE5.0 Class.h member order): SuperStruct 0x40 → Children 0x48 → ChildProperties 0x50 → PropertiesSize 0x54 → MinAlignment 0x58 (both int32). The 0xA8 read landed INSIDE UScriptStruct's own members (past the UStruct header) and returned 0/garbage → 'bad row sizes 0/0' DataTable abort + ClipSizeOf always default 30.
constexpr unsigned FunctionParamSize = 0xB6;
constexpr unsigned PropFlags       = 0x38;
constexpr unsigned PropOffset      = 0x44;
constexpr unsigned PropElementSize = 0x34;
// [F86-a] FProperty::ArrayDim (UE5.1: 0x30). Cross-validated by the walk
// itself — a wrong value falls to the sanity clamp (dim=1) and the computed
// size stays correct for non-array structs.
constexpr unsigned PropArrayDim    = 0x30;
constexpr unsigned BoolSize        = 0x80;
constexpr unsigned BoolByteOffset  = 0x81;
constexpr unsigned BoolFieldMask   = 0x82;
constexpr unsigned FFieldName = 0x28;
constexpr unsigned FFieldNext = 0x20;

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
constexpr uintptr_t QuickBarResize               = 0x056925E0;  // [F81] engine quickbar slot resize+init (verified: ResizeGrow + per-slot init flags=0x03/prev=-1)
constexpr unsigned  Controller_ClientQuickBars   = 0x1a60;
constexpr unsigned  Controller_QuickBarReadyByte = 0x1a68;
constexpr uintptr_t GetItemInQuickbarSlot_Gate   = 0x06673524;
constexpr unsigned  QuickBars_VtableSlot_GetItemInSlot = 0x6f0;
constexpr unsigned  QuickBars_CtxOwner           = 0x138;   // [F94] ctx the native init IsA-casts (0x56607ec: null/fail-cast → silent ret)

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

// [F35] Deep pointer probe — IsBadPtr only validates the VALUE range. On
// Android, dereferencing a freed/unmapped pointer that happens to sit inside
// the valid range range is a SIGSEGV (the "crash on exiting build mode"
// class of bugs). Probing through write(/dev/null) makes the KERNEL validate
// readability without ever touching the memory from our thread.
static int g_devnull_fd = -1;
static bool IsReadable(const void* p, size_t n = 8) {
    if (!p || IsBadPtr(p)) return false;
    if (g_devnull_fd < 0) g_devnull_fd = open("/dev/null", O_WRONLY);
    if (g_devnull_fd < 0) return true;               // cannot probe — assume ok
    ssize_t r = write(g_devnull_fd, p, n);
    return r == (ssize_t)n || (r < 0 && errno != EFAULT);
}

static uintptr_t g_base = 0;
static std::atomic<int> g_status{0};
static std::atomic<int> g_gameTid{0};
static thread_local bool g_internal = false;

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
    // [F28] Weak-pointer resolution needs the full item (serial check).
    FUObjectItem* GetItem(uint32 id) {
        if (!Objects) return nullptr;
        uint32 ci = id / 0x10000, wi = id % 0x10000;
        if ((int32)ci >= NumChunks) return nullptr;
        auto* chunk = (FUObjectItem*)Objects[ci];
        if (!chunk) return nullptr;
        return &chunk[wi];
    }
};
struct TScriptInterface { void* Object; void* Interface; };

// ============================================================================
// FORWARD DECLS
// ============================================================================
static void* FindObj(const std::string& path);
static std::string ObjName(void* obj);
namespace local_loot { int TableCount(); }   // [F92] heartbeat accessor
static bool TryActivateHinted(void* asc, const char* hint);
static FName StringToFName(const char* str);
static bool ApplyAbilitySet(void* pawn, void* abilitySet);
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
    if (!g_base) return nullptr;
    GObjects_t* go = Read<GObjects_t*>((void*)(g_base + profile::GObjects));
    if (!go || IsBadPtr(go)) return nullptr;
    if (go->NumElements < 0 || go->NumElements > 4000000) return nullptr;
    if (go->NumChunks < 0 || go->NumChunks > 128) return nullptr;
    return go;
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
static void* ByShortName(const std::string& wanted) {
    GObjects_t* go = GObjectArray();
    if (!go) return nullptr;
    void* definition_type = FindObj("/Script/FortniteGame.FortItemDefinition");
    if (!definition_type) return nullptr;
    for (int32 i = 0; i < go->NumElements; i += 1) {
        void* object = go->GetById((uint32)i);
        if (object && !IsBadPtr(object) && IsA(object, definition_type) && ObjName(object) == wanted)
            return object;
    }
    return nullptr;
}

// [F41/F83-b] Find a LOADED object of a type by short name (AnimBPs stream
// in late — a package-path FindObj misses them until they are resident).
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

// [F52/F83-b] Find a GameplayAbility subclass by short name (BP ability
// classes are not FindObj-able by package path before they stream in).
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
        // [F92] Name-walk fallback — the device log proved path lookup fails
        // for some UFunctions ('⚠️ OnRep_CosmeticLoadout not present' while
        // GetProp reflection works fine), silently killing OnRep chains
        // (→ dead anims/skin!) and every other call whose StaticFindObject
        // misses. Resolve by NAME over the receiver's class chain — the exact
        // machinery GetProp uses successfully everywhere. Receiver instance →
        // its Class; otherwise the class object named in the path itself.
        if (!m_func) {
            std::string p(path);
            auto dot = p.rfind('.');
            std::string clsPath = (dot != std::string::npos) ? p.substr(0, dot) : std::string();
            std::string fname   = (dot != std::string::npos) ? p.substr(dot + 1) : p;
            void* funcType = FindObj("/Script/CoreUObject.Function");
            void* klass = nullptr;
            if (event_receiver && !IsBadPtr(event_receiver))
                klass = Read<void*>(event_receiver, profile::ObjectClass);
            if (!klass && !clsPath.empty()) {
                void* holder = FindObj(clsPath);
                if (holder && !IsBadPtr(holder)) {
                    void* classCls = FindObj("/Script/CoreUObject.Class");
                    klass = (classCls && IsA(holder, classCls)) ? holder
                                                                : Read<void*>(holder, profile::ObjectClass);
                }
            }
            for (int d = 0; d < 64 && !m_func && klass && !IsBadPtr(klass); ++d) {
                void* f = Read<void*>(klass, profile::StructChildProps);
                for (int i = 0; f && !IsBadPtr(f) && i < 1024; ++i) {
                    std::string n;
                    if (SafeFieldName(f, profile::FFieldName, n) && n == fname && (!funcType || IsA(f, funcType))) { m_func = f; break; }
                    f = Read<void*>(f, profile::FFieldNext);
                }
                if (!m_func) {
                    void* c = Read<void*>(klass, profile::StructChildren);
                    for (int i = 0; c && !IsBadPtr(c) && i < 1024; ++i) {
                        std::string n;
                        if (SafeFieldName(c, profile::ObjectName, n) && n == fname && (!funcType || IsA(c, funcType))) { m_func = c; break; }
                        c = Read<void*>(c, profile::FieldNext);
                    }
                }
                if (!m_func) klass = Read<void*>(klass, profile::StructSuper);
            }
            if (m_func) LOG("[Call] [F92] name-resolved '%s' (StaticFindObject missed it)", fname.c_str());
        }
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
        int total = Read<uint16_t>(m_func, profile::FunctionParamSize);
        if (total < 0 || total > (int)m_data.size()) return;
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
        for (auto& p : m_params) {
            if (p.name != key) continue;
            if (p.size == size && src) { memcpy(m_data.data() + p.offset, src, size); return true; }
            if (p.size == 0) return true;              // [F32] wildcard out param — presence only
            if (p.size >= size && src) { memcpy(m_data.data() + p.offset, src, size); return true; }  // [F32] wildcard sized at compile time
        }
        return false;
    }
    // [F32] Raw param access for out params whose contents the native fills
    // (row copies written by GetDataTableRow exceed the reflected size).
    void* param_slot(const char* key) {
        for (auto& p : m_params)
            if (p.name == key) return m_data.data() + p.offset;
        return nullptr;
    }
    int param_size(const char* key) {
        for (auto& p : m_params) if (p.name == key) return p.size;
        return -1;
    }
    // [F93] POSITIONAL/OUT BINDING — layout-stable fallback for K2 functions
    // whose param NAMES drift across builds while the layout does not. Used
    // by DataTableRow when the named binds miss: (a) nth non-ReturnParm param
    // by OFFSET order (memory layout, not linked-list order), (b) first
    // CPF_OutParm slot regardless of name/size.
    bool bind_at(int nonReturnIndex, const void* src, int size) {
        std::vector<const Param*> ordered;
        for (auto& p : m_params) if (!(p.flags & profile::CPF_ReturnParm)) ordered.push_back(&p);
        std::sort(ordered.begin(), ordered.end(), [](const Param* a, const Param* b) { return a->offset < b->offset; });
        if (nonReturnIndex < 0 || nonReturnIndex >= (int)ordered.size()) return false;
        const Param& p = *ordered[(size_t)nonReturnIndex];
        if (p.offset < 0 || size <= 0 || p.offset + size > (int)m_data.size() || !src) return false;
        memcpy(m_data.data() + p.offset, src, (size_t)size);
        return true;
    }
    int param_at_size(int nonReturnIndex) const {
        std::vector<const Param*> ordered;
        for (auto& p : m_params) if (!(p.flags & profile::CPF_ReturnParm)) ordered.push_back(&p);
        std::sort(ordered.begin(), ordered.end(), [](const Param* a, const Param* b) { return a->offset < b->offset; });
        if (nonReturnIndex < 0 || nonReturnIndex >= (int)ordered.size()) return -1;
        return ordered[(size_t)nonReturnIndex]->size;
    }
    void* out_slot() {
        for (auto& p : m_params)
            if (p.flags & profile::CPF_OutParm) return m_data.data() + p.offset;
        return nullptr;
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
    bool run(void* object) {
        if (!m_valid || !object || !orig_PE) return false;
        // [F16] Receiver validation — refuse cross-class execution.
        void* owner = Read<void*>(m_func, profile::ObjectOuter);
        if (owner && !IsBadPtr(owner) && !IsA(object, owner)) {
            LOG("[Call] rejected %s: receiver=%s owner=%s", ObjName(m_func).c_str(),
                ObjName(Read<void*>(object, profile::ObjectClass)).c_str(), ObjName(owner).c_str());
            return false;
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

// ============================================================================
// REFLECTION HELPERS — [F28/F30/F32] dynamic property resolution used by the
// loot tables, vehicle seats and building bookkeeping. All lookups walk the
// ChildProperties/Children chains and the super classes; every consumer must
// tolerate a -1/nullptr result (feature degrades, never crashes).
// ============================================================================
static void* FindChildProp(void* s, const char* name, int* sizeOut = nullptr) {
    if (!s || IsBadPtr(s) || !name) return nullptr;
    void* f = Read<void*>(s, profile::StructChildProps);
    for (int i = 0; f && !IsBadPtr(f) && i < 512; ++i) {
        std::string n;
        if (SafeFieldName(f, profile::FFieldName, n) && n == name) {
            if (sizeOut) *sizeOut = Read<int>(f, profile::PropElementSize);
            return f;
        }
        f = Read<void*>(f, profile::FFieldNext);
    }
    void* c = Read<void*>(s, profile::StructChildren);
    for (int i = 0; c && !IsBadPtr(c) && i < 512; ++i) {
        std::string n;
        if (SafeFieldName(c, profile::ObjectName, n) && n == name) {
            if (sizeOut) *sizeOut = Read<int>(c, profile::PropElementSize);
            return c;
        }
        c = Read<void*>(c, profile::FieldNext);
    }
    void* sup = Read<void*>(s, profile::StructSuper);
    for (int d = 0; sup && !IsBadPtr(sup) && sup != s && d < 32; ++d) {
        void* r = FindChildProp(sup, name, sizeOut);
        if (r) return r;
        void* next = Read<void*>(sup, profile::StructSuper);
        if (next == sup) break;
        sup = next;
    }
    return nullptr;
}
static int PropOffsetOn(void* s, const char* name, int* sizeOut = nullptr) {
    void* p = FindChildProp(s, name, sizeOut);
    return p ? Read<int>(p, profile::PropOffset) : -1;
}
// [F86-a/LOOT-1] Resolve a UScriptStruct row size WITHOUT trusting the
// build-broken PropertiesSize@0xA8 read: walk the FField ChildProperties
// chains (same proven machinery as FindChildProp/PropOffsetOn — verified
// working on device by every reflection consumer) and compute
//   size = max(prop.Offset + prop.ElementSize * prop.ArrayDim)
// across the struct and its SuperStruct chain, 8-aligned.
// mustHave/mustHaveCount: required member names — the walk counts hits and
// returns 0 when none match, so a WRONG RowStruct pointer (bad 0x28 on some
// build) is detected instead of producing a garbage size. Pass nullptr to
// skip validation (caller already validates members via PropOffsetOn).
// Never reads, never crashes: every value is range-clamped; a prop with a
// wild offset/size is ignored.
static int StructSizeByFFieldWalk(void* s, const char* const* mustHave, int mustHaveCount) {
    if (!s || IsBadPtr(s)) return 0;
    int best = 0, hits = 0;
    void* u = s;
    for (int d = 0; u && !IsBadPtr(u) && d < 16; ++d) {
        // FField chain (UE4.22+ reflection)
        void* f = Read<void*>(u, profile::StructChildProps);
        for (int i = 0; f && !IsBadPtr(f) && i < 512; ++i) {
            int off = Read<int>(f, profile::PropOffset);
            int esz = Read<int>(f, profile::PropElementSize);
            int dim = Read<int>(f, profile::PropArrayDim);
            if (dim < 1 || dim > 64) dim = 1;
            if (off >= 0 && off < 4096 && esz > 0 && esz <= 512) {
                int end = off + esz * dim;
                if (end > best) best = end;
            }
            if (mustHave) for (int k = 0; k < mustHaveCount; ++k) {
                std::string n;
                if (SafeFieldName(f, profile::FFieldName, n) && n == mustHave[k]) ++hits;
            }
            f = Read<void*>(f, profile::FFieldNext);
        }
        // Legacy UProperty children chain (pre-FField / mixed builds)
        void* c = Read<void*>(u, profile::StructChildren);
        for (int i = 0; c && !IsBadPtr(c) && i < 512; ++i) {
            int off = Read<int>(c, profile::PropOffset);
            int esz = Read<int>(c, profile::PropElementSize);
            int dim = Read<int>(c, profile::PropArrayDim);
            if (dim < 1 || dim > 64) dim = 1;
            if (off >= 0 && off < 4096 && esz > 0 && esz <= 512) {
                int end = off + esz * dim;
                if (end > best) best = end;
            }
            if (mustHave) for (int k = 0; k < mustHaveCount; ++k) {
                std::string n;
                if (SafeFieldName(c, profile::ObjectName, n) && n == mustHave[k]) ++hits;
            }
            c = Read<void*>(c, profile::FieldNext);
        }
        void* next = Read<void*>(u, profile::StructSuper);
        if (next == u) break;
        u = next;
    }
    if (mustHave && mustHaveCount > 0 && hits < 2) return 0;   // wrong struct pointer — not our row type
    if (best <= 0 || best > 512) return 0;
    return (best + 7) & ~7;   // align to 8 (row copies are struct-aligned)
}
static std::string FNameToString(const void* fnameBytes) {
    if (!fnameBytes) return {};
    FStringNative ns{};
    Fn<void(*)(const void*, FStringNative*)>(profile::NameAppendString)(fnameBytes, &ns);
    std::string out = Narrow(ns.Data, ns.Count > 0 ? std::min(ns.Count, 128) : 0);
    if (ns.Data) Fn<void(*)(void*)>(profile::Free)(ns.Data);
    return out;
}
// [F32] Resolve a TSoftObjectPtr by handing its raw bytes to LoadAsset_Blocking
// (the soft-pointer struct layout is identical on both sides).
static void* LoadSoftPtr(const void* bytes, int stride) {
    if (!bytes || stride < 8 || stride > 64) return nullptr;
    Call load("/Script/Engine.KismetSystemLibrary.LoadAsset_Blocking");
    if (!load.ok()) return nullptr;
    if (!load.set_bytes("Asset", bytes, stride)) return nullptr;
    if (!load.run(FindObj("/Script/Engine.Default__KismetSystemLibrary"))) return nullptr;
    void* out = load.result<void*>();
    return (out && !IsBadPtr(out)) ? out : nullptr;
}
// [F32] UDataTable row enumeration through the engine Kismet library — works
// regardless of the RowMap key type (FName on stock builds, int on FNE forks).
static bool DataTableRowNames(void* table, std::vector<FName>& out) {
    if (!table || IsBadPtr(table)) return false;
    Call c("/Script/Engine.DataTableFunctionLibrary.GetDataTableRowNames");
    if (!c.ok()) return false;
    Array empty{};
    if (!c.set("Table", table) || !c.set_bytes("OutRowNames", &empty, sizeof(Array))) return false;
    if (!c.run(FindObj("/Script/Engine.Default__DataTableFunctionLibrary"))) return false;
    void* slot = c.param_slot("OutRowNames");
    if (!slot) return false;
    Array res = Read<Array>(slot);
    if (res.Count < 0 || res.Count > 200000) { if (res.Data) Fn<void(*)(void*)>(profile::Free)(res.Data); return false; }
    out.reserve((size_t)res.Count);
    for (int i = 0; i < res.Count; ++i) out.push_back(Read<FName>(res.Data, (size_t)i * sizeof(FName)));
    if (res.Data) Fn<void(*)(void*)>(profile::Free)(res.Data);
    return !out.empty();
}
// [F32] Copy a row into a caller buffer. The native writes Table->RowStruct->
// PropertiesSize bytes at the OutRow slot, so the slot is read back raw.
// [F93] one-shot diagnosis: every historical failure of this function was
// SILENT (all return-false paths unlogged) — the 'cache ready=0 tiers=0
// packages=0' device line with 'tiers=252 rows' one line above proves all
// 2401 calls fail without telling us WHY. This names the path, once.
static bool DataTableRow(void* table, const FName& row, void* dst, int rowSize) {
    if (!table || IsBadPtr(table) || rowSize <= 0 || rowSize > 512 || !dst) return false;
    Call c("/Script/Engine.DataTableFunctionLibrary.GetDataTableRow");
    static bool dumped = false;   // one diagnosis per process — 2401 failures, 1 log
    auto diagnose = [&](const char* why) {
        if (dumped) return;
        dumped = true;
        LOG("[Loot][F93] DataTableRow first-failure diagnosis: %s", why);
        c.dump("GetDataTableRow");
    };
    if (!c.ok()) { diagnose("c.ok()==false — UFunction unresolved (F92 name-walk also missed)"); return false; }
    // [F93] Named binds first (fast path); positional fallback binds by
    // LAYOUT — GetDataTableRow(Table, RowName, OutRow): Table+RowName are
    // the two 8-byte in-parms in offset order, OutRow is the CPF_OutParm
    // slot. Names drift across builds; the layout does not.
    bool tblBound = c.set("Table", table);
    bool rowBound = c.set("RowName", row);
    void* slot = (tblBound && rowBound) ? c.param_slot("OutRow") : nullptr;
    if (!tblBound) {
        if (c.param_at_size(0) == (int)sizeof(void*) && c.bind_at(0, &table, sizeof(void*))) tblBound = true;
        else { diagnose("Table bind failed (named AND positional)"); return false; }
    }
    if (!rowBound) {
        if (c.param_at_size(1) == (int)sizeof(FName) && c.bind_at(1, &row, sizeof(FName))) rowBound = true;
        else { diagnose("RowName bind failed (named AND positional)"); return false; }
    }
    if (!slot) {
        slot = c.out_slot();
        if (!slot && c.param_size("OutRow") > 0) slot = c.param_slot("OutRow");
        if (!slot) { diagnose("OutRow slot unresolved (named AND CPF_OutParm scan)"); return false; }
        static bool posLogged = false;
        if (!posLogged) { posLogged = true; LOG("[Loot][F93] positional binding engaged — param names drifted, layout held"); }
    }
    // zero the out slot before the call (native writes rowSize bytes there)
    {   int zsz = c.param_size("OutRow");
        if (zsz <= 0) zsz = 8;                        // wildcard out param
        memset(slot, 0, (size_t)std::min(zsz, 256));
    }
    void* cdo = FindObj("/Script/Engine.Default__DataTableFunctionLibrary");
    if (!cdo) { diagnose("Default__DataTableFunctionLibrary CDO unresolved"); return false; }
    if (!c.run(cdo)) { diagnose("run() rejected/failed on the CDO"); return false; }
    memcpy(dst, slot, (size_t)rowSize);
    return true;
}
// [F31] Weapon clip size from the WeaponStatHandle row (Era GetStats parity).
static int ClipSizeOf(void* weaponDef) {
    void* p = GetProp("/Script/FortniteGame.FortWeaponItemDefinition.WeaponStatHandle");
    if (!p || !weaponDef || IsBadPtr(weaponDef)) return 30;
    void* handle = GetField(weaponDef, p);
    void* table = Read<void*>(handle, 0);
    FName row = Read<FName>(handle, 8);
    if (!table || IsBadPtr(table) || !row.CompIdx) return 30;
    void* rs = Read<void*>(table, 0x28);   // UDataTable::RowStruct
    if (!rs || IsBadPtr(rs)) return 30;
    int rowSize = Read<int>(rs, profile::StructPropertiesSize);
    if (rowSize <= 0 || rowSize > 512)   // [F86-a/LOOT-1] 0xA8 reads 0 on this build
        rowSize = StructSizeByFFieldWalk(rs, nullptr, 0);   // ClipSize presence is validated right below
    if (rowSize <= 0 || rowSize > 512) return 30;
    int off = PropOffsetOn(rs, "ClipSize");
    if (off < 0) return 30;
    uint8_t buf[512]{};
    if (!DataTableRow(table, row, buf, rowSize)) return 30;
    int v = Read<int32>(buf, off);
    return (v > 0 && v <= 1000) ? v : 30;
}
// [F31] Erbium IsPrimaryQuickbar parity (class-based approximation — a false
// primary only costs a hotbar slot, never a crash).
static bool IsPrimaryQuickbarItem(void* def) {
    if (!def) return false;
    static const char* secondary[] = {
        "/Script/FortniteGame.FortWeaponMeleeItemDefinition",
        "/Script/FortniteGame.FortAmmoItemDefinition",
        "/Script/FortniteGame.FortBuildingItemDefinition",
        "/Script/FortniteGame.FortEditToolItemDefinition",
        "/Script/FortniteGame.FortResourceItemDefinition",
        "/Script/FortniteGame.FortTrapItemDefinition",
        nullptr
    };
    for (int i = 0; secondary[i]; ++i) {
        void* cls = FindObj(secondary[i]);
        if (cls && IsA(def, cls)) return false;
    }
    return true;
}
static int MaxStackSizeOf(void* def) {
    void* p = GetProp("/Script/FortniteGame.FortItemDefinition.MaxStackSize");
    if (!p || !def) return 0;
    int sz = Read<int>(p, profile::PropElementSize);
    if (sz == 8) { double v = Read<double>(GetField(def, p)); if (v > 1.0 && v <= 9999.0) return (int)v; }
    else if (sz == 4) { float v = Read<float>(GetField(def, p)); if (v > 1.0f && v <= 9999.0f) return (int)v; }
    return 0;
}
// [F28] TWeakObjectPtr → object (GObjects serial check).
static void* ResolveWeak(void* weak) {
    if (!weak || IsBadPtr(weak)) return nullptr;
    int32 idx = Read<int32>(weak, 0);
    int32 serial = Read<int32>(weak, 4);
    GObjects_t* go = GObjectArray();
    if (!go || idx < 0 || idx > 4000000) return nullptr;
    FUObjectItem* item = go->GetItem((uint32)idx);
    if (!item || !item->Object || IsBadPtr(item->Object)) return nullptr;
    if (serial != 0 && item->Serial != serial) return nullptr;
    return item->Object;
}

// [F35] Object → TWeakObjectPtr bytes (the reverse of ResolveWeak). Needed
// for AFortPickup.PickupLocationData.PickupTarget (the fly-to-player
// animation reads it) — writing the RAW pointer into a TWeakObjectPtr would
// corrupt it, so the {ObjectIndex, ObjectSerial} pair is built from GObjects.
static bool MakeWeakRef(void* obj, void* weakDst, bool& cached, int32& cIdx, int32& cSerial) {
    if (!obj || IsBadPtr(obj) || !weakDst) return false;
    if (!cached) {
        cached = true; cIdx = -1; cSerial = 0;
        GObjects_t* go = GObjectArray();
        if (go) {
            // Bounded scan; GC may grow the array between calls, NumElements
            // is read fresh every time. The pawn never moves, so this runs once.
            int32 n = go->NumElements;
            if (n > 0 && n < 8000000) {
                for (int32 i = 0; i < n; ++i) {
                    FUObjectItem* it = go->GetItem((uint32)i);
                    if (!it) continue;
                    if (it->Object == obj) { cIdx = i; cSerial = it->Serial; break; }
                }
            }
        }
        if (cIdx >= 0) LOG("[Pickup] weak ref cached idx=%d serial=%d", cIdx, cSerial);
    }
    if (cIdx < 0) return false;
    Write<int32>(weakDst, 0, cIdx);
    Write<int32>(weakDst, 4, cSerial);
    return true;
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
    void* player_state = nullptr;      // [F43] Erbium FortPlayerStateAthena parity
    void* tactical_sprint = nullptr;
    bool  cosmetics_with_state = false; // [F82] cosmetic flow ran WITH a PlayerState present
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
    bool  body_fixed = false;           // [F41/F83-b] locomotion AnimInstance restored
    bool  world_abilities_granted = false; // [F52/F83-b] slide + vehicle ability specs
    bool  lobby_requested = false;      // [F24]
    int   last_phase = -1;              // [F24] GamePhase tracking
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
    // [F28] vehicles
    bool  vehicles_spawned = false;
    int   vehicle_attempts = 0;         // [F41] keep retrying until spawners stream in
    int   qb_retries = 0;               // [F91/BEX-1] per-match QB retry budget (was a Tick function-local static — a second match inherited the exhausted counter and never retried QuickBars)
    Guid  vehicle_weapon_guid{};
    int   vehicle_weapon_seat = -1;
    bool  vehicle_weapon_active = false;
    Guid  previous_weapon_guid{};
    // [F29] shared build dedupe (RPC path records, touch path honors)
    double rpc_build_time = 0;
    // [F35] Erbium pickup pipeline: pickups fly to the player natively, the
    // inventory transfer is scheduled after the FlyTime animation.
    struct PendingPickup { void* pickup; double at; };
    std::vector<PendingPickup> pending_pickups;
    // [F35] cached weak ref of the pawn (PickupTarget)
    bool  pawn_weak_ready = false;
    int32 pawn_weak_idx = -1;
    int32 pawn_weak_serial = 0;
    // [F40] match state (alive players / players-left HUD)
    bool  match_state_ready = false;
    // [F-door r1/r2] doors we opened stay open until the next interact closes
    // them again; the sprint fast-open path caches nearby walls so the scan
    // costs nothing unless the player is actively sprinting.
    struct PinnedDoor { void* wall; };
    std::vector<PinnedDoor> door_pins;
    std::vector<void*>      sprint_doors;   // closed BuildingWalls within reach
    double last_door_scan = 0;
    // [F-cities] building-instruction handler load attempts (streaming window)
    int   building_load_attempts = 0;
    // [F-loot-auto] floor loot retries while spawners still stream in
    int   floor_loot_attempts = 0;
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
// ATHENA NAMESPACE
// ============================================================================
namespace Athena {
    void SetupWorld(void* world);
    void Tick();
    bool TryInitializePlayer(void* world, void* mode, void* controller);
    void SetupCheats();
    void FinalizePlayer();
    void ShowSkin();
    void SyncLockerLoadout();          // [F22] MCP locker sync
    void LoadProfileCosmetics();
    void LoadDefaultCosmetics();
    void ForcePlayerVisible();
    void BeginPlayerSkydiving();
    void EquipSlot(int slot);
    void DestroyLods();
    void DropLoadingScreen();
    void FixBuildingFoundations();
    void FixMissingBuildings();        // [F-cities] standalone building-load driver
    void TickBuildingLoads();          // [F-cities] streaming retry window
    void TickFloorLootStreaming();     // [F-loot-auto] floor loot while streaming
    void FixBus();
    void InitializeInventory();
    bool GiveItem(void* def, int count, int slot, uint8_t bar, bool require_new, int loadedAmmo = -1, int level = 1); // [F17/F31]
    void AddToInventory(void* def, int count, int slot);
    void InventoryUpdate();
    void GrantDefaultAbilities();
    void GrantAbility(void* klass);
    void GrantAllAbilitySets();
    void GrantWorldAbilities();        // [F52/F83-b] slide + vehicle abilities as real specs
    void FixBodyAndAnimations();       // [F41/F68/F83-b] restore missing locomotion AnimInstance
    void EnsurePlayerState();          // [F43] Erbium standalone PlayerState 1:1
    void InitStaminaAttributes();
    void RefreshHUD();
    void ApplySprintSpeed();
    void TickMovementAnimation();
    void SetupTacticalSprint();
    void SetTacticalSprinting(bool sprinting);
    void SpawnSafeZoneIndicator();
    void SpawnPickup(void* def, int count, Vector loc, uint8_t sourceFlags, uint8_t spawnSource, bool toss, int level = -1, int loadedAmmo = -1);
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
    void HandleDoor(void* door);
    void TickDoorPins();               // [F-door r1] stay-open guard
    void TickSprintDoors();            // [F-door r2] sprint fast-open
    void OnServerCreateBuildingActor(void* params);
    bool LocalBuild();                 // [F21]
    bool SpawnBuildingActor(void* klass, const Vector& loc, const Rotator& rot, bool mirrored, int level); // [F30]
    void PollTouchBuild();             // [F21/F29] dedupe shared with the RPC path

    // [F28] Vehicles (Erbium AFortAthenaVehicleSpawner path, 1:1)
    void SpawnVehicles();
    void TickVehicleSeats();
    void GiveVehicleWeapon(void* weaponDef, int seatIdx);
    void RemoveVehicleWeapon(bool reequip);
    void TickMantleAssist();           // [F45] 21.30 wall climb / mantle assist
    void TickRealityProximity();       // [F44] reality tree proximity effect
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
    void VerifyHUDReadPath();   // [F92] replay the exact native HUD read chain
    bool BindSlot(void* item, int slot, uint8_t bar);
    void ActivateQuickBarSlot(uint8_t bar, int slot);
    void ForceUpdateQuickBars();
    void BuildQuickBarsManually();

    // Loot [F20/F32]
    void SpawnFloorLoot();
    void ResetStaleContainers();       // [F34] standalone cache fix
    bool SearchContainer(void* actor, const Vector& location, bool force = false);

    // [F18] pickups / [movement]
    void ServerHandlePickup(void* pickup);
    void EnableMovementFeatures();

    // [F35] Erbium pickup pipeline (SetPickupTarget / InternalPickup parity)
    void HandlePickupRPC(void* pickup, float flyTime);
    void ProcessPendingPickups();
    void SetPickupTargetAnim(void* pickup, float flyTime);
    bool InternalPickup(void* entry, void* pickupActor);

    // [F36] QuickBars native binding (ServerAddItemInternal parity)
    bool QBAddItem(Guid guid, bool bSecondary, int slot);
    void QBActivateSlot(bool bSecondary, int slot);
    void QBRemoveItem(Guid guid, int slot);

    // [F39] harvest resources on building damage (Erbium OnDamageServer 1:1)
    void HandleDamageServer(void* actor, void* func, void* params);

    // [F40] alive-players / players-left HUD state
    void SetupMatchState();
    void DebugDump();
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
// تبحث في قدرات اللاعب عن قدرة باسم يحتوي على hint ثم تفعّلها
// إرسال GameplayEvent (بديل لـ TryActivate — لبعض القدرات التي تعتمد على event)
static bool SendGameplayEvent(void* asc, const char* eventTagName) {
    if (!asc || IsBadPtr(asc) || !eventTagName) return false;
    void* tag = ByShortName(eventTagName);
    // GameplayTag = FName (8 بايت)
    // جرّب عدة أشكال للوسم
    static const char* const tags[] = {
        eventTagName, nullptr
    };
    for (int i = 0; tags[i]; ++i) {
        FName tagName = StringToFName(tags[i]);
        if (!tagName.CompIdx) continue;
        Call c("/Script/GameplayAbilities.AbilitySystemComponent.HandleGameplayEvent");
        if (!c.ok()) continue;
        // HandleGameplayEvent(GameplayTag, FGameplayEventData*)
        uint8_t eventData[256]{};
        if (c.set_bytes("EventTag", &tagName, sizeof(FName)) && c.set_bytes("Payload", eventData, 8)) {
            if (c.run(asc)) { LOG("[Ability] ✅ event %s", tags[i]); return true; }
        }
    }
    return false;
}

void Athena::GrantAbility(void* klass) {
    if (!klass || !session.pawn) return;
    void* asc = GetASC(session.pawn);
    if (!asc) { LOG("[Ability] no ASC"); return; }

    // المحاولة 1: TryActivateAbilityByClass (تعمل إذا كان الـ spec موجوداً
    // مسبقاً عبر EquipFortAbilitySet — مثل Sprint/Jump)
    {
        Call c("/Script/GameplayAbilities.AbilitySystemComponent.TryActivateAbilityByClass");
        if (c.ok()) {
            bool bAllow = true;
            if (c.set("InAbilityToActivate", klass) && c.set("bAllowRemoteActivation", bAllow)) {
                if (c.run(asc)) {
                    LOG("[Ability] activated existing spec: %s", ObjName(klass).c_str());
                    return;
                }
            }
        }
    }

    // المحاولة 2: منح القدرة عبر FortAbilitySet مؤقت (المسار الفعلي)
    // EquipFortAbilitySet هو الذي يستدعي native GiveAbility داخلياً
    void* setCls = FindObj("/Script/FortniteGame.FortAbilitySet");
    if (!setCls) { LOG("[Ability] FortAbilitySet class missing"); return; }

    void* set = Fn<void*(*)(void*, void*)>(profile::SpawnObject)(setCls, session.pawn);
    if (!set || IsBadPtr(set)) { LOG("[Ability] temp set creation failed"); return; }

    void* abProp = GetProp("/Script/FortniteGame.FortAbilitySet.GameplayAbilities");
    if (!abProp) { LOG("[Ability] GameplayAbilities prop missing"); return; }

    Array* arr = reinterpret_cast<Array*>(GetField(set, abProp));
    if (!arr || arr->Count != 0) { LOG("[Ability] temp set array unavailable"); return; }

    Fn<int(*)(void*, void*, const void*)>(profile::GenericArrayAdd)(arr, abProp, &klass);

    if (ApplyAbilitySet(session.pawn, set)) {
        LOG("[Ability] ✅ granted+activated via set: %s", ObjName(klass).c_str());
        // بعد المنح، نحاول التفعيل مرة أخرى (اختياري)
        Call c("/Script/GameplayAbilities.AbilitySystemComponent.TryActivateAbilityByClass");
        if (c.ok()) {
            bool bAllow = true;
            if (c.set("InAbilityToActivate", klass) && c.set("bAllowRemoteActivation", bAllow))
                c.run(asc);
        }
    } else {
        LOG("[Ability] ❌ EquipFortAbilitySet failed for %s", ObjName(klass).c_str());
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
    if (!coreSet) { LOG("[Abilities] GAS_AthenaPlayer missing — will retry (asset may stream late)"); return; }   // [F90-a/AUD-1] no premature commit: the 10s Tick retry re-invokes until the set lands

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

    // Fallback direct activations (only works for already-granted abilities).
        if (ApplyAbilitySet(session.pawn, coreSet)) session.granted_ability_sets.push_back(coreSet);

    // [F13] Fallback direct activation — only fires for abilities already
    // granted via the ability set above (the spec must exist in the ASC).
    // Expanded per 21.30: Sprint + Jump + interact + harvest + emote + rift.
    static const char* const individualAbilities[] = {
        "/Script/GameplayAbilities.GameplayAbility_CharacterJump",
        "/Script/FortniteGame.FortGameplayAbility_Sprint",
        "/Script/FortniteGame.FortGameplayAbility_Jump",
        "/Game/Abilities/Player/Generic/Traits/DefaultPlayer/GA_DefaultPlayer_InteractUse.GA_DefaultPlayer_InteractUse_C",
        "/Game/Abilities/Player/Generic/Traits/DefaultPlayer/GA_DefaultPlayer_InteractSearch.GA_DefaultPlayer_InteractSearch_C",
        "/Game/Abilities/Player/Generic/Traits/DefaultPlayer/GA_DefaultPlayer_Harvest.GA_DefaultPlayer_Harvest_C",
        "/Game/Abilities/Player/Generic/Traits/DefaultPlayer/GA_DefaultPlayer_Death.GA_DefaultPlayer_Death_C",
        "/Game/Abilities/Player/Emotes/GAB_Emote_Generic.GAB_Emote_Generic_C",
        "/Game/Athena/Items/ForagedItems/Rift/GA_Rift_Athena_Skydive.GA_Rift_Athena_Skydive_C",
        nullptr
    };
    for (int i = 0; individualAbilities[i]; ++i) {
        void* cls = FindObj(individualAbilities[i]);
        if (!cls) cls = LoadClass(individualAbilities[i], "/Script/GameplayAbilities.GameplayAbility");
        if (!cls) continue;
        GrantAbility(cls);
    }
    session.abilities_granted = true;
    session.melee_granted = true;
    LOG("[Abilities] Done — sets=%zu", session.granted_ability_sets.size());
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
    // [F83-c] runtime cross-check of the F81-verified layout (0x278/0x308 —
    // binary-verified via disassembly) against live reflection. A drift here
    // means every read below is a false 0/0 and the builder would write the
    // wrong bars; the log settles it in one device run.
    {
        void* pProp = GetProp("/Script/FortniteGame.FortQuickBars.PrimaryQuickBar");
        void* sProp = GetProp("/Script/FortniteGame.FortQuickBars.SecondaryQuickBar");
        if (pProp) { int ro = Read<int>(pProp, profile::PropOffset);
            if (ro != (int)profile::QuickBars_PrimaryOffset)
                LOG("[QB] ⚠️ PrimaryQuickBar offset drift: reflect=0x%X const=0x%X", ro, (int)profile::QuickBars_PrimaryOffset); }
        if (sProp) { int ro = Read<int>(sProp, profile::PropOffset);
            if (ro != (int)profile::QuickBars_SecondaryOffset)
                LOG("[QB] ⚠️ SecondaryQuickBar offset drift: reflect=0x%X const=0x%X", ro, (int)profile::QuickBars_SecondaryOffset); }
    }
    void* primary = (char*)session.quickbars + profile::QuickBars_PrimaryOffset;
    void* secondary = (char*)session.quickbars + profile::QuickBars_SecondaryOffset;
    Array p_before = Read<Array>(primary, profile::QuickBar_SlotsOffset);
    Array s_before = Read<Array>(secondary, profile::QuickBar_SlotsOffset);
    LOG("[QB] Before: Primary=%d Secondary=%d", p_before.Count, s_before.Count);
    // [F43] Native init is called with (this, controller) — the 21.30 entry
    // reads the owning controller as its argument (harmless when the param
    // list is empty; without it the call early-outs and the bars stay empty).
    { Guard g; reinterpret_cast<void(*)(void*, void*)>(init_slot)(session.quickbars, session.controller); }
    Array p_after = Read<Array>(primary, profile::QuickBar_SlotsOffset);
    Array s_after = Read<Array>(secondary, profile::QuickBar_SlotsOffset);
    LOG("[QB] After: Primary=%d Secondary=%d", p_after.Count, s_after.Count);
    if (p_after.Count >= 1 && s_after.Count >= 1) { LOG("[QB] ✅ Init succeeded"); session.quickbars_initialized = true; }
    else {
        LOG("[QB] ⚠️ Init may have failed");
        // [F92] disasm 0x56607ec: native init bails SILENTLY when ctx
        // ([this+0x138]) is null OR fails both IsA casts (0x7071ee4 /
        // 0x70975fc probes; sources owner+0x3DA0 | owner+0x31A0). The spawn
        // code already tried ctx=controller. Diagnostic-only retry with
        // ctx=PlayerState — never sets the flag (the manual builder remains
        // the item provider); this just names the real owner for the log.
        void* psCtx = GetObjField(session.controller, "/Script/Engine.Controller.PlayerState");
        if (psCtx && !IsBadPtr(psCtx)) {
            Write<void*>((char*)session.quickbars + profile::QuickBars_CtxOwner, 0, psCtx);
            { Guard g; reinterpret_cast<void(*)(void*, void*)>(init_slot)(session.quickbars, session.controller); }
            Array p2 = Read<Array>(primary, profile::QuickBar_SlotsOffset);
            Array s2 = Read<Array>(secondary, profile::QuickBar_SlotsOffset);
            LOG("[QB] ctx=PlayerState retry: P=%d S=%d%s", p2.Count, s2.Count,
                (p2.Count >= 1 && s2.Count >= 1) ? " — native owner IS the PlayerState" : "");
            Write<void*>((char*)session.quickbars + profile::QuickBars_CtxOwner, 0, session.controller);
        } else LOG("[QB] [F92] no PlayerState for ctx retry");
    }
}

// [F01] Manual QuickBar construction — the enable byte is at slot+0x10
// (bEnabled bit0 | bIsDirty bit1 | bIsReserved bit2 | bIsOccupied bit3);
// the old code wrote slot+0x18 which is UsedByItemGuid.
void Athena::BuildQuickBarsManually() {
    if (!session.quickbars) { LOG("[QB] No quickbars"); return; }
    // [F44/F83-a] Dedupe: if slot 0 of the primary bar already holds a GUID
    // we are already filled — re-running would leak arrays, re-grow stale
    // buffers and duplicate entries (the Tick retry path calls us repeatedly).
    {
        void* barChk = GetProp("/Script/FortniteGame.FortQuickBars.PrimaryQuickBar");
        if (barChk) {
            void* bar = GetField(session.quickbars, barChk);
            if (bar && !IsBadPtr(bar)) {
                Array arr = Read<Array>((char*)bar + profile::QuickBar_SlotsOffset);
                if (arr.Count > 0 && arr.Data && !IsBadPtr(arr.Data)) {
                    Guid g0 = Read<Guid>((char*)arr.Data + 0x18, 0);   // UsedByItemGuid
                    if (g0.A | g0.B | g0.C | g0.D) {
                        LOG("[QB] already filled (slot0 guid) — skipping rebuild");
                        session.quickbars_initialized = true;
                        return;
                    }
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

    // [F81] Grow slot arrays through the ENGINE'S OWN quickbar resize helper
    // (0x56925E0, verified in the binary: ResizeGrow + per-slot init with
    // flags=(old&0xF0)|0x03 and prev=-1). The old raw Realloc+memset left
    // flags=0 and prev=0 — state the native writer never produces.
    auto ensure_slots = [&](void* bar, int want, uint8_t barId) -> Array {
        void* slotsField = (char*)bar + profile::QuickBar_SlotsOffset;
        Array arr = Read<Array>(slotsField);
        // [F83-a] never trust a stale/garbage header — a wild Data pointer
        // must not reach the engine helpers (Resize/Realloc on a freed or
        // wild pointer corrupts the heap; teardown on build exit suspect).
        if (arr.Count < 0 || arr.Count > 64 || (arr.Count > 0 && IsBadPtr(arr.Data))) {
            LOG("[QB] ⚠️ bar%u header sanitized (Count=%d Data=%p)", barId, arr.Count, arr.Data);
            arr = Array{nullptr, 0, 0};
        }
        if (arr.Count < want) {
            LOG("[QB] grow bar%u: Data=%p Count=%d Max=%d want=%d", barId, arr.Data, arr.Count, arr.Max, want);
            { Guard g; Fn<void(*)(void*, int32, int32)>(profile::QuickBarResize)(slotsField, want, 1); }
            arr = Read<Array>(slotsField);
            LOG("[QB] grow bar%u readback: Data=%p Count=%d Max=%d", barId, arr.Data, arr.Count, arr.Max);
            if (arr.Count < want || !arr.Data) {
                // [F36] fallback: UE-allocator growth (FMemory::Realloc 3-arg).
                LOG("[QB] ⚠️ native grow short — manual fallback bar%u", barId);
                int oldBytes = arr.Count > 0 ? profile::QuickBarSlot_SlotSize * arr.Count : 0;
                int newBytes = profile::QuickBarSlot_SlotSize * want;
                void* newData = Fn<void*(*)(void*, size_t, uint32_t)>(profile::Realloc)(
                    arr.Count > 0 ? arr.Data : nullptr, (size_t)newBytes, 0u);
                if (!newData) { LOG("[QB] ⚠️ slot growth failed (want=%d)", want); return arr; }
                // [F83-a] CRASH FIX (fallback path): the old `oldBytes > 0`
                // guard skipped this memset exactly when Count==0 — the
                // fresh Realloc'd chunk (recycled from a previous match's
                // teardown) went in UNINITIALIZED and the entries loop read
                // garbage Items headers from it (wild memcmp/GenericArrayAdd
                // → the Bar1-slot1 crash). Zero beyond preserved contents,
                // unconditionally.
                if (newBytes > oldBytes)
                    memset((char*)newData + oldBytes, 0, (size_t)(newBytes - oldBytes));
                Write<void*>(slotsField, 0, newData);
                Write<int32>(slotsField, 8, want);
                Write<int32>(slotsField, 12, want);
                arr = Read<Array>(slotsField);
                LOG("[QB] manual grow bar%u readback: Data=%p Count=%d", barId, arr.Data, arr.Count);
            }
        }
        return arr;
    };

    Array primArr = ensure_slots(GetField(session.quickbars, barProp), 6, 0);
    Array secArr  = ensure_slots(GetField(session.quickbars, secProp), 5, 1);
    // [F43] all 21.30 quickbars: Primary(6) + Secondary(5) + Creative(6).
    {
        void* creatProp = GetProp("/Script/FortniteGame.FortQuickBars.CreativeQuickBar");
        if (creatProp) {
            Array c = ensure_slots(GetField(session.quickbars, creatProp), 6, 2);
            LOG("[QB] Creative bar slots=%d (Data=%p)", c.Count, c.Data);
        }
    }

    // [F81] every path is now logged with readbacks — if anything still dies,
    // the LAST "[QB] bind" line names the exact bar/slot/step.
    for (auto& e : entries) {
        Array& arr = e.bar == 0 ? primArr : secArr;
        if (e.idx < 0 || e.idx >= arr.Count || !arr.Data) {
            LOG("[QB] SKIP bar%u slot%d (Count=%d Data=%p)", e.bar, e.idx, arr.Count, arr.Data);
            continue;
        }
        void* slot = (char*)arr.Data + (size_t)e.idx * profile::QuickBarSlot_SlotSize;
        // Items array (TArray<FGuid>) at slot+0x00 — grown through the UE
        // allocator via GenericArrayAdd so native code can keep mutating it.
        void* itemsField = (char*)slot + profile::QuickBarSlot_ItemsOffset;
        Array values = Read<Array>(itemsField);
        LOG("[QB] bind bar%u slot%d: slot=%p items=%p count=%d max=%d",
            e.bar, e.idx, slot, values.Data, values.Count, values.Max);
        // [F83-a] harden: a garbage Items header must read as empty — never
        // as a wild Data pointer for the memcmp scan below.
        if (values.Count < 0 || values.Count > 16 || (values.Count > 0 && IsBadPtr(values.Data)))
            values = Array{nullptr, 0, 0};
        bool present = false;
        for (int i = 0; i < values.Count; ++i)
            if (!memcmp((char*)values.Data + (size_t)i * sizeof(Guid), &e.guid, sizeof(Guid))) { present = true; break; }
        if (!present) {
            auto add = Fn<int(*)(void*, void*, const void*)>(profile::GenericArrayAdd);
            int rc = add(itemsField, GetProp("/Script/FortniteGame.QuickBarSlot.Items"), &e.guid);
            Array after = Read<Array>(itemsField);
            LOG("[QB] add rc=%d → items=%p count=%d", rc, after.Data, after.Count);
            if (after.Count < 1 || IsBadPtr(after.Data)) { LOG("[QB] ⚠️ add failed bar%u slot%d — skip", e.bar, e.idx); continue; }
        }
        // [F81] native-writer parity: prev slot index = -1, flags keep the
        // high nibble and set Enabled|Dirty|Occupied (native init writes 0x03;
        // occupied marks the slot as holding an item for the HUD).
        Write<int32>(slot, 0x14, -1);
        uint8_t* flags = (uint8_t*)((char*)slot + profile::QuickBarSlot_EnabledOffset);
        *flags = (uint8_t)((*flags & 0xF0) | profile::QuickBarSlot_EnabledBit |
                           profile::QuickBarSlot_OccupiedBit | 0x02);
        LOG("[QB] Bar%u slot %d -> GUID %08X:%08X ✅", e.bar, e.idx, e.guid.A, e.guid.B);
    }

    // [F43] Push the refreshed bars into the UI layer (Erbium calls the
    // OnRep_* pair after manual slot writes; both degrade silently when the
    // UFunctions are absent on this build).
    {
        LOG("[QB] OnRep Primary...");
        Call r1("/Script/FortniteGame.FortQuickBars.OnRep_PrimaryQuickBar");
        if (r1.ok()) r1.run(session.quickbars);
        LOG("[QB] OnRep Secondary...");
        Call r2("/Script/FortniteGame.FortQuickBars.OnRep_SecondaryQuickBar");
        if (r2.ok()) r2.run(session.quickbars);
    }
    LOG("[QB] ClientForceUpdateQuickbar...");
    ForceUpdateQuickBars();
    session.quickbars_initialized = true;
    LOG("[QB] ✅ Manual initialization done");
}

void Athena::SetQuickBarReady() {
    if (!session.controller) return;
    // [F92] the readiness byte is a disasm-verified RAW offset (PC+0x1A68):
    // native GetItemInQuickbarSlot (0x6673508) AND GetQuickBarSlotCount
    // (0x66734cc) both cbz on it as their second gate — 0 means the HUD
    // reads NOTHING. Reflection is informational only; never gate the
    // write on it (the old 3 skip-guards could leave it 0 forever).
    void* qp = GetProp("/Script/FortniteGame.FortPlayerController.ClientQuickBars");
    int propOff = qp ? Read<int>(qp, profile::PropOffset) : -1;
    LOG("[QB] ClientQuickBars reflect offset=0x%X (const 0x%X)", propOff, (int)profile::Controller_ClientQuickBars);
    // HUD check #1 — the raw pointer link (must be OUR quickbars object)
    void* cur = Read<void*>(session.controller, profile::Controller_ClientQuickBars);
    if (cur != session.quickbars) {
        LOG("[QB] ⚠️ PC->ClientQuickBars=%p != quickbars=%p — writing raw link", cur, session.quickbars);
        Write<void*>(session.controller, profile::Controller_ClientQuickBars, session.quickbars);
    }
    if (!session.quickbars_initialized) { LOG("[QB] ❌ QuickBars not initialized — readiness deferred"); return; }
    uint8_t before = Read<uint8_t>(session.controller, profile::Controller_QuickBarReadyByte);
    Write<uint8_t>(session.controller, profile::Controller_QuickBarReadyByte, 1);
    LOG("[QB] ✅ readiness byte: %u -> 1 (raw write — HUD read path is now live)", before);
}

// [F92] Replay the EXACT native HUD read chain (GetItemInQuickbarSlot
// 0x6673508 + GetQuickBarSlotCount 0x66734cc, both fully disassembled):
//   1) PC->0x1A60 (ClientQuickBars) non-null       → else HUD reads nothing
//   2) PC->0x1A68 readiness byte non-zero           → else HUD reads nothing
//   3) quickbars+0x278/0x308 bar Count > slot      (bar+0x18)
//   4) quickbars vtable[0x6f0] GetItemInSlot(type, slot)
// If all four pass, the hotbar HUD WILL show our items; if any fails the
// log names the exact broken link. At most once per minute.
void Athena::VerifyHUDReadPath() {
    static double lastRun = -1e9;
    double now = Monotonic();
    if (now - lastRun < 60.0) return;
    lastRun = now;
    if (!session.controller || !session.quickbars || IsBadPtr(session.quickbars)) return;
    void* qbLink = Read<void*>(session.controller, profile::Controller_ClientQuickBars);
    uint8_t gate = Read<uint8_t>(session.controller, profile::Controller_QuickBarReadyByte);
    LOG("[QB] HUD-verify: PC->ClientQuickBars=%p qb=%p (%s) gate=%u", qbLink, session.quickbars,
        (qbLink == session.quickbars ? "MATCH" : "MISMATCH"), (unsigned)gate);
    void* vt = Read<void*>(session.quickbars);
    if (!vt || IsBadPtr(vt)) { LOG("[QB] HUD-verify: vtable invalid"); return; }
    using GIS_t = void* (*)(void*, int, int);
    GIS_t gis = reinterpret_cast<GIS_t>(Read<uintptr_t>(vt, profile::QuickBars_VtableSlot_GetItemInSlot));
    if (!gis || IsBadPtr((void*)gis)) { LOG("[QB] HUD-verify: GetItemInSlot vtable[0x6f0] invalid"); return; }
    const struct { const char* name; unsigned off; } bars[2] = {
        {"Primary", profile::QuickBars_PrimaryOffset}, {"Secondary", profile::QuickBars_SecondaryOffset} };
    for (int b = 0; b < 2; ++b) {
        Array slots = Read<Array>((char*)session.quickbars + bars[b].off, profile::QuickBar_SlotsOffset);
        LOG("[QB] HUD-verify: %s slots=%d", bars[b].name, slots.Count);
        for (int s = 0; s < slots.Count && s < 6; ++s) {
            void* item = nullptr;
            { Guard g; item = gis(session.quickbars, b, s); }
            LOG("[QB] HUD-verify:   %s[%d] -> %p %s", bars[b].name, s, item, item ? "OK" : "NULL");
        }
    }
}

// [F19] Bind through the native Kismet helper, then verify by reading the
// slot back. EquipItemToQuickBarSlot(WorldContextObject, ItemDefinition,
// ItemVariantGuid, QuickBarType, SlotIndex) exists in 21.30.
bool Athena::BindSlot(void* item, int slot, uint8_t bar) {
    if (!item || !session.controller || bar > 1 || slot < 0) return false;
    void* definition = nullptr;
    { Call d("/Script/FortniteGame.FortItem.GetItemDefinitionBP"); if (d.ok() && d.run(item)) definition = d.result<void*>(); }
    // [F43] fallback: read the definition straight from the item's entry
    // (FortWorldItem.ItemEntry.ItemDefinition) instead of failing silently.
    if (!definition) {
        void* entryProp = GetProp("/Script/FortniteGame.FortWorldItem.ItemEntry");
        void* defProp   = GetProp("/Script/FortniteGame.FortItemEntry.ItemDefinition");
        if (entryProp && defProp)
            definition = Read<void*>(GetField(GetField(item, entryProp), defProp));
    }
    if (!definition) { LOG("[QB] BindSlot: no definition (slot=%d bar=%u)", slot, bar); return false; }
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
    if (!barProp || !slotsProp || !itemsProp) { LOG("[QB] BindSlot: reflection missing"); return false; }
    void* quickbar = GetObjField(session.controller, "/Script/FortniteGame.FortPlayerController.ClientQuickBars");
    if (!quickbar) { LOG("[QB] BindSlot: ClientQuickBars missing"); return false; }
    void* barField = GetField(quickbar, barProp);
    Array slots = Read<Array>(GetField(barField, slotsProp));
    if (!slots.Data || slots.Count < 1 || slot >= slots.Count) {
        LOG("[QB] BindSlot: slots not built yet (bar=%u count=%d)", bar, slots.Count);
        return false;
    }
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

// ============================================================================
// [F36] QUICKBARS NATIVE — Erbium binds every item through the QuickBars
// actor itself (GiveItem → ServerAddItemInternal, swap → ServerActivateSlot-
// Internal, Remove → ServerRemoveItemInternal). In standalone those RPCs run
// locally, so the client HUD sees exactly what the native bookkeeping wrote —
// no manual slot surgery needed. Every helper degrades to the manual path
// when the UFunction is absent on this build.
// ============================================================================
bool Athena::QBAddItem(Guid guid, bool bSecondary, int slot) {
    void* qb = session.quickbars;
    if (!qb || IsBadPtr(qb) || !(guid.A | guid.B | guid.C | guid.D)) return false;
    Call add("/Script/FortniteGame.FortQuickBars.ServerAddItemInternal");
    if (!add.ok()) { LOG("[QB] ServerAddItemInternal not present"); return false; }
    bool bound = false;
    bound |= add.set("ItemGuid", guid) || add.set("Guid", guid) || add.set("InItemGuid", guid);
    bool sec = bSecondary;
    bound |= add.set("bSecondary", sec) || add.set("bSecondaryQuickBar", sec) || add.set("Secondary", sec) ||
             add.set_at(1, sec);
    int slotVal = slot;
    if (!add.set("SlotIndex", slotVal) && !add.set("Slot", slotVal) && !add.set("NewSlot", slotVal))
        add.set_at(2, slotVal);
    if (!add.run(qb)) return false;
    LOG("[QB] ✅ ServerAddItemInternal bar=%d slot=%d", (int)bSecondary, slot);
    return true;
}

void Athena::QBActivateSlot(bool bSecondary, int slot) {
    void* qb = session.quickbars;
    if (!qb || IsBadPtr(qb) || slot < 0) return;
    // [F36] Erbium parity: QuickBars.ServerActivateSlotInternal(bar, slot, delay, updatePrev)
    Call act("/Script/FortniteGame.FortQuickBars.ServerActivateSlotInternal");
    if (act.ok()) {
        uint8_t bar = bSecondary ? 1 : 0;
        float delay = 0.f; bool upd = true;
        bool bound = act.set("QuickBarType", bar) || act.set("bSecondary", bar) || act.set_at(0, bar);
        bound |= act.set("SlotIndex", slot) || act.set("Slot", slot) || act.set_at(1, slot);
        act.set("ActivateDelay", delay) || act.set_at(2, delay);
        act.set("bUpdatePreviousFocusedSlot", upd) || act.set_at(3, upd);
        if (bound && act.run(qb)) return;
    }
    // Legacy fallback: controller-level activation.
    if (!session.controller) return;
    Call legacy("/Script/FortniteGame.FortPlayerController.ClientActivateSlot");
    if (!legacy.ok()) return;
    float delay = 0.f; bool bUpdatePrev = true; bool bForce = true;
    if (legacy.set("InQuickBar", bSecondary ? (uint8_t)1 : (uint8_t)0) &&
        legacy.set("Slot", slot) && legacy.set("ActivateDelay", delay) &&
        legacy.set("bUpdatePreviousFocusedSlot", bUpdatePrev) && legacy.set("bForceExecution", bForce))
        legacy.run(session.controller);
}

void Athena::QBRemoveItem(Guid guid, int slot) {
    void* qb = session.quickbars;
    if (!qb || IsBadPtr(qb)) return;
    // Erbium Remove(): QuickBars->EmptySlot(bar, i) + ServerRemoveItemInternal(guid, false, true)
    if (slot >= 0 && slot < 6) {
        Call empty("/Script/FortniteGame.FortQuickBars.EmptySlot");
        if (empty.ok()) {
            bool sec = false; int s = slot;
            bool bound = empty.set("bSecondary", sec) || empty.set("Secondary", sec) || empty.set_at(0, sec);
            bound |= empty.set("SlotIndex", s) || empty.set("Slot", s) || empty.set_at(1, s);
            if (bound) empty.run(qb);
        }
    }
    Call rem("/Script/FortniteGame.FortQuickBars.ServerRemoveItemInternal");
    if (!rem.ok()) return;
    bool bound = rem.set("ItemGuid", guid) || rem.set("Guid", guid) || rem.set("InItemGuid", guid);
    bool f1 = false, f2 = true;
    rem.set("bForceRemoval", f1) || rem.set_at(1, f1);
    rem.set("bDestroyItem", f2) || rem.set_at(2, f2);
    if (bound) rem.run(qb);
}

// ============================================================================
// MOVEMENT FEATURES
// ============================================================================
void Athena::EnableMovementFeatures() {
    if (!session.pawn) return;
    LOG("[Movement] Enabling slide & swim...");

    bool canSlideOk = false;   // [F90-b/AUD-1] movement commits only when the critical gate landed
    Call canSlide("/Script/FortniteGame.FortPlayerPawn.SetCanSlide");
    if (canSlide.ok()) {
        bool bTrue = true;
        if (canSlide.set("bCanSlide", bTrue)) {
            if (canSlide.run(session.pawn)) { LOG("[Movement] ✅ SetCanSlide(true)"); canSlideOk = true; }
            else LOG("[Movement] SetCanSlide run failed");
        }
    } else LOG("[Movement] SetCanSlide not found");

    // [F51/F52/F83-b] bWantsToSlide is input/game-driven — forcing it on
    // permanently broke the slide state machine (the F51 fix the rewrite
    // lost). Only SetCanSlide stays forced; the actual SLIDE ability spec is
    // granted by GrantWorldAbilities below.

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

    // [F52/F67/F83-b] Slide is delivered as a REAL ability spec through a
    // mini FortAbilitySet + EquipFortAbilitySet (GrantWorldAbilities retries
    // from Tick until the BP classes stream in). The old direct
    // TryActivateAbilityByClass fire here activated the ability ONCE outside
    // the spec system — that path corrupted pawn state on device (F67).
    GrantWorldAbilities();

    // [F90-b/AUD-1] commit ONLY on SetCanSlide success: on failure the 8s
    // Tick retry re-invokes this function — GrantWorldAbilities is
    // flag-guarded (idempotent) and the comp fields are re-applied by
    // ApplySprintSpeed every 2s, so a re-run is free of side effects.
    session.movement_enabled = canSlideOk;
    LOG("[Movement] %s", canSlideOk ? "Done" : "incomplete — retry scheduled");
}

// ============================================================================
// [F41/F68/F83-b] BODY + LOCOMOTION ANIMS — Erbium assembles the body through
// parts only (F82 cosmetics flow, already present). Here we (a) re-run the
// part pipeline when no parts ever landed, (b) restore a MISSING
// AnimScriptInstance — without it the pawn has NO locomotion graph at all,
// which is exactly "dead run/slide anims" even with correct parts, (c) keep
// the body visible. No direct mesh swap (F68 — that caused the "sick body").
// Complements F82: F82 fixes the PARTS pipeline, this fixes the ANIM GRAPH.
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
        if (!animBP) animBP = FindLoadedObjectOfType("/Script/Engine.AnimBlueprintGeneratedClass", "M_MED_Lancelot_AnimBP_C");
        if (animBP) {
            Call a("/Script/Engine.SkeletalMeshComponent.SetAnimClass");
            if (a.ok() && a.set("NewClass", animBP) && a.run(mesh))
                LOG("[Body] AnimScriptInstance restored (locomotion)");
        }
    }
    animInst = GetObjField(mesh, "/Script/Engine.SkeletalMeshComponent.AnimScriptInstance");
    if (!animInst) return;   // AnimBP not resident yet — retry next pass

    Call body("/Script/FortniteGame.FortPlayerPawnAthena.SetCharacterBodyVisibilityForPossession");
    if (body.ok()) { bool bShow = true; if (body.set("bVisible", bShow)) body.run(session.pawn); }
    Call hid("/Script/Engine.Actor.SetActorHiddenInGame");
    if (hid.ok()) { bool h = false; if (hid.set("bNewHidden", h)) hid.run(session.pawn); }

    session.body_fixed = true;
    LOG("[Body] ✅ locomotion online — body via %zu character parts (Erbium)",
        session.character_parts.size());
}

// ============================================================================
// [F52/F67/F83-b] WORLD ABILITIES — slide + vehicle abilities delivered the
// Erbium way: build a mini FortAbilitySet and Equip it, so the ASC creates
// real ability specs with input bindings.
void Athena::GrantWorldAbilities() {
    if (session.world_abilities_granted || !session.pawn) return;

    // Erbium AFortGameMode::FinishWorldInitialization — 21.30 ability sets
static const char* const setPaths[] = {
    "/TacticalSprintGame/Gameplay/AS_TacticalSprint.AS_TacticalSprint",
    "/TacticalSprint/Gameplay/AS_TacticalSprint.AS_TacticalSprint",        // fallback
    "/Ascender/Gameplay/Ascender/AS_Ascender.AS_Ascender",
    "/DoorBashContent/Gameplay/AS_DoorBash.AS_DoorBash",
    "/HillScramble/Gameplay/AS_HillScramble.AS_HillScramble",              // ← جديد
    "/SlideImpulse/Gameplay/AS_SlideImpulse.AS_SlideImpulse",              // ← جديد
    "/RealitySeedGameplay/Environment/Foliage/GAS_Athena_RealitySapling.GAS_Athena_RealitySapling",
    nullptr
};

    int applied = 0, tried = 0;
    for (int i = 0; setPaths[i]; ++i) {
        void* set = LoadAsset(setPaths[i], "/Script/FortniteGame.FortAbilitySet");
        if (!set) set = FindObj(setPaths[i]);          // بعض المجموعات تُحمَّل كـ UObject عادي
        if (!set || IsBadPtr(set)) continue;
        ++tried;
        if (ApplyAbilitySet(session.pawn, set)) {
            session.granted_ability_sets.push_back(set);
            ++applied;
            LOG("[World] ✅ Applied: %s", setPaths[i]);
        }
    }

    // نعتبر النجاح إذا تم منح مجموعتين على الأقل (مثل HillScramble + DoorBash)
    if (applied >= 2) {
        session.world_abilities_granted = true;
        LOG("[World] ✅ %d/%d movement ability sets delivered (Erbium 1:1)", applied, tried);
    } else {
        LOG("[World] only %d/%d delivered — retry on next Tick", applied, tried);
    }
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

void Athena::TickMovementAnimation() {
    if (!session.pawn || IsBadPtr(session.pawn)) return;
    static double lastCheck = 0;
    double now = Monotonic();
    if (now - lastCheck < 0.15) return;
    lastCheck = now;

    // قراءة السرعة من CharacterMovement
    void* cmc = GetObjField(session.pawn, "/Script/Engine.Character.CharacterMovement");
    if (!cmc || IsBadPtr(cmc)) return;
    void* velProp = GetProp("/Script/Engine.MovementComponent.Velocity");
    if (!velProp || Read<int>(velProp, profile::PropElementSize) != 24) return;
    Vector vel = Read<Vector>(GetField(cmc, velProp));
    double hSpeed = std::sqrt(vel.X*vel.X + vel.Y*vel.Y);

    // --- Sprint detection ---
    bool sprinting = (hSpeed > 750.0);

    // اكتب TacticalSprint flags
    if (session.tactical_sprint && !IsBadPtr(session.tactical_sprint)) {
        uint8_t* flags = (uint8_t*)session.tactical_sprint + profile::TacticalSprint_Flags;
        if (sprinting)
            *flags |= profile::TacticalSprint_Bit_WantsSprint | profile::TacticalSprint_Bit_IsSprinting;
        else
            *flags &= (uint8_t)~(profile::TacticalSprint_Bit_IsSprinting);
    }

    // اكتب bIsSprinting على الـ pawn (أول مسار موجود)
    static const char* const sprintProps[] = {
        "/Script/FortniteGame.FortPlayerPawn.bIsSprinting",
        "/Script/FortniteGame.FortPawn.bIsSprinting",
        "/Script/FortniteGame.FortPlayerPawnAthena.bIsSprinting",
        "/Script/FortniteGame.FortPlayerPawn.bIsSprintActive",
        nullptr
    };
    for (int i = 0; sprintProps[i]; ++i) {
        if (!GetProp(sprintProps[i])) continue;
        bool current = GetBool(session.pawn, sprintProps[i]);
        if (current != sprinting) {
            SetBool(session.pawn, sprintProps[i], sprinting);
            std::string onRep = std::string(sprintProps[i]);
            auto dot = onRep.rfind('.');
            if (dot != std::string::npos) {
                onRep = onRep.substr(0, dot) + ".OnRep_bIsSprinting";
                Call r(onRep.c_str());
                if (r.ok()) r.run(session.pawn);
            }
            static int logged = 0;
            if (logged++ < 3) LOG("[Anim] %s=%d (speed=%.0f)", sprintProps[i], (int)sprinting, hSpeed);
        }
        break;
    }

    // --- Slide detection ---
    // ✅ الإصلاح: currentlySliding معرّف هنا، خارج if
    bool currentlySliding = false;
    uint8_t* slidingByte = (uint8_t*)session.pawn + profile::FortPlayerPawn_bIsSliding_Byte;
    if (!IsBadPtr(slidingByte) && IsReadable(slidingByte, 1)) {
        currentlySliding = ((*slidingByte >> profile::FortPlayerPawn_bIsSliding_Bit) & 1) != 0;
        uint8_t mode = Read<uint8_t>(cmc, profile::CharacterMovement_MovementMode);
        bool shouldSlide = (hSpeed > 900.0) && (mode == 3 || mode == 7);
        if (currentlySliding != shouldSlide) {
            if (shouldSlide) *slidingByte |= (1 << profile::FortPlayerPawn_bIsSliding_Bit);
            else             *slidingByte &= ~(1 << profile::FortPlayerPawn_bIsSliding_Bit);
            currentlySliding = shouldSlide;
            static int sLogged = 0;
            if (sLogged++ < 3) LOG("[Anim] bIsSliding=%d (speed=%.0f mode=%u)", (int)shouldSlide, hSpeed, mode);
        }
    }

    // --- AnimInstance fallback ---
    void* mesh = GetObjField(session.pawn, "/Script/Engine.Character.Mesh");
    if (mesh && !IsBadPtr(mesh)) {
        void* animInst = GetObjField(mesh, "/Script/Engine.SkeletalMeshComponent.AnimScriptInstance");
        if (animInst && !IsBadPtr(animInst)) {
            static bool dumped = false;
            if (!dumped) {
                dumped = true;
                void* sp = GetProp("/Script/FortniteGame.FortPlayerAnimInstance.bIsSprinting");
                void* sl = GetProp("/Script/FortniteGame.FortPlayerAnimInstance.bIsSliding");
                if (!sp) LOG("[Anim] ⚠️ bIsSprinting not on AnimInstance");
                if (!sl) LOG("[Anim] ⚠️ bIsSliding not on AnimInstance");
                if (sp && sl) LOG("[Anim] ✅ AnimInstance props OK");
            }
            SetBool(animInst, "/Script/FortniteGame.FortPlayerAnimInstance.bIsSprinting", sprinting);
            SetBool(animInst, "/Script/FortniteGame.FortPlayerAnimInstance.bIsSliding", currentlySliding);
        }
    }
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
    if (!session.mode || !session.state) return;
    LOG("[Bus] Attempting bus...");
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
void Athena::OnAircraftJump() { HandleJumpFromHudStart(); }
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
// DOORS — [F-door r1] 21.30 bDoorOpen is bit 5 of the 0x0FDE bit-pack
// ============================================================================
// 21.30 layout (MobileDumper7 dump, ABuildingWall 0x0FF8):
//   0x0FDE = bSwingingDoor(0)|bSlidingDoor(1)|bAutomaticSlidingDoor(2)|
//            bDoubleDoor(3)|bCreateDoorLink(4)|bDoorOpen(5,RepNotify)|
//            bLocalDoorOpen(6)|bDoorCollisionDisabled(7)
//   0x0FDF = bFastOpenRequested(4, Net|Transient)
// The previous "plain bool byte" write flipped the wall-type bits along with
// bDoorOpen — with bAutomaticSlidingDoor forced on, the native trigger path
// slammed the door shut a moment later: the door-closes-by-itself symptom.
// Every door write now touches only the door bits.
static void DoorOpenApply(void* wall, bool fast) {
    uint8_t* pack = (uint8_t*)wall + profile::BuildingWall_bDoorOpen_Byte;
    *pack |= 0x20 | 0x40;                        // bDoorOpen | bLocalDoorOpen
    if (fast) {
        uint8_t* fastPack = (uint8_t*)wall + profile::BuildingWall_bDoorOpen_Byte + 1;
        *fastPack |= 0x10;                       // bFastOpenRequested
    }
    Call r("/Script/FortniteGame.BuildingWall.OnRep_bDoorOpen");
    if (r.ok()) r.run(wall);

    Call netup("/Script/Engine.Actor.ForceNetUpdate");
    if (netup.ok()) netup.run(wall);
}

static void DoorCloseApply(void* wall) {
    uint8_t* pack = (uint8_t*)wall + profile::BuildingWall_bDoorOpen_Byte;
    *pack &= uint8_t(~(0x20 | 0x40));            // clear bDoorOpen|bLocalDoorOpen
    Call r("/Script/FortniteGame.BuildingWall.OnRep_bDoorOpen");
    if (r.ok()) r.run(wall);

    Call netup("/Script/Engine.Actor.ForceNetUpdate");
    if (netup.ok()) netup.run(wall);
}

void Athena::HandleDoor(void* door) {
    if (!door || IsBadPtr(door)) return;
    static void* wallCls = nullptr;
    if (!wallCls) wallCls = FindObj("/Script/FortniteGame.BuildingWall");
    if (!wallCls || !IsA(door, wallCls)) return;

    uint8_t* pack = (uint8_t*)door + profile::BuildingWall_bDoorOpen_Byte;
    const bool open = (*pack & 0x20) != 0;

    // Interact toggle: closing removes the stay-open pin first; opening
    // registers it so TickDoorPins can re-assert if anything slams it shut.
    bool wasPinned = false;
    for (auto it = session.door_pins.begin(); it != session.door_pins.end(); ++it) {
        if (it->wall == door) { wasPinned = true; session.door_pins.erase(it); break; }
    }
    if (!open && !wasPinned) {
        DoorOpenApply(door, /*fast=*/false);
        session.door_pins.push_back({door});
        if (session.door_pins.size() > 64) session.door_pins.erase(session.door_pins.begin());   // cap runaway growth
        LOG("[Door] OPEN (pinned, pins=%zu)", session.door_pins.size());
    } else {
        DoorCloseApply(door);
        LOG("[Door] CLOSED (was %s)", wasPinned ? "pinned" : "native-open");
    }

    // [F16] Blueprint event — resolve the receiver's override.
    Call interact("/Script/FortniteGame.BuildingActor.BlueprintOnInteract", door);
    if (interact.ok() && session.pawn) {
        if (interact.set("InteractingPawn", session.pawn)) interact.run(door);
    }
}

// [F-door r1] stay-open guard: whatever slams a door back shut on this build
// (trigger volume, replication reset, edit path), we only act when bDoorOpen
// actually flipped — doors we closed, and doors nobody touched, stay as-is.
void Athena::TickDoorPins() {
    if (session.door_pins.empty()) return;
    static double last = 0;
    double now = Monotonic();
    if (now - last < 0.30) return;
    last = now;
    int reasserted = 0;
    for (auto it = session.door_pins.begin(); it != session.door_pins.end(); ) {
        void* wall = it->wall;
        if (!wall || IsBadPtr(wall)) { it = session.door_pins.erase(it); continue; }
        uint8_t* pack = (uint8_t*)wall + profile::BuildingWall_bDoorOpen_Byte;
        if (!(*pack & 0x20)) {
            DoorOpenApply(wall, false);
            ++reasserted;
        }
        ++it;
    }
    if (reasserted) LOG("[Door] [F-door] re-asserted %d pinned doors", reasserted);
}

// [F-door r2] sprint fast-open (bFastOpenRequested — the 21.30 native behaviour
// pair of AutoOpenDoorInputMagnitude/AutoOpenDoorTraceDistance): while the
// player is tactically sprinting, closed BuildingWalls within 400 uu are
// queued (2 s scan throttle) and any one reached within 150 uu opens with the
// fast flag. Doors opened this way land in door_pins, so interact-close still
// works and the stay-open guard keeps them from slamming back.
static bool GetAllActorsOfClassRaw(void* klass, std::vector<void*>& out);   // [F-door r2] defined below
void Athena::TickSprintDoors() {
    if (!session.tactical_sprint || !session.pawn) return;
    if (IsBadPtr(session.tactical_sprint)) return;
    double now = Monotonic();
    const bool sprinting = (Read<uint8_t>(session.tactical_sprint, profile::TacticalSprint_Flags)
                            & profile::TacticalSprint_Bit_IsSprinting) != 0;
    if (!sprinting) return;

    // مسح دوري
    if (now - session.last_door_scan > 2.0) {
        session.last_door_scan = now;
        session.sprint_doors.clear();
        void* wallCls = FindObj("/Script/FortniteGame.BuildingWall");
        if (!wallCls) return;
        std::vector<void*> walls;
        if (GetAllActorsOfClassRaw(wallCls, walls)) {
            for (void* w : walls) {
                if (!w || IsBadPtr(w)) continue;
                uint8_t* pack = (uint8_t*)w + profile::BuildingWall_bDoorOpen_Byte;
                if (*pack & 0x20) continue;
                Vector loc{};
                if (!ActorLocation(w, loc) || !NearPlayer(loc, 400.0f)) continue;
                session.sprint_doors.push_back(w);
                if (session.sprint_doors.size() >= 32) break;
            }
        }
    }

    int opened = 0;
    for (void* w : session.sprint_doors) {
        if (!w || IsBadPtr(w)) continue;
        uint8_t* pack = (uint8_t*)w + profile::BuildingWall_bDoorOpen_Byte;
        if (*pack & 0x20) continue;
        Vector loc{};
        if (!ActorLocation(w, loc) || !NearPlayer(loc, 150.0f)) continue;
        static void* wallCls = nullptr;
        if (!wallCls) wallCls = FindObj("/Script/FortniteGame.BuildingWall");
        if (!wallCls || !IsA(w, wallCls)) continue;

        // ✅ جديد: جرّب تفعيل DoorBash الأصلي أولاً
        void* asc = GetASC(session.pawn);
        bool abilityFired = asc ? TryActivateHinted(asc, "DoorBash") : false;

        // fallback: فتح يدوي (يعمل دائماً)
        DoorOpenApply(w, /*fast=*/true);
        session.door_pins.push_back({w});
        if (session.door_pins.size() > 64) session.door_pins.erase(session.door_pins.begin());

        static int logged = 0;
        if (logged++ < 5)
            LOG("[Door] bash on %s ability=%d", ObjName(w).c_str(), (int)abilityFired);
        ++opened;
    }
    if (opened) LOG("[Door] [F-door] sprint fast-open %d", opened);
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
            Call ght("/Script/FortniteGame.FortHero.GetHeroTypeBP");
            if (ght.ok() && ght.run(heroDef)) heroType = ght.result<void*>();
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
    // [F82] ROOT CAUSE of dead run/slide anims: the pawn is spawned with an
    // EMPTY CosmeticLoadout, and on this client build the mesh + animation
    // stack only materialize through the loadout evaluation path
    // (OnRep_CosmeticLoadout — verified present in libUnreal.so; server RPCs
    // like EvaluateCharacterParts are stripped). Give the locker a REAL
    // default AthenaCharacterItemDefinition first, mirror it onto the pawn,
    // then the native OnRep rebuilds parts + anim instance.
    {
        void* loadoutProp = GetProp("/Script/FortniteGame.FortPlayerController.CosmeticLoadoutPC");
        void* charProp    = GetProp("/Script/FortniteGame.FortAthenaLoadout.Character");
        if (loadoutProp && charProp) {
            void* loadout = GetField(session.controller, loadoutProp);
            void* cur = GetObjField(loadout, "/Script/FortniteGame.FortAthenaLoadout.Character");
            if (!cur || IsBadPtr(cur)) {
                void* cid = LoadAsset("/Game/Athena/Items/Cosmetics/Characters/CID_001_Athena_Character_F.CID_001_Athena_Character_F",
                                      "/Script/FortniteGame.AthenaCharacterItemDefinition");
                if (!cid) cid = LoadAsset("/Game/Athena/Items/Cosmetics/Characters/CID_001_Athena_Character_M.CID_001_Athena_Character_M",
                                          "/Script/FortniteGame.AthenaCharacterItemDefinition");
                if (cid) {
                    Write<void*>(GetField(loadout, charProp), 0, cid);
                    LOG("[Cosmetics] default CID written into locker loadout");
                } else LOG("[Cosmetics] ⚠️ default CID asset not found");
            }
            // Mirror onto the pawn and fire the CLIENT-side evaluation.
            int pcSize = Read<int>(loadoutProp, profile::PropElementSize);
            void* pawnLoadoutProp = GetProp("/Script/FortniteGame.FortPlayerPawn.CosmeticLoadout");
            if (pawnLoadoutProp && Read<int>(pawnLoadoutProp, profile::PropElementSize) == pcSize) {
                memcpy(GetField(session.pawn, pawnLoadoutProp), loadout, (size_t)pcSize);
                // [F92] pass the receiver — enables the name-walk fallback
                // when StaticFindObject misses the UFunction (device-proven).
                Call ro("/Script/FortniteGame.FortPlayerPawn.OnRep_CosmeticLoadout", session.pawn);
                if (ro.ok()) { ro.run(session.pawn); LOG("[Cosmetics] ✅ pawn OnRep_CosmeticLoadout"); }
                else LOG("[Cosmetics] ⚠️ OnRep_CosmeticLoadout not present");
            }
        }
    }
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
    }
    // [F43] HeroType is NOT gated on the parts — it drives the pawn's
    // animation layers (root cause of the dead run/slide animation together
    // with the missing PlayerState).
    {
        void* hero = LoadAsset("/Game/Athena/Heroes/HID_001_Athena_Commando_F.HID_001_Athena_Commando_F", "/Script/FortniteGame.FortHeroType");
        if (!hero) hero = LoadAsset("/Game/Athena/Heroes/HID_001_Athena_Commando_M.HID_001_Athena_Commando_M", "/Script/FortniteGame.FortHeroType");
        if (!hero) hero = LoadAsset("/Game/Athena/Heroes/HID_Commando_Athena_01.HID_Commando_Athena_01", "/Script/FortniteGame.FortHeroType");
        void* heroTypeProp = GetProp("/Script/FortniteGame.FortPlayerState.HeroType");
        if (hero && heroTypeProp) Write<void*>(GetField(state, heroTypeProp), 0, hero);
    }
    // [F92] receivers passed — name-walk fallback covers StaticFindObject misses
    Call r1("/Script/FortniteGame.FortPlayerState.OnRep_HeroType", state); if (r1.ok()) r1.run(state);
    Call r3("/Script/FortniteGame.FortPlayerState.OnRep_CharacterData", state); if (r3.ok()) r3.run(state);
    // [F82] OnRep_CharacterParts exists client-side (verified) and drives the
    // state→pawn part evaluation — the old flow never called it.
    Call rcp("/Script/FortniteGame.FortPlayerState.OnRep_CharacterParts", state);
    if (rcp.ok()) rcp.run(state);
    void* klib = FindObj("/Script/FortniteGame.Default__FortKismetLibrary");
    if (klib) {
        Call upd("/Script/FortniteGame.FortKismetLibrary.UpdatePlayerCustomCharacterPartsVisualization");
        if (upd.ok() && upd.set("PlayerState", state)) upd.run(klib);
    }
    session.cosmetics_with_state = true;
    LOG("[Cosmetics] ✅ full flow with state done");
}
void Athena::ShowSkin() { LoadDefaultCosmetics(); }

void Athena::ForcePlayerVisible() {
    if (!session.pawn || IsBadPtr(session.pawn)) return;
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

// [F43] Erbium 1:1 standalone PlayerState (its CreatePlayer fallback path):
// the pawn was spawned and possessed WITHOUT a FortPlayerStateAthena, so the
// cosmetic/animation stack had no PlayerState to read — locomotion/slide
// animation stayed dead while movement itself worked. Erbium spawns the
// state, wires Controller+Pawn, fires both OnReps, sets owner and HeroType.
void Athena::EnsurePlayerState() {
    if (!session.controller || !session.world) return;
    void* state = GetObjField(session.controller, "/Script/Engine.Controller.PlayerState");
    if (!state || IsBadPtr(state)) {
        void* psCls = FindObj("/Script/FortniteGame.FortPlayerStateAthena");
        if (!psCls) psCls = FindObj("/Script/FortniteGame.FortPlayerState");
        if (!psCls) { LOG("[PS] PlayerState class missing"); return; }
        Transform t{};
        t.Location[0] = 0.0; t.Location[1] = 0.0; t.Location[2] = 5000.0;
        state = SpawnActor(session.world, psCls, session.controller, t);
        LOG("[PS] Spawned FortPlayerStateAthena: %p", state);
    }
    if (!state || IsBadPtr(state)) { LOG("[PS] no PlayerState available"); return; }
    session.player_state = state;

    void* pcPsProp = GetProp("/Script/Engine.Controller.PlayerState");
    if (pcPsProp) Write<void*>(GetField(session.controller, pcPsProp), 0, state);
    if (session.pawn) {
        void* pawnPsProp = GetProp("/Script/Engine.Pawn.PlayerState");
        if (pawnPsProp) Write<void*>(GetField(session.pawn, pawnPsProp), 0, state);
    }
    { Call so("/Script/Engine.Actor.SetOwner");
      if (so.ok() && (so.set("NewOwner", session.controller) || so.set_at(0, session.controller))) so.run(state); }

    // Erbium: PlayerController->OnRep_PlayerState(); Pawn->OnRep_PlayerState();
    { Call r("/Script/Engine.Controller.OnRep_PlayerState"); if (r.ok()) r.run(session.controller); }
    { Call r("/Script/Engine.Pawn.OnRep_PlayerState"); if (r.ok() && session.pawn) r.run(session.pawn); }

    // HeroType fallback exactly like Erbium's bot creation.
    void* heroProp = GetProp("/Script/FortniteGame.FortPlayerState.HeroType");
    if (heroProp && !Read<void*>(GetField(state, heroProp))) {
        void* hero = LoadAsset("/Game/Athena/Heroes/HID_001_Athena_Commando_F.HID_001_Athena_Commando_F", "/Script/FortniteGame.FortHeroType");
        if (!hero) hero = LoadAsset("/Game/Athena/Heroes/HID_Commando_Athena_01.HID_Commando_Athena_01", "/Script/FortniteGame.FortHeroType");
        if (hero) Write<void*>(GetField(state, heroProp), 0, hero);
    }
    { Call r("/Script/FortniteGame.FortPlayerState.OnRep_HeroType"); if (r.ok()) r.run(state); }
    { Call r("/Script/FortniteGame.FortPlayerState.OnRep_CharacterData"); if (r.ok()) r.run(state); }
    LOG("[PS] ✅ PlayerState ready: %p", state);
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
    LOG("SetupWorld: EnsurePlayerState"); EnsurePlayerState();   // [F43] fixes dead anim + cosmetics
    LOG("SetupWorld: GrantAllAbilitySets"); GrantAllAbilitySets();
    LOG("SetupWorld: SetupTacticalSprint"); SetupTacticalSprint();
    LOG("SetupWorld: EnableMovementFeatures"); EnableMovementFeatures();
    LOG("SetupWorld: ShowSkin"); ShowSkin();
    LOG("SetupWorld: DestroyLods"); DestroyLods();
    LOG("SetupWorld: InitializeInventory"); InitializeInventory();
    LOG("SetupWorld: ResetStaleContainers"); ResetStaleContainers();   // [F34]
    LOG("SetupWorld: SpawnFloorLoot"); SpawnFloorLoot();
    LOG("SetupWorld: SpawnVehicles"); SpawnVehicles();                 // [F28]
    LOG("SetupWorld: ApplySprintSpeed"); ApplySprintSpeed();
    LOG("SetupWorld: SpawnSafeZoneIndicator"); SpawnSafeZoneIndicator();
    // [F38] SpawnBuildPreviews removed — the hidden hand-rolled preview actors
    // collided with the native preview system when build mode ended (exit
    // crash) and were never used by the placement path.
    LOG("SetupWorld: DropLoadingScreen"); DropLoadingScreen();
    LOG("SetupWorld: SpawnBattleBus"); SpawnBattleBus();
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
    if (!session.player_state || IsBadPtr(session.player_state)) EnsurePlayerState();   // [F43]
    // [F82] anim restoration: at SetupWorld time the PlayerState did not exist
    // yet, so LoadDefaultCosmetics early-returned (log proof: cosmetics line
    // printed BEFORE 'SetupWorld: EnsurePlayerState'). Re-run the full flow
    // now that the state is guaranteed.
    if (!session.cosmetics_with_state) { LOG("Finalize: cosmetics with state"); LoadDefaultCosmetics(); }
    // Readiness can change possession — refresh cached pawn-dependent state.
    Call ps("/Script/Engine.Pawn.OnRep_PlayerState", pawn); if (ps.ok()) ps.run(pawn);
    if (!session.abilities_granted) GrantAllAbilitySets();
    if (!session.tactical_sprint_setup) SetupTacticalSprint();
    if (!session.movement_enabled) EnableMovementFeatures();
    FixBodyAndAnimations();        // [F41/F83-b] restore the locomotion AnimInstance
    GrantWorldAbilities();         // [F52/F83-b] slide + vehicle abilities as real specs
    if (!session.quickbars_initialized) {
        InitializeQuickBarsNative();
        if (!session.quickbars_initialized) BuildQuickBarsManually();
        SetQuickBarReady();
        ForceUpdateQuickBars();
        ActivateQuickBarSlot(0, 0);
    }
    VerifyHUDReadPath();   // [F92] once per minute max — names the exact broken HUD link if any
    if (!session.ready_called) {
        // [F89] commit only when the readiness RPC actually dispatched —
        // an unconditional commit here froze failed readiness forever
        // (same one-shot-commit family as F85/F88); the loading-screen
        // drop path retries until it lands.
        Call srt("/Script/FortniteGame.FortPlayerController.ServerReadyToStartMatch");
        bool srtOk = srt.ok() && srt.run(pc);
        Call cps("/Script/FortniteGame.FortPlayerControllerZone.ClientOnPawnSpawned"); if (cps.ok()) cps.run(pc);
        if (srtOk) session.ready_called = true;
    }
    Call restart("/Script/Engine.PlayerController.ClientRestart");
    if (restart.ok() && restart.set("NewPawn", pawn)) restart.run(pc);
    Call cam("/Script/Engine.PlayerController.SetViewTargetWithBlend");
    if (cam.ok() && cam.set("NewViewTarget", pawn)) cam.run(pc);
    session.visibility_forced = false;
    ForcePlayerVisible();
    SetPawnHealth(100.0f);
    SetupMatchState();   // [F40] AlivePlayers + PlayersLeft HUD state
    RefreshHUD();
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
    static double lastAnimCheck = 0;      // [F83-b] body/anim + world ability retries
    static double lastCosmeticsCheck = 0;  // [F90-c/AUD-1] cosmetics with-state retry
    if (session.finalized && now - lastVisibility > 8.0) { lastVisibility = now; if (!session.visibility_forced) ForcePlayerVisible(); }
if (session.finalized && session.pawn && now - lastSpeedReapply > 2.0) {
    lastSpeedReapply = now;
    ApplySprintSpeed();
}
if (session.finalized && session.pawn) {
    TickMovementAnimation();   // ← جديد
}
    // [F92] 30s heartbeat — the whole state machine in one line so ANY
    // partial device-log fragment always carries the state picture.
    static double lastHB = 0;
    if (session.finalized && now - lastHB > 30.0) {
        lastHB = now;
        uint8_t hbGate = session.controller ? Read<uint8_t>(session.controller, profile::Controller_QuickBarReadyByte) : 0;
        void* qbLink = session.controller ? Read<void*>(session.controller, profile::Controller_ClientQuickBars) : nullptr;
        int hbP = -1, hbS = -1;
        if (session.quickbars && !IsBadPtr(session.quickbars)) {
            Array p = Read<Array>((char*)session.quickbars + profile::QuickBars_PrimaryOffset, profile::QuickBar_SlotsOffset);
            Array s = Read<Array>((char*)session.quickbars + profile::QuickBars_SecondaryOffset, profile::QuickBar_SlotsOffset);
            hbP = p.Count; hbS = s.Count;
        }
        LOG("[HB] qb_init=%d qb_link=%s gate=%u bars=%d/%d loot_tables=%d veh=%d match=%d cosmetics=%d body=%d wabilities=%d",
            (int)session.quickbars_initialized,
            (qbLink == session.quickbars ? (session.quickbars ? "ok" : "n/a") : "BAD"),
            (unsigned)hbGate, hbP, hbS,
            local_loot::TableCount(),
            (int)session.vehicles_spawned,
            (int)session.match_state_ready,
            (int)session.cosmetics_with_state,
            (int)session.body_fixed,
            (int)session.world_abilities_granted);
    }
    // [F94-a] reviewer note (1): state can change after FinalizePlayer — the
    // internal 60s rate-limit makes this call free next to the heartbeat.
    VerifyHUDReadPath();
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
        if (!session.quickbars_initialized && session.qb_retries++ < 5) {
            LOG("Tick: QuickBars retry #%d", session.qb_retries);
            InitializeQuickBarsNative();
            if (!session.quickbars_initialized) BuildQuickBarsManually();
            if (session.quickbars_initialized) SetQuickBarReady();
        }
    }
    // [F88] match-state retry — mode/state may replicate late; re-invoke
    // until SetupMatchState fully covers AlivePlayers + PlayersLeft.
    static double lastMatchState = 0;
    if (session.finalized && session.controller && now - lastMatchState > 5.0) {
        lastMatchState = now;
        if (!session.match_state_ready) SetupMatchState();
    }
    if (session.finalized && session.pawn && now - lastMovementCheck > 8.0) {
        lastMovementCheck = now;
        if (!session.movement_enabled) EnableMovementFeatures();
    }
    if (session.finalized && session.pawn && now - lastAnimCheck > 8.0) {
        lastAnimCheck = now;
        FixBodyAndAnimations();       // [F83-b] retries until the AnimBP streams in
        GrantWorldAbilities();        // [F52] retries until the core ability pair streams in
    }
    if (session.finalized && now - lastCosmeticsCheck > 8.0) {   // [F90-c/AUD-1]
        lastCosmeticsCheck = now;
        if (!session.cosmetics_with_state) LoadDefaultCosmetics();   // re-runs the full with-state flow once PlayerState replicates
    }
    if (session.finalized) {
        PollTouchBuild();
        TickVehicleSeats();
        ProcessPendingPickups();   // [F35] fly-time → inventory transfer
        TickMantleAssist();        // [F45] 21.30 wall climb assist
        TickRealityProximity();    // [F44] reality tree proximity effect
        TickDoorPins();            // [F-door r1] doors stay open until we close them
        TickSprintDoors();         // [F-door r2] sprint fast-open
        TickBuildingLoads();       // [F-cities] drive LoadBuildings over a streaming window
        TickFloorLootStreaming();  // [F-loot-auto] roll floor loot as spawners stream in
    }
    // [F28/F41] one-shot vehicle spawning with a retry window (vehicle spawners
    // stream in after the world is ready — 12 attempts × 15 s ≈ 3 min max).
    static double lastVehicleTry = 0;
    if (session.finalized && session.world && now - lastVehicleTry > 15.0) {
        lastVehicleTry = now;
        if (!session.vehicles_spawned && session.vehicle_attempts < 12) SpawnVehicles();
    }
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
        // [F89] same conditional commit — see FinalizePlayer site
        Call srt("/Script/FortniteGame.FortPlayerController.ServerReadyToStartMatch");
        bool srtOk = srt.ok() && srt.run(session.controller);
        Call cps("/Script/FortniteGame.FortPlayerControllerZone.ClientOnPawnSpawned"); if (cps.ok()) cps.run(session.controller);
        if (srtOk) session.ready_called = true;
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
bool Athena::GiveItem(void* def, int count, int slot, uint8_t bar, bool require_new, int loadedAmmo, int level) {
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
        int lvl = level > 0 ? level : 1;   // [F31] loot level carry-over
        if (c.ok() && c.set("Count", count) && c.set("Level", lvl) && c.run(def)) inst = c.result<void*>();
    }
    if (!inst || !IsA(inst, FindObj("/Script/FortniteGame.FortWorldItem"))) return false;
    { Call c("/Script/FortniteGame.FortItem.SetOwningControllerForTemporaryItem");
      if (!c.ok() || !c.set("InController", session.controller) || !c.run(inst)) return false; }

    void* entrySrc = GetField(inst, itemEntryProp);
    if (!entrySrc) return false;
    void* guidProp = GetProp("/Script/FortniteGame.FortItemEntry.ItemGuid");
    if (!guidProp || Read<int>(guidProp, profile::PropElementSize) != sizeof(Guid)) return false;

    // [F36] Erbium GiveItem 1:1: OwnerInventory + ParentInventory bind the
    // instance to this inventory — without them the inventory UI shows the
    // item as orphaned and stacking/search miss it.
    {
        void* oiProp = GetProp("/Script/FortniteGame.FortWorldItem.OwnerInventory");
        if (oiProp) Write<void*>(GetField(inst, oiProp), 0, session.inventory);
        void* piProp = GetProp("/Script/FortniteGame.FortItemEntry.ParentInventory");
        if (piProp) Write<void*>(GetField(entrySrc, piProp), 0, session.inventory);
    }

    // Assign the native GUID before the deep copy so the replicated entry
    // and the instance share identity (Era core ordering). TrackerGuid is
    // zero-filled too (Erbium parity) — some equips read it.
    Guid guid = MakeGuid();
    if (!(guid.A | guid.B | guid.C | guid.D)) return false;
    Write<Guid>(GetField(entrySrc, guidProp), 0, guid);
    {
        void* trackerProp = GetProp("/Script/FortniteGame.FortItemEntry.TrackerGuid");
        if (trackerProp) {
            Guid t = Read<Guid>(GetField(entrySrc, trackerProp));
            if (!(t.A | t.B | t.C | t.D)) Write<Guid>(GetField(entrySrc, trackerProp), 0, MakeGuid());
        }
    }
    Guid reported = ItemGuidOf(inst);
    if (memcmp(&reported, &guid, sizeof(guid)) != 0) {
        LOG("Temporary item GUID accessor mismatch; insertion refused");
        return false;
    }

    Write<float>(entrySrc, profile::ItemEntry_Durability, 1.0f);
    if (IsA(def, FindObj("/Script/FortniteGame.FortWeaponItemDefinition"))) {
        // [F31] real clip size from the WeaponStatHandle row (Era GetStats
        // parity), or the pickup entry's own LoadedAmmo when carried over.
        int ammo = loadedAmmo >= 0 ? loadedAmmo : ClipSizeOf(def);
        Write<int32>(entrySrc, profile::ItemEntry_LoadedAmmo, ammo);
        if (GetBool(def, "/Script/FortniteGame.FortWeaponItemDefinition.bUsesPhantomReserveAmmo"))
            Write<int32>(entrySrc, profile::ItemEntry_PhantomAmmo, 300);
    }
    if (level > 1) {
        void* lvlProp = GetProp("/Script/FortniteGame.FortItemEntry.Level");
        if (lvlProp && Read<int>(lvlProp, profile::PropElementSize) == 4)
            Write<int32>(GetField(entrySrc, lvlProp), 0, level);
    }

    auto add = Fn<int(*)(void*,void*,const void*)>(profile::GenericArrayAdd);
    int idx = add(instArr, instProp, &inst);
    if (idx < 0) return false;
    int entryIdx = add(entryArr, entryProp, entrySrc);
    if (entryIdx < 0) { LOG("Entry copy failed after instance insertion; stop granting items"); return false; }
    // [F36] Erbium parity: flag the replicated copy + mark dirty so the
    // inventory UI and the net layer treat the new entry as authoritative.
    {
        void* brcProp = GetProp("/Script/FortniteGame.FortItemEntry.bIsReplicatedCopy");
        if (brcProp && Read<int>(brcProp, profile::PropElementSize) == 1) {
            void* list2 = GetField(session.inventory, invProp);
            Array reps = Read<Array>(GetField(list2, entryProp));
            if (entryIdx < reps.Count && reps.Data) {
                int stride = Read<int>(entryProp, profile::PropElementSize);
                int off = Read<int>(brcProp, profile::PropOffset);
                if (stride > 0 && off >= 0 && off < stride)
                    Write<uint8_t>((char*)reps.Data + (size_t)entryIdx * stride, off, 1);
            }
        }
    }

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
        // [F36] Native binding first (Erbium ServerAddItemInternal parity),
        // manual slot surgery only as the fallback.
        if (!QBAddItem(guid, bar == 1, slot)) BindSlot(inst, slot, bar);
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
    // [F43] SetOwner actually needs the owner param (the old call left it null).
    { Call so("/Script/FortniteGame.FortInventory.SetOwner");
      if (so.ok() && (so.set("NewOwner", session.controller) || so.set_at(0, session.controller))) so.run(session.inventory); }

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
            Write<void*>((char*)session.quickbars + profile::QuickBars_CtxOwner, 0, session.controller);
            Call soQB("/Script/FortniteGame.FortQuickBars.SetOwner");
            if (soQB.ok() && (soQB.set("NewOwner", session.controller) || soQB.set_at(0, session.controller))) soQB.run(session.quickbars);
            // [F43] Erbium binds the controller's QuickBars pointer (the
            // replicated prop) as well as ClientQuickBars — the hotbar HUD
            // reads one or the other depending on the build.
            void* qbp2 = GetProp("/Script/FortniteGame.FortPlayerController.QuickBars");
            if (qbp2 && qbp2 != qbProp) Write<void*>(GetField(session.controller, qbp2), 0, session.quickbars);
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
    }
}

void Athena::OnServerExecuteInventoryItem(Guid guid) {
    if (!session.inventory || !session.pawn) return;
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

// [F31] Resolve the GUID of an item already present in the world inventory
// (vehicle-seat weapon re-grants reuse the existing instance).
static Guid GuidOfDefInInventory(void* def) {
    void* invProp  = GetProp("/Script/FortniteGame.FortInventory.Inventory");
    void* instProp = GetProp("/Script/FortniteGame.FortItemList.ItemInstances");
    if (!def || !session.inventory || !invProp || !instProp) return Guid{};
    void* list = GetField(session.inventory, invProp);
    Array arr = Read<Array>(GetField(list, instProp));
    for (int i = 0; i < arr.Count; ++i) {
        void* inst = Read<void*>(arr.Data, (size_t)i * 8);
        if (inst && !IsBadPtr(inst) && ItemDefinitionOf(inst) == def) return ItemGuidOf(inst);
    }
    return Guid{};
}

// [F28/F31] Shared inventory removal: optionally spawns the world pickup
// (inventory-drop path) or silently removes (vehicle-seat weapon path —
// Erbium ServerOnExitVehicle removes the granted weapon without dropping).
static bool RemoveItemByGuid(Guid guid, bool drop, int count = -1) {
    if (!session.inventory) return false;
    void* invProp   = GetProp("/Script/FortniteGame.FortInventory.Inventory");
    void* instProp  = GetProp("/Script/FortniteGame.FortItemList.ItemInstances");
    void* entryProp = GetProp("/Script/FortniteGame.FortItemList.ReplicatedEntries");
    if (!invProp || !instProp || !entryProp) return false;
    void* list = GetField(session.inventory, invProp);
    void* instArr = GetField(list, instProp);
    void* entryArr = GetField(list, entryProp);
    Array arr = Read<Array>(instArr);
    for (int i = 0; i < arr.Count; ++i) {
        void* inst = Read<void*>(arr.Data, (size_t)i * 8);
        if (!inst) continue;
        Guid cand = ItemGuidOf(inst);
        if (memcmp(&cand, &guid, sizeof(Guid)) != 0) continue;
        void* def = ItemDefinitionOf(inst);
        int have = 1;
        if (drop) {
            void* itemEntryProp = GetProp("/Script/FortniteGame.FortWorldItem.ItemEntry");
            if (itemEntryProp) {
                void* e = GetField(inst, itemEntryProp);
                void* cp = GetProp("/Script/FortniteGame.FortItemEntry.Count");
                if (cp) have = Read<int>(GetField(e, cp));
            }
        }
        if (drop && def && session.pawn) {
            Vector loc{};
            ActorLocation(session.pawn, loc);
            loc.Z += 60.0;
            int dropped = count > 0 ? std::min(count, have > 0 ? have : count) : std::max(1, have);
            Athena::SpawnPickup(def, dropped, loc, 16 /*Tossed*/, 10 /*TossedByPlayer*/, true);
        }
        Fn<void(*)(void*,void*,int)>(profile::GenericArrayRemove)(instArr, instProp, i);
        Array entries = Read<Array>(entryArr);
        if (i < entries.Count)
            Fn<void(*)(void*,void*,int)>(profile::GenericArrayRemove)(entryArr, entryProp, i);
        for (int s = 0; s < 6; ++s) if (session.slots[(size_t)s] == inst) session.slots[(size_t)s] = nullptr;
        for (int s = 0; s < 6; ++s) if (session.secondary_slots[(size_t)s] == inst) session.secondary_slots[(size_t)s] = nullptr;
        // [F36] Erbium Remove(): empty the quickbar slot + native removal.
        Athena::QBRemoveItem(guid, i);
        Athena::InventoryUpdate();
        return true;
    }
    return false;
}

// ============================================================================
// PICKUPS — [F35] Erbium 1:1: AFortPlayerControllerAthena::InternalPickup
// (stacking / GiveOrSwapStack / GiveOrSwap) + the SetPickupTarget fly-to-
// player animation. In the standalone client the native chain
// (capsule overlap → ServerHandlePickup → FinishedTargetSpline) never
// completes because its inventory half lives on the dedicated-server build —
// loot either floated in place or flew to the player and vanished. This port
// performs the whole flow locally: native animation state + a scheduled,
// guaranteed inventory transfer.
// ============================================================================

// Erbium InternalPickup FinalLoc: pawn + forward*450 + z+50 + random 100u disc.
static Vector FinalLocForToss() {
    Vector loc{};
    if (!session.pawn || !ActorLocation(session.pawn, loc)) return loc;
    Vector fwd{1.0, 0.0, 0.0};
    { Call c("/Script/Engine.Actor.GetActorForwardVector");
      if (c.ok() && c.run(session.pawn)) fwd = c.result<Vector>(); }
    fwd.Z = 0.0;
    double len = std::sqrt(fwd.X * fwd.X + fwd.Y * fwd.Y);
    if (len > 0.0001) { fwd.X /= len; fwd.Y /= len; } else { fwd.X = 1.0; fwd.Y = 0.0; }
    loc.X += fwd.X * 450.0;
    loc.Y += fwd.Y * 450.0;
    loc.Z += 50.0;
    float angleVar = ((float)rand() * 0.00109866634f) - 18.0f;      // Erbium scaling
    double rad = (double)angleVar * 0.017453292519943295;
    loc.X += std::cos(rad) * 100.0;
    loc.Y += std::sin(rad) * 100.0;
    return loc;
}

// Erbium InternalPickup's ItemCount: occupied primary quickbar slots
// (pickaxe slot 0 excluded — IsPrimaryQuickbar(melee) is false in Erbium).
static int PrimaryStackCount() {
    int n = 0;
    for (int i = 1; i < 6; ++i)
        if (session.slots[(size_t)i] && !IsBadPtr(session.slots[(size_t)i])) ++n;
    return n;
}

static void EntryAmmoLevel(void* entry, int& loadedAmmo, int& level) {
    loadedAmmo = -1; level = 1;
    if (!entry) return;
    void* ap = GetProp("/Script/FortniteGame.FortItemEntry.LoadedAmmo");
    if (ap && Read<int>(ap, profile::PropElementSize) == 4) {
        int v = Read<int>(GetField(entry, ap));
        if (v > 0 && v <= 1000) loadedAmmo = v;
    }
    void* lp = GetProp("/Script/FortniteGame.FortItemEntry.Level");
    if (lp && Read<int>(lp, profile::PropElementSize) == 4) {
        int v = Read<int>(GetField(entry, lp));
        if (v > 1 && v <= 100) level = v;
    }
}

// [F35] SetPickupTarget emulation (Erbium's native-call fallback, 1:1):
//   PickupLocationData.PickupGuid / PickupTarget / FlyTime(+StartDirection)
//   → OnRep_PickupLocationData → bPickedUp → OnRep_bPickedUp
//   → Pawn.IncomingPickups.Add — this is what makes the pickup home to the
//   player exactly like a live-server pickup.
void Athena::SetPickupTargetAnim(void* pickup, float flyTime) {
    if (!pickup || IsBadPtr(pickup) || !session.pawn) return;
    void* entry = PickupEntry(pickup);
    void* data = GetObjField(pickup, "/Script/FortniteGame.FortPickup.PickupLocationData");
    if (entry && data && !IsBadPtr(data)) {
        void* guidProp = GetProp("/Script/FortniteGame.FortPickupLocationData.PickupGuid");
        if (guidProp && Read<int>(guidProp, profile::PropElementSize) == sizeof(Guid))
            Write<Guid>(GetField(data, guidProp), 0, Read<Guid>(GetField(entry, GetProp("/Script/FortniteGame.FortItemEntry.ItemGuid"))));
        // PickupTarget — a TWeakObjectPtr; build it from GObjects (cached).
        void* tgtProp = GetProp("/Script/FortniteGame.FortPickupLocationData.PickupTarget");
        if (tgtProp) {
            int sz = Read<int>(tgtProp, profile::PropElementSize);
            if (sz >= 8) {
                void* tgtField = GetField(data, tgtProp);
                MakeWeakRef(session.pawn, tgtField, session.pawn_weak_ready, session.pawn_weak_idx, session.pawn_weak_serial);
            }
        }
        // FlyTime — clamp: a zero/garbage FlyTime skips the whole animation.
        void* ftProp = GetProp("/Script/FortniteGame.FortPickupLocationData.FlyTime");
        if (ftProp && Read<int>(ftProp, profile::PropElementSize) == 4) {
            float ft = flyTime;
            if (!(ft > 0.05f) || !std::isfinite(ft)) ft = 0.45f;
            if (ft > 1.5f) ft = 1.5f;
            Write<float>(GetField(data, ftProp), 0, ft);
        }
        // StartDirection — small upward nudge (native sends the throw vector).
        void* sdProp = GetProp("/Script/FortniteGame.FortPickupLocationData.StartDirection");
        if (sdProp && Read<int>(sdProp, profile::PropElementSize) == 24)
            Write<Vector>(GetField(data, sdProp), 0, Vector{0.0, 0.0, 120.0});
        // bPlayPickupSound bit — best effort (struct-relative, on the data).
        SetBool(data, "/Script/FortniteGame.FortPickupLocationData.bPlayPickupSound", true);
        Call rep("/Script/FortniteGame.FortPickup.OnRep_PickupLocationData");
        if (rep.ok()) rep.run(pickup);
    }
    // bPickedUp + OnRep — hides the pickup from the ground-loot HUD and plays
    // the fly animation.
    SetBool(pickup, "/Script/FortniteGame.FortPickup.bPickedUp", true);
    { Call r("/Script/FortniteGame.FortPickup.OnRep_bPickedUp"); if (r.ok()) r.run(pickup); }
    // Pawn.IncomingPickups.Add(pickup) — the pawn keeps flying pickups here.
    {
        void* ipProp = GetProp("/Script/FortniteGame.FortPlayerPawn.IncomingPickups");
        if (!ipProp) ipProp = GetProp("/Script/FortniteGame.FortPawn.IncomingPickups");
        if (ipProp) {
            void* arrField = GetField(session.pawn, ipProp);
            if (arrField) {
                Array cur = Read<Array>(arrField);
                bool known = false;
                for (int i = 0; i < cur.Count && cur.Data; ++i)
                    if (Read<void*>(cur.Data, (size_t)i * 8) == pickup) { known = true; break; }
                if (!known && cur.Count >= 0 && cur.Count < 64)
                    Fn<int(*)(void*, void*, const void*)>(profile::GenericArrayAdd)(arrField, ipProp, &pickup);
            }
        }
    }
    LOG("[Pickup] SetPickupTarget anim fly=%.2f", flyTime > 0.05f && std::isfinite(flyTime) ? std::min(flyTime, 1.5f) : 0.45f);
}

// [F35] Entry point for every pickup request (ServerHandlePickup* RPCs and
// capsule overlaps). Deduplicates through bPickedUp + the pending list, sets
// the native animation, and schedules the inventory transfer for Tick.
void Athena::HandlePickupRPC(void* pickup, float flyTime) {
    if (!pickup || IsBadPtr(pickup) || !session.pawn || !session.inventory) return;
    if (!IsReadable(pickup, 0x40)) return;
    void* pickupCls = FindObj("/Script/FortniteGame.FortPickup");
    if (!pickupCls || !IsA(pickup, pickupCls)) return;
    if (GetBool(pickup, "/Script/FortniteGame.FortPickup.bPickedUp")) return;   // already in flight
    Vector location{};
    if (!ActorLocation(pickup, location)) return;
    if (!NearPlayer(location, 600.0f)) return;
    void* entry = PickupEntry(pickup);
    if (!entry || !EntryDefinition(entry)) return;   // nothing to give — let native handle
    for (auto& p : session.pending_pickups)
        if (p.pickup == pickup) return;              // already scheduled
    float ft = flyTime;
    if (!(ft > 0.05f) || !std::isfinite(ft)) ft = 0.45f;
    if (ft > 1.5f) ft = 1.5f;
    SetPickupTargetAnim(pickup, ft);
    session.pending_pickups.push_back({pickup, Monotonic() + (double)ft});
}

// [F35] Tick-side completion: after the fly time elapses, run the Erbium
// InternalPickup port and destroy the pickup (FinishedTargetSpline parity).
void Athena::ProcessPendingPickups() {
    if (session.pending_pickups.empty()) return;
    double now = Monotonic();
    std::vector<Session::PendingPickup> remaining;
    for (auto& p : session.pending_pickups) {
        if (now < p.at) { remaining.push_back(p); continue; }
        void* pickup = p.pickup;
        bool ok = false;
        // The tick runs on the game thread — GC cannot interleave here.
        if (pickup && !IsBadPtr(pickup) && IsReadable(pickup, 0x40)) {
            void* entry = PickupEntry(pickup);
            if (entry && EntryDefinition(entry)) ok = InternalPickup(entry, pickup);
        }
        if (ok) {
            Call d("/Script/Engine.Actor.K2_DestroyActor");
            if (d.ok() && pickup) d.run(pickup);
            LOG("[Pickup] ✅ transfer complete");
        } else {
            LOG("[Pickup] transfer skipped (dead pickup or full inventory) — dropped from queue");
        }
    }
    session.pending_pickups.swap(remaining);
}

// [F35] Erbium AFortPlayerControllerAthena::InternalPickup 1:1.
bool Athena::InternalPickup(void* entry, void* pickupActor) {
    if (!entry || !session.inventory) return false;
    void* def = EntryDefinition(entry);
    int count = EntryCount(entry);
    if (!def || count < 1 || count > 9999) return false;

    int loadedAmmo = -1, level = 1;
    EntryAmmoLevel(entry, loadedAmmo, level);

    int maxStack = MaxStackSizeOf(def);
    if (maxStack <= 0) maxStack = 1;
    int itemCount = PrimaryStackCount();

    bool bAllowMultipleStacks = false;
    { void* p = GetProp("/Script/FortniteGame.FortItemDefinition.bAllowMultipleStacks");
      if (p && Read<int>(p, profile::PropElementSize) == 1)
          bAllowMultipleStacks = Read<uint8_t>(GetField(def, p)) != 0; }

    void* invProp   = GetProp("/Script/FortniteGame.FortInventory.Inventory");
    void* instProp  = GetProp("/Script/FortniteGame.FortItemList.ItemInstances");
    void* entryProp = GetProp("/Script/FortniteGame.FortItemList.ReplicatedEntries");
    void* itemEntryProp = GetProp("/Script/FortniteGame.FortWorldItem.ItemEntry");
    void* countProp = GetProp("/Script/FortniteGame.FortItemEntry.Count");
    void* guidProp  = GetProp("/Script/FortniteGame.FortItemEntry.ItemGuid");
    void* defProp   = GetProp("/Script/FortniteGame.FortItemEntry.ItemDefinition");
    if (!invProp || !instProp || !entryProp || !itemEntryProp || !countProp || !guidProp || !defProp) return false;
    void* list = GetField(session.inventory, invProp);
    void* instArr = GetField(list, instProp);
    void* entryArr = GetField(list, entryProp);
    int entryStride = Read<int>(entryProp, profile::PropElementSize);
    if (entryStride <= 0 || entryStride > 1024) return false;

    // GiveItem wrapper that reports the GUID of the granted stack.
    // require_new is relaxed for multi-stack definitions (Erbium allows a
    // second stack); unique items stay duplicate-protected.
    auto giveNew = [&](int giveCount, int slot, uint8_t bar) -> Guid {
        if (!GiveItem(def, giveCount, slot, bar, !bAllowMultipleStacks, loadedAmmo, level)) return Guid{};
        return GuidOfDefInInventory(def);
    };
    // Re-toss (Erbium GiveOrSwapStack overflow / hotbar-full reject).
    auto retoss = [&](int outCount) {
        if (outCount < 1) return;
        Vector loc = FinalLocForToss();
        SpawnPickup(def, outCount, loc, 1 /*Player*/, 0, true, level, loadedAmmo);
    };
    // Equip + activate the slot the new item occupies (Erbium end of swap).
    auto equipNew = [&](Guid g) {
        if (!(g.A | g.B | g.C | g.D)) return;
        OnServerExecuteInventoryItem(g);
        for (int i = 0; i < 6; ++i)
            if (session.slots[(size_t)i] && !IsBadPtr(session.slots[(size_t)i])) {
                Guid ig = ItemGuidOf(session.slots[(size_t)i]);
                if (!memcmp(&ig, &g, sizeof g)) { QBActivateSlot(false, i); return; }
            }
    };

    if (maxStack > 1) {
        // ---- Erbium MaxStack > 1 branch (stack into an existing partial) ----
        void* item = nullptr;
        Array instances = Read<Array>(instArr);
        for (int i = 0; i < instances.Count; ++i) {
            void* cand = Read<void*>(instances.Data, (size_t)i * 8);
            if (!cand || IsBadPtr(cand) || ItemDefinitionOf(cand) != def) continue;
            void* ie = GetField(cand, itemEntryProp);
            if (Read<int>(GetField(ie, countProp)) < maxStack) { item = cand; break; }
        }
        void* matchingEntry = nullptr;
        Array replicated = Read<Array>(entryArr);
        for (int j = 0; j < replicated.Count && replicated.Data; ++j) {
            void* cand = (char*)replicated.Data + (size_t)j * entryStride;
            if (Read<void*>(GetField(cand, defProp)) != def) continue;
            if (Read<int>(GetField(cand, countProp)) < maxStack) { matchingEntry = cand; break; }
        }
        if (item && matchingEntry) {
            int original = Read<int>(GetField(matchingEntry, countProp));
            int updated = original + count;
            int overflow = 0;
            if (updated > maxStack) { overflow = updated - maxStack; updated = maxStack; }
            Write<int>(GetField(matchingEntry, countProp), 0, updated);
            // keep the instance entry in sync (Erbium: (*item)->ItemEntry.Count = ...)
            void* itemEntry = GetField(item, itemEntryProp);
            Write<int>(GetField(itemEntry, countProp), 0, updated);
            InventoryUpdate();
            LOG("[Pickup] stacked +%d -> %d/%d (%s)", count - overflow, updated, maxStack, ObjName(def).c_str());
            if (overflow > 0) {
                // Erbium GiveOrSwapStack: extra stacks when allowed, else re-toss
                if (bAllowMultipleStacks && itemCount < 5) {
                    GiveItem(def, overflow, -1, IsPrimaryQuickbarItem(def) ? (uint8_t)0 : (uint8_t)-1, true, loadedAmmo, level);
                } else {
                    retoss(overflow);
                }
            }
            return true;
        }
        // No partial stack — Erbium: full-stack guard for single-stack items.
        void* fullEntry = nullptr;
        for (int j = 0; j < replicated.Count && replicated.Data; ++j) {
            void* cand = (char*)replicated.Data + (size_t)j * entryStride;
            if (Read<void*>(GetField(cand, defProp)) == def) { fullEntry = cand; break; }
        }
        if (!matchingEntry && fullEntry && !bAllowMultipleStacks) { retoss(count); return true; }
        int giveCount = count;
        if (count > maxStack) { giveCount = maxStack; retoss(count - maxStack); }
        // ---- Erbium GiveOrSwap ----
        if (itemCount >= 5 && IsPrimaryQuickbarItem(def)) {
            void* cur = session.pawn ? Read<void*>(session.pawn, profile::FortPawn_CurrentWeapon) : nullptr;
            void* curDef = (cur && !IsBadPtr(cur)) ? GetObjField(cur, "/Script/FortniteGame.FortWeapon.WeaponData") : nullptr;
            if (cur && curDef && !IsBadPtr(curDef) && IsPrimaryQuickbarItem(curDef)) {
                // Toss the held weapon out in front of the player, remove, give.
                void* gp = GetProp("/Script/FortniteGame.FortWeapon.ItemEntryGuid");
                Guid curGuid = gp ? Read<Guid>(GetField(cur, gp)) : Guid{};
                if (curGuid.A | curGuid.B | curGuid.C | curGuid.D) {
                    int dropCount = 1;
                    Array insts = Read<Array>(instArr);
                    for (int i = 0; i < insts.Count; ++i) {
                        void* cand = Read<void*>(insts.Data, (size_t)i * 8);
                        if (cand && !IsBadPtr(cand)) {
                            Guid g = ItemGuidOf(cand);
                            if (!memcmp(&g, &curGuid, sizeof g)) {
                                void* ie = GetField(cand, itemEntryProp);
                                dropCount = std::max(1, Read<int>(GetField(ie, countProp)));
                                break;
                            }
                        }
                    }
                    Vector tossLoc{};
                    if (session.pawn) { ActorLocation(session.pawn, tossLoc); tossLoc.Z += 50.0; }
                    SpawnPickup(curDef, dropCount, tossLoc, 1 /*Player*/, 0, true, level, loadedAmmo);
                    RemoveItemByGuid(curGuid, false);
                    LOG("[Pickup] swap: tossed %s", ObjName(curDef).c_str());
                }
                int slot = -1;
                for (int i = 1; i < 6; ++i) if (!session.slots[(size_t)i]) { slot = i; break; }
                Guid g = giveNew(giveCount, slot, 0);
                equipNew(g);
                return true;
            }
            // Holding a non-primary (pickaxe): re-toss in front (Erbium parity).
            retoss(count);
            return true;
        }
        // Regular give — primary items take the first free slot.
        bool primary = IsPrimaryQuickbarItem(def);
        int slot = -1;
        if (primary) {
            for (int i = 1; i < 6; ++i) if (!session.slots[(size_t)i]) { slot = i; break; }
            if (slot < 0) { retoss(count); return true; }
        }
        Guid g = giveNew(giveCount, slot, primary ? (uint8_t)0 : (uint8_t)-1);
        return (g.A | g.B | g.C | g.D) != 0;
    }

    // ---- Erbium MaxStack <= 1 branch (unique items: weapons, gadgets) ----
    if (itemCount >= 5 && IsPrimaryQuickbarItem(def)) {
        void* cur = session.pawn ? Read<void*>(session.pawn, profile::FortPawn_CurrentWeapon) : nullptr;
        void* curDef = (cur && !IsBadPtr(cur)) ? GetObjField(cur, "/Script/FortniteGame.FortWeapon.WeaponData") : nullptr;
        if (cur && curDef && !IsBadPtr(curDef) && IsPrimaryQuickbarItem(curDef)) {
            void* gp = GetProp("/Script/FortniteGame.FortWeapon.ItemEntryGuid");
            Guid curGuid = gp ? Read<Guid>(GetField(cur, gp)) : Guid{};
            if (curGuid.A | curGuid.B | curGuid.C | curGuid.D) {
                int dropCount = 1;
                Array insts = Read<Array>(instArr);
                for (int i = 0; i < insts.Count; ++i) {
                    void* cand = Read<void*>(insts.Data, (size_t)i * 8);
                    if (cand && !IsBadPtr(cand)) {
                        Guid g = ItemGuidOf(cand);
                        if (!memcmp(&g, &curGuid, sizeof g)) {
                            void* ie = GetField(cand, itemEntryProp);
                            dropCount = std::max(1, Read<int>(GetField(ie, countProp)));
                            break;
                        }
                    }
                }
                Vector tossLoc{};
                if (session.pawn) { ActorLocation(session.pawn, tossLoc); tossLoc.Z += 50.0; }
                SpawnPickup(curDef, dropCount, tossLoc, 1 /*Player*/, 0, true, level, loadedAmmo);
                RemoveItemByGuid(curGuid, false);
                LOG("[Pickup] swap: tossed %s", ObjName(curDef).c_str());
            }
            int slot = -1;
            for (int i = 1; i < 6; ++i) if (!session.slots[(size_t)i]) { slot = i; break; }
            Guid g = giveNew(count, slot, 0);
            equipNew(g);
            return true;
        }
        retoss(count);
        return true;
    }
    bool primary = IsPrimaryQuickbarItem(def);
    int slot = -1;
    if (primary) {
        for (int i = 1; i < 6; ++i) if (!session.slots[(size_t)i]) { slot = i; break; }
        if (slot < 0) { retoss(count); return true; }
    }
    Guid g = giveNew(count, slot, primary ? (uint8_t)0 : (uint8_t)-1);
    return (g.A | g.B | g.C | g.D) != 0;
}

// [F35] Immediate pickup (compat path — era commands and direct requests).
bool Athena::LocalPickup(void* pickup) {
    if (!pickup || IsBadPtr(pickup)) return false;
    void* pickupCls = FindObj("/Script/FortniteGame.FortPickup");
    if (!pickupCls || !IsA(pickup, pickupCls)) return false;
    Vector location{};
    if (!ActorLocation(pickup, location) || !NearPlayer(location, 600.0f)) return false;
    void* entry = PickupEntry(pickup);
    if (!entry || !EntryDefinition(entry)) return true;
    if (!InternalPickup(entry, pickup)) return true;
    Call d("/Script/Engine.Actor.K2_DestroyActor");
    if (d.ok()) d.run(pickup);
    InventoryUpdate();
    return true;
}

void Athena::ServerHandlePickup(void* pickup) { HandlePickupRPC(pickup, 0.45f); }

// [F06/F33/F35] SpawnPickup — Erbium AFortInventory::SpawnPickup 1:1:
// entry (ItemDefinition/Count/Level/LoadedAmmo), PawnWhoDroppedPickup,
// source flags, bRandomRotation → OnRep_PrimaryPickupItemEntry → TossPickup
// (param names probed with fallbacks — Erbium's signature is
// (FinalLocation, Pawn, MaxStackCount, bToss, bCombine, SourceFlags, Source)).
void Athena::SpawnPickup(void* def, int count, Vector loc, uint8_t sourceFlags, uint8_t spawnSource, bool toss, int level, int loadedAmmo) {
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
    void* lvlProp = GetProp("/Script/FortniteGame.FortItemEntry.Level");
    void* ammProp = GetProp("/Script/FortniteGame.FortItemEntry.LoadedAmmo");
    if (entry && defProp) Write<void*>(GetField(entry, defProp), 0, def);
    if (entry && cntProp) Write<int>(GetField(entry, cntProp), 0, count);
    if (entry && lvlProp && level > 0 && Read<int>(lvlProp, profile::PropElementSize) == 4)
        Write<int>(GetField(entry, lvlProp), 0, level);
    if (entry && ammProp && loadedAmmo > 0 && Read<int>(ammProp, profile::PropElementSize) == 4)
        Write<int>(GetField(entry, ammProp), 0, loadedAmmo);
    // [F35] Erbium parity — the overlap pickup gate checks this pawn pointer.
    {
        void* pwdProp = GetProp("/Script/FortniteGame.FortPickup.PawnWhoDroppedPickup");
        if (pwdProp && session.pawn) Write<void*>(GetField(pickup, pwdProp), 0, session.pawn);
    }
    Write<uint8_t>(pickup, profile::AFortPickup_SourceFlags, sourceFlags);
    Write<uint8_t>(pickup, profile::AFortPickup_SpawnSource, spawnSource);
    SetBool(pickup, "/Script/FortniteGame.FortPickup.bRandomRotation", true);
    Call rep("/Script/FortniteGame.FortPickup.OnRep_PrimaryPickupItemEntry"); if (rep.ok()) rep.run(pickup);
    if (spawnSource == 2 || spawnSource == 4) {   // [F33] container-tossed FX
        SetBool(pickup, "/Script/FortniteGame.FortPickup.bTossedFromContainer", true);
        Call r3("/Script/FortniteGame.FortPickup.OnRep_TossedFromContainer");
        if (r3.ok()) r3.run(pickup);
    }
    if (toss) {
        Call c("/Script/FortniteGame.FortPickup.TossPickup");
        if (c.ok()) {
            bool locBound = c.set("FinalLocation", loc) || c.set("TargetLoc", loc) || c.set("InTargetLoc", loc);
            // [F35] Erbium param names — Pawn + MaxStackCount(-1).
            c.set("Pawn", session.pawn) || c.set("ItemOwner", session.pawn) || c.set("InItemOwner", session.pawn);
            int maxStack = -1;
            c.set("MaxStackCount", maxStack) || c.set("OverrideMaxStackCount", maxStack);
            bool bToss = true; c.set("bToss", bToss);
            bool combine = true; c.set("bShouldCombinePickupsWhenTossCompletes", combine);
            // EEFortPickupSourceTypeFlag (uint8) / EEFortPickupSpawnSource (uint8)
            c.set_any((uint8_t)sourceFlags, "InPickupSourceTypeFlags", "PickupSourceTypeFlags");
            c.set_any((uint8_t)spawnSource, "InPickupSpawnSource", "PickupSpawnSource");
            if (locBound) c.run(pickup);
            else LOG("[Pickup] TossPickup location unbound — pickup stays in place");
        }
    }
}

void Athena::DropInventoryItem(Guid guid, int count) {
    // [F31] shared removal path (visual drop + slot cleanup + inventory refresh)
    RemoveItemByGuid(guid, true, count);
}

// ============================================================================
// LOOT — [F20/F32] Erbium pipeline 1:1: DataTable-driven roll
// (ChooseLootForContainer + SetupLDSForPackage over the real cooked tables)
// with the curated table kept as a last-resort fallback.
// ============================================================================
namespace local_loot {
    struct Row  { const char* name; float weight; int count; };
    struct Tier { const char* group; int weaponsMin, weaponsMax, ammoMin, ammoMax, resources; float rarityBoost; };

    // Weapon short names — every row is resolved through ByShortName and
    // silently skipped when the asset is absent, so the table is safe to
    // extend for other seasons. [F32] 21.30 (Chapter 3) pool first.
    static const Row weapons[] = {
        {"WID_Assault_Hammer_Athena_SR",             20, 1},
        {"WID_Assault_Ranger_Athena_SR",             18, 1},
        {"WID_Assault_Auto_Athena_VR_SR",            14, 1},
        {"WID_SMG_Stinger_Athena_SR",                18, 1},
        {"WID_Shotgun_StrikerPump_Athena_SR",        16, 1},
        {"WID_Shotgun_Auto_Athena_SR",               10, 1},
        {"WID_Shotgun_LeverAction_Athena_SR",         8, 1},
        {"WID_Sniper_DMR_Athena_SR",                 10, 1},
        {"WID_Sniper_HunterBoltAction_Athena_SR",     8, 1},
        {"WID_Assault_Auto_Athena_C_Ore_T03",        12, 1},
        {"WID_Shotgun_Standard_Athena_C_Ore_T03",    10, 1},
        {"WID_Shotgun_SemiAuto_Athena_C_Ore_T03",     8, 1},
        {"WID_Sniper_Standard_Athena_C_Ore_T03",      6, 1},
        {"WID_Assault_Semi_Athena_C_Ore_T03",         6, 1},
        {"WID_SMG_Tactical_Athena_C_Ore_T03",         6, 1},
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
        {"Loot_AthenaAmmo",           0, 0, 2, 3, 0, 0.0f},
        {"Loot_AthenaFloorLoot",      1, 1, 1, 2, 1, 0.0f},
        {"Loot_AthenaFloorLoot_Warmup", 1, 1, 1, 2, 1, 0.0f},
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

    // ------------------------------------------------------------------
    // [F32] DataTable-driven roll — Erbium ChooseLootForContainer 1:1.
    // Sources: /Game/Items/Datatables/AthenaLootTierData_Client and
    // AthenaLootPackages_Client (playlist overrides are skipped: the
    // standalone session pins Playlist_DefaultSolo anyway).
    // ------------------------------------------------------------------
    struct Drop { void* def; int count; int level; };

    struct LPkgRow {
        std::string id;                 // LootPackageID (string form)
        float weight = 0;
        std::string call;               // LootPackageCall (recursion target)
        uint8_t soft[64]{};             // TSoftObjectPtr bytes (lazy resolve)
        int softStride = 0;
        int32 count = 0, category = -1, minW = -1, maxW = -1;
        void* def = nullptr;
    };
    struct LTierRow {
        std::string group;              // TierGroup
        float numDrops = 0, weight = 0;
        int32 lootTier = -1;
        std::string package;            // LootPackage
        std::vector<int32> wArr, minArr, maxArr;
    };
    struct Cache {
        std::vector<LTierRow> tiers;
        std::vector<LPkgRow> packages;
        std::map<std::string, std::vector<LTierRow*>> tierIndex;
        std::map<std::string, std::vector<LPkgRow*>> pkgIndex;
        bool ready = false;
        double failAt = -1000.0;
    };
    static Cache g_cache;

    // [F92] heartbeat accessor — table count for the [HB] state line
    int TableCount() {
        return g_cache.ready ? (int)(g_cache.tiers.size() + g_cache.packages.size()) : 0;
    }

    static int WorldLevelNow() {
        int lvl = 0;
        if (session.state) GetFieldValue<int32>(session.state, "/Script/FortniteGame.FortGameStateAthena.WorldLevel", lvl);
        if (lvl < 0 || lvl > 64) lvl = 0;
        return lvl;
    }
    static void* ResolveSoft(LPkgRow& r) {
        if (r.def) return r.def;
        if (!r.softStride) return nullptr;
        r.def = LoadSoftPtr(r.soft, r.softStride);
        return r.def;
    }
    template<class T> static T* PickWeighted(std::vector<T*>& rows) {
        float total = 0;
        for (auto* r : rows) total += r->weight;
        if (total <= 0) return rows.empty() ? nullptr : rows.front();
        float pick = random01() * total;
        for (auto* r : rows) {
            if (r->weight <= 0) continue;
            pick -= r->weight;
            if (pick < 0) return r;
        }
        return rows.back();
    }

    static void BuildCache() {
        if (g_cache.ready) return;
        if (Monotonic() - g_cache.failAt < 30.0) return;   // retry window
        auto pickTable = [&](const char* athena, const char* base) -> void* {
            void* t = FindObj(athena);
            if (!t) t = LoadAsset(athena, "/Script/Engine.DataTable");
            if (!t) { t = FindObj(base); if (!t) t = LoadAsset(base, "/Script/Engine.DataTable"); }
            return t;
        };
        void* tierTable = pickTable("/Game/Items/Datatables/AthenaLootTierData_Client.AthenaLootTierData_Client",
                                    "/Game/Items/Datatables/LootTierData_Client.LootTierData_Client");
        void* pkgTable  = pickTable("/Game/Items/Datatables/AthenaLootPackages_Client.AthenaLootPackages_Client",
                                    "/Game/Items/Datatables/LootPackages_Client.LootPackages_Client");
        if (!tierTable || !pkgTable) { LOG("[Loot] ⚠️ datatables unavailable (%p/%p) — curated fallback only", tierTable, pkgTable); g_cache.failAt = Monotonic(); return; }
        void* tierStruct = Read<void*>(tierTable, 0x28);   // UDataTable::RowStruct
        void* pkgStruct  = Read<void*>(pkgTable, 0x28);
        if (!tierStruct || !pkgStruct || IsBadPtr(tierStruct) || IsBadPtr(pkgStruct)) { LOG("[Loot] ⚠️ row structs missing"); g_cache.failAt = Monotonic(); return; }
        // [F86-a/LOOT-1] PropertiesSize@0xA8 reads 0/0 on this build (the
        // reported bug): raw read kept as primary when sane, FField-walk as
        // fallback + RowStruct validation via required members. Both values
        // logged — the first device run settles the true constant.
        static const char* const tierMust[] = { "LootPackage", "Weight", "TierGroup", "NumLootPackageDrops", "LootTier" };
        static const char* const pkgMust[]  = { "LootPackageID", "Weight", "ItemDefinition", "Count" };
        int tierRaw = Read<int>(tierStruct, profile::StructPropertiesSize);
        int pkgRaw  = Read<int>(pkgStruct, profile::StructPropertiesSize);
        int tierRowSize = (tierRaw > 0 && tierRaw <= 512) ? tierRaw : StructSizeByFFieldWalk(tierStruct, tierMust, 5);
        int pkgRowSize  = (pkgRaw  > 0 && pkgRaw  <= 512) ? pkgRaw  : StructSizeByFFieldWalk(pkgStruct,  pkgMust,  4);
        LOG("[Loot] row sizes: tier=0x%X (raw@0x54=%d) pkg=0x%X (raw@0x54=%d)",
            tierRowSize, tierRaw, pkgRowSize, pkgRaw);
        if (tierRowSize <= 0 || tierRowSize > 512 || pkgRowSize <= 0 || pkgRowSize > 512) { LOG("[Loot] ⚠️ bad row sizes %d/%d", tierRowSize, pkgRowSize); g_cache.failAt = Monotonic(); return; }
        LOG("[Loot] [F86] row sizes OK: tier=%d pkg=%d (UStruct::PropertiesSize @0x54)", tierRowSize, pkgRowSize);

        int toNum  = PropOffsetOn(tierStruct, "NumLootPackageDrops");
        int toGrp  = PropOffsetOn(tierStruct, "TierGroup");
        int toTier = PropOffsetOn(tierStruct, "LootTier");
        int toWgt  = PropOffsetOn(tierStruct, "Weight");
        int toPkg  = PropOffsetOn(tierStruct, "LootPackage");
        int toWA   = PropOffsetOn(tierStruct, "LootPackageCategoryWeightArray");
        int toMin  = PropOffsetOn(tierStruct, "LootPackageCategoryMinArray");
        int toMax  = PropOffsetOn(tierStruct, "LootPackageCategoryMaxArray");
        if (toGrp < 0 || toPkg < 0 || toWgt < 0 || toNum < 0) { LOG("[Loot] ⚠️ LootTierData offsets unresolved"); g_cache.failAt = Monotonic(); return; }
        int poId  = PropOffsetOn(pkgStruct, "LootPackageID");
        int poWgt = PropOffsetOn(pkgStruct, "Weight");
        int poCal = PropOffsetOn(pkgStruct, "LootPackageCall");
        int poDef = PropOffsetOn(pkgStruct, "ItemDefinition");
        int poCnt = PropOffsetOn(pkgStruct, "Count");
        int poCat = PropOffsetOn(pkgStruct, "LootPackageCategory");
        int poMin = PropOffsetOn(pkgStruct, "MinWorldLevel");
        int poMax = PropOffsetOn(pkgStruct, "MaxWorldLevel");
        int defStride = 24;
        { int s = 0; if (FindChildProp(pkgStruct, "ItemDefinition", &s) && s >= 8 && s <= 64) defStride = s; }
        if (poId < 0 || poWgt < 0 || poDef < 0) { LOG("[Loot] ⚠️ LootPackageData offsets unresolved"); g_cache.failAt = Monotonic(); return; }

        std::vector<FName> tierNames, pkgNames;
        if (!DataTableRowNames(tierTable, tierNames) || !DataTableRowNames(pkgTable, pkgNames)) { LOG("[Loot] ⚠️ row enumeration failed"); g_cache.failAt = Monotonic(); return; }
        LOG("[Loot] tables: tiers=%d rows, packages=%d rows", (int)tierNames.size(), (int)pkgNames.size());

        // Erbium dedupe: tiers by (TierGroup, LootPackage) — later rows
        // override; packages by row key — later rows override.
        std::map<uint64_t, LTierRow> tierDedupe;
        std::map<uint32_t, LPkgRow> pkgDedupe;
        uint8_t buf[512];
        auto readArr = [&](const uint8_t* base, int off) {
            std::vector<int32> v;
            if (off < 0) return v;
            Array a = Read<Array>(base, off);
            if (a.Count > 0 && a.Count <= 64 && a.Data && !IsBadPtr(a.Data))
                for (int i = 0; i < a.Count; ++i) v.push_back(Read<int32>(a.Data, (size_t)i * 4));
            return v;
        };
        for (auto& n : tierNames) {
            memset(buf, 0, sizeof(buf));
            if (!DataTableRow(tierTable, n, buf, tierRowSize)) continue;
            LTierRow r;
            r.numDrops = Read<float>(buf, toNum);
            r.weight   = Read<float>(buf, toWgt);
            r.lootTier = toTier >= 0 ? Read<int32>(buf, toTier) : -1;
            r.group    = FNameToString(buf + toGrp);
            r.package  = FNameToString(buf + toPkg);
            r.wArr  = readArr(buf, toWA);
            r.minArr = readArr(buf, toMin);
            r.maxArr = readArr(buf, toMax);
            uint32_t gIdx = Read<uint32>(buf, toGrp);
            uint32_t pIdx = Read<uint32>(buf, toPkg);
            tierDedupe[(uint64_t(gIdx) << 32) | pIdx] = r;
        }
        for (auto& n : pkgNames) {
            memset(buf, 0, sizeof(buf));
            if (!DataTableRow(pkgTable, n, buf, pkgRowSize)) continue;
            LPkgRow r;
            r.weight = Read<float>(buf, poWgt);
            r.id     = FNameToString(buf + poId);
            if (poCal >= 0) {
                FStringNative fs = Read<FStringNative>(buf, poCal);
                r.call = Narrow(fs.Data, fs.Count > 0 ? std::min(fs.Count, 256) : 0);
                if (fs.Data) Fn<void(*)(void*)>(profile::Free)(fs.Data);
            }
            if (poDef >= 0) {
                r.softStride = defStride;
                memcpy(r.soft, buf + poDef, (size_t)std::min(defStride, 64));
            }
            r.count    = poCnt >= 0 ? Read<int32>(buf, poCnt) : 1;
            r.category = poCat >= 0 ? Read<int32>(buf, poCat) : -1;
            r.minW     = poMin >= 0 ? Read<int32>(buf, poMin) : -1;
            r.maxW     = poMax >= 0 ? Read<int32>(buf, poMax) : -1;
            pkgDedupe[n.CompIdx] = r;
        }
        for (auto& kv : tierDedupe) g_cache.tiers.push_back(kv.second);
        for (auto& kv : pkgDedupe) g_cache.packages.push_back(kv.second);
        for (auto& r : g_cache.tiers) g_cache.tierIndex[r.group].push_back(&r);
        for (auto& r : g_cache.packages) g_cache.pkgIndex[r.id].push_back(&r);
        g_cache.ready = !g_cache.tiers.empty() && !g_cache.packages.empty();
        if (!g_cache.ready) g_cache.failAt = Monotonic();
        LOG("[Loot] cache ready=%d tiers=%zu packages=%zu", (int)g_cache.ready, g_cache.tiers.size(), g_cache.packages.size());
    }

    // [F33] Ammo companion entry (Erbium SetupLDSForPackage step 5): match the
    // ammo definition's owned gameplay tags against FortWeaponPickupSpawnAmmo-
    // Data rows. Degrades to 0 (no companion ammo) on any resolution failure.
    static int AmmoCountFor(void* ammoDef, int worldLevel) {
        (void)worldLevel;
        void* dataObj = FindObj("/Game/Athena/Balance/Pickups/FortWeaponPickupSpawnAmmoData.FortWeaponPickupSpawnAmmoData");
        if (!dataObj || IsBadPtr(dataObj)) return 0;
        void* arrProp = GetProp("/Script/FortniteGame.FortWeaponPickupSpawnAmmoData.WeaponPickupAmmoCountArray");
        if (!arrProp) return 0;
        int stride = Read<int>(arrProp, profile::PropElementSize);
        if (stride < 12 || stride > 64) return 0;
        Array rows = Read<Array>(GetField(dataObj, arrProp));
        if (!rows.Data || IsBadPtr(rows.Data) || rows.Count <= 0 || rows.Count > 32) return 0;
        void* inner = Read<void*>(arrProp, 0x70);            // FArrayProperty.Inner
        void* rs = inner ? Read<void*>(inner, 0x70) : nullptr; // FStructProperty.Struct
        if (!rs || IsBadPtr(rs)) return 0;
        int tagOff = PropOffsetOn(rs, "AmmoItemDefinitionTag");
        int cntOff = PropOffsetOn(rs, "SpawnCount");
        if (tagOff < 0 || cntOff < 0) return 0;
        float mult = 1.0f;
        void* multProp = GetProp("/Script/FortniteGame.FortWeaponPickupSpawnAmmoData.DefaultWeaponAmmoMultiplier");
        if (multProp && Read<int>(multProp, profile::PropElementSize) >= 4) {
            float m = Read<float>(GetField(dataObj, multProp));
            if (m > 0) mult = m;
        }
        // owned tags of the ammo definition (IGameplayTagAssetInterface vft[2])
        uint8_t tags[64]{};
        bool haveTags = false;
        void* ifaceType = FindObj("/Script/GameplayAbilities.GameplayTagAssetInterface");
        if (!ifaceType) ifaceType = FindObj("/Script/GameplayAbilities.IGameplayTagAssetInterface");
        if (ifaceType) {
            void* iface = ResolveInterface(ammoDef, ifaceType);
            if (iface && !IsBadPtr(iface)) {
                auto getTags = (void(*)(void*, void*))Read<uintptr_t>(iface, 0x10);
                if (getTags && !IsBadPtr((void*)getTags)) { getTags(iface, tags); haveTags = true; }
            }
        }
        bool found = false;
        int out = 0;
        for (int i = 0; i < rows.Count && !found; ++i) {
            void* row = (char*)rows.Data + (size_t)i * stride;
            uint32_t rowTag = Read<uint32>(row, tagOff);
            bool match = true;
            if (haveTags) {
                match = false;
                Array owned = Read<Array>(tags, 0);
                if (owned.Data && !IsBadPtr(owned.Data) && owned.Count > 0 && owned.Count <= 64)
                    for (int t = 0; t < owned.Count; ++t)
                        if (Read<uint32>(owned.Data, (size_t)t * 8) == rowTag) { match = true; break; }
            }
            if (!match) continue;
            float count = Read<float>(row, cntOff);
            out = count > 0 ? (int)(count * mult) : 0;
            found = true;
        }
        if (!found && !haveTags) {
            float count = Read<float>(rows.Data, cntOff);   // tags unavailable — first row
            out = count > 0 ? (int)(count * mult) : 0;
        }
        return out;
    }

    // [F32] Item level from the def's LootLevelData handle (Era GetLevel 1:1).
    static int ItemLevelOf(void* def, int worldLevel) {
        if (!def || worldLevel <= 0) return 0;
        void* p = GetProp("/Script/FortniteGame.FortItemDefinition.LootLevelData");
        if (!p) return 0;
        void* handle = GetField(def, p);
        void* table = Read<void*>(handle, 0);
        uint32_t rowContents = Read<uint32>(handle, 16);   // FDataTableCategoryHandle.RowContents
        if (!table || IsBadPtr(table) || !rowContents) return 0;
        void* rs = Read<void*>(table, 0x28);
        if (!rs || IsBadPtr(rs)) return 0;
        int rowSize = Read<int>(rs, profile::StructPropertiesSize);
        if (rowSize <= 0 || rowSize > 512)   // [F86-a/LOOT-1] 0xA8 reads 0 on this build
            rowSize = StructSizeByFFieldWalk(rs, nullptr, 0);   // Category/LootLevel validated below
        if (rowSize <= 0 || rowSize > 512) return 0;
        int catOff = PropOffsetOn(rs, "Category");
        if (catOff < 0) catOff = PropOffsetOn(rs, "category");
        int lvlOff = PropOffsetOn(rs, "LootLevel");
        int minOff = PropOffsetOn(rs, "MinItemLevel");
        int maxOff = PropOffsetOn(rs, "MaxItemLevel");
        if (catOff < 0 || lvlOff < 0) return 0;
        std::vector<FName> names;
        if (!DataTableRowNames(table, names)) return 0;
        int best = 0, bestLevel = 0;
        uint8_t buf[512];
        for (auto& n : names) {
            memset(buf, 0, sizeof(buf));
            if (!DataTableRow(table, n, buf, rowSize)) continue;
            if (Read<uint32>(buf, catOff) != rowContents) continue;
            int32_t ll = Read<int32>(buf, lvlOff);
            if (ll > worldLevel || ll <= bestLevel) continue;
            bestLevel = ll;
            int32_t mn = minOff >= 0 ? Read<int32>(buf, minOff) : 0;
            int32_t mx = maxOff >= 0 ? Read<int32>(buf, maxOff) : 0;
            int32_t span = mx - mn;
            int level = mn;
            if (span > 0) level = mn + (int)(random01() * float(span + 1));   // Erbium uniform pick
            if (level < 0) level = 0;
            best = level;
        }
        if (best > 0) {
            int32_t minL = 0, maxL = 0;
            if (GetFieldValue<int32>(def, "/Script/FortniteGame.FortItemDefinition.MinLevel", minL) &&
                GetFieldValue<int32>(def, "/Script/FortniteGame.FortItemDefinition.MaxLevel", maxL) && maxL > 0)
                best = std::clamp(best, minL, maxL);
        }
        return best;
    }

    static void SetupLDSForPackage(std::vector<Drop>& out, const std::string& package, int category, int worldLevel) {
        auto it = g_cache.pkgIndex.find(package);
        if (it == g_cache.pkgIndex.end() || it->second.empty()) return;
        std::vector<LPkgRow*> rows;
        for (LPkgRow* r : it->second) {
            if (category != -1 && r->category != category) continue;
            if (worldLevel >= 0) {
                if (r->minW >= 0 && worldLevel < r->minW) continue;
                if (r->maxW >= 0 && worldLevel > r->maxW) continue;
            }
            rows.push_back(r);
        }
        if (rows.empty()) return;
        LPkgRow* pkg = PickWeighted(rows);
        if (!pkg) return;
        if (pkg->call.size() > 1) {
            int times = pkg->count > 0 ? pkg->count : 1;
            for (int i = 0; i < times; ++i) SetupLDSForPackage(out, pkg->call, 0, worldLevel);   // Erbium recursion
            return;
        }
        void* def = ResolveSoft(*pkg);
        if (!def) return;
        // [F33] ammo companion entry
        Drop ammoDrop{nullptr, 0, 0};
        {
            Call g("/Script/FortniteGame.FortWorldItemDefinition.GetAmmoWorldItemDefinition_BP");
            if (g.ok() && g.run(def)) {
                void* ammoDef = g.result<void*>();
                if (ammoDef && !IsBadPtr(ammoDef)) {
                    int finalCount = AmmoCountFor(ammoDef, worldLevel);
                    if (finalCount > 0) ammoDrop = {ammoDef, finalCount, 0};
                }
            }
        }
        // Stack merging with MaxStackSize clamping (Erbium 1:1)
        int maxStack = MaxStackSizeOf(def);
        if (maxStack <= 0) maxStack = 1;
        for (auto& d : out) {
            if (d.def != def || d.count >= maxStack) continue;
            d.count += pkg->count;
            if (maxStack > 1 && d.count > maxStack) {
                out.push_back({def, d.count - maxStack, d.level});
                d.count = maxStack;
            }
            if (ammoDrop.def) {
                int ammoMax = MaxStackSizeOf(ammoDrop.def);
                for (auto& a : out) {
                    if (a.def != ammoDrop.def || ammoMax <= 1 || a.count >= ammoMax) continue;
                    a.count += ammoDrop.count;
                    if (a.count > ammoMax) { out.push_back({ammoDrop.def, a.count - ammoMax, 0}); a.count = ammoMax; }
                    ammoDrop.def = nullptr;
                    break;
                }
            }
            return;
        }
        if (pkg->count > 0) out.push_back({def, pkg->count, ItemLevelOf(def, worldLevel)});
        if (ammoDrop.def) out.push_back(ammoDrop);
    }

    // Erbium ChooseLootForContainer 1:1 (weighted tier row → DropCount with
    // probabilistic fraction → category min/max bumping → package execution).
    static void ChooseLootForContainer(std::vector<Drop>& out, const std::string& tierGroup, int lootTier, int worldLevel) {
        auto it = g_cache.tierIndex.find(tierGroup);
        if (it == g_cache.tierIndex.end() || it->second.empty()) return;
        std::vector<LTierRow*> rows;
        for (LTierRow* r : it->second)
            if (lootTier < 0 || r->lootTier == lootTier) rows.push_back(r);
        if (rows.empty()) return;
        LTierRow* tier = PickWeighted(rows);
        if (!tier || tier->numDrops <= 0) return;
        int dropCount;
        if (tier->numDrops < 1.f) dropCount = 1;
        else {
            dropCount = (int)((tier->numDrops * 2.f) - .5f) >> 1;
            float rem = tier->numDrops - (float)dropCount;
            if (rem > 0.0000099999997f && rem >= random01()) dropCount += 1;
        }
        std::map<int32_t, int32_t> numMap, weightMap;
        int amountOfDrops = 0, sumWeights = 0;
        for (size_t i = 0; i < tier->minArr.size(); ++i) { numMap[(int32_t)i] = tier->minArr[i]; amountOfDrops += tier->minArr[i]; }
        for (size_t i = 0; i < tier->wArr.size(); ++i) {
            int32_t cat = (int32_t)i, w = tier->wArr[i];
            int32_t mx = i < tier->maxArr.size() ? tier->maxArr[i] : -1;
            if (w > 0 && (mx < 0 || numMap[cat] < mx)) { weightMap[cat] = w; sumWeights += w; }
        }
        while (sumWeights > 0 && amountOfDrops < dropCount) {
            int randomWeight = (int)std::floor(random01() * (float)sumWeights);
            int category = -1;
            for (auto& kv : weightMap) {
                if (randomWeight <= kv.second && randomWeight <= (int)tier->numDrops) { category = kv.first; break; }
                randomWeight -= kv.second;
            }
            if (category == -1) break;
            amountOfDrops++;
            numMap[category]++;
            int32_t mx = category < (int)tier->maxArr.size() ? tier->maxArr[category] : -1;
            if (mx >= 0 && numMap[category] >= mx) { sumWeights -= tier->wArr[category]; weightMap.erase(category); }
            if (amountOfDrops >= dropCount) break;
        }
        for (auto& kv : numMap)                                    // ascending categories
            for (int i = 0; i < kv.second; ++i)
                SetupLDSForPackage(out, tier->package, kv.first, worldLevel);
    }

    // Entry point for SearchContainer — true when the DataTable path exists.
    static bool RollGroup(const std::string& group, std::vector<Drop>& out) {
        BuildCache();
        if (!g_cache.ready) return false;
        ChooseLootForContainer(out, group, -1, WorldLevelNow());
        return true;
    }
}

// [F32] GetAllActorsOfClass with a CORRECT out-param read. The old code used
// Call::result<Array>() which only reads ReturnParm params — GetAllActors-
// OfClass returns void with an out TArray, so the old floor-loot pass always
// saw 0 actors (one of the reasons the map had no loot at all).
static bool GetAllActorsOfClassRaw(void* klass, std::vector<void*>& out) {
    out.clear();
    void* wb = FindObj("/Script/Engine.Default__GameplayStatics");
    if (!wb || !klass || IsBadPtr(klass) || !session.world) return false;
    Call gac("/Script/Engine.GameplayStatics.GetAllActorsOfClass");
    if (!gac.ok()) return false;
    Array empty{};
    if (!gac.set("WorldContextObject", session.world) ||
        !gac.set("ActorClass", klass) ||
        !gac.set_bytes("OutActors", &empty, sizeof(Array))) return false;
    if (!gac.run(wb)) return false;
    void* slot = gac.param_slot("OutActors");
    if (!slot) return false;
    Array res = Read<Array>(slot);
    if (res.Count < 0 || res.Count > 65536) { if (res.Data) Fn<void(*)(void*)>(profile::Free)(res.Data); return false; }
    out.reserve((size_t)res.Count);
    for (int i = 0; i < res.Count; ++i) {
        void* a = Read<void*>(res.Data, (size_t)i * 8);
        if (a && !IsBadPtr(a)) out.push_back(a);
    }
    if (res.Data) Fn<void(*)(void*)>(profile::Free)(res.Data);
    return true;
}

// [F34] Standalone worlds roll containers with EMPTY native caches (the
// server-side FinishWorldInitialization never runs here), so any bAlready-
// Searched flag observed before the first interaction is a dead roll. Reset
// it (honoring bStartAlreadySearched_Athena) so chests/ammo actually open.
void Athena::ResetStaleContainers() {
    void* cls = FindObj("/Script/FortniteGame.BuildingContainer");
    if (!cls) return;
    std::vector<void*> actors;
    if (!GetAllActorsOfClassRaw(cls, actors)) return;
    int reset = 0;
    for (void* c : actors) {
        if (!c || IsBadPtr(c)) continue;
        if (!GetBool(c, "/Script/FortniteGame.BuildingContainer.bAlreadySearched")) continue;
        if (GetBool(c, "/Script/FortniteGame.BuildingContainer.bStartAlreadySearched_Athena")) continue;
        SetBool(c, "/Script/FortniteGame.BuildingContainer.bAlreadySearched", false);
        ++reset;
    }
    LOG("[Loot] stale-search reset on %d/%zu containers", reset, actors.size());
}

// [F06/F20/F32/F33] Container search: Erbium DataTable roll first, curated
// table fallback, container-axis placement + guarded toss.
bool Athena::SearchContainer(void* actor, const Vector& location, bool force) {
    void* searchedProp = GetProp("/Script/FortniteGame.BuildingContainer.bAlreadySearched");
    if (!actor || !searchedProp) return false;
    if (!force && GetBool(actor, "/Script/FortniteGame.BuildingContainer.bAlreadySearched")) return true;

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

    std::vector<local_loot::Drop> drops;
    bool usedTables = local_loot::RollGroup(group, drops);
    if (!usedTables) {
        // Curated fallback (DataTable roll unavailable).
        const local_loot::Tier* tier = nullptr;
        for (auto& t : local_loot::tiers)
            if (group == t.group) { tier = &t; break; }
        if (!tier) { LOG("[Loot] group not ported: %s", group.c_str()); return false; }
        int weapons = tier->weaponsMax > tier->weaponsMin ? local_loot::rollBetween(tier->weaponsMin, tier->weaponsMax) : tier->weaponsMin;
        int ammo = tier->ammoMax > tier->ammoMin ? local_loot::rollBetween(tier->ammoMin, tier->ammoMax) : tier->ammoMin;
        for (int i = 0; i < weapons; ++i)
            if (void* def = local_loot::pickWeapon(tier->rarityBoost)) drops.push_back({def, 1, 0});
        for (int i = 0; i < ammo; ++i)
            if (void* def = local_loot::pickRow(local_loot::ammo, sizeof(local_loot::ammo) / sizeof(local_loot::Row))) {
                int cnt = 30 + (int)(local_loot::random01() * 60.0f);
                drops.push_back({def, cnt, 0});
            }
        for (int i = 0; i < tier->resources; ++i)
            if (void* def = local_loot::pickRow(local_loot::resources, sizeof(local_loot::resources) / sizeof(local_loot::Row)))
                drops.push_back({def, 50, 0});
    }
    if (drops.empty()) { LOG("[Loot] roll produced no drops for %s", group.c_str()); return false; }

    // [F33] Container-relative placement (Erbium SpawnPickup(Container) 1:1):
    // container origin offset by LootSpawnLocation_Athena along F/R/U axes.
    Vector fwd{1.0, 0.0, 0.0}, right{0.0, 1.0, 0.0}, up{0.0, 0.0, 1.0};
    {
        Call c("/Script/Engine.Actor.GetActorForwardVector");
        if (c.ok() && c.run(actor)) fwd = c.result<Vector>();
        c = Call("/Script/Engine.Actor.GetActorRightVector");
        if (c.ok() && c.run(actor)) right = c.result<Vector>();
        c = Call("/Script/Engine.Actor.GetActorUpVector");
        if (c.ok() && c.run(actor)) up = c.result<Vector>();
    }
    Vector off{60.0, 0.0, 40.0};
    {
        void* p = GetProp("/Script/FortniteGame.BuildingContainer.LootSpawnLocation_Athena");
        if (p && Read<int>(p, profile::PropElementSize) == 24) off = Read<Vector>(GetField(actor, p));
        else {
            p = GetProp("/Script/FortniteGame.BuildingContainer.LootSpawnLocation");
            if (p && Read<int>(p, profile::PropElementSize) == 24) off = Read<Vector>(GetField(actor, p));
        }
    }

    // Resolve/spawn every pickup first; roll back partial spawns so a retry
    // cannot duplicate loot (Era core guarantee).
    std::vector<void*> spawned;
    for (size_t i = 0; i < drops.size(); ++i) {
        double spreadX = (double)(i % 3) * 30.0;
        double spreadY = (double)(i / 3) * 30.0;
        Vector loc{
            location.X + fwd.X * off.X + right.X * off.Y + up.X * off.Z + spreadX,
            location.Y + fwd.Y * off.X + right.Y * off.Y + up.Y * off.Z + spreadY,
            location.Z + fwd.Z * off.X + right.Z * off.Y + up.Z * off.Z + 40.0
        };
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
                void* lvlProp = GetProp("/Script/FortniteGame.FortItemEntry.Level");
                if (entry && defProp) Write<void*>(GetField(entry, defProp), 0, drops[i].def);
                if (entry && cntProp) Write<int>(GetField(entry, cntProp), 0, drops[i].count);
                if (entry && lvlProp && drops[i].level > 0 && Read<int>(lvlProp, profile::PropElementSize) == 4)
                    Write<int>(GetField(entry, lvlProp), 0, drops[i].level);
                Write<uint8_t>(pickup, profile::AFortPickup_SourceFlags, 4);
                Write<uint8_t>(pickup, profile::AFortPickup_SpawnSource, spawnSource);
                SetBool(pickup, "/Script/FortniteGame.FortPickup.bRandomRotation", true);   // [F33]
                Call rep("/Script/FortniteGame.FortPickup.OnRep_PrimaryPickupItemEntry");
                if (rep.ok()) rep.run(pickup);
                // [F35] Erbium SpawnPickup(Container) parity: without TossPickup
                // the loot hovers where it spawned (above the chest) and the
                // pickup overlap never sees it properly — toss it out like the
                // native chest does (fallback branch of the Erbium container
                // spawner) and mark the container-toss FX.
                {
                    Call tp("/Script/FortniteGame.FortPickup.TossPickup");
                    if (tp.ok()) {
                        bool bound = tp.set("FinalLocation", loc) || tp.set("InTargetLoc", loc) || tp.set("TargetLoc", loc);
                        tp.set("Pawn", session.pawn) || tp.set("ItemOwner", session.pawn);
                        int maxStack = -1;
                        tp.set("MaxStackCount", maxStack) || tp.set("OverrideMaxStackCount", maxStack);
                        bool bToss = true; tp.set("bToss", bToss);
                        bool combine = true; tp.set("bShouldCombinePickupsWhenTossCompletes", combine);
                        uint8_t sf = 4; tp.set_any(sf, "InPickupSourceTypeFlags", "PickupSourceTypeFlags");
                        tp.set_any(spawnSource, "InPickupSpawnSource", "PickupSpawnSource");
                        if (bound) tp.run(pickup);
                    }
                    SetBool(pickup, "/Script/FortniteGame.FortPickup.bTossedFromContainer", true);
                    Call r3("/Script/FortniteGame.FortPickup.OnRep_TossedFromContainer");
                    if (r3.ok()) r3.run(pickup);
                }
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
    LOG("[Loot] ✅ Container search %s spawned=%zu tables=%d", group.c_str(), spawned.size(), (int)usedTables);
    return true;
}

void Athena::Loot(void* actor) {
    if (!actor || !session.pawn) return;
    LOG("[Loot] Opening %s", ObjName(Read<void*>(actor, profile::ObjectClass)).c_str());
    Vector location{};
    bool haveLoc = ActorLocation(actor, location);

    // Local roll first (offline-deterministic); fall back to the native
    // BP_SpawnLoot when the group is not ported.
    if (haveLoc && SearchContainer(actor, location)) return;
    Call spawn("/Script/FortniteGame.BuildingContainer.BP_SpawnLoot");
    if (spawn.ok()) {
        if (spawn.set("PlayerPawn", session.pawn)) {
            if (spawn.run(actor)) {
                SetBool(actor, "/Script/FortniteGame.BuildingContainer.bAlreadySearched", true);
                Call r1("/Script/FortniteGame.BuildingContainer.OnRep_bAlreadySearched"); if (r1.ok()) r1.run(actor);
                Call r2("/Script/FortniteGame.BuildingContainer.BounceContainer"); if (r2.ok()) r2.run(actor);
                LOG("[Loot] ✅ native BP_SpawnLoot path");
                return;
            }
        }
    }
    LOG("[Loot] ❌ container search failed (local+native)");
}

// [F20/F32/F34] Floor loot: force-roll every floor-loot spawner through the
// DataTable pipeline, then destroy the spawner itself (Erbium ≥11 behaviour —
// stock builds remove the spawners once the loot is on the ground).
void Athena::SpawnFloorLoot() {
    LOG("[Loot] Spawning floor loot...");
    const char* containers[] = {
        "/Game/Athena/Environments/Blueprints/Tiered_Athena_FloorLoot_01.Tiered_Athena_FloorLoot_01_C",
        "/Game/Athena/Environments/Blueprints/Tiered_Athena_FloorLoot_Warmup.Tiered_Athena_FloorLoot_Warmup_C",
        nullptr
    };
    int total = 0, destroyed = 0;
    for (int i = 0; containers[i]; ++i) {
        void* cls = FindObj(containers[i]);
        if (!cls) cls = LoadClass(containers[i], "/Script/Engine.Actor");
        if (!cls) continue;
        std::vector<void*> actors;
        if (!GetAllActorsOfClassRaw(cls, actors)) continue;
        LOG("[Loot] Type %d: %zu actors", i, actors.size());
        for (void* c : actors) {
            if (!c || IsBadPtr(c)) continue;
            bool ok = false;
            Vector loc{};
            if (ActorLocation(c, loc)) ok = SearchContainer(c, loc, /*force=*/true);   // [F34] force: BeginPlay may have dead-rolled
            if (!ok) {
                // Native BP path second (it rolls the real cooked tables).
                Call bp("/Script/FortniteGame.BuildingContainer.BP_SpawnLoot");
                if (bp.ok() && bp.set("PlayerPawn", session.pawn)) ok = bp.run(c);
            }
            if (ok) ++total;
            // [F20 1:1] the spawner container is removed after rolling
            Call d("/Script/Engine.Actor.K2_DestroyActor");
            if (d.ok() && d.run(c)) ++destroyed;
        }
    }
    LOG("[Loot] ✅ floor loot rolled on %d spawners (%d destroyed)", total, destroyed);
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

// [F30] Erbium spawn order 1:1: SpawnActorUnfinished →
// InitializeKismetSpawnedBuildingActor → FinishSpawnActor → level/OnRep →
// SetMirrored → bPlayerPlaced → Team/TeamIndex bookkeeping.
bool Athena::SpawnBuildingActor(void* klass, const Vector& loc, const Rotator& rot, bool mirrored, int level) {
    if (!session.world || !klass || IsBadPtr(klass)) return false;
    void* smCls = FindObj("/Script/FortniteGame.BuildingSMActor");
    if (!smCls || !SubclassOfFast(klass, smCls)) return false;
    Transform t{};
    t.Location[0] = loc.X; t.Location[1] = loc.Y; t.Location[2] = loc.Z;
    RotatorToQuat(rot, t.Rotation);
    void* actor = Fn<void*(*)(void*, void*, const Transform*, uint8_t, void*)>(profile::BeginSpawn)(session.world, klass, &t, 1, session.controller);
    if (!actor) { LOG("[Build] BeginSpawn failed"); return false; }
    bool initOk = false;
    {
        Call init("/Script/FortniteGame.BuildingActor.InitializeKismetSpawnedBuildingActor");
        if (init.ok()) {
            init.set("BuildingOwner", actor);
            if (!init.set("SpawningController", session.controller))
                init.set("CreatingController", session.controller);
            bool animations = true;
            init.set("bUsePlayerBuildAnimations", animations);
            initOk = init.run(actor);
        }
    }
    void* finished = Fn<void*(*)(void*, const Transform*)>(profile::FinishSpawn)(actor, &t);
    if (!finished) { LOG("[Build] FinishSpawn failed"); return false; }
    if (!initOk) {
        // [F30] Erbium parity: abort the placement when init fails (the old
        // hook destroyed AFTER FinishSpawn, leaving partial state behind).
        LOG("[Build] init failed — piece destroyed (Erbium parity)");
        Call d("/Script/Engine.Actor.K2_DestroyActor");
        if (d.ok()) d.run(actor);
        return false;
    }
    SetBool(actor, "/Script/FortniteGame.BuildingActor.bPlayerPlaced", true);
    // [F03] SetMirrored lives on BuildingSMActor in 21.30.
    Call mirror("/Script/FortniteGame.BuildingSMActor.SetMirrored");
    if (mirror.ok()) { bool m = mirrored; if (mirror.set("bIsMirrored", m)) mirror.run(actor); }
    if (level > 0) {
        void* lvlProp = GetProp("/Script/FortniteGame.BuildingSMActor.CurrentBuildingLevel");
        if (lvlProp && Read<int>(lvlProp, profile::PropElementSize) == 4)
            Write<int32>(GetField(actor, lvlProp), 0, level);
        Call lr("/Script/FortniteGame.BuildingSMActor.OnRep_CurrentBuildingLevel");
        if (lr.ok()) lr.run(actor);
    }
    // [F30] Team bookkeeping (Erbium: PlayerState.TeamIndex → Team/TeamIndex).
    void* ps = GetObjField(session.controller, "/Script/Engine.Controller.PlayerState");
    if (ps && !IsBadPtr(ps)) {
        void* tiProp = GetProp("/Script/FortniteGame.FortPlayerStateAthena.TeamIndex");
        if (!tiProp) tiProp = GetProp("/Script/FortniteGame.FortPlayerState.TeamIndex");
        void* teamProp = GetProp("/Script/FortniteGame.BuildingSMActor.Team");
        void* ti2Prop = GetProp("/Script/FortniteGame.BuildingSMActor.TeamIndex");
        if (tiProp && teamProp && Read<int>(tiProp, profile::PropElementSize) == 1) {
            uint8_t team = Read<uint8_t>(GetField(ps, tiProp));
            Write<uint8_t>(GetField(actor, teamProp), 0, team);
            if (ti2Prop && Read<int>(ti2Prop, profile::PropElementSize) == 1)
                Write<uint8_t>(GetField(actor, ti2Prop), 0, team);
        }
    }
    Call net("/Script/Engine.Actor.ForceNetUpdate");
    if (net.ok()) net.run(actor);
    return true;
}

// [F21/F29/F30] Build from the controller's local preview state. The verified
// controller offsets (0x19B0/0x19C8/0x1898) are the primary source — the old
// reflection-only read was why building never fired on this build.
bool Athena::LocalBuild() {
    if (!session.controller || !session.world) return false;
    Vector loc{};
    Rotator rot{};
    loc = Read<Vector>(session.controller, profile::Controller_PreviewSnapLoc);
    rot = Read<Rotator>(session.controller, profile::Controller_PreviewSnapRot);
    if (!std::isfinite(loc.X) || !std::isfinite(loc.Y) || !std::isfinite(loc.Z))
        GetFieldValue<Vector>(session.controller, "/Script/FortniteGame.FortPlayerController.LastBuildPreviewGridSnapLoc", loc);
    if (!std::isfinite(rot.Pitch) || !std::isfinite(rot.Yaw) || !std::isfinite(rot.Roll))
        GetFieldValue<Rotator>(session.controller, "/Script/FortniteGame.FortPlayerController.LastBuildPreviewGridSnapRot", rot);
    void* klass = nullptr;
    {
        void* classProp = GetProp("/Script/FortniteGame.FortPlayerController.CurrentBuildableClass");
        if (classProp) klass = Read<void*>(GetField(session.controller, classProp));
        if (!klass || IsBadPtr(klass)) klass = Read<void*>(session.controller, profile::Controller_CurrentBuildableClass);
    }
    void* smCls = FindObj("/Script/FortniteGame.BuildingSMActor");
    if (!klass || IsBadPtr(klass) || !smCls || !SubclassOfFast(klass, smCls) || !NearPlayer(loc, 4000.0f) ||
        !std::isfinite(rot.Pitch) || !std::isfinite(rot.Yaw) || !std::isfinite(rot.Roll)) {
        LOG("[Build] invalid selected class or preview transform");
        return true;
    }
    // Mirroring from the controller state (bBuildPreviewMirrored @0x1859).
    bool mirrored = Read<uint8_t>(session.controller, profile::Controller_BuildPreviewMirrored) != 0;

    // [F29] Dedup: identical class+pose within 0.25s (touch repeat) plus the
    // shared guard against the ServerCreateBuildingActor RPC path.
    double pose[6] = {loc.X, loc.Y, loc.Z, rot.Pitch, rot.Yaw, rot.Roll};
    double now = Monotonic();
    if (now - session.rpc_build_time < 0.6) return true;
    if (klass == session.last_build_class && now - session.last_build_time < 0.25) {
        bool same = true;
        for (int i = 0; i < 6; ++i) if (session.last_build_pose[(size_t)i] != pose[i]) { same = false; break; }
        if (same) return true;
    }
    LOG("[Build] preview %s at %.0f %.0f %.0f", ObjName(klass).c_str(), loc.X, loc.Y, loc.Z);
    if (SpawnBuildingActor(klass, loc, rot, mirrored, 0)) {
        session.last_build_class = klass;
        session.last_build_time = now;
        for (int i = 0; i < 6; ++i) session.last_build_pose[(size_t)i] = pose[i];
    }
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

// [F07/F30] ServerCreateBuildingActor parse — FCreateBuildingActorData (0x50):
//   BuildingClassHandle u32 @0x00, BuildLoc FVector @0x08, BuildRot FRotator
//   @0x20, bMirrored @0x38, SyncKey f32 @0x3C, FBuildingClassData @0x40
//   (BuildingClass UClass* @0x40, PreviousBuildingLevel i32 @0x48,
//   UpgradeLevel i32 @0x4C).
// The v20+ wire format carries ONLY BuildingClassHandle from the client —
// the class is resolved locally from CurrentBuildableClass (exactly the piece
// the preview is showing; Erbium BuildingClassMap parity).
void Athena::OnServerCreateBuildingActor(void* params) {
    if (!params) return;
    void* fn = FindObj("/Script/FortniteGame.FortPlayerController.ServerCreateBuildingActor");
    uint16_t total = fn ? Read<uint16_t>(fn, profile::FunctionParamSize) : 0;
    if (total == 0x50) {
        Vector loc = Read<Vector>(params, 0x08);
        Rotator rot = Read<Rotator>(params, 0x20);
        bool mirrored = Read<uint8_t>(params, 0x38) != 0;
        int32_t upgradeLevel = Read<int32>(params, 0x4C);
        void* cls = Read<void*>(params, 0x40);
        if (!cls || IsBadPtr(cls)) {
            void* cp = GetProp("/Script/FortniteGame.FortPlayerController.CurrentBuildableClass");
            if (cp) cls = Read<void*>(GetField(session.controller, cp));
            if (!cls || IsBadPtr(cls)) cls = Read<void*>(session.controller, profile::Controller_CurrentBuildableClass);
        }
        if (!cls || IsBadPtr(cls)) { LOG("[Build] RPC class unresolved — preview fallback"); LocalBuild(); return; }
        if (!session.world || !NearPlayer(loc, 4000.0f)) { LOG("[Build] RPC rejected (far/no world)"); return; }
        if (SpawnBuildingActor(cls, loc, rot, mirrored, upgradeLevel)) {
            double now = Monotonic();
            session.last_build_class = cls;
            session.last_build_time = now;
            session.rpc_build_time = now;   // [F29] guard the touch-poll path
            LOG("[Build] ✅ RPC placement %s lvl=%d mirrored=%d", ObjName(cls).c_str(), upgradeLevel, (int)mirrored);
        }
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
    LOG("[Edit] editing tool bound to %s", ObjName(piece).c_str());
}

// [F08/F10/F30] Edit replacement at the OLD actor transform (RotationIterations
// × 90° yaw), keeping the building level — the old code spawned the new piece
// at the world origin.
void Athena::OnFinishEditActor(void* actor, void* newClass, int rotIter, bool mirrored) {
    if (!actor) return;
    Vector loc{};
    Rotator rot{};
    if (!ActorLocation(actor, loc)) { LOG("[Edit] no location on edited piece"); return; }
    {
        Call c("/Script/Engine.Actor.K2_GetActorRotation");
        if (c.ok() && c.run(actor)) rot = c.result<Rotator>();
    }
    int oldLevel = 0;
    {
        void* lvlProp = GetProp("/Script/FortniteGame.BuildingSMActor.CurrentBuildingLevel");
        if (lvlProp && Read<int>(lvlProp, profile::PropElementSize) == 4)
            oldLevel = Read<int>(GetField(actor, lvlProp));
    }
    if (!std::isfinite(rot.Yaw)) rot.Yaw = 0.0;
    rot.Yaw += double(rotIter) * 90.0;
    SetBool(actor, "/Script/FortniteGame.BuildingActor.bPlayDestructionEffects", false);
    if (!newClass || IsBadPtr(newClass)) return;
    if (!SpawnBuildingActor(newClass, loc, rot, mirrored != 0, oldLevel)) return;
    // Erbium parity: the replaced piece is removed via SilentDie (no FX).
    Call sd("/Script/FortniteGame.BuildingSMActor.SilentDie");
    if (sd.ok() && sd.run(actor)) { /* replaced quietly */ }
    else { Call d("/Script/Engine.Actor.K2_DestroyActor"); if (d.ok()) d.run(actor); }
    LOG("[Edit] replaced with %s (level=%d iter=%d mirrored=%d)", ObjName(newClass).c_str(), oldLevel, rotIter, (int)mirrored);
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
// [F39] HARVEST — Erbium ABuildingSMActor::OnDamageServer 1:1. The pickaxe
// damage on trees/rocks/builds grants the mapped resource directly into the
// inventory (stack-clamped, overflow re-tossed) and reports it to the HUD.
// The original damage still applies: pe_hook falls through to the native.
// ============================================================================
void Athena::HandleDamageServer(void* actor, void* func, void* params) {
    if (!actor || !func || !params || !session.controller || !session.inventory) return;
    if (!IsReadable(actor, 0x60) || !IsReadable(params, 0x20)) return;
    static void* smCls = nullptr;
    if (!smCls) smCls = FindObj("/Script/FortniteGame.BuildingSMActor");
    if (!smCls) return;
    void* cls = Read<void*>(actor, profile::ObjectClass);
    if (!cls || !SubclassOfFast(cls, smCls)) return;

    void* instSlot = incoming(func, params, "InstigatedBy", 8);
    void* causeSlot = incoming(func, params, "DamageCauser", 8);
    void* dmgSlot  = incoming(func, params, "Damage", 4);
    if (!instSlot || !causeSlot || !dmgSlot) return;
    void* inst = Read<void*>(instSlot);
    void* causer = Read<void*>(causeSlot);
    float damage = Read<float>(dmgSlot);
    if (!inst || !causer) return;

    // Erbium gates: bPlayerPlaced pieces and dropped-resource gating.
    if (GetBool(actor, "/Script/FortniteGame.BuildingActor.bPlayerPlaced")) return;
    {
        // Erbium: HasbAllowResourceDrop() && !bAllowResourceDrop → no harvest.
        void* ardProp = GetProp("/Script/FortniteGame.BuildingSMActor.bAllowResourceDrop");
        if (ardProp && Read<int>(ardProp, profile::PropElementSize) == 1) {
            void* f = GetField(actor, ardProp);
            if (f && !Read<uint8_t>(f)) return;
        }
    }

    // Erbium: only the pickaxe (melee WeaponData) harvests.
    static void* weaponCls = nullptr, *meleeCls = nullptr;
    if (!weaponCls) weaponCls = FindObj("/Script/FortniteGame.FortWeapon");
    if (!meleeCls) meleeCls = FindObj("/Script/FortniteGame.FortWeaponMeleeItemDefinition");
    if (!weaponCls || !meleeCls) return;
    if (!IsA(causer, weaponCls)) return;
    void* weaponDef = GetObjField(causer, "/Script/FortniteGame.FortWeapon.WeaponData");
    if (!weaponDef || IsBadPtr(weaponDef) || !IsA(weaponDef, meleeCls)) return;

    // Resource definition for the actor's ResourceType (Kismet helper).
    uint8_t resType = 0;
    {
        void* rtProp = GetProp("/Script/FortniteGame.BuildingSMActor.ResourceType");
        if (rtProp && Read<int>(rtProp, profile::PropElementSize) == 1)
            resType = Read<uint8_t>(GetField(actor, rtProp));
    }
    void* resource = nullptr;
    {
        Call c("/Script/FortniteGame.FortKismetLibrary.K2_GetResourceItemDefinition");
        if (c.ok()) {
            uint8_t t = resType;
            if (c.set("Type", t) || c.set_at(0, t)) {
                if (c.run(FindObj("/Script/FortniteGame.Default__FortKismetLibrary")))
                    resource = c.result<void*>();
            }
        }
    }
    if (!resource || IsBadPtr(resource)) return;
    int maxMat = MaxStackSizeOf(resource);
    if (maxMat <= 0) maxMat = 999;

    // Erbium curve: BuildingResourceAmountOverride row → Out / (MaxHealth / Damage).
    int resCount = 0;
    float maxHealth = 0.0f;
    {
        void* mhProp = GetProp("/Script/FortniteGame.BuildingActor.MaxHealth");
        if (!mhProp) mhProp = GetProp("/Script/FortniteGame.BuildingSMActor.MaxHealth");
        if (mhProp && Read<int>(mhProp, profile::PropElementSize) == 4) {
            float v = Read<float>(GetField(actor, mhProp));
            if (v > 1.0f && v < 100000.0f) maxHealth = v;
        }
    }
    {
        void* broProp = GetProp("/Script/FortniteGame.BuildingSMActor.BuildingResourceAmountOverride");
        if (broProp) {
            void* handle = GetField(actor, broProp);
            void* curveTable = handle ? Read<void*>(handle, 0) : nullptr;
            FName row = handle ? Read<FName>(handle, 8) : FName{};
            if (!curveTable || IsBadPtr(curveTable) || !row.CompIdx) {
                // Class-level override lives on the row table default — try the
                // shared AthenaResourceRates table with the class's row.
                curveTable = FindObj("/Game/Athena/Balance/DataTables/AthenaResourceRates.AthenaResourceRates");
                if (!curveTable) curveTable = LoadAsset("/Game/Athena/Balance/DataTables/AthenaResourceRates.AthenaResourceRates", "/Script/Engine.DataTable");
            }
            if (curveTable && !IsBadPtr(curveTable) && row.CompIdx) {
                Call ev("/Script/Engine.DataTableFunctionLibrary.EvaluateCurveTableRow");
                if (ev.ok()) {
                    float t0 = 0.0f;
                    uint8_t zeroRow[64]{};
                    bool bound = ev.set("CurveTable", curveTable) && ev.set("RowName", row) && ev.set("InTime", t0);
                    bound |= ev.set_bytes("OutRow", zeroRow, 8);
                    if (bound && ev.run(FindObj("/Script/Engine.Default__DataTableFunctionLibrary"))) {
                        void* slot = ev.param_slot("OutValue");
                        float out = slot ? Read<float>(slot) : 0.0f;
                        if (out > 0.0f && damage > 0.0f) {
                            float denom = maxHealth > 1.0f ? maxHealth : 100.0f;
                            float rc = out / (denom / damage);
                            resCount = (int)std::lround(rc);
                        }
                    }
                }
            }
        }
        if (resCount <= 0 && damage > 0.0f) {
            // No curve row resolved — proportional fallback: 50 resources per
            // full break of the target (AthenaResourceRates ballpark).
            float denom = maxHealth > 1.0f ? maxHealth : 100.0f;
            resCount = (int)std::lround(50.0f * damage / denom);
        }
        if (resCount < 0) resCount = 0;
        if (resCount > 100) resCount = 100;   // single-hit sanity cap
    }
    if (resCount <= 0) return;

    // Erbium give: merge into the existing stack, overflow re-tossed.
    void* invProp   = GetProp("/Script/FortniteGame.FortInventory.Inventory");
    void* instProp  = GetProp("/Script/FortniteGame.FortItemList.ItemInstances");
    void* entryProp = GetProp("/Script/FortniteGame.FortItemList.ReplicatedEntries");
    void* itemEntryProp = GetProp("/Script/FortniteGame.FortWorldItem.ItemEntry");
    void* countProp = GetProp("/Script/FortniteGame.FortItemEntry.Count");
    void* defProp   = GetProp("/Script/FortniteGame.FortItemEntry.ItemDefinition");
    if (invProp && instProp && entryProp && itemEntryProp && countProp && defProp) {
        void* list = GetField(session.inventory, invProp);
        Array instances = Read<Array>(GetField(list, instProp));
        Array replicated = Read<Array>(GetField(list, entryProp));
        void* foundInst = nullptr;
        for (int i = 0; i < instances.Count; ++i) {
            void* cand = Read<void*>(instances.Data, (size_t)i * 8);
            if (cand && !IsBadPtr(cand) && ItemDefinitionOf(cand) == resource) { foundInst = cand; break; }
        }
        if (foundInst) {
            int stride = Read<int>(entryProp, profile::PropElementSize);
            for (int j = 0; j < replicated.Count && replicated.Data && stride > 0; ++j) {
                void* ent = (char*)replicated.Data + (size_t)j * stride;
                if (Read<void*>(GetField(ent, defProp)) != resource) continue;
                int updated = Read<int>(GetField(ent, countProp)) + resCount;
                if (updated > maxMat) {
                    Vector loc{};
                    if (session.pawn) ActorLocation(session.pawn, loc);
                    SpawnPickup(resource, updated - maxMat, loc, 16 /*Tossed*/, 0, true);
                    updated = maxMat;
                }
                Write<int>(GetField(ent, countProp), 0, updated);
                Write<int>(GetField(GetField(foundInst, itemEntryProp), countProp), 0, updated);
                InventoryUpdate();
                LOG("[Harvest] ✅ +%d %s (total %d)", resCount, ObjName(resource).c_str(), updated);
                break;
            }
        } else {
            int grant = resCount;
            if (grant > maxMat) {
                Vector loc{};
                if (session.pawn) ActorLocation(session.pawn, loc);
                SpawnPickup(resource, grant - maxMat, loc, 16 /*Tossed*/, 0, true);
                grant = maxMat;
            }
            if (GiveItem(resource, grant, -1, (uint8_t)-1, false))
                LOG("[Harvest] ✅ new stack +%d %s", grant, ObjName(resource).c_str());
        }
    }

    // HUD feedback (Erbium ClientReportDamagedResourceBuilding).
    {
        Call rep("/Script/FortniteGame.FortPlayerController.ClientReportDamagedResourceBuilding");
        if (rep.ok() && session.controller) {
            bool bound = rep.set_any((uint8_t)resType, "ResourceType", "Type");
            int rc = resCount; bound |= rep.set("ResCount", rc) || rep.set("Count", rc) || rep.set("ResourceCount", rc);
            bool destroyed = false; bound |= rep.set("bDestroyed", destroyed) || rep.set("Destroyed", destroyed);
            bool weak = damage >= 100.0f; bound |= rep.set("bWeakspot", weak) || rep.set("bWeakSpot", weak) || rep.set("bWeakSpotAttack", weak);
            if (bound) rep.run(session.controller);
        }
    }
    { Call n("/Script/Engine.Actor.ForceNetUpdate"); if (n.ok()) n.run(actor); }
}

// ============================================================================
// [F40] MATCH STATE — Erbium GameMode/GameState parity for the HUD:
// AlivePlayers.Add(PC) + PlayersLeft=1 + OnRep_PlayersLeft so the alive-
// players counter actually shows a number instead of staying empty.
// ============================================================================
void Athena::SetupMatchState() {
    if (session.match_state_ready || !session.world || !session.controller) return;
    // [F88] re-resolve mode/state at call time + commit ready ONLY when both
    // blocks actually ran. The old code set match_state_ready=true BEFORE the
    // blocks: if GameMode/GameState were not replicated yet on the first call
    // (end of FinalizePlayer), the flag committed and the alive-players HUD
    // stayed dead forever (same one-shot-commit family as F85).
    void* msModeProp  = GetProp("/Script/Engine.World.AuthorityGameMode");
    void* msStateProp = GetProp("/Script/Engine.World.GameState");
    void* msMode  = msModeProp  ? Read<void*>(GetField(session.world, msModeProp))  : nullptr;
    void* msState = msStateProp ? Read<void*>(GetField(session.world, msStateProp)) : nullptr;
    if (msMode && !IsBadPtr(msMode))   session.mode  = msMode;
    if (msState && !IsBadPtr(msState)) session.state = msState;
    bool aliveDone = false, leftDone = false;
    if (session.mode && !IsBadPtr(session.mode)) {
        void* apProp = GetProp("/Script/FortniteGame.FortGameMode.AlivePlayers");
        if (!apProp) apProp = GetProp("/Script/FortniteGame.FortGameModeAthena.AlivePlayers");
        if (apProp) {
            void* arrField = GetField(session.mode, apProp);
            Array cur = Read<Array>(arrField);
            bool known = false;
            for (int i = 0; i < cur.Count && cur.Data; ++i)
                if (Read<void*>(cur.Data, (size_t)i * 8) == session.controller) known = true;
            if (!known && cur.Count >= 0 && cur.Count < 256)
                Fn<int(*)(void*, void*, const void*)>(profile::GenericArrayAdd)(arrField, apProp, &session.controller);
            LOG("[MatchState] AlivePlayers count=%d", Read<Array>(arrField).Count);
        }
        aliveDone = true;
    }
    if (session.state && !IsBadPtr(session.state)) {
        void* plProp = GetProp("/Script/FortniteGame.FortGameStateAthena.PlayersLeft");
        if (plProp && Read<int>(plProp, profile::PropElementSize) == 4) {
            int cur = Read<int>(GetField(session.state, plProp));
            if (cur < 1) {
                Write<int>(GetField(session.state, plProp), 0, 1);
                Call n("/Script/FortniteGame.FortGameStateAthena.OnRep_PlayersLeft");
                if (n.ok()) n.run(session.state);
                LOG("[MatchState] PlayersLeft=1 + OnRep (was %d)", cur);
            }
        }
        leftDone = true;
    }
    // [F88] commit only on full coverage; the Tick retry (5s) re-invokes us
    // until mode/state exist, then the flag freezes the pass.
    session.match_state_ready = aliveDone && leftDone;
    if (session.match_state_ready) LOG("[MatchState] ✅ ready (alive + playersleft)");
}

// [F42] Diagnostics dump — quickbar slots, inventory entries, match state.
void Athena::DebugDump() {
    LOG("=== era.debug ===");
    LOG("world=%p pc=%p pawn=%p inv=%p qb=%p pendingPickups=%zu",
        session.world, session.controller, session.pawn, session.inventory,
        session.quickbars, session.pending_pickups.size());
    for (int i = 0; i < 6; ++i)
        LOG("SLOT %d: %s", i, ObjName(ItemDefinitionOf(session.slots[(size_t)i])).c_str());
    for (int i = 0; i < 6; ++i)
        LOG("BUILD %d: %s", i, ObjName(ItemDefinitionOf(session.secondary_slots[(size_t)i])).c_str());
    if (session.quickbars && !IsBadPtr(session.quickbars)) {
        void* barProp = GetProp("/Script/FortniteGame.FortQuickBars.PrimaryQuickBar");
        void* slotsProp = GetProp("/Script/FortniteGame.QuickBar.Slots");
        if (barProp && slotsProp) {
            void* barField = GetField(session.quickbars, barProp);
            Array slots = Read<Array>(GetField(barField, slotsProp));
            LOG("PrimaryQuickBar slots=%d", slots.Count);
            for (int i = 0; i < slots.Count && slots.Data && i < 6; ++i) {
                void* slot = (char*)slots.Data + (size_t)i * profile::QuickBarSlot_SlotSize;
                Array items = Read<Array>(slot);
                uint8_t flags = Read<uint8_t>(slot, profile::QuickBarSlot_EnabledOffset);
                LOG("  slot %d items=%d flags=0x%02X", i, items.Count, flags);
            }
        }
    }
    if (session.state && !IsBadPtr(session.state)) {
        void* plProp = GetProp("/Script/FortniteGame.FortGameStateAthena.PlayersLeft");
        if (plProp) LOG("PlayersLeft=%d", Read<int>(GetField(session.state, plProp)));
    }
    LOG("=== end dump ===");
}

// ============================================================================
// VEHICLES — [F28] Erbium AFortAthenaVehicleSpawner path (1:1) + seat weapons
// ============================================================================
// [F43] Erbium's LivingWorld vehicle tag map (1:1): AFortAthenaLivingWorld-
// StaticPointProvider spawners carry GameplayTags that select the vehicle
// class. On real s20–s22 servers the AFortAthenaVehicleSpawner path is native-
// auto, so this provider pass is what actually fills the map with cars.
// Erbium 1:1 — Chapter 3 S21 vehicle tag map.
// المطابقة حرفية (tag == needle) وليس find().
static const struct { const char* tag; const char* classPath; const char* shortName; } kVehicleTagRules[] = {
    // --- Upgraded Valet (الفئات الفعلية على خريطة S21) ---
    {"Athena.Vehicle.SpawnLocation.Valet.BasicTruck.Upgraded",
     "/Valet/BasicTruck/Valet_BasicTruck_Vehicle_Upgrade.Valet_BasicTruck_Vehicle_Upgrade_C",
     "Valet_BasicTruck_Vehicle_Upgrade_C"},
    {"Athena.Vehicle.SpawnLocation.Valet.BigRig.Upgraded",
     "/Valet/BigRig/Valet_BigRig_Vehicle_Upgrade.Valet_BigRig_Vehicle_Upgrade_C",
     "Valet_BigRig_Vehicle_Upgrade_C"},
    {"Athena.Vehicle.SpawnLocation.Valet.SportsCar.Upgraded",
     "/Valet/SportsCar/Valet_SportsCar_Vehicle_Upgrade.Valet_SportsCar_Vehicle_Upgrade_C",
     "Valet_SportsCar_Vehicle_Upgrade_C"},
    {"Athena.Vehicle.SpawnLocation.Valet.BasicCar.Upgraded",
     "/Valet/BasicCar/Valet_BasicCar_Vehicle_Upgrade.Valet_BasicCar_Vehicle_Upgrade_C",
     "Valet_BasicCar_Vehicle_Upgrade_C"},

    // --- Variants الخاصة (بدون Upgraded) ---
    {"Athena.Vehicle.SpawnLocation.Valet.BasicCar.Taxi",
     "/Valet/TaxiCab/Valet_TaxiCab_Vehicle.Valet_TaxiCab_Vehicle_C",
     "Valet_TaxiCab_Vehicle_C"},
    {"Athena.Vehicle.SpawnLocation.Valet.BasicCar.Modded",
     "/ModdedBasicCar/Vehicle/Valet_BasicCar_Vehicle_SuperSedan.Valet_BasicCar_Vehicle_SuperSedan_C",
     "Valet_BasicCar_Vehicle_SuperSedan_C"},

    // --- Motorcycles ---
    {"Athena.Vehicle.SpawnLocation.Motorcycle.Dirtbike",
     "/Dirtbike/Vehicle/Motorcycle_DirtBike_Vehicle.Motorcycle_DirtBike_Vehicle_C",
     "Motorcycle_DirtBike_Vehicle_C"},
    {"Athena.Vehicle.SpawnLocation.Motorcycle.Sportbike",
     "/Sportbike/Vehicle/Motorcycle_Sport_Vehicle.Motorcycle_Sport_Vehicle_C",
     "Motorcycle_Sport_Vehicle_C"},

    // --- Base variants (احتياطي — قد تظهر في بعض الخرائط/الأوضاع) ---
    {"Athena.Vehicle.SpawnLocation.Valet.BasicCar",
     "/Valet/BasicCar/Valet_BasicCar_Vehicle_Upgrade.Valet_BasicCar_Vehicle_Upgrade_C",
     "Valet_BasicCar_Vehicle_Upgrade_C"},
    {"Athena.Vehicle.SpawnLocation.Valet.SportsCar",
     "/Valet/SportsCar/Valet_SportsCar_Vehicle_Upgrade.Valet_SportsCar_Vehicle_Upgrade_C",
     "Valet_SportsCar_Vehicle_Upgrade_C"},
    {"Athena.Vehicle.SpawnLocation.Valet.BasicTruck",
     "/Valet/BasicTruck/Valet_BasicTruck_Vehicle_Upgrade.Valet_BasicTruck_Vehicle_Upgrade_C",
     "Valet_BasicTruck_Vehicle_Upgrade_C"},
    {"Athena.Vehicle.SpawnLocation.Valet.BigRig",
     "/Valet/BigRig/Valet_BigRig_Vehicle_Upgrade.Valet_BigRig_Vehicle_Upgrade_C",
     "Valet_BigRig_Vehicle_Upgrade_C"},

    {nullptr, nullptr, nullptr}
};

static bool SpawnVehicleAt(void* cls, const Vector& loc, const Rotator& rot) {
    if (!cls || IsBadPtr(cls)) return false;
    Transform t{};
    t.Location[0] = loc.X; t.Location[1] = loc.Y; t.Location[2] = loc.Z + 5.0;
    RotatorToQuat(rot, t.Rotation);
    void* vehicle = SpawnActor(session.world, cls, nullptr, t);
    if (!vehicle) return false;

    // الوقود الكامل
    {
        std::string clsName = ObjName(Read<void*>(vehicle, profile::ObjectClass));
        Call f((std::string("/Script/FortniteGame.") + clsName + ".SetFuel").c_str());
        if (!f.ok()) f = Call("/Script/FortniteGame.FortDagwoodVehicle.SetFuel");
        if (!f.ok()) f = Call("/Script/FortniteGame.FortAthenaVehicle.SetFuel");
        if (f.ok() && f.set_at(0, 100.0f)) f.run(vehicle);
        void* fuelProp = GetProp("/Script/FortniteGame.FortAthenaVehicle.Fuel");
        if (fuelProp && Read<int>(fuelProp, profile::PropElementSize) == 4)
            Write<float>(GetField(vehicle, fuelProp), 0, 100.0f);
    }

    // تفعيل السيارة — كتابة خصائص مباشرة (UFUNCTIONs غير موجودة في 21.30)
    {
    // عمليات آمنة فقط + تشخيص للقراءة (لا كتابة على خصائص مجهولة)
    {
        // Tick + collision (آمنتان تماماً)
        Call tick("/Script/Engine.Actor.SetActorTickEnabled");
        if (tick.ok()) { bool b = true; tick.set("bEnabled", b); tick.run(vehicle); }
        Call coll("/Script/Engine.Actor.SetActorEnableCollision");
        if (coll.ok()) { bool b = true; coll.set("bNewActorEnableCollision", b); coll.run(vehicle); }

        // تشخيص: اقرأ فقط، لا تكتب (آمن)
        static const char* const diagProps[] = {
            "/Script/FortniteGame.FortAthenaVehicle.bCanBeUsed",
            "/Script/FortniteGame.FortAthenaVehicle.bIsReadyForPlayers",
            "/Script/FortniteGame.FortAthenaVehicle.bPlayersCanEnter",
            "/Script/FortniteGame.FortAthenaVehicle.bIsVehicleDestroyed",
            "/Script/FortniteGame.FortAthenaVehicle.bIsVehicleSetup",
            nullptr
        };
        for (int i = 0; diagProps[i]; ++i) {
            if (GetProp(diagProps[i]))
                LOG("[Vehicles] diag %s=%d", diagProps[i], (int)GetBool(vehicle, diagProps[i]));
        }

        // سجّل سعة المقاعد
        Call get("/Script/Engine.Actor.GetComponentByClass");
        void* seatCls = FindObj("/Script/FortniteGame.FortVehicleSeatComponent");
        if (seatCls && get.ok() && get.set("ComponentClass", seatCls) && get.run(vehicle)) {
            void* sc = get.result<void*>();
            if (sc && !IsBadPtr(sc)) {
                void* psProp = GetProp("/Script/FortniteGame.FortVehicleSeatComponent.PlayerSlots");
                if (psProp) {
                    Array slots = Read<Array>(GetField(sc, psProp));
                    LOG("[Vehicles] seat slots=%d", slots.Count);
                }
            }
        }
    }

    return true;
}
    return true;
}
void Athena::SpawnVehicles() {
    if (!session.world || session.vehicles_spawned) return;
    if (!WorldIsStandalone(session.world)) return;

    ++session.vehicle_attempts;
    static std::vector<Vector> spawnedAt;
    int spawned = 0;
    int viable = 0;

    // ---- Pass 1: LivingWorld providers ----
    {
        void* providerCls = FindObj("/Script/FortniteGame.FortAthenaLivingWorldStaticPointProvider");
        if (providerCls) {
            std::vector<void*> providers;
            if (GetAllActorsOfClassRaw(providerCls, providers) && !providers.empty()) {
                LOG("[Vehicles] LivingWorld providers: %zu", providers.size());
                void* tagsProp    = GetProp("/Script/FortniteGame.FortAthenaLivingWorldStaticPointProvider.FiltersTags");
                void* tagsArrProp = tagsProp ? GetProp("/Script/GameplayTags.GameplayTagContainer.GameplayTags") : nullptr;
                if (!tagsArrProp) tagsArrProp = GetProp("/Script/FortniteGame.GameplayTagContainer.GameplayTags");

                for (void* sp : providers) {
                    if (!sp || IsBadPtr(sp)) continue;
                    std::vector<std::string> tags;
                    if (tagsProp && tagsArrProp) {
                        void* container = GetField(sp, tagsProp);
                        if (container) {
                            Array arr = Read<Array>(GetField(container, tagsArrProp));
                            for (int i = 0; i < arr.Count && i < 16 && arr.Data; ++i)
                                tags.push_back(FNameToString((char*)arr.Data + (size_t)i * 8));
                        }
                    }
                    if (tags.empty()) continue;

                    Vector loc{}; Rotator rot{};
                    if (!ActorLocation(sp, loc)) continue;
                    { Call c("/Script/Engine.Actor.K2_GetActorRotation"); if (c.ok() && c.run(sp)) rot = c.result<Rotator>(); }

                    bool dup = false;
                    for (auto& s : spawnedAt) {
                        double dx = s.X - loc.X, dy = s.Y - loc.Y;
                        if (dx*dx + dy*dy < 600.0 * 600.0) { dup = true; break; }
                    }
                    if (dup) continue;

                    for (int r = 0; kVehicleTagRules[r].tag; ++r) {
                        bool hit = false;
                        for (auto& tg : tags) if (tg == kVehicleTagRules[r].tag) { hit = true; break; }
                        if (!hit) continue;

                        void* cls = LoadClass(kVehicleTagRules[r].classPath, "/Script/Engine.Actor");
                        if (!cls) cls = ByShortName(kVehicleTagRules[r].shortName);
                        if (!cls || IsBadPtr(cls)) {
                            LOG("[Vehicles] ⚠️ class missing for tag '%s' (%s)", tags[0].c_str(), kVehicleTagRules[r].shortName);
                            break;
                        }

                        ++viable;
                        if (SpawnVehicleAt(cls, loc, rot)) {
                            ++spawned;
                            spawnedAt.push_back(loc);
                            LOG("[Vehicles] ✅ %s (tag %s) @ %.0f %.0f %.0f",
                                kVehicleTagRules[r].shortName, tags[0].c_str(), loc.X, loc.Y, loc.Z);
                        }
                        break;
                    }
                }
            }
        }
    }

    // ---- Pass 2: AFortAthenaVehicleSpawner ----
    {
        void* spawnerCls = FindObj("/Script/FortniteGame.FortAthenaVehicleSpawner");
        if (!spawnerCls) spawnerCls = LoadClass("/Script/FortniteGame.FortAthenaVehicleSpawner", "/Script/Engine.Actor");
        std::vector<void*> spawners;
        if (spawnerCls && GetAllActorsOfClassRaw(spawnerCls, spawners) && !spawners.empty()) {
            LOG("[Vehicles] spawner actors: %zu", spawners.size());
            for (void* spawner : spawners) {
                if (!spawner || IsBadPtr(spawner)) continue;

                void* vehicleClass = nullptr;
                { Call c("/Script/FortniteGame.FortAthenaVehicleSpawner.GetVehicleClass"); if (c.ok() && c.run(spawner)) vehicleClass = c.result<void*>(); }
                if (!vehicleClass || IsBadPtr(vehicleClass)) continue;
                ++viable;

                void* cachedDef = GetObjField(spawner, "/Script/FortniteGame.FortAthenaVehicleSpawner.CachedFortVehicleItemDef");
                bool forced = GetBool(spawner, "/Script/FortniteGame.FortAthenaVehicleSpawner.bForceSpawnAlways");
                if (cachedDef && !IsBadPtr(cachedDef) && !forced) {
                    float mn = 100.0f, mx = 100.0f;
                    void* minProp = GetProp("/Script/FortniteGame.FortVehicleItemDefinition.VehicleMinSpawnPercent");
                    void* maxProp = GetProp("/Script/FortniteGame.FortVehicleItemDefinition.VehicleMaxSpawnPercent");
                    if (minProp && Read<int>(minProp, profile::PropElementSize) >= 4) mn = Read<float>(GetField(cachedDef, minProp));
                    if (maxProp && Read<int>(maxProp, profile::PropElementSize) >= 4) mx = Read<float>(GetField(cachedDef, maxProp));
                    double lo = std::clamp((double)mn * 0.01, 0.0, 1.0);
                    double hi = std::clamp((double)mx * 0.01, 0.0, 1.0);
                    if (hi < lo) hi = lo;
                    double spawnPercent = lo + (hi - lo) * (rand() / (float)RAND_MAX);
                    if ((rand() / (float)RAND_MAX) > spawnPercent) continue;
                }

                Vector loc{}; Rotator rot{};
                if (!ActorLocation(spawner, loc)) continue;
                { Call c("/Script/Engine.Actor.K2_GetActorRotation"); if (c.ok() && c.run(spawner)) rot = c.result<Rotator>(); }

                bool dup = false;
                for (auto& s : spawnedAt) {
                    double dx = s.X - loc.X, dy = s.Y - loc.Y;
                    if (dx*dx + dy*dy < 600.0 * 600.0) { dup = true; break; }
                }
                if (dup) continue;

                if (SpawnVehicleAt(vehicleClass, loc, rot)) {
                    ++spawned;
                    spawnedAt.push_back(loc);
                    LOG("[Vehicles] ✅ %s @ %.0f %.0f %.0f", ObjName(vehicleClass).c_str(), loc.X, loc.Y, loc.Z);
                }
            }
        }
    }

    if (spawned > 0 && spawned >= viable) {
        session.vehicles_spawned = true;
        LOG("[Vehicles] ✅ complete: %d vehicles total (attempts=%d)", (int)spawnedAt.size(), session.vehicle_attempts);
    } else if (spawned > 0) {
        LOG("[Vehicles] partial: +%d this pass, %d total, viable=%d — retrying (attempts=%d)", spawned, (int)spawnedAt.size(), viable, session.vehicle_attempts);
    } else if (session.vehicle_attempts >= 12) {
        session.vehicles_spawned = true;
        LOG("[Vehicles] retry window exhausted: %d vehicles total", (int)spawnedAt.size());
    } else {
        LOG("[Vehicles] pass %d: no vehicles yet — will retry", session.vehicle_attempts);
    }
}
// [F45] 21.30 wall-climb / mantle assist — standalone has no mantle path, so
// a jump pressed against a wall relaunches the pawn up+forward (climb-over
// feel, Grapple-Glove-era mobility). Pure movement impulse — safe.
void Athena::TickMantleAssist() {
    if (!session.pawn) return;
    static double next = 0, lastBoost = 0;
    double now = Monotonic();
    if (now < next) return;
    next = now + 0.1;
    if (now - lastBoost < 0.8) return;
    void* movement = GetObjField(session.pawn, "/Script/Engine.Character.CharacterMovement");
    if (!movement || IsBadPtr(movement)) return;
    uint8_t mode = Read<uint8_t>(movement, profile::CharacterMovement_MovementMode);
    if (mode != 3) return;                       // MOVE_Falling only
    if (!GetBool(session.pawn, "/Script/Engine.Character.bPressedJump")) return;
    Vector vel{};
    void* velProp = GetProp("/Script/Engine.MovementComponent.Velocity");
    if (velProp && Read<int>(velProp, profile::PropElementSize) == 24)
        vel = Read<Vector>(GetField(movement, velProp));
    if (vel.Z >= 0.0) return;                    // must be falling into the wall
    double hSpeed = std::sqrt(vel.X * vel.X + vel.Y * vel.Y);
    if (hSpeed > 480.0) return;                  // not pushing into it
    Vector fwd{};
    { Call f("/Script/Engine.Actor.GetActorForwardVector"); if (f.ok() && f.run(session.pawn)) fwd = f.result<Vector>(); }
    Vector launch{ fwd.X * 180.0, fwd.Y * 180.0, 640.0 };
    Call l("/Script/Engine.Character.LaunchCharacter");
    if (l.ok() && l.set("LaunchVelocity", launch) && l.run(session.pawn)) {
        lastBoost = now;
        static bool logged = false;
        if (!logged) { logged = true; LOG("[Climb] ✅ mantle assist live (jump against wall = climb)"); }
    }
}

// [F44] Reality Tree proximity (21.30) — discover the map's reality actors
// once through GObjects, then re-trigger their proximity blueprint events
// when the player walks close. Best-effort: the visual/audio systems are
// cooked inside the tree's BP; without a server they just need a nudge.
static FName StringToFName(const char* str) {
    if (!str || !*str) return FName{};
    auto w = ToWide(str);
    FStringNative fs{(TCHAR*)w.data(), (int32)w.size() + 1, (int32)w.size() + 1};
    Call c("/Script/Engine.KismetSystemLibrary.Conv_StringToName");
    if (!c.ok()) c = Call("/Script/Engine.KismetStringLibrary.Conv_StringToName");
    if (!c.ok()) return FName{};
    if (!c.set("InString", fs)) return FName{};
    void* cdo = FindObj("/Script/Engine.Default__KismetSystemLibrary");
    if (!cdo) cdo = FindObj("/Script/Engine.Default__KismetStringLibrary");
    if (!cdo || !c.run(cdo)) return FName{};
    return c.result<FName>();
}

static bool TryActivateHinted(void* asc, const char* hint) {
    if (!asc || IsBadPtr(asc) || !hint) return false;

    void* actProp = GetProp("/Script/GameplayAbilities.AbilitySystemComponent.ActivatableAbilities");
    if (!actProp) return false;
    void* container = GetField(asc, actProp);
    if (!container) return false;
    void* itemsProp = GetProp("/Script/GameplayAbilities.GameplayAbilitySpecContainer.Items");
    if (!itemsProp) itemsProp = GetProp("/Script/GameplayAbilities.GameplayAbilitySpecContainer.ActivatableAbilities");
    if (!itemsProp) return false;
    int stride = Read<int>(itemsProp, profile::PropElementSize);
    void* inner = Read<void*>(itemsProp, 0x70);
    void* specStruct = inner ? Read<void*>(inner, 0x70) : nullptr;
    if (!specStruct || stride <= 0) return false;
    int abilityOff = PropOffsetOn(specStruct, "Ability");
    if (abilityOff < 0) return false;

    Array arr = Read<Array>(GetField(container, itemsProp));
    if (arr.Count < 0 || arr.Count > 256 || !arr.Data) return false;

    for (int i = 0; i < arr.Count; ++i) {
        void* row = (char*)arr.Data + (size_t)i * stride;
        void* ability = Read<void*>(row, abilityOff);
        if (!ability || IsBadPtr(ability)) continue;
        void* abilityClass = Read<void*>(ability, profile::ObjectClass);
        if (!abilityClass) continue;
        std::string n = ObjName(abilityClass);
        if (n.find(hint) == std::string::npos) continue;

        Call c("/Script/GameplayAbilities.AbilitySystemComponent.TryActivateAbilityByClass");
        if (!c.ok()) continue;
        bool bAllow = true;
        if (c.set("InAbilityToActivate", abilityClass) && c.set("bAllowRemoteActivation", bAllow)) {
            if (c.run(asc)) { LOG("[Ability] ✅ activated %s (hint=%s)", n.c_str(), hint); return true; }
        }
    }
    static std::set<std::string> logged;
    if (logged.insert(hint).second)
        LOG("[Ability] ⚠️ hint '%s' not found in ASC", hint);
    return false;
}

static bool SendGameplayEventTag(void* asc, const char* tagStr) {
    if (!asc || !tagStr) return false;
    FName tag = StringToFName(tagStr);
    if (!tag.CompIdx) return false;

    Call c("/Script/GameplayAbilities.AbilitySystemComponent.HandleGameplayEvent");
    if (!c.ok()) c = Call("/Script/GameplayAbilities.AbilitySystemComponent.SendGameplayEvent");
    if (!c.ok()) return false;

    uint8_t eventData[256]{};
    if (c.set_bytes("EventTag", &tag, sizeof(FName)) ||
        c.set_bytes("GameplayEventTag", &tag, sizeof(FName))) {
        if (c.set_bytes("Payload", eventData, 8) || c.set_bytes("Data", eventData, 8)) {
            if (c.run(asc)) { LOG("[Ability] ✅ event %s", tagStr); return true; }
        }
    }
    return false;
}

void Athena::TickRealityProximity() {
    static std::vector<void*> classes;
    static std::vector<void*> actors;
    static bool discovered = false;
    static int failedSweeps = 0;
    static double nextScan = 0;
    static double lastApproach = 0;
    double now = Monotonic();

    // الاكتشاف مرة واحدة
    if (!discovered) {
        if (failedSweeps >= 2 || now < nextScan) return;
        nextScan = now + 8.0;
        GObjects_t* go = GObjectArray();
        void* classType = FindObj("/Script/CoreUObject.Class");
        if (!go || !classType) { ++failedSweeps; return; }
        classes.clear();
        for (int32 i = 0; i < go->NumElements && (int)classes.size() < 4; ++i) {
            void* object = go->GetById((uint32)i);
            if (!object || IsBadPtr(object)) continue;
            if (Read<void*>(object, profile::ObjectClass) != classType) continue;
            std::string n = ObjName(object);
            if (n.find("RealityTree") == std::string::npos &&
                n.find("RealitySapling") == std::string::npos) continue;
            classes.push_back(object);
            LOG("[Reality] class found: %s", n.c_str());
        }
        if (!classes.empty()) discovered = true;
        else ++failedSweeps;
        return;
    }

    // تحديث قائمة الـ actors
    if (now - nextScan > 3.0) {
        nextScan = now;
        actors.clear();
        for (void* cls : classes) {
            std::vector<void*> found;
            if (GetAllActorsOfClassRaw(cls, found))
                for (void* a : found) if (a && !IsBadPtr(a)) actors.push_back(a);
        }
    }
    if (actors.empty() || !session.pawn) return;

    Vector pawnLoc{};
    if (!ActorLocation(session.pawn, pawnLoc)) return;

    // ابحث عن أقرب شجرة ضمن 800 وحدة (8 متر — مدى معقول)
    void* nearest = nullptr;
    double nearestDist2 = 800.0 * 800.0;
    for (void* a : actors) {
        Vector loc{};
        if (!ActorLocation(a, loc)) continue;
        double dx = loc.X - pawnLoc.X, dy = loc.Y - pawnLoc.Y, dz = loc.Z - pawnLoc.Z;
        double d2 = dx*dx + dy*dy + dz*dz;
        if (d2 < nearestDist2) { nearestDist2 = d2; nearest = a; }
    }
    if (!nearest) return;
    if (now - lastApproach < 3.0) return;   // cooldown قصير — نسمح بمحاولات متكررة
    lastApproach = now;

    void* asc = GetASC(session.pawn);
    if (!asc) return;

    // 1. جرّب التفعيل المباشر
    bool ok = TryActivateHinted(asc, "Sapling");
    if (!ok) ok = TryActivateHinted(asc, "Reality");

    // 2. إذا فشل، جرّب GameplayEvent tags
    if (!ok) {
        static const char* const tagCandidates[] = {
            "GameplayEvent.RealitySapling.Enter",
            "GameplayEvent.RealitySapling",
            "GameplayEvent.Athena.RealitySapling.Enter",
            "GameplayEvent.Interact.RealitySapling",
            "RealitySapling.Enter",
            "Event.RealitySapling.Enter",
            nullptr
        };
        for (int i = 0; tagCandidates[i] && !ok; ++i)
            ok = SendGameplayEventTag(asc, tagCandidates[i]);
    }

    if (ok) {
        LOG("[Reality] ✅ triggered near %s (dist=%.0f)",
            ObjName(Read<void*>(nearest, profile::ObjectClass)).c_str(),
            std::sqrt(nearestDist2));
    } else {
        static bool once = false;
        if (!once) { once = true; LOG("[Reality] ⚠️ all activate paths failed — need correct event tag"); }
    }
}
// [F28] Seat weapons (Erbium EquipVehicleWeapon parity): while the pawn sits
// in a WeaponSeatDefinitions seat, grant + equip the seat weapon with a full
// clip; on seat change/exit remove it and re-equip the previous weapon or the
// pickaxe (ServerOnExitVehicle parity).
void Athena::GiveVehicleWeapon(void* weaponDef, int seatIdx) {
    if (!weaponDef || IsBadPtr(weaponDef) || !session.pawn || !session.inventory) return;
    if (session.vehicle_weapon_active && seatIdx == session.vehicle_weapon_seat) return;
    if (session.vehicle_weapon_active && seatIdx != session.vehicle_weapon_seat) RemoveVehicleWeapon(false);

    void* cur = Read<void*>(session.pawn, profile::FortPawn_CurrentWeapon);
    if (cur && !IsBadPtr(cur)) {
        void* p = GetProp("/Script/FortniteGame.FortWeapon.ItemEntryGuid");
        Guid cg = p ? Read<Guid>(GetField(cur, p)) : Guid{};
        if (!memcmp(&cg, &session.vehicle_weapon_guid, sizeof(Guid))) {
            session.vehicle_weapon_active = true;
            return;
        }
        if (!session.vehicle_weapon_active)
            session.previous_weapon_guid = cg;
    }
    Guid guid = GuidOfDefInInventory(weaponDef);
    if (!(guid.A | guid.B | guid.C | guid.D)) {
        int slot = -1;
        for (int i = 1; i < 6; ++i) if (!session.slots[(size_t)i]) { slot = i; break; }
        if (!GiveItem(weaponDef, 1, slot, slot >= 0 ? 0 : (uint8_t)-1, false, ClipSizeOf(weaponDef))) return;
        guid = GuidOfDefInInventory(weaponDef);
    }
    if (!(guid.A | guid.B | guid.C | guid.D)) return;
    session.vehicle_weapon_guid = guid;
    session.vehicle_weapon_seat = seatIdx;
    session.vehicle_weapon_active = true;
    OnServerExecuteInventoryItem(guid);
    LOG("[Vehicle] ✅ seat weapon %s seat=%d", ObjName(weaponDef).c_str(), seatIdx);
}

void Athena::RemoveVehicleWeapon(bool reequip) {
    if (!session.vehicle_weapon_active) return;
    session.vehicle_weapon_active = false;

    // احذف سلاح المقعد من المخزون (بدون drop — Erbium parity)
    RemoveItemByGuid(session.vehicle_weapon_guid, false, -1);
    session.vehicle_weapon_guid = Guid{};
    session.vehicle_weapon_seat = -1;

    if (reequip) {
        // إعادة تجهيز السلاح السابق أو الفأس
        if (session.previous_weapon_guid.A | session.previous_weapon_guid.B |
            session.previous_weapon_guid.C | session.previous_weapon_guid.D) {
            OnServerExecuteInventoryItem(session.previous_weapon_guid);
        } else if (session.slots[0]) {
            EquipSlot(0);
        }
        session.previous_weapon_guid = Guid{};
    }
}
// cache دالة GetVehicleActor — يمنع name-walk كل 0.5s (كان spam بالآلاف)
static void* g_getVehicleFn = nullptr;
static bool  g_getVehicleTried = false;
static void* GetVehicleFn() {
    if (!g_getVehicleTried) {
        g_getVehicleTried = true;
        for (const char* p : {
            "/Script/FortniteGame.FortPlayerPawnAthena.GetVehicleActor",
            "/Script/FortniteGame.FortPawn.GetVehicleActor",
            "/Script/FortniteGame.FortPlayerPawnAthena.GetVehicle",
            "/Script/FortniteGame.FortPawn.GetVehicle"
        }) {
            void* f = FindObj(p);
            if (f && !IsBadPtr(f)) { g_getVehicleFn = f; LOG("[Vehicle] cached GetVehicleActor=%p", f); break; }
        }
        if (!g_getVehicleFn) LOG("[Vehicle] ⚠️ GetVehicleActor not found");
    }
    return g_getVehicleFn;
}
static void* GetVehicleFromPawn() {
    void* fn = GetVehicleFn();
    if (!fn || !session.pawn || !orig_PE) return nullptr;
    Guard g;
    uint8_t buf[16]{};
    orig_PE(session.pawn, fn, buf);
    void* v = Read<void*>(buf);
    return (v && !IsBadPtr(v)) ? v : nullptr;
}

void Athena::TickVehicleSeats() {
    if (!session.pawn) return;
    static double nextPoll = 0;
    double now = Monotonic();
    if (now < nextPoll) return;
    nextPoll = now + 0.5;

    void* vehicle = GetVehicleFromPawn();
    if (!vehicle) { RemoveVehicleWeapon(true); return; }

    void* seatComp = nullptr, *wpnComp = nullptr;
    {
        Call get("/Script/Engine.Actor.GetComponentByClass");
        if (get.ok()) {
            void* sc = FindObj("/Script/FortniteGame.FortVehicleSeatComponent");
            if (sc && get.set("ComponentClass", sc) && get.run(vehicle)) seatComp = get.result<void*>();
            void* wc = FindObj("/Script/FortniteGame.FortVehicleSeatWeaponComponent");
            if (wc) {
                get = Call("/Script/Engine.Actor.GetComponentByClass");
                if (get.ok() && get.set("ComponentClass", wc) && get.run(vehicle)) wpnComp = get.result<void*>();
            }
        }
    }
    if (!seatComp) { RemoveVehicleWeapon(true); return; }

    int seatIdx = -1;
    {
        Call find("/Script/FortniteGame.FortVehicleSeatComponent.FindSeatIndex");
        if (find.ok() && find.set("Pawn", session.pawn) && find.run(seatComp))
            seatIdx = find.result<int>();
    }
    if (seatIdx < 0) { RemoveVehicleWeapon(true); return; }
    if (!wpnComp || IsBadPtr(wpnComp)) { RemoveVehicleWeapon(true); return; }

    void* activeSeatProp = GetProp("/Script/FortniteGame.FortVehicleSeatWeaponComponent.ActiveSeatIdx");
    if (activeSeatProp && Read<int>(activeSeatProp, profile::PropElementSize) == 4)
        Write<int32>(GetField(wpnComp, activeSeatProp), 0, seatIdx);

    void* wsdProp = GetProp("/Script/FortniteGame.FortVehicleSeatWeaponComponent.WeaponSeatDefinitions");
    if (!wsdProp) { RemoveVehicleWeapon(true); return; }
    int stride = Read<int>(wsdProp, profile::PropElementSize);
    void* inner = Read<void*>(wsdProp, 0x70);
    void* sStruct = inner ? Read<void*>(inner, 0x70) : nullptr;
    int seatOff = sStruct ? PropOffsetOn(sStruct, "SeatIndex") : -1;
    int wpnOff  = sStruct ? PropOffsetOn(sStruct, "VehicleWeapon") : -1;
    if (stride < 16 || seatOff < 0 || wpnOff < 0) { RemoveVehicleWeapon(true); return; }

    Array defs = Read<Array>(GetField(wpnComp, wsdProp));
    if (!defs.Data || IsBadPtr(defs.Data) || defs.Count < 0 || defs.Count > 16) { RemoveVehicleWeapon(true); return; }

    for (int i = 0; i < defs.Count; ++i) {
        void* row = (char*)defs.Data + (size_t)i * stride;
        if (Read<int32>(row, seatOff) != seatIdx) continue;
        void* wdef = Read<void*>(row, wpnOff);
        if (wdef && !IsBadPtr(wdef)) {
            GiveVehicleWeapon(wdef, seatIdx);
            int lewOff = sStruct ? PropOffsetOn(sStruct, "LastEquippedVehicleWeapon") : -1;
            if (lewOff >= 0) Write<void*>(row, lewOff, wdef);
            return;
        }
        break;
    }
    RemoveVehicleWeapon(true);
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

// ============================================================================
// MISSING CITIES / BUILDINGS — [F-cities r1]
// ============================================================================
// 21.30 preassembled structures (POIs, cities, buildings) stream through
// AFortSaveFileBuildingInstructionsHandler::LoadBuildings(FortPC) — the same
// native driver the live server runs. In standalone nobody drives it, so the
// handler ships inert (bLoadInvisible / no auto-spawn) and whole POIs never
// materialise. Fix: find the handler actors, flip the invisible flag off and
// run the load pass ourselves. The retry window (TickBuildingLoads) covers
// handlers that stream in late with the POI sublevels.
void Athena::FixMissingBuildings() {
    if (!session.world) return;
    void* cls = FindObj("/Script/FortniteGame.FortSaveFileBuildingInstructionsHandler");
    if (!cls) { LOG("[Cities] handler class not present"); return; }
    std::vector<void*> handlers;
    if (!GetAllActorsOfClassRaw(cls, handlers) || handlers.empty()) { LOG("[Cities] no handler actors (yet)"); return; }
    int driven = 0;
    for (void* h : handlers) {
        if (!h || IsBadPtr(h)) continue;
        SetBool(h, "/Script/FortniteGame.FortSaveFileBuildingInstructionsHandler.bLoadInvisible", false);
        Call load("/Script/FortniteGame.FortSaveFileBuildingInstructionsHandler.LoadBuildings");
        if (!load.ok()) continue;
        load.set("FortPC", session.controller);
        if (load.run(h)) ++driven;
    }
    LOG("[Cities] LoadBuildings driven on %d/%zu handlers", driven, handlers.size());
    if (driven > 0) {
        // Native driver loaded the pieces — level-set the foundations so the
        // static/ dynamic split matches the standalone expectations.
        FixBuildingFoundations();
    }
}

void Athena::TickBuildingLoads() {
    if (session.building_load_attempts >= 10) return;
    static double last = 0;
    double now = Monotonic();
    if (now - last < 15.0) return;
    last = now;
    void* cls = FindObj("/Script/FortniteGame.FortSaveFileBuildingInstructionsHandler");
    if (!cls) return;
    std::vector<void*> handlers;
    if (!GetAllActorsOfClassRaw(cls, handlers) || handlers.empty()) return;
    if (session.building_load_attempts == 0) LOG("[Cities] handler actors found: %zu", handlers.size());
    ++session.building_load_attempts;
    LOG("[Cities] loading attempt %d", session.building_load_attempts);
    FixMissingBuildings();
}

// ============================================================================
// FLOOR LOOT WHILE STREAMING — [F-loot-auto r1]
// ============================================================================
// "Loot seems manual" — Atlas: the one-shot SpawnFloorLoot ran during
// SetupWorld, but Tiered_Athena_FloorLoot/Warmup spawners stream in with the
// POI sublevels minutes later; the early pass found nothing, so the only loot
// the player ever saw came from chest interaction. Fix: keep re-rolling the
// floor-loot spawners over a 20 s × 6 window like the vehicles path
// (SpawnFloorLoot destroys each spawner after rolling it, so retries only
// process newly streamed ones).
void Athena::TickFloorLootStreaming() {
    if (session.floor_loot_attempts >= 6) return;
    static double last = 0;
    double now = Monotonic();
    if (now - last < 20.0) return;
    last = now;
    void* cls = FindObj("/Game/Athena/Environments/Blueprints/Tiered_Athena_FloorLoot_01.Tiered_Athena_FloorLoot_01_C");
    if (!cls) cls = LoadClass("/Game/Athena/Environments/Blueprints/Tiered_Athena_FloorLoot_01.Tiered_Athena_FloorLoot_01_C", "/Script/Engine.Actor");
    if (!cls) return;
    std::vector<void*> spawners;
    if (!GetAllActorsOfClassRaw(cls, spawners) || spawners.empty()) return;
    session.floor_loot_attempts = 6;   // spawners present — final full rollout below covers warmup type too
    LOG("[Loot] streaming rollout: %zu floor-loot spawners", spawners.size());
    SpawnFloorLoot();
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
    bool reequipAfterExit = false;   // ← أضف هذا
    if (!g_internal && func && g_gameTid.load() == (int)gettid()) {
        if (session.lobby_requested) { orig_PE(obj, func, params); return; }

        std::string fn = ObjName(func);
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

        // [F37] Container/BuildingSMActor receivers: interaction + harvest
        // functions are dispatched ON the world actor, not on the controller
        // (ABuildingContainer::ServerOnAttemptInteract in Erbium v3,
        // ABuildingSMActor::OnDamageServer) — the old filter dropped them.
        bool isWorldTarget = false;
        static void* s_bcCls = nullptr, *s_smCls = nullptr;
        if (obj && !isPC && !isPawn && !isWeapon && !isInteraction &&
            (fn == "ServerOnAttemptInteract" || fn == "OnDamageServer")) {
            void* cls = Read<void*>(obj, profile::ObjectClass);
            if (!s_bcCls) s_bcCls = FindObj("/Script/FortniteGame.BuildingContainer");
            if (!s_smCls) s_smCls = FindObj("/Script/FortniteGame.BuildingSMActor");
            if ((s_bcCls && SubclassOfFast(cls, s_bcCls)) || (s_smCls && SubclassOfFast(cls, s_smCls)))
                isWorldTarget = true;
        }

bool reequipAfterExit = false;   // [F28-b] خروج من مركبة يحمل سلاح مقعد
if (isPC || isPawn || isWeapon || isInteraction || isWorldTarget) {
    Guard g;
            if (fn == "ServerExecuteInventoryItem") {
                if (Read<uint16_t>(func, profile::FunctionParamSize) >= (int)sizeof(Guid))
                    Athena::OnServerExecuteInventoryItem(Read<Guid>(params));
            }
            else if (fn == "ServerExecuteInventoryWeapon") Athena::OnServerExecuteInventoryWeapon(Read<void*>(params));
            // [F35] ServerHandlePickup(PickUp, InFlyTime, InStartDirection,
            // bPlayPickupSound) / ServerHandlePickupInfo(Pickup, Info) /
            // ServerHandlePickupWithRequestedSwap — the fly-to-player flow is
            // FULLY replaced (Erbium SetPickupTarget + scheduled InternalPickup);
            // the native server-side half does not exist in this build, which
            // is why loot vanished instead of being picked up. The OG call is
            // therefore SKIPPED for pickup RPCs.
            else if ((fn == "ServerHandlePickupInfo" || fn == "ServerHandlePickup" ||
                      fn == "ServerHandlePickupWithRequestedSwap") && isPawn && params) {
                void* arg = incoming(func, params, "PickUp", 8);
                if (!arg) arg = incoming(func, params, "Pickup", 8);
                if (!arg) arg = params;   // PickUp is the first parameter
                float flyTime = 0.45f;
                { void* ft = incoming(func, params, "InFlyTime", 4);
                  if (ft) flyTime = Read<float>(ft);
                  else if ((ft = incoming(func, params, "FlyTime", 4))) flyTime = Read<float>(ft); }
                Athena::HandlePickupRPC(Read<void*>(arg), flyTime);
                return;   // native flow replaced — no OG
            }
            // [F35] Erbium OnCapsuleBeginOverlap parity: the pawn's capsule is
            // what triggers pickup on a live server; in standalone the native
            // client pawn never requests the pickup, so detect it here.
            else if (fn == "OnCapsuleBeginOverlap" && isPawn && params) {
                void* otherSlot = incoming(func, params, "OtherActor", 8);
                if (!otherSlot) otherSlot = incoming(func, params, "OtherActor", 0);
                if (otherSlot) {
                    void* other = Read<void*>(otherSlot);
                    if (other && !IsBadPtr(other) && IsReadable(other, 0x40)) {
                        static void* pickCls = nullptr;
                        if (!pickCls) pickCls = FindObj("/Script/FortniteGame.FortPickupAthena");
                        if (!pickCls) pickCls = FindObj("/Script/FortniteGame.FortPickup");
                        if (pickCls && IsA(other, pickCls))
                            Athena::HandlePickupRPC(other, 0.45f);
                    }
                }
            }
            // [F37] Interaction — receivers: interaction component, PC, and
            // (v3 Erbium parity) the container itself via ServerOnAttemptInteract.
            else if ((fn == "ServerAttemptInteract" || fn == "ServerOnAttemptInteract") && params && (isInteraction || isPC || isWorldTarget)) {
                // [F84] Named-param target resolution (Erbium local_interact
                // parity): ServerAttemptInteract carries ReceivingActor as a
                // NAMED 8-byte property; a raw offset-0 read returns garbage
                // whenever the cooked param layout shifts — the silent
                // dead-interact symptom. Named lookup first, raw fallback
                // second, container-self third.
                void* slot = incoming(func, params, "ReceivingActor", 8);
                void* target = slot ? Read<void*>(slot) : nullptr;
                if (!target || IsBadPtr(target)) target = Read<void*>(params, 0);
                // ServerOnAttemptInteract dispatched on the container: obj IS the target.
                if ((!target || IsBadPtr(target)) && isWorldTarget) target = obj;
                if (target && !IsBadPtr(target) && IsReadable(target, 0x60)) {
                    // [F84] Erbium gates (local_interact): the target must be
                    // a real Actor within 600 uu of the pawn — wild pointers
                    // from a stale param slot are rejected instead of being
                    // dispatched into (crash/poison source). Un-locatable
                    // targets still dispatch to preserve the old paths.
                    static void* s_engineActorCls = nullptr;
                    if (!s_engineActorCls) s_engineActorCls = FindObj("/Script/Engine.Actor");
                    Vector tloc{};
                    if (s_engineActorCls && !IsA(target, s_engineActorCls)) {
                        LOG("[Interact] F84 non-actor target rejected");
                    } else if (ActorLocation(target, tloc) && !NearPlayer(tloc, 600.0f)) {
                        LOG("[Interact] F84 far target rejected (%.0f,%.0f,%.0f)", tloc.X, tloc.Y, tloc.Z);
                    } else {
                    void* bcCls   = FindObj("/Script/FortniteGame.BuildingContainer");
                    void* wallCls = FindObj("/Script/FortniteGame.BuildingWall");
                    void* actorCls = FindObj("/Script/FortniteGame.BuildingActor");
                    void* targetCls = Read<void*>(target, profile::ObjectClass);
                    LOG("[Interact] target=%p class=%s", target, ObjName(targetCls).c_str());
static void* vCls = nullptr;
if (!vCls) vCls = FindObj("/Script/FortniteGame.FortAthenaVehicle");
if (vCls && SubclassOfFast(targetCls, vCls)) {
    LOG("[Vehicle] interact -> %s (native handles enter)", ObjName(targetCls).c_str());
    // لا return — يجب أن يصل إلى orig_PE() في نهاية pe_hook
}
                    if (bcCls && SubclassOfFast(targetCls, bcCls)) Athena::Loot(target);
                    else if (wallCls && SubclassOfFast(targetCls, wallCls)) Athena::HandleDoor(target);
                    else if (actorCls && SubclassOfFast(targetCls, actorCls)) {
                        // [F16] generic building interaction via BP override.
                        Call interact("/Script/FortniteGame.BuildingActor.BlueprintOnInteract", target);
                        if (interact.ok() && interact.set("InteractingPawn", session.pawn)) interact.run(target);
                    }
                    }
                }
            }
            // [F39] Harvest resources on pickaxe damage (Erbium OnDamageServer).
            else if (fn == "OnDamageServer" && isWorldTarget) {
                Athena::HandleDamageServer(obj, func, params);
            }
            else if (fn == "ServerAttemptAircraftJump" && !session.aircraft_jumped) Athena::HandleJumpFromHudStart();
            // [F38] Building RPCs are FULLY replaced (Erbium ServerCreate-
            // BuildingActor path). The native implementation is skipped: in
            // standalone its BuildingClassMap is empty and the exit-build-mode
            // payload crashes it — the crash the user saw when leaving build mode.
            else if (fn == "ServerCreateBuildingActor" && params && isPC) {
                Athena::OnServerCreateBuildingActor(params);
                return;   // native path replaced — no OG
            }
            else if (fn.rfind("ServerCreateBuilding", 0) == 0 && params && isPC) {
                Athena::LocalBuild();
                return;   // native path replaced — no OG
            }
            else if (fn == "ServerBeginEditingBuildingActor" && params) {
                void* piece = Read<void*>(params, 0);
                if (piece && !IsBadPtr(piece) && IsReadable(piece, 0x40)) Athena::OnBeginEditActor(piece);
            }
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
            // [F28-b] الخروج: احذف سلاح المقعد الآن، وأعد التجهيز بعد اكتمال
// الخروج الأصلي (Erbium ترتيب: Remove → native → re-equip).
else if (fn == "ServerOnExitVehicle" && isPawn) {
    if (session.vehicle_weapon_active) {
        Athena::RemoveVehicleWeapon(false);   // حذف فقط، لا reequip
        reequipAfterExit = true;
    }
}
// ServerAttemptExitVehicle يُتْرك للأصلي (native يتولّى RPC الخروج)            // [F24] ServerAttemptInventoryDrop(ItemGuid, Count, bTrash).
            else if (fn == "ServerAttemptInventoryDrop" && params) {
                Guid gg = Read<Guid>(params);
                int cnt = Read<int>(params, sizeof(Guid));
                Athena::DropInventoryItem(gg, cnt);
            }
        }
    }
    orig_PE(obj, func, params);

    // [F28-b] إعادة تجهيز السلاح السابق بعد اكتمال الخروج
    if (reequipAfterExit) {
        if (session.previous_weapon_guid.A | session.previous_weapon_guid.B |
            session.previous_weapon_guid.C | session.previous_weapon_guid.D) {
            Athena::OnServerExecuteInventoryItem(session.previous_weapon_guid);
            LOG("[Vehicle] re-equipped previous weapon after exit");
        } else if (session.slots[0]) {
            Athena::EquipSlot(0);   // فأس كاحتياطي
            LOG("[Vehicle] re-equipped pickaxe after exit");
        }
        session.previous_weapon_guid = Guid{};
    }
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
        // [F91-b/BEX-1] transition-window scrub: we are in Frontend while the
        // session still points at the departed terrain world — every actor
        // pointer (pawn/controller/inventory/quickbars/state) is stale and
        // pe_hook class-based handlers (JumpFromHudStart family) would
        // dereference freed actors. Scrub the pointers (flags keep their
        // semantics for the pending-finalize check above); the terrain
        // detector issues a full fresh session on the next match, which also
        // re-arms session.qb_retries [F91-a].
        if (session.world && session.world != world) {
            session.pawn = nullptr; session.controller = nullptr;
            session.inventory = nullptr; session.quickbars = nullptr;
            session.player_state = nullptr; session.tactical_sprint = nullptr;
            session.world = nullptr; session.world_index = -1;
            LOG(">>> [F91] stale terrain session scrubbed (entered %s)", wn.c_str());
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
        } else if (normalized == "era.vehicles" && session.world) {
            // [F28] manual re-run of the vehicle spawner pass
            session.vehicles_spawned = false;
            session.vehicle_attempts = 0;
            Athena::SpawnVehicles();
        } else if (normalized == "era.debug") {
            Athena::DebugDump();   // [F42] quickbars/inventory/match-state dump
        } else if (normalized == "era.fix" && session.world && session.finalized) {
            // [F42/F43] re-run every world-side fix pass (state, QBs, loot, cars, HUD)
            Athena::EnsurePlayerState();
            if (!session.quickbars_initialized) {
                Athena::InitializeQuickBarsNative();
                if (!session.quickbars_initialized) Athena::BuildQuickBarsManually();
                if (session.quickbars_initialized) Athena::SetQuickBarReady();
            }
            Athena::ResetStaleContainers();
            Athena::SpawnFloorLoot();
            session.vehicles_spawned = false;
            session.vehicle_attempts = 0;
            Athena::SpawnVehicles();
            Athena::SetupMatchState();
            Athena::InventoryUpdate();
            Athena::ForceUpdateQuickBars();
            LOG("era.fix: all passes re-run");
        } else if (normalized == "era.loot" && session.world && session.pawn) {
            // [F32] roll every unsearched container within 30 m (diagnostic aid)
            void* cls = FindObj("/Script/FortniteGame.BuildingContainer");
            std::vector<void*> actors;
            if (cls && GetAllActorsOfClassRaw(cls, actors)) {
                int n = 0;
                for (void* c : actors) {
                    Vector loc{};
                    if (!c || IsBadPtr(c) || !ActorLocation(c, loc) || !NearPlayer(loc, 3000.0f)) continue;
                    if (Athena::SearchContainer(c, loc)) ++n;
                }
                LOG("era.loot: %d containers rolled", n);
            }
        } else if (normalized.rfind("era.spawn ", 0) == 0 && session.world && session.pawn) {
            char path[512]{};
            if (sscanf(cmd.c_str(), "%*s %511[^\r\n]", path) == 1) {
                void* asDef = LoadAsset(path, "/Script/FortniteGame.FortItemDefinition");
                if (asDef) {
                    Vector loc{}; ActorLocation(session.pawn, loc); loc.Z += 100.0;
                    Athena::SpawnPickup(asDef, 1, loc, 1, 0, true);
                    LOG("Spawned pickup %s", path);
                } else {
                    void* cls = LoadClass(path, "/Script/Engine.Actor");
                    if (cls) {
                        Vector loc{}; ActorLocation(session.pawn, loc); loc.Z += 200.0;
                        Transform t{}; t.Location[0] = loc.X; t.Location[1] = loc.Y; t.Location[2] = loc.Z;
                        void* a = SpawnActor(session.world, cls, session.controller, t);
                        LOG("Spawned %s -> %p", path, a);
                    } else LOG("era.spawn: nothing found at %s", path);
                }
            }
        } else LOG("Commands: era.status | era.inventory | era.debug | era.fix | era.equip 0..5 | era.give Name [count] [slot] | era.vehicles | era.loot | era.spawn /Game/Path | fly | walk | ghost | god | teleport | era.lobby");
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
