// ============================================================================
// EraCH3 — Fortnite 21.30 — Full Erbium port + safe inventory
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

namespace profile {
constexpr uintptr_t ProcessEvent         = 0x07611B5C;
constexpr uintptr_t StaticFindObject     = 0x0762A664;
constexpr uintptr_t StaticLoadObject     = 0x0761C568;
constexpr uintptr_t StaticLoadClass      = 0x0762CFB8;
constexpr uintptr_t NameAppendString     = 0x074937CC;
constexpr uintptr_t Free                 = 0x07355370;
constexpr uintptr_t WorldTick            = 0x0949685C;
constexpr uintptr_t EngineExec           = 0x098BB160;
constexpr uintptr_t GetPlayerController  = 0x093A1D48;
constexpr uintptr_t BeginSpawn           = 0x093A37E4;
constexpr uintptr_t FinishSpawn          = 0x0939F1B8;
constexpr uintptr_t SpawnObject          = 0x093A3388;
constexpr uintptr_t GenericArrayAdd      = 0x0944A668;
constexpr uintptr_t GenericArrayRemove   = 0x0944B2DC;
constexpr uintptr_t GObjects             = 0x0DCD6E08;
constexpr uintptr_t GetInterfaceAddress  = 0x0761AA88;

constexpr unsigned ObjectClass  = 0x10;
constexpr unsigned ObjectName   = 0x18;
constexpr unsigned FieldNext    = 0x28;
constexpr unsigned StructSuper  = 0x40;
constexpr unsigned StructChildren   = 0x48;
constexpr unsigned StructChildProps = 0x50;
constexpr unsigned FunctionParamSize = 0xB6;
constexpr unsigned PropFlags       = 0x38;
constexpr unsigned PropOffset      = 0x44;
constexpr unsigned PropElementSize = 0x34;
constexpr unsigned BoolSize       = 0x80;
constexpr unsigned BoolByteOffset = 0x81;
constexpr unsigned BoolFieldMask  = 0x82;
constexpr unsigned FFieldName = 0x28;
constexpr unsigned FFieldNext = 0x20;

constexpr uint64_t CPF_Parm       = 0x00000080ULL;
constexpr uint64_t CPF_ReturnParm = 0x00000400ULL;

// FastArraySerializerItem base fields (relative to FFortItemEntry start)
constexpr unsigned Entry_ReplicationID      = 0x00;
constexpr unsigned Entry_ReplicationKey     = 0x04;
constexpr unsigned Entry_MostRecentArrayKey = 0x08;

// Fallback offsets if GetProp fails
constexpr unsigned ItemEntry_Durability    = 0x24;
constexpr unsigned ItemEntry_LoadedAmmo    = 0x2C;
constexpr unsigned ItemEntry_TrackerGuid   = 0x78;

constexpr unsigned FortPlayerState_HeroType          = 0x0430;
constexpr unsigned FortPlayerController_MyPlayerInfo = 0x2088;
constexpr unsigned RegisteredPlayerInfo_HeroInstance = 0x2D0;
constexpr unsigned FortHero_CharacterParts           = 0x238;
constexpr unsigned FortPlayerPawn_bIsSkydiving       = 0x1A1D;
constexpr unsigned BuildingWall_bDoorOpen_Byte       = 0x0FDE;
constexpr unsigned BuildingWall_bDoorOpen_Bit        = 5;
constexpr unsigned PlayerPawn_Athena_Speed_Sprint    = 0x53EC;
constexpr unsigned FortPawn_LastReplicatedEmoteExecuted = 0x0CA0;

constexpr unsigned TacticalSprint_Flags              = 0xC0;
constexpr uint8_t TacticalSprint_Bit_Enabled   = 0x01;

constexpr unsigned Vending_ControllingPlayer = 0x09C8;
}

extern "C" int DobbyHook(void* target, void* replace, void** origin);

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
struct Guard { bool s = g_internal; Guard() { g_internal = true; } ~Guard() { g_internal = s; } };
template<class F> static F Fn(uintptr_t rva) { return reinterpret_cast<F>(g_base + rva); }

struct FStringNative { TCHAR* Data; int32 Count; int32 Max; };
struct FName         { uint32 CompIdx; uint32 DispIdx; };
struct Array         { void* Data; int32 Count; int32 Max; };
struct Guid          { uint32 A, B, C, D; };
struct Vector        { double X, Y, Z; };
struct Rotator       { double Pitch, Yaw, Roll; };
struct alignas(16) Transform {
    double Rotation[4]{0,0,0,1};
    double Location[3]{0,0,0};
    uint8_t _padL[8]{};
    double Scale[3]{1,1,1};
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
static void* GetPawn(void* controller);

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
    if (compIdx == 0xFFFFFFFF || compIdx > 0x8000000) return {};
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
static void* GetObjField(void* obj, const char* path) {
    void* p = GetProp(path);
    if (!p || !obj) return nullptr;
    if (Read<int>(p, profile::PropElementSize) != 8) return nullptr;
    return Read<void*>(GetField(obj, p));
}

using PE_t   = void(*)(void*, void*, void*);
using Tick_t = void(*)(void*, int, float);
using Exec_t = bool(*)(void*, void*, const char16_t*, void*);

static PE_t   orig_PE = nullptr;
static Tick_t orig_Tick = nullptr;
static Exec_t orig_Exec = nullptr;

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
    explicit Call(const char* path) {
        m_func = FindObj(path);
        if (!m_func) return;
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
    template<class T> bool set(const char* key, const T& value) {
        for (auto& p : m_params)
            if (p.name == key && p.size == (int)sizeof(T)) {
                memcpy(m_data.data() + p.offset, &value, sizeof(T));
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
    bool run(void* object) {
        if (!m_valid || !object || !orig_PE) return false;
        Guard g;
        orig_PE(object, m_func, m_data.data());
        return true;
    }
};

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
    if (!c.ok()) return;
    if (!c.set("InPawn", pawn)) return;
    c.run(controller);
}
static void* SpawnActor(void* world, void* klass, void* owner, const Transform& t) {
    if (!world || !klass) return nullptr;
    void* actor = Fn<void*(*)(void*, void*, const Transform*, uint8_t, void*)>(
        profile::BeginSpawn)(world, klass, &t, 1, owner);
    return actor ? Fn<void*(*)(void*, const Transform*)>(profile::FinishSpawn)(actor, &t) : nullptr;
}
static void* SpawnActorLoc(void* world, void* klass, void* owner, Vector loc) {
    Transform t{};
    t.Location[0] = loc.X; t.Location[1] = loc.Y; t.Location[2] = loc.Z;
    return SpawnActor(world, klass, owner, t);
}
static void* ResolveInterface(void* object, void* ifaceClass) {
    if (!object || !ifaceClass) return nullptr;
    using Fn_t = void*(*)(void*, void*);
    return ((Fn_t)(g_base + profile::GetInterfaceAddress))(object, ifaceClass);
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
    bool  movement_enabled = false;
    bool  playlist_loaded = false;
    bool  managers_setup = false;
    bool  hud_created = false;
    bool  gm_hook_fired = false;
    bool  inventory_populated = false;
    bool  equip_done = false;
    bool  vehicles_attempted = false;    // ← أضف هذا
    bool  wildlife_attempted = false;    // ← أضف هذا
    double observed_at = 0;
    double retry_at = 0;
    double finalize_at = 0;
    double populate_at = 0;
    std::array<void*, 6> slots{};
    std::vector<void*> character_parts;
    std::vector<void*> granted_ability_sets;
    void* edit_tool = nullptr;
} session;

static double Monotonic() {
    timespec ts{}; clock_gettime(CLOCK_MONOTONIC, &ts);
    return double(ts.tv_sec) + double(ts.tv_nsec) * 1e-9;
}

// ============================================================================
// ATHENA
// ============================================================================
namespace Athena {
    void SetupWorld(void* world);
    void Tick();
    bool TryInitializePlayer(void* world, void* mode, void* controller);
    void SetupCheats();
    void FinalizePlayer();
    void ShowSkin();
    void LoadDefaultCosmetics();
    void ForcePlayerVisible();
    void BeginPlayerSkydiving();
    void EquipSlot(int slot);
    void DestroyLods();
    void DropLoadingScreen();
    bool ReadInventoryPointers();
    void PopulateInventory();
    void AddToInventory(void* def, int count, int slot);
    void InventoryUpdate();
    void GrantAbility(void* klass);
    void GrantAllAbilitySets();
    void GrantMeleeAbility();
    void GrantClamberAbility();
    void GrantAllDefaultAbilities();
    void InitStaminaAttributes();
    void RefreshHUD();
    void ApplySprintSpeed();
    void SetupTacticalSprint();
    void SpawnSafeZoneIndicator();
    void SpawnPickup(void* def, int count, Vector loc, bool toss = true);
    void DropInventoryItem(Guid guid, int count);
    void OnServerExecuteInventoryItem(Guid guid);
    void OnServerExecuteInventoryWeapon(void* weapon);
    void HandleJumpFromHudStart();
    void SpawnBattleBus();
    void RemoveNetDebugUI();
    void ServerHandlePickup(void* pickup);
    void Loot(void* actor);
    void HandleDoor(void* door);
    void OnServerCreateBuildingActor(void* params);
    void OnBeginEditActor(void* piece);
    void OnFinishEditActor(void* actor, void* newClass, int rotIter, bool mirrored);
    void PlayEmoteItem(void* item);
    void OnExitVehicle();
    void UseVendingMachine(void* machine);
    void ActivateQuickBarSlot(uint8_t bar, int slot);
    void ForceUpdateQuickBars();
    void EnableMovementFeatures();
    void EnablePlaylist(void* gs);
    void SetupGameModeManagers(void* gm, void* gs);
    void SpawnDefaultVehicles();
    void EnableWildlife();
    void SetupHUD();
    void HandleReadyToStartMatch(void* gm);
    // Erbium-style AbilitySets
    void GrantAllAbilitySetsLikeErbium();
}

static void* GetASC(void* actor) {
    if (!actor) return nullptr;
    void* asc = GetObjField(actor, "/Script/FortniteGame.FortPawn.AbilitySystemComponent");
    if (asc) return asc;
    asc = GetObjField(actor, "/Script/FortniteGame.FortPlayerState.AbilitySystemComponent");
    if (asc) return asc;
    asc = GetObjField(actor, "/Script/GameplayAbilities.AbilitySystemComponent");
    return asc;
}

// ============================================================================
// ABILITIES — Erbium port
// ============================================================================
void Athena::GrantAbility(void* klass) {
    if (!klass || !session.pawn) return;
    void* asc = GetASC(session.pawn);
    if (!asc) return;
    Call c("/Script/GameplayAbilities.AbilitySystemComponent.TryActivateAbilityByClass");
    if (!c.ok()) return;
    bool bAllow = true;
    if (c.set("InAbilityToActivate", klass) && c.set("bAllowRemoteActivation", bAllow)) {
        c.run(asc);
        LOG("[Ability] Activated: %s", ObjName(klass).c_str());
    }
}
static void GiveAbilityOnly(void* klass) {
    if (!klass || !session.pawn) return;
    void* asc = GetASC(session.pawn);
    if (!asc) return;
    Call c("/Script/GameplayAbilities.AbilitySystemComponent.K2_GiveAbility");
    if (!c.ok()) return;
    int level = 1, inputID = -1;
    if (c.set("AbilityClass", klass) && c.set("Level", level) && c.set("InputID", inputID))
        c.run(asc);
}

static bool ApplyAbilitySet(void* pawn, void* abilitySet) {
    if (!pawn || !abilitySet || IsBadPtr(abilitySet)) return false;
    void* klib = FindObj("/Script/FortniteGame.Default__FortKismetLibrary");
    if (!klib) return false;
    Call c("/Script/FortniteGame.FortKismetLibrary.EquipFortAbilitySet");
    if (!c.ok()) return false;
    void* ifaceType = FindObj("/Script/GameplayAbilities.AbilitySystemInterface");
    if (!ifaceType) return false;
    void* ifaceAddr = ResolveInterface(pawn, ifaceType);
    if (!ifaceAddr) {
        void* state = GetObjField(pawn, "/Script/Engine.Controller.PlayerState");
        if (!state) {
            void* pc = GetObjField(pawn, "/Script/Engine.Pawn.Controller");
            if (pc) state = GetObjField(pc, "/Script/Engine.Controller.PlayerState");
        }
        if (state) ifaceAddr = ResolveInterface(state, ifaceType);
    }
    TScriptInterface iface{pawn, ifaceAddr};
    bool s1 = c.set("AbilitySystemInterfaceActor", iface);
    bool s2 = c.set("AbilitySet", abilitySet);
    bool s3 = c.set("OverrideSourceObject", pawn);
    if (!s1 || !s2 || !s3) return false;
    if (!c.run(klib)) return false;
    LOG("[AbilitySet] ✅ Applied: %s", ObjName(abilitySet).c_str());
    return true;
}

void Athena::InitStaminaAttributes() {
    if (!session.pawn) return;
    void* asc = GetASC(session.pawn);
    if (!asc) return;
    void* attrClass = FindObj("/Script/FortniteGame.FortPlayerAttributeSet");
    if (!attrClass) attrClass = FindObj("/Script/GameplayAbilities.AttributeSet");
    if (!attrClass) return;
    void* dataTable = FindObj("/Game/Athena/Items/Quests/AthenaObjectiveStatXPTable.AthenaObjectiveStatXPTable");
    Call c("/Script/GameplayAbilities.AbilitySystemComponent.K2_InitStats");
    if (!c.ok()) return;
    if (!c.set("Attributes", attrClass)) return;
    if (dataTable) c.set("DataTable", dataTable);
    if (!c.run(asc)) return;
    session.stamina_initialized = true;
    LOG("[Stamina] Initialized");
}

// ⭐ Erbium-style: All AbilitySets for s21
void Athena::GrantAllAbilitySetsLikeErbium() {
    if (!session.pawn) return;
    LOG("[AbilitySets] Loading all Erbium AbilitySets...");

    // GAS_AthenaPlayer (core)
    void* coreSet = FindObj("/Game/Abilities/Player/Generic/Traits/DefaultPlayer/GAS_AthenaPlayer.GAS_AthenaPlayer");
    if (!coreSet) coreSet = LoadAsset("/Game/Abilities/Player/Generic/Traits/DefaultPlayer/GAS_AthenaPlayer.GAS_AthenaPlayer",
                                      "/Script/FortniteGame.FortAbilitySet");
    if (coreSet) {
        ApplyAbilitySet(session.pawn, coreSet);
        session.granted_ability_sets.push_back(coreSet);
    }

    // Sprint (s20+)
    const char* sprintSets[] = {
        "/TacticalSprintGame/Gameplay/AS_TacticalSprint.AS_TacticalSprint",
        "/TacticalSprint/Gameplay/AS_TacticalSprint.AS_TacticalSprint",
        nullptr
    };
    for (int i = 0; sprintSets[i]; ++i) {
        void* s = FindObj(sprintSets[i]);
        if (!s) s = LoadAsset(sprintSets[i], "/Script/FortniteGame.FortAbilitySet");
        if (s) {
            ApplyAbilitySet(session.pawn, s);
            session.granted_ability_sets.push_back(s);
            LOG("[AbilitySets] ✅ Sprint");
            break;
        }
    }

    // Ascender / Zipline (s20+)
    const char* ascenderSets[] = {
        "/Ascender/Gameplay/Ascender/AS_Ascender.AS_Ascender",
        "/Ascender/Gameplay/AS_Ascender.AS_Ascender",
        nullptr
    };
    for (int i = 0; ascenderSets[i]; ++i) {
        void* s = FindObj(ascenderSets[i]);
        if (!s) s = LoadAsset(ascenderSets[i], "/Script/FortniteGame.FortAbilitySet");
        if (s) {
            ApplyAbilitySet(session.pawn, s);
            session.granted_ability_sets.push_back(s);
            LOG("[AbilitySets] ✅ Ascender");
            break;
        }
    }

    // Door Bash (s20+)
    const char* doorBashSets[] = {
        "/DoorBashContent/Gameplay/AS_DoorBash.AS_DoorBash",
        nullptr
    };
    for (int i = 0; doorBashSets[i]; ++i) {
        void* s = FindObj(doorBashSets[i]);
        if (!s) s = LoadAsset(doorBashSets[i], "/Script/FortniteGame.FortAbilitySet");
        if (s) {
            ApplyAbilitySet(session.pawn, s);
            session.granted_ability_sets.push_back(s);
            LOG("[AbilitySets] ✅ DoorBash");
            break;
        }
    }

    // Hill Scramble (s20+)
    const char* hillSets[] = {
        "/HillScramble/Gameplay/AS_HillScramble.AS_HillScramble",
        nullptr
    };
    for (int i = 0; hillSets[i]; ++i) {
        void* s = FindObj(hillSets[i]);
        if (!s) s = LoadAsset(hillSets[i], "/Script/FortniteGame.FortAbilitySet");
        if (s) {
            ApplyAbilitySet(session.pawn, s);
            session.granted_ability_sets.push_back(s);
            LOG("[AbilitySets] ✅ HillScramble");
            break;
        }
    }

    // Slide Impulse (s20+)
    const char* slideSets[] = {
        "/SlideImpulse/Gameplay/AS_SlideImpulse.AS_SlideImpulse",
        nullptr
    };
    for (int i = 0; slideSets[i]; ++i) {
        void* s = FindObj(slideSets[i]);
        if (!s) s = LoadAsset(slideSets[i], "/Script/FortniteGame.FortAbilitySet");
        if (s) {
            ApplyAbilitySet(session.pawn, s);
            session.granted_ability_sets.push_back(s);
            LOG("[AbilitySets] ✅ SlideImpulse");
            break;
        }
    }

    // Reality Sapling (s21 only)
    const char* realitySets[] = {
        "/RealitySeedGameplay/Environment/Foliage/GAS_Athena_RealitySapling.GAS_Athena_RealitySapling",
        nullptr
    };
    for (int i = 0; realitySets[i]; ++i) {
        void* s = FindObj(realitySets[i]);
        if (!s) s = LoadAsset(realitySets[i], "/Script/FortniteGame.FortAbilitySet");
        if (s) {
            ApplyAbilitySet(session.pawn, s);
            session.granted_ability_sets.push_back(s);
            LOG("[AbilitySets] ✅ RealitySapling");
            break;
        }
    }

    LOG("[AbilitySets] Total: %zu", session.granted_ability_sets.size());
}

void Athena::GrantAllAbilitySets() {
    if (!session.pawn || session.abilities_granted) return;
    LOG("[Abilities] Setting up...");
    void* asc = GetASC(session.pawn);
    if (!asc) { LOG("[Abilities] no ASC"); return; }
    if (!session.stamina_initialized) InitStaminaAttributes();

    // Core set
    void* coreSet = LoadAsset(
        "/Game/Abilities/Player/Generic/Traits/DefaultPlayer/GAS_AthenaPlayer.GAS_AthenaPlayer",
        "/Script/FortniteGame.FortAbilitySet");
    if (coreSet) {
        if (ApplyAbilitySet(session.pawn, coreSet)) session.granted_ability_sets.push_back(coreSet);
    }

    // Extra native abilities
    const char* abilities[] = {
        "/Script/GameplayAbilities.GameplayAbility_CharacterJump",
        "/Game/Abilities/Player/Generic/Traits/DefaultPlayer/GA_DefaultPlayer_InteractUse.GA_DefaultPlayer_InteractUse_C",
        "/Game/Abilities/Player/Generic/Traits/DefaultPlayer/GA_DefaultPlayer_InteractSearch.GA_DefaultPlayer_InteractSearch_C",
        "/Game/Abilities/Player/Generic/Traits/DefaultPlayer/GA_DefaultPlayer_Death.GA_DefaultPlayer_Death_C",
        "/Game/Athena/Items/ForagedItems/Rift/GA_Rift_Athena_Skydive.GA_Rift_Athena_Skydive_C",
        nullptr
    };
    for (int i = 0; abilities[i]; ++i) {
        void* cls = FindObj(abilities[i]);
        if (!cls) cls = LoadClass(abilities[i], "/Script/GameplayAbilities.GameplayAbility");
        if (!cls) continue;
        GrantAbility(cls);
    }
    session.abilities_granted = true;

    // Erbium-style sets
    GrantAllAbilitySetsLikeErbium();
    GrantMeleeAbility();
    GrantClamberAbility();
    LOG("[Abilities] Done");
}

void Athena::GrantMeleeAbility() {
    if (!session.pawn || session.melee_granted) return;
    session.melee_granted = true;
    void* asc = GetASC(session.pawn);
    if (!asc) return;
    const char* paths[] = {
        "/Game/Abilities/Weapons/Melee/GAB_Melee_GenericCombo.GAB_Melee_GenericCombo_C",
        "/Game/Abilities/Weapons/Melee/GAB_Melee_Generic.GAB_Melee_Generic_C",
        nullptr
    };
    for (int i = 0; paths[i]; ++i) {
        void* meleeCls = LoadClass(paths[i], "/Script/GameplayAbilities.GameplayAbility");
        if (meleeCls) { GiveAbilityOnly(meleeCls); LOG("[Melee] Granted"); return; }
    }
}
void Athena::GrantClamberAbility() {
    if (!session.pawn) return;
    const char* paths[] = {
        "/Game/Abilities/Player/Traversal/GA_Athena_Player_Clamber.GA_Athena_Player_Clamber_C",
        "/Game/Abilities/Player/Generic/Traversal/GA_Athena_Player_Clamber.GA_Athena_Player_Clamber_C",
        nullptr
    };
    for (int i = 0; paths[i]; ++i) {
        void* cls = LoadClass(paths[i], "/Script/GameplayAbilities.GameplayAbility");
        if (cls) { GiveAbilityOnly(cls); LOG("[Clamber] Granted"); return; }
    }
}

void Athena::RefreshHUD() {
    if (!session.controller) return;
    void* asc = GetASC(session.pawn);
    if (asc) {
        Call c("/Script/GameplayAbilities.AbilitySystemComponent.OnRep_ActivateAbilities");
        if (c.ok()) c.run(asc);
    }
}

// ============================================================================
// MOVEMENT
// ============================================================================
void Athena::EnableMovementFeatures() {
    if (!session.pawn) return;
    Call canSlide("/Script/FortniteGame.FortPlayerPawn.SetCanSlide");
    if (canSlide.ok()) { bool bTrue = true; if (canSlide.set("bCanSlide", bTrue)) canSlide.run(session.pawn); }
    SetBool(session.pawn, "/Script/FortniteGame.FortPawn.bWantsToSlide", true);
    void* movement = GetObjField(session.pawn, "/Script/Engine.Character.CharacterMovement");
    if (movement && !IsBadPtr(movement)) SetBool(movement, "/Script/Engine.NavMovementComponent.bCanSwim", true);
    void* slideAbility = LoadClass(
        "/Game/Abilities/Player/Generic/Traits/DefaultPlayer/GA_Athena_Player_Slide.GA_Athena_Player_Slide_C",
        "/Script/GameplayAbilities.GameplayAbility");
    if (slideAbility) GiveAbilityOnly(slideAbility);
    session.movement_enabled = true;
}
void Athena::SetupTacticalSprint() {
    if (!session.controller) return;
    if (session.tactical_sprint_setup && session.tactical_sprint) return;
    void* compClass = FindObj("/Script/TacticalSprintRuntime.FortPlayerControllerComponent_TacticalSprint");
    if (!compClass) { session.tactical_sprint_setup = true; return; }
    void* comp = nullptr;
    Call get("/Script/Engine.Actor.GetComponentByClass");
    if (get.ok() && get.set("ComponentClass", compClass) && get.run(session.controller)) comp = get.result<void*>();
    if (!comp || IsBadPtr(comp)) {
        Call add("/Script/Engine.Actor.AddComponentByClass");
        if (add.ok()) {
            bool bManual = false; Transform t{}; bool bDeferred = false;
            add.set("Class", compClass); add.set("bManualAttachment", bManual);
            add.set("RelativeTransform", t); add.set("bDeferredFinish", bDeferred);
            if (add.run(session.controller)) comp = add.result<void*>();
        }
    }
    if (!comp || IsBadPtr(comp)) { session.tactical_sprint_setup = true; return; }
    session.tactical_sprint = comp;
    uint8_t* flags = (uint8_t*)comp + profile::TacticalSprint_Flags;
    *flags |= profile::TacticalSprint_Bit_Enabled;
    session.tactical_sprint_setup = true;
    LOG("[Sprint] Component setup");
}
void Athena::ApplySprintSpeed() {
    if (!session.pawn) return;
    void* cmc = GetObjField(session.pawn, "/Script/Engine.Character.CharacterMovement");
    if (cmc && !IsBadPtr(cmc)) {
        void* prop = GetProp("/Script/Engine.CharacterMovementComponent.MaxWalkSpeed");
        if (prop) { float* val = (float*)GetField(cmc, prop); if (val && std::isfinite(*val) && *val < 500) *val = 600.f; }
    }
    session.sprint_applied = true;
}

// ============================================================================
// PLAYLIST + GMM
// ============================================================================
void Athena::EnablePlaylist(void* gs) {
    if (session.playlist_loaded) return;
    if (!gs || IsBadPtr(gs)) return;
    session.playlist_loaded = true;
    void* playlist = FindObj("/Game/Athena/Playlists/Playlist_DefaultSolo.Playlist_DefaultSolo");
    if (!playlist) playlist = LoadAsset("/Game/Athena/Playlists/Playlist_DefaultSolo.Playlist_DefaultSolo",
                                        "/Script/FortniteGame.FortPlaylistAthena");
    if (!playlist) { LOG("[Playlist] ❌ not found"); return; }
    void* cpProp = GetProp("/Script/FortniteGame.FortGameStateAthena.CurrentPlaylistData");
    if (cpProp) Write<void*>(GetField(gs, cpProp), 0, playlist);
    void* cpiProp = GetProp("/Script/FortniteGame.FortGameStateAthena.CurrentPlaylistInfo");
    if (cpiProp) {
        void* cpi = GetField(gs, cpiProp);
        void* bpProp = GetProp("/Script/FortniteGame.FortPlaylistPropertyArray.BasePlaylist");
        if (bpProp && cpi) Write<void*>(GetField(cpi, bpProp), 0, playlist);
    }
    Call a("/Script/FortniteGame.FortGameStateAthena.OnRep_CurrentPlaylistData");
    if (a.ok()) a.run(gs);
    Call b("/Script/FortniteGame.FortGameStateAthena.OnRep_CurrentPlaylistInfo");
    if (b.ok()) b.run(gs);
    LOG("[Playlist] ✅ Done");
}
void Athena::SetupGameModeManagers(void* gm, void* gs) {
    if (session.managers_setup || !gm || !gs) return;
    session.managers_setup = true;
    void* wb = FindObj("/Script/Engine.Default__GameplayStatics");
    if (!wb) return;
    void* aidCls = FindObj("/Game/AIDirector/AIDirector_Fortnite.AIDirector_Fortnite_C");
    if (!aidCls) aidCls = FindObj("/Script/FortniteGame.FortAIDirector");
    if (aidCls) {
        Call spawn("/Script/Engine.GameplayStatics.SpawnObject");
        if (spawn.ok()) {
            spawn.set("ObjectClass", aidCls); spawn.set("Outer", gm);
            if (spawn.run(wb)) {
                void* aid = spawn.result<void*>();
                if (aid) { Call act("/Script/FortniteGame.FortAIDirector.Activate"); if (act.ok()) act.run(aid); }
                LOG("[GMM] AIDirector=%p", aid);
            }
        }
    }
    void* spmCls = FindObj("/Script/FortniteGame.FortAthenaSpawningPolicyManager");
    if (spmCls && session.world) {
        Transform t{};
        void* spm = SpawnActor(session.world, spmCls, gm, t);
        if (spm) {
            void* p1 = GetProp("/Script/FortniteGame.FortAthenaSpawningPolicyManager.GameStateAthena");
            if (p1) Write<void*>(GetField(spm, p1), 0, gs);
            void* p2 = GetProp("/Script/FortniteGame.FortAthenaSpawningPolicyManager.GameModeAthena");
            if (p2) Write<void*>(GetField(spm, p2), 0, gm);
        }
    }
    LOG("[GMM] Done");
}
void Athena::HandleReadyToStartMatch(void* gm) {
    if (session.gm_hook_fired) return;
    session.gm_hook_fired = true;
    LOG("[GM-Hook] ReadyToStartMatch GM=%p", gm);
    if (!session.mode) session.mode = gm;
    void* gs = nullptr;
    void* gsProp = GetProp("/Script/FortniteGame.FortGameMode.GameState");
    if (gsProp) gs = Read<void*>(GetField(gm, gsProp));
    if (!gs && session.state) gs = session.state;
    if (!gs) { LOG("[GM-Hook] no GS"); return; }
    if (!session.state) session.state = gs;
    EnablePlaylist(gs);
    SetupGameModeManagers(gm, gs);
}

// ============================================================================
// VEHICLES + WILDLIFE
// ============================================================================
void Athena::SpawnDefaultVehicles() {
    if (session.vehicles_attempted || !session.world) return;
    session.vehicles_attempted = true;
    void* wb = FindObj("/Script/Engine.Default__GameplayStatics");
    if (!wb) return;
    void* spawnerCls = FindObj("/Script/FortniteGame.FortAthenaVehicleSpawner");
    if (!spawnerCls) { LOG("[Veh] no class"); return; }
    Call gac("/Script/Engine.GameplayStatics.GetAllActorsOfClass");
    if (!gac.ok()) return;
    gac.set("WorldContextObject", session.world);
    gac.set("ActorClass", spawnerCls);
    if (!gac.run(wb)) return;
    Array arr = gac.result<Array>();
    LOG("[Veh] Found %d spawners", arr.Count);
}
void Athena::EnableWildlife() {
    if (session.wildlife_attempted || !session.world) return;
    session.wildlife_attempted = true;
    LOG("[Wildlife] Attempting spawn near player...");
    Call gl("/Script/Engine.Actor.K2_GetActorLocation");
    Vector base{0, 0, 500};
    if (session.pawn && gl.ok() && gl.run(session.pawn)) { base = gl.result<Vector>(); base.Z = 500; }
    const char* paths[] = {
        "/Game/Athena/AI/Wildlife/Wolf/B_Athena_Wolf.B_Athena_Wolf_C",
        "/Game/Athena/AI/Wildlife/Boar/B_Athena_Boar.B_Athena_Boar_C",
        "/Game/Athena/AI/Wildlife/Chicken/B_Athena_Chicken.B_Athena_Chicken_C",
        nullptr
    };
    for (int i = 0; paths[i]; ++i) {
        void* cls = LoadClass(paths[i], "/Script/FortniteGame.FortAIPawn");
        if (!cls) { LOG("[Wildlife] missing: %s", paths[i]); continue; }
        Vector loc = base; loc.X += 400 + i * 250;
        void* aip = SpawnActorLoc(session.world, cls, nullptr, loc);
        if (aip) {
            Call sdc("/Script/Engine.Pawn.SpawnDefaultController");
            if (sdc.ok()) sdc.run(aip);
            LOG("[Wildlife] ✅ Spawned %s", paths[i]);
        }
    }
}

// ============================================================================
// HUD
// ============================================================================
void Athena::SetupHUD() {
    if (!session.controller || session.hud_created) return;
    session.hud_created = true;
    void* hudCls = FindObj("/Game/Athena/HUD/AthenaHUD.AthenaHUD_C");
    if (!hudCls) hudCls = LoadClass("/Game/Athena/HUD/AthenaHUD.AthenaHUD_C", "/Script/Engine.HUD");
    if (!hudCls) return;
    Call csh("/Script/Engine.PlayerController.ClientSetHUD");
    if (csh.ok() && csh.set("NewHUDClass", hudCls)) {
        csh.run(session.controller);
        LOG("[HUD] ClientSetHUD done");
    }
}

// ============================================================================
// COSMETICS
// ============================================================================
void Athena::LoadDefaultCosmetics() {
    if (!session.pawn || !session.controller) return;
    const char* partPaths[] = {
        "/Game/Athena/Heroes/Meshes/Heads/F_Med_Head1_ATH.F_Med_Head1_ATH",
        "/Game/Athena/Heroes/Meshes/Bodies/CP_001_Athena_Body.CP_001_Athena_Body",
    };
    void* loaded[2]{};
    for (int i = 0; i < 2; ++i) {
        void* asset = LoadAsset(partPaths[i], "/Script/FortniteGame.CustomCharacterPart");
        if (!asset) continue;
        loaded[i] = asset;
        Call choose("/Script/FortniteGame.FortPlayerPawn.ServerChoosePart");
        if (choose.ok() && choose.set("Part", (uint8_t)i) && choose.set("ChosenCharacterPart", asset))
            choose.run(session.pawn);
    }
    void* state = GetObjField(session.controller, "/Script/Engine.Controller.PlayerState");
    if (!state) return;
    if (loaded[0] && loaded[1]) {
        Array partsArr{loaded, 2, 2};
        Call apply("/Script/FortniteGame.FortKismetLibrary.ApplyCharacterCosmetics");
        if (apply.ok() && apply.set("WorldContextObject", session.world) &&
            apply.set("CharacterParts", partsArr) && apply.set("PlayerState", state)) {
            void* klib = FindObj("/Script/FortniteGame.Default__FortKismetLibrary");
            if (klib) apply.run(klib);
        }
    }
    Call r1("/Script/FortniteGame.FortPlayerState.OnRep_HeroType"); if (r1.ok()) r1.run(state);
    Call r2("/Script/FortniteGame.FortPlayerState.OnRep_CharacterParts"); if (r2.ok()) r2.run(state);
    Call r3("/Script/FortniteGame.FortPlayerState.OnRep_CharacterData"); if (r3.ok()) r3.run(state);
    Call cl("/Script/FortniteGame.FortPlayerPawn.OnRep_CustomizationLoadout");
    if (cl.ok()) cl.run(session.pawn);
}
void Athena::ShowSkin() { LoadDefaultCosmetics(); }
void Athena::ForcePlayerVisible() {
    if (!session.pawn) return;
    SetBool(session.pawn, "/Script/FortniteGame.FortPlayerPawn.bIsSkydiving", false);
    SetBool(session.pawn, "/Script/FortniteGame.FortPlayerPawn.bIsParachuteOpen", false);
    SetBool(session.pawn, "/Script/FortniteGame.FortPlayerPawn.bIsParachuteAvailable", true);
    Call sd("/Script/FortniteGame.FortPlayerPawn.OnRep_IsSkydiving"); if (sd.ok()) sd.run(session.pawn);
    Call po("/Script/FortniteGame.FortPlayerPawn.OnRep_IsParachuteOpen"); if (po.ok()) po.run(session.pawn);
    Call hid("/Script/Engine.Actor.SetActorHiddenInGame");
    if (hid.ok()) { bool bHidden = false; if (hid.set("bNewHidden", bHidden)) hid.run(session.pawn); }
    SetBool(session.pawn, "/Script/Engine.Actor.bHidden", false);
    ShowSkin();
    session.visibility_forced = true;
}
bool Athena::TryInitializePlayer(void* world, void* mode, void* controller) {
    void* pawnCls = LoadClass("/Game/Athena/PlayerPawn_Athena.PlayerPawn_Athena_C", "/Script/Engine.Pawn");
    if (!pawnCls) return false;
    void* existing = GetPawn(controller);
    if (existing && !IsBadPtr(existing) && IsA(existing, pawnCls)) { session.pawn = existing; return true; }
    Transform t{};
    t.Location[2] = 3100.0;
    session.pawn = SpawnActor(world, pawnCls, controller, t);
    if (!session.pawn) return false;
    SetBool(session.pawn, "/Script/FortniteGame.FortPlayerPawn.bIsSkydiving", false);
    Possess(controller, session.pawn);
    if (GetPawn(controller) != session.pawn) return false;
    SetupCheats();
    ForcePlayerVisible();
    return true;
}

// ============================================================================
// INVENTORY — SAFE version
// ============================================================================
void Athena::InventoryUpdate() {
    // ForceNetUpdate only — never HandleInventoryLocalUpdate (it crashes on
    // entries we added manually because their ReplicationIDs are 0, not -1)
    if (session.inventory && !IsBadPtr(session.inventory)) {
        Call c("/Script/Engine.Actor.ForceNetUpdate");
        if (c.ok()) c.run(session.inventory);
    }
}

bool Athena::ReadInventoryPointers() {
    if (!session.controller) return false;
    void* invProp = GetProp("/Script/FortniteGame.FortPlayerController.WorldInventory");
    if (!invProp) { LOG("[Inv] no invProp"); return false; }
    session.inventory = Read<void*>(GetField(session.controller, invProp));
    if (!session.inventory || IsBadPtr(session.inventory)) { LOG("[Inv] WorldInventory null"); return false; }
    LOG("[Inv] inventory=%p", session.inventory);
    void* qbProp = GetProp("/Script/FortniteGame.FortPlayerController.ClientQuickBars");
    if (qbProp) {
        session.quickbars = Read<void*>(GetField(session.controller, qbProp));
        if (!session.quickbars) LOG("[QB] not ready");
        else LOG("[QB] quickbars=%p", session.quickbars);
    }
    session.inventory_ready = true;
    return true;
}

// Add item — Erbium-style with GUID generation
void Athena::AddToInventory(void* def, int count, int slot) {
    if (IsBadPtr(def)) return;
    if (!session.inventory || IsBadPtr(session.inventory)) return;

    void* inst = nullptr;
    {
        Call c("/Script/FortniteGame.FortItemDefinition.CreateTemporaryItemInstanceBP");
        int lvl = 1;
        if (!c.ok() || !c.set("Count", count) || !c.set("Level", lvl) || !c.run(def)) return;
        inst = c.result<void*>();
    }
    if (!inst || IsBadPtr(inst)) return;

    Call oc("/Script/FortniteGame.FortItem.SetOwningControllerForTemporaryItem");
    if (oc.ok() && oc.set("InController", session.controller)) oc.run(inst);

    void* invProp = GetProp("/Script/FortniteGame.FortInventory.Inventory");
    void* instProp = GetProp("/Script/FortniteGame.FortItemList.ItemInstances");
    void* entryProp = GetProp("/Script/FortniteGame.FortItemList.ReplicatedEntries");
    void* itemEntryProp = GetProp("/Script/FortniteGame.FortWorldItem.ItemEntry");
    if (!invProp || !instProp || !entryProp || !itemEntryProp) return;

    void* list = GetField(session.inventory, invProp);
    if (!list || IsBadPtr(list)) return;
    void* instArr = GetField(list, instProp);
    void* entryArr = GetField(list, entryProp);
    if (!instArr || !entryArr) return;
    void* entrySrc = GetField(inst, itemEntryProp);
    if (!entrySrc || IsBadPtr(entrySrc)) return;

    // Set FastArray fields
    Write<int32>(entrySrc, profile::Entry_ReplicationID, -1);
    Write<int32>(entrySrc, profile::Entry_ReplicationKey, -1);
    Write<int32>(entrySrc, profile::Entry_MostRecentArrayKey, -1);

    // Generate GUID via KismetGuidLibrary
    void* guidProp = GetProp("/Script/FortniteGame.FortItemEntry.ItemGuid");
    if (guidProp) {
        Guid* gd = (Guid*)GetField(entrySrc, guidProp);
        if (gd && !IsBadPtr(gd) && !(gd->A|gd->B|gd->C|gd->D)) {
            void* klib = FindObj("/Script/Engine.Default__KismetGuidLibrary");
            if (klib) {
                Call g("/Script/Engine.KismetGuidLibrary.NewGuid");
                if (g.ok() && g.run(klib)) {
                    Guid fresh = g.result<Guid>();
                    *gd = fresh;
                }
            }
        }
    }

    // TrackerGuid
    void* trackerProp = GetProp("/Script/FortniteGame.FortItemEntry.TrackerGuid");
    if (trackerProp) {
        Guid* tg = (Guid*)GetField(entrySrc, trackerProp);
        if (tg && !IsBadPtr(tg) && !(tg->A|tg->B|tg->C|tg->D)) {
            void* klib = FindObj("/Script/Engine.Default__KismetGuidLibrary");
            if (klib) {
                Call g("/Script/Engine.KismetGuidLibrary.NewGuid");
                if (g.ok() && g.run(klib)) {
                    Guid fresh = g.result<Guid>();
                    *tg = fresh;
                }
            }
        }
    }

    // ParentInventory
    void* parentProp = GetProp("/Script/FortniteGame.FortItemEntry.ParentInventory");
    if (parentProp) Write<void*>(GetField(entrySrc, parentProp), 0, session.inventory);

    // Durability
    void* duraProp = GetProp("/Script/FortniteGame.FortItemEntry.Durability");
    if (duraProp) {
        float* d = (float*)GetField(entrySrc, duraProp);
        if (d) *d = 1.0f;
    }

    // LoadedAmmo
    void* ammoProp = GetProp("/Script/FortniteGame.FortItemEntry.LoadedAmmo");
    if (ammoProp) {
        int32* a = (int32*)GetField(entrySrc, ammoProp);
        if (a) *a = 30;
    }

    // Add to arrays
    Fn<int(*)(void*,void*,const void*)>(profile::GenericArrayAdd)(instArr, instProp, &inst);
    Fn<int(*)(void*,void*,const void*)>(profile::GenericArrayAdd)(entryArr, entryProp, entrySrc);
    if (slot >= 0 && slot < 6) session.slots[slot] = inst;
}

// Populate — Erbium-style default items
void Athena::PopulateInventory() {
    if (session.inventory_populated) return;
    session.inventory_populated = true;
    LOG("[Inv] Populating...");

    struct DefSlot { const char* path; int slot; };
    DefSlot defaults[] = {
        {"/Game/Athena/Items/Weapons/WID_Harvest_Pickaxe_Athena_C_T01.WID_Harvest_Pickaxe_Athena_C_T01", 0},
        {"/Game/Items/Weapons/BuildingTools/BuildingItemData_Wall.BuildingItemData_Wall", 1},
        {"/Game/Items/Weapons/BuildingTools/BuildingItemData_Floor.BuildingItemData_Floor", 2},
        {"/Game/Items/Weapons/BuildingTools/BuildingItemData_Stair_W.BuildingItemData_Stair_W", 3},
        {"/Game/Items/Weapons/BuildingTools/BuildingItemData_RoofS.BuildingItemData_RoofS", 4},
    };
    for (auto& d : defaults) {
        void* def = LoadAsset(d.path);
        if (!def) def = FindObj(d.path);
        if (!def) { LOG("[Inv] missing: %s", d.path); continue; }
        AddToInventory(def, 1, d.slot);
        LOG("[Inv+] %s", d.path);
    }

    // Edit tool (not in slots)
    void* editTool = LoadAsset("/Game/Items/Weapons/BuildingTools/EditTool.EditTool");
    if (editTool) {
        session.edit_tool = editTool;
        LOG("[Inv+] EditTool");
    }

    // Ammo
    const char* ammo[] = {
        "/Game/Athena/Items/Ammo/AthenaAmmoDataBulletsLight.AthenaAmmoDataBulletsLight",
        "/Game/Athena/Items/Ammo/AthenaAmmoDataBulletsMedium.AthenaAmmoDataBulletsMedium",
        "/Game/Athena/Items/Ammo/AthenaAmmoDataBulletsHeavy.AthenaAmmoDataBulletsHeavy",
        "/Game/Athena/Items/Ammo/AthenaAmmoDataShells.AthenaAmmoDataShells",
        nullptr};
    for (int i = 0; ammo[i]; ++i) {
        void* def = LoadAsset(ammo[i]);
        if (def) { AddToInventory(def, 999, 10); LOG("[Inv+] ammo %d", i); }
    }

    // Resources
    struct ResDef { const char* path; int slot; };
    ResDef resources[] = {
        {"/Game/Items/ResourcePickups/WoodItemData.WoodItemData", 10},
        {"/Game/Items/ResourcePickups/StoneItemData.StoneItemData", 10},
        {"/Game/Items/ResourcePickups/MetalItemData.MetalItemData", 10},
    };
    for (auto& r : resources) {
        void* def = LoadAsset(r.path);
        if (def) { AddToInventory(def, 500, r.slot); LOG("[Inv+] resource %s", r.path); }
    }

    InventoryUpdate();
    LOG("[Inv] ✅ Populated");
}

void Athena::ActivateQuickBarSlot(uint8_t bar, int slot) {
    if (!session.controller) return;
    Call act("/Script/FortniteGame.FortPlayerController.ClientActivateSlot");
    if (!act.ok()) return;
    float delay = 0.f; bool bUpdatePrev = true; bool bForce = true;
    if (!act.set("InQuickBar", bar) || !act.set("Slot", slot) || !act.set("ActivateDelay", delay) ||
        !act.set("bUpdatePreviousFocusedSlot", bUpdatePrev) || !act.set("bForceExecution", bForce)) return;
    act.run(session.controller);
}
void Athena::ForceUpdateQuickBars() {
    if (!session.controller) return;
    for (uint8_t bar : {(uint8_t)0, (uint8_t)1}) {
        Call c("/Script/FortniteGame.FortPlayerController.ClientForceUpdateQuickbar");
        if (c.ok() && c.set("QuickbarToRefresh", bar)) c.run(session.controller);
    }
}
void Athena::EquipSlot(int slot) {
    if (slot < 0 || slot >= 6 || !session.slots[slot]) return;
    void* item = session.slots[slot];
    if (IsBadPtr(item)) return;
    void* def = nullptr;
    Call d("/Script/FortniteGame.FortItem.GetItemDefinitionBP");
    if (d.ok() && d.run(item)) def = d.result<void*>();
    if (!def || IsBadPtr(def)) return;
    Guid guid{};
    Call g("/Script/FortniteGame.FortItem.GetItemGuid");
    if (g.ok() && g.run(item)) guid = g.result<Guid>();
    Call eq("/Script/FortniteGame.FortPawn.EquipWeaponDefinition");
    if (!eq.ok()) return;
    bool bDisableAnim = false;
    Guid trackerGuid{};
    eq.set("WeaponData", def);
    eq.set("ItemEntryGuid", guid);
    eq.set("TrackerGuid", trackerGuid);
    eq.set("bDisableEquipAnimation", bDisableAnim);
    eq.run(session.pawn);
}

// ============================================================================
// LOOT + VENDING
// ============================================================================
void Athena::Loot(void* actor) {
    if (!actor || !session.pawn) return;
    Call spawn("/Script/FortniteGame.BuildingContainer.BP_SpawnLoot");
    if (spawn.ok() && spawn.set("PlayerPawn", session.pawn)) spawn.run(actor);
    SetBool(actor, "/Script/FortniteGame.BuildingContainer.bAlreadySearched", true);
    Call r1("/Script/FortniteGame.BuildingContainer.OnRep_bAlreadySearched"); if (r1.ok()) r1.run(actor);
    Call r2("/Script/FortniteGame.BuildingContainer.BounceContainer"); if (r2.ok()) r2.run(actor);
}
void Athena::UseVendingMachine(void* machine) {
    if (!machine || !session.controller) return;
    Write<void*>(machine, profile::Vending_ControllingPlayer, session.controller);
    Call boi("/Script/FortniteGame.BuildingItemCollectorActor.BlueprintOnInteract");
    if (boi.ok()) { boi.set("InteractingPawn", session.pawn); boi.run(machine); }
    Call sv("/Script/FortniteGame.BuildingItemCollectorActor.StartVend");
    if (sv.ok()) sv.run(machine);
}
void Athena::ServerHandlePickup(void* pickup) {
    if (!pickup || !session.inventory) return;
    void* defProp = GetProp("/Script/FortniteGame.FortPickup.ItemDefinition");
    void* cntProp = GetProp("/Script/FortniteGame.FortPickup.Count");
    void* def = defProp ? Read<void*>(GetField(pickup, defProp)) : nullptr;
    int count = cntProp ? Read<int>(GetField(pickup, cntProp)) : 1;
    if (!def) return;
    AddToInventory(def, count, 10);
    Call d("/Script/Engine.Actor.K2_DestroyActor");
    if (d.ok()) d.run(pickup);
}
void Athena::OnServerExecuteInventoryItem(Guid guid) {
    if (!session.inventory || !session.pawn) return;
    void* instProp = GetProp("/Script/FortniteGame.FortItemList.ItemInstances");
    void* invProp  = GetProp("/Script/FortniteGame.FortInventory.Inventory");
    if (!instProp || !invProp) return;
    void* list = GetField(session.inventory, invProp);
    Array arr = Read<Array>(GetField(list, instProp));
    for (int i = 0; i < arr.Count; ++i) {
        void* inst = Read<void*>(arr.Data, (size_t)i * 8);
        if (!inst) continue;
        Call g("/Script/FortniteGame.FortItem.GetItemGuid");
        if (!g.ok() || !g.run(inst)) continue;
        Guid cand = g.result<Guid>();
        if (memcmp(&cand, &guid, sizeof(Guid)) == 0) {
            for (int s = 0; s < 6; ++s) if (session.slots[s] == inst) { EquipSlot(s); return; }
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
void Athena::DropInventoryItem(Guid guid, int count) {
    if (!session.inventory) return;
    void* invProp = GetProp("/Script/FortniteGame.FortInventory.Inventory");
    void* instProp = GetProp("/Script/FortniteGame.FortItemList.ItemInstances");
    void* entryProp = GetProp("/Script/FortniteGame.FortItemList.ReplicatedEntries");
    if (!invProp || !instProp || !entryProp) return;
    void* list = GetField(session.inventory, invProp);
    void* instArr = GetField(list, instProp);
    void* entryArr = GetField(list, entryProp);
    Array arr = Read<Array>(instArr);
    for (int i = 0; i < arr.Count; ++i) {
        void* inst = Read<void*>(arr.Data, (size_t)i * 8);
        if (!inst) continue;
        Call g("/Script/FortniteGame.FortItem.GetItemGuid");
        if (!g.ok() || !g.run(inst)) continue;
        Guid cand = g.result<Guid>();
        if (memcmp(&cand, &guid, sizeof(Guid)) == 0) {
            Fn<void(*)(void*,void*,int)>(profile::GenericArrayRemove)(instArr, instProp, i);
            Fn<void(*)(void*,void*,int)>(profile::GenericArrayRemove)(entryArr, entryProp, i);
            return;
        }
    }
}

// ============================================================================
// BUILDING + DOORS + EMOTES
// ============================================================================
void Athena::OnServerCreateBuildingActor(void* params) {
    if (!params) return;
    struct OldP { void* Class; uint8 _p[0x8]; Vector Loc; Rotator Rot; };
    struct NewP { int32 ClassHandle; Vector Loc; Rotator Rot; uint8 _p[0x10]; void* Class; };
    void* cls = nullptr; Vector loc{}; Rotator rot{};
    void* fn = FindObj("/Script/FortniteGame.FortPlayerController.ServerCreateBuildingActor");
    if (fn && Read<uint16_t>(fn, profile::FunctionParamSize) == sizeof(void*) + 0x8 + 24 + 24) {
        auto p = *(OldP*)params; cls = p.Class; loc = p.Loc; rot = p.Rot;
    } else {
        auto p = *(NewP*)params; cls = p.Class; loc = p.Loc; rot = p.Rot;
    }
    if (!cls) return;
    Transform t{};
    t.Location[0] = loc.X; t.Location[1] = loc.Y; t.Location[2] = loc.Z;
    void* actor = SpawnActor(session.world, cls, session.controller, t);
    if (!actor) return;
    Call init("/Script/FortniteGame.BuildingActor.InitializeKismetSpawnedBuildingActor");
    if (init.ok()) {
        init.set("BuildingOwner", actor);
        init.set("SpawningController", session.controller);
        bool bUse = true;
        init.set("bUsePlayerBuildAnimations", bUse);
        init.run(actor);
    }
}
void Athena::OnBeginEditActor(void* piece) {
    if (!session.edit_tool || !session.pawn || !piece) return;
    Call c("/Script/FortniteGame.FortItemDefinition.CreateTemporaryItemInstanceBP");
    int cnt = 1, lvl = 1;
    if (!c.ok() || !c.set("Count", cnt) || !c.set("Level", lvl) || !c.run(session.edit_tool)) return;
    void* inst = c.result<void*>();
    if (!inst) return;
    Call g("/Script/FortniteGame.FortItem.GetItemGuid");
    Guid guid{};
    if (g.ok() && g.run(inst)) guid = g.result<Guid>();
    Call eq("/Script/FortniteGame.FortPawn.EquipWeaponDefinition");
    if (!eq.ok()) return;
    bool bd = false; Guid tg{};
    eq.set("WeaponData", session.edit_tool); eq.set("ItemEntryGuid", guid);
    eq.set("TrackerGuid", tg); eq.set("bDisableEquipAnimation", bd);
    if (!eq.run(session.pawn)) return;
    void* editActor = eq.result<void*>();
    if (!editActor) return;
    void* p = GetProp("/Script/FortniteGame.FortWeapon.EditActor");
    if (p) Write<void*>(GetField(editActor, p), 0, piece);
    Call r("/Script/FortniteGame.FortWeapon.OnRep_EditActor");
    if (r.ok()) r.run(editActor);
}
void Athena::OnFinishEditActor(void* actor, void* newClass, int rotIter, bool mirrored) {
    if (!actor) return;
    SetBool(actor, "/Script/FortniteGame.BuildingActor.bPlayDestructionEffects", false);
    Vector zero{0,0,0};
    Call s("/Script/Engine.Actor.SetActorScale3D");
    if (s.ok() && s.set("NewScale3D", zero)) s.run(actor);
    Call d("/Script/Engine.Actor.K2_DestroyActor");
    if (d.ok()) d.run(actor);
    if (!newClass) return;
    Transform t{};
    void* edited = SpawnActor(session.world, newClass, session.controller, t);
    if (!edited) return;
    Call m("/Script/Engine.Actor.SetMirrored");
    if (m.ok() && m.set("bNewMirrored", mirrored)) m.run(edited);
    Call init("/Script/FortniteGame.BuildingActor.InitializeKismetSpawnedBuildingActor");
    if (init.ok()) {
        init.set("BuildingOwner", edited);
        init.set("SpawningController", session.controller);
        bool bUse = true;
        init.set("bUsePlayerBuildAnimations", bUse);
        init.run(edited);
    }
}
void Athena::HandleDoor(void* door) {
    if (!door || IsBadPtr(door)) return;
    static void* wallCls = nullptr;
    if (!wallCls) wallCls = FindObj("/Script/FortniteGame.BuildingWall");
    if (!wallCls || !IsA(door, wallCls)) return;
    uint8_t* byte = (uint8_t*)door + profile::BuildingWall_bDoorOpen_Byte;
    bool wasOpen = ((*byte) >> profile::BuildingWall_bDoorOpen_Bit) & 1;
    if (wasOpen) *byte &= ~(1 << profile::BuildingWall_bDoorOpen_Bit);
    else         *byte |=  (1 << profile::BuildingWall_bDoorOpen_Bit);
    Call r("/Script/FortniteGame.BuildingWall.OnRep_bDoorOpen"); if (r.ok()) r.run(door);
    Call netup("/Script/Engine.Actor.ForceNetUpdate"); if (netup.ok()) netup.run(door);
}
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
    if (ref.ok()) { ref.set("Type", (char)6); ref.set("Animation", (char)3); if (ref.run(item)) montage = ref.result<void*>(); }
    if (!montage) return;
    Call play("/Script/Engine.AnimInstance.Montage_Play");
    if (play.ok()) {
        play.set("MontageToPlay", montage); play.set("InPlayRate", 1.0f); play.set("InStartTime", 0.0f);
        bool bStop = true; play.set("bStopAllMontages", bStop); play.run(ai);
    }
    Write<void*>(session.pawn, profile::FortPawn_LastReplicatedEmoteExecuted, item);
    Call rep("/Script/FortniteGame.FortPawn.OnRep_LastReplicatedEmoteExecuted");
    if (rep.ok()) rep.run(session.pawn);
}
void Athena::OnExitVehicle() {
    if (!session.pawn) return;
    Call v("/Script/FortniteGame.FortPlayerPawn.ServerOnExitVehicle");
    if (v.ok()) { int32 ExitForce = 0; bool bDestroy = false; v.set("ExitForceBehavior", ExitForce); v.set("bDestroyVehicleWhenForced", bDestroy); v.run(session.pawn); }
}

// ============================================================================
// MISC
// ============================================================================
void Athena::DestroyLods() {
    void* cm = GetObjField(session.controller, "/Script/Engine.PlayerController.CheatManager");
    if (!cm) { SetupCheats(); cm = GetObjField(session.controller, "/Script/Engine.PlayerController.CheatManager"); }
    if (!cm) return;
    void* cls = FindObj("/Script/FortniteGame.FortHLODSMActor");
    if (!cls) return;
    Call c("/Script/Engine.CheatManager.DestroyAll");
    if (c.ok() && c.set("aClass", cls)) c.run(cm);
}
void Athena::SetupCheats() {
    void* pc = session.controller; if (!pc) return;
    void* p = GetProp("/Script/Engine.PlayerController.CheatManager");
    void* klass = FindObj("/Script/FortniteGame.FortCheatManager");
    if (!klass) klass = FindObj("/Script/Engine.CheatManager");
    if (!p || !klass) return;
    void* cheat = Read<void*>(GetField(pc, p));
    if (!cheat) { cheat = Fn<void*(*)(void*, void*)>(profile::SpawnObject)(klass, pc); if (cheat) Write<void*>(GetField(pc, p), 0, cheat); }
    if (cheat) { Call init("/Script/Engine.CheatManager.ReceiveInitCheatManager"); if (init.ok()) init.run(cheat); session.cheats_ready = true; }
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
    if (!session.ready_called) {
        Call srt("/Script/FortniteGame.FortPlayerController.ServerReadyToStartMatch"); if (srt.ok()) srt.run(session.controller);
        Call cps("/Script/FortniteGame.FortPlayerControllerZone.ClientOnPawnSpawned"); if (cps.ok()) cps.run(session.controller);
        session.ready_called = true;
    }
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
void Athena::SpawnPickup(void* def, int count, Vector loc, bool toss) {
    if (!def) return;
    void* klass = FindObj("/Script/FortniteGame.FortPickupAthena");
    if (!klass) return;
    Transform t{};
    t.Location[0] = loc.X; t.Location[1] = loc.Y; t.Location[2] = loc.Z;
    void* pickup = SpawnActor(session.world, klass, session.pawn, t);
    if (!pickup) return;
    void* defProp = GetProp("/Script/FortniteGame.FortPickup.ItemDefinition");
    void* cntProp = GetProp("/Script/FortniteGame.FortPickup.Count");
    if (defProp) Write<void*>(GetField(pickup, defProp), 0, def);
    if (cntProp) Write<int>(GetField(pickup, cntProp), 0, count);
}

// ============================================================================
// BUS + SAFEZONE
// ============================================================================
void Athena::SpawnBattleBus() {
    if (!session.mode || !session.state) return;
    void* gp = GetProp("/Script/FortniteGame.FortGameStateAthena.GamePhase");
    if (gp) {
        uint8_t phase = 3;
        Write<uint8_t>(GetField(session.state, gp), 0, phase);
        Call n("/Script/FortniteGame.FortGameStateAthena.OnRep_GamePhase");
        if (n.ok() && n.set("OldGamePhase", (uint8_t)2)) n.run(session.state);
    }
    if (session.pawn) {
        Vector loc{0, 0, 50000}; Rotator rot{0, 0, 0};
        Call sl("/Script/Engine.Actor.K2_SetActorLocationAndRotation");
        if (sl.ok()) {
            bool bSweep = false, bTeleport = true;
            sl.set("NewLocation", loc); sl.set("NewRotation", rot);
            sl.set("bSweep", bSweep); sl.set("bTeleport", bTeleport);
            sl.run(session.pawn);
        }
        BeginPlayerSkydiving();
    }
}
void Athena::HandleJumpFromHudStart() {
    if (!session.controller || !session.pawn || session.aircraft_jumped) return;
    Guard g;
    Call aircraft("/Script/FortniteGame.FortPlayerController.ServerAttemptAircraftJump");
    if (aircraft.ok()) { Rotator r{0,0,0}; if (aircraft.set("ClientRotation", r)) aircraft.run(session.controller); }
    Call sky("/Script/FortniteGame.FortPlayerPawn.BeginSkydiving");
    if (sky.ok()) { bool bFromBus = true; if (sky.set("bFromBus", bFromBus)) sky.run(session.pawn); }
    Write<uint8_t>(session.pawn, profile::FortPlayerPawn_bIsSkydiving, 1);
    session.aircraft_jumped = true;
}
void Athena::BeginPlayerSkydiving() {
    if (!session.pawn) return;
    Call bsd("/Script/FortniteGame.FortPlayerPawn.BeginSkydiving");
    if (bsd.ok()) { bool bFromBus = true; if (bsd.set("bFromBus", bFromBus)) bsd.run(session.pawn); }
    Write<uint8_t>(session.pawn, profile::FortPlayerPawn_bIsSkydiving, 1);
}
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

// ============================================================================
// SETUP WORLD + FINALIZE + TICK
// ============================================================================
void Athena::SetupWorld(void* world) {
    session.world = world;
    session.controller = GetPC(world, 0);
    if (!session.controller) return;
    void* baseCtrl = FindObj("/Script/FortniteGame.FortPlayerController");
    if (!baseCtrl || !IsA(session.controller, baseCtrl)) return;
    void* modeProp  = GetProp("/Script/Engine.World.AuthorityGameMode");
    void* stateProp = GetProp("/Script/Engine.World.GameState");
    session.mode  = modeProp  ? Read<void*>(GetField(world, modeProp))  : nullptr;
    session.state = stateProp ? Read<void*>(GetField(world, stateProp)) : nullptr;
    if (!session.mode || !session.state) return;
    LOG("SetupWorld: PC=%s GM=%s GS=%s", ObjName(session.controller).c_str(), ObjName(session.mode).c_str(), ObjName(session.state).c_str());

    if (!TryInitializePlayer(world, session.mode, session.controller)) return;

    LOG("SetupWorld: GrantAllAbilitySets"); GrantAllAbilitySets();
    LOG("SetupWorld: SetupTacticalSprint"); SetupTacticalSprint();
    LOG("SetupWorld: EnableMovementFeatures"); EnableMovementFeatures();
    LOG("SetupWorld: ShowSkin"); ShowSkin();
    LOG("SetupWorld: DestroyLods"); DestroyLods();
    LOG("SetupWorld: ApplySprintSpeed"); ApplySprintSpeed();
    LOG("SetupWorld: SpawnSafeZoneIndicator"); SpawnSafeZoneIndicator();
    LOG("SetupWorld: DropLoadingScreen"); DropLoadingScreen();
    LOG("SetupWorld: SpawnBattleBus"); SpawnBattleBus();
    LOG("SetupWorld: RemoveNetDebugUI"); RemoveNetDebugUI();
    LOG("SetupWorld: SpawnDefaultVehicles"); SpawnDefaultVehicles();
    LOG("SetupWorld: EnableWildlife"); EnableWildlife();
    LOG("SetupWorld: SetupHUD"); SetupHUD();
    RefreshHUD();

    session.finalize_at = Monotonic() + 3.0;
    session.attempted = true;
    session.initialized = true;
    LOG("SetupWorld: done");
}

void Athena::FinalizePlayer() {
    if (session.finalized) return;
    session.finalized = true;
    LOG("=== FinalizePlayer ===");

    if (!session.inventory_ready) ReadInventoryPointers();
    session.populate_at = Monotonic() + 2.0;
    LOG("Finalize: populate scheduled");

    void* pc = GetPC(session.world, 0);
    if (pc != session.controller) return;
    void* pawnCls = FindObj("/Script/FortniteGame.FortPlayerPawnAthena");
    void* pawn = GetPawn(pc);
    if (pawn && pawnCls && IsA(pawn, pawnCls)) session.pawn = pawn;

    if (!session.ready_called) {
        Call srt("/Script/FortniteGame.FortPlayerController.ServerReadyToStartMatch"); if (srt.ok()) srt.run(pc);
        Call cps("/Script/FortniteGame.FortPlayerControllerZone.ClientOnPawnSpawned"); if (cps.ok()) cps.run(pc);
        session.ready_called = true;
    }
    if (pawn) {
        Call restart("/Script/Engine.PlayerController.ClientRestart");
        if (restart.ok() && restart.set("NewPawn", pawn)) restart.run(pc);
        Call cam("/Script/Engine.PlayerController.SetViewTargetWithBlend");
        if (cam.ok() && cam.set("NewViewTarget", pawn)) cam.run(pc);
        session.visibility_forced = false;
        ForcePlayerVisible();
    }
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
    if (session.finalize_at > 0 && !session.finalized && now >= session.finalize_at) {
        FinalizePlayer();
        return;
    }
    if (session.finalized && !session.inventory_ready) {
        static double lastRead = 0;
        if (now - lastRead > 2.0) { lastRead = now; ReadInventoryPointers(); }
    }
    if (session.finalized && session.inventory_ready && !session.inventory_populated &&
        session.populate_at > 0 && now >= session.populate_at) {
        PopulateInventory();
    }
    if (session.inventory_populated && !session.equip_done) {
        static double lastEquip = 0;
        if (now - lastEquip > 1.0) {
            lastEquip = now;
            session.equip_done = true;
            ActivateQuickBarSlot(0, 0);
            EquipSlot(0);
            ForceUpdateQuickBars();
            LOG("Tick: equipped slot 0");
        }
    }
    static double lastVisibility = 0, lastSpeed = 0, lastAbility = 0;
    if (session.finalized && now - lastVisibility > 8.0) { lastVisibility = now; if (!session.visibility_forced) ForcePlayerVisible(); }
    if (session.finalized && session.pawn && now - lastSpeed > 2.0) { lastSpeed = now; ApplySprintSpeed(); }
    if (session.finalized && session.pawn && now - lastAbility > 10.0) {
        lastAbility = now;
        if (!session.abilities_granted) { GrantAllAbilitySets(); RefreshHUD(); }
    }
}

// ============================================================================
// HOOKS
// ============================================================================
static void* g_interactionClass = nullptr;
static void* g_containerClass = nullptr;
static void* g_vendingClass = nullptr;
static void* g_gameModeClass = nullptr;

static void pe_hook(void* obj, void* func, void* params) {
    if (!orig_PE) return;
    if (!g_internal && func && g_gameTid.load() == (int)gettid()) {
        if (obj && !session.gm_hook_fired) {
            if (!g_gameModeClass) g_gameModeClass = FindObj("/Script/FortniteGame.FortGameMode");
            if (g_gameModeClass) {
                void* objCls = Read<void*>(obj, profile::ObjectClass);
                if (SubclassOfFast(objCls, g_gameModeClass)) {
                    std::string fn = ObjName(func);
                    if (fn == "ReadyToStartMatch") {
                        Guard g; Athena::HandleReadyToStartMatch(obj);
                    }
                }
            }
        }
        if (obj && obj != session.controller && obj != session.pawn) {
            if (!g_containerClass) g_containerClass = FindObj("/Script/FortniteGame.BuildingContainer");
            if (!g_vendingClass)   g_vendingClass = FindObj("/Script/FortniteGame.BuildingItemCollectorActor");
            void* objCls = Read<void*>(obj, profile::ObjectClass);
            std::string fn = ObjName(func);
            if (g_containerClass && SubclassOfFast(objCls, g_containerClass)) {
                if (fn == "ServerOnAttemptInteract" || fn == "BlueprintOnInteract") { Guard g; Athena::Loot(obj); return; }
            }
            if (g_vendingClass && SubclassOfFast(objCls, g_vendingClass)) {
                if (fn == "ServerOnAttemptInteract" || fn == "BlueprintOnInteract") { Guard g; Athena::UseVendingMachine(obj); return; }
            }
        }
        if (obj == session.controller && params == nullptr) {
            std::string fn = ObjName(func);
            if (fn == "JumpFromHudStart") { Athena::HandleJumpFromHudStart(); return orig_PE(obj, func, params); }
        }
        if (!g_interactionClass) g_interactionClass = FindObj("/Script/FortniteGame.FortControllerComponent_Interaction");
        bool isPC   = (obj == session.controller);
        bool isPawn = (obj == session.pawn);
        bool isInteraction = false;
        if (g_interactionClass && obj) { void* cls = Read<void*>(obj, profile::ObjectClass); isInteraction = SubclassOfFast(cls, g_interactionClass); }
        if (isPC || isPawn || isInteraction) {
            Guard g;
            std::string n = ObjName(func);
            if (n == "ServerExecuteInventoryItem") {
                if (Read<uint16_t>(func, profile::FunctionParamSize) >= (int)sizeof(Guid))
                    Athena::OnServerExecuteInventoryItem(Read<Guid>(params));
            }
            else if (n == "ServerExecuteInventoryWeapon") Athena::OnServerExecuteInventoryWeapon(Read<void*>(params));
            else if (n == "ServerHandlePickupInfo" || n == "ServerHandlePickup") Athena::ServerHandlePickup(Read<void*>(params));
            else if (n == "ServerAttemptInteract" && params) {
                void* target = Read<void*>(params, 0);
                if (target && !IsBadPtr(target)) {
                    void* bcCls   = FindObj("/Script/FortniteGame.BuildingContainer");
                    void* wallCls = FindObj("/Script/FortniteGame.BuildingWall");
                    void* vendCls = FindObj("/Script/FortniteGame.BuildingItemCollectorActor");
                    void* targetCls = Read<void*>(target, profile::ObjectClass);
                    if (bcCls && SubclassOfFast(targetCls, bcCls)) Athena::Loot(target);
                    else if (vendCls && SubclassOfFast(targetCls, vendCls)) Athena::UseVendingMachine(target);
                    else if (wallCls && SubclassOfFast(targetCls, wallCls)) Athena::HandleDoor(target);
                }
            }
            else if (n == "ServerAttemptAircraftJump" && !session.aircraft_jumped) Athena::HandleJumpFromHudStart();
            else if (n == "ServerCreateBuildingActor" && params) Athena::OnServerCreateBuildingActor(params);
            else if (n == "ServerBeginEditingBuildingActor" && params) Athena::OnBeginEditActor(Read<void*>(params, 0));
            else if (n == "ServerEditBuildingActor" && params) {
                struct P { void* BA; void* NC; int RI; bool M; };
                auto* p = (P*)params;
                Athena::OnFinishEditActor(p->BA, p->NC, p->RI, p->M);
            }
            else if (n == "ServerPlayEmoteItem") Athena::PlayEmoteItem(Read<void*>(params));
            else if (n == "ServerAttemptExitVehicle") Athena::OnExitVehicle();
            else if (n == "ServerSpawnInventoryDrop" || n == "ServerAttemptInventoryDrop") {
                Guid gg = Read<Guid>(params);
                int cnt = Read<int>(params, sizeof(Guid));
                Athena::DropInventoryItem(gg, cnt);
            }
        }
    }
    orig_PE(obj, func, params);
}

static void tick_hook(void* world, int type, float delta) {
    if (orig_Tick) orig_Tick(world, type, delta);
    if (g_internal || !world) return;
    int expected = 0;
    if (g_gameTid.compare_exchange_strong(expected, (int)gettid())) LOG("WorldTick first: tid=%d", (int)gettid());
    if (g_gameTid.load() != (int)gettid()) return;
    Guard g;
    std::string wn = ObjName(world);
    if (wn.find("Frontend") != std::string::npos) {
        if (session.attempted && !session.finalized && session.finalize_at > 0) {
            double now = Monotonic();
            if (now >= session.finalize_at) Athena::FinalizePlayer();
        }
        return;
    }
    if (wn.find("Artemis_Terrain") == std::string::npos && wn.find("Athena_Terrain") == std::string::npos) return;
    int index = Read<int>(world, 0xC);
    if (session.world != world) {
        session = Session{};
        session.world = world;
        session.world_index = index;
        session.observed_at = Monotonic();
        LOG(">>> Terrain: %s", wn.c_str());
    }
    double now = Monotonic();
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
    if (normalized == "open athena_terrain" || normalized.rfind("open athena_terrain", 0) == 0) {
        session = Session{};
        session.attempted = false;
        return orig_Exec(engine, world, u"open Athena_Terrain", output);
    }
    return orig_Exec(engine, world, command, output);
}

static void InstallHooks() {
    LOG("=== InstallHooks ===");
    if (DobbyHook((void*)(g_base + profile::ProcessEvent), (void*)pe_hook, (void**)&orig_PE) != 0 || !orig_PE) { LOG("PE FAILED"); return; }
    if (DobbyHook((void*)(g_base + profile::WorldTick), (void*)tick_hook, (void**)&orig_Tick) != 0 || !orig_Tick) LOG("Tick FAILED");
    if (DobbyHook((void*)(g_base + profile::EngineExec), (void*)exec_hook, (void**)&orig_Exec) != 0 || !orig_Exec) LOG("Exec FAILED");
    LOG("All hooks installed");
}

static int InspectCb(dl_phdr_info* info, size_t, void*) {
    const char* name = strrchr(info->dlpi_name, '/');
    name = name ? name + 1 : info->dlpi_name;
    if (strcmp(name, "libUnreal.so")) return 0;
    g_base = info->dlpi_addr;
    g_status.store(1);
    LOG("base=%p", (void*)g_base);
    return 1;
}

static void* Worker(void*) {
    LOG("=== EraCH3 ===");
    for (int i = 0; i < 600 && g_status.load() == 0; ++i) {
        dl_iterate_phdr(InspectCb, nullptr);
        if (g_status.load() == 0) usleep(100000);
    }
    if (g_status.load() != 1) return nullptr;
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
