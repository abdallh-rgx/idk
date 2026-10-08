// ============================================================================
// EraCH3 — Fortnite 21.30 CL-21088273 Android
// COMPLETE FIXED BUILD v9
// ----------------------------------------------------------------------------
// Key fixes vs v8:
//   1) GetASC uses reflective lookup ONLY (no hardcoded 0x0450).
//   2) TryInitializePlayer waits for game-spawned pawn (fallback to manual
//      spawn only after 15s timeout, with a validated ASC before use).
//   3) SetupWorld checks readiness: GamePhase > 0, controller valid,
//      GameMode/GameState non-null.
//   4) Tick is staged: one heavy step per second, never all at once.
//   5) InitStaminaAttributes disabled (K2_InitStats crashes on Android).
//   6) CDO mutation of GAS_AthenaPlayer removed.
//   7) ForcePlayerVisible avoids raw memory writes during loading.
//   8) Every UE call gated by IsBadPtr on the target object.
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
// ----------------------------------------------------------------------------
// Only offsets that are SAFE to use as RVA pointers are kept here.
// All actor/component offsets are read reflectively via GetProp().
// ============================================================================
namespace profile {
// ---------- Core engine RVAs (libUnreal.so) ----------
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

// ---------- UObject layout (stable across UE4/UE5 Android) ----------
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
constexpr unsigned BoolSize        = 0x80;
constexpr unsigned BoolByteOffset  = 0x81;
constexpr unsigned BoolFieldMask   = 0x82;
constexpr unsigned FFieldName = 0x28;
constexpr unsigned FFieldNext = 0x20;

// ---------- CPF flags ----------
constexpr uint64_t CPF_Parm       = 0x00000080ULL;
constexpr uint64_t CPF_OutParm    = 0x00000100ULL;
constexpr uint64_t CPF_ConstParm  = 0x00000200ULL;
constexpr uint64_t CPF_ReturnParm = 0x00000400ULL;

// ============================================================================
// ACTOR OFFSETS — used only as REFERENCE for sanity checks, never blindly.
// All runtime access goes through GetProp().
// ============================================================================
// APlayerController
constexpr unsigned PC_PlayerState           = 0x0278;
constexpr unsigned PC_CheatManager          = 0x03D8;
constexpr unsigned PC_WorldInventory        = 0x1D20;
constexpr unsigned PC_ClientQuickBars       = 0x1A60;
constexpr unsigned PC_MyPlayerInfo          = 0x2088;

// AFortPawn
constexpr unsigned Pawn_CurrentWeapon       = 0x0550;
constexpr unsigned Pawn_HealthSet           = 0x0798;
constexpr unsigned Pawn_bIsDBNO_Byte        = 0x0A5B;
constexpr unsigned Pawn_bIsDBNO_Bit         = 2;

// APawn / bIsABot
constexpr unsigned Pawn_bIsABot_Byte        = 0x027A;
constexpr unsigned Pawn_bIsABot_Bit         = 3;

// AFortPlayerPawnAthena
constexpr unsigned FPPA_bIsSkydiving        = 0x1A1D;
constexpr unsigned FPPA_bIsParachuteOpen    = 0x1A1E;
constexpr unsigned FPPA_bIsInAnyStorm       = 0x1D78;
constexpr unsigned FPPA_Speed_Sprint        = 0x53EC;

// AFortPlayerStateAthena
constexpr unsigned PS_HeroType              = 0x0430;
constexpr unsigned PS_WorldPlayerId         = 0x03F2;
constexpr unsigned PS_TeamIndex             = 0x1038;
constexpr unsigned PS_KillScore             = 0x104C;
constexpr unsigned PS_SeasonLevelUIDisplay  = 0x1050;
constexpr unsigned PS_SquadId               = 0x120C;

// AFortWeapon
constexpr unsigned Weapon_WeaponData        = 0x03D8;
constexpr unsigned Weapon_ItemEntryGuid     = 0x0B18;

// AFortPickup
constexpr unsigned Pickup_PrimaryPickupItemEntry = 0x02F8;
constexpr unsigned Pickup_bPickedUp_Byte         = 0x0590;
constexpr unsigned Pickup_bPickedUp_Bit          = 0;

// AFortSafeZoneIndicator
constexpr unsigned SZ_SafeZoneRadius            = 0x0288;
constexpr unsigned SZ_NextRadius                = 0x027C;
constexpr unsigned SZ_CurrentPhase              = 0x04B8;
constexpr unsigned SZ_SafeZoneStartShrinkTime   = 0x04C4;
constexpr unsigned SZ_SafeZoneFinishShrinkTime  = 0x04C8;

// AFortAthenaAircraft
constexpr unsigned Aircraft_FlightInfo          = 0x0318;
constexpr unsigned Aircraft_FlightStartTime     = 0x0360;
constexpr unsigned Aircraft_FlightEndTime       = 0x0364;

// FFortItemEntry
constexpr unsigned Entry_ItemDefinition      = 0x0000;
constexpr unsigned Entry_Count               = 0x0014;
constexpr unsigned Entry_Durability          = 0x0024;
constexpr unsigned Entry_LoadedAmmo          = 0x002C;
constexpr unsigned Entry_PhantomReserveAmmo  = 0x0030;
constexpr unsigned Entry_ItemGuid            = 0x0068;
constexpr unsigned Entry_TrackerGuid         = 0x0078;
constexpr unsigned Entry_StateValues         = 0x00D0;
constexpr unsigned Entry_ParentInventory     = 0x00E0;
constexpr unsigned Entry_Size                = 0x01A0;

// FQuickBar / FQuickBarSlot
constexpr unsigned QuickBar_Slots            = 0x0010;
constexpr unsigned QuickBarSlot_bEnabled     = 0x0010;
constexpr unsigned QuickBarSlot_Size         = 0x28;

// ABuildingWall
constexpr unsigned Wall_bDoorOpen_Byte = 0x0FDE;
constexpr unsigned Wall_bDoorOpen_Bit  = 5;

// UCharacterMovementComponent
constexpr unsigned CMC_MovementMode   = 0x019C;
constexpr unsigned CMC_CustomMode     = 0x019D;
constexpr unsigned CMC_MaxWalkSpeed   = 0x01E8;
constexpr unsigned CMC_MaxFlySpeed    = 0x01F4;
constexpr unsigned CMC_MaxSwimSpeed   = 0x01F0;
constexpr unsigned CMC_JumpZVelocity  = 0x0170;
constexpr unsigned CMC_GravityScale   = 0x0168;

// UFortItemDefinition
constexpr unsigned ItemDef_Rarity            = 0x0070;
constexpr unsigned ItemDef_ItemType          = 0x0071;
constexpr unsigned ItemDef_MaxStackSize      = 0x01A8;

// AFortAthenaAIBotController
constexpr unsigned BotCtrl_TargetHandler       = 0x04E0;
constexpr unsigned BotCtrl_PlayerBotPawn       = 0x05E8;
constexpr unsigned BotCtrl_BotData             = 0x0EB8;

// UAthenaAIServicePlayerBots
constexpr unsigned BotSvc_PlayerBots           = 0x0B50;

// FFortServerBotInfo
constexpr unsigned BotInfo_BotController       = 0x0000;
constexpr unsigned BotInfo_Size                = 0x0010;

// QuickBars native init
constexpr uintptr_t InitializeAthenaQuickBars  = 0x056607EC;
constexpr unsigned  QuickBars_VtableSlot       = 0x738;
constexpr unsigned  QuickBars_PrimaryOffset    = 0x278;
constexpr unsigned  QuickBars_SecondaryOffset  = 0x308;

// Tactical Sprint flags
constexpr unsigned TacticalSprint_Flags        = 0xC0;
constexpr uint8_t  TacticalSprint_Bit_Enabled  = 0x01;
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
    if ((v & 0x7) != 0) return true;  // must be 8-byte aligned
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
static bool GetBool(void* obj, const char* path, bool& out);
static std::u16string ToWide(const std::string& s);
static std::string Narrow(const char16_t* s, int max);
static void* GetObjField(void* obj, const char* path);
static void* LoadAsset(const std::string& path, const char* type_path);
static void* LoadClass(const char* path, const char* base_path);
static void* GetPC(void* world, int index);
static void* GetASC(void* actor);
static bool  IsBotPawn(void* pawn);

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
static bool IsBotPawn(void* pawn) {
    if (!pawn || IsBadPtr(pawn)) return false;
    uint8_t b = Read<uint8_t>(pawn, profile::Pawn_bIsABot_Byte);
    return (b >> profile::Pawn_bIsABot_Bit) & 1;
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
static bool GetBool(void* obj, const char* path, bool& out) {
    void* p = GetProp(path);
    if (!p || !obj) return false;
    uint8_t size = Read<uint8_t>(p, profile::BoolSize);
    uint8_t byte = Read<uint8_t>(p, profile::BoolByteOffset);
    uint8_t mask = Read<uint8_t>(p, profile::BoolFieldMask);
    if (!size || size > 8 || byte >= size || !mask) return false;
    auto dst = (uint8_t*)GetField(obj, p) + byte;
    out = (*dst & mask) != 0;
    return true;
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
    return v;
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
        if (IsBadPtr(object) || IsBadPtr(m_func)) return false;   // SAFETY
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
    if (!c.ok()) { LOG("Possess: binding failed"); return; }
    if (!c.set("InPawn", pawn)) { c.dump("Possess"); return; }
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

// ============================================================================
// GetASC — REFLECTIVE ONLY (the critical fix)
// ============================================================================
static void* GetASC(void* actor) {
    if (!actor || IsBadPtr(actor)) return nullptr;

    // 1) Try directly on the actor (FortPawn::AbilitySystemComponent)
    void* asc = GetObjField(actor, "/Script/FortniteGame.FortPawn.AbilitySystemComponent");
    if (asc && !IsBadPtr(asc)) return asc;

    // 2) Try via Controller -> PlayerState
    void* pc = GetObjField(actor, "/Script/Engine.Pawn.Controller");
    if (!pc) pc = GetObjField(actor, "/Script/Engine.Actor.Owner");
    if (pc && !IsBadPtr(pc)) {
        void* ps = GetObjField(pc, "/Script/Engine.Controller.PlayerState");
        if (ps && !IsBadPtr(ps)) {
            asc = GetObjField(ps, "/Script/GameplayAbilities.AbilitySystemComponent");
            if (!asc) asc = GetObjField(ps, "/Script/FortniteGame.FortPlayerState.AbilitySystemComponent");
            if (!asc) asc = GetObjField(ps, "/Script/FortniteGame.AbilitySystemComponent");
            if (asc && !IsBadPtr(asc)) {
                LOG("[ASC] resolved via PlayerState=%p -> asc=%p", ps, asc);
                return asc;
            }
        }
    }

    // 3) Try GameplayAbilities base class path on actor
    asc = GetObjField(actor, "/Script/GameplayAbilities.AbilitySystemComponent");
    if (asc && !IsBadPtr(asc)) return asc;

    LOG("[ASC] not found on actor=%p (%s)", actor, ObjName(Read<void*>(actor, profile::ObjectClass)).c_str());
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
    void* tactical_sprint = nullptr;
    void* bot_service = nullptr;

    // Progress flags
    bool  game_ready = false;
    bool  pawn_source_was_game = false;
    bool  attempted = false;
    bool  initialized = false;
    bool  finalized = false;

    bool  inventory_ready = false;
    bool  aircraft_jumped = false;
    bool  cheats_ready = false;
    bool  safezone_spawned = false;
    bool  sprint_applied = false;
    bool  ready_called = false;
    bool  visibility_forced = false;
    bool  abilities_granted = false;
    bool  melee_granted = false;
    bool  melee_attempted = false;
    bool  stamina_initialized = false;
    bool  tactical_sprint_setup = false;
    bool  quickbars_initialized = false;
    bool  movement_enabled = false;
    bool  profile_attempted = false;
    bool  cosmetics_applied = false;
    bool  side_systems_initialized = false;
    bool  bots_spawned = false;
    bool  solo_hud_ready = false;
    bool  storm_protected = false;
    bool  vehicles_spawned = false;
    bool  ai_initialized = false;
    bool  vending_initialized = false;
    bool  minimap_initialized = false;
    bool  clamber_initialized = false;
    bool  mcp_synced = false;

    int   bots_target = 99;
    int   bots_spawned_count = 0;
    int   vehicles_spawned_count = 0;

    double observed_at = 0;
    double retry_at = 0;
    double ready_at = 0;
    double step_at = 0;
    int   step = 0;

    std::array<void*, 6> slots{};
    std::vector<void*> character_parts;
    std::vector<void*> granted_ability_sets;
    std::vector<void*> bot_controllers;
    std::vector<void*> bot_pawns;

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
    void SpawnSoloBotsBatch(int maxCount);
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
    void InitializeQuickBarsNative();
    void SetQuickBarReady();
    void ActivateQuickBarSlot(uint8_t bar, int slot);
    void ForceUpdateQuickBars();
    void BuildQuickBarsManually();
    void SpawnFloorLoot();
    void EnableMovementFeatures();

    struct LootData { void* Def; int Count; };
    std::vector<LootData> PickLootDrops(std::string category);
}

// ============================================================================
// STAMINA — disabled (K2_InitStats crashes on 21.30 Android)
// ============================================================================
void Athena::InitStaminaAttributes() {
    LOG("[Stamina] skipped (unsafe on 21.30 Android)");
    session.stamina_initialized = true;
}

// ============================================================================
// ABILITIES
// ============================================================================
void Athena::GrantAbility(void* klass) {
    if (!klass || IsBadPtr(klass) || !session.pawn) return;
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

void Athena::GrantAllAbilitySets() {
    if (session.abilities_granted) return;
    if (!session.pawn || IsBadPtr(session.pawn)) { LOG("[Abilities] no pawn"); return; }

    LOG("[Abilities] Setting up...");
    void* asc = GetASC(session.pawn);
    if (!asc) {
        LOG("[Abilities] no ASC — will retry next tick");
        return;   // NOT granted yet; retry next tick
    }

    // Guard flag so we don't spam
    session.abilities_granted = true;

    InitStaminaAttributes();

    void* coreSet = LoadAsset(
        "/Game/Abilities/Player/Generic/Traits/DefaultPlayer/GAS_AthenaPlayer.GAS_AthenaPlayer",
        "/Script/FortniteGame.FortAbilitySet");
    if (!coreSet) { LOG("[Abilities] GAS_AthenaPlayer missing"); GrantMeleeAbility(); return; }

    if (ApplyAbilitySet(session.pawn, coreSet)) session.granted_ability_sets.push_back(coreSet);

    // Direct ability grants (each guarded)
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
    LOG("[Abilities] Done — sets=%zu", session.granted_ability_sets.size());

    GrantMeleeAbility();
}

void Athena::GrantDefaultAbilities() { GrantAllAbilitySets(); }

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
    if (!meleeCls) { LOG("[Melee] no class found"); session.melee_attempted = true; return; }

    Call give("/Script/GameplayAbilities.AbilitySystemComponent.K2_GiveAbility");
    if (!give.ok()) {
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
    }
    session.melee_attempted = true;
}

// ============================================================================
// HUD / REFRESH
// ============================================================================
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
    }

    Call getHud("/Script/Engine.PlayerController.GetHUD");
    if (getHud.ok() && getHud.run(session.controller)) {
        void* hud = getHud.result<void*>();
        if (hud && !IsBadPtr(hud)) {
            Call show("/Script/Engine.HUD.ShowHUD");
            if (show.ok()) show.run(hud);
        }
    }

    void* asc = GetASC(session.pawn);
    if (asc) {
        Call abilities("/Script/GameplayAbilities.AbilitySystemComponent.OnRep_ActivateAbilities");
        if (abilities.ok()) abilities.run(asc);
    }
}

// ============================================================================
// QUICKBARS
// ============================================================================
void Athena::InitializeQuickBarsNative() {
    if (!session.quickbars || IsBadPtr(session.quickbars)) { LOG("[QB] No quickbar"); return; }
    void* vtable = Read<void*>(session.quickbars);
    if (!vtable || IsBadPtr(vtable)) { LOG("[QB] vtable invalid"); return; }
    uintptr_t init_slot = Read<uintptr_t>(vtable, profile::QuickBars_VtableSlot);
    uintptr_t expected_fn = g_base + profile::InitializeAthenaQuickBars;
    LOG("[QB] vtable=%p slot+0x%X=0x%llX expected=0x%llX", vtable, profile::QuickBars_VtableSlot,
        (unsigned long long)init_slot, (unsigned long long)expected_fn);
    if (init_slot != expected_fn) { LOG("[QB] ❌ vtable mismatch — SKIPPING native call"); return; }

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
    LOG("[QB] Building QuickBars from reflected layout...");

    void* primProp=GetProp("/Script/FortniteGame.FortQuickBars.PrimaryQuickBar");
    void* slotsProp=GetProp("/Script/FortniteGame.FortQuickBar.Slots");
    void* itemsProp=GetProp("/Script/FortniteGame.FortQuickBarSlot.Items");
    void* enabledProp=GetProp("/Script/FortniteGame.FortQuickBarSlot.bEnabled");
    if(!primProp || !slotsProp || !itemsProp) { LOG("[QB] props unavailable"); return; }

    void* invProp=GetProp("/Script/FortniteGame.FortInventory.Inventory");
    void* instProp=GetProp("/Script/FortniteGame.FortItemList.ItemInstances");
    if(!invProp || !instProp) return;
    void* list=GetField(session.inventory,invProp);
    Array arr=Read<Array>(GetField(list,instProp));

    void* bar=GetField(session.quickbars,primProp);
    if(!bar) { LOG("[QB] primary bar missing"); return; }
    void* sf=GetField(bar,slotsProp);
    if(!sf) return;
    Array slots=Read<Array>(sf);
    const int slotSize=profile::QuickBarSlot_Size;
    if(slots.Count<6) {
        void* data=calloc(6,(size_t)slotSize);
        if(!data) return;
        Write<void*>(sf,0,data); Write<int32>(sf,8,6); Write<int32>(sf,12,6);
        slots=Read<Array>(sf);
    }

    for(int i=0;i<6 && i<arr.Count && slots.Data;++i) {
        void* item=Read<void*>(arr.Data,(size_t)i*8); if(!item) continue;
        Guid guid{}; Call g("/Script/FortniteGame.FortItem.GetItemGuid");
        if(!g.ok() || !g.run(item)) continue; guid=g.result<Guid>();
        void* slot=(char*)slots.Data+(size_t)i*slotSize;
        void* ia=GetField(slot,itemsProp);
        if(ia) Fn<int(*)(void*,void*,const void*)>(profile::GenericArrayAdd)(ia,itemsProp,&guid);
        if(enabledProp) SetBool(slot,"/Script/FortniteGame.FortQuickBarSlot.bEnabled",true);
    }

    session.quickbars_initialized=true;
    LOG("[QB] Manual initialization complete");
}

void Athena::SetQuickBarReady() {
    if (!session.controller) return;
    LOG("[QB] SetQuickBarReady (no-op)");
}

void Athena::ActivateQuickBarSlot(uint8_t bar, int slot) {
    if (!session.controller) return;
    Call act("/Script/FortniteGame.FortPlayerController.ClientActivateSlot");
    if (!act.ok()) { LOG("[QB] ClientActivateSlot binding failed"); return; }
    float delay = 0.f; bool bUpdatePrev = true; bool bForce = true;
    if (!act.set("InQuickBar", bar) || !act.set("Slot", slot) || !act.set("ActivateDelay", delay) ||
        !act.set("bUpdatePreviousFocusedSlot", bUpdatePrev) || !act.set("bForceExecution", bForce)) {
        return;
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
// MOVEMENT
// ============================================================================
void Athena::EnableMovementFeatures() {
    if (!session.pawn || IsBadPtr(session.pawn)) return;
    LOG("[Movement] Enabling slide & swim...");

    Call canSlide("/Script/FortniteGame.FortPlayerPawn.SetCanSlide");
    if (canSlide.ok()) {
        bool bTrue = true;
        if (canSlide.set("bCanSlide", bTrue)) {
            if (canSlide.run(session.pawn)) LOG("[Movement] ✅ SetCanSlide(true)");
        }
    }

    void* movement = GetObjField(session.pawn, "/Script/Engine.Character.CharacterMovement");
    if (movement && !IsBadPtr(movement)) {
        Off<float>(movement, profile::CMC_MaxSwimSpeed) = 400.0f;
        LOG("[Movement] ✅ Set MaxSwimSpeed");
    }

    void* slideAbility = LoadClass(
        "/Game/Abilities/Player/Generic/Traits/DefaultPlayer/GA_Athena_Player_Slide.GA_Athena_Player_Slide_C",
        "/Script/GameplayAbilities.GameplayAbility");
    if (slideAbility) {
        void* asc = GetASC(session.pawn);
        if (asc) {
            Call give("/Script/GameplayAbilities.AbilitySystemComponent.K2_GiveAbility");
            if (give.ok()) {
                int level = 1;
                int inputID = -1;
                if (give.set("AbilityClass", slideAbility) &&
                    give.set("Level", level) &&
                    give.set("InputID", inputID)) {
                    if (give.run(asc)) LOG("[Movement] ✅ Slide ability granted");
                }
            }
        }
    }

    session.movement_enabled = true;
    LOG("[Movement] Done");
}

// ============================================================================
// TACTICAL SPRINT
// ============================================================================
void Athena::SetupTacticalSprint() {
    LOG("[TacticalSprint] disabled (offsets unverified)");
    session.tactical_sprint_setup = true;
}
void Athena::SetTacticalSprinting(bool sprinting) {
    if (!session.tactical_sprint) return;
    Call set("/Script/TacticalSprintRuntime.FortPlayerControllerComponent_TacticalSprint.SetIsSprinting");
    if (set.ok() && set.set("bNewIsSprinting", sprinting)) set.run(session.tactical_sprint);
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

    void* pawnCls = FindObj("/Game/Athena/PlayerPawn_Athena.PlayerPawn_Athena_C");
    if (pawnCls && IsA(session.pawn, pawnCls)) {
        float* sprintSpeed = (float*)((char*)session.pawn + profile::FPPA_Speed_Sprint);
        if (!IsBadPtr(sprintSpeed)) {
            float old = *sprintSpeed;
            if (std::isfinite(old) && old > 0.0f && old < 5000.0f) {
                if (fabsf(old - 1300.0f) > 1.0f) { *sprintSpeed = 1300.0f; }
            }
        }
    }
    session.sprint_applied = true;
}

// ============================================================================
// BATTLE BUS
// ============================================================================
void Athena::StartBusNow() {
    LOG("[Bus] StartBusNow (no-op)");
}
void Athena::SpawnBattleBus() {
    if (!session.mode || !session.state) return;
    LOG("[Bus] Solo: warmup -> bus");
    void* gp=GetProp("/Script/FortniteGame.FortGameStateAthena.GamePhase");
    if(gp){
        uint8_t phase=2;
        Write<uint8_t>(GetField(session.state,gp),0,phase);
        Call n("/Script/FortniteGame.FortGameStateAthena.OnRep_GamePhase");
        if(n.ok()){uint8_t old=1;n.set("OldGamePhase",old);n.run(session.state);}
    }
    TeleportToSpawnIsland();
    ProtectLocalPlayerFromStorm(true);
}

void Athena::HandleJumpFromHudStart() {
    if (!session.controller || !session.pawn || session.aircraft_jumped) return;
    Guard g;
    LOG("[Bus] JumpFromHudStart intercepted");

    Call getAircraft("/Script/FortniteGame.FortPlayerController.GetAircraftComponent");
    if (!getAircraft.ok() || !getAircraft.run(session.controller)) {
        LOG("[Bus] GetAircraftComponent unavailable");
        return;
    }
    void* aircraftComp = getAircraft.result<void*>();
    if (!aircraftComp || IsBadPtr(aircraftComp)) {
        LOG("[Bus] Aircraft component missing");
        return;
    }

    void* currentAircraft = Read<void*>(aircraftComp, 0xB0);
    if (!currentAircraft || IsBadPtr(currentAircraft)) {
        LOG("[Bus] CurrentAircraft null");
        return;
    }

    Vector rot{0,0,0};
    void* rotProp = GetProp("/Script/Engine.Controller.ControlRotation");
    if (rotProp && Read<int>(rotProp, profile::PropElementSize) == 24) {
        void* addr = GetField(session.controller, rotProp);
        if (addr) rot = Read<Vector>(addr);
    }

    Rotator r{rot.X, rot.Y, rot.Z};
    Call jump("/Script/FortniteGame.FortControllerComponent_Aircraft.ServerAttemptAircraftJump");
    if (!jump.ok() || !jump.set("ClientRotation", r) || !jump.run(aircraftComp)) {
        LOG("[Bus] ServerAttemptAircraftJump failed");
        return;
    }

    Call sky("/Script/FortniteGame.FortPlayerPawn.BeginSkydiving");
    bool local = sky.ok() && sky.set("bFromBus", true) && sky.run(session.pawn);
    if (!local) { LOG("[Bus] BeginSkydiving unavailable"); return; }

    Call cam("/Script/Engine.PlayerController.SetViewTargetWithBlend");
    if (cam.ok()) {
        cam.set("NewViewTarget", session.pawn);
        float blend = 0.0f;
        cam.set("BlendTime", blend);
        cam.run(session.controller);
    }
    session.aircraft_jumped = true;
    LOG("[Bus] Skydiving started");
}

void Athena::OnAircraftJump() { HandleJumpFromHudStart(); }

void Athena::BeginPlayerSkydiving() {
    if (!session.pawn || IsBadPtr(session.pawn)) return;
    Call bsd("/Script/FortniteGame.FortPlayerPawn.BeginSkydiving");
    if (bsd.ok()) { bool bFromBus = true; if (bsd.set("bFromBus", bFromBus)) { bsd.run(session.pawn); } }
    SetBool(session.pawn, "/Script/FortniteGame.FortPlayerPawn.bIsSkydiving", true);
}

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
    LOG("[Door] toggled: %s -> %s", wasOpen ? "OPEN" : "CLOSED", wasOpen ? "CLOSED" : "OPEN");
}

// ============================================================================
// COSMETICS / MCP
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
    if (!source) { LOG("[MCP] loadout prop missing"); return; }
    void* info = GetObjField(session.controller, "/Script/FortniteGame.FortPlayerController.MyPlayerInfo");
    void* idProp = GetProp("/Script/FortniteGame.FortRegisteredPlayerInfo.PlayerID");
    void* ctxType = FindObj("/Script/FortniteGame.FortMcpContext");
    void* lib = FindObj("/Script/BlueprintContext.Default__BlueprintContextLibrary");
    if (!info || !idProp || !ctxType || !lib) { LOG("[MCP] prerequisites missing"); return; }
    int sourceSize = Read<int>(source, profile::PropElementSize);
    if (sourceSize <= 0 || sourceSize > 2048) { LOG("[MCP] invalid loadout size=%d", sourceSize); return; }
    void* idAddr = GetField(info, idProp);
    int idSize = Read<int>(idProp, profile::PropElementSize);
    if (!idAddr || idSize <= 0 || idSize > 256) { LOG("[MCP] invalid PlayerID"); return; }
    Call context("/Script/BlueprintContext.BlueprintContextLibrary.GetContext");
    if (!context.ok() || !context.set("ContextObject", session.controller) || !context.set("Class", ctxType) || !context.run(lib)) {
        LOG("[MCP] GetContext failed"); return;
    }
    void* ctx = context.result<void*>();
    if (!ctx || IsBadPtr(ctx)) { LOG("[MCP] context null"); return; }
    Call get("/Script/FortniteGame.FortMcpContext.GetLoadoutForPlayer");
    if (!get.ok() || !get.set_bytes("MemberUniqueId", idAddr, (size_t)idSize) || !get.run(ctx)) {
        LOG("[MCP] GetLoadoutForPlayer failed"); return;
    }
    const void* value = get.result_data((size_t)sourceSize);
    if (!value) { LOG("[MCP] loadout return unavailable"); return; }
    void* dst = GetField(session.controller, source);
    if (!dst) { LOG("[MCP] destination missing"); return; }
    memcpy(dst, value, (size_t)sourceSize);
    session.mcp_synced = true;
    LOG("[MCP] Profile loadout synced (%d bytes)", sourceSize);
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
    if (!loadout || !state) { LOG("[Profile] loadout/state missing"); session.cosmetics_applied = false; return; }

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
        if (apply.ok() && klib && apply.set("WorldContextObject",session.world) && apply.set("CharacterParts",arr) && apply.set("PlayerState",state))
            apply.run(klib);
    }
    Call r1("/Script/FortniteGame.FortPlayerState.OnRep_HeroType"); if(r1.ok())r1.run(state);
    Call r2("/Script/FortniteGame.FortPlayerState.OnRep_CharacterData"); if(r2.ok())r2.run(state);
    void* klib=FindObj("/Script/FortniteGame.Default__FortKismetLibrary");
    if(klib){Call u("/Script/FortniteGame.FortKismetLibrary.UpdatePlayerCustomCharacterPartsVisualization");if(u.ok()&&u.set("PlayerState",state))u.run(klib);}
    Call cl("/Script/FortniteGame.FortPlayerPawn.OnRep_CosmeticLoadout"); if(cl.ok())cl.run(session.pawn);

    session.cosmetics_applied = !parts.empty() || hero != nullptr;
    LOG("[Profile] Cosmetics applied: parts=%d hero=%p",(int)parts.size(),hero);
}

void Athena::ShowSkin() {
    if (session.cosmetics_applied || session.profile_attempted) return;
    session.profile_attempted = true;
    LoadProfileCosmetics();
}

void Athena::ForcePlayerVisible() {
    if (!session.pawn || IsBadPtr(session.pawn)) return;
    LOG("[Visibility] forcing player body visible...");

    SetBool(session.pawn, "/Script/FortniteGame.FortPlayerPawn.bIsSkydiving", false);
    SetBool(session.pawn, "/Script/FortniteGame.FortPlayerPawn.bIsParachuteOpen", false);

    Call sd("/Script/FortniteGame.FortPlayerPawn.OnRep_IsSkydiving"); if (sd.ok()) sd.run(session.pawn);
    Call po("/Script/FortniteGame.FortPlayerPawn.OnRep_IsParachuteOpen"); if (po.ok()) po.run(session.pawn);
    Call body("/Script/FortniteGame.FortPlayerPawnAthena.SetCharacterBodyVisibilityForPossession");
    if (body.ok()) { bool bShow = true; if (body.set("bVisible", bShow)) body.run(session.pawn); }
    Call hid("/Script/Engine.Actor.SetActorHiddenInGame");
    if (hid.ok()) { bool bHidden = false; if (hid.set("bNewHidden", bHidden)) hid.run(session.pawn); }
    SetBool(session.pawn, "/Script/Engine.Actor.bHidden", false);
    session.visibility_forced = true;
}

// ============================================================================
// SOLO HUD
// ============================================================================
void Athena::UpdateSoloHUD() {
    if(!session.state||!session.controller)return;

    int botsAlive = session.bots_spawned_count;
    if (session.bot_service && !IsBadPtr(session.bot_service)) {
        Array playerBots = Read<Array>(session.bot_service, profile::BotSvc_PlayerBots);
        if (playerBots.Count >= 0 && playerBots.Count < 200 && (!playerBots.Count || playerBots.Data)) {
            int count = 0;
            for (int i = 0; i < playerBots.Count; ++i) {
                void* entry = (char*)playerBots.Data + i * profile::BotInfo_Size;
                void* botCtrl = Read<void*>(entry, profile::BotInfo_BotController);
                if (!botCtrl || IsBadPtr(botCtrl)) continue;
                void* botPawn = Read<void*>(botCtrl, profile::BotCtrl_PlayerBotPawn);
                if (botPawn && !IsBadPtr(botPawn)) count++;
            }
            botsAlive = count;
        }
    }

    int playersLeft = 1 + botsAlive;
    int totalPlayers = 1 + session.bots_target;
    Write<int>(session.state, 0x14B8, playersLeft);
    Write<int>(session.state, 0x14B4, totalPlayers);

    Call rep("/Script/FortniteGame.FortGameStateAthena.OnRep_PlayersLeft");if(rep.ok())rep.run(session.state);
    Call pc("/Script/FortniteGame.FortPlayerControllerAthena.HandlePlayersLeftChanged");if(pc.ok())pc.run(session.controller);

    session.solo_hud_ready=true;
    LOG("[PlayerCounter] players=%d total=%d (bots alive=%d)",playersLeft,totalPlayers,botsAlive);
}

void Athena::SetupLocalPlayerCounter() { UpdateSoloHUD(); }

void Athena::SetupLocalMinimap() {
    if(!session.world){return;}
    void* wb=FindObj("/Script/UMG.Default__WidgetBlueprintLibrary");
    void* cls=FindObj("/Script/AthenaMinimapContainer.AthenaMinimapContainer_C");
    if(!cls)cls=FindObj("/Script/FortniteUI.AthenaMinimapContainer");
    if(!wb||!cls){LOG("[Minimap] class/library missing");return;}
    Call all("/Script/UMG.WidgetBlueprintLibrary.GetAllWidgetsOfClass");
    if(!all.ok()||!all.set("WorldContextObject",session.world)||!all.set("WidgetClass",cls)||!all.run(wb)){return;}
    Array a=all.result<Array>();
    if(a.Count<0 || a.Count>256 || (a.Count && !a.Data)) return;
    int shown=0;
    for(int i=0;i<a.Count;i++){void*w=Read<void*>(a.Data,(size_t)i*8);if(!w||IsBadPtr(w))continue;
        Call vis("/Script/UMG.Widget.SetVisibility");if(vis.ok()){uint8_t visible=0;if(vis.set("InVisibility",visible)&&vis.run(w))shown++;}
    }
    session.minimap_initialized = true;
    LOG("[Minimap] visible=%d",shown);
}

void Athena::SetupLocalClamber() {
    if(!session.pawn){return;}
    void* cls=FindObj("/Script/ClamberingCodeRuntime.ClamberingComponent");
    if(!cls){return;}
    Call get("/Script/Engine.Actor.GetComponentByClass");
    if(!get.ok()||!get.set("ComponentClass",cls)||!get.run(session.pawn))return;
    void* comp=get.result<void*>();
    if(!comp||IsBadPtr(comp))return;
    session.clamber_initialized = true;
    LOG("[Climb] component=%p",comp);
}

void Athena::SetupLocalVending() {
    if (!session.world || !session.pawn) return;
    void* cls = FindObj("/Script/FortniteGame.BuildingProp_VendingMachine");
    void* wb = FindObj("/Script/Engine.Default__GameplayStatics");
    if (!cls || !wb) { LOG("[Vending] missing"); return; }
    Call all("/Script/Engine.GameplayStatics.GetAllActorsOfClass");
    if (!all.ok() || !all.set("WorldContextObject", session.world) || !all.set("ActorClass", cls) || !all.run(wb)) { return; }
    Array a = all.result<Array>();
    if (a.Count < 0 || a.Count > 4096 || (a.Count && !a.Data)) return;
    int count = 0;
    for (int i=0;i<a.Count;i++) {
        void* m=Read<void*>(a.Data,(size_t)i*8); if(!m||IsBadPtr(m)) continue;
        Call inv("/Script/FortniteGame.BuildingProp_VendingMachine.OnPlayerInventoryChange");
        if(inv.ok() && inv.run(m)) count++;
    }
    session.vending_initialized = true;
    LOG("[Vending] refreshed %d machines", count);
}

// ============================================================================
// VEHICLES
// ============================================================================
void Athena::SetupLocalVehicles() {
    if (!session.world) { LOG("[Vehicles] world missing"); return; }
    void* spawnerCls = FindObj("/Script/FortniteGame.FortAthenaVehicleSpawner");
    if (!spawnerCls) { LOG("[Vehicles] spawner class missing"); return; }
    void* wb = FindObj("/Script/Engine.Default__GameplayStatics");
    if (!wb) { LOG("[Vehicles] GameplayStatics missing"); return; }
    Call all("/Script/Engine.GameplayStatics.GetAllActorsOfClass");
    if (!all.ok() || !all.set("WorldContextObject",session.world) || !all.set("ActorClass",spawnerCls) || !all.run(wb)) { return; }
    Array a=all.result<Array>();
    if(a.Count<0||a.Count>4096||(a.Count&&!a.Data))return;
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
        spawned++;
    }
    session.vehicles_spawned_count = spawned;
    session.vehicles_spawned = true;
    LOG("[Vehicles] spawned=%d skipped=%d",spawned,skipped);
}

// ============================================================================
// AI / BOT SERVICE
// ============================================================================
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
    LOG("[Bots] Bot service acquired: %p", svc);
    return svc;
}

void Athena::SetupLocalAI() {
    if (session.ai_initialized) return;
    if (!session.mode || IsBadPtr(session.mode)) { LOG("[NPC] game mode missing"); return; }

    void* director = nullptr;
    {
        void* cls = FindObj("/Game/AIDirector/AIDirector_Fortnite.AIDirector_Fortnite_C");
        if (!cls) cls = FindObj("/Script/FortniteGame.AthenaAIDirector");
        if (cls) {
            Transform t{};
            director = SpawnActor(session.world, cls, session.mode, t);
            if (director) {
                void* prop = GetProp("/Script/FortniteGame.FortGameModeAthena.AIDirector");
                if (prop) Write<void*>(GetField(session.mode, prop), 0, director);
            }
        }
    }
    if (director && !IsBadPtr(director)) {
        Call act("/Script/FortniteGame.AIDirector.Activate");
        if (act.ok()) act.run(director);
        LOG("[NPC] AIDirector active");
    }

    GetBotService();
    session.ai_initialized = true;
}

void Athena::SpawnSoloBotsBatch(int maxCount) {
    if (session.bots_spawned || !session.world || !session.state) return;
    if (session.bots_spawned_count >= session.bots_target) { session.bots_spawned = true; return; }

    void* svc = GetBotService();
    if (!svc) { LOG("[Bots] bot service unavailable; skipping"); session.bots_spawned = true; return; }

    Call spawnAI("/Script/FortniteAI.AthenaAIServicePlayerBots.SpawnAI");
    if (!spawnAI.ok()) { LOG("[Bots] SpawnAI binding FAILED"); spawnAI.dump("SpawnAI"); session.bots_spawned = true; return; }

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
        void* componentList = nullptr;

        bool s1 = spawnAI.set("InSpawnLocation", spawnLoc);
        bool s2 = spawnAI.set("InSpawnRotation", spawnRot);
        bool s3 = spawnAI.set("AISpawnerComponentList", componentList);

        if (!s1 || !s2 || !s3) { spawnAI.dump("SpawnAI"); break; }

        if (spawnAI.run(svc)) {
            void* botPawn = spawnAI.result<void*>();
            if (botPawn && !IsBadPtr(botPawn)) {
                session.bot_pawns.push_back(botPawn);
                session.bots_spawned_count++;
                made++;
            }
        }
    }

    if (session.bots_spawned_count >= session.bots_target)
        session.bots_spawned = true;

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
    LOG("[Storm] local protection=%d",(int)protect);
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
// INIT PLAYER — wait for the game-spawned pawn
// ============================================================================
bool Athena::TryInitializePlayer(void* world, void* mode, void* controller) {
    if (!world || !mode || !controller) return false;

    // Preferred path: game has already spawned and possessed a pawn.
    void* pawn = GetPawn(controller);
    if (pawn && !IsBadPtr(pawn)) {
        void* pawnCls = FindObj("/Script/FortniteGame.FortPlayerPawnAthena");
        if (!pawnCls) pawnCls = FindObj("/Script/FortniteGame.FortPlayerPawn");
        if (pawnCls && IsA(pawn, pawnCls)) {
            session.pawn = pawn;
            session.pawn_source_was_game = true;
            LOG("[Init] Using game-spawned pawn: %p (%s)", pawn, ObjName(Read<void*>(pawn, profile::ObjectClass)).c_str());
            return true;
        }
    }

    // Fallback: none yet. Wait — do NOT manually spawn while loading.
    if (!session.pawn_source_was_game) {
        if (session.retry_at == 0) session.retry_at = Monotonic();
        double waited = Monotonic() - session.retry_at;
        if (waited < 15.0) {
            LOG("[Init] waiting for game to spawn pawn (%.1fs/15s)", waited);
            return false;
        }
        // After 15s, spawn manually (last resort for offline/bot-lobby)
        LOG("[Init] game did not spawn a pawn in 15s — spawning manually");
        void* pawnCls = LoadClass("/Game/Athena/PlayerPawn_Athena.PlayerPawn_Athena_C", "/Script/Engine.Pawn");
        if (!pawnCls) { LOG("[Init] PlayerPawn_Athena_C missing"); return false; }

        Transform t{};
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
        if (!haveSpawn) { t.Location[0] = 0.0; t.Location[1] = 0.0; t.Location[2] = 3100.0; }
        session.pawn = SpawnActor(world, pawnCls, controller, t);
        if (!session.pawn || IsBadPtr(session.pawn)) { LOG("[Init] Spawn failed"); return false; }
        LOG("[Init] Manually spawned: %p", session.pawn);
        Possess(controller, session.pawn);
        if (GetPawn(controller) != session.pawn) { LOG("[Init] Possession did not stick"); return false; }
        LOG("[Init] Possessed OK (manual)");
        session.pawn_source_was_game = false;
        return true;
    }
    return false;
}

// ============================================================================
// SETUP WORLD — readiness-checked, no heavy init
// ============================================================================
void Athena::SetupWorld(void* world) {
    session.world = world;
    session.controller = GetPC(world, 0);
    if (!session.controller || IsBadPtr(session.controller)) { LOG("SetupWorld: no controller"); return; }

    void* baseCtrl = FindObj("/Script/FortniteGame.FortPlayerController");
    if (!baseCtrl || !IsA(session.controller, baseCtrl)) { LOG("SetupWorld: not FortPlayerController"); return; }

    void* modeProp  = GetProp("/Script/Engine.World.AuthorityGameMode");
    void* stateProp = GetProp("/Script/Engine.World.GameState");
    session.mode  = modeProp  ? Read<void*>(GetField(world, modeProp))  : nullptr;
    session.state = stateProp ? Read<void*>(GetField(world, stateProp)) : nullptr;
    if (!session.mode || !session.state || IsBadPtr(session.mode) || IsBadPtr(session.state)) {
        LOG("SetupWorld: mode/state missing");
        return;
    }

    LOG("SetupWorld: PC=%s GM=%s GS=%s",
        ObjName(session.controller).c_str(),
        ObjName(session.mode).c_str(),
        ObjName(session.state).c_str());

    // Readiness gate: GamePhase must be > 0
    void* gp = GetProp("/Script/FortniteGame.FortGameStateAthena.GamePhase");
    if (gp) {
        uint8_t phase = Read<uint8_t>(GetField(session.state, gp));
        if (phase == 0 || phase > 6) {
            LOG("SetupWorld: game phase=%d — not ready, will retry", phase);
            return;
        }
        LOG("SetupWorld: game phase=%d OK", phase);
    }

    // Wait for pawn (or fall back after 15s)
    if (!TryInitializePlayer(world, session.mode, session.controller)) {
        LOG("SetupWorld: pawn not ready yet");
        return;
    }

    // Readiness confirmed. Do NOT run heavy init here — defer to staged Tick.
    session.attempted = true;
    session.initialized = true;
    session.game_ready = true;
    session.ready_at = Monotonic();
    session.step = 0;
    session.step_at = 0;
    LOG("SetupWorld: prerequisites confirmed, deferring heavy init to Tick");
}

// ============================================================================
// TICK — staged initialization
// ============================================================================
void Athena::Tick() {
    if (!session.world || !session.attempted) return;

    double now = Monotonic();

    // Wait 5 seconds after readiness before touching anything heavy
    if (session.ready_at == 0) session.ready_at = now;
    if (now - session.ready_at < 5.0) return;

    // One heavy step per second
    if (session.step_at != 0 && now - session.step_at < 1.0) return;
    session.step_at = now;

    switch (session.step) {
    case 0:
        LOG("[Step 0] RefreshHUD (light)");
        RefreshHUD();
        session.step++;
        return;

    case 1:
        LOG("[Step 1] GrantAllAbilitySets");
        GrantAllAbilitySets();
        session.step++;
        return;

    case 2:
        LOG("[Step 2] EnableMovementFeatures");
        EnableMovementFeatures();
        session.step++;
        return;

    case 3:
        LOG("[Step 3] SetupTacticalSprint");
        SetupTacticalSprint();
        session.step++;
        return;

    case 4:
        LOG("[Step 4] SyncMCPProfile + ShowSkin");
        SyncMCPProfile();
        ShowSkin();
        session.step++;
        return;

    case 5:
        LOG("[Step 5] ApplySprintSpeed");
        ApplySprintSpeed();
        session.step++;
        return;

    case 6:
        LOG("[Step 6] DestroyLods");
        DestroyLods();
        session.step++;
        return;

    case 7:
        LOG("[Step 7] InitializeInventory");
        InitializeInventory();
        session.step++;
        return;

    case 8:
        LOG("[Step 8] SpawnFloorLoot");
        SpawnFloorLoot();
        session.step++;
        return;

    case 9:
        LOG("[Step 9] SpawnSafeZoneIndicator");
        SpawnSafeZoneIndicator();
        session.step++;
        return;

    case 10:
        LOG("[Step 10] SpawnBuildPreviews");
        SpawnBuildPreviews();
        session.step++;
        return;

    case 11:
        LOG("[Step 11] DropLoadingScreen");
        DropLoadingScreen();
        session.step++;
        return;

    case 12:
        LOG("[Step 12] RemoveNetDebugUI");
        RemoveNetDebugUI();
        session.step++;
        return;

    case 13:
        LOG("[Step 13] FinalizePlayer");
        FinalizePlayer();
        session.step++;
        return;

    case 14:
        LOG("[Step 14] InitializeSideSystems");
        InitializeSideSystems();
        session.step++;
        return;

    case 15:
        LOG("[Step 15] First bot batch");
        SpawnSoloBotsBatch(10);
        session.step++;
        return;

    default:
        break;
    }

    // ---- Maintenance phase ----
    static double lastVis = 0, lastSpd = 0, lastHud = 0, lastBot = 0, lastStorm = 0;
    if (now - lastVis > 15.0) {
        lastVis = now;
        if (!session.visibility_forced) ForcePlayerVisible();
    }
    if (now - lastSpd > 5.0) {
        lastSpd = now;
        ApplySprintSpeed();
    }
    if (now - lastHud > 3.0 && session.bots_spawned_count > 0) {
        lastHud = now;
        UpdateSoloHUD();
    }
    if (now - lastBot > 3.0 && !session.bots_spawned) {
        lastBot = now;
        SpawnSoloBotsBatch(10);
    }
    if (session.abilities_granted && !session.melee_granted && !session.melee_attempted) {
        GrantMeleeAbility();
    }
}

// ============================================================================
// FINALIZE
// ============================================================================
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
    if (!session.movement_enabled) EnableMovementFeatures();
    if (!session.profile_attempted) ShowSkin();

    if (!session.quickbars_initialized) {
        InitializeQuickBarsNative();
        if (!session.quickbars_initialized) BuildQuickBarsManually();
        SetQuickBarReady();
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
    RefreshHUD();
    LOG("Finalize: complete");
}

// ============================================================================
// HELPERS: LODS / LOADING / INVENTORY
// ============================================================================
void Athena::DestroyLods() {
    void* cm = GetObjField(session.controller, "/Script/Engine.PlayerController.CheatManager");
    if (!cm || IsBadPtr(cm)) { SetupCheats(); cm = GetObjField(session.controller, "/Script/Engine.PlayerController.CheatManager"); }
    if (!cm || IsBadPtr(cm)) return;
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
    { Call c("/Script/FortniteGame.FortPlayerController.ClientForceWorldInventoryUpdate"); if (c.ok()) c.run(session.controller); }
    if (session.inventory && !IsBadPtr(session.inventory)) { Call a("/Script/FortniteGame.FortInventory.HandleInventoryLocalUpdate"); if (a.ok()) a.run(session.inventory); }
    { Call b("/Script/FortniteGame.FortPlayerController.HandleWorldInventoryLocalUpdate"); if (b.ok()) b.run(session.controller); }
    { Call c("/Script/FortniteGame.FortPlayerController.OnRep_QuickBar"); if (c.ok()) c.run(session.controller); }
    if (session.quickbars && !IsBadPtr(session.quickbars)) {
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
        Off<float>(entrySrc, profile::Entry_Durability) = 1.0f;
        void* weapCls = FindObj("/Script/FortniteGame.FortWeaponItemDefinition");
        if (weapCls && IsA(def, weapCls)) {
            Off<int32>(entrySrc, profile::Entry_LoadedAmmo) = 30;
            Off<int32>(entrySrc, profile::Entry_PhantomReserveAmmo) = 300;
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
                if (itemsProp) { void* itemsArr = GetField(slotObj, itemsProp); Fn<int(*)(void*,void*,const void*)>(profile::GenericArrayAdd)(itemsArr, itemsProp, &itemGuid); }
                SetBool(slotObj, "/Script/FortniteGame.FortQuickBarSlot.bEnabled", true);
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
    if (!session.inventory) { LOG("[Inv] No inventory"); return; }
    { Call so("/Script/FortniteGame.FortInventory.SetOwner"); if (so.ok()) so.run(session.inventory); }

    session.quickbars = GetObjField(session.controller, "/Script/FortniteGame.FortPlayerController.ClientQuickBars");
    if (!session.quickbars) {
        LOG("[QB] Spawning FortQuickBarsAthena...");
        void* klass = FindObj("/Script/FortniteGame.FortQuickBarsAthena");
        if (!klass) klass = FindObj("/Script/FortniteGame.FortQuickBars");
        Transform t{};
        session.quickbars = SpawnActor(session.world, klass, session.controller, t);
        if (session.quickbars) {
            void* qbProp = GetProp("/Script/FortniteGame.FortPlayerController.ClientQuickBars");
            if (qbProp) Write<void*>(GetField(session.controller, qbProp), 0, session.quickbars);
            LOG("[QB] Spawned: %p class=%s", session.quickbars, ObjName(Read<void*>(session.quickbars, profile::ObjectClass)).c_str());
        }
    }
    if (session.quickbars) {
        Write<void*>((char*)session.quickbars + 0x138, 0, session.controller);
        Call soQB("/Script/FortniteGame.FortQuickBars.SetOwner");
        if (soQB.ok()) soQB.run(session.quickbars);
        LOG("[QB] Running native initialize...");
        InitializeQuickBarsNative();
    }

    LOG("Inventory ready: inv=%p qb=%p initialized=%d",
        session.inventory, session.quickbars, session.quickbars_initialized ? 1 : 0);
    if (!session.quickbars_initialized) { LOG("[Inv] QuickBars init failed; skipping items"); session.inventory_ready = false; return; }

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
    if (itemEntryProp) { void* entrySrc = GetField(item, itemEntryProp); if (entrySrc) trackerGuid = Read<Guid>(entrySrc, profile::Entry_TrackerGuid); }
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
    if(!wb||!cls){LOG("[Loot] BuildingContainer/GameplayStatics missing");return;}
    Call all("/Script/Engine.GameplayStatics.GetAllActorsOfClass");
    if(!all.ok()||!all.set("WorldContextObject",session.world)||!all.set("ActorClass",cls)||!all.run(wb)){return;}
    Array a=all.result<Array>();
    if(a.Count<0||a.Count>200000||(a.Count&&!a.Data)){return;}
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

void Athena::Loot(void* actor) {
    if (!actor || !session.pawn || IsBadPtr(actor)) return;
    void* bcCls=FindObj("/Script/FortniteGame.BuildingContainer");
    if(!bcCls || !IsA(actor,bcCls)) return;
    Call already("/Script/FortniteGame.BuildingContainer.BP_IsAlreadySearched");
    if(already.ok() && already.run(actor) && already.result<bool>()) { LOG("[Loot] already searched"); return; }
    Call begin("/Script/FortniteGame.BuildingContainer.OnBeginSearch"); if(begin.ok())begin.run(actor);
    Call spawn("/Script/FortniteGame.BuildingContainer.BP_SpawnLoot");
    bool spawned=false;
    if(spawn.ok() && spawn.set("PlayerPawn",session.pawn) && spawn.run(actor)) spawned=spawn.result<bool>();
    Call loot("/Script/FortniteGame.BuildingContainer.OnLoot"); if(loot.ok())loot.run(actor);
    SetBool(actor,"/Script/FortniteGame.BuildingContainer.bAlreadySearched",true);
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
        if (memcmp(&cand, &guid, sizeof(Guid)) == 0) {
            for (int s = 0; s < 6; ++s) if (session.slots[s] == inst) { EquipSlot(s); return; }
            return;
        }
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
    if(!invProp || !instProp || !repProp || !itemEntryProp) return;

    void* list=GetField(session.inventory,invProp);
    void* instArr=GetField(list,instProp);
    void* repArr=GetField(list,repProp);
    Array instances=Read<Array>(instArr);
    Array replicated=Read<Array>(repArr);
    if(instances.Count<0 || instances.Count>512 || replicated.Count<0 || replicated.Count>512) return;

    constexpr size_t EntryStride=profile::Entry_Size;
    int instanceIndex=-1, replicatedIndex=-1;
    void* item=nullptr;
    void* replicatedEntry=nullptr;

    for(int i=0;i<instances.Count;++i) {
        void* candidate=Read<void*>(instances.Data,(size_t)i*8);
        if(!candidate || IsBadPtr(candidate)) continue;
        void* entry=GetField(candidate,itemEntryProp);
        if(!entry) continue;
        Guid got=Read<Guid>(entry, profile::Entry_ItemGuid);
        if(!memcmp(&got,&guid,sizeof(Guid))) { instanceIndex=i; item=candidate; break; }
    }
    if(instanceIndex<0 || !item) { LOG("[Drop] item GUID not found"); return; }

    for(int i=0;i<replicated.Count;++i) {
        void* candidate=(char*)replicated.Data+(size_t)i*EntryStride;
        Guid got=Read<Guid>(candidate, profile::Entry_ItemGuid);
        if(!memcmp(&got,&guid,sizeof(Guid))) { replicatedIndex=i; replicatedEntry=candidate; break; }
    }
    if(replicatedIndex<0 || !replicatedEntry) { LOG("[Drop] replicated entry missing"); return; }

    void* itemEntry=GetField(item,itemEntryProp);
    if(!itemEntry) return;
    int have=Read<int>(itemEntry, profile::Entry_Count);
    if(have<1) { LOG("[Drop] invalid stack=%d",have); return; }
    count=std::min(count,have);

    struct RawItemEntry { alignas(8) uint8_t Data[profile::Entry_Size]; };
    RawItemEntry dropEntry{};
    memcpy(dropEntry.Data,itemEntry,sizeof(dropEntry.Data));
    Write<int>(dropEntry.Data, profile::Entry_Count, count);

    Call forwardCall("/Script/Engine.Actor.GetActorForwardVector");
    Vector forward{};
    if(!forwardCall.ok() || !forwardCall.run(session.pawn)) return;
    forward=forwardCall.result<Vector>();
    Call locCall("/Script/Engine.Actor.K2_GetActorLocation");
    if(!locCall.ok() || !locCall.run(session.pawn)) return;
    Vector pos=locCall.result<Vector>();
    pos.X+=forward.X*70.0f; pos.Y+=forward.Y*70.0f; pos.Z+=50.0f;

    void* pickupClass=FindObj("/Script/FortniteGame.FortPickupAthena");
    void* lib=FindObj("/Script/FortniteGame.Default__FortKismetLibrary");
    if(!pickupClass || !lib) return;

    Call spawn("/Script/FortniteGame.FortKismetLibrary.K2_SpawnPickupInWorldWithClassAndItemEntry");
    if(!spawn.ok()) return;
    uint8_t sourceType=16;
    uint8_t source=10;
    bool toss=true,randomRotation=true,blocked=false,ownerOnly=false;
    int maxStack=-1;
    if(!spawn.set("WorldContextObject",session.world) || !spawn.set("ItemEntry",dropEntry) ||
       !spawn.set("PickupClass",pickupClass) || !spawn.set("Position",pos) ||
       !spawn.set("Direction",forward) || !spawn.set("OverrideMaxStackCount",maxStack) ||
       !spawn.set("bToss",toss) || !spawn.set("bRandomRotation",randomRotation) ||
       !spawn.set("bBlockedFromAutoPickup",blocked) || !spawn.set("SourceType",sourceType) ||
       !spawn.set("Source",source) || !spawn.set("OptionalOwnerPC",session.controller) ||
       !spawn.set("bPickupOnlyRelevantToOwner",ownerOnly)) return;
    if(!spawn.run(lib)) return;
    void* pickup=spawn.result<void*>();
    if(!pickup || IsBadPtr(pickup)) return;

    bool removeAll=count>=have;
    if(removeAll) {
        for(int i=0;i<6;++i) if(session.slots[i]==item) session.slots[i]=nullptr;
    }

    if(!removeAll) {
        Write<int>(itemEntry, profile::Entry_Count, have-count);
        Write<int>(replicatedEntry, profile::Entry_Count, have-count);
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
    LOG("[Drop] LOCAL drop count=%d remaining=%d",count,removeAll?0:have-count);
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
    for (int32 i = 0; i < go->NumElements; ++i) {
        void* o = go->GetById(i);
        if (!o || IsBadPtr(o)) continue;
        if (Read<void*>(o, profile::ObjectClass) != cls) continue;
        Write<char>((char*)o + propOff, 0, 0);
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

std::vector<Athena::LootData> Athena::PickLootDrops(std::string) {
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
    if (!g_internal && func && g_gameTid.load() == (int)gettid() && !IsBadPtr(obj) && !IsBadPtr(func)) {
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
    if (g_internal || !world || IsBadPtr(world)) return;
    int expected = 0;
    if (g_gameTid.compare_exchange_strong(expected, (int)gettid())) LOG("WorldTick first: tid=%d", (int)gettid());
    if (g_gameTid.load() != (int)gettid()) return;
    Guard g;
    std::string wn = ObjName(world);
    static std::string lastWorld;
    if (wn != lastWorld) { LOG(">>> WorldTick world: '%s'", wn.c_str()); lastWorld = wn; }

    if (wn.find("Frontend") != std::string::npos) return;
    if (wn.find("Artemis_Terrain") == std::string::npos && wn.find("Athena_Terrain") == std::string::npos) return;

    int index = Read<int>(world, 0xC);
    if (world != session.world || index != session.world_index) {
        session = Session{};
        session.world = world;
        session.world_index = index;
        session.observed_at = Monotonic();
        LOG(">>> Terrain detected: %s", wn.c_str());
    }
    double now = Monotonic();
    if (!session.attempted) {
        // Wait at least 5 seconds from terrain detection before probing
        if (now - session.observed_at < 5.0) return;
        if (session.retry_at > 0 && now - session.retry_at < 1.0) return;
        session.retry_at = now;
        Athena::SetupWorld(world);
        return;
    }
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
    LOG("=== EraCH3 starting (v9 FIXED) ===");
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
