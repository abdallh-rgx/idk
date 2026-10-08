// ============================================================================
// EraCH3 — Fortnite 21.30 CL-21088273 Android
// COMPLETE FINAL — All fixes: Loot, QuickBars, Abilities, Movement, Doors
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
// PROFILE — All offsets for Fortnite 21.30
// ============================================================================
namespace profile {
// Core engine
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

// Object layout
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
constexpr unsigned BoolSize       = 0x80;
constexpr unsigned BoolByteOffset = 0x81;
constexpr unsigned BoolFieldMask  = 0x82;
constexpr unsigned FFieldName = 0x28;
constexpr unsigned FFieldNext = 0x20;

// CPF flags
constexpr uint64_t CPF_Parm       = 0x00000080ULL;
constexpr uint64_t CPF_OutParm    = 0x00000100ULL;
constexpr uint64_t CPF_ConstParm  = 0x00000200ULL;
constexpr uint64_t CPF_ReturnParm = 0x00000400ULL;

// Item entry offsets
constexpr unsigned ItemEntry_Durability    = 0x24;
constexpr unsigned ItemEntry_LoadedAmmo    = 0x2C;
constexpr unsigned ItemEntry_PhantomAmmo   = 0x30;
constexpr unsigned ItemEntry_TrackerGuid   = 0x78;
constexpr unsigned ItemEntry_StateValues   = 0xD0;

// Fortnite specific
constexpr unsigned FortPlayerState_HeroType          = 0x0430;
constexpr unsigned FortPlayerController_MyPlayerInfo = 0x2088;
constexpr unsigned RegisteredPlayerInfo_HeroInstance = 0x2D0;
constexpr unsigned FortHero_CharacterParts           = 0x238;
constexpr unsigned FortPlayerPawn_bIsSkydiving       = 0x1A1D;
constexpr unsigned BuildingWall_bDoorOpen_Byte       = 0x0FDE;
constexpr unsigned BuildingWall_bDoorOpen_Bit        = 5;
constexpr unsigned PlayerPawn_Athena_Speed_Sprint    = 0x53EC;

// ⭐ Movement offsets (from SDK)
constexpr unsigned FortPawn_bWantsToSlide            = 0x1030;
constexpr unsigned FortPlayerPawn_bIsSliding_Byte    = 0x06DB;
constexpr unsigned FortPlayerPawn_bIsSliding_Bit     = 3;
constexpr unsigned FortPlayerPawn_bIsSurfaceSwim_Byte = 0x0770;
constexpr unsigned FortPlayerPawn_bIsSurfaceSwim_Bit  = 4;
constexpr unsigned CharacterMovement_MovementMode    = 0x019C;
constexpr unsigned CharacterMovement_CustomMode      = 0x019D;

// TacticalSprint
constexpr unsigned TacticalSprint_Flags              = 0xC0;
constexpr uint8_t TacticalSprint_Bit_Enabled   = 0x01;
constexpr uint8_t TacticalSprint_Bit_WantsSprint = 0x02;
constexpr uint8_t TacticalSprint_Bit_IsSprinting = 0x08;

// ⭐ QuickBars (21.30 — libUnreal.so)
constexpr uintptr_t InitializeAthenaQuickBars    = 0x056607ec;
constexpr uintptr_t FortQuickBarsAthena_vtable   = 0x0BDDF4B0;
constexpr unsigned  QuickBars_VtableSlot         = 0x738;
constexpr unsigned  QuickBars_PrimaryOffset      = 0x278;
constexpr unsigned  QuickBars_SecondaryOffset    = 0x308;
constexpr unsigned  QuickBars_CreativeOffset     = 0x398;
constexpr unsigned  Controller_ClientQuickBars     = 0x1a60;
constexpr unsigned  Controller_QuickBarReadyByte   = 0x1a68;
constexpr uintptr_t GetItemInQuickbarSlot_Gate     = 0x06673524;
constexpr unsigned  QuickBars_VtableSlot_GetItemInSlot = 0x6f0;
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
// UE5 STRUCTS
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
    bool  melee_attempted = false;
    bool  cosmetics_applied = false;
    bool  profile_attempted = false;
    bool  side_systems_initialized = false;
    bool  stamina_initialized = false;
    bool  tactical_sprint_setup = false;
    bool  quickbars_initialized = false;
    bool  movement_enabled = false;
    bool  mcp_synced = false;
    bool  profile_wrap_applied = false;
    bool  bots_spawned = false;
    bool  solo_hud_ready = false;
    bool  storm_protected = false;
    bool  bus_scheduled = false;
    bool  bus_started = false;
    int   bots_target = 99;
    int   bots_spawned_count = 0;
    double bus_at = 0;
    double storm_at = 0;
    std::vector<void*> bot_controllers;
    std::vector<void*> bot_pawns;
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
    void ShowSkin();
    void LoadProfileCosmetics();
    void InitializeSideSystems();
    void SyncMCPProfile();
    void ApplyProfileWrap(Guid guid);
    void SpawnSoloBotsBatch(int maxCount = 10);
    void UpdateSoloHUD();
    void ProtectLocalPlayerFromStorm(bool protect);
    void StartBusNow();
    void SetupLocalVehicles();
    void SetupLocalAI();
    void SetupLocalVending();
    void SetupLocalPlayerCounter();
    void SetupLocalMinimap();
    void SetupLocalClamber();
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
    void OnServerExecuteInventoryWeapon(void* weapon);
    void HandleJumpFromHudStart();
    void SpawnBattleBus();
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

    // QuickBars
    void InitializeQuickBarsNative();
    void SetQuickBarReady();
    void ActivateQuickBarSlot(uint8_t bar, int slot);
    void ForceUpdateQuickBars();
    void BuildQuickBarsManually();

    // Loot
    void SpawnFloorLoot();

    // Movement
    void EnableMovementFeatures();

    struct LootData { void* Def; int Count; };
    std::vector<LootData> PickLootDrops(std::string category);
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

void Athena::GrantAbility(void* klass) {
    if (!klass || !session.pawn) return;
    void* asc = GetASC(session.pawn);
    if (!asc) { LOG("[Ability] no ASC"); return; }
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
    LOG("[Abilities] ✅ Applied set: %s", ObjName(abilitySet).c_str());
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

    // ⭐ أضف الـ abilities الإضافية للـ set قبل Equip
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

    // Fallback direct abilities
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
    LOG("[Abilities] Done — sets=%zu", session.granted_ability_sets.size());

    // ⭐ أعطِ melee ability
    GrantMeleeAbility();
}

void Athena::GrantDefaultAbilities() { GrantAllAbilitySets(); }

// ============================================================================
// MELEE ABILITY
// ============================================================================
void Athena::GrantMeleeAbility() {
    if (!session.pawn || session.melee_granted) return;
    void* asc = GetASC(session.pawn);
    if (!asc) return;

    const char* meleePaths[] = {
        "/Game/Abilities/Weapons/Melee/GAB_Melee_GenericCombo.GAB_Melee_GenericCombo_C",
        "/Game/Abilities/Weapons/Melee/GAB_Melee_Generic_EventGraph.GAB_Melee_Generic_EventGraph_C",
        "/Game/Abilities/Weapons/Melee/GAB_Melee_Generic.GAB_Melee_Generic_C",
        nullptr
    };

    void* meleeCls = nullptr;
    for (int i = 0; meleePaths[i]; ++i) {
        meleeCls = LoadClass(meleePaths[i], "/Script/GameplayAbilities.GameplayAbility");
        if (meleeCls) { LOG("[Melee] Loaded: %s", meleePaths[i]); break; }
    }
    if (!meleeCls) { LOG("[Melee] ❌ No melee ability class found"); return; }
    Call give("/Script/GameplayAbilities.AbilitySystemComponent.K2_GiveAbility");
    if (!give.ok()) {
        LOG("[Melee] K2_GiveAbility not found, trying GiveAbility");
        Call give2("/Script/GameplayAbilities.AbilitySystemComponent.GiveAbility");
        if (give2.ok()) {
            int level = 1;
            give2.set("AbilityClass", meleeCls);
            give2.set("Level", level);
            if (give2.run(asc)) {
                LOG("[Melee] ✅ Granted via GiveAbility");
                session.melee_granted = true;
                session.melee_attempted = true;
            }
        }
        return;
    }

    int level = 1;
    int inputID = -1;
    give.set("AbilityClass", meleeCls);
    give.set("Level", level);
    give.set("InputID", inputID);
    if (give.run(asc)) {
        LOG("[Melee] ✅ Granted via K2_GiveAbility");
        session.melee_granted = true;
        session.melee_attempted = true;
    }
}

void Athena::RefreshHUD() {
    if (!session.controller || !session.world) return;

    Call spawn("/Script/FortniteGame.FortPlayerControllerZone.ClientOnPawnSpawned");
    if (spawn.ok()) spawn.run(session.controller);

    Call worldInv("/Script/FortniteGame.FortPlayerController.ClientForceWorldInventoryUpdate");
    if (worldInv.ok()) worldInv.run(session.controller);

    Call inv("/Script/FortniteGame.FortInventory.HandleInventoryLocalUpdate");
    if (inv.ok() && session.inventory) inv.run(session.inventory);

    Call local("/Script/FortniteGame.FortPlayerController.HandleWorldInventoryLocalUpdate");
    if (local.ok()) local.run(session.controller);

    for (uint8_t bar : {uint8_t(0), uint8_t(1)}) {
        Call qb("/Script/FortniteGame.FortPlayerController.ClientForceUpdateQuickbar");
        if (qb.ok() && qb.set("QuickbarToRefresh", bar)) qb.run(session.controller);
    }

    Call hudInit("/Script/Engine.GameModeBase.InitializeHUDForPlayer");
    if (hudInit.ok() && session.mode && hudInit.set("NewPlayer", session.controller)) {
        hudInit.run(session.mode);
        LOG("[HUD] InitializeHUDForPlayer dispatched");
    }

    Call getHud("/Script/Engine.PlayerController.GetHUD");
    if (getHud.ok() && getHud.run(session.controller)) {
        void* hud = getHud.result<void*>();
        if (hud) {
            Call show("/Script/Engine.HUD.ShowHUD");
            if (show.ok()) show.run(hud);
            LOG("[HUD] HUD=%p shown", hud);
        }
    }

    void* asc = GetASC(session.pawn);
    if (asc) {
        Call abilities("/Script/GameplayAbilities.AbilitySystemComponent.OnRep_ActivateAbilities");
        if (abilities.ok()) abilities.run(asc);
    }

    void* lib = FindObj("/Script/BlueprintContext.Default__BlueprintContextLibrary");
    void* type = FindObj("/Script/FortniteUI.FortHUDContext");
    if (lib && type) {
        Call get("/Script/BlueprintContext.BlueprintContextLibrary.GetContext");
        if (get.ok() && get.set("ContextObject", session.controller) && get.set("Class", type) && get.run(lib)) {
            void* ctx = get.result<void*>();
            if (ctx) {
                Call pawn("/Script/FortniteUI.FortHUDContext.HandleLocalPawnSpawned");
                if (pawn.ok()) pawn.run(ctx);
                Call build("/Script/FortniteUI.FortHUDContext.HandleLocalPlayerBuildCostChanged");
                if (build.ok()) build.run(ctx);
                Call ind("/Script/FortniteUI.FortHUDContext.SetIndicatorsAllowed");
                if (ind.ok()) { bool yes=true; ind.set("bIndicatorsAllowed",yes); ind.run(ctx); }
            }
        }
    }
}

// ============================================================================
// QuickBars
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
    Array p_before = Read<Array>(primary, 0x10);
    Array s_before = Read<Array>(secondary, 0x10);
    LOG("[QB] Before: Primary=%d Secondary=%d", p_before.Count, s_before.Count);
    { Guard g; reinterpret_cast<void(*)(void*)>(init_slot)(session.quickbars); }
    Array p_after = Read<Array>(primary, 0x10);
    Array s_after = Read<Array>(secondary, 0x10);
    LOG("[QB] After: Primary=%d Secondary=%d", p_after.Count, s_after.Count);
    if (p_after.Count >= 1 && s_after.Count >= 1) { LOG("[QB] ✅ Init succeeded"); session.quickbars_initialized = true; }
    else LOG("[QB] ⚠️ Init may have failed");
}

void Athena::BuildQuickBarsManually() {
    if (!session.quickbars || !session.inventory) { LOG("[QB] No quickbars/inventory"); return; }
    LOG("[QB] Building QuickBars from reflected 21.30 layout...");

    void* primProp=GetProp("/Script/FortniteGame.FortQuickBars.PrimaryQuickBar");
    void* secProp=GetProp("/Script/FortniteGame.FortQuickBars.SecondaryQuickBar");
    void* slotsProp=GetProp("/Script/FortniteGame.QuickBar.Slots");
    void* itemsProp=GetProp("/Script/FortniteGame.QuickBarSlot.Items");
    void* enabledProp=GetProp("/Script/FortniteGame.QuickBarSlot.bEnabled");
    if(!primProp || !slotsProp || !itemsProp || !enabledProp) {
        LOG("[QB] reflected quickbar properties unavailable"); return;
    }

    const int reflectedSlotSize=Read<int>(slotsProp,profile::PropElementSize);
    const int reflectedGuidSize=Read<int>(itemsProp,profile::PropElementSize);
    const int slotSize=0x28;
    const int guidSize=0x10;
    LOG("[QB] reflected slotSize=0x%X guidSize=0x%X; SDK slotSize=0x28 guidSize=0x10",reflectedSlotSize,reflectedGuidSize);
    if(reflectedGuidSize!=0x10 && reflectedGuidSize!=0) {
        LOG("[QB] GUID layout mismatch; refusing QuickBar construction"); return;
    }
    if(reflectedSlotSize!=0x28)
        LOG("[QB] reflected slot size is metadata=0x%X; using verified FQuickBarSlot size 0x28",reflectedSlotSize);

    void* invProp=GetProp("/Script/FortniteGame.FortInventory.Inventory");
    void* instProp=GetProp("/Script/FortniteGame.FortItemList.ItemInstances");
    if(!invProp || !instProp) return;
    void* list=GetField(session.inventory,invProp);
    void* instField=list?GetField(list,instProp):nullptr;
    Array arr=instField?Read<Array>(instField):Array{};

    auto setupBar=[&](void* barProp)->bool {
        if(!barProp) return false;
        void* bar=GetField(session.quickbars,barProp);
        if(!bar) return false;
        void* sf=GetField(bar,slotsProp);
        if(!sf) return false;
        Array slots=Read<Array>(sf);
        if(slots.Count<6) {
            void* data=calloc(6,(size_t)slotSize);
            if(!data) return false;
            Write<void*>(sf,0,data); Write<int32>(sf,8,6); Write<int32>(sf,12,6);
            slots=Read<Array>(sf);
        }
        return slots.Data && slots.Count>=6 && slots.Count<=64;
    };

    if(!setupBar(primProp)) { LOG("[QB] primary slot array allocation failed"); return; }
    if(secProp) setupBar(secProp);

    // Bind actual item GUIDs through the reflected quickbar API.  This also
    // preserves FQuickBarSlot's occupied/dirty bookkeeping.
    int bound=0;
    for(int i=0;i<arr.Count && i<6;++i) {
        void* item=Read<void*>(arr.Data,(size_t)i*8);
        if(!item) continue;
        Call defCall("/Script/FortniteGame.FortItem.GetItemDefinitionBP");
        void* def=nullptr;
        if(defCall.ok() && defCall.run(item)) def=defCall.result<void*>();
        if(!def) continue;
        Call bind("/Script/FortniteGame.FortKismetLibrary.EquipItemToQuickBarSlot");
        if(bind.ok()) {
            uint8_t bar=0; int slot=i;
            bool dispatched=bind.set("WorldContextObject",session.world) &&
                bind.set("ItemDefinition",def) && bind.set("QuickBarType",bar) &&
                bind.set("SlotIndex",slot) && bind.run(FindObj("/Script/FortniteGame.Default__FortKismetLibrary"));
            if(dispatched) ++bound;
        }
    }

    // Ensure the six slot records are physically populated even if the Kismet
    // binding accepts the call but does not materialize the local record.
    void* bar=GetField(session.quickbars,primProp);
    void* sf=bar?GetField(bar,slotsProp):nullptr;
    Array slots=sf?Read<Array>(sf):Array{};
    for(int i=0;i<6 && i<arr.Count && slots.Data;++i) {
        void* item=Read<void*>(arr.Data,(size_t)i*8); if(!item) continue;
        Guid guid{}; Call g("/Script/FortniteGame.FortItem.GetItemGuid");
        if(!g.ok() || !g.run(item)) continue; guid=g.result<Guid>();
        void* slot=(char*)slots.Data+(size_t)i*slotSize;
        void* ia=GetField(slot,itemsProp);
        Array values=ia?Read<Array>(ia):Array{};
        bool found=false;
        for(int j=0;j<values.Count;++j) if(values.Data && !memcmp((char*)values.Data+(size_t)j*guidSize,&guid,16)) found=true;
        if(!found && ia) Fn<int(*)(void*,void*,const void*)>(profile::GenericArrayAdd)(ia,itemsProp,&guid);
        SetBool(slot,"/Script/FortniteGame.QuickBarSlot.bEnabled",true);
    }

    session.quickbars_initialized=true;
    LOG("[QB] Manual initialization complete bound=%d",bound);
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
// MOVEMENT FEATURES
// ============================================================================
void Athena::EnableMovementFeatures() {
    if (!session.pawn) return;
    LOG("[Movement] Enabling slide & swim...");

    // ⭐ SLIDE: SetCanSlide(true)
    Call canSlide("/Script/FortniteGame.FortPlayerPawn.SetCanSlide");
    if (canSlide.ok()) {
        bool bTrue = true;
        if (canSlide.set("bCanSlide", bTrue)) {
            if (canSlide.run(session.pawn)) {
                LOG("[Movement] ✅ SetCanSlide(true)");
            } else {
                LOG("[Movement] SetCanSlide run failed");
            }
        }
    } else {
        LOG("[Movement] SetCanSlide not found");
    }

    // Fallback: bWantsToSlide مباشرة
    SetBool(session.pawn, "/Script/FortniteGame.FortPawn.bWantsToSlide", true);

    // ⭐ SWIM
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
            if (p) {
                SetBool(movement, swimPaths[i], true);
                LOG("[Movement] ✅ Set %s", swimPaths[i]);
                break;
            }
        }
    }

    // ⭐ Slide Ability
    void* slideAbility = LoadClass(
        "/Game/Abilities/Player/Generic/Traits/DefaultPlayer/GA_Athena_Player_Slide.GA_Athena_Player_Slide_C",
        "/Script/GameplayAbilities.GameplayAbility");
    if (slideAbility) {
        LOG("[Movement] Found slide ability");
        void* asc = GetASC(session.pawn);
        if (asc) {
            Call give("/Script/GameplayAbilities.AbilitySystemComponent.K2_GiveAbility");
            if (give.ok()) {
                int level = 1;
                int inputID = -1;
                if (give.set("AbilityClass", slideAbility) &&
                    give.set("Level", level) &&
                    give.set("InputID", inputID)) {
                    if (give.run(asc)) {
                        LOG("[Movement] ✅ Slide ability granted");
                    }
                }
            }
        }
    } else {
        LOG("[Movement] Slide ability class not found");
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
            add.set("Class", compClass); add.set("bManualAttachment", bManual);
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
// BATTLE BUS
// ============================================================================
void Athena::StartBusNow() {
    if (session.bus_started || !session.state || !session.controller || !session.pawn) {
        LOG("[Bus] Start skipped: invalid state/controller/pawn");
        return;
    }

    Guard g;
    LOG("[Bus] Starting aircraft phase after warmup");

    if (!session.pawn || !session.controller) {
        LOG("[Bus] Pawn/controller unavailable");
        return;
    }

    TeleportToSpawnIsland();

    void* gp = GetProp("/Script/FortniteGame.FortGameStateAthena.GamePhase");
    if (!gp) {
        LOG("[Bus] GamePhase property missing; aborting aircraft transition");
        return;
    }

    void* gpAddr = GetField(session.state, gp);
    if (!gpAddr) {
        LOG("[Bus] GamePhase field missing; aborting aircraft transition");
        return;
    }

    uint8_t oldPhase = Read<uint8_t>(gpAddr);
    uint8_t aircraftPhase = 3;
    Write<uint8_t>(gpAddr, 0, aircraftPhase);

    Call n("/Script/FortniteGame.FortGameStateAthena.OnRep_GamePhase");
    if (n.ok()) {
        n.set("OldGamePhase", oldPhase);
        if (!n.run(session.state)) {
            LOG("[Bus] OnRep_GamePhase failed");
            Write<uint8_t>(gpAddr, 0, oldPhase);
            return;
        }
    } else {
        LOG("[Bus] OnRep_GamePhase missing; restoring GamePhase");
        Write<uint8_t>(gpAddr, 0, oldPhase);
        return;
    }

    session.bus_started = true;
    session.aircraft_jumped = false;
    LOG("[Bus] Aircraft phase started; waiting for jump");
}

void Athena::SpawnBattleBus() {
    if(!session.mode||!session.state||session.bus_scheduled)return;
    LOG("[Bus] Solo: Spawn Island warmup -> bus");
    void* gp=GetProp("/Script/FortniteGame.FortGameStateAthena.GamePhase");
    if(gp){uint8_t phase=2;Write<uint8_t>(GetField(session.state,gp),0,phase);Call n("/Script/FortniteGame.FortGameStateAthena.OnRep_GamePhase");if(n.ok()){uint8_t old=1;n.set("OldGamePhase",old);n.run(session.state);}}
    TeleportToSpawnIsland();
    session.bus_at=Monotonic()+8.0;session.storm_at=Monotonic()+90.0;session.bus_scheduled=true;
    ProtectLocalPlayerFromStorm(true);
}

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
    Call aircraft("/Script/FortniteGame.FortPlayerController.ServerAttemptAircraftJump");
    if (aircraft.ok()) {
        Rotator r{rot.X, rot.Y, rot.Z};
        if (aircraft.set("ClientRotation", r)) aircraft.run(session.controller);
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
// DOORS — FIX: Toggle + ForceNetUpdate
// ============================================================================
void Athena::HandleDoor(void* door) {
    if (!door || IsBadPtr(door)) return;
    static void* wallCls = nullptr;
    if (!wallCls) wallCls = FindObj("/Script/FortniteGame.BuildingWall");
    if (!wallCls || !IsA(door, wallCls)) return;

    uint8_t* byte = (uint8_t*)door + profile::BuildingWall_bDoorOpen_Byte;
    bool wasOpen = ((*byte) >> profile::BuildingWall_bDoorOpen_Bit) & 1;
    if (wasOpen) *byte &= ~(1 << profile::BuildingWall_bDoorOpen_Bit);
    else *byte |= (1 << profile::BuildingWall_bDoorOpen_Bit);

    Call rep("/Script/FortniteGame.BuildingWall.OnRep_bDoorOpen");
    if (rep.ok()) rep.run(door);
    Call netup("/Script/Engine.Actor.ForceNetUpdate");
    if (netup.ok()) netup.run(door);
    LOG("[Door] local toggle: %s -> %s", wasOpen ? "OPEN" : "CLOSED", wasOpen ? "CLOSED" : "OPEN");
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
    if (!source) { LOG("[MCP] loadout property missing - keep local defaults"); return; }
    void* info = GetObjField(session.controller, "/Script/FortniteGame.FortPlayerController.MyPlayerInfo");
    void* idProp = GetProp("/Script/FortniteGame.FortRegisteredPlayerInfo.PlayerID");
    void* ctxType = FindObj("/Script/FortniteGame.FortMcpContext");
    void* lib = FindObj("/Script/BlueprintContext.Default__BlueprintContextLibrary");
    if (!info || !idProp || !ctxType || !lib) { LOG("[MCP] context/profile prerequisites missing - skip"); return; }
    int sourceSize = Read<int>(source, profile::PropElementSize);
    if (sourceSize <= 0 || sourceSize > 2048) { LOG("[MCP] invalid loadout size=%d - skip", sourceSize); return; }
    void* idAddr = GetField(info, idProp);
    int idSize = Read<int>(idProp, profile::PropElementSize);
    if (!idAddr || idSize <= 0 || idSize > 256) { LOG("[MCP] invalid PlayerID - skip"); return; }
    Call context("/Script/BlueprintContext.BlueprintContextLibrary.GetContext");
    if (!context.ok() || !context.set("ContextObject", session.controller) || !context.set("Class", ctxType) || !context.run(lib)) {
        LOG("[MCP] GetContext failed - skip"); return;
    }
    void* ctx = context.result<void*>();
    if (!ctx) { LOG("[MCP] context null - skip"); return; }
    Call get("/Script/FortniteGame.FortMcpContext.GetLoadoutForPlayer");
    if (!get.ok() || !get.set_bytes("MemberUniqueId", idAddr, (size_t)idSize) || !get.run(ctx)) {
        LOG("[MCP] GetLoadoutForPlayer failed - skip"); return;
    }
    const void* value = get.result_data((size_t)sourceSize);
    if (!value) { LOG("[MCP] loadout return unavailable - skip"); return; }
    void* dst = GetField(session.controller, source);
    if (!dst) { LOG("[MCP] destination missing - skip"); return; }
    memcpy(dst, value, (size_t)sourceSize);
    session.mcp_synced = true;
    LOG("[MCP] Profile loadout synchronized (%d bytes)", sourceSize);
}

void Athena::LoadProfileCosmetics() {
    if (!session.pawn || !session.controller || session.cosmetics_applied) return;
    LOG("[Profile] Loading cosmetics...");
    SyncMCPProfile();
    void* source = nullptr;
    for (const char* p : {
        "/Script/FortniteGame.FortPlayerController.CosmeticLoadoutPC",
        "/Script/FortniteGame.FortPlayerControllerAthena.CustomizationLoadout",
        "/Script/FortniteGame.FortPlayerController.CustomizationLoadout"}) {
        source = GetProp(p); if (source) break;
    }
    void* loadout = source ? GetField(session.controller, source) : nullptr;
    void* state = GetObjField(session.controller, "/Script/Engine.Controller.PlayerState");
    if (!loadout || !state) { LOG("[Profile] loadout/state missing - keeping current loadout"); session.cosmetics_applied = false; return; }

    void* character = GetObjField(loadout, "/Script/FortniteGame.FortAthenaLoadout.Character");
    void* hero = GetObjField(character, "/Script/FortniteGame.AthenaCharacterItemDefinition.HeroDefinition");
    void* info = GetObjField(session.controller, "/Script/FortniteGame.FortPlayerController.MyPlayerInfo");
    if (!hero) hero = GetObjField(info, "/Script/FortniteGame.FortRegisteredPlayerInfo.AthenaMenuHeroDef");
    void* menu = GetObjField(info, "/Script/FortniteGame.FortRegisteredPlayerInfo.TempAthenaMenuHeroInstance");
    if (!hero && menu) {
        Call c("/Script/FortniteGame.FortHero.GetHeroTypeBP");
        if (c.ok() && c.run(menu)) hero = c.result<void*>();
    }
    if (hero) {
        void* hp = GetProp("/Script/FortniteGame.FortPlayerState.HeroType");
        if (hp && Read<int>(hp, profile::PropElementSize) == 8) Write<void*>(GetField(state, hp), 0, hero);
    }

    void* dest = GetProp("/Script/FortniteGame.FortPlayerPawn.CosmeticLoadout");
    if (!dest) dest = GetProp("/Script/FortniteGame.FortPlayerPawn.CustomizationLoadout");
    if (source && dest && Read<int>(source, profile::PropElementSize) == Read<int>(dest, profile::PropElementSize))
        memcpy(GetField(session.pawn, dest), loadout, (size_t)Read<int>(dest, profile::PropElementSize));

    std::vector<void*> parts;
    void* heroPartsProp = GetProp("/Script/FortniteGame.FortHero.CharacterParts");
    if (menu && heroPartsProp) {
        Array a = Read<Array>(GetField(menu, heroPartsProp));
        if (a.Count >= 0 && a.Count <= 32 && (!a.Count || a.Data))
            for (int i=0;i<a.Count;i++) { void* part=Read<void*>(a.Data,(size_t)i*8); if(part&&!IsBadPtr(part)) parts.push_back(part); }
    }
    void* backpack = GetObjField(loadout, "/Script/FortniteGame.FortAthenaLoadout.Backpack");
    void* backPartsProp = GetProp("/Script/FortniteGame.AthenaCharacterPartItemDefinition.CharacterParts");
    if (backpack && backPartsProp) {
        Array a = Read<Array>(GetField(backpack, backPartsProp));
        if (a.Count >= 0 && a.Count <= 16 && (!a.Count || a.Data))
            for (int i=0;i<a.Count;i++) { void* part=Read<void*>(a.Data,(size_t)i*8); if(part&&!IsBadPtr(part)) parts.push_back(part); }
    }
    void* partCls = FindObj("/Script/FortniteGame.CustomCharacterPart");
    for (void* part : parts) {
        if (!partCls || !IsA(part, partCls)) continue;
        void* typeProp = GetProp("/Script/FortniteGame.CustomCharacterPart.CharacterPartType");
        if (!typeProp) continue;
        uint8_t type = Read<uint8_t>(GetField(part, typeProp));
        if (type > 6) continue;
        Call choose("/Script/FortniteGame.FortPlayerPawn.ServerChoosePart");
        if (choose.ok() && choose.set("Part", type) && choose.set("ChosenCharacterPart", part) && choose.run(session.pawn))
            session.character_parts.push_back(part);
    }
    if (!parts.empty()) {
        Array arr{parts.data(), (int32)parts.size(), (int32)parts.size()};
        Call apply("/Script/FortniteGame.FortKismetLibrary.ApplyCharacterCosmetics");
        void* klib=FindObj("/Script/FortniteGame.Default__FortKismetLibrary");
        if (apply.ok() && klib && apply.set("WorldContextObject",session.world) && apply.set("CharacterParts",arr) && apply.set("PlayerState",state)) apply.run(klib);
    }
    Call r1("/Script/FortniteGame.FortPlayerState.OnRep_HeroType"); if(r1.ok())r1.run(state);
    Call r2("/Script/FortniteGame.FortPlayerState.OnRep_CharacterData"); if(r2.ok())r2.run(state);
    void* klib=FindObj("/Script/FortniteGame.Default__FortKismetLibrary");
    if(klib){Call u("/Script/FortniteGame.FortKismetLibrary.UpdatePlayerCustomCharacterPartsVisualization");if(u.ok()&&u.set("PlayerState",state))u.run(klib);}
    Call cl("/Script/FortniteGame.FortPlayerPawn.OnRep_CosmeticLoadout"); if(cl.ok())cl.run(session.pawn);
    session.cosmetics_applied = !parts.empty() || hero != nullptr;
    if (!session.cosmetics_applied) { LOG("[Profile] MCP had no usable cosmetics - keeping current loadout"); return; }
    LOG("[Profile] MCP cosmetics applied: parts=%d hero=%p backpack=%p",(int)parts.size(),hero,backpack);
}

void Athena::ShowSkin() {
    if (session.cosmetics_applied || session.profile_attempted) { LOG("[Profile] Cosmetics already processed - skip"); return; }
    session.profile_attempted = true;
    LoadProfileCosmetics();
}

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
    session.visibility_forced = true;
}

void Athena::UpdateSoloHUD() {
    if(!session.state||!session.controller)return;
    void* playersProp=GetProp("/Script/FortniteGame.FortGameStateAthena.PlayersLeft");
    void* totalProp=GetProp("/Script/FortniteGame.FortGameStateAthena.TotalPlayers");
    if(playersProp)Write<int>(GetField(session.state,playersProp),0,std::max(1,1+session.bots_spawned_count));
    if(totalProp)Write<int>(GetField(session.state,totalProp),0,std::max(1,1+session.bots_target));
    Call rep("/Script/FortniteGame.FortGameStateAthena.OnRep_PlayersLeft");if(rep.ok())rep.run(session.state);
    Call pc("/Script/FortniteGame.FortPlayerControllerAthena.HandlePlayersLeftChanged");if(pc.ok())pc.run(session.controller);
    void* wb=FindObj("/Script/UMG.Default__WidgetBlueprintLibrary");
    if(!wb||!session.world)return;
    for(const char* path:{"/Script/FortniteUI.AthenaMapPlayersLeftWidget","/Script/FortniteUI.AthenaPlayersLeftBase","/Script/FortniteUI.AthenaPlayersLeftWidget.AthenaPlayersLeftWidget_C"}){
        void* cls=FindObj(path);if(!cls)continue;
        Call all("/Script/UMG.WidgetBlueprintLibrary.GetAllWidgetsOfClass");
        if(!all.ok()||!all.set("WorldContextObject",session.world)||!all.set("WidgetClass",cls)||!all.run(wb))continue;
        Array a=all.result<Array>();if(a.Count<0||a.Count>256||(a.Count&&!a.Data))continue;
        for(int i=0;i<a.Count;i++){void*w=Read<void*>(a.Data,(size_t)i*8);if(!w||IsBadPtr(w))continue;
            Call h("/Script/FortniteUI.AthenaMapPlayersLeftWidget.HandlePlayersLeftChanged");if(h.ok())h.run(w);
            Call u("/Script/FortniteUI.AthenaPlayersLeftBase.SetPlayersLeftText");if(u.ok()){int n=1+session.bots_spawned_count;if(u.set("PlayersLeft",n))u.run(w);}
            Call vis("/Script/UMG.Widget.SetVisibility");if(vis.ok()){uint8_t v=0;if(vis.set("InVisibility",v))vis.run(w);}
        }
    }
    session.solo_hud_ready=true;
    LOG("[PlayerCounter] players=%d total=%d",1+session.bots_spawned_count,1+session.bots_target);
}

void Athena::SetupLocalPlayerCounter() { UpdateSoloHUD(); }

void Athena::SetupLocalMinimap() {
    if(!session.world){LOG("[Minimap] world missing - skip");return;}
    void* wb=FindObj("/Script/UMG.Default__WidgetBlueprintLibrary");
    void* cls=FindObj("/Script/AthenaMinimapContainer.AthenaMinimapContainer_C");
    if(!cls)cls=FindObj("/Script/FortniteUI.AthenaMinimapContainer");
    if(!wb||!cls){LOG("[Minimap] class/library missing - skip");return;}
    Call all("/Script/UMG.WidgetBlueprintLibrary.GetAllWidgetsOfClass");
    if(!all.ok()||!all.set("WorldContextObject",session.world)||!all.set("WidgetClass",cls)||!all.run(wb)){LOG("[Minimap] enumeration failed - skip");return;}
    Array a=all.result<Array>();if(a.Count<0||a.Count>256||(a.Count&&!a.Data)){LOG("[Minimap] invalid widgets - skip");return;}
    int shown=0;
    for(int i=0;i<a.Count;i++){void*w=Read<void*>(a.Data,(size_t)i*8);if(!w||IsBadPtr(w))continue;
        Call vis("/Script/UMG.Widget.SetVisibility");if(vis.ok()){uint8_t visible=0;if(vis.set("InVisibility",visible)&&vis.run(w))shown++;}
        Call ev("/Script/FortniteUI.FortHUDElementWidget.OnVisibilitySetEvent");if(ev.ok())ev.run(w);
    }
    LOG("[Minimap] visible=%d",shown);
}

void Athena::SetupLocalClamber() {
    if(!session.pawn){LOG("[Climb] pawn missing - skip");return;}
    void* cls=FindObj("/Script/ClamberingCodeRuntime.ClamberingComponent");if(!cls){LOG("[Climb] component class missing - skip");return;}
    Call get("/Script/Engine.Actor.GetComponentByClass");if(!get.ok()||!get.set("ComponentClass",cls)||!get.run(session.pawn)){LOG("[Climb] component lookup failed - skip");return;}
    void* comp=get.result<void*>();if(!comp||IsBadPtr(comp)){LOG("[Climb] component absent - skip");return;}
    Call en("/Script/ClamberingCodeRuntime.ClamberingComponent.IsClamberingEnabled");
    Call au("/Script/ClamberingCodeRuntime.ClamberingComponent.IsAutoClamberingEnabled");
    bool e=false,a=false;if(en.ok()&&en.run(comp))e=en.result<bool>();if(au.ok()&&au.run(comp))a=au.result<bool>();
    LOG("[Climb] component=%p enabled=%d auto=%d",comp,(int)e,(int)a);
}

void Athena::SetupLocalVending() {
    if (!session.world || !session.pawn) { LOG("[Vending] prerequisites missing - skip"); return; }
    void* cls = FindObj("/Script/FortniteGame.BuildingProp_VendingMachine");
    void* wb = FindObj("/Script/Engine.Default__GameplayStatics");
    if (!cls || !wb) { LOG("[Vending] class/library missing - skip"); return; }
    Call all("/Script/Engine.GameplayStatics.GetAllActorsOfClass");
    if (!all.ok() || !all.set("WorldContextObject", session.world) || !all.set("ActorClass", cls) || !all.run(wb)) { LOG("[Vending] enumeration failed - skip"); return; }
    Array a = all.result<Array>();
    if (a.Count < 0 || a.Count > 4096 || (a.Count && !a.Data)) { LOG("[Vending] invalid actor array - skip"); return; }
    int count = 0;
    for (int i=0;i<a.Count;i++) {
        void* m=Read<void*>(a.Data,(size_t)i*8); if(!m||IsBadPtr(m)) continue;
        Call inv("/Script/FortniteGame.BuildingProp_VendingMachine.OnPlayerInventoryChange");
        if(inv.ok() && inv.run(m)) count++;
    }
    LOG("[Vending] refreshed %d machines", count);
}

void Athena::SetupLocalVehicles() {
    if (!session.world) { LOG("[Vehicles] world missing - skip"); return; }
    void* spawnerCls = FindObj("/Script/FortniteGame.FortAthenaVehicleSpawner");
    if (!spawnerCls) { LOG("[Vehicles] spawner class missing - skip"); return; }
    void* wb = FindObj("/Script/Engine.Default__GameplayStatics");
    if (!wb) { LOG("[Vehicles] GameplayStatics missing - skip"); return; }
    Call all("/Script/Engine.GameplayStatics.GetAllActorsOfClass");
    if (!all.ok() || !all.set("WorldContextObject",session.world) || !all.set("ActorClass",spawnerCls) || !all.run(wb)) { LOG("[Vehicles] spawner enumeration failed - skip"); return; }
    Array a=all.result<Array>();
    if(a.Count<0||a.Count>4096||(a.Count&&!a.Data)){LOG("[Vehicles] invalid spawner array - skip");return;}
    int spawned=0, skipped=0;
    for(int i=0;i<a.Count;i++){
        void* sp=Read<void*>(a.Data,(size_t)i*8); if(!sp||IsBadPtr(sp)) continue;
        Call get("/Script/FortniteGame.FortAthenaVehicleSpawner.GetVehicleClass");
        if(!get.ok()||!get.run(sp)){skipped++;continue;}
        void* vc=get.result<void*>(); if(!vc||IsBadPtr(vc)){skipped++;continue;}
        Call loc("/Script/Engine.Actor.K2_GetActorLocation"); Call rot("/Script/Engine.Actor.K2_GetActorRotation");
        if(!loc.ok()||!rot.ok()||!loc.run(sp)||!rot.run(sp)){skipped++;continue;}
        Transform t{}; Vector v=loc.result<Vector>(); Rotator r=rot.result<Rotator>();
        t.Location[0]=v.X; t.Location[1]=v.Y; t.Location[2]=v.Z; t.Scale[0]=t.Scale[1]=t.Scale[2]=1.0;
        void* vehicle=SpawnActor(session.world,vc,sp,t);
        if(!vehicle){skipped++;continue;}
        Call fuel("/Script/FortniteGame.FortDagwoodVehicle.SetFuel");
        if(fuel.ok()){float f=100.f;fuel.set("Fuel",f);fuel.run(vehicle);}
        Call wake("/Script/FortniteGame.FortAthenaVehicle.BreakSpawnedSleepLock");if(wake.ok())wake.run(vehicle);
        Call mat("/Script/FortniteGame.FortAthenaVehicle.ApplyMaterialAndUseWrapIfPossible");if(mat.ok())mat.run(vehicle);
        spawned++;
    }
    LOG("[Vehicles] spawned=%d skipped=%d",spawned,skipped);
}

void Athena::SetupLocalAI() {
    if(!session.mode||IsBadPtr(session.mode)){LOG("[NPC] game mode missing - skip");return;}
    void* director=GetObjField(session.mode,"/Script/FortniteGame.FortGameModeAthena.AIDirector");
    if(!director||IsBadPtr(director)){
        void* cls=FindObj("/Game/AIDirector/AIDirector_Fortnite.AIDirector_Fortnite_C");
        if(!cls) cls=FindObj("/Script/FortniteGame.AthenaAIDirector");
        if(!cls) cls=FindObj("/Script/FortniteGame.FortAIDirector");
        if(cls){Transform t{};director=SpawnActor(session.world,cls,session.mode,t);if(director){void* prop=GetProp("/Script/FortniteGame.FortGameModeAthena.AIDirector");if(prop)Write<void*>(GetField(session.mode,prop),0,director);}}
    }
    if(!director){LOG("[NPC] AIDirector unavailable - skip");return;}
    Call act("/Script/FortniteGame.AIDirector.Activate");
    if(act.ok()&&!act.run(director))LOG("[NPC] AIDirector.Activate failed"); else LOG("[NPC] AIDirector active");
    void* goal=GetObjField(session.mode,"/Script/FortniteGame.FortGameModeAthena.AIGoalManager");
    if(!goal){void* cls=FindObj("/Game/AI/GoalSelection/AIGoalManager.AIGoalManager_C");if(!cls)cls=FindObj("/Script/FortniteGame.FortAIGoalManager");if(cls){Transform t{};goal=SpawnActor(session.world,cls,session.mode,t);}}
    if(goal)LOG("[NPC] AIGoalManager active"); else LOG("[NPC] AIGoalManager unavailable - skip");
}

void Athena::SpawnSoloBotsBatch(int maxCount) {
    if(!session.world||!session.mode||!session.state||session.bots_spawned_count>=session.bots_target)return;
    void* ctrlCls=FindObj("/Script/FortniteAI.FortAthenaAIBotController");
    if(!ctrlCls)ctrlCls=FindObj("/Script/FortniteAI.FortBotController");
    void* pawnCls=FindObj("/Game/Athena/PlayerPawn_Athena.PlayerPawn_Athena_C");
    void* psCls=FindObj("/Script/FortniteGame.FortPlayerStateAthena");
    if(!ctrlCls||!pawnCls||!psCls){LOG("[NPC] bot classes missing - skip");session.bots_spawned=true;return;}
    void* aliveProp=GetProp("/Script/FortniteGame.FortGameModeAthena.AlivePlayers");
    int made=0;
    for(int k=0;k<maxCount&&session.bots_spawned_count<session.bots_target;k++){
        Transform t{};
        double ang=(session.bots_spawned_count*0.6180339887)*6.28318530718;
        double rad=1500.0+double(session.bots_spawned_count%10)*450.0;
        Call loc("/Script/Engine.Actor.K2_GetActorLocation");Vector base{0,0,1000};
        if(loc.ok()&&session.pawn&&loc.run(session.pawn))base=loc.result<Vector>();
        t.Location[0]=base.X+cos(ang)*rad;t.Location[1]=base.Y+sin(ang)*rad;t.Location[2]=base.Z+100.0;
        void* bc=SpawnActor(session.world,ctrlCls,session.mode,t);if(!bc)continue;
        void* bp=SpawnActor(session.world,pawnCls,bc,t);if(!bp){Call d("/Script/Engine.Actor.K2_DestroyActor");if(d.ok())d.run(bc);continue;}
        void* ps=SpawnActor(session.world,psCls,bc,t);if(!ps){Call d("/Script/Engine.Actor.K2_DestroyActor");if(d.ok())d.run(bp);if(d.ok())d.run(bc);continue;}
        void* cPs=GetProp("/Script/Engine.Controller.PlayerState");if(cPs)Write<void*>(GetField(bc,cPs),0,ps);
        void* pPs=GetProp("/Script/Engine.Pawn.PlayerState");if(pPs)Write<void*>(GetField(bp,pPs),0,ps);
        SetBool(ps,"/Script/FortniteGame.FortPlayerStateAthena.bIsABot",true);
        void* team=GetProp("/Script/FortniteGame.FortPlayerStateAthena.TeamIndex");if(team)Write<uint8_t>(GetField(ps,team),0,(uint8_t)((session.bots_spawned_count%100)+1));
        void* squad=GetProp("/Script/FortniteGame.FortPlayerStateAthena.SquadId");if(squad)Write<uint8_t>(GetField(ps,squad),0,(uint8_t)(session.bots_spawned_count%16));
        Call opr("/Script/FortniteGame.FortPlayerState.OnRep_PlayerState");if(opr.ok())opr.run(ps);
        Possess(bc,bp);
        Call ready("/Script/FortniteAI.FortAthenaAIBotController.OnGamePhaseChanged");if(ready.ok())ready.run(bc);
        Call safe("/Script/FortniteAI.FortAthenaAIBotController.OnSafeZoneStateChanged");if(safe.ok())safe.run(bc);
        Call melee("/Script/FortniteAI.FortAthenaAIBotController.EquipMeleeWeapon");if(melee.ok())melee.run(bc);
        Call patrol("/Script/FortniteAI.FortAthenaAIBotController.SetPatrollingEnable");if(patrol.ok()){bool yes=true;patrol.set("bEnabled",yes);patrol.run(bc);}
        if(aliveProp){void* arr=GetField(session.mode,aliveProp);if(arr)Fn<int(*)(void*,void*,const void*)>(profile::GenericArrayAdd)(arr,aliveProp,&bc);}
        session.bot_controllers.push_back(bc);session.bot_pawns.push_back(bp);session.bots_spawned_count++;made++;
    }
    if(session.bots_spawned_count>=session.bots_target)session.bots_spawned=true;
    LOG("[NPC] batch=%d bots=%d/%d",made,session.bots_spawned_count,session.bots_target);
}

void Athena::ProtectLocalPlayerFromStorm(bool protect) {
    if(!session.state)return;
    void* indicator=GetObjField(session.state,"/Script/FortniteGame.FortGameStateAthena.SafeZoneIndicator");
    if(!indicator)return;
    void* r1=GetProp("/Script/FortniteGame.FortSafeZoneIndicator.PreviousRadius");
    void* r2=GetProp("/Script/FortniteGame.FortSafeZoneIndicator.NextRadius");
    void* r3=GetProp("/Script/FortniteGame.FortSafeZoneIndicator.NextNextRadius");
    void* p=GetProp("/Script/FortniteGame.FortSafeZoneIndicator.bPaused");
    float radius=protect?100000000.f:0.f;
    if(protect){if(r1)Write<float>(GetField(indicator,r1),0,radius);if(r2)Write<float>(GetField(indicator,r2),0,radius);if(r3)Write<float>(GetField(indicator,r3),0,radius);}
    if(p)SetBool(indicator,"/Script/FortniteGame.FortSafeZoneIndicator.bPaused",protect);
    session.storm_protected=protect;
    LOG("[Storm] local protection=%d",(int)protect);
}

void Athena::UpdateSoloHUD();

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

bool Athena::TryInitializePlayer(void* world, void* mode, void* controller) {
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
    LOG("SetupWorld: SyncMCPProfile"); SyncMCPProfile();
    LOG("SetupWorld: ShowSkin"); ShowSkin();
    LOG("SetupWorld: DestroyLods"); DestroyLods();
    LOG("SetupWorld: InitializeInventory"); InitializeInventory();
    LOG("SetupWorld: SpawnFloorLoot"); SpawnFloorLoot();
    LOG("SetupWorld: ApplySprintSpeed"); ApplySprintSpeed();
    LOG("SetupWorld: SpawnSafeZoneIndicator"); SpawnSafeZoneIndicator();
    LOG("SetupWorld: SpawnBuildPreviews"); SpawnBuildPreviews();
    LOG("SetupWorld: DropLoadingScreen"); DropLoadingScreen();
    LOG("SetupWorld: scheduling Solo warmup -> aircraft");
    session.bus_scheduled = true;
    session.bus_started = false;
    session.aircraft_jumped = false;
    session.bus_at = Monotonic() + 8.0;
    session.storm_at = Monotonic() + 90.0;
    ProtectLocalPlayerFromStorm(true);
    LOG("[Bus] Warmup stabilized; aircraft deferred by 8 seconds");
    LOG("SetupWorld: RemoveNetDebugUI"); RemoveNetDebugUI();
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
    if (!session.abilities_granted) GrantAllAbilitySets();
    if (!session.tactical_sprint_setup) SetupTacticalSprint();
    if (!session.movement_enabled) EnableMovementFeatures();
    if (!session.profile_attempted) ShowSkin();
    if (!session.quickbars_initialized) {
        InitializeQuickBarsNative();
        if (!session.quickbars_initialized) BuildQuickBarsManually();
        SetQuickBarReady();
        ForceUpdateQuickBars();
        ActivateQuickBarSlot(0, 0);
        ActivateQuickBarSlot(0, 1);
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
    RefreshHUD();
    InitializeSideSystems();
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
    static int qbRetries = 0;
    if (session.finalized && now - lastVisibility > 8.0) { lastVisibility = now; if (!session.visibility_forced) ForcePlayerVisible(); }
    if (session.finalized && session.pawn && now - lastSpeedReapply > 2.0) { lastSpeedReapply = now; ApplySprintSpeed(); }
    if (session.finalized && session.pawn && now - lastAbilityCheck > 10.0) {
        lastAbilityCheck = now;
        if (!session.abilities_granted) { GrantAllAbilitySets(); RefreshHUD(); }
        if (!session.melee_granted) GrantMeleeAbility();
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
    if (!session.profile_attempted) ShowSkin();
    }
    if (session.bus_scheduled && !session.bus_started && now >= session.bus_at) StartBusNow();
    if (session.finalized && !session.bots_spawned) SpawnSoloBotsBatch(10);
    if (session.finalized && session.bots_spawned_count > 0 && now - lastQBCheck > 2.0) UpdateSoloHUD();
    if (session.storm_protected && session.storm_at > 0 && now >= session.storm_at) {
        session.storm_at = 0;
        Call jump("/Script/FortniteGame.FortGameModeAthena.JumpToSafeZonePhase");
        if (jump.ok() && jump.run(session.mode)) { ProtectLocalPlayerFromStorm(false); LOG("[Storm] real safe-zone phases enabled"); }
        else LOG("[Storm] JumpToSafeZonePhase unavailable; keeping protection");
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
        void* bp = GetProp("/Script/FortniteGame.FortGameStateAthena.BasePlaylist");
        void* op = GetProp("/Script/FortniteGame.FortGameStateAthena.OverridePlaylist");
        void* cp = GetProp("/Script/FortniteGame.FortGameStateAthena.CurrentPlaylistData");
        if (bp) Write<void*>(GetField(session.state, bp), 0, playlist);
        if (op) Write<void*>(GetField(session.state, op), 0, playlist);
        if (cp) Write<void*>(GetField(session.state, cp), 0, playlist);
        Call a("/Script/FortniteGame.FortGameStateAthena.OnRep_CurrentPlaylistData"); if (a.ok()) a.run(session.state);
        Call b("/Script/FortniteGame.FortGameStateAthena.OnRep_CurrentPlaylistInfo"); if (b.ok()) b.run(session.state);
    }
    if (!session.ready_called) {
        Call srt("/Script/FortniteGame.FortPlayerController.ServerReadyToStartMatch"); if (srt.ok()) srt.run(session.controller);
        Call cps("/Script/FortniteGame.FortPlayerControllerZone.ClientOnPawnSpawned"); if (cps.ok()) cps.run(session.controller);
        session.ready_called = true;
    }
    LOG("Loading screen dropped");
}

void Athena::InventoryUpdate() {
    if (!session.controller) return;
    {
        Call c("/Script/FortniteGame.FortPlayerController.ClientForceWorldInventoryUpdate");
        if (c.ok()) c.run(session.controller);
    }
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

    // Re-drive the actual gameplay HUD widgets instead of only refreshing GAS.
    void* wb = FindObj("/Script/UMG.Default__WidgetBlueprintLibrary");
    if (wb && session.world) {
        Call all("/Script/UMG.WidgetBlueprintLibrary.GetAllWidgetsOfClass");
        if (all.ok() && all.set("WorldContextObject", session.world)) {
            const char* classes[] = {
                "/Script/AthenaHUDItemList.AthenaHUDItemList_C",
                "/Script/AthenaMinimapContainer.AthenaMinimapContainer_C",
                "/Script/QuickbarBase.QuickbarBase_C",
                nullptr
            };
            for (int k = 0; classes[k]; ++k) {
                void* cls = FindObj(classes[k]);
                if (!cls || !all.set("WidgetClass", cls) || !all.run(wb)) continue;
                Array widgets = all.result<Array>();
                if (widgets.Count < 0 || widgets.Count > 256 || (widgets.Count && !widgets.Data)) continue;
                for (int i = 0; i < widgets.Count; ++i) {
                    void* w = Read<void*>(widgets.Data, (size_t)i * 8);
                    if (!w || IsBadPtr(w)) continue;
                    if (k == 0) {
                        Call c("/Script/AthenaHUDItemList.AthenaHUDItemList_C.OnInventoryUpdated");
                        if (c.ok()) c.run(w);
                    } else if (k == 1) {
                        // ExecuteUbergraph is an internal Blueprint dispatcher; never call it blindly.
                        // Minimap initialization is optional and must not be allowed to crash gameplay.
                        Call vis("/Script/FortniteUI.FortHUDElementWidget.OnVisibilitySetEvent");
                        if (vis.ok()) {
                            LOG("[HUD] Minimap found; skipped unsafe ExecuteUbergraph");
                        } else {
                            LOG("[HUD] Minimap found; no safe visibility callback, skipped");
                        }
                    } else {
                        Call c("/Script/QuickbarBase.QuickbarBase_C.RefreshQuickbar");
                        if (c.ok()) { bool force = true; c.set("ForceItemsChanged", force); c.run(w); }
                    }
                }
            }
        }
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
    { Call c("/Script/FortniteGame.FortItem.SetOwningControllerForTemporaryItem"); if (c.ok() && c.set("InController", session.controller)) c.run(inst); }
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
    if (entrySrc) {
        Write<float>(entrySrc, profile::ItemEntry_Durability, 1.0f);
        void* weapCls = FindObj("/Script/FortniteGame.FortWeaponItemDefinition");
        if (weapCls && IsA(def, weapCls)) {
            Write<int32>(entrySrc, profile::ItemEntry_LoadedAmmo, 30);
            void* phProp = GetProp("/Script/FortniteGame.FortWeaponItemDefinition.bUsesPhantomReserveAmmo");
            if (phProp) Write<int32>(entrySrc, profile::ItemEntry_PhantomAmmo, 300);
        }
    }
    Fn<int(*)(void*,void*,const void*)>(profile::GenericArrayAdd)(instArr, instProp, &inst);
    Fn<int(*)(void*,void*,const void*)>(profile::GenericArrayAdd)(entryArr, entryProp, entrySrc);
    Guid itemGuid{0, 0, 0, 0};
    if (itemGuidProp && entrySrc) {
        itemGuid = Read<Guid>(GetField(entrySrc, itemGuidProp));
        if (!(itemGuid.A | itemGuid.B | itemGuid.C | itemGuid.D)) {
            void* klib = FindObj("/Script/Engine.Default__KismetGuidLibrary");
            if (klib) { Call g("/Script/Engine.KismetGuidLibrary.NewGuid"); if (g.ok() && g.run(klib)) { itemGuid = g.result<Guid>(); Write<Guid>(GetField(entrySrc, itemGuidProp), 0, itemGuid); } }
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
                if (itemsProp) { void* itemsArr = GetField(slotObj, itemsProp); Fn<int(*)(void*,void*,const void*)>(profile::GenericArrayAdd)(itemsArr, itemsProp, &itemGuid); }
                if (enabledProp) {
                    uint8_t byte = Read<uint8_t>(enabledProp, profile::BoolByteOffset);
                    uint8_t mask = Read<uint8_t>(enabledProp, profile::BoolFieldMask);
                    ((uint8_t*)slotObj)[byte] |= mask;
                }
            }
        }
    }
    Call onrep("/Script/FortniteGame.FortQuickBars.OnRep_PrimaryQuickBar"); if (onrep.ok()) onrep.run(session.quickbars);
    Call onrepSec("/Script/FortniteGame.FortQuickBars.OnRep_SecondaryQuickBar"); if (onrepSec.ok()) onrepSec.run(session.quickbars);
    Call hiu("/Script/FortniteGame.FortInventory.HandleInventoryLocalUpdate"); if (hiu.ok()) hiu.run(session.inventory);
    Call hwi("/Script/FortniteGame.FortPlayerController.HandleWorldInventoryLocalUpdate"); if (hwi.ok()) hwi.run(session.controller);
    LOG("Inv+ %s x%d slot=%d", ObjName(def).c_str(), count, slot);
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
            // اكتب Owner مباشرة
            Write<void*>((char*)session.quickbars + 0x138, 0, session.controller);
            Call soQB("/Script/FortniteGame.FortQuickBars.SetOwner");
            if (soQB.ok()) soQB.run(session.quickbars);
            LOG("[QB] Running native initialize...");
            InitializeQuickBarsNative();
        }
    }

    LOG("Inventory ready: inv=%p qb=%p initialized=%d", session.inventory, session.quickbars, session.quickbars_initialized ? 1 : 0);
    if (!session.quickbars_initialized) {
        LOG("[Inv] QuickBars native initialization failed; skipping item population safely");
        session.inventory_ready = false;
        return;
    }

    struct DefSlot { const char* path; int slot; };
    void* profilePickaxe = nullptr;
    if (session.mcp_synced) {
        void* source = nullptr;
        for (const char* p : {"/Script/FortniteGame.FortPlayerController.CosmeticLoadoutPC","/Script/FortniteGame.FortPlayerControllerAthena.CustomizationLoadout","/Script/FortniteGame.FortPlayerController.CustomizationLoadout"}) { source=GetProp(p); if(source)break; }
        if (source) {
            void* loadout=GetField(session.controller,source);
            void* pick=GetObjField(loadout,"/Script/FortniteGame.FortAthenaLoadout.Pickaxe");
            profilePickaxe=GetObjField(pick,"/Script/FortniteGame.AthenaPickaxeItemDefinition.WeaponDefinition");
        }
    }
    DefSlot defaults[] = {
        {nullptr, 0},
        {"/Game/Athena/Items/Weapons/WID_Assault_Auto_Athena_C_Ore_T03.WID_Assault_Auto_Athena_C_Ore_T03", 1},
        {"/Game/Items/Weapons/BuildingTools/BuildingItemData_Wall.BuildingItemData_Wall", 2},
        {"/Game/Items/Weapons/BuildingTools/BuildingItemData_Floor.BuildingItemData_Floor", 3},
        {"/Game/Items/Weapons/BuildingTools/BuildingItemData_Stair_W.BuildingItemData_Stair_W", 4},
        {"/Game/Items/Weapons/BuildingTools/BuildingItemData_RoofS.BuildingItemData_RoofS", 5},
    };
    for (auto& d : defaults) {
        void* def = (d.slot == 0 && profilePickaxe) ? profilePickaxe : (d.path ? LoadAsset(d.path) : nullptr);
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
    void* itemEntryProp = GetProp("/Script/FortniteGame.FortWorldItem.ItemEntry");
    Guid trackerGuid{};
    if (itemEntryProp) { void* entrySrc = GetField(item, itemEntryProp); if (entrySrc) trackerGuid = Read<Guid>(entrySrc, profile::ItemEntry_TrackerGuid); }
    Call eq("/Script/FortniteGame.FortPawn.EquipWeaponDefinition");
    if (!eq.ok()) return;
    bool bDisableAnim = false;
    eq.set("WeaponData", def); eq.set("ItemEntryGuid", guid);
    eq.set("TrackerGuid", trackerGuid); eq.set("bDisableEquipAnimation", bDisableAnim);
    eq.run(session.pawn);
}

// ============================================================================
// FLOOR LOOT
// ============================================================================
void Athena::SpawnFloorLoot() {
    if(!session.world||!session.pawn)return;
    void* wb=FindObj("/Script/Engine.Default__GameplayStatics");
    void* cls=FindObj("/Script/FortniteGame.BuildingContainer");
    if(!wb||!cls){LOG("[Loot] BuildingContainer/GameplayStatics missing - skip");return;}
    Call all("/Script/Engine.GameplayStatics.GetAllActorsOfClass");
    if(!all.ok()||!all.set("WorldContextObject",session.world)||!all.set("ActorClass",cls)||!all.run(wb)){LOG("[Loot] enumeration failed - skip");return;}
    Array a=all.result<Array>(); if(a.Count<0||a.Count>200000||(a.Count&&!a.Data)){LOG("[Loot] invalid container array - skip");return;}
    int floor=0,skipped=0;
    for(int i=0;i<a.Count;i++){
        void* c=Read<void*>(a.Data,(size_t)i*8);if(!c||IsBadPtr(c))continue;
        std::string n=ObjName(Read<void*>(c,profile::ObjectClass));
        std::string low=n;std::transform(low.begin(),low.end(),low.begin(),::tolower);
        if(low.find("floorloot")==std::string::npos && low.find("tiered") == std::string::npos && low.find("loot") == std::string::npos) {skipped++;continue;}
        Call q("/Script/FortniteGame.BuildingContainer.BP_IsAlreadySearched");
        if(q.ok()&&q.run(c)&&q.result<bool>())continue;
        Call spawn("/Script/FortniteGame.BuildingContainer.BP_SpawnLoot");
        if(spawn.ok()&&spawn.set("PlayerPawn",session.pawn)&&spawn.run(c)){floor++;}
    }
    LOG("[Loot] floor candidates=%d skipped=%d total=%d",floor,skipped,a.Count);
}

// ============================================================================
// LOOT — Containers
// ============================================================================
void Athena::Loot(void* actor) {
    if (!actor || !session.pawn || IsBadPtr(actor)) return;
    void* bcCls=FindObj("/Script/FortniteGame.BuildingContainer");
    if(!bcCls || !IsA(actor,bcCls)) return;
    Call already("/Script/FortniteGame.BuildingContainer.BP_IsAlreadySearched");
    if(already.ok() && already.run(actor) && already.result<bool>()) { LOG("[Loot] already searched"); return; }
    LOG("[Loot] Opening %s",ObjName(Read<void*>(actor,profile::ObjectClass)).c_str());
    Call begin("/Script/FortniteGame.BuildingContainer.OnBeginSearch"); if(begin.ok())begin.run(actor);
    Call spawn("/Script/FortniteGame.BuildingContainer.BP_SpawnLoot");
    bool spawned=false;
    if(spawn.ok() && spawn.set("PlayerPawn",session.pawn) && spawn.run(actor)) spawned=spawn.result<bool>();
    Call loot("/Script/FortniteGame.BuildingContainer.OnLoot"); if(loot.ok())loot.run(actor);
    Call mark("/Script/FortniteGame.BuildingContainer.BP_SetAlreadySearched");
    if(mark.ok()){bool yes=true;mark.set("bNewValue",yes);mark.run(actor);} else SetBool(actor,"/Script/FortniteGame.BuildingContainer.bAlreadySearched",true);
    Call rep("/Script/FortniteGame.BuildingContainer.OnRep_bAlreadySearched");if(rep.ok())rep.run(actor);
    Call bounce("/Script/FortniteGame.BuildingContainer.BounceContainer");if(bounce.ok())bounce.run(actor);
    LOG("[Loot] BP result=%d",(int)spawned);
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
        if (memcmp(&cand, &guid, sizeof(Guid)) == 0) { for (int s = 0; s < 6; ++s) if (session.slots[s] == inst) { EquipSlot(s); return; } return; }
    }
}
void Athena::OnServerExecuteInventoryWeapon(void* weapon) {
    if (!weapon || !session.pawn) return;
    void* p = GetProp("/Script/FortniteGame.FortWeapon.ItemEntryGuid");
    if (!p) return;
    Guid guid = Read<Guid>(GetField(weapon, p));
    OnServerExecuteInventoryItem(guid);
}

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
    if (fn && Read<uint16_t>(fn, profile::FunctionParamSize) == sizeof(void*) + 0x8 + 24 + 24) { auto p = *(OldP*)params; cls = p.Class; loc = p.Loc; rot = p.Rot; }
    else { auto p = *(NewP*)params; cls = p.Class; loc = p.Loc; rot = p.Rot; }
    if (!cls) return;
    Transform t{};
    t.Location[0] = loc.X; t.Location[1] = loc.Y; t.Location[2] = loc.Z;
    void* actor = SpawnActor(session.world, cls, session.controller, t);
    if (!actor) return;
    Call init("/Script/FortniteGame.BuildingActor.InitializeKismetSpawnedBuildingActor");
    if (init.ok()) { init.set("BuildingOwner", actor); init.set("SpawningController", session.controller); bool bUse = true; init.set("bUsePlayerBuildAnimations", bUse); init.run(actor); }
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
    if (init.ok()) { init.set("BuildingOwner", edited); init.set("SpawningController", session.controller); bool bUse = true; init.set("bUsePlayerBuildAnimations", bUse); init.run(edited); }
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
    if (c.ok()) { c.set("FinalLocation", loc); c.set("ItemOwner", session.pawn); c.set("OverrideMaxStackCount", -1); c.set("bToss", toss); c.set("bShouldCombinePickupsWhenTossCompletes", true); c.run(pickup); }
}

void Athena::DropInventoryItem(Guid guid, int count) {
    if(!session.controller || !session.inventory || !session.world || !session.pawn) return;
    if(!(guid.A|guid.B|guid.C|guid.D) || count<1 || count>9999) return;

    void* invProp=GetProp("/Script/FortniteGame.FortInventory.Inventory");
    void* instProp=GetProp("/Script/FortniteGame.FortItemList.ItemInstances");
    void* repProp=GetProp("/Script/FortniteGame.FortItemList.ReplicatedEntries");
    void* itemEntryProp=GetProp("/Script/FortniteGame.FortWorldItem.ItemEntry");
    void* countProp=GetProp("/Script/FortniteGame.FortItemEntry.Count");
    void* guidProp=GetProp("/Script/FortniteGame.FortItemEntry.ItemGuid");
    if(!invProp || !instProp || !repProp || !itemEntryProp || !countProp || !guidProp) {
        LOG("[Drop] inventory reflection incomplete"); return;
    }

    void* list=GetField(session.inventory,invProp);
    void* instArr=GetField(list,instProp);
    void* repArr=GetField(list,repProp);
    Array instances=Read<Array>(instArr);
    Array replicated=Read<Array>(repArr);
    if(instances.Count<0 || instances.Count>512 || replicated.Count<0 || replicated.Count>512 ||
       (instances.Count && !instances.Data) || (replicated.Count && !replicated.Data)) return;

    constexpr size_t EntryStride=0x1A0;
    int instanceIndex=-1, replicatedIndex=-1;
    void* item=nullptr;
    void* replicatedEntry=nullptr;

    for(int i=0;i<instances.Count;++i) {
        void* candidate=Read<void*>(instances.Data,(size_t)i*8);
        if(!candidate || IsBadPtr(candidate)) continue;
        void* entry=GetField(candidate,itemEntryProp);
        if(!entry) continue;
        Guid got=Read<Guid>(GetField(entry,guidProp));
        if(!memcmp(&got,&guid,sizeof(Guid))) { instanceIndex=i; item=candidate; break; }
    }
    if(instanceIndex<0 || !item) { LOG("[Drop] item GUID not found"); return; }

    for(int i=0;i<replicated.Count;++i) {
        void* candidate=(char*)replicated.Data+(size_t)i*EntryStride;
        Guid got=Read<Guid>(GetField(candidate,guidProp));
        if(!memcmp(&got,&guid,sizeof(Guid))) { replicatedIndex=i; replicatedEntry=candidate; break; }
    }
    if(replicatedIndex<0 || !replicatedEntry) { LOG("[Drop] replicated entry missing"); return; }

    void* itemEntry=GetField(item,itemEntryProp);
    if(!itemEntry) return;
    int countOffset=Read<int>(countProp,profile::PropOffset);
    int have=Read<int>((char*)itemEntry+countOffset);
    if(have<1) { LOG("[Drop] invalid stack=%d",have); return; }
    count=std::min(count,have);

    // 21.30 FFortItemEntry is exactly 0x1A0. Copy only through the native
    // pickup-spawn parameter so nested inventory arrays are never aliased.
    struct RawItemEntry { alignas(8) uint8_t Data[0x1A0]; };
    RawItemEntry dropEntry{};
    memcpy(dropEntry.Data,itemEntry,sizeof(dropEntry.Data));
    Write<int>(dropEntry.Data,countOffset,count);

    Call forwardCall("/Script/Engine.Actor.GetActorForwardVector");
    Vector forward{};
    if(!forwardCall.ok() || !forwardCall.run(session.pawn)) { LOG("[Drop] forward unavailable"); return; }
    forward=forwardCall.result<Vector>();
    Call locCall("/Script/Engine.Actor.K2_GetActorLocation");
    if(!locCall.ok() || !locCall.run(session.pawn)) { LOG("[Drop] location unavailable"); return; }
    Vector pos=locCall.result<Vector>();
    pos.X+=forward.X*70.0f; pos.Y+=forward.Y*70.0f; pos.Z+=50.0f;

    void* pickupClass=FindObj("/Script/FortniteGame.FortPickupAthena");
    void* lib=FindObj("/Script/FortniteGame.Default__FortKismetLibrary");
    if(!pickupClass || !lib) { LOG("[Drop] pickup class/library unavailable"); return; }

    Call spawn("/Script/FortniteGame.FortKismetLibrary.K2_SpawnPickupInWorldWithClassAndItemEntry");
    if(!spawn.ok()) { LOG("[Drop] K2_SpawnPickupInWorldWithClassAndItemEntry unavailable"); return; }
    uint8_t sourceType=16; // Tossed
    uint8_t source=10;     // TossedByPlayer
    bool toss=true,randomRotation=true,blocked=false,ownerOnly=false;
    int maxStack=-1;
    if(!spawn.set("WorldContextObject",session.world) || !spawn.set("ItemEntry",dropEntry) ||
       !spawn.set("PickupClass",pickupClass) || !spawn.set("Position",pos) ||
       !spawn.set("Direction",forward) || !spawn.set("OverrideMaxStackCount",maxStack) ||
       !spawn.set("bToss",toss) || !spawn.set("bRandomRotation",randomRotation) ||
       !spawn.set("bBlockedFromAutoPickup",blocked) || !spawn.set("SourceType",sourceType) ||
       !spawn.set("Source",source) || !spawn.set("OptionalOwnerPC",session.controller) ||
       !spawn.set("bPickupOnlyRelevantToOwner",ownerOnly)) {
        spawn.dump("K2_SpawnPickupInWorldWithClassAndItemEntry"); return;
    }
    if(!spawn.run(lib)) { LOG("[Drop] local pickup spawn failed; inventory untouched"); return; }
    void* pickup=spawn.result<void*>();
    if(!pickup || IsBadPtr(pickup)) { LOG("[Drop] pickup was not created; inventory untouched"); return; }

    void* definition=GetField(itemEntry,GetProp("/Script/FortniteGame.FortItemEntry.ItemDefinition"));
    bool removeAll=count>=have;
    if(removeAll && definition) {
        Call removeQB("/Script/FortniteGame.FortPlayerController.RemoveItemFromQuickBars");
        if(removeQB.ok()) { removeQB.set("ItemDefinition",definition); removeQB.run(session.controller); }
        for(int i=0;i<6;++i) if(session.slots[i]==item) session.slots[i]=nullptr;
    }

    if(!removeAll) {
        Write<int>((char*)itemEntry,countOffset,have-count);
        Write<int>((char*)replicatedEntry,countOffset,have-count);
    } else {
        using RemoveFn=void(*)(void*,int32_t,int32_t,int32_t);
        auto removeFn=Fn<RemoveFn>(profile::GenericArrayRemove);
        removeFn(instArr,instanceIndex,1,8);
        replicated=Read<Array>(repArr);
        if(replicatedIndex>=0 && replicatedIndex<replicated.Count && replicated.Data)
            removeFn(repArr,replicatedIndex,1,(int32_t)EntryStride);
    }

    InventoryUpdate();
    RefreshHUD();
    LOG("[Drop] LOCAL drop guid=%08X:%08X:%08X:%08X count=%d remaining=%d",
        guid.A,guid.B,guid.C,guid.D,count,removeAll?0:have-count);
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
    Vector loc{0.0, 0.0, 22500.0}; Rotator rot{0.0, 0.0, 0.0};
    Call sl("/Script/Engine.Actor.K2_SetActorLocationAndRotation");
    if (sl.ok()) { bool bSweep = false, bTeleport = true; sl.set("NewLocation", loc); sl.set("NewRotation", rot); sl.set("bSweep", bSweep); sl.set("bTeleport", bTeleport); sl.run(session.pawn); }
    BeginPlayerSkydiving();
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
        if (Read<void*>(o, profile::ObjectClass) != cls) continue;
        Write<char>((char*)o + propOff, 0, 0);
        ++fixed;
    }
}

void Athena::TeleportToSpawnIsland() {
    if (!session.pawn) return;
    void* cls = FindObj("/Script/FortniteGame.FortPlayerStartWarmup");
    if (!cls) return;
    GObjects_t* go = *(GObjects_t**)(g_base + profile::GObjects);
    if (!go) return;
    std::vector<void*> starts;
    for (int32 i = 0; i < go->NumElements; ++i) { void* o = go->GetById(i); if (o && !IsBadPtr(o)) { void* k = Read<void*>(o, profile::ObjectClass); if (k == cls) starts.push_back(o); } }
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
    for (int i = 0; i < arr.Count; ++i) { void* w = Read<void*>(arr.Data, (size_t)i * 8); if (w) { Call r("/Script/UMG.UserWidget.RemoveFromViewport"); if (r.ok()) r.run(w); } }
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
    if (play.ok()) { play.set("MontageToPlay", montage); play.set("InPlayRate", 1.0f); play.set("InStartTime", 0.0f); bool bStop = true; play.set("bStopAllMontages", bStop); play.run(ai); }
}

void Athena::OnExitVehicle() {
    if (!session.pawn) return;
    Call v("/Script/FortniteGame.FortPlayerPawn.ServerOnExitVehicle");
    if (v.ok()) { int32 ExitForce = 0; bool bDestroy = false; v.set("ExitForceBehavior", ExitForce); v.set("bDestroyVehicleWhenForced", bDestroy); v.run(session.pawn); }
}

void Athena::CheatScript(const char* script) {
    if (!script || !session.pawn) return;
    std::string s = script;
    std::transform(s.begin(), s.end(), s.begin(), ::tolower);
    LOG("CheatScript: %s", s.c_str());
}

std::vector<Athena::LootData> Athena::PickLootDrops(std::string category) {
    std::vector<LootData> ret;
    void* wood = LoadAsset("/Game/Items/ResourcePickups/WoodItemData.WoodItemData");
    void* stone = LoadAsset("/Game/Items/ResourcePickups/StoneItemData.StoneItemData");
    void* metal = LoadAsset("/Game/Items/ResourcePickups/MetalItemData.MetalItemData");
    if (wood) ret.push_back({wood, 50});
    if (stone) ret.push_back({stone, 50});
    if (metal) ret.push_back({metal, 50});
    return ret;
}

// ============================================================================
// HOOKS
// ============================================================================
static void* g_interactionClass = nullptr;

static void pe_hook(void* obj, void* func, void* params) {
    if (!orig_PE) return;
    if (!g_internal && func && g_gameTid.load() == (int)gettid()) {
        if (obj == session.controller && params == nullptr) {
            std::string fn = ObjName(func);
            if (fn == "JumpFromHudStart") { Athena::HandleJumpFromHudStart(); return orig_PE(obj, func, params); }
        }
        if (!g_interactionClass) g_interactionClass = FindObj("/Script/FortniteGame.FortControllerComponent_Interaction");
        bool isPC   = (obj == session.controller);
        bool isPawn = (obj == session.pawn);
        bool isInteraction = false;
        if (g_interactionClass && obj) { void* cls = Read<void*>(obj, profile::ObjectClass); isInteraction = SubclassOfFast(cls, g_interactionClass); }
        std::string n = ObjName(func);
        if (n == "ServerAttemptInteract" && params) {
            Guard g;
            void* target = Read<void*>(params, 0);
            if (target && !IsBadPtr(target)) {
                void* bcCls=FindObj("/Script/FortniteGame.BuildingContainer");
                void* wallCls=FindObj("/Script/FortniteGame.BuildingWall");
                void* targetCls=Read<void*>(target,profile::ObjectClass);
                LOG("[Interact] target=%p class=%s",target,ObjName(targetCls).c_str());
                if(bcCls&&SubclassOfFast(targetCls,bcCls)){Athena::Loot(target);return;}
                if(wallCls&&SubclassOfFast(targetCls,wallCls)){Athena::HandleDoor(target);return;}
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
            else if (n == "ServerPlayEmoteItem") Athena::PlayEmoteItem(Read<void*>(params));
            else if (n == "ServerAttemptExitVehicle") Athena::OnExitVehicle();
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
    if (g_internal || !world) return;
    int expected = 0;
    if (g_gameTid.compare_exchange_strong(expected, (int)gettid())) LOG("WorldTick first: tid=%d", (int)gettid());
    if (g_gameTid.load() != (int)gettid()) return;
    Guard g;
    std::string wn = ObjName(world);
    static std::string lastWorld;
    if (wn != lastWorld) { LOG(">>> WorldTick world: '%s'", wn.c_str()); lastWorld = wn; }
    if (wn.find("Frontend") != std::string::npos) {
        if (session.attempted && !session.finalized && session.finalize_at > 0) { double now = Monotonic(); if (now >= session.finalize_at) Athena::FinalizePlayer(); }
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
        LOG("Intercepting Athena travel");
        session = Session{};
        session.attempted = false;
        return orig_Exec(engine, world, u"open Athena_Terrain", output);
    }
    return orig_Exec(engine, world, command, output);
}

static void InstallHooks() {
    LOG("=== InstallHooks ===");
    if (DobbyHook((void*)(g_base + profile::ProcessEvent), (void*)pe_hook, (void**)&orig_PE) != 0 || !orig_PE) { LOG("ProcessEvent FAILED"); return; }
    LOG("ProcessEvent hooked");
    if (DobbyHook((void*)(g_base + profile::WorldTick), (void*)tick_hook, (void**)&orig_Tick) != 0 || !orig_Tick) LOG("WorldTick FAILED");
    else LOG("WorldTick hooked");
    if (DobbyHook((void*)(g_base + profile::EngineExec), (void*)exec_hook, (void**)&orig_Exec) != 0 || !orig_Exec) LOG("EngineExec FAILED");
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
    LOG("sizeof(Vector)=%zu sizeof(Rotator)=%zu sizeof(Transform)=%zu", sizeof(Vector), sizeof(Rotator), sizeof(Transform));
    for (int i = 0; i < 600 && g_status.load() == 0; ++i) { dl_iterate_phdr(InspectCb, nullptr); if (g_status.load() == 0) usleep(100000); }
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
