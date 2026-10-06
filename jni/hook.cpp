#include <jni.h>
#include <pthread.h>
#include <dlfcn.h>
#include <unistd.h>
#include <cstring>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <cwchar>
#include <string>
#include <vector>
#include <map>
#include <algorithm>
#include <android/log.h>
#include "../third_party/include/dobby.h"

#define TAG "EraCH3"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__)

#define CURLOPT_SSL_VERIFYPEER 64
#define CURLOPT_SSL_VERIFYHOST 81
#define CURLOPT_URL 10002
#define REDIRECT_SCHEME "http"
#define REDIRECT_HOST "127.0.0.1:3551"

using int8  = int8_t;
using int16 = int16_t;
using int32 = int32_t;
using int64 = int64_t;
using uint8 = uint8_t;
using uint16 = uint16_t;
using uint32 = uint32_t;
using uint64 = uint64_t;

namespace RVAs {
    constexpr uint64_t ProcessEvent                  = 0x07611B5C;
    constexpr uint64_t GObjects                      = 0x0DCD6E08;
    constexpr uint64_t SpawnActor_Thunk              = 0x3217298;
    constexpr uint64_t SpawnActor_2_Thunk            = 0x9CE0828;
    constexpr uint64_t GetEngineVersion              = 0x945F4B4;
    constexpr uint64_t FNameToString                 = 0x74937C0;
    constexpr uint64_t FreeInternal                  = 0x7355370;
    constexpr uint64_t Realloc                       = 0x735E520;
    constexpr uint64_t CopyScriptStruct              = 0x7535848;
    constexpr uint64_t GetDataTableRowFromName_Thunk = 0x99CD07C;
    constexpr uint64_t GetDataTableRowFromName_ORIG  = 0x9319C70;
    constexpr uint64_t GetDataTableRowNames_ORIG     = 0x9319CD8;
    constexpr uint64_t UDataTable_FindRowUnchecked   = 0x359CB9C;
    constexpr uint64_t GenericArray_Remove           = 0x944B2DC;
    constexpr uint64_t UStruct_IsChildOf             = 0x753C14C;
    constexpr uint64_t GMalloc                       = 0xDC55AC0;
    constexpr uint64_t curl_easy_setopt              = 0x0B1CA0B8;
}

namespace offsets {
    constexpr int32 Class              = 0x10;
    constexpr int32 Name               = 0x18;
    constexpr int32 Outer              = 0x20;
    constexpr int32 Children           = 0x48;
    constexpr int32 Next               = 0x30;
    constexpr int32 SuperClass         = 0x40;
    constexpr int32 Offset             = 0x4C;
    constexpr int32 ParamsSize         = 0xE6;
    constexpr int32 ReturnValueOffset  = 0xE8;
    constexpr int32 StructSize         = 0x5C;
    constexpr int32 RowStruct          = 0x28;
    constexpr int32 ElementSize        = 0x34;
    constexpr int32 PropertyFlags      = 0x78;
    constexpr int32 Inner              = 0x70;
    constexpr int32 StructFlags        = 0x38;
    constexpr int32 StructOps          = 0x40;
}

// ============================================================
// FORWARD DECLARATIONS
// ============================================================
static void (*FNameToStringFn)(void*, void*) = nullptr;
static void (*FreeInternalFn)(void*) = nullptr;
static void* (*ReallocFn)(void*, size_t, uint32) = nullptr;

// ============================================================
// BASIC STRUCTS
// ============================================================
struct FName {
    uint32 ComparisonIndex;
    uint32 DisplayIndex;

    std::string ToString() {
        if (!FNameToStringFn) return "";
        // FString is {wchar_t* Data; int32 Count; int32 Max;}
        struct FS { wchar_t* Data; int32 Count; int32 Max; } out{};
        FNameToStringFn(this, &out);
        std::string s;
        if (out.Data && out.Count > 0) {
            s.reserve(out.Count);
            for (int32 i = 0; i < out.Count - 1; ++i) {
                wchar_t wc = out.Data[i];
                s.push_back(wc < 128 ? (char)wc : '?');
            }
        }
        if (out.Data && FreeInternalFn) FreeInternalFn(out.Data);
        return s;
    }
};

struct FString {
    wchar_t* Data;
    int32 Count;
    int32 Max;

    FString() : Data(nullptr), Count(0), Max(0) {}

    FString(const wchar_t* other) {
        if (!other) { Data = nullptr; Count = 0; Max = 0; return; }
        Data = const_cast<wchar_t*>(other);
        Count = (int32)wcslen(other) + 1;
        Max = Count;
    }

    std::string ToString() const {
        if (!Data) return {};
        std::string out;
        out.reserve(Count);
        for (int32 i = 0; i < Count - 1; ++i) {
            wchar_t wc = Data[i];
            out.push_back(wc < 128 ? (char)wc : '?');
        }
        return out;
    }

    std::wstring ToWString() const {
        if (!Data) return {};
        return std::wstring(Data, Data + (Count > 0 ? Count - 1 : 0));
    }
};

struct FVector { float X, Y, Z; };
struct FRotator { float Pitch, Yaw, Roll; };
struct FGuid { int32 A, B, C, D; };
struct FVector2D { float X, Y; };
struct FMargin { float Left, Top, Right, Bottom; };
struct FLinearColor { float R, G, B, A; };

struct FSlateColor {
    FLinearColor SpecifiedColor;
    uint8 ColorUseRule;
    uint8 _pad[0x17];
};

struct FSlateBrush {
    uint8 _pad[0x8];
    FVector2D ImageSize;
    FMargin Margin;
    FSlateColor TintColor;
    class UObject* ObjectResource;
};

struct FGameplayAbilitySpecDef {
    class UObject* Ability;
    uint8 _pad[0x90];
};

struct FGameplayEffectContextHandle { uint8 _pad[0x18]; };
struct FActiveGameplayEffectHandle { int32 Handle; bool bPassed; uint8 _pad[0x3]; };

struct FGameplayTagContainer {
    void* GameplayTags_Data;
    int32 GameplayTags_Count;
    int32 GameplayTags_Max;
    void* ParentTags_Data;
    int32 ParentTags_Count;
    int32 ParentTags_Max;
};

struct GEHard { class UObject* Effect; float Level; uint8 _pad[0x4]; };
struct FItemAndCount { int32 Count; uint8 _pad[0x4]; class UObject* Item; };
struct FKey { FName KeyName; uint8 _pad[0x10]; };
struct SpawnActorParams { uint8 Parms[0x40]; };
struct SoftObjectPtr { uint8 _pad[0x28]; };

class FFieldClass {
public:
    FName NamePrivate;
    uint8 Id;
    void* CastFlags;
    uint8 ClassFlags;
    FFieldClass* Super;
    void* DefaultObj;

    std::string GetName() {
        std::string name = NamePrivate.ToString();
        auto pos = name.rfind('/');
        return pos == std::string::npos ? name : name.substr(pos + 1);
    }
};

class FField {
public:
    void* VTablePointer;
    FFieldClass* ClassPointer;
    uint8 _pad[0x10];
    FField* Next;
    FName Name;
    uint8 Flags;

    bool IsValid() { return !IsBadPtr(&Name); }

    std::string GetName() {
        std::string name = Name.ToString();
        auto pos = name.rfind('/');
        return pos == std::string::npos ? name : name.substr(pos + 1);
    }
};

static inline bool IsBadPtr(const void* p) {
    if (!p) return true;
    return (uintptr_t)p < 0x10000 || (uintptr_t)p > 0x7FFFFFFFFFFF;
}

class UObject {
public:
    void** Vtable;
    int32 ObjectFlags;
    int32 InternalIndex;
    UObject* Class;
    FName Name;
    UObject* Outer;

    bool IsA(UObject* c) { return this->Class == c; }

    bool IsChild(UObject* parent) {
        UObject* c = this->Class;
        while (c) {
            if (c == parent) return true;
            c = *(UObject**)((uintptr_t)c + offsets::SuperClass);
        }
        return false;
    }

    std::string GetName() {
        std::string s = Name.ToString();
        auto pos = s.rfind('/');
        return pos == std::string::npos ? s : s.substr(pos + 1);
    }

    std::string GetFullName() {
        std::string temp;
        for (auto outer = this->Outer; outer; outer = outer->Outer) {
            temp = outer->GetName() + "." + temp;
        }
        return reinterpret_cast<UObject*>(Class)->GetName() + " " + temp + this->GetName();
    }

    int32 EnumIndex(const std::string& enumName) {
        auto* arr = reinterpret_cast<void*>(int64(this) + 0x40);
        int32 count = *(int32*)((uintptr_t)arr + 8);
        void* data = *(void**)arr;
        for (int i = 0; i < count; ++i) {
            void* entry = (void*)((uintptr_t)data + i * 0x10);
            FName* n = (FName*)entry;
            std::string s = n->ToString();
            auto pos = s.find(this->GetName() + "::");
            if (pos != std::string::npos) s = s.substr(pos + this->GetName().length() + 2);
            if (s == enumName) return i;
        }
        return 0;
    }

    template<typename T = UObject*>
    T& Child(const std::string& name, bool bFunction = false, bool bUseCache = true) {
        static std::map<std::string, int32> cache;
        std::string key = GetName() + "::" + name;

        if (bUseCache && cache.find(key) != cache.end()) {
            return *(T*)(int64(this) + cache[key]);
        }

        UObject* klass = this->Class;

        while (klass) {
            UObject* children = *(UObject**)(int64(klass) + offsets::Children);
            if (children) {
                UObject* cur = children;
                while (cur) {
                    std::string cn = cur->Class->GetName();
                    if (cn == "StructProperty") {
                        UObject* structChildren = *(UObject**)(int64(*(UObject**)(int64(cur) + offsets::Class)) + offsets::Children);
                        while (structChildren) {
                            if (structChildren->GetName() == name) {
                                return *(T*)(int64(this) + *(uint32*)(int64(cur) + offsets::Offset) + *(uint32*)(int64(structChildren) + offsets::Offset));
                            }
                            structChildren = *(UObject**)(int64(structChildren) + offsets::Next);
                        }
                    }
                    if (cur->GetName() == name) {
                        if (bFunction) return *(T*)(&*cur);
                        int32 off = *(int32*)(int64(cur) + offsets::Offset);
                        if (bUseCache) cache[key] = off;
                        return *(T*)(int64(this) + off);
                    }
                    cur = *(UObject**)(int64(cur) + offsets::Next);
                }
            }
            klass = *(UObject**)(int64(klass) + offsets::SuperClass);
        }

        klass = this->Class;
        while (klass) {
            FField* fp = *(FField**)(int64(klass) + 0x50);
            if (fp && fp->IsValid()) {
                while (fp->IsValid()) {
                    if (fp->ClassPointer->GetName() == "StructProperty") {
                        FField* sc = *(FField**)(int64(*(UObject**)(int64(fp) + offsets::Class)) + 0x50);
                        while (sc) {
                            if (sc->GetName() == name) {
                                return *(T*)(int64(this) + *(uint32*)(int64(fp) + 0x4C) + *(uint32*)(int64(sc) + 0x4C));
                            }
                            sc = sc->Next;
                        }
                    }
                    if (fp->GetName() == name) {
                        int32 off = *(int32*)(int64(fp) + 0x4C);
                        if (bUseCache) cache[key] = off;
                        return *(T*)(int64(this) + off);
                    }
                    fp = fp->Next;
                }
            }
            klass = *(UObject**)(int64(klass) + offsets::SuperClass);
        }

        return *(T*)(0);
    }

    std::vector<int32> GetFunctionChildrenOffset() {
        std::vector<int32> r;
        UObject* children = *(UObject**)(int64(this) + offsets::Children);
        FField* fp = *(FField**)(int64(this) + 0x50);
        while (children) {
            r.push_back(*(int32*)(int64(children) + offsets::Offset));
            children = *(UObject**)(int64(children) + offsets::Next);
        }
        while (fp && fp->IsValid()) {
            r.push_back(*(int32*)(int64(fp) + 0x4C));
            fp = fp->Next;
        }
        return r;
    }

    template<typename T = int, int16 ReturnOffset = -1, typename... Params>
    T Call(const std::string& name, Params... args) {
        extern void* (*PE)(UObject*, UObject*, void*);
        static std::map<std::string, UObject*> funcCache;
        UObject* func = nullptr;
        std::string key = GetName() + "::" + name;

        if (funcCache.find(key) != funcCache.end()) {
            func = funcCache[key];
        } else {
            func = &(this->Child<UObject>(name, true));
            funcCache[key] = func;
        }

        if (IsBadPtr(func)) return T{};

        int16 ps = *(int16*)(int64(func) + offsets::ParamsSize);
        int16 ro = *(int16*)(int64(func) + offsets::ReturnValueOffset);
        if (ro == -1) ro = ReturnOffset;

        uint8* params = (uint8*)calloc(1, ps);
        auto offs = func->GetFunctionChildrenOffset();

        int i = 0;
        auto fill = [&](auto& v) {
            if (i < (int)offs.size() && offs[i] > 0 && offs[i] < ps) {
                size_t sz = sizeof(v);
                if ((size_t)offs[i] + sz <= (size_t)ps)
                    memcpy(params + offs[i], &v, sz);
            }
            i++;
        };
        (fill(args), ...);

        PE(this, func, params);
        T r{};
        if (ro > 0 && ro < ps) r = *(T*)(params + ro);
        free(params);
        return r;
    }
};

struct FUObjectItem { UObject* Object; int32 Flags; int32 Cluster; int32 Serial; };
struct TUObjectArray { void* Objects[9]; };

struct GObjects_t {
    TUObjectArray* ObjectArray;
    uint8 _pad[0xC];
    uint32 ObjectCount;

    UObject* GetById(uint32 id) {
        const uint32 chunkSize = 0x10000;
        uint32 ci = id / chunkSize;
        uint32 wi = id % chunkSize;
        if (ci >= 9) return nullptr;
        auto* chunk = (FUObjectItem*)ObjectArray->Objects[ci];
        if (!chunk) return nullptr;
        return chunk[wi].Object;
    }
};

template<class T>
class TArray {
public:
    T* Data;
    int32 Count;
    int32 Max;

    void Add(T in) {
        extern void* (*ReallocFn)(void*, size_t, uint32);
        Data = (T*)ReallocFn(Data, sizeof(T) * (Count + 1), 0);
        Data[Count++] = in;
        Max = Count;
    }

    void AddWithSize(size_t sz, void* elem) {
        extern void* (*ReallocFn)(void*, size_t, uint32);
        uintptr_t oldBase = (uintptr_t)Data;
        Data = (T*)ReallocFn(Data, sz * (Count + 1), 0);
        memcpy((void*)((uintptr_t)Data + sz * Count), elem, sz);
        (void)oldBase;
        Count++;
        Max = Count;
    }

    T& operator[](int i) { return Data[i]; }
    const T& operator[](int i) const { return Data[i]; }
    int MaxIndex() { return Count - 1; }
    T* begin() { return Data; }
    T* end() { return Data + Count; }
};

struct Struct {
    template<typename T = UObject*>
    T& Child(UObject* StructObj, const std::string& vname) {
        static std::map<std::pair<UObject*, std::string>, int32> cache;
        auto key = std::make_pair(StructObj, vname);
        if (cache.find(key) != cache.end()) return *(T*)(int64(this) + cache[key]);

        UObject* klass = StructObj;
        while (klass) {
            UObject* c = *(UObject**)(int64(klass) + offsets::Children);
            if (c) {
                while (c) {
                    if (c->Class->GetName() == "StructProperty") {
                        UObject* sc = *(UObject**)(int64(*(UObject**)(int64(c) + offsets::Class)) + offsets::Children);
                        while (sc) {
                            if (sc->GetName() == vname) {
                                return *(T*)(int64(this) + *(uint32*)(int64(c) + offsets::Offset) + *(uint32*)(int64(sc) + offsets::Offset));
                            }
                            sc = *(UObject**)(int64(sc) + offsets::Next);
                        }
                    }
                    if (c->GetName() == vname) {
                        int32 off = *(int32*)(int64(c) + offsets::Offset);
                        cache[key] = off;
                        return *(T*)(int64(this) + off);
                    }
                    c = *(UObject**)(int64(c) + offsets::Next);
                }
            }
            klass = *(UObject**)(int64(klass) + offsets::SuperClass);
        }
        return *(T*)(0);
    }
};

// ============================================================
// GLOBALS
// ============================================================
static uintptr_t g_base = 0;
static void* (*PE)(UObject*, UObject*, void*) = nullptr;
static void* (*SpawnActorFn)(void*, UObject*, FVector*, FRotator*, const SpawnActorParams&) = nullptr;
static UObject* (*StaticFindObjectFn)(UObject*, void*, const wchar_t*, bool) = nullptr;
static UObject* (*StaticLoadObjectFn)(UObject*, UObject*, const wchar_t*, const wchar_t*, int, void*, bool) = nullptr;
static bool (*GetDataTableRowFn)(UObject*, FName, void*) = nullptr;
static void (*GenericArray_RemoveFn)(void*, void*, int) = nullptr;
static void* (*orig_ProcessEvent)(UObject*, UObject*, void*) = nullptr;

static UObject* PlayerController = nullptr;
static UObject* GameState = nullptr;
static UObject* GameMode = nullptr;
static UObject* World = nullptr;
static UObject* Pawn = nullptr;
static UObject* GameViewport = nullptr;
static UObject* CheatManager = nullptr;
static UObject* GameStatics = nullptr;
static UObject* kismetMathLib = nullptr;
static UObject* kismetStringLib = nullptr;
static UObject* kismetSystemLib = nullptr;
static UObject* kismetGuidLib = nullptr;
static UObject* DataTableFunctionLibrary = nullptr;
static UObject* WorldInventory = nullptr;
static UObject* Quickbars = nullptr;
static UObject* WorldSettings = nullptr;
static UObject* EditToolItem = nullptr;
static UObject* Wall = nullptr;
static UObject* Floor = nullptr;
static UObject* Stairs = nullptr;
static UObject* Roof = nullptr;
static std::vector<UObject*> CharacterPartsArray;

static GObjects_t* gGObjects = nullptr;

static bool bInFrontend = true;
static bool bPressedPlay = false;
static bool bLoadedInMatch = false;
static bool bDroppedFromAircraft = false;
static bool bIsEmoting = false;
static bool g_bIsVehicleVersion = false;
static bool l_bIsOutsideSafeZone = false;

static FVector EditComponentLocation;
static FRotator EditComponentRotation;

// ============================================================
// LIB HELPERS
// ============================================================
namespace Lib {
    static uintptr_t Find(const char* name) {
        FILE* f = fopen("/proc/self/maps", "r");
        if (!f) return 0;
        char line[512];
        uintptr_t base = 0;
        while (fgets(line, sizeof(line), f)) {
            if (strstr(line, name)) { base = strtoul(line, nullptr, 16); break; }
        }
        fclose(f);
        return base;
    }

    static uintptr_t WaitFor(const char* name, int t = 60) {
        for (int i = 0; i < t * 10; ++i) {
            uintptr_t b = Find(name);
            if (b) return b;
            usleep(100000);
        }
        return 0;
    }
}

static std::wstring ToWide(const std::string& s) {
    std::wstring w;
    for (char c : s) w.push_back((wchar_t)c);
    return w;
}

static UObject* FindObject(const wchar_t* path, bool bLoad = false, bool bAny = false) {
    if (!StaticFindObjectFn) return nullptr;
    UObject* o = StaticFindObjectFn(nullptr, bAny ? (void*)-1 : nullptr, path, false);
    if (!o && bLoad && StaticLoadObjectFn) {
        o = StaticLoadObjectFn(nullptr, nullptr, path, nullptr, 0, nullptr, true);
    }
    return o;
}

static UObject* SpawnActorEasy(UObject* klass, FVector loc, FRotator rot) {
    if (!klass) return nullptr;
    SpawnActorParams p{};
    return SpawnActorFn((void*)World, klass, &loc, &rot, p);
}

// ============================================================
// ATHENA NAMESPACE DECLARATIONS
// ============================================================
namespace Athena {
    void SpawnPawn();
    void ShowSkin();
    void DestroyLods();
    void DropLoadingScreen();
    void AddToInventory(UObject* item, int count, char index, int slot);
    void InitializeInventory();
    void OnServerExecuteInventoryItem(FGuid guid);
    void OnServerExecuteInventoryWeapon(UObject* weapon);
    void GrantDefaultAbilities();
    void InventoryUpdate();
    void OnAircraftJump();
    void RemoveNetDebugUI();
    void TeleportToSpawnIsland();
    void ConsoleKey();
    void CheatScript(const char* script);
    void ServerHandlePickup(UObject* pickup);
    void FixBuildingFoundations();
    void Fixbus();
    void Loot(UObject* actor);
    void OnServerCreateBuildingActor(void* params);
    void OnBeginEditActor(UObject* piece);
    void OnFinishEditActor(UObject* actor, UObject* newClass, int rot, bool mirrored, void* params);
    void SpawnPickup(UObject* def, int count, FVector loc, bool toss = true);
    void GrantAbility(UObject* klass);
    void SpawnBuildPreviews();
    void SpawnFloorLoot();
    void GrantEffect(UObject* effect, float level);
    void PlayEmoteItem(UObject* item);
    void Tick();
    void OnExitVehicle();
    void DropInventoryItem(FGuid guid, int count, bool checkCount = true);

    struct LootData { UObject* ItemDefinition; int Count; };
    std::vector<LootData> PickLootDrops(std::string category);
}

// ============================================================
// ATHENA IMPLEMENTATION
// ============================================================
void Athena::SpawnPawn() {
    Pawn = SpawnActorEasy(FindObject(L"/Game/Athena/PlayerPawn_Athena.PlayerPawn_Athena_C"), FVector{0, 0, 5000}, FRotator{0, 0, 0});
    if (PlayerController && Pawn) PlayerController->Call("Possess", Pawn);
}

void Athena::ShowSkin() {
    static UObject* Hero = nullptr;

    if (auto t = &PlayerController->Child<UObject*>("MyPlayerInfo")->Child<UObject*>("AthenaMenuHeroDef"); !IsBadPtr(t)) Hero = *t;
    if (auto t = &PlayerController->Child<UObject*>("MyPlayerInfo")->Child<UObject*>("TempAthenaMenuHeroInstance"); !IsBadPtr(t)) Hero = *t;
    if (!Hero) return;

    static auto charParts = Hero->Child<TArray<UObject*>>("CharacterParts");

    if (CharacterPartsArray.size() == 0) {
        for (UObject* cp : charParts) CharacterPartsArray.push_back(cp);
    }

    for (UObject* cp : CharacterPartsArray) {
        Pawn->Call("ServerChoosePart", cp->Child<char>("CharacterPartType"), cp);
    }

    PlayerController->Child<UObject*>("PlayerState")->Call("OnRep_CharacterParts");
    PlayerController->Child<UObject*>("PlayerState")->Call("OnRep_CharacterData");
}

void Athena::DestroyLods() {
    if (CheatManager) CheatManager->Call("DestroyAll", FindObject(L"/Script/FortniteGame.FortHLODSMActor"));
}

void Athena::DropLoadingScreen() {
    if (!GameState || !PlayerController) return;

    GameState->Child<char>("GamePhase") = 2;
    GameState->Call("OnRep_GamePhase", (char)0);
    PlayerController->Call("ServerReadyToStartMatch");

    PlayerController->Child<bool>("bHasServerFinishedLoading") = true;
    PlayerController->Child<bool>("bHasClientFinishedLoading") = true;

    PlayerController->Child<UObject*>("PlayerState")->Call("OnRep_SquadId");
    PlayerController->Child<UObject*>("PlayerState")->Child<char>("TeamIndex") = (char)2;
    PlayerController->Child<UObject*>("PlayerState")->Call("OnRep_TeamIndex");

    if (auto m = &GameState->Child<FSlateBrush>("MinimapNextCircleBrush"); !IsBadPtr(m)) *m = {};
    if (auto m = &GameState->Child<FSlateBrush>("FullMapNextCircleBrush"); !IsBadPtr(m)) *m = {};
    if (auto m = &GameState->Child<FSlateBrush>("MinimapSafeZoneBrush"); !IsBadPtr(m)) *m = {};
    if (auto m = &GameState->Child<FSlateBrush>("MinimapCircleBrush"); !IsBadPtr(m)) *m = {};
    if (auto m = &GameState->Child<FSlateBrush>("FullMapCircleBrush"); !IsBadPtr(m)) *m = {};
    if (auto m = &GameState->Child<FSlateBrush>("AircraftPathBrush"); !IsBadPtr(m)) m->ObjectResource = FindObject(L"/Game/Athena/HUD/MiniMap/M_BusPath.M_BusPath");
    if (auto m = &GameState->Child<FSlateBrush>("MinimapBackgroundBrush"); !IsBadPtr(m)) {
        auto ws = &WorldSettings->Child<FSlateBrush>("AthenaMapImage");
        m->ObjectResource = IsBadPtr(ws) ? nullptr : ws->ObjectResource;
    }
    if (auto m = &GameState->Child<UObject*>("MinimapMaterial"); !IsBadPtr(m)) {
        UObject* apollo = FindObject(L"/Game/Athena/Apollo/Maps/UI/M_MiniMapApollo.M_MiniMapApollo");
        *m = apollo ? apollo : FindObject(L"/Game/Athena/HUD/MiniMap/M_MiniMapAthena.M_MiniMapAthena");
    }

    UObject* playlist = FindObject(L"/Game/Athena/Playlists/Creative/Playlist_PlaygroundV2.Playlist_PlaygroundV2");
    if (!playlist) playlist = FindObject(L"/Game/Athena/Playlists/Playground/Playlist_Playground.Playlist_Playground");
    if (playlist) {
        playlist->Child<bool>("bIsLargeTeamGame") = true;

        if (kismetStringLib) {
            FName tag = kismetStringLib->Call<FName>("Conv_StringToName", FString(L"HUD.Athena.PlayersLeft"));
            // FGameplayTagContainer manipulation is version-specific; skip if problematic
            (void)tag;
        }

        if (auto bp = &GameState->Child<UObject*>("BasePlaylist"); !IsBadPtr(bp)) *bp = playlist;
        if (auto op = &GameState->Child<UObject*>("OverridePlaylist"); !IsBadPtr(op)) *op = playlist;
        if (auto cp = &GameState->Child<UObject*>("CurrentPlaylistData"); !IsBadPtr(cp)) *cp = playlist;
        GameState->Call("OnRep_CurrentPlaylistData");
        GameState->Call("OnRep_CurrentPlaylistInfo");
    }

    if (auto bus = FindObject(L"BBID_WinterBus", false, true); bus) {
        PlayerController->Child<UObject*>("BattleBus") = bus;
    }

    if (Pawn) Pawn->Call("OnRep_CustomizationLoadout");
}

void Athena::InventoryUpdate() {
    PlayerController->Call("HandleWorldInventoryLocalUpdate");
    if (WorldInventory) WorldInventory->Call("HandleInventoryLocalUpdate");
    PlayerController->Call("OnRep_QuickBar");
    if (Quickbars) {
        Quickbars->Call("OnRep_SecondaryQuickBar");
        Quickbars->Call("OnRep_PrimaryQuickBar");
    }
    PlayerController->Call("ClientForceUpdateQuickbar", (char)0);
    PlayerController->Call("ClientForceUpdateQuickbar", (char)1);
}

void Athena::AddToInventory(UObject* item, int count, char index, int slot) {
    if (IsBadPtr(item)) return;
    if (!WorldInventory || !Quickbars) return;

    UObject* itemInstance = item->Call<UObject*>("CreateTemporaryItemInstanceBP", count, 1);
    if (!itemInstance) return;

    WorldInventory->Child<TArray<UObject*>>("ItemInstances").Add(itemInstance);

    UObject* entryStruct = FindObject(L"/Script/FortniteGame.FortItemEntry");
    if (!entryStruct) return;
    int32 structSize = *(int32*)(int64(entryStruct) + offsets::StructSize);
    WorldInventory->Child<TArray<char>>("ReplicatedEntries").AddWithSize(structSize, &itemInstance->Child<void*>("ItemEntry"));

    itemInstance->Call("SetOwningControllerForTemporaryItem", PlayerController);

    FGuid guid = itemInstance->Call<FGuid>("GetItemGuid");
    Quickbars->Call("ServerAddItemInternal", guid, index, slot);
}

void Athena::InitializeInventory() {
    WorldInventory = PlayerController->Child<UObject*>("WorldInventory");

    if (auto qb = &PlayerController->Child<UObject*>("ClientQuickBars"); !IsBadPtr(qb)) {
        Quickbars = *qb;
    } else {
        Quickbars = SpawnActorEasy(FindObject(L"/Script/FortniteGame.FortQuickBars"), FVector{0, 0, 0}, FRotator{0, 0, 0});
        PlayerController->Child<UObject*>("QuickBars") = Quickbars;
    }

    if (Quickbars) Quickbars->Call("SetOwner", PlayerController);
    if (WorldInventory) WorldInventory->Call("SetOwner", PlayerController);

    auto startingItems = GameMode->Child<TArray<FItemAndCount>>("StartingItems");

    UObject* pickaxeWeapon = PlayerController->Child<UObject*>("Pickaxe")->Child<UObject*>("WeaponDefinition");
    AddToInventory(pickaxeWeapon, 1, 0, 0);
    if (startingItems.Count > 0) AddToInventory(startingItems[0].Item, 1, 1, 0);
    if (startingItems.Count > 1) AddToInventory(startingItems[1].Item, 1, 1, 1);
    if (startingItems.Count > 2) AddToInventory(startingItems[2].Item, 1, 1, 2);
    if (startingItems.Count > 3) AddToInventory(startingItems[3].Item, 1, 1, 3);
    if (startingItems.Count > 5) AddToInventory(startingItems[5].Item, 999, 3, 0);

    AddToInventory(FindObject(L"/Game/Athena/Items/Ammo/AthenaAmmoDataShells.AthenaAmmoDataShells"), 999, 3, 0);
    AddToInventory(FindObject(L"/Game/Athena/Items/Ammo/AthenaAmmoDataBulletsMedium.AthenaAmmoDataBulletsMedium"), 999, 3, 0);
    AddToInventory(FindObject(L"/Game/Athena/Items/Ammo/AthenaAmmoDataBulletsLight.AthenaAmmoDataBulletsLight"), 999, 3, 0);
    AddToInventory(FindObject(L"/Game/Athena/Items/Ammo/AthenaAmmoDataBulletsHeavy.AthenaAmmoDataBulletsHeavy"), 999, 3, 0);
    AddToInventory(FindObject(L"/Game/Athena/Items/Ammo/AmmoDataRockets.AmmoDataRockets"), 999, 3, 0);
    AddToInventory(FindObject(L"/Game/Athena/Items/Traps/TID_Floor_Player_Launch_Pad_Athena.TID_Floor_Player_Launch_Pad_Athena"), 1, 3, 0);
    AddToInventory(FindObject(L"/Game/Athena/Items/Traps/TID_Floor_Spikes_Athena_R_T03.TID_Floor_Spikes_Athena_R_T03"), 1, 3, 0);

    if (startingItems.Count > 4) EditToolItem = startingItems[4].Item;

    if (kismetStringLib && kismetSystemLib) {
        FName infAmmo = kismetStringLib->Call<FName>("Conv_StringToName", FString(L"bInfiniteAmmo"));
        FName buildFree = kismetStringLib->Call<FName>("Conv_StringToName", FString(L"bBuildFree"));
        kismetSystemLib->Call("SetBoolPropertyByName", PlayerController, infAmmo, true);
        kismetSystemLib->Call("SetBoolPropertyByName", PlayerController, buildFree, true);
    }

    InventoryUpdate();
}

void Athena::OnServerExecuteInventoryItem(FGuid guid) {
    auto instances = WorldInventory->Child<TArray<UObject*>>("ItemInstances");
    for (UObject* inst : instances) {
        if (kismetGuidLib->Call<bool>("EqualEqual_GuidGuid", inst->Call<FGuid>("GetItemGuid"), guid)) {
            UObject* def = inst->Call<UObject*>("GetItemDefinitionBP");

            if (def->IsA(FindObject(L"/Script/FortniteGame.FortTrapItemDefinition"))) {
                Pawn->Call<bool>("PickUpActor", nullptr, def);
                Pawn->Child<UObject*>("CurrentWeapon")->Child<FGuid>("ItemEntryGuid") = inst->Call<FGuid>("GetItemGuid");
            }
            else if (def->IsA(FindObject(L"AthenaGadgetItemDefinition", false, true)) ||
                     def->IsA(FindObject(L"FortGadgetItemDefinition", false, true))) {
                if (def->GetName() == "AGID_CarminePack") {
                    UObject* head = FindObject(L"Dev_TestAsset_Head_M_XL", false, true);
                    UObject* body = FindObject(L"Dev_TestAsset_Body_M_XL", false, true);
                    if (head) Pawn->Call("ServerChoosePart", head->Child<char>("CharacterPartType"), head);
                    if (body) Pawn->Call("ServerChoosePart", body->Child<char>("CharacterPartType"), body);
                }
                PlayerController->Child<UObject*>("PlayerState")->Call("OnRep_CharacterParts");
                PlayerController->Child<UObject*>("PlayerState")->Call("OnRep_CharacterData");

                UObject* abilitySet = kismetSystemLib->Call<UObject*>("Conv_SoftObjectReferenceToObject", def->Child<SoftObjectPtr>("AbilitySet"));
                if (abilitySet) {
                    for (auto* a : abilitySet->Child<TArray<UObject*>>("GameplayAbilities")) Athena::GrantAbility(a);
                    for (auto& e : abilitySet->Child<TArray<GEHard>>("GrantedGameplayEffects")) Athena::GrantEffect(e.Effect, e.Level);
                }

                Pawn->Child<UObject*>("Mesh")->Call("SetAnimInstanceClass", def->Child<UObject*>("AnimBPOverride"));
                Pawn->Call<UObject*>("EquipWeaponDefinition", def->Call<UObject*>("GetDecoItemDefinition"), inst->Call<FGuid>("GetItemGuid"));
            }
            else {
                Pawn->Call<UObject*>("EquipWeaponDefinition", def, inst->Call<FGuid>("GetItemGuid"));
            }
        }
    }
}

void Athena::OnServerExecuteInventoryWeapon(UObject* weapon) {
    if (!weapon) return;
    FGuid guid = weapon->Child<FGuid>("ItemEntryGuid");
    auto instances = WorldInventory->Child<TArray<UObject*>>("ItemInstances");
    for (UObject* inst : instances) {
        if (kismetGuidLib->Call<bool>("EqualEqual_GuidGuid", inst->Call<FGuid>("GetItemGuid"), guid)) {
            Pawn->Call<UObject*>("EquipWeaponDefinition", inst->Call<UObject*>("GetItemDefinitionBP"), inst->Call<FGuid>("GetItemGuid"));
        }
    }
}

void Athena::RemoveNetDebugUI() {
    UObject* wbLib = FindObject(L"/Script/UMG.Default__WidgetBlueprintLibrary");
    if (!wbLib) return;
    TArray<UObject*> empty{};
    auto arr = wbLib->Call<TArray<UObject*>, 0x8>("GetAllWidgetsOfClass", World, empty, FindObject(L"/Game/Athena/HUD/NetDebugUI.NetDebugUI_C"), false);
    if (arr.Count > 0 && arr[0]) arr[0]->Call("RemoveFromViewport");
}

void Athena::TeleportToSpawnIsland() {
    UObject* cls = FindObject(L"/Script/FortniteGame.FortPlayerStartWarmup");
    if (!cls) return;
    TArray<UObject*> empty{};
    auto arr = GameStatics->Call<TArray<UObject*>, 0x10>("GetAllActorsOfClass", World, cls, empty);
    if (arr.Count == 0) return;
    int idx = kismetMathLib->Call<int>("RandomIntegerInRange", 0, arr.MaxIndex());
    FVector loc = arr[idx]->Call<FVector>("K2_GetActorLocation");
    Pawn->Call("K2_TeleportTo", loc, FRotator{0, 0, 0});
}

void Athena::ConsoleKey() {
    UObject* inputSettings = FindObject(L"/Script/Engine.Default__InputSettings");
    if (!inputSettings) return;
    FName f6 = kismetStringLib->Call<FName>("Conv_StringToName", FString(L"F6"));
    auto& keys = inputSettings->Child<TArray<FKey>>("ConsoleKeys");
    if (keys.Count > 1) keys[1] = FKey{f6};
}

void Athena::CheatScript(const char* script) {
    std::string s = script;
    std::transform(s.begin(), s.end(), s.begin(), ::tolower);

    if (s.find("spawnrift") != std::string::npos) {
        if (!SpawnActorEasy(FindObject(L"/Game/Athena/Items/Consumables/RiftItem/BGA_RiftPortal_Item_Athena.BGA_RiftPortal_Item_Athena_C"), Pawn->Call<FVector>("K2_GetActorLocation"), FRotator{0, 0, 0})) {
            GameMode->Call("Say", FString(L"Rifts unavailable in this version."));
        }
    }
    else if (s.find("spawnweapon") != std::string::npos) {
        std::string wn = s.substr(s.find(" ") + 1);
        UObject* def = FindObject(ToWide(wn).c_str(), false, true);
        if (def) SpawnPickup(def, 1, Pawn->Call<FVector>("K2_GetActorLocation"));
    }
    else if (s.find("startevent") != std::string::npos) {
        bool started = false;
        UObject* cattus = FindObject(L"/Game/Athena/Maps/Athena_POI_Foundations.Athena_POI_Foundations.PersistentLevel.BP_CattusDoggus_Scripting_2");
        UObject* nightNight = FindObject(L"/Game/Athena/Maps/Athena_POI_Foundations.Athena_POI_Foundations.PersistentLevel.BP_NightNight_Scripting_2");

        if (cattus) { cattus->Call<bool>("LoadCattusLevel"); cattus->Call("startevent"); started = true; }
        else if (nightNight) { nightNight->Call<bool>("LoadNightNightLevel"); nightNight->Call("startevent"); started = true; }
        if (started) GameMode->Call("Say", FString(L"The event started successfully."));
    }
}

void Athena::ServerHandlePickup(UObject* pickup) {
    if (!pickup) return;
    UObject* def = pickup->Child<UObject*>("ItemDefinition");
    int count = pickup->Child<int>("Count");

    UObject* enumE = FindObject(L"EFortItemType", false, true);
    int idxAmmo = enumE ? enumE->EnumIndex("Ammo") : 0;
    int idxTrap = enumE ? enumE->EnumIndex("Trap") : 0;
    int idxRes = enumE ? enumE->EnumIndex("WorldResource") : 0;
    char itemType = def->Child<char>("ItemType");

    if (itemType == (char)idxAmmo || itemType == (char)idxTrap || itemType == (char)idxRes) {
        auto instances = WorldInventory->Child<TArray<UObject*>>("ItemInstances");
        for (int i = 0; i < instances.Count; ++i) {
            if (instances[i]->Call<UObject*>("GetItemDefinitionBP") == def) {
                int sum = instances[i]->Child<int>("Count") + count;
                int maxStack = def->Child<int>("MaxStackSize");
                if (sum <= maxStack) {
                    AddToInventory(def, sum, 0, 10);
                } else {
                    AddToInventory(def, maxStack, 0, 10);
                    SpawnPickup(def, sum - maxStack, Pawn->Call<FVector>("K2_GetActorLocation"));
                }
                if (GenericArray_RemoveFn) {
                    GenericArray_RemoveFn(&WorldInventory->Child<TArray<UObject*>>("ItemInstances"), nullptr, i);
                    GenericArray_RemoveFn(&WorldInventory->Child<TArray<char>>("ReplicatedEntries"), nullptr, i);
                }
                InventoryUpdate();
                pickup->Call("K2_DestroyActor");
                return;
            }
        }
        AddToInventory(def, count, 3, 10);
        InventoryUpdate();
        pickup->Call("K2_DestroyActor");
        return;
    }

    UObject* bpcLib = FindObject(L"/Script/BlueprintContext.Default__BlueprintContextLibrary");
    UObject* lp0 = GameViewport->Child<UObject*>("GameInstance")->Child<TArray<UObject*>>("LocalPlayers")[0];
    UObject* invCtx = bpcLib ? bpcLib->Call<UObject*>("GetContext", lp0, FindObject(L"/Script/FortniteUI.FortInventoryContext")) : nullptr;

    if (invCtx) {
        for (int i = 0; i < 6; ++i) {
            if (!invCtx->Call<UObject*>("GetQuickBarSlottedItem", (char)0, i)) {
                AddToInventory(def, count, 0, i);
                InventoryUpdate();
                pickup->Call("K2_DestroyActor");
                return;
            }
        }

        int focused = (&Quickbars->Child<Struct>("PrimaryQuickBar"))->Child<int>(FindObject(L"/Script/FortniteGame.QuickBar"), "CurrentFocusedSlot");
        if (focused == 0) return;
        UObject* inst = invCtx->Call<UObject*>("GetQuickBarSlottedItem", (char)0, focused);
        if (!inst) return;
        DropInventoryItem(inst->Call<FGuid>("GetItemGuid"), inst->Child<int>("Count"), false);
        AddToInventory(def, count, 0, focused);
        InventoryUpdate();
        Quickbars->Call("ServerActivateSlotInternal", (char)0, focused, 0.f, false);
        pickup->Call("K2_DestroyActor");
    }
}

void Athena::FixBuildingFoundations() {
    auto arr = GameStatics->Call<TArray<UObject*>, 0x10>("GetAllActorsOfClass", World, FindObject(L"/Script/FortniteGame.BuildingFoundation"), TArray<UObject*>{});
    for (UObject* b : arr) b->Child<char>("DynamicFoundationType") = 0;
}

void Athena::Fixbus() {
    if (!GameState) return;
    GameState->Child<char>("GamePhase") = 3;
    GameState->Call("OnRep_GamePhase", (char)2);
    if (!Pawn) return;
    Pawn->Call<bool>("K2_TeleportTo", FVector{0, 0, 22500}, FRotator{0, 0, 0});
    Pawn->Child<bool>("bIsSkydiving") = true;
    Pawn->Call("OnRep_IsSkydiving", false);
}

void Athena::Loot(UObject* actor) {
    if (!actor) return;
    std::string tier = "None";
    UObject* bcCls = FindObject(L"BuildingContainer", false, true);
    UObject* sdCls = FindObject(L"FortAthenaSupplyDrop", false, true);

    if (bcCls && actor->IsChild(bcCls)) {
        FName n = actor->Child<FName>("SearchLootTierGroup");
        tier = n.ToString();
    } else if (sdCls && actor->IsChild(sdCls)) {
        FName n = actor->Child<FName>("LootTableName", false, false);
        tier = n.ToString();
    }

    if (tier.find("None") != std::string::npos) return;
    if (tier.find("Athena") == std::string::npos) {
        auto p = tier.find("_");
        if (p != std::string::npos) tier.replace(p, 1, "_Athena");
    }

    if (bcCls && actor->IsChild(bcCls)) {
        if (actor->Child<float>("TimeUntilLootRegenerates") == 0) actor->Child<float>("TimeUntilLootRegenerates") = -1;
        else return;
    }

    auto results = PickLootDrops(tier);
    for (auto& r : results) {
        FVector right = kismetMathLib->Call<FVector>("GetRightVector", actor->Call<FRotator>("K2_GetActorRotation"));
        right.X *= 64; right.Y *= 64;
        FVector loc = kismetMathLib->Call<FVector>("Add_VectorVector", actor->Call<FVector>("K2_GetActorLocation"), right);
        SpawnPickup(r.ItemDefinition, r.Count, loc);
    }

    if (bcCls && actor->IsChild(bcCls)) {
        FName n = kismetStringLib->Call<FName>("Conv_StringToName", FString(L"bAlreadySearched"));
        kismetSystemLib->Call("SetBoolPropertyByName", actor, n, true);
        actor->Call("OnRep_bAlreadySearched");
    }
}

void Athena::OnServerCreateBuildingActor(void* params) {
    struct OldP { UObject* Class; uint8 _pad[0x8]; FVector Loc; FRotator Rot; };
    struct NewP { int32 ClassHandle; FVector Loc; FRotator Rot; uint8 _pad[0xC]; UObject* Class; };

    UObject* cls = nullptr;
    FVector loc{}; FRotator rot{};

    auto* serverCreateFn = FindObject(L"/Script/FortniteGame.FortPlayerController.ServerCreateBuildingActor");
    if (serverCreateFn && serverCreateFn->GetFunctionChildrenOffset().size() == 1) {
        auto p = *(NewP*)params;
        cls = p.Class; loc = p.Loc; rot = p.Rot;
    } else {
        auto p = *(OldP*)params;
        cls = p.Class; loc = p.Loc; rot = p.Rot;
    }

    UObject* actor = SpawnActorEasy(cls, loc, rot);
    if (!actor) return;
    auto* initFn = FindObject(L"/Script/FortniteGame.BuildingActor.InitializeKismetSpawnedBuildingActor");
    if (initFn && initFn->GetFunctionChildrenOffset().size() == 3) {
        actor->Call("InitializeKismetSpawnedBuildingActor", actor, PlayerController, true);
    } else {
        actor->Call("InitializeKismetSpawnedBuildingActor", actor, PlayerController);
    }
}

void Athena::OnBeginEditActor(UObject* piece) {
    if (!EditToolItem) return;
    UObject* temp = EditToolItem->Call<UObject*>("CreateTemporaryItemInstanceBP", 1, 1);
    if (!temp) return;
    UObject* editActor = Pawn->Call<UObject*>("EquipWeaponDefinition", EditToolItem, temp->Call<FGuid>("GetItemGuid"));
    if (!editActor) return;
    editActor->Child<UObject*>("EditActor") = piece;
    editActor->Call("OnRep_EditActor");
}

void Athena::OnFinishEditActor(UObject* actor, UObject* newClass, int rotIter, bool mirrored, void* params) {
    struct FP { UObject* BuildingActor; UObject* NewClass; int8 Rot; bool Mir; };
    auto fp = *(FP*)params;

    actor->Child<bool>("bPlayDestructionEffects") = false;
    actor->Call("SetActorScale3D", FVector{0, 0, 0});
    actor->Call("K2_DestroyActor");

    UObject* edited = SpawnActorEasy(newClass, EditComponentLocation, EditComponentRotation);
    if (!edited) return;

    auto* editFn = FindObject(L"/Script/FortniteGame.FortPlayerController.ServerEditBuildingActor");
    if (editFn) {
        auto offs = editFn->GetFunctionChildrenOffset();
        if (offs.size() > 3 && offs[3] != 0x14 && offs[3] > -1) {
            edited->Call("SetMirrored", fp.Mir);
        } else {
            edited->Call("SetMirrored", mirrored);
        }
    } else {
        edited->Call("SetMirrored", mirrored);
    }

    auto* initFn = FindObject(L"/Script/FortniteGame.BuildingActor.InitializeKismetSpawnedBuildingActor");
    if (initFn && initFn->GetFunctionChildrenOffset().size() == 3) {
        edited->Call("InitializeKismetSpawnedBuildingActor", edited, PlayerController, true);
    } else {
        edited->Call("InitializeKismetSpawnedBuildingActor", edited, PlayerController);
    }
}

void Athena::SpawnPickup(UObject* def, int count, FVector loc, bool toss) {
    if (!def) return;
    UObject* pickup = SpawnActorEasy(FindObject(L"/Script/FortniteGame.FortPickupAthena"), loc, FRotator{0, 0, 0});
    if (!pickup) return;
    pickup->Child<UObject*>("ItemDefinition") = def;
    pickup->Child<int>("Count") = count;
    pickup->Call("OnRep_PrimaryPickupItemEntry");
    pickup->Call("TossPickup", loc, Pawn, 1, toss);
}

void Athena::GrantEffect(UObject* effect, float level) {
    if (!effect || !Pawn) return;
    UObject* asc = Pawn->Child<UObject*>("AbilitySystemComponent");
    if (!asc) return;
    FGameplayEffectContextHandle ctx{};
    asc->Call<FActiveGameplayEffectHandle>("BP_ApplyGameplayEffectToSelf", effect, level, ctx);
}

void Athena::GrantAbility(UObject* klass) {
    if (!klass || !Pawn) return;
    UObject* ge = FindObject(L"/Game/Athena/Items/Consumables/PurpleStuff/GE_Athena_PurpleStuff.GE_Athena_PurpleStuff_C");
    if (!ge) ge = FindObject(L"/Game/Athena/Items/Consumables/PurpleStuff/GE_Athena_PurpleStuff_Health.GE_Athena_PurpleStuff_Health_C");
    if (!ge) return;

    UObject* defUI = FindObject(L"/Script/FortniteGame.Default__FortAbilitySystemUI");
    if (!defUI) return;
    UObject* defEffect = defUI->Call<UObject*>("GetDefaultObjectOfGameplayEffectType", ge);
    if (!defEffect) return;

    auto& abilities = defEffect->Child<TArray<FGameplayAbilitySpecDef>>("GrantedAbilities");
    if (abilities.Count > 0) abilities[0].Ability = klass;
    defEffect->Child<char>("DurationPolicy") = 1;

    UObject* asc = Pawn->Child<UObject*>("AbilitySystemComponent");
    if (!asc) return;
    FGameplayEffectContextHandle ctx{};
    asc->Call<FActiveGameplayEffectHandle>("BP_ApplyGameplayEffectToSelf", ge, 1.f, ctx);
}

void Athena::GrantDefaultAbilities() {
    GrantAbility(FindObject(L"/Script/FortniteGame.FortGameplayAbility_Sprint"));
    GrantAbility(FindObject(L"/Script/FortniteGame.FortGameplayAbility_Jump"));
    GrantAbility(FindObject(L"/Game/Abilities/Player/Generic/Traits/DefaultPlayer/GA_DefaultPlayer_InteractUse.GA_DefaultPlayer_InteractUse_C"));
    GrantAbility(FindObject(L"/Game/Abilities/Player/Generic/Traits/DefaultPlayer/GA_DefaultPlayer_InteractSearch.GA_DefaultPlayer_InteractSearch_C"));
    GrantAbility(FindObject(L"/Game/Athena/DrivableVehicles/GA_AthenaEnterVehicle.GA_AthenaEnterVehicle_C"));
    GrantAbility(FindObject(L"/Game/Athena/DrivableVehicles/GA_AthenaExitVehicle.GA_AthenaExitVehicle_C"));
    GrantAbility(FindObject(L"/Game/Athena/DrivableVehicles/GA_AthenaInVehicle.GA_AthenaInVehicle_C"));
    GrantAbility(FindObject(L"/Game/Athena/Items/ForagedItems/Rift/GA_Rift_Athena_Skydive.GA_Rift_Athena_Skydive_C"));
    GrantAbility(FindObject(L"/Game/Athena/Environments/Blueprints/SurfaceEffects/GAB_SurfaceChange.GAB_SurfaceChange_C"));
}

void Athena::OnAircraftJump() {
    FVector loc = PlayerController->Call<FVector>("GetFocalLocation");
    FRotator rot = PlayerController->Call<FRotator>("GetControlRotation");
    rot.Pitch = 0; rot.Roll = 0;

    Pawn = SpawnActorEasy(FindObject(L"/Game/Athena/PlayerPawn_Athena.PlayerPawn_Athena_C"), loc, rot);
    if (!Pawn) return;
    PlayerController->Call("Possess", Pawn);
    Pawn->Call("OnRep_CustomizationLoadout");
    ShowSkin();
    bDroppedFromAircraft = true;

    UObject* bpcLib = FindObject(L"/Script/BlueprintContext.Default__BlueprintContextLibrary");
    if (!bpcLib) return;
    UObject* lp0 = GameViewport->Child<UObject*>("GameInstance")->Child<TArray<UObject*>>("LocalPlayers")[0];
    UObject* invCtx = bpcLib->Call<UObject*>("GetContext", lp0, FindObject(L"/Script/FortniteUI.FortInventoryContext"));
    if (!invCtx) return;
    UObject* pickaxe = invCtx->Call<UObject*>("GetQuickBarSlottedItem", (char)0, 0);
    if (pickaxe) Pawn->Call<UObject*>("EquipWeaponDefinition", pickaxe->Call<UObject*>("GetItemDefinitionBP"), pickaxe->Call<FGuid>("GetItemGuid"));
}

std::vector<Athena::LootData> Athena::PickLootDrops(std::string category) {
    std::vector<LootData> ret;
    UObject* ltd = FindObject(L"/Game/Items/Datatables/AthenaLootTierData_Client.AthenaLootTierData_Client", true);
    UObject* lpt = FindObject(L"/Game/Items/Datatables/AthenaLootPackages_Client.AthenaLootPackages_Client", true);
    if (!ltd || !lpt || !GetDataTableRowFn || !DataTableFunctionLibrary) return ret;

    UObject* ltdRS = ltd->Child<UObject*>("RowStruct");
    UObject* lptRS = lpt->Child<UObject*>("RowStruct");
    if (!ltdRS || !lptRS) return ret;

    int32 ltdSize = *(int32*)(int64(ltdRS) + offsets::StructSize);
    int32 lptSize = *(int32*)(int64(lptRS) + offsets::StructSize);

    auto ltdNames = DataTableFunctionLibrary->Call<TArray<FName>, 0x8>("GetDataTableRowNames", ltd, TArray<FName>{});
    auto lptNames = DataTableFunctionLibrary->Call<TArray<FName>, 0x8>("GetDataTableRowNames", lpt, TArray<FName>{});

    // Struct accessors
    auto getTierGroup = [&](void* row) -> std::string {
        Struct s;
        auto& f = s.Child<FName>(ltdRS, "TierGroup");
        return f.ToString();
    };
    auto getWeight = [&](void* row, UObject* rs, const char* fld) -> float {
        // Use reflection to find offset
        Struct s;
        auto& f = s.Child<float>(rs, fld);
        (void)f;
        // Get pointer via Child
        uintptr_t off = (uintptr_t)&s.Child<float>(rs, fld) - (uintptr_t)&s;
        return *(float*)((uintptr_t)row + off);
    };
    auto getNumDrops = [&](void* row) -> float {
        Struct s;
        uintptr_t off = (uintptr_t)&s.Child<float>(ltdRS, "NumLootPackageDrops") - (uintptr_t)&s;
        return *(float*)((uintptr_t)row + off);
    };
    auto getLootPackageName = [&](void* row) -> std::string {
        Struct s;
        uintptr_t off = (uintptr_t)&s.Child<FName>(ltdRS, "LootPackage") - (uintptr_t)&s;
        return ((FName*)((uintptr_t)row + off))->ToString();
    };
    auto getLootPackageID = [&](void* row) -> std::string {
        Struct s;
        uintptr_t off = (uintptr_t)&s.Child<FName>(lptRS, "LootPackageID") - (uintptr_t)&s;
        return ((FName*)((uintptr_t)row + off))->ToString();
    };
    auto getLootPackageCall = [&](void* row) -> std::string {
        Struct s;
        uintptr_t off = (uintptr_t)&s.Child<FString>(lptRS, "LootPackageCall") - (uintptr_t)&s;
        return ((FString*)((uintptr_t)row + off))->ToString();
    };
    auto getItemDef = [&](void* row) -> UObject* {
        Struct s;
        uintptr_t off = (uintptr_t)&s.Child<SoftObjectPtr>(lptRS, "ItemDefinition") - (uintptr_t)&s;
        auto& ref = *(SoftObjectPtr*)((uintptr_t)row + off);
        return kismetSystemLib->Call<UObject*>("Conv_SoftObjectReferenceToObject", ref);
    };
    auto getCount = [&](void* row) -> int32 {
        Struct s;
        uintptr_t off = (uintptr_t)&s.Child<int32>(lptRS, "Count") - (uintptr_t)&s;
        return *(int32*)((uintptr_t)row + off);
    };

    std::map<std::string, void*> catRows;
    float maxW = 0;
    for (FName name : ltdNames) {
        void* row = calloc(1, ltdSize);
        GetDataTableRowFn(ltd, name, row);
        if (getTierGroup(row) == category) {
            float w = getWeight(row, ltdRS, "Weight");
            if (w != 0.f) {
                catRows[name.ToString()] = row;
                maxW += w;
            } else {
                free(row);
            }
        } else {
            free(row);
        }
    }

    if (catRows.empty() || maxW == 0.f) {
        for (auto& p : catRows) free(p.second);
        return ret;
    }

    float rv = kismetMathLib->Call<float>("RandomFloatInRange", 0.f, maxW);
    void* pickedRow = nullptr;
    for (auto& p : catRows) {
        float w = getWeight(p.second, ltdRS, "Weight");
        if (rv < w) { pickedRow = p.second; break; }
        rv -= w;
    }
    if (!pickedRow) {
        for (auto& p : catRows) free(p.second);
        return ret;
    }

    float numDrops = getNumDrops(pickedRow);
    std::string pkgName = getLootPackageName(pickedRow);
    for (auto& p : catRows) free(p.second);
    catRows.clear();

    std::map<std::string, void*> pkgRows;
    for (FName name : lptNames) {
        void* row = calloc(1, lptSize);
        GetDataTableRowFn(lpt, name, row);
        if (getLootPackageID(row) == pkgName) {
            pkgRows[name.ToString()] = row;
        } else {
            free(row);
        }
    }

    std::map<std::string, void*> finalCalls;
    for (int i = 0; i < (int)numDrops; ++i) {
        auto it = pkgRows.begin();
        if (it == pkgRows.end()) break;
        std::advance(it, i);
        if (it == pkgRows.end()) break;

        std::string call = getLootPackageCall(it->second);
        if (pkgName.find(".Empty") != std::string::npos) continue;
        if (call.find(".Empty") != std::string::npos) { ++i; continue; }

        std::map<std::string, void*> tempCalls;
        float tempMax = 0;
        if (!strstr(pkgName.c_str(), "WorldList")) {
            for (FName name : lptNames) {
                void* row = calloc(1, lptSize);
                GetDataTableRowFn(lpt, name, row);
                if (getLootPackageID(row) == call) {
                    tempCalls[name.ToString()] = row;
                } else {
                    free(row);
                }
            }
        } else {
            for (auto& p : pkgRows) tempCalls[p.first] = p.second;
        }

        for (auto& p : tempCalls) tempMax += getWeight(p.second, lptRS, "Weight");
        float trv = kismetMathLib->Call<float>("RandomFloatInRange", 0.f, tempMax);

        for (auto& p : tempCalls) {
            float w = getWeight(p.second, lptRS, "Weight");
            if (trv < w) {
                finalCalls[p.first] = p.second;
                break;
            }
            trv -= w;
        }

        if (!strstr(pkgName.c_str(), "WorldList")) {
            for (auto& p : tempCalls) {
                if (finalCalls.find(p.first) == finalCalls.end()) free(p.second);
            }
        }
    }

    for (auto& p : finalCalls) {
        UObject* def = getItemDef(p.second);
        if (def) ret.push_back(LootData{def, getCount(p.second)});
    }

    for (auto& p : pkgRows) free(p.second);
    for (auto& p : finalCalls) free(p.second);

    return ret;
}

void Athena::SpawnFloorLoot() {
    auto arr = GameStatics->Call<TArray<UObject*>, 0x10>("GetAllActorsOfClass", World, FindObject(L"Tiered_Athena_FloorLoot_01_C", false, true), TArray<UObject*>{});
    for (UObject* e : arr) {
        auto loot = PickLootDrops("Loot_AthenaFloorLoot");
        for (auto& l : loot) SpawnPickup(l.ItemDefinition, l.Count, e->Call<FVector>("K2_GetActorLocation"), false);
    }
    auto arr2 = GameStatics->Call<TArray<UObject*>, 0x10>("GetAllActorsOfClass", World, FindObject(L"Tiered_Athena_FloorLoot_Warmup_C", false, true), TArray<UObject*>{});
    for (UObject* e : arr2) {
        auto loot = PickLootDrops("Loot_AthenaFloorLoot_Warmup");
        for (auto& l : loot) SpawnPickup(l.ItemDefinition, l.Count, e->Call<FVector>("K2_GetActorLocation"), false);
    }
}

void Athena::SpawnBuildPreviews() {
    UObject* previewCls = FindObject(L"BuildingPlayerPrimitivePreview", false, true);
    if (!previewCls) return;

    auto build = [&](const wchar_t* mesh, char type) -> UObject* {
        UObject* a = SpawnActorEasy(previewCls, FVector{0, 0, 0}, FRotator{0, 0, 0});
        if (!a) return nullptr;
        UObject* mc = a->Call<UObject*>("GetBuildingMeshComponent");
        if (mc) {
            mc->Call<bool>("SetStaticMesh", FindObject(mesh, false, true));
            mc->Call("SetMaterial", 0, PlayerController->Child<UObject*>("BuildPreviewMarkerMID"));
        }
        a->Call("OnBuildingActorInitialized", (char)5, type);
        a->Call("SetActorHiddenInGame", true);
        return a;
    };

    Wall = build(L"PBW_W1_Solid", 0);
    Floor = build(L"PBW_W1_Floor", 1);
    Stairs = build(L"PBW_W1_StairW", 5);
    Roof = build(L"PBW_W1_RoofC", 6);
}

void Athena::PlayEmoteItem(UObject* item) {
    if (!item) return;
    if (!FindObject(L"GetAnimationHardReference", false, true)) return;
    UObject* montage = item->Call<UObject*>("GetAnimationHardReference", (char)6, (char)3);
    if (IsBadPtr(montage) || !montage) return;
    UObject* animInst = Pawn->Child<UObject*>("Mesh")->Call<UObject*>("GetAnimInstance");
    if (!animInst) return;
    animInst->Call<float>("Montage_Play", montage, 1.f, (char)0, 0.f, true);
    bIsEmoting = true;
}

void Athena::Tick() {
    if (bIsEmoting && PlayerController && !PlayerController->Call<bool>("IsInAircraft")) {
        bool moving = PlayerController->Child<bool>("bIsPlayerActivelyMoving");
        UObject* ai = Pawn ? Pawn->Child<UObject*>("Mesh")->Call<UObject*>("GetAnimInstance") : nullptr;
        if (ai) {
            bool jumping = ai->Child<bool>("bIsJumping");
            bool falling = ai->Child<bool>("bIsFalling");
            if (moving || jumping || falling) {
                bIsEmoting = false;
                Pawn->Call("ServerRootMotionInterruptNotifyStopMontage", Pawn->Call<UObject*>("GetCurrentMontage"));
            }
        }
    }

    if (g_bIsVehicleVersion && Pawn) {
        bool inVeh = Pawn->Call<bool>("IsInVehicle");
        Pawn->Child<char>("Role") = inVeh ? (char)2 : (char)3;
        if (inVeh) {
            UObject* v = Pawn->Call<UObject*>("GetVehicle");
            if (v) v->Child<char>("Role") = (char)2;
        }
    }

    if (Pawn) {
        auto p = &Pawn->Child<bool>("bIsOutsideSafeZone");
        if (!IsBadPtr(p)) {
            if ((*p && !l_bIsOutsideSafeZone) || (!*p && l_bIsOutsideSafeZone)) {
                l_bIsOutsideSafeZone = !l_bIsOutsideSafeZone;
                Pawn->Call("OnRep_IsOutsideSafeZone");
            }
        }
    }
}

void Athena::OnExitVehicle() {
    if (!Pawn) return;
    Pawn->Child<char>("Role") = 3;
    UObject* v = Pawn->Call<UObject*>("GetVehicle");
    if (v) v->Child<char>("Role") = 3;
}

void Athena::DropInventoryItem(FGuid guid, int count, bool checkCount) {
    auto instances = WorldInventory->Child<TArray<UObject*>>("ItemInstances");
    for (int i = 0; i < instances.Count; ++i) {
        if (kismetGuidLib->Call<bool>("EqualEqual_GuidGuid", instances[i]->Call<FGuid>("GetItemGuid"), guid)) {
            UObject* curDef = instances[i]->Call<UObject*>("GetItemDefinitionBP");
            int diff = instances[i]->Child<int>("Count") - count;

            if (diff != 0 && checkCount) {
                if (GenericArray_RemoveFn) {
                    GenericArray_RemoveFn(&WorldInventory->Child<TArray<UObject*>>("ItemInstances"), nullptr, i);
                    GenericArray_RemoveFn(&WorldInventory->Child<TArray<char>>("ReplicatedEntries"), nullptr, i);
                }
                AddToInventory(curDef, diff, 0, 10);
                InventoryUpdate();
                SpawnPickup(curDef, count, Pawn->Call<FVector>("K2_GetActorLocation"));
                return;
            }

            if (GenericArray_RemoveFn) {
                GenericArray_RemoveFn(&WorldInventory->Child<TArray<UObject*>>("ItemInstances"), nullptr, i);
                GenericArray_RemoveFn(&WorldInventory->Child<TArray<char>>("ReplicatedEntries"), nullptr, i);
            }
            InventoryUpdate();
            SpawnPickup(curDef, count, Pawn->Call<FVector>("K2_GetActorLocation"));

            UObject* bpcLib = FindObject(L"/Script/BlueprintContext.Default__BlueprintContextLibrary");
            if (!bpcLib) return;
            UObject* lp0 = GameViewport->Child<UObject*>("GameInstance")->Child<TArray<UObject*>>("LocalPlayers")[0];
            UObject* invCtx = bpcLib->Call<UObject*>("GetContext", lp0, FindObject(L"/Script/FortniteUI.FortInventoryContext"));
            if (!invCtx) return;
            int focused = (&Quickbars->Child<Struct>("PrimaryQuickBar"))->Child<int>(FindObject(L"/Script/FortniteGame.QuickBar"), "CurrentFocusedSlot");
            for (int j = focused - 1; j >= 0; --j) {
                if (invCtx->Call<UObject*>("GetQuickBarSlottedItem", (char)0, j)) {
                    Quickbars->Call("ServerActivateSlotInternal", (char)0, j, 0.f, false);
                    break;
                }
            }
        }
    }
}

// ============================================================
// CURL HOOK
// ============================================================
static bool HostShouldRedirect(const std::string& host) {
    return host.find("ol.epicgames.com") != std::string::npos ||
           host.find(".akamaized.net") != std::string::npos ||
           host.find("on.epicgames.com") != std::string::npos ||
           host.find("epicgames.com") != std::string::npos ||
           host.find("fortnite.com") != std::string::npos ||
           host.find("unrealengine.com") != std::string::npos;
}

static std::string RewriteUrl(const std::string& url) {
    size_t se = url.find("://");
    if (se == std::string::npos) return url;
    size_t hs = se + 3;
    size_t he = url.find_first_of("/?", hs);
    if (he == std::string::npos) he = url.size();
    std::string host = url.substr(hs, he - hs);
    if (!HostShouldRedirect(host)) return url;
    return std::string(REDIRECT_SCHEME) + "://" + REDIRECT_HOST + url.substr(he);
}

static void* (*orig_curl_easy_setopt)(void*, int, void*) = nullptr;

static void* curl_easy_setopt_Hook(void* curl, int option, void* arg) {
    if (option == CURLOPT_URL && !IsBadPtr(arg)) {
        std::string orig = (const char*)arg;
        std::string rw = RewriteUrl(orig);
        if (rw != orig) {
            LOGI("[curl] %s -> %s", orig.c_str(), rw.c_str());
            return orig_curl_easy_setopt(curl, option, (void*)rw.c_str());
        }
        return orig_curl_easy_setopt(curl, option, arg);
    }
    if (option == CURLOPT_SSL_VERIFYPEER || option == CURLOPT_SSL_VERIFYHOST) {
        return orig_curl_easy_setopt(curl, option, (void*)0);
    }
    return orig_curl_easy_setopt(curl, option, arg);
}

// ============================================================
// GLOBALS INIT
// ============================================================
static void InitializeGlobals() {
    if (!gGObjects) return;

    for (uint32 i = 0; i < gGObjects->ObjectCount; ++i) {
        UObject* o = gGObjects->GetById(i);
        if (!o) continue;
        std::string n = o->GetName();
        if (!PlayerController && (n == "FortPlayerControllerAthena" || n == "FortPlayerController")) PlayerController = o;
        if (!GameState && (n == "FortGameStateAthena" || n == "FortGameState")) GameState = o;
        if (!GameMode && (n == "FortGameModeAthena" || n == "FortGameMode")) GameMode = o;
        if (PlayerController && GameState && GameMode) break;
    }

    if (PlayerController) {
        World = PlayerController->Call<UObject*>("GetWorld");
        auto playerPtr = &PlayerController->Child<UObject*>("Player");
        if (!IsBadPtr(playerPtr) && *playerPtr) {
            auto vpPtr = &(*playerPtr)->Child<UObject*>("ViewportClient");
            if (!IsBadPtr(vpPtr)) GameViewport = *vpPtr;
        }
        CheatManager = FindObject(L"/Script/FortniteGame.FortCheatManager");
        if (CheatManager) PlayerController->Child<UObject*>("CheatManager") = CheatManager;
    }

    if (World) {
        auto plPtr = &World->Child<UObject*>("PersistentLevel");
        if (!IsBadPtr(plPtr) && *plPtr) {
            auto wsPtr = &(*plPtr)->Child<UObject*>("WorldSettings");
            if (!IsBadPtr(wsPtr)) WorldSettings = *wsPtr;
        }
    }

    GameStatics = FindObject(L"/Script/Engine.Default__GameplayStatics");
    kismetMathLib = FindObject(L"/Script/Engine.Default__KismetMathLibrary");
    kismetStringLib = FindObject(L"/Script/Engine.Default__KismetStringLibrary");
    kismetSystemLib = FindObject(L"/Script/Engine.Default__KismetSystemLibrary");
    kismetGuidLib = FindObject(L"/Script/Engine.Default__KismetGuidLibrary");
    DataTableFunctionLibrary = FindObject(L"/Script/Engine.Default__DataTableFunctionLibrary");

    Athena::ConsoleKey();
}

static void PlayButton() {
    InitializeGlobals();
    if (!PlayerController) return;

    UObject* func = nullptr;
    UObject* klass = PlayerController->Class;
    while (klass && !func) {
        UObject* child = *(UObject**)((uintptr_t)klass + offsets::Children);
        while (child) {
            if (child->GetName() == "SwitchLevel") { func = child; break; }
            child = *(UObject**)((uintptr_t)child + offsets::Next);
        }
        klass = *(UObject**)((uintptr_t)klass + offsets::SuperClass);
    }
    if (!func) return;

    int16 ps = *(int16*)((uintptr_t)func + offsets::ParamsSize);
    uint8* params = (uint8*)calloc(1, ps);

    static std::wstring w = L"Artemis_Terrain";
    FString* fs = (FString*)params;
    fs->Data = (wchar_t*)w.c_str();
    fs->Count = (int32)w.size() + 1;
    fs->Max = (int32)w.size() + 1;

    orig_ProcessEvent(PlayerController, func, params);
    bPressedPlay = true;
    bInFrontend = false;
}

static void OnReadyToStartMatch() {
    LOGI("ReadyToStartMatch");
    bLoadedInMatch = true;
    bInFrontend = false;
    InitializeGlobals();
    Athena::SpawnPawn();
    Athena::ShowSkin();
    Athena::DestroyLods();
    Athena::DropLoadingScreen();
    Athena::FixBuildingFoundations();
    Athena::InitializeInventory();
    Athena::GrantDefaultAbilities();
    Athena::SpawnBuildPreviews();
}

static void OnServerLoadingScreenDropped() {
    LOGI("ServerLoadingScreenDropped");
    if (bInFrontend) {
        bPressedPlay = false;
        bLoadedInMatch = false;
        bDroppedFromAircraft = false;
        return;
    }
    Athena::RemoveNetDebugUI();
    Athena::TeleportToSpawnIsland();
    Athena::Fixbus();
    Athena::SpawnFloorLoot();
}

static void* ProcessEventHook(UObject* self, UObject* func, void* params) {
    if (!func || !orig_ProcessEvent) return orig_ProcessEvent(self, func, params);
    std::string n = func->GetName();
    if (n.empty()) return orig_ProcessEvent(self, func, params);

    if (n.find("ReadyToStartMatch") != std::string::npos && !bLoadedInMatch) {
        OnReadyToStartMatch();
    }
    else if (n.find("ServerHandlePickup") != std::string::npos) {
        Athena::ServerHandlePickup(*(UObject**)params);
    }
    else if (n.find("GiveWeapon") != std::string::npos) {
        struct P { FString Item; int Slot; int Count; };
        auto* p = (P*)params;
        UObject* def = FindObject(p->Item.ToWString().c_str(), false, true);
        if (def) Athena::AddToInventory(def, p->Count, 0, p->Slot);
        Athena::InventoryUpdate();
        return nullptr;
    }
    else if (n.find("ServerCreateBuildingActor") != std::string::npos) {
        Athena::OnServerCreateBuildingActor(params);
    }
    else if (n.find("ServerBeginEditingBuildingActor") != std::string::npos) {
        Athena::OnBeginEditActor(*(UObject**)params);
    }
    else if (n.find("OnSuccessfulMatchInteract") != std::string::npos) {
        auto editActor = &PlayerController->Child<UObject*>("EditBuildingActor");
        if (!IsBadPtr(editActor) && *editActor) {
            auto preview = &(*editActor)->Child<UObject*>("EditModeSupport")->Child<UObject*>("PreviewComponent");
            if (!IsBadPtr(preview) && *preview) {
                EditComponentLocation = (*preview)->Call<FVector>("K2_GetComponentLocation");
                EditComponentRotation = (*preview)->Call<FRotator>("K2_GetComponentRotation");
            }
        }
    }
    else if (n.find("ServerEditBuildingActor") != std::string::npos) {
        struct P { UObject* BuildingActor; UObject* NewClass; int RotationIteration; bool bMirrored; };
        auto* p = (P*)params;
        Athena::OnFinishEditActor(p->BuildingActor, p->NewClass, p->RotationIteration, p->bMirrored, params);
    }
    else if (n.find("ServerLoadingScreenDropped") != std::string::npos && bLoadedInMatch) {
        OnServerLoadingScreenDropped();
    }
    else if (n.find("ServerExecuteInventoryItem") != std::string::npos) {
        Athena::OnServerExecuteInventoryItem(*(FGuid*)params);
    }
    else if (n.find("ServerExecuteInventoryWeapon") != std::string::npos) {
        Athena::OnServerExecuteInventoryWeapon(*(UObject**)params);
    }
    else if ((n.find("ServerAttemptAircraftJump") != std::string::npos ||
              n.find("OnAircraftExitedDropZone") != std::string::npos) && !bDroppedFromAircraft) {
        Athena::OnAircraftJump();
    }
    else if (n.find("CheatScript") != std::string::npos) {
        Athena::CheatScript(((FString*)params)->ToString().c_str());
    }
    else if (n.find("ServerAttemptInteract") != std::string::npos) {
        Athena::Loot(*(UObject**)params);
    }
    else if (n.find("ReturnToMainMenu") != std::string::npos) {
        PlayButton();
        bInFrontend = true;
        bPressedPlay = false;
    }
    else if ((n.find("BP_PlayButton") != std::string::npos ||
              n.find("PlayButton") != std::string::npos) && !bPressedPlay) {
        PlayButton();
    }
    else if (n.find("ServerPlayEmoteItem") != std::string::npos) {
        Athena::PlayEmoteItem(*(UObject**)params);
    }
    else if (n.find("OnPawnEnterVehicle") != std::string::npos) {
        g_bIsVehicleVersion = true;
    }
    else if (n.find("ServerAttemptExitVehicle") != std::string::npos) {
        Athena::OnExitVehicle();
    }
    else if (n.find("ServerSpawnInventoryDrop") != std::string::npos ||
             n.find("ServerAttemptInventoryDrop") != std::string::npos) {
        FGuid g = *(FGuid*)params;
        int cnt = *(int*)((uintptr_t)params + sizeof(FGuid));
        Athena::DropInventoryItem(g, cnt);
    }
    else if (n.find("Tick") != std::string::npos && self == PlayerController) {
        Athena::Tick();
    }

    return orig_ProcessEvent(self, func, params);
}

// ============================================================
// CORE SETUP
// ============================================================
static void CoreSetup() {
    g_base = Lib::Find("libUnreal.so");
    if (!g_base) { LOGE("libUnreal.so missing"); return; }
    LOGI("base = %p", (void*)g_base);

    FNameToStringFn = (decltype(FNameToStringFn))(g_base + RVAs::FNameToString);
    FreeInternalFn = (decltype(FreeInternalFn))(g_base + RVAs::FreeInternal);
    ReallocFn = (decltype(ReallocFn))(g_base + RVAs::Realloc);
    SpawnActorFn = (decltype(SpawnActorFn))(g_base + RVAs::SpawnActor_Thunk);
    GetDataTableRowFn = (decltype(GetDataTableRowFn))(g_base + RVAs::GetDataTableRowFromName_ORIG);
    GenericArray_RemoveFn = (decltype(GenericArray_RemoveFn))(g_base + RVAs::GenericArray_Remove);

    gGObjects = *(GObjects_t**)(g_base + RVAs::GObjects);
    LOGI("GObjects = %p count=%u", gGObjects, gGObjects ? gGObjects->ObjectCount : 0);

    void* h = dlopen("libUnreal.so", RTLD_NOW | RTLD_NOLOAD);
    if (h) {
        StaticFindObjectFn = (decltype(StaticFindObjectFn))dlsym(h, "StaticFindObject");
        StaticLoadObjectFn = (decltype(StaticLoadObjectFn))dlsym(h, "StaticLoadObject");
    }
    LOGI("StaticFindObject = %p", StaticFindObjectFn);

    void* pPE = (void*)(g_base + RVAs::ProcessEvent);
    if (DobbyHook(pPE, (void*)ProcessEventHook, (void**)&orig_ProcessEvent) == 0) {
        PE = orig_ProcessEvent;
        LOGI("ProcessEvent hooked @ %p", pPE);
    }

    void* pCurl = (void*)(g_base + RVAs::curl_easy_setopt);
    if (DobbyHook(pCurl, (void*)curl_easy_setopt_Hook, (void**)&orig_curl_easy_setopt) == 0) {
        LOGI("curl_easy_setopt hooked @ %p", pCurl);
    }

    InitializeGlobals();
}

static void* MainThread(void*) {
    LOGI("EraCH3 starting");
    if (!Lib::WaitFor("libUnreal.so")) { LOGE("wait timeout"); return nullptr; }
    usleep(300000);
    CoreSetup();
    LOGI("EraCH3 ready");
    return nullptr;
}

extern "C" __attribute__((constructor))
void libEraCH3_main() {
    pthread_t t;
    pthread_create(&t, nullptr, MainThread, nullptr);
    pthread_detach(t);
}
