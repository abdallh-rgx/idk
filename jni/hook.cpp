// ============================================================================
// EraCH3 — Fortnite 21.30 CL-21088273 Android
// COMPLETE FINAL BUILD — Full source, no truncation
// All fixes from SDK analysis integrated
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
    for (int i = 0; p[i]; ++i) {
        g_logFile = fopen(p[i], "w");
        if (g_logFile) break;
    }
}
static void LogImpl(const char* fmt, ...) {
    char line[1024];
    va_list ap;
    va_start(ap, fmt);
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
// PROFILE — RVA offsets for CL-21088273
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
constexpr uintptr_t GetDataTableRow_Orig = 0x09319C70;
constexpr uintptr_t CopyScriptStruct     = 0x07535848;

constexpr unsigned ObjectVtable = 0x00;
constexpr unsigned ObjectFlags  = 0x08;
constexpr unsigned ObjectIndex  = 0x0C;
constexpr unsigned ObjectClass  = 0x10;
constexpr unsigned ObjectName   = 0x18;
constexpr unsigned ObjectOuter  = 0x20;

constexpr unsigned FieldNext        = 0x28;
constexpr unsigned StructBaseChain  = 0x30;
constexpr unsigned StructSuper      = 0x40;
constexpr unsigned StructChildren   = 0x48;
constexpr unsigned StructChildProps = 0x50;
constexpr unsigned StructSize       = 0x58;
constexpr unsigned StructMinAlign   = 0x5C;

constexpr unsigned ClassCastFlags     = 0xD8;
constexpr unsigned ClassDefaultObject = 0x110;
constexpr unsigned ClassImplemented   = 0x200;

constexpr unsigned FunctionFlags     = 0xB0;
constexpr unsigned FunctionNumParams = 0xB4;
constexpr unsigned FunctionParamSize = 0xB6;
constexpr unsigned FunctionExecFunc  = 0xD8;

constexpr unsigned PropArrayDim    = 0x30;
constexpr unsigned PropElementSize = 0x34;
constexpr unsigned PropFlags       = 0x38;
constexpr unsigned PropOffset      = 0x44;
constexpr unsigned PropSize        = 0x70;

constexpr unsigned BoolSize        = 0x80;
constexpr unsigned BoolByteOffset  = 0x81;
constexpr unsigned BoolFieldMask   = 0x82;

constexpr unsigned FFieldClass     = 0x08;
constexpr unsigned FFieldOwner     = 0x10;
constexpr unsigned FFieldNext      = 0x20;
constexpr unsigned FFieldName      = 0x28;

constexpr unsigned EnumNames = 0x40;

constexpr uint64_t CPF_Parm       = 0x00000080ULL;
constexpr uint64_t CPF_OutParm    = 0x00000100ULL;
constexpr uint64_t CPF_ConstParm  = 0x00000200ULL;
constexpr uint64_t CPF_ReturnParm = 0x00000400ULL;

// 🔧 SDK-confirmed struct offsets
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
// UE5 STRUCTS (LWC = 24-byte Vector)
// ============================================================================
struct FStringNative { TCHAR* Data; int32 Count; int32 Max; };
struct FUEString     { TCHAR* data; int32 length; int32 capacity; };
struct FName         { uint32 CompIdx; uint32 DispIdx; };
struct Array         { void* Data; int32 Count; int32 Max; };

template<class T> struct TArray {
    T* Data;
    int32 Count;
    int32 Max;
    T& operator[](int i) { return Data[i]; }
    const T& operator[](int i) const { return Data[i]; }
    int Num() const { return Count; }
};

struct Guid    { uint32 A, B, C, D; };
struct Vector  { double X, Y, Z; };
struct Rotator { double Pitch, Yaw, Roll; };

struct ItemAndCount { int32 Count; uint32 _pad; void* Item; };
struct SoftObjRef   { uint8 _pad[0x28]; };

struct alignas(16) Transform {
    double Rotation[4]{0, 0, 0, 1};
    double Location[3]{0, 0, 0};
    uint8_t _padL[8]{};
    double Scale[3]{1, 1, 1};
    uint8_t _padS[8]{};
};

static_assert(sizeof(Vector)    == 24, "Vector must be 24 bytes (LWC)");
static_assert(sizeof(Rotator)   == 24, "Rotator must be 24 bytes (LWC)");
static_assert(sizeof(Transform) == 96, "Transform must be 96 bytes (LWC)");
static_assert(offsetof(Transform, Location) == 0x20, "Transform.Location offset");
static_assert(offsetof(Transform, Scale)    == 0x40, "Transform.Scale offset");

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

// ============================================================================
// FORWARD DECLS
// ============================================================================
static void* FindObj(const std::string& path);
static std::string ObjName(void* obj);
static std::string ObjNameFromFName(FName n);
static std::string ObjNameFromUEString(FUEString* s);
static bool IsA(void* obj, void* type);
static bool SubclassOf(void* klass, void* type);
static void* GetProp(const char* path);
static void* GetField(void* obj, void* prop);
static void SetBool(void* obj, const char* path, bool enabled);
static std::u16string ToWide(const std::string& s);
static std::string Narrow(const char16_t* s, int max);

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

static bool SubclassOf(void* klass, void* type) {
    if (!klass || !type || IsBadPtr(klass) || IsBadPtr(type)) return false;
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

static std::string ObjNameFromFName(FName n) {
    void* klib = FindObj("/Script/Engine.Default__KismetStringLibrary");
    if (!klib) return "";
    return "";
}

static std::string ObjNameFromUEString(FUEString* s) {
    if (!s || !s->data || s->length <= 0) return "";
    return Narrow(s->data, s->length > 512 ? 512 : s->length);
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
// CALL — Reflection-based function invocation
// ============================================================================
class Call {
    void* m_func = nullptr;
    struct Param { std::string name; int offset, size; uint64_t flags; };
    std::vector<Param> m_params;
    alignas(16) std::array<uint8_t, 4096> m_data{};
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
    const std::vector<Param>& params() const { return m_params; }

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
// CORE WRAPPERS
// ============================================================================
static void* LoadClass(const char* path, const char* base_path = "/Script/Engine.Actor") {
    if (void* r = FindObj(path)) return r;
    auto s = ToWide(path);
    void* parent = FindObj(base_path);
    if (!parent) return nullptr;
    return Fn<void*(*)(void*, void*, const char16_t*, const char16_t*, uint32_t, void*)>(
        profile::StaticLoadClass)(parent, nullptr, s.c_str(), nullptr, 0, nullptr);
}

static void* LoadAsset(const std::string& path,
                       const char* type_path = "/Script/FortniteGame.FortItemDefinition") {
    if (void* r = FindObj(path)) return r;
    auto s = ToWide(path);
    void* type = FindObj(type_path);
    if (!type) return nullptr;
    return Fn<void*(*)(void*, void*, const char16_t*, const char16_t*, uint32_t, void*, bool, void*)>(
        profile::StaticLoadObject)(type, nullptr, s.c_str(), nullptr, 0, nullptr, true, nullptr);
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

static void* GetPC(void* world, int index = 0) {
    if (!world) return nullptr;
    return Fn<void*(*)(void*, int)>(profile::GetPlayerController)(world, index);
}

static void* GetObjField(void* obj, const char* path) {
    void* p = GetProp(path);
    if (!p || !obj) return nullptr;
    if (Read<int>(p, profile::PropElementSize) != 8) return nullptr;
    return Read<void*>(GetField(obj, p));
}

static void* SpawnActor(void* world, void* klass, void* owner, const Transform& t) {
    if (!world || !klass) return nullptr;
    void* actor = Fn<void*(*)(void*, void*, const Transform*, uint8_t, void*)>(
        profile::BeginSpawn)(world, klass, &t, 1, owner);
    return actor ? Fn<void*(*)(void*, const Transform*)>(profile::FinishSpawn)(actor, &t) : nullptr;
}

static void* GetByShortName(const std::string& wanted) {
    void* defType = FindObj("/Script/FortniteGame.FortItemDefinition");
    if (!defType) return nullptr;
    GObjects_t* go = *(GObjects_t**)(g_base + profile::GObjects);
    if (!go) return nullptr;
    for (int32 i = 0; i < go->NumElements; ++i) {
        void* o = go->GetById(i);
        if (!o || IsBadPtr(o)) continue;
        void* k = Read<void*>(o, profile::ObjectClass);
        if (k != defType) continue;
        if (ObjName(o) == wanted) return o;
    }
    return nullptr;
}

// ============================================================================
// SESSION STATE
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
    double observed_at = 0;
    double retry_at = 0;
    double finalize_at = 0;
    std::array<void*, 6> slots{};
    std::vector<void*> character_parts;
    void* edit_tool = nullptr;
    void* wall = nullptr;
    void* floor = nullptr;
    void* stairs = nullptr;
    void* roof = nullptr;
} session;

static double Monotonic() {
    timespec ts{};
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return double(ts.tv_sec) + double(ts.tv_nsec) * 1e-9;
}

// ============================================================================
// ATHENA NAMESPACE — All declarations
// ============================================================================
namespace Athena {
    void SetupWorld(void* world);
    void Tick();
    bool TryInitializePlayer(void* world, void* mode, void* controller);
    void SetupCheats();
    void FinalizePlayer();
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
    void GrantEffect(void* effect, float level);
    void ApplySprintSpeed();
    void SpawnSafeZoneIndicator();
    void SpawnPickup(void* def, int count, Vector loc, bool toss = true);
    void DropInventoryItem(Guid guid, int count);
    void OnServerExecuteInventoryItem(Guid guid);
    void OnServerExecuteInventoryWeapon(void* weapon);
    void OnAircraftJump();
    void TeleportToSpawnIsland();
    void RemoveNetDebugUI();
    void SpawnBuildPreviews();
    void ServerHandlePickup(void* pickup);
    void Loot(void* actor);
    void HandleDoor(void* door);
    void OnServerCreateBuildingActor(void* params);
    void OnBeginEditActor(void* piece);
    void OnFinishEditActor(void* actor, void* newClass, int rotIter, bool mirrored);
    void PlayEmoteItem(void* item);
    void OnExitVehicle();
    void CheatScript(const char* script);
    void RestoreHUD();
    void RestoreInput();
    void RestoreAircraft();
    void RestoreVehicleState();
    void RestoreClientSystems();
    void ToggleInventoryHUD();
    void ToggleFullMap();
    void RestoreSprint();
    void SpawnSupplyDrop();

    struct LootData { void* Def; int Count; };
    std::vector<LootData> PickLootDrops(std::string category);
}

// ============================================================================
// 🔧 FIX: BeginPlayerSkydiving via native UFunction
// ============================================================================
void Athena::BeginPlayerSkydiving() {
    if (!session.pawn || IsBadPtr(session.pawn)) return;

    Call bsd("/Script/FortniteGame.FortPlayerPawn.BeginSkydiving");
    if (bsd.ok()) {
        bool bFromBus = true;
        if (bsd.set("bFromBus", bFromBus)) {
            bsd.run(session.pawn);
            LOG("[Skydive] BeginSkydiving(true) dispatched");
        } else {
            bsd.dump("BeginSkydiving");
        }
    } else {
        LOG("[Skydive] BeginSkydiving binding FAILED");
    }

    // Safety
    Write<uint8_t>(session.pawn, profile::FortPlayerPawn_bIsSkydiving, 1);
}

// ============================================================================
// SetupCheats
// ============================================================================
void Athena::SetupCheats() {
    void* pc = session.controller;
    if (!pc) return;
    void* p = GetProp("/Script/Engine.PlayerController.CheatManager");
    void* klass = FindObj("/Script/FortniteGame.FortCheatManager");
    if (!klass) klass = FindObj("/Script/Engine.CheatManager");
    if (!p || !klass || Read<int>(p, profile::PropElementSize) != 8) {
        LOG("[Cheat] property or class missing");
        return;
    }
    void* cheat = Read<void*>(GetField(pc, p));
    if (!cheat) {
        cheat = Fn<void*(*)(void*, void*)>(profile::SpawnObject)(klass, pc);
        if (cheat) Write<void*>(GetField(pc, p), 0, cheat);
    }
    LOG("[Cheat] manager=%p class=%s", cheat,
        ObjName(cheat ? Read<void*>(cheat, profile::ObjectClass) : nullptr).c_str());
    if (cheat) {
        Call init("/Script/Engine.CheatManager.ReceiveInitCheatManager");
        if (init.ok()) init.run(cheat);
        session.cheats_ready = true;
    }
}

// ============================================================================
// 🔧 FIX: HandleDoor with correct offset
// ============================================================================
void Athena::HandleDoor(void* door) {
    if (!door || IsBadPtr(door)) return;

    static void* wallCls = nullptr;
    if (!wallCls) wallCls = FindObj("/Script/FortniteGame.BuildingWall");
    if (!wallCls || !IsA(door, wallCls)) return;

    uint8_t* byte = (uint8_t*)door + profile::BuildingWall_bDoorOpen_Byte;
    bool wasOpen = ((*byte) >> profile::BuildingWall_bDoorOpen_Bit) & 1;
    if (wasOpen) *byte &= ~(1 << profile::BuildingWall_bDoorOpen_Bit);
    else         *byte |=  (1 << profile::BuildingWall_bDoorOpen_Bit);

    LOG("[Door] 0xFDE: %02X -> %s (bit 5)", *byte, wasOpen ? "CLOSED" : "OPENED");

    Call r("/Script/FortniteGame.BuildingWall.OnRep_bDoorOpen");
    if (r.ok()) r.run(door);
}

// ============================================================================
// 🔧 FIX: LoadProfileCosmetics
// ============================================================================
void Athena::LoadProfileCosmetics() {
    if (!session.pawn || !session.controller) return;
    LOG("[Profile] Loading cosmetics from MyPlayerInfo...");

    bool applied = false;
    void* heroType = nullptr;

    // Path 1: MyPlayerInfo @ 0x2088 → TempAthenaMenuHeroInstance @ 0x2D0
    void* info = Read<void*>(session.controller, profile::FortPlayerController_MyPlayerInfo);
    if (info && !IsBadPtr(info)) {
        void* hero = Read<void*>(info, profile::RegisteredPlayerInfo_HeroInstance);
        LOG("[Profile] MyPlayerInfo=%p Hero=%p class=%s", info, hero,
            ObjName(hero ? Read<void*>(hero, profile::ObjectClass) : nullptr).c_str());

        if (hero && !IsBadPtr(hero)) {
            Array heroParts = Read<Array>(hero, profile::FortHero_CharacterParts);
            LOG("[Profile] UFortHero.CharacterParts@0x238: count=%d max=%d", heroParts.Count, heroParts.Max);

            if (heroParts.Count > 0 && heroParts.Data && heroParts.Count < 64) {
                for (int i = 0; i < heroParts.Count; ++i) {
                    void* part = Read<void*>(heroParts.Data, (size_t)i * 8);
                    if (!part || IsBadPtr(part)) continue;
                    void* tp = GetProp("/Script/FortniteGame.CustomCharacterPart.CharacterPartType");
                    if (!tp) continue;
                    char type = Read<char>(GetField(part, tp));
                    Call c("/Script/FortniteGame.FortPlayerPawn.ServerChoosePart");
                    if (c.ok() && c.set("Part", (uint8_t)type) && c.set("ChosenCharacterPart", part)) {
                        c.run(session.pawn);
                        session.character_parts.push_back(part);
                        LOG("[Profile]   part[%d] type=%d: %s", i, (int)type, ObjName(part).c_str());
                    }
                }
                applied = true;
            }

            // GetHeroTypeBP
            Call ght("/Script/FortniteGame.FortHero.GetHeroTypeBP");
            if (ght.ok() && ght.run(hero)) {
                heroType = ght.result<void*>();
                LOG("[Profile] HeroType=%s", ObjName(heroType).c_str());
            }
        }
    }

    // Path 2: FortPlayerState.HeroType @ 0x430
    if (!heroType || IsBadPtr(heroType)) {
        void* st = GetObjField(session.controller, "/Script/Engine.Controller.PlayerState");
        if (st && !IsBadPtr(st)) {
            heroType = Read<void*>(st, profile::FortPlayerState_HeroType);
            LOG("[Profile] Fallback HeroType@0x430 = %s", ObjName(heroType).c_str());

            if (heroType && !IsBadPtr(heroType) && !applied) {
                void* cpProp = GetProp("/Script/FortniteGame.FortHeroType.CharacterParts");
                if (cpProp) {
                    Array parts = Read<Array>(GetField(heroType, cpProp));
                    for (int i = 0; i < parts.Count && i < 16; ++i) {
                        void* part = Read<void*>(parts.Data, (size_t)i * 8);
                        if (!part || IsBadPtr(part)) continue;
                        void* tp = GetProp("/Script/FortniteGame.CustomCharacterPart.CharacterPartType");
                        if (!tp) continue;
                        char type = Read<char>(GetField(part, tp));
                        Call c("/Script/FortniteGame.FortPlayerPawn.ServerChoosePart");
                        if (c.ok() && c.set("Part", (uint8_t)type) && c.set("ChosenCharacterPart", part)) {
                            c.run(session.pawn);
                            session.character_parts.push_back(part);
                        }
                    }
                    applied = session.character_parts.size() > 0;
                }
            }
        }
    }

    // Path 3: Hardcoded fallback
    if (!applied) {
        LOG("[Profile] No profile — using hardcoded defaults");
        LoadDefaultCosmetics();
        return;
    }

    // Inject HeroType + ApplyCharacterCosmetics + UpdateVisualization
    void* state = GetObjField(session.controller, "/Script/Engine.Controller.PlayerState");
    if (state && !IsBadPtr(state)) {
        if (heroType && !IsBadPtr(heroType)) {
            Write<void*>(state, profile::FortPlayerState_HeroType, heroType);
            LOG("[Profile] HeroType injected into PlayerState@0x430");
        }

        if (!session.character_parts.empty()) {
            Array partsArr{session.character_parts.data(),
                           (int32)session.character_parts.size(),
                           (int32)session.character_parts.size()};
            Call apply("/Script/FortniteGame.FortKismetLibrary.ApplyCharacterCosmetics");
            if (apply.ok()
                && apply.set("WorldContextObject", session.world)
                && apply.set("CharacterParts", partsArr)
                && apply.set("PlayerState", state)) {
                void* klib = FindObj("/Script/FortniteGame.Default__FortKismetLibrary");
                if (klib) { apply.run(klib); LOG("[Profile] ApplyCharacterCosmetics dispatched"); }
            } else {
                apply.dump("ApplyCharacterCosmetics");
            }
        }

        Call r1("/Script/FortniteGame.FortPlayerState.OnRep_HeroType");
        if (r1.ok()) r1.run(state);
        Call r2("/Script/FortniteGame.FortPlayerState.OnRep_CharacterParts");
        if (r2.ok()) r2.run(state);
        Call r3("/Script/FortniteGame.FortPlayerState.OnRep_CharacterData");
        if (r3.ok()) r3.run(state);

        // 🔧 THE KEY CALL — UpdatePlayerCustomCharacterPartsVisualization
        void* klib = FindObj("/Script/FortniteGame.Default__FortKismetLibrary");
        if (klib) {
            Call upd("/Script/FortniteGame.FortKismetLibrary.UpdatePlayerCustomCharacterPartsVisualization");
            if (upd.ok() && upd.set("PlayerState", state)) {
                upd.run(klib);
                LOG("[Profile] 🔧 UpdatePlayerCustomCharacterPartsVisualization DISPATCHED");
            } else {
                upd.dump("UpdatePlayerCustomCharacterPartsVisualization");
            }
        }
    }

    Call cl("/Script/FortniteGame.FortPlayerPawn.OnRep_CustomizationLoadout");
    if (cl.ok()) cl.run(session.pawn);

    // Verify mesh
    void* mesh = GetObjField(session.pawn, "/Script/Engine.Character.Mesh");
    if (mesh && !IsBadPtr(mesh)) {
        void* sp = GetProp("/Script/Engine.SkinnedMeshComponent.SkeletalMesh");
        void* skm = sp ? Read<void*>(GetField(mesh, sp)) : nullptr;
        LOG("[Profile] mesh=%p SkeletalMesh=%s", mesh, ObjName(skm).c_str());
    }
}

// ============================================================================
// LoadDefaultCosmetics (hardcoded fallback)
// ============================================================================
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
        if (!asset) { LOG("[Cosmetics] Missing: %s", partPaths[i]); continue; }
        void* partCls = FindObj("/Script/FortniteGame.CustomCharacterPart");
        if (!partCls || !IsA(asset, partCls)) {
            LOG("[Cosmetics] Wrong class: %s", partPaths[i]);
            continue;
        }
        loaded[i] = asset;
        Call choose("/Script/FortniteGame.FortPlayerPawn.ServerChoosePart");
        if (choose.ok() && choose.set("Part", (uint8_t)i) && choose.set("ChosenCharacterPart", asset)) {
            choose.run(session.pawn);
            if (std::find(session.character_parts.begin(), session.character_parts.end(), asset) == session.character_parts.end())
                session.character_parts.push_back(asset);
            LOG("[Cosmetics] Applied part[%d]: %s", i, ObjName(asset).c_str());
        } else {
            choose.dump("ServerChoosePart");
        }
    }

    void* state = GetObjField(session.controller, "/Script/Engine.Controller.PlayerState");
    if (!state || IsBadPtr(state)) return;

    if (loaded[0] && loaded[1]) {
        Array partsArr{loaded, 2, 2};
        Call apply("/Script/FortniteGame.FortKismetLibrary.ApplyCharacterCosmetics");
        if (apply.ok()
            && apply.set("WorldContextObject", session.world)
            && apply.set("CharacterParts", partsArr)
            && apply.set("PlayerState", state)) {
            void* klib = FindObj("/Script/FortniteGame.Default__FortKismetLibrary");
            if (klib) { apply.run(klib); LOG("[Cosmetics] ApplyCharacterCosmetics dispatched"); }
        }

        void* hero = LoadAsset("/Game/Athena/Heroes/HID_001_Athena_Commando_F.HID_001_Athena_Commando_F",
                               "/Script/FortniteGame.FortHeroType");
        if (!hero)
            hero = LoadAsset("/Game/Athena/Heroes/HID_001_Athena_Commando_M.HID_001_Athena_Commando_M",
                             "/Script/FortniteGame.FortHeroType");
        if (hero) {
            Write<void*>(state, profile::FortPlayerState_HeroType, hero);
            LOG("[Cosmetics] HeroType injected: %s", ObjName(hero).c_str());
        }
    }

    Call r1("/Script/FortniteGame.FortPlayerState.OnRep_HeroType");
    if (r1.ok()) r1.run(state);
    Call r2("/Script/FortniteGame.FortPlayerState.OnRep_CharacterParts");
    if (r2.ok()) r2.run(state);
    Call r3("/Script/FortniteGame.FortPlayerState.OnRep_CharacterData");
    if (r3.ok()) r3.run(state);

    void* klib = FindObj("/Script/FortniteGame.Default__FortKismetLibrary");
    if (klib) {
        Call upd("/Script/FortniteGame.FortKismetLibrary.UpdatePlayerCustomCharacterPartsVisualization");
        if (upd.ok() && upd.set("PlayerState", state)) upd.run(klib);
    }

    Call cl("/Script/FortniteGame.FortPlayerPawn.OnRep_CustomizationLoadout");
    if (cl.ok()) cl.run(session.pawn);

    void* mesh = GetObjField(session.pawn, "/Script/Engine.Character.Mesh");
    if (mesh && !IsBadPtr(mesh)) {
        void* sp = GetProp("/Script/Engine.SkinnedMeshComponent.SkeletalMesh");
        void* skm = sp ? Read<void*>(GetField(mesh, sp)) : nullptr;
        LOG("[Cosmetics] mesh=%p SkeletalMesh=%s", mesh, ObjName(skm).c_str());
    }
}

void Athena::ShowSkin() {
    LoadProfileCosmetics();
}

// ============================================================================
// 🔧 FIX: ForcePlayerVisible
// ============================================================================
void Athena::ForcePlayerVisible() {
    if (!session.pawn || IsBadPtr(session.pawn)) return;
    LOG("[Visibility] Forcing player body visible...");

    SetBool(session.pawn, "/Script/FortniteGame.FortPlayerPawn.bIsSkydiving", false);
    SetBool(session.pawn, "/Script/FortniteGame.FortPlayerPawn.bIsParachuteOpen", false);
    SetBool(session.pawn, "/Script/FortniteGame.FortPlayerPawn.bIsParachuteAvailable", true);

    Call sd("/Script/FortniteGame.FortPlayerPawn.OnRep_IsSkydiving");
    if (sd.ok()) sd.run(session.pawn);
    Call po("/Script/FortniteGame.FortPlayerPawn.OnRep_IsParachuteOpen");
    if (po.ok()) po.run(session.pawn);

    Call body("/Script/FortniteGame.FortPlayerPawnAthena.SetCharacterBodyVisibilityForPossession");
    if (body.ok()) {
        bool bShow = true;
        if (body.set("bVisible", bShow)) {
            body.run(session.pawn);
            bool ret = body.result<bool>();
            LOG("[Visibility] SetCharacterBodyVisibilityForPossession(TRUE) -> %d", (int)ret);
        } else {
            body.dump("SetCharacterBodyVisibilityForPossession");
        }
    } else {
        LOG("[Visibility] binding failed");
    }

    Call hid("/Script/Engine.Actor.SetActorHiddenInGame");
    if (hid.ok()) {
        bool bHidden = false;
        if (hid.set("bNewHidden", bHidden)) hid.run(session.pawn);
    }
    SetBool(session.pawn, "/Script/Engine.Actor.bHidden", false);

    ShowSkin();
    session.visibility_forced = true;
    LOG("[Visibility] Done");
}

// ============================================================================
// TryInitializePlayer
// ============================================================================
bool Athena::TryInitializePlayer(void* world, void* mode, void* controller) {
    void* pawnCls = LoadClass("/Game/Athena/PlayerPawn_Athena.PlayerPawn_Athena_C",
                              "/Script/Engine.Pawn");
    if (!pawnCls) { LOG("[Init] PlayerPawn_Athena_C missing"); return false; }

    void* existing = GetPawn(controller);
    if (existing && !IsBadPtr(existing) && IsA(existing, pawnCls)) {
        session.pawn = existing;
        LOG("[Init] Using existing engine pawn: %s", ObjName(existing).c_str());
        return true;
    }

    Transform t{};
    t.Location[0] = 0.0;
    t.Location[1] = 0.0;
    t.Location[2] = 500.0;
    bool haveSpawn = false;

    Call choose("/Script/Engine.GameModeBase.ChoosePlayerStart");
    if (choose.ok() && choose.set("Player", controller) && choose.run(mode)) {
        void* start = choose.result<void*>();
        if (start && !IsBadPtr(start)) {
            Call loc("/Script/Engine.Actor.K2_GetActorLocation");
            if (loc.ok() && loc.run(start)) {
                Vector v = loc.result<Vector>();
                if (std::isfinite(v.X) && std::isfinite(v.Y) && std::isfinite(v.Z)) {
                    t.Location[0] = v.X;
                    t.Location[1] = v.Y;
                    t.Location[2] = v.Z + 100.0;
                    haveSpawn = true;
                    LOG("[Init] PlayerStart chosen at (%.0f,%.0f,%.0f)", v.X, v.Y, v.Z);
                }
            }
        }
    }

    if (!haveSpawn) {
        t.Location[0] = 0.0;
        t.Location[1] = 0.0;
        t.Location[2] = 3100.0;
        LOG("[Init] Fallback spawn (0,0,3100)");
    }

    LOG("[Init] Spawning at (%.0f,%.0f,%.0f)", t.Location[0], t.Location[1], t.Location[2]);
    session.pawn = SpawnActor(world, pawnCls, controller, t);
    if (!session.pawn) { LOG("[Init] Spawn failed"); return false; }
    LOG("[Init] Spawned: %p (%s)", session.pawn, ObjName(session.pawn).c_str());

    SetBool(session.pawn, "/Script/FortniteGame.FortPlayerPawn.bIsSkydiving", false);
    SetBool(session.pawn, "/Script/FortniteGame.FortPlayerPawn.bIsParachuteOpen", false);

    Possess(controller, session.pawn);
    if (GetPawn(controller) != session.pawn) {
        LOG("[Init] Possession did not stick");
        return false;
    }
    LOG("[Init] Possessed OK");

    SetupCheats();

    Call sd("/Script/FortniteGame.FortPlayerPawn.OnRep_IsSkydiving");
    if (sd.ok()) sd.run(session.pawn);
    Call po("/Script/FortniteGame.FortPlayerPawn.OnRep_IsParachuteOpen");
    if (po.ok()) po.run(session.pawn);

    Call restart("/Script/Engine.PlayerController.ClientRestart");
    if (restart.ok() && restart.set("NewPawn", session.pawn))
        restart.run(controller);

    ForcePlayerVisible();
    LOG("[Init] Pawn ready: %p", session.pawn);
    return true;
}

// ============================================================================
// SetupWorld
// ============================================================================
void Athena::SetupWorld(void* world) {
    session.world = world;
    session.controller = GetPC(world, 0);
    if (!session.controller) { LOG("SetupWorld: no controller"); return; }

    void* baseCtrl = FindObj("/Script/FortniteGame.FortPlayerController");
    if (!baseCtrl || !IsA(session.controller, baseCtrl)) {
        LOG("SetupWorld: controller is not FortPlayerController");
        return;
    }

    void* modeProp  = GetProp("/Script/Engine.World.AuthorityGameMode");
    void* stateProp = GetProp("/Script/Engine.World.GameState");
    session.mode  = modeProp  ? Read<void*>(GetField(world, modeProp))  : nullptr;
    session.state = stateProp ? Read<void*>(GetField(world, stateProp)) : nullptr;
    if (!session.mode || !session.state) { LOG("SetupWorld: mode/state missing"); return; }

    LOG("SetupWorld: PC=%s GM=%s GS=%s",
        ObjName(session.controller).c_str(),
        ObjName(session.mode).c_str(),
        ObjName(session.state).c_str());

    if (!TryInitializePlayer(world, session.mode, session.controller)) {
        LOG("SetupWorld: prerequisites not ready, will retry");
        return;
    }

    LOG("SetupWorld: ShowSkin");
    ShowSkin();

    LOG("SetupWorld: DestroyLods");
    DestroyLods();

    LOG("SetupWorld: InitializeInventory");
    InitializeInventory();

    LOG("SetupWorld: GrantDefaultAbilities");
    GrantDefaultAbilities();

    LOG("SetupWorld: ApplySprintSpeed");
    ApplySprintSpeed();

    LOG("SetupWorld: SpawnSafeZoneIndicator");
    SpawnSafeZoneIndicator();

    LOG("SetupWorld: SpawnBuildPreviews");
    SpawnBuildPreviews();

    LOG("SetupWorld: DropLoadingScreen");
    DropLoadingScreen();

    session.finalize_at = Monotonic() + 2.0;
    session.attempted   = true;
    session.initialized = true;
    LOG("SetupWorld: initial pass done, finalize in 2s (pawn=%p)", session.pawn);
}

// ============================================================================
// FinalizePlayer
// ============================================================================
void Athena::FinalizePlayer() {
    if (session.finalized) return;
    session.finalized = true;
    LOG("=== FinalizePlayer ===");

    void* pc = GetPC(session.world, 0);
    if (pc != session.controller) { LOG("Finalize: controller changed"); return; }

    void* pawnCls = FindObj("/Script/FortniteGame.FortPlayerPawnAthena");
    void* pawn = GetPawn(pc);
    LOG("Finalize: engine pawn=%p old=%p class=%s",
        pawn, session.pawn,
        ObjName(pawn ? Read<void*>(pawn, profile::ObjectClass) : nullptr).c_str());

    if (!pawn || !pawnCls || !IsA(pawn, pawnCls)) {
        LOG("Finalize: pawn is not FortPlayerPawnAthena");
        return;
    }
    session.pawn = pawn;

    Call loc("/Script/Engine.Actor.K2_GetActorLocation");
    if (loc.ok() && loc.run(session.pawn)) {
        Vector v = loc.result<Vector>();
        LOG("Finalize: pawn at (%.0f,%.0f,%.0f)", v.X, v.Y, v.Z);

        if (v.Z < -1000.0) {
            LOG("Finalize: pawn underground — snapping to PlayerStart");
            Call choose("/Script/Engine.GameModeBase.ChoosePlayerStart");
            if (choose.ok() && choose.set("Player", pc) && choose.run(session.mode)) {
                void* start = choose.result<void*>();
                if (start && !IsBadPtr(start)) {
                    Call sloc("/Script/Engine.Actor.K2_GetActorLocation");
                    if (sloc.ok() && sloc.run(start)) {
                        Vector sv = sloc.result<Vector>();
                        Rotator rot{0.0, 0.0, 0.0};
                        Call cs("/Script/Engine.Controller.ClientSetLocation");
                        if (cs.ok() && cs.set("NewLocation", sv) && cs.set("NewRotation", rot))
                            cs.run(pc);
                        LOG("Finalize: snapped to (%.0f,%.0f,%.0f)", sv.X, sv.Y, sv.Z);
                    }
                }
            }
        } else {
            LOG("Finalize: location OK (not teleporting)");
        }
    }

    if (!session.ready_called) {
        LOG("Finalize: ServerReadyToStartMatch + ClientOnPawnSpawned");
        Call srt("/Script/FortniteGame.FortPlayerController.ServerReadyToStartMatch");
        if (srt.ok()) srt.run(pc);
        Call cps("/Script/FortniteGame.FortPlayerControllerZone.ClientOnPawnSpawned");
        if (cps.ok()) cps.run(pc);
        session.ready_called = true;
    }

    Call restart("/Script/Engine.PlayerController.ClientRestart");
    if (restart.ok() && restart.set("NewPawn", pawn)) restart.run(pc);

    Call cam("/Script/Engine.PlayerController.SetViewTargetWithBlend");
    if (cam.ok() && cam.set("NewViewTarget", pawn)) cam.run(pc);

    session.visibility_forced = false;
    ForcePlayerVisible();
    LOG("Finalize: complete, visible=%d", (int)session.visibility_forced);
}

// ============================================================================
// Client restoration layer — keep the real Fortnite client systems alive.
// ============================================================================
void Athena::RestoreHUD() {
    if (!session.controller) return;

    if (session.mode) {
        Call init("/Script/Engine.GameModeBase.InitializeHUDForPlayer");
        if (init.ok() && init.set("NewPlayer", session.controller)) {
            init.run(session.mode);
            LOG("[HUD] InitializeHUDForPlayer dispatched");
        }
    }

    void* hud = GetObjField(session.controller, "/Script/Engine.PlayerController.HUD");
    if (hud) {
        Call show("/Script/Engine.HUD.ShowHUD");
        if (show.ok()) {
            show.run(hud);
            LOG("[HUD] ShowHUD dispatched");
        }
    }

    Call localInv("/Script/FortniteGame.FortInventory.HandleInventoryLocalUpdate");
    if (localInv.ok() && session.inventory) localInv.run(session.inventory);

    Call worldInv("/Script/FortniteGame.FortPlayerController.HandleWorldInventoryLocalUpdate");
    if (worldInv.ok()) worldInv.run(session.controller);

    Call quick("/Script/FortniteGame.FortPlayerController.ClientForceUpdateQuickbar");
    if (quick.ok()) {
        uint8_t bar = 0;
        quick.set("QuickbarToRefresh", bar);
        quick.run(session.controller);
    }

    Call sec("/Script/FortniteGame.FortQuickBars.OnRep_SecondaryQuickBar");
    if (sec.ok() && session.quickbars) sec.run(session.quickbars);
    Call prim("/Script/FortniteGame.FortQuickBars.OnRep_PrimaryQuickBar");
    if (prim.ok() && session.quickbars) prim.run(session.quickbars);
}

void Athena::RestoreInput() {
    if (!session.controller) return;

    SetBool(session.controller, "/Script/Engine.PlayerController.bShowMouseCursor", false);
    SetBool(session.controller, "/Script/Engine.PlayerController.bEnableClickEvents", false);
    SetBool(session.controller, "/Script/Engine.PlayerController.bEnableMouseOverEvents", false);

    Call move("/Script/Engine.Controller.SetIgnoreMoveInput");
    if (move.ok()) {
        bool disabled = false;
        if (move.set("bNewMoveInput", disabled)) move.run(session.controller);
    }

    Call look("/Script/Engine.Controller.SetIgnoreLookInput");
    if (look.ok()) {
        bool disabled = false;
        if (look.set("bNewLookInput", disabled)) look.run(session.controller);
    }

    GrantDefaultAbilities();
    ApplySprintSpeed();
}

void Athena::RestoreAircraft() {
    if (!session.controller) return;

    Call get("/Script/FortniteGame.FortPlayerController.GetAircraftComponent");
    if (!get.ok() || !get.run(session.controller)) return;
    void* comp = get.result<void*>();
    if (!comp || IsBadPtr(comp)) return;

    void* aircraft = nullptr;
    void* aircraftProp = GetProp("/Script/FortniteGame.FortControllerComponent_Aircraft.CurrentAircraft");
    if (aircraftProp) aircraft = Read<void*>(GetField(comp, aircraftProp));

    if (!aircraft) {
        void* aircraftCls = FindObj("/Script/FortniteGame.FortAthenaAircraft");
        GObjects_t* go = *(GObjects_t**)(g_base + profile::GObjects);
        if (go && aircraftCls) {
            for (int32 i = 0; i < go->NumElements; ++i) {
                void* o = go->GetById(i);
                if (!o || IsBadPtr(o)) continue;
                if (IsA(o, aircraftCls)) { aircraft = o; break; }
            }
        }
    }

    if (aircraft) {
        Call enter("/Script/FortniteGame.FortControllerComponent_Aircraft.ClientEnterAircraft");
        if (enter.ok() && enter.set("InAircraft", aircraft)) {
            enter.run(comp);
            LOG("[Bus] ClientEnterAircraft restored");
        }
    }

}

void Athena::RestoreVehicleState() {
    if (!session.pawn) return;

    Call get("/Script/FortniteGame.FortPlayerPawn.GetVehicleActor");
    if (!get.ok() || !get.run(session.pawn)) return;
    void* vehicle = get.result<void*>();
    if (!vehicle || IsBadPtr(vehicle)) return;

    Call entered("/Script/FortniteGame.FortPlayerPawn.OnEnteredVehicle");
    if (entered.ok()) entered.run(session.pawn);

    Call ack("/Script/FortniteGame.FortPlayerPawn.ClientAcknowledgeVehicleInputState");
    if (ack.ok()) {
        float timestamp = (float)Monotonic();
        if (ack.set("Timestamp", timestamp)) ack.run(session.pawn);
    }
}

void Athena::RestoreClientSystems() {
    if (!session.finalized || !session.controller || !session.pawn) return;

    RestoreHUD();
    RestoreInput();
    RestoreAircraft();
    RestoreVehicleState();
    RestoreSprint();
    RemoveNetDebugUI();
    FixBuildingFoundations();
    SpawnSafeZoneIndicator();
    InventoryUpdate();
}

void Athena::ToggleInventoryHUD() {
    if (!session.controller) return;
    Call c("/Script/FortniteGame.FortPlayerController.ToggleInventory");
    if (c.ok()) {
        c.run(session.controller);
        LOG("[HUD] ToggleInventory dispatched");
    }
}

void Athena::RestoreSprint() {
    if (!session.controller || !session.pawn) return;

    void* sprint = FindObj("/Script/FortniteGame.FortGameplayAbility_Sprint");
    if (sprint) GrantAbility(sprint);

    Call behavior("/Script/FortniteGame.FortPlayerController.SetSprintBehaviorIsOverridden");
    if (behavior.ok()) {
        bool overridden = true;
        bool sprintByDefault = true;
        if (behavior.set("bNewSprintBehaviorIsOverridden", overridden) &&
            behavior.set("bNewSprintByDefaultWhenSprintIsOverridden", sprintByDefault))
            behavior.run(session.controller);
    }

    Call toggle("/Script/FortniteGame.FortPlayerController.ServerUpdateSprintToggleable");
    if (toggle.ok()) {
        bool toggleable = true;
        if (toggle.set("bShouldBeToggleable", toggleable)) toggle.run(session.controller);
    }

    ApplySprintSpeed();
    LOG("[Sprint] Sprint ability/input restored");
}

void Athena::SpawnSupplyDrop() {
    if (!session.world || !session.pawn) return;

    void* cls = LoadClass("/Script/FortniteGame.FortAthenaSupplyDrop", "/Script/Engine.Actor");
    if (!cls) {
        LOG("[SupplyDrop] FortAthenaSupplyDrop class missing");
        return;
    }

    Call getLoc("/Script/Engine.Actor.K2_GetActorLocation");
    Vector loc{0.0, 0.0, 1000.0};
    if (getLoc.ok() && getLoc.run(session.pawn)) loc = getLoc.result<Vector>();
    loc.Z += 2500.0;

    Transform t{};
    t.Location[0] = loc.X;
    t.Location[1] = loc.Y;
    t.Location[2] = loc.Z;

    void* drop = SpawnActor(session.world, cls, session.pawn, t);
    if (!drop) {
        LOG("[SupplyDrop] spawn failed");
        return;
    }

    Call ground("/Script/FortniteGame.FortAthenaSupplyDrop.FindGroundLocationAt");
    if (ground.ok() && ground.set("InLocation", loc) && ground.run(drop)) {
        Vector g = ground.result<Vector>();
        Call setLoc("/Script/Engine.Actor.K2_SetActorLocation");
        if (setLoc.ok()) {
            bool sweep = false;
            setLoc.set("NewLocation", g);
            setLoc.set("bSweep", sweep);
            setLoc.run(drop);
            loc = g;
        }
    }

    Call loot("/Script/FortniteGame.FortAthenaSupplyDrop.OnLootSpawned");
    if (loot.ok()) loot.run(drop);

    LOG("[SupplyDrop] spawned at %.0f %.0f %.0f", loc.X, loc.Y, loc.Z);
}

void Athena::ToggleFullMap() {
    if (!session.controller) return;
    Call c("/Script/FortniteGame.FortPlayerController.ToggleFullScreenMap");
    if (c.ok()) {
        c.run(session.controller);
        LOG("[Map] ToggleFullScreenMap dispatched");
    }
}

// ============================================================================
// Tick
// ============================================================================
void Athena::Tick() {
    if (!session.world) return;
    double now = Monotonic();
    if (!session.attempted) {
        if (now - session.retry_at < 1.0) return;
        session.retry_at = now;
        LOG(">>> Tick: retrying SetupWorld");
        SetupWorld(session.world);
        return;
    }
    if (session.finalize_at > 0 && !session.finalized && now >= session.finalize_at) {
        FinalizePlayer();
        return;
    }
    static double lastVisibility = 0;
    static double lastSpeedReapply = 0;
    static double lastClientRestore = 0;
    if (session.finalized && now - lastClientRestore > 5.0) {
        lastClientRestore = now;
        RestoreClientSystems();
    }
    if (session.finalized && now - lastVisibility > 8.0) {
        lastVisibility = now;
        if (!session.visibility_forced) {
            LOG(">>> Tick: visibility not confirmed, retrying");
            ForcePlayerVisible();
        }
    }
    if (session.finalized && session.pawn && now - lastSpeedReapply > 2.0) {
        lastSpeedReapply = now;
        ApplySprintSpeed();
    }
}

// ============================================================================
// DestroyLods
// ============================================================================
void Athena::DestroyLods() {
    void* cm = GetObjField(session.controller, "/Script/Engine.PlayerController.CheatManager");
    if (!cm) {
        LOG("DestroyLods: no CheatManager, creating...");
        SetupCheats();
        cm = GetObjField(session.controller, "/Script/Engine.PlayerController.CheatManager");
    }
    if (!cm) { LOG("DestroyLods: still no CheatManager"); return; }
    void* cls = FindObj("/Script/FortniteGame.FortHLODSMActor");
    if (!cls) return;
    Call c("/Script/Engine.CheatManager.DestroyAll");
    if (c.ok() && c.set("aClass", cls)) c.run(cm);
    LOG("DestroyLods: done");
}

// ============================================================================
// DropLoadingScreen
// ============================================================================
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

    // Playlist setup
    void* playlist = FindObj("/Game/Athena/Playlists/Playlist_DefaultSolo.Playlist_DefaultSolo");
    if (!playlist) playlist = FindObj("/Game/Athena/Playlists/Playground/Playlist_Playground.Playlist_Playground");
    if (playlist) {
        void* bp = GetProp("/Script/FortniteGame.FortGameStateAthena.BasePlaylist");
        void* op = GetProp("/Script/FortniteGame.FortGameStateAthena.OverridePlaylist");
        void* cp = GetProp("/Script/FortniteGame.FortGameStateAthena.CurrentPlaylistData");
        if (bp) Write<void*>(GetField(session.state, bp), 0, playlist);
        if (op) Write<void*>(GetField(session.state, op), 0, playlist);
        if (cp) Write<void*>(GetField(session.state, cp), 0, playlist);
        Call a("/Script/FortniteGame.FortGameStateAthena.OnRep_CurrentPlaylistData");
        if (a.ok()) a.run(session.state);
        Call b("/Script/FortniteGame.FortGameStateAthena.OnRep_CurrentPlaylistInfo");
        if (b.ok()) b.run(session.state);
    }

    if (!session.ready_called) {
        Call srt("/Script/FortniteGame.FortPlayerController.ServerReadyToStartMatch");
        if (srt.ok()) srt.run(session.controller);
        Call cps("/Script/FortniteGame.FortPlayerControllerZone.ClientOnPawnSpawned");
        if (cps.ok()) cps.run(session.controller);
        session.ready_called = true;
        LOG("DropLoadingScreen: ready callbacks dispatched");
    }

    LOG("Loading screen dropped");
}

// ============================================================================
// FixBuildingFoundations
// ============================================================================
void Athena::FixBuildingFoundations() {
    void* cls = FindObj("/Script/FortniteGame.BuildingFoundation");
    if (!cls || !session.world) return;
    GObjects_t* go = *(GObjects_t**)(g_base + profile::GObjects);
    if (!go) return;
    void* p = GetProp("/Script/FortniteGame.BuildingFoundation.DynamicFoundationType");
    if (!p) return;
    int propOff = Read<int>(p, profile::PropOffset);
    if (propOff < 0 || propOff > 0x20000) return;
    int fixed = 0;
    for (int32 i = 0; i < go->NumElements; ++i) {
        void* o = go->GetById(i);
        if (!o || IsBadPtr(o)) continue;
        void* k = Read<void*>(o, profile::ObjectClass);
        if (k != cls) continue;
        Write<char>((char*)o + propOff, 0, 0);
        ++fixed;
    }
    LOG("FixBuildingFoundations: fixed=%d", fixed);
}

// ============================================================================
// FixBus
// ============================================================================
void Athena::FixBus() {
    if (!session.state) return;
    void* gp = GetProp("/Script/FortniteGame.FortGameStateAthena.GamePhase");
    if (gp) {
        Write<uint8_t>(GetField(session.state, gp), 0, 3);
        Call n("/Script/FortniteGame.FortGameStateAthena.OnRep_GamePhase");
        if (n.ok()) { n.set("OldGamePhase", (uint8_t)2); n.run(session.state); }
    }
    if (!session.pawn) return;
    Vector loc{0.0, 0.0, 22500.0};
    Rotator rot{0.0, 0.0, 0.0};
    Call sl("/Script/Engine.Actor.K2_SetActorLocationAndRotation");
    if (sl.ok()) {
        bool bSweep = false, bTeleport = true;
        sl.set("NewLocation", loc);
        sl.set("NewRotation", rot);
        sl.set("bSweep", bSweep);
        sl.set("bTeleport", bTeleport);
        sl.run(session.pawn);
    }
    BeginPlayerSkydiving();
}

// ============================================================================
// InventoryUpdate
// ============================================================================
void Athena::InventoryUpdate() {
    if (session.inventory) {
        Call a("/Script/FortniteGame.FortInventory.HandleInventoryLocalUpdate");
        if (a.ok()) a.run(session.inventory);
    }
    { Call b("/Script/FortniteGame.FortPlayerController.HandleWorldInventoryLocalUpdate"); if (b.ok()) b.run(session.controller); }
    { Call c("/Script/FortniteGame.FortPlayerController.OnRep_QuickBar"); if (c.ok()) c.run(session.controller); }
    if (session.quickbars) {
        Call d("/Script/FortniteGame.FortQuickBars.OnRep_SecondaryQuickBar");
        if (d.ok()) d.run(session.quickbars);
        Call e("/Script/FortniteGame.FortQuickBars.OnRep_PrimaryQuickBar");
        if (e.ok()) e.run(session.quickbars);
    }
    for (uint8_t bar : {uint8_t(0), uint8_t(1)}) {
        Call c("/Script/FortniteGame.FortPlayerController.ClientForceUpdateQuickbar");
        if (c.ok() && c.set("QuickbarToRefresh", bar)) c.run(session.controller);
    }
}

// ============================================================================
// AddToInventory with FFortItemEntry initialization
// ============================================================================
void Athena::AddToInventory(void* def, int count, int slot) {
    if (IsBadPtr(def) || !session.inventory || !session.quickbars) return;

    void* inst = nullptr;
    {
        Call c("/Script/FortniteGame.FortItemDefinition.CreateTemporaryItemInstanceBP");
        int lvl = 1;
        if (!c.ok() || !c.set("Count", count) || !c.set("Level", lvl) || !c.run(def)) {
            LOG("AddToInventory: CreateTemporaryItemInstanceBP failed");
            return;
        }
        inst = c.result<void*>();
    }
    if (!inst || IsBadPtr(inst)) return;

    {
        Call c("/Script/FortniteGame.FortItem.SetOwningControllerForTemporaryItem");
        if (c.ok() && c.set("InController", session.controller)) c.run(inst);
    }

    void* invProp   = GetProp("/Script/FortniteGame.FortInventory.Inventory");
    void* instProp  = GetProp("/Script/FortniteGame.FortItemList.ItemInstances");
    void* entryProp = GetProp("/Script/FortniteGame.FortItemList.ReplicatedEntries");
    void* itemEntryProp = GetProp("/Script/FortniteGame.FortWorldItem.ItemEntry");
    void* itemGuidProp  = GetProp("/Script/FortniteGame.FortItemEntry.ItemGuid");
    if (!invProp || !instProp || !entryProp || !itemEntryProp) return;

    void* list = GetField(session.inventory, invProp);
    void* instArr  = GetField(list, instProp);
    void* entryArr = GetField(list, entryProp);
    void* entrySrc = GetField(inst, itemEntryProp);

    // 🔧 FIX 1: Fill FFortItemEntry fields
    if (entrySrc) {
        Write<float>(entrySrc, profile::ItemEntry_Durability, 1.0f);

        void* weapCls = FindObj("/Script/FortniteGame.FortWeaponItemDefinition");
        if (weapCls && IsA(def, weapCls)) {
            Write<int32>(entrySrc, profile::ItemEntry_LoadedAmmo, 30);
            void* phProp = GetProp("/Script/FortniteGame.FortWeaponItemDefinition.bUsesPhantomReserveAmmo");
            if (phProp) Write<int32>(entrySrc, profile::ItemEntry_PhantomAmmo, 300);
        }
        LOG("[FFortItemEntry] Init for %s: Durability=1.0", ObjName(def).c_str());
    }

    Fn<int(*)(void*,void*,const void*)>(profile::GenericArrayAdd)(instArr, instProp, &inst);
    Fn<int(*)(void*,void*,const void*)>(profile::GenericArrayAdd)(entryArr, entryProp, entrySrc);

    // GUID generation
    Guid itemGuid{0, 0, 0, 0};
    if (itemGuidProp) {
        itemGuid = Read<Guid>(GetField(entrySrc, itemGuidProp));
        if (!(itemGuid.A | itemGuid.B | itemGuid.C | itemGuid.D)) {
            void* klib = FindObj("/Script/Engine.Default__KismetGuidLibrary");
            if (klib) {
                Call g("/Script/Engine.KismetGuidLibrary.NewGuid");
                if (g.ok() && g.run(klib)) {
                    itemGuid = g.result<Guid>();
                    Write<Guid>(GetField(entrySrc, itemGuidProp), 0, itemGuid);
                    LOG("[GUID] Generated for %s", ObjName(def).c_str());
                }
            }
        }
    }

    if (slot >= 0 && slot < 6) session.slots[slot] = inst;

    void* primBarProp = GetProp("/Script/FortniteGame.FortQuickBars.PrimaryQuickBar");
    if (primBarProp) {
        void* primBar = GetField(session.quickbars, primBarProp);
        void* slotsProp = GetProp("/Script/FortniteGame.FortQuickBar.Slots");
        if (slotsProp) {
            Array slotsArr = Read<Array>(GetField(primBar, slotsProp));
            int slotSize = Read<int>(slotsProp, profile::PropElementSize);
            if (slot >= 0 && slot < slotsArr.Count && slotSize > 0) {
                void* slotObj = (char*)slotsArr.Data + slot * slotSize;
                void* itemsProp = GetProp("/Script/FortniteGame.FortQuickBarSlot.Items");
                void* enabledProp = GetProp("/Script/FortniteGame.FortQuickBarSlot.bEnabled");
                if (itemsProp) {
                    void* itemsArr = GetField(slotObj, itemsProp);
                    Fn<int(*)(void*,void*,const void*)>(profile::GenericArrayAdd)(itemsArr, itemsProp, &itemGuid);
                }
                if (enabledProp) {
                    uint8_t byte = Read<uint8_t>(enabledProp, profile::BoolByteOffset);
                    uint8_t mask = Read<uint8_t>(enabledProp, profile::BoolFieldMask);
                    ((uint8_t*)slotObj)[byte] |= mask;
                }
            }
        }
    }

    Call onrep("/Script/FortniteGame.FortQuickBars.OnRep_PrimaryQuickBar");
    if (onrep.ok()) onrep.run(session.quickbars);
    Call onrepSec("/Script/FortniteGame.FortQuickBars.OnRep_SecondaryQuickBar");
    if (onrepSec.ok()) onrepSec.run(session.quickbars);
    Call hiu("/Script/FortniteGame.FortInventory.HandleInventoryLocalUpdate");
    if (hiu.ok()) hiu.run(session.inventory);
    Call hwi("/Script/FortniteGame.FortPlayerController.HandleWorldInventoryLocalUpdate");
    if (hwi.ok()) hwi.run(session.controller);

    LOG("Inv+ %s x%d slot=%d", ObjName(def).c_str(), count, slot);
}

// ============================================================================
// InitializeInventory
// ============================================================================
void Athena::InitializeInventory() {
    if (!session.controller) return;
    void* invProp = GetProp("/Script/FortniteGame.FortPlayerController.WorldInventory");
    if (!invProp) { LOG("InitializeInventory: WorldInventory prop missing"); return; }
    session.inventory = Read<void*>(GetField(session.controller, invProp));
    if (!session.inventory) {
        void* klass = FindObj("/Script/FortniteGame.FortInventory");
        Transform t{};
        session.inventory = SpawnActor(session.world, klass, session.controller, t);
        if (session.inventory) Write<void*>(GetField(session.controller, invProp), 0, session.inventory);
    }
    if (!session.inventory) { LOG("No inventory"); return; }

    void* qbProp = GetProp("/Script/FortniteGame.FortPlayerController.ClientQuickBars");
    if (qbProp) {
        session.quickbars = Read<void*>(GetField(session.controller, qbProp));
        if (!session.quickbars) {
            void* klass = FindObj("/Script/FortniteGame.FortQuickBars");
            Transform t{};
            session.quickbars = SpawnActor(session.world, klass, session.controller, t);
            if (session.quickbars) Write<void*>(GetField(session.controller, qbProp), 0, session.quickbars);
        }
    }
    if (session.quickbars) {
        Call so("/Script/FortniteGame.FortQuickBars.SetOwner");
        if (so.ok()) so.run(session.quickbars);
    }
    { Call so("/Script/FortniteGame.FortInventory.SetOwner"); if (so.ok()) so.run(session.inventory); }

    LOG("Inventory ready: inv=%p qb=%p", session.inventory, session.quickbars);

    struct DefSlot { const char* path; int slot; };
    DefSlot defaults[] = {
        {"/Game/Athena/Items/Weapons/WID_Harvest_Pickaxe_Athena_C_T01.WID_Harvest_Pickaxe_Athena_C_T01", 0},
        {"/Game/Athena/Items/Weapons/WID_Assault_Auto_Athena_C_Ore_T03.WID_Assault_Auto_Athena_C_Ore_T03", 1},
        {"/Game/Items/Weapons/BuildingTools/BuildingItemData_Wall.BuildingItemData_Wall", 2},
        {"/Game/Items/Weapons/BuildingTools/BuildingItemData_Floor.BuildingItemData_Floor", 3},
        {"/Game/Items/Weapons/BuildingTools/BuildingItemData_Stair_W.BuildingItemData_Stair_W", 4},
        {"/Game/Items/Weapons/BuildingTools/BuildingItemData_RoofS.BuildingItemData_RoofS", 5},
    };
    for (auto& d : defaults) {
        void* def = LoadAsset(d.path);
        if (!def) def = FindObj(d.path);
        if (!def) continue;
        AddToInventory(def, 1, d.slot);
    }
    void* editTool = LoadAsset("/Game/Items/Weapons/BuildingTools/EditTool.EditTool");
    if (editTool) session.edit_tool = editTool;

    const char* ammo[] = {
        "/Game/Athena/Items/Ammo/AthenaAmmoDataBulletsLight.AthenaAmmoDataBulletsLight",
        "/Game/Athena/Items/Ammo/AthenaAmmoDataBulletsMedium.AthenaAmmoDataBulletsMedium",
        "/Game/Athena/Items/Ammo/AthenaAmmoDataBulletsHeavy.AthenaAmmoDataBulletsHeavy",
        "/Game/Athena/Items/Ammo/AthenaAmmoDataShells.AthenaAmmoDataShells",
        "/Game/Athena/Items/Ammo/AmmoDataRockets.AmmoDataRockets", nullptr};
    for (int i = 0; ammo[i]; ++i) {
        void* def = LoadAsset(ammo[i]);
        if (def) AddToInventory(def, 999, 10);
    }

    struct ResDef { const char* path; int slot; };
    ResDef resources[] = {
        {"/Game/Items/ResourcePickups/WoodItemData.WoodItemData", 10},
        {"/Game/Items/ResourcePickups/StoneItemData.StoneItemData", 10},
        {"/Game/Items/ResourcePickups/MetalItemData.MetalItemData", 10},
    };
    for (auto& r : resources) {
        void* def = LoadAsset(r.path);
        if (def) AddToInventory(def, 500, r.slot);
    }

    InventoryUpdate();
    session.inventory_ready = true;
    EquipSlot(0);
}

// ============================================================================
// 🔧 FIX 2: EquipSlot with 4 params
// ============================================================================
void Athena::EquipSlot(int slot) {
    if (slot < 0 || slot >= 6 || !session.slots[slot]) {
        LOG("EquipSlot[%d]: empty", slot);
        return;
    }
    void* item = session.slots[slot];

    void* def = nullptr;
    {
        Call d("/Script/FortniteGame.FortItem.GetItemDefinitionBP");
        if (d.ok() && d.run(item)) def = d.result<void*>();
    }
    if (!def) { LOG("EquipSlot[%d]: no definition", slot); return; }

    Guid guid{};
    {
        Call g("/Script/FortniteGame.FortItem.GetItemGuid");
        if (g.ok() && g.run(item)) guid = g.result<Guid>();
    }

    void* itemEntryProp = GetProp("/Script/FortniteGame.FortWorldItem.ItemEntry");
    Guid trackerGuid{};
    if (itemEntryProp) {
        void* entrySrc = GetField(item, itemEntryProp);
        if (entrySrc) trackerGuid = Read<Guid>(entrySrc, profile::ItemEntry_TrackerGuid);
    }

    Call eq("/Script/FortniteGame.FortPawn.EquipWeaponDefinition");
    if (!eq.ok()) {
        LOG("EquipSlot[%d]: binding failed", slot);
        eq.dump("EquipWeaponDefinition");
        return;
    }

    bool bDisableAnim = false;
    eq.set("WeaponData", def);
    eq.set("ItemEntryGuid", guid);
    eq.set("TrackerGuid", trackerGuid);
    eq.set("bDisableEquipAnimation", bDisableAnim);

    if (eq.run(session.pawn)) {
        void* weapon = eq.result<void*>();
        LOG("EquipSlot[%d]: %s -> weapon=%p", slot, ObjName(def).c_str(), weapon);
    } else {
        LOG("EquipSlot[%d]: run failed", slot);
    }
}

// ============================================================================
// GrantDefaultAbilities via GrantAndActivateAbilityOnSelf
// ============================================================================
void Athena::GrantAbility(void* klass) {
    if (!klass || !session.pawn) return;
    void* asc = GetObjField(session.pawn, "/Script/FortniteGame.FortPawn.AbilitySystemComponent");
    if (!asc) { LOG("[Ability] no ASC"); return; }

    void* klib = FindObj("/Script/FortniteGame.Default__FortAbilitySystemBlueprintLibrary");
    if (!klib) { LOG("[Ability] no library"); return; }

    Call c("/Script/FortniteGame.FortAbilitySystemBlueprintLibrary.GrantAndActivateAbilityOnSelf");
    if (!c.ok()) {
        LOG("[Ability] binding failed");
        c.dump("GrantAndActivateAbilityOnSelf");
        return;
    }
    if (c.set("AbilitySystem", asc) && c.set("AbilityToGrant", klass)) {
        if (c.run(klib)) {
            LOG("[Ability] Granted: %s", ObjName(klass).c_str());
        }
    }
}

void Athena::GrantEffect(void* effect, float level) {
    if (!effect || !session.pawn) return;
    void* asc = GetObjField(session.pawn, "/Script/FortniteGame.FortPawn.AbilitySystemComponent");
    if (!asc) return;
    Call c("/Script/GameplayAbilities.AbilitySystemComponent.BP_ApplyGameplayEffectToSelf");
    if (c.ok() && c.set("GameplayEffectClass", effect) && c.set("Level", level)) c.run(asc);
}

void Athena::GrantDefaultAbilities() {
    GrantAbility(FindObj("/Script/FortniteGame.FortGameplayAbility_Sprint"));
    GrantAbility(FindObj("/Script/FortniteGame.FortGameplayAbility_Jump"));
    GrantAbility(FindObj("/Game/Abilities/Player/Generic/Traits/DefaultPlayer/GA_DefaultPlayer_InteractUse.GA_DefaultPlayer_InteractUse_C"));
    GrantAbility(FindObj("/Game/Abilities/Player/Generic/Traits/DefaultPlayer/GA_DefaultPlayer_InteractSearch.GA_DefaultPlayer_InteractSearch_C"));
    GrantAbility(FindObj("/Game/Athena/Items/ForagedItems/Rift/GA_Rift_Athena_Skydive.GA_Rift_Athena_Skydive_C"));
    LOG("Granted default abilities");
}

// ============================================================================
// ApplySprintSpeed
// ============================================================================
void Athena::ApplySprintSpeed() {
    if (!session.pawn || IsBadPtr(session.pawn)) return;
    void* cmc = GetObjField(session.pawn, "/Script/Engine.Character.CharacterMovement");
    if (!cmc || IsBadPtr(cmc)) return;
    void* prop = GetProp("/Script/Engine.CharacterMovementComponent.MaxWalkSpeed");
    if (prop) {
        float* val = (float*)GetField(cmc, prop);
        if (val && !IsBadPtr(val)) {
            if (*val < 599.0f) LOG("ApplySprintSpeed: %.0f -> 600", *val);
            *val = 600.f;
            session.sprint_applied = true;
        }
    }
    void* flyProp = GetProp("/Script/Engine.CharacterMovementComponent.MaxFlySpeed");
    if (flyProp) {
        float* val = (float*)GetField(cmc, flyProp);
        if (val && !IsBadPtr(val)) *val = 1200.f;
    }
}

// ============================================================================
// SpawnSafeZoneIndicator
// ============================================================================
void Athena::SpawnSafeZoneIndicator() {
    if (session.safezone_spawned) return;
    if (!session.state || !session.world) return;

    void* existing = GetObjField(session.state, "/Script/FortniteGame.FortGameStateAthena.SafeZoneIndicator");
    if (existing && !IsBadPtr(existing)) {
        LOG("SafeZone already exists");
        session.safezone_spawned = true;
        return;
    }

    void* cls = FindObj("/Script/FortniteGame.FortSafeZoneIndicator");
    if (!cls) { LOG("SafeZoneIndicator class missing"); return; }

    Transform t{};
    void* indicator = SpawnActor(session.world, cls, session.state, t);
    if (!indicator) { LOG("SafeZone spawn failed"); return; }

    void* prop = GetProp("/Script/FortniteGame.FortGameStateAthena.SafeZoneIndicator");
    if (prop) Write<void*>(GetField(session.state, prop), 0, indicator);

    Call n("/Script/FortniteGame.FortGameStateAthena.OnRep_SafeZoneIndicator");
    if (n.ok()) n.run(session.state);

    session.safezone_spawned = true;
    LOG("SafeZoneIndicator spawned: %p", indicator);
}

// ============================================================================
// SpawnPickup
// ============================================================================
void Athena::SpawnPickup(void* def, int count, Vector loc, bool toss) {
    if (!def) return;
    void* klass = FindObj("/Script/FortniteGame.FortPickupAthena");
    if (!klass) return;
    Transform t{};
    t.Location[0] = loc.X;
    t.Location[1] = loc.Y;
    t.Location[2] = loc.Z;
    void* pickup = SpawnActor(session.world, klass, session.pawn, t);
    if (!pickup) return;
    void* defProp = GetProp("/Script/FortniteGame.FortPickup.ItemDefinition");
    void* cntProp = GetProp("/Script/FortniteGame.FortPickup.Count");
    if (defProp) Write<void*>(GetField(pickup, defProp), 0, def);
    if (cntProp) Write<int>(GetField(pickup, cntProp), 0, count);
    Call rep("/Script/FortniteGame.FortPickup.OnRep_PrimaryPickupItemEntry");
    if (rep.ok()) rep.run(pickup);
    Call c("/Script/FortniteGame.FortPickup.TossPickup");
    if (c.ok()) {
        c.set("FinalLocation", loc);
        c.set("ItemOwner", session.pawn);
        c.set("OverrideMaxStackCount", -1);
        c.set("bToss", toss);
        c.set("bShouldCombinePickupsWhenTossCompletes", true);
        c.run(pickup);
    }
    LOG("Pickup spawned: %s x%d", ObjName(def).c_str(), count);
}

// ============================================================================
// Loot — Open a chest
// ============================================================================
void Athena::Loot(void* actor) {
    if (!actor) return;
    std::string tier = "None";
    void* bcCls = FindObj("/Script/FortniteGame.BuildingContainer");
    void* sdCls = FindObj("/Script/FortniteGame.FortAthenaSupplyDrop");

    if (bcCls && IsA(actor, bcCls)) {
        void* p = GetProp("/Script/FortniteGame.BuildingContainer.SearchLootTierGroup");
        if (p) {
            uint32 ci = Read<uint32>(GetField(actor, p));
            LOG("[Loot] chest SearchLootTierGroup cmp=%u", ci);
            tier = "Loot_AthenaTreasure";
        }
    } else if (sdCls && IsA(actor, sdCls)) {
        tier = "Loot_AthenaSupplyDrop";
    }

    LOG("[Loot] chest=%p tier=%s", actor, tier.c_str());

    // Spawn some loot around the chest
    Call loc("/Script/Engine.Actor.K2_GetActorLocation");
    Vector v{0.0, 0.0, 100.0};
    if (loc.ok() && loc.run(actor)) v = loc.result<Vector>();
    v.Z += 50.0;

    void* wood = LoadAsset("/Game/Items/ResourcePickups/WoodItemData.WoodItemData");
    void* stone = LoadAsset("/Game/Items/ResourcePickups/StoneItemData.StoneItemData");
    void* metal = LoadAsset("/Game/Items/ResourcePickups/MetalItemData.MetalItemData");
    void* ammoM = LoadAsset("/Game/Athena/Items/Ammo/AthenaAmmoDataBulletsMedium.AthenaAmmoDataBulletsMedium");
    void* ammoS = LoadAsset("/Game/Athena/Items/Ammo/AthenaAmmoDataShells.AthenaAmmoDataShells");
    void* gun = LoadAsset("/Game/Athena/Items/Weapons/WID_Assault_Auto_Athena_C_Ore_T03.WID_Assault_Auto_Athena_C_Ore_T03");
    void* shell = LoadAsset("/Game/Athena/Items/Weapons/WID_Shotgun_Standard_Athena_C_Ore_T03.WID_Shotgun_Standard_Athena_C_Ore_T03");

    if (wood) SpawnPickup(wood, 30, v);
    if (stone) SpawnPickup(stone, 30, v);
    if (metal) SpawnPickup(metal, 30, v);
    if (ammoM) SpawnPickup(ammoM, 60, v);
    if (ammoS) SpawnPickup(ammoS, 20, v);
    if (gun) SpawnPickup(gun, 1, v);
    if (shell) SpawnPickup(shell, 1, v);

    // Mark as searched
    SetBool(actor, "/Script/FortniteGame.BuildingContainer.bAlreadySearched", true);
    Call r1("/Script/FortniteGame.BuildingContainer.OnRep_bAlreadySearched");
    if (r1.ok()) r1.run(actor);

    Call r2("/Script/FortniteGame.BuildingContainer.BounceContainer");
    if (r2.ok()) { r2.run(actor); LOG("[Loot] BounceContainer dispatched"); }

    LOG("[Loot] chest opened: %p", actor);
}

// ============================================================================
// ServerHandlePickup
// ============================================================================
void Athena::ServerHandlePickup(void* pickup) {
    if (!pickup || !session.inventory) return;
    void* defProp = GetProp("/Script/FortniteGame.FortPickup.ItemDefinition");
    void* cntProp = GetProp("/Script/FortniteGame.FortPickup.Count");
    void* def = defProp ? Read<void*>(GetField(pickup, defProp)) : nullptr;
    int count = cntProp ? Read<int>(GetField(pickup, cntProp)) : 1;
    if (!def) return;
    AddToInventory(def, count, 10);
    InventoryUpdate();
    Call d("/Script/Engine.Actor.K2_DestroyActor");
    if (d.ok()) d.run(pickup);
}

// ============================================================================
// OnServerExecuteInventoryItem
// ============================================================================
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
            for (int s = 0; s < 6; ++s)
                if (session.slots[s] == inst) { EquipSlot(s); return; }
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
// Building
// ============================================================================
void Athena::SpawnBuildPreviews() {
    void* previewCls = FindObj("BuildingPlayerPrimitivePreview");
    if (!previewCls || !session.controller) return;

    auto build = [&](const char* meshPath, char type) -> void* {
        Transform t{};
        void* actor = SpawnActor(session.world, previewCls, session.controller, t);
        if (!actor) return nullptr;
        void* mesh = LoadAsset(meshPath, nullptr);
        void* mc = nullptr;
        Call g("/Script/FortniteGame.BuildingActor.GetBuildingMeshComponent");
        if (g.ok() && g.run(actor)) mc = g.result<void*>();
        if (mc && mesh) {
            Call s("/Script/Engine.StaticMeshComponent.SetStaticMesh");
            if (s.ok() && s.set("NewMesh", mesh)) s.run(mc);
        }
        Call init("/Script/FortniteGame.BuildingActor.OnBuildingActorInitialized");
        if (init.ok()) {
            init.set("BuildingType", type);
            init.set("BuildingState", (char)0);
            init.run(actor);
        }
        Call hide("/Script/Engine.Actor.SetActorHiddenInGame");
        bool h = true;
        if (hide.ok() && hide.set("bNewHidden", h)) hide.run(actor);
        return actor;
    };

    session.wall   = build("/Game/Building/ActorBlueprints/Player/Wood/L1/PBWA_W1_Solid.PBWA_W1_Solid", 0);
    session.floor  = build("/Game/Building/ActorBlueprints/Player/Wood/L1/PBWA_W1_Floor.PBWA_W1_Floor", 1);
    session.stairs = build("/Game/Building/ActorBlueprints/Player/Wood/L1/PBWA_W1_StairW.PBWA_W1_StairW", 5);
    session.roof   = build("/Game/Building/ActorBlueprints/Player/Wood/L1/PBWA_W1_RoofC.PBWA_W1_RoofC", 6);
    LOG("Build previews: W=%p F=%p S=%p R=%p", session.wall, session.floor, session.stairs, session.roof);
}

void Athena::OnServerCreateBuildingActor(void* params) {
    if (!params) return;

    struct OldP { void* Class; uint8 _p[0x8]; Vector Loc; Rotator Rot; };
    struct NewP { int32 ClassHandle; Vector Loc; Rotator Rot; uint8 _p[0x10]; void* Class; };

    void* cls = nullptr;
    Vector loc{};
    Rotator rot{};

    void* fn = FindObj("/Script/FortniteGame.FortPlayerController.ServerCreateBuildingActor");
    if (fn && Read<uint16_t>(fn, profile::FunctionParamSize) == sizeof(void*) + 0x8 + 24 + 24) {
        auto p = *(OldP*)params;
        cls = p.Class; loc = p.Loc; rot = p.Rot;
    } else {
        auto p = *(NewP*)params;
        cls = p.Class; loc = p.Loc; rot = p.Rot;
    }
    if (!cls) return;

    Transform t{};
    t.Location[0] = loc.X;
    t.Location[1] = loc.Y;
    t.Location[2] = loc.Z;

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

    LOG("Building: %s at (%.0f,%.0f,%.0f)", ObjName(cls).c_str(), loc.X, loc.Y, loc.Z);
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
    bool bd = false;
    Guid tg{};
    eq.set("WeaponData", session.edit_tool);
    eq.set("ItemEntryGuid", guid);
    eq.set("TrackerGuid", tg);
    eq.set("bDisableEquipAnimation", bd);
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

    Vector zero{0.0, 0.0, 0.0};
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

// ============================================================================
// Aircraft
// ============================================================================
void Athena::OnAircraftJump() {
    if (!session.controller || session.aircraft_jumped) return;
    session.aircraft_jumped = true;
    LOG("[Bus] Aircraft jump triggered");

    Vector spawnLoc{0.0, 0.0, 50000.0};
    Call choose("/Script/Engine.GameModeBase.ChoosePlayerStart");
    if (choose.ok() && choose.set("Player", session.controller) && choose.run(session.mode)) {
        void* start = choose.result<void*>();
        if (start && !IsBadPtr(start)) {
            Call sl("/Script/Engine.Actor.K2_GetActorLocation");
            if (sl.ok() && sl.run(start)) {
                Vector v = sl.result<Vector>();
                spawnLoc.X = v.X;
                spawnLoc.Y = v.Y;
                spawnLoc.Z = 50000.0;
            }
        }
    }

    void* klass = LoadClass("/Game/Athena/PlayerPawn_Athena.PlayerPawn_Athena_C", "/Script/Engine.Pawn");
    if (!klass) return;

    Transform t{};
    t.Location[0] = spawnLoc.X;
    t.Location[1] = spawnLoc.Y;
    t.Location[2] = spawnLoc.Z;

    void* newPawn = SpawnActor(session.world, klass, session.controller, t);
    if (!newPawn) return;

    Possess(session.controller, newPawn);
    session.pawn = newPawn;

    BeginPlayerSkydiving();
    ShowSkin();
    LOG("[Bus] Airborne at (%.0f,%.0f,%.0f)", spawnLoc.X, spawnLoc.Y, spawnLoc.Z);
}

// ============================================================================
// TeleportToSpawnIsland
// ============================================================================
void Athena::TeleportToSpawnIsland() {
    if (!session.pawn) return;
    void* cls = FindObj("/Script/FortniteGame.FortPlayerStartWarmup");
    if (!cls) return;
    GObjects_t* go = *(GObjects_t**)(g_base + profile::GObjects);
    if (!go) return;
    std::vector<void*> starts;
    for (int32 i = 0; i < go->NumElements; ++i) {
        void* o = go->GetById(i);
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
    if (sl.ok()) {
        bool bSweep = false, bTeleport = true;
        sl.set("NewLocation", loc);
        sl.set("NewRotation", rot);
        sl.set("bSweep", bSweep);
        sl.set("bTeleport", bTeleport);
        sl.run(session.pawn);
    }
}

// ============================================================================
// RemoveNetDebugUI
// ============================================================================
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
        if (w) {
            Call r("/Script/UMG.UserWidget.RemoveFromViewport");
            if (r.ok()) r.run(w);
        }
    }
}

// ============================================================================
// PlayEmoteItem
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
        ref.set("Type", (char)6);
        ref.set("Animation", (char)3);
        if (ref.run(item)) montage = ref.result<void*>();
    }
    if (!montage) return;

    Call play("/Script/Engine.AnimInstance.Montage_Play");
    if (play.ok()) {
        play.set("MontageToPlay", montage);
        play.set("InPlayRate", 1.0f);
        play.set("InStartTime", 0.0f);
        bool bStop = true;
        play.set("bStopAllMontages", bStop);
        play.run(ai);
    }
}

// ============================================================================
// OnExitVehicle
// ============================================================================
void Athena::OnExitVehicle() {
    if (!session.pawn) return;
    Call v("/Script/FortniteGame.FortPlayerPawn.ServerOnExitVehicle");
    if (v.ok()) {
        int32 ExitForce = 0;
        bool bDestroy = false;
        v.set("ExitForceBehavior", ExitForce);
        v.set("bDestroyVehicleWhenForced", bDestroy);
        v.run(session.pawn);
    }
}

// ============================================================================
// DropInventoryItem
// ============================================================================
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
            InventoryUpdate();
            return;
        }
    }
}

// ============================================================================
// CheatScript
// ============================================================================
void Athena::CheatScript(const char* script) {
    if (!script || !session.pawn) return;
    std::string s = script;
    std::transform(s.begin(), s.end(), s.begin(), ::tolower);
    LOG("CheatScript: %s", s.c_str());

    if (s.find("spawnweapon") != std::string::npos) {
        auto pos = s.find(" ");
        if (pos == std::string::npos) return;
        std::string wn = s.substr(pos + 1);
        void* def = GetByShortName(wn);
        if (!def) def = FindObj(wn);
        if (def) {
            Call c("/Script/Engine.Actor.K2_GetActorLocation");
            Vector v{0.0, 0.0, 5000.0};
            if (c.ok() && c.run(session.pawn)) v = c.result<Vector>();
            SpawnPickup(def, 1, v);
        }
    }
}

// ============================================================================
// PickLootDrops (simplified)
// ============================================================================
std::vector<Athena::LootData> Athena::PickLootDrops(std::string category) {
    std::vector<LootData> ret;
    void* wood = LoadAsset("/Game/Items/ResourcePickups/WoodItemData.WoodItemData");
    void* stone = LoadAsset("/Game/Items/ResourcePickups/StoneItemData.StoneItemData");
    void* metal = LoadAsset("/Game/Items/ResourcePickups/MetalItemData.MetalItemData");
    void* ammoM = LoadAsset("/Game/Athena/Items/Ammo/AthenaAmmoDataBulletsMedium.AthenaAmmoDataBulletsMedium");
    void* ammoS = LoadAsset("/Game/Athena/Items/Ammo/AthenaAmmoDataShells.AthenaAmmoDataShells");
    void* gun = LoadAsset("/Game/Athena/Items/Weapons/WID_Assault_Auto_Athena_C_Ore_T03.WID_Assault_Auto_Athena_C_Ore_T03");

    if (wood) ret.push_back({wood, 50});
    if (stone) ret.push_back({stone, 50});
    if (metal) ret.push_back({metal, 50});
    if (ammoM) ret.push_back({ammoM, 60});
    if (ammoS) ret.push_back({ammoS, 30});
    if (gun) ret.push_back({gun, 1});

    return ret;
}

// ============================================================================
// HOOKS
// ============================================================================
static void* g_interactionClass = nullptr;

static void pe_hook(void* obj, void* func, void* params) {
    if (!orig_PE) return;

    if (!g_internal && func && params && g_gameTid.load() == (int)gettid()) {
        if (!g_interactionClass)
            g_interactionClass = FindObj("/Script/FortniteGame.FortControllerComponent_Interaction");

        bool isPC          = (obj == session.controller);
        bool isPawn        = (obj == session.pawn);
        bool isInteraction = (g_interactionClass && IsA(obj, g_interactionClass));

        if (isPC || isPawn || isInteraction) {
            Guard g;
            std::string n = ObjName(func);

            if (n == "ServerExecuteInventoryItem") {
                if (Read<uint16_t>(func, profile::FunctionParamSize) >= (int)sizeof(Guid))
                    Athena::OnServerExecuteInventoryItem(Read<Guid>(params));
            }
            else if (n == "ServerExecuteInventoryWeapon") {
                Athena::OnServerExecuteInventoryWeapon(Read<void*>(params));
            }
            else if (n == "ServerHandlePickupInfo" || n == "ServerHandlePickup") {
                Athena::ServerHandlePickup(Read<void*>(params));
            }
            else if (n == "ServerAttemptInteract") {
                void* target = Read<void*>(params, 0);
                if (target && !IsBadPtr(target)) {
                    void* bcCls   = FindObj("/Script/FortniteGame.BuildingContainer");
                    void* wallCls = FindObj("/Script/FortniteGame.BuildingWall");
                    LOG("[Interact] target=%p class=%s", target,
                        ObjName(Read<void*>(target, profile::ObjectClass)).c_str());
                    if (bcCls && IsA(target, bcCls)) {
                        Athena::Loot(target);
                    } else if (wallCls && IsA(target, wallCls)) {
                        Athena::HandleDoor(target);
                    }
                }
            }
            else if (n == "ServerAttemptAircraftJump" && !session.aircraft_jumped) {
                Athena::OnAircraftJump();
            }
            else if (n == "ServerCreateBuildingActor") {
                Athena::OnServerCreateBuildingActor(params);
            }
            else if (n == "ServerBeginEditingBuildingActor") {
                Athena::OnBeginEditActor(Read<void*>(params, 0));
            }
            else if (n == "ServerEditBuildingActor") {
                struct P { void* BA; void* NC; int RI; bool M; };
                auto* p = (P*)params;
                Athena::OnFinishEditActor(p->BA, p->NC, p->RI, p->M);
            }
            else if (n == "ServerPlayEmoteItem") {
                Athena::PlayEmoteItem(Read<void*>(params));
            }
            else if (n == "ServerAttemptExitVehicle") {
                Athena::OnExitVehicle();
            }
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
    if (g_gameTid.compare_exchange_strong(expected, (int)gettid()))
        LOG("WorldTick first: tid=%d", (int)gettid());
    if (g_gameTid.load() != (int)gettid()) return;

    Guard g;
    std::string wn = ObjName(world);
    static std::string lastWorld;
    if (wn != lastWorld) {
        LOG(">>> WorldTick world: '%s'", wn.c_str());
        lastWorld = wn;
    }

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
    while (!normalized.empty() && (normalized.back() == ' ' || normalized.back() == '\t'))
        normalized.pop_back();

    if (!cmd.empty()) LOG("Exec: %s", cmd.c_str());

    if (normalized == "fixclient" || normalized == "fixall") { Athena::RestoreClientSystems(); return true; }
    if (normalized == "fixhud") { Athena::RestoreHUD(); return true; }
    if (normalized == "fixinput") { Athena::RestoreInput(); return true; }
    if (normalized == "fixbus") { Athena::RestoreAircraft(); return true; }
    if (normalized == "fixvehicle") { Athena::RestoreVehicleState(); return true; }
    if (normalized == "inventory" || normalized == "toggleinventory") { Athena::ToggleInventoryHUD(); return true; }
    if (normalized == "map" || normalized == "togglemap") { Athena::ToggleFullMap(); return true; }
    if (normalized == "sprint" || normalized == "fixsprint") { Athena::RestoreSprint(); return true; }
    if (normalized == "supplydrop" || normalized == "spawnsupplydrop") { Athena::SpawnSupplyDrop(); return true; }

    // Intercept Athena travel
    if (normalized == "open athena_terrain" || normalized.rfind("open athena_terrain", 0) == 0) {
        LOG("Intercepting Athena travel");
        session = Session{};
        session.attempted = false;
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
    LOG("sizeof(Vector)=%zu sizeof(Rotator)=%zu sizeof(Transform)=%zu",
        sizeof(Vector), sizeof(Rotator), sizeof(Transform));

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
