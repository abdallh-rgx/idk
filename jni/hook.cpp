#include "profile.h"
#include "loot_data.h"
#include "../include/athena.h"
#include <android/log.h>
#include <unistd.h>
#include <atomic>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <time.h>

#define LOG(...) athena_log(__VA_ARGS__)
extern "C" int DobbyHook(void*,void*,void**);
namespace {
uintptr_t base=0;
template<class F> F fn(uintptr_t rva) {return reinterpret_cast<F>(base+rva);}
template<class T> T read(const void* p,size_t off=0) {
    T result{}; if(p) memcpy(&result,static_cast<const char*>(p)+off,sizeof(T)); return result;
}
template<class T> void write(void* p,size_t off,const T& value) {
    memcpy(static_cast<char*>(p)+off,&value,sizeof(T));
}
using PE=void(*)(void*,void*,void*);
using Tick=void(*)(void*,int,float);
using Exec=bool(*)(void*,void*,const char16_t*,void*);
PE original_pe=nullptr; Tick original_tick=nullptr; Exec original_exec=nullptr;
std::atomic<int> game_tid{0};
thread_local bool internal=false;
struct Guard {bool saved=internal; Guard(){internal=true;} ~Guard(){internal=saved;}};
struct FString {char16_t* data=nullptr; int count=0,capacity=0;};
struct Array {void* data;int count,capacity;};
struct Guid {uint32_t a,b,c,d;};
struct alignas(16) Transform {
    float rotation[4]{0,0,0,1};
    float location[3]{0,0,5000};
    float scale[3]{1,1,1};
    float trailing_padding[2]{};
};
static_assert(sizeof(Transform)==48 && sizeof(FString)==16 && sizeof(Guid)==16);
// Android scalar FTransform has contiguous FVector members, unlike the PC SIMD layout.
static_assert(offsetof(Transform,rotation)==0 && offsetof(Transform,location)==0x10 &&
              offsetof(Transform,scale)==0x1c);

std::string narrow(const char16_t* s,int maximum=1024) {
    std::string out;
    if(s) for(int i=0;i<maximum && s[i];++i) out.push_back(s[i]<128?char(s[i]):'?');
    return out;
}
std::u16string wide(const std::string& s) {return std::u16string(s.begin(),s.end());}
std::string name(void* obj) {
    if(!obj) return {};
    FString s{};
    fn<void(*)(const void*,FString*)>(profile::NameAppendString)(
        static_cast<char*>(obj)+profile::ObjectName,&s);
    std::string out=narrow(s.data,std::min(std::max(s.count,0),1024));
    if(s.data) fn<void(*)(void*)>(profile::Free)(s.data);
    return out;
}
void* find(const std::string& path) {
    auto s=wide(path);
    return fn<void*(*)(void*,void*,const char16_t*,bool)>(profile::StaticFindObject)(
        nullptr,nullptr,s.c_str(),false);
}
bool isa(void* object,void* type) {
    if(!object || !type) return false;
    void* klass=read<void*>(object,profile::ObjectClass);
    if(!klass) return false;
    int n=read<int>(type,0x48), limit=read<int>(klass,0x48);
    void* chain=read<void*>(klass,0x40);
    return n>=0 && n<1024 && n<=limit && chain &&
        read<void*>(chain,size_t(n)*8)==static_cast<char*>(type)+0x40;
}
bool subclass(void* klass,void* type) {
    if(!klass || !type)return false;
    int n=read<int>(type,0x48),limit=read<int>(klass,0x48);
    void* chain=read<void*>(klass,0x40);
    return n>=0 && n<1024 && n<=limit && chain &&
        read<void*>(chain,size_t(n)*8)==static_cast<char*>(type)+0x40;
}
void* property(const std::string& path) {
    void* p=find(path);
    if(!p) {auto colon=path;auto pos=colon.rfind('.');if(pos!=std::string::npos)colon[pos]=':';p=find(colon);}
    if(!p) {LOG("Missing property %s",path.c_str()); return nullptr;}
    int offset=read<int>(p,profile::PropertyOffset), size=read<int>(p,profile::PropertyElementSize);
    if(offset<0 || offset>0x20000 || size<=0 || size>0x10000) {
        LOG("Rejected property layout %s offset=%x size=%x",path.c_str(),offset,size); return nullptr;
    }
    return p;
}
void* field(void* obj,void* p) {
    return obj && p ? static_cast<char*>(obj)+read<int>(p,profile::PropertyOffset):nullptr;
}
void set_bool(void* obj,const char* path,bool enabled) {
    void* p=property(path); if(!p || !obj) return;
    uint8_t size=read<uint8_t>(p,0x80), byte=read<uint8_t>(p,0x81),mask=read<uint8_t>(p,0x82);
    if(!size || size>8 || byte>=size || !mask) {LOG("Rejected bool mask %s",path);return;}
    auto dst=static_cast<uint8_t*>(field(obj,p))+byte;
    *dst=enabled?(*dst|mask):(*dst&~mask);
}

// Parameter storage comes from Android UProperty metadata. No Windows SDK layouts.
class Call {
    void* function=nullptr;
    struct Param {std::string name;int offset,size;uint64_t flags;};
    std::vector<Param> params;
    alignas(16) std::array<uint8_t,4096> data{};
    bool valid=false;
public:
    explicit Call(const char* path,void* event_receiver=nullptr) {
        function=find(path);
        if(!function) {LOG("Missing function %s",path);return;}
        if(event_receiver) {
            // Blueprint events must resolve the receiver's override, not execute the empty base event.
            std::string event=name(function);
            void* klass=read<void*>(event_receiver,profile::ObjectClass);
            void* chain=read<void*>(klass,0x40);int depth=read<int>(klass,0x48);
            bool found=false;
            if(chain && depth>=0 && depth<1024)for(int i=depth;i>=0 && !found;--i) {
                void* base_chain=read<void*>(chain,size_t(i)*8);if(!base_chain)continue;
                void* type=static_cast<char*>(base_chain)-0x40;
                void* child=read<void*>(type,profile::StructChildren);
                for(int j=0;child && j<4096;++j,child=read<void*>(child,profile::FieldNext))
                    if(name(child)==event && isa(child,find("/Script/CoreUObject.Function"))) {
                        function=child;found=true;break;
                    }
            }
        }
        int total=read<uint16_t>(function,profile::FunctionParamsSize);
        if(total>int(data.size())) return;
        void* p=read<void*>(function,profile::StructChildren);
        for(int i=0;p && i<128;++i,p=read<void*>(p,profile::FieldNext)) {
            uint64_t flags=read<uint64_t>(p,profile::PropertyFlags);
            if(!(flags&0x80)) continue;
            int offset=read<int>(p,profile::PropertyOffset), size=read<int>(p,profile::PropertyElementSize);
            if(offset<0 || size<1 || offset+size>total) {
                LOG("Invalid parameter metadata %s",path);return;
            }
            params.push_back({name(p),offset,size,flags});
        }
        valid=total==0 || !params.empty();
    }
    bool ok()const {return valid;}
    template<class T> bool set(const char* key,const T& value) {
        for(auto& p:params) if(p.name==key && p.size==sizeof(T)) {
            memcpy(data.data()+p.offset,&value,sizeof(T)); return true;
        }
        LOG("Parameter unavailable: %s size=%zu",key,sizeof(T));return false;
    }
    template<class T> T result()const {
        for(auto& p:params) if((p.flags&0x400) && p.size==sizeof(T)) return read<T>(data.data(),p.offset);
        return {};
    }
    template<class T> T get(const char* key)const {
        for(auto& p:params)if(p.name==key && p.size==sizeof(T))return read<T>(data.data(),p.offset);
        return {};
    }
    bool set_bytes(const char* key,const void* source,int size) {
        for(auto& p:params)if(p.name==key && p.size==size && source) {
            memcpy(data.data()+p.offset,source,size);return true;
        }
        LOG("Parameter size mismatch: %s size=%d",key,size);return false;
    }
    void* result_data(int size) {
        for(auto& p:params)if((p.flags&0x400) && p.size==size)return data.data()+p.offset;
        return nullptr;
    }
    void dump(const char* label)const {
        LOG("[Call %s] valid=%d params=%zu",label,(int)valid,params.size());
        for(auto& p:params)LOG("   '%s' off=%d size=%d flags=%llx",p.name.c_str(),p.offset,p.size,
            (unsigned long long)p.flags);
    }
    bool run(void* object) {
        if(!valid || !object) return false;
        void* owner=read<void*>(function,profile::ObjectOuter);
        if(!isa(object,owner)) {
            LOG("Rejected call %s: receiver=%s owner=%s",name(function).c_str(),
                name(read<void*>(object,profile::ObjectClass)).c_str(),name(owner).c_str());
            return false;
        }
        Guard guard;
        original_pe(object,function,data.data());return true;
    }
};
void invoke0(void* obj,const char* path) {Call c(path);if(c.ok()) c.run(obj);}
void* incoming(void* function,void* data,const char* key,int expected_size=0) {
    if(!function || !data)return nullptr;
    int total=read<uint16_t>(function,profile::FunctionParamsSize);
    void* p=read<void*>(function,profile::StructChildren);
    for(int i=0;p && i<128;++i,p=read<void*>(p,profile::FieldNext)) {
        if(!(read<uint64_t>(p,profile::PropertyFlags)&0x80) || name(p)!=key)continue;
        int offset=read<int>(p,profile::PropertyOffset),size=read<int>(p,profile::PropertyElementSize);
        if(offset<0 || size<1 || offset+size>total || (expected_size && size!=expected_size))return nullptr;
        return static_cast<char*>(data)+offset;
    }
    return nullptr;
}
template<class T> bool get_field(void* object,const char* path,T& value) {
    void* p=property(path);
    if(!object || !p || read<int>(p,profile::PropertyElementSize)!=sizeof(T))return false;
    value=read<T>(field(object,p));return true;
}

void* load_class(const char* path,const char* base_class="/Script/Engine.Actor") {
    if(void* result=find(path)) return result;
    auto s=wide(path);void* parent=find(base_class);if(!parent)return nullptr;
    return fn<void*(*)(void*,void*,const char16_t*,const char16_t*,uint32_t,void*)>(profile::StaticLoadClass)(
        parent,nullptr,s.c_str(),nullptr,0,nullptr);
}
void* load_asset(const std::string& path,const char* type_path="/Script/FortniteGame.FortItemDefinition") {
    if(void* result=find(path)) return result;
    auto s=wide(path);void* type=find(type_path);if(!type)return nullptr;
    return fn<void*(*)(void*,void*,const char16_t*,const char16_t*,uint32_t,void*,bool,void*)>(profile::StaticLoadObject)(
        type,nullptr,s.c_str(),nullptr,0,nullptr,true,nullptr);
}
void* by_short_name(const std::string& wanted) {
    void* definition_type=find("/Script/FortniteGame.FortItemDefinition");
    if(!definition_type)return nullptr;
    auto objects=reinterpret_cast<void*>(base+profile::ObjectArray);
    void* chunks=read<void*>(objects,0x10);int total=read<int>(objects,0x24);
    int chunk_count=read<int>(objects,0x2c);
    if(!chunks || total<0 || total>4000000 || chunk_count<1 || chunk_count>128)return nullptr;
    for(int i=0;i<total;++i) {
        int chunk_index=i>>16;if(chunk_index>=chunk_count)break;
        void* chunk=read<void*>(chunks,size_t(chunk_index)*8);if(!chunk)continue;
        void* object=read<void*>(chunk,size_t(i&0xffff)*24);
        if(object && isa(object,definition_type) && name(object)==wanted)return object;
    }
    return nullptr;
}
void* spawn(void* world,void* klass,void* owner,const Transform& transform) {
    if(!world || !klass)return nullptr;
    void* rotation=property("/Script/CoreUObject.Transform.Rotation");
    void* translation=property("/Script/CoreUObject.Transform.Translation");
    void* scale=property("/Script/CoreUObject.Transform.Scale3D");
    if(!rotation || !translation || !scale ||
       read<int>(rotation,profile::PropertyOffset)!=0 || read<int>(translation,profile::PropertyOffset)!=0x10 ||
       read<int>(scale,profile::PropertyOffset)!=0x1c) {
        LOG("Spawn refused: Android FTransform reflection layout mismatch");return nullptr;
    }
    LOG("Spawn transform verified: scale=%.2f/%.2f/%.2f",transform.scale[0],transform.scale[1],transform.scale[2]);
    void* actor=fn<void*(*)(void*,void*,const Transform*,uint8_t,void*)>(profile::BeginSpawn)(
        world,klass,&transform,1,owner); // AlwaysSpawn
    return actor?fn<void*(*)(void*,const Transform*)>(profile::FinishSpawn)(actor,&transform):nullptr;
}

struct Session {
    void* world=nullptr;int world_index=-1;
    void* controller=nullptr;void* pawn=nullptr;void* inventory=nullptr;
    bool requested=false,attempted=false,inventory_ready=false;
    bool awaiting_travel=false,wait_reported=false;
    int last_phase=-1;
    double observed_at=0,travel_at=0,finalize_at=0;
    bool finalized=false;
    float elapsed=0;
    std::array<void*,6> slots{};
    std::array<void*,6> secondary_slots{};
    unsigned input_reports=0;
    bool lobby_requested=false,touch_build_held=false;
    double next_calendar=0,next_build_poll=0,last_build_time=0;
    void* held_build_class=nullptr;void* last_build_class=nullptr;
    std::array<float,6> held_build_transform{},last_build_transform{};
    void* event_stream=nullptr;
    bool event_pending=false,event_started=false;
    double event_deadline=0,event_poll=0;
} session;
const char* athena_phase_name(int phase) {
    switch(phase) {
        case 0:return "None"; case 1:return "Setup"; case 2:return "Warmup";
        case 3:return "Aircraft"; case 4:return "SafeZones"; case 5:return "EndGame";
        case 6:return "Count"; default:return "Unknown";
    }
}
void* departing_world=nullptr;
int departing_index=-1;
std::atomic<int> ui_command{0};
extern "C" void athena_enqueue_command(const char* command) {
    if(!command)return;
    if(!strcmp(command,"teleport"))ui_command.store(1);
    else if(!strcmp(command,"open frontend"))ui_command.store(2);
    else if(!strcmp(command,"startevent"))ui_command.store(3);
    else if(!strcmp(command,"open Athena_Terrain"))ui_command.store(4);
}
double monotonic_seconds() {
    timespec ts{};clock_gettime(CLOCK_MONOTONIC,&ts);
    return double(ts.tv_sec)+double(ts.tv_nsec)*1e-9;
}
void request_end_event();
// Build 5 retains Athena's original game mode, controller, and game state.
// No CDO changes, replacement-controller hook, or Engine.GameMode travel.
void restore_defaults() {}
void* object_field(void* object,const char* path) {
    void* p=property(path);
    return p && object && read<int>(p,profile::PropertyElementSize)==8?read<void*>(field(object,p)):nullptr;
}
struct Vector {float x,y,z;};
bool start_transform(void* mode,void* controller,Transform& transform) {
    Call choose("/Script/Engine.GameModeBase.ChoosePlayerStart");
    if(!choose.ok() || !choose.set("Player",controller) || !choose.run(mode))return false;
    void* start=choose.result<void*>();
    if(!isa(start,find("/Script/Engine.PlayerStart")))return false;
    Call location("/Script/Engine.Actor.K2_GetActorLocation");
    if(!location.run(start))return false;
    auto v=location.result<Vector>();
    if(!std::isfinite(v.x) || !std::isfinite(v.y) || !std::isfinite(v.z))return false;
    transform.location[0]=v.x;transform.location[1]=v.y;transform.location[2]=v.z+100.0f;
    LOG("PlayerStart %s at %.0f %.0f %.0f",name(start).c_str(),v.x,v.y,v.z);
    return true;
}

// ============================================================================
// 🔧 FIX: grant_athena_abilities — PlayerState ASC first + direct grant
// ============================================================================
void grant_athena_abilities() {
    constexpr uintptr_t resolve_interface=0x5d8f3c8;
    const uint32_t expected[]={0xa9be4ff4,0xa9017bfd,0x910043fd,0xaa0103f4};
    if(memcmp(reinterpret_cast<void*>(base+resolve_interface),expected,sizeof(expected))) {
        LOG("Ability grant stopped: interface resolver prologue mismatch");return;
    }
    void* type=find("/Script/GameplayAbilities.AbilitySystemInterface");
    void* state=object_field(session.controller,"/Script/Engine.Controller.PlayerState");
    void* pawn_asc=object_field(session.pawn,"/Script/FortniteGame.FortPawn.AbilitySystemComponent");
    void* state_asc=object_field(state,"/Script/FortniteGame.FortPlayerState.AbilitySystemComponent");
    LOG("Ability components pawn=%p state=%p",pawn_asc,state_asc);
    struct Interface {void* object;void* address;};
    Interface actor{session.pawn,nullptr};
    if(type)actor.address=fn<void*(*)(void*,void*)>(resolve_interface)(actor.object,type);
    if(!actor.address && state && type) {
        actor.object=state;actor.address=fn<void*(*)(void*,void*)>(resolve_interface)(state,type);
    }
    if(!actor.address) {LOG("Ability grant stopped: no native ability interface");return;}
    void* ability_set=load_asset("/Game/Abilities/Player/Generic/Traits/DefaultPlayer/GAS_AthenaPlayer.GAS_AthenaPlayer",
        "/Script/FortniteGame.FortAbilitySet");
    if(!isa(ability_set,find("/Script/FortniteGame.FortAbilitySet"))) {LOG("GAS_AthenaPlayer unavailable");return;}
    // The stock 10.40 Athena set contains movement abilities, but the mobile
    // interaction/harvesting input abilities are granted separately by Era.
    // Add those classes to the loaded set before EquipFortAbilitySet runs so
    // the native set delivery path initializes their replicated specs too.
    void* abilities_prop=property("/Script/FortniteGame.FortAbilitySet.GameplayAbilities");
    Array* abilities=abilities_prop?reinterpret_cast<Array*>(field(ability_set,abilities_prop)):nullptr;
    if(abilities && abilities->count>=0 && abilities->count<256 && abilities->capacity>=abilities->count) {
        const char* extra[] = {
            "/Script/FortniteGame.FortGameplayAbility_Sprint",
            "/Script/FortniteGame.FortGameplayAbility_Jump",
            "/Game/Abilities/Player/Generic/Traits/DefaultPlayer/GA_DefaultPlayer_InteractSearch.GA_DefaultPlayer_InteractSearch_C",
            "/Game/Abilities/Player/Generic/Traits/DefaultPlayer/GA_DefaultPlayer_InteractUse.GA_DefaultPlayer_InteractUse_C"
        };
        for(const char* path:extra) {
            void* klass=load_class(path,"/Script/GameplayAbilities.GameplayAbility");
            if(!klass) { LOG("Default ability unavailable: %s",path); continue; }
            bool present=false;
            for(int i=0;i<abilities->count;++i) if(read<void*>(abilities->data,size_t(i)*8)==klass) {present=true;break;}
            if(!present) {
                int index=fn<int(*)(void*,void*,const void*)>(profile::GenericArrayAdd)(abilities,abilities_prop,&klass);
                LOG("Default ability %s index=%d",name(klass).c_str(),index);
            }
        }
        LOG("Athena ability set classes=%d",abilities->count);
    } else LOG("Athena ability set class array unavailable");
    Call grant("/Script/FortniteGame.FortKismetLibrary.EquipFortAbilitySet");
    if(!grant.ok() || !grant.set("AbilitySystemInterfaceActor",actor) || !grant.set("AbilitySet",ability_set) ||
       !grant.set("OverrideSourceObject",session.pawn))return;
    LOG("Equipping GAS_AthenaPlayer through native ability interface");
    if(!grant.run(find("/Script/FortniteGame.Default__FortKismetLibrary")))return;
    // Android wrapper destroys these two returned handle arrays; it does not revoke the abilities.
    struct SetHandle {uint64_t owner;Array abilities;Array effects;};
    static_assert(sizeof(SetHandle)==40);
    auto handle=grant.result<SetHandle>();
    LOG("Ability set returned abilities=%d effects=%d",handle.abilities.count,handle.effects.count);
    if(handle.abilities.data)fn<void(*)(void*)>(profile::Free)(handle.abilities.data);
    if(handle.effects.data)fn<void(*)(void*)>(profile::Free)(handle.effects.data);

    // 🔧 FIX: direct grant of Sprint/Jump on PlayerState ASC
    void* asc = state_asc ? state_asc : pawn_asc;
    if(asc) {
        void* abilityLib = find("/Script/FortniteGame.Default__FortAbilitySystemBlueprintLibrary");
        if(abilityLib) {
            const char* direct[] = {
                "/Script/FortniteGame.FortGameplayAbility_Sprint",
                "/Script/FortniteGame.FortGameplayAbility_Jump",
                nullptr
            };
            for(int i=0;direct[i];++i) {
                void* klass=find(direct[i]);
                if(!klass) continue;
                Call ga("/Script/FortniteGame.FortAbilitySystemBlueprintLibrary.GrantAndActivateAbilityOnSelf");
                if(!ga.ok()) {ga.dump("GrantAndActivateAbilityOnSelf"); break;}
                if(ga.set("AbilitySystem",asc) && ga.set("AbilityToGrant",klass)) {
                    if(ga.run(abilityLib)) LOG("Direct grant: %s",direct[i]);
                }
            }
        }
    }
}
void* optional_property(const char* path) {return find(path)?property(path):nullptr;}
void* optional_object(void* obj,const char* path) {
    void* p=optional_property(path);return p && obj && read<int>(p,profile::PropertyElementSize)==8?read<void*>(field(obj,p)):nullptr;
}
void* locker_loadout_property() {
    for(const char* path:{"/Script/FortniteGame.FortPlayerController.CosmeticLoadoutPC",
        "/Script/FortniteGame.FortPlayerControllerAthena.CustomizationLoadout",
        "/Script/FortniteGame.FortPlayerController.CustomizationLoadout"})
        if(void* p=optional_property(path))return p;
    return nullptr;
}
bool copy_struct_value(void* p,void* dest,const void* src) {
    if(!p || !dest || !src || !isa(p,find("/Script/CoreUObject.StructProperty")))return false;
    uintptr_t method=read<uintptr_t>(read<void*>(p),0x2c0);
    // Same reflected deep-copy operation used by GenericArrayAdd on this binary.
    if(method<base+0x2acd8a0 || method>=base+0x9223000)return false;
    reinterpret_cast<void(*)(void*,void*,const void*)>(method)(p,dest,src);return true;
}
void sync_locker_loadout() {
    void* target=locker_loadout_property();if(!target)return;
    void* info=optional_object(session.controller,"/Script/FortniteGame.FortPlayerController.MyPlayerInfo");
    void* idprop=optional_property("/Script/FortniteGame.FortRegisteredPlayerInfo.PlayerID");
    if(!info || !idprop) {LOG("Locker: MyPlayerInfo/PlayerID not ready");return;}
    Call context("/Script/BlueprintContext.BlueprintContextLibrary.GetContext");
    void* type=find("/Script/FortniteGame.FortMcpContext");
    if(!type || !context.set("ContextObject",session.controller) || !context.set("Class",type) ||
       !context.run(find("/Script/BlueprintContext.Default__BlueprintContextLibrary")))return;
    Call get("/Script/FortniteGame.FortMcpContext.GetLoadoutForPlayer");
    if(!get.set_bytes("MemberUniqueId",field(info,idprop),read<int>(idprop,profile::PropertyElementSize)) ||
       !get.run(context.result<void*>()))return;
    void* value=get.result_data(read<int>(target,profile::PropertyElementSize));
    if(value && copy_struct_value(target,field(session.controller,target),value))LOG("Locker loadout resolved through MyPlayerInfo and MCP");
}
void* locker_pickaxe() {
    void* prop=locker_loadout_property();void* loadout=field(session.controller,prop);
    void* pick=optional_object(loadout,"/Script/FortniteGame.FortAthenaLoadout.Pickaxe");
    return optional_object(pick,"/Script/FortniteGame.AthenaPickaxeItemDefinition.WeaponDefinition");
}
std::vector<void*> soft_asset_array(void* owner,const char* path) {
    std::vector<void*> result;void* p=property(path);if(!p || !owner)return result;
    void* inner=read<void*>(p,0x80);int stride=read<int>(inner,profile::PropertyElementSize);
    Array values=read<Array>(field(owner,p));
    if(stride!=40 || values.count<0 || values.count>64 || values.capacity<values.count || !values.data)return result;
    for(int i=0;i<values.count;++i) {
        Call load("/Script/Engine.KismetSystemLibrary.LoadAsset_Blocking");
        if(load.set_bytes("Asset",static_cast<char*>(values.data)+size_t(i)*stride,stride) &&
           load.run(find("/Script/Engine.Default__KismetSystemLibrary")))
            if(void* value=load.result<void*>())result.push_back(value);
    }
    return result;
}
void setup_cosmetics() {
    void* source=locker_loadout_property();void* loadout=field(session.controller,source);
    void* character=optional_object(loadout,"/Script/FortniteGame.FortAthenaLoadout.Character");
    void* state=object_field(session.controller,"/Script/Engine.Controller.PlayerState");
    void* hero=optional_object(character,"/Script/FortniteGame.AthenaCharacterItemDefinition.HeroDefinition");
    void* info=optional_object(session.controller,"/Script/FortniteGame.FortPlayerController.MyPlayerInfo");
    if(!hero)hero=optional_object(info,"/Script/FortniteGame.FortRegisteredPlayerInfo.AthenaMenuHeroDef");
    void* menu=optional_object(info,"/Script/FortniteGame.FortRegisteredPlayerInfo.TempAthenaMenuHeroInstance");
    if(!hero && menu) {Call get("/Script/FortniteGame.FortHero.GetHeroTypeBP");if(get.run(menu))hero=get.result<void*>();}
    void* hp=property("/Script/FortniteGame.FortPlayerState.HeroType");
    if(hero && state && hp && read<int>(hp,profile::PropertyElementSize)==8) {
        write(field(state,hp),0,hero);invoke0(state,"/Script/FortniteGame.FortPlayerState.OnRep_HeroType");
    }
    void* dest=optional_property("/Script/FortniteGame.FortPlayerPawn.CosmeticLoadout");
    if(!dest)dest=optional_property("/Script/FortniteGame.FortPlayerPawn.CustomizationLoadout");
    if(source && dest && read<int>(source,profile::PropertyElementSize)==read<int>(dest,profile::PropertyElementSize) &&
       read<void*>(source,0x80)==read<void*>(dest,0x80))
        copy_struct_value(dest,field(session.pawn,dest),loadout);
    Array menu_parts{};std::vector<void*> equipped_parts;
    if(menu && get_field(menu,"/Script/FortniteGame.FortHero.CharacterParts",menu_parts) &&
       menu_parts.data && menu_parts.count>0 && menu_parts.count<=16)
        for(int i=0;i<menu_parts.count;++i)equipped_parts.push_back(read<void*>(menu_parts.data,i*8));
    if(equipped_parts.empty() && hero)
        for(void* spec:soft_asset_array(hero,"/Script/FortniteGame.FortHeroType.Specializations"))
            for(void* part:soft_asset_array(spec,"/Script/FortniteGame.FortHeroSpecialization.CharacterParts"))equipped_parts.push_back(part);
    void* backpack=optional_object(loadout,"/Script/FortniteGame.FortAthenaLoadout.Backpack");
    Array back_parts{};
    if(backpack && get_field(backpack,"/Script/FortniteGame.AthenaCharacterPartItemDefinition.CharacterParts",back_parts) &&
       back_parts.data && back_parts.count>=0 && back_parts.count<=16)
        for(int i=0;i<back_parts.count;++i)equipped_parts.push_back(read<void*>(back_parts.data,i*8));
    for(void* part:equipped_parts) {
        uint8_t type=255;
        if(!isa(part,find("/Script/FortniteGame.CustomCharacterPart")) ||
           !get_field(part,"/Script/FortniteGame.CustomCharacterPart.CharacterPartType",type) || type>6)continue;
        Call choose("/Script/FortniteGame.FortPlayerPawn.ServerChoosePart");
        if(choose.set("Part",type) && choose.set("ChosenCharacterPart",part))choose.run(session.pawn);
    }
    if(!equipped_parts.empty()) {
        Array parts{equipped_parts.data(),int(equipped_parts.size()),int(equipped_parts.size())};
        Call apply("/Script/FortniteGame.FortKismetLibrary.ApplyCharacterCosmetics");
        if(apply.set("WorldContextObject",session.world) && apply.set("CharacterParts",parts) && apply.set("PlayerState",state))
            apply.run(find("/Script/FortniteGame.Default__FortKismetLibrary"));
    }
    invoke0(state,"/Script/FortniteGame.FortPlayerState.OnRep_CharacterData");
    Call refresh("/Script/FortniteGame.FortKismetLibrary.UpdatePlayerCustomCharacterPartsVisualization");
    if(refresh.set("PlayerState",state))refresh.run(find("/Script/FortniteGame.Default__FortKismetLibrary"));
    if(dest)invoke0(session.pawn,"/Script/FortniteGame.FortPlayerPawn.OnRep_CosmeticLoadout");
    LOG("Locker cosmetics character=%s hero=%s menu_parts=%d",name(character).c_str(),name(hero).c_str(),menu_parts.count);
    Call body("/Script/FortniteGame.FortPlayerPawnAthena.SetCharacterBodyVisibilityForPossession");
    bool shown=true;
    if(body.ok() && body.set("bVisible",shown)) {
        body.run(session.pawn);LOG("Body visibility request returned=%d",body.result<bool>());
    }
    Call visible("/Script/Engine.Actor.SetActorHiddenInGame");bool hidden=false;
    if(visible.ok() && visible.set("bNewHidden",hidden))visible.run(session.pawn);
}
bool readiness_layout(void* controller) {
    void* p=property("/Script/FortniteGame.FortPlayerController.bReadyToStartMatch");
    if(!p || !isa(controller,find("/Script/FortniteGame.FortPlayerController")))return false;
    bool valid=read<int>(p,profile::PropertyOffset)==0x630 &&
        read<uint8_t>(p,0x81)==0 && read<uint8_t>(p,0x82)==0x20;
    LOG("Readiness reflection matches Android setter=%d",valid);return valid;
}
void startup_commands() {
    // User-requested commands, once per successfully initialized Athena world.
    void* library=find("/Script/Engine.Default__KismetSystemLibrary");
    for(const char* text:{"bugitgo 0 0 3100","destroyall FortHLODSMActor"}) {
        auto buffer=wide(text);
        FString command{buffer.data(),int(buffer.size()+1),int(buffer.size()+1)};
        Call execute("/Script/Engine.KismetSystemLibrary.ExecuteConsoleCommand");
        if(execute.ok() && execute.set("WorldContextObject",session.world) &&
           execute.set("Command",command) && execute.set("SpecificPlayer",session.controller)) {
            LOG("Startup command: %s",text);
            LOG("Startup command dispatched=%d",execute.run(library));
        } else LOG("Startup command binding failed: %s",text);
    }
}

// ============================================================================
// 🔧 FIX 2: finish_loading — pushes GamePhase to Aircraft
// ============================================================================
void finish_loading(void* state) {
    // Build 19's working readiness flow: keep the normal Warmup phase and
    // let the existing offline bugitgo path settle the player on the map.
    void* phase=property("/Script/FortniteGame.FortGameStateAthena.GamePhase");
    if(phase && read<int>(phase,profile::PropertyElementSize)==1) {
        uint8_t old=read<uint8_t>(field(state,phase));
        if(old<2) {
            uint8_t warmup=2;write(field(state,phase),0,warmup);
            Call notify("/Script/FortniteGame.FortGameStateAthena.OnRep_GamePhase");
            if(notify.ok() && notify.set("OldGamePhase",old))notify.run(state);
        }
    }
    set_bool(session.controller,"/Script/FortniteGame.FortPlayerController.bHasClientFinishedLoading",true);
    set_bool(session.controller,"/Script/FortniteGame.FortPlayerController.bHasServerFinishedLoading",true);
    LOG("Using Build19 warmup readiness flow");
    invoke0(session.controller,"/Script/FortniteGame.FortPlayerController.ServerReadyToStartMatch");
    LOG("Readiness flags after callback=0x%02x",read<uint8_t>(session.controller,0x630));
    invoke0(session.controller,"/Script/FortniteGame.FortPlayerControllerZone.ClientOnPawnSpawned");

    // 🔧 FIX 2: Push GamePhase to Aircraft so the bus spawns
    if(phase && read<int>(phase,profile::PropertyElementSize)==1) {
        uint8_t current=read<uint8_t>(field(state,phase));
        if(current<3) {
            uint8_t aircraft=3;
            write(field(state,phase),0,aircraft);
            Call n("/Script/FortniteGame.FortGameStateAthena.OnRep_GamePhase");
            if(n.ok()) { n.set("OldGamePhase",current); n.run(state); }
            LOG("GamePhase forced: %s -> Aircraft",athena_phase_name(current));
        }
    }

    Call hud("/Script/Engine.PlayerController.GetHUD");
    if(hud.run(session.controller)) {
        void* value=hud.result<void*>();
        LOG("HUD=%p class=%s",value,name(value?read<void*>(value,profile::ObjectClass):nullptr).c_str());
    }
}
void setup_cheats() {
    void* pc=session.controller;
    void* p=property("/Script/Engine.PlayerController.CheatManager");
    void* klass=find("/Script/FortniteGame.FortCheatManager");
    if(!klass)klass=find("/Script/Engine.CheatManager");
    if(!p || !klass || read<int>(p,profile::PropertyElementSize)!=8)return;
    void* cheat=read<void*>(field(pc,p));
    if(!cheat) {
        cheat=fn<void*(*)(void*,void*)>(profile::SpawnObject)(klass,pc);
        if(cheat)write(field(pc,p),0,cheat);
    }
    LOG("CheatManager=%p class=%s",cheat,name(cheat?read<void*>(cheat,0x10):nullptr).c_str());
    if(cheat)invoke0(cheat,"/Script/Engine.CheatManager.ReceiveInitCheatManager");
}
void update_inventory() {
    invoke0(session.controller,"/Script/FortniteGame.FortPlayerController.ClientForceWorldInventoryUpdate");
    invoke0(session.inventory,"/Script/FortniteGame.FortInventory.HandleInventoryLocalUpdate");
    invoke0(session.controller,"/Script/FortniteGame.FortPlayerController.HandleWorldInventoryLocalUpdate");
    for(uint8_t bar:{uint8_t(0),uint8_t(1)}) {
        Call c("/Script/FortniteGame.FortPlayerController.ClientForceUpdateQuickbar");
        if(c.ok() && c.set("QuickbarToRefresh",bar))c.run(session.controller);
    }
}
void* item_definition(void* item) {
    Call c("/Script/FortniteGame.FortItem.GetItemDefinitionBP");
    return c.run(item)?c.result<void*>():nullptr;
}
Guid item_guid(void* item) {
    Call c("/Script/FortniteGame.FortItem.GetItemGuid");
    return c.run(item)?c.result<Guid>():Guid{};
}
bool bind_slot(void* item,int slot,uint8_t bar) {
    void* definition=item_definition(item);
    if(!definition)return false;
    void* quickbar=object_field(session.controller,"/Script/FortniteGame.FortPlayerController.ClientQuickBars");
    if(!quickbar || bar>1 || slot<0 || !fn<bool(*)(void*)>(profile::IsStandalone)(session.world))return false;
    Guid guid=item_guid(item);
    if(!(guid.a|guid.b|guid.c|guid.d))return false;
    Call bind("/Script/FortniteGame.FortKismetLibrary.EquipItemToQuickBarSlot");
    if(bind.ok() && bind.set("WorldContextObject",session.world) &&
       bind.set("ItemDefinition",definition) && bind.set("QuickBarType",bar) &&
       bind.set("SlotIndex",slot))bind.run(find("/Script/FortniteGame.Default__FortKismetLibrary"));
    // Native 0x4bc8a34 checks enabled slots and accepted item types before
    // insertion. Kismet returns true even when that routine rejects the item.
    // Read the actual slot; never treat a dispatched call as a successful bind.
    void* barprop=property(bar==0?"/Script/FortniteGame.FortQuickBars.PrimaryQuickBar":
        "/Script/FortniteGame.FortQuickBars.SecondaryQuickBar");
    void* slotsprop=property("/Script/FortniteGame.QuickBar.Slots");
    void* itemsprop=property("/Script/FortniteGame.QuickBarSlot.Items");
    if(!barprop || !slotsprop || !itemsprop)return false;
    void* inner=read<void*>(slotsprop,0x80);
    void* guidinner=read<void*>(itemsprop,0x80);
    // Independently corroborated by native slot indexing at 0x4bc9088.
    if(!inner || read<int>(inner,profile::PropertyElementSize)!=0x28 ||
       !guidinner || read<int>(guidinner,profile::PropertyElementSize)!=sizeof(Guid) ||
       read<int>(slotsprop,profile::PropertyElementSize)!=sizeof(Array) ||
       read<int>(itemsprop,profile::PropertyElementSize)!=sizeof(Array) ||
       read<int>(itemsprop,profile::PropertyOffset)!=0)return false;
    Array slots=read<Array>(field(field(quickbar,barprop),slotsprop));
    if(!slots.data || slots.count<1 || slots.count>64 || slots.capacity<slots.count || slot>=slots.count)return false;
    void* target=static_cast<char*>(slots.data)+size_t(slot)*0x28;
    void* array=field(target,itemsprop);
    Array values=read<Array>(array);
    if(values.count<0 || values.count>64 || values.capacity<values.count || (values.count && !values.data))return false;
    bool matched=false;
    for(int i=0;i<values.count;++i)if(!memcmp(static_cast<char*>(values.data)+size_t(i)*sizeof(Guid),&guid,sizeof(Guid)))matched=true;
    if(!matched && values.count==1) {
        // This binding is for a slot explicitly assigned by this standalone session.
        // A previous temporary-instance GUID must not survive reconciliation.
        write(values.data,0,guid);
        matched=true;
    }
    if(!matched && values.count==0) {
        int index=fn<int(*)(void*,void*,const void*)>(profile::GenericArrayAdd)(array,itemsprop,&guid);
        values=read<Array>(array);
        matched=index>=0 && index<values.count && values.data &&
            !memcmp(static_cast<char*>(values.data)+size_t(index)*sizeof(Guid),&guid,sizeof(Guid));
        if(matched)set_bool(target,"/Script/FortniteGame.QuickBarSlot.bEnabled",true);
        LOG("Standalone slot repair bar=%u slot=%d guid_verified=%d",bar,slot,matched);
    }
    LOG("Slot contents bar=%u slot=%d count=%d guid_verified=%d",bar,slot,values.count,matched);
    return matched;
}
bool initialize_quickbars() {
    void* quickbar=object_field(session.controller,"/Script/FortniteGame.FortPlayerController.ClientQuickBars");
    if(!isa(quickbar,find("/Script/FortniteGame.FortQuickBarsAthena"))) {
        LOG("Quickbar initialization requires FortQuickBarsAthena; actual=%s",name(quickbar?read<void*>(quickbar,profile::ObjectClass):nullptr).c_str());return false;
    }
    void* owner=object_field(quickbar,"/Script/Engine.Actor.Owner");
    if(owner!=session.controller) {LOG("Quickbar owner mismatch");return false;}
    // The Android routine is a virtual Athena quickbar initializer. It copies the
    // owning controller's definitions and allocates the three FQuickBar slot arrays.
    void* primary=static_cast<char*>(quickbar)+0x230;
    void* secondary=static_cast<char*>(quickbar)+0x2c0;
    Array p=read<Array>(primary,0x10),s=read<Array>(secondary,0x10);
    LOG("Quickbar class=%s slots=%d/%d",name(read<void*>(quickbar,profile::ObjectClass)).c_str(),p.count,s.count);
    if(p.count==0 && s.count==0) {
        const uint32_t expected[]={0xa9bc5ff8,0xa90157f6,0xa9024ff4,0xa9037bfd};
        if(memcmp(reinterpret_cast<void*>(base+profile::InitializeAthenaQuickBars),expected,sizeof(expected))) {
            LOG("Athena quickbar initializer prologue mismatch");return false;
        }
        void* vtable=read<void*>(quickbar);
        uintptr_t init_slot=read<uintptr_t>(vtable,0x668);
        if(init_slot!=base+profile::InitializeAthenaQuickBars && read<uintptr_t>(vtable,0x678)!=base+profile::InitializeAthenaQuickBars) {
            LOG("Athena controller initializer slot mismatch");return false;
        }
        if(init_slot!=base+profile::InitializeAthenaQuickBars)init_slot=read<uintptr_t>(vtable,0x678);
        reinterpret_cast<void(*)(void*)>(init_slot)(quickbar);
        p=read<Array>(primary,0x10);s=read<Array>(secondary,0x10);
        LOG("Athena quickbar initializer completed slots=%d/%d",p.count,s.count);
    }
    if(p.count<1 || s.count<1)return false;
    // Android GetItemInQuickbarSlot (0x5059fec) checks both ClientQuickBars
    // and the byte immediately after it. A manually spawned quickbar never
    // completes that controller readiness transition. Verify the consumer's
    // instructions and reflected pointer offset before enabling it locally.
    void* qp=property("/Script/FortniteGame.FortPlayerController.ClientQuickBars");
    const uint32_t gate[]={0x52827808,0x38686aa8};
    if(!qp || read<int>(qp,profile::PropertyOffset)!=0x13b8 ||
       memcmp(reinterpret_cast<void*>(base+0x505a010),gate,sizeof(gate)) ||
       !fn<bool(*)(void*)>(profile::IsStandalone)(session.world)) {
        LOG("Quickbar readiness layout mismatch; refusing write");return false;
    }
    uint8_t before=read<uint8_t>(session.controller,0x13c0);
    if(before>1) {LOG("Unexpected quickbar readiness byte=%u",before);return false;}
    write(session.controller,0x13c0,uint8_t(1));
    LOG("Quickbar controller readiness %u -> %u",before,read<uint8_t>(session.controller,0x13c0));
    return true;
}
void refresh_slots() {
    if(!initialize_quickbars()) {LOG("Quickbar slot initialization incomplete; skipping bindings");return;}
    update_inventory();
    for(uint8_t bar:{uint8_t(0),uint8_t(1)}) {
        auto& items=bar==0?session.slots:session.secondary_slots;
        for(int slot=0;slot<int(items.size());++slot)if(items[size_t(slot)])
            LOG("Quickbar %u slot %d guid_verified=%d",bar,slot,bind_slot(items[size_t(slot)],slot,bar));
    }
    update_inventory();
    // Read back after the final refresh, which can rebuild the client's cached inventory state.
    for(uint8_t bar:{uint8_t(0),uint8_t(1)}) {
        auto& items=bar==0?session.slots:session.secondary_slots;
        Call num("/Script/FortniteGame.FortPlayerController.GetNumQuickbarSlots");
        if(num.set("QuickBarType",bar) && num.run(session.controller))
            LOG("Quickbar %u slot count=%d",bar,num.result<int>());
        for(int slot=0;slot<int(items.size());++slot)if(items[size_t(slot)]) {
            Call query("/Script/FortniteGame.FortPlayerController.GetItemInQuickbarSlot");
            void* actual=nullptr;
            if(query.set("QuickBarType",bar) && query.set("SlotIndex",slot) && query.run(session.controller))
                actual=query.result<void*>();
            LOG("Quickbar %u slot %d matches=%d item=%s",bar,slot,
                actual==items[size_t(slot)],name(actual?item_definition(actual):nullptr).c_str());
        }
    }
    Call activate("/Script/FortniteGame.FortPlayerController.ClientActivateSlot");
    uint8_t primary=0;int slot=0;float delay=0;bool force=true;
    if(activate.set("InQuickBar",primary) && activate.set("Slot",slot) &&
       activate.set("ActivateDelay",delay) && activate.set("bUpdatePreviousFocusedSlot",force) &&
       activate.set("bForceExecution",force))activate.run(session.controller);
}
bool equip(void* item) {
    if(!item || !session.pawn)return false;
    void* definition=item_definition(item);Guid guid=item_guid(item);
    if(!definition || !(guid.a|guid.b|guid.c|guid.d))return false;
    Call c("/Script/FortniteGame.FortPawn.EquipWeaponDefinition");
    if(!c.ok() || !c.set("WeaponData",definition) || !c.set("ItemEntryGuid",guid))return false;
    if(!c.run(session.pawn))return false;
    void* weapon=c.result<void*>();
    LOG("Equip %s -> %p",name(definition).c_str(),weapon);return weapon!=nullptr;
}
bool inventory_layout(void*& list,void*& instance_property,void*& entry_property,void*& item_entry_property) {
    void* invprop=property("/Script/FortniteGame.FortInventory.Inventory");
    instance_property=property("/Script/FortniteGame.FortItemList.ItemInstances");
    entry_property=property("/Script/FortniteGame.FortItemList.ReplicatedEntries");
    item_entry_property=property("/Script/FortniteGame.FortWorldItem.ItemEntry");
    list=field(session.inventory,invprop);
    return list && instance_property && entry_property && item_entry_property &&
        read<int>(instance_property,profile::PropertyElementSize)==16 &&
        read<int>(entry_property,profile::PropertyElementSize)==16;
}
bool give(void* definition,int count,int slot,uint8_t bar=0,bool require_new=false) {
    if(!definition || !session.inventory || count<1 || count>9999 || slot< -1 || slot>5)return false;
    if(!isa(definition,find("/Script/FortniteGame.FortItemDefinition")))return false;
    void *list,*instances,*entries,*item_entry;
    if(!inventory_layout(list,instances,entries,item_entry))return false;
    void* inner=read<void*>(entries,0x80);
    if(!inner || read<int>(inner,profile::PropertyElementSize)!=read<int>(item_entry,profile::PropertyElementSize)) {
        LOG("FortItemEntry size mismatch; refusing inventory copy");return false;
    }
    void* instance_array=field(list,instances);void* entry_array=field(list,entries);
    Array current=read<Array>(instance_array),replicated=read<Array>(entry_array);
    if(current.count<0 || current.count>512 || current.capacity<current.count ||
       replicated.count<0 || replicated.count>512 || replicated.capacity<replicated.count)return false;
    // Do not duplicate starter definitions; callers can give a different item.
    for(int i=0;i<current.count;++i) {
        void* existing=read<void*>(current.data,size_t(i)*8);
        if(existing && item_definition(existing)==definition) {
            if(require_new)return false; // A duplicate pickup must not be consumed without transferring its count.
            if(slot>=0) (bar==0?session.slots:session.secondary_slots)[size_t(slot)]=existing;
            LOG("Already present: %s",name(definition).c_str());return true;
        }
    }
    Call create("/Script/FortniteGame.FortItemDefinition.CreateTemporaryItemInstanceBP");
    int level=1;
    if(!create.ok() || !create.set("Count",count) || !create.set("Level",level) || !create.run(definition))return false;
    void* item=create.result<void*>();if(!isa(item,find("/Script/FortniteGame.FortWorldItem")))return false;
    Call owner("/Script/FortniteGame.FortItem.SetOwningControllerForTemporaryItem");
    if(!owner.ok() || !owner.set("InController",session.controller) || !owner.run(item))return false;
    void* guidprop=property("/Script/FortniteGame.FortItemEntry.ItemGuid");
    void* entry=field(item,item_entry);
    const uint32_t guid_code[]={0xd10083ff,0xa9017bfd,0x910043fd,0x910003e0};
    if(!guidprop || read<int>(guidprop,profile::PropertyElementSize)!=sizeof(Guid) ||
       read<int>(guidprop,profile::PropertyOffset)!=0x50 ||
       memcmp(reinterpret_cast<void*>(base+profile::NewGuid),guid_code,sizeof(guid_code)))return false;
    // Inventory reconciliation assigns a GUID to a zero-GUID replicated copy
    // and drops instances whose GUID no longer exists in that list. Assign it
    // before deep-copying the entry, then resolve the surviving instance.
    Guid guid=fn<Guid(*)()>(profile::NewGuid)();
    if(!(guid.a|guid.b|guid.c|guid.d))return false;
    write(field(entry,guidprop),0,guid);
    Guid reported=item_guid(item);
    if(memcmp(&reported,&guid,sizeof(guid))) {
        LOG("Temporary item GUID accessor mismatch; insertion refused");return false;
    }
    auto add=fn<int(*)(void*,void*,const void*)>(profile::GenericArrayAdd);
    // GenericArray_Add performs reflected initialization and deep copy (embedded arrays included).
    int index=add(instance_array,instances,&item);
    if(index<0)return false;
    int entry_index=add(entry_array,entries,field(item,item_entry));
    if(entry_index<0) {LOG("Entry copy failed after instance insertion; stop granting items");return false;}
    update_inventory();
    current=read<Array>(instance_array);
    void* canonical=nullptr;
    if(current.count>=0 && current.count<=512 && current.capacity>=current.count && current.data)
        for(int i=0;i<current.count;++i) {
            void* candidate=read<void*>(current.data,size_t(i)*8);
            Guid id=item_guid(candidate);
            if(candidate && !memcmp(&id,&guid,sizeof(guid))) {canonical=candidate;break;}
        }
    if(!canonical || item_definition(canonical)!=definition) {
        LOG("Inventory reconciliation lost %s GUID=%08x:%08x:%08x:%08x",name(definition).c_str(),guid.a,guid.b,guid.c,guid.d);
        return false;
    }
    item=canonical;
    LOG("Inventory identity verified %s GUID=%08x:%08x:%08x:%08x",name(definition).c_str(),guid.a,guid.b,guid.c,guid.d);
    if(slot>=0) {
        (bar==0?session.slots:session.secondary_slots)[size_t(slot)]=item;
        bind_slot(item,slot,bar);
    }
    LOG("Inventory inserted %s count=%d slot=%d indices=%d/%d",name(definition).c_str(),count,slot,index,entry_index);
    return true;
}
bool actor_location(void* actor,Vector& value) {
    Call c("/Script/Engine.Actor.K2_GetActorLocation");
    if(!c.run(actor))return false;
    value=c.result<Vector>();return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}
bool near_player(Vector point,float limit) {
    Vector origin{};if(!actor_location(session.pawn,origin))return false;
    float x=point.x-origin.x,y=point.y-origin.y,z=point.z-origin.z;
    return std::isfinite(x+y+z) && x*x+y*y+z*z<=limit*limit;
}
bool local_pickup(void* pickup) {
    if(!isa(pickup,find("/Script/FortniteGame.FortPickup")))return false;
    Vector location{};if(!actor_location(pickup,location) || !near_player(location,600))return false;
    void* entry=field(pickup,property("/Script/FortniteGame.FortPickup.PrimaryPickupItemEntry"));
    void* definition=nullptr;int count=0;
    if(!get_field(entry,"/Script/FortniteGame.FortItemEntry.ItemDefinition",definition) ||
       !get_field(entry,"/Script/FortniteGame.FortItemEntry.Count",count))return false;
    if(!definition || count<1 || count>9999)return true;
    int maximum=0;
    if(get_field(definition,"/Script/FortniteGame.FortItemDefinition.MaxStackSize",maximum) && maximum>1 && maximum<=9999) {
        void *list,*instances,*entries,*item_entry;
        if(!inventory_layout(list,instances,entries,item_entry))return true;
        Array current=read<Array>(field(list,instances)),replicated=read<Array>(field(list,entries));
        void* count_prop=property("/Script/FortniteGame.FortItemEntry.Count");
        void* guid_prop=property("/Script/FortniteGame.FortItemEntry.ItemGuid");
        int stride=read<int>(item_entry,profile::PropertyElementSize);
        if(!count_prop || !guid_prop || current.count<0 || current.count>512 || replicated.count<0 || replicated.count>512 ||
           (current.count && !current.data) || (replicated.count && !replicated.data))return true;
        for(int i=0;i<current.count;++i) {
            void* item=read<void*>(current.data,size_t(i)*8);
            if(!item || item_definition(item)!=definition)continue;
            int have=read<int>(field(field(item,item_entry),count_prop));
            int amount=std::min(count,std::max(0,maximum-have));
            Guid id=item_guid(item);void* matching=nullptr;
            for(int j=0;j<replicated.count;++j) {
                void* candidate=static_cast<char*>(replicated.data)+size_t(j)*stride;
                Guid other=read<Guid>(field(candidate,guid_prop));
                if(!memcmp(&id,&other,sizeof(id))) {matching=candidate;break;}
            }
            if(!matching || amount<1) {LOG("Pickup retained: stack full or entry unavailable");return true;}
            int updated=have+amount;
            write(field(field(item,item_entry),count_prop),0,updated);write(field(matching,count_prop),0,updated);
            count-=amount;
            if(count) {
                write(field(entry,count_prop),0,count);
                invoke0(pickup,"/Script/FortniteGame.FortPickup.OnRep_PrimaryPickupItemEntry");
            } else invoke0(pickup,"/Script/Engine.Actor.K2_DestroyActor");
            update_inventory();LOG("Pickup stacked %s transferred=%d remainder=%d",name(definition).c_str(),amount,count);
            return true;
        }
    }
    int slot=-1;
    if(isa(definition,find("/Script/FortniteGame.FortWeaponItemDefinition"))) {
        for(int i=1;i<6;++i)if(!session.slots[size_t(i)]) {slot=i;break;}
        if(slot<0) {LOG("Pickup retained: primary quickbar full");return true;}
    }
    if(!give(definition,count,slot,0,true)) {
        LOG("Pickup retained: duplicate, full stack, or inventory insertion failed (%s)",name(definition).c_str());
        return true;
    }
    invoke0(pickup,"/Script/Engine.Actor.K2_DestroyActor");
    update_inventory();
    LOG("Local pickup transferred %s count=%d slot=%d",name(definition).c_str(),count,slot);
    return true;
}
struct LootDrop {void* definition;int count;};
float loot_random() {
    static uint32_t state=0;
    if(!state)state=uint32_t(monotonic_seconds()*1000000)|1;
    state^=state<<13;state^=state>>17;state^=state<<5;
    return float(state>>8)/16777216.0f;
}
bool roll_package(const char* package,int category,int depth,std::vector<LootDrop>& drops) {
    if(depth>16 || drops.size()>64)return false;
    float total=0;
    for(auto& row:local_loot::rows)if(!strcmp(row.package,package) && (category<0 || row.category==category))total+=row.weight;
    if(total<=0)return false;
    float pick=loot_random()*total;
    const local_loot::Row* selected=nullptr;
    for(auto& row:local_loot::rows)if(!strcmp(row.package,package) && (category<0 || row.category==category)) {
        selected=&row;pick-=row.weight;if(pick<0)break;
    }
    if(!selected || selected->count<1 || selected->count>9999)return false;
    if(*selected->call) {
        if(selected->count>32)return false;
        for(int i=0;i<selected->count;++i)if(!roll_package(selected->call,-1,depth+1,drops))return false;
    } else {
        void* definition=load_asset(selected->asset);
        if(!isa(definition,find("/Script/FortniteGame.FortWorldItemDefinition"))) {
            LOG("Loot asset unavailable on Android: %s",selected->asset);return false;
        }
        drops.push_back({definition,selected->count});
    }
    return true;
}
bool search_container(void* actor,Vector location) {
    void* searched=property("/Script/FortniteGame.BuildingContainer.bAlreadySearched");
    void* group_prop=property("/Script/FortniteGame.BuildingContainer.SearchLootTierGroup");
    if(!searched || !group_prop)return false;
    uint8_t byte=read<uint8_t>(searched,0x81),mask=read<uint8_t>(searched,0x82);
    if(byte>=8 || !mask)return false;
    if(read<uint8_t>(field(actor,searched),byte)&mask)return true;
    FString text{};
    fn<void(*)(const void*,FString*)>(profile::NameAppendString)(field(actor,group_prop),&text);
    std::string group=narrow(text.data,128);
    if(text.data)fn<void(*)(void*)>(profile::Free)(text.data);
    if(group=="Loot_Treasure")group="Loot_AthenaTreasure";
    if(group=="Loot_Ammo")group="Loot_AthenaAmmoLarge";
    float total=0;for(auto& tier:local_loot::tiers)if(group==tier.group)total+=tier.weight;
    if(total<=0) {LOG("Container loot group not ported: %s",group.c_str());return true;}
    float pick=loot_random()*total;const local_loot::Tier* selected=nullptr;
    for(auto& tier:local_loot::tiers)if(group==tier.group) {
        selected=&tier;pick-=tier.weight;if(pick<0)break;
    }
    if(!selected)return true;
    std::vector<LootDrop> drops;
    for(int category=0;category<11;++category)
        for(int i=0;i<selected->minimum[category];++i)
            if(!roll_package(selected->package,category,0,drops))return true;
    if(drops.empty())return true;
    void* entry_prop=property("/Script/FortniteGame.FortPickup.PrimaryPickupItemEntry");
    void* count_prop=property("/Script/FortniteGame.FortItemEntry.Count");
    void* definition_prop=property("/Script/FortniteGame.FortItemEntry.ItemDefinition");
    if(!entry_prop || !count_prop || !definition_prop || read<int>(count_prop,profile::PropertyElementSize)!=4 ||
       read<int>(definition_prop,profile::PropertyElementSize)!=8)return true;
    // Resolve every asset first; roll back a partial spawn so retries cannot duplicate loot.
    std::vector<void*> spawned;
    for(size_t i=0;i<drops.size();++i) {
        Transform t{};t.location[0]=location.x+60.0f+float(i%3)*35;
        t.location[1]=location.y+float(i/3)*35;t.location[2]=location.z+50;
        void* pickup=spawn(session.world,find("/Script/FortniteGame.FortPickupAthena"),nullptr,t);
        if(!pickup) {
            for(void* previous:spawned)invoke0(previous,"/Script/Engine.Actor.K2_DestroyActor");
            LOG("Container pickup spawn failed; search not consumed");return true;
        }
        spawned.push_back(pickup);
        void* entry=field(pickup,entry_prop);
        write(field(entry,count_prop),0,drops[i].count);write(field(entry,definition_prop),0,drops[i].definition);
        invoke0(pickup,"/Script/FortniteGame.FortPickup.OnRep_PrimaryPickupItemEntry");
    }
    set_bool(actor,"/Script/FortniteGame.BuildingContainer.bAlreadySearched",true);
    invoke0(actor,"/Script/FortniteGame.BuildingContainer.OnRep_bAlreadySearched");
    LOG("Container search %s spawned=%zu (exported 10.40 loot)",group.c_str(),spawned.size());
    return true;
}
bool local_interact(void* function,void* params) {
    void* arg=incoming(function,params,"ReceivingActor",8);
    void* actor=arg?read<void*>(arg):nullptr;
    Vector location{};
    if(!isa(actor,find("/Script/Engine.Actor")) || !actor_location(actor,location) || !near_player(location,600))return false;
    LOG("Interaction target=%s class=%s",name(actor).c_str(),name(read<void*>(actor,profile::ObjectClass)).c_str());
    if(isa(actor,find("/Script/FortniteGame.BuildingContainer")))return search_container(actor,location);
    if(isa(actor,find("/Script/FortniteGame.BuildingActor"))) {
        Call interact("/Script/FortniteGame.BuildingActor.BlueprintOnInteract",actor);
        return interact.set("InteractingPawn",session.pawn) && interact.run(actor);
    }
    return false;
}
bool local_build(void* function,void* params) {
    // Era GameManager::Build uses the local preview, not server RPC payloads.
    Vector location{},rotation{};bool mirrored=false;
    void* klass=object_field(session.controller,"/Script/FortniteGame.FortPlayerController.CurrentBuildableClass");
    if(!get_field(session.controller,"/Script/FortniteGame.FortPlayerController.LastBuildPreviewGridSnapLoc",location) ||
       !get_field(session.controller,"/Script/FortniteGame.FortPlayerController.LastBuildPreviewGridSnapRot",rotation)) {
        LOG("Era build: local preview properties unavailable");return true;
    }
    if(!subclass(klass,find("/Script/FortniteGame.BuildingSMActor")) || !near_player(location,2000) ||
       !std::isfinite(rotation.x) || !std::isfinite(rotation.y) || !std::isfinite(rotation.z)) {
        LOG("Era build: invalid selected class or preview transform");return true;
    }
    // Mirroring is supplemental; lack of an RPC payload does not prevent placement.
    if(void* data=incoming(function,params,"CreateBuildingData",56)) {
        void* p=property("/Script/FortniteGame.CreateBuildingActorData.bMirrored");
        if(p && read<int>(p,profile::PropertyOffset)>=0 && read<int>(p,profile::PropertyOffset)<56)
            get_field(data,"/Script/FortniteGame.CreateBuildingActorData.bMirrored",mirrored);
    }
    std::array<float,6> pose{location.x,location.y,location.z,rotation.x,rotation.y,rotation.z};
    double now=monotonic_seconds();
    if(klass==session.last_build_class && pose==session.last_build_transform && now-session.last_build_time<0.25)return true;
    LOG("Era build preview %s at %.0f %.0f %.0f",name(klass).c_str(),location.x,location.y,location.z);
    Transform t{};t.location[0]=location.x;t.location[1]=location.y;t.location[2]=location.z;
    // Unreal FRotator (pitch/yaw/roll, degrees) -> FQuat, preserving Android's scalar transform layout.
    const float h=0.00872664626f;
    float sp=std::sin(rotation.x*h),cp=std::cos(rotation.x*h),sy=std::sin(rotation.y*h),cy=std::cos(rotation.y*h);
    float sr=std::sin(rotation.z*h),cr=std::cos(rotation.z*h);
    t.rotation[0]=cr*sp*sy-sr*cp*cy;t.rotation[1]=-cr*sp*cy-sr*cp*sy;
    t.rotation[2]=cr*cp*sy-sr*sp*cy;t.rotation[3]=cr*cp*cy+sr*sp*sy;
    void* actor=spawn(session.world,klass,session.controller,t);
    if(!actor) {LOG("Era build: spawn returned null");return true;}
    session.last_build_class=klass;session.last_build_transform=pose;session.last_build_time=now;
    set_bool(actor,"/Script/FortniteGame.BuildingActor.bPlayerPlaced",true);
    Call mirror("/Script/FortniteGame.BuildingSMActor.SetMirrored");
    if(mirror.set("bIsMirrored",mirrored))mirror.run(actor);
    Call init("/Script/FortniteGame.BuildingActor.InitializeKismetSpawnedBuildingActor");bool animations=true;
    if(init.set("BuildingOwner",actor) && init.set("SpawningController",session.controller) &&
       init.set("bUsePlayerBuildAnimations",animations) && init.run(actor))LOG("Local build initialized %s",name(klass).c_str());
    else invoke0(actor,"/Script/Engine.Actor.K2_DestroyActor");
    return true;
}
bool setup_inventory() {
    void* type=find("/Script/FortniteGame.FortPlayerController");
    if(!isa(session.controller,type)) {LOG("Inventory requires FortPlayerController; actual=%s",name(read<void*>(session.controller,0x10)).c_str());return false;}
    void* p=property("/Script/FortniteGame.FortPlayerController.WorldInventory");
    if(!p || read<int>(p,profile::PropertyElementSize)!=8)return false;
    session.inventory=read<void*>(field(session.controller,p));
    if(!session.inventory) {
        Transform t{};
        session.inventory=spawn(session.world,find("/Script/FortniteGame.FortInventory"),session.controller,t);
        if(session.inventory)write(field(session.controller,p),0,session.inventory);
    }
    if(!session.inventory)return false;
    void* quickbar_property=property("/Script/FortniteGame.FortPlayerController.ClientQuickBars");
    if(quickbar_property && read<int>(quickbar_property,profile::PropertyElementSize)==8 &&
       !read<void*>(field(session.controller,quickbar_property))) {
        // UObjectPropertyBase::PropertyClass follows UProperty at +0x80 on this build.
        void* declared_class=read<void*>(quickbar_property,0x80);
        void* expected_class=find("/Script/FortniteGame.FortQuickBars");
        void* klass=find("/Script/FortniteGame.FortQuickBarsAthena");
        if(declared_class==expected_class && subclass(klass,expected_class)) {
            Transform transform{};
            void* quickbar=subclass(klass,find("/Script/Engine.Actor"))?
                spawn(session.world,klass,session.controller,transform):
                fn<void*(*)(void*,void*)>(profile::SpawnObject)(klass,session.controller);
            if(quickbar)write(field(session.controller,quickbar_property),0,quickbar);
            LOG("ClientQuickBars created=%p class=%s",quickbar,name(klass).c_str());
        } else LOG("ClientQuickBars class requires further binding; no guessed object created");
    }
    initialize_quickbars();
    void* pickaxe=locker_pickaxe();
    if(pickaxe)give(pickaxe,1,0);
    else LOG("Locker pickaxe unavailable; no hardcoded substitute equipped");
    const char* rifle="WID_Assault_Auto_Athena_C_Ore_T03";
    if(void* definition=by_short_name(rifle))give(definition,1,1);
    for(const char* resource:{"WoodItemData","StoneItemData","MetalItemData","AthenaAmmoDataBulletsMedium","AthenaAmmoDataShells"}) {
        if(void* item=by_short_name(resource))give(item,999,-1);
    }
    const char* tools[]={"BuildingItemData_Wall","BuildingItemData_Floor","BuildingItemData_Stair_W","BuildingItemData_RoofS","EditTool"};
    for(int i=0;i<5;++i) {
        std::string path=std::string("/Game/Items/Weapons/BuildingTools/")+tools[i]+"."+tools[i];
        void* item=load_asset(path);
        if(item)give(item,1,i,1);
    }
    equip(session.slots[0]);
    return session.slots[0] || session.slots[1];
}

// ============================================================================
// 🔧 FIX 3: initialize_player — SetupInputComponent + ClientSetHUD
// ============================================================================
void initialize_player(void* world) {
    if(!fn<bool(*)(void*)>(profile::IsStandalone)(world)) {
        LOG("Setup stopped: Athena is not standalone");session.attempted=true;return;
    }
    void* controller=fn<void*(*)(void*,int)>(profile::GetPlayerController)(world,0);
    if(!controller)return;
    void* mode=object_field(world,"/Script/Engine.World.AuthorityGameMode");
    void* state=object_field(world,"/Script/Engine.World.GameState");
    if(!isa(controller,find("/Script/FortniteGame.FortPlayerControllerAthena")) ||
       !isa(mode,find("/Script/FortniteGame.FortGameModeAthena")) ||
       !isa(state,find("/Script/FortniteGame.FortGameStateAthena"))) {
        if(!session.wait_reported)LOG("Waiting for Athena types PC=%s GM=%s GS=%s; use open Athena_Terrain without Game=Engine.GameMode",
            name(controller?read<void*>(controller,0x10):nullptr).c_str(),
            name(mode?read<void*>(mode,0x10):nullptr).c_str(),name(state?read<void*>(state,0x10):nullptr).c_str());
        session.wait_reported=true;return;
    }
    session.controller=controller;
    if(!readiness_layout(controller)) {session.attempted=true;return;}
    LOG("Original Athena types PC=%s GM=%s GS=%s",name(read<void*>(controller,0x10)).c_str(),
        name(read<void*>(mode,0x10)).c_str(),name(read<void*>(state,0x10)).c_str());
    void* klass=load_class("/Game/Athena/PlayerPawn_Athena.PlayerPawn_Athena_C","/Script/Engine.Pawn");
    if(!klass || !subclass(klass,find("/Script/FortniteGame.FortPlayerPawnAthena"))) {
        LOG("PlayerPawn class missing or incompatible");session.attempted=true;return;
    }
    void* old=fn<void*(*)(void*)>(profile::GetPawn)(controller);
    session.pawn=isa(old,klass)?old:nullptr;
    if(!session.pawn) {
        Transform transform{};
        if(!start_transform(mode,controller,transform)) {LOG("Waiting for a valid PlayerStart");return;}
        session.attempted=true;
        LOG("Spawning PlayerPawn_Athena at PlayerStart");
        session.pawn=spawn(world,klass,controller,transform);
        if(!session.pawn) {LOG("Pawn spawn failed");return;}
        LOG("Possessing pawn");
        fn<void(*)(void*,void*)>(profile::Possess)(controller,session.pawn);
    }
    session.attempted=true;
    if(fn<void*(*)(void*)>(profile::GetPawn)(controller)!=session.pawn) {
        LOG("Possession did not stick");return;
    }

    // 🔧 FIX 3: Input component + HUD setup BEFORE ClientRestart
    Call setupInput("/Script/Engine.PlayerController.SetupInputComponent");
    if(setupInput.ok()) {
        setupInput.run(controller);
        LOG("Input component setup");
    }
    Call inputMode("/Script/Engine.PlayerController.SetInputMode");
    uint8_t gameOnly=0;
    if(inputMode.ok() && inputMode.set("InMode",gameOnly))inputMode.run(controller);
    void* hudClass=find("/Game/Athena/HUD/AthenaHUD.AthenaHUD_C");
    if(!hudClass)hudClass=find("/Script/FortniteGame.FortHUD");
    if(hudClass) {
        Call csh("/Script/Engine.PlayerController.ClientSetHUD");
        if(csh.ok() && csh.set("NewHUDClass",hudClass)) {
            csh.run(controller);
            LOG("ClientSetHUD: %s",name(hudClass).c_str());
        }
    }

    Call restart("/Script/Engine.PlayerController.ClientRestart");
    if(restart.ok() && restart.set("NewPawn",session.pawn))restart.run(controller);
    setup_cheats();
    for(const char* method:{"/Script/FortniteGame.FortPawn.SetMaxHealth","/Script/FortniteGame.FortPawn.SetHealth"}) {
        Call health(method);float value=100.0f;
        if(health.ok() && health.set("NewHealthVal",value))health.run(session.pawn);
    }
    sync_locker_loadout();
    LOG("Setting up inventory");
    session.inventory_ready=setup_inventory();
    finish_loading(state);
    startup_commands();
    session.finalize_at=monotonic_seconds()+2.0;
    LOG("Build25 initial setup finished: pawn=%p inventory=%p items=%d; device test required",session.pawn,session.inventory,session.inventory_ready);
}

// ============================================================================
// 🔧 FIX 4: finalize_player — re-equip pickaxe + MaxWalkSpeed = 600
// ============================================================================
void finalize_player() {
    session.finalized=true;
    void* pc=fn<void*(*)(void*,int)>(profile::GetPlayerController)(session.world,0);
    if(pc!=session.controller) {LOG("Finalization stopped: local controller changed");return;}
    void* pawn=fn<void*(*)(void*)>(profile::GetPawn)(pc);
    LOG("Post-readiness pawn=%p expected=%p class=%s",pawn,session.pawn,
        name(pawn?read<void*>(pawn,profile::ObjectClass):nullptr).c_str());
    // Readiness can change possession. Do not operate on a stale pre-readiness pawn.
    if(!isa(pawn,find("/Script/FortniteGame.FortPlayerPawnAthena"))) {
        LOG("Finalization stopped: active pawn is not FortPlayerPawnAthena");return;
    }
    session.pawn=pawn;
    Call actor_scale("/Script/Engine.Actor.GetActorScale3D");
    if(actor_scale.run(pawn)) {
        auto value=actor_scale.result<Vector>();
        LOG("Pawn world scale=%.3f/%.3f/%.3f",value.x,value.y,value.z);
    }
    invoke0(pawn,"/Script/Engine.Pawn.OnRep_PlayerState");
    setup_cosmetics();
    grant_athena_abilities();

    // 🔧 FIX 4a: Re-equip pickaxe after ability grant
    if(session.slots[0]) {
        equip(session.slots[0]);
        LOG("Finalize: re-equipped pickaxe after ability grant");
    }

    Call restart("/Script/Engine.PlayerController.ClientRestart");
    if(restart.ok() && restart.set("NewPawn",pawn))restart.run(pc);
    Call camera("/Script/Engine.PlayerController.SetViewTargetWithBlend");
    if(camera.ok() && camera.set("NewViewTarget",pawn))camera.run(pc);
    void* movement=object_field(pawn,"/Script/Engine.Character.CharacterMovement");
    if(isa(movement,find("/Script/Engine.CharacterMovementComponent"))) {
        invoke0(movement,"/Script/Engine.MovementComponent.StopMovementImmediately");
        for(const char* key:{"GroundFriction","BrakingDecelerationWalking","MaxStepHeight","MaxWalkSpeed","MaxAcceleration"}) {
            std::string path=std::string("/Script/Engine.CharacterMovementComponent.")+key;
            void* prop=property(path);
            if(!prop || read<int>(prop,profile::PropertyElementSize)!=4)continue;
            float value=read<float>(field(movement,prop));
            LOG("Movement %s=%.2f",key,value);
        }
    }
    // 🔧 FIX 4b: Force MaxWalkSpeed = 600
    if(movement) {
        void* p=property("/Script/Engine.CharacterMovementComponent.MaxWalkSpeed");
        if(p) {
            float* val=reinterpret_cast<float*>(field(movement,p));
            if(val) { *val=600.0f; LOG("Finalize: MaxWalkSpeed = 600"); }
        }
    }
    Call target("/Script/Engine.Controller.GetViewTarget");
    if(target.run(pc))LOG("Camera target=%p pawn=%p",target.result<void*>(),pawn);
    Call components("/Script/Engine.Actor.GetComponentsByClass");
    void* mesh_class=find("/Script/Engine.SkeletalMeshComponent");
    if(components.ok() && components.set("ComponentClass",mesh_class) && components.run(pawn)) {
        auto meshes=components.result<Array>();
        LOG("Skeletal mesh components=%d",meshes.count);
        if(meshes.data && meshes.count>=0 && meshes.count<=64) {
            for(int i=0;i<meshes.count;++i) {
                void* component=read<void*>(meshes.data,size_t(i)*8);
                if(!isa(component,mesh_class))continue;
                void* asset=object_field(component,"/Script/Engine.SkinnedMeshComponent.SkeletalMesh");
                Call scale("/Script/Engine.SceneComponent.K2_GetComponentScale");
                Vector value{};if(scale.run(component))value=scale.result<Vector>();
                LOG("Mesh %s asset=%s scale=%.2f/%.2f/%.2f",name(component).c_str(),name(asset).c_str(),value.x,value.y,value.z);
            }
        }
        if(meshes.data)fn<void(*)(void*)>(profile::Free)(meshes.data);
    }
    refresh_slots();
    equip(session.slots[0]?session.slots[0]:session.slots[1]);
    for(const char* path:{"/Script/FortniteGame.FortPlayerController.IsActionInputIgnored",
                          "/Script/FortniteGame.FortPawn.IsActionInputIgnored"}) {
        Call ignored(path);void* receiver=strstr(path,"Controller")?pc:pawn;
        if(ignored.run(receiver))LOG("%s=%d",path,ignored.result<bool>());
    }
    // This single-player port currently uses free building, like its unlimited starter materials.
    set_bool(pc,"/Script/FortniteGame.FortPlayerController.bBuildFree",true);
    LOG("Build25 inventory/interaction handlers ready; device validation required");
}

void poll_touch_build(double now) {
    if(!session.finalized || now<session.next_build_poll)return;
    session.next_build_poll=now+0.05;
    bool held=false;
    if(!get_field(session.controller,"/Script/FortniteGame.FortPlayerController.bHoldingPrimaryFireFromTouch",held))return;
    if(!held) {session.touch_build_held=false;session.held_build_class=nullptr;return;}
    Call mode("/Script/FortniteGame.FortPlayerController.IsInBuildMode");
    if(!mode.run(session.controller) || !mode.result<bool>())return;
    Vector loc{},rot{};
    if(!get_field(session.controller,"/Script/FortniteGame.FortPlayerController.LastBuildPreviewGridSnapLoc",loc) ||
       !get_field(session.controller,"/Script/FortniteGame.FortPlayerController.LastBuildPreviewGridSnapRot",rot))return;
    void* klass=object_field(session.controller,"/Script/FortniteGame.FortPlayerController.CurrentBuildableClass");
    std::array<float,6> pose{loc.x,loc.y,loc.z,rot.x,rot.y,rot.z};
    if(session.touch_build_held && session.held_build_class==klass && session.held_build_transform==pose)return;
    session.touch_build_held=true;session.held_build_class=klass;session.held_build_transform=pose;
    LOG("Touch placement confirm observed");local_build(nullptr,nullptr);
}
void refresh_calendar(double now) {
    if(!session.finalized || now<session.next_calendar || session.event_started || session.event_pending)return;
    bool first=session.next_calendar==0;session.next_calendar=now+60;
    void* gs=object_field(session.world,"/Script/Engine.World.GameState");
    if(first) {
        void* info=property("/Script/FortniteGame.FortGameStateAthena.CurrentPlaylistInfo");
        void* baseprop=property("/Script/FortniteGame.PlaylistPropertyArray.BasePlaylist");
        if(info && baseprop && read<int>(baseprop,profile::PropertyElementSize)==8 &&
           read<int>(baseprop,profile::PropertyOffset)+8<=read<int>(info,profile::PropertyElementSize)) {
            void* target=field(field(gs,info),baseprop);
            if(target && !read<void*>(target)) {
                void* playlist=load_asset("/Game/Athena/Playlists/Playlist_DefaultSolo.Playlist_DefaultSolo","/Script/FortniteGame.FortPlaylistAthena");
                if(playlist) {write(target,0,playlist);invoke0(gs,"/Script/FortniteGame.FortGameStateAthena.OnRep_CurrentPlaylistInfo");}
            }
        }
    }
    // Use Fortnite's existing online calendar: its async parser handles the
    // configured backend's client-events and activeSince/activeUntil windows.
    // This also refreshes registered world calendar listeners.
    Call refresh("/Script/FortniteGame.FortGameState.Client_RefreshEventCalendar");
    LOG("Backend calendar refresh dispatched=%d",refresh.run(gs));
    for(const char* flag:{"EventFlag.Season10","RBFI"}) {
        auto text=wide(flag);FString input{text.data(),int(text.size()+1),int(text.size()+1)};
        Call check("/Script/FortniteGame.FortKismetLibrary.GetCalendarEventInformation");
        if(check.set("WorldContextObject",session.world) && check.set("EventName",input) &&
           check.run(find("/Script/FortniteGame.Default__FortKismetLibrary")))
            LOG("Calendar cached flag %s active=%d",flag,check.get<bool>("bIsEventActive"));
    }
}
void return_to_lobby(void* world) {
    // Travel after WorldTick, never from inside a controller return-menu RPC.
    void* controller=session.controller;
    departing_world=world;departing_index=read<int>(world,0xc);
    session=Session{};
    auto text=wide("open Frontend");FString command{text.data(),int(text.size()+1),int(text.size()+1)};
    Call travel("/Script/Engine.KismetSystemLibrary.ExecuteConsoleCommand");
    bool dispatched=travel.set("WorldContextObject",world) && travel.set("Command",command) &&
        travel.set("SpecificPlayer",controller) && travel.run(find("/Script/Engine.Default__KismetSystemLibrary"));
    LOG("Standalone lobby travel dispatched=%d; old session cleared",dispatched);
    if(!dispatched) {departing_world=nullptr;departing_index=-1;}
}
const char* night_script="/Game/Athena/Prototype/Blueprints/NightNight/BP_NightNight_Scripting.BP_NightNight_Scripting_C";
void request_end_event() {
    if(!session.finalized || !session.world || !fn<bool(*)(void*)>(profile::IsStandalone)(session.world)) {
        LOG("startevent requires a ready standalone Athena world");return;
    }
    if(session.event_pending || session.event_started) {LOG("The End is already loading or started");return;}
    Guard guard;
    auto map=wide("/Game/Athena/Maps/Test/S10/NightNightSequenceMap");
    FString map_string{map.data(),int(map.size()+1),int(map.size()+1)};
    Call convert("/Script/Engine.KismetStringLibrary.Conv_StringToName");
    if(convert.set("InString",map_string) && convert.run(find("/Script/Engine.Default__KismetStringLibrary"))) {
        auto map_name=convert.result<uint64_t>();
        Call existing("/Script/Engine.GameplayStatics.GetStreamingLevel");
        if(existing.set("WorldContextObject",session.world) && existing.set("PackageName",map_name) &&
           existing.run(find("/Script/Engine.Default__GameplayStatics")))session.event_stream=existing.result<void*>();
    }
    if(!session.event_stream) {
        Call stream("/Script/Engine.LevelStreamingDynamic.LoadLevelInstance");Vector zero{};
        bool args=stream.ok() && stream.set("WorldContextObject",session.world) && stream.set("LevelName",map_string) &&
           stream.set("Location",zero) && stream.set("Rotation",zero);
        bool dispatched=args && stream.run(find("/Script/Engine.Default__LevelStreamingDynamic"));
        bool accepted=dispatched && stream.get<bool>("bOutSuccess");
        session.event_stream=accepted?stream.result<void*>():nullptr;
        LOG("NightNight full-path stream args=%d dispatched=%d accepted=%d object=%p",args,dispatched,accepted,session.event_stream);
        if(!session.event_stream) {
            // The event's own loader knows the cooked package reference on this build.
            void* klass=load_class(night_script);void* script=nullptr;
            Call actors("/Script/Engine.GameplayStatics.GetAllActorsOfClass");
            if(klass && actors.set("WorldContextObject",session.world) && actors.set("ActorClass",klass) &&
               actors.run(find("/Script/Engine.Default__GameplayStatics"))) {
                Array list=actors.get<Array>("OutActors");
                if(list.count>0 && list.count<4096 && list.data)script=read<void*>(list.data);
                if(list.data)fn<void(*)(void*)>(profile::Free)(list.data);
            }
            std::string path=std::string(night_script)+".LoadNightNightLevel";
            Call loader(path.c_str(),script);bool condition=true;
            if(!script || !loader.set("Condition",condition) || !loader.run(script)) {
                LOG("NightNight package and Blueprint loader unavailable; Athena retained");return;
            }
            session.event_pending=true;session.event_deadline=monotonic_seconds()+120;session.event_poll=0;
            LOG("NightNight Blueprint loader dispatched; waiting for streaming entry");return;
        }
    }
    bool yes=true;
    Call loaded("/Script/Engine.LevelStreaming.SetShouldBeLoaded");
    Call visible("/Script/Engine.LevelStreaming.SetShouldBeVisible");
    if(!loaded.set("bInShouldBeLoaded",yes) || !loaded.run(session.event_stream) ||
       !visible.set("bInShouldBeVisible",yes) || !visible.run(session.event_stream)) {
        LOG("NightNight stream visibility setup failed");return;
    }
    session.event_pending=true;session.event_deadline=monotonic_seconds()+120;session.event_poll=0;
    LOG("Streaming NightNightSequenceMap into Athena; waiting for loaded and visible");
}
void poll_end_event(double now) {
    if(!session.event_pending || now<session.event_poll)return;
    session.event_poll=now+1;
    if(now>session.event_deadline) {session.event_pending=false;LOG("NightNight streaming timed out; startevent can retry");return;}
    if(!session.event_stream) {
        auto map=wide("NightNightSequenceMap");FString value{map.data(),int(map.size()+1),int(map.size()+1)};
        Call convert("/Script/Engine.KismetStringLibrary.Conv_StringToName");
        if(convert.set("InString",value) && convert.run(find("/Script/Engine.Default__KismetStringLibrary"))) {
            auto id=convert.result<uint64_t>();Call query("/Script/Engine.GameplayStatics.GetStreamingLevel");
            if(query.set("WorldContextObject",session.world) && query.set("PackageName",id) &&
               query.run(find("/Script/Engine.Default__GameplayStatics")))session.event_stream=query.result<void*>();
        }
        if(!session.event_stream)return;
    }
    Call loaded("/Script/Engine.LevelStreaming.IsLevelLoaded"),visible("/Script/Engine.LevelStreaming.IsLevelVisible");
    if(!loaded.run(session.event_stream) || !loaded.result<bool>() ||
       !visible.run(session.event_stream) || !visible.result<bool>())return;
    session.event_pending=false;
    void* klass=load_class(night_script);void* script=nullptr;
    if(!klass) {LOG("NightNight scripting class missing in Android content");return;}
    Call actors("/Script/Engine.GameplayStatics.GetAllActorsOfClass");
    if(actors.set("WorldContextObject",session.world) && actors.set("ActorClass",klass) &&
       actors.run(find("/Script/Engine.Default__GameplayStatics"))) {
        Array list=actors.get<Array>("OutActors");
        if(list.count>0 && list.count<4096 && list.data)script=read<void*>(list.data);
        if(list.data)fn<void(*)(void*)>(profile::Free)(list.data);
    }
    if(!script) {Transform t{};t.location[2]=0;script=spawn(session.world,klass,nullptr,t);}
    if(!script) {LOG("NightNight script spawn failed");return;}
    void* gs=object_field(session.world,"/Script/Engine.World.GameState");
    void* playlist=load_asset("/Game/Athena/Playlists/Music/Playlist_Music_High.Playlist_Music_High","/Script/FortniteGame.FortPlaylist");
    std::array<uint8_t,32> tags{};
    if(!gs || !playlist || !get_field(playlist,"/Script/FortniteGame.FortPlaylist.GameplayTagContainer",tags)) {
        LOG("NightNight readiness requires GameState and event playlist tags");return;
    }
    std::string ready_path=std::string(night_script)+".OnReady_D0847F7B4E80F01E77156AA4E7131AF6";
    Call ready(ready_path.c_str(),script);
    if(!ready.set("GameState",gs) || !ready.set("Playlist",playlist) ||
       !ready.set("PlaylistContextTags",tags) || !ready.run(script)) {LOG("NightNight OnReady failed");return;}
    std::string start_path=std::string(night_script)+".StartEvent";
    Call start(start_path.c_str(),script);
    session.event_started=start.run(script);
    LOG("NightNight map loaded and visible; StartEvent dispatched=%d world=%p",session.event_started,session.world);
}
void tick_hook(void* world,int type,float delta) {
    original_tick(world,type,delta);
    if(internal || !world)return;
    int expected=0;
    if(game_tid.compare_exchange_strong(expected,int(gettid())))
        LOG("WorldTick first callback: tid=%d world=%p",int(gettid()),world);
    if(game_tid.load()!=gettid())return;
    Guard guard;
    if(read<uint8_t>(reinterpret_cast<void*>(base+profile::IsSavingPackage)) ||
        fn<bool(*)()>(profile::IsGarbageCollecting)())return;
    if(session.lobby_requested && session.world==world) {return_to_lobby(world);return;}
    if(world==departing_world && read<int>(world,0xc)==departing_index)return;
    int index=read<int>(world,0xc);
    if(world!=session.world || index!=session.world_index) {
        auto world_name=name(world);
        if(world_name.find("Athena_Terrain")==std::string::npos) {
            if(session.world && !session.awaiting_travel && fn<void*(*)(void*,int)>(profile::GetPlayerController)(world,0)) {
                restore_defaults();session=Session{};
            }
            return;
        }
        bool requested=session.requested;
        session=Session{};session.requested=requested;
        session.world=world;session.world_index=index;
        session.observed_at=monotonic_seconds();
        LOG("Athena world observed %s requested=%d",world_name.c_str(),requested);
    }
    double now=monotonic_seconds();
    void* phase_state=object_field(world,"/Script/Engine.World.GameState");
    uint8_t phase_value=0;
    if(phase_state && get_field(phase_state,"/Script/FortniteGame.FortGameStateAthena.GamePhase",phase_value)) {
        if(session.last_phase!=int(phase_value)) {
            LOG("GamePhase observed=%u (%s) world=%p elapsed=%.1f",phase_value,
                athena_phase_name(phase_value),world,now-session.observed_at);
            session.last_phase=int(phase_value);
        }
    }
    int ui=ui_command.exchange(0);
    if(ui==2) {
        session.lobby_requested=true;
        LOG("UI command queued: Return to Lobby");
    } else if(ui==3) {
        request_end_event();
    } else if(ui==4 && session.controller) {
        // Execute the same travel command as the in-game console on the game thread.
        void* controller=session.controller;
        auto text=wide("open Athena_Terrain");
        FString command{text.data(),int(text.size()+1),int(text.size()+1)};
        Call travel("/Script/Engine.KismetSystemLibrary.ExecuteConsoleCommand");
        bool dispatched=travel.set("WorldContextObject",world) && travel.set("Command",command) &&
            travel.set("SpecificPlayer",controller) && travel.run(find("/Script/Engine.Default__KismetSystemLibrary"));
        if(dispatched) {
            session=Session{};session.requested=true;
            LOG("UI command dispatched: open Athena_Terrain");
        } else LOG("UI command failed: open Athena_Terrain");
    } else if(ui==1 && session.controller) {
        Call teleport("/Script/FortniteGame.FortPlayerController.ServerTeleportToReticle");
        Vector target{};
        bool have_target=get_field(session.controller,"/Script/FortniteGame.FortPlayerController.LocationUnderReticle",target);
        if(have_target && teleport.set("TeleportLocation",target) && teleport.run(session.controller))
            LOG("Teleport-to-reticle dispatched to %.0f %.0f %.0f",target.x,target.y,target.z);
        else LOG("Teleport-to-reticle unavailable or no reticle location on this build");
    }
    poll_end_event(now);
    poll_touch_build(now);
    refresh_calendar(now);
    if(session.attempted) {
        if(session.finalize_at>0 && !session.finalized && now>=session.finalize_at)finalize_player();
        return;
    }
    if(now-session.observed_at<3.0)return;
    if(now-session.observed_at>45.0) {LOG("Athena setup timed out waiting for prerequisites");session.attempted=true;return;}
    // Retry missing world/controller/start prerequisites at most once per second.
    if(now-session.travel_at<1.0)return;
    session.travel_at=now;session.requested=true;
    initialize_player(world);
}

// ============================================================================
// 🔧 FIX 5: pe_hook — full with JumpFromHudStart
// ============================================================================
void pe_hook(void* object,void* function,void* params) {
    if(!internal && session.inventory_ready && session.world &&
       game_tid.load()==gettid() && function) {
        std::string early_method=name(function);
        if(object==session.controller && (early_method=="ReturnToMainMenu" || early_method=="ServerReturnToMainMenu" ||
           early_method=="ClientReturnToMainMenu" || early_method=="ClientReturnToMainMenuWithTextReason")) {
            session.lobby_requested=true;LOG("Lobby return queued: %s",early_method.c_str());return;
        }
        // 🔧 FIX 5: Mobile's battle-bus HUD raises JumpFromHudStart instead of
        // reliably delivering ServerAttemptAircraftJump. Teleport to 50000m and
        // trigger native BeginSkydiving.
        if(session.controller && (early_method=="JumpFromHudStart" ||
           (early_method=="ServerAttemptAircraftJump" && !params))) {
            LOG("Aircraft jump via %s",early_method.c_str());

            Vector spawnLoc{0.0f,0.0f,50000.0f};
            Call choose("/Script/Engine.GameModeBase.ChoosePlayerStart");
            if(choose.ok() && choose.set("Player",session.controller) &&
               choose.run(object_field(session.world,"/Script/Engine.World.AuthorityGameMode"))) {
                void* start=choose.result<void*>();
                if(start && !isa(start,nullptr)) {
                    Call sloc("/Script/Engine.Actor.K2_GetActorLocation");
                    if(sloc.ok() && sloc.run(start)) {
                        Vector v=sloc.result<Vector>();
                        if(std::isfinite(v.x) && std::isfinite(v.y)) {
                            spawnLoc.x=v.x;spawnLoc.y=v.y;spawnLoc.z=50000.0f;
                        }
                    }
                }
            }

            if(session.pawn) {
                Call k2set("/Script/Engine.Actor.K2_SetActorLocation");
                if(k2set.ok()) {
                    bool bSweep=false,bTeleport=true;
                    k2set.set("NewLocation",spawnLoc);
                    k2set.set("bSweep",bSweep);
                    k2set.set("bTeleport",bTeleport);
                    k2set.run(session.pawn);
                }
                Call skydive("/Script/FortniteGame.FortPlayerPawn.BeginSkydiving");
                bool local=skydive.ok() && skydive.set("bFromBus",true) && skydive.run(session.pawn);
                write<uint8_t>(session.pawn,0x1A1D,1);
                Call camera("/Script/Engine.PlayerController.SetViewTargetWithBlend");
                bool view=camera.ok() && camera.set("NewViewTarget",session.pawn) && camera.run(session.controller);
                LOG("Skydiving initiated: local=%d camera=%d at (%.0f,%.0f,%.0f)",
                    local,view,spawnLoc.x,spawnLoc.y,spawnLoc.z);
            }
            return;
        }
        if(session.lobby_requested) {original_pe(object,function,params);return;}
        bool relevant=object==session.controller || object==session.pawn;
        void* current_weapon=object_field(session.pawn,"/Script/FortniteGame.FortPawn.CurrentWeapon");
        relevant=relevant || object==current_weapon;
        static void* interaction_class=nullptr;
        if(!interaction_class)interaction_class=find("/Script/FortniteGame.FortControllerComponent_Interaction");
        bool interaction=isa(object,interaction_class);
        if(!relevant && !interaction) {original_pe(object,function,params);return;}
        Guard guard;
        std::string method=name(function);
        if(method=="ServerAttemptInteract" && interaction) {
            Call owner("/Script/Engine.ActorComponent.GetOwner");
            relevant=owner.run(object) && owner.result<void*>()==session.controller;
        }
        if(relevant && fn<bool(*)(void*)>(profile::IsStandalone)(session.world)) {
            if(session.input_reports<48) {
                ++session.input_reports;LOG("Input event %s receiver=%s",method.c_str(),name(read<void*>(object,profile::ObjectClass)).c_str());
            }
            if(method=="ServerExecuteInventoryItem" && object==session.controller) {
                void* arg=incoming(function,params,"ItemGuid",sizeof(Guid));
                if(arg)for(auto* items:{&session.slots,&session.secondary_slots})for(void* item:*items)if(item) {
                    Guid candidate=item_guid(item),requested=read<Guid>(arg);
                    if(!memcmp(&candidate,&requested,sizeof(Guid)) && equip(item))return;
                }
            }
            if((method=="PawnStartFire" || method=="ServerStartFire") &&
               (object==session.pawn || object==current_weapon)) {
                uint8_t mode=0;void* arg=incoming(function,params,"FireModeNum",1);
                if(arg)mode=read<uint8_t>(arg);
                LOG("Fire request mode=%u weapon=%p",mode,current_weapon);
            }
            if(method=="ServerHandlePickup" && object==session.pawn) {
                void* arg=incoming(function,params,"Pickup",8);
                if(arg && local_pickup(read<void*>(arg)))return;
            }
            if(method=="ServerAttemptInteract" && local_interact(function,params))return;
            if(method.rfind("ServerCreateBuilding",0)==0 && object==session.controller && local_build(function,params))return;
        }
    }
    original_pe(object,function,params);
}
std::string trim_lower(std::string s) {
    while(!s.empty() && (s.back()==' ' || s.back()=='\t'))s.pop_back();
    auto pos=s.find_first_not_of(" \t");if(pos!=std::string::npos)s.erase(0,pos);
    for(char& c:s)if(c>='A' && c<='Z')c=char(c-'A'+'a');return s;
}
bool exec_hook(void* engine,void* world,const char16_t* command,void* output) {
    std::string raw=narrow(command), normalized=trim_lower(raw);
    static std::atomic<bool> first_exec{true};
    if(first_exec.exchange(false))LOG("EngineExec first callback: tid=%d game_tid=%d",int(gettid()),game_tid.load());
    if(normalized=="athena.lobby" && game_tid.load()==gettid() && session.world) {
        session.lobby_requested=true;return true;
    }
    if(normalized=="athena.build" && game_tid.load()==gettid() && session.finalized) {
        Guard guard;local_build(nullptr,nullptr);return true;
    }
    if(normalized=="athena.calendar" && game_tid.load()==gettid() && session.finalized) {
        Guard guard;session.next_calendar=0;refresh_calendar(monotonic_seconds());return true;
    }
    if(normalized=="startevent") {
        if(game_tid.load()!=gettid()) {LOG("startevent rejected outside game thread");return true;}
        request_end_event();return true;
    }
    bool athena=normalized=="open athena_terrain" || normalized=="open athena_terrain?play=1?game=engine.gamemode";
    if(athena || normalized.rfind("athena.",0)==0)
        LOG("Local command observed: tid=%d game_tid=%d athena_travel=%d",int(gettid()),game_tid.load(),athena);
    if((athena || normalized.rfind("athena.",0)==0) && game_tid.load()!=gettid())
        LOG("Command forwarded without modification: WorldTick game-thread identity does not match");
    if(athena && game_tid.load()==gettid()) {
        Guard guard;
        session=Session{};session.requested=true;
        LOG("Travel requested; keeping the original Athena game mode");
        return original_exec(engine,world,u"open Athena_Terrain",output);
    }
    if(session.pawn && session.world==world && game_tid.load()==gettid() &&
       (normalized=="fly" || normalized=="walk" || normalized=="ghost" || normalized=="god")) {
        Guard guard;
        if(normalized=="god") {
            void* p=property("/Script/Engine.Actor.bCanBeDamaged");
            if(p) {
                uint8_t byte=read<uint8_t>(p,0x81),mask=read<uint8_t>(p,0x82);
                if(byte<8 && mask) {
                    bool current=(read<uint8_t>(field(session.pawn,p),byte)&mask)!=0;
                    set_bool(session.pawn,"/Script/Engine.Actor.bCanBeDamaged",!current);
                    LOG("God mode=%d",current);
                }
            }
        } else {
            void* p=property("/Script/Engine.Character.CharacterMovement");
            void* movement=p?read<void*>(field(session.pawn,p)):nullptr;
            Call mode("/Script/Engine.CharacterMovementComponent.SetMovementMode");
            uint8_t movement_mode=normalized=="walk"?1:5,custom=0;
            if(movement && mode.ok() && mode.set("NewMovementMode",movement_mode) &&
               mode.set("NewCustomMode",custom))mode.run(movement);
            Call collision("/Script/Engine.Actor.SetActorEnableCollision");
            bool enabled=normalized!="ghost";
            if(collision.ok() && collision.set("bNewActorEnableCollision",enabled))collision.run(session.pawn);
            LOG("Local movement cheat %s",normalized.c_str());
        }
        return true;
    }
    if(normalized.rfind("open ",0)==0 && game_tid.load()==gettid()) {
        restore_defaults();session=Session{};
    }
    if(normalized.rfind("athena.",0)==0 && game_tid.load()==gettid()) {
        Guard guard;
        if(normalized=="athena.status") {
            LOG("STATUS world=%p controller=%p pawn=%p inventory=%p inventory_ready=%d attempted=%d",session.world,session.controller,session.pawn,session.inventory,session.inventory_ready,session.attempted);
        } else if(normalized=="athena.inventory" && session.inventory) {
            for(size_t i=0;i<session.slots.size();++i)LOG("SLOT %zu %s",i,name(item_definition(session.slots[i])).c_str());
        } else if(normalized.rfind("athena.equip ",0)==0) {
            int slot=-1;if(sscanf(normalized.c_str(),"athena.equip %d",&slot)==1 && slot>=0 && slot<6)equip(session.slots[size_t(slot)]);
        } else if(normalized.rfind("athena.give ",0)==0 && session.inventory_ready) {
            char asset[512]{};int count=1,slot=-1;
            if(sscanf(raw.c_str(),"%*s %511s %d %d",asset,&count,&slot)>=1) {
                void* definition=asset[0]=='/'?load_asset(asset):by_short_name(asset);
                LOG("Give result=%d",give(definition,count,slot));
            }
        } else LOG("Commands: athena.status | athena.inventory | athena.equip 0..5 | athena.give AssetName [count] [slot]");
        return true;
    }
    return original_exec(engine,world,command,output);
}
bool prologue(const char* label,uintptr_t address,std::initializer_list<uint32_t> words) {
    size_t i=0;
    for(auto word:words) {
        uint32_t actual=read<uint32_t>(reinterpret_cast<void*>(base+address),4*i);
        if(actual!=word) {
            LOG("PROLOGUE MISMATCH %s RVA=%zx byte=%zu expected=%08x actual=%08x",label,size_t(address),4*i,word,actual);
            return false;
        }
        ++i;
    }
    LOG("Prologue verified %s RVA=%zx",label,size_t(address));
    return true;
}
}
void install_game_hooks(uintptr_t engine_base) {
    base=engine_base;
    bool pe_valid=prologue("ProcessEvent",profile::ProcessEvent,{0xf81a0ffc,0xa90167fa,0xa9025ff8,0xa90357f6});
    bool tick_valid=prologue("WorldTick",profile::WorldTick,{0xd10503ff,0x6d0d23e9,0xa90e6ffc,0xa90f67fa});
    bool exec_valid=prologue("EngineExec",profile::EngineExec,{0xd10243ff,0xf90023f9,0xa9055ff8,0xa90657f6});
    if(!pe_valid || !tick_valid || !exec_valid) {
        LOG("Hook prologue mismatch; refusing to patch another mod's detour");return;
    }
    int pe=DobbyHook(reinterpret_cast<void*>(base+profile::ProcessEvent),reinterpret_cast<void*>(pe_hook),reinterpret_cast<void**>(&original_pe));
    if(pe!=0 || !original_pe){LOG("ProcessEvent hook failed: %d",pe);return;}
    int tick=DobbyHook(reinterpret_cast<void*>(base+profile::WorldTick),reinterpret_cast<void*>(tick_hook),reinterpret_cast<void**>(&original_tick));
    if(tick!=0 || !original_tick){LOG("WorldTick hook failed: %d",tick);return;}
    int exec=DobbyHook(reinterpret_cast<void*>(base+profile::EngineExec),reinterpret_cast<void*>(exec_hook),reinterpret_cast<void**>(&original_exec));
    if(exec!=0 || !original_exec){LOG("EngineExec hook failed: %d; Athena travel is unavailable",exec);return;}
    LOG("Hooks installed ProcessEvent=%d WorldTick=%d EngineExec=%d",pe,tick,exec);
}
