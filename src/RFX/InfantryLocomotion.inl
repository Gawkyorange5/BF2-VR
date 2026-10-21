// BF2142VR's native-turn policy (MIT), independently profiled for BF2 1.5.
// Only the local first-person soldier's normal input update is adapted.
// No soldier transform, physics position, remote player or global input map is written.
// The optional wingsuit adds local velocity after the native update; collision and damage remain native.
bool locomotionProfile=false;
using InfantryUpdate=void(__thiscall*)(void*,void*,const void*,float,unsigned);
using YawRecoil=float(__thiscall*)(void*);
InfantryUpdate originalInfantryUpdate=nullptr;
YawRecoil originalYawRecoil=nullptr;
struct FootCommand {
    void* soldier=nullptr;void* identity=nullptr;Mat head{};Vec stick{};
    bool headMove=false,room=false;float radius=.18f,snap=0,smoothRate=0;
    std::uint64_t frame=0,origin=0;ULONGLONG tick=0;
    bool wingsuit=false,wingPose=false;
} footCommand;
SRWLOCK footLock=SRWLOCK_INIT;
PVOID volatile footOwnerPublished=nullptr;
struct RoomFollow {
    void* owner=nullptr;void* identity=nullptr;std::uint64_t origin=0;
    Vec position{},direction{};float remaining=0;ULONGLONG tick=0;
    bool following=false,valid=false;
} roomFollow;
// Active only inside a verified, local soldier input update on this thread.
__declspec(thread) void* inputSoldier=nullptr;
__declspec(thread) float inputHeadTurn=0,inputSnapTurn=0,inputPresentationTurn=0;
__declspec(thread) bool inputTurnConsumed=false;
float Wrapped(float radians){return std::remainder(radians,6.283185307f);}
float Heading(const Mat& m){return std::atan2(m.m[2][0],m.m[2][2]);}
#include "InfantryPresentation.inl"
bool FootOwner(void*& owner,void*& identity){
    if(!game||!referenceValid||!LocalAlive())return false;
    void* player=Player();identity=Value<void*>(player,0xCC);owner=Value<void*>(identity,4);
    return owner&&Value<void*>(owner)==game+0x4E80F8&&
        Value<void*>(Value<void*>(player,0x80),4)==owner&&
        !(Value<unsigned>(owner,4)&1)&&!Value<void*>(owner,0x28)&&
        !Value<void*>(owner,0x254)&&Value<int>(owner,0x2B8)==0;
}
#include "Wingsuit.inl"
void ClearFootCommand(){
    InterlockedExchangePointer(&footOwnerPublished,nullptr);
    AcquireSRWLockExclusive(&footLock);footCommand={};footPresentation={};ReleaseSRWLockExclusive(&footLock);
    roomFollow={};wingGesture={};
}
void RotateReference(float headTurn,float snapTurn,const Mat& head){
    const Vec origin=Position(reference),pivot=Position(head);
    if(std::abs(headTurn)>1.e-6f){
        reference=Mul(reference,Yaw(headTurn));SetPosition(reference,origin);
    }
    // Snap around the player's actual head, not the centre of their play area.
    if(std::abs(snapTurn)>1.e-6f)
        SetPosition(reference,Sub(pivot,Direction(Sub(pivot,Position(reference)),Yaw(-snapTurn))));
}
float __fastcall InfantryYawHook(void* soldier,void*){
    const auto caller=_ReturnAddress();const float recoil=originalYawRecoil(soldier);
    if(caller!=game+0x1AE328||soldier!=inputSoldier||inputTurnConsumed)return recoil;
    inputTurnConsumed=true;
    return recoil+(inputHeadTurn+inputSnapTurn+inputPresentationTurn)*57.295779513f;
}
void __fastcall InfantryUpdateHook(void* soldier,void*,void* context,const void* input,float seconds,unsigned flags){
    // This native entry also updates AI/remote soldiers. Reject them before
    // class-manager lookups, locks, matrices or foreground-window queries.
    if(inputSoldier||soldier!=InterlockedCompareExchangePointer(&footOwnerPublished,nullptr,nullptr)){
        originalInfantryUpdate(soldier,context,input,seconds,flags);return;
    }
    void* owner=nullptr;void* identity=nullptr;
    if(!FootOwner(owner,identity)||owner!=soldier){originalInfantryUpdate(soldier,context,input,seconds,flags);return;}
    FootCommand command{};FootPresentation presentation{};AcquireSRWLockShared(&footLock);command=footCommand;presentation=footPresentation;ReleaseSRWLockShared(&footLock);
    const auto now=GetTickCount64();DWORD foreground=0;GetWindowThreadProcessId(GetForegroundWindow(),&foreground);
    // A suspended app, menu, death, seat change or old XR sample cannot keep moving.
    if(command.soldier!=owner||command.identity!=identity||!command.tick||now<command.tick||now-command.tick>150||
        foreground!=GetCurrentProcessId()||!std::isfinite(seconds)||seconds<=0||seconds>.1f){
        roomFollow={};originalInfantryUpdate(soldier,context,input,seconds,flags);return;
    }
    // BF2 copies exactly 0x46 dwords: 64 float axes followed by validity masks.
    struct Packet {float axis[64];unsigned mask[6];} packet{};
    static_assert(sizeof(Packet)==0x118,"BF2 1.5 input packet size");
    if(!Copy(&packet,input,sizeof(packet))){originalInfantryUpdate(soldier,context,input,seconds,flags);return;}
    if(InventoryFirePolicy(owner,packet.axis[10],packet.axis[11]))packet.mask[0]|=(1u<<10)|(1u<<11);
    Mat world{};const float nativeYaw=(Value<float>(owner,0x224)+Value<float>(owner,0x240))*.01745329252f;
    if(!Read(owner,0x88,world)||!Rigid(world)||!std::isfinite(nativeYaw)){originalInfantryUpdate(soldier,context,input,seconds,flags);return;}
    // The presented heading belongs to this XR request. Native prediction
    // can call the input update more than once: converge to an absolute target
    // instead of integrating the same new turn again on each replay.
    bool presented=presentation.enabled&&presentation.valid&&presentation.owner==owner&&presentation.identity==identity&&
        presentation.origin==command.origin&&presentation.generation==referenceGeneration&&
        presentation.frame==command.frame&&now>=presentation.mouseUntil;
    inputPresentationTurn=presented?Wrapped(presentation.yaw+Heading(reference)-nativeYaw):0.f;
    // Physical mouse use yields to native look until it has been idle for
    // 150ms. Never cancel or repeatedly re-add somebody's mouse input.
    const bool mouseLook=(packet.mask[0]&(1u<<4))&&std::isfinite(packet.axis[4])&&std::abs(packet.axis[4])>1.e-6f;
    if(mouseLook||(presented&&std::abs(inputPresentationTurn)>1.047197551f)){
        AcquireSRWLockExclusive(&footLock);
        footPresentation.valid=false;
        if(mouseLook)footPresentation.mouseUntil=now+150;
        ReleaseSRWLockExclusive(&footLock);
        presented=false;inputPresentationTurn=0;
    }
    auto& room=roomFollow;const Vec position=Position(world);
    if(room.valid&&room.owner==owner&&room.identity==identity&&room.origin==command.origin&&now>=room.tick&&now-room.tick<150){
        Vec moved=Sub(position,room.position);moved.y=0;
        const float length=Length(moved),advance=Dot(moved,room.direction);
        if(room.following&&length<.4f&&advance>0&&room.remaining>0){
            // Subtract only actual, collision-resolved travel, never requested travel.
            // This prevents the eye moving twice as the collision body catches up.
            moved=Scale(moved,std::min(1.f,room.remaining/std::max(length,.00001f)));
            const Vec tracked=Direction(moved,Yaw(Heading(reference)-nativeYaw));
            SetPosition(reference,Add(Position(reference),Scale(tracked,1.f/Config().units)));
        }
    }
    room={owner,identity,command.origin,position,{},0,now,false,true};
    Vec stick=command.stick;const float magnitude=std::hypot(stick.x,stick.z),deadzone=Config().moveDeadzone;
    const bool stickMoving=magnitude>deadzone;
    if(stickMoving)stick=Scale(stick,std::min(1.f,(magnitude-deadzone)/(1.f-deadzone))/magnitude);
    else stick={};
    const bool keyboardMoving=((packet.mask[0]&1)&&std::abs(packet.axis[0])>.05f)||
        ((packet.mask[0]&8)&&std::abs(packet.axis[3])>.05f);
    // Registered input IDs at BF2+29078C/+2907AE: AltSprint=12, Sprint=13.
    // 9 is Action (jump); 37 is Lie. Neither is a sprint channel.
    const bool sprinting=((packet.mask[0]&(1u<<13))&&packet.axis[13]>.5f)||
        ((packet.mask[0]&(1u<<12))&&packet.axis[12]>.5f);
    const Mat relative=Mul(command.head,Inverse(reference));
    const Vec forward=Row(relative,2);
    // Align native forward while moving so BF2's forward-only sprint follows
    // the same HMD heading as walking. Compensate the XR reference exactly once.
    inputHeadTurn=command.headMove&&(stickMoving||sprinting)&&std::hypot(forward.x,forward.z)>.25f?Heading(relative):0.f;
    inputSnapTurn=command.snap;
    if(inputSnapTurn){
        AcquireSRWLockExclusive(&footLock);
        if(footCommand.frame==command.frame)footCommand.snap=0;else inputSnapTurn=0;
        ReleaseSRWLockExclusive(&footLock);
    }
    if(stickMoving){
        // Native movement has a squared response; undo it to keep a linear stick.
        packet.axis[0]=std::copysign(std::sqrt(std::abs(stick.x)),stick.x);
        packet.axis[3]=std::copysign(std::sqrt(std::abs(stick.z)),stick.z);packet.mask[0]|=9;
    }else if(command.room&&!keyboardMoving&&!sprinting&&Value<int>(owner,0x230)<=1&&Value<int>(owner,0x230)>=0){
        Vec offset=Position(relative);offset.y=0;const float distance=Length(offset);
        // Retain a small lean area. Deliberate physical steps beyond it request
        // normal walk input; there is no position teleport through level geometry.
        if(distance>command.radius&&distance<2.f){
            const float remaining=(distance-command.radius)*Config().units;
            const int speedIndex=Value<int>(owner,0x2D8);
            float speed=speedIndex>=0&&speedIndex<4?Value<float>(game,0x5EC428+speedIndex*4):0;
            if(!std::isfinite(speed)||speed<.5f||speed>12.f)speed=3.f;
            const float fraction=std::min(.7f,std::min(1.8f,remaining*5.f)/speed);
            const Vec direction=Scale(offset,1.f/distance),axis=Scale(direction,fraction);
            packet.axis[0]=std::copysign(std::sqrt(std::abs(axis.x)),axis.x);
            packet.axis[3]=std::copysign(std::sqrt(std::abs(axis.z)),axis.z);packet.mask[0]|=9;
            // Keep jump/crouch/prone intact. Room walking contributes only
            // normal movement axes, never a synthetic sprint input.
            room.following=true;room.direction=Direction(direction,Yaw(nativeYaw+inputSnapTurn+inputPresentationTurn));room.remaining=remaining;
        }
    }
    // Retain r89's native-axis fallback while presentation is unavailable
    // or a physical mouse owns look. The normal presentation route already
    // supplies an absolute heading correction above: do not add this rate too.
    // +2D81C3 initializes BF2's base mouse-axis scale to 0.03; +1AE1F7 reads
    // axis 4 and +1AE205 applies the native soldier look factor (default 5).
    // The fallback passes through native look/zoom scaling. Presentation uses
    // the default calibration directly, independent of native zoom factors.
    // Neither route writes soldier transforms or queues missed render time.
    const float smoothAxis=presented?0.f:command.smoothRate*seconds*.03f;
    if(smoothAxis!=0&&std::isfinite(smoothAxis)){
        const float existing=(packet.mask[0]&(1u<<4))?packet.axis[4]:0.f;
        if(std::isfinite(existing)){packet.axis[4]=existing+smoothAxis;packet.mask[0]|=1u<<4;}
    }
    inputSoldier=owner;inputTurnConsumed=false;
    originalInfantryUpdate(soldier,context,&packet,seconds,flags);
    if(inputTurnConsumed)RotateReference(inputHeadTurn,inputSnapTurn,command.head);
    if(command.wingsuit)ApplyWingsuit(owner,identity,command,seconds);else wingFlight={};
    // Bounded startup evidence for the private candidate, not a per-frame
    // profiler. Useful on other PCs without adding ongoing log traffic.
    static unsigned turnReports=0;
    if((smoothAxis!=0||inputPresentationTurn!=0)&&turnReports<8){
        ++turnReports;
        const float after=(Value<float>(owner,0x224)+Value<float>(owner,0x240))*.01745329252f;
        Log("TURN90 native follow rate=%.2f inputUnits/s dtMs=%.3f fallbackAxis=%.5f yawDeltaDeg=%.4f xrFrame=%llu flags=%u consumed=%d",
            command.smoothRate,seconds*1000.f,smoothAxis,Wrapped(after-nativeYaw)*57.295779513f,command.frame,flags,inputTurnConsumed?1:0);
    }
    static unsigned reports=0;
    if(reports<8&&(inputSnapTurn!=0||std::abs(inputHeadTurn)>.25f||room.following)){
        ++reports;Log("MOVE47 native input head=%.1f snap=%.1f room=%d consumed=%d",inputHeadTurn*57.2957795f,inputSnapTurn*57.2957795f,room.following?1:0,inputTurnConsumed?1:0);
    }
    inputSoldier=nullptr;inputHeadTurn=inputSnapTurn=inputPresentationTurn=0;inputTurnConsumed=false;
}
bool InstallLocomotion(){
    wingProfile=InstallWingProfile();
    const BYTE update[]={0x55,0x8B,0xEC,0x81,0xEC,0,2,0,0};
    const BYTE recoil[]={0x8B,0x81,0xE0,1,0,0};
    const BYTE copy[]={0x8B,0x75,0x0C,0xB9,0x46,0,0,0,0x8D,0xBD,0x40,0xFE,0xFF,0xFF,0xF3,0xA5};
    const BYTE call[]={0xE8,0x18,0x7C,0xFF,0xFF,0xD8,0x45,0xFC};
    const BYTE yaw[]={0xD9,0x45,0xD4,0xD8,0x83,0x24,2,0,0};
    const BYTE move[]={0x8B,0x95,0x40,0xFE,0xFF,0xFF};
    const BYTE look[]={0xD9,0x85,0x50,0xFE,0xFF,0xFF};
    if(Value<void*>(game,0x4E80F8+63*4)!=game+0x1ADEA0||
        !Match(game+0x1ADEA0,update,sizeof(update))||!Match(game+0x1A5F40,recoil,sizeof(recoil))||
        !Match(game+0x1ADFBF,copy,sizeof(copy))||!Match(game+0x1AE323,call,sizeof(call))||
        !Match(game+0x1AE8C6,yaw,sizeof(yaw))||!Match(game+0x1AE175,move,sizeof(move))||!Match(game+0x1AE1F7,look,sizeof(look)))return false;
    if(!Jump(game+0x1A5F40,recoil,sizeof(recoil),reinterpret_cast<void*>(&InfantryYawHook),reinterpret_cast<void**>(&originalYawRecoil)))return false;
    return Jump(game+0x1ADEA0,update,sizeof(update),reinterpret_cast<void*>(&InfantryUpdateHook),reinterpret_cast<void**>(&originalInfantryUpdate));
}
