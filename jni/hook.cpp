// ============================================================================
// EraCH3 v15.0 — Fortnite 21.30 CL-21088273 Android
// MERGED: game.cpp techniques + hook(9).cpp systems + SDK 21.30 offsets
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
    char line[2048]; va_list ap; va_start(ap, fmt);
    vsnprintf(line, sizeof(line), fmt, ap); va_end(ap);
    __android_log_write(ANDROID_LOG_INFO, "EraCH3", line);
    if (g_logFile) { fprintf(g_logFile, "%s\n", line); fflush(g_logFile); }
}
#define LOG(...) LogImpl(__VA_ARGS__)

using int8=int8_t; using int16=int16_t; using int32=int32_t; using int64=int64_t;
using uint8=uint8_t; using uint16=uint16_t; using uint32=uint32_t; using uint64=uint64_t;
using TCHAR=char16_t;

// ============================================================================
// PROFILE — RVAs + Offsets verified from SDK Dumper 21.30
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
constexpr uintptr_t IsStandalone        = 0x0945F840;
constexpr uintptr_t GetPlayerController = 0x093A1D48;
constexpr uintptr_t GetPawn             = 0x093A1D60;
constexpr uintptr_t Possess             = 0x093A3A00;
constexpr uintptr_t BeginSpawn          = 0x093A37E4;
constexpr uintptr_t FinishSpawn         = 0x0939F1B8;
constexpr uintptr_t SpawnObject         = 0x093A3388;
constexpr uintptr_t GenericArrayAdd     = 0x0944A668;
constexpr uintptr_t GenericArrayRemove  = 0x0944B2DC;
constexpr uintptr_t IsGarbageCollecting = 0x0756E138;
constexpr uintptr_t IsSavingPackage     = 0x0760A59C;
constexpr uintptr_t GObjects            = 0x0DCD6E08;
constexpr uintptr_t GetInterfaceAddress = 0x0761AA88;
constexpr uintptr_t NewGuid             = 0x0753B000;
constexpr uintptr_t InitializeAthenaQuickBars = 0x056607EC;
constexpr uintptr_t ResolveInterfaceRVA = 0x05d8f3c8;

// ─── Object layout ───
constexpr unsigned ObjectClass      = 0x10;
constexpr unsigned ObjectName       = 0x18;
constexpr unsigned ObjectOuter      = 0x20;
constexpr unsigned FieldNext        = 0x28;
constexpr unsigned StructSuper      = 0x40;
constexpr unsigned ClassDefaultObjChain = 0x40;
constexpr unsigned ClassDefaultObjDepth = 0x48;
constexpr unsigned StructChildren   = 0x48;
constexpr unsigned StructChildProps = 0x50;
constexpr unsigned FunctionParamsSize = 0xB6;
constexpr unsigned PropFlags        = 0x38;
constexpr unsigned PropOffset       = 0x44;
constexpr unsigned PropElementSize  = 0x34;

// BoolProperty (FField hierarchy on UE4.27 Android)
constexpr unsigned BoolSize         = 0x80;
constexpr unsigned BoolByteOffset   = 0x81;
constexpr unsigned BoolFieldMask    = 0x82;

constexpr unsigned FFieldName = 0x28;
constexpr unsigned FFieldNext = 0x20;

constexpr uint64_t CPF_Parm       = 0x00000080ULL;
constexpr uint64_t CPF_OutParm    = 0x00000100ULL;
constexpr uint64_t CPF_ConstParm  = 0x00000200ULL;
constexpr uint64_t CPF_ReturnParm = 0x00000400ULL;

// ─── Actor / Controller / Pawn ───
constexpr unsigned PC_WorldInventory        = 0x1D20;
constexpr unsigned PC_ClientQuickBars       = 0x1A60;
constexpr unsigned PC_CosmeticLoadoutPC     = 0x1E70;
constexpr unsigned PC_MyPlayerInfo          = 0x2088;
constexpr unsigned PC_CheatManager          = 0x2F20;
constexpr unsigned PC_bBuildFree_Byte       = 0x185B;
constexpr unsigned PC_bBuildFree_Bit        = 0;

constexpr unsigned Pawn_bIsSkydiving        = 0x1A1D;
constexpr unsigned Pawn_bIsParachuteOpen    = 0x1A1E;

constexpr unsigned PS_HeroType              = 0x0430;
constexpr unsigned PS_TeamIndex             = 0x0C24;

constexpr unsigned Pickup_PrimaryEntry      = 0x02F8;

constexpr unsigned Container_SearchLootTierGroup = 0x0E20;
constexpr unsigned Container_bAlreadySearched_Byte = 0x0FB1;
constexpr unsigned Container_bAlreadySearched_Bit  = 7;

constexpr unsigned Wall_bDoorOpen_Byte      = 0x0FDE;
constexpr unsigned Wall_bDoorOpen_Bit       = 5;

constexpr unsigned GS_GamePhase             = 0x2150;
constexpr unsigned GS_SafeZoneIndicator     = 0x19A8;

constexpr unsigned ItemEntry_Count          = 0x0014;
constexpr unsigned ItemEntry_Durability     = 0x0024;
constexpr unsigned ItemEntry_LoadedAmmo     = 0x002C;
constexpr unsigned ItemEntry_ItemGuid       = 0x0050;
constexpr unsigned ItemEntry_Size           = 0x01A0;

constexpr unsigned QuickBar_Slots           = 0x0010;
constexpr unsigned QuickBarSlot_Size        = 0x28;

constexpr unsigned CMC_MaxWalkSpeed         = 0x01E8;
constexpr unsigned CMC_MaxFlySpeed          = 0x01F4;
constexpr unsigned CMC_MaxSwimSpeed         = 0x01F0;
constexpr unsigned CMC_JumpZVelocity        = 0x0170;
constexpr unsigned CMC_GravityScale         = 0x0168;
}

extern "C" int DobbyHook(void* target, void* replace, void** origin);

// ============================================================================
// MEMORY
// ============================================================================
static inline bool IsBadPtr(const void* p) {
    if (!p) return true;
    uintptr_t v = (uintptr_t)p;
    return v < 0x10000 || v > 0x0000FFFFFFFFFFFFULL;
}
template<class T> static T Read(const void* p, size_t off = 0) {
    T r{}; if (p) memcpy(&r, (const char*)p + off, sizeof(T)); return r;
}
template<class T> static void Write(void* p, size_t off, const T& v) {
    if (p) memcpy((char*)p + off, &v, sizeof(T));
}
template<typename T> static T& Off(void* base, unsigned offset) {
    return *reinterpret_cast<T*>(reinterpret_cast<char*>(base) + offset);
}

static uintptr_t g_base = 0;
static std::atomic<int> g_status{0};
static std::atomic<int> g_gameTid{0};
static thread_local bool g_internal = false;
struct Guard { bool s = g_internal; Guard() { g_internal = true; } ~Guard() { g_internal = s; } };
template<class F> static F Fn(uintptr_t rva) { return reinterpret_cast<F>(g_base + rva); }

// ============================================================================
// TYPES — ★ CRITICAL: float (Android scalar), NOT double
// ============================================================================
struct FStringNative { TCHAR* Data; int32 Count; int32 Max; };
struct FName         { uint32 CompIdx; uint32 DispIdx; };
struct Array         { void* Data; int32 Count; int32 Max; };
struct Guid          { uint32 A, B, C, D; };
struct Vector        { float X, Y, Z; };
struct Rotator       { float Pitch, Yaw, Roll; };

struct alignas(16) Transform {
    float Rotation[4]{0, 0, 0, 1};
    float Location[3]{0, 0, 0};
    float Scale[3]{1, 1, 1};
    float TrailingPadding[2]{};
};
static_assert(sizeof(Vector) == 12, "Vector must be 12 bytes (Android float)");
static_assert(sizeof(Rotator) == 12, "Rotator must be 12 bytes (Android float)");
static_assert(sizeof(Transform) == 48, "Transform must be 48 bytes (Android scalar)");
static_assert(sizeof(FStringNative) == 16, "FString must be 16 bytes");
static_assert(sizeof(Guid) == 16, "Guid must be 16 bytes");
static_assert(offsetof(Transform, Rotation) == 0 && offsetof(Transform, Location) == 0x10 &&
              offsetof(Transform, Scale) == 0x1C, "Android scalar FTransform layout");

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
static bool SubclassOf(void* klass, void* type);
static void* GetProp(const char* path);
static void* GetField(void* obj, void* prop);
static void SetBool(void* obj, const char* path, bool enabled);
static bool GetBool(void* obj, const char* path);
static std::u16string ToWide(const std::string& s);
static std::string Narrow(const char16_t* s, int max);
static void* GetObjField(void* obj, const char* path);
static void* LoadAsset(const std::string& path, const char* type_path);
static void* LoadClass(const char* path, const char* base_path);
static void* GetPC(void* world, int index);
static void* GetPawnFn(void* controller);
static void* GetASC(void* actor);

// ============================================================================
// STRINGS
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
// TYPE CHECKS — ★ CDO chain method (from game.cpp, faster + safer)
// ============================================================================
static bool IsA(void* object, void* type) {
    if (!object || !type || IsBadPtr(object) || IsBadPtr(type)) return false;
    void* klass = Read<void*>(object, profile::ObjectClass);
    if (!klass || IsBadPtr(klass)) return false;
    int n = Read<int>(type, profile::ClassDefaultObjDepth);
    int limit = Read<int>(klass, profile::ClassDefaultObjDepth);
    void* chain = Read<void*>(klass, profile::ClassDefaultObjChain);
    return n >= 0 && n < 1024 && n <= limit && chain &&
        Read<void*>(chain, size_t(n) * 8) == static_cast<char*>(type) + 0x40;
}
static bool SubclassOf(void* klass, void* type) {
    if (!klass || !type || IsBadPtr(klass) || IsBadPtr(type)) return false;
    int n = Read<int>(type, profile::ClassDefaultObjDepth);
    int limit = Read<int>(klass, profile::ClassDefaultObjDepth);
    void* chain = Read<void*>(klass, profile::ClassDefaultObjChain);
    return n >= 0 && n < 1024 && n <= limit && chain &&
        Read<void*>(chain, size_t(n) * 8) == static_cast<char*>(type) + 0x40;
}

// ============================================================================
// PROPERTIES
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
    int off = Read<int>(prop, profile::PropOffset);
    if (off < 0 || off > 0x20000) return nullptr;
    return (char*)obj + off;
}
static void SetBool(void* obj, const char* path, bool enabled) {
    void* p = GetProp(path);
    if (!p || !obj) return;
    uint8_t size = Read<uint8_t>(p, profile::BoolSize);
    uint8_t byte = Read<uint8_t>(p, profile::BoolByteOffset);
    uint8_t mask = Read<uint8_t>(p, profile::BoolFieldMask);
    if (!size || size > 8 || byte >= size || !mask) {
        LOG("[SetBool] rejected %s size=%u byte=%u mask=0x%02X", path, size, byte, mask);
        return;
    }
    auto dst = (uint8_t*)GetField(obj, p) + byte;
    *dst = enabled ? (*dst | mask) : (*dst & ~mask);
}
static bool GetBool(void* obj, const char* path) {
    void* p = GetProp(path);
    if (!p || !obj) return false;
    uint8_t byte = Read<uint8_t>(p, profile::BoolByteOffset);
    uint8_t mask = Read<uint8_t>(p, profile::BoolFieldMask);
    uint8_t val = Read<uint8_t>(GetField(obj, p), byte);
    return (val & mask) != 0;
}
static void* GetObjField(void* obj, const char* path) {
    void* p = GetProp(path);
    if (!p || !obj) return nullptr;
    if (Read<int>(p, profile::PropElementSize) != 8) return nullptr;
    void* v = Read<void*>(GetField(obj, p));
    return (v && !IsBadPtr(v)) ? v : nullptr;
}
template<class T> static bool GetFieldValue(void* object, const char* path, T& value) {
    void* p = GetProp(path);
    if (!object || !p || Read<int>(p, profile::PropElementSize) != sizeof(T)) return false;
    value = Read<T>(GetField(object, p));
    return true;
}

// ============================================================================
// HOOKS
// ============================================================================
using PE_t   = void(*)(void*, void*, void*);
using Tick_t = void(*)(void*, int, float);
using Exec_t = bool(*)(void*, void*, const char16_t*, void*);
static PE_t   orig_PE = nullptr;
static Tick_t orig_Tick = nullptr;
static Exec_t orig_Exec = nullptr;

// ============================================================================
// CALL — ★ with Blueprint event_receiver resolution (from game.cpp)
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
        if (!m_func || IsBadPtr(m_func)) return;

        // ★ Resolve Blueprint override if receiver provided
        if (event_receiver && !IsBadPtr(event_receiver)) {
            std::string event = ObjName(m_func);
            void* klass = Read<void*>(event_receiver, profile::ObjectClass);
            void* chain = Read<void*>(klass, profile::ClassDefaultObjChain);
            int depth = Read<int>(klass, profile::ClassDefaultObjDepth);
            bool found = false;
            if (chain && depth >= 0 && depth < 1024) {
                for (int i = depth; i >= 0 && !found; --i) {
                    void* base_chain = Read<void*>(chain, size_t(i) * 8);
                    if (!base_chain) continue;
                    void* type = static_cast<char*>(base_chain) - 0x40;
                    void* child = Read<void*>(type, profile::StructChildren);
                    for (int j = 0; child && j < 4096; ++j, child = Read<void*>(child, profile::FieldNext)) {
                        if (ObjName(child) == event && IsA(child, FindObj("/Script/CoreUObject.Function"))) {
                            m_func = child; found = true; break;
                        }
                    }
                }
            }
        }

        int total = Read<uint16_t>(m_func, profile::FunctionParamsSize);
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

    template<class T> bool set(const char* key, const T& value) {
        for (auto& p : m_params)
            if (p.name == key && p.size == (int)sizeof(T)) {
                memcpy(m_data.data() + p.offset, &value, sizeof(T));
                return true;
            }
        return false;
    }
    bool set_bytes(const char* key, const void* src, size_t size) {
        if (!src || size == 0) return false;
        for (auto& p : m_params)
            if (p.name == key && p.size == (int)size) {
                memcpy(m_data.data() + p.offset, src, size);
                return true;
            }
        return false;
    }
    const void* result_data(size_t size) const {
        for (auto& p : m_params)
            if ((p.flags & profile::CPF_ReturnParm) && p.size == (int)size)
                return m_data.data() + p.offset;
        return nullptr;
    }
    template<class T> T result() const {
        for (auto& p : m_params)
            if ((p.flags & profile::CPF_ReturnParm) && p.size == (int)sizeof(T))
                return Read<T>(m_data.data(), p.offset);
        return {};
    }
    template<class T> T get(const char* key) const {
        for (auto& p : m_params)
            if (p.name == key && p.size == (int)sizeof(T))
                return Read<T>(m_data.data(), p.offset);
        return {};
    }
    bool run(void* object) {
        if (!m_valid || !object || !orig_PE) return false;
        if (IsBadPtr(object) || IsBadPtr(m_func)) return false;
        Guard g;
        orig_PE(object, m_func, m_data.data());
        return true;
    }
};

static void Invoke0(void* obj, const char* path) {
    Call c(path);
    if (c.ok()) c.run(obj);
}

// ★ Extract incoming parameter from RPC payload (from game.cpp)
static void* Incoming(void* function, void* data, const char* key, int expected_size = 0) {
    if (!function || !data) return nullptr;
    int total = Read<uint16_t>(function, profile::FunctionParamsSize);
    void* p = Read<void*>(function, profile::StructChildren);
    for (int i = 0; p && i < 128; ++i, p = Read<void*>(p, profile::FieldNext)) {
        if (!(Read<uint64_t>(p, profile::PropFlags) & 0x80) || ObjName(p) != key) continue;
        int offset = Read<int>(p, profile::PropOffset);
        int size = Read<int>(p, profile::PropElementSize);
        if (offset < 0 || size < 1 || offset + size > total || (expected_size && size != expected_size))
            return nullptr;
        return static_cast<char*>(data) + offset;
    }
    return nullptr;
}

// ============================================================================
// ASSETS & SPAWNING
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
    if (!world || IsBadPtr(world)) return nullptr;
    return Fn<void*(*)(void*, int)>(profile::GetPlayerController)(world, index);
}
static void* GetPawnFn(void* controller) {
    if (!controller || IsBadPtr(controller)) return nullptr;
    return Fn<void*(*)(void*)>(profile::GetPawn)(controller);
}
static void* ResolveInterface(void* object, void* ifaceClass) {
    if (!object || !ifaceClass) return nullptr;
    return Fn<void*(*)(void*, void*)>(profile::ResolveInterfaceRVA)(object, ifaceClass);
}
static bool IsStandaloneWorld(void* world) {
    if (!world || IsBadPtr(world)) return false;
    return Fn<bool(*)(void*)>(profile::IsStandalone)(world);
}

// ★ Spawn with FTransform reflection verification (from game.cpp)
static void* SpawnActor(void* world, void* klass, void* owner, const Transform& t) {
    if (!world || !klass || IsBadPtr(world) || IsBadPtr(klass)) return nullptr;
    void* rotProp = GetProp("/Script/CoreUObject.Transform.Rotation");
    void* locProp = GetProp("/Script/CoreUObject.Transform.Translation");
    void* scaleProp = GetProp("/Script/CoreUObject.Transform.Scale3D");
    if (!rotProp || !locProp || !scaleProp ||
        Read<int>(rotProp, profile::PropOffset) != 0 ||
        Read<int>(locProp, profile::PropOffset) != 0x10 ||
        Read<int>(scaleProp, profile::PropOffset) != 0x1C) {
        LOG("[Spawn] FTransform layout mismatch, refusing");
        return nullptr;
    }
    void* actor = Fn<void*(*)(void*, void*, const Transform*, uint8_t, void*)>(
        profile::BeginSpawn)(world, klass, &t, 1, owner);
    return actor ? Fn<void*(*)(void*, const Transform*)>(profile::FinishSpawn)(actor, &t) : nullptr;
}

static void* GetASC(void* actor) {
    if (!actor || IsBadPtr(actor)) return nullptr;
    void* asc = GetObjField(actor, "/Script/FortniteGame.FortPawn.AbilitySystemComponent");
    if (asc) return asc;
    void* pc = GetObjField(actor, "/Script/Engine.Pawn.Controller");
    if (!pc) pc = GetObjField(actor, "/Script/Engine.Actor.Owner");
    if (pc && !IsBadPtr(pc)) {
        void* ps = GetObjField(pc, "/Script/Engine.Controller.PlayerState");
        if (ps) {
            asc = GetObjField(ps, "/Script/GameplayAbilities.AbilitySystemComponent");
            if (!asc) asc = GetObjField(ps, "/Script/FortniteGame.FortPlayerState.AbilitySystemComponent");
            if (asc) return asc;
        }
    }
    return GetObjField(actor, "/Script/GameplayAbilities.AbilitySystemComponent");
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
    void* aircraft = nullptr;

    bool  attempted = false;
    bool  finalized = false;
    bool  inventory_ready = false;
    bool  quickbars_initialized = false;
    bool  abilities_granted = false;
    bool  melee_granted = false;
    bool  movement_enabled = false;
    bool  cosmetics_applied = false;
    bool  cheats_ready = false;
    bool  safezone_spawned = false;
    bool  ready_called = false;
    bool  visibility_forced = false;
    bool  aircraft_phase_started = false;
    bool  aircraft_jumped = false;
    bool  vehicles_spawned = false;
    bool  hud_ready = false;

    double observed_at = 0;
    double retry_at = 0;
    double finalize_at = 0;
    double bus_at = 0;

    int   last_phase = -1;

    std::array<void*, 6> slots{};
    std::array<void*, 6> secondary_slots{};
    std::vector<void*> character_parts;
    std::vector<void*> granted_ability_sets;
    void* edit_tool = nullptr;

    // Build state
    void* last_build_class = nullptr;
    std::array<float, 6> last_build_transform{};
    double last_build_time = 0;
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
    void ForcePlayerVisible();
    void EquipSlot(int slot);
    void DestroyLods();
    void DropLoadingScreen();
    void InitializeInventory();
    bool Give(void* def, int count, int slot, uint8_t bar = 0);
    void InventoryUpdate();
    void GrantAbility(void* klass);
    void GrantAllAbilitySets();
    void RefreshHUD();
    void ApplySprintSpeed();
    void EnableMovementFeatures();
    void SpawnSafeZoneIndicator();
    void SpawnBattleBus();
    void HandleJumpFromHudStart();
    void RemoveNetDebugUI();
    void SpawnBuildPreviews();
    bool LocalPickup(void* pickup);
    bool SearchContainer(void* actor, Vector location);
    bool LocalInteract(void* function, void* params);
    bool LocalBuild(void* function, void* params);
    void HandleDoor(void* door);
    void DropInventoryItem(Guid guid, int count);
    void OnServerExecuteInventoryItem(Guid guid);
    bool BindSlot(void* item, int slot, uint8_t bar);
    bool InitializeQuickBars();
    void RefreshSlots();
}

// ============================================================================
// ABILITIES
// ============================================================================
void Athena::GrantAbility(void* klass) {
    if (!klass || !session.pawn) return;
    void* asc = GetASC(session.pawn);
    if (!asc) return;
    Call c("/Script/GameplayAbilities.AbilitySystemComponent.TryActivateAbilityByClass");
    if (!c.ok()) return;
    bool bAllow = true;
    if (c.set("InAbilityToActivate", klass) && c.set("bAllowRemoteActivation", bAllow))
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
    if (!c.set("AbilitySystemInterfaceActor", iface) ||
        !c.set("AbilitySet", abilitySet) ||
        !c.set("OverrideSourceObject", pawn)) return false;
    if (!c.run(klib)) return false;
    LOG("[Abilities] Applied: %s", ObjName(abilitySet).c_str());
    return true;
}

void Athena::GrantAllAbilitySets() {
    if (!session.pawn || session.abilities_granted) return;
    session.abilities_granted = true;
    LOG("[Abilities] Setup");
    void* asc = GetASC(session.pawn);
    if (!asc) { LOG("[Abilities] no ASC"); return; }

    void* coreSet = LoadAsset(
        "/Game/Abilities/Player/Generic/Traits/DefaultPlayer/GAS_AthenaPlayer.GAS_AthenaPlayer",
        "/Script/FortniteGame.FortAbilitySet");
    if (!coreSet) { LOG("[Abilities] GAS_AthenaPlayer missing"); return; }

    // ★ Add extra abilities to the set before equipping (from game.cpp)
    void* abilitiesProp = GetProp("/Script/FortniteGame.FortAbilitySet.GameplayAbilities");
    Array* abilities = abilitiesProp ? reinterpret_cast<Array*>(GetField(coreSet, abilitiesProp)) : nullptr;
    if (abilities && abilities->Count >= 0 && abilities->Count < 256 && abilities->Max >= abilities->Count) {
        const char* extras[] = {
            "/Script/FortniteGame.FortGameplayAbility_Sprint",
            "/Script/FortniteGame.FortGameplayAbility_Jump",
            "/Game/Abilities/Player/Generic/Traits/DefaultPlayer/GA_DefaultPlayer_InteractSearch.GA_DefaultPlayer_InteractSearch_C",
            "/Game/Abilities/Player/Generic/Traits/DefaultPlayer/GA_DefaultPlayer_InteractUse.GA_DefaultPlayer_InteractUse_C",
            "/Game/Abilities/Weapons/Melee/GAB_Melee_GenericCombo.GAB_Melee_GenericCombo_C",
            nullptr
        };
        for (int i = 0; extras[i]; ++i) {
            void* klass = LoadClass(extras[i], "/Script/GameplayAbilities.GameplayAbility");
            if (!klass) continue;
            bool present = false;
            for (int j = 0; j < abilities->Count; ++j)
                if (Read<void*>(abilities->Data, j * 8) == klass) { present = true; break; }
            if (!present) {
                Fn<int(*)(void*, void*, const void*)>(profile::GenericArrayAdd)(abilities, abilitiesProp, &klass);
                LOG("[Abilities] Added: %s", extras[i]);
            }
        }
    }

    if (ApplyAbilitySet(session.pawn, coreSet)) {
        session.granted_ability_sets.push_back(coreSet);
        LOG("[Abilities] ✅ Core set applied");
    }

    // MME movement sets
    const char* mmeSets[] = {
        "/TacticalSprint/Gameplay/AS_TacticalSprint.AS_TacticalSprint",
        "/Ascender/Gameplay/Ascender/AS_Ascender.AS_Ascender",
        "/DoorBashContent/Gameplay/AS_DoorBash.AS_DoorBash",
        "/HillScramble/Gameplay/AS_HillScramble.AS_HillScramble",
        "/SlideImpulse/Gameplay/AS_SlideImpulse.AS_SlideImpulse",
        nullptr
    };
    for (int i = 0; mmeSets[i]; ++i) {
        void* set = FindObj(mmeSets[i]);
        if (!set) set = LoadAsset(mmeSets[i], "/Script/FortniteGame.FortAbilitySet");
        if (set && ApplyAbilitySet(session.pawn, set))
            session.granted_ability_sets.push_back(set);
    }

    const char* activateList[] = {
        "/Script/GameplayAbilities.GameplayAbility_CharacterJump",
        "/Game/Abilities/Player/Generic/Traits/DefaultPlayer/GA_DefaultPlayer_InteractUse.GA_DefaultPlayer_InteractUse_C",
        "/Game/Abilities/Player/Generic/Traits/DefaultPlayer/GA_DefaultPlayer_InteractSearch.GA_DefaultPlayer_InteractSearch_C",
        "/Game/Abilities/Player/Generic/Traits/DefaultPlayer/GA_DefaultPlayer_Death.GA_DefaultPlayer_Death_C",
        nullptr
    };
    for (int i = 0; activateList[i]; ++i) {
        void* cls = FindObj(activateList[i]);
        if (!cls) cls = LoadClass(activateList[i], "/Script/GameplayAbilities.GameplayAbility");
        if (cls) GrantAbility(cls);
    }
    session.melee_granted = true;
    LOG("[Abilities] Done — %zu sets", session.granted_ability_sets.size());
}

// ============================================================================
// MOVEMENT
// ============================================================================
void Athena::EnableMovementFeatures() {
    if (!session.pawn || session.movement_enabled) return;
    Call cs("/Script/FortniteGame.FortPlayerPawn.SetCanSlide");
    if (cs.ok()) { bool t = true; if (cs.set("bCanSlide", t)) cs.run(session.pawn); }
    void* slide = LoadClass("/Game/Athena/Items/Abilities/GA_Athena_Player_Slide.GA_Athena_Player_Slide_C",
                            "/Script/GameplayAbilities.GameplayAbility");
    if (slide) GrantAbility(slide);
    session.movement_enabled = true;
    LOG("[Movement] Done");
}

void Athena::ApplySprintSpeed() {
    if (!session.pawn || IsBadPtr(session.pawn)) return;
    void* cmc = GetObjField(session.pawn, "/Script/Engine.Character.CharacterMovement");
    if (cmc && !IsBadPtr(cmc)) {
        Off<float>(cmc, profile::CMC_MaxWalkSpeed)  = 600.0f;
        Off<float>(cmc, profile::CMC_MaxFlySpeed)   = 1200.0f;
        Off<float>(cmc, profile::CMC_MaxSwimSpeed)  = 400.0f;
        Off<float>(cmc, profile::CMC_JumpZVelocity) = 700.0f;
        Off<float>(cmc, profile::CMC_GravityScale)  = 1.0f;
    }
}

// ============================================================================
// INVENTORY — ★ GUID verification (from game.cpp)
// ============================================================================
void Athena::InventoryUpdate() {
    if (!session.controller) return;
    Invoke0(session.controller, "/Script/FortniteGame.FortPlayerController.ClientForceWorldInventoryUpdate");
    if (session.inventory)
        Invoke0(session.inventory, "/Script/FortniteGame.FortInventory.HandleInventoryLocalUpdate");
    Invoke0(session.controller, "/Script/FortniteGame.FortPlayerController.HandleWorldInventoryLocalUpdate");
    for (uint8_t bar : {uint8_t(0), uint8_t(1)}) {
        Call c("/Script/FortniteGame.FortPlayerController.ClientForceUpdateQuickbar");
        if (c.ok() && c.set("QuickbarToRefresh", bar)) c.run(session.controller);
    }
}

bool Athena::Give(void* def, int count, int slot, uint8_t bar) {
    if (IsBadPtr(def) || !session.inventory || !session.controller || count < 1 || count > 9999) return false;
    if (!IsA(def, FindObj("/Script/FortniteGame.FortItemDefinition"))) return false;

    // Check duplicate
    void* invProp = GetProp("/Script/FortniteGame.FortInventory.Inventory");
    void* instProp = GetProp("/Script/FortniteGame.FortItemList.ItemInstances");
    void* entryProp = GetProp("/Script/FortniteGame.FortItemList.ReplicatedEntries");
    void* itemEntryProp = GetProp("/Script/FortniteGame.FortWorldItem.ItemEntry");
    if (!invProp || !instProp || !entryProp || !itemEntryProp) return false;
    void* list = GetField(session.inventory, invProp);
    void* instArr = GetField(list, instProp);
    void* entryArr = GetField(list, entryProp);

    Array current = Read<Array>(instArr);
    for (int i = 0; i < current.Count; ++i) {
        void* existing = Read<void*>(current.Data, (size_t)i * 8);
        if (!existing) continue;
        Call d("/Script/FortniteGame.FortItem.GetItemDefinitionBP");
        if (d.ok() && d.run(existing) && d.result<void*>() == def) {
            if (slot >= 0) (bar == 0 ? session.slots : session.secondary_slots)[size_t(slot)] = existing;
            return true;
        }
    }

    Call create("/Script/FortniteGame.FortItemDefinition.CreateTemporaryItemInstanceBP");
    int level = 1;
    if (!create.ok() || !create.set("Count", count) || !create.set("Level", level) || !create.run(def)) return false;
    void* item = create.result<void*>();
    if (!IsA(item, FindObj("/Script/FortniteGame.FortWorldItem"))) return false;

    Call owner("/Script/FortniteGame.FortItem.SetOwningControllerForTemporaryItem");
    if (owner.ok() && owner.set("InController", session.controller)) owner.run(item);

    void* entry = GetField(item, itemEntryProp);
    Write<float>(entry, profile::ItemEntry_Durability, 1.0f);

    // ★ Assign GUID before insertion (from game.cpp)
    void* guidProp = GetProp("/Script/FortniteGame.FortItemEntry.ItemGuid");
    if (guidProp) {
        Guid g = Read<Guid>(GetField(entry, guidProp));
        if ((g.A | g.B | g.C | g.D) == 0) {
            Guid newG = Fn<Guid(*)()>(profile::NewGuid)();
            Write<Guid>(GetField(entry, guidProp), 0, newG);
        }
    }

    Fn<int(*)(void*, void*, const void*)>(profile::GenericArrayAdd)(instArr, instProp, &item);
    Fn<int(*)(void*, void*, const void*)>(profile::GenericArrayAdd)(entryArr, entryProp, entry);

    // ★ Verify canonical instance after reconciliation (from game.cpp)
    current = Read<Array>(instArr);
    void* canonical = nullptr;
    Guid targetGuid = Read<Guid>(GetField(entry, guidProp));
    for (int i = 0; i < current.Count; ++i) {
        void* candidate = Read<void*>(current.Data, (size_t)i * 8);
        if (!candidate) continue;
        Call g("/Script/FortniteGame.FortItem.GetItemGuid");
        if (g.ok() && g.run(candidate)) {
            Guid candGuid = g.result<Guid>();
            if (!memcmp(&candGuid, &targetGuid, sizeof(Guid))) { canonical = candidate; break; }
        }
    }
    if (canonical) item = canonical;

    if (slot >= 0) {
        (bar == 0 ? session.slots : session.secondary_slots)[size_t(slot)] = item;
        BindSlot(item, slot, bar);
    }
    LOG("[Inv+] %s x%d slot=%d", ObjName(def).c_str(), count, slot);
    return true;
}

bool Athena::BindSlot(void* item, int slot, uint8_t bar) {
    Call d("/Script/FortniteGame.FortItem.GetItemDefinitionBP");
    if (!d.ok() || !d.run(item)) return false;
    void* definition = d.result<void*>();
    if (!definition) return false;
    Call g("/Script/FortniteGame.FortItem.GetItemGuid");
    if (!g.ok() || !g.run(item)) return false;
    Guid guid = g.result<Guid>();
    if (!(guid.A | guid.B | guid.C | guid.D)) return false;

    Call bind("/Script/FortniteGame.FortKismetLibrary.EquipItemToQuickBarSlot");
    if (bind.ok() && bind.set("WorldContextObject", session.world) &&
        bind.set("ItemDefinition", definition) && bind.set("QuickBarType", bar) &&
        bind.set("SlotIndex", slot))
        bind.run(FindObj("/Script/FortniteGame.Default__FortKismetLibrary"));
    return true;
}

void Athena::EquipSlot(int slot) {
    if (slot < 0 || slot >= 6 || !session.slots[slot]) return;
    void* item = session.slots[slot];
    Call d("/Script/FortniteGame.FortItem.GetItemDefinitionBP");
    if (!d.ok() || !d.run(item)) return;
    void* def = d.result<void*>();
    if (!def) return;
    Call g("/Script/FortniteGame.FortItem.GetItemGuid");
    if (!g.ok() || !g.run(item)) return;
    Guid guid = g.result<Guid>();
    Call eq("/Script/FortniteGame.FortPawn.EquipWeaponDefinition");
    if (!eq.ok()) return;
    bool bd = false; Guid tg{};
    eq.set("WeaponData", def); eq.set("ItemEntryGuid", guid);
    eq.set("TrackerGuid", tg); eq.set("bDisableEquipAnimation", bd);
    eq.run(session.pawn);
}

void Athena::InitializeInventory() {
    if (!session.controller) return;
    session.inventory = GetObjField(session.controller, "/Script/FortniteGame.FortPlayerController.WorldInventory");
    if (!session.inventory) {
        void* invProp = GetProp("/Script/FortniteGame.FortPlayerController.WorldInventory");
        if (invProp) {
            void* klass = FindObj("/Script/FortniteGame.FortInventory");
            Transform t{};
            session.inventory = SpawnActor(session.world, klass, session.controller, t);
            if (session.inventory) Write<void*>(GetField(session.controller, invProp), 0, session.inventory);
        }
    }
    if (!session.inventory) { LOG("[Inv] no inventory"); return; }

    session.quickbars = GetObjField(session.controller, "/Script/FortniteGame.FortPlayerController.ClientQuickBars");
    if (!session.quickbars) {
        void* klass = FindObj("/Script/FortniteGame.FortQuickBarsAthena");
        if (!klass) klass = FindObj("/Script/FortniteGame.FortQuickBars");
        if (klass) {
            void* quickbar = Fn<void*(*)(void*, void*)>(profile::SpawnObject)(klass, session.controller);
            if (quickbar) {
                void* qbProp = GetProp("/Script/FortniteGame.FortPlayerController.ClientQuickBars");
                if (qbProp) Write<void*>(GetField(session.controller, qbProp), 0, quickbar);
                session.quickbars = quickbar;
            }
        }
    }

    struct DefSlot { const char* path; int count; int slot; uint8_t bar; };
    DefSlot defaults[] = {
        {"/Game/Athena/Items/Weapons/WID_Harvest_Pickaxe_Athena_C_T01.WID_Harvest_Pickaxe_Athena_C_T01", 1, 0, 0},
        {"/Game/Athena/Items/Weapons/WID_Assault_Auto_Athena_C_Ore_T03.WID_Assault_Auto_Athena_C_Ore_T03", 1, 1, 0},
        {"/Game/Items/Weapons/BuildingTools/BuildingItemData_Wall.BuildingItemData_Wall", 1, 2, 0},
        {"/Game/Items/Weapons/BuildingTools/BuildingItemData_Floor.BuildingItemData_Floor", 1, 3, 0},
        {"/Game/Items/Weapons/BuildingTools/BuildingItemData_Stair_W.BuildingItemData_Stair_W", 1, 4, 0},
        {"/Game/Items/Weapons/BuildingTools/BuildingItemData_RoofS.BuildingItemData_RoofS", 1, 5, 0},
    };
    for (auto& d : defaults) {
        void* def = LoadAsset(d.path);
        if (def) Give(def, d.count, d.slot, d.bar);
    }
    void* editTool = LoadAsset("/Game/Items/Weapons/BuildingTools/EditTool.EditTool");
    if (editTool) { session.edit_tool = editTool; Give(editTool, 1, 0, 1); }

    const char* ammo[] = {
        "/Game/Athena/Items/Ammo/AthenaAmmoDataBulletsLight.AthenaAmmoDataBulletsLight",
        "/Game/Athena/Items/Ammo/AthenaAmmoDataBulletsMedium.AthenaAmmoDataBulletsMedium",
        "/Game/Athena/Items/Ammo/AthenaAmmoDataBulletsHeavy.AthenaAmmoDataBulletsHeavy",
        "/Game/Athena/Items/Ammo/AthenaAmmoDataShells.AthenaAmmoDataShells", nullptr};
    for (int i = 0; ammo[i]; ++i) { void* def = LoadAsset(ammo[i]); if (def) Give(def, 999, -1); }

    const char* res[] = {
        "/Game/Items/ResourcePickups/WoodItemData.WoodItemData",
        "/Game/Items/ResourcePickups/StoneItemData.StoneItemData",
        "/Game/Items/ResourcePickups/MetalItemData.MetalItemData", nullptr};
    for (int i = 0; res[i]; ++i) { void* def = LoadAsset(res[i]); if (def) Give(def, 500, -1); }

    session.inventory_ready = true;
    InventoryUpdate();
    EquipSlot(0);
}

// ============================================================================
// QUICKBARS — ★ virtual initializer (from game.cpp)
// ============================================================================
bool Athena::InitializeQuickBars() {
    void* quickbar = GetObjField(session.controller, "/Script/FortniteGame.FortPlayerController.ClientQuickBars");
    if (!IsA(quickbar, FindObj("/Script/FortniteGame.FortQuickBarsAthena"))) {
        LOG("[QB] not FortQuickBarsAthena: %s", ObjName(quickbar).c_str());
        return false;
    }
    void* primary = static_cast<char*>(quickbar) + 0x230;
    void* secondary = static_cast<char*>(quickbar) + 0x2C0;
    Array p = Read<Array>(primary, 0x10), s = Read<Array>(secondary, 0x10);
    LOG("[QB] slots=%d/%d", p.Count, s.Count);
    if (p.Count == 0 && s.Count == 0) {
        void* vtable = Read<void*>(quickbar);
        uintptr_t initSlot = Read<uintptr_t>(vtable, 0x668);
        if (initSlot != g_base + profile::InitializeAthenaQuickBars)
            initSlot = Read<uintptr_t>(vtable, 0x678);
        if (initSlot == g_base + profile::InitializeAthenaQuickBars) {
            reinterpret_cast<void(*)(void*)>(initSlot)(quickbar);
            p = Read<Array>(primary, 0x10);
            s = Read<Array>(secondary, 0x10);
            LOG("[QB] after init slots=%d/%d", p.Count, s.Count);
        }
    }
    if (p.Count < 1 || s.Count < 1) return false;
    session.quickbars_initialized = true;
    return true;
}

void Athena::RefreshSlots() {
    if (!InitializeQuickBars()) { LOG("[QB] init incomplete"); return; }
    InventoryUpdate();
    for (uint8_t bar : {uint8_t(0), uint8_t(1)}) {
        auto& items = bar == 0 ? session.slots : session.secondary_slots;
        for (int slot = 0; slot < (int)items.size(); ++slot)
            if (items[size_t(slot)]) BindSlot(items[size_t(slot)], slot, bar);
    }
    InventoryUpdate();
    Call activate("/Script/FortniteGame.FortPlayerController.ClientActivateSlot");
    uint8_t primary = 0; int slot = 0; float delay = 0; bool force = true;
    if (activate.set("InQuickBar", primary) && activate.set("Slot", slot) &&
        activate.set("ActivateDelay", delay) &&
        activate.set("bUpdatePreviousFocusedSlot", force) &&
        activate.set("bForceExecution", force))
        activate.run(session.controller);
}

// ============================================================================
// LOOT / PICKUP — ★ stacking + container search (from game.cpp)
// ============================================================================
static bool ActorLocation(void* actor, Vector& value) {
    Call c("/Script/Engine.Actor.K2_GetActorLocation");
    if (!c.run(actor)) return false;
    value = c.result<Vector>();
    return std::isfinite(value.X) && std::isfinite(value.Y) && std::isfinite(value.Z);
}
static bool NearPlayer(Vector point, float limit) {
    Vector origin{};
    if (!ActorLocation(session.pawn, origin)) return false;
    float x = point.X - origin.X, y = point.Y - origin.Y, z = point.Z - origin.Z;
    return std::isfinite(x + y + z) && x*x + y*y + z*z <= limit*limit;
}

bool Athena::LocalPickup(void* pickup) {
    if (!IsA(pickup, FindObj("/Script/FortniteGame.FortPickup"))) return false;
    Vector location{};
    if (!ActorLocation(pickup, location) || !NearPlayer(location, 600)) return false;
    void* entry = GetField(pickup, GetProp("/Script/FortniteGame.FortPickup.PrimaryPickupItemEntry"));
    void* definition = nullptr; int count = 0;
    if (!GetFieldValue(entry, "/Script/FortniteGame.FortItemEntry.ItemDefinition", definition) ||
        !GetFieldValue(entry, "/Script/FortniteGame.FortItemEntry.Count", count)) return false;
    if (!definition || count < 1) return true;
    if (!Give(definition, count, -1)) {
        LOG("[Pickup] give failed: %s", ObjName(definition).c_str());
        return true;
    }
    Invoke0(pickup, "/Script/Engine.Actor.K2_DestroyActor");
    InventoryUpdate();
    LOG("[Pickup] collected %s x%d", ObjName(definition).c_str(), count);
    return true;
}

bool Athena::SearchContainer(void* actor, Vector location) {
    void* searchedProp = GetProp("/Script/FortniteGame.BuildingContainer.bAlreadySearched");
    if (!searchedProp) return false;
    uint8_t byte = Read<uint8_t>(searchedProp, profile::BoolByteOffset);
    uint8_t mask = Read<uint8_t>(searchedProp, profile::BoolFieldMask);
    if (byte >= 8 || !mask) return false;
    if (Read<uint8_t>(GetField(actor, searchedProp), byte) & mask) return true;

    // Use native BP_SpawnLoot
    Call sp("/Script/FortniteGame.BuildingContainer.BP_SpawnLoot");
    if (sp.ok() && sp.set("PlayerPawn", session.pawn)) sp.run(actor);

    SetBool(actor, "/Script/FortniteGame.BuildingContainer.bAlreadySearched", true);
    Invoke0(actor, "/Script/FortniteGame.BuildingContainer.OnRep_bAlreadySearched");
    Invoke0(actor, "/Script/FortniteGame.BuildingContainer.BounceContainer");
    LOG("[Loot] container searched: %s", ObjName(actor).c_str());
    return true;
}

bool Athena::LocalInteract(void* function, void* params) {
    void* arg = Incoming(function, params, "ReceivingActor", 8);
    void* actor = arg ? Read<void*>(arg) : nullptr;
    Vector location{};
    if (!IsA(actor, FindObj("/Script/Engine.Actor")) || !ActorLocation(actor, location) || !NearPlayer(location, 600))
        return false;
    if (IsA(actor, FindObj("/Script/FortniteGame.BuildingContainer")))
        return SearchContainer(actor, location);
    if (IsA(actor, FindObj("/Script/FortniteGame.BuildingWall"))) {
        HandleDoor(actor);
        return true;
    }
    if (IsA(actor, FindObj("/Script/FortniteGame.BuildingActor"))) {
        Call interact("/Script/FortniteGame.BuildingActor.BlueprintOnInteract", actor);
        return interact.set("InteractingPawn", session.pawn) && interact.run(actor);
    }
    return false;
}

void Athena::HandleDoor(void* door) {
    if (!door || IsBadPtr(door)) return;
    uint8_t* byte = (uint8_t*)door + profile::Wall_bDoorOpen_Byte;
    bool wasOpen = ((*byte) >> profile::Wall_bDoorOpen_Bit) & 1;
    if (wasOpen) *byte &= ~(1 << profile::Wall_bDoorOpen_Bit);
    else *byte |= (1 << profile::Wall_bDoorOpen_Bit);
    Invoke0(door, "/Script/FortniteGame.BuildingWall.OnRep_bDoorOpen");
    Invoke0(door, "/Script/Engine.Actor.ForceNetUpdate");
    LOG("[Door] toggled: open=%d", !wasOpen);
}

// ============================================================================
// BUILD — ★ from CurrentBuildableClass + preview (from game.cpp)
// ============================================================================
bool Athena::LocalBuild(void* function, void* params) {
    Vector location{}, rotation{};
    bool mirrored = false;
    void* klass = GetObjField(session.controller, "/Script/FortniteGame.FortPlayerController.CurrentBuildableClass");
    if (!GetFieldValue(session.controller, "/Script/FortniteGame.FortPlayerController.LastBuildPreviewGridSnapLoc", location) ||
        !GetFieldValue(session.controller, "/Script/FortniteGame.FortPlayerController.LastBuildPreviewGridSnapRot", rotation)) {
        LOG("[Build] preview properties unavailable");
        return true;
    }
    void* buildingCls = FindObj("/Script/FortniteGame.BuildingSMActor");
    if (!SubclassOf(klass, buildingCls) || !NearPlayer(location, 2000) ||
        !std::isfinite(rotation.X) || !std::isfinite(rotation.Y) || !std::isfinite(rotation.Z)) {
        LOG("[Build] invalid class or transform");
        return true;
    }
    if (void* data = Incoming(function, params, "CreateBuildingData", 56)) {
        void* mp = GetProp("/Script/FortniteGame.CreateBuildingActorData.bMirrored");
        if (mp) {
            int moff = Read<int>(mp, profile::PropOffset);
            if (moff >= 0 && moff < 56) GetFieldValue(data, "/Script/FortniteGame.CreateBuildingActorData.bMirrored", mirrored);
        }
    }
    std::array<float, 6> pose{location.X, location.Y, location.Z, rotation.X, rotation.Y, rotation.Z};
    double now = Monotonic();
    if (klass == session.last_build_class && pose == session.last_build_transform && now - session.last_build_time < 0.25)
        return true;

    Transform t{};
    t.Location[0] = location.X; t.Location[1] = location.Y; t.Location[2] = location.Z;

    // ★ FRotator (degrees) -> FQuat (from game.cpp)
    const float h = 0.00872664626f;
    float sp = std::sin(rotation.X * h), cp = std::cos(rotation.X * h);
    float sy = std::sin(rotation.Y * h), cy = std::cos(rotation.Y * h);
    float sr = std::sin(rotation.Z * h), cr = std::cos(rotation.Z * h);
    t.Rotation[0] = cr*sp*sy - sr*cp*cy;
    t.Rotation[1] = -cr*sp*cy - sr*cp*sy;
    t.Rotation[2] = cr*cp*sy - sr*sp*cy;
    t.Rotation[3] = cr*cp*cy + sr*sp*sy;

    void* actor = SpawnActor(session.world, klass, session.controller, t);
    if (!actor) { LOG("[Build] spawn failed"); return true; }
    session.last_build_class = klass;
    session.last_build_transform = pose;
    session.last_build_time = now;

    SetBool(actor, "/Script/FortniteGame.BuildingActor.bPlayerPlaced", true);
    Call mirror("/Script/FortniteGame.BuildingSMActor.SetMirrored");
    if (mirror.ok() && mirror.set("bIsMirrored", mirrored)) mirror.run(actor);

    Call init("/Script/FortniteGame.BuildingActor.InitializeKismetSpawnedBuildingActor");
    bool animations = true;
    if (init.ok() && init.set("BuildingOwner", actor) &&
        init.set("SpawningController", session.controller) &&
        init.set("bUsePlayerBuildAnimations", animations) && init.run(actor))
        LOG("[Build] ✅ %s", ObjName(klass).c_str());
    else
        Invoke0(actor, "/Script/Engine.Actor.K2_DestroyActor");
    return true;
}

void Athena::SpawnBuildPreviews() {}

// ============================================================================
// COSMETICS
// ============================================================================
void Athena::LoadProfileCosmetics() {
    if (!session.pawn || !session.controller || session.cosmetics_applied) return;
    void* source = nullptr;
    for (const char* p : {
        "/Script/FortniteGame.FortPlayerController.CosmeticLoadoutPC",
        "/Script/FortniteGame.FortPlayerControllerAthena.CustomizationLoadout",
        "/Script/FortniteGame.FortPlayerController.CustomizationLoadout"}) { source = GetProp(p); if (source) break; }
    void* loadout = source ? GetField(session.controller, source) : nullptr;
    void* state = GetObjField(session.controller, "/Script/Engine.Controller.PlayerState");
    if (!loadout || !state) { session.cosmetics_applied = true; return; }

    void* character = GetObjField(loadout, "/Script/FortniteGame.FortAthenaLoadout.Character");
    void* hero = GetObjField(character, "/Script/FortniteGame.AthenaCharacterItemDefinition.HeroDefinition");
    void* info = GetObjField(session.controller, "/Script/FortniteGame.FortPlayerController.MyPlayerInfo");
    if (!hero && info) hero = GetObjField(info, "/Script/FortniteGame.FortRegisteredPlayerInfo.AthenaMenuHeroDef");
    void* menu = info ? GetObjField(info, "/Script/FortniteGame.FortRegisteredPlayerInfo.TempAthenaMenuHeroInstance") : nullptr;
    if (!hero && menu) {
        Call c("/Script/FortniteGame.FortHero.GetHeroTypeBP");
        if (c.ok() && c.run(menu)) hero = c.result<void*>();
    }
    if (hero) {
        void* htProp = GetProp("/Script/FortniteGame.FortPlayerState.HeroType");
        if (htProp) Write<void*>(GetField(state, htProp), 0, hero);
    }

    std::vector<void*> parts;
    Array menuParts{};
    if (menu && GetFieldValue(menu, "/Script/FortniteGame.FortHero.CharacterParts", menuParts) &&
        menuParts.Data && menuParts.Count > 0 && menuParts.Count <= 16)
        for (int i = 0; i < menuParts.Count; ++i) parts.push_back(Read<void*>(menuParts.Data, i * 8));

    void* backpack = GetObjField(loadout, "/Script/FortniteGame.FortAthenaLoadout.Backpack");
    Array backParts{};
    if (backpack && GetFieldValue(backpack, "/Script/FortniteGame.AthenaCharacterPartItemDefinition.CharacterParts", backParts) &&
        backParts.Data && backParts.Count >= 0 && backParts.Count <= 16)
        for (int i = 0; i < backParts.Count; ++i) parts.push_back(Read<void*>(backParts.Data, i * 8));

    for (void* part : parts) {
        if (!IsA(part, FindObj("/Script/FortniteGame.CustomCharacterPart"))) continue;
        uint8_t type = 255;
        if (!GetFieldValue(part, "/Script/FortniteGame.CustomCharacterPart.CharacterPartType", type) || type > 6) continue;
        Call ch("/Script/FortniteGame.FortPlayerPawn.ServerChoosePart");
        if (ch.ok() && ch.set("Part", type) && ch.set("ChosenCharacterPart", part)) {
            ch.run(session.pawn);
            session.character_parts.push_back(part);
        }
    }
    if (!parts.empty()) {
        Array arr{parts.data(), (int32)parts.size(), (int32)parts.size()};
        Call ap("/Script/FortniteGame.FortKismetLibrary.ApplyCharacterCosmetics");
        void* klib = FindObj("/Script/FortniteGame.Default__FortKismetLibrary");
        if (ap.ok() && klib && ap.set("WorldContextObject", session.world) &&
            ap.set("CharacterParts", arr) && ap.set("PlayerState", state))
            ap.run(klib);
    }
    Invoke0(state, "/Script/FortniteGame.FortPlayerState.OnRep_HeroType");
    Invoke0(state, "/Script/FortniteGame.FortPlayerState.OnRep_CharacterData");
    Call refresh("/Script/FortniteGame.FortKismetLibrary.UpdatePlayerCustomCharacterPartsVisualization");
    if (refresh.ok() && refresh.set("PlayerState", state))
        refresh.run(FindObj("/Script/FortniteGame.Default__FortKismetLibrary"));
    session.cosmetics_applied = true;
    LOG("[Cosmetics] applied parts=%zu", parts.size());
}

void Athena::ShowSkin() { LoadProfileCosmetics(); }

void Athena::ForcePlayerVisible() {
    if (!session.pawn || IsBadPtr(session.pawn)) return;
    SetBool(session.pawn, "/Script/FortniteGame.FortPlayerPawn.bIsSkydiving", false);
    SetBool(session.pawn, "/Script/FortniteGame.FortPlayerPawn.bIsParachuteOpen", false);
    Call body("/Script/FortniteGame.FortPlayerPawnAthena.SetCharacterBodyVisibilityForPossession");
    if (body.ok()) { bool bShow = true; if (body.set("bVisible", bShow)) body.run(session.pawn); }
    Call hid("/Script/Engine.Actor.SetActorHiddenInGame");
    if (hid.ok()) { bool bHidden = false; if (hid.set("bNewHidden", bHidden)) hid.run(session.pawn); }
    SetBool(session.pawn, "/Script/Engine.Actor.bHidden", false);
    session.visibility_forced = true;
}

// ============================================================================
// HUD — ★ native readiness flow (from game.cpp finish_loading)
// ============================================================================
void Athena::FinalizeHUD() {
    if (!session.controller || !session.pawn || !session.state) return;
    LOG("=== FinalizeHUD ===");

    // Set GamePhase to Warmup (2) — native flow
    void* gp = GetProp("/Script/FortniteGame.FortGameStateAthena.GamePhase");
    if (gp && Read<int>(gp, profile::PropElementSize) == 1) {
        uint8_t old = Read<uint8_t>(GetField(session.state, gp));
        if (old < 2) {
            Write<uint8_t>(GetField(session.state, gp), 0, uint8_t(2));
            Call n("/Script/FortniteGame.FortGameStateAthena.OnRep_GamePhase");
            if (n.ok() && n.set("OldGamePhase", old)) n.run(session.state);
        }
    }

    SetBool(session.controller, "/Script/FortniteGame.FortPlayerController.bHasClientFinishedLoading", true);
    SetBool(session.controller, "/Script/FortniteGame.FortPlayerController.bHasServerFinishedLoading", true);

    Invoke0(session.controller, "/Script/FortniteGame.FortPlayerController.ServerReadyToStartMatch");
    Invoke0(session.controller, "/Script/FortniteGame.FortPlayerControllerZone.ClientOnPawnSpawned");
    session.ready_called = true;

    Call cr("/Script/Engine.PlayerController.ClientRestart");
    if (cr.ok() && cr.set("NewPawn", session.pawn)) cr.run(session.controller);

    Call cam("/Script/Engine.PlayerController.SetViewTargetWithBlend");
    if (cam.ok()) {
        cam.set("NewViewTarget", session.pawn);
        float blend = 0.0f;
        cam.set("BlendTime", blend);
        cam.run(session.controller);
    }

    Call hud("/Script/Engine.PlayerController.GetHUD");
    if (hud.ok() && hud.run(session.controller)) {
        void* value = hud.result<void*>();
        LOG("[HUD] %p class=%s", value, ObjName(value).c_str());
    }

    InventoryUpdate();
    session.hud_ready = true;
    LOG("FinalizeHUD: complete");
}

void Athena::RefreshHUD() {
    if (!session.controller) return;
    Invoke0(session.controller, "/Script/FortniteGame.FortPlayerControllerZone.ClientOnPawnSpawned");
    InventoryUpdate();
}

// ============================================================================
// CHEATS / CLEANUP
// ============================================================================
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
        Invoke0(cheat, "/Script/Engine.CheatManager.ReceiveInitCheatManager");
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
    void* gp = GetProp("/Script/FortniteGame.FortGameStateAthena.GamePhase");
    if (gp && Read<int>(gp, profile::PropElementSize) == 1) {
        uint8_t old = Read<uint8_t>(GetField(session.state, gp));
        if (old < 2) {
            Write<uint8_t>(GetField(session.state, gp), 0, uint8_t(2));
            Call n("/Script/FortniteGame.FortGameStateAthena.OnRep_GamePhase");
            if (n.ok() && n.set("OldGamePhase", old)) n.run(session.state);
        }
    }
    SetBool(session.controller, "/Script/FortniteGame.FortPlayerController.bHasServerFinishedLoading", true);
    SetBool(session.controller, "/Script/FortniteGame.FortPlayerController.bHasClientFinishedLoading", true);
    if (!session.ready_called) {
        Invoke0(session.controller, "/Script/FortniteGame.FortPlayerController.ServerReadyToStartMatch");
        Invoke0(session.controller, "/Script/FortniteGame.FortPlayerControllerZone.ClientOnPawnSpawned");
        session.ready_called = true;
    }
}

void Athena::RemoveNetDebugUI() {
    void* wb = FindObj("/Script/UMG.Default__WidgetBlueprintLibrary");
    if (!wb) return;
    void* cls = FindObj("/Game/Athena/HUD/NetDebugUI.NetDebugUI_C");
    if (!cls) return;
    Call c("/Script/UMG.WidgetBlueprintLibrary.GetAllWidgetsOfClass");
    if (!c.ok() || !c.set("WorldContextObject", session.world) || !c.set("WidgetClass", cls) || !c.run(wb)) return;
    Array arr = c.result<Array>();
    for (int i = 0; i < arr.Count; ++i) {
        void* w = Read<void*>(arr.Data, (size_t)i * 8);
        if (w) { Call r("/Script/UMG.UserWidget.RemoveFromViewport"); if (r.ok()) r.run(w); }
    }
}

void Athena::DropInventoryItem(Guid guid, int count) {
    if (!session.controller) return;
    Call c("/Script/FortniteGame.FortPlayerController.ServerAttemptInventoryDrop");
    if (!c.ok()) return;
    int32 cnt = count; bool trash = false;
    c.set("ItemGuid", guid); c.set("Count", cnt); c.set("bTrash", trash);
    c.run(session.controller);
}

void Athena::OnServerExecuteInventoryItem(Guid guid) {
    for (int s = 0; s < 6; ++s) {
        if (!session.slots[s]) continue;
        Call g("/Script/FortniteGame.FortItem.GetItemGuid");
        if (!g.ok() || !g.run(session.slots[s])) continue;
        Guid cand = g.result<Guid>();
        if (!memcmp(&cand, &guid, sizeof(Guid))) { EquipSlot(s); return; }
    }
}

// ============================================================================
// AIRCRAFT
// ============================================================================
void Athena::SpawnBattleBus() {
    if (!session.mode || !session.state || session.aircraft_phase_started) return;
    LOG("[Bus] Starting aircraft phase");

    Call startAircraft("/Script/FortniteGame.FortGameModeAthena.StartAircraftPhase");
    if (startAircraft.ok() && startAircraft.run(session.mode)) {
        LOG("[Bus] ✅ StartAircraftPhase called");
    }

    void* gp = GetProp("/Script/FortniteGame.FortGameStateAthena.GamePhase");
    if (gp) {
        uint8_t old = Read<uint8_t>(GetField(session.state, gp));
        if (old < 3) {
            Write<uint8_t>(GetField(session.state, gp), 0, uint8_t(3));
            Call n("/Script/FortniteGame.FortGameStateAthena.OnRep_GamePhase");
            if (n.ok()) { n.set("OldGamePhase", old); n.run(session.state); }
        }
    }

    if (session.pawn) {
        Vector loc{0.0f, 0.0f, 25000.0f};
        Rotator rot{0.0f, 0.0f, 0.0f};
        Call sl("/Script/Engine.Actor.K2_SetActorLocationAndRotation");
        if (sl.ok()) {
            bool bSweep = false, bTeleport = true;
            sl.set("NewLocation", loc); sl.set("NewRotation", rot);
            sl.set("bSweep", bSweep); sl.set("bTeleport", bTeleport);
            sl.run(session.pawn);
        }
    }
    session.aircraft_phase_started = true;
}

void Athena::HandleJumpFromHudStart() {
    if (!session.controller || !session.pawn || session.aircraft_jumped) return;
    session.aircraft_jumped = true;
    LOG("[Bus] Jump");

    Vector rotation{};
    GetFieldValue(session.controller, "/Script/Engine.Controller.ControlRotation", rotation);

    Call aircraft("/Script/FortniteGame.FortPlayerController.ServerAttemptAircraftJump");
    if (aircraft.ok()) {
        Rotator r{rotation.X, rotation.Y, rotation.Z};
        if (aircraft.set("ClientRotation", r)) aircraft.run(session.controller);
    }

    Call sky("/Script/FortniteGame.FortPlayerPawn.BeginSkydiving");
    if (sky.ok()) { bool bFromBus = true; if (sky.set("bFromBus", bFromBus)) sky.run(session.pawn); }
    Write<uint8_t>(session.pawn, profile::Pawn_bIsSkydiving, 1);

    Call cam("/Script/Engine.PlayerController.SetViewTargetWithBlend");
    if (cam.ok()) {
        cam.set("NewViewTarget", session.pawn);
        float blend = 0.0f; cam.set("BlendTime", blend);
        cam.run(session.controller);
    }
    Invoke0(session.pawn, "/Script/FortniteGame.FortPlayerPawn.OnRep_IsSkydiving");
}

// ============================================================================
// SAFE ZONE — ★ native init (from SDK)
// ============================================================================
void Athena::SpawnSafeZoneIndicator() {
    if (session.safezone_spawned || !session.state || !session.world) return;
    void* existing = GetObjField(session.state, "/Script/FortniteGame.FortGameStateAthena.SafeZoneIndicator");
    if (existing && !IsBadPtr(existing)) {
        Invoke0(existing, "/Script/FortniteGame.FortSafeZoneIndicator.InitializeWithMapDefaultPhases");
        Call start("/Script/FortniteGame.FortSafeZoneIndicator.StartPhase");
        if (start.ok()) { int idx = 0; start.set("InPhaseIndex", idx); start.run(existing); }
        session.safezone_spawned = true;
        LOG("[Storm] existing indicator initialized");
        return;
    }
    void* cls = FindObj("/Script/FortniteGame.FortSafeZoneIndicator");
    if (!cls) return;
    Transform t{};
    void* indicator = SpawnActor(session.world, cls, session.state, t);
    if (!indicator) { LOG("[Storm] spawn failed"); return; }
    void* prop = GetProp("/Script/FortniteGame.FortGameStateAthena.SafeZoneIndicator");
    if (prop) Write<void*>(GetField(session.state, prop), 0, indicator);
    Invoke0(session.state, "/Script/FortniteGame.FortGameStateAthena.OnRep_SafeZoneIndicator");
    Invoke0(indicator, "/Script/FortniteGame.FortSafeZoneIndicator.InitializeWithMapDefaultPhases");
    Call start("/Script/FortniteGame.FortSafeZoneIndicator.StartPhase");
    if (start.ok()) { int idx = 0; start.set("InPhaseIndex", idx); start.run(indicator); }
    session.safezone_spawned = true;
    LOG("[Storm] indicator=%p", indicator);
}

// ============================================================================
// PLAYER INIT
// ============================================================================
bool Athena::TryInitializePlayer(void* world, void* mode, void* controller) {
    void* pawnCls = LoadClass("/Game/Athena/PlayerPawn_Athena.PlayerPawn_Athena_C", "/Script/Engine.Pawn");
    if (!pawnCls) { LOG("[Init] PlayerPawn_Athena_C missing"); return false; }

    void* existing = GetPawnFn(controller);
    if (existing && !IsBadPtr(existing) && IsA(existing, pawnCls)) {
        session.pawn = existing;
        LOG("[Init] Using existing pawn");
        return true;
    }

    Transform t{};
    t.Location[0] = 0.0f; t.Location[1] = 0.0f; t.Location[2] = 500.0f;
    Call choose("/Script/Engine.GameModeBase.ChoosePlayerStart");
    if (choose.ok() && choose.set("Player", controller) && choose.run(mode)) {
        void* start = choose.result<void*>();
        if (start && !IsBadPtr(start)) {
            Vector v{};
            if (ActorLocation(start, v)) {
                t.Location[0] = v.X; t.Location[1] = v.Y; t.Location[2] = v.Z + 100.0f;
            }
        }
    }

    session.pawn = SpawnActor(world, pawnCls, controller, t);
    if (!session.pawn) { LOG("[Init] Spawn failed"); return false; }
    LOG("[Init] Spawned: %p", session.pawn);

    SetBool(session.pawn, "/Script/FortniteGame.FortPlayerPawn.bIsSkydiving", false);
    SetBool(session.pawn, "/Script/FortniteGame.FortPlayerPawn.bIsParachuteOpen", false);

    Fn<void(*)(void*, void*)>(profile::Possess)(controller, session.pawn);
    if (GetPawnFn(controller) != session.pawn) { LOG("[Init] Possession failed"); return false; }
    LOG("[Init] Possessed OK");

    Call restart("/Script/Engine.PlayerController.ClientRestart");
    if (restart.ok() && restart.set("NewPawn", session.pawn)) restart.run(controller);

    SetupCheats();
    ForcePlayerVisible();
    return true;
}

// ============================================================================
// SETUP WORLD
// ============================================================================
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
    if (!session.mode || !session.state) { LOG("SetupWorld: missing GM/GS"); return; }
    LOG("SetupWorld: PC=%s GM=%s GS=%s",
        ObjName(session.controller).c_str(), ObjName(session.mode).c_str(), ObjName(session.state).c_str());

    if (!IsStandaloneWorld(world)) { LOG("SetupWorld: not standalone"); session.attempted = true; return; }

    if (!TryInitializePlayer(world, session.mode, session.controller)) return;

    GrantAllAbilitySets();
    EnableMovementFeatures();
    ShowSkin();
    DestroyLods();
    InitializeInventory();
    ApplySprintSpeed();
    SpawnSafeZoneIndicator();
    DropLoadingScreen();
    RemoveNetDebugUI();
    RefreshHUD();

    session.finalize_at = Monotonic() + 2.0;
    session.attempted = true;
    LOG("SetupWorld: initial done");
}

void Athena::FinalizePlayer() {
    if (session.finalized) return;
    session.finalized = true;
    LOG("=== FinalizePlayer ===");
    void* pc = GetPC(session.world, 0);
    if (pc != session.controller) { LOG("Finalize: ctrl changed"); return; }

    // ★ Accept any valid pawn class (from game.cpp)
    void* pawn = GetPawnFn(pc);
    if (!pawn || IsBadPtr(pawn)) { LOG("Finalize: no pawn"); return; }
    void* nativeCls = FindObj("/Script/FortniteGame.FortPlayerPawnAthena");
    void* bpCls = FindObj("/Game/Athena/PlayerPawn_Athena.PlayerPawn_Athena_C");
    void* baseCls = FindObj("/Script/FortniteGame.FortPlayerPawn");
    bool valid = (nativeCls && IsA(pawn, nativeCls)) || (bpCls && IsA(pawn, bpCls)) || (baseCls && IsA(pawn, baseCls));
    if (!valid) {
        void* anyPawn = FindObj("/Script/Engine.Pawn");
        if (anyPawn && IsA(pawn, anyPawn)) {
            LOG("Finalize: generic Pawn, proceeding");
        } else {
            LOG("Finalize: unrecognized pawn class %s", ObjName(pawn).c_str());
        }
    }
    session.pawn = pawn;
    LOG("Finalize: pawn=%p class=%s", pawn, ObjName(pawn).c_str());

    if (!session.abilities_granted) GrantAllAbilitySets();
    if (!session.movement_enabled) EnableMovementFeatures();

    FinalizeHUD();
    RefreshSlots();
    EquipSlot(session.slots[0] ? 0 : 1);

    ForcePlayerVisible();
    ApplySprintSpeed();

    // Free building
    SetBool(pc, "/Script/FortniteGame.FortPlayerController.bBuildFree", true);

    session.bus_at = Monotonic() + 30.0;
    LOG("Finalize: complete");
}

// ============================================================================
// TICK
// ============================================================================
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
    if (!session.finalized) return;

    if (!session.aircraft_phase_started && session.bus_at > 0 && now >= session.bus_at)
        SpawnBattleBus();

    static double lastVis = 0, lastSpd = 0;
    if (now - lastVis > 15.0) { lastVis = now; if (!session.visibility_forced) ForcePlayerVisible(); }
    if (session.pawn && now - lastSpd > 5.0) { lastSpd = now; ApplySprintSpeed(); }
}

// ============================================================================
// HOOKS
// ============================================================================
static void* g_interactionClass = nullptr;

static void pe_hook(void* obj, void* func, void* params) {
    if (!orig_PE) return;
    if (!g_internal && func && g_gameTid.load() == (int)gettid() && !IsBadPtr(obj) && !IsBadPtr(func)) {
        if (obj == session.controller && params == nullptr) {
            std::string fn = ObjName(func);
            if (fn == "JumpFromHudStart") {
                Athena::HandleJumpFromHudStart();
                return orig_PE(obj, func, params);
            }
        }
        if (!g_interactionClass) g_interactionClass = FindObj("/Script/FortniteGame.FortControllerComponent_Interaction");
        bool isPC   = (obj == session.controller);
        bool isPawn = (obj == session.pawn);
        bool isInteraction = false;
        if (g_interactionClass && obj) isInteraction = IsA(obj, g_interactionClass);

        if (isPC || isPawn || isInteraction) {
            Guard g;
            std::string n = ObjName(func);

            if (n == "ServerAttemptInteract" && params) {
                if (Athena::LocalInteract(func, params)) return;
            }
            else if (n == "ServerExecuteInventoryItem" && params) {
                Athena::OnServerExecuteInventoryItem(Read<Guid>(params));
            }
            else if (n == "ServerHandlePickup" && params) {
                void* arg = Incoming(func, params, "Pickup", 8);
                if (arg && Athena::LocalPickup(Read<void*>(arg))) return;
            }
            else if (n == "ServerAttemptAircraftJump" && !session.aircraft_jumped) {
                Athena::HandleJumpFromHudStart();
            }
            else if (n.rfind("ServerCreateBuilding", 0) == 0 && params) {
                if (Athena::LocalBuild(func, params)) return;
            }
            else if (n == "ServerSpawnInventoryDrop" || n == "ServerAttemptInventoryDrop") {
                Guid gg = Read<Guid>(params);
                int cnt = Read<int>(params, sizeof(Guid));
                Athena::DropInventoryItem(gg, cnt);
                return;
            }
        }
    }
    orig_PE(obj, func, params);
}

static void tick_hook(void* world, int type, float delta) {
    if (orig_Tick) orig_Tick(world, type, delta);
    if (g_internal || !world || IsBadPtr(world)) return;
    int expected = 0;
    if (g_gameTid.compare_exchange_strong(expected, (int)gettid()))
        LOG("WorldTick first: tid=%d", (int)gettid());
    if (g_gameTid.load() != (int)gettid()) return;
    Guard g;
    if (Read<uint8_t>((void*)(g_base + profile::IsSavingPackage)) ||
        Fn<bool(*)()>(profile::IsGarbageCollecting)()) return;

    std::string wn = ObjName(world);
    static std::string lastWorld;
    if (wn != lastWorld) { LOG(">>> WorldTick: '%s'", wn.c_str()); lastWorld = wn; }
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
        session.world = world;
        session.world_index = index;
        session.observed_at = Monotonic();
        LOG(">>> Terrain: %s", wn.c_str());
    }
    double now = Monotonic();
    if (now - session.observed_at < 3.0) return;
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
        LOG("Intercepting Athena travel");
        session = Session{};
        session.attempted = false;
        return orig_Exec(engine, world, u"open Athena_Terrain", output);
    }
    return orig_Exec(engine, world, command, output);
}

// ============================================================================
// PROLOGUE VERIFICATION — ★ refuses to patch another detour (from game.cpp)
// ============================================================================
static bool Prologue(const char* label, uintptr_t address, std::initializer_list<uint32_t> words) {
    size_t i = 0;
    for (auto word : words) {
        uint32_t actual = Read<uint32_t>(reinterpret_cast<void*>(g_base + address), 4 * i);
        if (actual != word) {
            LOG("PROLOGUE MISMATCH %s RVA=%zx byte=%zu expected=%08x actual=%08x",
                label, size_t(address), 4 * i, word, actual);
            return false;
        }
        ++i;
    }
    LOG("Prologue verified %s", label);
    return true;
}

static void InstallHooks() {
    LOG("=== InstallHooks ===");
    bool pe_valid = Prologue("ProcessEvent", profile::ProcessEvent,
        {0xf81a0ffc, 0xa90167fa, 0xa9025ff8, 0xa90357f6});
    bool tick_valid = Prologue("WorldTick", profile::WorldTick,
        {0xd10503ff, 0x6d0d23e9, 0xa90e6ffc, 0xa90f67fa});
    bool exec_valid = Prologue("EngineExec", profile::EngineExec,
        {0xd10243ff, 0xf90023f9, 0xa9055ff8, 0xa90657f6});

    if (!pe_valid || !tick_valid || !exec_valid) {
        LOG("Hook prologue mismatch; refusing to patch another mod's detour");
        return;
    }

    if (DobbyHook((void*)(g_base + profile::ProcessEvent), (void*)pe_hook, (void**)&orig_PE) != 0 || !orig_PE) {
        LOG("PE FAILED"); return;
    }
    LOG("PE hooked");
    if (DobbyHook((void*)(g_base + profile::WorldTick), (void*)tick_hook, (void**)&orig_Tick) != 0 || !orig_Tick)
        LOG("Tick FAILED");
    else LOG("Tick hooked");
    if (DobbyHook((void*)(g_base + profile::EngineExec), (void*)exec_hook, (void**)&orig_Exec) != 0 || !orig_Exec)
        LOG("Exec FAILED");
    else LOG("Exec hooked");
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
    LOG("=== EraCH3 v15.0 starting ===");
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

extern "C" JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM*, void*) { return JNI_VERSION_1_6; }
