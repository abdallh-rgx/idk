// ============================================================================
// EraCH3 — Fortnite 21.30 CL-21088273 Android server
// - FixBuildingFoundations disabled (crashes on this build)
// - GetProp has FName sanity check (defense in depth)
// - Extra logs in InitializeInventory / GrantDefaultAbilities
// - All previous fixes preserved
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
// PROFILE — Fortnite 21.30 CL-21088273
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

constexpr unsigned ArrayInner       = 0x70;
constexpr unsigned StructPropStruct = 0x70;
constexpr unsigned ObjectPropClass  = 0x70;

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
// BASIC STRUCTS
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
struct Guid          { uint32 A, B, C, D; };
struct Vector        { float X, Y, Z; };
struct Rotator       { float Pitch, Yaw, Roll; };
struct ItemAndCount  { int32 Count; uint32 _pad; void* Item; };
struct SoftObjRef    { uint8 _pad[0x28]; };
struct alignas(16) Transform {
    float Rotation[4]{0,0,0,1};
    float Location[3]{0,0,5000};
    float Scale[3]{1,1,1};
    float _pad[2]{};
};
static_assert(sizeof(Transform) == 48, "Transform must be 48 bytes");

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
// isa / subclass
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

// Safely resolve an FName from an FField/UObject to std::string
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

// ============================================================================
// ObjName — thread-safe + FName sanity
// ============================================================================
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

// ============================================================================
// PROPERTY LOOKUP — UE5 FField chain (with FName sanity check)
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
        // FField chain
        void* f = Read<void*>(klass, profile::StructChildProps);
        for (int i = 0; f && !IsBadPtr(f) && i < 1024; ++i) {
            std::string n;
            if (SafeFieldName(f, profile::FFieldName, n) && n == propName) return f;
            f = Read<void*>(f, profile::FFieldNext);
        }
        // UField chain
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
// Call class
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
static void* LoadAsset(const std::string& path, const char* type_path = "/Script/FortniteGame.FortItemDefinition") {
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
    if (!resolved) {
        pawnProp = GetProp("/Script/Engine.Controller.Pawn");
        resolved = true;
        LOG("GetPawn: pawnProp=%p", pawnProp);
    }
    if (!pawnProp) return nullptr;
    return Read<void*>(GetField(controller, pawnProp));
}

static void Possess(void* controller, void* pawn) {
    if (!controller || !pawn) return;
    Call c("/Script/Engine.Controller.Possess");
    if (!c.ok()) { LOG("Possess: binding failed"); return; }
    if (!c.set("InPawn", pawn)) { c.dump("Possess"); LOG("Possess: param 'InPawn' missing"); return; }
    if (c.run(controller)) LOG("Possess: dispatched -> %p", pawn);
    else LOG("Possess: run failed");
}

static void* GetPC(void* world, int index = 0) {
    if (!world) return nullptr;
    return Fn<void*(*)(void*, int)>(profile::GetPlayerController)(world, index);
}
static void* GetWorldField(void* world, const char* path) {
    void* p = GetProp(path);
    if (!p || !world) return nullptr;
    return Read<void*>(GetField(world, p));
}
static void* GetObjField(void* obj, const char* path) {
    void* p = GetProp(path);
    if (!p || !obj) return nullptr;
    if (Read<int>(p, profile::PropElementSize) != 8) return nullptr;
    return Read<void*>(GetField(obj, p));
}
static void* SpawnObj(void* klass, void* outer) {
    if (!klass) return nullptr;
    return Fn<void*(*)(void*, void*)>(profile::SpawnObject)(klass, outer);
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
    double observed_at = 0;
    double retry_at = 0;
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
    void SpawnPawn();
    void ShowSkin();
    void DestroyLods();
    void DropLoadingScreen();
    void InitializeInventory();
    void AddToInventory(void* def, int count, int slot);
    void InventoryUpdate();
    void GrantDefaultAbilities();
    void GrantAbility(void* klass);
    void SpawnPickup(void* def, int count, Vector loc, bool toss = true);
    void DropInventoryItem(Guid guid, int count);
    void OnServerExecuteInventoryItem(Guid guid);
    void OnServerExecuteInventoryWeapon(void* weapon);
    void OnAircraftJump();
    void SpawnBuildPreviews();
    void ServerHandlePickup(void* pickup);
    void Loot(void* actor);
    void OnServerCreateBuildingActor(void* params);
    void OnBeginEditActor(void* piece);
    void OnFinishEditActor(void* actor, void* newClass, int rotIter, bool mirrored, void* params);
    void PlayEmoteItem(void* item);
    void Tick();
    void OnExitVehicle();

    bool FindSafeSpawnLocation(void* world, void* mode, void* controller, Vector& out);
}

// ============================================================================
// SPAWN LOCATION
// ============================================================================
bool Athena::FindSafeSpawnLocation(void* world, void* mode, void* controller, Vector& out) {
    out = Vector{0, 0, 5000};

    if (mode && controller) {
        Call choose("/Script/Engine.GameModeBase.ChoosePlayerStart");
        if (choose.ok() && choose.set("Player", controller)) {
            if (choose.run(mode)) {
                void* start = choose.result<void*>();
                if (start && !IsBadPtr(start)) {
                    Call loc("/Script/Engine.Actor.K2_GetActorLocation");
                    if (loc.ok() && loc.run(start)) {
                        out = loc.result<Vector>();
                        LOG("[Spawn] ChoosePlayerStart -> %s loc=(%.0f,%.0f,%.0f)",
                            ObjName(start).c_str(), out.X, out.Y, out.Z);
                        return true;
                    }
                }
            }
        }
    }

    GObjects_t* go = *(GObjects_t**)(g_base + profile::GObjects);
    if (!go) return false;

    struct Candidate { const char* path; const char* label; };
    Candidate want[] = {
        { "/Script/FortniteGame.FortPlayerStart", "FortPlayerStart" },
        { "/Script/FortniteGame.FortPlayerStartWarmup", "FortPlayerStartWarmup" },
        { "/Script/Engine.PlayerStart", "PlayerStart" },
    };

    for (auto& w : want) {
        void* cls = FindObj(w.path);
        if (!cls || IsBadPtr(cls)) continue;
        void* best = nullptr;
        Vector bestLoc{0,0,0};
        for (int32 i = 0; i < go->NumElements; ++i) {
            void* o = go->GetById(i);
            if (!o || IsBadPtr(o)) continue;
            void* k = Read<void*>(o, profile::ObjectClass);
            if (k != cls) continue;
            Call loc("/Script/Engine.Actor.K2_GetActorLocation");
            if (!loc.ok() || !loc.run(o)) continue;
            Vector v = loc.result<Vector>();
            if (best == nullptr || v.Z > bestLoc.Z) { best = o; bestLoc = v; }
        }
        if (best) {
            out = bestLoc;
            LOG("[Spawn] Manual %s -> %s loc=(%.0f,%.0f,%.0f)",
                w.label, ObjName(best).c_str(), bestLoc.X, bestLoc.Y, bestLoc.Z);
            return true;
        }
    }

    LOG("[Spawn] WARNING: no PlayerStart found, using fallback (0,0,5000)");
    return false;
}

void Athena::SpawnPawn() {
    void* klass = LoadClass("/Game/Athena/PlayerPawn_Athena.PlayerPawn_Athena_C", "/Script/Engine.Pawn");
    if (!klass) { LOG("SpawnPawn: class missing"); return; }

    Vector loc{0, 0, 5000};
    FindSafeSpawnLocation(session.world, session.mode, session.controller, loc);

    Transform t{};
    t.Location[0] = loc.X;
    t.Location[1] = loc.Y;
    t.Location[2] = loc.Z + 100.0f;

    LOG("SpawnPawn: spawning at (%.0f,%.0f,%.0f)...", t.Location[0], t.Location[1], t.Location[2]);
    session.pawn = SpawnActor(session.world, klass, session.controller, t);
    if (!session.pawn) { LOG("SpawnPawn: spawn failed"); return; }

    LOG("Pawn spawned: %s at (%.0f,%.0f,%.0f)",
        ObjName(session.pawn).c_str(), t.Location[0], t.Location[1], t.Location[2]);

    LOG("SpawnPawn: calling Possess...");
    Possess(session.controller, session.pawn);
    LOG("SpawnPawn: Possess returned");

    void* check = GetPawn(session.controller);
    LOG("SpawnPawn: GetPawn returned %p (expected %p)", check, session.pawn);
    if (check != session.pawn) LOG("WARNING: possession did not stick");
    LOG("SpawnPawn: complete");
}

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
        if (c.ok() && c.set("CharacterPartType", type) && c.set("ChosenCharacterPart", part)) {
            c.run(session.pawn);
        }
    }

    void* state = GetObjField(session.controller, "/Script/Engine.Controller.PlayerState");
    if (state) {
        Call a("/Script/FortniteGame.FortPlayerState.OnRep_CharacterParts");
        if (a.ok()) a.run(state);
        Call b("/Script/FortniteGame.FortPlayerState.OnRep_CharacterData");
        if (b.ok()) b.run(state);
    }
    Call cl("/Script/FortniteGame.FortPlayerPawn.OnRep_CustomizationLoadout");
    if (cl.ok()) cl.run(session.pawn);
    LOG("Skin applied (%zu parts)", session.character_parts.size());
}

void Athena::DestroyLods() {
    LOG("DestroyLods: entering");
    void* cm = GetObjField(session.controller, "/Script/Engine.PlayerController.CheatManager");
    if (!cm) { LOG("DestroyLods: no CheatManager"); return; }
    void* cls = FindObj("/Script/FortniteGame.FortHLODSMActor");
    if (!cls) { LOG("DestroyLods: no class"); return; }
    Call c("/Script/Engine.CheatManager.DestroyAll");
    if (c.ok() && c.set("aClass", cls)) c.run(cm);
    LOG("DestroyLods: done");
}

// ============================================================================
// DROP LOADING
// ============================================================================
void Athena::DropLoadingScreen() {
    LOG("DropLoadingScreen: entering");
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
    if (rsm.ok()) rsm.run(session.controller);
    LOG("DropLoadingScreen: done");
}

// ============================================================================
// INVENTORY
// ============================================================================
void Athena::InventoryUpdate() {
    if (session.inventory) {
        Call a("/Script/FortniteGame.FortInventory.HandleInventoryLocalUpdate");
        if (a.ok()) a.run(session.inventory);
    }
    {
        Call b("/Script/FortniteGame.FortPlayerController.HandleWorldInventoryLocalUpdate");
        if (b.ok()) b.run(session.controller);
    }
    {
        Call c("/Script/FortniteGame.FortPlayerController.OnRep_QuickBar");
        if (c.ok()) c.run(session.controller);
    }
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
    if (qb.ok() && qb.set("ItemDefinition", def) && qb.set("QuickBarType", bar) && qb.set("Slot", slot)) {
        qb.run(session.controller);
    }
    LOG("Inv+ %s x%d slot=%d", ObjName(def).c_str(), count, slot);
}

void Athena::InitializeInventory() {
    LOG("InitializeInventory: entering");
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
    LOG("Inventory ready: inv=%p qb=%p", session.inventory, session.quickbars);

    const char* ammo[] = {
        "/Game/Athena/Items/Ammo/AthenaAmmoDataBulletsLight.AthenaAmmoDataBulletsLight",
        "/Game/Athena/Items/Ammo/AthenaAmmoDataBulletsMedium.AthenaAmmoDataBulletsMedium",
        "/Game/Athena/Items/Ammo/AthenaAmmoDataBulletsHeavy.AthenaAmmoDataBulletsHeavy",
        "/Game/Athena/Items/Ammo/AthenaAmmoDataShells.AthenaAmmoDataShells",
        nullptr};
    for (int i = 0; ammo[i]; ++i) {
        void* def = LoadAsset(ammo[i]);
        if (def) AddToInventory(def, 999, 3);
    }
    InventoryUpdate();
    LOG("InitializeInventory: done");
}

// ============================================================================
// WEAPON EQUIP
// ============================================================================
static bool EquipWeapon(void* pawn, void* def, Guid guid) {
    if (!pawn || !def) return false;
    Call c("/Script/FortniteGame.FortPawn.EquipWeaponDefinition");
    if (!c.ok()) { LOG("EquipWeapon: binding failed"); return false; }
    if (!c.set("WeaponData", def)) { c.dump("EquipWeaponDefinition"); return false; }
    if (!c.set("ItemEntry", guid)) { c.dump("EquipWeaponDefinition"); return false; }
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
// ABILITIES
// ============================================================================
void Athena::GrantAbility(void* klass) {
    if (!klass || !session.pawn) return;
    void* asc = GetObjField(session.pawn, "/Script/FortniteGame.FortPawn.AbilitySystemComponent");
    if (!asc) return;

    void* ge = FindObj("/Game/Athena/Items/Consumables/PurpleStuff/GE_Athena_PurpleStuff.GE_Athena_PurpleStuff_C");
    if (!ge) ge = FindObj("/Game/Athena/Items/Consumables/PurpleStuff/GE_Athena_PurpleStuff_Health.GE_Athena_PurpleStuff_Health_C");
    if (!ge) return;

    Call apply("/Script/GameplayAbilities.AbilitySystemComponent.BP_ApplyGameplayEffectToSelf");
    if (apply.ok() && apply.set("GameplayEffectClass", ge) && apply.set("Level", 1.0f)) {
        apply.run(asc);
    }
}

void Athena::GrantDefaultAbilities() {
    LOG("GrantDefaultAbilities: entering");
    GrantAbility(FindObj("/Script/FortniteGame.FortGameplayAbility_Sprint"));
    GrantAbility(FindObj("/Script/FortniteGame.FortGameplayAbility_Jump"));
    LOG("GrantDefaultAbilities: done");
}

// ============================================================================
// SPAWN PICKUP
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
    (void)toss;
}

// ============================================================================
// LOOT INTERACT
// ============================================================================
void Athena::Loot(void* actor) {
    if (!actor) return;
    LOG("Loot: called on %s", ObjName(actor).c_str());
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
    Call d("/Script/Engine.Actor.K2_DestroyActor");
    if (d.ok()) d.run(pickup);
}

// ============================================================================
// BUILDING
// ============================================================================
void Athena::SpawnBuildPreviews() {
    LOG("SpawnBuildPreviews: entering");
    void* previewCls = FindObj("BuildingPlayerPrimitivePreview");
    if (!previewCls || !session.controller) { LOG("SpawnBuildPreviews: no class/controller"); return; }
    LOG("SpawnBuildPreviews: done");
}

void Athena::OnServerCreateBuildingActor(void* params) { (void)params; }
void Athena::OnBeginEditActor(void* piece) { (void)piece; }
void Athena::OnFinishEditActor(void* actor, void* newClass, int rotIter, bool mirrored, void* params) {
    (void)actor; (void)newClass; (void)rotIter; (void)mirrored; (void)params;
}

// ============================================================================
// AIRCRAFT
// ============================================================================
void Athena::OnAircraftJump() {
    LOG("OnAircraftJump: entering");
    if (!session.controller) return;
    Call loc("/Script/Engine.PlayerController.GetFocalLocation");
    Vector l{0, 0, 20000};
    if (loc.ok() && loc.run(session.controller)) l = loc.result<Vector>();
    void* klass = LoadClass("/Game/Athena/PlayerPawn_Athena.PlayerPawn_Athena_C", "/Script/Engine.Pawn");
    if (!klass) return;
    Transform t{};
    t.Location[0] = l.X; t.Location[1] = l.Y; t.Location[2] = l.Z;
    session.pawn = SpawnActor(session.world, klass, session.controller, t);
    if (!session.pawn) return;
    Possess(session.controller, session.pawn);
    session.aircraft_jumped = true;
    LOG("Aircraft jump -> Pawn %p", session.pawn);
}

void Athena::PlayEmoteItem(void* item) { (void)item; }

void Athena::Tick() {
    if (!session.pawn) return;
}

void Athena::OnExitVehicle() { (void)session.pawn; }

void Athena::DropInventoryItem(Guid guid, int count) {
    (void)guid; (void)count;
}

// ============================================================================
// SETUP WORLD
// ============================================================================
void Athena::SetupWorld(void* world) {
    session.world = world;
    session.controller = GetPC(world, 0);
    if (!session.controller) { LOG("SetupWorld: no controller yet"); return; }

    void* ctrlClass = Read<void*>(session.controller, profile::ObjectClass);
    LOG("SetupWorld: controller class = %s", ObjName(ctrlClass).c_str());

    void* baseCtrl = FindObj("/Script/FortniteGame.FortPlayerController");
    if (!baseCtrl) { LOG("SetupWorld: FortPlayerController base not found"); return; }
    if (!IsA(session.controller, baseCtrl)) {
        LOG("SetupWorld: controller is not a FortPlayerController");
        return;
    }

    void* modeProp  = GetProp("/Script/Engine.World.AuthorityGameMode");
    void* stateProp = GetProp("/Script/Engine.World.GameState");
    LOG("SetupWorld: modeProp=%p stateProp=%p", modeProp, stateProp);

    session.mode  = modeProp  ? Read<void*>(GetField(world, modeProp))  : nullptr;
    session.state = stateProp ? Read<void*>(GetField(world, stateProp)) : nullptr;

    LOG("SetupWorld: PC=%s GM=%s GS=%s",
        ObjName(session.controller).c_str(),
        ObjName(session.mode).c_str(),
        ObjName(session.state).c_str());

    if (!session.mode) { LOG("SetupWorld: GameMode is null"); return; }
    if (!session.state) { LOG("SetupWorld: GameState is null"); return; }

    LOG("SetupWorld: reading Pawn property...");
    void* pawnProp = GetProp("/Script/Engine.Controller.Pawn");
    void* oldPawn = pawnProp ? Read<void*>(GetField(session.controller, pawnProp)) : nullptr;
    LOG("SetupWorld: oldPawn=%p", oldPawn);

    void* pawnCls = FindObj("/Game/Athena/PlayerPawn_Athena.PlayerPawn_Athena_C");
    bool isPawn = oldPawn && pawnCls && IsA(oldPawn, pawnCls);
    LOG("SetupWorld: isPawn=%d", (int)isPawn);

    if (!isPawn) SpawnPawn();
    else session.pawn = oldPawn;

    if (!session.pawn) { LOG("SetupWorld: no pawn — aborting"); return; }

    LOG("SetupWorld: calling ShowSkin");
    ShowSkin();
    LOG("SetupWorld: ShowSkin done");

    LOG("SetupWorld: calling DestroyLods");
    DestroyLods();
    LOG("SetupWorld: DestroyLods done");

    LOG("SetupWorld: SKIPPING FixBuildingFoundations");

    LOG("SetupWorld: calling InitializeInventory");
    InitializeInventory();
    LOG("SetupWorld: InitializeInventory done");

    LOG("SetupWorld: calling GrantDefaultAbilities");
    GrantDefaultAbilities();
    LOG("SetupWorld: GrantDefaultAbilities done");

    LOG("SetupWorld: calling SpawnBuildPreviews");
    SpawnBuildPreviews();
    LOG("SetupWorld: SpawnBuildPreviews done");

    LOG("SetupWorld: calling DropLoadingScreen");
    DropLoadingScreen();
    LOG("SetupWorld: DropLoadingScreen done");

    session.initialized = true;
    LOG("World setup complete: pawn=%p at %s",
        session.pawn, ObjName(session.pawn).c_str());
}

// ============================================================================
// HOOKS
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
    if (session.initialized) { Athena::Tick(); return; }
    if (now - session.observed_at < 5.0) return;
    if (now - session.retry_at < 1.0) return;
    session.retry_at = now;
    Athena::SetupWorld(world);
}

static bool exec_hook(void* engine, void* world, const char16_t* command, void* output) {
    if (!orig_Exec) return false;
    std::string cmd = Narrow(command, 256);
    if (!cmd.empty()) LOG("Exec: %s", cmd.c_str());
    return orig_Exec(engine, world, command, output);
}

// ============================================================================
// INSTALL HOOKS
// ============================================================================
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

// ============================================================================
// LIB DETECTION
// ============================================================================
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
    for (int i = 0; i < 600 && g_status.load() == 0; ++i) {
        dl_iterate_phdr(InspectCb, nullptr);
        if (g_status.load() == 0) usleep(100000);
    }
    if (g_status.load() != 1) { LOG("Timeout waiting for libUnreal.so"); return nullptr; }
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
