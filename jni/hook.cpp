// ============================================================================
// EraCH3 — Fortnite 21.30 CL-21088273 Android
// COMPLETE FINAL BUILD — every function restored + all SDK fixes
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
constexpr uintptr_t GetDataTableRow_Orig = 0x09319C70;

constexpr unsigned ObjectClass  = 0x10;
constexpr unsigned ObjectName   = 0x18;
constexpr unsigned FieldNext    = 0x28;
constexpr unsigned StructSuper  = 0x40;
constexpr unsigned StructChildren   = 0x48;
constexpr unsigned StructChildProps = 0x50;
constexpr unsigned StructSize       = 0x58;

constexpr unsigned FunctionParamSize = 0xB6;
constexpr unsigned PropElementSize = 0x34;
constexpr unsigned PropFlags       = 0x38;
constexpr unsigned PropOffset      = 0x44;

constexpr unsigned BoolSize        = 0x80;
constexpr unsigned BoolByteOffset  = 0x81;
constexpr unsigned BoolFieldMask   = 0x82;

constexpr unsigned FFieldNext      = 0x20;
constexpr unsigned FFieldName      = 0x28;

constexpr uint64_t CPF_Parm       = 0x00000080ULL;
constexpr uint64_t CPF_ReturnParm = 0x00000400ULL;

constexpr unsigned ItemEntry_Durability    = 0x24;
constexpr unsigned ItemEntry_LoadedAmmo    = 0x2C;
constexpr unsigned ItemEntry_PhantomAmmo   = 0x30;
constexpr unsigned ItemEntry_TrackerGuid   = 0x78;
constexpr unsigned ItemEntry_StateValues   = 0xD0;

constexpr unsigned FortPlayerState_HeroType = 0x0430;
constexpr unsigned FortPlayerController_MyPlayerInfo = 0x2088;
constexpr unsigned RegisteredPlayerInfo_HeroInstance = 0x2D0;
constexpr unsigned FortHero_CharacterParts           = 0x238;
constexpr unsigned FortPlayerPawn_bIsSkydiving = 0x1A1D;
constexpr unsigned BuildingWall_bDoorOpen_Byte = 0x0FDE;
constexpr unsigned BuildingWall_bDoorOpen_Bit  = 5;
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
template<class T> static T Read(const void* p, size_t off=0) {
    T r{}; if(p) memcpy(&r, (const char*)p+off, sizeof(T)); return r;
}
template<class T> static void Write(void* p, size_t off, const T& v) {
    if(p) memcpy((char*)p+off, &v, sizeof(T));
}
static uintptr_t g_base = 0;
static std::atomic<int> g_status{0};
static std::atomic<int> g_gameTid{0};
static thread_local bool g_internal = false;
struct Guard { bool s=g_internal; Guard(){g_internal=true;} ~Guard(){g_internal=s;} };
template<class F> static F Fn(uintptr_t rva) { return reinterpret_cast<F>(g_base+rva); }

// ============================================================================
// STRUCTS
// ============================================================================
struct FStringNative { TCHAR* Data; int32 Count; int32 Max; };
struct FUEString     { TCHAR* data; int32 length; int32 capacity; };
struct FName         { uint32 CompIdx; uint32 DispIdx; };
struct Array         { void* Data; int32 Count; int32 Max; };
template<class T> struct TArray {
    T* Data; int32 Count; int32 Max;
    T& operator[](int i){return Data[i];}
    int Num() const {return Count;}
};
struct Guid  { uint32 A, B, C, D; };
struct Vector  { double X, Y, Z; };
struct Rotator { double Pitch, Yaw, Roll; };
struct SoftObjRef { uint8 _pad[0x28]; };

struct alignas(16) Transform {
    double Rotation[4]{0,0,0,1};
    double Location[3]{0,0,0};
    uint8_t _padL[8]{};
    double Scale[3]{1,1,1};
    uint8_t _padS[8]{};
};
static_assert(sizeof(Vector)==24);
static_assert(sizeof(Transform)==96);

struct FUObjectItem { void* Object; int32 Flags; int32 Cluster; int32 Serial; };
struct GObjects_t {
    uint8 _pad0[0x10]; void** Objects; uint8 _pad1[0x8];
    int32 MaxElements; int32 NumElements; int32 MaxChunks; int32 NumChunks;
    void* GetById(uint32 id) {
        if (!Objects) return nullptr;
        uint32 ci = id/0x10000, wi = id%0x10000;
        if ((int32)ci >= NumChunks) return nullptr;
        auto* chunk = (FUObjectItem*)Objects[ci];
        return chunk ? chunk[wi].Object : nullptr;
    }
};

// ============================================================================
// FORWARD
// ============================================================================
static void* FindObj(const std::string&);
static std::string ObjName(void*);
static std::string ObjNameFromFName(FName);
static std::string ObjNameFromUEString(FUEString*);
static bool IsA(void*, void*);
static bool SubclassOf(void*, void*);
static void* GetProp(const char*);
static void* GetField(void*, void*);
static void SetBool(void*, const char*, bool);
static std::u16string ToWide(const std::string&);
static std::string Narrow(const char16_t*, int);

static bool IsA(void* obj, void* type) {
    if (!obj||!type||IsBadPtr(obj)||IsBadPtr(type)) return false;
    void* k = Read<void*>(obj, profile::ObjectClass);
    if (!k||IsBadPtr(k)) return false;
    std::string t = ObjName(type); if (t.empty()) return false;
    for (int d=0;d<64&&k&&!IsBadPtr(k);++d) {
        if (ObjName(k)==t) return true;
        void* s = Read<void*>(k, profile::StructSuper);
        if (s==k) break; k=s;
    }
    return false;
}
static bool SubclassOf(void* k, void* t) {
    if (!k||!t||IsBadPtr(k)||IsBadPtr(t)) return false;
    std::string tn = ObjName(t); if (tn.empty()) return false;
    for (int d=0;d<64&&k&&!IsBadPtr(k);++d) {
        if (ObjName(k)==tn) return true;
        void* s = Read<void*>(k, profile::StructSuper);
        if (s==k) break; k=s;
    }
    return false;
}
static std::u16string ToWide(const std::string& s){return std::u16string(s.begin(),s.end());}
static std::string Narrow(const char16_t* s, int m) {
    std::string o; if(s) for(int i=0;i<m&&s[i];++i) o.push_back(s[i]<128?(char)s[i]:'?');
    return o;
}
static void* FindObj(const std::string& path) {
    auto w = ToWide(path);
    return Fn<void*(*)(void*,void*,const char16_t*,bool)>(profile::StaticFindObject)(nullptr,nullptr,w.c_str(),false);
}
static bool SafeFieldName(void* f, uint32 off, std::string& out) {
    if (!f||IsBadPtr(f)) return false;
    uint32 ci = Read<uint32>(f,off+0), di = Read<uint32>(f,off+4);
    if (ci==0xFFFFFFFF||ci>0x8000000) return false;
    if (di==0xFFFFFFFF||di>0x8000000) return false;
    FStringNative ns{};
    Fn<void(*)(const void*,FStringNative*)>(profile::NameAppendString)((char*)f+off,&ns);
    out = Narrow(ns.Data,128);
    if (ns.Data) Fn<void(*)(void*)>(profile::Free)(ns.Data);
    return true;
}
static std::string ObjName(void* obj) {
    if (!obj||IsBadPtr(obj)) return {};
    uint32 ci = Read<uint32>(obj,profile::ObjectName+0);
    if (ci==0xFFFFFFFF||ci>0x8000000) return {};
    static pthread_mutex_t m = PTHREAD_MUTEX_INITIALIZER;
    static std::map<void*, std::string> cache;
    pthread_mutex_lock(&m);
    auto it = cache.find(obj);
    if (it != cache.end()) { std::string r = it->second; pthread_mutex_unlock(&m); return r; }
    pthread_mutex_unlock(&m);
    FStringNative s{};
    Fn<void(*)(const void*,FStringNative*)>(profile::NameAppendString)((char*)obj+profile::ObjectName,&s);
    std::string out = Narrow(s.Data, std::min(std::max(s.Count,0),512));
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
static void* GetProp(const char* path) {
    if (!path) return nullptr;
    std::string s(path);
    if (void* d = FindObj(s)) return d;
    auto pos = s.rfind('.'); if (pos==std::string::npos) return nullptr;
    std::string cp = s.substr(0,pos), pn = s.substr(pos+1);
    if (void* a = FindObj(cp+":"+pn)) return a;
    void* k = FindObj(cp); if (!k||IsBadPtr(k)) return nullptr;
    for (int d=0;d<32&&k&&!IsBadPtr(k);++d) {
        void* f = Read<void*>(k, profile::StructChildProps);
        for (int i=0;f&&!IsBadPtr(f)&&i<1024;++i) {
            std::string n;
            if (SafeFieldName(f,profile::FFieldName,n)&&n==pn) return f;
            f = Read<void*>(f,profile::FFieldNext);
        }
        void* c = Read<void*>(k, profile::StructChildren);
        for (int i=0;c&&!IsBadPtr(c)&&i<1024;++i) {
            std::string n;
            if (SafeFieldName(c,profile::ObjectName,n)&&n==pn) return c;
            c = Read<void*>(c,profile::FieldNext);
        }
        k = Read<void*>(k, profile::StructSuper);
    }
    return nullptr;
}
static void* GetField(void* obj, void* p) {
    return (obj&&p)?(char*)obj+Read<int>(p,profile::PropOffset):nullptr;
}
static void SetBool(void* obj, const char* path, bool en) {
    void* p = GetProp(path); if(!p||!obj) return;
    uint8_t sz = Read<uint8_t>(p,profile::BoolSize);
    uint8_t by = Read<uint8_t>(p,profile::BoolByteOffset);
    uint8_t mk = Read<uint8_t>(p,profile::BoolFieldMask);
    if (!sz||sz>8||by>=sz||!mk) return;
    auto d = (uint8_t*)GetField(obj,p)+by;
    *d = en?(*d|mk):(*d&~mk);
}

// ============================================================================
// CALL
// ============================================================================
using PE_t   = void(*)(void*,void*,void*);
using Tick_t = void(*)(void*,int,float);
using Exec_t = bool(*)(void*,void*,const char16_t*,void*);
static PE_t orig_PE = nullptr;
static Tick_t orig_Tick = nullptr;
static Exec_t orig_Exec = nullptr;

class Call {
    void* m_func = nullptr;
    struct Param { std::string name; int offset, size; uint64_t flags; };
    std::vector<Param> m_params;
    alignas(16) std::array<uint8_t,4096> m_data{};
    bool m_valid = false;
    void addp(void* p, uint64_t flags, int total, bool ffield) {
        if (!(flags&profile::CPF_Parm)) return;
        int off = Read<int>(p,profile::PropOffset);
        int sz = Read<int>(p,profile::PropElementSize);
        if (off<0||sz<1||off+sz>total) return;
        std::string n; int nameOff = ffield?profile::FFieldName:profile::ObjectName;
        if (!SafeFieldName(p,nameOff,n)) return;
        m_params.push_back({n,off,sz,flags});
    }
public:
    explicit Call(const char* path) {
        m_func = FindObj(path); if (!m_func) return;
        int total = Read<uint16_t>(m_func, profile::FunctionParamSize);
        if (total<0 || total>(int)m_data.size()) return;
        void* p = Read<void*>(m_func, profile::StructChildren);
        for (int i=0;p&&!IsBadPtr(p)&&i<128;++i) {
            addp(p, Read<uint64_t>(p,profile::PropFlags), total, false);
            p = Read<void*>(p, profile::FieldNext);
        }
        void* fp = Read<void*>(m_func, profile::StructChildProps);
        for (int i=0;fp&&!IsBadPtr(fp)&&i<128;++i) {
            addp(fp, Read<uint64_t>(fp,profile::PropFlags), total, true);
            fp = Read<void*>(fp, profile::FFieldNext);
        }
        m_valid = (total==0) || !m_params.empty();
    }
    bool ok() const {return m_valid;}
    void* func() const {return m_func;}
    void dump(const char* label) const {
        LOG("[Call %s] valid=%d params=%zu", label, (int)m_valid, m_params.size());
        for (auto& p : m_params) LOG("   '%s' off=%d size=%d flags=%llx", p.name.c_str(), p.offset, p.size, (unsigned long long)p.flags);
    }
    template<class T> bool set(const char* key, const T& v) {
        for (auto& p : m_params)
            if (p.name==key && p.size==(int)sizeof(T)) {
                memcpy(m_data.data()+p.offset, &v, sizeof(T));
                return true;
            }
        return false;
    }
    template<class T> T result() const {
        for (auto& p : m_params)
            if ((p.flags&profile::CPF_ReturnParm) && p.size==(int)sizeof(T))
                return Read<T>(m_data.data(), p.offset);
        return {};
    }
    bool run(void* obj) {
        if (!m_valid||!obj||!orig_PE) return false;
        Guard g;
        orig_PE(obj, m_func, m_data.data());
        return true;
    }
};

// ============================================================================
// WRAPPERS
// ============================================================================
static void* LoadClass(const char* path, const char* base_path="/Script/Engine.Actor") {
    if (void* r=FindObj(path)) return r;
    auto s = ToWide(path);
    void* parent = FindObj(base_path); if (!parent) return nullptr;
    return Fn<void*(*)(void*,void*,const char16_t*,const char16_t*,uint32_t,void*)>(
        profile::StaticLoadClass)(parent,nullptr,s.c_str(),nullptr,0,nullptr);
}
static void* LoadAsset(const std::string& path, const char* type_path="/Script/FortniteGame.FortItemDefinition") {
    if (void* r=FindObj(path)) return r;
    auto s = ToWide(path);
    void* type = FindObj(type_path); if (!type) return nullptr;
    return Fn<void*(*)(void*,void*,const char16_t*,const char16_t*,uint32_t,void*,bool,void*)>(
        profile::StaticLoadObject)(type,nullptr,s.c_str(),nullptr,0,nullptr,true,nullptr);
}
static void* GetPawn(void* ctrl) {
    if (!ctrl) return nullptr;
    static void* p = nullptr; static bool r = false;
    if (!r) { p = GetProp("/Script/Engine.Controller.Pawn"); r = true; }
    return p ? Read<void*>(GetField(ctrl, p)) : nullptr;
}
static void Possess(void* ctrl, void* pawn) {
    if (!ctrl||!pawn) return;
    Call c("/Script/Engine.Controller.Possess");
    if (c.ok() && c.set("InPawn", pawn)) c.run(ctrl);
}
static void* GetPC(void* world, int idx=0) {
    return world ? Fn<void*(*)(void*,int)>(profile::GetPlayerController)(world,idx) : nullptr;
}
static void* GetObjField(void* obj, const char* path) {
    void* p = GetProp(path);
    if (!p||!obj) return nullptr;
    if (Read<int>(p,profile::PropElementSize)!=8) return nullptr;
    return Read<void*>(GetField(obj, p));
}
static void* SpawnActor(void* world, void* klass, void* owner, const Transform& t) {
    if (!world||!klass) return nullptr;
    void* a = Fn<void*(*)(void*,void*,const Transform*,uint8_t,void*)>(profile::BeginSpawn)(world,klass,&t,1,owner);
    return a ? Fn<void*(*)(void*,const Transform*)>(profile::FinishSpawn)(a,&t) : nullptr;
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
    void* world=nullptr; int world_index=-1;
    void* controller=nullptr; void* mode=nullptr; void* state=nullptr;
    void* pawn=nullptr; void* inventory=nullptr; void* quickbars=nullptr;
    bool initialized=false, inventory_ready=false, aircraft_jumped=false;
    bool attempted=false, finalized=false, cheats_ready=false;
    bool safezone_spawned=false, sprint_applied=false;
    bool ready_called=false, visibility_forced=false;
    double observed_at=0, retry_at=0, finalize_at=0;
    std::array<void*,6> slots{};
    std::vector<void*> character_parts;
    void* edit_tool=nullptr;
    void* wall=nullptr; void* floor=nullptr; void* stairs=nullptr; void* roof=nullptr;
} session;

static double Monotonic() {
    timespec ts{}; clock_gettime(CLOCK_MONOTONIC,&ts);
    return double(ts.tv_sec)+double(ts.tv_nsec)*1e-9;
}

// ============================================================================
// ATHENA NAMESPACE
// ============================================================================
namespace Athena {
    void SetupWorld(void*);
    void Tick();
    bool TryInitializePlayer(void*, void*, void*);
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
    void SpawnPickup(void* def, int count, Vector loc, bool toss=true);
    void SpawnPickupEntry(void* def, int count, Vector loc, void* pawn);
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

    struct LootData { void* Def; int Count; };
    std::vector<LootData> PickLootDrops(std::string category);
}

// ============================================================================
// 🔧 FIX 3: BeginSkydiving
// ============================================================================
void Athena::BeginPlayerSkydiving() {
    if (!session.pawn || IsBadPtr(session.pawn)) return;
    Call bsd("/Script/FortniteGame.FortPlayerPawn.BeginSkydiving");
    if (bsd.ok()) {
        bool bFromBus = true;
        if (bsd.set("bFromBus", bFromBus)) bsd.run(session.pawn);
        else bsd.dump("BeginSkydiving");
    }
    Write<uint8_t>(session.pawn, profile::FortPlayerPawn_bIsSkydiving, 1);
    LOG("[Skydive] BeginSkydiving dispatched");
}

// ============================================================================
// SetupCheats
// ============================================================================
void Athena::SetupCheats() {
    void* pc = session.controller; if (!pc) return;
    void* p = GetProp("/Script/Engine.PlayerController.CheatManager");
    void* k = FindObj("/Script/FortniteGame.FortCheatManager");
    if (!k) k = FindObj("/Script/Engine.CheatManager");
    if (!p||!k||Read<int>(p,profile::PropElementSize)!=8) {
        LOG("[Cheat] property/class missing"); return;
    }
    void* cm = Read<void*>(GetField(pc, p));
    if (!cm) {
        cm = Fn<void*(*)(void*,void*)>(profile::SpawnObject)(k, pc);
        if (cm) Write<void*>(GetField(pc, p), 0, cm);
    }
    LOG("[Cheat] manager=%p class=%s", cm,
        ObjName(cm ? Read<void*>(cm, profile::ObjectClass) : nullptr).c_str());
    if (cm) {
        Call i("/Script/Engine.CheatManager.ReceiveInitCheatManager");
        if (i.ok()) i.run(cm);
        session.cheats_ready = true;
    }
}

// ============================================================================
// Door
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
    LOG("[Door] %s (0xFDE=0x%02X)", wasOpen?"CLOSED":"OPENED", *byte);
    Call r("/Script/FortniteGame.BuildingWall.OnRep_bDoorOpen");
    if (r.ok()) r.run(door);
}

// ============================================================================
// 🔧 FIX 4: LoadProfileCosmetics
// ============================================================================
void Athena::LoadProfileCosmetics() {
    if (!session.pawn || !session.controller) return;
    LOG("[Profile] Loading cosmetics...");

    bool applied = false;
    void* heroType = nullptr;

    // Path 1: MyPlayerInfo → TempAthenaMenuHeroInstance → UFortHero.CharacterParts @ 0x238
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

            // Get HeroType via GetHeroTypeBP
            Call ght("/Script/FortniteGame.FortHero.GetHeroTypeBP");
            if (ght.ok() && ght.run(hero)) {
                heroType = ght.result<void*>();
                LOG("[Profile] HeroType=%s", ObjName(heroType).c_str());
            }
        }
    }

    // Path 2: PlayerState.HeroType @ 0x430
    if (!heroType || IsBadPtr(heroType)) {
        void* st = GetObjField(session.controller, "/Script/Engine.Controller.PlayerState");
        if (st && !IsBadPtr(st)) {
            heroType = Read<void*>(st, profile::FortPlayerState_HeroType);
            LOG("[Profile] HeroType@0x430 = %s", ObjName(heroType).c_str());

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
        LOG("[Profile] Using hardcoded fallback");
        LoadDefaultCosmetics();
        return;
    }

    // Inject HeroType + OnReps + ApplyCharacterCosmetics + UpdateVisualization
    void* state = GetObjField(session.controller, "/Script/Engine.Controller.PlayerState");
    if (state && !IsBadPtr(state)) {
        if (heroType && !IsBadPtr(heroType)) {
            Write<void*>(state, profile::FortPlayerState_HeroType, heroType);
            LOG("[Profile] HeroType injected into PlayerState");
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
            }
        }

        Call r1("/Script/FortniteGame.FortPlayerState.OnRep_HeroType");   if (r1.ok()) r1.run(state);
        Call r2("/Script/FortniteGame.FortPlayerState.OnRep_CharacterParts"); if (r2.ok()) r2.run(state);
        Call r3("/Script/FortniteGame.FortPlayerState.OnRep_CharacterData");  if (r3.ok()) r3.run(state);

        void* klib = FindObj("/Script/FortniteGame.Default__FortKismetLibrary");
        if (klib) {
            Call upd("/Script/FortniteGame.FortKismetLibrary.UpdatePlayerCustomCharacterPartsVisualization");
            if (upd.ok() && upd.set("PlayerState", state)) {
                upd.run(klib);
                LOG("[Profile] 🔧 UpdatePlayerCustomCharacterPartsVisualization DISPATCHED");
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
// LoadDefaultCosmetics
// ============================================================================
void Athena::LoadDefaultCosmetics() {
    if (!session.pawn || !session.controller) return;
    LOG("[Cosmetics] Loading hardcoded parts...");

    const char* partPaths[] = {
        "/Game/Athena/Heroes/Meshes/Heads/F_Med_Head1_ATH.F_Med_Head1_ATH",
        "/Game/Athena/Heroes/Meshes/Bodies/CP_001_Athena_Body.CP_001_Athena_Body",
    };
    void* loaded[2]{};
    for (int i = 0; i < 2; ++i) {
        void* asset = LoadAsset(partPaths[i], "/Script/FortniteGame.CustomCharacterPart");
        if (!asset) { LOG("[Cosmetics] Missing: %s", partPaths[i]); continue; }
        loaded[i] = asset;
        Call choose("/Script/FortniteGame.FortPlayerPawn.ServerChoosePart");
        if (choose.ok() && choose.set("Part", (uint8_t)i) && choose.set("ChosenCharacterPart", asset)) {
            choose.run(session.pawn);
            if (std::find(session.character_parts.begin(), session.character_parts.end(), asset) == session.character_parts.end())
                session.character_parts.push_back(asset);
            LOG("[Cosmetics] Applied part[%d]: %s", i, ObjName(asset).c_str());
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
            if (klib) apply.run(klib);
        }

        void* hero = LoadAsset("/Game/Athena/Heroes/HID_001_Athena_Commando_F.HID_001_Athena_Commando_F",
                               "/Script/FortniteGame.FortHeroType");
        if (!hero) hero = LoadAsset("/Game/Athena/Heroes/HID_001_Athena_Commando_M.HID_001_Athena_Commando_M",
                                     "/Script/FortniteGame.FortHeroType");
        if (hero) Write<void*>(state, profile::FortPlayerState_HeroType, hero);
    }

    Call r1("/Script/FortniteGame.FortPlayerState.OnRep_HeroType");   if (r1.ok()) r1.run(state);
    Call r2("/Script/FortniteGame.FortPlayerState.OnRep_CharacterParts"); if (r2.ok()) r2.run(state);
    Call r3("/Script/FortniteGame.FortPlayerState.OnRep_CharacterData");  if (r3.ok()) r3.run(state);

    void* klib = FindObj("/Script/FortniteGame.Default__FortKismetLibrary");
    if (klib) {
        Call upd("/Script/FortniteGame.FortKismetLibrary.UpdatePlayerCustomCharacterPartsVisualization");
        if (upd.ok() && upd.set("PlayerState", state)) upd.run(klib);
    }

    Call cl("/Script/FortniteGame.FortPlayerPawn.OnRep_CustomizationLoadout");
    if (cl.ok()) cl.run(session.pawn);
}

void Athena::ShowSkin() {
    LoadProfileCosmetics();
}

// ============================================================================
// 🔧 FIX 4: ForcePlayerVisible
// ============================================================================
void Athena::ForcePlayerVisible() {
    if (!session.pawn || IsBadPtr(session.pawn)) return;
    LOG("[Visibility] Forcing...");

    // End any skydiving state
    SetBool(session.pawn, "/Script/FortniteGame.FortPlayerPawn.bIsSkydiving", false);
    SetBool(session.pawn, "/Script/FortniteGame.FortPlayerPawn.bIsParachuteOpen", false);

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
    if (hid.ok()) { bool h = false; if (hid.set("bNewHidden", h)) hid.run(session.pawn); }
    SetBool(session.pawn, "/Script/Engine.Actor.bHidden", false);

    ShowSkin();
    session.visibility_forced = true;
    LOG("[Visibility] Done");
}

// ============================================================================
// 🔧 FIX 3: TryInitializePlayer
// ============================================================================
bool Athena::TryInitializePlayer(void* world, void* mode, void* ctrl) {
    void* pawnCls = LoadClass("/Game/Athena/PlayerPawn_Athena.PlayerPawn_Athena_C", "/Script/Engine.Pawn");
    if (!pawnCls) { LOG("[Init] pawn class missing"); return false; }

    void* existing = GetPawn(ctrl);
    if (existing && !IsBadPtr(existing) && IsA(existing, pawnCls)) {
        session.pawn = existing;
        LOG("[Init] Using existing pawn: %s", ObjName(existing).c_str());
        return true;
    }

    Transform t{};
    t.Location[0]=0.0; t.Location[1]=0.0; t.Location[2]=500.0;
    bool haveSpawn = false;

    Call choose("/Script/Engine.GameModeBase.ChoosePlayerStart");
    if (choose.ok() && choose.set("Player", ctrl) && choose.run(mode)) {
        void* start = choose.result<void*>();
        if (start && !IsBadPtr(start)) {
            Call loc("/Script/Engine.Actor.K2_GetActorLocation");
            if (loc.ok() && loc.run(start)) {
                Vector v = loc.result<Vector>();
                if (std::isfinite(v.X) && std::isfinite(v.Y) && std::isfinite(v.Z)) {
                    t.Location[0]=v.X; t.Location[1]=v.Y; t.Location[2]=v.Z+100.0;
                    haveSpawn = true;
                    LOG("[Init] PlayerStart (%.0f,%.0f,%.0f)", v.X, v.Y, v.Z);
                }
            }
        }
    }
    if (!haveSpawn) {
        t.Location[0]=0.0; t.Location[1]=0.0; t.Location[2]=3100.0;
        LOG("[Init] Fallback spawn (0,0,3100)");
    }

    session.pawn = SpawnActor(world, pawnCls, ctrl, t);
    if (!session.pawn) { LOG("[Init] Spawn failed"); return false; }
    LOG("[Init] Spawned: %p (%s)", session.pawn, ObjName(session.pawn).c_str());

    // Ensure not skydiving at spawn
    SetBool(session.pawn, "/Script/FortniteGame.FortPlayerPawn.bIsSkydiving", false);
    SetBool(session.pawn, "/Script/FortniteGame.FortPlayerPawn.bIsParachuteOpen", false);

    Possess(ctrl, session.pawn);
    if (GetPawn(ctrl) != session.pawn) { LOG("[Init] Possess failed"); return false; }
    LOG("[Init] Possessed OK");

    SetupCheats();

    Call restart("/Script/Engine.PlayerController.ClientRestart");
    if (restart.ok() && restart.set("NewPawn", session.pawn)) restart.run(ctrl);

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
    if (!baseCtrl || !IsA(session.controller, baseCtrl)) { LOG("SetupWorld: wrong ctrl"); return; }

    void* mp = GetProp("/Script/Engine.World.AuthorityGameMode");
    void* sp = GetProp("/Script/Engine.World.GameState");
    session.mode = mp ? Read<void*>(GetField(world, mp)) : nullptr;
    session.state = sp ? Read<void*>(GetField(world, sp)) : nullptr;
    if (!session.mode || !session.state) { LOG("SetupWorld: mode/state missing"); return; }

    LOG("SetupWorld: PC=%s GM=%s GS=%s",
        ObjName(session.controller).c_str(),
        ObjName(session.mode).c_str(),
        ObjName(session.state).c_str());

    if (!TryInitializePlayer(world, session.mode, session.controller)) { LOG("SetupWorld: retry"); return; }

    ShowSkin();
    DestroyLods();
    InitializeInventory();
    GrantDefaultAbilities();
    ApplySprintSpeed();
    SpawnSafeZoneIndicator();
    SpawnBuildPreviews();
    DropLoadingScreen();

    session.finalize_at = Monotonic() + 2.0;
    session.attempted = session.initialized = true;
    LOG("SetupWorld: initial pass done");
}

// ============================================================================
// 🔧 FIX 4: FinalizePlayer
// ============================================================================
void Athena::FinalizePlayer() {
    if (session.finalized) return;
    session.finalized = true;
    LOG("=== FinalizePlayer ===");

    void* pc = GetPC(session.world, 0);
    if (pc != session.controller) { LOG("Finalize: ctrl changed"); return; }

    void* pawnCls = FindObj("/Script/FortniteGame.FortPlayerPawnAthena");
    void* pawn = GetPawn(pc);
    if (!pawn || !pawnCls || !IsA(pawn, pawnCls)) { LOG("Finalize: pawn type wrong"); return; }
    session.pawn = pawn;

    Call loc("/Script/Engine.Actor.K2_GetActorLocation");
    if (loc.ok() && loc.run(session.pawn)) {
        Vector v = loc.result<Vector>();
        LOG("Finalize: pawn at (%.0f,%.0f,%.0f)", v.X, v.Y, v.Z);
        if (v.Z < -1000.0) {
            LOG("Finalize: underground, snapping");
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
                    }
                }
            }
        }
    }

    if (!session.ready_called) {
        Call srt("/Script/FortniteGame.FortPlayerController.ServerReadyToStartMatch");
        if (srt.ok()) srt.run(pc);
        Call cps("/Script/FortniteGame.FortPlayerControllerZone.ClientOnPawnSpawned");
        if (cps.ok()) cps.run(pc);
        session.ready_called = true;
        LOG("Finalize: ready callbacks sent");
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
// Tick
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
    static double lastVis = 0, lastSpeed = 0;
    if (session.finalized && now - lastVis > 8.0) {
        lastVis = now;
        if (!session.visibility_forced) ForcePlayerVisible();
    }
    if (session.finalized && session.pawn && now - lastSpeed > 2.0) {
        lastSpeed = now;
        ApplySprintSpeed();
    }
}

// ============================================================================
// DestroyLods
// ============================================================================
void Athena::DestroyLods() {
    void* cm = GetObjField(session.controller, "/Script/Engine.PlayerController.CheatManager");
    if (!cm) { SetupCheats(); cm = GetObjField(session.controller, "/Script/Engine.PlayerController.CheatManager"); }
    if (!cm) { LOG("DestroyLods: no cm"); return; }
    void* k = FindObj("/Script/FortniteGame.FortHLODSMActor");
    if (!k) return;
    Call c("/Script/Engine.CheatManager.DestroyAll");
    if (c.ok() && c.set("aClass", k)) c.run(cm);
    LOG("DestroyLods: done");
}

// ============================================================================
// DropLoadingScreen
// ============================================================================
void Athena::DropLoadingScreen() {
    if (!session.state || !session.controller) return;
    void* gp = GetProp("/Script/FortniteGame.FortGameStateAthena.GamePhase");
    if (gp && Read<int>(gp,profile::PropElementSize)==1) {
        uint8_t old = Read<uint8_t>(GetField(session.state, gp));
        if (old < 2) {
            uint8_t ph = 2;
            Write<uint8_t>(GetField(session.state, gp), 0, ph);
            Call n("/Script/FortniteGame.FortGameStateAthena.OnRep_GamePhase");
            if (n.ok() && n.set("OldGamePhase", old)) n.run(session.state);
        }
    }
    SetBool(session.controller, "/Script/FortniteGame.FortPlayerController.bHasServerFinishedLoading", true);
    SetBool(session.controller, "/Script/FortniteGame.FortPlayerController.bHasClientFinishedLoading", true);

    // Playlist
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
        Call srt("/Script/FortniteGame.FortPlayerController.ServerReadyToStartMatch");
        if (srt.ok()) srt.run(session.controller);
        Call cps("/Script/FortniteGame.FortPlayerControllerZone.ClientOnPawnSpawned");
        if (cps.ok()) cps.run(session.controller);
        session.ready_called = true;
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
    int off = Read<int>(p, profile::PropOffset);
    if (off<0 || off>0x20000) return;
    int fixed = 0;
    for (int32 i = 0; i < go->NumElements; ++i) {
        void* o = go->GetById(i);
        if (!o || IsBadPtr(o)) continue;
        void* k = Read<void*>(o, profile::ObjectClass);
        if (k != cls) continue;
        Write<char>((char*)o + off, 0, 0);
        ++fixed;
    }
    LOG("FixBuildingFoundations: %d", fixed);
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
        bool bS = false, bT = true;
        sl.set("NewLocation", loc);
        sl.set("NewRotation", rot);
        sl.set("bSweep", bS);
        sl.set("bTeleport", bT);
        sl.run(session.pawn);
    }
    BeginPlayerSkydiving();
}

// ============================================================================
// InventoryUpdate
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

// ============================================================================
// 🔧 FIX 1: AddToInventory with FFortItemEntry initialization
// ============================================================================
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
    }

    Fn<int(*)(void*,void*,const void*)>(profile::GenericArrayAdd)(instArr, instProp, &inst);
    Fn<int(*)(void*,void*,const void*)>(profile::GenericArrayAdd)(entryArr, entryProp, entrySrc);

    Guid itemGuid{0,0,0,0};
    if (itemGuidProp) {
        itemGuid = Read<Guid>(GetField(entrySrc, itemGuidProp));
        if (!(itemGuid.A|itemGuid.B|itemGuid.C|itemGuid.D)) {
            void* klib = FindObj("/Script/Engine.Default__KismetGuidLibrary");
            if (klib) {
                Call g("/Script/Engine.KismetGuidLibrary.NewGuid");
                if (g.ok() && g.run(klib)) {
                    itemGuid = g.result<Guid>();
                    Write<Guid>(GetField(entrySrc, itemGuidProp), 0, itemGuid);
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
                if (itemsProp) {
                    void* itemsArr = GetField(slotObj, itemsProp);
                    Fn<int(*)(void*,void*,const void*)>(profile::GenericArrayAdd)(itemsArr, itemsProp, &itemGuid);
                }
                void* enabledProp = GetProp("/Script/FortniteGame.FortQuickBarSlot.bEnabled");
                if (enabledProp) {
                    uint8_t by = Read<uint8_t>(enabledProp, profile::BoolByteOffset);
                    uint8_t mk = Read<uint8_t>(enabledProp, profile::BoolFieldMask);
                    ((uint8_t*)slotObj)[by] |= mk;
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
    if (!invProp) { LOG("No WorldInventory prop"); return; }
    session.inventory = Read<void*>(GetField(session.controller, invProp));
    if (!session.inventory) {
        void* k = FindObj("/Script/FortniteGame.FortInventory");
        Transform t{};
        session.inventory = SpawnActor(session.world, k, session.controller, t);
        if (session.inventory) Write<void*>(GetField(session.controller, invProp), 0, session.inventory);
    }
    if (!session.inventory) { LOG("No inventory"); return; }

    void* qbProp = GetProp("/Script/FortniteGame.FortPlayerController.ClientQuickBars");
    if (qbProp) {
        session.quickbars = Read<void*>(GetField(session.controller, qbProp));
        if (!session.quickbars) {
            void* k = FindObj("/Script/FortniteGame.FortQuickBars");
            Transform t{};
            session.quickbars = SpawnActor(session.world, k, session.controller, t);
            if (session.quickbars) Write<void*>(GetField(session.controller, qbProp), 0, session.quickbars);
        }
    }
    if (session.quickbars) { Call so("/Script/FortniteGame.FortQuickBars.SetOwner"); if (so.ok()) so.run(session.quickbars); }
    { Call so("/Script/FortniteGame.FortInventory.SetOwner"); if (so.ok()) so.run(session.inventory); }

    LOG("Inventory ready: inv=%p qb=%p", session.inventory, session.quickbars);

    struct D { const char* p; int s; };
    D defs[] = {
        {"/Game/Athena/Items/Weapons/WID_Harvest_Pickaxe_Athena_C_T01.WID_Harvest_Pickaxe_Athena_C_T01", 0},
        {"/Game/Athena/Items/Weapons/WID_Assault_Auto_Athena_C_Ore_T03.WID_Assault_Auto_Athena_C_Ore_T03", 1},
        {"/Game/Items/Weapons/BuildingTools/BuildingItemData_Wall.BuildingItemData_Wall", 2},
        {"/Game/Items/Weapons/BuildingTools/BuildingItemData_Floor.BuildingItemData_Floor", 3},
        {"/Game/Items/Weapons/BuildingTools/BuildingItemData_Stair_W.BuildingItemData_Stair_W", 4},
    };
    for (auto& d : defs) {
        void* def = LoadAsset(d.p);
        if (!def) def = FindObj(d.p);
        if (def) AddToInventory(def, 1, d.s);
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
        void* d = LoadAsset(ammo[i]);
        if (d) AddToInventory(d, 999, 5);
    }

    const char* res[] = {
        "/Game/Items/ResourcePickups/WoodItemData.WoodItemData",
        "/Game/Items/ResourcePickups/StoneItemData.StoneItemData",
        "/Game/Items/ResourcePickups/MetalItemData.MetalItemData", nullptr};
    for (int i = 0; res[i]; ++i) {
        void* d = LoadAsset(res[i]);
        if (d) AddToInventory(d, 500, 10);
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
    { Call d("/Script/FortniteGame.FortItem.GetItemDefinitionBP"); if (d.ok() && d.run(item)) def = d.result<void*>(); }
    if (!def) { LOG("EquipSlot[%d]: no def", slot); return; }

    Guid guid{};
    { Call g("/Script/FortniteGame.FortItem.GetItemGuid"); if (g.ok() && g.run(item)) guid = g.result<Guid>(); }

    void* itemEntryProp = GetProp("/Script/FortniteGame.FortWorldItem.ItemEntry");
    Guid trackerGuid{};
    if (itemEntryProp) {
        void* entrySrc = GetField(item, itemEntryProp);
        if (entrySrc) trackerGuid = Read<Guid>(entrySrc, profile::ItemEntry_TrackerGuid);
    }

    Call eq("/Script/FortniteGame.FortPawn.EquipWeaponDefinition");
    if (!eq.ok()) { LOG("EquipSlot[%d]: binding failed", slot); eq.dump("EquipWeaponDefinition"); return; }

    bool bDisableAnim = false;
    eq.set("WeaponData", def);
    eq.set("ItemEntryGuid", guid);
    eq.set("TrackerGuid", trackerGuid);
    eq.set("bDisableEquipAnimation", bDisableAnim);

    if (eq.run(session.pawn)) {
        void* weapon = eq.result<void*>();
        LOG("EquipSlot[%d]: %s -> %p", slot, ObjName(def).c_str(), weapon);
    } else {
        LOG("EquipSlot[%d]: run failed", slot);
    }
}

// ============================================================================
// 🔧 FIX 5: GrantDefaultAbilities via GrantAndActivateAbilityOnSelf
// ============================================================================
void Athena::GrantAbility(void* klass) {
    if (!klass || !session.pawn) return;
    void* asc = GetObjField(session.pawn, "/Script/FortniteGame.FortPawn.AbilitySystemComponent");
    if (!asc) return;

    void* klib = FindObj("/Script/FortniteGame.Default__FortAbilitySystemBlueprintLibrary");
    if (!klib) return;

    Call c("/Script/FortniteGame.FortAbilitySystemBlueprintLibrary.GrantAndActivateAbilityOnSelf");
    if (!c.ok()) return;
    if (c.set("AbilitySystem", asc) && c.set("AbilityToGrant", klass)) {
        c.run(klib);
        LOG("[Ability] Granted: %s", ObjName(klass).c_str());
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
    void* p = GetProp("/Script/Engine.CharacterMovementComponent.MaxWalkSpeed");
    if (p) {
        float* v = (float*)GetField(cmc, p);
        if (v && !IsBadPtr(v)) {
            if (*v < 599.0f) LOG("ApplySprintSpeed: %.0f -> 600", *v);
            *v = 600.f;
            session.sprint_applied = true;
        }
    }
    void* fp = GetProp("/Script/Engine.CharacterMovementComponent.MaxFlySpeed");
    if (fp) { float* v = (float*)GetField(cmc, fp); if (v && !IsBadPtr(v)) *v = 1200.f; }
}

// ============================================================================
// SpawnSafeZoneIndicator
// ============================================================================
void Athena::SpawnSafeZoneIndicator() {
    if (session.safezone_spawned || !session.state || !session.world) return;
    void* existing = GetObjField(session.state, "/Script/FortniteGame.FortGameStateAthena.SafeZoneIndicator");
    if (existing && !IsBadPtr(existing)) { session.safezone_spawned = true; return; }
    void* k = FindObj("/Script/FortniteGame.FortSafeZoneIndicator");
    if (!k) { LOG("SafeZone class missing"); return; }
    Transform t{};
    void* ind = SpawnActor(session.world, k, session.state, t);
    if (!ind) { LOG("SafeZone spawn failed"); return; }
    void* p = GetProp("/Script/FortniteGame.FortGameStateAthena.SafeZoneIndicator");
    if (p) Write<void*>(GetField(session.state, p), 0, ind);
    Call n("/Script/FortniteGame.FortGameStateAthena.OnRep_SafeZoneIndicator");
    if (n.ok()) n.run(session.state);
    session.safezone_spawned = true;
    LOG("SafeZone spawned: %p", ind);
}

// ============================================================================
// 🔧 FIX 6: SpawnPickup
// ============================================================================
void Athena::SpawnPickup(void* def, int count, Vector loc, bool toss) {
    if (!def) return;
    void* k = FindObj("/Script/FortniteGame.FortPickupAthena");
    if (!k) return;
    Transform t{};
    t.Location[0]=loc.X; t.Location[1]=loc.Y; t.Location[2]=loc.Z;
    void* pick = SpawnActor(session.world, k, session.pawn, t);
    if (!pick) return;
    void* dp = GetProp("/Script/FortniteGame.FortPickup.ItemDefinition");
    void* cp = GetProp("/Script/FortniteGame.FortPickup.Count");
    if (dp) Write<void*>(GetField(pick, dp), 0, def);
    if (cp) Write<int>(GetField(pick, cp), 0, count);
    Call r("/Script/FortniteGame.FortPickup.OnRep_PrimaryPickupItemEntry");
    if (r.ok()) r.run(pick);
    Call tc("/Script/FortniteGame.FortPickup.TossPickup");
    if (tc.ok()) {
        tc.set("FinalLocation", loc);
        tc.set("ItemOwner", session.pawn);
        tc.set("OverrideMaxStackCount", -1);
        tc.set("bToss", toss);
        tc.set("bShouldCombinePickupsWhenTossCompletes", true);
        tc.run(pick);
    }
    LOG("Pickup %s x%d", ObjName(def).c_str(), count);
}

// ============================================================================
// 🔧 FIX 6: Loot with BounceContainer
// ============================================================================
void Athena::Loot(void* actor) {
    if (!actor) return;
    std::string tier = "None";
    void* bc = FindObj("/Script/FortniteGame.BuildingContainer");
    void* sd = FindObj("/Script/FortniteGame.FortAthenaSupplyDrop");

    if (bc && IsA(actor, bc)) {
        void* p = GetProp("/Script/FortniteGame.BuildingContainer.SearchLootTierGroup");
        if (p) tier = "Loot_AthenaTreasure";
    } else if (sd && IsA(actor, sd)) {
        tier = "Loot_AthenaSupplyDrop";
    }
    LOG("[Loot] chest=%p tier=%s", actor, tier.c_str());

    // Give basic items
    void* wood = LoadAsset("/Game/Items/ResourcePickups/WoodItemData.WoodItemData");
    void* stone = LoadAsset("/Game/Items/ResourcePickups/StoneItemData.StoneItemData");
    void* ammo = LoadAsset("/Game/Athena/Items/Ammo/AthenaAmmoDataBulletsMedium.AthenaAmmoDataBulletsMedium");
    Call loc("/Script/Engine.Actor.K2_GetActorLocation");
    Vector v{0.0, 0.0, 100.0};
    if (loc.ok() && loc.run(actor)) v = loc.result<Vector>();
    if (wood) SpawnPickup(wood, 30, v);
    if (stone) SpawnPickup(stone, 30, v);
    if (ammo) SpawnPickup(ammo, 60, v);

    // Mark searched
    SetBool(actor, "/Script/FortniteGame.BuildingContainer.bAlreadySearched", true);
    Call r1("/Script/FortniteGame.BuildingContainer.OnRep_bAlreadySearched");
    if (r1.ok()) r1.run(actor);

    // Bounce
    Call r2("/Script/FortniteGame.BuildingContainer.BounceContainer");
    if (r2.ok()) { r2.run(actor); LOG("[Loot] BounceContainer dispatched"); }

    LOG("[Loot] chest opened: %p", actor);
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
    Call d("/Script/Engine.Actor.K2_DestroyActor"); if (d.ok()) d.run(pickup);
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
        Guid c = g.result<Guid>();
        if (memcmp(&c, &guid, sizeof(Guid)) == 0) {
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
    void* pc = FindObj("BuildingPlayerPrimitivePreview");
    if (!pc || !session.controller) return;
    auto build = [&](const char* meshPath, char type) -> void* {
        Transform t{};
        void* actor = SpawnActor(session.world, pc, session.controller, t);
        if (!actor) return nullptr;
        void* mesh = LoadAsset(meshPath, nullptr);
        void* mc = nullptr;
        Call g("/Script/FortniteGame.BuildingActor.GetBuildingMeshComponent");
        if (g.ok() && g.run(actor)) mc = g.result<void*>();
        if (mc && mesh) {
            Call s("/Script/Engine.StaticMeshComponent.SetStaticMesh");
            if (s.ok() && s.set("NewMesh", mesh)) s.run(mc);
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
    void* cls = Read<void*>(params, 0);
    Vector loc = Read<Vector>(params, 0x10);
    if (!cls) return;
    Transform t{};
    t.Location[0]=loc.X; t.Location[1]=loc.Y; t.Location[2]=loc.Z;
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
    LOG("Built: %s", ObjName(cls).c_str());
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
    eq.set("WeaponData", session.edit_tool);
    eq.set("ItemEntryGuid", guid);
    Guid tg{};
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
    LOG("[Bus] Aircraft jump");

    Vector spawnLoc{0.0, 0.0, 50000.0};
    Call choose("/Script/Engine.GameModeBase.ChoosePlayerStart");
    if (choose.ok() && choose.set("Player", session.controller) && choose.run(session.mode)) {
        void* start = choose.result<void*>();
        if (start && !IsBadPtr(start)) {
            Call sl("/Script/Engine.Actor.K2_GetActorLocation");
            if (sl.ok() && sl.run(start)) {
                Vector v = sl.result<Vector>();
                spawnLoc.X = v.X; spawnLoc.Y = v.Y; spawnLoc.Z = 50000.0;
            }
        }
    }

    void* k = LoadClass("/Game/Athena/PlayerPawn_Athena.PlayerPawn_Athena_C", "/Script/Engine.Pawn");
    if (!k) return;
    Transform t{};
    t.Location[0]=spawnLoc.X; t.Location[1]=spawnLoc.Y; t.Location[2]=spawnLoc.Z;
    void* newPawn = SpawnActor(session.world, k, session.controller, t);
    if (!newPawn) return;
    Possess(session.controller, newPawn);
    session.pawn = newPawn;
    BeginPlayerSkydiving();
    ShowSkin();
    LOG("[Bus] Airborne at (%.0f,%.0f,%.0f)", spawnLoc.X, spawnLoc.Y, spawnLoc.Z);
}

// ============================================================================
// Misc
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
        bool bS = false, bT = true;
        sl.set("NewLocation", loc); sl.set("NewRotation", rot);
        sl.set("bSweep", bS); sl.set("bTeleport", bT);
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
        ref.set("Type", (char)6); ref.set("Animation", (char)3);
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

void Athena::OnExitVehicle() {
    if (!session.pawn) return;
    Call v("/Script/FortniteGame.FortPlayerPawn.ServerOnExitVehicle");
    if (v.ok()) {
        int32 ef = 0; bool bd = false;
        v.set("ExitForceBehavior", ef);
        v.set("bDestroyVehicleWhenForced", bd);
        v.run(session.pawn);
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
        Guid c = g.result<Guid>();
        if (memcmp(&c, &guid, sizeof(Guid)) == 0) {
            Fn<void(*)(void*,void*,int)>(profile::GenericArrayRemove)(instArr, instProp, i);
            Fn<void(*)(void*,void*,int)>(profile::GenericArrayRemove)(entryArr, entryProp, i);
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
static void* g_interactClass = nullptr;

static void pe_hook(void* obj, void* func, void* params) {
    if (!orig_PE) return;
    if (!g_internal && func && params && g_gameTid.load() == (int)gettid()) {
        if (!g_interactClass)
            g_interactClass = FindObj("/Script/FortniteGame.FortControllerComponent_Interaction");
        bool isPC = (obj == session.controller);
        bool isPawn = (obj == session.pawn);
        bool isInteract = (g_interactClass && IsA(obj, g_interactClass));
        if (isPC || isPawn || isInteract) {
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
                    void* bc = FindObj("/Script/FortniteGame.BuildingContainer");
                    void* wall = FindObj("/Script/FortniteGame.BuildingWall");
                    LOG("[Interact] target=%p class=%s", target,
                        ObjName(Read<void*>(target, profile::ObjectClass)).c_str());
                    if (bc && IsA(target, bc)) Athena::Loot(target);
                    else if (wall && IsA(target, wall)) Athena::HandleDoor(target);
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
    if (wn != lastWorld) { LOG(">>> WorldTick world: '%s'", wn.c_str()); lastWorld = wn; }
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

static bool exec_hook(void* engine, void* world, const char16_t* cmd, void* out) {
    if (!orig_Exec) return false;
    std::string c = Narrow(cmd, 256);
    std::string normalized = c;
    std::transform(normalized.begin(), normalized.end(), normalized.begin(), ::tolower);
    // Strip trailing spaces
    while (!normalized.empty() && (normalized.back()==' '||normalized.back()=='\t')) normalized.pop_back();
    if (!c.empty()) LOG("Exec: %s", c.c_str());

    // Intercept Athena travel
    if (normalized == "open athena_terrain" || normalized.rfind("open athena_terrain", 0) == 0) {
        LOG("Intercepting Athena travel");
        session = Session{};
        session.attempted = false;
        return orig_Exec(engine, world, u"open Athena_Terrain", out);
    }
    return orig_Exec(engine, world, cmd, out);
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
