// ============================================================================
// EraCH3 — Fortnite 21.30 CL-21088273 Android server
// FINAL — UE5 LWC (double precision) structs
//   Vector = 24 bytes (3 doubles)
//   Rotator = 24 bytes (3 doubles)
//   Transform = 96 bytes (with LWC alignment)
//   Spawn via BeginSpawn with proper Transform
//   Teleport via ClientSetLocation + K2_SetActorLocation
//
// ONLY CHANGE vs original: fallback inventory if StartingItems is empty.
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
        "/data/local/tmp/era_log.txt", "/sdcard/era_log.txt", nullptr};
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
// 🔑 BASIC STRUCTS — UE5 LWC doubles
// ============================================================================
struct FStringNative { TCHAR* Data; int32 Count; int32 Max; };
struct FUEString     { TCHAR* data; int32 length; int32 capacity; };
struct FName         { uint32 CompIdx; uint32 DispIdx; };
struct Array         { void* Data; int32 Count; int32 Max; };
template<class T> struct TArray {
    T* Data; int32 Count; int32 Max;
    T& operator[](int i) { return Data[i]; }
    const T& operator[](int i) const { return Data[i]; }
    int Num() const { return Count; }
};

struct Guid  { uint32 A, B, C, D; };

// 🔑 LWC — doubles
struct Vector  { double X, Y, Z; };
struct Rotator { double Pitch, Yaw, Roll; };

struct ItemAndCount { int32 Count; uint32 _pad; void* Item; };
struct SoftObjRef   { uint8 _pad[0x28]; };

// 🔑 UE5 FTransform (96 bytes with LWC alignment)
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

static std::u16string ToWide(const std::string& s) { return std::u16string(s.begin(), s.end()); }
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
    if (it != cache.end()) { std::string r = it->second; pthread_mutex_unlock(&m); return r; }
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
static std::string ObjNameFromUEString(FUEString* s) {
    if (!s || !s->data || s->length <= 0) return "";
    return Narrow(s->data, s->length > 512 ? 512 : s->length);
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

// ============================================================================
// Function pointer typedefs
// ============================================================================
using PE_t   = void(*)(void*, void*, void*);
using Tick_t = void(*)(void*, int, float);
using Exec_t = bool(*)(void*, void*, const char16_t*, void*);
static PE_t   orig_PE = nullptr;
static Tick_t orig_Tick = nullptr;
static Exec_t orig_Exec = nullptr;

// ============================================================================
// Call
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

static std::string ObjNameFromFName(FName n) {
    void* klib = FindObj("/Script/Engine.Default__KismetStringLibrary");
    if (!klib) return "";
    Call c("/Script/Engine.KismetStringLibrary.Conv_NameToString");
    if (!c.ok() || !c.set("InName", n)) return "";
    if (!c.run(klib)) return "";
    FStringNative s = c.result<FStringNative>();
    std::string out = Narrow(s.Data, std::min(s.Count > 0 ? s.Count : 0, 512));
    if (s.Data) Fn<void(*)(void*)>(profile::Free)(s.Data);
    return out;
}

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
// ATHENA NAMESPACE
// ============================================================================
namespace Athena {
    void SetupWorld(void* world);
    void Tick();
    bool TryInitializePlayer(void* world, void* mode, void* controller);
    void SetupCheats();
    void FinalizePlayer();
    void ShowSkin();
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
    void OnServerCreateBuildingActor(void* params);
    void OnBeginEditActor(void* piece);
    void OnFinishEditActor(void* actor, void* newClass, int rotIter, bool mirrored, void* params);
    void PlayEmoteItem(void* item);
    void OnExitVehicle();
    void CheatScript(const char* script);

    struct LootData { void* Def; int Count; };
    std::vector<LootData> PickLootDrops(std::string category);
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
    if (!p || !klass || Read<int>(p, profile::PropElementSize) != 8) return;
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
// TryInitializePlayer — ORIGINAL (unchanged)
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

    LOG("[Init] Spawning at (%.0f,%.0f,%.0f)", t.Location[0], t.Location[1], t.Location[2]);
    session.pawn = SpawnActor(world, pawnCls, controller, t);
    if (!session.pawn) { LOG("[Init] Spawn failed"); return false; }
    LOG("[Init] Spawned: %p (%s)", session.pawn, ObjName(session.pawn).c_str());

    SetBool(session.pawn, "/Script/FortniteGame.FortPlayerPawn.bIsSkydiving", true);
    SetBool(session.pawn, "/Script/FortniteGame.FortPlayerPawn.bIsParachuteOpen", false);

    Possess(controller, session.pawn);
    if (GetPawn(controller) != session.pawn) {
        LOG("[Init] Possession did not stick");
        return false;
    }
    LOG("[Init] Possessed OK");

    SetupCheats();

    {
        Call cs("/Script/Engine.Controller.ClientSetLocation");
        if (cs.ok()) {
            Vector loc{0.0, 0.0, 3100.0};
            Rotator rot{0.0, 0.0, 0.0};
            if (cs.set("NewLocation", loc) && cs.set("NewRotation", rot)) {
                cs.run(controller);
                LOG("[Init] ClientSetLocation sent");
            }
        }
    }

    {
        Call sl("/Script/Engine.Actor.K2_SetActorLocation");
        if (sl.ok()) {
            Vector loc{0.0, 0.0, 3100.0};
            bool bSweep = false, bTeleport = true;
            sl.set("NewLocation", loc);
            sl.set("bSweep", bSweep);
            sl.set("bTeleport", bTeleport);
            sl.run(session.pawn);
            LOG("[Init] K2_SetActorLocation sent");
        }
    }

    {
        Call chk("/Script/Engine.Actor.K2_GetActorLocation");
        if (chk.ok() && chk.run(session.pawn)) {
            Vector v = chk.result<Vector>();
            LOG("[Init] After teleport: (%.0f,%.0f,%.0f)", v.X, v.Y, v.Z);
        }
    }

    SetBool(session.pawn, "/Script/FortniteGame.FortPlayerPawn.bIsSkydiving", true);
    SetBool(session.pawn, "/Script/FortniteGame.FortPlayerPawn.bIsParachuteOpen", false);
    Call sd("/Script/FortniteGame.FortPlayerPawn.OnRep_IsSkydiving");
    if (sd.ok()) sd.run(session.pawn);
    Call po("/Script/FortniteGame.FortPlayerPawn.OnRep_IsParachuteOpen");
    if (po.ok()) po.run(session.pawn);

    Call restart("/Script/Engine.PlayerController.ClientRestart");
    if (restart.ok() && restart.set("NewPawn", session.pawn))
        restart.run(controller);

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
    if (!session.mode || !session.state) {
        LOG("SetupWorld: mode/state missing");
        return;
    }

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

    LOG("SetupWorld: SKIPPING FixBuildingFoundations (crash)");

    LOG("SetupWorld: InitializeInventory");
    InitializeInventory();

    LOG("SetupWorld: GrantDefaultAbilities");
    GrantDefaultAbilities();

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
// FinalizePlayer — ORIGINAL (unchanged)
// ============================================================================
void Athena::FinalizePlayer() {
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

        if (v.Z < 500.0) {
            LOG("Finalize: Z too low — retry ClientSetLocation");
            Vector dest{0.0, 0.0, 3100.0};
            Rotator rot{0.0, 0.0, 0.0};
            Call cs("/Script/Engine.Controller.ClientSetLocation");
            if (cs.ok() && cs.set("NewLocation", dest) && cs.set("NewRotation", rot))
                cs.run(pc);
            Call sl("/Script/Engine.Actor.K2_SetActorLocation");
            if (sl.ok()) {
                bool bSweep = false, bTeleport = true;
                sl.set("NewLocation", dest);
                sl.set("bSweep", bSweep);
                sl.set("bTeleport", bTeleport);
                sl.run(session.pawn);
            }
            SetBool(session.pawn, "/Script/FortniteGame.FortPlayerPawn.bIsSkydiving", true);
            SetBool(session.pawn, "/Script/FortniteGame.FortPlayerPawn.bIsParachuteOpen", false);
            Call sd3("/Script/FortniteGame.FortPlayerPawn.OnRep_IsSkydiving");
            if (sd3.ok()) sd3.run(session.pawn);
        } else {
            LOG("Finalize: pawn airborne at Z=%.0f", v.Z);
        }
    }

    ShowSkin();

    Call restart("/Script/Engine.PlayerController.ClientRestart");
    if (restart.ok() && restart.set("NewPawn", pawn)) restart.run(pc);

    Call cam("/Script/Engine.PlayerController.SetViewTargetWithBlend");
    if (cam.ok() && cam.set("NewViewTarget", pawn)) cam.run(pc);

    LOG("Finalize: complete, pawn=%p", session.pawn);
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
}

// ============================================================================
// ShowSkin
// ============================================================================
void Athena::ShowSkin() {
    if (!session.controller || !session.pawn) return;
    void* info = GetObjField(session.controller, "/Script/FortniteGame.FortPlayerController.MyPlayerInfo");
    if (!info) { LOG("ShowSkin: no MyPlayerInfo"); return; }
    void* hero = GetObjField(info, "/Script/FortniteGame.FortAthenaLocalPlayerInfo.AthenaMenuHeroDef");
    if (!hero) hero = GetObjField(info, "/Script/FortniteGame.FortAthenaLocalPlayerInfo.TempAthenaMenuHeroInstance");
    if (!hero) { LOG("ShowSkin: no hero def"); return; }

    if (session.character_parts.empty()) {
        void* cpProp = GetProp("/Script/FortniteGame.FortHeroType.CharacterParts");
        if (cpProp) {
            Array parts = Read<Array>(GetField(hero, cpProp));
            for (int i = 0; i < parts.Count && i < 16; ++i) {
                void* p = Read<void*>(parts.Data, (size_t)i * 8);
                if (p && !IsBadPtr(p)) session.character_parts.push_back(p);
            }
        }
    }

    for (void* part : session.character_parts) {
        void* typeProp = GetProp("/Script/FortniteGame.CustomCharacterPart.CharacterPartType");
        if (!typeProp) continue;
        char type = Read<char>(GetField(part, typeProp));
        Call c("/Script/FortniteGame.FortPlayerPawn.ServerChoosePart");
        if (c.ok() && c.set("CharacterPartType", type) && c.set("ChosenCharacterPart", part)) c.run(session.pawn);
    }
    void* state = GetObjField(session.controller, "/Script/Engine.Controller.PlayerState");
    if (state) {
        Call a("/Script/FortniteGame.FortPlayerState.OnRep_CharacterParts"); if (a.ok()) a.run(state);
        Call b("/Script/FortniteGame.FortPlayerState.OnRep_CharacterData"); if (b.ok()) b.run(state);
    }
    Call cl("/Script/FortniteGame.FortPlayerPawn.OnRep_CustomizationLoadout"); if (cl.ok()) cl.run(session.pawn);
    LOG("Skin applied (%zu parts)", session.character_parts.size());
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

// ============================================================================
// DropLoadingScreen — ORIGINAL (unchanged)
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
    Call rsm("/Script/FortniteGame.FortPlayerController.ServerReadyToStartMatch");
    if (rsm.ok()) {
        rsm.run(session.controller);
        LOG("ServerReadyToStartMatch sent");
    }

    const char* brushes[] = {"MinimapNextCircleBrush","FullMapNextCircleBrush",
        "MinimapSafeZoneBrush","MinimapCircleBrush","FullMapCircleBrush", nullptr};
    for (int i = 0; brushes[i]; ++i) {
        std::string path = std::string("/Script/FortniteGame.FortGameStateAthena.").append(brushes[i]);
        void* p = GetProp(path.c_str());
        if (p && !IsBadPtr(GetField(session.state, p))) memset(GetField(session.state, p), 0, 8);
    }

    void* playlist = FindObj("/Game/Athena/Playlists/Creative/Playlist_PlaygroundV2.Playlist_PlaygroundV2");
    if (!playlist) playlist = FindObj("/Game/Athena/Playlists/Playground/Playlist_Playground.Playlist_Playground");
    if (playlist) {
        void* bp = GetProp("/Script/FortniteGame.FortGameStateAthena.BasePlaylist");
        void* op = GetProp("/Script/FortniteGame.FortGameStateAthena.OverridePlaylist");
        void* cp = GetProp("/Script/FortniteGame.FortGameStateAthena.CurrentPlaylistData");
        if (bp) Write<void*>(GetField(session.state, bp), 0, playlist);
        if (op) Write<void*>(GetField(session.state, op), 0, playlist);
        if (cp) Write<void*>(GetField(session.state, cp), 0, playlist);
        Call a("/Script/FortniteGame.FortGameStateAthena.OnRep_CurrentPlaylistData"); if (a.ok()) a.run(session.state);
        Call b("/Script/FortniteGame.FortGameStateAthena.OnRep_CurrentPlaylistInfo"); if (b.ok()) b.run(session.state);
    }
    LOG("Loading screen dropped");
}

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
    SetBool(session.pawn, "/Script/FortniteGame.FortPlayerPawn.bIsSkydiving", true);
    Call sd("/Script/FortniteGame.FortPlayerPawn.OnRep_IsSkydiving");
    if (sd.ok()) sd.run(session.pawn);
}

// ============================================================================
// INVENTORY — with ONLY fallback addition
// ============================================================================
void Athena::InventoryUpdate() {
    if (session.inventory) {
        Call a("/Script/FortniteGame.FortInventory.HandleInventoryLocalUpdate"); if (a.ok()) a.run(session.inventory);
    }
    { Call b("/Script/FortniteGame.FortPlayerController.HandleWorldInventoryLocalUpdate"); if (b.ok()) b.run(session.controller); }
    { Call c("/Script/FortniteGame.FortPlayerController.OnRep_QuickBar"); if (c.ok()) c.run(session.controller); }
    if (session.quickbars) {
        Call d("/Script/FortniteGame.FortQuickBars.OnRep_SecondaryQuickBar"); if (d.ok()) d.run(session.quickbars);
        Call e("/Script/FortniteGame.FortQuickBars.OnRep_PrimaryQuickBar"); if (e.ok()) e.run(session.quickbars);
    }
    for (uint8_t bar : {uint8_t(0), uint8_t(1)}) {
        Call c("/Script/FortniteGame.FortPlayerController.ClientForceUpdateQuickbar");
        if (c.ok() && c.set("QuickbarToRefresh", bar)) c.run(session.controller);
    }
}

void Athena::AddToInventory(void* def, int count, int slot) {
    if (IsBadPtr(def) || !session.inventory || !session.quickbars) return;
    void* inst = nullptr;
    {
        Call c("/Script/FortniteGame.FortItemDefinition.CreateTemporaryItemInstanceBP");
        int lvl = 1;
        if (!c.ok() || !c.set("Count", count) || !c.set("Level", lvl) || !c.run(def)) return;
        inst = c.result<void*>();
    }
    if (!inst || IsBadPtr(inst)) return;
    {
        Call c("/Script/FortniteGame.FortItem.SetOwningControllerForTemporaryItem");
        if (c.ok() && c.set("InController", session.controller)) c.run(inst);
    }
    void* invProp = GetProp("/Script/FortniteGame.FortInventory.Inventory");
    void* instProp = GetProp("/Script/FortniteGame.FortItemList.ItemInstances");
    void* entryProp = GetProp("/Script/FortniteGame.FortItemList.ReplicatedEntries");
    void* itemEntryProp = GetProp("/Script/FortniteGame.FortWorldItem.ItemEntry");
    if (!invProp || !instProp || !entryProp || !itemEntryProp) return;
    void* list = GetField(session.inventory, invProp);
    void* instArr = GetField(list, instProp);
    void* entryArr = GetField(list, entryProp);
    Fn<int(*)(void*, void*, const void*)>(profile::GenericArrayAdd)(instArr, instProp, &inst);
    Fn<int(*)(void*, void*, const void*)>(profile::GenericArrayAdd)(entryArr, entryProp, GetField(inst, itemEntryProp));

    Call qb("/Script/FortniteGame.FortPlayerController.AddItemToQuickBars");
    uint8_t bar = 0;
    if (qb.ok() && qb.set("ItemDefinition", def) && qb.set("QuickBarType", bar) && qb.set("Slot", slot))
        qb.run(session.controller);
    LOG("Inv+ %s x%d slot=%d", ObjName(def).c_str(), count, slot);
}

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
        Call so("/Script/FortniteGame.FortQuickBars.SetOwner"); if (so.ok()) so.run(session.quickbars);
    }
    { Call so("/Script/FortniteGame.FortInventory.SetOwner"); if (so.ok()) so.run(session.inventory); }

    LOG("Inventory ready: inv=%p qb=%p", session.inventory, session.quickbars);

    bool gaveAnything = false;
    void* startingItemsProp = GetProp("/Script/FortniteGame.FortGameModeAthena.StartingItems");
    if (startingItemsProp && session.mode) {
        Array arr = Read<Array>(GetField(session.mode, startingItemsProp));
        LOG("StartingItems.Count = %d", arr.Count);
        for (int i = 0; i < arr.Count && i < 6; ++i) {
            ItemAndCount ic = Read<ItemAndCount>(arr.Data, (size_t)i * 16);
            if (ic.Item && !IsBadPtr(ic.Item)) {
                if (i == 0) AddToInventory(ic.Item, 1, 0);
                else if (i == 4) session.edit_tool = ic.Item;
                else AddToInventory(ic.Item, 1, i - 1);
                gaveAnything = true;
            }
        }
    }

    // 🔑 THE ONLY ADDITION: fallback inventory if StartingItems is empty
    if (!gaveAnything) {
        LOG("=== StartingItems empty — using fallback ===");
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
            if (!def) continue;
            AddToInventory(def, 1, d.slot);
        }
        // Edit tool
        void* editTool = LoadAsset("/Game/Items/Weapons/BuildingTools/EditTool.EditTool");
        if (editTool) session.edit_tool = editTool;
    }

    const char* ammo[] = {
        "/Game/Athena/Items/Ammo/AthenaAmmoDataBulletsLight.AthenaAmmoDataBulletsLight",
        "/Game/Athena/Items/Ammo/AthenaAmmoDataBulletsMedium.AthenaAmmoDataBulletsMedium",
        "/Game/Athena/Items/Ammo/AthenaAmmoDataBulletsHeavy.AthenaAmmoDataBulletsHeavy",
        "/Game/Athena/Items/Ammo/AthenaAmmoDataShells.AthenaAmmoDataShells",
        "/Game/Athena/Items/Ammo/AmmoDataRockets.AmmoDataRockets", nullptr};
    for (int i = 0; ammo[i]; ++i) {
        void* def = LoadAsset(ammo[i]);
        if (def) AddToInventory(def, 999, 3);
    }
    InventoryUpdate();
}

// ============================================================================
// WEAPON EQUIP — ORIGINAL (unchanged, no tracker)
// ============================================================================
static bool EquipWeapon(void* pawn, void* def, Guid guid) {
    if (!pawn || !def) return false;
    Call c("/Script/FortniteGame.FortPawn.EquipWeaponDefinition");
    if (!c.ok()) return false;
    if (!c.set("WeaponData", def)) return false;
    if (!c.set("ItemEntry", guid)) return false;
    if (!c.run(pawn)) return false;
    return c.result<void*>() != nullptr;
}

void Athena::OnServerExecuteInventoryItem(Guid guid) {
    if (!session.inventory || !session.pawn) return;
    void* instProp = GetProp("/Script/FortniteGame.FortItemList.ItemInstances");
    void* invProp = GetProp("/Script/FortniteGame.FortInventory.Inventory");
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
            Call d("/Script/FortniteGame.FortItem.GetItemDefinitionBP");
            void* def = d.ok() && d.run(inst) ? d.result<void*>() : nullptr;
            if (def) EquipWeapon(session.pawn, def, cand);
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
// ABILITIES — ORIGINAL (GrantAbility approach)
// ============================================================================
void Athena::GrantAbility(void* klass) {
    if (!klass || !session.pawn) return;
    void* asc = GetObjField(session.pawn, "/Script/FortniteGame.FortPawn.AbilitySystemComponent");
    if (!asc) return;
    void* ge = FindObj("/Game/Athena/Items/Consumables/PurpleStuff/GE_Athena_PurpleStuff.GE_Athena_PurpleStuff_C");
    if (!ge) ge = FindObj("/Game/Athena/Items/Consumables/PurpleStuff/GE_Athena_PurpleStuff_Health.GE_Athena_PurpleStuff_Health_C");
    if (!ge) return;

    void* defUI = FindObj("/Script/FortniteGame.Default__FortAbilitySystemUI");
    if (defUI) {
        Call d("/Script/FortniteGame.FortAbilitySystemUI.GetDefaultObjectOfGameplayEffectType");
        if (d.ok() && d.set("EffectType", ge) && d.run(defUI)) {
            void* defEffect = d.result<void*>();
            if (defEffect) {
                void* abilProp = GetProp("/Script/FortniteGame.GameplayEffect.GrantedAbilities");
                if (abilProp) {
                    Array arr = Read<Array>(GetField(defEffect, abilProp));
                    if (arr.Count > 0 && arr.Data) {
                        void* abilityPtrProp = GetProp("/Script/FortniteGame.GameplayAbilitySpecDef.Ability");
                        if (abilityPtrProp) Write<void*>(arr.Data, 0, klass);
                    }
                }
            }
        }
    }
    Call apply("/Script/GameplayAbilities.AbilitySystemComponent.BP_ApplyGameplayEffectToSelf");
    if (apply.ok() && apply.set("GameplayEffectClass", ge) && apply.set("Level", 1.0f))
        apply.run(asc);
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
// SPAWN PICKUP — ORIGINAL
// ============================================================================
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
    Call rep("/Script/FortniteGame.FortPickup.OnRep_PrimaryPickupItemEntry"); if (rep.ok()) rep.run(pickup);
    Call c("/Script/FortniteGame.FortPickup.TossPickup");
    if (c.ok()) {
        c.set("NewLocation", loc);
        c.set("NewInstigator", session.pawn);
        c.set("OverriddenCount", 1);
        c.set("bUseNonDefaultPickupLocation", toss);
        c.run(pickup);
    }
}

// ============================================================================
// LOOT TABLES
// ============================================================================
std::vector<Athena::LootData> Athena::PickLootDrops(std::string category) {
    std::vector<LootData> ret;
    void* ltd = LoadAsset("/Game/Items/Datatables/AthenaLootTierData_Client.AthenaLootTierData_Client", nullptr);
    void* lpt = LoadAsset("/Game/Items/Datatables/AthenaLootPackages_Client.AthenaLootPackages_Client", nullptr);
    if (!ltd || !lpt) return ret;
    void* ltdRS = GetObjField(ltd, "/Script/Engine.DataTable.RowStruct");
    void* lptRS = GetObjField(lpt, "/Script/Engine.DataTable.RowStruct");
    if (!ltdRS || !lptRS) return ret;
    int32 ltdSize = Read<int>(ltdRS, profile::StructSize);
    int32 lptSize = Read<int>(lptRS, profile::StructSize);
    if (ltdSize <= 0 || lptSize <= 0 || ltdSize > 0x1000 || lptSize > 0x1000) return ret;
    void* dtLib = FindObj("/Script/Engine.Default__DataTableFunctionLibrary");
    if (!dtLib) return ret;
    TArray<FName> ltdNames{}, lptNames{};
    { Call c("/Script/Engine.DataTableFunctionLibrary.GetDataTableRowNames"); if (c.ok() && c.set("Table", ltd) && c.run(dtLib)) ltdNames = c.result<TArray<FName>>(); }
    { Call c("/Script/Engine.DataTableFunctionLibrary.GetDataTableRowNames"); if (c.ok() && c.set("Table", lpt) && c.run(dtLib)) lptNames = c.result<TArray<FName>>(); }
    if (ltdNames.Count <= 0 || lptNames.Count <= 0) return ret;
    auto fieldOff = [](void* rs, const char* fld) -> int32 {
        void* klass = rs;
        while (klass && !IsBadPtr(klass)) {
            void* c = Read<void*>(klass, profile::StructChildren);
            while (c && !IsBadPtr(c)) {
                if (ObjName(c) == fld) return Read<int>(c, profile::PropOffset);
                c = Read<void*>(c, profile::FieldNext);
            }
            void* fp = Read<void*>(klass, profile::StructChildProps);
            while (fp && !IsBadPtr(fp)) {
                if (ObjName(fp) == fld) return Read<int>(fp, profile::PropOffset);
                fp = Read<void*>(fp, profile::FFieldNext);
            }
            klass = Read<void*>(klass, profile::StructSuper);
        }
        return -1;
    };
    int32 tg = fieldOff(ltdRS, "TierGroup");
    int32 wt = fieldOff(ltdRS, "Weight");
    int32 nd = fieldOff(ltdRS, "NumLootPackageDrops");
    int32 lp = fieldOff(ltdRS, "LootPackage");
    int32 lpid = fieldOff(lptRS, "LootPackageID");
    int32 lpc = fieldOff(lptRS, "LootPackageCall");
    int32 ido = fieldOff(lptRS, "ItemDefinition");
    int32 cnt = fieldOff(lptRS, "Count");
    if (tg < 0 || wt < 0 || nd < 0 || lp < 0) return ret;
    if (lpid < 0 || lpc < 0 || ido < 0 || cnt < 0) return ret;
    using GetRowFn = bool(*)(void*, FName, void*);
    GetRowFn getRow = (GetRowFn)(g_base + profile::GetDataTableRow_Orig);
    std::map<int, void*> catRows;
    float maxW = 0;
    for (int i = 0; i < ltdNames.Count && i < 512; ++i) {
        void* row = calloc(1, ltdSize);
        getRow(ltd, ltdNames.Data[i], row);
        std::string tname = ObjNameFromFName(*(FName*)((uintptr_t)row + tg));
        if (tname == category) {
            float w = *(float*)((uintptr_t)row + wt);
            if (w != 0) { catRows[i] = row; maxW += w; }
            else free(row);
        } else free(row);
    }
    if (catRows.empty() || maxW <= 0) { for (auto& p : catRows) free(p.second); return ret; }
    float rv = ((float)rand() / (float)RAND_MAX) * maxW;
    void* pickedRow = nullptr;
    for (auto& p : catRows) {
        float w = *(float*)((uintptr_t)p.second + wt);
        if (rv < w) { pickedRow = p.second; break; }
        rv -= w;
    }
    if (!pickedRow) { for (auto& p : catRows) free(p.second); return ret; }
    float numDrops = *(float*)((uintptr_t)pickedRow + nd);
    std::string pkgName = ObjNameFromFName(*(FName*)((uintptr_t)pickedRow + lp));
    for (auto& p : catRows) free(p.second);
    catRows.clear();
    std::map<int, void*> pkgRows;
    for (int i = 0; i < lptNames.Count && i < 2048; ++i) {
        void* row = calloc(1, lptSize);
        getRow(lpt, lptNames.Data[i], row);
        std::string pid = ObjNameFromFName(*(FName*)((uintptr_t)row + lpid));
        if (pid == pkgName) pkgRows[i] = row;
        else free(row);
    }
    int want = (int)numDrops;
    if (want > (int)pkgRows.size()) want = (int)pkgRows.size();
    for (int i = 0; i < want; ++i) {
        auto it = pkgRows.begin();
        std::advance(it, i);
        if (it == pkgRows.end()) break;
        void* row = it->second;
        std::string call = ObjNameFromUEString((FUEString*)((uintptr_t)row + lpc));
        if (pkgName.find(".Empty") != std::string::npos) continue;
        if (call.find(".Empty") != std::string::npos) continue;
        for (int j = 0; j < lptNames.Count && j < 2048; ++j) {
            void* cand = calloc(1, lptSize);
            getRow(lpt, lptNames.Data[j], cand);
            std::string cid = ObjNameFromFName(*(FName*)((uintptr_t)cand + lpid));
            if (cid == call) {
                SoftObjRef soft = *(SoftObjRef*)((uintptr_t)cand + ido);
                void* klib = FindObj("/Script/Engine.Default__KismetSystemLibrary");
                if (klib) {
                    Call cvt("/Script/Engine.KismetSystemLibrary.Conv_SoftObjectReferenceToObject");
                    if (cvt.ok() && cvt.set("SoftObject", soft) && cvt.run(klib)) {
                        void* def = cvt.result<void*>();
                        int ic = *(int*)((uintptr_t)cand + cnt);
                        if (def) ret.push_back({def, ic});
                    }
                }
                free(cand);
                break;
            }
            free(cand);
        }
    }
    for (auto& p : pkgRows) free(p.second);
    return ret;
}

// ============================================================================
// LOOT INTERACT — ORIGINAL
// ============================================================================
void Athena::Loot(void* actor) {
    if (!actor) return;
    std::string tier = "None";
    void* bcCls = FindObj("BuildingContainer");
    void* sdCls = FindObj("FortAthenaSupplyDrop");
    if (bcCls && IsA(actor, bcCls)) {
        void* p = GetProp("/Script/FortniteGame.BuildingContainer.SearchLootTierGroup");
        if (p) tier = ObjNameFromFName(Read<FName>(GetField(actor, p)));
    } else if (sdCls && IsA(actor, sdCls)) {
        void* p = GetProp("/Script/FortniteGame.FortAthenaSupplyDrop.LootTableName");
        if (p) tier = ObjNameFromFName(Read<FName>(GetField(actor, p)));
    }
    if (tier.find("None") != std::string::npos) return;
    if (tier.find("Athena") == std::string::npos) {
        auto pos = tier.find("_");
        if (pos != std::string::npos) tier.replace(pos, 1, "_Athena");
    }
    if (bcCls && IsA(actor, bcCls)) {
        void* p = GetProp("/Script/FortniteGame.BuildingContainer.TimeUntilLootRegenerates");
        if (p && Read<float>(GetField(actor, p)) == 0) Write<float>(GetField(actor, p), 0, -1);
        else return;
    }
    auto results = PickLootDrops(tier);
    for (auto& r : results) {
        Call c("/Script/Engine.Actor.K2_GetActorLocation");
        if (c.run(actor)) {
            Vector v = c.result<Vector>();
            SpawnPickup(r.Def, r.Count, v);
        }
    }
    if (bcCls && IsA(actor, bcCls)) {
        SetBool(actor, "/Script/FortniteGame.BuildingContainer.bAlreadySearched", true);
        Call r("/Script/FortniteGame.BuildingContainer.OnRep_bAlreadySearched"); if (r.ok()) r.run(actor);
    }
}

void Athena::ServerHandlePickup(void* pickup) {
    if (!pickup || !session.inventory) return;
    void* defProp = GetProp("/Script/FortniteGame.FortPickup.ItemDefinition");
    void* cntProp = GetProp("/Script/FortniteGame.FortPickup.Count");
    void* def = defProp ? Read<void*>(GetField(pickup, defProp)) : nullptr;
    int count = cntProp ? Read<int>(GetField(pickup, cntProp)) : 1;
    if (!def) return;
    AddToInventory(def, count, 10);
    InventoryUpdate();
    Call d("/Script/Engine.Actor.K2_DestroyActor"); if (d.ok()) d.run(pickup);
}

// ============================================================================
// BUILDING — ORIGINAL
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
    void* cls = nullptr; Vector loc{}; Rotator rot{};
    auto* fn = FindObj("/Script/FortniteGame.FortPlayerController.ServerCreateBuildingActor");
    if (fn && Read<uint16_t>(fn, profile::FunctionNumParams) == 1) {
        auto p = *(NewP*)params; cls = p.Class; loc = p.Loc; rot = p.Rot;
    } else {
        auto p = *(OldP*)params; cls = p.Class; loc = p.Loc; rot = p.Rot;
    }
    if (!cls) return;
    Transform t{};
    t.Location[0] = loc.X; t.Location[1] = loc.Y; t.Location[2] = loc.Z;
    void* actor = SpawnActor(session.world, cls, session.controller, t);
    if (!actor) return;
    Call init("/Script/FortniteGame.BuildingActor.InitializeKismetSpawnedBuildingActor");
    if (init.ok()) {
        init.set("NewBuilding", actor);
        init.set("OwningController", session.controller);
        init.set("bIsInitializeSpawnedFromKismet", true);
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
    if (!eq.ok() || !eq.set("WeaponData", session.edit_tool) || !eq.set("ItemEntry", guid)) return;
    if (!eq.run(session.pawn)) return;
    void* editActor = eq.result<void*>();
    if (!editActor) return;
    void* p = GetProp("/Script/FortniteGame.FortWeapon.EditActor");
    if (p) Write<void*>(GetField(editActor, p), 0, piece);
    Call r("/Script/FortniteGame.FortWeapon.OnRep_EditActor"); if (r.ok()) r.run(editActor);
}

void Athena::OnFinishEditActor(void* actor, void* newClass, int rotIter, bool mirrored, void* params) {
    if (!actor) return;
    SetBool(actor, "/Script/FortniteGame.BuildingActor.bPlayDestructionEffects", false);
    Vector zero{0.0, 0.0, 0.0};
    Call s("/Script/Engine.Actor.SetActorScale3D");
    if (s.ok() && s.set("NewScale3D", zero)) s.run(actor);
    Call d("/Script/Engine.Actor.K2_DestroyActor"); if (d.ok()) d.run(actor);
    Transform t{};
    void* edited = SpawnActor(session.world, newClass, session.controller, t);
    if (!edited) return;
    Call m("/Script/Engine.Actor.SetMirrored");
    if (m.ok() && m.set("bNewMirrored", mirrored)) m.run(edited);
    Call init("/Script/FortniteGame.BuildingActor.InitializeKismetSpawnedBuildingActor");
    if (init.ok()) {
        init.set("NewBuilding", edited);
        init.set("OwningController", session.controller);
        init.set("bIsInitializeSpawnedFromKismet", true);
        init.run(edited);
    }
}

// ============================================================================
// AIRCRAFT / WORLD — ORIGINAL
// ============================================================================
void Athena::OnAircraftJump() {
    if (!session.controller) return;
    Call loc("/Script/Engine.PlayerController.GetFocalLocation");
    Vector l{0.0, 0.0, 20000.0};
    if (loc.ok() && loc.run(session.controller)) l = loc.result<Vector>();
    Call rot("/Script/Engine.PlayerController.GetControlRotation");
    Rotator r{0.0, 0.0, 0.0};
    if (rot.ok() && rot.run(session.controller)) { r = rot.result<Rotator>(); r.Pitch = 0; r.Roll = 0; }
    void* klass = LoadClass("/Game/Athena/PlayerPawn_Athena.PlayerPawn_Athena_C", "/Script/Engine.Pawn");
    if (!klass) return;
    Transform t{};
    t.Location[0] = l.X; t.Location[1] = l.Y; t.Location[2] = l.Z;
    session.pawn = SpawnActor(session.world, klass, session.controller, t);
    if (!session.pawn) return;
    Possess(session.controller, session.pawn);
    ShowSkin();
    Call cl("/Script/FortniteGame.FortPlayerPawn.OnRep_CustomizationLoadout"); if (cl.ok()) cl.run(session.pawn);
    session.aircraft_jumped = true;
    LOG("Aircraft jump -> Pawn %p", session.pawn);
}

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
            Call r("/Script/UMG.UserWidget.RemoveFromViewport"); if (r.ok()) r.run(w);
        }
    }
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
        play.set("bStopAllMontages", true);
        play.run(ai);
    }
}

void Athena::OnExitVehicle() {
    if (!session.pawn) return;
    void* p = GetProp("/Script/Engine.Actor.Role");
    if (p) Write<char>(GetField(session.pawn, p), 0, 3);
    Call v("/Script/FortniteGame.FortPlayerPawn.GetVehicle");
    if (v.ok() && v.run(session.pawn)) {
        void* veh = v.result<void*>();
        if (veh) {
            void* vp = GetProp("/Script/Engine.Actor.Role");
            if (vp) Write<char>(GetField(veh, vp), 0, 3);
        }
    }
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
            Fn<void(*)(void*, void*, int)>(profile::GenericArrayRemove)(instArr, instProp, i);
            Fn<void(*)(void*, void*, int)>(profile::GenericArrayRemove)(entryArr, entryProp, i);
            InventoryUpdate();
            return;
        }
    }
}

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
// HOOKS — ORIGINAL (simple)
// ============================================================================
static void pe_hook(void* obj, void* func, void* params) {
    if (!orig_PE) return;
    if (!g_internal && func && params && session.controller && obj == session.controller &&
        g_gameTid.load() == (int)gettid()) {
        Guard g;
        std::string n = ObjName(func);
        if (n == "ServerExecuteInventoryItem") {
            if (Read<uint16_t>(func, profile::FunctionParamSize) >= (int)sizeof(Guid))
                Athena::OnServerExecuteInventoryItem(Read<Guid>(params));
        } else if (n == "ServerExecuteInventoryWeapon") {
            Athena::OnServerExecuteInventoryWeapon(Read<void*>(params));
        } else if (n == "ServerHandlePickup") {
            Athena::ServerHandlePickup(Read<void*>(params));
        } else if (n == "ServerAttemptInteract") {
            Athena::Loot(Read<void*>(params));
        } else if (n == "ServerAttemptAircraftJump" && !session.aircraft_jumped) {
            Athena::OnAircraftJump();
        } else if (n == "ServerCreateBuildingActor") {
            Athena::OnServerCreateBuildingActor(params);
        } else if (n == "ServerBeginEditingBuildingActor") {
            Athena::OnBeginEditActor(Read<void*>(params));
        } else if (n == "ServerEditBuildingActor") {
            struct P { void* BA; void* NC; int RI; bool M; };
            auto* p = (P*)params;
            Athena::OnFinishEditActor(p->BA, p->NC, p->RI, p->M, params);
        } else if (n == "ServerPlayEmoteItem") {
            Athena::PlayEmoteItem(Read<void*>(params));
        } else if (n == "ServerAttemptExitVehicle") {
            Athena::OnExitVehicle();
        } else if (n == "ServerSpawnInventoryDrop" || n == "ServerAttemptInventoryDrop") {
            Guid gg = Read<Guid>(params);
            int cnt = Read<int>(params, sizeof(Guid));
            Athena::DropInventoryItem(gg, cnt);
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
    if (wn != lastWorld) { LOG(">>> WorldTick world: '%s'", wn.c_str()); lastWorld = wn; }
    if (wn.find("Frontend") != std::string::npos) return;
    if (wn.find("Artemis_Terrain") == std::string::npos) return;
    int index = Read<int>(world, 0xC);
    if (world != session.world || index != session.world_index) {
        session = Session{};
        session.world = world;
        session.world_index = index;
        session.observed_at = Monotonic();
        LOG(">>> Artemis_Terrain detected: %s", wn.c_str());
    }
    double now = Monotonic();
    if (now - session.observed_at < 5.0) return;
    Athena::Tick();
}

static bool exec_hook(void* engine, void* world, const char16_t* command, void* output) {
    if (!orig_Exec) return false;
    std::string cmd = Narrow(command, 256);
    if (!cmd.empty()) LOG("Exec: %s", cmd.c_str());
    return orig_Exec(engine, world, command, output);
}

static void InstallHooks() {
    LOG("=== InstallHooks ===");
    if (DobbyHook((void*)(g_base + profile::ProcessEvent), (void*)pe_hook, (void**)&orig_PE) != 0 || !orig_PE) {
        LOG("ProcessEvent hook FAILED"); return;
    }
    LOG("ProcessEvent hooked");
    if (DobbyHook((void*)(g_base + profile::WorldTick), (void*)tick_hook, (void**)&orig_Tick) != 0 || !orig_Tick)
        LOG("WorldTick hook FAILED");
    else LOG("WorldTick hooked");
    if (DobbyHook((void*)(g_base + profile::EngineExec), (void*)exec_hook, (void**)&orig_Exec) != 0 || !orig_Exec)
        LOG("EngineExec hook FAILED");
    else LOG("EngineExec hooked");
}

static int InspectCb(dl_phdr_info* info, size_t, void*) {
    const char* name = strrchr(info->dlpi_name, '/');
    name = name ? name + 1 : info->dlpi_name;
    if (strcmp(name, "libUnreal.so")) return 0;
    g_base = info->dlpi_addr;
    g_status.store(1);
    LOG("Found libUnreal.so base=%p", (void*)g_base);
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
