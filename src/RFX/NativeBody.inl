// BF2 1.50 / RendDX9: an additional native SkinnedMesh instance, owned only by
// this adapter. The gameplay soldier remains untouched. Only verified local
// 1P arm packets are cropped to detailed hands (or suppressed in 3P-hand mode).
// Included inside Bf2Adapter.cpp's private namespace after the camera state.
using SkinPrepare=void(__thiscall*)(void*,unsigned,unsigned,unsigned);
SkinPrepare originalSkinPrepare=nullptr;
using BodyPose=void*(__thiscall*)(void*,int);
using MeshFactory=void*(__thiscall*)(void*);
using NativeRef=unsigned(__thiscall*)(void*);
using MeshPart=void(__thiscall*)(void*,int);
using MeshWorld=void(__thiscall*)(void*,const Mat*);
using MeshBones=void(__thiscall*)(void*,const Mat*,int);
using MeshOpacity=void(__thiscall*)(void*,float);

// Native render packets contain pointers into their instance. Keep instances
// and their template references alive for the session, including across map
// changes. A fixed limit prevents unlimited allocations with custom models.
struct BodyMesh {void* definition=nullptr;void* instance=nullptr;};
BodyMesh bodyMeshes[16]{};
void* bodyOwner=nullptr;void* bodySource=nullptr;void* bodyInstance=nullptr;
Mat bodyWorld{},bodyBones[80]{};
bool bodyProfile=false,bodyReady=false,bodySubmitted=false,bodyUnified=false;
bool bodyDetailTargets=false,bodyDead=false;
bool bodyDepthProfile=false,bodyDepthShared=false;
unsigned bodyEyeDepthClears=0;
void RememberBodyDepth(IDirect3DDevice9* device);
void ReleaseBodyDepth();
unsigned bodySourceCalls=0,bodySubmissions=0;
const char* bodyLastStatus=nullptr;
void BodyStatus(const char* why){if(bodyLastStatus!=why){bodyLastStatus=why;Log("FULLBODY2 %s",why);}}
#include "NativeBodyDraw.inl"
#include "NativeBodyDepth.inl"

// Implemented after the 1P hand cache. Capture BEFORE the native 3P helper:
// that helper can enter the hand finalizer while temporarily in another mode.
bool ReadBodyHandTargets(void* owner,const Mat& nativeWorld,const Runtime::Frame& frame,
                         Mat& camera,Mat& leftPalm,Mat& rightPalm,Vec wrists[2],bool& chargingContact,void*& selectedWeapon,bool& akSupport);
float ChargingArmWeight(void* owner,void* rig,bool active){
    static void* lastOwner=nullptr;static void* lastRig=nullptr;
    static std::int64_t lastTime=0;static std::uint64_t origin=0;static float weight=0;
    const auto now=pairFrame.time;
    if(owner!=lastOwner||rig!=lastRig||origin!=pairFrame.origin||!lastTime||now<lastTime||now-lastTime>150000000){weight=0;lastTime=now;}
    const float dt=std::min(.05f,static_cast<float>((now-lastTime)*1.e-9));
    const float goal=active?1.f:0.f;
    weight+=(goal-weight)*(1.f-std::exp(-dt/(active?.055f:.085f)));
    if(!active&&weight<.001f)weight=0;
    lastOwner=owner;lastRig=rig;lastTime=now;origin=pairFrame.origin;return weight;
}
bool ReloadBodyArmsHeld(void* owner);
void StableReloadBodyArms(void* owner,void* rig){
    static void* cachedOwner=nullptr;static void* cachedRig=nullptr;
    static Mat rest[80]{};static bool valid=false,wasHeld=false;
    if(cachedOwner!=owner||cachedRig!=rig){cachedOwner=owner;cachedRig=rig;valid=false;wasHeld=false;}
    const bool held=ReloadBodyArmsHeld(owner);
    if(!held){
        const Mat inv=Inverse(bodyBones[13]);
        for(int i=15;i<=44;++i)rest[i]=Mul(bodyBones[i],inv);
        valid=true;
    }else if(valid){
        // Use the last non-reload arm shape as the skinning basis. Native
        // reload helpers can translate/twist while the manual cycle waits.
        // Keep this frame's torso, shoulders, stance and tracking targets.
        for(int side=0;side<2;++side){
            const int first=side?31:15,last=side?44:29;
            const Vec shift=Sub(Position(bodyBones[first]),Position(Mul(rest[first],bodyBones[13])));
            for(int i=first;i<=last;++i){bodyBones[i]=Mul(rest[i],bodyBones[13]);SetPosition(bodyBones[i],Add(Position(bodyBones[i]),shift));}
        }
    }
    if(held&&!wasHeld)Log("MRELOAD21 connected sleeve basis held=%d",valid?1:0);
    wasHeld=held;
}

bool BodyTopology(void* rig){
    if(Value<int>(rig,0x14)!=80)return false;
    BYTE* meta=Value<BYTE*>(rig,8);if(!meta)return false;
    // The stock 3P skeleton is shared by teams and light/heavy kits. Fail
    // closed for custom skeletons rather than applying stock bone IDs to them.
    const int ids[]={1,2,6,7,11,12,13,14,15,30,31,45,46,47,52,53,72,73,74,75,76,77,78,79};
    const short parents[]={0,1,0,6,0,11,12,13,14,13,30,13,45,46,47,47,13,12,12,0,0,0,0,0};
    for(unsigned i=0;i<sizeof(ids)/sizeof(ids[0]);++i)
        if(Value<int>(meta,ids[i]*0x24)!=ids[i]||Value<short>(meta,ids[i]*0x24+6)!=parents[i])return false;
    // Validate every arm/helper/finger link before assigning 3P targets.
    for(int side=0;side<2;++side){
        const int first=side?31:15,wrist=side?35:20;
        for(int i=first+1;i<=wrist;++i)
            if(Value<short>(meta,i*0x24+6)!=i-1)return false;
        for(int f=0;f<3;++f)for(int j=0;j<3;++j){
            const int i=wrist+1+f*3+j;
            if(Value<short>(meta,i*0x24+6)!=(j?i-1:wrist))return false;
        }
    }
    for(int i=0;i<80;++i){
        const short p=Value<short>(meta,i*0x24+6);
        if(Value<int>(meta,i*0x24)!=i||p<-1||p>=i)return false;
    }
    return true;
}
#include "NativeCorpsePose.inl"
#include "NativeBodySmooth.inl"
void FollowVisualBodyHead(){
    auto& state=visualBodyFilter;
    // Work from the unmodified native world each pair: never feed this
    // visual turn back into gameplay, locomotion, aiming or the pose filter.
    const Vec spine=Direction(Sub(Position(bodyBones[47]),Position(bodyBones[0])),bodyWorld);
    const bool upright=Length(spine)>.2f&&Unit(spine).y>.65f;
    const Mat relative=Mul(pairFrame.head,Inverse(reference));
    const Vec forward=Row(relative,2);
    const float horizontal=std::sqrt(forward.x*forward.x+forward.z*forward.z);
    float goal=state.headYaw;
    if(!Tuning().bodyHeadFollow||!upright)goal=0;
    else if(horizontal>.35f){
        // A 30 degree free-look zone avoids making every glance turn feet.
        const float yaw=std::atan2(forward.x,forward.z),free=.523598776f;
        goal=std::copysign(std::max(0.f,std::abs(yaw)-free),yaw);
    }
    const float dt=state.arms.seconds;
    const float delta=std::remainder(goal-state.headYaw,6.283185307f);
    state.headYaw=std::remainder(state.headYaw+delta*(dt>0?1-std::exp(-dt/.12f):0),6.283185307f);
    if(!upright)return; // native prone orientation stays intact
    const Vec p=Position(bodyWorld);
    bodyWorld=Mul(bodyWorld,Yaw(state.headYaw));SetPosition(bodyWorld,p);
}
bool AlignFullBody(void* owner){
    // Align the model's anatomical eyes to the actual rendered head. r5's
    // forced synthetic shoulder frame pushed the connected collar into view.
    // Keep this model's own head-to-shoulder proportions and torso rotation.
    const Mat nativeWorld=bodyWorld,base=TrackingBase(views[0].source);
    const Mat head=Compose(base,reference,pairFrame.head,Config().units);
    if(!Rigid(head))return false;
    const Vec eyeWorld=Point(Point({0.f,.07304f,.07993f},bodyBones[47]),nativeWorld);
    const Vec baseline=Sub(Position(views[0].source),eyeWorld);
    const Vec correction=Sub(Position(head),eyeWorld);
    if(!std::isfinite(Length(correction))||Length(baseline)>.85f||Length(correction)>1.6f||std::abs(correction.y)>1.1f)return false;
    SetPosition(bodyWorld,Add(Position(nativeWorld),{correction.x,0.f,correction.z}));
    // A little eye clearance keeps bulky vest/collar meshes out of the
    // near view. Controller wrist targets are solved again after this shift.
    const Vec spine=Direction(Sub(Position(bodyBones[47]),Position(bodyBones[0])),nativeWorld);
    if(Length(spine)>.2f&&Unit(spine).y>.65f){
        const Vec backward=Scale(Row(Level(nativeWorld),2),-Tuning().bodySetback*Config().units);
        SetPosition(bodyWorld,Add(Position(bodyWorld),backward));
    }
    // Feet keep their native height. Vertical correction is distributed up
    // the spine, and the upper torso and both arms translate together.
    const Vec lift=Direction({0.f,correction.y,0.f},Inverse(bodyWorld));
    for(int i=11;i<75;++i){
        const float weight=i==11?.35f:(i==12||i==73||i==74)?.70f:1.f;
        SetPosition(bodyBones[i],Add(Position(bodyBones[i]),Scale(lift,weight)));
        if(!Rigid(bodyBones[i]))return false;
    }
    static void* lastOwner=nullptr;static int lastStance=-1;
    static std::uint64_t lastRecenter=0;static unsigned aligned=0;
    const int stance=Value<int>(owner,0x230);const bool recentered=lastRecenter!=referenceGeneration;
    if(++aligned<=3||aligned%300==1||lastOwner!=owner||lastStance!=stance||recentered){
        Mat headFrame=base;SetPosition(headFrame,Position(head));const Mat inv=Inverse(headFrame);
        const Vec eye=Point(Point(Point({0.f,.07304f,.07993f},bodyBones[47]),bodyWorld),inv);
        const Vec left=Point(Point(Position(bodyBones[15]),bodyWorld),inv),right=Point(Point(Position(bodyBones[31]),bodyWorld),inv);
        Log("FULLBODY6 anatomical fit frame=%llu stance=%d recentered=%d eyeError=(%.3f %.3f %.3f) left=(%.3f %.3f %.3f) right=(%.3f %.3f %.3f) feetY=preserved",
            pairFrame.id,stance,recentered?1:0,eye.x,eye.y,eye.z,left.x,left.y,left.z,right.x,right.y,right.z);
    }
    lastOwner=owner;lastStance=stance;lastRecenter=referenceGeneration;return true;
}
void* BodyMeshFor(void* definition){
    for(const auto& mesh:bodyMeshes)if(mesh.definition==definition)return mesh.instance;
    for(auto& mesh:bodyMeshes)if(!mesh.instance){
        // Factory slot 9 registers the instance with its template; slot 0
        // additionally retains the template through map/kit transitions.
        reinterpret_cast<NativeRef>(Value<void*>(Value<void*>(definition),0))(definition);
        void* instance=reinterpret_cast<MeshFactory>(render+0x10F840)(definition);
        if(!instance){reinterpret_cast<NativeRef>(Value<void*>(Value<void*>(definition),8))(definition);return nullptr;}
        mesh={definition,instance};
        if(Value<void*>(instance)!=render+0x221410){BodyStatus("unexpected factory instance; body omitted");return nullptr;}
        reinterpret_cast<MeshPart>(render+0x1A74F0)(instance,1);
        reinterpret_cast<MeshOpacity>(render+0x1A8F90)(instance,1.f);
        // Leave native object owner (+330) null: this is a visual instance,
        // not another playable soldier, weapon, attachment, or shadow owner.
        Log("FULLBODY2 mesh created instance=%p template=%p subGeometry=%d",instance,definition,Value<int>(instance,0x3C));
        return instance;
    }
    BodyStatus("session mesh cache limit reached; body omitted");return nullptr;
}
#include "NativeBodyRagdoll.inl"
bool BeginDownedBody(){
    void* owner=HelmetBodyOwner();if(!owner||LocalAlive())return false;
    void* source=Value<void*>(owner,0x38);void* definition=Value<void*>(source,0x58);void* rig=Value<void*>(owner,0x290);
    if(Value<void*>(owner)!=game+0x4E80F8||Value<void*>(source)!=render+0x221410||
       Value<void*>(definition)!=render+0x21C160||!Value<BYTE>(definition,0x4C)||!BodyTopology(rig))return false;
    const auto geo=Value<std::uintptr_t>(definition,0xE0),end=Value<std::uintptr_t>(definition,0xE4);
    const auto materials=Value<std::uintptr_t>(definition,0xD0),matEnd=Value<std::uintptr_t>(definition,0xD4);
    if(!geo||end<geo||end-geo<64||end-geo>1024||!materials||matEnd<materials||matEnd-materials<32||matEnd-materials>512)return false;
    const bool nativePose=PrepareCorpsePose(owner,rig,pairFrame.id);
    if(nativePose){memcpy(bodyBones,corpsePose.bones,sizeof(bodyBones));bodyWorld=corpsePose.world;downedArms={};}
    else{
        // Unsupported/custom physics retain the previously working view.
        if(!Copy(bodyBones,Value<Mat*>(rig,0x10),sizeof(bodyBones))||!Read(owner,0x88,bodyWorld)||!Rigid(bodyWorld))return false;
        for(const auto& bone:bodyBones)if(!Rigid(bone))return false;
        RelaxDownedArms(owner,rig);
    }
    void* instance=BodyMeshFor(definition);
    if(!instance||Value<void*>(instance)!=render+0x221410||Value<int>(instance,0x3C)!=1)return false;
    reinterpret_cast<MeshWorld>(render+0x1A6CB0)(instance,&bodyWorld);
    reinterpret_cast<MeshBones>(render+0x1A8F30)(instance,bodyBones,80);
    bodyOwner=owner;bodySource=source;bodyInstance=instance;bodyReady=bodyUnified=bodyDead=true;
    BodyStatus(nativePose?"full native ragdoll snapshot; helmet/head masked":"native corpse with passive visual arms; helmet/head masked");return true;
}
void BeginFullBodyPair(){
    bodyReady=bodyUnified=bodyDepthShared=bodyDetailTargets=bodyDead=false;bodyOwner=bodySource=bodyInstance=nullptr;
    if(!bodyProfile){BodyStatus("native body profile unavailable");return;}
    if(!LocalAlive()){if(!BeginDownedBody())BodyStatus("downed body unavailable");return;}
    downedArms={};corpsePose={};
    if(!Tuning().showNativeFullBody){BodyStatus("disabled in settings");return;}
    void* owner=Soldier();void* seat=Value<void*>(Value<void*>(Player(),0x80),4);
    if(!LocalAlive()||!owner||seat!=owner||Value<int>(owner,0x2B8)!=0||Value<void*>(owner,0x254)){
        BodyStatus("waiting for living first-person infantry");return;
    }
    if(Value<void*>(owner)!=game+0x4E80F8){BodyStatus("local soldier type rejected");return;}
    void* source=Value<void*>(owner,0x38);void* definition=Value<void*>(source,0x58);
    void* rig=Value<void*>(owner,0x290);
    if(Value<void*>(source)!=render+0x221410||Value<void*>(definition)!=render+0x21C160||!Value<BYTE>(definition,0x4C)){
        BodyStatus("waiting for native skinned soldier geometry");return;
    }
    // Check both native subgeometry vectors before the native selector indexes
    // element 1. Each logical geometry has its own LOD vector.
    const auto geo=Value<std::uintptr_t>(definition,0xE0),geoEnd=Value<std::uintptr_t>(definition,0xE4);
    const auto materials=Value<std::uintptr_t>(definition,0xD0),materialsEnd=Value<std::uintptr_t>(definition,0xD4);
    if(!geo||geoEnd<geo||geoEnd-geo<64||geoEnd-geo>1024||!materials||materialsEnd<materials||materialsEnd-materials<32||materialsEnd-materials>512){
        static unsigned reports=0;if(reports++<4)Log("FULLBODY2 subgeometry rejected geometry=(%p %p) materials=(%p %p)",reinterpret_cast<void*>(geo),reinterpret_cast<void*>(geoEnd),reinterpret_cast<void*>(materials),reinterpret_cast<void*>(materialsEnd));
        BodyStatus("third-person subgeometry layout rejected");return;
    }
    if(!BodyTopology(rig)){
        static unsigned reports=0;if(reports++<4)Log("FULLBODY2 topology rejected rig=%p bones=%d",rig,Value<int>(rig,0x14));
        BodyStatus("third-person skeleton rejected");return;
    }
    if(!Value<void*>(owner,0x28C)||!Value<void*>(owner,0x294)||!Value<Mat*>(rig,0x10)){
        BodyStatus("waiting for both native animation systems");return;
    }
    Mat nativeWorld{},handCamera{},leftPalm{},rightPalm{};Vec handWrists[2]{};bool chargingContact=false,akSupport=false;void* selectedWeapon=nullptr;
    if(!Read(owner,0x88,nativeWorld)||!Rigid(nativeWorld)){BodyStatus("native body world unavailable");return;}
    const bool targets=Tuning().useNativeArms&&ReadBodyHandTargets(owner,nativeWorld,pairFrame,handCamera,leftPalm,rightPalm,handWrists,chargingContact,selectedWeapon,akSupport);
    bodySource=source;
    bodyDetailTargets=targets&&Tuning().highDetailHands&&DetailHandsAvailable();
    // Native BF2+1A6000 generates the 3P pose with forceGeometry=false,
    // then restores the saved mode, stance and selected weapon mode itself.
    // -1 keeps the current standing/crouched/prone pose. Respect BF2's own pose
    // tick/stance cache (BF2+C4420) so headset frames do not advance animation
    // repeatedly within a single simulation tick.
    void* posed=rig;
    if(Value<int>(owner,0x2A0)!=Value<int>(game,0x5A7428)||Value<int>(owner,0x2A4)!=Value<int>(owner,0x230))
        posed=reinterpret_cast<BodyPose>(game+0x1A6000)(owner,-1);
    if(posed!=rig||Value<int>(owner,0x2B8)!=0||!Copy(bodyBones,Value<Mat*>(rig,0x10),sizeof(bodyBones))||!Read(owner,0x88,bodyWorld)||!Rigid(bodyWorld)){
        BodyStatus("native body pose unavailable");return;
    }
    for(const auto& bone:bodyBones)if(!Rigid(bone)){BodyStatus("native body bone matrix rejected");return;}
    // Failure leaves the current native pose intact and resets the filter.
    SmoothVisualBody(owner,rig,definition);
    FollowVisualBodyHead();
    // Carry the private torso through the same presentation frame as its
    // hands and eyes. Never modify the soldier's physics/world transform.
    bodyWorld=Mul(Mul(bodyWorld,Inverse(Level(views[0].source))),TrackingBase(views[0].source));
    if(!AlignFullBody(owner)){BodyStatus("native body/camera alignment rejected; extra body omitted");return;}
    // A new weapon changes the authored wrist/arm basis. Keep torso and leg
    // smoothing continuous, but never carry sleeve twist across that change.
    if(!targets||visualBodyFilter.weapon!=selectedWeapon){visualBodyFilter.arms={};visualBodyFilter.akElbowWeight=0;}
    visualBodyFilter.weapon=selectedWeapon;
    // AK support grips can ask the forearm to twist against the default
    // down/out elbow plane. Ease into the wrist-aware swivel and spread the
    // elbow bend across its helper; keep shoulder/wrist and gun targets exact.
    const float elbowGoal=akSupport?1.f:0.f;
    const float elbowDt=visualBodyFilter.arms.seconds;
    visualBodyFilter.akElbowWeight+=(elbowGoal-visualBodyFilter.akElbowWeight)*elbowDt/(elbowDt+.055f);
    if(!akSupport&&visualBodyFilter.akElbowWeight<.001f)visualBodyFilter.akElbowWeight=0;
    StableReloadBodyArms(owner,rig);
    const float chargeWeight=ChargingArmWeight(owner,rig,targets&&chargingContact);
    BodyArmStats armStats{};
    if(targets){
        // The hidden 1P solve and its gun stay in their original rig. Transfer
        // only its palm targets into our fitted private 3P instance. Detailed
        // hands use the actual 1P wrist endpoints, not the shorter 3P palms.
        const Mat intoBody=Mul(nativeWorld,Inverse(bodyWorld));
        const Vec wrists[]={Point(handWrists[0],intoBody),Point(handWrists[1],intoBody)};
        Mat solved[80];
        bodyUnified=SolveBodyArms(bodyBones,Mul(leftPalm,intoBody),Mul(rightPalm,intoBody),Mul(TrackingBase(views[0].source),Inverse(bodyWorld)),solved,armStats,bodyDetailTargets?wrists:nullptr,chargeWeight,&visualBodyFilter.arms,visualBodyFilter.akElbowWeight);
        if(bodyUnified)memcpy(bodyBones,solved,sizeof(bodyBones));else visualBodyFilter.arms={};
    }
    static unsigned attempts=0;static bool previousUnified=false;
    if(++attempts<=3||attempts%300==1||previousUnified!=bodyUnified){
        const Vec cameraGap=targets?Sub(Position(Mul(handCamera,nativeWorld)),Position(views[0].source)):Vec{};
        Log("FULLBODY8 arms frame=%llu targets=%d connected=%d detailTargets=%d cameraGap=(%.3f %.3f %.3f) reach=(%.3f %.3f) stretch=(%.3f %.3f) folded=%u rejected=%d fallback=%s",pairFrame.id,targets?1:0,bodyUnified?1:0,bodyDetailTargets?1:0,cameraGap.x,cameraGap.y,cameraGap.z,armStats.reach[0],armStats.reach[1],armStats.stretch[0],armStats.stretch[1],armStats.folded,armStats.rejected,bodyUnified?"none":"1P");
    }
    previousUnified=bodyUnified;
    static bool supportReported=false;
    if(akSupport&&bodyUnified&&!supportReported){supportReported=true;Log("FULLBODY41 AK support: restored r39 authored helper solve; r40 rest-pose roll removed; exact gun/hand targets retained");}
    static bool lastCharge=false;static ULONGLONG lastChargeReport=0;
    const ULONGLONG reportNow=GetTickCount64();
    if(chargingContact!=lastCharge||(chargingContact&&reportNow-lastChargeReport>500)){
        Log("MRELOAD21 sleeve contact=%d weight=%.3f connected=%d leftRollDeg=%.1f elbowSwivelDeg=%.1f reach=%.3f stretch=%.3f",chargingContact?1:0,chargeWeight,bodyUnified?1:0,armStats.roll[0]*57.2957795f,armStats.swivel[0]*57.2957795f,armStats.reach[0],armStats.stretch[0]);
        lastChargeReport=reportNow;
    }
    lastCharge=chargingContact;
    void* instance=BodyMeshFor(definition);
    if(!instance||Value<void*>(instance)!=render+0x221410||Value<int>(instance,0x3C)!=1){BodyStatus("body mesh creation failed");return;}
    reinterpret_cast<MeshWorld>(render+0x1A6CB0)(instance,&bodyWorld);
    reinterpret_cast<MeshBones>(render+0x1A8F30)(instance,bodyBones,80);
    bodyOwner=owner;bodySource=source;bodyInstance=instance;bodyReady=true;
    BodyStatus(bodyUnified?(bodyDetailTargets?"connected body/sleeves with detailed 1P hands":"connected native body and tracked 3P arms"):"anatomical torso with 1P arm fallback");
}
void BeginFullBodyEye(){
    ReleaseBodyDepth();bodyEyeDepthClears=0;
    bodySubmitted=bodyRenderConnected=bodyRenderDetails=bodyDetailsChecked=false;
    bodySourceCalls=0;bodyEyePackets=bodyEyeDraws=bodyEyeHandMask=bodyEyeHiddenArms=bodyEyeKeptArms=bodyEyeDetailDraws=0;
    for(auto& profile:bodyEyeDetailProfiles)profile=nullptr;
}
void SubmitDownedBody(unsigned a,unsigned b,unsigned c){
    if(!eyeActive||scopePass||!bodyReady||!bodyDead||bodySubmitted||bodyOwner!=HelmetBodyOwner()||!originalSkinPrepare)return;
    bodySubmitted=true;const DWORD stale=0xffffffff;
    Copy(static_cast<BYTE*>(bodyInstance)+0x34C,&stale,4);Copy(static_cast<BYTE*>(bodyInstance)+0x350,&stale,4);
    originalSkinPrepare(bodyInstance,a,b,c);bodyRenderConnected=true;++bodySubmissions;
}
void __fastcall SkinPrepareHook(void* self,void*,unsigned a,unsigned b,unsigned c){
    SubmitDownedBody(a,b,c);
    if(eyeActive&&bodyDead&&bodySubmitted&&self==bodySource&&!scopePass)return;
    if(scopePass&&bodySource&&self==bodySource)return;
    // Use the exact local soldier geometry pointer, not a draw-size heuristic.
    // This boundary queues mesh packets before BF2 draws its world/1P lists.
    if(eyeActive&&bodyReady&&self==bodySource){
        ++bodySourceCalls;
        if(!bodySubmitted&&bodyOwner==Soldier()&&Value<void*>(bodyOwner,0x38)==self){
            bodySubmitted=true;
            // The renderer's palette/LOD caches are keyed by its native frame
            // ID; both eye replays can share that ID. Invalidate only our clone.
            const DWORD stale=0xFFFFFFFF;
            Copy(static_cast<BYTE*>(bodyInstance)+0x34C,&stale,4);
            Copy(static_cast<BYTE*>(bodyInstance)+0x350,&stale,4);
            originalSkinPrepare(bodyInstance,a,b,c);
            bodyRenderConnected=bodyUnified&&ConnectedBodyAvailable();
            ++bodySubmissions;
            if(bodySubmissions<=6||bodySubmissions%3600==1)
                Log("FULLBODY2 submitted eye=%d frame=%llu local=%p source=%p body=%p subGeometry=%d bones=%d fov=%.3f palette0=%u preparedFrame=%u",
                    activeEye,pairFrame.id,bodyOwner,self,bodyInstance,Value<int>(bodyInstance,0x3C),Value<int>(bodyInstance,0x178),Value<float>(bodyInstance,0x340),Value<unsigned>(bodyInstance,0x320),Value<unsigned>(bodyInstance,0x20));
        }
    }
    originalSkinPrepare(self,a,b,c);
}
void EndFullBodyEye(){
    if(activeEye==1)ReportBodyFilter();
    if(bodyReady&&bodyUnified){
        static unsigned eyes=0;if(++eyes<=6||eyes%600==1)
            Log("FULLBODY8 eye=%d frame=%llu connected=%d detailHands=%d draws=%u detailDraws=%u handMask=%u hidden1P=%u kept1P=%u sharedDepth=%d depthSurface=%d preservedClears=%u",activeEye,pairFrame.id,bodyRenderConnected?1:0,bodyRenderDetails?1:0,bodyEyeDraws,bodyEyeDetailDraws,bodyEyeHandMask,bodyEyeHiddenArms,bodyEyeKeptArms,bodyDepthShared?1:0,bodyDepthSurface?1:0,bodyEyeDepthClears);
    }
    if(bodyReady&&bodySubmitted&&!bodyEyeDraws){
        static unsigned empty=0;if(empty++<6||empty%1800==1)
            Log("FULLBODY3 no masked body draw eye=%d frame=%llu packets=%u",activeEye,pairFrame.id,bodyEyePackets);
    }
    if(bodyReady&&!bodySubmitted){
        static unsigned missed=0;if(missed++<6||missed%1800==1)
            Log("FULLBODY2 no local mesh submission eye=%d frame=%llu source=%p sourceCalls=%u",activeEye,pairFrame.id,bodySource,bodySourceCalls);
    }
    ReleaseBodyDepth();
}
bool InstallFullBody(){
    const BYTE prepare[]={0x55,0x8B,0xEC,0x81,0xEC,0x94,0,0,0};
    const BYTE pose[]={0x55,0x8B,0xEC,0x51,0x56,0x8B,0xF1,0xC6,0x86,0xB4,2,0,0,1};
    const BYTE restore[]={0x89,0x9E,0xB8,2,0,0};
    const BYTE nonforcing[]={0x6A,0,0x68,0x89,0x88,8,0x3D,0x8B,0xCE};
    const BYTE tick[]={0xA1,0x28,0x74,0x9A,0,0xC3};
    const BYTE world[]={0x55,0x8B,0xEC,0x8B,0x45,8,0x89,0x41,0x5C,0x5D,0xC2,4,0};
    const BYTE bones[]={0x55,0x8B,0xEC,0x8B,0x45,8,0x8B,0x55,0x0C,0x89,0x81,0x74,1,0,0,0x89,0x91,0x78,1,0,0};
    if(!Match(game+0x1A6000,pose,sizeof(pose))||!Match(game+0x1A6083,nonforcing,sizeof(nonforcing))||!Match(game+0x1A60AD,restore,sizeof(restore))||!Match(game+0xC4420,tick,sizeof(tick))||
        !Match(render+0x1A6CB0,world,sizeof(world))||!Match(render+0x1A8F30,bones,sizeof(bones))||
        Value<void*>(game,0x4E80F8+41*4)!=game+0x1A5A90||
        Value<void*>(render,0x21C160+9*4)!=render+0x10F840||
        Value<void*>(render,0x221410+6*4)!=render+0x1A7FD0||Value<void*>(render,0x221410+27*4)!=render+0x1A74F0||
        Value<void*>(render,0x221410+61*4)!=render+0x1A8F90)return false;
    if(!InstallBodyDraw())return false;
    return Jump(render+0x1A7FD0,prepare,sizeof(prepare),reinterpret_cast<void*>(&SkinPrepareHook),reinterpret_cast<void**>(&originalSkinPrepare));
}
