// ============================================================================
// EraCH3 v13 — Fortnite 21.30 CL-21088273 Android
// COMPLETE FIX: Erbium-style aircraft phase, safe HUD, delayed bus
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
#include <unordered_map>
#include <unordered_set>
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
constexpr uintptr_t CopyScriptStruct     = 0x07535848;
constexpr uintptr_t GetInterfaceAddress  = 0x0761AA88;

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
constexpr unsigned BoolSize        = 0x78;
constexpr unsigned BoolByteOffset  = 0x79;
constexpr unsigned BoolFieldMask   = 0x7A;
constexpr unsigned FFieldName = 0x28;
constexpr unsigned FFieldNext = 0x20;

constexpr uint64_t CPF_Parm       = 0x00000080ULL;
constexpr uint64_t CPF_OutParm    = 0x00000100ULL;
constexpr uint64_t CPF_ConstParm  = 0x00000200ULL;
constexpr uint64_t CPF_ReturnParm = 0x00000400ULL;

constexpr unsigned AActor_Owner             = 0x0138;
constexpr unsigned PC_PlayerState           = 0x0278;
constexpr unsigned PC_CheatManager          = 0x03D8;
constexpr unsigned PC_WorldInventory        = 0x1D20;
constexpr unsigned PC_ClientQuickBars       = 0x1A60;
constexpr unsigned PC_bReadyToStartMatch    = 0x0A28;
constexpr unsigned PC_MyPlayerInfo          = 0x2088;
constexpr unsigned PC_XPComponent           = 0x4330;

constexpr unsigned Pawn_CurrentWeapon       = 0x0550;
constexpr unsigned Pawn_HealthSet           = 0x0798;
constexpr unsigned Pawn_bIsABot_Byte        = 0x027A;
constexpr unsigned Pawn_bIsABot_Bit         = 3;

constexpr unsigned FPPA_bIsSkydiving        = 0x1A1D;
constexpr unsigned FPPA_bIsParachuteOpen    = 0x1A1E;
constexpr unsigned FPPA_bIsInAnyStorm       = 0x1D78;
constexpr unsigned FPPA_bIsInsideSafeZone   = 0x1D79;
constexpr unsigned FPPA_bIsSkydivingFromBus = 0x1A1F;
constexpr unsigned FPPA_Speed_Sprint        = 0x53EC;

constexpr unsigned PS_HeroType              = 0x0430;
constexpr unsigned PS_TeamIndex             = 0x1038;
constexpr unsigned PS_KillScore             = 0x104C;
constexpr unsigned PS_SeasonLevelUIDisplay  = 0x1050;
constexpr unsigned PS_SquadId               = 0x120C;
constexpr unsigned PS_Place                 = 0x1044;

constexpr unsigned Weapon_WeaponData        = 0x03D8;
constexpr unsigned Weapon_ItemEntryGuid     = 0x0B18;

constexpr unsigned Pickup_PrimaryPickupItemEntry = 0x02F8;
constexpr unsigned Pickup_bPickedUp_Byte         = 0x0590;

constexpr unsigned SZ_SafeZoneRadius            = 0x0284;
constexpr unsigned SZ_NextRadius                = 0x02E0;
constexpr unsigned SZ_SafeZoneStartShrinkTime   = 0x0288;
constexpr unsigned SZ_SafeZoneFinishShrinkTime  = 0x028C;
constexpr unsigned SZ_CurrentPhase              = 0x0290;
constexpr unsigned SZ_PhaseCount                = 0x0294;

constexpr unsigned Aircraft_FlightInfo          = 0x0318;
constexpr unsigned Aircraft_FlightStartTime     = 0x0360;
constexpr unsigned Aircraft_FlightEndTime       = 0x0364;
constexpr unsigned Aircraft_DropStartTime       = 0x0368;
constexpr unsigned Aircraft_DropEndTime         = 0x036C;
constexpr unsigned Aircraft_FlightElapsedTime   = 0x0370;
constexpr unsigned Aircraft_ReplicatedFlightTimestamp = 0x0374;

constexpr unsigned Entry_ItemDefinition      = 0x0000;
constexpr unsigned Entry_Count               = 0x0014;
constexpr unsigned Entry_Durability          = 0x0024;
constexpr unsigned Entry_LoadedAmmo          = 0x002C;
constexpr unsigned Entry_PhantomReserveAmmo  = 0x0030;
constexpr unsigned Entry_ItemGuid            = 0x0068;
constexpr unsigned Entry_TrackerGuid         = 0x0078;
constexpr unsigned Entry_StateValues         = 0x00D0;
constexpr unsigned Entry_Size                = 0x01A0;

constexpr unsigned QuickBar_Slots            = 0x0010;
constexpr unsigned QuickBarSlot_Size         = 0x28;

constexpr unsigned Wall_bDoorOpen_Byte = 0x0FDE;
constexpr unsigned Wall_bDoorOpen_Bit  = 5;

constexpr unsigned CMC_MovementMode   = 0x019C;
constexpr unsigned CMC_CustomMode     = 0x019D;
constexpr unsigned CMC_MaxWalkSpeed   = 0x01E8;
constexpr unsigned CMC_MaxFlySpeed    = 0x01F4;
constexpr unsigned CMC_MaxSwimSpeed   = 0x01F0;
constexpr unsigned CMC_JumpZVelocity  = 0x0170;
constexpr unsigned CMC_GravityScale   = 0x0168;

constexpr unsigned GS_GamePhase             = 0x14AC;
constexpr unsigned GS_GamePhaseStep         = 0x14AD;
constexpr unsigned GS_PlayersLeft           = 0x14B8;
constexpr unsigned GS_TotalPlayers          = 0x14B4;

constexpr unsigned BotCtrl_PlayerBotPawn       = 0x05E8;
constexpr unsigned BotSvc_PlayerBots           = 0x0B50;
constexpr unsigned BotInfo_BotController       = 0x0000;
constexpr unsigned BotInfo_Size                = 0x0010;

constexpr uintptr_t InitializeAthenaQuickBars  = 0x056607EC;

constexpr unsigned TacticalSprint_Flags        = 0xC0;
}

extern "C" int DobbyHook(void* target, void* replace, void** origin);

// ============================================================================
// MEMORY
// ============================================================================
static inline bool IsBadPtr(const void* p) {
    if (!p) return true;
    uintptr_t v = (uintptr_t)p;
    if (v < 0x10000) return true;
    if (v > 0x0000FFFFFFFFFFFFULL) return true;
    if ((v & 0x7) != 0) return true;
    return false;
}
template<class T> static T Read(const void* p, size_t off = 0) {
    T r{}; if (p) memcpy(&r, (const char*)p + off, sizeof(T)); return r;
}
template<class T> static void Write(void* p, size_t off, const T& v) {
    if (p) memcpy((char*)p + off, &v, sizeof(T));
}
template<typename T>
static T& Off(void* base, unsigned offset) {
    return *reinterpret_cast<T*>(reinterpret_cast<char*>(base) + offset);
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
static void* GetPropEx(const char* primary, const char* fallback);
static void* GetField(void* obj, void* prop);
static void SetBool(void* obj, const char* path, bool enabled);
static std::u16string ToWide(const std::string& s);
static std::string Narrow(const char16_t* s, int max);
static void* GetObjField(void* obj, const char* path);
static void* LoadAsset(const std::string& path, const char* type_path);
static void* LoadClass(const char* path, const char* base_path);
static void* GetPC(void* world, int index);
static void* GetPawn(void* controller);
static void* GetASC(void* actor);
static bool IsStandaloneWorld(void* world);

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
    void* klass = Read<void*>(obj, profile::ObjectClass);
    if (!klass || IsBadPtr(klass)) return {};
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
// PROPS
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
static void* GetPropEx(const char* a, const char* b) {
    if (void* p = GetProp(a)) return p;
    return b ? GetProp(b) : nullptr;
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
    void* v = Read<void*>(GetField(obj, p));
    return (v && !IsBadPtr(v)) ? v : nullptr;
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
// CALL
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
        if (!m_func || IsBadPtr(m_func)) return;
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
    template<class T> T result() const {
        for (auto& p : m_params)
            if ((p.flags & profile::CPF_ReturnParm) && p.size == (int)sizeof(T))
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

// ============================================================================
// ASSETS
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
static void* GetPawn(void* controller) {
    if (!controller || IsBadPtr(controller)) return nullptr;
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
    if (!world || !klass || IsBadPtr(world) || IsBadPtr(klass)) return nullptr;
    void* actor = Fn<void*(*)(void*, void*, const Transform*, uint8_t, void*)>(
        profile::BeginSpawn)(world, klass, &t, 1, owner);
    return actor ? Fn<void*(*)(void*, const Transform*)>(profile::FinishSpawn)(actor, &t) : nullptr;
}
static void* ResolveInterface(void* object, void* ifaceClass) {
    if (!object || !ifaceClass) return nullptr;
    using Fn_t = void*(*)(void*, void*);
    return ((Fn_t)(g_base + profile::GetInterfaceAddress))(object, ifaceClass);
}
static bool IsStandaloneWorld(void* world) {
    if (!world || IsBadPtr(world)) return false;
    return Fn<bool(*)(void*)>(profile::IsStandalone)(world);
}

static void* GetASC(void* actor) {
    if (!actor || IsBadPtr(actor)) return nullptr;
    void* asc = GetObjField(actor, "/Script/FortniteGame.FortPawn.AbilitySystemComponent");
    if (asc) return asc;
    void* pc = GetObjField(actor, "/Script/Engine.Pawn.Controller");
    if (!pc) pc = GetObjField(actor, "/Script/Engine.Actor.Owner");
    if (pc && !IsBadPtr(pc)) {
        void* ps = GetObjField(pc, "/Script/Engine.Controller.PlayerState");
        if (ps && !IsBadPtr(ps)) {
            asc = GetObjField(ps, "/Script/GameplayAbilities.AbilitySystemComponent");
            if (!asc) asc = GetObjField(ps, "/Script/FortniteGame.FortPlayerState.AbilitySystemComponent");
            if (asc) return asc;
        }
    }
    asc = GetObjField(actor, "/Script/GameplayAbilities.AbilitySystemComponent");
    return asc;
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
    void* bot_service = nullptr;
    void* aircraft = nullptr;

    bool  attempted = false;
    bool  finalized = false;
    bool  abilities_granted = false;
    bool  stamina_initialized = false;
    bool  melee_granted = false;
    bool  melee_attempted = false;
    bool  movement_enabled = false;
    bool  cosmetics_applied = false;
    bool  profile_attempted = false;
    bool  mcp_synced = false;
    bool  inventory_ready = false;
    bool  quickbars_initialized = false;
    bool  ready_called = false;
    bool  visibility_forced = false;
    bool  safezone_spawned = false;
    bool  side_systems_initialized = false;
    bool  bots_spawned = false;
    bool  storm_protected = false;
    bool  aircraft_phase_started = false;
    bool  aircraft_jumped = false;
    bool  cheats_ready = false;
    bool  vehicles_spawned = false;
    bool  vending_initialized = false;
    bool  minimap_initialized = false;
    bool  clamber_initialized = false;
    bool  solo_hud_ready = false;

    int   bots_target = 99;
    int   bots_spawned_count = 0;

    double observed_at = 0;
    double travel_at = 0;
    double finalize_at = 0;
    double bus_at = 0;
    double storm_at = 0;

    std::array<void*, 6> slots{};
    std::vector<void*> character_parts;
    std::vector<void*> granted_ability_sets;
    std::vector<void*> bot_pawns;
    std::vector<void*> bot_controllers;
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
// ATHENA
// ============================================================================
namespace Athena {
    void SetupWorld(void* world);
    void Tick();
    bool TryInitializePlayer(void* world, void* mode, void* controller);
    void SetupCheats();
    void FinalizePlayer();
    void ShowSkin();
    void LoadProfileCosmetics();
    void InitializeSideSystems();
    void SyncMCPProfile();
    void SpawnSoloBotsBatch(int maxCount);
    void UpdateSoloHUD();
    void ProtectLocalPlayerFromStorm(bool protect);
    void SetupLocalVehicles();
    void SetupLocalAI();
    void SetupLocalVending();
    void SetupLocalPlayerCounter();
    void SetupLocalMinimap();
    void SetupLocalClamber();
    void ForcePlayerVisible();
    void EquipSlot(int slot);
    void DestroyLods();
    void DropLoadingScreen();
    void InitializeInventory();
    void AddToInventory(void* def, int count, int slot);
    void InventoryUpdate();
    void GrantAbility(void* klass);
    void GrantAllAbilitySets();
    void GrantMeleeAbility();
    void InitStaminaAttributes();
    void RefreshHUD();
    void InitializeHUD();
    void ApplySprintSpeed();
    void SetupTacticalSprint();
    void SpawnSafeZoneIndicator();
    void DropInventoryItem(Guid guid, int count);
    void OnServerExecuteInventoryItem(Guid guid);
    void OnServerExecuteInventoryWeapon(void* weapon);
    void HandleJumpFromHudStart();
    void SpawnBattleBus();
    void StartAircraftPhase();
    void OnAircraftJump();
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
    void BuildQuickBarsManually();
    void InitializeQuickBarsNative();
    void SetQuickBarReady();
    void ActivateQuickBarSlot(uint8_t bar, int slot);
    void ForceUpdateQuickBars();
    void SpawnFloorLoot();
    void EnableMovementFeatures();
}

// ============================================================================
// STAMINA
// ============================================================================
void Athena::InitStaminaAttributes() {
    LOG("[Stamina] skipped");
    session.stamina_initialized = true;
}

// ============================================================================
// ABILITIES
// ============================================================================
void Athena::GrantAbility(void* klass) {
    if (!klass || IsBadPtr(klass) || !session.pawn) return;
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
    LOG("[Abilities] Setup");
    void* asc = GetASC(session.pawn);
    if (!asc) { LOG("[Abilities] no ASC"); return; }
    if (!session.stamina_initialized) InitStaminaAttributes();

    void* coreSet = LoadAsset(
        "/Game/Abilities/Player/Generic/Traits/DefaultPlayer/GAS_AthenaPlayer.GAS_AthenaPlayer",
        "/Script/FortniteGame.FortAbilitySet");
    if (!coreSet) { session.abilities_granted = true; return; }
    if (ApplyAbilitySet(session.pawn, coreSet)) session.granted_ability_sets.push_back(coreSet);

    const char* extraSets[] = {
        "/TacticalSprint/Gameplay/AS_TacticalSprint.AS_TacticalSprint",
        "/Ascender/Gameplay/Ascender/AS_Ascender.AS_Ascender",
        "/DoorBashContent/Gameplay/AS_DoorBash.AS_DoorBash",
        "/HillScramble/Gameplay/AS_HillScramble.AS_HillScramble",
        "/SlideImpulse/Gameplay/AS_SlideImpulse.AS_SlideImpulse",
        nullptr
    };
    for (int i = 0; extraSets[i]; ++i) {
        void* set = FindObj(extraSets[i]);
        if (!set) continue;
        if (ApplyAbilitySet(session.pawn, set)) {
            session.granted_ability_sets.push_back(set);
            LOG("[Abilities] ✅ extra: %s", extraSets[i]);
        }
    }

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
    LOG("[Abilities] Done — %zu sets", session.granted_ability_sets.size());
    GrantMeleeAbility();
}

void Athena::GrantMeleeAbility() {
    if (!session.pawn || session.melee_granted || session.melee_attempted) return;
    session.melee_attempted = true;
    void* asc = GetASC(session.pawn);
    if (!asc) return;
    void* meleeCls = LoadClass(
        "/Game/Abilities/Weapons/Melee/GAB_Melee_GenericCombo.GAB_Melee_GenericCombo_C",
        "/Script/GameplayAbilities.GameplayAbility");
    if (!meleeCls) return;
    Call give("/Script/GameplayAbilities.AbilitySystemComponent.K2_GiveAbility");
    if (!give.ok()) return;
    int level = 1, inputID = -1;
    give.set("AbilityClass", meleeCls);
    give.set("Level", level);
    give.set("InputID", inputID);
    if (give.run(asc)) { LOG("[Melee] granted"); session.melee_granted = true; }
}

// ============================================================================
// QUICKBARS
// ============================================================================
void Athena::BuildQuickBarsManually() {
    if (!session.quickbars || !session.inventory) { LOG("[QB] no qb/inv"); return; }
    void* primProp = GetProp("/Script/FortniteGame.FortQuickBars.PrimaryQuickBar");
    void* slotsProp = GetPropEx("/Script/FortniteGame.QuickBar.Slots",
                                "/Script/FortniteGame.FortQuickBar.Slots");
    void* itemsProp = GetPropEx("/Script/FortniteGame.QuickBarSlot.Items",
                                "/Script/FortniteGame.FortQuickBarSlot.Items");
    if (!primProp || !slotsProp || !itemsProp) { LOG("[QB] props missing"); return; }
    void* invProp  = GetProp("/Script/FortniteGame.FortInventory.Inventory");
    void* instProp = GetProp("/Script/FortniteGame.FortItemList.ItemInstances");
    if (!invProp || !instProp) return;
    void* list = GetField(session.inventory, invProp);
    if (!list) return;
    Array instArr = Read<Array>(GetField(list, instProp));
    void* bar = GetField(session.quickbars, primProp);
    if (!bar) return;
    void* sf = GetField(bar, slotsProp);
    if (!sf) return;
    Array slots = Read<Array>(sf);
    const int slotSize = profile::QuickBarSlot_Size;
    if (slots.Count < 6 || !slots.Data) {
        void* newData = calloc(6, (size_t)slotSize);
        if (!newData) return;
        Write<void*>(sf, 0, newData);
        Write<int32>(sf, 8, 6);
        Write<int32>(sf, 12, 6);
        slots = Read<Array>(sf);
    }
    int bound = 0;
    for (int i = 0; i < instArr.Count && i < 6 && slots.Data; ++i) {
        void* item = Read<void*>(instArr.Data, (size_t)i * 8);
        if (!item || IsBadPtr(item)) continue;
        Call g("/Script/FortniteGame.FortItem.GetItemGuid");
        if (!g.ok() || !g.run(item)) continue;
        Guid guid = g.result<Guid>();
        if (!(guid.A | guid.B | guid.C | guid.D)) continue;
        void* slot = (char*)slots.Data + (size_t)i * slotSize;
        void* ia = GetField(slot, itemsProp);
        if (!ia) continue;
        Array empty{}; Write<Array>(ia, 0, empty);
        Fn<int(*)(void*,void*,const void*)>(profile::GenericArrayAdd)(ia, itemsProp, &guid);
        SetBool(slot, "/Script/FortniteGame.QuickBarSlot.bEnabled", true);
        bound++;
    }
    LOG("[QB] bound=%d", bound);
    session.quickbars_initialized = (bound > 0);
}
void Athena::InitializeQuickBarsNative() {
    LOG("[QB] native disabled");
    session.quickbars_initialized = false;
}
void Athena::SetQuickBarReady() {}
void Athena::ActivateQuickBarSlot(uint8_t bar, int slot) {
    if (!session.controller) return;
    Call act("/Script/FortniteGame.FortPlayerController.ClientActivateSlot");
    if (!act.ok()) return;
    float delay = 0.f; bool bp = true, bf = true;
    if (!act.set("InQuickBar", bar) || !act.set("Slot", slot) ||
        !act.set("ActivateDelay", delay) ||
        !act.set("bUpdatePreviousFocusedSlot", bp) ||
        !act.set("bForceExecution", bf)) return;
    act.run(session.controller);
}
void Athena::ForceUpdateQuickBars() {
    if (!session.controller) return;
    for (uint8_t bar : {(uint8_t)0, (uint8_t)1}) {
        Call c("/Script/FortniteGame.FortPlayerController.ClientForceUpdateQuickbar");
        if (c.ok() && c.set("QuickbarToRefresh", bar)) c.run(session.controller);
    }
}

// ============================================================================
// MOVEMENT
// ============================================================================
void Athena::EnableMovementFeatures() {
    if (!session.pawn || IsBadPtr(session.pawn)) return;
    Call cs("/Script/FortniteGame.FortPlayerPawn.SetCanSlide");
    if (cs.ok()) { bool t = true; if (cs.set("bCanSlide", t)) cs.run(session.pawn); }
    void* cmc = GetObjField(session.pawn, "/Script/Engine.Character.CharacterMovement");
    if (cmc && !IsBadPtr(cmc)) Off<float>(cmc, profile::CMC_MaxSwimSpeed) = 400.0f;
    void* slide = LoadClass("/Game/Abilities/Player/Generic/Traits/DefaultPlayer/GA_Athena_Player_Slide.GA_Athena_Player_Slide_C",
                             "/Script/GameplayAbilities.GameplayAbility");
    if (slide) {
        void* asc = GetASC(session.pawn);
        if (asc) {
            Call give("/Script/GameplayAbilities.AbilitySystemComponent.K2_GiveAbility");
            if (give.ok()) {
                int l = 1, i = -1;
                if (give.set("AbilityClass", slide) && give.set("Level", l) && give.set("InputID", i))
                    give.run(asc);
            }
        }
    }
    session.movement_enabled = true;
    LOG("[Movement] Done");
}

void Athena::SetupTacticalSprint() {}

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
    void* pawnCls = FindObj("/Game/Athena/PlayerPawn_Athena.PlayerPawn_Athena_C");
    if (pawnCls && IsA(session.pawn, pawnCls)) {
        float* sp = (float*)((char*)session.pawn + profile::FPPA_Speed_Sprint);
        if (!IsBadPtr(sp)) {
            float old = *sp;
            if (std::isfinite(old) && old > 0.0f && old < 5000.0f && fabsf(old - 1300.0f) > 1.0f)
                *sp = 1300.0f;
        }
    }
}

// ============================================================================
// AIRCRAFT — Erbium style
// ============================================================================
void Athena::StartAircraftPhase() { SpawnBattleBus(); }
void Athena::OnAircraftJump() { HandleJumpFromHudStart(); }

void Athena::SpawnBattleBus() {
    if (!session.finalized || !session.mode || !session.state || !session.world) return;
    if (session.aircraft_phase_started) return;
    if (IsBadPtr(session.state) || IsBadPtr(session.world)) return;

    LOG("[Bus] === StartAircraftPhase ===");

    // 1) Get MapInfo
    void* mapInfo = GetObjField(session.state, "/Script/FortniteGame.FortGameStateAthena.MapInfo");
    if (!mapInfo) { LOG("[Bus] no MapInfo"); return; }

    // 2) Get FlightInfo[0] from MapInfo
    void* flightArrProp = GetProp("/Script/FortniteGame.AthenaMapInfo.FlightInfos");
    if (!flightArrProp) { LOG("[Bus] no FlightInfos prop"); return; }
    void* flightArrField = GetField(mapInfo, flightArrProp);
    if (!flightArrField) { LOG("[Bus] no flight array"); return; }
    Array flights = Read<Array>(flightArrField);
    LOG("[Bus] FlightInfos count=%d", flights.Count);
    if (flights.Count < 1 || !flights.Data) { LOG("[Bus] no FlightInfo"); return; }
    void* flightInfoPtr = flights.Data;

    // 3) AircraftClass
    void* aircraftClsProp = GetProp("/Script/FortniteGame.AthenaMapInfo.AircraftClass");
    void* aircraftCls = nullptr;
    if (aircraftClsProp) aircraftCls = Read<void*>(GetField(mapInfo, aircraftClsProp));
    if (!aircraftCls || IsBadPtr(aircraftCls))
        aircraftCls = LoadClass("/Game/Athena/Aircraft/B_Athena_Aircraft.B_Athena_Aircraft_C",
                                "/Script/FortniteGame.FortAthenaAircraft");
    if (!aircraftCls || IsBadPtr(aircraftCls)) { LOG("[Bus] no aircraft class"); return; }
    LOG("[Bus] class=%s", ObjName(aircraftCls).c_str());

    // 4) Try native SpawnAircraft
    void* aircraft = nullptr;
    Call spawnAircraft("/Script/FortniteGame.FortAthenaAircraft.SpawnAircraft");
    if (spawnAircraft.ok()) {
        spawnAircraft.set("WorldContextObject", session.world);
        spawnAircraft.set("AircraftClass", aircraftCls);
        spawnAircraft.set_bytes("FlightInfo", flightInfoPtr, 0x60);
        void* cdo = FindObj("/Script/FortniteGame.Default__FortAthenaAircraft");
        if (cdo && spawnAircraft.run(cdo)) {
            aircraft = spawnAircraft.result<void*>();
            if (aircraft && IsBadPtr(aircraft)) aircraft = nullptr;
            LOG("[Bus] native aircraft=%p", aircraft);
        }
    }

    // 5) Fallback — raw SpawnActor
    if (!aircraft) {
        LOG("[Bus] fallback spawn via SpawnActor");
        Transform t{};
        t.Location[0] = 0.0;
        t.Location[1] = 0.0;
        t.Location[2] = 25000.0;
        t.Scale[0] = t.Scale[1] = t.Scale[2] = 1.0;
        aircraft = SpawnActor(session.world, aircraftCls, session.state, t);
        LOG("[Bus] fallback aircraft=%p", aircraft);
    }
    if (!aircraft || IsBadPtr(aircraft)) { LOG("[Bus] spawn failed"); return; }
    session.aircraft = aircraft;

    // 6) Set flight times
    float now = (float)Monotonic();
    Off<float>(aircraft, profile::Aircraft_FlightStartTime)   = now;
    Off<float>(aircraft, profile::Aircraft_FlightEndTime)     = now + 60.0f;
    Off<float>(aircraft, profile::Aircraft_DropStartTime)     = now + 5.0f;
    Off<float>(aircraft, profile::Aircraft_DropEndTime)       = now + 40.0f;
    Off<float>(aircraft, profile::Aircraft_FlightElapsedTime) = 0.0f;
    Off<float>(aircraft, profile::Aircraft_ReplicatedFlightTimestamp) = now;

    // 7) Register
    void* arrProp = GetProp("/Script/FortniteGame.FortGameStateAthena.Aircrafts");
    if (arrProp) {
        void* arrField = GetField(session.state, arrProp);
        if (arrField) {
            Array a = Read<Array>(arrField);
            if (a.Count == 0) {
                Fn<int(*)(void*,void*,const void*)>(profile::GenericArrayAdd)(arrField, arrProp, &aircraft);
                LOG("[Bus] added to Aircrafts[]");
            }
        }
    }
    void* singleProp = GetProp("/Script/FortniteGame.FortGameStateAthena.Aircraft");
    if (singleProp) Write<void*>(GetField(session.state, singleProp), 0, aircraft);

    // 8) Destroy local pawn (Erbium style)
    LOG("[Bus] destroying local pawn %p", session.pawn);
    if (session.pawn && !IsBadPtr(session.pawn)) {
        SetBool(session.pawn, "/Script/FortniteGame.FortPlayerPawn.bIsInsideSafeZone", true);
        Call safe("/Script/FortniteGame.FortPlayerPawn.OnRep_IsInsideSafeZone");
        if (safe.ok()) safe.run(session.pawn);
        Call destroy("/Script/Engine.Actor.K2_DestroyActor");
        if (destroy.ok()) destroy.run(session.pawn);
    }
    session.pawn = nullptr;

    // 9) Spawn fresh skydiving pawn
    LOG("[Bus] spawning skydive pawn");
    void* pawnCls = LoadClass("/Game/Athena/PlayerPawn_Athena.PlayerPawn_Athena_C", "/Script/Engine.Pawn");
    if (pawnCls) {
        Transform t{};
        t.Location[0] = 0.0;
        t.Location[1] = 0.0;
        t.Location[2] = 25000.0;
        t.Scale[0] = t.Scale[1] = t.Scale[2] = 1.0;
        void* newPawn = SpawnActor(session.world, pawnCls, session.controller, t);
        if (newPawn && !IsBadPtr(newPawn)) {
            session.pawn = newPawn;
            SetBool(newPawn, "/Script/FortniteGame.FortPlayerPawn.bIsSkydiving", true);
            SetBool(newPawn, "/Script/FortniteGame.FortPlayerPawn.bIsSkydivingFromBus", true);
            SetBool(newPawn, "/Script/FortniteGame.FortPlayerPawn.bIsParachuteOpen", false);
            Possess(session.controller, newPawn);

            Call rc("/Script/Engine.PlayerController.ClientRestart");
            if (rc.ok()) rc.set("NewPawn", newPawn), rc.run(session.controller);

            Call cam("/Script/Engine.PlayerController.SetViewTargetWithBlend");
            if (cam.ok()) {
                cam.set("NewViewTarget", newPawn);
                float blend = 0.5f;
                cam.set("BlendTime", blend);
                cam.run(session.controller);
            }
            LOG("[Bus] new skydive pawn %p", newPawn);
        }
    }

    // 10) Switch GamePhase
    void* gp = GetProp("/Script/FortniteGame.FortGameStateAthena.GamePhase");
    if (gp) {
        uint8_t phase = 3;
        Write<uint8_t>(GetField(session.state, gp), 0, phase);
        Call n("/Script/FortniteGame.FortGameStateAthena.OnRep_GamePhase");
        if (n.ok()) { uint8_t old = 2; n.set("OldGamePhase", old); n.run(session.state); }
    }
    void* stepProp = GetProp("/Script/FortniteGame.FortGameStateAthena.GamePhaseStep");
    if (stepProp) {
        uint8_t s = 4; // BusLocked
        Write<uint8_t>(GetField(session.state, stepProp), 0, s);
    }

    session.aircraft_phase_started = true;
    session.storm_at = Monotonic() + 90.0;
    LOG("[Bus] ✅ aircraft phase active");
}

void Athena::HandleJumpFromHudStart() {
    if (!session.controller || !session.pawn || session.aircraft_jumped) return;
    Guard g;
    LOG("[Bus] Jump intercepted");

    // Transition to SafeZones phase
    void* gp = GetProp("/Script/FortniteGame.FortGameStateAthena.GamePhase");
    if (gp) {
        uint8_t phase = 4;
        Write<uint8_t>(GetField(session.state, gp), 0, phase);
        Call n("/Script/FortniteGame.FortGameStateAthena.OnRep_GamePhase");
        if (n.ok()) { uint8_t old = 3; n.set("OldGamePhase", old); n.run(session.state); }
    }
    void* stepProp = GetProp("/Script/FortniteGame.FortGameStateAthena.GamePhaseStep");
    if (stepProp) {
        uint8_t s = 6; // StormForming
        Write<uint8_t>(GetField(session.state, stepProp), 0, s);
    }

    Call sky("/Script/FortniteGame.FortPlayerPawn.BeginSkydiving");
    if (sky.ok()) {
        bool bFromBus = true;
        sky.set("bFromBus", bFromBus);
        sky.run(session.pawn);
    }

    Call cam("/Script/Engine.PlayerController.SetViewTargetWithBlend");
    if (cam.ok()) {
        cam.set("NewViewTarget", session.pawn);
        float blend = 0.0f;
        cam.set("BlendTime", blend);
        cam.run(session.controller);
    }

    session.aircraft_jumped = true;
    LOG("[Bus] ✅ skydiving");
}

// ============================================================================
// CHEATS
// ============================================================================
void Athena::SetupCheats() {
    void* pc = session.controller; if (!pc) return;
    void* p = GetProp("/Script/Engine.PlayerController.CheatManager");
    void* klass = FindObj("/Script/FortniteGame.FortCheatManager");
    if (!klass) klass = FindObj("/Script/Engine.CheatManager");
    if (!p || !klass || Read<int>(p, profile::PropElementSize) != 8) return;
    void* cheat = Read<void*>(GetField(pc, p));
    if (!cheat || IsBadPtr(cheat)) {
        cheat = Fn<void*(*)(void*, void*)>(profile::SpawnObject)(klass, pc);
        if (cheat) Write<void*>(GetField(pc, p), 0, cheat);
    }
    if (cheat && !IsBadPtr(cheat)) {
        Call init("/Script/Engine.CheatManager.ReceiveInitCheatManager");
        if (init.ok()) init.run(cheat);
        session.cheats_ready = true;
    }
}

// ============================================================================
// DOORS
// ============================================================================
void Athena::HandleDoor(void* door) {
    if (!door || IsBadPtr(door)) return;
    static void* wallCls = nullptr;
    if (!wallCls) wallCls = FindObj("/Script/FortniteGame.BuildingWall");
    if (!wallCls || !IsA(door, wallCls)) return;
    uint8_t* byte = (uint8_t*)door + profile::Wall_bDoorOpen_Byte;
    bool wasOpen = ((*byte) >> profile::Wall_bDoorOpen_Bit) & 1;
    if (wasOpen) *byte &= ~(1 << profile::Wall_bDoorOpen_Bit);
    else *byte |= (1 << profile::Wall_bDoorOpen_Bit);
    Call rep("/Script/FortniteGame.BuildingWall.OnRep_bDoorOpen");
    if (rep.ok()) rep.run(door);
    Call netup("/Script/Engine.Actor.ForceNetUpdate");
    if (netup.ok()) netup.run(door);
}

// ============================================================================
// COSMETICS
// ============================================================================
void Athena::SyncMCPProfile() {
    if (session.mcp_synced || !session.controller) return;
    void* source = nullptr;
    const char* paths[] = {
        "/Script/FortniteGame.FortPlayerController.CosmeticLoadoutPC",
        "/Script/FortniteGame.FortPlayerControllerAthena.CustomizationLoadout",
        "/Script/FortniteGame.FortPlayerController.CustomizationLoadout",
        nullptr
    };
    for (int i=0; paths[i]; ++i) { source = GetProp(paths[i]); if (source) break; }
    if (!source) return;
    void* info = GetObjField(session.controller, "/Script/FortniteGame.FortPlayerController.MyPlayerInfo");
    void* idProp = GetProp("/Script/FortniteGame.FortRegisteredPlayerInfo.PlayerID");
    void* ctxType = FindObj("/Script/FortniteGame.FortMcpContext");
    void* lib = FindObj("/Script/BlueprintContext.Default__BlueprintContextLibrary");
    if (!info || !idProp || !ctxType || !lib) return;
    int sourceSize = Read<int>(source, profile::PropElementSize);
    if (sourceSize <= 0 || sourceSize > 2048) return;
    void* idAddr = GetField(info, idProp);
    int idSize = Read<int>(idProp, profile::PropElementSize);
    if (!idAddr || idSize <= 0 || idSize > 256) return;
    Call context("/Script/BlueprintContext.BlueprintContextLibrary.GetContext");
    if (!context.ok() || !context.set("ContextObject", session.controller) ||
        !context.set("Class", ctxType) || !context.run(lib)) return;
    void* ctx = context.result<void*>();
    if (!ctx || IsBadPtr(ctx)) return;
    Call get("/Script/FortniteGame.FortMcpContext.GetLoadoutForPlayer");
    if (!get.ok() || !get.set_bytes("MemberUniqueId", idAddr, (size_t)idSize) || !get.run(ctx)) return;
    const void* value = get.result_data((size_t)sourceSize);
    if (!value) return;
    void* dst = GetField(session.controller, source);
    if (!dst) return;
    memcpy(dst, value, (size_t)sourceSize);
    session.mcp_synced = true;
}

void Athena::LoadProfileCosmetics() {
    if (!session.pawn || !session.controller || session.cosmetics_applied) return;
    SyncMCPProfile();
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
    if (hero && !IsBadPtr(hero)) {
        void* htProp = GetProp("/Script/FortniteGame.FortPlayerState.HeroType");
        if (htProp) Write<void*>(GetField(state, htProp), 0, hero);
    }
    std::vector<void*> parts;
    void* hp = GetProp("/Script/FortniteGame.FortHero.CharacterParts");
    if (menu && hp) {
        Array a = Read<Array>(GetField(menu, hp));
        if (a.Count >= 0 && a.Count <= 32 && (!a.Count || a.Data))
            for (int i=0;i<a.Count;i++) { void* p=Read<void*>(a.Data,(size_t)i*8); if(p&&!IsBadPtr(p)) parts.push_back(p); }
    }
    void* backpack = GetObjField(loadout, "/Script/FortniteGame.FortAthenaLoadout.Backpack");
    void* bp = GetProp("/Script/FortniteGame.AthenaCharacterPartItemDefinition.CharacterParts");
    if (backpack && bp) {
        Array a = Read<Array>(GetField(backpack, bp));
        if (a.Count >= 0 && a.Count <= 16 && (!a.Count || a.Data))
            for (int i=0;i<a.Count;i++) { void* p=Read<void*>(a.Data,(size_t)i*8); if(p&&!IsBadPtr(p)) parts.push_back(p); }
    }
    void* partCls = FindObj("/Script/FortniteGame.CustomCharacterPart");
    for (void* part : parts) {
        if (!partCls || !IsA(part, partCls)) continue;
        void* tp = GetProp("/Script/FortniteGame.CustomCharacterPart.CharacterPartType");
        if (!tp) continue;
        uint8_t t = Read<uint8_t>(GetField(part, tp));
        if (t > 6) continue;
        Call ch("/Script/FortniteGame.FortPlayerPawn.ServerChoosePart");
        if (ch.ok() && ch.set("Part", t) && ch.set("ChosenCharacterPart", part) && ch.run(session.pawn))
            session.character_parts.push_back(part);
    }
    if (!parts.empty()) {
        Array arr{parts.data(), (int32)parts.size(), (int32)parts.size()};
        Call ap("/Script/FortniteGame.FortKismetLibrary.ApplyCharacterCosmetics");
        void* klib = FindObj("/Script/FortniteGame.Default__FortKismetLibrary");
        if (ap.ok() && klib && ap.set("WorldContextObject", session.world) &&
            ap.set("CharacterParts", arr) && ap.set("PlayerState", state))
            ap.run(klib);
    }
    Call r1("/Script/FortniteGame.FortPlayerState.OnRep_HeroType"); if(r1.ok())r1.run(state);
    Call r2("/Script/FortniteGame.FortPlayerState.OnRep_CharacterData"); if(r2.ok())r2.run(state);
    session.cosmetics_applied = true;
}

void Athena::ShowSkin() {
    if (session.cosmetics_applied || session.profile_attempted) return;
    session.profile_attempted = true;
    LoadProfileCosmetics();
}

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
// HUD
// ============================================================================
void Athena::InitializeHUD() {
    if (!session.controller || !session.world) return;
    LOG("[HUD] InitializeHUD");
    Call spawn("/Script/FortniteGame.FortPlayerControllerZone.ClientOnPawnSpawned");
    if (spawn.ok()) spawn.run(session.controller);
    Call hudInit("/Script/Engine.GameModeBase.InitializeHUDForPlayer");
    if (hudInit.ok() && session.mode && hudInit.set("NewPlayer", session.controller))
        hudInit.run(session.mode);
    Call getHud("/Script/Engine.PlayerController.GetHUD");
    if (getHud.ok() && getHud.run(session.controller)) {
        void* hud = getHud.result<void*>();
        if (hud && !IsBadPtr(hud)) {
            Call show("/Script/Engine.HUD.ShowHUD");
            if (show.ok()) show.run(hud);
        }
    }
    void* lib = FindObj("/Script/BlueprintContext.Default__BlueprintContextLibrary");
    void* type = FindObj("/Script/FortniteUI.FortHUDContext");
    if (lib && type) {
        Call get("/Script/BlueprintContext.BlueprintContextLibrary.GetContext");
        if (get.ok() && get.set("ContextObject", session.controller) &&
            get.set("Class", type) && get.run(lib)) {
            void* ctx = get.result<void*>();
            if (ctx && !IsBadPtr(ctx)) {
                Call pawn("/Script/FortniteUI.FortHUDContext.HandleLocalPawnSpawned");
                if (pawn.ok()) pawn.run(ctx);
            }
        }
    }
}

void Athena::RefreshHUD() {
    if (!session.controller || !session.world) return;
    Call spawn("/Script/FortniteGame.FortPlayerControllerZone.ClientOnPawnSpawned");
    if (spawn.ok()) spawn.run(session.controller);
    Call worldInv("/Script/FortniteGame.FortPlayerController.ClientForceWorldInventoryUpdate");
    if (worldInv.ok()) worldInv.run(session.controller);
    if (session.inventory) {
        Call inv("/Script/FortniteGame.FortInventory.HandleInventoryLocalUpdate");
        if (inv.ok()) inv.run(session.inventory);
    }
    Call local("/Script/FortniteGame.FortPlayerController.HandleWorldInventoryLocalUpdate");
    if (local.ok()) local.run(session.controller);
    for (uint8_t bar : {uint8_t(0), uint8_t(1)}) {
        Call qb("/Script/FortniteGame.FortPlayerController.ClientForceUpdateQuickbar");
        if (qb.ok() && qb.set("QuickbarToRefresh", bar)) qb.run(session.controller);
    }
}

// ============================================================================
// SOLO HUD / MAP
// ============================================================================
void Athena::UpdateSoloHUD() {
    if(!session.state||!session.controller)return;
    int botsAlive = session.bots_spawned_count;
    if (session.bot_service && !IsBadPtr(session.bot_service)) {
        Array pb = Read<Array>(session.bot_service, profile::BotSvc_PlayerBots);
        if (pb.Count >= 0 && pb.Count < 200 && (!pb.Count || pb.Data)) {
            int cnt = 0;
            for (int i = 0; i < pb.Count; ++i) {
                void* e = (char*)pb.Data + i * profile::BotInfo_Size;
                void* bc = Read<void*>(e, profile::BotInfo_BotController);
                if (!bc || IsBadPtr(bc)) continue;
                void* bp = Read<void*>(bc, profile::BotCtrl_PlayerBotPawn);
                if (bp && !IsBadPtr(bp)) cnt++;
            }
            botsAlive = cnt;
        }
    }
    int playersLeft = 1 + botsAlive;
    int totalPlayers = 1 + session.bots_target;
    Write<int>(session.state, profile::GS_PlayersLeft, playersLeft);
    Write<int>(session.state, profile::GS_TotalPlayers, totalPlayers);
    Call rep("/Script/FortniteGame.FortGameStateAthena.OnRep_PlayersLeft");if(rep.ok())rep.run(session.state);
    Call pc("/Script/FortniteGame.FortPlayerControllerAthena.HandlePlayersLeftChanged");if(pc.ok())pc.run(session.controller);
    session.solo_hud_ready=true;
}
void Athena::SetupLocalPlayerCounter() { UpdateSoloHUD(); }

void Athena::SetupLocalMinimap() {
    if(!session.world)return;
    void* wb=FindObj("/Script/UMG.Default__WidgetBlueprintLibrary");
    void* cls=FindObj("/Script/AthenaMinimapContainer.AthenaMinimapContainer_C");
    if(!cls)cls=FindObj("/Script/FortniteUI.AthenaMinimapContainer");
    if(!wb||!cls)return;
    Call all("/Script/UMG.WidgetBlueprintLibrary.GetAllWidgetsOfClass");
    if(!all.ok()||!all.set("WorldContextObject",session.world)||
       !all.set("WidgetClass",cls)||!all.run(wb)) return;
    Array a=all.result<Array>();
    if(a.Count<0 || a.Count>256 || (a.Count && !a.Data)) return;
    for(int i=0;i<a.Count;i++){
        void*w=Read<void*>(a.Data,(size_t)i*8);if(!w||IsBadPtr(w))continue;
        Call vis("/Script/UMG.Widget.SetVisibility");
        if(vis.ok()){uint8_t v=0;vis.set("InVisibility",v);vis.run(w);}
    }
    session.minimap_initialized = true;
}

void Athena::SetupLocalClamber() {
    if(!session.pawn)return;
    void* cls=FindObj("/Script/ClamberingCodeRuntime.ClamberingComponent");
    if(!cls)return;
    Call get("/Script/Engine.Actor.GetComponentByClass");
    if(!get.ok()||!get.set("ComponentClass",cls)||!get.run(session.pawn))return;
    session.clamber_initialized = true;
}

void Athena::SetupLocalVending() {
    if (!session.world || !session.pawn) return;
    void* cls = FindObj("/Script/FortniteGame.BuildingProp_VendingMachine");
    void* wb = FindObj("/Script/Engine.Default__GameplayStatics");
    if (!cls || !wb) return;
    Call all("/Script/Engine.GameplayStatics.GetAllActorsOfClass");
    if (!all.ok() || !all.set("WorldContextObject", session.world) ||
        !all.set("ActorClass", cls) || !all.run(wb)) return;
    Array a = all.result<Array>();
    if (a.Count < 0 || a.Count > 4096 || (a.Count && !a.Data)) return;
    for (int i=0;i<a.Count;i++) {
        void* m=Read<void*>(a.Data,(size_t)i*8); if(!m||IsBadPtr(m)) continue;
        Call inv("/Script/FortniteGame.BuildingProp_VendingMachine.OnPlayerInventoryChange");
        if(inv.ok())inv.run(m);
    }
    session.vending_initialized = true;
}

void Athena::SetupLocalVehicles() {
    if (!session.world) return;
    void* spawnerCls = FindObj("/Script/FortniteGame.FortAthenaVehicleSpawner");
    if (!spawnerCls) return;
    void* wb = FindObj("/Script/Engine.Default__GameplayStatics");
    if (!wb) return;
    Call all("/Script/Engine.GameplayStatics.GetAllActorsOfClass");
    if (!all.ok() || !all.set("WorldContextObject",session.world) ||
        !all.set("ActorClass",spawnerCls) || !all.run(wb)) return;
    Array a=all.result<Array>();
    if(a.Count<0||a.Count>4096||(a.Count&&!a.Data))return;
    int spawned=0;
    for(int i=0;i<a.Count;i++){
        void* sp=Read<void*>(a.Data,(size_t)i*8); if(!sp||IsBadPtr(sp)) continue;
        Call get("/Script/FortniteGame.FortAthenaVehicleSpawner.GetVehicleClass");
        if(!get.ok()||!get.run(sp))continue;
        void* vc=get.result<void*>(); if(!vc||IsBadPtr(vc))continue;
        Call loc("/Script/Engine.Actor.K2_GetActorLocation");
        Call rot("/Script/Engine.Actor.K2_GetActorRotation");
        if(!loc.ok()||!rot.ok()||!loc.run(sp)||!rot.run(sp))continue;
        Transform t{}; Vector v=loc.result<Vector>(); Rotator r=rot.result<Rotator>();
        t.Location[0]=v.X; t.Location[1]=v.Y; t.Location[2]=v.Z;
        t.Scale[0]=t.Scale[1]=t.Scale[2]=1.0;
        void* vehicle=SpawnActor(session.world,vc,sp,t);
        if(!vehicle)continue;
        Call fuel("/Script/FortniteGame.FortDagwoodVehicle.SetFuel");
        if(fuel.ok()){float f=100.f;fuel.set("Fuel",f);fuel.run(vehicle);}
        spawned++;
    }
    session.vehicles_spawned = true;
    LOG("[Vehicles] %d", spawned);
}

static void* GetBotService() {
    if (session.bot_service && !IsBadPtr(session.bot_service)) return session.bot_service;
    void* klib = FindObj("/Script/FortniteAI.Default__AthenaAIBlueprintLibrary");
    if (!klib) return nullptr;
    Call getSvc("/Script/FortniteAI.AthenaAIBlueprintLibrary.GetAIServicePlayerBots");
    if (!getSvc.ok()) return nullptr;
    if (!getSvc.set("WorldContextObject", session.world)) return nullptr;
    if (!getSvc.run(klib)) return nullptr;
    void* svc = getSvc.result<void*>();
    if (!svc || IsBadPtr(svc)) return nullptr;
    session.bot_service = svc;
    return svc;
}

void Athena::SetupLocalAI() {
    if (session.ai_initialized) return;
    // no-op for now
    GetBotService();
    session.ai_initialized = true;
}

void Athena::SpawnSoloBotsBatch(int maxCount) {
    if (session.bots_spawned || !session.world || !session.state) return;
    if (session.bots_spawned_count >= session.bots_target) { session.bots_spawned = true; return; }
    void* svc = GetBotService();
    if (!svc) { session.bots_spawned = true; return; }
    Call spawnAI("/Script/FortniteAI.AthenaAIServicePlayerBots.SpawnAI");
    if (!spawnAI.ok()) { session.bots_spawned = true; return; }
    int made = 0;
    for (int i = 0; i < maxCount && session.bots_spawned_count < session.bots_target; ++i) {
        Vector base{0, 0, 10000};
        if (session.pawn && !IsBadPtr(session.pawn)) {
            Call loc("/Script/Engine.Actor.K2_GetActorLocation");
            if (loc.ok() && loc.run(session.pawn)) base = loc.result<Vector>();
        }
        double ang = (session.bots_spawned_count + i) * 0.6180339887 * 6.28318530718;
        double rad = 3000.0 + ((session.bots_spawned_count + i) % 10) * 400.0;
        Vector spawnLoc;
        spawnLoc.X = base.X + cos(ang) * rad;
        spawnLoc.Y = base.Y + sin(ang) * rad;
        spawnLoc.Z = base.Z + 500.0;
        Rotator spawnRot{0, 0, 0};
        void* cl = nullptr;
        if (!spawnAI.set("InSpawnLocation", spawnLoc) ||
            !spawnAI.set("InSpawnRotation", spawnRot) ||
            !spawnAI.set("AISpawnerComponentList", cl)) break;
        if (spawnAI.run(svc)) {
            void* bp = spawnAI.result<void*>();
            if (bp && !IsBadPtr(bp)) {
                session.bot_pawns.push_back(bp);
                session.bots_spawned_count++;
                made++;
            }
        }
    }
    if (session.bots_spawned_count >= session.bots_target) session.bots_spawned = true;
    LOG("[Bots] batch=%d total=%d/%d", made, session.bots_spawned_count, session.bots_target);
}

// ============================================================================
// STORM
// ============================================================================
void Athena::ProtectLocalPlayerFromStorm(bool protect) {
    if(!session.state)return;
    void* indicator=GetObjField(session.state,"/Script/FortniteGame.FortGameStateAthena.SafeZoneIndicator");
    if(!indicator || IsBadPtr(indicator))return;
    float radius=protect?100000000.f:0.f;
    if(protect){
        Off<float>(indicator, profile::SZ_NextRadius) = radius;
        Off<float>(indicator, profile::SZ_SafeZoneRadius) = radius;
    }
    SetBool(indicator,"/Script/FortniteGame.FortSafeZoneIndicator.bPaused",protect);
    session.storm_protected=protect;
}

void Athena::SpawnSafeZoneIndicator() {
    if (session.safezone_spawned || !session.state || !session.world) return;
    void* existing = GetObjField(session.state, "/Script/FortniteGame.FortGameStateAthena.SafeZoneIndicator");
    if (existing && !IsBadPtr(existing)) { session.safezone_spawned = true; return; }
    void* cls = nullptr;
    if (session.mode && !IsBadPtr(session.mode)) {
        void* p = GetProp("/Script/FortniteGame.FortGameMode.SafeZoneIndicatorClass");
        if (p) cls = Read<void*>(GetField(session.mode, p));
        if (cls && IsBadPtr(cls)) cls = nullptr;
    }
    if (!cls) cls = FindObj("/Script/FortniteGame.FortSafeZoneIndicator");
    if (!cls || IsBadPtr(cls)) { LOG("[Storm] no class"); return; }
    Transform t{};
    void* indicator = SpawnActor(session.world, cls, session.state, t);
    if (!indicator || IsBadPtr(indicator)) { LOG("[Storm] spawn failed"); return; }
    void* prop = GetProp("/Script/FortniteGame.FortGameStateAthena.SafeZoneIndicator");
    if (prop) Write<void*>(GetField(session.state, prop), 0, indicator);
    Call n("/Script/FortniteGame.FortGameStateAthena.OnRep_SafeZoneIndicator");
    if (n.ok()) n.run(session.state);
    session.safezone_spawned = true;
    LOG("[Storm] indicator=%p", indicator);
}

// ============================================================================
// SIDE SYSTEMS
// ============================================================================
void Athena::InitializeSideSystems() {
    if(!session.finalized||!session.pawn||!session.world||session.side_systems_initialized)return;
    session.side_systems_initialized = true;
    LOG("=== SideSystems ===");
    SetupLocalPlayerCounter();
    SetupLocalMinimap();
    SetupLocalClamber();
    SetupLocalVending();
    SetupLocalVehicles();
    SetupLocalAI();
    if(!session.storm_protected)ProtectLocalPlayerFromStorm(true);
    LOG("=== SideSystems done ===");
}

// ============================================================================
// PLAYER INIT
// ============================================================================
bool Athena::TryInitializePlayer(void* world, void* mode, void* controller) {
    if (!world || !mode || !controller) return false;
    void* existing = GetPawn(controller);
    if (existing && !IsBadPtr(existing)) {
        void* pc = FindObj("/Script/FortniteGame.FortPlayerPawnAthena");
        if (pc && IsA(existing, pc)) { session.pawn = existing; LOG("[Init] existing %p", existing); return true; }
    }
    void* pawnCls = LoadClass("/Game/Athena/PlayerPawn_Athena.PlayerPawn_Athena_C", "/Script/Engine.Pawn");
    if (!pawnCls) return false;
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
                }
            }
        }
    }
    if (!haveSpawn) t.Location[2] = 3100.0;
    session.pawn = SpawnActor(world, pawnCls, controller, t);
    if (!session.pawn || IsBadPtr(session.pawn)) return false;
    LOG("[Init] pawn %p", session.pawn);
    SetBool(session.pawn, "/Script/FortniteGame.FortPlayerPawn.bIsSkydiving", false);
    SetBool(session.pawn, "/Script/FortniteGame.FortPlayerPawn.bIsParachuteOpen", false);
    Possess(controller, session.pawn);
    if (GetPawn(controller) != session.pawn) return false;
    LOG("[Init] possessed");
    SetupCheats();
    Call restart("/Script/Engine.PlayerController.ClientRestart");
    if (restart.ok() && restart.set("NewPawn", session.pawn)) restart.run(controller);
    ForcePlayerVisible();
    return true;
}

// ============================================================================
// SETUP WORLD
// ============================================================================
void Athena::SetupWorld(void* world) {
    session.world = world;
    session.controller = GetPC(world, 0);
    if (!session.controller || IsBadPtr(session.controller)) { LOG("SetupWorld: no controller"); return; }
    void* baseCtrl = FindObj("/Script/FortniteGame.FortPlayerController");
    if (!baseCtrl || !IsA(session.controller, baseCtrl)) { LOG("SetupWorld: wrong ctrl"); return; }
    void* modeProp  = GetProp("/Script/Engine.World.AuthorityGameMode");
    void* stateProp = GetProp("/Script/Engine.World.GameState");
    session.mode  = modeProp  ? Read<void*>(GetField(world, modeProp))  : nullptr;
    session.state = stateProp ? Read<void*>(GetField(world, stateProp)) : nullptr;
    if (!session.mode || !session.state || IsBadPtr(session.mode) || IsBadPtr(session.state)) {
        LOG("SetupWorld: mode/state missing"); return;
    }
    LOG("SetupWorld: PC=%s GM=%s GS=%s",
        ObjName(session.controller).c_str(), ObjName(session.mode).c_str(), ObjName(session.state).c_str());
    if (!IsStandaloneWorld(world)) { LOG("SetupWorld: not standalone"); session.attempted = true; return; }

    // Readiness
    void* rp = GetProp("/Script/FortniteGame.FortPlayerController.bReadyToStartMatch");
    if (rp) {
        int off = Read<int>(rp, profile::PropOffset);
        uint8_t bo = Read<uint8_t>(rp, profile::BoolByteOffset);
        uint8_t mask = Read<uint8_t>(rp, profile::BoolFieldMask);
        LOG("[Ready] off=0x%X byte=%u mask=0x%02X", off, bo, mask);
        if (off > 0 && off < 0x20000 && bo < 8 && mask != 0) {
            uint8_t* f = (uint8_t*)session.controller + off + bo;
            *f |= mask;
        }
    }
    SetBool(session.controller, "/Script/FortniteGame.FortPlayerController.bHasClientFinishedLoading", true);
    SetBool(session.controller, "/Script/FortniteGame.FortPlayerController.bHasServerFinishedLoading", true);

    if (!TryInitializePlayer(world, session.mode, session.controller)) { LOG("SetupWorld: pawn failed"); return; }

    GrantAllAbilitySets();
    SetupTacticalSprint();
    EnableMovementFeatures();
    SyncMCPProfile();
    ShowSkin();
    DestroyLods();
    InitializeInventory();
    SpawnFloorLoot();
    ApplySprintSpeed();
    SpawnBuildPreviews();
    DropLoadingScreen();
    RemoveNetDebugUI();
    InitializeHUD();
    RefreshHUD();

    session.finalize_at = Monotonic() + 2.0;
    session.attempted   = true;
    LOG("SetupWorld: initial done");
}

void Athena::FinalizePlayer() {
    if (session.finalized) return;
    session.finalized = true;
    LOG("=== FinalizePlayer ===");
    void* pc = GetPC(session.world, 0);
    if (pc != session.controller) { LOG("Finalize: ctrl changed"); return; }
    void* pawnCls = FindObj("/Script/FortniteGame.FortPlayerPawnAthena");
    void* pawn = GetPawn(pc);
    if (!pawn || !pawnCls || !IsA(pawn, pawnCls)) { LOG("Finalize: not FortPlayerPawnAthena"); return; }
    session.pawn = pawn;

    if (!session.abilities_granted) GrantAllAbilitySets();
    if (!session.movement_enabled) EnableMovementFeatures();
    if (!session.quickbars_initialized) {
        BuildQuickBarsManually();
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

    // Delayed side systems
    SpawnSafeZoneIndicator();
    ProtectLocalPlayerFromStorm(true);

    InitializeHUD();
    RefreshHUD();
    InitializeSideSystems();

    // Bus will fire after 30s
    session.bus_at = Monotonic() + 30.0;
    LOG("Finalize: complete (bus at +30s)");
}

// ============================================================================
// TICK
// ============================================================================
void Athena::Tick() {
    if (!session.world) return;
    double now = Monotonic();
    if (!session.attempted) {
        if (now - session.travel_at < 1.0) return;
        session.travel_at = now;
        SetupWorld(session.world);
        return;
    }
    if (session.finalize_at > 0 && !session.finalized && now >= session.finalize_at) {
        FinalizePlayer();
        return;
    }
    if (!session.finalized) return;

    // Trigger bus after 30s
    if (!session.aircraft_phase_started && session.bus_at > 0 && now >= session.bus_at) {
        SpawnBattleBus();
    }

    static double lastVis = 0, lastSpd = 0, lastHUD = 0, lastBot = 0, lastStorm = 0, lastQB = 0, lastMove = 0, lastMelee = 0;
    static int qbRetries = 0;

    if (now - lastVis > 15.0) {
        lastVis = now;
        if (!session.visibility_forced) ForcePlayerVisible();
    }
    if (session.pawn && now - lastSpd > 5.0) { lastSpd = now; ApplySprintSpeed(); }
    if (session.pawn && now - lastMove > 10.0) {
        lastMove = now;
        if (!session.movement_enabled) EnableMovementFeatures();
    }
    if (session.pawn && now - lastMelee > 15.0) {
        lastMelee = now;
        if (!session.melee_granted && !session.melee_attempted) GrantMeleeAbility();
    }
    if (session.controller && now - lastQB > 10.0 && qbRetries < 3) {
        lastQB = now;
        if (!session.quickbars_initialized) {
            qbRetries++;
            BuildQuickBarsManually();
        }
    }
    if (session.finalized && !session.aircraft_phase_started && now - lastHUD > 2.0 && session.solo_hud_ready == false) {
        lastHUD = now;
        if (session.bots_spawned_count > 0) UpdateSoloHUD();
    }
    if (!session.bots_spawned && now - lastBot > 5.0 && session.aircraft_jumped) {
        lastBot = now;
        SpawnSoloBotsBatch(10);
    }
    if (session.storm_protected && session.storm_at > 0 && now >= session.storm_at) {
        session.storm_at = 0;
        Call jump("/Script/FortniteGame.FortGameModeAthena.JumpToSafeZonePhase");
        if (jump.ok() && jump.run(session.mode)) {
            ProtectLocalPlayerFromStorm(false);
            LOG("[Storm] real phases enabled");
        }
    }
}

// ============================================================================
// HELPERS
// ============================================================================
void Athena::DestroyLods() {
    void* cm = GetObjField(session.controller, "/Script/Engine.PlayerController.CheatManager");
    if (!cm) { SetupCheats(); cm = GetObjField(session.controller, "/Script/Engine.PlayerController.CheatManager"); }
    if (!cm || IsBadPtr(cm)) return;
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
    LOG("Loading screen dropped");
}

void Athena::InventoryUpdate() {
    if (!session.controller) return;
    { Call c("/Script/FortniteGame.FortPlayerController.ClientForceWorldInventoryUpdate"); if (c.ok()) c.run(session.controller); }
    if (session.inventory) { Call a("/Script/FortniteGame.FortInventory.HandleInventoryLocalUpdate"); if (a.ok()) a.run(session.inventory); }
    { Call b("/Script/FortniteGame.FortPlayerController.HandleWorldInventoryLocalUpdate"); if (b.ok()) b.run(session.controller); }
}

void Athena::AddToInventory(void* def, int count, int slot) {
    if (IsBadPtr(def) || !session.inventory) return;
    void* inst = nullptr;
    {
        Call c("/Script/FortniteGame.FortItemDefinition.CreateTemporaryItemInstanceBP");
        int lvl = 1;
        if (!c.ok() || !c.set("Count", count) || !c.set("Level", lvl) || !c.run(def)) return;
        inst = c.result<void*>();
    }
    if (!inst || IsBadPtr(inst)) return;
    { Call c("/Script/FortniteGame.FortItem.SetOwningControllerForTemporaryItem"); if (c.ok() && c.set("InController", session.controller)) c.run(inst); }
    void* invProp   = GetProp("/Script/FortniteGame.FortInventory.Inventory");
    void* instProp  = GetProp("/Script/FortniteGame.FortItemList.ItemInstances");
    void* entryProp = GetProp("/Script/FortniteGame.FortItemList.ReplicatedEntries");
    void* itemEntryProp = GetProp("/Script/FortniteGame.FortWorldItem.ItemEntry");
    if (!invProp || !instProp || !entryProp || !itemEntryProp) return;
    void* list = GetField(session.inventory, invProp);
    void* instArr  = GetField(list, instProp);
    void* entryArr = GetField(list, entryProp);
    void* entrySrc = GetField(inst, itemEntryProp);
    if (entrySrc) {
        Off<float>(entrySrc, profile::Entry_Durability) = 1.0f;
        void* weapCls = FindObj("/Script/FortniteGame.FortWeaponItemDefinition");
        if (weapCls && IsA(def, weapCls)) {
            Off<int32>(entrySrc, profile::Entry_LoadedAmmo) = 30;
            Off<int32>(entrySrc, profile::Entry_PhantomReserveAmmo) = 300;
        }
    }
    Fn<int(*)(void*,void*,const void*)>(profile::GenericArrayAdd)(instArr, instProp, &inst);
    Fn<int(*)(void*,void*,const void*)>(profile::GenericArrayAdd)(entryArr, entryProp, entrySrc);
    if (slot >= 0 && slot < 6) session.slots[slot] = inst;
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
    { Call so("/Script/FortniteGame.FortInventory.SetOwner"); if (so.ok()) so.run(session.inventory); }

    session.quickbars = GetObjField(session.controller, "/Script/FortniteGame.FortPlayerController.ClientQuickBars");
    if (!session.quickbars) {
        void* klass = FindObj("/Script/FortniteGame.FortQuickBarsAthena");
        if (!klass) klass = FindObj("/Script/FortniteGame.FortQuickBars");
        Transform t{};
        session.quickbars = SpawnActor(session.world, klass, session.controller, t);
        if (session.quickbars) {
            void* qbProp = GetProp("/Script/FortniteGame.FortPlayerController.ClientQuickBars");
            if (qbProp) Write<void*>(GetField(session.controller, qbProp), 0, session.quickbars);
        }
    }
    if (session.quickbars) {
        Write<void*>((char*)session.quickbars + profile::AActor_Owner, 0, session.controller);
        Call orp("/Script/Engine.Actor.OnRep_Owner");
        if (orp.ok()) orp.run(session.quickbars);
        Call soq("/Script/FortniteGame.FortQuickBars.SetOwner");
        if (soq.ok()) { soq.set("NewOwner", session.controller); soq.run(session.quickbars); }
        BuildQuickBarsManually();
    }

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
        if (!def) continue;
        AddToInventory(def, 1, d.slot);
    }
    void* editTool = LoadAsset("/Game/Items/Weapons/BuildingTools/EditTool.EditTool");
    if (editTool) session.edit_tool = editTool;
    const char* ammo[] = {
        "/Game/Athena/Items/Ammo/AthenaAmmoDataBulletsLight.AthenaAmmoDataBulletsLight",
        "/Game/Athena/Items/Ammo/AthenaAmmoDataBulletsMedium.AthenaAmmoDataBulletsMedium",
        "/Game/Athena/Items/Ammo/AthenaAmmoDataBulletsHeavy.AthenaAmmoDataBulletsHeavy",
        "/Game/Athena/Items/Ammo/AthenaAmmoDataShells.AthenaAmmoDataShells", nullptr};
    for (int i = 0; ammo[i]; ++i) { void* def = LoadAsset(ammo[i]); if (def) AddToInventory(def, 999, 10); }
    struct ResDef { const char* path; int slot; };
    ResDef resources[] = {
        {"/Game/Items/ResourcePickups/WoodItemData.WoodItemData", 10},
        {"/Game/Items/ResourcePickups/StoneItemData.StoneItemData", 10},
        {"/Game/Items/ResourcePickups/MetalItemData.MetalItemData", 10},
    };
    for (auto& r : resources) { void* def = LoadAsset(r.path); if (def) AddToInventory(def, 500, r.slot); }
    InventoryUpdate();
    ForceUpdateQuickBars();
    ActivateQuickBarSlot(0, 0);
    session.inventory_ready = true;
    EquipSlot(0);
}

void Athena::EquipSlot(int slot) {
    if (slot < 0 || slot >= 6 || !session.slots[slot]) return;
    void* item = session.slots[slot];
    void* def = nullptr;
    { Call d("/Script/FortniteGame.FortItem.GetItemDefinitionBP"); if (d.ok() && d.run(item)) def = d.result<void*>(); }
    if (!def) return;
    Guid guid{};
    { Call g("/Script/FortniteGame.FortItem.GetItemGuid"); if (g.ok() && g.run(item)) guid = g.result<Guid>(); }
    Call eq("/Script/FortniteGame.FortPawn.EquipWeaponDefinition");
    if (!eq.ok()) return;
    bool bd = false; Guid tg{};
    eq.set("WeaponData", def); eq.set("ItemEntryGuid", guid);
    eq.set("TrackerGuid", tg); eq.set("bDisableEquipAnimation", bd);
    eq.run(session.pawn);
}

// ============================================================================
// FLOOR LOOT / LOOT / BUILD
// ============================================================================
void Athena::SpawnFloorLoot() {
    if(!session.world||!session.pawn)return;
    void* wb=FindObj("/Script/Engine.Default__GameplayStatics");
    void* cls=FindObj("/Script/FortniteGame.BuildingContainer");
    if(!wb||!cls)return;
    Call all("/Script/Engine.GameplayStatics.GetAllActorsOfClass");
    if(!all.ok()||!all.set("WorldContextObject",session.world)||
       !all.set("ActorClass",cls)||!all.run(wb))return;
    Array a=all.result<Array>();
    if(a.Count<0||a.Count>200000||(a.Count&&!a.Data))return;
    int floor=0;
    for(int i=0;i<a.Count;i++){
        void* c=Read<void*>(a.Data,(size_t)i*8);if(!c||IsBadPtr(c))continue;
        std::string low=ObjName(Read<void*>(c,profile::ObjectClass));
        std::transform(low.begin(),low.end(),low.begin(),::tolower);
        if(low.find("floorloot")==std::string::npos && low.find("tiered")==std::string::npos && low.find("loot")==std::string::npos) continue;
        Call q("/Script/FortniteGame.BuildingContainer.BP_IsAlreadySearched");
        if(q.ok()&&q.run(c)&&q.result<bool>())continue;
        Call sp("/Script/FortniteGame.BuildingContainer.BP_SpawnLoot");
        if(sp.ok()&&sp.set("PlayerPawn",session.pawn)&&sp.run(c)){floor++;}
    }
    LOG("[Loot] floor=%d total=%d",floor,a.Count);
}

void Athena::Loot(void* actor) {
    if (!actor || !session.pawn || IsBadPtr(actor)) return;
    void* bcCls=FindObj("/Script/FortniteGame.BuildingContainer");
    if(!bcCls || !IsA(actor,bcCls)) return;
    Call already("/Script/FortniteGame.BuildingContainer.BP_IsAlreadySearched");
    if(already.ok() && already.run(actor) && already.result<bool>()) return;
    Call begin("/Script/FortniteGame.BuildingContainer.OnBeginSearch"); if(begin.ok())begin.run(actor);
    Call sp("/Script/FortniteGame.BuildingContainer.BP_SpawnLoot");
    if(sp.ok() && sp.set("PlayerPawn",session.pawn)) sp.run(actor);
    SetBool(actor,"/Script/FortniteGame.BuildingContainer.bAlreadySearched",true);
    Call rep("/Script/FortniteGame.BuildingContainer.OnRep_bAlreadySearched");if(rep.ok())rep.run(actor);
    Call bounce("/Script/FortniteGame.BuildingContainer.BounceContainer");if(bounce.ok())bounce.run(actor);
}

void Athena::ServerHandlePickup(void* pickup) {
    if (!pickup || !session.inventory) return;
    void* dp = GetProp("/Script/FortniteGame.FortPickup.ItemDefinition");
    void* cp = GetProp("/Script/FortniteGame.FortPickup.Count");
    void* def = dp ? Read<void*>(GetField(pickup, dp)) : nullptr;
    int count = cp ? Read<int>(GetField(pickup, cp)) : 1;
    if (!def) return;
    AddToInventory(def, count, 10);
    InventoryUpdate();
    Call d("/Script/Engine.Actor.K2_DestroyActor");
    if (d.ok()) d.run(pickup);
}

void Athena::OnServerExecuteInventoryItem(Guid guid) {
    if (!session.inventory || !session.pawn) return;
    for (int s = 0; s < 6; ++s) {
        if (!session.slots[s]) continue;
        Call g("/Script/FortniteGame.FortItem.GetItemGuid");
        if (!g.ok() || !g.run(session.slots[s])) continue;
        Guid cand = g.result<Guid>();
        if (memcmp(&cand, &guid, sizeof(Guid)) == 0) { EquipSlot(s); return; }
    }
}

void Athena::OnServerExecuteInventoryWeapon(void* weapon) {
    if (!weapon || !session.pawn) return;
    Guid guid = Read<Guid>(weapon, profile::Weapon_ItemEntryGuid);
    OnServerExecuteInventoryItem(guid);
}

void Athena::SpawnBuildPreviews() {
    void* previewCls = FindObj("BuildingPlayerPrimitivePreview");
    if (!previewCls || !session.controller) return;
    auto build = [&](const char* meshPath, char) -> void* {
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
    session.wall   = build("/Game/Building/ActorBlueprints/Player/Wood/L1/PBWA_W1_Solid.PBWA_W1_Solid", 0);
    session.floor  = build("/Game/Building/ActorBlueprints/Player/Wood/L1/PBWA_W1_Floor.PBWA_W1_Floor", 1);
    session.stairs = build("/Game/Building/ActorBlueprints/Player/Wood/L1/PBWA_W1_StairW.PBWA_W1_StairW", 5);
    session.roof   = build("/Game/Building/ActorBlueprints/Player/Wood/L1/PBWA_W1_RoofC.PBWA_W1_RoofC", 6);
}

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
    eq.set("WeaponData", session.edit_tool);
    eq.set("ItemEntryGuid", guid);
    eq.set("TrackerGuid", tg);
    eq.set("bDisableEquipAnimation", bd);
    if (!eq.run(session.pawn)) return;
    void* ea = eq.result<void*>();
    if (!ea) return;
    void* p = GetProp("/Script/FortniteGame.FortWeapon.EditActor");
    if (p) Write<void*>(GetField(ea, p), 0, piece);
    Call r("/Script/FortniteGame.FortWeapon.OnRep_EditActor");
    if (r.ok()) r.run(ea);
}

void Athena::OnFinishEditActor(void* actor, void* newClass, int, bool mirrored) {
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

void Athena::DropInventoryItem(Guid guid, int count) {
    if(!session.controller || !session.inventory || !session.world || !session.pawn) return;
    if(!(guid.A|guid.B|guid.C|guid.D) || count<1 || count>9999) return;
    void* invProp=GetProp("/Script/FortniteGame.FortInventory.Inventory");
    void* instProp=GetProp("/Script/FortniteGame.FortItemList.ItemInstances");
    void* repProp=GetProp("/Script/FortniteGame.FortItemList.ReplicatedEntries");
    void* itemEntryProp=GetProp("/Script/FortniteGame.FortWorldItem.ItemEntry");
    if(!invProp || !instProp || !repProp || !itemEntryProp) return;
    void* list=GetField(session.inventory,invProp);
    void* instArr=GetField(list,instProp);
    void* repArr=GetField(list,repProp);
    Array instances=Read<Array>(instArr);
    Array replicated=Read<Array>(repArr);
    if(instances.Count<0 || instances.Count>512 || replicated.Count<0 || replicated.Count>512) return;
    constexpr size_t Stride=profile::Entry_Size;
    int ii=-1, ri=-1;
    void* item=nullptr;
    void* re=nullptr;
    for(int i=0;i<instances.Count;++i) {
        void* cand=Read<void*>(instances.Data,(size_t)i*8);
        if(!cand || IsBadPtr(cand)) continue;
        void* e=GetField(cand,itemEntryProp);
        if(!e) continue;
        Guid got=Read<Guid>(e, profile::Entry_ItemGuid);
        if(!memcmp(&got,&guid,sizeof(Guid))) { ii=i; item=cand; break; }
    }
    if(ii<0) return;
    for(int i=0;i<replicated.Count;++i) {
        void* cand=(char*)replicated.Data+(size_t)i*Stride;
        Guid got=Read<Guid>(cand, profile::Entry_ItemGuid);
        if(!memcmp(&got,&guid,sizeof(Guid))) { ri=i; re=cand; break; }
    }
    if(ri<0) return;
    void* ie=GetField(item,itemEntryProp);
    if(!ie) return;
    int have=Read<int>(ie, profile::Entry_Count);
    if(have<1) return;
    count=std::min(count,have);
    bool rm = count>=have;
    if(rm) for(int i=0;i<6;++i) if(session.slots[i]==item) session.slots[i]=nullptr;
    if(!rm) {
        Write<int>(ie, profile::Entry_Count, have-count);
        Write<int>(re, profile::Entry_Count, have-count);
    } else {
        using RemoveFn=void(*)(void*,int32_t,int32_t,int32_t);
        auto fn=Fn<RemoveFn>(profile::GenericArrayRemove);
        fn(instArr,ii,1,8);
        replicated=Read<Array>(repArr);
        if(ri>=0 && ri<replicated.Count && replicated.Data) fn(repArr,ri,1,(int32_t)Stride);
    }
    InventoryUpdate();
}

void Athena::RemoveNetDebugUI() {
    void* wb = FindObj("/Script/UMG.Default__WidgetBlueprintLibrary");
    if (!wb) return;
    void* cls = FindObj("/Game/Athena/HUD/NetDebugUI.NetDebugUI_C");
    if (!cls) return;
    Call c("/Script/UMG.WidgetBlueprintLibrary.GetAllWidgetsOfClass");
    if (!c.ok() || !c.set("WorldContextObject", session.world) || !c.set("WidgetClass", cls)) return;
    if (!c.run(wb)) return;
}

void Athena::PlayEmoteItem(void*) {}
void Athena::OnExitVehicle() {}

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
        if (g_interactionClass && obj) {
            void* cls = Read<void*>(obj, profile::ObjectClass);
            isInteraction = SubclassOfFast(cls, g_interactionClass);
        }
        std::string n = ObjName(func);
        if (n == "ServerAttemptInteract" && params) {
            Guard g;
            void* target = Read<void*>(params, 0);
            if (target && !IsBadPtr(target)) {
                void* bcCls=FindObj("/Script/FortniteGame.BuildingContainer");
                void* wallCls=FindObj("/Script/FortniteGame.BuildingWall");
                void* tc=Read<void*>(target,profile::ObjectClass);
                if(bcCls&&SubclassOfFast(tc,bcCls)){Athena::Loot(target);return;}
                if(wallCls&&SubclassOfFast(tc,wallCls)){Athena::HandleDoor(target);return;}
            }
        }
        if (isPC || isPawn || isInteraction) {
            Guard g;
            if (n == "ServerExecuteInventoryItem") {
                if (Read<uint16_t>(func, profile::FunctionParamSize) >= (int)sizeof(Guid))
                    Athena::OnServerExecuteInventoryItem(Read<Guid>(params));
            }
            else if (n == "ServerExecuteInventoryWeapon") Athena::OnServerExecuteInventoryWeapon(Read<void*>(params));
            else if (n == "ServerHandlePickupInfo" || n == "ServerHandlePickup") Athena::ServerHandlePickup(Read<void*>(params));
            else if (n == "ServerAttemptAircraftJump" && !session.aircraft_jumped) Athena::HandleJumpFromHudStart();
            else if (n == "ServerCreateBuildingActor" && params) Athena::OnServerCreateBuildingActor(params);
            else if (n == "ServerBeginEditingBuildingActor" && params) Athena::OnBeginEditActor(Read<void*>(params, 0));
            else if (n == "ServerEditBuildingActor" && params) {
                struct P { void* BA; void* NC; int RI; bool M; };
                auto* p = (P*)params;
                Athena::OnFinishEditActor(p->BA, p->NC, p->RI, p->M);
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
    if (g_gameTid.compare_exchange_strong(expected, (int)gettid())) LOG("WorldTick first: tid=%d", (int)gettid());
    if (g_gameTid.load() != (int)gettid()) return;
    Guard g;
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
    static double lastTry = 0;
    if (!session.attempted && lastTry != 0 && now - lastTry < 1.0) return;
    lastTry = now;
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

static void InstallHooks() {
    LOG("=== InstallHooks ===");
    if (DobbyHook((void*)(g_base + profile::ProcessEvent), (void*)pe_hook, (void**)&orig_PE) != 0 || !orig_PE) { LOG("PE FAILED"); return; }
    LOG("PE hooked");
    if (DobbyHook((void*)(g_base + profile::WorldTick), (void*)tick_hook, (void**)&orig_Tick) != 0 || !orig_Tick) LOG("Tick FAILED");
    else LOG("Tick hooked");
    if (DobbyHook((void*)(g_base + profile::EngineExec), (void*)exec_hook, (void**)&orig_Exec) != 0 || !orig_Exec) LOG("Exec FAILED");
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
    LOG("=== EraCH3 starting (v13) ===");
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
