// ============================================================================
// EraCH3 — Fortnite 21.30 CL-21088273 Android
// COMPLETE FINAL v2 — Fixed: Drop, Loot, QuickBars, Melee, Doors, Mantling, HUD
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

// ============================================================================
// PROFILE
// ============================================================================
namespace profile {
// ─── Core engine ───
constexpr uintptr_t ProcessEvent        = 0x07611B5C;
constexpr uintptr_t StaticFindObject    = 0x0762A664;
constexpr uintptr_t StaticLoadObject    = 0x0761C568;
constexpr uintptr_t StaticLoadClass     = 0x0762CFB8;
constexpr uintptr_t NameAppendString    = 0x074937CC;
constexpr uintptr_t Free                = 0x07355370;
constexpr uintptr_t WorldTick           = 0x0949685C;
constexpr uintptr_t EngineExec          = 0x098BB160;
constexpr uintptr_t GObjects            = 0x0DCD6E08;
constexpr uintptr_t GenericArrayAdd     = 0x0944A668;
constexpr uintptr_t GenericArrayRemove  = 0x0944B2DC;
constexpr uintptr_t GetPlayerController = 0x093A1D48;
constexpr uintptr_t BeginSpawn          = 0x093A37E4;
constexpr uintptr_t FinishSpawn         = 0x0939F1B8;
constexpr uintptr_t SpawnObject         = 0x093A3388;
constexpr uintptr_t GetInterfaceAddress = 0x0761AA88;

// ─── Object layout ───
constexpr unsigned ObjectClass      = 0x10;
constexpr unsigned ObjectName       = 0x18;
constexpr unsigned ObjectOuter      = 0x20;
constexpr unsigned FieldNext        = 0x28;
constexpr unsigned StructSuper      = 0x40;
constexpr unsigned StructChildren   = 0x48;
constexpr unsigned StructChildProps = 0x50;
constexpr unsigned FunctionParamSize= 0xB6;
constexpr unsigned PropFlags        = 0x38;
constexpr unsigned PropOffset       = 0x44;
constexpr unsigned PropElementSize  = 0x34;
constexpr unsigned BoolSize         = 0x80;
constexpr unsigned BoolByteOffset   = 0x81;
constexpr unsigned BoolFieldMask    = 0x82;
constexpr unsigned FFieldName       = 0x28;
constexpr unsigned FFieldNext       = 0x20;

constexpr uint64_t CPF_Parm       = 0x00000080ULL;
constexpr uint64_t CPF_OutParm    = 0x00000100ULL;
constexpr uint64_t CPF_ConstParm  = 0x00000200ULL;
constexpr uint64_t CPF_ReturnParm = 0x00000400ULL;

// ─── ★ ORIG native addresses (from offsets-21.30.h) ───
constexpr uintptr_t ORIG_OpenActor                              = 0x05ec83f4;
constexpr uintptr_t ORIG_CloseActor                             = 0x05ec8468;
constexpr uintptr_t ORIG_IsDoorOpen                             = 0x05957c5c;
constexpr uintptr_t ORIG_SpawnLoot                              = 0x05414df4;
constexpr uintptr_t ORIG_BP_SpawnLoot                           = 0x0586f430;
constexpr uintptr_t ORIG_BP_IsAlreadySearched                   = 0x0586f448;
constexpr uintptr_t ORIG_BP_SetAlreadySearched                  = 0x0586f454;
constexpr uintptr_t ORIG_TossPickupFromContainer                = 0x05eb7d60;
constexpr uintptr_t ORIG_TossPickup                             = 0x060ef500;
constexpr uintptr_t ORIG_K2_RemoveItemFromPlayer                = 0x05ebcf74;
constexpr uintptr_t ORIG_K2_RemoveItemFromPlayerByGuid          = 0x05ebcfe4;
constexpr uintptr_t ORIG_HandleWorldInventoryLocalUpdate        = 0x06681258;
constexpr uintptr_t ORIG_CreateTemporaryItemInstanceBP          = 0x060d2d24;
constexpr uintptr_t ORIG_SetOwningControllerForTemporaryItem    = 0x060d1b74;
constexpr uintptr_t ORIG_ActivateQuickbarSlot                   = 0x05ec74b8;
constexpr uintptr_t ORIG_TryActivateAbilityByClass              = 0x0317ad5c;
constexpr uintptr_t ORIG_OnRep_ActivateAbilities                = 0x03165f88;
constexpr uintptr_t ORIG_K2_InitStats                           = 0x0315b730;
constexpr uintptr_t ORIG_BP_ApplyGameplayEffectToSelf           = 0x0315ce50;
constexpr uintptr_t ORIG_GetAbilitySystemComponentFromActorInfo = 0x0313d440;
constexpr uintptr_t ORIG_EquipFortAbilitySet                    = 0x051b27b4;
constexpr uintptr_t ORIG_UnequipFortAbilitySet                  = 0x05ec31c4;
constexpr uintptr_t ORIG_K2_GetResourceItemDefinition           = 0x05ec4ccc;
constexpr uintptr_t ORIG_ApplyCharacterCosmetics                = 0x05ec5db8;
constexpr uintptr_t ORIG_UpdatePlayerCustomCharacterParts       = 0x05ec7ca8;
constexpr uintptr_t ORIG_BeginSkydiving                         = 0x065d031c;
constexpr uintptr_t ORIG_SetMovementSpeedMultiplier             = 0x0656aca4;

// ─── ★ SDK offsets (verified from Dumper) ───
constexpr unsigned ItemEntry_LoadedAmmo           = 0x2C;
constexpr unsigned ItemEntry_PhantomReserveAmmo   = 0x30;
constexpr unsigned ItemEntry_ItemGuid             = 0x68;
constexpr unsigned ItemEntry_TrackerGuid          = 0x78;
constexpr unsigned ItemEntry_bIsReplicatedCopy    = 0x9E;
constexpr unsigned ItemEntry_bIsDirty             = 0x9F;
constexpr unsigned ItemEntry_StateValues          = 0xD0;
constexpr unsigned ItemEntry_ParentInventory      = 0xE0;
constexpr unsigned ItemEntry_Size                 = 0x1A0;

constexpr unsigned FortItemList_ReplicatedEntries = 0x0108;
constexpr unsigned FortItemList_ItemInstances     = 0x0168;

constexpr unsigned BuildingContainer_SearchLootTierGroup      = 0x0E20;
constexpr unsigned BuildingContainer_LootSpawnLocation        = 0x0F28;
constexpr unsigned BuildingContainer_LootSpawnLocation_Athena = 0x0F78;
constexpr unsigned BuildingContainer_LootTossDirection_Athena = 0x0F90;
constexpr unsigned BuildingContainer_LootTossSpeed_Athena     = 0x0FA8;
constexpr unsigned BuildingContainer_LootTossConeHalfAngle    = 0x0FAC;
constexpr unsigned BuildingContainer_bAlreadySearched_Byte    = 0x0FB1;
constexpr unsigned BuildingContainer_bAlreadySearched_Bit     = 7;
constexpr unsigned BuildingContainer_SearchBounceData         = 0x0FB8;
constexpr unsigned BuildingContainer_SearchAnimationCount     = 0x0FD0;

constexpr unsigned BuildingWall_bDoorOpen_Byte = 0x0FDE;
constexpr unsigned BuildingWall_bDoorOpen_Bit  = 5;

constexpr unsigned FortPlayerController_WorldInventory   = 0x1D20;
constexpr unsigned FortPlayerController_ClientQuickBars  = 0x1A60;
constexpr unsigned FortPlayerController_bHasInitializedWorldInventory_Byte = 0x1D84;
constexpr unsigned FortPlayerController_bHasInitializedWorldInventory_Bit  = 0;

constexpr unsigned FortPlayerPawn_bIsSkydiving     = 0x1A1D;
constexpr unsigned FortPlayerPawn_bIsParachuteOpen = 0x1A1E;
constexpr unsigned FortPlayerPawn_CurrentWeaponList= 0x0800;

constexpr unsigned FortPickup_PrimaryPickupItemEntry = 0x02F8;
constexpr unsigned FortPickup_PickupLocationData     = 0x04A8;
constexpr unsigned FortPickup_bPickedUp              = 0x0590;
constexpr unsigned FortPickup_PawnWhoDroppedPickup   = 0x05C8;

constexpr unsigned FortPlayerState_HeroType = 0x0430;

// ─── ★ MME AbilitySet paths (for mantling/climbing) ───
constexpr const char* AS_TacticalSprint     = "/TacticalSprintGame/Gameplay/AS_TacticalSprint.AS_TacticalSprint";
constexpr const char* AS_TacticalSprint_Alt = "/TacticalSprint/Gameplay/AS_TacticalSprint.AS_TacticalSprint";
constexpr const char* AS_Ascender           = "/Ascender/Gameplay/Ascender/AS_Ascender.AS_Ascender";
constexpr const char* AS_HillScramble       = "/HillScramble/Gameplay/AS_HillScramble.AS_HillScramble";
constexpr const char* AS_SlideImpulse       = "/SlideImpulse/Gameplay/AS_SlideImpulse.AS_SlideImpulse";
constexpr const char* AS_DoorBash           = "/DoorBashContent/Gameplay/AS_DoorBash.AS_DoorBash";
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

struct Guard { bool s = g_internal; Guard() { g_internal = true; } ~Guard() { g_internal = s; } };
template<class F> static F Fn(uintptr_t rva) { return reinterpret_cast<F>(g_base + rva); }

// ============================================================================
// UE STRUCTS
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
static void* GetObjField(void* obj, const char* path) {
    void* p = GetProp(path);
    if (!p || !obj) return nullptr;
    if (Read<int>(p, profile::PropElementSize) != 8) return nullptr;
    return Read<void*>(GetField(obj, p));
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
// CALL SYSTEM
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

// ============================================================================
// ASSET LOADING
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

// ============================================================================
// NATIVE WRAPPERS — استدعاء ORIG مباشرة
// ============================================================================
static void* GetKismetLib() {
    static void* klib = nullptr;
    if (!klib) klib = FindObj("/Script/FortniteGame.Default__FortKismetLibrary");
    return klib;
}

// Doors
static bool NativeOpenActor(void* actor, void* controller, bool bFast) {
    void* klib = GetKismetLib();
    if (!klib || !actor) return false;
    using FnT = bool(*)(void*, void*, void*, bool);
    return ((FnT)(g_base + profile::ORIG_OpenActor))(klib, actor, controller, bFast);
}
static bool NativeCloseActor(void* actor, void* controller) {
    void* klib = GetKismetLib();
    if (!klib || !actor) return false;
    using FnT = bool(*)(void*, void*, void*);
    return ((FnT)(g_base + profile::ORIG_CloseActor))(klib, actor, controller);
}

// Loot — ★ الدالة السحرية
static void NativeSpawnLoot(void* container) {
    if (!container) return;
    using FnT = void(*)(void*);
    ((FnT)(g_base + profile::ORIG_SpawnLoot))(container);
}
static bool NativeBP_SpawnLoot(void* container, void* pawn) {
    if (!container) return false;
    using FnT = bool(*)(void*, void*);
    return ((FnT)(g_base + profile::ORIG_BP_SpawnLoot))(container, pawn);
}
static bool NativeBP_IsAlreadySearched(void* c) {
    if (!c) return false;
    using FnT = bool(*)(void*);
    return ((FnT)(g_base + profile::ORIG_BP_IsAlreadySearched))(c);
}
static void NativeBP_SetAlreadySearched(void* c, bool s) {
    if (!c) return;
    using FnT = void(*)(void*, bool);
    ((FnT)(g_base + profile::ORIG_BP_SetAlreadySearched))(c, s);
}

// Inventory
static void NativeHandleWorldInventoryLocalUpdate(void* pc) {
    if (!pc) return;
    using FnT = void(*)(void*);
    ((FnT)(g_base + profile::ORIG_HandleWorldInventoryLocalUpdate))(pc);
}
static void* NativeCreateTemporaryItemInstanceBP(void* def, int count, int level) {
    if (!def) return nullptr;
    using FnT = void*(*)(void*, int, int);
    return ((FnT)(g_base + profile::ORIG_CreateTemporaryItemInstanceBP))(def, count, level);
}
static void NativeSetOwningControllerForTemporaryItem(void* item, void* controller) {
    if (!item) return;
    using FnT = void(*)(void*, void*);
    ((FnT)(g_base + profile::ORIG_SetOwningControllerForTemporaryItem))(item, controller);
}

// Ability
static bool NativeTryActivateAbilityByClass(void* asc, void* abilityClass, bool bRemote) {
    if (!asc || !abilityClass) return false;
    using FnT = bool(*)(void*, void*, bool);
    return ((FnT)(g_base + profile::ORIG_TryActivateAbilityByClass))(asc, abilityClass, bRemote);
}
static void NativeOnRep_ActivateAbilities(void* asc) {
    if (!asc) return;
    using FnT = void(*)(void*);
    ((FnT)(g_base + profile::ORIG_OnRep_ActivateAbilities))(asc);
}
static void NativeK2_InitStats(void* asc, void* attrClass, void* dataTable) {
    if (!asc) return;
    using FnT = void(*)(void*, void*, void*);
    ((FnT)(g_base + profile::ORIG_K2_InitStats))(asc, attrClass, dataTable);
}
static void NativeUpdatePlayerCustomCharacterParts(void* state) {
    if (!state) return;
    void* klib = GetKismetLib();
    if (!klib) return;
    using FnT = void(*)(void*, void*);
    ((FnT)(g_base + profile::ORIG_UpdatePlayerCustomCharacterParts))(klib, state);
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
    bool  hud_ready = false;   // ★ جديد
    double observed_at = 0;
    double retry_at = 0;
    double finalize_at = 0;
    std::array<void*, 6> slots{};
    std::vector<void*> character_parts;
    std::vector<void*> granted_ability_sets;
    void* edit_tool = nullptr;
    void* wall = nullptr;
    void* floor = nullptr;
    void* stairs = nullptr;
    void* roof = nullptr;
} session;

static double Monotonic() {
    timespec ts{}; clock_gettime(CLOCK_MONOTONIC, &ts);
    return double(ts.tv_sec) + double(ts.tv_nsec) * 1e-9;
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
    void FinalizeHUD();
    void ShowSkin();
    void LoadProfileCosmetics();
    void LoadDefaultCosmetics();
    void ForcePlayerVisible();
    void BeginPlayerSkydiving();
    void EquipSlot(int slot);
    void DestroyLods();
    void DropLoadingScreen();
    void FixBuildingFoundations();
    void FixBus();
    void InitializeInventory();
    void AddToInventory(void* def, int count, int slot);
    void InventoryUpdate();
    void GrantDefaultAbilities();
    void GrantAbility(void* klass);
    void GrantAllAbilitySets();
    void GrantMMEAbilitySets();
    void GrantMeleeAbility();
    void InitStaminaAttributes();
    void RefreshHUD();
    void ApplySprintSpeed();
    void SetupTacticalSprint();
    void SetTacticalSprinting(bool sprinting);
    void SpawnSafeZoneIndicator();
    void SpawnPickup(void* def, int count, Vector loc, bool toss = true);
    void DropInventoryItem(Guid guid, int count);
    void OnServerExecuteInventoryItem(Guid guid);
    void HandleJumpFromHudStart();
    void SpawnBattleBus();
    void RemoveNetDebugUI();
    void SpawnBuildPreviews();
    void ServerHandlePickup(void* pickup);
    void Loot(void* actor);
    void HandleDoor(void* door);
    void OnServerCreateBuildingActor(void* params);
    void OnBeginEditActor(void* piece);
    void OnFinishEditActor(void* actor, void* newClass, int rotIter, bool mirrored);
    void PlayEmoteItem(void* item);
    void EnableMovementFeatures();
    void SpawnFloorLoot();
}

static void* GetASC(void* actor) {
    if (!actor) return nullptr;
    void* asc = GetObjField(actor, "/Script/FortniteGame.FortPawn.AbilitySystemComponent");
    if (asc) return asc;
    asc = GetObjField(actor, "/Script/FortniteGame.FortPlayerState.AbilitySystemComponent");
    return asc;
}

// ═══════════════════════════════════════════════════════════════
// ABILITY SETS
// ═══════════════════════════════════════════════════════════════
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
    c.set("AbilitySystemInterfaceActor", iface);
    c.set("AbilitySet", abilitySet);
    c.set("OverrideSourceObject", pawn);
    return c.run(klib);
}

void Athena::InitStaminaAttributes() {
    if (!session.pawn) return;
    void* asc = GetASC(session.pawn);
    if (!asc) { LOG("[Stamina] no ASC"); return; }
    void* attrClass = FindObj("/Script/FortniteGame.FortPlayerAttributeSet");
    if (!attrClass) attrClass = FindObj("/Script/GameplayAbilities.AttributeSet");
    if (!attrClass) return;
    void* dataTable = FindObj("/Game/Athena/Items/Quests/AthenaObjectiveStatXPTable.AthenaObjectiveStatXPTable");
    Call c("/Script/GameplayAbilities.AbilitySystemComponent.K2_InitStats");
    if (!c.ok()) return;
    c.set("Attributes", attrClass);
    if (dataTable) c.set("DataTable", dataTable);
    if (c.run(asc)) {
        session.stamina_initialized = true;
        LOG("[Stamina] Initialized");
    }
}

void Athena::GrantMMEAbilitySets() {
    if (!session.pawn) return;
    LOG("[MME] Applying movement ability sets...");

    const char* mmeSets[] = {
        profile::AS_TacticalSprint,
        profile::AS_TacticalSprint_Alt,
        profile::AS_Ascender,
        profile::AS_HillScramble,
        profile::AS_SlideImpulse,
        profile::AS_DoorBash,
        nullptr
    };

    for (int i = 0; mmeSets[i]; ++i) {
        void* set = FindObj(mmeSets[i]);
        if (!set) set = LoadAsset(mmeSets[i], "/Script/FortniteGame.FortAbilitySet");
        if (!set || IsBadPtr(set)) {
            LOG("[MME] ❌ Not found: %s", mmeSets[i]);
            continue;
        }
        // تجنّب التطبيق المزدوج
        bool already = false;
        for (auto* s : session.granted_ability_sets)
            if (s == set) { already = true; break; }
        if (already) continue;

        if (ApplyAbilitySet(session.pawn, set)) {
            session.granted_ability_sets.push_back(set);
            LOG("[MME] ✅ Applied: %s", mmeSets[i]);
        } else {
            LOG("[MME] ⚠️ Failed to apply: %s", mmeSets[i]);
        }
    }
}

void Athena::GrantAbility(void* klass) {
    if (!klass || !session.pawn) return;
    void* asc = GetASC(session.pawn);
    if (!asc) return;
    NativeTryActivateAbilityByClass(asc, klass, true);
    LOG("[Ability] Activated: %s", ObjName(klass).c_str());
}

void Athena::GrantAllAbilitySets() {
    if (!session.pawn || session.abilities_granted) return;
    session.abilities_granted = true;   // ★ فوراً لمنع spam
    LOG("[Abilities] Setting up...");

    void* asc = GetASC(session.pawn);
    if (!asc) { LOG("[Abilities] no ASC"); return; }
    if (!session.stamina_initialized) InitStaminaAttributes();

    // ═══ 1. GAS_AthenaPlayer core set ═══
    void* coreSet = LoadAsset(
        "/Game/Abilities/Player/Generic/Traits/DefaultPlayer/GAS_AthenaPlayer.GAS_AthenaPlayer",
        "/Script/FortniteGame.FortAbilitySet");
    if (!coreSet) {
        LOG("[Abilities] GAS_AthenaPlayer missing");
        // لا نُعلّم session.melee_granted هنا — سنحاول مجدداً من MME
    } else {
        // ★ حقن abilities إضافية داخل coreSet قبل التطبيق
        void* abilitiesProp = GetProp("/Script/FortniteGame.FortAbilitySet.GameplayAbilities");
        if (abilitiesProp) {
            int off = Read<int>(abilitiesProp, profile::PropOffset);
            Array* abilities = (Array*)((char*)coreSet + off);
            if (abilities && abilities->Count >= 0 && abilities->Count < 256) {
                const char* extras[] = {
                    "/Game/Abilities/Weapons/Melee/GAB_Melee_GenericCombo.GAB_Melee_GenericCombo_C",
                    "/Game/Abilities/Weapons/Melee/GAB_Melee_Generic.GAB_Melee_Generic_C",
                    "/Game/Abilities/Player/Generic/Traits/DefaultPlayer/GA_DefaultPlayer_InteractSearch.GA_DefaultPlayer_InteractSearch_C",
                    "/Game/Abilities/Player/Generic/Traits/DefaultPlayer/GA_DefaultPlayer_InteractUse.GA_DefaultPlayer_InteractUse_C",
                    "/Game/Abilities/Player/Generic/Traits/DefaultPlayer/GA_DefaultPlayer_Harvest.GA_DefaultPlayer_Harvest_C",
                    nullptr
                };
                for (int i = 0; extras[i]; ++i) {
                    void* klass = LoadClass(extras[i], "/Script/GameplayAbilities.GameplayAbility");
                    if (!klass) continue;
                    bool present = false;
                    for (int j = 0; j < abilities->Count; ++j)
                        if (Read<void*>(abilities->Data, j * 8) == klass) { present = true; break; }
                    if (!present) {
                        Fn<int(*)(void*,void*,const void*)>(profile::GenericArrayAdd)(abilities, abilitiesProp, &klass);
                        LOG("[Abilities] Added: %s", extras[i]);
                    }
                }
            }
        }

        if (ApplyAbilitySet(session.pawn, coreSet)) {
            session.granted_ability_sets.push_back(coreSet);
            LOG("[Abilities] ✅ Applied coreSet");
        }
    }

    // ═══ 2. MME Ability Sets (التسلق + السبرنت + السلايد) ═══
    GrantMMEAbilitySets();

    // ═══ 3. fallback abilities ═══
    const char* abilities[] = {
        "/Script/GameplayAbilities.GameplayAbility_CharacterJump",
        "/Game/Abilities/Player/Generic/Traits/DefaultPlayer/GA_DefaultPlayer_InteractUse.GA_DefaultPlayer_InteractUse_C",
        "/Game/Abilities/Player/Generic/Traits/DefaultPlayer/GA_DefaultPlayer_InteractSearch.GA_DefaultPlayer_InteractSearch_C",
        "/Game/Abilities/Player/Generic/Traits/DefaultPlayer/GA_DefaultPlayer_Death.GA_DefaultPlayer_Death_C",
        nullptr
    };
    for (int i = 0; abilities[i]; ++i) {
        void* cls = FindObj(abilities[i]);
        if (!cls) cls = LoadClass(abilities[i], "/Script/GameplayAbilities.GameplayAbility");
        if (cls) GrantAbility(cls);
    }

    // ★ الميلي يُعطى فقط من الـ ability set الآن
    session.melee_granted = true;

    LOG("[Abilities] Done — %zu sets", session.granted_ability_sets.size());
}

void Athena::GrantDefaultAbilities() { GrantAllAbilitySets(); }

void Athena::GrantMeleeAbility() {
    // ★ لم نعد نعطي ميلي منفصل — ياتي من GAS_AthenaPlayer
    session.melee_granted = true;
}

void Athena::RefreshHUD() {
    if (!session.controller) return;
    void* asc = GetASC(session.pawn);
    if (asc) NativeOnRep_ActivateAbilities(asc);
}

// ═══════════════════════════════════════════════════════════════
// INVENTORY
// ═══════════════════════════════════════════════════════════════
void Athena::InitializeInventory() {
    if (!session.controller) return;

    // احصل على WorldInventory
    session.inventory = Read<void*>(session.controller, profile::FortPlayerController_WorldInventory);
    if (!session.inventory) {
        LOG("[Inv] No WorldInventory yet — will retry");
        return;
    }

    // احصل على ClientQuickBars
    session.quickbars = Read<void*>(session.controller, profile::FortPlayerController_ClientQuickBars);

    LOG("[Inv] inv=%p qb=%p", session.inventory, session.quickbars);

    // ★ لا تبنِ default items يدوياً بعد الآن — نعطيها عبر AddToInventory
    struct DefSlot { const char* path; int count; int slot; };
    DefSlot defaults[] = {
        {"/Game/Athena/Items/Weapons/WID_Harvest_Pickaxe_Athena_C_T01.WID_Harvest_Pickaxe_Athena_C_T01", 1, 0},
        {"/Game/Items/Weapons/BuildingTools/BuildingItemData_Wall.BuildingItemData_Wall", 1, 2},
        {"/Game/Items/Weapons/BuildingTools/BuildingItemData_Floor.BuildingItemData_Floor", 1, 3},
        {"/Game/Items/Weapons/BuildingTools/BuildingItemData_Stair_W.BuildingItemData_Stair_W", 1, 4},
        {"/Game/Items/Weapons/BuildingTools/BuildingItemData_RoofS.BuildingItemData_RoofS", 1, 5},
    };
    for (auto& d : defaults) {
        void* def = LoadAsset(d.path);
        if (!def) def = FindObj(d.path);
        if (!def) continue;
        AddToInventory(def, d.count, d.slot);
    }

    session.inventory_ready = true;
    InventoryUpdate();
}

void Athena::AddToInventory(void* def, int count, int slot) {
    if (IsBadPtr(def) || !session.inventory || !session.controller) return;

    // ★ استخدم Native CreateTemporaryItemInstanceBP
    void* inst = NativeCreateTemporaryItemInstanceBP(def, count, 1);
    if (!inst || IsBadPtr(inst)) return;

    NativeSetOwningControllerForTemporaryItem(inst, session.controller);

    // احصل على ItemEntry داخل الـ instance
    void* entryProp = GetProp("/Script/FortniteGame.FortWorldItem.ItemEntry");
    if (!entryProp) return;
    void* entrySrc = GetField(inst, entryProp);
    if (!entrySrc) return;

    // ★ خصّص GUID فريد إن لم يكن موجوداً
    Guid g = Read<Guid>(entrySrc, profile::ItemEntry_ItemGuid);
    if ((g.A | g.B | g.C | g.D) == 0) {
        Call ng("/Script/Engine.KismetGuidLibrary.NewGuid");
        void* klib = FindObj("/Script/Engine.Default__KismetGuidLibrary");
        if (ng.ok() && klib && ng.run(klib)) {
            Guid newG = ng.result<Guid>();
            Write<Guid>(entrySrc, profile::ItemEntry_ItemGuid, newG);
        }
    }

    // Durability = 1.0
    Write<float>(entrySrc, 0x24, 1.0f);

    // ★ أضف للـ arrays
    void* list = (char*)session.inventory + 0x100;  // inventory offset داخل AFortInventory
    // ← نستخدم الفعلي:
    void* invProp = GetProp("/Script/FortniteGame.FortInventory.Inventory");
    if (!invProp) return;
    list = (char*)session.inventory + Read<int>(invProp, profile::PropOffset);

    void* instArr = (char*)list + profile::FortItemList_ItemInstances;
    void* entryArr = (char*)list + profile::FortItemList_ReplicatedEntries;

    Fn<int(*)(void*,void*,const void*)>(profile::GenericArrayAdd)(instArr, invProp, &inst);

    // نضيف entrySrc (هو مؤشر مباشر داخل inst، لذا هو مستمر)
    // لكن للأمان، نضيف نسخة:
    Fn<int(*)(void*,void*,const void*)>(profile::GenericArrayAdd)(entryArr, invProp, entrySrc);

    if (slot >= 0 && slot < 6) session.slots[slot] = inst;

    LOG("[Inv+] %s x%d slot=%d", ObjName(def).c_str(), count, slot);
}

void Athena::InventoryUpdate() {
    if (!session.controller) return;

    // ★ Native HandleWorldInventoryLocalUpdate — يبني QuickBars تلقائياً
    NativeHandleWorldInventoryLocalUpdate(session.controller);

    // ForceUpdateQuickbar RPC
    for (uint8_t bar : {(uint8_t)0, (uint8_t)1}) {
        Call c("/Script/FortniteGame.FortPlayerController.ClientForceUpdateQuickbar");
        if (c.ok()) {
            c.set("QuickbarToRefresh", bar);
            c.run(session.controller);
        }
    }
}

void Athena::EquipSlot(int slot) {
    if (slot < 0 || slot >= 6 || !session.slots[slot]) return;
    void* item = session.slots[slot];

    Guid guid{};
    Call g("/Script/FortniteGame.FortItem.GetItemGuid");
    if (g.ok() && g.run(item)) guid = g.result<Guid>();
    if ((guid.A | guid.B | guid.C | guid.D) == 0) return;

    // ServerExecuteInventoryItem (RPC)
    Call eq("/Script/FortniteGame.FortPlayerController.ServerExecuteInventoryItem");
    if (eq.ok()) {
        eq.set("ItemGuid", guid);
        eq.run(session.controller);
    }

    // ClientEquipItem (RPC)
    Call ce("/Script/FortniteGame.FortPlayerController.ClientEquipItem");
    if (ce.ok()) {
        ce.set("ItemGuid", guid);
        bool bForce = true;
        ce.set("bForceExecution", bForce);
        ce.run(session.controller);
    }
}

// ═══════════════════════════════════════════════════════════════
// DROP — ★ الإصلاح الرئيسي
// ═══════════════════════════════════════════════════════════════
void Athena::DropInventoryItem(Guid guid, int count) {
    if (!session.controller) return;

    // ★ ServerAttemptInventoryDrop(FGuid, int32, bool) — 3 params
    Call c("/Script/FortniteGame.FortPlayerController.ServerAttemptInventoryDrop");
    if (!c.ok()) {
        LOG("[Drop] RPC binding failed");
        return;
    }

    int32 countParam = count;
    bool bTrash = false;

    c.set("ItemGuid", guid);
    c.set("Count", countParam);
    c.set("bTrash", bTrash);

    if (c.run(session.controller))
        LOG("[Drop] %08X:%08X count=%d", guid.A, guid.B, count);
    else
        LOG("[Drop] RPC run failed");
}

void Athena::OnServerExecuteInventoryItem(Guid guid) {
    if (!session.inventory || !session.pawn) return;
    void* invProp = GetProp("/Script/FortniteGame.FortInventory.Inventory");
    if (!invProp) return;
    void* list = (char*)session.inventory + Read<int>(invProp, profile::PropOffset);
    void* instArrPtr = (char*)list + profile::FortItemList_ItemInstances;
    Array arr = Read<Array>(instArrPtr);

    for (int i = 0; i < arr.Count; ++i) {
        void* inst = Read<void*>(arr.Data, (size_t)i * 8);
        if (!inst) continue;
        Call g("/Script/FortniteGame.FortItem.GetItemGuid");
        if (!g.ok() || !g.run(inst)) continue;
        Guid cand = g.result<Guid>();
        if (memcmp(&cand, &guid, sizeof(Guid)) == 0) {
            for (int s = 0; s < 6; ++s)
                if (session.slots[s] == inst) { EquipSlot(s); return; }
            return;
        }
    }
}

// ═══════════════════════════════════════════════════════════════
// LOOT — ★ نادي SpawnLoot_ORIG فقط
// ═══════════════════════════════════════════════════════════════
void Athena::Loot(void* actor) {
    if (!actor || IsBadPtr(actor) || !session.pawn) return;

    static void* containerCls = nullptr;
    if (!containerCls) containerCls = FindObj("/Script/FortniteGame.BuildingContainer");
    if (!containerCls || !IsA(actor, containerCls)) return;

    // ★ نادي Native SpawnLoot مباشرة — تسوي كل شي أوتوماتيك
    NativeSpawnLoot(actor);

    LOG("[Loot] NativeSpawnLoot called on %s",
        ObjName(Read<void*>(actor, profile::ObjectClass)).c_str());
}

void Athena::SpawnFloorLoot() {
    LOG("[Loot] Floor loot setup...");
    // نفس المنطق: الطبقات تحضّر صناديق `Tiered_Athena_FloorLoot` والـ SpawnLoot_hook سيُستدعى تلقائياً
}

// ═══════════════════════════════════════════════════════════════
// DOORS — ★ استخدم OpenActor/CloseActor ORIG
// ═══════════════════════════════════════════════════════════════
void Athena::HandleDoor(void* door) {
    if (!door || IsBadPtr(door) || !session.pawn) return;

    static void* wallCls = nullptr;
    if (!wallCls) wallCls = FindObj("/Script/FortniteGame.BuildingWall");
    if (!wallCls || !IsA(door, wallCls)) return;

    // اقرأ الحالة
    uint8_t* byte = (uint8_t*)door + profile::BuildingWall_bDoorOpen_Byte;
    bool wasOpen = ((*byte) >> profile::BuildingWall_bDoorOpen_Bit) & 1;

    // ★ نادي native بدل تعديل byte يدوياً
    bool success;
    if (wasOpen) {
        success = NativeCloseActor(door, session.controller);
        LOG("[Door] Closing -> %d", success);
    } else {
        success = NativeOpenActor(door, session.controller, false);
        LOG("[Door] Opening -> %d", success);
    }

    // ForceNetUpdate احتياطي
    Call n("/Script/Engine.Actor.ForceNetUpdate");
    if (n.ok()) n.run(door);
}

// ═══════════════════════════════════════════════════════════════
// SKIN / VISIBILITY
// ═══════════════════════════════════════════════════════════════
void Athena::LoadProfileCosmetics() { LoadDefaultCosmetics(); }

void Athena::LoadDefaultCosmetics() {
    if (!session.pawn || !session.controller) return;
    LOG("[Cosmetics] Loading defaults...");

    void* state = GetObjField(session.controller, "/Script/Engine.Controller.PlayerState");
    if (!state) return;

    void* hero = LoadAsset("/Game/Athena/Heroes/HID_001_Athena_Commando_F.HID_001_Athena_Commando_F",
                            "/Script/FortniteGame.FortHeroType");
    if (!hero) hero = LoadAsset("/Game/Athena/Heroes/HID_001_Athena_Commando_M.HID_001_Athena_Commando_M",
                                "/Script/FortniteGame.FortHeroType");
    if (hero) Write<void*>(state, profile::FortPlayerState_HeroType, hero);

    Call r1("/Script/FortniteGame.FortPlayerState.OnRep_HeroType"); if (r1.ok()) r1.run(state);
    Call r2("/Script/FortniteGame.FortPlayerState.OnRep_CharacterParts"); if (r2.ok()) r2.run(state);
    Call r3("/Script/FortniteGame.FortPlayerState.OnRep_CharacterData"); if (r3.ok()) r3.run(state);

    // ★ استخدم Native للـ visualization update
    NativeUpdatePlayerCustomCharacterParts(state);
}

void Athena::ShowSkin() { LoadProfileCosmetics(); }

void Athena::ForcePlayerVisible() {
    if (!session.pawn || IsBadPtr(session.pawn)) return;

    SetBool(session.pawn, "/Script/FortniteGame.FortPlayerPawn.bIsSkydiving", false);
    SetBool(session.pawn, "/Script/FortniteGame.FortPlayerPawn.bIsParachuteOpen", false);

    Call sd("/Script/FortniteGame.FortPlayerPawn.OnRep_IsSkydiving");
    if (sd.ok()) sd.run(session.pawn);
    Call po("/Script/FortniteGame.FortPlayerPawn.OnRep_IsParachuteOpen");
    if (po.ok()) po.run(session.pawn);

    Call hid("/Script/Engine.Actor.SetActorHiddenInGame");
    if (hid.ok()) {
        bool bHidden = false;
        if (hid.set("bNewHidden", bHidden)) hid.run(session.pawn);
    }

    SetBool(session.pawn, "/Script/Engine.Actor.bHidden", false);

    LoadDefaultCosmetics();
    session.visibility_forced = true;
}

// ═══════════════════════════════════════════════════════════════
// PLAYER INIT
// ═══════════════════════════════════════════════════════════════
bool Athena::TryInitializePlayer(void* world, void* mode, void* controller) {
    void* pawnCls = LoadClass("/Game/Athena/PlayerPawn_Athena.PlayerPawn_Athena_C", "/Script/Engine.Pawn");
    if (!pawnCls) { LOG("[Init] PlayerPawn_Athena_C missing"); return false; }

    void* existing = GetPawn(controller);
    if (existing && !IsBadPtr(existing) && IsA(existing, pawnCls)) {
        session.pawn = existing;
        LOG("[Init] Using existing pawn");
        return true;
    }

    Transform t{};
    t.Location[0] = 0.0; t.Location[1] = 0.0; t.Location[2] = 500.0;

    Call choose("/Script/Engine.GameModeBase.ChoosePlayerStart");
    if (choose.ok() && choose.set("Player", controller) && choose.run(mode)) {
        void* start = choose.result<void*>();
        if (start && !IsBadPtr(start)) {
            Call loc("/Script/Engine.Actor.K2_GetActorLocation");
            if (loc.ok() && loc.run(start)) {
                Vector v = loc.result<Vector>();
                if (std::isfinite(v.X) && std::isfinite(v.Y) && std::isfinite(v.Z)) {
                    t.Location[0] = v.X; t.Location[1] = v.Y; t.Location[2] = v.Z + 100.0;
                }
            }
        }
    }

    session.pawn = SpawnActor(world, pawnCls, controller, t);
    if (!session.pawn) { LOG("[Init] Spawn failed"); return false; }

    LOG("[Init] Spawned: %p", session.pawn);

    SetBool(session.pawn, "/Script/FortniteGame.FortPlayerPawn.bIsSkydiving", false);
    SetBool(session.pawn, "/Script/FortniteGame.FortPlayerPawn.bIsParachuteOpen", false);

    Possess(controller, session.pawn);

    if (GetPawn(controller) != session.pawn) { LOG("[Init] Possession did not stick"); return false; }
    LOG("[Init] Possessed OK");

    SetupCheats();
    ForcePlayerVisible();
    return true;
}

// ═══════════════════════════════════════════════════════════════
// HUD
// ═══════════════════════════════════════════════════════════════
void Athena::FinalizeHUD() {
    if (!session.controller || !session.pawn || !session.state) return;
    LOG("=== FinalizeHUD ===");

    // 1) PlaylistDataIsLoaded
    void* plLoadedProp = GetProp("/Script/FortniteGame.FortGameStateAthena.bPlaylistDataIsLoaded");
    if (plLoadedProp) Write<bool>(GetField(session.state, plLoadedProp), 0, true);

    Call n1("/Script/FortniteGame.FortGameStateAthena.OnRep_CurrentPlaylistData");
    if (n1.ok()) n1.run(session.state);
    Call n2("/Script/FortniteGame.FortGameStateAthena.OnRep_CurrentPlaylistInfo");
    if (n2.ok()) n2.run(session.state);

    // 2) Loading flags
    SetBool(session.controller, "/Script/FortniteGame.FortPlayerController.bHasServerFinishedLoading", true);
    SetBool(session.controller, "/Script/FortniteGame.FortPlayerController.bHasClientFinishedLoading", true);
    SetBool(session.controller, "/Script/FortniteGame.FortPlayerController.bHasFinishedLoading", true);
    SetBool(session.controller, "/Script/FortniteGame.FortPlayerController.bClientHasFinishedLoading", true);

    // 3) ServerReadyToStartMatch
    Call srt("/Script/FortniteGame.FortPlayerController.ServerReadyToStartMatch");
    if (srt.ok()) srt.run(session.controller);

    // 4) ClientOnPawnSpawned — ★ حاسم
    Call cps("/Script/FortniteGame.FortPlayerControllerZone.ClientOnPawnSpawned");
    if (cps.ok()) cps.run(session.controller);

    // 5) ClientRestart
    Call cr("/Script/Engine.PlayerController.ClientRestart");
    if (cr.ok()) { cr.set("NewPawn", session.pawn); cr.run(session.controller); }

    // 6) SetViewTarget
    Call cam("/Script/Engine.PlayerController.SetViewTargetWithBlend");
    if (cam.ok()) {
        cam.set("NewViewTarget", session.pawn);
        float blend = 0.0f;
        cam.set("BlendTime", blend);
        cam.run(session.controller);
    }

    // 7) RefreshHUD
    RefreshHUD();

    LOG("FinalizeHUD: complete");
}

// ═══════════════════════════════════════════════════════════════
// MOVEMENT
// ═══════════════════════════════════════════════════════════════
void Athena::EnableMovementFeatures() {
    if (!session.pawn) return;
    LOG("[Movement] Enabling...");

    // SetCanSlide
    Call canSlide("/Script/FortniteGame.FortPlayerPawn.SetCanSlide");
    if (canSlide.ok()) {
        bool bTrue = true;
        if (canSlide.set("bCanSlide", bTrue)) {
            if (canSlide.run(session.pawn)) LOG("[Movement] ✅ SetCanSlide");
        }
    }

    // Fallback: bWantsToSlide
    SetBool(session.pawn, "/Script/FortniteGame.FortPawn.bWantsToSlide", true);

    session.movement_enabled = true;
    LOG("[Movement] Done — MME sets applied in GrantAllAbilitySets");
}

void Athena::ApplySprintSpeed() {
    if (!session.pawn || IsBadPtr(session.pawn)) return;
    void* pawnCls = FindObj("/Game/Athena/PlayerPawn_Athena.PlayerPawn_Athena_C");
    if (pawnCls && IsA(session.pawn, pawnCls)) {
        void* prop = GetProp("/Script/FortniteGame.FortPlayerPawnAthena.Speed_Sprint");
        if (prop) {
            float* val = (float*)GetField(session.pawn, prop);
            if (val && !IsBadPtr(val) && std::isfinite(*val) && *val > 0.0f && *val < 5000.0f) {
                if (fabsf(*val - 1300.0f) > 1.0f) *val = 1300.0f;
            }
        }
    }
    void* cmc = GetObjField(session.pawn, "/Script/Engine.Character.CharacterMovement");
    if (cmc && !IsBadPtr(cmc)) {
        void* prop = GetProp("/Script/Engine.CharacterMovementComponent.MaxWalkSpeed");
        if (prop) { float* val = (float*)GetField(cmc, prop); if (val && !IsBadPtr(val)) { if (fabsf(*val - 600.f) > 1.0f) *val = 600.f; } }
    }
    session.sprint_applied = true;
}

void Athena::SetupTacticalSprint() {
    // ★ MME AbilitySet يتولى هذا — لا حاجة لإنشاء components يدوياً
    session.tactical_sprint_setup = true;
}

void Athena::SetTacticalSprinting(bool) {}

// ═══════════════════════════════════════════════════════════════
// WORLDTICK-BASED UTILS
// ═══════════════════════════════════════════════════════════════
void Athena::SetupCheats() {
    void* pc = session.controller; if (!pc) return;
    void* p = GetProp("/Script/Engine.PlayerController.CheatManager");
    void* klass = FindObj("/Script/FortniteGame.FortCheatManager");
    if (!klass) klass = FindObj("/Script/Engine.CheatManager");
    if (!p || !klass) return;
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

void Athena::DestroyLods() {
    void* cm = GetObjField(session.controller, "/Script/Engine.PlayerController.CheatManager");
    if (!cm) { SetupCheats(); cm = GetObjField(session.controller, "/Script/Engine.PlayerController.CheatManager"); }
    if (!cm) return;
    void* cls = FindObj("/Script/FortniteGame.FortHLODSMActor");
    if (!cls) return;
    Call c("/Script/Engine.CheatManager.DestroyAll");
    if (c.ok() && c.set("aClass", cls)) c.run(cm);
}

void Athena::DropLoadingScreen() {
    if (!session.state || !session.controller) return;
    SetBool(session.controller, "/Script/FortniteGame.FortPlayerController.bHasServerFinishedLoading", true);
    SetBool(session.controller, "/Script/FortniteGame.FortPlayerController.bHasClientFinishedLoading", true);
    if (!session.ready_called) {
        Call srt("/Script/FortniteGame.FortPlayerController.ServerReadyToStartMatch");
        if (srt.ok()) srt.run(session.controller);
        Call cps("/Script/FortniteGame.FortPlayerControllerZone.ClientOnPawnSpawned");
        if (cps.ok()) cps.run(session.controller);
        session.ready_called = true;
    }
}

void Athena::FixBuildingFoundations() {}
void Athena::FixBus() {}
void Athena::SpawnSafeZoneIndicator() {}
void Athena::SpawnBuildPreviews() {}
void Athena::RemoveNetDebugUI() {}
void Athena::BeginPlayerSkydiving() {}
void Athena::HandleJumpFromHudStart() {}
void Athena::SpawnBattleBus() {}
void Athena::OnServerCreateBuildingActor(void*) {}
void Athena::OnBeginEditActor(void*) {}
void Athena::OnFinishEditActor(void*, void*, int, bool) {}
void Athena::PlayEmoteItem(void*) {}
void Athena::ServerHandlePickup(void*) {}
void Athena::SpawnPickup(void*, int, Vector, bool) {}

// ═══════════════════════════════════════════════════════════════
// SETUP WORLD
// ═══════════════════════════════════════════════════════════════
void Athena::SetupWorld(void* world) {
    session.world = world;
    session.controller = GetPC(world, 0);
    if (!session.controller) { LOG("SetupWorld: no controller"); return; }

    void* baseCtrl = FindObj("/Script/FortniteGame.FortPlayerController");
    if (!baseCtrl || !IsA(session.controller, baseCtrl)) { LOG("SetupWorld: not FortPC"); return; }

    void* modeProp  = GetProp("/Script/Engine.World.AuthorityGameMode");
    void* stateProp = GetProp("/Script/Engine.World.GameState");
    session.mode  = modeProp  ? Read<void*>(GetField(world, modeProp))  : nullptr;
    session.state = stateProp ? Read<void*>(GetField(world, stateProp)) : nullptr;
    if (!session.mode || !session.state) { LOG("SetupWorld: mode/state missing"); return; }

    LOG("SetupWorld: PC=%s GM=%s GS=%s",
        ObjName(session.controller).c_str(),
        ObjName(session.mode).c_str(),
        ObjName(session.state).c_str());

    if (!TryInitializePlayer(world, session.mode, session.controller)) return;

    LOG("SetupWorld: GrantAllAbilitySets"); GrantAllAbilitySets();
    LOG("SetupWorld: EnableMovementFeatures"); EnableMovementFeatures();
    LOG("SetupWorld: ShowSkin"); ShowSkin();
    LOG("SetupWorld: InitializeInventory"); InitializeInventory();
    LOG("SetupWorld: ApplySprintSpeed"); ApplySprintSpeed();
    LOG("SetupWorld: DropLoadingScreen"); DropLoadingScreen();

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
    if (pawn && pawnCls && IsA(pawn, pawnCls)) session.pawn = pawn;

    if (!session.abilities_granted) GrantAllAbilitySets();
    if (!session.movement_enabled) EnableMovementFeatures();
    if (!session.ready_called) {
        Call srt("/Script/FortniteGame.FortPlayerController.ServerReadyToStartMatch");
        if (srt.ok()) srt.run(pc);
        Call cps("/Script/FortniteGame.FortPlayerControllerZone.ClientOnPawnSpawned");
        if (cps.ok()) cps.run(pc);
        session.ready_called = true;
    }

    session.visibility_forced = false;
    ForcePlayerVisible();
    RefreshHUD();
    FinalizeHUD();   // ★
    LOG("Finalize: complete");
}

// ═══════════════════════════════════════════════════════════════
// TICK
// ═══════════════════════════════════════════════════════════════
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

    static double lastVisibility = 0, lastHudCheck = 0;
    if (session.finalized && now - lastVisibility > 8.0) {
        lastVisibility = now;
        if (!session.visibility_forced) ForcePlayerVisible();
    }
    if (session.finalized && !session.hud_ready && now - lastHudCheck > 2.0) {
        lastHudCheck = now;
        void* p = GetProp("/Script/FortniteGame.FortPlayerController.bHasServerFinishedLoading");
        if (p) {
            uint8_t byte = Read<uint8_t>(p, profile::BoolByteOffset);
            uint8_t mask = Read<uint8_t>(p, profile::BoolFieldMask);
            uint8_t val = Read<uint8_t>(GetField(session.controller, p));
            if ((val >> byte) & 1) {
                session.hud_ready = true;
                LOG("HUD detected ready");
            } else {
                LOG("HUD not ready — reapplying FinalizeHUD");
                FinalizeHUD();
            }
        }
    }
}

// ============================================================================
// HOOKS
// ============================================================================
static void* g_interactionClass = nullptr;

static void pe_hook(void* obj, void* func, void* params) {
    if (!orig_PE) return;
    if (!g_internal && func && g_gameTid.load() == (int)gettid()) {
        if (!g_interactionClass) g_interactionClass = FindObj("/Script/FortniteGame.FortControllerComponent_Interaction");

        bool isPC   = (obj == session.controller);
        bool isPawn = (obj == session.pawn);
        bool isInteraction = false;
        if (g_interactionClass && obj) {
            void* cls = Read<void*>(obj, profile::ObjectClass);
            isInteraction = SubclassOfFast(cls, g_interactionClass);
        }

        if (isPC || isPawn || isInteraction) {
            Guard g;
            std::string n = ObjName(func);

            // ★ Drop
            if (n == "ServerAttemptInventoryDrop" || n == "ServerSpawnInventoryDrop") {
                if (params && Read<uint16_t>(func, profile::FunctionParamSize) >= 24) {
                    Guid gg = Read<Guid>(params, 0);
                    int cnt = Read<int32>(params, 16);
                    Athena::DropInventoryItem(gg, cnt);
                }
            }
            // Interact — Door أو Container
            else if (n == "ServerAttemptInteract" && params) {
                void* target = Read<void*>(params, 0);
                if (target && !IsBadPtr(target)) {
                    void* targetCls = Read<void*>(target, profile::ObjectClass);
                    void* bcCls   = FindObj("/Script/FortniteGame.BuildingContainer");
                    void* wallCls = FindObj("/Script/FortniteGame.BuildingWall");
                    if (bcCls && SubclassOfFast(targetCls, bcCls)) {
                        Athena::Loot(target);
                    } else if (wallCls && SubclassOfFast(targetCls, wallCls)) {
                        Athena::HandleDoor(target);
                    }
                }
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
    if (wn.find("Artemis_Terrain") == std::string::npos &&
        wn.find("Athena_Terrain") == std::string::npos) return;

    int index = Read<int>(world, 0xC);
    if (world != session.world || index != session.world_index) {
        session = Session{};
        session.world = world;
        session.world_index = index;
        session.observed_at = Monotonic();
        LOG(">>> Terrain detected: %s", wn.c_str());
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

    if (normalized == "open athena_terrain" || normalized.rfind("open athena_terrain", 0) == 0) {
        LOG("Intercepting Athena travel");
        session = Session{};
        session.attempted = false;
        return orig_Exec(engine, world, u"open Artemis_Terrain", output);
    }
    return orig_Exec(engine, world, command, output);
}

static void InstallHooks() {
    LOG("=== InstallHooks ===");
    if (DobbyHook((void*)(g_base + profile::ProcessEvent), (void*)pe_hook, (void**)&orig_PE) != 0 || !orig_PE) {
        LOG("ProcessEvent FAILED"); return;
    }
    LOG("ProcessEvent hooked");
    if (DobbyHook((void*)(g_base + profile::WorldTick), (void*)tick_hook, (void**)&orig_Tick) != 0 || !orig_Tick)
        LOG("WorldTick FAILED");
    else LOG("WorldTick hooked");
    if (DobbyHook((void*)(g_base + profile::EngineExec), (void*)exec_hook, (void**)&orig_Exec) != 0 || !orig_Exec)
        LOG("EngineExec FAILED");
    else LOG("EngineExec hooked");
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
    LOG("=== EraCH3 starting ===");
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
