// ============================================================================
// EraCH3.cpp — Single-file Android Server for Fortnite Chapter 3 Season 3
// Target: libUnreal.so 21.30 (UE 4.27) / arm64-v8a
// Author: adapted from EraV2 / Project Nova
// ============================================================================

#include <jni.h>
#include <pthread.h>
#include <dlfcn.h>
#include <unistd.h>
#include <cstring>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>
#include <map>
#include <android/log.h>

#include "Dobby/dobby.h"

// ============================================================================
// LOGGING
// ============================================================================
#define TAG "EraCH3"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,  TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__)

// ============================================================================
// CURL OPTIONS (hardcoded, from curl.h)
// ============================================================================
#define CURLOPT_SSL_VERIFYPEER  64
#define CURLOPT_SSL_VERIFYHOST  81
#define CURLOPT_URL             10002

// ============================================================================
// REDIRECT TARGET
// ============================================================================
#define REDIRECT_SCHEME  "http"
#define REDIRECT_HOST    "127.0.0.1:3551"

// ============================================================================
// OFFSETS — libUnreal.so 21.30 (CH3 S3, UE 4.27)
// ============================================================================
namespace RVAs {
    constexpr uint64_t ProcessEvent                      = 0x07611B5C;
    constexpr uint64_t GObjects                          = 0x0DCD6E08;

    constexpr uint64_t SpawnActor_Thunk                  = 0x3217298;
    constexpr uint64_t SpawnActor_2_Thunk                = 0x9CE0828;

    constexpr uint64_t GetEngineVersion                  = 0x945F4B4;

    constexpr uint64_t FNameToString                     = 0x74937C0;
    constexpr uint64_t FreeInternal                      = 0x7355370;
    constexpr uint64_t Realloc                           = 0x735E520;

    constexpr uint64_t CopyScriptStruct                  = 0x7535848;

    constexpr uint64_t GetDataTableRowFromName_Thunk     = 0x99CD07C;
    constexpr uint64_t GetDataTableRowFromName_ORIG      = 0x9319C70;
    constexpr uint64_t GetDataTableRowNames_ORIG         = 0x9319CD8;
    constexpr uint64_t UDataTable_FindRowUnchecked       = 0x359CB9C;

    constexpr uint64_t GenericArray_Remove               = 0x944B2DC;

    constexpr uint64_t UStruct_IsChildOf                 = 0x753C14C;
    constexpr uint64_t GMalloc                           = 0xDC55AC0;

    // ==== curl_easy_setopt inside libUnreal.so ====
    constexpr uint64_t curl_easy_setopt                  = 0x0B1CA0B8;
}

// UE 4.27 Android (Itanium ABI) struct offsets
namespace offsets {
    constexpr int32_t Class        = 0x10;
    constexpr int32_t Name         = 0x18;
    constexpr int32_t Outer        = 0x20;
    constexpr int32_t Children     = 0x48;
    constexpr int32_t Next         = 0x30;
    constexpr int32_t SuperClass   = 0x40;
    constexpr int32_t Offset       = 0x4C;
    constexpr int32_t ParamsSize   = 0xE6;
    constexpr int32_t ReturnValueOffset = 0xE8;
    constexpr int32_t StructSize   = 0x5C;
    constexpr int32_t RowStruct    = 0x28;
}

// ============================================================================
// UTILITIES — Memory
// ============================================================================
static inline bool IsBadPtr(const void* p) {
    if (!p) return true;
    // Android: 47-bit user space
    return (uintptr_t)p < 0x10000 || (uintptr_t)p > 0x7FFFFFFFFFFF;
}

// ============================================================================
// LIBRARY UTILITIES
// ============================================================================
namespace Lib {
    static uintptr_t Find(const char* name) {
        FILE* f = fopen("/proc/self/maps", "r");
        if (!f) return 0;
        char line[512];
        uintptr_t base = 0;
        while (fgets(line, sizeof(line), f)) {
            if (strstr(line, name)) {
                base = strtoul(line, nullptr, 16);
                break;
            }
        }
        fclose(f);
        return base;
    }

    static uintptr_t WaitFor(const char* name, int timeoutSec = 60) {
        for (int i = 0; i < timeoutSec * 10; ++i) {
            uintptr_t b = Find(name);
            if (b) return b;
            usleep(100 * 1000);
        }
        return 0;
    }
}

// ============================================================================
// SDK — Minimal UE4 Types
// ============================================================================
using int32  = int32_t;
using int64  = int64_t;
using uint8  = uint8_t;
using uint32 = uint32_t;
using uint64 = uint64_t;

class UObject;

// ---- FName ----
struct FName {
    uint32 ComparisonIndex;
    uint32 DisplayIndex;
};

// ---- FString ----
struct FString {
    wchar_t* Data;
    int32    Count;
    int32    Max;

    FString() : Data(nullptr), Count(0), Max(0) {}

    std::string ToString() const {
        if (!Data) return {};
        std::string out;
        out.reserve(Count);
        for (int32 i = 0; i < Count - 1; ++i) { // exclude null term
            wchar_t wc = Data[i];
            out.push_back(wc < 128 ? (char)wc : '?');
        }
        return out;
    }
};

// ---- TArray ----
template<typename T>
struct TArray {
    T*    Data;
    int32 Count;
    int32 Max;

    T& operator[](int32 i) { return Data[i]; }
    const T& operator[](int32 i) const { return Data[i]; }
    T* begin() { return Data; }
    T* end()   { return Data + Count; }
    int32 MaxIndex() const { return Count - 1; }

    void Add(const T& item) {
        extern void* (*ReallocFn)(void*, size_t, uint32);
        Data = (T*)ReallocFn(Data, sizeof(T) * (Count + 1), 0);
        Data[Count++] = item;
        Max = Count;
    }
};

// ---- FVector / FRotator / FGuid ----
struct FVector   { float X, Y, Z; };
struct FRotator  { float Pitch, Yaw, Roll; };
struct FGuid     { int32 A, B, C, D; };

// ---- UObject ----
class UObject {
public:
    void**  Vtable;
    int32   ObjectFlags;
    int32   InternalIndex;
    UObject* Class;
    FName    Name;
    UObject* Outer;

    std::string GetName() {
        extern void (*FNameToStringFn)(FName*, FString&);
        extern void (*FreeInternalFn)(void*);

        FString out;
        FNameToStringFn(&this->Name, out);
        std::string s = out.ToString();
        if (out.Data) FreeInternalFn(out.Data);
        return s;
    }

    bool IsChild(UObject* parent) {
        UObject* c = this->Class;
        while (c) {
            if (c == parent) return true;
            c = *(UObject**)((uintptr_t)c + offsets::SuperClass);
        }
        return false;
    }
};

// ---- GObjects ----
struct FUObjectItem { UObject* Object; int32 Flags; int32 Cluster; int32 Serial; };
struct TUObjectArray { void* Objects[9]; };
struct GObjects_t {
    TUObjectArray* ObjectArray;
    uint8  _pad[0xC];
    uint32 ObjectCount;

    UObject* GetById(uint32 id) {
        // UE 4.27 chunk size = 0x10000
        const uint32 chunkSize = 0x10000;
        uint32 chunkIdx = id / chunkSize;
        uint32 within   = id % chunkSize;
        if (chunkIdx >= 9) return nullptr;
        auto* chunk = (FUObjectItem*)ObjectArray->Objects[chunkIdx];
        if (!chunk) return nullptr;
        return chunk[within].Object;
    }
};

// ============================================================================
// GLOBALS
// ============================================================================
static uintptr_t g_base = 0;

// Function pointers
static void   (*FNameToStringFn)(FName*, FString&)          = nullptr;
static void   (*FreeInternalFn)(void*)                       = nullptr;
static void*  (*ReallocFn)(void*, size_t, uint32)            = nullptr;
static void*  (*SpawnActorFn)(void*, UObject*, FVector*, FRotator*, const void*) = nullptr;
static UObject* (*StaticFindObjectFn)(UObject*, void*, const wchar_t*, bool) = nullptr;
static UObject* (*StaticLoadObjectFn)(UObject*, UObject*, const wchar_t*, const wchar_t*, int, void*, bool) = nullptr;

// ProcessEvent
static void* (*orig_ProcessEvent)(UObject*, UObject*, void*) = nullptr;

// Game state
static UObject* PlayerController = nullptr;
static UObject* GameState        = nullptr;
static UObject* GameMode         = nullptr;
static UObject* World            = nullptr;
static UObject* Pawn             = nullptr;
static UObject* GameViewport     = nullptr;
static UObject* CheatManager     = nullptr;
static UObject* GameStatics      = nullptr;
static UObject* kismetStringLib  = nullptr;
static UObject* kismetSystemLib  = nullptr;
static GObjects_t* GObjects      = nullptr;

// Flags
static bool bInFrontend        = true;
static bool bPressedPlay       = false;
static bool bLoadedInMatch     = false;
static bool bDroppedFromAircraft = false;

// ============================================================================
// OBJECT FINDERS
// ============================================================================
static UObject* FindObject(const wchar_t* path, bool bLoad = false, bool bAnyPkg = false) {
    if (!StaticFindObjectFn) return nullptr;
    UObject* obj = StaticFindObjectFn(nullptr, bAnyPkg ? (void*)-1 : nullptr, path, false);
    if (!obj && bLoad && StaticLoadObjectFn) {
        obj = StaticLoadObjectFn(nullptr, nullptr, path, nullptr, 0, nullptr, true);
    }
    return obj;
}

static std::wstring ToWide(const std::string& s) {
    std::wstring w;
    w.reserve(s.size());
    for (char c : s) w.push_back((wchar_t)c);
    return w;
}

static std::string ToNarrow(const wchar_t* w) {
    if (!w) return {};
    std::string s;
    while (*w) { s.push_back(*w < 128 ? (char)*w : '?'); ++w; }
    return s;
}

// ============================================================================
// UObject::Call — via ProcessEvent
// ============================================================================
template<typename Ret = void, typename... Args>
static Ret UCall(UObject* obj, const char* funcName, Args... args) {
    if (!obj || !orig_ProcessEvent) return Ret{};

    // Find the function
    UObject* func = nullptr;
    UObject* klass = obj->Class;
    while (klass && !func) {
        UObject* child = *(UObject**)((uintptr_t)klass + offsets::Children);
        while (child) {
            std::string n = child->GetName();
            if (n == funcName) { func = child; break; }
            child = *(UObject**)((uintptr_t)child + offsets::Next);
        }
        klass = *(UObject**)((uintptr_t)klass + offsets::SuperClass);
    }
    if (!func) return Ret{};

    // Get params size and offsets
    int16_t paramsSize = *(int16_t*)((uintptr_t)func + offsets::ParamsSize);
    int16_t retOffset  = *(int16_t*)((uintptr_t)func + offsets::ReturnValueOffset);

    uint8_t* params = (uint8_t*)calloc(1, paramsSize);

    // Fill args sequentially
    int idx = 0;
    auto fill = [&](auto&& v) {
        // Each arg offset — we approximate as sizeof-aligned
        // For accurate placement, need to walk Function->Children offsets
        // Simplified: assume first args are at fixed offsets
        memcpy(params + (idx == 0 ? 0 : 0) + (idx * 8), &v, sizeof(v));
        idx++;
    };
    (void)fill; // reserved for future arg mapping

    orig_ProcessEvent(obj, func, params);
    Ret r{};
    if (retOffset > 0) r = *(Ret*)(params + retOffset);
    free(params);
    return r;
}

// ============================================================================
// CURL HOOK — URL redirect + SSL bypass
// ============================================================================
static bool HostShouldRedirect(const std::string& host) {
    return host.find("ol.epicgames.com")   != std::string::npos ||
           host.find(".akamaized.net")     != std::string::npos ||
           host.find("on.epicgames.com")   != std::string::npos ||
           host.find("epicgames.com")      != std::string::npos ||
           host.find("fortnite.com")       != std::string::npos ||
           host.find("unrealengine.com")   != std::string::npos;
}

static std::string RewriteUrl(const std::string& url) {
    size_t schemeEnd = url.find("://");
    if (schemeEnd == std::string::npos) return url;

    size_t hostStart = schemeEnd + 3;
    size_t hostEnd   = url.find_first_of("/?", hostStart);
    if (hostEnd == std::string::npos) hostEnd = url.size();

    std::string host = url.substr(hostStart, hostEnd - hostStart);
    if (!HostShouldRedirect(host)) return url;

    // Rebuild: scheme://newhost + original path/query
    return std::string(REDIRECT_SCHEME) + "://" + REDIRECT_HOST + url.substr(hostEnd);
}

install_hook_name(curl_easy_setopt, void*, void* curl, int option, void* arg)
{
    if (option == CURLOPT_URL && !IsBadPtr(arg)) {
        std::string original = (const char*)arg;
        std::string rewritten = RewriteUrl(original);
        if (rewritten != original) {
            LOGI("[curl] %s -> %s", original.c_str(), rewritten.c_str());
            return orig_curl_easy_setopt(curl, option, (void*)rewritten.c_str());
        }
        return orig_curl_easy_setopt(curl, option, arg);
    }

    if (option == CURLOPT_SSL_VERIFYPEER || option == CURLOPT_SSL_VERIFYHOST) {
        return orig_curl_easy_setopt(curl, option, (void*)0);
    }

    return orig_curl_easy_setopt(curl, option, arg);
}

// ============================================================================
// GAME LOGIC
// ============================================================================
static void SwitchMap(const char* mapName) {
    if (!PlayerController) return;

    // PlayerController->SwitchLevel(FString(mapName))
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
    if (!func) { LOGE("SwitchLevel not found"); return; }

    int16_t ps = *(int16_t*)((uintptr_t)func + offsets::ParamsSize);
    uint8_t* params = (uint8_t*)calloc(1, ps);

    // Build FString on heap (needs to be static so engine can read it during call)
    std::wstring w = ToWide(mapName);
    std::wstring* wheap = new std::wstring(w);
    FString* fs = (FString*)(params);
    fs->Data  = (wchar_t*)wheap->c_str();
    fs->Count = (int32)wheap->size() + 1;
    fs->Max   = (int32)wheap->size() + 1;

    orig_ProcessEvent(PlayerController, func, params);

    // Note: don't free here — engine may keep a reference. Leak is fine.
}

static void PlayButton() {
    SwitchMap("Artemis_Terrain"); // CH3 S3
    bPressedPlay = true;
    bInFrontend  = false;
    LOGI("Play pressed -> Artemis_Terrain");
}

static void InitializeGlobals() {
    // Find world/GameInstance chain
    UObject* defaultGameStatics = FindObject(L"/Script/Engine.Default__GameplayStatics");
    if (!defaultGameStatics) return;

    // GetPlayerController(World, 0) — try via chain
    // For robustness, use GObjects scan
    // Alternate: find first PlayerController in GObjects
    for (uint32 i = 0; i < GObjects->ObjectCount; ++i) {
        UObject* o = GObjects->GetById(i);
        if (!o) continue;
        std::string n = o->GetName();
        if (n == "FortPlayerControllerAthena" || n == "FortPlayerController") {
            PlayerController = o;
            break;
        }
    }

    if (!PlayerController) {
        LOGI("PlayerController not found yet");
        return;
    }

    World  = *(UObject**)((uintptr_t)PlayerController + 0); // placeholder
    // World access via PlayerController->GetWorld() — skip for brevity, use global search

    for (uint32 i = 0; i < GObjects->ObjectCount; ++i) {
        UObject* o = GObjects->GetById(i);
        if (!o) continue;
        std::string n = o->GetName();
        if (!GameState && (n == "FortGameStateAthena" || n == "FortGameState")) GameState = o;
        if (!GameMode  && (n == "FortGameModeAthena"  || n == "FortGameMode"))  GameMode  = o;
        if (GameState && GameMode) break;
    }

    LOGI("PC=%p GS=%p GM=%p", PlayerController, GameState, GameMode);
}

static void OnReadyToStartMatch() {
    LOGI("=== ReadyToStartMatch ===");
    bLoadedInMatch = true;
    bInFrontend    = false;

    InitializeGlobals();

    // NOTE: full Athena spawn logic (SpawnPawn, Inventory, ShowSkin...) lives here
    // in the full project. For this single-file build, we stop at logging.
}

static void OnServerLoadingScreenDropped() {
    LOGI("=== ServerLoadingScreenDropped ===");
    if (bInFrontend) return;
}

static void OnAircraftJump() {
    LOGI("=== ServerAttemptAircraftJump ===");
    bDroppedFromAircraft = true;
}

// ============================================================================
// PROCESS EVENT HOOK
// ============================================================================
static void* ProcessEventHook(UObject* self, UObject* func, void* params)
{
    if (!func || !orig_ProcessEvent) return nullptr;

    // Fast path: null-checked name
    std::string name = func->GetName();
    if (name.empty()) return orig_ProcessEvent(self, func, params);

    // ---- Game-flow events ----
    if (name.find("ReadyToStartMatch") != std::string::npos && !bLoadedInMatch) {
        OnReadyToStartMatch();
    }
    else if (name.find("ServerLoadingScreenDropped") != std::string::npos && bLoadedInMatch) {
        OnServerLoadingScreenDropped();
    }
    else if (name.find("ServerAttemptAircraftJump") != std::string::npos && !bDroppedFromAircraft) {
        OnAircraftJump();
    }
    // ---- Play button ----
    else if ((name.find("BP_PlayButton") != std::string::npos ||
              name.find("PlayButton")    != std::string::npos) && !bPressedPlay) {
        PlayButton();
    }
    // ---- Return to menu ----
    else if (name.find("ReturnToMainMenu") != std::string::npos) {
        SwitchMap("Frontend");
        bInFrontend = true;
        bPressedPlay = false;
    }

    return orig_ProcessEvent(self, func, params);
}

// ============================================================================
// CORE SETUP
// ============================================================================
static void CoreSetup()
{
    g_base = Lib::Find("libUnreal.so");
    if (!g_base) { LOGE("libUnreal.so not mapped!"); return; }
    LOGI("libUnreal.so base = %p", (void*)g_base);

    // ---- Function pointers ----
    FNameToStringFn   = (decltype(FNameToStringFn))  (g_base + RVAs::FNameToString);
    FreeInternalFn    = (decltype(FreeInternalFn))   (g_base + RVAs::FreeInternal);
    ReallocFn         = (decltype(ReallocFn))        (g_base + RVAs::Realloc);
    SpawnActorFn      = (decltype(SpawnActorFn))     (g_base + RVAs::SpawnActor_Thunk);

    // GObjects
    GObjects = *(GObjects_t**)(g_base + RVAs::GObjects);
    LOGI("GObjects = %p (count=%u)", GObjects, GObjects ? GObjects->ObjectCount : 0);

    // StaticFindObject / StaticLoadObject — resolve by scanning callers is complex.
    // For CH3, we approximate via direct exports if present, else skip.
    // IMPORTANT: If the game provides them as exported symbols, use dlsym:
    void* h = dlopen("libUnreal.so", RTLD_NOW | RTLD_NOLOAD);
    if (h) {
        StaticFindObjectFn = (decltype(StaticFindObjectFn))dlsym(h, "StaticFindObject");
        StaticLoadObjectFn = (decltype(StaticLoadObjectFn))dlsym(h, "StaticLoadObject");
    }

    // Fallback: FindObject won't work without StaticFindObject,
    // but we can use GObjects scan.
    LOGI("StaticFindObject = %p", StaticFindObjectFn);

    // ---- Hook ProcessEvent ----
    void* pPE = (void*)(g_base + RVAs::ProcessEvent);
    if (DobbyHook(pPE, (void*)ProcessEventHook, (void**)&orig_ProcessEvent) == 0) {
        LOGI("ProcessEvent hooked @ %p", pPE);
    } else {
        LOGE("Failed to hook ProcessEvent");
    }

    // ---- Hook curl_easy_setopt ----
    void* pCurl = (void*)(g_base + RVAs::curl_easy_setopt);
    if (DobbyHook(pCurl, (void*)hook_curl_easy_setopt, (void**)&orig_curl_easy_setopt) == 0) {
        LOGI("curl_easy_setopt hooked @ %p", pCurl);
    } else {
        LOGE("Failed to hook curl_easy_setopt");
    }
}

// ============================================================================
// MAIN THREAD
// ============================================================================
static void* MainThread(void*)
{
    LOGI("=== EraCH3 starting ===");

    if (!Lib::WaitFor("libUnreal.so")) {
        LOGE("libUnreal.so never appeared");
        return nullptr;
    }
    usleep(300 * 1000); // let it settle

    CoreSetup();

    LOGI("=== EraCH3 ready ===");
    return nullptr;
}

// ============================================================================
// JNI ENTRY
// ============================================================================
extern "C" __attribute__((constructor))
void libEraCH3_main()
{
    pthread_t t;
    pthread_create(&t, nullptr, MainThread, nullptr);
    pthread_detach(t);
}
