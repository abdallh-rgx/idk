#include <jni.h>
#include <pthread.h>
#include <dlfcn.h>
#include <link.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
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
#include <set>
#include <algorithm>
#include <android/log.h>

using int8=int8_t; using int16=int16_t; using int32=int32_t; using int64=int64_t;
using uint8=uint8_t; using uint16=uint16_t; using uint32=uint32_t; using uint64=uint64_t;
using TCHAR=char16_t;

#define LT "Era21"
static FILE* g_log=nullptr;
static void LI(const char* f,...){
    char b[1024]; va_list a; va_start(a,f); vsnprintf(b,sizeof(b),f,a); va_end(a);
    __android_log_write(ANDROID_LOG_INFO,LT,b);
    if(g_log){fprintf(g_log,"%s\n",b);fflush(g_log);}
}
#define LOG(...) LI(__VA_ARGS__)
static void InitLog(){
    const char* p[]={
        "/storage/emulated/0/Android/data/com.epicgames.fortnite/files/era21.txt",
        "/sdcard/Android/data/com.epicgames.fortnite/files/era21.txt",
        "/data/local/tmp/era21.txt",nullptr};
    for(int i=0;p[i];++i){ g_log=fopen(p[i],"w"); if(g_log) break; }
}

struct FName{uint32 Comp,Num;bool operator==(const FName&o)const{return Comp==o.Comp&&Num==o.Num;}};
struct FString{TCHAR* Data;int32 Num;int32 Max;};
struct FVector{double X,Y,Z;FVector():X(0),Y(0),Z(0){}FVector(double x,double y,double z):X(x),Y(y),Z(z){}};
struct FRotator{double Pitch,Yaw,Roll;FRotator():Pitch(0),Yaw(0),Roll(0){}FRotator(double p,double y,double r):Pitch(p),Yaw(y),Roll(r){}};
struct FQuat{double X,Y,Z,W;FQuat():X(0),Y(0),Z(0),W(1){}};
struct alignas(16) FTransform{FQuat R;FVector T;uint8 _a[8];FVector S;uint8 _b[8];FTransform(){S.X=S.Y=S.Z=1;}};
struct FGuid{uint32 A,B,C,D;bool Valid()const{return(A|B|C|D)!=0;}bool operator==(const FGuid&o)const{return A==o.A&&B==o.B&&C==o.C&&D==o.D;}};
struct FGTag{FName TagName;};
struct FGTContainer{TArray<FGTag> Tags;TArray<FGTag> Parent;};

enum:uint32{
    O_Flags=8,O_Index=0xC,O_Class=0x10,O_Name=0x18,O_Outer=0x20,
    S_Super=0x40,S_Children=0x48,S_ChildProps=0x50,S_Size=0x58,
    F_Flags=0xB0,F_PSize=0xB6,F_Native=0xC0,Fd_Next=0x20,Fd_Name=0x28,
    P_ArrayDim=0x30,P_ElemSize=0x34,P_Flags=0x38,P_Offset=0x44,
    B_Size=0x74,B_ByteOff=0x75,B_ByteMask=0x76,B_FieldMask=0x77,
    C_CDO=0x110,AP_Inner=0x70,SP_Struct=0x70
};
enum:uint64{CPF_Parm=0x80,CPF_Out=0x100,CPF_Ret=0x400};
enum:uint32{RF_CDO=0x10};

class UObject; class UClass; class UStruct; class UFunction; class FProperty;

class UObject{
public:
    uint32 Flags()const{return*(uint32*)((uint8*)this+O_Flags);}
    UClass* Class()const{return*(UClass**)((uint8*)this+O_Class);}
    FName Name()const{return*(FName*)((uint8*)this+O_Name);}
    UObject* Outer()const{return*(UObject**)((uint8*)this+O_Outer);}
    bool IsCDO()const{return(Flags()&RF_CDO)!=0;}
    std::string NS()const;
    std::string FN()const;
    bool IsA(UClass*)const;
    UFunction* GF(const char*);
    bool PE(UFunction*,void*);
    int32 Off(const char*);
};
class UField:public UObject{public:UField* Next()const{return*(UField**)((uint8*)this+Fd_Next);}};
class UStruct:public UField{public:
    UStruct* Super()const{return*(UStruct**)((uint8*)this+S_Super);}
    UField* Children()const{return*(UField**)((uint8*)this+S_Children);}
    UField* ChildProps()const{return*(UField**)((uint8*)this+S_ChildProps);}
    int32 Size()const{return*(int32*)((uint8*)this+S_Size);}
};
class UFunction:public UStruct{public:
    uint32 FF()const{return*(uint32*)((uint8*)this+F_Flags);}
    uint16 PSize()const{return*(uint16*)((uint8*)this+F_PSize);}
    void* Ntv()const{return*(void**)((uint8*)this+F_Native);}
};
class UClass:public UStruct{public:UObject* CDO()const{return*(UObject**)((uint8*)this+C_CDO);}};
class FProperty:public UField{public:
    int32 ES()const{return*(int32*)((uint8*)this+P_ElemSize);}
    uint64 PF()const{return*(uint64*)((uint8*)this+P_Flags);}
    int32 Of()const{return*(int32*)((uint8*)this+P_Offset);}
    bool Par()const{return(PF()&CPF_Parm)!=0;}
    bool Out()const{return(PF()&CPF_Out)!=0;}
    bool Ret()const{return(PF()&CPF_Ret)!=0;}
};

struct FUObjectItem{UObject* Object;int32 Flags;int32 Cluster;int32 Serial;};
struct FUObjectArray{
    void** Objects;uint8 _p[8];int32 Max;int32 Num;int32 MaxChunks;int32 NumChunks;
    UObject* At(uint32 i){
        if(!Objects)return nullptr;
        uint32 ci=i/0x10000,wi=i%0x10000;
        if((int32)ci>=NumChunks)return nullptr;
        auto c=(FUObjectItem*)Objects[ci];if(!c)return nullptr;
        return c[wi].Object;
    }
};

static uintptr_t g_base=0;
static FUObjectArray* g_go=nullptr;
static void*(*g_PE)(UObject*,UFunction*,void*)=nullptr;
static void*(*g_Find)(UObject*,UObject*,const char16_t*,bool)=nullptr;
static UObject*(*g_LoadO)(UClass*,UObject*,const char16_t*,const char16_t*,uint32,void*,bool,void*)=nullptr;
static void*(*g_LoadC)(UClass*,UObject*,const char16_t*,const char16_t*,uint32,void*)=nullptr;
static void*(*g_N2S)(const void*,FString*)=nullptr;
static void(*g_Free)(void*)=nullptr;
static void*(*g_Real)(void*,size_t,uint32)=nullptr;
static int32(*g_Add)(void*,void*,const void*)=nullptr;
static void(*g_Rem)(void*,void*,int32)=nullptr;
static void*(*g_Spawn)(UObject*,UObject*,const FTransform*,uint8,UObject*)=nullptr;
static UObject*(*g_Finish)(UObject*,const FTransform*)=nullptr;
static UObject*(*g_GetPC)(UObject*,int)=nullptr;
static bool(*g_Stand)(UObject*)=nullptr;

static constexpr uintptr_t R_PE=0x07611B5C,R_Find=0x0762A664,R_LoadO=0x0761C568,R_LoadC=0x0762CFB8;
static constexpr uintptr_t R_N2S=0x074937CC,R_Free=0x07355370,R_Real=0x0735E520;
static constexpr uintptr_t R_Add=0x0944A668,R_Rem=0x0944B2DC;
static constexpr uintptr_t R_GO=0x0DCD6E08,R_Tick=0x0949685C,R_Exec=0x098BB160;
static constexpr uintptr_t R_Stand=0x0945F840,R_GetPC=0x093A1D48;
static constexpr uintptr_t R_Begin=0x093A37E4,R_Finish=0x0939F1B8;
static constexpr uintptr_t R_Iface=0x0761AA88;

static std::atomic<int> g_tid{0};
static thread_local bool g_int=false;
struct G{bool s;G(){s=g_int;g_int=true;}~G(){g_int=s;}};

static bool Bad(const void* p){
    if(!p)return true;
    uintptr_t v=(uintptr_t)p;
    return v<0x10000||v>0x0000FFFFFFFFFFFFULL;
}
template<class T>static T Rd(const void* p,size_t o=0){T r{};if(p)memcpy(&r,(const char*)p+o,sizeof(T));return r;}
template<class T>static void Wr(void* p,size_t o,const T& v){if(p)memcpy((char*)p+o,&v,sizeof(v));}
template<class F>static F FA(uintptr_t r){return reinterpret_cast<F>(g_base+r);}

static void N2S(const void* p,std::string& out){
    FString s{};
    if(g_N2S) g_N2S(p,&s);
    if(s.Data){
        for(int i=0;i<s.Num&&s.Data[i];++i)
            out.push_back(s.Data[i]<128?(char)s.Data[i]:'?');
        g_Free(s.Data);
    }
}
std::string UObject::NS()const{std::string r;N2S((char*)this+O_Name,r);return r;}
std::string UObject::FN()const{
    std::string n=NS();UObject* o=Outer();
    while(o&&!Bad(o)){n=o->NS()+"."+n;o=o->Outer();}
    return n;
}
bool UObject::IsA(UClass* c)const{
    if(!c||Bad(c))return false;
    UClass* k=Class();
    for(int i=0;i<64&&k&&!Bad(k);++i){
        if(k==c)return true;
        k=(UClass*)k->Super();
    }
    return false;
}
UFunction* UObject::GF(const char* n){
    UClass* k=Class();
    for(int d=0;d<64&&k&&!Bad(k);++d){
        UField* c=k->ChildProps();
        for(int i=0;c&&!Bad(c)&&i<2048;++i,c=c->Next()){
            std::string cn;N2S((char*)c+Fd_Name,cn);
            if(cn==n)return(UFunction*)c;
        }
        UField* ch=k->Children();
        for(int i=0;ch&&!Bad(ch)&&i<2048;++i,ch=ch->Next()){
            std::string cn;N2S((char*)ch+O_Name,cn);
            if(cn==n)return(UFunction*)ch;
        }
        k=(UClass*)k->Super();
    }
    return nullptr;
}
bool UObject::PE(UFunction* f,void* p){
    if(!g_PE||!f)return false;
    G g;g_PE(this,f,p);return true;
}
int32 UObject::Off(const char* n){
    UClass* k=Class();
    for(int d=0;d<64&&k&&!Bad(k);++d){
        UField* c=k->ChildProps();
        for(int i=0;c&&!Bad(c)&&i<2048;++i,c=c->Next()){
            std::string cn;N2S((char*)c+Fd_Name,cn);
            if(cn==n)return Rd<int32>(c,P_Offset);
        }
        k=(UClass*)k->Super();
    }
    return -1;
}

static UObject* FindO(const char* p){
    if(!g_Find)return nullptr;
    std::u16string w(p,p+strlen(p));
    return g_Find(nullptr,nullptr,w.c_str(),false);
}
static UObject* FindS(const char* n){
    if(!g_go)return nullptr;
    for(int32 i=0;i<g_go->Num;i++){
        UObject* o=g_go->At(i);
        if(o&&!Bad(o)&&o->NS()==n)return o;
    }
    return nullptr;
}
static UClass* FindC(const char* n){
    UObject* o=FindO(n);
    if(o)return(UClass*)o;
    return(UClass*)FindS(n);
}
static UObject* LoadA(const std::string& p,const char* t){
    if(UObject* r=FindO(p.c_str()))return r;
    UClass* tc=FindC(t);
    if(!tc)return nullptr;
    std::u16string w(p.begin(),p.end());
    return g_LoadO(tc,nullptr,w.c_str(),nullptr,0,nullptr,true,nullptr);
}
static UClass* LoadC(const char* p,const char* b){
    if(UObject* r=FindO(p))return(UClass*)r;
    UClass* bc=FindC(b);
    if(!bc)return nullptr;
    std::u16string w(p,p+strlen(p));
    return(UClass*)g_LoadC(bc,nullptr,w.c_str(),nullptr,0,nullptr);
}

static bool InitGO(){
    if(g_go)return true;
    if(!g_base)return false;
    auto ok=[](FUObjectArray* g){
        if(!g||Bad(g))return false;
        if(g->Num<1000||g->Num>4000000)return false;
        if(g->NumChunks<1||g->NumChunks>512)return false;
        if(Bad(g->Objects))return false;
        void* c0=Rd<void*>(g->Objects,0);if(Bad(c0))return false;
        void* f=Rd<void*>(c0,0);if(Bad(f))return false;
        if(Bad(Rd<void*>(f,O_Class)))return false;
        return true;
    };
    FUObjectArray* ind=Rd<FUObjectArray*>((void*)(g_base+R_GO));
    if(ok(ind)){g_go=ind;LOG("GO indirect");return true;}
    FUObjectArray* dir=(FUObjectArray*)(g_base+R_GO);
    if(ok(dir)){g_go=dir;LOG("GO direct");return true;}
    return false;
}
static void* M(size_t n){return g_Real?g_Real(nullptr,n,0):malloc(n);}

struct Call{
    void* F=nullptr;
    struct P{std::string n;int o,s;uint64 f;};
    std::vector<P> Ps;
    alignas(16) uint8 D[16384]{};
    bool ok_=false;
    explicit Call(const char* p,UObject* recv=nullptr){
        F=FindO(p);
        if(!F)return;
        if(recv&&!Bad(recv)){
            std::string ev=((UObject*)F)->NS();
            UObject* ft=FindO("/Script/CoreUObject.Function");
            UClass* k=recv->Class();UObject* dv=nullptr;
            for(int d=0;d<64&&k&&!Bad(k)&&ft;++d){
                UField* c=k->ChildProps();
                for(int i=0;c&&!Bad(c)&&i<2048;++i,c=c->Next()){
                    std::string cn;N2S((char*)c+Fd_Name,cn);
                    if(cn==ev&&((UObject*)c)->IsA((UClass*)ft)){dv=(UObject*)c;break;}
                }
                if(dv)break;
                k=(UClass*)k->Super();
            }
            if(dv)F=dv;
        }
        I();
    }
    explicit Call(void* f){F=f;I();}
private:
    void I(){
        if(!F||Bad(F)){F=nullptr;return;}
        int t=Rd<uint16>(F,F_PSize);
        if(t<0||t>(int)sizeof(D))return;
        auto add=[&](void* p,bool field){
            uint64 fl=Rd<uint64>(p,P_Flags);
            if(!(fl&CPF_Parm))return;
            int o=Rd<int>(p,P_Offset);
            int s=Rd<int>(p,P_ElemSize);
            if(o<0||s<1||o+s>t)return;
            std::string n;N2S((char*)p+(field?Fd_Name:O_Name),n);
            Ps.push_back({n,o,s,fl});
        };
        UField* c=((UStruct*)F)->ChildProps();
        for(int i=0;c&&!Bad(c)&&i<256;++i,c=c->Next())add(c,true);
        UField* ch=((UStruct*)F)->Children();
        for(int i=0;ch&&!Bad(ch)&&i<256;++i,ch=ch->Next())add(ch,false);
        ok_=(t==0)||!Ps.empty();
    }
public:
    bool ok()const{return ok_;}
    template<class T>bool set(const char* k,const T& v){
        for(auto& p:Ps)if(p.n==k&&p.s==(int)sizeof(T)){memcpy(D+p.o,&v,sizeof(T));return true;}
        return false;
    }
    template<class T>bool setAny(const T& v,const char* a,const char* b=nullptr,const char* c=nullptr){
        return set(a,v)||(b&&set(b,v))||(c&&set(c,v));
    }
    template<class T>bool setAt(int i,const T& v){
        int n=0;
        for(auto& p:Ps)if(!(p.f&CPF_Ret)&&p.s==(int)sizeof(T))if(n++==i){memcpy(D+p.o,&v,sizeof(T));return true;}
        return false;
    }
    bool setB(const char* k,const void* s,int z){
        for(auto& p:Ps)if(p.n==k&&p.s==z&&s){memcpy(D+p.o,s,z);return true;}
        return false;
    }
    template<class T>T res()const{for(auto& p:Ps)if((p.f&CPF_Ret)&&p.s==(int)sizeof(T))return Rd<T>(D,p.o);return{};}
    template<class T>T out(const char* k)const{for(auto& p:Ps)if(p.n==k&&(p.f&CPF_Out)&&p.s==(int)sizeof(T))return Rd<T>(D,p.o);return{};}
    template<class T>T get(const char* k)const{for(auto& p:Ps)if(p.n==k&&p.s==(int)sizeof(T))return Rd<T>(D,p.o);return{};}
    bool run(UObject* o){
        if(!ok_||!o||Bad(o)||!g_PE)return false;
        UObject* ow=((UObject*)F)->Outer();
        if(ow&&!Bad(ow)&&!o->IsA((UClass*)ow)){
            if(((UObject*)F)->NS()!="HandleQuickbarContentChanged")return false;
        }
        G g;g_PE(o,(UFunction*)F,D);return true;
    }
};
static void* Inc(void* f,void* d,const char* k,int exp=0){
    if(!f||!d||Bad(f))return nullptr;
    int t=Rd<uint16>(f,F_PSize);
    UField* c=((UStruct*)f)->ChildProps();
    for(int i=0;c&&!Bad(c)&&i<256;++i,c=c->Next()){
        uint64 fl=Rd<uint64>(c,P_Flags);
        if(!(fl&CPF_Parm))continue;
        std::string n;N2S((char*)c+Fd_Name,n);
        if(n!=k)continue;
        int o=Rd<int>(c,P_Offset);
        int s=Rd<int>(c,P_ElemSize);
        if(o<0||s<1||o+s>t)return nullptr;
        if(exp&&s!=exp)return nullptr;
        return(char*)d+o;
    }
    return nullptr;
}

// === WORLD HELPERS ===
static double Now(){
    timespec t{};clock_gettime(CLOCK_MONOTONIC,&t);
    return t.tv_sec+t.tv_nsec*1e-9;
}
static float WT(){
    static float c=0;static double at=0;
    double n=Now();
    if(n-at<0.2)return c;
    at=n;
    if(!g_base)return c;
    UObject* cdo=FindO("/Script/Engine.Default__GameplayStatics");
    if(!cdo)return c;
    Call f("/Script/Engine.GameplayStatics.GetTimeSeconds");
    if(!f.ok())return c;
    return c;
}
static uint32 Rnd(){static uint32 s=0x9E3779B9;s^=s<<13;s^=s>>17;s^=s<<5;return s;}
static float R01(){return(Rnd()&0xFFFFFF)/16777216.0f;}
static void RotToQ(const FRotator& r,double q[4]){
    const double h=0.00872664626;
    double sp=std::sin(r.Pitch*h),cp=std::cos(r.Pitch*h);
    double sy=std::sin(r.Yaw*h),cy=std::cos(r.Yaw*h);
    double sr=std::sin(r.Roll*h),cr=std::cos(r.Roll*h);
    q[0]=cr*sp*sy-sr*cp*cy;
    q[1]=-cr*sp*cy-sr*cp*sy;
    q[2]=cr*cp*sy-sr*sp*cy;
    q[3]=cr*cp*cy+sr*sp*sy;
}

// === SESSION ===
struct Session{
    UObject* world=nullptr,*controller=nullptr,*mode=nullptr,*state=nullptr,*pawn=nullptr;
    UObject* inventory=nullptr,*quickbars=nullptr,*editTool=nullptr;
    bool attempt=false,finalized=false,ability=false,qb=false,visibility=false,poolReady=false;
    std::array<UObject*,6> slots{};
    std::array<UObject*,6> bslots{};
    std::vector<UObject*> parts,sets;
    double finalizeAt=0,observedAt=0,retryAt=0,lastVis=0,lastSp=0,lastAb=0,lastQC=0;
    double lastWild=0,lastVeh=0,lastNear=0,lastInteract=0,lastBotWave=0,lastNpc=0,lastStorm=0;
    int lastPhase=-1,worldIdx=-1;
    bool aircraftJumped=false,lobbyRequest=false,scanDone=false,npcSpawned=false;
    bool autoInteract=true;
    std::vector<UObject*> nearContainers,nearPickups,containers,vehicles,npcs,heads,bodies;
    std::vector<FVector> nearContainerLocs,nearPickupLocs,containerLocs;
    std::map<UObject*,double> autoAt,pickupAt;
    UObject* botPawn=nullptr,*botCtrl=nullptr,*botState=nullptr,*botHero=nullptr;
    int botCount=0,botTarget=99;
    FGuid lastEquip{};
} S;

struct Motor{
    UObject* pawn;
    float yaw;
    double next;
    float lastX,lastY;
    double lastCheck;
    uint8 vel;
    uint8 busState;
    float jumpA;
    FVector dropT;
    float landZ;
    double nextStrike;
};
static std::vector<Motor> g_bots,g_wild;

static UObject* g_hudCtx=nullptr,*g_airCmp=nullptr,*g_wallC=nullptr,*g_bcC=nullptr,*g_pickC=nullptr,*g_vehC=nullptr;
static UObject* g_pawnC=nullptr,*g_botCtrlC=nullptr,*g_botStateC=nullptr;

// === BAD POINTER GUARD ===
static bool BadA(UObject* o){return !o||Bad(o)||o->IsCDO();}

// === BASIC WORLD ===
static UObject* GetPC(UObject* w,int i=0){return g_GetPC?g_GetPC(w,i):nullptr;}
static UObject* GetPawn(UObject* c){
    if(!c)return nullptr;
    int o=c->Off("Pawn");
    if(o<0)return nullptr;
    return Rd<UObject*>(c,(size_t)o);
}
static void Possess(UObject* c,UObject* p){
    if(!c||!p)return;
    Call k("/Script/Engine.Controller.Possess");
    if(k.ok()&&k.set("InPawn",p))k.run(c);
}
static UObject* SpawnActor(UObject* w,UObject* k,UObject* o,const FTransform& t){
    if(!w||!k||!g_Spawn||!g_Finish)return nullptr;
    UObject* a=g_Spawn(w,k,&t,1,o);
    if(!a)return nullptr;
    return g_Finish(a,&t);
}
static bool ActorLoc(UObject* a,FVector& out){
    if(!a)return false;
    Call c("/Script/Engine.Actor.K2_GetActorLocation");
    if(!c.ok()||!c.run(a))return false;
    out=c.res<FVector>();
    return std::isfinite(out.X)&&std::isfinite(out.Y)&&std::isfinite(out.Z);
}
static bool NearPlayer(const FVector& p,float lim){
    FVector o{};
    if(!S.pawn||!ActorLoc(S.pawn,o))return false;
    double dx=p.X-o.X,dy=p.Y-o.Y,dz=p.Z-o.Z;
    return dx*dx+dy*dy+dz*dz<=double(lim)*double(lim);
}
static void* Iface(UObject* o,UObject* ic){
    if(!o||!ic)return nullptr;
    using Fn=void*(*)(UObject*,UObject*);
    return FA<Fn>(R_Iface)(o,ic);
}
static FGuid MakeGuid(){
    FGuid g{};
    UObject* k=FindO("/Script/Engine.Default__KismetGuidLibrary");
    if(!k)return g;
    Call c("/Script/Engine.KismetGuidLibrary.NewGuid");
    if(c.ok()&&c.run(k))g=c.res<FGuid>();
    return g;
}
static UObject* GetASC(UObject* a){
    if(!a)return nullptr;
    int o=a->Off("AbilitySystemComponent");
    if(o>0){UObject* r=Rd<UObject*>(a,(size_t)o);if(r&&!Bad(r))return r;}
    return nullptr;
}

// === BOOL REFLECTION ===
static bool SetBool(UObject* o,const char* p,bool v){
    if(!o)return false;
    int32 off=o->Off(p);
    if(off<0)return false;
    UClass* k=o->Class();
    for(int d=0;d<64&&k&&!Bad(k);++d){
        UField* c=k->ChildProps();
        for(int i=0;c&&!Bad(c)&&i<2048;++i,c=c->Next()){
            std::string cn;N2S((char*)c+Fd_Name,cn);
            if(cn!=p)continue;
            uint8 sz=Rd<uint8>(c,B_Size);
            uint8 bo=Rd<uint8>(c,B_ByteOff);
            uint8 fm=Rd<uint8>(c,B_FieldMask);
            if(sz==0||sz>8||bo>=sz||fm==0)return false;
            uint8* dst=(uint8*)o+off+bo;
            *dst=v?(*dst|fm):(*dst&~fm);
            return true;
        }
        k=(UClass*)k->Super();
    }
    return false;
}
static bool GetBool(UObject* o,const char* p){
    if(!o)return false;
    UClass* k=o->Class();
    for(int d=0;d<64&&k&&!Bad(k);++d){
        UField* c=k->ChildProps();
        for(int i=0;c&&!Bad(c)&&i<2048;++i,c=c->Next()){
            std::string cn;N2S((char*)c+Fd_Name,cn);
            if(cn!=p)continue;
            int32 off=Rd<int32>(c,P_Offset);
            uint8 sz=Rd<uint8>(c,B_Size);
            uint8 bo=Rd<uint8>(c,B_ByteOff);
            uint8 fm=Rd<uint8>(c,B_FieldMask);
            if(sz==0||sz>8||bo>=sz||fm==0)return false;
            return(Rd<uint8>(o,(size_t)(off+bo))&fm)!=0;
        }
        k=(UClass*)k->Super();
    }
    return false;
}
static UObject* GetObj(UObject* o,const char* p){
    if(!o)return nullptr;
    int32 off=o->Off(p);
    if(off<0)return nullptr;
    return Rd<UObject*>(o,(size_t)off);
}

// === GROUND TRACE ===
static bool GroundZ(float x,float y,float& z){
    if(!S.world)return false;
    Call t("/Script/Engine.KismetSystemLibrary.LineTraceSingle");
    if(!t.ok()||!t.set("WorldContextObject",S.world))return false;
    FVector s{x,y,20000},e{x,y,-20000};
    TArray<void*> ign{};
    uint8 ch=1,dbg=0;
    bool cx=false,ig=true;
    uint8 hit[224]{};
    t.set("Start",s);
    t.set("End",e);
    t.set("TraceChannel",ch);
    t.set("bTraceComplex",cx);
    t.setB("ActorsToIgnore",&ign,sizeof(ign));
    t.set("DrawDebugType",dbg);
    t.setB("OutHit",hit,sizeof(hit));
    t.set("bIgnoreSelf",ig);
    if(!t.run(FindO("/Script/Engine.Default__KismetSystemLibrary")))return false;
    if(!(hit[0]&1))return false;
    for(int off=0x10;off<=0x40;off+=8){
        FVector v=Rd<FVector>(hit,(size_t)off);
        if(std::isfinite(v.X)&&std::isfinite(v.Y)&&std::isfinite(v.Z)&&
           std::fabs(v.X-x)<500&&std::fabs(v.Y-y)<500&&std::fabs(v.Z)<50000){
            z=v.Z;return true;
        }
    }
    return false;
}

// === CRASH HANDLER ===
static void Crash(int sig,siginfo_t* si,void* ctx){
    char b[2048];int n=0;
    n+=snprintf(b+n,sizeof(b)-(size_t)n,"CRASH sig=%d addr=%p base=%p\n",sig,si?si->si_addr:nullptr,(void*)g_base);
    ucontext_t* uc=(ucontext_t*)ctx;
    if(uc){
#if defined(__aarch64__)
        uintptr_t pc=(uintptr_t)uc->uc_mcontext.pc;
        uintptr_t sp=(uintptr_t)uc->uc_mcontext.sp;
        n+=snprintf(b+n,sizeof(b)-(size_t)n,"PC=0x%lx lib+0x%lx\n",(unsigned long)pc,g_base?(unsigned long)(pc-g_base):0ul);
        if(sp){
            for(uintptr_t p=sp&~(uintptr_t)0xF;p<sp+0x400&&n<(int)sizeof(b)-64;p+=8){
                uintptr_t v=*(uintptr_t*)p;
                if(g_base&&v>g_base&&v<g_base+0xE000000)
                    n+=snprintf(b+n,sizeof(b)-(size_t)n,"  lib+0x%lx\n",(unsigned long)(v-g_base));
            }
        }
#endif
    }
    int fd=open("/data/local/tmp/era21_crash.txt",O_WRONLY|O_CREAT|O_APPEND,0644);
    if(fd>=0){ssize_t w=write(fd,b,(size_t)n);(void)w;close(fd);}
    signal(sig,SIG_DFL);
    raise(sig);
}
static void InstallCrash(){
    struct sigaction sa;
    memset(&sa,0,sizeof(sa));
    sa.sa_sigaction=Crash;
    sa.sa_flags=SA_SIGINFO;
    sigaction(SIGSEGV,&sa,nullptr);
    sigaction(SIGBUS,&sa,nullptr);
    sigaction(SIGILL,&sa,nullptr);
    sigaction(SIGABRT,&sa,nullptr);
}
// ==== PART 2: INVENTORY / QUICKBARS / COSMETICS / ABILITIES / SETUP ====

// === INVENTORY ===
static void InvUpdate(){
    if(S.inventory){
        Call a("/Script/FortniteGame.FortInventory.HandleInventoryLocalUpdate");
        if(a.ok())a.run(S.inventory);
    }
    if(S.controller){
        Call b("/Script/FortniteGame.FortPlayerController.HandleWorldInventoryLocalUpdate");
        if(b.ok())b.run(S.controller);
        Call c("/Script/FortniteGame.FortPlayerController.ClientForceWorldInventoryUpdate");
        if(c.ok())c.run(S.controller);
    }
    for(uint8 bar=0;bar<2;++bar){
        Call c("/Script/FortniteGame.FortPlayerController.ClientForceUpdateQuickbar");
        if(c.ok()&&c.set("QuickbarToRefresh",bar))c.run(S.controller);
    }
}
static UObject* ItemDefOf(UObject* it){
    if(!it)return nullptr;
    Call c("/Script/FortniteGame.FortItem.GetItemDefinitionBP");
    return c.ok()&&c.run(it)?c.res<UObject*>():nullptr;
}
static FGuid ItemGuidOf(UObject* it){
    if(!it)return {};
    Call c("/Script/FortniteGame.FortItem.GetItemGuid");
    return c.ok()&&c.run(it)?c.res<FGuid>():FGuid{};
}
static UObject* WeaponDefForEquip(UObject* def){
    if(!def)return nullptr;
    UObject* pc=FindO("/Script/FortniteGame.AthenaPickaxeItemDefinition");
    if(pc&&def->IsA((UClass*)pc)){
        UObject* w=GetObj(def,"WeaponDefinition");
        if(w&&!Bad(w))return w;
    }
    return def;
}
static bool BindSlot(UObject* item,int slot,uint8 bar){
    if(!item||!S.controller||bar>1||slot<0)return false;
    UObject* def=ItemDefOf(item);
    if(!def)return false;
    FGuid g=ItemGuidOf(item);
    if(!g.Valid())return false;
    UObject* klib=FindO("/Script/FortniteGame.Default__FortKismetLibrary");
    if(klib){
        Call b("/Script/FortniteGame.FortKismetLibrary.EquipItemToQuickBarSlot");
        if(b.ok()&&b.set("WorldContextObject",S.world)&&b.set("ItemDefinition",def)&&
           b.set("ItemVariantGuid",g)&&b.set("QuickBarType",bar)&&b.set("SlotIndex",slot)){
            b.run(klib);
        }
    }
    Call fu("/Script/FortniteGame.FortPlayerController.ClientForceUpdateQuickbar");
    if(fu.ok()&&fu.set("QuickbarToRefresh",bar))fu.run(S.controller);
    return true;
}
static bool GiveItem(UObject* def,int count,int slot,uint8 bar,bool requireNew){
    if(!def||!S.inventory||count<1||count>9999)return false;
    UObject* idc=FindO("/Script/FortniteGame.FortItemDefinition");
    if(idc&&!def->IsA((UClass*)idc))return false;
    int invOff=S.inventory->Off("Inventory");
    if(invOff<0)return false;
    UObject* list=(UObject*)((char*)S.inventory+invOff);
    int instOff=list->Off("ItemInstances");
    int repOff=list->Off("ReplicatedEntries");
    if(instOff<0||repOff<0)return false;
    UObject* entryCdo=FindO("/Script/FortniteGame.Default__FortWorldItem");
    int worldEntryOff=entryCdo?entryCdo->Off("ItemEntry"):-1;
    if(worldEntryOff<0)return false;
    TArray<UObject*> cur=Rd<TArray<UObject*>>(list,(size_t)instOff);
    for(int i=0;i<cur.Num;++i){
        UObject* e=cur.Data[i];
        if(!e||Bad(e))continue;
        if(ItemDefOf(e)==def){
            if(requireNew)return false;
            if(slot>=0&&slot<6)(bar==0?S.slots:S.bslots)[slot]=e;
            return true;
        }
    }
    UObject* inst=nullptr;
    {
        Call c("/Script/FortniteGame.FortItemDefinition.CreateTemporaryItemInstanceBP");
        int lvl=1;
        if(c.ok()&&c.set("Count",count)&&c.set("Level",lvl)&&c.run(def))inst=c.res<UObject*>();
    }
    if(!inst||Bad(inst))return false;
    {
        Call c("/Script/FortniteGame.FortItem.SetOwningControllerForTemporaryItem");
        if(!c.ok()||!c.set("InController",S.controller)||!c.run(inst))return false;
    }
    void* srcEntry=(char*)inst+worldEntryOff;
    UObject* entryType=FindO("/Script/FortniteGame.FortItemEntry");
    int guidOff=-1;
    if(entryType)guidOff=((UClass*)entryType)->Off("ItemGuid");
    if(guidOff<0)return false;
    FGuid g=MakeGuid();
    if(!g.Valid())return false;
    Wr<FGuid>(srcEntry,(size_t)guidOff,g);
    FGuid check=ItemGuidOf(inst);
    if(!(check==g))return false;
    int durOff=-1,cntOff=-1;
    if(entryType){
        durOff=((UClass*)entryType)->Off("Durability");
        cntOff=((UClass*)entryType)->Off("Count");
    }
    if(durOff>0)Wr<float>(srcEntry,(size_t)durOff,1.0f);
    if(cntOff>0)Wr<int32>(srcEntry,(size_t)cntOff,count);
    UObject* wc=FindO("/Script/FortniteGame.FortWeaponItemDefinition");
    if(wc&&def->IsA((UClass*)wc)){
        int ammoOff=-1;
        if(entryType)ammoOff=((UClass*)entryType)->Off("LoadedAmmo");
        if(ammoOff>0)Wr<int32>(srcEntry,(size_t)ammoOff,30);
    }
    if(!g_Add)return false;
    int idx=g_Add((char*)list+instOff,(void*)((char*)list+instOff),&inst);
    if(idx<0)return false;
    int eIdx=g_Add((char*)list+repOff,(void*)((char*)list+repOff),srcEntry);
    if(eIdx<0)return false;
    InvUpdate();
    if(slot>=0&&slot<6){
        (bar==0?S.slots:S.bslots)[slot]=inst;
        BindSlot(inst,slot,bar);
    }
    LOG("GiveItem %s x%d slot=%d bar=%u",def->NS().c_str(),count,slot,(unsigned)bar);
    return true;
}
static void EquipSlot(int slot){
    if(slot<0||slot>=6)return;
    UObject* it=S.slots[slot];
    if(!it||Bad(it))return;
    UObject* def=ItemDefOf(it);
    if(!def)return;
    FGuid g=ItemGuidOf(it);
    if(!g.Valid())return;
    UObject* wdef=WeaponDefForEquip(def);
    if(!wdef)return;
    Call eq("/Script/FortniteGame.FortPawn.EquipWeaponDefinition");
    if(!eq.ok())return;
    FGuid tg{};
    bool dis=false;
    eq.set("WeaponData",wdef);
    eq.set("ItemEntryGuid",g);
    eq.set("TrackerGuid",tg);
    eq.set("bDisableEquipAnimation",dis);
    if(eq.run(S.pawn)){
        LOG("Equip slot %d",slot);
        S.lastEquip=g;
    }
}

// === QUICKBARS ===
static void BuildQB(){
    if(!S.quickbars||Bad(S.quickbars))return;
    int barOff=S.quickbars->Off("PrimaryQuickBar");
    int secOff=S.quickbars->Off("SecondaryQuickBar");
    if(barOff<0||secOff<0)return;
    UObject* qbCdo=FindO("/Script/FortniteGame.Default__FortQuickBars");
    int slotsOff=qbCdo?qbCdo->Off("Slots"):-1;
    UObject* slotCdo=FindO("/Script/FortniteGame.Default__QuickBarSlot");
    int itemsOff=slotCdo?slotCdo->Off("Items"):-1;
    if(slotsOff<0||itemsOff<0)return;
    int slotSize=0x28;
    auto ensure=[&](UObject* bar,int want){
        void* sf=(char*)bar+slotsOff;
        TArray<uint8> arr=Rd<TArray<uint8>>(sf,0);
        if(arr.Num>=want)return arr;
        void* nd=M((size_t)slotSize*want);
        if(!nd)return arr;
        memset(nd,0,(size_t)slotSize*want);
        Wr<void*>(sf,0,nd);
        Wr<int32>(sf,8,want);
        Wr<int32>(sf,12,want);
        return Rd<TArray<uint8>>(sf,0);
    };
    UObject* prim=(UObject*)((char*)S.quickbars+barOff);
    UObject* sec=(UObject*)((char*)S.quickbars+secOff);
    TArray<uint8> pa=ensure(prim,6);
    TArray<uint8> sa=ensure(sec,5);
    auto write=[&](TArray<uint8>& arr,int i,const FGuid& g){
        if(i<0||i>=arr.Num||!arr.Data)return;
        uint8* slot=arr.Data+(size_t)i*slotSize;
        void* itemsField=slot+itemsOff;
        FGuid* gd=(FGuid*)M(16);
        if(!gd)return;
        *gd=g;
        Wr<void*>(itemsField,0,gd);
        Wr<int32>(itemsField,8,1);
        Wr<int32>(itemsField,12,1);
        uint8* flags=slot+0x10;
        *flags|=0x01|0x08;
    };
    for(int i=0;i<6;++i){
        if(S.slots[i]){
            FGuid g=ItemGuidOf(S.slots[i]);
            if(g.Valid())write(pa,i,g);
        }
    }
    for(int i=0;i<5;++i){
        if(S.bslots[i]){
            FGuid g=ItemGuidOf(S.bslots[i]);
            if(g.Valid())write(sa,i,g);
        }
    }
    S.qb=true;
}
static void SetQBReady(){
    if(!S.controller)return;
    int off=S.controller->Off("ClientQuickBars");
    if(off<0)return;
    uint8* p=(uint8*)S.controller+off+8;
    if(*p<=1)*p=1;
}

// === COSMETICS ===
static std::vector<UObject*> SoftArr(UObject* owner,const char* p){
    std::vector<UObject*> r;
    if(!owner)return r;
    int off=owner->Off(p);
    if(off<0)return r;
    int stride=-1;
    UClass* k=owner->Class();
    for(int d=0;d<64&&k&&!Bad(k);++d){
        UField* c=k->ChildProps();
        for(int i=0;c&&!Bad(c)&&i<2048;++i,c=c->Next()){
            std::string cn;N2S((char*)c+Fd_Name,cn);
            if(cn==p){stride=Rd<int32>(c,P_ElemSize);break;}
        }
        if(stride>0)break;
        k=(UClass*)k->Super();
    }
    if(stride<8||stride>128)return r;
    TArray<uint8> arr=Rd<TArray<uint8>>(owner,(size_t)off);
    if(arr.Num<0||arr.Num>64||!arr.Data)return r;
    UObject* klib=FindO("/Script/Engine.Default__KismetSystemLibrary");
    for(int i=0;i<arr.Num;++i){
        Call l("/Script/Engine.KismetSystemLibrary.LoadAsset_Blocking");
        if(l.ok()&&l.setB("Asset",arr.Data+(size_t)i*stride,stride)&&l.run(klib)){
            UObject* v=l.res<UObject*>();
            if(v&&!Bad(v))r.push_back(v);
        }
    }
    return r;
}
static void ApplySkin(UObject* pawn){
    if(!pawn||Bad(pawn))return;
    UObject* mesh=GetObj(pawn,"Mesh");
    if(mesh&&S.parts.empty()){
        UObject* h=LoadA("/Game/Athena/Heroes/Meshes/Heads/F_Med_Head1_ATH.F_Med_Head1_ATH","/Script/FortniteGame.CustomCharacterPart");
        UObject* b=LoadA("/Game/Athena/Heroes/Meshes/Bodies/CP_001_Athena_Body.CP_001_Athena_Body","/Script/FortniteGame.CustomCharacterPart");
        if(h)S.parts.push_back(h);
        if(b)S.parts.push_back(b);
    }
    for(size_t i=0;i<S.parts.size()&&i<7;++i){
        Call c("/Script/FortniteGame.FortPlayerPawn.ServerChoosePart");
        if(c.ok()&&c.set("Part",(uint8)i)&&c.set("ChosenCharacterPart",S.parts[i]))c.run(pawn);
    }
    UObject* st=GetObj(S.controller,"PlayerState");
    if(st&&!S.parts.empty()){
        TArray<UObject*> arr{S.parts.data(),(int32)S.parts.size(),(int32)S.parts.size()};
        UObject* klib=FindO("/Script/FortniteGame.Default__FortKismetLibrary");
        if(klib){
            Call c("/Script/FortniteGame.FortKismetLibrary.ApplyCharacterCosmetics");
            if(c.ok()&&c.set("WorldContextObject",S.world)&&c.set("CharacterParts",arr)&&c.set("PlayerState",st))c.run(klib);
        }
    }
    Call v("/Script/FortniteGame.FortPlayerPawnAthena.SetCharacterBodyVisibilityForPossession");
    if(v.ok()){bool b=true;if(v.set("bVisible",b))v.run(pawn);}
    Call h("/Script/Engine.Actor.SetActorHiddenInGame");
    if(h.ok()){bool b=false;if(h.set("bNewHidden",b))h.run(pawn);}
}
static void SyncLocker(){
    if(!S.controller||!S.pawn)return;
    int loOff=S.controller->Off("CosmeticLoadoutPC");
    if(loOff<0)return;
    UObject* lo=(UObject*)((char*)S.controller+loOff);
    UObject* ch=GetObj(lo,"Character");
    UObject* pk=GetObj(lo,"Pickaxe");
    UObject* st=GetObj(S.controller,"PlayerState");
    if(ch&&st){
        UObject* heroDef=GetObj(ch,"HeroDefinition");
        if(heroDef&&!Bad(heroDef)){
            int htOff=st->Off("HeroType");
            if(htOff>0){
                Wr<UObject*>(st,(size_t)htOff,heroDef);
                Call r("/Script/FortniteGame.FortPlayerState.OnRep_HeroType");
                if(r.ok())r.run(st);
            }
        }
    }
    if(pk)LOG("Locker pickaxe=%s",pk->NS().c_str());
    if(ch)LOG("Locker char=%s",ch->NS().c_str());
}

// === ABILITIES ===
static bool ApplyAbilitySet(UObject* pawn,UObject* set){
    if(!pawn||!set||Bad(set))return false;
    UObject* klib=FindO("/Script/FortniteGame.Default__FortKismetLibrary");
    if(!klib)return false;
    Call c("/Script/FortniteGame.FortKismetLibrary.EquipFortAbilitySet");
    if(!c.ok())return false;
    UObject* ifaceT=FindO("/Script/GameplayAbilities.AbilitySystemInterface");
    if(!ifaceT)return false;
    void* ia=Iface(pawn,ifaceT);
    if(!ia){
        UObject* st=GetObj(pawn,"PlayerState");
        if(st)ia=Iface(st,ifaceT);
    }
    struct II{UObject* O;void* I;};
    II ii{pawn,ia};
    if(!c.set("AbilitySystemInterfaceActor",ii))return false;
    if(!c.set("AbilitySet",set))return false;
    if(!c.set("OverrideSourceObject",pawn))return false;
    if(!c.run(klib))return false;
    LOG("AbilitySet applied %s",set->NS().c_str());
    return true;
}
static void InitStamina(){
    if(!S.pawn)return;
    UObject* asc=GetASC(S.pawn);
    if(!asc)return;
    UObject* attr=FindO("/Script/FortniteGame.FortPlayerAttributeSet");
    if(!attr)attr=FindO("/Script/GameplayAbilities.AttributeSet");
    if(!attr)return;
    Call c("/Script/GameplayAbilities.AbilitySystemComponent.K2_InitStats");
    if(c.ok()&&c.set("Attributes",attr)&&c.run(asc))LOG("Stats init");
}
static void GrantAbilities(){
    if(!S.pawn||S.ability)return;
    InitStamina();
    UObject* core=LoadA("/Game/Abilities/Player/Generic/Traits/DefaultPlayer/GAS_AthenaPlayer.GAS_AthenaPlayer","/Script/FortniteGame.FortAbilitySet");
    if(!core){
        LOG("Core ability set missing");
        S.ability=true;
        return;
    }
    if(ApplyAbilitySet(S.pawn,core))S.sets.push_back(core);
    const char* extra[]={
        "/TacticalSprintGame/Gameplay/AS_TacticalSprint.AS_TacticalSprint",
        "/Ascender/Gameplay/Ascender/AS_Ascender.AS_Ascender",
        "/DoorBashContent/Gameplay/AS_DoorBash.AS_DoorBash",
        "/HillScramble/Gameplay/AS_HillScramble.AS_HillScramble",
        "/SlideImpulse/Gameplay/AS_SlideImpulse.AS_SlideImpulse",
        "/RealitySeedGameplay/Environment/Foliage/GAS_Athena_RealitySapling.GAS_Athena_RealitySapling",
        nullptr
    };
    for(int i=0;extra[i];++i){
        UObject* s=LoadA(extra[i],"/Script/FortniteGame.FortAbilitySet");
        if(s&&ApplyAbilitySet(S.pawn,s))S.sets.push_back(s);
    }
    if(S.state){
        UObject* pl=GetObj(S.state,"CurrentPlaylistData");
        if(!pl){
            UObject* info=GetObj(S.state,"CurrentPlaylistInfo");
            if(info)pl=GetObj(info,"BasePlaylist");
        }
        if(pl&&!Bad(pl)){
            int modOff=pl->Off("ModifierList");
            if(modOff>0){
                int stride=-1;
                UClass* k=pl->Class();
                for(int d=0;d<64&&k&&!Bad(k);++d){
                    UField* c=k->ChildProps();
                    for(int i=0;c&&!Bad(c)&&i<2048;++i,c=c->Next()){
                        std::string cn;N2S((char*)c+Fd_Name,cn);
                        if(cn=="ModifierList"){stride=Rd<int32>(c,P_ElemSize);break;}
                    }
                    if(stride>0)break;
                    k=(UClass*)k->Super();
                }
                if(stride>=8&&stride<=128){
                    TArray<uint8> mods=Rd<TArray<uint8>>(pl,(size_t)modOff);
                    UObject* klib=FindO("/Script/Engine.Default__KismetSystemLibrary");
                    for(int i=0;i<mods.Num&&mods.Data;++i){
                        Call l("/Script/Engine.KismetSystemLibrary.LoadAsset_Blocking");
                        if(!l.ok())continue;
                        if(l.setB("Asset",mods.Data+(size_t)i*stride,stride)&&l.run(klib)){
                            UObject* mod=l.res<UObject*>();
                            if(!mod||Bad(mod))continue;
                            int pasOff=mod->Off("PersistentAbilitySets");
                            if(pasOff<0)continue;
                            TArray<uint8> dls=Rd<TArray<uint8>>(mod,(size_t)pasOff);
                            int dstride=-1;
                            UClass* dk=mod->Class();
                            for(int dd=0;dd<64&&dk&&!Bad(dk);++dd){
                                UField* cc=dk->ChildProps();
                                for(int ii=0;cc&&!Bad(cc)&&ii<2048;++ii,cc=cc->Next()){
                                    std::string cn2;N2S((char*)cc+Fd_Name,cn2);
                                    if(cn2=="PersistentAbilitySets"){dstride=Rd<int32>(cc,P_ElemSize);break;}
                                }
                                if(dstride>0)break;
                                dk=(UClass*)dk->Super();
                            }
                            (void)dstride;
                        }
                    }
                }
            }
        }
    }
    S.ability=true;
    LOG("Abilities complete: %zu sets",S.sets.size());
}

// === INIT PLAYER ===
static bool InitPlayer(UObject* w,UObject* m,UObject* pc){
    if(!g_Stand||!g_Stand(w))return false;
    UObject* pcls=LoadC("/Game/Athena/PlayerPawn_Athena.PlayerPawn_Athena_C","/Script/Engine.Pawn");
    if(!pcls)return false;
    UObject* ex=GetPawn(pc);
    if(ex&&!Bad(ex)&&ex->IsA((UClass*)pcls)){
        S.pawn=ex;
        return true;
    }
    FTransform t{};
    t.T.X=0;t.T.Y=0;t.T.Z=500;
    Call ch("/Script/Engine.GameModeBase.ChoosePlayerStart");
    if(ch.ok()&&ch.set("Player",pc)&&ch.run(m)){
        UObject* st=ch.res<UObject*>();
        if(st&&!Bad(st)){
            FVector loc{};
            if(ActorLoc(st,loc)){
                t.T.X=loc.X;t.T.Y=loc.Y;t.T.Z=loc.Z+100;
            }
        }
    }
    S.pawn=SpawnActor(w,pcls,pc,t);
    if(!S.pawn)return false;
    SetBool(S.pawn,"bIsSkydiving",false);
    SetBool(S.pawn,"bIsParachuteOpen",false);
    Possess(pc,S.pawn);
    if(GetPawn(pc)!=S.pawn)return false;
    for(const char* mn:{"/Script/FortniteGame.FortPawn.SetMaxHealth","/Script/FortniteGame.FortPawn.SetHealth"}){
        Call c(mn);
        if(c.ok()&&c.setAt(0,100.0f))c.run(S.pawn);
    }
    Call sd("/Script/FortniteGame.FortPlayerPawn.OnRep_IsSkydiving");
    if(sd.ok())sd.run(S.pawn);
    Call po("/Script/FortniteGame.FortPlayerPawn.OnRep_IsParachuteOpen");
    if(po.ok())po.run(S.pawn);
    Call st("/Script/FortniteGame.FortPlayerController.ServerReadyToStartMatch");
    if(st.ok())st.run(pc);
    Call cps("/Script/FortniteGame.FortPlayerControllerZone.ClientOnPawnSpawned");
    if(cps.ok())cps.run(pc);
    Call rs("/Script/Engine.PlayerController.ClientRestart");
    if(rs.ok()&&rs.set("NewPawn",S.pawn))rs.run(pc);
    ApplySkin(S.pawn);
    return true;
}

// === SETUP CHEATS ===
static void SetupCheats(){
    if(!S.controller)return;
    int off=S.controller->Off("CheatManager");
    if(off<0)return;
    UObject* cm=Rd<UObject*>(S.controller,(size_t)off);
    if(!cm||Bad(cm)){
        UObject* k=FindC("/Script/FortniteGame.FortCheatManager");
        if(!k)k=FindC("/Script/Engine.CheatManager");
        if(k){
            FTransform t{};
            cm=SpawnActor(S.world,k,S.controller,t);
            if(cm)Wr<UObject*>(S.controller,(size_t)off,cm);
        }
    }
    if(cm){
        Call init("/Script/Engine.CheatManager.ReceiveInitCheatManager");
        if(init.ok())init.run(cm);
    }
}

// === SPAWN LOADOUT ===
static void SpawnDefaultLoadout(){
    UObject* pick=LoadA("/Game/Athena/Items/Weapons/WID_Harvest_Pickaxe_Athena_C_T01.WID_Harvest_Pickaxe_Athena_C_T01","/Script/FortniteGame.FortItemDefinition");
    if(pick)GiveItem(pick,1,0,0,false);
    UObject* rifle=LoadA("/Game/Athena/Items/Weapons/WID_Assault_Auto_Athena_C_Ore_T03.WID_Assault_Auto_Athena_C_Ore_T03","/Script/FortniteGame.FortItemDefinition");
    if(rifle)GiveItem(rifle,1,1,0,false);
    const char* tools[]={
        "/Game/Items/Weapons/BuildingTools/BuildingItemData_Wall.BuildingItemData_Wall",
        "/Game/Items/Weapons/BuildingTools/BuildingItemData_Floor.BuildingItemData_Floor",
        "/Game/Items/Weapons/BuildingTools/BuildingItemData_Stair_W.BuildingItemData_Stair_W",
        "/Game/Items/Weapons/BuildingTools/BuildingItemData_RoofS.BuildingItemData_RoofS",
        nullptr
    };
    for(int i=0;tools[i];++i){
        UObject* t=LoadA(tools[i],"/Script/FortniteGame.FortItemDefinition");
        if(t)GiveItem(t,1,i,1,false);
    }
    S.editTool=LoadA("/Game/Items/Weapons/BuildingTools/EditTool.EditTool","/Script/FortniteGame.FortItemDefinition");
    if(S.editTool)GiveItem(S.editTool,1,4,1,false);
    const char* ammo[]={
        "/Game/Athena/Items/Ammo/AthenaAmmoDataBulletsLight.AthenaAmmoDataBulletsLight",
        "/Game/Athena/Items/Ammo/AthenaAmmoDataBulletsMedium.AthenaAmmoDataBulletsMedium",
        "/Game/Athena/Items/Ammo/AthenaAmmoDataBulletsHeavy.AthenaAmmoDataBulletsHeavy",
        "/Game/Athena/Items/Ammo/AthenaAmmoDataShells.AthenaAmmoDataShells",
        nullptr
    };
    for(int i=0;ammo[i];++i){
        UObject* a=LoadA(ammo[i],"/Script/FortniteGame.FortItemDefinition");
        if(a)GiveItem(a,999,-1,0,false);
    }
    UObject* wood=LoadA("/Game/Items/ResourcePickups/WoodItemData.WoodItemData","/Script/FortniteGame.FortItemDefinition");
    if(wood)GiveItem(wood,500,-1,0,false);
    UObject* stone=LoadA("/Game/Items/ResourcePickups/StoneItemData.StoneItemData","/Script/FortniteGame.FortItemDefinition");
    if(stone)GiveItem(stone,500,-1,0,false);
    UObject* metal=LoadA("/Game/Items/ResourcePickups/MetalItemData.MetalItemData","/Script/FortniteGame.FortItemDefinition");
    if(metal)GiveItem(metal,500,-1,0,false);
}

// === SETUP WORLD ===
static void SetupWorld(UObject* w){
    S.world=w;
    S.controller=GetPC(w,0);
    if(!S.controller)return;
    UObject* base=FindO("/Script/FortniteGame.FortPlayerController");
    if(!base||!S.controller->IsA((UClass*)base))return;
    int moOff=w->Off("AuthorityGameMode");
    int stOff=w->Off("GameState");
    if(moOff>0)S.mode=Rd<UObject*>(w,(size_t)moOff);
    if(stOff>0)S.state=Rd<UObject*>(w,(size_t)stOff);
    if(!S.mode||!S.state)return;
    LOG("Setup PC=%s GM=%s GS=%s",S.controller->NS().c_str(),S.mode->NS().c_str(),S.state->NS().c_str());
    if(!InitPlayer(w,S.mode,S.controller)){
        LOG("InitPlayer failed");
        return;
    }
    SetupCheats();
    SyncLocker();
    int qbOff=S.controller->Off("ClientQuickBars");
    if(qbOff>0){
        S.quickbars=Rd<UObject*>(S.controller,(size_t)qbOff);
        if(!S.quickbars||Bad(S.quickbars)){
            UObject* k=FindC("/Script/FortniteGame.FortQuickBarsAthena");
            if(!k)k=FindC("/Script/FortniteGame.FortQuickBars");
            if(k){
                FTransform t{};
                S.quickbars=SpawnActor(w,k,S.controller,t);
                if(S.quickbars)Wr<UObject*>(S.controller,(size_t)qbOff,S.quickbars);
            }
        }
    }
    int invOff=S.controller->Off("WorldInventory");
    if(invOff>0){
        S.inventory=Rd<UObject*>(S.controller,(size_t)invOff);
        if(!S.inventory||Bad(S.inventory)){
            UObject* k=FindC("/Script/FortniteGame.FortInventory");
            if(k){
                FTransform t{};
                S.inventory=SpawnActor(w,k,S.controller,t);
                if(S.inventory)Wr<UObject*>(S.controller,(size_t)invOff,S.inventory);
            }
        }
    }
    if(S.inventory){
        Call so("/Script/FortniteGame.FortInventory.SetOwner");
        if(so.ok())so.run(S.inventory);
    }
    SpawnDefaultLoadout();
    InvUpdate();
    BuildQB();
    SetQBReady();
    EquipSlot(0);
    GrantAbilities();
    SetBool(S.controller,"bBuildFree",true);
    int rb=S.controller->Off("bReadyToStartMatch");
    if(rb>0){
        uint8* p=(uint8*)S.controller+rb;
        *p|=(1u<<5);
    }
    S.finalizeAt=Now()+2.0;
    S.attempt=true;
    LOG("SetupWorld done");
}

// === FINALIZE ===
static void Finalize(){
    if(S.finalized)return;
    S.finalized=true;
    LOG("Finalize");
    if(!S.controller)return;
    UObject* p=GetPawn(S.controller);
    if(!p||Bad(p)){
        LOG("No pawn at finalize");
        return;
    }
    S.pawn=p;
    Call ps("/Script/Engine.Pawn.OnRep_PlayerState");
    if(ps.ok())ps.run(p);
    if(!S.ability)GrantAbilities();
    if(!S.qb){
        BuildQB();
        SetQBReady();
    }
    EquipSlot(0);
    InvUpdate();
    for(const char* mn:{"/Script/FortniteGame.FortPawn.SetMaxHealth","/Script/FortniteGame.FortPawn.SetHealth"}){
        Call c(mn);
        if(c.ok()&&c.setAt(0,100.0f))c.run(S.pawn);
    }
    Call rs("/Script/Engine.PlayerController.ClientRestart");
    if(rs.ok()&&rs.set("NewPawn",S.pawn))rs.run(S.controller);
    Call cam("/Script/Engine.PlayerController.SetViewTargetWithBlend");
    if(cam.ok())cam.set("NewViewTarget",S.pawn);
    ApplySkin(S.pawn);
    S.visibility=true;
    LOG("Finalize complete");
}
// ==== PART 3: BR LIFECYCLE / BOTS / WILDLIFE / VEHICLES / INTERACTION / HOOKS ====

// === BR STATE ===
namespace BR {
    bool started=false;
    uint8 phase=0,step=0;
    UObject* indicator=nullptr;
    UObject* mapinfo=nullptr;
    UObject* aircraft=nullptr;
    float warmupEnd=-1,safeStart=-1,dropStart=-1,dropEnd=-1,flightEnd=-1;
    bool busUnlock=false,dropForced=false,flightDone=false,zoneFormed=false,zoneShrink=false;
    int zoneCount=9;
    std::vector<float> rad,wait,shrink,dmg;
    std::vector<FVector> centers;
    std::vector<UObject*> botPawns;
    double nextWave=0;
    int lastLogged=-1;
    bool aliveAdded=false;
    bool haveRoute=false;
    FVector routeStart{};
    float routeYaw=0,routeSpeed=4500,routeDur=60;
    float flightStart=-1;
    bool boarded=false;
    double jumpAt=-1;
    UObject* oldPawn=nullptr;
    bool spectating=false;
    int spectIdx=0;
}

// === SET PHASE ===
static void SetPhase(uint8 p){
    uint8 old=BR::phase;
    if(S.state){
        int gp=S.state->Off("GamePhase");
        if(gp>0)Wr<uint8>(S.state,(size_t)gp,p);
        Call n("/Script/FortniteGame.FortGameStateAthena.OnRep_GamePhase");
        if(n.ok()){
            n.setAt(0,old);
            n.run(S.state);
        }
    }
    BR::phase=p;
    LOG("Phase %u",(unsigned)p);
}
static void SetStep(uint8 s){
    BR::step=s;
    if(S.state){
        int sp=S.state->Off("GamePhaseStep");
        if(sp>0)Wr<uint8>(S.state,(size_t)sp,s);
    }
    UObject* sig=FindO("/Script/FortniteGame.OnGamePhaseStepChanged__DelegateSignature");
    if(sig&&S.state){
        uint8 prm[32]{};
        prm[0x10]=s;
        G g;
        g_PE(S.state,(UFunction*)sig,prm);
    }
    LOG("Step %u",(unsigned)s);
}

// === ZONE TABLES ===
static void ZoneTables(){
    static const float kR[9]={25600,19800,14600,10600,7600,5400,3600,2200,1000};
    static const float kW[9]={50,40,35,30,25,20,20,15,10};
    static const float kS[9]={40,35,32,28,25,21,17,13,10};
    static const float kD[9]={1,1,1,2,2,5,6,8,10};
    BR::zoneCount=9;
    BR::rad.assign(kR,kR+9);
    BR::wait.assign(kW,kW+9);
    BR::shrink.assign(kS,kS+9);
    BR::dmg.assign(kD,kD+9);
    LOG("Zone tables ready");
}
static void PickCenters(){
    BR::centers.clear();
    FVector c{0,0,0};
    if(BR::mapinfo){
        Call g("/Script/FortniteGame.FortAthenaMapInfo.GetMapCenter");
        if(g.ok()&&g.run(BR::mapinfo))c=g.res<FVector>();
    }
    if(!std::isfinite(c.X)||!std::isfinite(c.Y))c=FVector{0,0,0};
    BR::centers.push_back(c);
    for(int i=1;i<BR::zoneCount;++i){
        float gap=BR::rad[i-1]-BR::rad[i];
        if(gap<500)gap=500;
        float d=gap*(0.20f+0.50f*R01());
        float a=R01()*6.2831853f;
        BR::centers.push_back(FVector{c.X+std::cos(a)*d,c.Y+std::sin(a)*d,c.Z});
    }
    LOG("Centers picked");
}

// === SAFE ZONE INDICATOR ===
static void SetupIndicator(){
    if(BR::indicator&&!Bad(BR::indicator))return;
    if(!S.state||!S.world)return;
    UObject* ind=GetObj(S.state,"SafeZoneIndicator");
    if(!ind||Bad(ind)){
        UObject* cls=FindO("/Script/FortniteGame.FortSafeZoneIndicator");
        if(!cls)return;
        FTransform t{};
        ind=SpawnActor(S.world,cls,S.state,t);
        if(!ind)return;
    }
    BR::indicator=ind;
    int arrOff=ind->Off("SafeZonePhases");
    if(arrOff<0)return;
    UObject* pc=FindO("/Script/FortniteGame.Default__FortSafeZonePhaseInfo");
    if(!pc)return;
    int rOff=pc->Off("Radius");
    int wOff=pc->Off("WaitTime");
    int sOff=pc->Off("ShrinkTime");
    int cOff=pc->Off("Center");
    int dOff=pc->Off("DamageInfo");
    UObject* dcdo=FindO("/Script/FortniteGame.Default__FortSafeZoneDamageInfo");
    int dmgOff=dcdo?dcdo->Off("Damage"):-1;
    int pctOff=dcdo?dcdo->Off("bPercentageBasedDamage"):-1;
    if(rOff<0||wOff<0||sOff<0||cOff<0)return;
    int esz=0x60;
    std::vector<uint8> e((size_t)esz,0);
    for(int i=0;i<BR::zoneCount;++i){
        std::fill(e.begin(),e.end(),0);
        Wr<float>(e.data(),(size_t)rOff,BR::rad[i]);
        Wr<float>(e.data(),(size_t)wOff,BR::wait[i]);
        Wr<float>(e.data(),(size_t)sOff,BR::shrink[i]);
        Wr<FVector>(e.data(),(size_t)cOff,BR::centers[i]);
        if(dOff>0&&dmgOff>0)Wr<float>(e.data(),(size_t)(dOff+dmgOff),BR::dmg[i]);
        if(dOff>0&&pctOff>0)Wr<uint8>(e.data(),(size_t)(dOff+pctOff),1);
        if(g_Add)g_Add((char*)ind+arrOff,(void*)((char*)ind+arrOff),e.data());
    }
    int pcOff=ind->Off("PhaseCount");
    if(pcOff>0)Wr<int32>(ind,(size_t)pcOff,BR::zoneCount);
    Call rpc("/Script/FortniteGame.FortSafeZoneIndicator.OnRep_PhaseCount");
    if(rpc.ok())rpc.run(ind);
    float now=WT();
    int ss=ind->Off("SafeZoneStartShrinkTime");
    int fs=ind->Off("SafeZoneFinishShrinkTime");
    if(ss>0)Wr<float>(ind,(size_t)ss,now+BR::wait[0]);
    if(fs>0)Wr<float>(ind,(size_t)fs,now+BR::wait[0]+BR::shrink[0]);
    int cp=ind->Off("CurrentPhase");
    if(cp>0)Wr<int32>(ind,(size_t)cp,0);
    Call rcp("/Script/FortniteGame.FortSafeZoneIndicator.OnRep_CurrentPhase");
    if(rcp.ok())rcp.run(ind);
    int stp=S.state->Off("SafeZoneIndicator");
    if(stp>0)Wr<UObject*>(S.state,(size_t)stp,ind);
    Call rs("/Script/FortniteGame.FortGameStateAthena.OnRep_SafeZoneIndicator");
    if(rs.ok())rs.run(S.state);
    LOG("Indicator ready");
}

static void NewZonePhase(int p){
    if(!BR::indicator||Bad(BR::indicator))return;
    if(p<0||p>=BR::zoneCount)return;
    float now=WT();
    if(p>0){
        int off=BR::indicator->Off("PreviousCenter");
        if(off>0)Wr<FVector>(BR::indicator,(size_t)off,BR::centers[p-1]);
        int r=BR::indicator->Off("PreviousRadius");
        if(r>0)Wr<float>(BR::indicator,(size_t)r,BR::rad[p-1]);
    }
    int nc=BR::indicator->Off("NextCenter");
    int nr=BR::indicator->Off("NextRadius");
    if(nc>0)Wr<FVector>(BR::indicator,(size_t)nc,BR::centers[p]);
    if(nr>0)Wr<float>(BR::indicator,(size_t)nr,BR::rad[p]);
    if(p+1<BR::zoneCount){
        int nnc=BR::indicator->Off("NextNextCenter");
        int nnr=BR::indicator->Off("NextNextRadius");
        if(nnc>0)Wr<FVector>(BR::indicator,(size_t)nnc,BR::centers[p+1]);
        if(nnr>0)Wr<float>(BR::indicator,(size_t)nnr,BR::rad[p+1]);
    }
    float start=now+BR::wait[p];
    int ss=BR::indicator->Off("SafeZoneStartShrinkTime");
    int fs=BR::indicator->Off("SafeZoneFinishShrinkTime");
    if(ss>0)Wr<float>(BR::indicator,(size_t)ss,start);
    if(fs>0)Wr<float>(BR::indicator,(size_t)fs,start+BR::shrink[p]);
    int cp=BR::indicator->Off("CurrentPhase");
    if(cp>0)Wr<int32>(BR::indicator,(size_t)cp,p);
    Call rcp("/Script/FortniteGame.FortSafeZoneIndicator.OnRep_CurrentPhase");
    if(rcp.ok())rcp.run(BR::indicator);
    Call osc("/Script/FortniteGame.FortSafeZoneIndicator.OnSafeZoneStateChange");
    if(osc.ok()){
        osc.setAt(0,(uint8)2);
        osc.setAt(1,(uint8)(p==0?1:0));
        osc.run(BR::indicator);
    }
    SetStep(7);
    BR::lastLogged=-1;
    LOG("Zone phase %d",p);
}

// === AIRCRAFT ===
static void BuildFlightInfo(uint8 out[0x48],FVector& start,float& yaw,float& spd,float& dur){
    FVector c{0,0,0};
    float r=150000;
    if(BR::mapinfo){
        int off=BR::mapinfo->Off("CachedPlayableBoundsForClients");
        if(off>0){
            FVector o=Rd<FVector>(BR::mapinfo,(size_t)off);
            if(std::isfinite(o.X)&&std::fabs(o.X)<1e7)c=o;
        }
    }
    float h=10500;
    spd=4500;
    if(BR::mapinfo){
        int a=BR::mapinfo->Off("AircraftHeight");
        if(a>0){float v=Rd<float>(BR::mapinfo,(size_t)a);if(v>100&&v<100000)h=v;}
        int b=BR::mapinfo->Off("AircraftSpeed");
        if(b>0){float v=Rd<float>(BR::mapinfo,(size_t)b);if(v>2000&&v<9000)spd=v;}
    }
    float ang=R01()*6.2831853f;
    float half=r*0.85f;
    start=FVector{c.X-std::cos(ang)*half,c.Y-std::sin(ang)*half,h};
    yaw=(float)(std::atan2(std::sin(ang),std::cos(ang))*57.2957795);
    float dist=2.0f*half;
    dur=dist/spd;
    if(dur<45){dur=45;spd=dist/dur;}
    if(dur>95){dur=95;spd=dist/dur;}
    memset(out,0,0x48);
    memcpy(out+0x00,&start,24);
    FRotator rr{};
    rr.Pitch=-6;
    rr.Yaw=yaw;
    memcpy(out+0x18,&rr,24);
    memcpy(out+0x30,&spd,4);
    float tfe=dur,tds=dur*0.08f,tde=dur*0.88f;
    memcpy(out+0x34,&tfe,4);
    memcpy(out+0x38,&tds,4);
    memcpy(out+0x3C,&tde,4);
    out[0x40]=1;
}

static void StartAircraftPhase(){
    if(BR::phase>=3)return;
    if(!S.world||!S.state)return;
    float now=WT();
    LOG("StartAircraft t=%.0f",now);
    UObject* acCls=BR::mapinfo?GetObj(BR::mapinfo,"AircraftClass"):nullptr;
    if(!acCls||Bad(acCls))acCls=FindO("/Script/FortniteGame.FortAthenaAircraft");
    if(!acCls)return;
    int fiOff=BR::mapinfo?BR::mapinfo->Off("FlightInfos"):-1;
    bool haveFI=false;
    uint8 fiBuf[0x48]{};
    if(fiOff>0){
        TArray<uint8> fi=Rd<TArray<uint8>>(BR::mapinfo,(size_t)fiOff);
        if(fi.Num>0&&fi.Data)haveFI=true;
    }
    if(!haveFI){
        FVector rs{};
        float ry=0,rsp=4500,rd=60;
        BuildFlightInfo(fiBuf,rs,ry,rsp,rd);
        if(fiOff>0&&BR::mapinfo&&g_Add)
            g_Add((char*)BR::mapinfo+fiOff,(void*)((char*)BR::mapinfo+fiOff),fiBuf);
        BR::haveRoute=true;
        BR::routeStart=rs;
        BR::routeYaw=ry;
        BR::routeSpeed=rsp;
        BR::routeDur=rd;
        BR::dropStart=now+rd*0.08f;
        BR::dropEnd=now+rd*0.88f;
        BR::flightEnd=now+rd;
        LOG("Flight path synthesized");
    }
    UObject* aircraft=nullptr;
    int ac1Off=S.state->Off("Aircraft");
    if(ac1Off>0){
        UObject* ex=Rd<UObject*>(S.state,(size_t)ac1Off);
        if(ex&&!Bad(ex))aircraft=ex;
    }
    if(!aircraft){
        FTransform t{};
        if(BR::haveRoute){
            t.T=BR::routeStart;
            FRotator rr{};
            rr.Pitch=-6;
            rr.Yaw=BR::routeYaw;
            double q[4];
            RotToQ(rr,q);
            t.R.Q.X=q[0];
            t.R.Q.Y=q[1];
            t.R.Q.Z=q[2];
            t.R.Q.W=q[3];
        }
        aircraft=SpawnActor(S.world,acCls,S.state,t);
    }
    if(!aircraft)return;
    BR::aircraft=aircraft;
    if(BR::dropStart<0){
        BR::dropStart=now+5;
        BR::dropEnd=now+50;
        BR::flightEnd=now+55;
    }
    Call sfp("/Script/FortniteGame.FortAthenaAircraft.StartFlightPath");
    if(sfp.ok()){
        float st=now;
        if(sfp.set("StartTime",st))sfp.run(aircraft);
    }
    int acsOff=S.state->Off("Aircrafts");
    if(acsOff>0&&g_Add)
        g_Add((char*)S.state+acsOff,(void*)((char*)S.state+acsOff),&aircraft);
    Call ra("/Script/FortniteGame.FortGameStateAthena.OnRep_Aircrafts");
    if(ra.ok())ra.run(S.state);
    if(ac1Off>0)Wr<UObject*>(S.state,(size_t)ac1Off,aircraft);
    BR::flightStart=now;
    if(S.pawn&&!Bad(S.pawn)){
        Call oe("/Script/FortniteGame.FortPlayerPawn.OnEnteredAircraft");
        if(oe.ok())oe.run(S.pawn);
        Call up("/Script/Engine.Controller.UnPossess");
        if(up.ok())up.run(S.controller);
        BR::oldPawn=S.pawn;
        Call d("/Script/Engine.Actor.K2_DestroyActor");
        if(d.ok())d.run(S.pawn);
        S.pawn=nullptr;
        Call cam("/Script/Engine.PlayerController.SetViewTargetWithBlend");
        if(cam.ok()){
            cam.set("NewViewTarget",aircraft);
            float b=0;
            cam.set("BlendTime",b);
            cam.run(S.controller);
        }
    }
    SetPhase(3);
    SetStep(4);
}

// === BR JUMP ===
static void BRJump(){
    if(!S.controller)return;
    if(BR::spectating)return;
    if(S.aircraftJumped)return;
    LOG("BRJump");
    UObject* cmpc=nullptr;
    if(g_airCmp){
        Call g("/Script/Engine.Actor.GetComponentByClass");
        if(g.ok()&&g.set("ComponentClass",g_airCmp)&&g.run(S.controller))cmpc=g.res<UObject*>();
    }
    bool fired=false;
    if(cmpc&&!Bad(cmpc)){
        Call ac("/Script/FortniteGame.FortControllerComponent_Aircraft.ServerAttemptAircraftJump");
        if(ac.ok()){
            FRotator r{};
            if(ac.set("ClientRotation",r)&&ac.run(cmpc))fired=true;
        }
    }
    if(!fired){
        Call pc("/Script/FortniteGame.FortPlayerControllerAthena.ServerAttemptAircraftJump");
        if(pc.ok()){
            FRotator r{};
            if(pc.set("ClientRotation",r)&&pc.run(S.controller))fired=true;
        }
    }
    S.aircraftJumped=true;
    BR::jumpAt=Now();
    LOG("Jump fired=%d",fired);
}

// === BOT POOLS ===
static void BuildPools(){
    if(S.poolReady)return;
    if(!g_go)return;
    UObject* ccp=FindO("/Script/FortniteGame.CustomCharacterPart");
    if(!ccp)return;
    for(int32 i=0;i<g_go->Num;++i){
        if(S.heads.size()>=64&&S.bodies.size()>=64)break;
        UObject* o=g_go->At((uint32)i);
        if(!o||Bad(o)||o->IsCDO())continue;
        if(o->Class()!=(UClass*)ccp)continue;
        std::string n=o->NS();
        if(n.find("Empty")!=std::string::npos)continue;
        if(n.find("Head")!=std::string::npos&&S.heads.size()<64)S.heads.push_back(o);
        else if(n.find("Body")!=std::string::npos&&S.bodies.size()<64)S.bodies.push_back(o);
    }
    S.poolReady=true;
    LOG("Pools: heads=%zu bodies=%zu",S.heads.size(),S.bodies.size());
}

// === BOT SPAWN ===
static void SpawnBotWave(int n){
    if(!S.world||!S.mode||!S.state)return;
    if(!S.botPawn)S.botPawn=LoadC("/Game/Athena/PlayerPawn_Athena.PlayerPawn_Athena_C","/Script/Engine.Pawn");
    if(!S.botCtrl)S.botCtrl=LoadC("/Game/Athena/Athena_PlayerController.Athena_PlayerController_C","/Script/FortniteGame.FortPlayerControllerAthena");
    if(!S.botState)S.botState=FindO("/Script/FortniteGame.FortPlayerStateAthena");
    if(!S.botPawn||!S.botCtrl||!S.botState){
        LOG("Bot classes missing");
        BR::botTarget=BR::botPawns.size();
        return;
    }
    if(!S.botHero)S.botHero=LoadA("/Game/Athena/Heroes/HID_001_Athena_Commando_F.HID_001_Athena_Commando_F","/Script/FortniteGame.FortHeroType");
    BuildPools();
    int aliveOff=S.mode->Off("AlivePlayers");
    int spawned=0;
    for(int k=0;k<n&&(int)BR::botPawns.size()<BR::botTarget;++k){
        FVector loc{};
        bool haveLoc=false;
        if(S.pawn&&ActorLoc(S.pawn,loc)){
            loc.Z+=300;
            haveLoc=true;
        }
        if(!haveLoc){
            FVector c{0,0,0};
            if(!BR::centers.empty())c=BR::centers[0];
            float ang=R01()*6.2831853f,dist=2000+R01()*60000;
            loc=FVector{c.X+std::cos(ang)*dist,c.Y+std::sin(ang)*dist,c.Z+1000};
        }
        FTransform t{};
        t.T.X=loc.X;
        t.T.Y=loc.Y;
        t.T.Z=loc.Z;
        UObject* pawn=SpawnActor(S.world,S.botPawn,nullptr,t);
        UObject* ctrl=pawn?SpawnActor(S.world,S.botCtrl,nullptr,t):nullptr;
        UObject* st=ctrl?SpawnActor(S.world,S.botState,nullptr,t):nullptr;
        if(!pawn||!ctrl||!st){
            if(pawn){Call d("/Script/Engine.Actor.K2_DestroyActor");d.run(pawn);}
            if(ctrl){Call d("/Script/Engine.Actor.K2_DestroyActor");d.run(ctrl);}
            if(st){Call d("/Script/Engine.Actor.K2_DestroyActor");d.run(st);}
            break;
        }
        Call so("/Script/Engine.Actor.SetOwner");
        if(so.ok()&&so.set("Owner",ctrl))so.run(st);
        int psOff=ctrl->Off("PlayerState");
        if(psOff>0)Wr<UObject*>(ctrl,(size_t)psOff,st);
        Call orc("/Script/Engine.PlayerController.OnRep_PlayerState");
        if(orc.ok())orc.run(ctrl);
        int ppOff=pawn->Off("PlayerState");
        if(ppOff>0)Wr<UObject*>(pawn,(size_t)ppOff,st);
        Call orp("/Script/Engine.Pawn.OnRep_PlayerState");
        if(orp.ok())orp.run(pawn);
        Possess(ctrl,pawn);
        int mfOff=ctrl->Off("MyFortPawn");
        if(mfOff>0)Wr<UObject*>(ctrl,(size_t)mfOff,pawn);
        for(const char* mn:{"/Script/FortniteGame.FortPawn.SetMaxHealth","/Script/FortniteGame.FortPawn.SetHealth"}){
            Call m(mn);
            if(m.ok()&&m.setAt(0,100.0f))m.run(pawn);
        }
        int tiOff=st->Off("TeamIndex");
        if(tiOff>0)Wr<uint8>(st,(size_t)tiOff,(uint8)(3+BR::botPawns.size()));
        int sqOff=st->Off("SquadId");
        if(sqOff>0)Wr<uint8>(st,(size_t)sqOff,(uint8)BR::botPawns.size());
        SetBool(st,"bIsABot",true);
        UObject* head=nullptr;
        UObject* body=nullptr;
        if(!S.heads.empty())head=S.heads[Rnd()%S.heads.size()];
        if(!S.bodies.empty())body=S.bodies[Rnd()%S.bodies.size()];
        if(head){
            Call ch("/Script/FortniteGame.FortPlayerPawn.ServerChoosePart");
            if(ch.ok()&&ch.set("Part",(uint8)0)&&ch.set("ChosenCharacterPart",head))ch.run(pawn);
        }
        if(body){
            Call ch("/Script/FortniteGame.FortPlayerPawn.ServerChoosePart");
            if(ch.ok()&&ch.set("Part",(uint8)1)&&ch.set("ChosenCharacterPart",body))ch.run(pawn);
        }
        if(head&&body){
            UObject* parts[2]={head,body};
            TArray<UObject*> arr{parts,2,2};
            UObject* klib=FindO("/Script/FortniteGame.Default__FortKismetLibrary");
            if(klib){
                Call ap("/Script/FortniteGame.FortKismetLibrary.ApplyCharacterCosmetics");
                if(ap.ok()&&ap.set("WorldContextObject",S.world)&&ap.set("CharacterParts",arr)&&ap.set("PlayerState",st))ap.run(klib);
            }
        }
        if(S.botHero){
            int htOff=st->Off("HeroType");
            if(htOff>0){
                Wr<UObject*>(st,(size_t)htOff,S.botHero);
                Call rh("/Script/FortniteGame.FortPlayerState.OnRep_HeroType");
                if(rh.ok())rh.run(st);
            }
        }
        BR::botPawns.push_back(pawn);
        g_bots.push_back({pawn,(float)(R01()*360.0),0.0,0,0,0,0,0,0,FVector{},0,0});
        for(UObject* set:S.sets)if(set)ApplyAbilitySet(pawn,set);
        if(aliveOff>0&&g_Add)g_Add((char*)S.mode+aliveOff,(void*)((char*)S.mode+aliveOff),&ctrl);
        ++spawned;
    }
    if(spawned>0){
        int pl=S.state->Off("PlayersLeft");
        if(pl>0){
            Wr<int32>(S.state,(size_t)pl,(int32)(1+BR::botPawns.size()));
            Call r("/Script/FortniteGame.FortGameStateAthena.OnRep_PlayersLeft");
            if(r.ok())r.run(S.state);
        }
        LOG("Bots +%d total=%zu",spawned,BR::botPawns.size());
    }
}

// === MOTOR ===
static void MotorTick(std::vector<Motor>& list,float spd,const char* tag){
    if(list.empty())return;
    double now=Now();
    static Call* smove=nullptr;
    static UObject* aiLib=nullptr;
    static Call* rot=nullptr;
    if(!smove)smove=new Call("/Script/AIModule.AIBlueprintHelperLibrary.SimpleMoveToLocation");
    if(!aiLib)aiLib=FindO("/Script/AIModule.Default__AIBlueprintHelperLibrary");
    if(!rot)rot=new Call("/Script/Engine.Controller.SetControlRotation");
    int ctrlOff=-1;
    int cmOff=-1;
    int velOff=-1;
    if(!list.empty()&&list[0].pawn){
        ctrlOff=list[0].pawn->Off("Controller");
        cmOff=list[0].pawn->Off("CharacterMovement");
        if(cmOff>0){
            UObject* cm=Rd<UObject*>(list[0].pawn,(size_t)cmOff);
            if(cm&&!Bad(cm))velOff=cm->Off("Velocity");
        }
    }
    size_t w=0;
    for(size_t i=0;i<list.size();++i){
        Motor& m=list[i];
        UObject* p=m.pawn;
        if(!p||Bad(p))continue;
        if(m.busState!=0){
            list[w++]=m;
            continue;
        }
        FVector at{};
        bool haveAt=false;
        if(ActorLoc(p,at))haveAt=true;
        if(now>=m.next){
            m.yaw=(float)(R01()*360.0);
            m.next=now+4.0+R01()*4.0;
            FVector goal{at.X,at.Y,at.Z};
            float wander=(!strcmp(tag,"Wild")?500.0f:1200.0f)+R01()*800.0f;
            float ang=R01()*6.2831853f;
            goal=FVector{at.X+std::cos(ang)*wander,at.Y+std::sin(ang)*wander,at.Z};
            UObject* ctrl=ctrlOff>0?Rd<UObject*>(p,(size_t)ctrlOff):nullptr;
            if(smove&&smove->ok()&&aiLib&&ctrl&&!Bad(ctrl)&&
               smove->set("Controller",ctrl)&&smove->set("Goal",goal)&&smove->run(aiLib)){
                m.vel=0;
                m.lastX=at.X;
                m.lastY=at.Y;
                m.lastCheck=now+2.2;
            }else m.vel=1;
        }
        if(!m.vel&&haveAt&&now>=m.lastCheck){
            double dx=at.X-m.lastX,dy=at.Y-m.lastY;
            if(dx*dx+dy*dy<40.0*40.0){
                m.vel=1;
                m.next=now+1.0+R01();
            }
            m.lastCheck=now+2.2;
            m.lastX=at.X;
            m.lastY=at.Y;
        }
        if(m.vel){
            float rad=m.yaw*3.14159265f/180.0f;
            FVector dir{std::cos(rad),std::sin(rad),0};
            if(cmOff>0&&velOff>0){
                UObject* cm=Rd<UObject*>(p,(size_t)cmOff);
                if(cm&&!Bad(cm)){
                    FVector v=Rd<FVector>(cm,(size_t)velOff);
                    if(std::isfinite(v.Z)){
                        v.X=dir.X*spd;
                        v.Y=dir.Y*spd;
                        Wr<FVector>(cm,(size_t)velOff,v);
                    }
                }
            }
            UObject* ctrl=ctrlOff>0?Rd<UObject*>(p,(size_t)ctrlOff):nullptr;
            if(ctrl&&!Bad(ctrl)&&rot&&rot->ok()){
                FRotator r{};
                r.Yaw=m.yaw;
                if(rot->set("NewRotation",r))rot->run(ctrl);
            }
        }
        list[w++]=m;
    }
    list.resize(w);
}

// === WILDLIFE ===
static void WildPool(std::vector<UObject*>& out){
    UObject* ai=FindO("/Script/FortniteGame.FortAIPawn");
    if(!ai||!g_go)return;
    static const char* pre[]={"irwin","burt","nug","smackie","wildlife",nullptr};
    static const char* skip[]={"parent","base","controller","loot","spawner",nullptr};
    UObject* cc=FindO("/Script/CoreUObject.Class");
    if(!cc)return;
    for(int32 i=0;i<g_go->Num&&out.size()<16;++i){
        UObject* o=g_go->At((uint32)i);
        if(!o||Bad(o)||o==ai)continue;
        if(!o->IsA((UClass*)cc))continue;
        UClass* c=(UClass*)o;
        bool isSub=false;
        for(int d=0;d<64&&c;d++){
            if(c==(UClass*)ai){isSub=true;break;}
            c=(UClass*)c->Super();
        }
        if(!isSub)continue;
        std::string n=o->NS();
        if(n.find("_C")==std::string::npos)continue;
        bool bad=false;
        for(int k=0;skip[k];++k)if(n.find(skip[k])!=std::string::npos){bad=true;break;}
        if(bad)continue;
        bool good=false;
        for(int k=0;pre[k];++k)if(n.find(pre[k])!=std::string::npos){good=true;break;}
        if(!good&&out.size()>=6)continue;
        out.push_back(o);
    }
}
static UObject* SpawnWild(UObject* cls,const FVector& at){
    FTransform t{};
    t.T.X=at.X;
    t.T.Y=at.Y;
    t.T.Z=at.Z;
    UObject* a=SpawnActor(S.world,cls,S.controller,t);
    if(!a)return nullptr;
    UObject* aicl=GetObj(a,"AIControllerClass");
    if(aicl&&!Bad(aicl)&&aicl->IsA((UClass*)FindO("/Script/Engine.AIController"))){
        FTransform ct{};
        ct.T.X=at.X;
        ct.T.Y=at.Y;
        ct.T.Z=at.Z;
        UObject* aic=SpawnActor(S.world,aicl,a,ct);
        if(aic)Possess(aic,a);
    }
    SetBool(a,"bCanBeDamaged",false);
    for(const char* mn:{"/Script/FortniteGame.FortPawn.SetMaxHealth","/Script/FortniteGame.FortPawn.SetHealth"}){
        Call m(mn);
        if(m.ok()&&m.setAt(0,100.0f))m.run(a);
    }
    Call sh("/Script/Engine.Actor.SetActorHiddenInGame");
    if(sh.ok()){bool b=false;if(sh.set("bNewHidden",b))sh.run(a);}
    g_wild.push_back({a,(float)(R01()*360.0),0.0,0,0,0,0,0,0,FVector{},0,0});
    return a;
}
static int SpawnWildlife(int max){
    if(!S.world||!S.pawn||max<1)return 0;
    std::vector<UObject*> cls;
    WildPool(cls);
    if(cls.empty())return 0;
    FVector c{0,0,0};
    bool have=false;
    if(!BR::centers.empty()){
        c=BR::centers[0];
        have=true;
    }else if(S.state){
        UObject* mi=GetObj(S.state,"MapInfo");
        if(mi&&!Bad(mi)){
            Call gc("/Script/FortniteGame.FortAthenaMapInfo.GetMapCenter");
            if(gc.ok()&&gc.run(mi)){
                c=gc.res<FVector>();
                if(std::fabs(c.X)+std::fabs(c.Y)>100)have=true;
            }
        }
    }
    if(!have&&S.pawn)ActorLoc(S.pawn,c);
    int sp=0;
    for(int i=0;i<max;++i){
        float ang=R01()*6.2831853f;
        float dist=12000+R01()*68000;
        FVector at{c.X+std::cos(ang)*dist,c.Y+std::sin(ang)*dist,600};
        float gz=at.Z;
        if(GroundZ(at.X,at.Y,gz))at.Z=gz+80;
        UObject* cl=cls[Rnd()%cls.size()];
        if(SpawnWild(cl,at))++sp;
    }
    LOG("Wildlife spawned %d",sp);
    return sp;
}

// === VEHICLES ===
static int SpawnVehicles(int max){
    if(!S.world||max<1)return 0;
    if(!g_vehC)g_vehC=FindO("/Script/FortniteGame.FortAthenaVehicle");
    if(!g_vehC)return 0;
    static UObject* vCls=nullptr;
    if(!vCls){
        const char* m[]={
            "/Dirtbike/Vehicle/Motorcycle_DirtBike_Vehicle.Motorcycle_DirtBike_Vehicle_C",
            "/Sportbike/Vehicle/Motorcycle_Sport_Vehicle.Motorcycle_Sport_Vehicle_C",
            nullptr
        };
        for(int i=0;m[i];++i){
            vCls=LoadC(m[i],"/Script/FortniteGame.FortAthenaVehicle");
            if(vCls)break;
        }
    }
    if(!vCls)return 0;
    FVector o{0,0,0};
    if(S.pawn)ActorLoc(S.pawn,o);
    int sp=0;
    for(int i=0;i<max;++i){
        FVector at=o;
        at.X+=700.0*((i&1)?1:-1);
        at.Y+=700.0*((i&2)?1:-1);
        at.Z+=150;
        FTransform t{};
        t.T.X=at.X;
        t.T.Y=at.Y;
        t.T.Z=at.Z;
        UObject* v=SpawnActor(S.world,vCls,S.controller,t);
        if(v){
            S.vehicles.push_back(v);
            ++sp;
        }
    }
    LOG("Vehicles spawned %d",sp);
    return sp;
}

// === INTERACTION ===
static void* PickupEntry(UObject* pk){
    if(!pk)return nullptr;
    int off=pk->Off("PrimaryPickupItemEntry");
    if(off<0)return nullptr;
    return(char*)pk+off;
}
static UObject* EntryDef(void* e){
    if(!e)return nullptr;
    UObject* cdo=FindO("/Script/FortniteGame.Default__FortItemEntry");
    int off=cdo?cdo->Off("ItemDefinition"):-1;
    return off>0?Rd<UObject*>(e,(size_t)off):nullptr;
}
static int EntryCount(void* e){
    if(!e)return 0;
    UObject* cdo=FindO("/Script/FortniteGame.Default__FortItemEntry");
    int off=cdo?cdo->Off("Count"):-1;
    return off>0?Rd<int32>(e,(size_t)off):0;
}
static bool NearRefresh(){
    if(!S.world||!S.pawn)return false;
    FVector o{};
    if(!ActorLoc(S.pawn,o))return false;
    S.nearContainers.clear();
    S.nearContainerLocs.clear();
    S.nearPickups.clear();
    S.nearPickupLocs.clear();
    if(!g_go)return false;
    if(!g_bcC)g_bcC=FindO("/Script/FortniteGame.BuildingContainer");
    if(!g_pickC)g_pickC=FindO("/Script/FortniteGame.FortPickup");
    if(!g_bcC||!g_pickC)return false;
    Call loc("/Script/Engine.Actor.K2_GetActorLocation");
    for(int32 i=0;i<g_go->Num;++i){
        UObject* a=g_go->At((uint32)i);
        if(!a||Bad(a)||a->IsCDO())continue;
        UClass* cl=a->Class();
        if(!cl||Bad(cl))continue;
        bool isBC=a->IsA((UClass*)g_bcC);
        bool isPK=!isBC&&a->IsA((UClass*)g_pickC);
        if(!isBC&&!isPK)continue;
        if(!loc.ok()||!loc.run(a))continue;
        FVector l=loc.res<FVector>();
        double dx=l.X-o.X,dy=l.Y-o.Y;
        if(dx*dx+dy*dy>500.0*500.0)continue;
        if(isBC){
            S.nearContainers.push_back(a);
            S.nearContainerLocs.push_back(l);
        }else{
            S.nearPickups.push_back(a);
            S.nearPickupLocs.push_back(l);
        }
    }
    return true;
}
static bool LocalPickup(UObject* pk){
    if(!pk||!S.inventory||!S.pawn)return false;
    void* e=PickupEntry(pk);
    UObject* def=EntryDef(e);
    int cnt=EntryCount(e);
    if(!def||cnt<1||cnt>9999)return true;
    int slot=-1;
    UObject* wc=FindO("/Script/FortniteGame.FortWeaponItemDefinition");
    if(wc&&def->IsA((UClass*)wc)){
        for(int i=1;i<6;++i){
            if(!S.slots[i]){slot=i;break;}
        }
        if(slot<0)return true;
    }
    if(!GiveItem(def,cnt,slot,0,true))return true;
    Call d("/Script/Engine.Actor.K2_DestroyActor");
    if(d.ok())d.run(pk);
    InvUpdate();
    return true;
}
static bool DoInteract(UObject* target){
    if(!target||Bad(target)||!S.pawn)return false;
    UClass* cl=target->Class();
    if(!g_bcC)g_bcC=FindO("/Script/FortniteGame.BuildingContainer");
    if(!g_vehC)g_vehC=FindO("/Script/FortniteGame.FortAthenaVehicle");
    if(!g_wallC)g_wallC=FindO("/Script/FortniteGame.BuildingWall");
    if(g_bcC&&target->IsA((UClass*)g_bcC)){
        Call bp("/Script/FortniteGame.BuildingContainer.BP_SpawnLoot");
        if(bp.ok()&&bp.set("PlayerPawn",S.pawn)&&bp.run(target)){
            if(GetBool(target,"bAlreadySearched"))return true;
        }
        return true;
    }
    if(g_vehC&&target->IsA((UClass*)g_vehC)){
        for(int seat=0;seat<4;++seat){
            Call f("/Script/FortniteGame.FortAthenaVehicle.ForceSeatSinglePawn");
            if(!f.ok())break;
            if(!f.set("PawnToSeat",S.pawn)||!f.set("SeatIndex",seat))break;
            if(!f.run(target))continue;
            if(f.res<bool>()){LOG("Seated %d",seat);return true;}
        }
        return false;
    }
    if(g_wallC&&target->IsA((UClass*)g_wallC)){
        Call bi("/Script/FortniteGame.BuildingActor.BlueprintOnInteract",target);
        if(bi.ok()&&bi.set("InteractingPawn",S.pawn)&&bi.run(target))return true;
    }
    Call bi("/Script/FortniteGame.BuildingActor.BlueprintOnInteract",target);
    if(bi.ok()&&bi.set("InteractingPawn",S.pawn)&&bi.run(target))return true;
    return false;
}
static void InteractTick(){
    if(!S.pawn||!S.world)return;
    double now=Now();
    if(now-S.lastNear>3.0){
        S.lastNear=now;
        NearRefresh();
    }
    FVector o{};
    if(!ActorLoc(S.pawn,o))return;
    UObject* best=nullptr;
    double bestD=0;
    int kind=0;
    auto consider=[&](UObject* a,const FVector& at,int k,double reach){
        double dx=at.X-o.X,dy=at.Y-o.Y,dz=at.Z-o.Z;
        double d=dx*dx+dy*dy+dz*dz;
        if(d>reach*reach)return;
        if(!best||d<bestD){best=a;bestD=d;kind=k;}
    };
    for(size_t i=0;i<S.nearContainers.size()&&i<S.nearContainerLocs.size();++i){
        UObject* c=S.nearContainers[i];
        if(!c||Bad(c))continue;
        if(GetBool(c,"bAlreadySearched"))continue;
        consider(c,S.nearContainerLocs[i],1,320.0);
    }
    for(UObject* v:S.vehicles){
        if(!v||Bad(v))continue;
        FVector l{};
        if(!ActorLoc(v,l))continue;
        consider(v,l,2,320.0);
    }
    if(best&&S.autoInteract){
        double d=std::sqrt(bestD);
        auto& last=S.autoAt[best];
        if(now-last>2.0){
            last=now;
            if(d<260.0)DoInteract(best);
        }
    }
    for(size_t i=0;i<S.nearPickups.size()&&i<S.nearPickupLocs.size();++i){
        UObject* pk=S.nearPickups[i];
        if(!pk||Bad(pk))continue;
        const FVector& l=S.nearPickupLocs[i];
        double dx=l.X-o.X,dy=l.Y-o.Y,dz=l.Z-o.Z;
        double d=dx*dx+dy*dy+dz*dz;
        if(d>300.0*300.0)continue;
        auto& last=S.pickupAt[pk];
        if(now-last<1.5)continue;
        last=now;
        LocalPickup(pk);
    }
}

// === BR START ===
static void BRStart(){
    BR::started=true;
    if(!S.state||!S.mode)return;
    BR::mapinfo=GetObj(S.state,"MapInfo");
    float now=WT();
    float warm=60.0f;
    int ss=S.state->Off("WarmupCountdownStartTime");
    int se=S.state->Off("WarmupCountdownEndTime");
    if(ss>0)Wr<float>(S.state,(size_t)ss,now);
    if(se>0)Wr<float>(S.state,(size_t)se,now+warm);
    int md=S.mode->Off("WarmupCountdownDuration");
    int me=S.mode->Off("WarmupEarlyCountdownDuration");
    if(md>0)Wr<float>(S.mode,(size_t)md,warm);
    if(me>0)Wr<float>(S.mode,(size_t)me,warm-10);
    BR::warmupEnd=now+warm;
    int wpc=S.mode->Off("WarmupRequiredPlayerCount");
    if(wpc>0)Wr<int32>(S.mode,(size_t)wpc,1);
    SetBool(S.mode,"bWorldIsReady",true);
    SetBool(S.state,"bPlaylistStoppedSafeZonePhases",false);
    if(!BR::aliveAdded){
        int ap=S.mode->Off("AlivePlayers");
        if(ap>0&&S.controller&&g_Add){
            g_Add((char*)S.mode+ap,(void*)((char*)S.mode+ap),&S.controller);
            BR::aliveAdded=true;
        }
    }
    int pl=S.state->Off("PlayersLeft");
    if(pl>0){
        Wr<int32>(S.state,(size_t)pl,1);
        Call r("/Script/FortniteGame.FortGameStateAthena.OnRep_PlayersLeft");
        if(r.ok())r.run(S.state);
    }
    SetPhase(2);
    SetStep(2);
    BR::nextWave=Now()+1.0;
    LOG("BR start warmup %.0f",warm);
}

// === BR TICK ===
static void BRTick(){
    if(!S.finalized||!S.state||!S.mode||!S.world)return;
    double mono=Now();
    if(!BR::started){
        if(mono-S.finalizeAt<2.0)return;
        BRStart();
        return;
    }
    if((int)BR::botPawns.size()<S.botTarget&&mono>=BR::nextWave){
        BR::nextWave=mono+2.0;
        SpawnBotWave(10);
    }
    MotorTick(g_bots,320.0f,"Bot");
    MotorTick(g_wild,200.0f,"Wild");
    float now=WT();
    int rw=S.state->Off("ReplicatedWorldTimeSeconds");
    if(rw>0)Wr<float>(S.state,(size_t)rw,now);
    if(BR::phase<=2){
        if(BR::step<3&&BR::warmupEnd>0&&now>=BR::warmupEnd-10.0f)SetStep(3);
        if(BR::warmupEnd>0&&now>=BR::warmupEnd)StartAircraftPhase();
        return;
    }
    if(BR::phase==3){
        UObject* ac=BR::aircraft;
        if(ac&&!Bad(ac)){
            if(!BR::busUnlock&&BR::dropStart>0&&now>=BR::dropStart){
                BR::busUnlock=true;
                SetBool(S.state,"bAircraftIsLocked",false);
                SetStep(5);
            }
            if(!BR::dropForced&&BR::dropEnd>0&&now>=BR::dropEnd){
                BR::dropForced=true;
                if(!S.aircraftJumped)BRJump();
            }
            if(!BR::flightDone&&BR::flightEnd>0&&now>=BR::flightEnd){
                BR::flightDone=true;
                Call d("/Script/Engine.Actor.K2_DestroyActor");
                if(d.ok())d.run(ac);
                BR::aircraft=nullptr;
                BR::safeStart=((BR::dropEnd>0)?BR::dropEnd:now)+60.0f;
                SetPhase(4);
                SetStep(6);
                LOG("Flight done");
            }
        }else if(!BR::flightDone){
            BR::flightDone=true;
            BR::safeStart=now+15.0f;
            SetPhase(4);
            SetStep(6);
        }
        if(BR::jumpAt>0&&!S.pawn&&Now()-BR::jumpAt>1.5){
            BR::jumpAt=-1;
            UObject* pcls=S.botPawn?S.botPawn:LoadC("/Game/Athena/PlayerPawn_Athena.PlayerPawn_Athena_C","/Script/Engine.Pawn");
            if(pcls){
                FVector end{BR::routeStart.X,BR::routeStart.Y,BR::routeStart.Z};
                float gz=500;
                if(!GroundZ(end.X,end.Y,gz))gz=500;
                FTransform t{};
                t.T.X=end.X;
                t.T.Y=end.Y;
                t.T.Z=gz+200;
                UObject* p=SpawnActor(S.world,pcls,nullptr,t);
                if(p){
                    Possess(S.controller,p);
                    int mf=S.controller->Off("MyFortPawn");
                    if(mf>0)Wr<UObject*>(S.controller,(size_t)mf,p);
                    S.pawn=p;
                    S.ability=false;
                    S.visibility=false;
                    S.aircraftJumped=false;
                    GrantAbilities();
                    ApplySkin(p);
                }
            }
        }
        return;
    }
    if(BR::phase==4){
        if(!BR::zoneFormed){
            if(BR::safeStart>0&&now>=BR::safeStart){
                BR::zoneFormed=true;
                ZoneTables();
                PickCenters();
                SetupIndicator();
                NewZonePhase(1);
            }
        }else if(BR::indicator&&!Bad(BR::indicator)){
            int ss=BR::indicator->Off("SafeZoneStartShrinkTime");
            int fs=BR::indicator->Off("SafeZoneFinishShrinkTime");
            float st=ss>0?Rd<float>(BR::indicator,(size_t)ss):0;
            float fn=fs>0?Rd<float>(BR::indicator,(size_t)fs):0;
            if(!BR::zoneShrink&&st>0&&now>=st&&now<fn){
                BR::zoneShrink=true;
                SetStep(8);
            }
            if(BR::zoneShrink&&fn>0&&now>=fn){
                int cp=BR::indicator->Off("CurrentPhase");
                int cur=cp>0?Rd<int32>(BR::indicator,(size_t)cp):0;
                if(cur+1<BR::zoneCount){
                    BR::zoneShrink=false;
                    NewZonePhase(cur+1);
                }else{
                    BR::zoneShrink=false;
                    if(fs>0)Wr<float>(BR::indicator,(size_t)fs,fn+3600);
                }
            }
        }
        return;
    }
}

// === TICK ===
static void Tick(){
    if(!S.world)return;
    double now=Now();
    if(!S.attempt){
        if(now-S.retryAt<1.0)return;
        S.retryAt=now;
        SetupWorld(S.world);
        return;
    }
    if(S.finalizeAt>0&&!S.finalized&&now>=S.finalizeAt){
        Finalize();
        return;
    }
    if(S.finalized&&now-S.lastVis>8.0){
        S.lastVis=now;
        if(!S.visibility&&S.pawn){
            ApplySkin(S.pawn);
            S.visibility=true;
        }
    }
    if(S.finalized&&S.pawn&&now-S.lastSp>2.0){
        S.lastSp=now;
        for(const char* mn:{"/Script/FortniteGame.FortPawn.SetMaxHealth","/Script/FortniteGame.FortPawn.SetHealth"}){
            Call c(mn);
            if(c.ok()&&c.setAt(0,100.0f))c.run(S.pawn);
        }
    }
    if(S.finalized&&S.pawn&&now-S.lastAb>10.0){
        S.lastAb=now;
        if(!S.ability)GrantAbilities();
    }
    if(S.finalized&&S.controller&&now-S.lastQC>6.0){
        S.lastQC=now;
        if(!S.qb){
            BuildQB();
            SetQBReady();
        }
    }
    if(S.finalized&&now-S.lastWild>30.0&&g_wild.empty()){
        S.lastWild=now;
        SpawnWildlife(8);
    }
    if(S.finalized&&now-S.lastVeh>25.0&&S.vehicles.empty()){
        S.lastVeh=now;
        SpawnVehicles(3);
    }
    if(S.finalized)BRTick();
    if(S.finalized&&!S.pawn&&S.controller&&!BR::spectating){
        UObject* p=GetPawn(S.controller);
        if(p&&!Bad(p)&&p!=BR::oldPawn){
            BR::oldPawn=nullptr;
            S.pawn=p;
            S.ability=false;
            S.visibility=false;
            S.aircraftJumped=false;
            GrantAbilities();
            ApplySkin(p);
        }
    }
    if(S.finalized)InteractTick();
}

// === HOOKS ===
static void (*g_oTick)(UObject*,int,float)=nullptr;
static void (*g_oExec)(UObject*,UObject*,const TCHAR*,void*)=nullptr;
static void (*g_oPE)(UObject*,UFunction*,void*)=nullptr;
extern "C" int DobbyHook(void*,void*,void**);

static const char* PN(int p){
    switch(p){
        case 0:return "None";
        case 1:return "Setup";
        case 2:return "Warmup";
        case 3:return "Aircraft";
        case 4:return "SafeZones";
        case 5:return "EndGame";
        default:return "?";
    }
}
static void pe_hook(UObject* obj,UFunction* f,void* p){
    if(!g_oPE)return;
    if(!g_int&&f&&g_tid.load()==(int)gettid()){
        if(S.lobbyRequest){g_oPE(obj,f,p);return;}
        std::string fn=f->NS();
        bool isPC=obj==S.controller;
        if(g_hudCtx&&obj&&fn=="JumpFromHudStart"){
            if(obj->IsA((UClass*)g_hudCtx)){
                BRJump();
                return;
            }
        }
        if(isPC&&fn=="JumpFromHudStart"){BRJump();return;}
        if(isPC&&(fn=="ReturnToMainMenu"||fn=="ServerReturnToMainMenu")){
            S.lobbyRequest=true;
        }
        if(fn=="ServerAttemptAircraftJump"){
            if(!S.aircraftJumped)BRJump();
            return;
        }
        if(fn=="ClientActivateSlot"){
            G g;
            uint8 bar=0;
            int slot=-1;
            if(void* bp=Inc(f,p,"InQuickBar",1))bar=Rd<uint8>(bp);
            if(void* sp=Inc(f,p,"Slot",4))slot=Rd<int32>(sp);
            if(slot>=0&&slot<6){
                UObject* inst=(bar==0?S.slots:S.bslots)[slot];
                if(inst&&!Bad(inst)){
                    FGuid gd=ItemGuidOf(inst);
                    if(gd.Valid()&&!(gd==S.lastEquip)){
                        S.lastEquip=gd;
                        EquipSlot(slot);
                    }
                }
            }
            return;
        }
        if(fn=="OnGamePhaseStepChanged")return;
        if(fn=="ReadyToEndMatch"||fn=="ReadyToStartMatch")return;
        if(fn=="ServerExecuteInventoryItem"||fn=="ServerExecuteInventoryWeapon")return;
        if(fn=="ServerHandlePickupInfo"||fn=="ServerHandlePickup"){
            if(obj==S.pawn){
                G g;
                void* a=Inc(f,p,"PickUp",8);
                if(!a)a=Inc(f,p,"Pickup",8);
                if(!a)a=p;
                if(a){
                    UObject* pk=Rd<UObject*>(a);
                    if(pk&&!Bad(pk))LocalPickup(pk);
                }
            }
            return;
        }
        if(fn=="ServerAttemptInteract"&&p){
            UObject* t=Rd<UObject*>(p);
            if(t&&!Bad(t))DoInteract(t);
            return;
        }
        if(fn=="ServerAttemptInventoryDrop")return;
        if(fn=="ServerCreateBuildingActor")return;
        if(fn=="ServerPlayEmoteItem"||fn=="ServerPlaySprayItem")return;
        if(fn=="PlayEmoteItem"&&isPC)return;
        if(fn=="ServerAttemptExitVehicle")return;
    }
    g_oPE(obj,f,p);
}
static void tick_hook(UObject* world,int t,float dt){
    if(g_oTick)g_oTick(world,t,dt);
    if(g_int||!world)return;
    int ex=0;
    if(g_tid.compare_exchange_strong(ex,(int)gettid()))LOG("Tick tid=%d",(int)gettid());
    if(g_tid.load()!=(int)gettid())return;
    G g;
    std::string wn=world->NS();
    static std::string last;
    if(wn!=last){
        LOG("World: %s",wn.c_str());
        last=wn;
    }
    if(wn.find("Frontend")!=std::string::npos){
        if(S.attempt&&!S.finalized&&S.finalizeAt>0&&Now()>=S.finalizeAt)Finalize();
        return;
    }
    if(wn.find("Artemis_Terrain")==std::string::npos&&wn.find("Athena_Terrain")==std::string::npos)return;
    int idx=Rd<int32>(world,0xC);
    if(world!=S.world||idx!=S.worldIdx){
        S=Session{};
        S.world=world;
        S.worldIdx=idx;
        S.observedAt=Now();
        LOG("Terrain: %s",wn.c_str());
    }
    double now=Now();
    if(S.state){
        static int pp=-1;
        if(pp<0)pp=S.state->Off("GamePhase");
        if(pp>0){
            uint8 ph=Rd<uint8>(S.state,(size_t)pp);
            if((int)ph!=S.lastPhase){
                LOG("Phase obs=%u (%s)",ph,PN(ph));
                S.lastPhase=(int)ph;
            }
        }
    }
    if(now-S.observedAt<5.0)return;
    Tick();
}
static bool exec_hook(UObject* e,UObject* w,const TCHAR* c,void* o){
    if(!g_oExec)return false;
    std::string cs;
    if(c){
        for(int i=0;i<256&&c[i];++i)cs.push_back(c[i]<128?(char)c[i]:'?');
    }
    if(!cs.empty())LOG("Exec: %s",cs.c_str());
    std::string n=cs;
    std::transform(n.begin(),n.end(),n.begin(),::tolower);
    while(!n.empty()&&(n.back()==' '||n.back()=='\t'))n.pop_back();
    bool onGame=g_tid.load()==(int)gettid();
    if(n=="open athena_terrain"||n.rfind("open athena_terrain",0)==0){
        S=Session{};
        S.attempt=false;
        return g_oExec(e,w,u"open Athena_Terrain",o);
    }
    if(onGame&&n.rfind("era.",0)==0){
        G g;
        if(n=="era.status"){
            LOG("Status world=%p pc=%p pawn=%p inv=%p qb=%p fin=%d phase=%d",
                S.world,S.controller,S.pawn,S.inventory,S.quickbars,(int)S.finalized,S.lastPhase);
        }else if(n=="era.vehicles"){
            SpawnVehicles(8);
        }else if(n=="era.animals"){
            SpawnWildlife(8);
        }else if(n=="era.bots"){
            int x=-1;
            sscanf(n.c_str(),"era.bots %d",&x);
            if(x<0)x=0;
            S.botTarget=x>120?120:x;
            BR::nextWave=0;
        }else if(n=="era.bus"){
            if(!BR::started)BR::started=true;
            BR::warmupEnd=WT();
            StartAircraftPhase();
        }else if(n=="era.jump"){
            BRJump();
        }else if(n=="era.zone"){
            int x=1;
            sscanf(n.c_str(),"era.zone %d",&x);
            if(BR::started){
                BR::zoneFormed=true;
                if(BR::centers.empty()){
                    ZoneTables();
                    PickCenters();
                }
                SetupIndicator();
                NewZonePhase(x);
            }
        }else if(n=="era.qb"){
            BuildQB();
            SetQBReady();
        }else if(n=="era.auto"){
            S.autoInteract=n.find("off")==std::string::npos;
            LOG("Auto %d",(int)S.autoInteract);
        }else if(n=="era.lobby"){
            S.lobbyRequest=true;
        }else if(n=="god"){
            if(S.pawn){
                bool dam=GetBool(S.pawn,"bCanBeDamaged");
                SetBool(S.pawn,"bCanBeDamaged",!dam);
                LOG("God %d",(int)!dam);
            }
        }else if(n=="teleport"){
            if(S.controller&&S.pawn){
                int lr=S.controller->Off("LocationUnderReticle");
                if(lr>0){
                    FVector t=Rd<FVector>(S.controller,(size_t)lr);
                    Call st("/Script/Engine.Actor.K2_SetActorLocation");
                    if(st.ok()){
                        st.set("NewLocation",t);
                        bool bs=false,bt=true;
                        st.set("bSweep",bs);
                        st.set("bTeleport",bt);
                        st.run(S.pawn);
                    }
                }
            }
        }else{
            LOG("era.status|era.vehicles|era.animals|era.bots [n]|era.bus|era.jump|era.zone [i]|era.qb|era.auto on|off|era.lobby|teleport|god");
        }
        return true;
    }
    return g_oExec(e,w,c,o);
}

// === PROLOGUE VERIFY ===
static bool VPro(const char* l,uintptr_t r,const uint32_t* w,size_t n){
    for(size_t i=0;i<n;++i){
        uint32_t a=Rd<uint32_t>((const void*)(g_base+r),4*i);
        if(a!=w[i]){
            LOG("MISMATCH %s RVA=%llx w%zu %08x!=%08x",l,(unsigned long long)r,i,w[i],a);
            return false;
        }
    }
    LOG("OK %s",l);
    return true;
}
static void InstallHooks(){
    LOG("InstallHooks");
    static const uint32_t tw[]={0xd10503ff,0x6d0d23e9,0xa90e6ffc,0xa90f67fa};
    static const uint32_t ew[]={0xd10183ff,0xa9025ff8,0xa90357f6,0xa9044ff4};
    static const uint32_t pe0[]={0xa9ba6ffc,0xf81a0ffc};
    static const uint32_t peT[]={0xa90167fa,0xa9025ff8,0xa90357f6};
    bool peOK=false;
    {
        uint32_t a0=Rd<uint32_t>((const void*)(g_base+R_PE));
        for(size_t i=0;i<2;++i)if(a0==pe0[i]){peOK=true;break;}
        if(peOK){
            for(size_t i=0;i<3;++i){
                uint32_t a=Rd<uint32_t>((const void*)(g_base+R_PE),4*(i+1));
                if(a!=peT[i]){peOK=false;break;}
            }
        }
        if(!peOK)LOG("PE prologue mismatch");
        else LOG("OK PE");
    }
    bool tkOK=VPro("Tick",R_Tick,tw,4);
    bool exOK=VPro("Exec",R_Exec,ew,4);
    int ins=0;
    if(peOK&&DobbyHook((void*)(g_base+R_PE),(void*)pe_hook,(void**)&g_oPE)==0&&g_oPE){
        LOG("PE hooked");
        ++ins;
    }
    if(tkOK&&DobbyHook((void*)(g_base+R_Tick),(void*)tick_hook,(void**)&g_oTick)==0&&g_oTick){
        LOG("Tick hooked");
        ++ins;
    }
    if(exOK&&DobbyHook((void*)(g_base+R_Exec),(void*)exec_hook,(void**)&g_oExec)==0&&g_oExec){
        LOG("Exec hooked");
        ++ins;
    }
    LOG("Hooks=%d",ins);
}

// === CRASH ===
static void Crash(int sig,siginfo_t* si,void* ctx){
    char b[2048];
    int n=0;
    n+=snprintf(b+n,sizeof(b)-(size_t)n,"CRASH sig=%d addr=%p base=%p\n",sig,si?si->si_addr:nullptr,(void*)g_base);
    ucontext_t* uc=(ucontext_t*)ctx;
    if(uc){
#if defined(__aarch64__)
        uintptr_t pc=(uintptr_t)uc->uc_mcontext.pc;
        uintptr_t sp=(uintptr_t)uc->uc_mcontext.sp;
        n+=snprintf(b+n,sizeof(b)-(size_t)n,"PC=0x%lx lib+0x%lx\n",(unsigned long)pc,g_base?(unsigned long)(pc-g_base):0ul);
        if(sp){
            for(uintptr_t p=sp&~(uintptr_t)0xF;p<sp+0x400&&n<(int)sizeof(b)-64;p+=8){
                uintptr_t v=*(uintptr_t*)p;
                if(g_base&&v>g_base&&v<g_base+0xE000000)
                    n+=snprintf(b+n,sizeof(b)-(size_t)n,"  lib+0x%lx\n",(unsigned long)(v-g_base));
            }
        }
#endif
    }
    int fd=open("/data/local/tmp/era21_crash.txt",O_WRONLY|O_CREAT|O_APPEND,0644);
    if(fd>=0){
        ssize_t w=write(fd,b,(size_t)n);
        (void)w;
        close(fd);
    }
    signal(sig,SIG_DFL);
    raise(sig);
}
static void InstallCrash(){
    struct sigaction sa;
    memset(&sa,0,sizeof(sa));
    sa.sa_sigaction=Crash;
    sa.sa_flags=SA_SIGINFO;
    sigaction(SIGSEGV,&sa,nullptr);
    sigaction(SIGBUS,&sa,nullptr);
    sigaction(SIGILL,&sa,nullptr);
    sigaction(SIGABRT,&sa,nullptr);
}

// === WORKER ===
static int Cb(dl_phdr_info* info,size_t,void*){
    const char* n=strrchr(info->dlpi_name,'/');
    n=n?n+1:info->dlpi_name;
    if(strcmp(n,"libUnreal.so"))return 0;
    g_base=info->dlpi_addr;
    return 1;
}
static void* Worker(void*){
    LOG("Era21 start");
    for(int i=0;i<600&&!g_base;++i){
        dl_iterate_phdr(Cb,nullptr);
        if(!g_base)usleep(100000);
    }
    if(!g_base){
        LOG("timeout");
        return nullptr;
    }
    LOG("base=%p",(void*)g_base);
    InstallCrash();
    g_PE=FA<decltype(g_PE)>(R_PE);
    g_Find=FA<decltype(g_Find)>(R_Find);
    g_LoadO=FA<decltype(g_LoadO)>(R_LoadO);
    g_LoadC=FA<decltype(g_LoadC)>(R_LoadC);
    g_N2S=FA<decltype(g_N2S)>(R_N2S);
    g_Free=FA<decltype(g_Free)>(R_Free);
    g_Real=FA<decltype(g_Real)>(R_Real);
    g_Add=FA<decltype(g_Add)>(R_Add);
    g_Rem=FA<decltype(g_Rem)>(R_Rem);
    g_Spawn=FA<decltype(g_Spawn)>(R_Begin);
    g_Finish=FA<decltype(g_Finish)>(R_Finish);
    g_GetPC=FA<decltype(g_GetPC)>(R_GetPC);
    g_Stand=FA<decltype(g_Stand)>(R_Stand);
    usleep(200000);
    if(!InitGO())LOG("GObjects unresolved");
    g_hudCtx=FindO("/Script/FortniteGame.AthenaHUDContext");
    g_airCmp=FindO("/Script/FortniteGame.FortControllerComponent_Aircraft");
    usleep(200000);
    InstallHooks();
    LOG("Ready");
    return nullptr;
}

extern "C" __attribute__((constructor)) void LibMain(){
    InitLog();
    pthread_t t;
    if(pthread_create(&t,nullptr,Worker,nullptr)==0)pthread_detach(t);
}
extern "C" JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM*,void*){return JNI_VERSION_1_6;}
