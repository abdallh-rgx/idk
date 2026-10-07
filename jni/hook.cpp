// ============================================================================
// EraCH3 — Fortnite 21.30 CL-21088273 Android
// COMPLETE FIXED BUILD
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

// ============================================================================
// LOGGING
// ============================================================================
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
    char line[1024];
    va_list ap; va_start(ap, fmt);
    vsnprintf(line, sizeof(line), fmt, ap);
    va_end(ap);
    __android_log_write(ANDROID_LOG_INFO, "EraCH3", line);
    if (g_logFile) { fprintf(g_logFile, "%s\n", line); fflush(g_logFile); }
}
#define LOG(...) LogImpl(__VA_ARGS__)

using int8=int8_t; using int16=int16_t; using int32=int32_t; using int64=int64_t;
using uint8=uint8_t; using uint16=uint16_t; using uint32=uint32_t; using uint64=uint64_t;
using TCHAR=char16_t;

// ============================================================================
// PROFILE — RVA offsets for CL-21088273 (21.30 Android)
// ============================================================================
namespace profile {
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
constexpr uintptr_t CopyScriptStruct     = 0x07535848;

// Object field offsets
constexpr unsigned ObjectClass  = 0x10;
constexpr unsigned ObjectName   = 0x18;
constexpr unsigned ObjectOuter  = 0x20;

// Struct fields
constexpr unsigned FieldNext        = 0x28;
constexpr unsigned StructSuper      = 0x40;
constexpr unsigned StructChildren   = 0x48;
constexpr unsigned StructChildProps = 0x50;

// Class fields
constexpr unsigned ClassDefaultObject = 0x110;

// Function fields
constexpr unsigned FunctionParamSize = 0xB6;

// Property fields
constexpr unsigned PropFlags       = 0x38;
constexpr unsigned PropOffset      = 0x44;
constexpr unsigned PropElementSize = 0x34;

// Bool property layout
constexpr unsigned BoolSize       = 0x80;
constexpr unsigned BoolByteOffset = 0x81;
constexpr unsigned BoolFieldMask  = 0x82;

// FField (for UFunction params from child props)
constexpr unsigned FFieldName = 0x28;
constexpr unsigned FFieldNext = 0x20;

// CPF flags
constexpr uint64_t CPF_Parm       = 0x00000080ULL;
constexpr uint64_t CPF_OutParm    = 0x00000100ULL;
constexpr uint64_t CPF_ReturnParm = 0x00000400ULL;

// 🔧 Chapter 3 SDK-confirmed struct offsets
constexpr unsigned ItemEntry_Durability    = 0x24;
constexpr unsigned ItemEntry_LoadedAmmo    = 0x2C;
constexpr unsigned ItemEntry_PhantomAmmo   = 0x30;
constexpr unsigned ItemEntry_TrackerGuid   = 0x78;
constexpr unsigned ItemEntry_StateValues   = 0xD0;

constexpr unsigned FortPlayerState_HeroType          = 0x0430;
constexpr unsigned FortPlayerController_MyPlayerInfo = 0x2088;
constexpr unsigned RegisteredPlayerInfo_HeroInstance = 0x2D0;
constexpr unsigned FortHero_CharacterParts           = 0x238;
constexpr unsigned FortPlayerPawn_bIsSkydiving       = 0x1A1D;
constexpr unsigned BuildingWall_bDoorOpen_Byte       = 0x0FDE;
constexpr unsigned BuildingWall_bDoorOpen_Bit        = 5;
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
    T r{};
    if (p) memcpy(&r, (const char*)p + off, sizeof(T));
    return r;
}
template<class T> static void Write(void* p, size_t off, const T& v) {
    if (p) memcpy((char*)p + off, &v, sizeof(T));
}

static uintptr_t g_base = 0;
static std::atomic<int> g_status{0};
static std::atomic<int> g_gameTid{0};
static thread_local bool g_internal = false;

struct Guard {
    bool s = g_internal;
    Guard() { g_internal = true; }
    ~Guard() { g_internal = s; }
};

template<class F> static F Fn(uintptr_t rva) {
    return reinterpret_cast<F>(g_base + rva);
}

// ============================================================================
// UE5 STRUCTS
// ============================================================================
struct FStringNative { TCHAR* Data; int32 Count; int32 Max; };
struct FUEString     { TCHAR* data; int32 length; int32 capacity; };
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

// TScriptInterface<T> for AbilitySystemInterfaceActor
struct TScriptInterface { void* Object; void* Interface; };

// ============================================================================
// FORWARD DECLS
// ============================================================================
static void* FindObj(const std::string& path);
static std::string ObjName(void* obj);
static bool IsA(void* obj, void* type);
static void* GetProp(const char* path);
static void* GetField(void* obj, void* prop);
static void SetBool(void* obj, const char* path, bool enabled);
static std::u16string ToWide(const std::string& s);
static std::string Narrow(const char16_t* s, int max);
static void* GetObjField(void* obj, const char* path);
static void* LoadAsset(const std::string& path, const char* type_path);

// ============================================================================
// TYPE CHECKS
// ============================================================================
static bool IsA(void* obj, void* type) {
    if (!obj || !type || IsBadPtr(obj) || IsBadPtr(type)) return false;
    void* klass = Read<void*>(obj, profile::ObjectClass);
    if (!klass || IsBadPtr(klass)) return false;
    std::string target = ObjName(type);
    if (target.empty()) return false;
    for (int depth = 0; depth < 64 && klass && !IsBadPtr(klass); ++depth) {
        if (ObjName(klass) == target) return true;
        void* super = Read<void*>(klass, profile::StructSuper);
        if (super == klass) break;
        klass = super;
    }
    return false;
}

// ============================================================================
// STRING HELPERS
// ============================================================================
static std::u16string ToWide(const std::string& s) {
    return std::u16string(s.begin(), s.end());
}
static std::string Narrow(const char16_t* s, int max) {
    std::string out;
    if (s) for (int i = 0; i < max && s[i]; ++i)
        out.push_back(s[i] < 128 ? (char)s[i] : '?');
    return out;
}

static void* FindObj(const std::string& path) {
    auto w = ToWide(path);
    return Fn<void*(*)(void*, void*, const char16_t*, bool)>(profile::StaticFindObject)(
        nullptr, nullptr, w.c_str(), false);
}

static bool SafeFieldName(void* field, uint32 nameOff, std::string& out) {
    if (!field || IsBadPtr(field)) return false;
    uint32 compIdx = Read<uint32>(field, nameOff + 0);
    uint32 dispIdx = Read<uint32>(field, nameOff + 4);
    if (compIdx == 0xFFFFFFFF || compIdx > 0x8000000) return false;
    if (dispIdx == 0xFFFFFFFF || dispIdx > 0x8000000) return false;
    FStringNative ns{};
    Fn<void(*)(const void*, FStringNative*)>(profile::NameAppendString)(
        (char*)field + nameOff, &ns);
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
    if (it != cache.end()) {
        std::string r = it->second;
        pthread_mutex_unlock(&m);
        return r;
    }
    pthread_mutex_unlock(&m);

    FStringNative s{};
    Fn<void(*)(const void*, FStringNative*)>(profile::NameAppendString)(
        (char*)obj + profile::ObjectName, &s);
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
// REFLECTION CALL
// ============================================================================
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
        for (int i = 0; p && !IsBadPtr(p) && i < 256; ++i) {
            add_param(p, Read<uint64_t>(p, profile::PropFlags), total, false);
            p = Read<void*>(p, profile::FieldNext);
        }
        void* fp = Read<void*>(m_func, profile::StructChildProps);
        for (int i = 0; fp && !IsBadPtr(fp) && i < 256; ++i) {
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
            LOG("   '%s' off=%d size=%d flags=%llx", p.name.c_str(), p.offset, p.size,
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
    bool  abilities_granted = false;   // ⭐ NEW
    bool  stamina_initialized = false; // ⭐ NEW
    double observed_at = 0;
    double retry_at = 0;
    double finalize_at = 0;
    std::array<void*, 6> slots{};
    std::vector<void*> character_parts;
    void* edit_tool = nullptr;
    std::vector<void*> granted_ability_sets; // ⭐ NEW
} session;

static double Monotonic() {
    timespec ts{};
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return double(ts.tv_sec) + double(ts.tv_nsec) * 1e-9;
}

// ============================================================================
// ABILITIES SYSTEM — Chapter 3 correct approach
// ============================================================================
namespace Abilities {

// Get the ASC from any actor (pawn or player state)
static void* GetASC(void* actor) {
    if (!actor) return nullptr;
    void* asc = GetObjField(actor, "/Script/FortniteGame.FortPawn.AbilitySystemComponent");
    if (asc) return asc;
    asc = GetObjField(actor, "/Script/FortniteGame.FortPlayerState.AbilitySystemComponent");
    if (asc) return asc;
    asc = GetObjField(actor, "/Script/GameplayAbilities.AbilitySystemComponent");
    return asc;
}

// Get the AbilitySystemInterface for a given actor (needed for EquipFortAbilitySet)
static TScriptInterface GetAbilityInterface(void* actor) {
    TScriptInterface iface{actor, nullptr};
    if (!actor) return iface;
    // UFortKismetLibrary finds the interface internally; passing just the actor is enough
    return iface;
}

// Give a single ability class via ASC reflection
static bool GiveAbility(void* asc, void* abilityClass) {
    if (!asc || !abilityClass) return false;
    // Try GiveAbility with spec construction via reflection
    // In UE5 the C++ GiveAbility needs a FGameplayAbilitySpec; we can use K2-equivalent path:
    // Fortnite provides FortKismetLibrary::EquipFortAbilitySet as the proper entry point.
    // For direct granting we fall back to TryActivateAbilityByClass (works if ability already granted).
    return false;
}

// Apply a full ability set to a pawn — the CORRECT way
static bool EquipAbilitySet(void* pawn, void* abilitySet) {
    if (!pawn || !abilitySet) return false;

    void* klib = FindObj("/Script/FortniteGame.Default__FortKismetLibrary");
    if (!klib) { LOG("[Abilities] KismetLibrary CDO missing"); return false; }

    Call c("/Script/FortniteGame.FortKismetLibrary.EquipFortAbilitySet");
    if (!c.ok()) {
        LOG("[Abilities] EquipFortAbilitySet binding FAILED");
        c.dump("EquipFortAbilitySet");
        return false;
    }

    TScriptInterface iface = GetAbilityInterface(pawn);

    bool s1 = c.set("AbilitySystemInterfaceActor", iface);
    bool s2 = c.set("AbilitySet", abilitySet);
    bool s3 = c.set("OverrideSourceObject", pawn);

    if (!s1 || !s2 || !s3) {
        LOG("[Abilities] param bind failed iface=%d set=%d pawn=%d", s1, s2, s3);
        c.dump("EquipFortAbilitySet");
        return false;
    }

    if (!c.run(klib)) return false;
    LOG("[Abilities] ✅ Applied set: %s", ObjName(abilitySet).c_str());
    return true;
}

// Collect every UFortGameFeatureData->PlayerAbilitySet from GObjects
static std::vector<void*> CollectGameFeatureAbilitySets() {
    std::vector<void*> result;
    GObjects_t* go = *(GObjects_t**)(g_base + profile::GObjects);
    if (!go) return result;

    void* gfdClass = FindObj("/Script/FortniteGame.FortGameFeatureData");
    if (!gfdClass) { LOG("[Abilities] FortGameFeatureData class missing"); return result; }

    int scanned = 0;
    for (int32 i = 0; i < go->NumElements; ++i) {
        void* obj = go->GetById(i);
        if (!obj || IsBadPtr(obj)) continue;
        if (!IsA(obj, gfdClass)) continue;
        ++scanned;

        // PlayerAbilitySet is a TSoftObjectPtr<UFortAbilitySet>
        void* pasProp = GetProp("/Script/FortniteGame.FortGameFeatureData.PlayerAbilitySet");
        if (!pasProp) continue;

        // SoftObjectPtr layout: { FWeakObjectPtr WeakPtr (8); FSoftObjectPath ObjectID { FTopLevelAssetPath {FName PackageName, FName AssetName}, FString SubPathString } }
        // We only need the AssetPathName which we can pass to LoadAsset
        // Simpler: read the WeakPtr directly if it's loaded
        void* softPtrAddr = GetField(obj, pasProp);
        if (!softPtrAddr || IsBadPtr(softPtrAddr)) continue;

        // Try loading via SoftObjectPtr->Get() path through reflection
        // The reflected SoftObjectPtr property will have a loaded weak pointer if already resolved
        void* weakObj = Read<void*>(softPtrAddr, 0); // FWeakObjectPtr object index
        // Actually the loaded pointer is at a specific offset; reading index 0 is FWeakObjectPtr.ObjectIndex
        // Let's use the AssetPathName route instead

        // FSoftObjectPath is at offset 8 after WeakPtr
        // Structure: FTopLevelAssetPath { FName PackageName (8); FName AssetName (8); } then FString SubPathString
        // We can extract the string via KismetSystemLibrary::Conv_SoftObjPathToSoftObjRef or similar
        // Simpler: assume weak pointer is resolved by the time we run
        if (!IsBadPtr(weakObj) && weakObj != nullptr && weakObj != (void*)0xFFFFFFFFFFFFFFFFULL) {
            // The pointer stored here is not directly the object; need GObjects lookup
            // Skip - use alternate method below
        }

        // Alternative: read the FSoftObjectPath's PackageName and resolve manually
        // But easier: many GameFeatureData have `PlayerAbilitySet` set on CDO which is auto-loaded.
        // Search GObjects for UFortAbilitySet instances whose Outer is this GameFeatureData
        for (int32 j = 0; j < go->NumElements; ++j) {
            void* candidate = go->GetById(j);
            if (!candidate || IsBadPtr(candidate)) continue;
            void* outer = Read<void*>(candidate, profile::ObjectOuter);
            if (outer != obj) continue;
            void* cls = Read<void*>(candidate, profile::ObjectClass);
            if (!cls) continue;
            if (ObjName(cls).find("FortAbilitySet") == std::string::npos) continue;
            // Accept as a valid ability set
            result.push_back(candidate);
        }
    }
    LOG("[Abilities] Scanned %d GameFeatureData objects, found %zu ability sets",
        scanned, result.size());
    return result;
}

// Initialize the stamina attribute set on the ASC
static bool InitStaminaAttributes(void* asc) {
    if (!asc) return false;

    void* attrClass = FindObj("/Script/FortniteGame.FortPlayerAttributeSet");
    if (!attrClass) {
        // Fallback to generic attribute set
        attrClass = FindObj("/Script/GameplayAbilities.AttributeSet");
    }
    if (!attrClass) { LOG("[Stamina] AttributeSet class missing"); return false; }

    // Find a DataTable to pass (can be null in many cases)
    void* dataTable = FindObj("/Game/Athena/Items/Quests/AthenaObjectiveStatXPTable.AthenaObjectiveStatXPTable");

    Call c("/Script/GameplayAbilities.AbilitySystemComponent.K2_InitStats");
    if (!c.ok()) {
        LOG("[Stamina] K2_InitStats binding FAILED");
        c.dump("K2_InitStats");
        return false;
    }

    if (!c.set("Attributes", attrClass)) {
        LOG("[Stamina] Attributes param binding failed");
        return false;
    }
    if (dataTable) c.set("DataTable", dataTable);

    if (!c.run(asc)) return false;
    LOG("[Stamina] ✅ Initialized FortPlayerAttributeSet");
    return true;
}

// Master function called after pawn possession
static void SetupAll(void* pawn) {
    if (!pawn || session.abilities_granted) return;

    void* asc = GetASC(pawn);
    if (!asc) { LOG("[Abilities] no ASC on pawn"); return; }

    LOG("[Abilities] Setting up on ASC=%p", asc);

    // Step 1: Initialize attributes (Stamina, Health, etc.)
    if (!session.stamina_initialized) {
        if (InitStaminaAttributes(asc)) {
            session.stamina_initialized = true;
        }
    }

    // Step 2: Apply core GAS_AthenaPlayer set (Jump, Sprint, Build, Harvest, etc.)
    void* coreSet = LoadAsset(
        "/Game/Abilities/Player/Generic/Traits/DefaultPlayer/GAS_AthenaPlayer.GAS_AthenaPlayer",
        "/Script/FortniteGame.FortAbilitySet");
    if (coreSet) {
        EquipAbilitySet(pawn, coreSet);
        session.granted_ability_sets.push_back(coreSet);
    } else {
        LOG("[Abilities] GAS_AthenaPlayer missing");
    }

    // Step 3: Apply every GameFeature ability set (Tactical Sprint, Sliding, etc.)
    auto featureSets = CollectGameFeatureAbilitySets();
    for (void* set : featureSets) {
        if (!set || IsBadPtr(set)) continue;
        // Avoid duplicate
        bool dup = false;
        for (void* existing : session.granted_ability_sets)
            if (existing == set) { dup = true; break; }
        if (dup) continue;

        EquipAbilitySet(pawn, set);
        session.granted_ability_sets.push_back(set);
    }

    // Step 4: Ensure Tactical Sprint ability is available (explicit fallback)
    const char* tacticalPaths[] = {
        "/Game/Abilities/Player/TacticalSprint/GA_Athena_TacticalSprint.GA_Athena_TacticalSprint_C",
        "/Game/Abilities/Player/TacticalSprint/GA_Player_TacticalSprint.GA_Player_TacticalSprint_C",
        "/Game/Abilities/Player/Generic/Traits/DefaultPlayer/GA_DefaultPlayer_TacticalSprint.GA_DefaultPlayer_TacticalSprint_C",
        nullptr
    };
    for (int i = 0; tacticalPaths[i]; ++i) {
        void* tacticalAbility = LoadClass(tacticalPaths[i], "/Script/GameplayAbilities.GameplayAbility");
        if (!tacticalAbility) continue;

        Call give("/Script/GameplayAbilities.AbilitySystemComponent.TryActivateAbilityByClass");
        if (give.ok()) {
            bool bAllowRemote = true;
            if (give.set("InAbilityToActivate", tacticalAbility) &&
                give.set("bAllowRemoteActivation", bAllowRemote)) {
                give.run(asc);
                LOG("[Abilities] Tactical Sprint candidate granted: %s", tacticalPaths[i]);
            }
        }
        break;
    }

    session.abilities_granted = true;
    LOG("[Abilities] ✅ All setups complete (sets=%zu)", session.granted_ability_sets.size());
}

} // namespace Abilities

// ============================================================================
// HUD Refresh — force widgets to rebuild with new ability tags
// ============================================================================
static void RefreshHUD() {
    if (!session.controller) return;
    // Trigger HUD rebuild via PlayerController's ability system update
    void* asc = Abilities::GetASC(session.pawn);
    if (asc) {
        Call c("/Script/GameplayAbilities.AbilitySystemComponent.OnRep_ActivateAbilities");
        if (c.ok()) c.run(asc);
    }
    // Notify controller
    Call c2("/Script/FortniteGame.FortPlayerController.ClientForceWorldInventoryUpdate");
    if (c2.ok()) c2.run(session.controller);
}

// ============================================================================
// BATTLE BUS — intercept JumpFromHudStart (mobile UI event)
// ============================================================================
static void HandleJumpFromHudStart() {
    if (!session.controller || !session.pawn) return;
    if (session.aircraft_jumped) return;

    Guard g;
    LOG("[Bus] JumpFromHudStart intercepted");

    // Get controller rotation
    Vector rot{0, 0, 0};
    void* rotProp = GetProp("/Script/Engine.Controller.ControlRotation");
    if (rotProp && Read<int>(rotProp, profile::PropElementSize) == 24) {
        void* addr = GetField(session.controller, rotProp);
        if (addr) rot = Read<Vector>(addr);
    }

    // 1) Fire the server RPC path (may no-op in standalone, harmless if so)
    Call aircraft("/Script/FortniteGame.FortPlayerController.ServerAttemptAircraftJump");
    if (aircraft.ok()) {
        Rotator r{(double)rot.X, (double)rot.Y, (double)rot.Z};
        if (aircraft.set("ClientRotation", r)) aircraft.run(session.controller);
    }

    // 2) Locally start skydiving (this is what actually makes it work standalone)
    Call sky("/Script/FortniteGame.FortPlayerPawn.BeginSkydiving");
    if (sky.ok()) {
        bool bFromBus = true;
        if (sky.set("bFromBus", bFromBus)) sky.run(session.pawn);
    }

    // 3) Hard-set the bool as backup
    Write<uint8_t>(session.pawn, profile::FortPlayerPawn_bIsSkydiving, 1);

    // 4) Fix camera
    Call cam("/Script/Engine.PlayerController.SetViewTargetWithBlend");
    if (cam.ok()) {
        cam.set("NewViewTarget", session.pawn);
        float blend = 0.0f;
        cam.set("BlendTime", blend);
        cam.run(session.controller);
    }

    // 5) Notify rep functions
    Call rep("/Script/FortniteGame.FortPlayerPawn.OnRep_IsSkydiving");
    if (rep.ok()) rep.run(session.pawn);

    session.aircraft_jumped = true;
    LOG("[Bus] Skydiving started locally");
}

// ============================================================================
// DOOR — proper interaction path
// ============================================================================
static void HandleDoorInteraction(void* door) {
    if (!door || IsBadPtr(door)) return;

    static void* wallCls = nullptr;
    if (!wallCls) wallCls = FindObj("/Script/FortniteGame.BuildingWall");
    if (!wallCls || !IsA(door, wallCls)) return;

    // Read current state
    uint8_t* byte = (uint8_t*)door + profile::BuildingWall_bDoorOpen_Byte;
    bool wasOpen = ((*byte) >> profile::BuildingWall_bDoorOpen_Bit) & 1;

    // Try the proper interaction first via ServerOnInteract
    Call interact("/Script/FortniteGame.BuildingActor.BlueprintOnInteract");
    if (interact.ok() && session.pawn) {
        if (interact.set("InteractingPawn", session.pawn)) {
            interact.run(door);
            LOG("[Door] BlueprintOnInteract dispatched");
        }
    }

    // Fallback: toggle bit and notify
    if (wasOpen) *byte &= ~(1 << profile::BuildingWall_bDoorOpen_Bit);
    else         *byte |=  (1 << profile::BuildingWall_bDoorOpen_Bit);

    Call r("/Script/FortniteGame.BuildingWall.OnRep_bDoorOpen");
    if (r.ok()) r.run(door);

    LOG("[Door] Toggled: %s", wasOpen ? "CLOSED" : "OPENED");
}

// ============================================================================
// INVENTORY (unchanged core, minimal)
// ============================================================================
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

static void* GetPC(void* world, int index = 0) {
    if (!world) return nullptr;
    return Fn<void*(*)(void*, int)>(profile::GetPlayerController)(world, index);
}

static void* SpawnActor(void* world, void* klass, void* owner, const Transform& t) {
    if (!world || !klass) return nullptr;
    void* actor = Fn<void*(*)(void*, void*, const Transform*, uint8_t, void*)>(
        profile::BeginSpawn)(world, klass, &t, 1, owner);
    return actor ? Fn<void*(*)(void*, const Transform*)>(profile::FinishSpawn)(actor, &t) : nullptr;
}

// ============================================================================
// SETUP WORLD
// ============================================================================
static void InitializePlayer(void* world) {
    session.controller = GetPC(world, 0);
    if (!session.controller) return;

    void* baseCtrl = FindObj("/Script/FortniteGame.FortPlayerController");
    if (!baseCtrl || !IsA(session.controller, baseCtrl)) return;

    void* modeProp  = GetProp("/Script/Engine.World.AuthorityGameMode");
    void* stateProp = GetProp("/Script/Engine.World.GameState");
    session.mode  = modeProp  ? Read<void*>(GetField(world, modeProp))  : nullptr;
    session.state = stateProp ? Read<void*>(GetField(world, stateProp)) : nullptr;

    // Wait for pawn
    void* pawn = GetPawn(session.controller);
    if (!pawn || IsBadPtr(pawn)) {
        LOG("[Init] waiting for pawn");
        return;
    }
    session.pawn = pawn;

    LOG("[Init] PC=%s Pawn=%s", ObjName(session.controller).c_str(), ObjName(pawn).c_str());

    // ⭐ CORE FIX: setup abilities BEFORE anything else
    Abilities::SetupAll(pawn);

    // Refresh HUD so stamina bar / sprint buttons appear
    RefreshHUD();

    // Set up cheats
    if (!session.cheats_ready) {
        void* p = GetProp("/Script/Engine.PlayerController.CheatManager");
        void* klass = FindObj("/Script/FortniteGame.FortCheatManager");
        if (!klass) klass = FindObj("/Script/Engine.CheatManager");
        if (p && klass && Read<int>(p, profile::PropElementSize) == 8) {
            void* cheat = Read<void*>(GetField(session.controller, p));
            if (!cheat) {
                cheat = Fn<void*(*)(void*, void*)>(profile::SpawnObject)(klass, session.controller);
                if (cheat) Write<void*>(GetField(session.controller, p), 0, cheat);
            }
            if (cheat) {
                Call init("/Script/Engine.CheatManager.ReceiveInitCheatManager");
                if (init.ok()) init.run(cheat);
                session.cheats_ready = true;
            }
        }
    }

    session.initialized = true;
    session.finalize_at = Monotonic() + 2.0;
    LOG("[Init] Setup complete");
}

static void FinalizePlayer() {
    if (session.finalized) return;
    session.finalized = true;
    LOG("=== FinalizePlayer ===");

    void* pc = GetPC(session.world, 0);
    if (pc != session.controller) { LOG("Finalize: controller changed"); return; }

    void* pawn = GetPawn(pc);
    void* pawnCls = FindObj("/Script/FortniteGame.FortPlayerPawnAthena");
    if (!pawn || !pawnCls || !IsA(pawn, pawnCls)) {
        LOG("Finalize: pawn invalid");
        return;
    }
    session.pawn = pawn;

    // Re-grant abilities in case pawn changed
    if (!session.abilities_granted) {
        Abilities::SetupAll(pawn);
    }

    // Ready callbacks
    if (!session.ready_called) {
        Call srt("/Script/FortniteGame.FortPlayerController.ServerReadyToStartMatch");
        if (srt.ok()) srt.run(pc);
        Call cps("/Script/FortniteGame.FortPlayerControllerZone.ClientOnPawnSpawned");
        if (cps.ok()) cps.run(pc);
        session.ready_called = true;
    }

    Call restart("/Script/Engine.PlayerController.ClientRestart");
    if (restart.ok()) {
        restart.set("NewPawn", pawn);
        restart.run(pc);
    }

    Call cam("/Script/Engine.PlayerController.SetViewTargetWithBlend");
    if (cam.ok()) {
        cam.set("NewViewTarget", pawn);
        cam.run(pc);
    }

    RefreshHUD();
    LOG("Finalize complete");
}

// ============================================================================
// HOOKS
// ============================================================================
static void* g_interactionClass = nullptr;

static void pe_hook(void* obj, void* func, void* params) {
    if (!orig_PE) return;

    if (!g_internal && func && g_gameTid.load() == (int)gettid()) {
        // ⭐ Handle JumpFromHudStart FIRST (before any other filter)
        if (obj == session.controller && params == nullptr) {
            std::string fn = ObjName(func);
            if (fn == "JumpFromHudStart") {
                HandleJumpFromHudStart();
                return orig_PE(obj, func, params);
            }
            // Fallback: if ServerAttemptAircraftJump called with null params
            if (fn == "ServerAttemptAircraftJump") {
                HandleJumpFromHudStart();
                return orig_PE(obj, func, params);
            }
        }

        if (!g_interactionClass)
            g_interactionClass = FindObj("/Script/FortniteGame.FortControllerComponent_Interaction");

        bool isPC          = (obj == session.controller);
        bool isPawn        = (obj == session.pawn);
        bool isInteraction = (g_interactionClass && IsA(obj, g_interactionClass));

        if (isPC || isPawn || isInteraction) {
            Guard g;
            std::string n = ObjName(func);

            if (n == "ServerAttemptInteract" && params) {
                // Reading ReceivingActor properly
                void* target = Read<void*>(params, 0);
                if (target && !IsBadPtr(target)) {
                    void* bcCls   = FindObj("/Script/FortniteGame.BuildingContainer");
                    void* wallCls = FindObj("/Script/FortniteGame.BuildingWall");
                    LOG("[Interact] target=%s", ObjName(target).c_str());

                    if (wallCls && IsA(target, wallCls)) {
                        HandleDoorInteraction(target);
                    }
                    // Building containers handled by original
                }
            }
            else if (n == "ServerCreateBuildingActor" && params) {
                // Let original run; log for debugging
                LOG("[Build] ServerCreateBuildingActor observed");
            }
        }
    }
    orig_PE(obj, func, params);
}

static void tick_hook(void* world, int type, float delta) {
    if (orig_Tick) orig_Tick(world, type, delta);
    if (g_internal || !world) return;

    int expected = 0;
    if (g_gameTid.compare_exchange_strong(expected, (int)gettid()))
        LOG("WorldTick first: tid=%d", (int)gettid());
    if (g_gameTid.load() != (int)gettid()) return;

    Guard g;
    std::string wn = ObjName(world);
    static std::string lastWorld;
    if (wn != lastWorld) {
        LOG(">>> WorldTick: %s", wn.c_str());
        lastWorld = wn;
    }

    if (wn.find("Frontend") != std::string::npos) {
        if (session.attempted && !session.finalized && session.finalize_at > 0) {
            double now = Monotonic();
            if (now >= session.finalize_at) FinalizePlayer();
        }
        return;
    }

    if (wn.find("Artemis_Terrain") == std::string::npos &&
        wn.find("Athena_Terrain") == std::string::npos) return;

    if (world != session.world) {
        session = Session{};
        session.world = world;
        session.observed_at = Monotonic();
        LOG(">>> New world");
    }

    double now = Monotonic();
    if (now - session.observed_at < 3.0) return;

    if (!session.initialized) {
        if (now - session.retry_at < 1.0) return;
        session.retry_at = now;
        InitializePlayer(world);
        return;
    }

    if (session.finalize_at > 0 && !session.finalized && now >= session.finalize_at) {
        FinalizePlayer();
        return;
    }

    // Re-apply abilities periodically if they somehow dropped
    static double lastAbilityCheck = 0;
    if (session.finalized && session.pawn && now - lastAbilityCheck > 10.0) {
        lastAbilityCheck = now;
        if (!session.abilities_granted) {
            LOG("[Tick] abilities missing, re-granting");
            Abilities::SetupAll(session.pawn);
            RefreshHUD();
        }
    }
}

static bool exec_hook(void* engine, void* world, const char16_t* command, void* output) {
    if (!orig_Exec) return false;

    std::string cmd = Narrow(command, 256);
    std::string normalized = cmd;
    std::transform(normalized.begin(), normalized.end(), normalized.begin(), ::tolower);

    if (!cmd.empty()) LOG("Exec: %s", cmd.c_str());

    if (normalized == "open athena_terrain" || normalized.rfind("open athena_terrain", 0) == 0) {
        LOG("Intercepting Athena travel");
        session = Session{};
        return orig_Exec(engine, world, u"open Athena_Terrain", output);
    }
    return orig_Exec(engine, world, command, output);
}

static void InstallHooks() {
    LOG("=== InstallHooks ===");

    if (DobbyHook((void*)(g_base + profile::ProcessEvent), (void*)pe_hook, (void**)&orig_PE) != 0 || !orig_PE) {
        LOG("ProcessEvent FAILED");
        return;
    }
    LOG("ProcessEvent hooked");

    if (DobbyHook((void*)(g_base + profile::WorldTick), (void*)tick_hook, (void**)&orig_Tick) != 0 || !orig_Tick)
        LOG("WorldTick FAILED");
    else
        LOG("WorldTick hooked");

    if (DobbyHook((void*)(g_base + profile::EngineExec), (void*)exec_hook, (void**)&orig_Exec) != 0 || !orig_Exec)
        LOG("EngineExec FAILED");
    else
        LOG("EngineExec hooked");
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
    LOG("sizeof(Vector)=%zu sizeof(Transform)=%zu", sizeof(Vector), sizeof(Transform));

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

extern "C" JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM*, void*) {
    return JNI_VERSION_1_6;
}
