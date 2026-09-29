#include "Bf2Adapter.h"
#include "CleanVR.h"
#include "VrSettings.h"
#include "Refractor.h"
#include "ReloadAudio.h"
#include "FrameTiming.h"
#include <cstring>
#include <cmath>
#include <cstdint>
#include <intrin.h>

namespace CleanVR { namespace Engine { namespace {
BYTE* game=nullptr;BYTE* render=nullptr;bool installed=false,attempted=false;
template<class T> bool Read(const void* base,size_t offset,T& out){__try{if(!base)return false;memcpy(&out,static_cast<const BYTE*>(base)+offset,sizeof(T));return true;}__except(EXCEPTION_EXECUTE_HANDLER){return false;}}
bool Copy(void* dest,const void* src,size_t length){__try{if(!dest||!src)return false;memcpy(dest,src,length);return true;}__except(EXCEPTION_EXECUTE_HANDLER){return false;}}
template<class T>T Value(const void* p,size_t o=0){T result{};Read(p,o,result);return result;}
bool Match(const BYTE* p,const BYTE* signature,size_t length){__try{return p&&memcmp(p,signature,length)==0;}__except(EXCEPTION_EXECUTE_HANDLER){return false;}}
bool Code(const void* p,HMODULE mod){
    MEMORY_BASIC_INFORMATION mbi{};return p&&VirtualQuery(p,&mbi,sizeof(mbi))&&mbi.AllocationBase==mod&&(mbi.Protect&(PAGE_EXECUTE|PAGE_EXECUTE_READ|PAGE_EXECUTE_READWRITE));
}
bool Jump(BYTE* target,const BYTE* expected,size_t length,void* hook,void** original){
    if(length<5||length>16||!Match(target,expected,length)){Log("PROFILE mismatch at %p; hook omitted",target);return false;}
    BYTE* trampoline=static_cast<BYTE*>(VirtualAlloc(nullptr,length+5,MEM_COMMIT|MEM_RESERVE,PAGE_EXECUTE_READWRITE));if(!trampoline)return false;
    memcpy(trampoline,target,length);trampoline[length]=0xE9;*reinterpret_cast<LONG*>(trampoline+length+1)=(LONG)(target+length-(trampoline+length+5));
    DWORD old=0;if(!VirtualProtect(target,length,PAGE_EXECUTE_READWRITE,&old)){VirtualFree(trampoline,0,MEM_RELEASE);return false;}
    // Publish the trampoline before enabling the entry point.
    *original=trampoline;target[0]=0xE9;*reinterpret_cast<LONG*>(target+1)=(LONG)(static_cast<BYTE*>(hook)-(target+5));for(size_t i=5;i<length;++i)target[i]=0x90;
    DWORD ignored;VirtualProtect(target,length,old,&ignored);FlushInstructionCache(GetCurrentProcess(),target,length);FlushInstructionCache(GetCurrentProcess(),trampoline,length+5);return true;
}
void* ClassRaw(const char* name){if(!DICE::detail::g_pClassManager)return nullptr;DICE::std::string key(name);return DICE::detail::g_pClassManager->getClassByName(&key);}
void* Class(const char* name){__try{return ClassRaw(name);}__except(EXCEPTION_EXECUTE_HANDLER){return nullptr;}}
void* Player(){return Value<void*>(Class("PlayerManager"),0x60);}
void* Soldier(){return Value<void*>(Value<void*>(Player(),0xCC),4);}
Mat reference=Identity();bool referenceValid=false;
// VehicleView/closed-loop aim adapted from BF2142VR (MIT), with independently
// inspected BF2 layouts. Only the local player's occupied PCO is considered.
bool vehicleProfile=false,vehicleViewValid=false;
struct VehicleSample {void* owner=nullptr;void* weak=nullptr;void* seat=nullptr;void* root=nullptr;VehicleRole role=VehicleRole::Infantry;bool aircraft=false;Mat world{};char name[80]{};};
VehicleSample vehicleSample{};
Mat vehicleLocal{},vehicleBase{},vehicleNative{};float vehicleYawOffset=0,vehiclePitchOffset=0;ULONGLONG vehicleTick=0;
Mat TrackingBase(const Mat& source){return vehicleViewValid?vehicleBase:Level(source);}
bool AimedSeat(VehicleRole role){return role==VehicleRole::Aimed||role==VehicleRole::PitchAimed;}
Mat VehicleHeading(const Mat& world,bool aircraft){
    Mat pitch=Identity();pitch.m[1][1]=pitch.m[2][2]=std::cos(vehiclePitchOffset);
    pitch.m[1][2]=-std::sin(vehiclePitchOffset);pitch.m[2][1]=std::sin(vehiclePitchOffset);
    if(aircraft){
        // Gunner heading/stick offsets belong to the aircraft, not the world
        // horizon. The hull carries all three axes through loops and rolls;
        // the turret's measured rotation remains feedback, avoiding a loop.
        Mat attitude=world;SetPosition(attitude,{});
        return Mul(Mul(pitch,Yaw(vehicleYawOffset)),attitude);
    }
    const float yaw=std::atan2(world.m[2][0],world.m[2][2])+vehicleYawOffset;
    return Mul(pitch,Yaw(yaw));
}
bool VehicleObject(void* object){
    auto* table=Value<BYTE*>(object);DWORD flags=0;
    if(!table||!Read(object,4,flags)||(flags&1))return false;
    // Shared object-base interface; accepts a PCO's rotational-bundle parents.
    return Value<void*>(table,24*4)==game+0x31D520&&Value<void*>(table,25*4)==game+0x31D530;
}
bool VehicleName(void* definition,char (&name)[80]){
    if(Value<void*>(definition)!=game+0x4F25E0)return false;
    // MSVC 7.1 string object at +0C includes a 4-byte allocator prefix.
    const unsigned n=Value<unsigned>(definition,0x20),cap=Value<unsigned>(definition,0x24);
    if(!n||n>=sizeof(name)||cap<n||cap>4096)return false;
    const char* data=cap<16?static_cast<const char*>(definition)+0x10:Value<const char*>(definition,0x10);
    if(!Copy(name,data,n+1)||name[n])return false;
    for(unsigned i=0;i<n;++i){char& c=name[i];if(c>='A'&&c<='Z')c+=32;if(!((c>='a'&&c<='z')||(c>='0'&&c<='9')||c=='_'))return false;}return true;
}
VehicleRole SeatRole(const char* name){
    struct Entry{const char* name;VehicleRole role;};static const Entry entries[]={
#include "VehicleProfiles.inl"
    };
    for(const auto& entry:entries)if(!strcmp(name,entry.name))return entry.role;
    return VehicleRole::Vehicle; // Unknown custom seats keep manual mouse aim.
}
bool SameVehicle(const VehicleSample& a,const VehicleSample& b){return a.owner==b.owner&&a.weak==b.weak&&a.seat==b.seat&&a.root==b.root;}
bool AircraftRootName(const char* name){
    // Separate camera coverage from input roles. Expansion and mod aircraft
    // need hull-relative cameras even when their control role is unprofiled.
    // This exact-name lookup runs only when the occupied seat changes.
    if(SeatRole(name)==VehicleRole::Pilot)return true;
    static const char* const roots[]={
#include "AircraftProfiles.inl"
    };
    for(const char* root:roots)if(!strcmp(name,root))return true;
    return false;
}
bool ReadVehicle(VehicleSample& s){
    if(!vehicleProfile||!LocalAlive())return false;
    void* player=Player();s.weak=Value<void*>(player,0xCC);s.owner=Value<void*>(s.weak,4);
    s.seat=Value<void*>(Value<void*>(player,0x80),4);
    if(!s.owner||!s.seat||s.seat==s.owner||Value<void*>(s.seat)!=game+0x4F2430||!VehicleObject(s.seat))return false;
    if(!VehicleName(Value<void*>(s.seat,0x14),s.name))return false;
    s.role=SeatRole(s.name);s.root=s.seat;void* ancestors[24]{};unsigned count=0;
    for(;;){
        if(count==24)return false;for(unsigned i=0;i<count;++i)if(ancestors[i]==s.root)return false;ancestors[count++]=s.root;
        void* parent=nullptr;if(!Read(s.root,0x28,parent))return false;if(!parent)break;
        if(!VehicleObject(parent))return false;s.root=parent;
    }
    if(!Read(s.root,0x88,s.world)||!Rigid(s.world))return false;
    if(SameVehicle(s,vehicleSample))s.aircraft=vehicleSample.aircraft;
    else if(s.root==s.seat)s.aircraft=AircraftRootName(s.name);
    else{
        char rootName[80]{};
        s.aircraft=VehicleName(Value<void*>(s.root,0x14),rootName)&&AircraftRootName(rootName);
    }
    return true;
}
void ReportVehicleReject(){
    if(!vehicleProfile||!LocalAlive())return;
    void* seat=Value<void*>(Value<void*>(Player(),0x80),4);if(!seat||seat==Soldier())return;
    static ULONGLONG last=0;const auto now=GetTickCount64();if(last&&now-last<5000)return;last=now;
    void* definition=Value<void*>(seat,0x14);
    Log("VEHICLE occupied seat rejected: object=%p table=%p flags=%08X template=%p table=%p nameLength=%u nameCapacity=%u parent=%p",
        seat,Value<void*>(seat),Value<unsigned>(seat,4),definition,Value<void*>(definition),Value<unsigned>(definition,0x20),Value<unsigned>(definition,0x24),Value<void*>(seat,0x28));
}
void UpdateVehicleView(const Mat& native){
    VehicleSample s;vehicleViewValid=false;
    if(!ReadVehicle(s)){ReportVehicleReject();vehicleSample={};vehicleTick=0;return;}
    Mat seatWorld{};
    if(!Read(s.seat,0x88,seatWorld)||!Rigid(seatWorld)||Length(Sub(Position(seatWorld),Position(native)))>20.f){vehicleTick=0;return;}
    if(!SameVehicle(s,vehicleSample)){
        // Acquire the native seat frame relative to the aircraft at entry (or
        // recenter). Recompose it with the LIVE full hull transform each pair.
        // Never level this frame: Euler heading extraction discards pitch/roll
        // and becomes ambiguous when flying vertically or upside down.
        vehicleLocal=Mul(native,Inverse(s.world));
        if(!Rigid(vehicleLocal)){vehicleTick=0;return;}
        if(s.aircraft)vehicleYawOffset=std::atan2(vehicleLocal.m[2][0],vehicleLocal.m[2][2]);
        else{
            const float hullYaw=std::atan2(s.world.m[2][0],s.world.m[2][2]);
            vehicleYawOffset=std::remainder(std::atan2(native.m[2][0],native.m[2][2])-hullYaw,6.283185307f);
        }
        vehiclePitchOffset=0;
        Log("VEHICLE43 seat=%s role=%d aircraft=%d seatObject=%p root=%p camera=%s; headset relative to seat",s.name,(int)s.role,s.aircraft?1:0,s.seat,s.root,
            s.aircraft?(AimedSeat(s.role)?"aircraft gunner attitude":"full seat attitude"):"established ground heading");
    }
    vehicleSample=s;
    const Mat attached=Mul(vehicleLocal,s.world);
    vehicleBase=s.aircraft&&!AimedSeat(s.role)?attached:VehicleHeading(s.world,s.aircraft);
    SetPosition(vehicleBase,Position(attached));
    // Detached missile/third-person cameras must not steer a occupied gun.
    if(Length(Sub(Position(vehicleBase),Position(native)))>4.f){vehicleTick=0;return;}
    // The turret's camera can orbit its hinge. Follow its CURRENT position,
    // while its rotation remains a feedback measurement only. Freezing this
    // position at entry leaves the player beside/inside the turning gun.
    if(AimedSeat(s.role))SetPosition(vehicleBase,Position(native));
    vehicleNative=native;vehicleTick=GetTickCount64();vehicleViewValid=Rigid(vehicleBase);
}
bool InstallVehicle(){
    const BYTE control[]={0x8B,0x81,0x80,0,0,0,0x85,0xC0,0x53,0xB3,1};
    const BYTE world[]={0x56,0x8B,0xF1,0x8B,0x46,4,0x25,0,0x80,8,0};
    const BYTE name[]={0x8D,0x41,0x0C,0xC3};
    return Match(game+0xFEF90,control,sizeof(control))&&Match(game+0x31DB50,world,sizeof(world))&&Match(game+0x25DDD0,name,sizeof(name))&&
        Value<void*>(game,0x1FA237)==game+0x4F2430&&Value<void*>(game,0x1FBE66)==game+0x4F25E0&&
        Value<void*>(game,0x4F2430+24*4)==game+0x31D520&&Value<void*>(game,0x4F2430+25*4)==game+0x31D530&&
        Value<void*>(game,0x4F2430+32*4)==game+0x31DB50&&Value<void*>(game,0x4F25E0+10*4)==game+0x25DDD0;
}
SceneFunction originalScene=nullptr;
using NativePresent=bool(__thiscall*)(void*,unsigned);NativePresent originalPresent=nullptr;
void* deferredRenderer=nullptr;unsigned deferredPresentMode=0;
using CameraSetter=bool(__thiscall*)(void*,const Mat*);CameraSetter originalSetter=nullptr;
using ProjectionBuilder=void(__thiscall*)(void*);ProjectionBuilder originalProjection=nullptr;
struct ViewSave {void* object=nullptr;BYTE bytes[0x418]{};Mat source{},eye{},projection{};float nearZ=.07f,farZ=8000,eyeNear=.07f,cullFov=1.6f,aspect=1;};
ViewSave views[2];Runtime::Frame pairFrame{};bool pair=false,eyeActive=false;int activeEye=-1;
bool scopePass=false;
#include "NativeBody.inl"
void ApplyLens(const ViewSave& v){
    const float delta=(scopePass&&v.object==views[1].object?5.f:v.farZ)-v.eyeNear;
    if(Value<float>(v.object,0x24)==v.cullFov&&Value<float>(v.object,0x28)==v.eyeNear&&Value<float>(v.object,0x2C)==delta&&Value<float>(v.object,0x30)==v.aspect)return;
    Copy(static_cast<BYTE*>(v.object)+0x24,&v.cullFov,4);Copy(static_cast<BYTE*>(v.object)+0x28,&v.eyeNear,4);Copy(static_cast<BYTE*>(v.object)+0x2C,&delta,4);Copy(static_cast<BYTE*>(v.object)+0x30,&v.aspect,4);
    const DWORD dirty=15;const BYTE yes=1;Copy(static_cast<BYTE*>(v.object)+0x428,&dirty,4);Copy(static_cast<BYTE*>(v.object)+0x42C,&yes,1);Copy(static_cast<BYTE*>(v.object)+0x42D,&yes,1);
}
bool __fastcall SetCamera(void* self,void*,const Mat* value){
    if(eyeActive)for(auto& v:views)if(self==v.object){ApplyLens(v);return originalSetter(self,&v.eye);}
    return originalSetter(self,value);
}
void __fastcall BuildProjection(void* self,void*){
    if(eyeActive)for(const auto& v:views)if(self==v.object){ApplyLens(v);break;}
    const DWORD wasDirty=Value<DWORD>(self,0x428);originalProjection(self);
    if(eyeActive)for(auto& v:views)if(self==v.object){
        Mat existing{};Read(self,0x13C,existing);
        if((wasDirty&1)||memcmp(&existing,&v.projection,sizeof(Mat))!=0){
            Copy(static_cast<BYTE*>(self)+0x13C,&v.projection,sizeof(Mat));DWORD dirty=(Value<DWORD>(self,0x428)|14u)&~1u;Copy(static_cast<BYTE*>(self)+0x428,&dirty,4);
            const BYTE yes=1;Copy(static_cast<BYTE*>(self)+0x42C,&yes,1);Copy(static_cast<BYTE*>(self)+0x42D,&yes,1);
        }return;
    }
}
bool __fastcall SceneHook(void* self,void*,unsigned a,unsigned b,unsigned c){return CleanVR::DrawScene(originalScene,self,a,b,c);}
bool __fastcall PresentHook(void* self,void*,unsigned mode){
    // BF2's native function calls IDirect3DSwapChain9::Present directly at
    // +5AB6 (and +5BA7 on retry); RFX's device Present wrapper never sees it.
    // Respect the native already-presented flag and explicit skip mode.
    if(mode==3||Value<BYTE>(self,0x101))return originalPresent(self,mode);
    if(CleanVR::BeforeNativePresent()){
        deferredRenderer=self;deferredPresentMode=mode;
        // Mode 3 performs BF2's ordinary bookkeeping without flipping buffers.
        return originalPresent(self,3);
    }
    return originalPresent(self,mode);
}
bool ValidView(void* view){
    void** table=Value<void**>(view);if(!table)return false;
    return Value<void*>(table,24*4)==render+0x1C3120&&Value<void*>(table,25*4)==render+0x1C2E70&&Value<void*>(table,27*4)==render+0x1C3150&&Value<DWORD>(view,0x18)==0;
}
bool ResolveViews(){
    void* renderer=Class("Renderer");if(!renderer||Value<void*>(renderer)!=render+0x1E0468)return false;
    void* world=Value<void*>(renderer,0xF8);void* weapon=Value<void*>(renderer,0xFC);
    if(world==weapon||!ValidView(world)||!ValidView(weapon))return false;
    views[0].object=world;views[1].object=weapon;return true;
}

using NativeHudDraw=void(__thiscall*)(void*);NativeHudDraw originalHudDraw=nullptr;
bool crosshairProfile=false,gunCrosshairReady=false,gunCrosshairAvailable=true;
bool hideFixedInfantryCrosshair=false; // per-pair explicit crosshair-off policy
Vec gunCrosshairPoint{};float gunCrosshairRadius=0;
// BF2's HudInformationLayer constructor registers CrossHairColorAlpha with
// float storage at +184. Scope the change to this infantry canvas draw, so
// vehicles, deployment screens and the user's stored color remain native.
void DrawNativeHud(void* self){
    void* layer=pair&&crosshairProfile&&(gunCrosshairReady||hideFixedInfantryCrosshair)?Class("HudInformationLayer"):nullptr;
    float saved=0;bool hidden=false;
    if(Value<void*>(layer)==game+0x49E968&&Read(layer,0x184,saved)&&std::isfinite(saved)&&saved>=0&&saved<=1){
        const float zero=0;hidden=Copy(static_cast<BYTE*>(layer)+0x184,&zero,sizeof(zero));
    }
    originalHudDraw(self);
    if(hidden)Copy(static_cast<BYTE*>(layer)+0x184,&saved,sizeof(saved));
}
void __fastcall HudDrawHook(void* self,void*){
    if(scopePass)return;
    // The isolated HUD uses the same restored mono camera in either eye.
    // Reuse the first successful canvas for the second replay of this frame.
    // If isolation failed, retain native drawing rather than hiding the HUD.
    if(eyeActive&&activeEye==1&&CleanVR::ReuseHud())return;
    const double drawStart=FrameClockMs();
    const bool captured=eyeActive&&CleanVR::BeginHud();
    static unsigned reports=0;if(reports++<4)Log("HUD draw boundary: stereo=%d isolated=%d",eyeActive?1:0,captured?1:0);
    if(!captured){DrawNativeHud(self);CleanVR::RecordHudDraw(FrameClockMs()-drawStart);return;}
    // The UI canvas is mono. World-camera getters used by HUD widgets must
    // not inherit either asymmetric eye projection. Save the current eye,
    // draw the original canvas into the isolated target, then restore it.
    BYTE saved[2][0x418];bool copied=true;
    for(int i=0;i<2;++i)copied=Copy(saved[i],static_cast<BYTE*>(views[i].object)+0x18,sizeof(saved[i]))&&copied;
    if(copied){
        eyeActive=false;
        for(auto& v:views){Copy(static_cast<BYTE*>(v.object)+0x18,v.bytes,sizeof(v.bytes));originalProjection(v.object);}
    }
    DrawNativeHud(self);
    if(copied){for(int i=0;i<2;++i)Copy(static_cast<BYTE*>(views[i].object)+0x18,saved[i],sizeof(saved[i]));eyeActive=true;}
    CleanVR::EndHud();
    CleanVR::RecordHudDraw(FrameClockMs()-drawStart);
}
bool InstallHudDraw(){
    // BF2 HudManager table slot 7 -> +34F110 -> root canvas slot 20. The
    // 7-byte cmp has no relative branch and is safe in the trampoline.
    const BYTE entry[]={0x83,0x3D,0x58,0x73,0x98,0,0};
    const BYTE rootDraw[]={0x8B,0x8E,0xF4,0,0,0,0x8B,0x11,0x5E,0xFF,0x62,0x50};
    if(Value<void*>(game,0x526130+7*4)!=game+0x34F110||!Match(game+0x34F14B,rootDraw,sizeof(rootDraw)))return false;
    if(!Jump(game+0x34F110,entry,sizeof(entry),reinterpret_cast<void*>(&HudDrawHook),reinterpret_cast<void**>(&originalHudDraw)))return false;
    const BYTE alphaStorage[]={0x8D,0x86,0x84,1,0,0,0x50,0x8D,0x4D,0xE4,0x51,0x8B,0xCE};
    crosshairProfile=Value<void*>(game,0x6ACF1)==game+0x49ED78&&
        Match(game+0x6ACFE,alphaStorage,sizeof(alphaStorage))&&
        Value<void*>(game,0x49E968+10*4)==game+0x664E0;
    Log("AIM38 native infantry crosshair alpha profile=%d",crosshairProfile?1:0);
    Log("HUD draw isolation installed: BF2+34F110 native canvas -> shared OpenXR HUD");return true;
}

// BF2's deployment GUI is separate from the Flash frontend. Resolve the
// current HUD/root every time; never reuse an object after a map transition.
struct HudTarget{void* root=nullptr;void* pointer=nullptr;float left=0,top=0,width=0,height=0;};
bool hudInstalled=false;
bool Hud(HudTarget& target,bool interactive=true){
    if(!hudInstalled||FrontendActive())return false;
    void* hud=Class("HudManager");
    if(Value<void*>(hud)!=game+0x526130||Value<BYTE>(hud,0x248)!=1)return false;
    if(interactive){using Query=bool(__thiscall*)(void*);if(!reinterpret_cast<Query>(game+0x3513A0)(hud))return false;}
    target.root=Value<void*>(hud,0xF4);target.pointer=Value<void*>(target.root,0x20);
    if(Value<void*>(target.root)!=game+0x535410||Value<void*>(target.pointer)!=game+0x5493A8||Value<BYTE>(target.pointer,0x10)!=1)return false;
    target.left=Value<float>(target.root,4);target.top=Value<float>(target.root,8);target.width=Value<float>(target.root,12);target.height=Value<float>(target.root,16);
    return std::isfinite(target.left+target.top+target.width+target.height)&&target.width>=100&&target.height>=100&&target.width<=8192&&target.height<=8192;
}
using HudMove=bool(__thiscall*)(void*,void*,float,float);HudMove hudMove=nullptr;
using HudUpdate=bool(__thiscall*)(void*,float);HudUpdate hudUpdate=nullptr;
using HudDown=bool(__thiscall*)(void*,int,int,int);using HudUp=bool(__thiscall*)(void*,int,int);
struct PointerRequest{bool valid=false,pressed=false;float u=0,v=0;ULONGLONG tick=0;std::uint64_t serial=0;};
PointerRequest pointerRequest{};SRWLOCK pointerLock=SRWLOCK_INIT;
bool PointerSample(PointerRequest& out){AcquireSRWLockShared(&pointerLock);out=pointerRequest;ReleaseSRWLockShared(&pointerLock);DWORD process=0;GetWindowThreadProcessId(GetForegroundWindow(),&process);return out.valid&&GetTickCount64()-out.tick<=200&&process==GetCurrentProcessId();}
bool __fastcall HudMoveHook(void* self,void*,void* root,float x,float y){
    PointerRequest request;HudTarget target;
    if(PointerSample(request)&&Hud(target)&&target.root==root&&target.pointer==self){x=target.left+request.u*target.width;y=target.top+request.v*target.height;}
    return hudMove(self,root,x,y);
}
bool __fastcall HudUpdateHook(void* root,void*,float delta){
    static void* lastRoot=nullptr;static void* lastPointer=nullptr;static bool held=false;static std::uint64_t consumed=0;
    HudTarget target;PointerRequest request;
    if(Hud(target)&&target.root==root){
        if(lastRoot!=root||lastPointer!=target.pointer){lastRoot=root;lastPointer=target.pointer;held=false;consumed=0;}
        const bool fresh=PointerSample(request);
        if(fresh&&consumed!=request.serial){
            hudMove(target.pointer,root,target.left+request.u*target.width,target.top+request.v*target.height);
            if(request.pressed!=held){
                if(request.pressed)reinterpret_cast<HudDown>(game+0x42A2D0)(root,1,1,1);else reinterpret_cast<HudUp>(game+0x42A2E0)(root,1,1);
                held=request.pressed;Log("HUD pointer %s u=%.3f v=%.3f",held?"press":"release",request.u,request.v);
            }
            consumed=request.serial;
        }else if(!fresh&&held){reinterpret_cast<HudUp>(game+0x42A2E0)(root,1,1);held=false;}
    }else if(root==lastRoot){
        if(held&&Hud(target,false)&&target.root==root&&target.pointer==lastPointer)reinterpret_cast<HudUp>(game+0x42A2E0)(root,1,1);
        held=false;consumed=0;lastRoot=lastPointer=nullptr;
    }
    return hudUpdate(root,delta);
}
bool InstallHud(){
    const BYTE move[]={0x55,0x8B,0xEC,0x83,0xEC,8};const BYTE update[]={0x55,0x8B,0xEC,0xD9,0x45,8};
    const BYTE query[]={0x56,0x8B,0xF1,0x8B,6,0xFF,0x90,0xC4,0,0,0,0x84,0xC0,0x75,0x1D};
    const BYTE down[]={0x55,0x8B,0xEC,0x8B,0x51,0x20,0x5D,0xE9,0x14,0xEB,0xFF,0xFF};
    const BYTE up[]={0x55,0x8B,0xEC,0x8B,0x51,0x20,0x5D,0xE9,0x34,0xEB,0xFF,0xFF};
    if(!Match(game+0x3513A0,query,sizeof(query))||!Match(game+0x42A2D0,down,sizeof(down))||!Match(game+0x42A2E0,up,sizeof(up))||
       Value<void*>(game,0x526130+12*4)!=game+0x3513A0||Value<void*>(game,0x535410+19*4)!=game+0x3AD100||Value<void*>(game,0x5493A8+18*4)!=game+0x429000)return false;
    if(!Jump(game+0x429000,move,6,reinterpret_cast<void*>(&HudMoveHook),reinterpret_cast<void**>(&hudMove)))return false;
    if(!Jump(game+0x3AD100,update,6,reinterpret_cast<void*>(&HudUpdateHook),reinterpret_cast<void**>(&hudUpdate)))return false;
    hudInstalled=true;Log("HUD deployment pointer installed: native canvas coordinates and button edges");return true;
}

// BF2+21F801 calls the native 1P finalizer while ECX is the soldier and
// EBX is its selected inventory object. The caller binds bone 54 to that
// exact object at +21F86B immediately afterward. No draw-size classification.
using Finalizer=void(__thiscall*)(void*,float);Finalizer originalFinalize=nullptr;
void* handOwner=nullptr;void* handWeapon=nullptr;void* handRig=nullptr;int handSlot=-1;
HandBinding binding{};Mat palmRight{},palmLeft{};bool palms=false,wasSupport=false;
HandAimState handAim{};std::uint64_t handOrigin=0;std::int64_t handPoseTime=0;
unsigned handsApplied=0;const char* lastHandStatus=nullptr;
struct FireFrame {
    void* owner=nullptr;void* weapon=nullptr;void* rig=nullptr;int slot=-1;
    Mat camera{},nativeWeapon{},trackedWeapon{},native[70]{},rightPalm{},leftPalm{};
    HandBinding gripBinding{};bool palmsValid=false,supporting=false,akFamily=false;
    HandAimState aimState{};
    // Same contact for detailed 1P fingers and the connected 3P wrist.
    // Magazine poses are grip-local; charging poses are rifle-local.
    Mat leftContact[21]{};int contactKind=0;
    std::uint64_t frame=0,origin=0;ULONGLONG tick=0;
} fire;
SRWLOCK fireLock=SRWLOCK_INIT;
#include "ManualReload.inl"
HandTargets Targets(const Mat& camera,const Mat& body,const Runtime::Frame& sample,const Mat& rp,const Mat& lp,bool palmValid,bool support){
    // Level in world space first, then express the stable frame in rig space.
    // Level(local camera) would inherit body pitch/roll when transformed back.
    const Mat anchor=LevelInRig(camera,body);const auto& right=sample.hands[1];const auto& left=sample.hands[0];
    HandTargets target{};target.torso=anchor;target.head=Compose(anchor,reference,sample.head,Config().units);
    target.rightGrip=Compose(anchor,reference,right.grip,Config().units);target.rightAim=Compose(anchor,reference,right.aim,Config().units);target.rightValid=right.gripValid&&right.aimValid;
    target.leftValid=left.gripValid&&left.aimValid;if(target.leftValid){target.leftGrip=Compose(anchor,reference,left.grip,Config().units);target.leftAim=Compose(anchor,reference,left.aim,Config().units);}
    target.palmsValid=palmValid;target.rightPalm=rp;target.leftPalm=lp;target.supportPressed=left.squeeze>(support?.35f:.65f);
    target.frame=sample.id;target.origin=sample.origin;target.time=sample.time;
    if(ReloadHandsFree(target))target.supportPressed=false;
    target.wasSupporting=support;return target;
}
#include "GunCrosshair.inl"
#include "GunScope.inl"
bool ReadBodyHandTargets(void* owner,const Mat& nativeWorld,const Runtime::Frame& frame,Mat& camera,Mat& leftPalm,Mat& rightPalm,Vec wrists[2],bool& chargingContact,void*& selectedWeapon,bool& akSupport){
    chargingContact=false;selectedWeapon=nullptr;akSupport=false;
    FireFrame pose{};AcquireSRWLockShared(&fireLock);pose=fire;ReleaseSRWLockShared(&fireLock);
    if(!Config().hands||!referenceValid||!frame.focused||MenuActive()||pose.owner!=owner||!pose.weapon||
       Value<void*>(owner,0x288)!=pose.rig||Value<int>(owner,0x1E0)!=pose.slot||Value<void*>(pose.weapon,0x28)!=owner||
       pose.origin!=frame.origin||pose.frame>frame.id||frame.id-pose.frame>2||GetTickCount64()-pose.tick>150||
       !pose.gripBinding.valid||!pose.palmsValid||!frame.hands[0].gripValid||!frame.hands[0].aimValid||
       !frame.hands[1].gripValid||!frame.hands[1].aimValid)return false;
    // The working 1P pose is valid during equip too. rightReady gates binding
    // stabilization/support; waiting for it here swapped meshes for ~1 second.
    // Retain all owner/slot/freshness guards, and leave gameplay firing alone.
    Mat solved[70];bool supporting=false;
    const HandTargets target=Targets(pose.native[0],nativeWorld,frame,pose.rightPalm,pose.leftPalm,true,pose.supporting);
    if(!SolveHands(pose.native,pose.gripBinding,target,solved,supporting,&pose.aimState))return false;
    bool contactApplied=false;
    if(pose.contactKind&&(frame.hands[0].trigger>.40f||target.supportPressed)&&ReloadContactHeld(owner,pose.weapon,pose.contactKind)){
        const Mat& anchor=pose.contactKind==1?target.leftGrip:solved[54];
        Mat contact[21];for(int i=0;i<21;++i)contact[i]=Mul(pose.leftContact[i],anchor);
        if(!SolveLeftContact(pose.native,target,contact,solved))return false;
        contactApplied=true;
    }
    // wrist = wristFromPalm * palm, so palm = inverse(wristFromPalm) * wrist.
    Mat leftBinding=pose.leftPalm;
    if(contactApplied&&!PalmBinding(solved,7,leftBinding))return false;
    leftPalm=Mul(Inverse(leftBinding),solved[7]);rightPalm=Mul(Inverse(pose.rightPalm),solved[33]);
    wrists[0]=Position(solved[7]);wrists[1]=Position(solved[33]);
    chargingContact=contactApplied&&pose.contactKind==2;
    akSupport=pose.akFamily&&supporting&&!contactApplied;
    camera=pose.native[0];selectedWeapon=pose.weapon;return Rigid(camera)&&Rigid(leftPalm)&&Rigid(rightPalm);
}
void HandStatus(const char* why){if(lastHandStatus!=why){lastHandStatus=why;Log("HANDS %s",why);}}
bool Topology(void* rig){
    if(Value<int>(rig,0x14)!=70)return false;BYTE* meta=Value<BYTE*>(rig,8);if(!meta)return false;
    const int ids[]={0,1,2,3,4,5,6,7,28,29,30,31,32,33,54,69};const short parents[]={-1,-1,1,2,3,4,5,6,1,28,29,30,31,32,1,1};
    for(unsigned i=0;i<sizeof(ids)/sizeof(ids[0]);++i)if(Value<int>(meta,ids[i]*0x24)!=ids[i]||Value<short>(meta,ids[i]*0x24+6)!=parents[i])return false;
    for(int w:{7,33})for(int f=0;f<5;++f)for(int j=0;j<4;++j){const int bone=w+1+f*4+j;if(Value<int>(meta,bone*0x24)!=bone||Value<short>(meta,bone*0x24+6)!=(j?bone-1:w))return false;}
    return true;
}
void __cdecl ApplyHands(void* owner,void* weapon){
    if(!Config().hands)return;
    void* local=Soldier();
    if(!owner||owner!=local){static unsigned skipped=0;if(++skipped==1||skipped%18000==0)Log("HANDS waiting for local finalizer: calls=%u local=%p",skipped,local);return;} // before inspecting or writing any remote skeleton
    AcquireSRWLockExclusive(&fireLock);fire=FireFrame{};ReleaseSRWLockExclusive(&fireLock);
    Runtime::Frame sample{};if(!Runtime::Snapshot(sample)||!sample.focused||!referenceValid){wasSupport=false;handAim={};HandStatus("waiting for a focused OpenXR frame");return;}
    DWORD process=0;GetWindowThreadProcessId(GetForegroundWindow(),&process);if(process!=GetCurrentProcessId()||MenuActive()){wasSupport=false;handAim={};return;}
    if(Value<int>(owner,0x2B8)!=0||Value<void*>(owner,0x254)){wasSupport=false;handAim={};HandStatus("native 1P mode inactive");return;}
    void* rig=Value<void*>(owner,0x288);if(!weapon||!Topology(rig)){HandStatus("local rig/topology rejected");return;}
    // The selected inventory object comes from the verified caller; validate
    // its parent as well before publishing a gameplay firing pose.
    const bool weaponOwned=Value<void*>(weapon,0x28)==owner;
    Mat* output=Value<Mat*>(rig,0x10);Mat native[70],solved[70];if(!Copy(native,output,sizeof(native))){HandStatus("native pose unreadable");return;}
    for(int i=0;i<70;++i)if(!Rigid(native[i])){static unsigned rejected=0;if(rejected++<6)Log("HANDS invalid bone=%d pos=(%.4f %.4f %.4f) affine=(%.4f %.4f %.4f %.4f)",i,native[i].m[3][0],native[i].m[3][1],native[i].m[3][2],native[i].m[0][3],native[i].m[1][3],native[i].m[2][3],native[i].m[3][3]);HandStatus("native matrix rejected");return;}
    const auto& right=sample.hands[1];const auto& left=sample.hands[0];if(!right.gripValid||!right.aimValid){wasSupport=false;handAim={};HandStatus("right aim/grip tracking unavailable");return;}
    if(handOrigin!=sample.origin||sample.time<handPoseTime||sample.time-handPoseTime>150000000){wasSupport=false;handAim={};}
    handOrigin=sample.origin;handPoseTime=sample.time;
    const int slot=Value<int>(owner,0x1E0);
    const bool akFamily=ReloadBind(owner,weapon,rig,slot,sample);
    if(owner!=handOwner||rig!=handRig){palms=false;}
    if(owner!=handOwner||weapon!=handWeapon||rig!=handRig||slot!=handSlot){
        handOwner=owner;handWeapon=weapon;handRig=rig;handSlot=slot;binding=HandBinding{};wasSupport=false;handAim={};
        Log("HANDS ownership local=%p selected=%p slot=%d rig=%p bones=70 parent=%d",owner,weapon,slot,rig,weaponOwned?1:0);
    }
    const bool settled=binding.rightReady;binding.Update(native,GetTickCount64());
    if(!palms||!settled){Mat rp{},lp{};if(PalmBinding(native,33,rp)&&PalmBinding(native,7,lp)){palmRight=rp;palmLeft=lp;palms=true;}}
    if(!settled&&binding.rightReady)Log("HANDS firing grip settled after equip animation");
    Mat body{};if(!Read(owner,0x88,body)||!Rigid(body)){HandStatus("local body transform rejected");return;}
    HandTargets target=Targets(native[0],body,sample,palmRight,palmLeft,palms,wasSupport);
    Mat basis[70];ReloadHandBasis(native,basis);
    if(!SolveHands(basis,binding,target,solved,wasSupport,&handAim)){HandStatus("IK rejected an invalid or unreachable pose");return;}
    int contactKind=0;Mat contactLocal[21]{};
    ApplyReload(sample,target,native,basis,solved,contactKind,contactLocal,wasSupport);
    if(!Copy(output+2,solved+2,68*sizeof(Mat))){HandStatus("native pose write failed");return;}
    HandStatus("native arms and weapon pose applied");++handsApplied;
    if(handsApplied==1||handsApplied%1800==1)Log("HANDS applied=%u frame=%llu right=(%.3f %.3f %.3f) left=(%.3f %.3f %.3f) support=%d",handsApplied,sample.id,Position(target.rightGrip).x,Position(target.rightGrip).y,Position(target.rightGrip).z,Position(target.leftGrip).x,Position(target.leftGrip).y,Position(target.leftGrip).z,wasSupport?1:0);
    if(weaponOwned){
        FireFrame next{};next.owner=owner;next.weapon=weapon;next.rig=rig;next.slot=slot;next.camera=Mul(native[0],body);next.nativeWeapon=Mul(native[54],body);next.trackedWeapon=Mul(solved[54],body);next.frame=sample.id;next.origin=sample.origin;next.tick=GetTickCount64();
        memcpy(next.native,basis,sizeof(basis));next.gripBinding=binding;next.rightPalm=palmRight;next.leftPalm=palmLeft;next.palmsValid=palms;next.supporting=wasSupport;next.aimState=handAim;
        next.contactKind=contactKind;next.akFamily=akFamily;memcpy(next.leftContact,contactLocal,sizeof(contactLocal));
        AcquireSRWLockExclusive(&fireLock);fire=next;ReleaseSRWLockExclusive(&fireLock);
    }
}
__declspec(naked) void FinalizeHook(){__asm{
    push ebp
    mov ebp,esp
    push ebx
    push ecx
    push dword ptr [ebp+8]
    call dword ptr [originalFinalize]
    push dword ptr [ebp-4]
    push dword ptr [ebp-8]
    call ApplyHands
    add esp,8
    pop ecx
    pop ebx
    pop ebp
    ret 4
}}
using Launch=const Mat*(__thiscall*)(void*);Launch originalLaunch=nullptr;
thread_local Mat launchResult{};
const Mat* __fastcall LaunchHook(void* self,void*){
    const Mat* native=originalLaunch(self);if(!self)return native;
    const void* launchedWeapon=Value<void*>(static_cast<BYTE*>(self)-4);
    AcquireSRWLockShared(&fireLock);
    // This hook also receives other soldiers' weapons. Reject those before
    // copying the entire bone/contact snapshot or looking up the local player.
    if(!fire.weapon||launchedWeapon!=fire.weapon){ReleaseSRWLockShared(&fireLock);return native;}
    FireFrame pose=fire;ReleaseSRWLockShared(&fireLock);
    if(!self||!pose.weapon||pose.owner!=Soldier()||Value<int>(pose.owner,0x1E0)!=pose.slot||Value<void*>(static_cast<BYTE*>(self)-4)!=pose.weapon||Value<void*>(pose.weapon,0x28)!=pose.owner||GetTickCount64()-pose.tick>150||MenuActive())return native;
    void* table=Value<void*>(self);if(table!=game+0x4F4FB0&&table!=game+0x4F77A0&&table!=game+0x4F7EE0&&table!=game+0x4FA8F8)return native;
    DWORD process=0;GetWindowThreadProcessId(GetForegroundWindow(),&process);
    Runtime::Frame f{};if(process!=GetCurrentProcessId()||!Runtime::Snapshot(f)||!f.focused||!f.hands[1].aimValid||!f.hands[1].gripValid||f.origin!=pose.origin||f.id<pose.frame||f.id-pose.frame>1||!LocalAlive()||Value<int>(pose.owner,0x2B8)!=0||Value<void*>(pose.owner,0x254)||Value<void*>(pose.owner,0x288)!=pose.rig)return native;
    // Input can fire before the next native animation callback. Recompute
    // from this frame's already-located sample using the last verified local
    // rig/binding; do not locate a second, different OpenXR pose here.
    if(f.id!=pose.frame){
        Mat solved[70],body{};bool support=false;
        if(!Read(pose.owner,0x88,body)||!Rigid(body))return native;
        const HandTargets target=Targets(pose.native[0],body,f,pose.rightPalm,pose.leftPalm,pose.palmsValid,pose.supporting);
        if(!SolveHands(pose.native,pose.gripBinding,target,solved,support,&pose.aimState))return native;
        pose.camera=Mul(pose.native[0],body);pose.trackedWeapon=Mul(solved[54],body);pose.frame=f.id;
    }
    // +228 sources the already-bound equipped object. Map relative to that
    // object, not the camera: this avoids applying the tracked turn twice.
    if(Value<BYTE>(pose.weapon,0x228)){
        Mat source{},bound{};if(!Read(native,0,source)||!Read(pose.weapon,0x88,bound)||!MapLaunch(source,pose.camera,bound,pose.trackedWeapon,true,launchResult))return native;
        return &launchResult;
    }
    Mat source{};if(!Read(native,0,source)||!MapLaunch(source,pose.camera,pose.nativeWeapon,pose.trackedWeapon,false,launchResult))return native;
    static unsigned n=0;if(++n<=8)Log("FIRE owned LaunchMatrix follows tracked gun; frame=%llu",pose.frame);return &launchResult;
}
bool InstallHands(){
    const BYTE call[]={0xE8,0x9A,0xE0,0xFF,0xFF};const BYTE finalize[]={0x55,0x8B,0xEC,0x81,0xEC,0xC4,0,0,0,0x57,0x8B,0xF9};
    const BYTE bindingCall[]={0x8B,0x13,0x50,0x8B,0xCB,0xFF,0x92,0xE8,0,0,0};
    if(!Match(game+0x21F801,call,5)||!Match(game+0x21D8A0,finalize,sizeof(finalize))||!Match(game+0x21F866,bindingCall,sizeof(bindingCall)))return false;
    DWORD old;if(!VirtualProtect(game+0x21F801,5,PAGE_EXECUTE_READWRITE,&old))return false;originalFinalize=reinterpret_cast<Finalizer>(game+0x21D8A0);
    *reinterpret_cast<LONG*>(game+0x21F802)=(LONG)(reinterpret_cast<BYTE*>(&FinalizeHook)-(game+0x21F806));DWORD ignored;VirtualProtect(game+0x21F801,5,old,&ignored);FlushInstructionCache(GetCurrentProcess(),game+0x21F801,5);
    const BYTE launch[]={0x55,0x8B,0xEC,0x83,0xEC,0x10};const bool fireHook=Jump(game+0x220950,launch,sizeof(launch),reinterpret_cast<void*>(&LaunchHook),reinterpret_cast<void**>(&originalLaunch));
    Log("HANDS native local-owner hook installed at BF2+21F801; owned launch adapter=%d",fireHook?1:0);return true;
}
}
void ResetBodyResources(){bodyReady=bodyDepthShared=false;ReleaseBodyDepth();ReleaseBodyMasks();}
bool Install(){
    if(attempted)return installed;attempted=true;game=reinterpret_cast<BYTE*>(GetModuleHandleA(nullptr));render=reinterpret_cast<BYTE*>(GetModuleHandleA("RendDX9.dll"));if(!game||!render)return false;
    // Renderer slot 10, after BF2+533AB installs this frame's player camera.
    // Slot 11 (+2C2E0) has a different ABI and is NOT the frame entry point.
    const BYTE scene[]={0x55,0x8B,0xEC,0x83,0xE4,0xF8};
    const BYTE present[]={0x55,0x8B,0xEC,0x81,0xEC,0xF8,0,0,0};
    const BYTE presentGate[]={0x80,0xBB,1,1,0,0,0,0x74,9};
    const BYTE skipPresent[]={0x8B,0x45,8,0x83,0xF8,3,0x75,0x0F,0xB0,1,0x88,0x83,1,1,0,0,0x5B,0x8B,0xE5,0x5D,0xC2,4,0};
    const BYTE endFlags[]={0xC6,0x86,1,1,0,0,0,0xC6,0x86,0,1,0,0,0};
    const BYTE swapPresent[]={0x52,0x6A,0,0x6A,0,0x6A,0,0x6A,0,0x50,0xFF,0x51,0x0C};
    const BYTE projection[]={0x55,0x8B,0xEC,0x51,0x56,0x8B,0xF1};const BYTE setter[]={0x55,0x8B,0xEC,0x8B,0x45,0x08};
    const BYTE projProfile[]={0xB8,1,0,0,0,0x84,0x86,0x28,4,0,0};const BYTE camProfile[]={0x50,0x8D,0x4E,0x3C};
    if(!Match(render+0x300C0,scene,sizeof(scene))||Value<void*>(render,0x1E0468+10*4)!=render+0x300C0||Value<void*>(render,0x1E0468+18*4)!=render+0xC9C0||Value<void*>(render,0x1E0468+53*4)!=render+0xC9D0||!Match(render+0x1C2FA7,projProfile,sizeof(projProfile))||!Match(render+0x1C3129,camProfile,sizeof(camProfile))){Log("Unsupported BF2/RendDX9 native profile; keeping desktop rendering");return false;}
    if(!Match(render+0x5A50,present,sizeof(present))||!Match(render+0x5A5C,presentGate,sizeof(presentGate))||!Match(render+0x5A6E,skipPresent,sizeof(skipPresent))||!Match(render+0x566A,endFlags,sizeof(endFlags))||!Match(render+0x5AAC,swapPresent,sizeof(swapPresent))||!Match(render+0x5B9D,swapPresent,sizeof(swapPresent))||Value<void*>(render,0x1E0468+52*4)!=render+0x5A50){Log("Native swap-chain Present profile rejected; stereo replay omitted");return false;}
    if(!Jump(render+0x5A50,present,sizeof(present),reinterpret_cast<void*>(&PresentHook),reinterpret_cast<void**>(&originalPresent)))return false;
    if(!Jump(render+0x1C3120,setter,sizeof(setter),reinterpret_cast<void*>(&SetCamera),reinterpret_cast<void**>(&originalSetter)))return false;
    if(!Jump(render+0x1C2FA0,projection,sizeof(projection),reinterpret_cast<void*>(&BuildProjection),reinterpret_cast<void**>(&originalProjection)))return false;
    if(!Jump(render+0x300C0,scene,sizeof(scene),reinterpret_cast<void*>(&SceneHook),reinterpret_cast<void**>(&originalScene)))return false;
    if(!InstallHud())Log("HUD deployment pointer profile rejected");
    if(!InstallHudDraw())Log("HUD draw profile rejected; keeping native canvas");
    if(Config().hands&&!InstallHands())Log("HANDS profile rejected; native hands disabled");
    reloadProfile=InstallReload();reloadSoundProfile=InstallReloadSound();
    Log("MRELOAD21 native ammo gate=%d enabled=%d chargingMode=%d sounds=%d profiles=%u; left trigger grab, B fallback",reloadProfile?1:0,Config().manualReload?1:0,Config().chargingHandle,reloadSoundProfile?1:0,static_cast<unsigned>(sizeof(reloadWeapons)/sizeof(reloadWeapons[0])));
    vehicleProfile=InstallVehicle();Log("VEHICLE r7 native occupied-seat profile=%d; stock pilot/turret roles enabled",vehicleProfile?1:0);
    bodyProfile=InstallFullBody();Log("FULLBODY2 native mesh submission profile=%d",bodyProfile?1:0);
    bodyDepthProfile=bodyProfile&&InstallBodyDepth();Log("FULLBODY7 weapon depth boundary profile=%d",bodyDepthProfile?1:0);
    installed=true;Log("PROFILE r3: RendDX9+300C0 frame entry and +5A50 native swap-chain Present connected");return true;
}
bool FlushDeferredPresent(){
    if(!deferredRenderer||!originalPresent)return false;
    void* renderer=deferredRenderer;const unsigned mode=deferredPresentMode;deferredRenderer=nullptr;
    // The skip-mode call marked this renderer presented. Re-open exactly that
    // boundary, then let the native path present its selected chain and handle
    // device loss/errors. Do not substitute a Device::Present on another chain.
    const BYTE pending=0;if(!Copy(static_cast<BYTE*>(renderer)+0x101,&pending,1)){Log("PRESENT could not reopen deferred native boundary");return true;}
    if(!originalPresent(renderer,mode)){static unsigned failed=0;if(++failed<=8)Log("PRESENT native deferred present failed #%u",failed);}return true;
}
bool FrontendActive(){if(!game)return false;void* menu=Value<void*>(game,0x587358);const auto table=Value<void*>(menu);return menu&&(table==game+0x4A29E8||table==game+0x4A2AF8)&&Value<BYTE>(menu,8)==1;}
bool MenuActive(){
    const bool frontend=FrontendActive(),deployment=DeploymentActive();
    if(game){
        void* hud=Class("HudManager");const int mode=Value<void*>(hud)==game+0x526130?Value<int>(hud,0x250):-1;
        static int prior=-2;static ULONGLONG last=0;
        if(mode!=prior&&GetTickCount64()-last>500){
            prior=mode;last=GetTickCount64();void* root=Value<void*>(hud,0xF4);void* pointer=Value<void*>(root,0x20);
            Log("HUD mode=%d enabled=%u frontend=%d deployment=%d root=%p table=%p pointer=%p table=%p",mode,(unsigned)Value<BYTE>(hud,0x248),frontend?1:0,deployment?1:0,root,Value<void*>(root),pointer,Value<void*>(pointer));
        }
    }return frontend||deployment;
}
bool DeploymentActive(){HudTarget target;return Hud(target);}
void PublishPointer(bool valid,float u,float v,bool pressed,std::uint64_t serial){
    valid=valid&&std::isfinite(u+v)&&u>=0&&u<=1&&v>=0&&v<=1;
    AcquireSRWLockExclusive(&pointerLock);pointerRequest={valid,valid&&pressed,u,v,GetTickCount64(),serial};ReleaseSRWLockExclusive(&pointerLock);
}
bool LocalAlive(){void* player=Player();return player&&Soldier()&&!Value<BYTE>(player,0xE8);}
bool CameraReady(){return installed&&LocalAlive()&&ResolveViews();}
void Recenter(const Runtime::Frame& frame){if(!frame.valid)return;CancelManualReload();handAim={};wasSupport=false;handPoseTime=0;reference=Level(frame.head);referenceValid=true;vehicleSample={};vehicleTick=0;vehicleViewValid=false;Log("TRACKING shared head/eye/hand reference reset frame=%llu",frame.id);}
Mat Reference(){return reference;}
void CancelManualReload(){
    AcquireSRWLockExclusive(&reloadLock);reloadInput.enabled=false;
    ReloadAudio::Stop();reload.soundSuppressed=false;
    if(ReloadActive())ReloadCancel("gameplay ended or recentered",GetTickCount64());
    reload.request=false;reload.feedback=0;reload.feedbackMs=0;
    // A respawn can reuse soldier/weapon addresses while replacing components.
    // Reacquire the ammo interface and idle socket after gameplay resumes.
    reload.buttonReload=reload.buttonReloadSeen=false;reload.buttonIdleTick=0;
    reload.ammo={};reload.restValid=false;reload.handRestValid=false;reload.visualKind=0;reload.poseTick=0;
    reload.magazineLatch=0;reload.magazineLatchTick=0;
    reload.shellPermit=false;reload.actionPending=false;reload.actionPoseUntil=0;reload.pumpContact=false;
    reload.pumpAimValid=false;reload.pumpAimReturnTick=0;
    for(auto& m:reloadActionMemory)m={};
    ReleaseSRWLockExclusive(&reloadLock);
}
ManualReloadInput ManualReloadControls(const Runtime::Frame& f,bool enabled,bool grab,bool fallback){
    ManualReloadInput result{};AcquireSRWLockExclusive(&reloadLock);
    const auto now=GetTickCount64();
    const bool wasFallback=reloadInput.bypass;
    const bool keptGrip=reloadInput.enabled&&reloadInput.supportGrip&&reloadInput.origin==f.origin&&now>=reloadInput.tick&&now-reloadInput.tick<150;
    const bool grip=f.hands[0].squeeze>(keptGrip?.35f:.65f);
    reloadInput={enabled&&Config().manualReload&&f.focused&&f.hands[0].gripValid&&f.hands[0].aimValid&&f.hands[1].gripValid&&f.hands[1].aimValid,grab,fallback,grip,f.origin,f.id,now};
    if(!reloadInput.enabled||fallback){
        if(fallback&&!wasFallback){
            ReloadCancel("B button reload",now);
            reload.buttonReload=true;reload.buttonReloadSeen=reload.ammo.state>=2;
            reload.buttonReloadTick=now;reload.buttonIdleTick=0;
            if(reload.profile)Log("MRELOAD35 button reload started weapon=%s nativeState=%d rounds=%d",reload.profile->name,reload.ammo.state,reload.ammo.rounds);
        }else if(!fallback&&(ReloadActive()||reload.actionPending))ReloadCancel("controls/tracking unavailable",now);
        if(fallback){reload.bypassUntil=now+(reload.profile&&reload.profile->reloadAmount?15000:7000);reload.buttonReloadTick=now;}
        reload.lastGrab=grab;
    }
    if(reloadInput.enabled&&!fallback){
        result.reloadPulse=reload.request&&ReloadOwned(now);reload.request=false;
        result.blockFire=ReloadBlocksFire()&&ReloadOwned(now);
        result.haptic=reload.feedback;result.hapticMs=reload.feedbackMs;
    }
    // Event-only feedback for the reported post-reload fire/support lock.
    // No per-frame diagnostic sampling or additional native fire queries.
    static bool fireWas=false,supportWas=false;
    const bool fireDown=f.hands[1].trigger>.55f,supportDown=reloadInput.supportGrip;
    if(reloadInput.enabled&&reload.supported&&((fireDown&&!fireWas)||(supportDown&&!supportWas)))
        Log("MRELOAD31 input weapon=%s fire=%d support=%d grab=%d phase=%s blockFire=%d pending=%d nativeState=%d rounds=%d",reload.profile->name,
            fireDown?1:0,supportDown?1:0,grab?1:0,ReloadPhaseName(),result.blockFire?1:0,reload.actionPending?1:0,reload.ammo.state,reload.ammo.rounds);
    fireWas=fireDown;supportWas=supportDown;
    reload.feedback=0;reload.feedbackMs=0;ReleaseSRWLockExclusive(&reloadLock);return result;
}
VehicleInput Vehicle(const Runtime::Frame& f,float yawStep,float pitchStep){
    VehicleInput out;VehicleSample s;if(!ReadVehicle(s))return out;
    out.role=s.role;out.seat=reinterpret_cast<std::uintptr_t>(s.seat);
    if(!f.valid||!f.focused||!vehicleViewValid||!SameVehicle(s,vehicleSample)||!vehicleTick||GetTickCount64()-vehicleTick>150||!Tuning().headAim)return out;
    if(!AimedSeat(s.role))return out;
    // Stick motion changes the common aiming/view center. Adding raw mouse
    // counts to head feedback would make that feedback undo the stick input.
    if(std::isfinite(yawStep+pitchStep)){
        if(s.role==VehicleRole::Aimed)vehicleYawOffset=std::remainder(vehicleYawOffset+std::fmax(-.175f,std::fmin(.175f,yawStep)),6.283185307f);
        vehiclePitchOffset=std::fmax(-1.3962634f,std::fmin(1.3962634f,vehiclePitchOffset+std::fmax(-.175f,std::fmin(.175f,pitchStep))));
        const Vec position=Position(vehicleBase);vehicleBase=VehicleHeading(vehicleSample.world,vehicleSample.aircraft);SetPosition(vehicleBase,position);
    }
    // Evaluate in the chassis frame: gun axes must still work on slopes and
    // when a helicopter banks. No accumulated error at native traverse limits.
    const Mat inv=Inverse(vehicleSample.world);
    const Mat a=Mul(vehicleNative,inv),b=Mul(Compose(vehicleBase,reference,f.head,Config().units),inv);
    const Vec forward=Row(a,2),target=Row(b,2);
    out.yawError=std::remainder(std::atan2(target.x,target.z)-std::atan2(forward.x,forward.z),6.283185307f);
    out.pitchError=std::atan2(target.y,std::hypot(target.x,target.z))-std::atan2(forward.y,std::hypot(forward.x,forward.z));
    out.aimValid=std::isfinite(out.yawError+out.pitchError);return out;
}
bool BeginPair(const Runtime::Frame& frame){
    if(pair||!installed||!frame.valid||!frame.shouldRender||MenuActive()||!LocalAlive())return false;
    if(!referenceValid)Recenter(frame);if(!ResolveViews()){static bool warned=false;if(!warned){warned=true;Log("STEREO waiting for verified native world/weapon render views");}return false;}
    for(auto& v:views){if(!Copy(v.bytes,static_cast<BYTE*>(v.object)+0x18,sizeof(v.bytes))||!Read(v.object,0x3C,v.source)||!Rigid(v.source))return false;
        v.nearZ=Value<float>(v.object,0x28);v.farZ=v.nearZ+Value<float>(v.object,0x2C);
        if(!std::isfinite(v.nearZ+v.farZ)||v.nearZ<=0||v.farZ<=v.nearZ)return false;
    }
    // The native renderer copies WORLD -> WEAPON at +30215. Its weapon view
    // still contains the previous frame here. Both eyes must use the fresh
    // world camera, including on the very first frame after deployment.
    views[1].source=views[0].source;
    UpdateVehicleView(views[0].source);
    static unsigned frames=0;if(++frames<=3||frames%1800==1)Log("CAMERA native frame=%llu position=(%.3f %.3f %.3f) world=%p weapon=%p",frame.id,Position(views[0].source).x,Position(views[0].source).y,Position(views[0].source).z,views[0].object,views[1].object);
    pairFrame=frame;pair=true;PrepareGunCrosshair();PrepareGunScope();BeginFullBodyPair();
    bodyDepthShared=bodyReady&&bodyDepthProfile&&Tuning().bodyWeaponDepth;
    // Retained world depth is meaningful only with the SAME near/far mapping.
    // Keep the world's established precision instead of moving its near plane.
    if(bodyDepthShared)views[1].farZ=views[0].farZ;
    return true;
}
bool BeginEye(int e){
    if(!pair||e<0||e>1)return false;eyeActive=false;scopePass=false;
    for(auto& v:views)if(!Copy(static_cast<BYTE*>(v.object)+0x18,v.bytes,sizeof(v.bytes)))return false;
    activeEye=e;const auto& f=pairFrame.views[e].fov;
    Mat trackedEye=pairFrame.eyes[e];
    if(vehicleViewValid&&AimedSeat(vehicleSample.role)&&Tuning().gunnerPositionLock){
        // Lock the head center to the live native seat camera. Preserve each
        // tracked eye's offset from that center, including rotated stereo IPD.
        SetPosition(trackedEye,Add(Position(reference),Sub(Position(trackedEye),Position(pairFrame.head))));
    }
    for(int i=0;i<2;++i){auto& v=views[i];v.eye=Compose(TrackingBase(v.source),reference,trackedEye,Config().units);
        const float n=bodyDepthShared?views[0].nearZ:(i==1?std::min(v.nearZ,.006f):v.nearZ);
        if(!Projection(std::tan(f.angleLeft),std::tan(f.angleRight),std::tan(f.angleUp),std::tan(f.angleDown),n,v.farZ,v.projection)||!Rigid(v.eye))return false;
        const float vertical=std::max(std::abs(std::tan(f.angleUp)),std::abs(std::tan(f.angleDown))),horizontal=std::max(std::abs(std::tan(f.angleLeft)),std::abs(std::tan(f.angleRight)));
        v.cullFov=2*std::atan(vertical);v.aspect=vertical/horizontal;v.eyeNear=n;ApplyLens(v);
    }
    eyeActive=true;BeginFullBodyEye();
    static unsigned reports=0;if(reports<4){++reports;const auto& p=views[0].projection;const Vec pos=Position(views[0].eye);Log("EYE index=%d frame=%llu position=(%.4f %.4f %.4f) projection=(%.4f %.4f %.4f %.4f)",e,pairFrame.id,pos.x,pos.y,pos.z,p.m[0][0],p.m[1][1],p.m[2][0],p.m[2][1]);}
    for(auto& v:views){originalSetter(v.object,&v.eye);BuildProjection(v.object,nullptr);}return true;
}
void EndEye(){EndFullBodyEye();}
void EndPair(){scopePass=scopeReady=false;eyeActive=false;gunCrosshairReady=hideFixedInfantryCrosshair=false;bodyReady=bodyDepthShared=false;ReleaseBodyDepth();activeEye=-1;if(!pair)return;for(auto& v:views)Copy(static_cast<BYTE*>(v.object)+0x18,v.bytes,sizeof(v.bytes));pair=false;}
#include "GunScopeCamera.inl"
bool EyeActive(){return eyeActive;}
Mat EyeProjection(){return eyeActive?views[0].projection:Identity();}
void GunCrosshairAvailable(bool available){gunCrosshairAvailable=available;if(!available)gunCrosshairReady=false;}
bool GunCrosshair(float& x,float& y,float& halfWidth,float& halfHeight){
    if(!pair||!eyeActive||!gunCrosshairReady)return false;
    const Vec p=Point(gunCrosshairPoint,Inverse(views[0].eye));
    if(!std::isfinite(p.x+p.y+p.z)||p.z<=views[0].eyeNear)return false;
    const Mat& projection=views[0].projection;
    x=p.x/p.z*projection.m[0][0]+projection.m[2][0];
    y=p.y/p.z*projection.m[1][1]+projection.m[2][1];
    halfWidth=gunCrosshairRadius*projection.m[0][0];
    halfHeight=gunCrosshairRadius*projection.m[1][1];
    return std::isfinite(x+y+halfWidth+halfHeight)&&std::abs(x)<1&&std::abs(y)<1;
}
bool HudPlacement(Mat& pose,float& width,float& height){
    if(!pair)return false;
    const auto tuning=Tuning();
    const float fov=tuning.hudFov*.01745329252f,aspect=Value<float>(views[0].bytes,0x30-0x18);
    if(!std::isfinite(fov+aspect)||fov<.2f||fov>2.8f||aspect<.2f||aspect>3.f)return false;
    // Use the native camera's heading/pitch in the common tracking basis.
    // Physical head rotation looks around this canvas, just as it looks
    // around the vehicle. A distant plane limits reticle vergence.
    const float distance=tuning.hudDepth;
    pose=Mul(Mul(views[0].source,Inverse(TrackingBase(views[0].source))),reference);
    SetPosition(pose,Position(pairFrame.head));SetPosition(pose,Point({0,0,distance},pose));
    height=2*distance*std::tan(fov*.5f)*tuning.hudScale;width=height/aspect;return Rigid(pose);
}
}}
