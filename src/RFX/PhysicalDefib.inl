// Physical gesture drives the ordinary BF2 fire input. BF2 still decides
// charge energy, cooldown, revive eligibility, collision and network results.
#include "DefibGrip.inl"
#include "Dzb2GadgetGrips.inl"
bool Dzb2Defibrillator(void* weapon){char name[80]{};return ReloadName(weapon,name)&&!strcmp(name,"yun_bf4defrib");}
const Mat& DefibLeftContact(void* weapon){return Dzb2Defibrillator(weapon)?dzDefibLeftFromWrist:defibLeftFromWrist;}
void DefibHandBinding(void* weapon){
    if(!Tuning().physicalDefib||!Dzb2Defibrillator(weapon))return;
    palmRight=dzDefibRightPalm;palmLeft=dzDefibLeftPalm;palms=true;
    binding.right=dzDefibRightWristFromItem;binding.rightReady=true;
}
void DefibHandBasis(void* weapon,Mat basis[70]){
    if(!Tuning().physicalDefib||!Dzb2Defibrillator(weapon))return;
    // Keep the authored grasp on each paddle through native charging/idle
    // animations. Only fingers change; the arm IK uses live controller wrists.
    const Mat left=basis[7],right=basis[33];
    for(int i=0;i<21;++i){basis[7+i]=Mul(dzDefibLeftFingers[i],left);basis[33+i]=Mul(dzDefibRightFingers[i],right);}
}
void ComfortablePaddles(HandTargets& t){
    // Both wrists use their controller grip poses, including the right one
    // which previously inherited the ordinary gun/aim orientation. A small
    // mirrored toe-in reduces how far real wrists must bend to meet plates.
    t.freeHands=true;t.supportPressed=false;t.virtualStock=false;
    t.leftGrip=Mul(Yaw(.26179939f),t.leftGrip);
    t.rightGrip=Mul(Yaw(-.26179939f),t.rightGrip);
}
struct PhysicalDefibState {
    void* owner=nullptr;void* weapon=nullptr;void* patient=nullptr;void* weak=nullptr;void* soldier=nullptr;
    std::uint64_t origin=0;ULONGLONG tick=0,together=0,pulseUntil=0,launchUntil=0,scan=0,feedback=0;
    int phase=0,contactBone=13;ULONGLONG rubTick=0;Mat launch{};Vec previousRelative{},localContact{};
    float rubMs=0,rubDistance=0;bool previousValid=false;
} defib;
SRWLOCK defibLock=SRWLOCK_INIT;
void ResetDefibRaw(){InterlockedExchangePointer(&defibWeaponPublished,nullptr);defib={};}
void ResetDefib(){AcquireSRWLockExclusive(&defibLock);ResetDefibRaw();ReleaseSRWLockExclusive(&defibLock);}
bool IsDefibrillator(void* weapon){char name[80]{};return ReloadName(weapon,name)&&(!strcmp(name,"defibrillator")||!strcmp(name,"yun_bf4defrib"));}
bool DefibPatient(void* patient,void* weak,void* soldier,Vec& chest){
    void* player=Player();
    if(!player||!patient||patient==player||Value<void*>(patient)!=Value<void*>(player)||
       Value<void*>(patient,0xCC)!=weak||Value<void*>(weak,4)!=soldier||!Value<BYTE>(patient,0xE8))return false;
    // Verified BF2 getters +104AC0 (team) and +104A90 (dead flag).
    const int team=Value<int>(player,0xD8);
    if(team<1||team>2||Value<int>(patient,0xD8)!=team||Value<void*>(soldier)!=game+0x4E80F8||
       (Value<unsigned>(soldier,4)&1))return false;
    void* rig=Value<void*>(soldier,0x290);if(!BodyTopology(rig))return false;
    Mat torso{},head{},world{};Mat* bones=Value<Mat*>(rig,0x10);
    if(!Read(bones,13*sizeof(Mat),torso)||!Read(bones,47*sizeof(Mat),head)||!Read(soldier,0x88,world)||
       !Rigid(torso)||!Rigid(head)||!Rigid(world))return false;
    const Vec local=Add(Scale(Position(torso),.8f),Scale(Position(head),.2f));
    if(Length(local)>3.f)return false;
    chest=Point(local,world);return true;
}
Vec ClosestBodySegment(Vec p,Vec a,Vec b){
    const Vec d=Sub(b,a);const float length=Dot(d,d);
    return Add(a,Scale(d,length>.000001f?std::max(0.f,std::min(1.f,Dot(Sub(p,a),d)/length)):0.f));
}
bool DefibBodyContact(void* soldier,const Vec paddles[2],Vec& contact,int& contactBone,float& score){
    void* rig=Value<void*>(soldier,0x290);if(!BodyTopology(rig))return false;
    Mat bones[80],world{};
    if(!Copy(bones,Value<void*>(rig,0x10),sizeof(bones))||!Read(soldier,0x88,world)||!Rigid(world))return false;
    BYTE* meta=Value<BYTE*>(rig,8);float best[2]={1000,1000};Vec closest[2]{};int nearestBone[2]={13,13};
    // Main skeleton through the head includes both legs, feet, arms and
    // hands. Parent capsules cover the spaces BETWEEN joints as well.
    for(int i=1;i<=47;++i){
        const int parent=Value<short>(meta,i*0x24+6);
        if(parent<0||parent>=80||!Rigid(bones[i])||!Rigid(bones[parent]))continue;
        const Vec a=Point(Position(bones[parent]),world),b=Point(Position(bones[i]),world);
        if(Length(Sub(a,Position(world)))>3.f||Length(Sub(b,a))>1.2f)continue;
        const float radius=Tuning().assistedRevive?1.25f*Config().units:((i>=11&&i<=14)||i==30||i==45?.30f:i>=46?.23f:.21f)*Config().units;
        for(int hand=0;hand<2;++hand){
            const Vec q=ClosestBodySegment(paddles[hand],a,b);
            const float distance=Length(Sub(q,paddles[hand]))/radius;
            if(distance<best[hand]){best[hand]=distance;closest[hand]=q;nearestBone[hand]=i;}
        }
    }
    if(best[0]>1.f||best[1]>1.f)return false;
    // Assisted reach still needs a clear path to the body; it cannot reach
    // through terrain or a wall. Allow the ray to end on the patient's skin.
    if(Tuning().assistedRevive){
        Vec hit{};for(int hand=0;hand<2;++hand)
            if(WorldSegment(paddles[hand],closest[hand],Soldier(),hit)&&Length(Sub(hit,closest[hand]))>.35f*Config().units)return false;
    }
    score=best[0]+best[1];const int hand=best[0]<best[1]?0:1;contact=closest[hand];contactBone=nearestBone[hand];return true;
}
bool DefibContact(const Vec paddles[2],void*& patient,void*& weak,void*& soldier,Vec& contact,int& contactBone){
    void* manager=Class("PlayerManager");void* table=Value<void*>(manager);
    if(table!=game+0x4AA5A8||Value<void*>(table,0x30)!=game+0x24E850||Value<void*>(table,0x40)!=game+0x25DDD0)return false;
    void* sentinel=Value<void*>(manager,0x10);const unsigned count=Value<unsigned>(manager,0x14);
    if(!sentinel||!count||count>256)return false;
    void* node=Value<void*>(sentinel);float best=2.f;bool found=false;
    for(unsigned i=0;i<count&&node&&node!=sentinel;++i){
        void* p=Value<void*>(node,8);void* w=Value<void*>(p,0xCC);void* body=Value<void*>(w,4);Vec chest{},point{};int bone=13;float score=0;
        if(DefibPatient(p,w,body,chest)&&Length(Sub(chest,paddles[0]))<2.2f*Config().units&&
           DefibBodyContact(body,paddles,point,bone,score)&&score<best){
            patient=p;weak=w;soldier=body;contact=point;contactBone=bone;best=score;found=true;
        }
        void* next=Value<void*>(node);if(next==node)break;node=next;
    }
    return found;
}
bool UpdateDefibRaw(const Runtime::Frame& f,bool enabled,bool& firing){
    firing=false;
    if(!enabled||!Tuning().physicalDefib){ResetDefibRaw();return false;}
    FireFrame pose{};AcquireSRWLockShared(&fireLock);
    if(!fire.defibrillator){ReleaseSRWLockShared(&fireLock);ResetDefibRaw();return false;}
    pose=fire;ReleaseSRWLockShared(&fireLock);
    if(!enabled||!Tuning().physicalDefib||!LocalAlive()||!f.valid||!f.focused||MenuActive()||
       !pose.defibrillator||pose.owner!=Soldier()||!pose.weapon||pose.origin!=f.origin||
       Value<int>(pose.owner,0x1E0)!=pose.slot||Value<void*>(pose.weapon,0x28)!=pose.owner||
       Value<void*>(pose.owner,0x288)!=pose.rig||pose.frame>f.id||f.id-pose.frame>2||GetTickCount64()-pose.tick>150||
       !f.hands[0].gripValid||!f.hands[1].gripValid){ResetDefibRaw();return false;}
    auto& s=defib;const auto now=f.tick;
    if(s.owner!=pose.owner||s.weapon!=pose.weapon||s.origin!=f.origin||now<s.tick||now-s.tick>150){
        ResetDefibRaw();s.owner=pose.owner;s.weapon=pose.weapon;s.origin=f.origin;
    }
    s.tick=now;firing=now<s.pulseUntil;
    if(now<s.launchUntil)return true;
    const Vec relative=Sub(Position(f.hands[0].grip),Position(f.hands[1].grip));
    const float separation=Length(relative);
    if(s.phase==0){
        if(separation<.28f){
            if(!s.together)s.together=now;
            if(!s.previousValid){s.previousRelative=relative;s.rubTick=now;s.previousValid=true;}
            const auto sampleMs=now-s.rubTick;
            if(sampleMs>=35){
                const float travel=Length(Sub(relative,s.previousRelative));
                const float speed=travel*1000.f/float(sampleMs);
                // Sample across several frames and require a 3.5 mm stroke.
                // Static controller jitter cannot accumulate a ready charge.
                if(sampleMs<=90&&travel>.0035f&&travel<.12f&&speed>.08f&&speed<3.f){
                    s.rubMs+=float(sampleMs);s.rubDistance+=travel;
                    if(now-s.feedback>=110){s.feedback=now;Runtime::Haptic(0,.20f,25);Runtime::Haptic(1,.20f,25);}
                }
                s.previousRelative=relative;s.rubTick=now;
            }
            if(s.rubMs>=1250.f&&s.rubDistance>=.22f){
                s.phase=2;s.feedback=now;Runtime::Haptic(0,.65f,90);Runtime::Haptic(1,.65f,90);
                ReloadAudio::DefibReady();Log("DEFIB53 charged after %.0fms rubbing, distance=%.2fm; whole-body contact ready",s.rubMs,s.rubDistance);
            }
        }else if(separation>.34f){s.together=0;s.rubMs=s.rubDistance=0;s.previousValid=false;}
        return true;
    }
    if(separation>.98f||now-s.scan<40)return true;
    s.scan=now;
    Mat body{},solved[70];bool support=false;
    if(!Read(pose.owner,0x88,body)||!Rigid(body))return true;
    HandTargets target=Targets(pose.native[0],body,f,pose.rightPalm,pose.leftPalm,pose.palmsValid,false);ComfortablePaddles(target);
    if(!SolveHands(pose.native,pose.gripBinding,target,solved,support,nullptr))return true;
    const Mat left=Mul(Mul(DefibLeftContact(pose.weapon),solved[7]),body),right=Mul(solved[54],body);
    const Vec paddles[2]={Position(left),Position(right)};Vec chest{};int bone=13;void *patient=nullptr,*weak=nullptr,*soldier=nullptr;
    if(!DefibContact(paddles,patient,weak,soldier,chest,bone))return true;
    // Approach the actual body contact from above. The old camera-to-chest
    // ray could start beneath sloping ground or miss a prone ragdoll entirely.
    Mat patientWorld{};if(!Read(soldier,0x88,patientWorld)||!Rigid(patientWorld))return true;
    Mat joint{};if(!Read(Value<void*>(Value<void*>(soldier,0x290),0x10),bone*sizeof(Mat),joint)||!Rigid(joint))return true;
    s.contactBone=bone;s.localContact=Point(chest,Inverse(Mul(joint,patientWorld)));
    s.launch=Facing(Identity(),{0,-1,0});SetPosition(s.launch,Add(chest,{0,.38f*Config().units,0}));
    s.patient=patient;s.weak=weak;s.soldier=soldier;s.pulseUntil=now+120;s.launchUntil=now+750;
    InterlockedExchangePointer(&defibWeaponPublished,s.weapon);
    firing=true;
    Log("DEFIB53 %s contact; native fire requested patient=%p body=%p bone=%d",Tuning().assistedRevive?"assisted":"touch",patient,soldier,bone);return true;
}
bool UpdateDefib(const Runtime::Frame& f,bool enabled,bool& firing){
    AcquireSRWLockExclusive(&defibLock);const bool result=UpdateDefibRaw(f,enabled,firing);ReleaseSRWLockExclusive(&defibLock);return result;
}
bool DefibLaunchRaw(void* owner,void* weapon,std::uint64_t origin,ULONGLONG now,Mat& result){
    if(defib.owner!=owner||defib.weapon!=weapon||defib.origin!=origin||now>=defib.launchUntil||now<defib.tick)return false;
    Vec chest{};Mat world{};
    if(!DefibPatient(defib.patient,defib.weak,defib.soldier,chest)||!Read(defib.soldier,0x88,world)||!Rigid(world))return false;
    Mat joint{};
    if(defib.contactBone<0||defib.contactBone>=80||!Read(Value<void*>(Value<void*>(defib.soldier,0x290),0x10),defib.contactBone*sizeof(Mat),joint)||!Rigid(joint))return false;
    result=defib.launch;SetPosition(result,Add(Point(defib.localContact,Mul(joint,world)),{0,.38f*Config().units,0}));return Rigid(result);
}

bool DefibLaunch(const FireFrame& pose,const Runtime::Frame& f,Mat& result){
    AcquireSRWLockShared(&defibLock);const bool ok=DefibLaunchRaw(pose.owner,pose.weapon,f.origin,f.tick,result);ReleaseSRWLockShared(&defibLock);return ok;
}
bool DefibNativeLaunch(void* weapon,Mat& launch,Vec& velocity){
    if(!Tuning().physicalDefib||!LocalAlive()||MenuActive()||Value<void*>(weapon,0x28)!=Soldier())return false;
    AcquireSRWLockShared(&defibLock);
    const bool ok=defib.owner==Soldier()&&InventoryItem(defib.owner,Value<int>(defib.owner,0x1E0))==weapon&&
        DefibLaunchRaw(defib.owner,weapon,defib.origin,GetTickCount64(),launch);
    ReleaseSRWLockShared(&defibLock);
    if(ok)velocity=Scale(Row(launch,2),80.f);return ok;
}
void DefibNativeCreated(void* weapon,void* object){
    if(!object)return;
    AcquireSRWLockExclusive(&defibLock);
    if(defib.weapon==weapon&&defib.phase>0){
        defib.phase=0;defib.together=0;defib.rubMs=defib.rubDistance=0;defib.previousValid=false;
        defib.pulseUntil=0;defib.launchUntil=GetTickCount64()+650;
        InterlockedExchangePointer(&defibWeaponPublished,nullptr);
        Runtime::Haptic(0,.8f,100);Runtime::Haptic(1,.8f,100);
        Log("DEFIB53 native shock created=%p; charge consumed (revive result remains native)",object);
    }
    ReleaseSRWLockExclusive(&defibLock);
}
