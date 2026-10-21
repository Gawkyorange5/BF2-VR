// Local-player magazine/charging prototype. Included inside Bf2Adapter's private
// namespace, after the owned-hand state. BF2 1.50 profile, OpenXR only.
// Native ammo owns all counts and completion notifications. Accepted manual
// steps expire only this local component's refill/finish deadlines.
#include "ReloadProfiles.inl"
#include "MagazineGuideProfiles.inl"
#include "ReloadContacts.inl"
#include "ReloadGripRefinements.inl"
#include "LmgReloadProfiles.inl"
static_assert(sizeof(reloadContacts)/sizeof(reloadContacts[0])==sizeof(reloadWeapons)/sizeof(reloadWeapons[0]),"Every rifle needs visual contact poses");
static_assert(sizeof(reloadHandleVisualOffsets)/sizeof(reloadHandleVisualOffsets[0])==sizeof(reloadWeapons)/sizeof(reloadWeapons[0]),"Every rifle needs a visual offset entry");
static_assert(ReloadAudio::WeaponCount==sizeof(reloadWeapons)/sizeof(reloadWeapons[0]),"Audio and interaction profile counts must match");
enum class ReloadPhase { Idle, GrabInstalled, Remove, Pouch, Insert, HandleReady, HandlePull, HandleReturn, Finish, ShellInsert, ShellCommit, CoverReady, CoverPull, BeltReady, BeltFeed, CoverCloseReady, PistolEject, RevolverCloseReady, RevolverClose };
struct ReloadAmmo {
    void* interfaceObject=nullptr;void* definition=nullptr;
    int rounds=0,capacity=0,magazines=0,state=0;bool refilled=false;
    ULONGLONG tick=0;
};
struct ReloadInput {
    bool enabled=false,grab=false,bypass=false,supportGrip=false;std::uint64_t origin=0,frame=0;
    ULONGLONG tick=0;
};
struct ReloadState {
    void* owner=nullptr;void* weapon=nullptr;void* rig=nullptr;int slot=-1;
    const ReloadWeaponProfile* profile=nullptr;
    const LmgReloadProfile* lmg=nullptr;
    float coverOpen=0,beltProgress=0,beltPeak=0;
    bool beltSeated=false,coverClosing=false,lmgGripValid=false;
    Vec lmgGripPoint{},lmgHandStart{};Mat lmgGrip[21]{};
    bool supported=false,restValid=false,lastGrab=false,request=false,nativeStarted=false,inRange=false;
    bool ejectPending=false,pistolReseat=false,originalMagazineHeld=false;ULONGLONG ejectTick=0;Mat ejectedMagazine{};Vec ejectDirection{};
    ReloadPhase phase=ReloadPhase::Idle;ReloadAmmo ammo{};
    bool buttonReload=false,buttonReloadSeen=false;ULONGLONG buttonReloadTick=0,buttonIdleTick=0;
    Mat rest[16]{},heldFromGrip{},handRest[54]{};
    bool magazineGuided=false,magazineGuideSeated=false;float magazineGuideBlend=0;ULONGLONG magazineGuideTick=0;
    bool handRestValid=false,idleMagazineSuppressed=false;int visualKind=0;
    float magazineLatch=0;ULONGLONG magazineLatchTick=0;
    bool shellPermit=false,actionPending=false,magazineSeated=false;int shellRounds=0,shellReserves=0;
    unsigned actionShots=0;
    float shellClosest=100.f;Vec shellClosestOffset{};
    ULONGLONG actionPoseUntil=0,shellRequestTick=0,shellChainReportTick=0;
    // Native ammo can finish before the original animation has finished.
    // These deadlines control only the local visual pose, never firing/ammo.
    ULONGLONG nativeVisualUntil=0,completedPoseUntil=0;
    // Optional racking is independent of native refill. It must never start
    // another reload or consume a second magazine.
    bool handleOnly=false,handleGripValid=false;Mat handleGrip[21]{};
    Vec handleHandStart{},actionGripPoint{};bool pumpContact=false;
    Mat pumpAimFromRight{};bool pumpAimValid=false;ULONGLONG pumpAimReturnTick=0;
    float handlePull=0,handlePeak=0,handleReturnFrom=0;
    bool needsCharge=false,handleCycled=false,magOutSound=false,handlePullSound=false,soundSuppressed=false;
    ULONGLONG handleReturnTick=0;
    int startRounds=0,startMagazines=0;
    std::uint64_t origin=0,lastFrame=0;
    ULONGLONG poseTick=0,started=0,finishTick=0,bypassUntil=0,feedbackTick=0;
    float feedback=0;unsigned feedbackMs=0;
};
SRWLOCK reloadLock=SRWLOCK_INIT;ReloadInput reloadInput{};ReloadState reload{};
// Rejection-only snapshot. Never dereference this published weapon pointer.
// AI/remote ammo callbacks keep their original behavior without taking the
// reload lock, reading time, or resolving the local player. Matching callbacks
// still revalidate the complete state under the lock before changing anything.
std::atomic<void*> reloadWeaponPublished{nullptr};
bool MaybeTrackedReloadAmmo(void* self){
    void* weapon=reloadWeaponPublished.load(std::memory_order_acquire);
    return weapon&&self&&Value<void*>(static_cast<const BYTE*>(self)-4)==weapon;
}
bool reloadProfile=false,reloadSoundProfile=false,reloadCompletionProfile=false;
void ResetLmgReload(){
    reload.coverOpen=reload.beltProgress=reload.beltPeak=0;
    reload.beltSeated=reload.coverClosing=reload.lmgGripValid=false;
}
const LmgReloadProfile* FindLmgReload(const char* name){
    for(const auto& p:lmgReloadProfiles)if(std::strcmp(p.name,name)==0)return &p;
    return nullptr;
}
int LmgBeltBone(const LmgReloadProfile& p,int i){return p.beltBones[i]?p.beltBones[i]:p.beltFirst+i;}
bool ValidLmgReload(const LmgReloadProfile& p,const ReloadWeaponProfile& weapon){
    if(p.beltCount<1||p.beltCount>11||p.coverFollowerCount>3)return false;
    unsigned used=0;
    for(int i=0;i<p.beltCount;++i){
        const int bone=LmgBeltBone(p,i);
        if(bone<55||bone>=70||(weapon.nativeMeshParts&&bone>=54+weapon.nativeMeshParts))return false;
        const unsigned bit=1u<<(bone-54);if(used&bit)return false;used|=bit;
        if(bone==weapon.magazineBone||bone==weapon.handleBone||bone==p.coverBone||
           !Rigid(p.beltLoose[i])||!Rigid(p.beltSeated[i]))return false;
    }
    for(unsigned i=0;i<p.coverFollowerCount;++i){
        const int bone=p.coverFollowerBones[i];
        if(!p.coverBone||bone<55||bone>=70||bone==p.coverBone||bone==weapon.magazineBone||
           bone==weapon.handleBone||bone==weapon.followerBone||
           (weapon.nativeMeshParts&&bone>=54+weapon.nativeMeshParts))return false;
        const unsigned bit=1u<<(bone-54);if(used&bit)return false;used|=bit;
    }
    const float travel=Length(Sub(Point(p.beltTip,p.beltLoose[p.beltCount-1]),Point(p.beltTip,p.beltSeated[p.beltCount-1])));
    if(!std::isfinite(travel)||travel<.005f||travel>.5f)return false;
    if(p.coverBone){
        if(p.coverBone<55||p.coverBone>=70||(weapon.nativeMeshParts&&p.coverBone>=54+weapon.nativeMeshParts)||p.coverBone==weapon.magazineBone||p.coverBone==weapon.handleBone||
           !p.coverKeys||p.coverKeyCount<2||p.coverKeyCount>32||p.coverKeys[0].progress!=0||
           p.coverKeys[p.coverKeyCount-1].progress!=1)return false;
        for(unsigned i=0;i<p.coverKeyCount;++i){
            for(unsigned j=0;j<p.coverFollowerCount;++j)if(!Rigid(p.coverKeys[i].followers[j]))return false;
            if(!Rigid(p.coverKeys[i].pose)||!std::isfinite(p.coverKeys[i].progress)||
               (i&&p.coverKeys[i].progress<=p.coverKeys[i-1].progress))return false;
        }
    }
    for(int i=0;i<21;++i)if(!Rigid(p.beltHand[i])||(p.coverBone&&!Rigid(p.coverHand[i])))return false;
    return true;
}
bool ManualPistol(){return reload.profile&&(reload.profile->pistol||(reload.profile->soundIndex>=35&&reload.profile->soundIndex<=40));}
bool ManualPullPistolMagazine(){
    // r115: this pistol has an exposed extended magazine, unlike the
    // flush magazines inside the firing grip. B ejection remains enabled.
    return reload.profile&&!std::strcmp(reload.profile->name,"wf_g18c");
}
bool ManualMagazineButton(){
    // MP7 magazines sit inside the firing grip. Reuse the pistol ejection
    // gesture without classifying the SMG as a pistol or changing its poses.
    return ManualPistol()||(reload.profile&&
        (!std::strcmp(reload.profile->name,"wf_mp7")||!std::strcmp(reload.profile->name,"sasrif_mp7")));
}
bool ManualAction(){return reload.profile&&reload.profile->manualReturn&&Config().chargingHandle!=0;}
bool ManualPump(){return ManualAction()&&(reload.profile->reloadAmount==1||reload.profile->pumpAction);}
float ActionRearThreshold(){return reload.profile->handleTravel*(ManualPump()?.70f:.75f);}
float ActionHomeThreshold(){
    const float home=std::fmax(.018f,std::fmin(.05f,reload.profile->handleTravel*.35f));
    // r34: start pump return assistance about 8mm earlier on the two current
    // pump rails. Keep at least 6mm between rear acceptance and home, so a
    // rear-only pull can never count as a completed forward stroke.
    // Bolt thresholds retain the previously accepted final lowering gesture.
    return ManualPump()?std::fmin(home+.010f,ActionRearThreshold()-.006f):std::fmax(.018f,home-.010f);
}
bool PumpSupportPhase(){return ManualPump()&&(reload.phase==ReloadPhase::Idle||reload.phase==ReloadPhase::HandleReady||
    reload.phase==ReloadPhase::HandlePull||reload.phase==ReloadPhase::HandleReturn);}
struct ReloadActionMemory {void* owner=nullptr;void* weapon=nullptr;void* definition=nullptr;int rounds=0;bool pending=false;unsigned shots=0;};
ReloadActionMemory reloadActionMemory[8]{};unsigned reloadActionCursor=0;
void RememberReloadAction(){
    if(!ManualAction()||!reload.ammo.interfaceObject)return;
    ReloadActionMemory* entry=nullptr;
    for(auto& m:reloadActionMemory)if(m.owner==reload.owner&&m.weapon==reload.weapon){entry=&m;break;}
    if(!entry)entry=&reloadActionMemory[reloadActionCursor++%8];
    *entry={reload.owner,reload.weapon,Value<void*>(reload.weapon,0x14),reload.ammo.rounds,reload.actionPending,reload.actionShots};
}
void ReloadCue(ReloadAudio::Cue cue){
    const bool rackCue=reload.handleOnly&&(cue==ReloadAudio::HandlePull||cue==ReloadAudio::HandleHome);
    const bool direct=reload.profile&&(reload.profile->reloadAmount||ManualAction()||ManualMagazineButton());
    if(reloadSoundProfile&&(reload.soundSuppressed||rackCue||direct)&&reload.profile&&ReloadAudio::Ready()){
        ReloadAudio::Play(reload.profile->soundIndex,cue);
        Log("MRELOAD21 cue weapon=%s event=%u",reload.profile->name,static_cast<unsigned>(cue));
    }
}

bool ReloadActive(){return reload.phase!=ReloadPhase::Idle;}
// Finish now lasts only until the next native update commits the reload.
// Keep trigger input gated until then: sending its press to a still-reloading
// semi-auto weapon can consume that press before native completion is seen.
bool ReloadBlocksFire(){return ReloadActive()||reload.actionPending||
    (reload.profile&&reload.profile->magazineFollowsLatch&&reload.magazineLatch>.001f);}
bool ReloadHolding(){return ReloadActive()&&!reload.handleOnly&&reload.phase!=ReloadPhase::Finish;}
const char* ReloadPhaseName(){
    switch(reload.phase){case ReloadPhase::PistolEject:return "pistol magazine ejecting";case ReloadPhase::GrabInstalled:return "grab installed magazine";
    case ReloadPhase::Remove:return "pull magazine out and release";case ReloadPhase::Pouch:return "grab fresh magazine at left chest";
    case ReloadPhase::Insert:return "insert and release magazine";case ReloadPhase::HandleReady:return "grab charging handle";
    case ReloadPhase::HandlePull:return "pull charging handle back";case ReloadPhase::HandleReturn:return "charging handle returning";
    case ReloadPhase::ShellInsert:return "insert one shell and release";
    case ReloadPhase::ShellCommit:return "waiting for one native shell refill";
    case ReloadPhase::CoverReady:return "grab and lift top cover";
    case ReloadPhase::CoverPull:return reload.coverClosing?"lower top cover":"lift top cover";
    case ReloadPhase::BeltReady:return "grab loose belt end";
    case ReloadPhase::BeltFeed:return "place belt into feed tray";
    case ReloadPhase::CoverCloseReady:return "grab and close top cover";
    case ReloadPhase::RevolverCloseReady:return "grab open cylinder";
    case ReloadPhase::RevolverClose:return "push cylinder closed";
    case ReloadPhase::Finish:return "native completion";default:return "idle";}
}
void ReloadFeedback(float strength,unsigned ms){
    if(strength>=reload.feedback){reload.feedback=strength;reload.feedbackMs=ms;}
}
void ReloadStep(ReloadPhase phase){
    reload.phase=phase;reload.inRange=false;
    reload.magazineGuided=reload.magazineGuideSeated=false;reload.magazineGuideBlend=0;reload.magazineGuideTick=0;
    if(phase==ReloadPhase::Remove)reload.magOutSound=false;
    if(phase==ReloadPhase::HandlePull)reload.handlePullSound=false;
    Log("MRELOAD21 phase=%s ammo=%d/%d magazines=%d nativeState=%d",ReloadPhaseName(),reload.ammo.rounds,reload.ammo.capacity,reload.ammo.magazines,reload.ammo.state);
}
void ResumeShellAction(){
    // A shell can be loaded with the pump parked. Preserve the stroke and
    // firing lock, then return to it after this independent shell gesture.
    ReloadStep(ManualPump()&&(reload.actionPending||reload.handlePull>.001f||reload.handleCycled)?ReloadPhase::HandleReady:ReloadPhase::Idle);
}
void ReloadCancel(const char* reason,ULONGLONG now){
    if(ReloadActive())Log("MRELOAD21 fallback reason=%s phase=%s; native ammo resumes",reason,ReloadPhaseName());
    ReloadAudio::Stop();reload.soundSuppressed=false;
    reload.phase=ReloadPhase::Idle;reload.request=false;reload.nativeStarted=false;reload.inRange=false;reload.ejectPending=reload.pistolReseat=reload.originalMagazineHeld=false;
    reload.handlePull=reload.handlePeak=reload.handleReturnFrom=0;reload.needsCharge=reload.handleCycled=false;
    reload.handleOnly=reload.handleGripValid=reload.pumpContact=false;reload.visualKind=0;ResetLmgReload();
    reload.pumpAimValid=false;reload.pumpAimReturnTick=0;
    reload.magazineLatch=0;reload.magazineLatchTick=0;
    reload.magazineGuided=reload.magazineGuideSeated=false;reload.magazineGuideBlend=0;reload.magazineGuideTick=0;
    reload.nativeVisualUntil=reload.completedPoseUntil=0;
    reload.shellPermit=false;reload.actionPending=false;reload.actionShots=0;reload.magazineSeated=false;reload.actionPoseUntil=0;RememberReloadAction();
    // Let an already-started native animation complete without re-catching it.
    reload.bypassUntil=now+(reload.profile&&reload.profile->reloadAmount?15000:7000);reload.lastGrab=true;
    reload.buttonReload=reload.buttonReloadSeen=false;reload.buttonIdleTick=0;
}
#include "SurvivalWeaponNames.inl"
bool ReloadName(void* weapon,char (&name)[80]){
    void* def=Value<void*>(weapon,0x14);const unsigned n=Value<unsigned>(def,0x20),cap=Value<unsigned>(def,0x24);
    if(!n||n>=sizeof(name)||cap<n||cap>4096)return false;
    const char* data=cap<16?static_cast<const char*>(def)+0x10:Value<const char*>(def,0x10);
    if(!Copy(name,data,n+1)||name[n])return false;
    for(unsigned i=0;i<n;++i){char& c=name[i];if(c>='A'&&c<='Z')c+=32;if(!((c>='a'&&c<='z')||(c>='0'&&c<='9')||c=='_'||c=='-'))return false;}SurvivalWeaponName(name);return true;
}
bool ReloadOwned(ULONGLONG now){
    return reloadProfile&&reload.supported&&reload.profile&&reloadInput.enabled&&now>=reloadInput.tick&&now-reloadInput.tick<350&&
        reload.poseTick&&now>=reload.poseTick&&now-reload.poseTick<350&&reload.origin==reloadInput.origin&&
        reload.owner&&reload.owner==Soldier()&&LocalAlive()&&Value<void*>(reload.weapon,0x28)==reload.owner&&
        Value<void*>(reload.owner,0x288)==reload.rig&&Value<int>(reload.owner,0x1E0)==reload.slot&&
        Value<int>(reload.owner,0x2B8)==0&&!Value<void*>(reload.owner,0x254);
}
bool ReadReloadAmmo(void* self,ReloadAmmo& out){
    if(!reload.profile||Value<void*>(self)!=game+0x4F5420)return false;
    const BYTE* base=static_cast<const BYTE*>(self)-0x10;
    if(Value<void*>(base)!=game+0x4F5500||Value<void*>(base,0x0C)!=reload.weapon)return false;
    void* def=Value<void*>(self,0x4C);
    const float seconds=Value<float>(def,0x0C);
    const int nativeCapacity=Value<int>(def,0x38);
    // DZB2 retains the stock G3 mesh/animation but specifies a 30-round mag.
    // Accept only that additional audited definition; never write ammo counts.
    const bool capacityMatches=nativeCapacity==reload.profile->capacity||
        (nativeCapacity==30&&!std::strcmp(reload.profile->name,"usrif_g3a3"));
    if(Value<void*>(def)!=game+0x4F5558||Value<int>(def,0x3C)!=reload.profile->reloadAmount||!capacityMatches||
       !std::isfinite(seconds)||std::abs(seconds-reload.profile->reloadSeconds)>.001f)return false;
    const auto begin=Value<std::uintptr_t>(self,8),end=Value<std::uintptr_t>(self,12),capacity=Value<std::uintptr_t>(self,16);
    if(!begin||end<=begin||end>capacity||(end-begin)%4||capacity-begin>512)return false;
    int rounds=0;if(!Read(reinterpret_cast<void*>(begin),0,rounds)||rounds<0||rounds>nativeCapacity)return false;
    const int state=Value<int>(self,0x3C);if(state<0||state>6)return false;
    out.interfaceObject=self;out.definition=def;out.rounds=rounds;out.capacity=nativeCapacity;
    out.magazines=(int)((end-begin)/4);out.state=state;out.refilled=Value<BYTE>(self,0x40)!=0;out.tick=GetTickCount64();return true;
}
// Native automatic-empty query is separate from the explicit reload command.
// Check local inventory ownership independently of tracking/holster visibility.
// Never touch another soldier or a mounted weapon's reload policy.
bool LocalManualAmmo(void* self){
    if(Value<void*>(self)!=game+0x4F5420)return false;
    const BYTE* base=static_cast<const BYTE*>(self)-0x10;
    if(Value<void*>(base)!=game+0x4F5500)return false;
    void* weapon=Value<void*>(base,0x0C);
    void* player=Player();void* owner=Value<void*>(Value<void*>(player,0xCC),4);
    // Reject another soldier before settings, alive/seat and inventory work.
    // One live player lookup also avoids repeatedly querying ClassManager.
    if(!owner||Value<void*>(weapon,0x28)!=owner)return false;
    if(!Tuning().manualReload||Value<BYTE>(player,0xE8)||
       Value<void*>(owner)!=game+0x4E80F8||Value<void*>(owner,0x254)||
       (Value<void*>(Value<void*>(player,0x80),4)!=owner&&!PassengerWeaponSeat(owner)))return false;
    const auto begin=Value<std::uintptr_t>(owner,0x1FC),end=Value<std::uintptr_t>(owner,0x200);
    if(!begin||end<begin||end-begin<16||end-begin>64||(end-begin)%4)return false;
    // Stock slots 2/3 are sidearm/primary. Only the audited magazine-fed XM25
    // aliases opt in from gadget slots; all other gadget cycles stay native.
    if(Value<void*>(reinterpret_cast<void*>(begin),8)!=weapon&&Value<void*>(reinterpret_cast<void*>(begin),12)!=weapon){
        bool owned=false;for(auto at=begin;at<end;at+=4)if(Value<void*>(reinterpret_cast<void*>(at))==weapon){owned=true;break;}
        char name[80]{};
        if(!owned||!ReloadName(weapon,name)||(strcmp(name,"bf4_xm25")&&strcmp(name,"bf4_xm25_i7")&&strcmp(name,"bf4_xm25_player")))return false;
    }
    void* def=Value<void*>(self,0x4C);
    return Value<void*>(def)==game+0x4F5558&&Value<int>(def,0x38)>1;
}
using AmmoAutoReload=bool(__thiscall*)(void*);AmmoAutoReload originalAmmoAutoReload=nullptr;
bool HoldWireAutomaticReload(void* self);
bool __fastcall ReloadAmmoAutomatic(void* self,void*){
    if(!originalAmmoAutoReload(self))return false;
    if(HoldWireAutomaticReload(self))return false;
    if(!LocalManualAmmo(self))return true;
    static void* last=nullptr;static ULONGLONG report=0;const auto now=GetTickCount64();
    if(last!=self||now-report>10000){last=self;report=now;Log("MRELOAD57 empty auto reload prevented component=%p; awaiting physical reload",self);}
    return false;
}
using AmmoUpdate=void(__thiscall*)(void*,unsigned,unsigned);AmmoUpdate originalAmmoUpdate=nullptr;
using AmmoCanFire=bool(__thiscall*)(void*,unsigned,unsigned);AmmoCanFire originalAmmoCanFire=nullptr;
using AmmoRefill=void(__thiscall*)(void*);AmmoRefill originalAmmoRefill=nullptr;
using AmmoReloadRequest=bool(__thiscall*)(void*);AmmoReloadRequest originalAmmoReloadRequest=nullptr;
// Called with reloadLock held. A shell permit belongs to one exact native
// component and one inventory snapshot, not to a duration or reload loop.
bool OwnedShellAmmo(void* self,ReloadAmmo& ammo,ULONGLONG now){
    return reload.profile&&reload.profile->reloadAmount==1&&reload.restValid&&
        !reloadInput.bypass&&now>=reload.bypassUntil&&ReloadOwned(now)&&
        (!reload.ammo.interfaceObject||reload.ammo.interfaceObject==self)&&ReadReloadAmmo(self,ammo);
}
bool ShellRefillPermitted(const ReloadAmmo& ammo){
    return reload.shellPermit&&reload.phase==ReloadPhase::ShellCommit&&
        ammo.rounds==reload.shellRounds&&ammo.magazines==reload.shellReserves&&
        ammo.rounds<ammo.capacity&&ammo.magazines>1;
}
// Called under reloadLock at the normal native ammo-update boundary. State2
// still negotiates reload startup normally. State3..6 compare +1C/+20 against
// the simulation clock, refill through the original method, then set +42 for
// the weapon to notify its animation/fire components. Never just clear state:
// that skips the notification and can leave a loaded weapon unable to fire.
bool FinishManualReloadNow(void* self,const ReloadAmmo& ammo,ULONGLONG now){
    if(!reloadCompletionProfile||ammo.state<3||ammo.state>6||
       self!=reload.ammo.interfaceObject||!reload.restValid||
       reloadInput.bypass||now<reload.bypassUntil||!ReloadOwned(now))return false;
    const bool shell=reload.profile->reloadAmount==1;
    if(shell){
        // A seated shell authorizes exactly one refill. Without that permit,
        // the existing refill hook rejects ammo changes; finish any leftover
        // native opening/closing animation so it cannot lock a completed pump.
        if(reload.shellPermit&&!ShellRefillPermitted(ammo))return false;
    }else if(ammo.state!=3||!reload.nativeStarted||!reload.magazineSeated||
             reload.phase!=ReloadPhase::Finish||reload.handleOnly||reload.actionPending)return false;
    // r55: a seated box alone cannot authorize a belt-fed refill.
    if(!shell&&reload.lmg&&(!reload.beltSeated||reload.coverOpen>.001f))return false;
    struct Deadlines{unsigned refill,finish;} before{},due{};
    unsigned tick=0;
    if(!Read(self,0x1C,before)||!Read(game,0x5A7428,tick))return false;
    if(before.refill<=tick&&before.finish<=tick)return true;
    due.refill=due.finish=tick;
    if(!Copy(static_cast<BYTE*>(self)+0x1C,&due,sizeof(due)))return false;
    Log("MRELOAD32 native deadlines released weapon=%s phase=%s nativeState=%d shellPermit=%d refillTicks=%u finishTicks=%u",
        reload.profile->name,ReloadPhaseName(),ammo.state,shell&&reload.shellPermit?1:0,
        before.refill>tick?before.refill-tick:0,before.finish>tick?before.finish-tick:0);
    return true;
}
void ObserveManualReloadCompletion(const ReloadAmmo& ammo,ULONGLONG now){
    if(reload.profile->reloadAmount){
        if(reload.phase==ReloadPhase::ShellCommit&&ammo.rounds>reload.shellRounds){
            if(ammo.rounds==reload.shellRounds+1&&ammo.magazines==reload.shellReserves-1){
                Log("MRELOAD21 shell committed weapon=%s rounds=%d -> %d reserves=%d -> %d",reload.profile->name,reload.shellRounds,ammo.rounds,reload.shellReserves,ammo.magazines);
                reload.shellPermit=false;reload.nativeStarted=false;reload.request=false;
                if(ManualAction()&&reload.shellRounds==0)reload.actionPending=true;
                ResumeShellAction();ReloadFeedback(.65f,65);RememberReloadAction();
            }else ReloadCancel("shell ammo changed outside single-shell update",now);
        }
    }else if(reload.nativeStarted&&ammo.state==0&&(reload.phase==ReloadPhase::Finish||reload.handleOnly)){
        Log("MRELOAD21 completed native rounds %d -> %d magazines %d -> %d",reload.startRounds,ammo.rounds,reload.startMagazines,ammo.magazines);
        if(reload.magazineSeated){
            reload.completedPoseUntil=std::max(now+350,reload.nativeVisualUntil);
            Log("MRELOAD86 completed pose retained weapon=%s visualMs=%llu; native firing released",reload.profile->name,reload.completedPoseUntil-now);
        }
        reload.nativeStarted=false;reload.request=false;reload.soundSuppressed=false;reload.needsCharge=false;
        // A partial reload can complete while its OPTIONAL handle is still
        // held. Finish ammo accounting without dropping that physical grip.
        if(!reload.handleOnly){ReloadStep(ReloadPhase::Idle);reload.handlePull=reload.handlePeak=0;reload.handleCycled=false;}
        ReloadFeedback(.7f,100);
    }
}
void __fastcall ReloadAmmoRefill(void* base,void*){
    if(!base||!MaybeTrackedReloadAmmo(static_cast<BYTE*>(base)+0x10)){originalAmmoRefill(base);return;}
    AcquireSRWLockShared(&reloadLock);
    ReloadAmmo ammo{};
    const bool block=OwnedShellAmmo(static_cast<BYTE*>(base)+0x10,ammo,GetTickCount64())&&!ShellRefillPermitted(ammo);
    ReleaseSRWLockShared(&reloadLock);
    // Leave the original UPDATE running: it must reach its deadline and emit
    // completion to the weapon's animation/fire components even if no shell
    // is authorized. No count, reserve, timer or state field is written here.
    if(!block)originalAmmoRefill(base);
}
bool __fastcall ReloadAmmoReloadRequest(void* self,void*){
    // Let BF2 consume its request flag. Merely returning early would leave an
    // old automatic request queued for the next physical shell interaction.
    const bool request=originalAmmoReloadRequest(self);
    if(!request)return false;
    const bool localManual=LocalManualAmmo(self);
    if(!localManual&&!MaybeTrackedReloadAmmo(self))return true;
    AcquireSRWLockExclusive(&reloadLock);
    ReloadAmmo ammo{};const auto now=GetTickCount64();
    const bool shellBlock=OwnedShellAmmo(self,ammo,now)&&!ShellRefillPermitted(ammo);
    const bool authorized=reload.profile&&self==reload.ammo.interfaceObject&&ReloadActive()&&!reload.handleOnly&&!reload.originalMagazineHeld&&
        (!reload.profile->reloadAmount||ShellRefillPermitted(reload.ammo));
    const bool block=shellBlock||(localManual&&!authorized);
    if(block&&ammo.rounds>0&&now-reload.shellChainReportTick>1000){
        reload.shellChainReportTick=now;
        Log("MRELOAD21 shell automatic continuation stopped weapon=%s rounds=%d reserves=%d nativeState=%d; awaiting next inserted shell",reload.profile->name,ammo.rounds,ammo.magazines,ammo.state);
    }
    ReleaseSRWLockExclusive(&reloadLock);
    return !block;
}
// Called only for the bound, validated local ammo component, under reloadLock.
// Shell reloads briefly return to state0 between rounds; +43 is BF2's queued
// continuation request. Wait for that queue and a short idle boundary before
// returning control, instead of the old unconditional 15-second lockout.
void ObserveButtonReload(const ReloadAmmo& ammo,ULONGLONG now){
    if(!reload.buttonReload)return;
    if(ammo.state>=2){reload.buttonReloadSeen=true;reload.buttonIdleTick=0;}
    const bool pending=Value<BYTE>(ammo.interfaceObject,0x43)!=0;
    if(reloadInput.bypass||ammo.state>=2||pending){
        reload.buttonIdleTick=0;reload.bypassUntil=now+15000;return;
    }
    if(!reload.buttonIdleTick)reload.buttonIdleTick=now;
    const bool completed=reload.buttonReloadSeen&&now-reload.buttonIdleTick>=60;
    const bool notStarted=!reload.buttonReloadSeen&&now-reload.buttonReloadTick>=350;
    if(!completed&&!notStarted)return;
    reload.buttonReload=false;reload.buttonReloadSeen=false;reload.buttonIdleTick=0;
    reload.bypassUntil=0;reload.lastGrab=reloadInput.grab;
    Log("MRELOAD35 button reload released weapon=%s nativeState=%d rounds=%d reserves=%d reason=%s; manual controls ready",
        reload.profile->name,ammo.state,ammo.rounds,ammo.magazines,completed?"native completion":"no native reload requested");
}
void __fastcall ReloadAmmoUpdate(void* self,void*,unsigned a,unsigned b){
    if(!MaybeTrackedReloadAmmo(self)){originalAmmoUpdate(self,a,b);return;}
    bool hold=false,finishNow=false;const auto now=GetTickCount64();
    AcquireSRWLockExclusive(&reloadLock);
    if(reload.supported&&Value<void*>(static_cast<const BYTE*>(self)-4)==reload.weapon){
        ReloadAmmo ammo{};
        if(ReadReloadAmmo(self,ammo)){
            if(!reload.ammo.interfaceObject)Log("MRELOAD21 native ammo bound weapon=%p interface=%p rounds=%d magazines=%d reload=%.3fs",reload.weapon,self,ammo.rounds,ammo.magazines,reload.profile->reloadSeconds);
            // A firearm can have separate primary/secondary components.
            // Once bound, a second component must not replace this identity.
            if(reload.ammo.interfaceObject&&reload.ammo.interfaceObject!=self){ReleaseSRWLockExclusive(&reloadLock);originalAmmoUpdate(self,a,b);return;}
            const ReloadAmmo previous=reload.ammo;reload.ammo=ammo;
            if(reload.buttonReload&&ReloadOwned(now))ObserveButtonReload(ammo,now);
            if(ManualAction()&&ReloadOwned(now)&&!reloadInput.bypass&&now>=reload.bypassUntil){
                if(!previous.interfaceObject){
                    for(const auto& m:reloadActionMemory)if(m.owner==reload.owner&&m.weapon==reload.weapon&&
                        m.definition==Value<void*>(reload.weapon,0x14)&&m.rounds==ammo.rounds){reload.actionPending=m.pending;reload.actionShots=m.shots;break;}
                }else if(reload.restValid&&reload.profile->cycleAfterShot&&previous.interfaceObject==self&&previous.rounds>ammo.rounds&&previous.magazines==ammo.magazines&&
                    unsigned(previous.rounds-ammo.rounds)<=reload.profile->shotsPerCycle){
                    reload.actionShots=std::min(reload.profile->shotsPerCycle,reload.actionShots+unsigned(previous.rounds-ammo.rounds));
                    reload.actionPending=reload.actionShots>=reload.profile->shotsPerCycle||ammo.rounds==0;
                    if(reload.actionPending)reload.actionPoseUntil=now+2500;
                    RememberReloadAction();
                    Log("MRELOAD111 shot cycle weapon=%s rounds=%d shots=%u/%u pending=%d; native ammo retained",reload.profile->name,ammo.rounds,reload.actionShots,reload.profile->shotsPerCycle,reload.actionPending?1:0);
                }
            }
            if(!ReloadOwned(now)){if(ReloadActive())ReloadCancel("owner/input/pose changed",now);}
            else if(reload.handleOnly&&!reload.nativeStarted&&ammo.state>=2&&!ManualAction()){
                ReloadCancel("native reload began during optional rack",now);
            }
            else if(reload.profile->reloadAmount&&!reloadInput.bypass&&now>=reload.bypassUntil&&reload.restValid){
                // Refill and automatic continuation are gated separately.
                // Never freeze this whole update for shells: doing so traps
                // a partly loaded shotgun in state5 and prevents shooting.
                if(previous.interfaceObject==self&&previous.state>=2&&ammo.state<2)
                    Log("MRELOAD21 shell native completion weapon=%s rounds=%d reserves=%d nativeState=%d",reload.profile->name,ammo.rounds,ammo.magazines,ammo.state);
                ObserveManualReloadCompletion(ammo,now);
                // The previous native sequence can be in its closing state
                // when the next shell arrives. Request once it becomes idle.
                if(reload.phase==ReloadPhase::ShellCommit&&ammo.rounds==reload.shellRounds&&ammo.state<2&&now-reload.shellRequestTick>350){
                    reload.request=true;reload.shellRequestTick=now;
                }
                if(reload.phase==ReloadPhase::ShellCommit&&now-reload.finishTick>7000){ReloadCancel("shell refill did not arrive",now);hold=false;}
                if(ReloadActive()&&now-reload.started>90000){ReloadCancel("shell/action recovery timeout",now);hold=false;}
            }
            else if(!reloadInput.bypass&&now>=reload.bypassUntil&&reload.restValid){
                // State 2 must run: it negotiates the native animation start.
                // State 3 is the whole-magazine reload; this update is the
                // verified refill + finish boundary. Return before both.
                if(!reload.handleOnly&&ammo.state==3&&!ammo.refilled&&ammo.magazines>1){
                    if(!ReloadActive()){
                        reload.started=now;reload.startRounds=ammo.rounds;reload.startMagazines=ammo.magazines;
                        ResetLmgReload();
                        ReloadStep(ManualPistol()&&!ManualPullPistolMagazine()?ReloadPhase::Pouch:reload.lmg&&reload.lmg->coverBone?ReloadPhase::CoverReady:ReloadPhase::GrabInstalled);ReloadFeedback(.35f,65);
                    }
                    if(!reload.nativeStarted){
                        reload.nativeStarted=true;
                        // Include the stock reload bundle's blend-out (up to
                        // 0.9s on the AK74u), plus a small frame margin.
                        reload.nativeVisualUntil=now+static_cast<ULONGLONG>(std::fmin(8.f,std::fmax(1.25f,reload.profile->reloadSeconds+1.25f))*1000.f);
                        reload.completedPoseUntil=0;
                        reload.magazineSeated=false;
                        // Manual magazine swaps include the charging step even
                        // when rounds remained. Preserve the legacy disabled
                        // setting and real pump/bolt action requirements.
                        reload.needsCharge=reload.profile->handleBone&&(Config().chargingHandle!=0||reload.actionPending||(ManualAction()&&reload.handlePull>.008f));
                        if(!ManualAction()){reload.handlePull=reload.handlePeak=reload.handleReturnFrom=0;reload.handleCycled=false;}
                        Log("MRELOAD21 native reload held before refill; rounds=%d magazines=%d charge=%d mode=%d",ammo.rounds,ammo.magazines,reload.needsCharge?1:0,Config().chargingHandle);
                    }
                    hold=ReloadHolding();
                }
                if(ReloadHolding()&&reload.nativeStarted&&(ammo.refilled||ammo.rounds>reload.startRounds||ammo.state<2)){
                    // A network correction/other path can replace native state.
                    // Never counter it by manufacturing or subtracting ammo.
                    ReloadCancel("native state changed outside held update",now);hold=false;
                }
                if(ReloadActive()&&now-reload.started>90000){ReloadCancel("90 second recovery timeout",now);hold=false;}
                ObserveManualReloadCompletion(ammo,now);
                if(ManualAction()&&reload.handleOnly&&ammo.state==3&&!ammo.refilled)hold=true;
            }
            if(!hold)finishNow=FinishManualReloadNow(self,ammo,now);
        }else{
            static unsigned reports=0;if(reports++<12)Log("MRELOAD21 ammo rejected interface=%p table=%p baseTable=%p definition=%p definitionTable=%p capacity=%d reload=%.3f",self,Value<void*>(self),Value<void*>(static_cast<const BYTE*>(self)-0x10),Value<void*>(self,0x4C),Value<void*>(Value<void*>(self,0x4C)),Value<int>(Value<void*>(self,0x4C),0x38),Value<float>(Value<void*>(self,0x4C),0x0C));
            if(ReloadActive()&&self==reload.ammo.interfaceObject)ReloadCancel("ammo layout rejected",now);
        }
    }
    ReleaseSRWLockExclusive(&reloadLock);
    if(!hold)originalAmmoUpdate(self,a,b);
    if(finishNow){
        // Observe this one completed transaction before returning to BF2's
        // weapon update. Do not wait another frame to clear ShellCommit or
        // Finish, and do not call the original update twice. Leave +42/+43
        // for the native weapon's notification/request consumers.
        AcquireSRWLockExclusive(&reloadLock);
        ReloadAmmo after{};const auto finished=GetTickCount64();
        if(self==reload.ammo.interfaceObject&&ReloadOwned(finished)&&
           !reloadInput.bypass&&finished>=reload.bypassUntil&&ReadReloadAmmo(self,after)){
            const auto wait=(reload.phase==ReloadPhase::Finish||reload.phase==ReloadPhase::ShellCommit)&&finished>=reload.finishTick?finished-reload.finishTick:0;
            reload.ammo=after;ObserveManualReloadCompletion(after,finished);
            Log("MRELOAD32 native completion observed weapon=%s nativeState=%d rounds=%d reserves=%d phase=%s blockFire=%d manualWaitMs=%llu",
                reload.profile->name,after.state,after.rounds,after.magazines,ReloadPhaseName(),ReloadBlocksFire()?1:0,wait);
        }
        ReleaseSRWLockExclusive(&reloadLock);
    }
}
bool __fastcall ReloadAmmoCanFire(void* self,void*,unsigned a,unsigned b){
    if(!MaybeTrackedReloadAmmo(self))return originalAmmoCanFire(self,a,b);
    AcquireSRWLockShared(&reloadLock);
    // Other soldiers' fire checks cannot belong to this manual reload. Keep
    // the full live ownership check for matching local components only.
    const bool block=ReloadBlocksFire()&&self&&Value<void*>(self)==game+0x4F5420&&
        Value<void*>(static_cast<const BYTE*>(self)-4)==reload.weapon&&ReloadOwned(GetTickCount64());
    ReleaseSRWLockShared(&reloadLock);
    return block?false:originalAmmoCanFire(self,a,b);
}
bool ReloadHandsFree(HandTargets& target){
    AcquireSRWLockShared(&reloadLock);
    // A trigger-operated pump is still a two-hand support grip. Shell and
    // magazine interactions release support, as do all other weapon handles.
    const auto now=GetTickCount64();
    const bool pump=PumpSupportPhase()&&reload.restValid&&ReloadOwned(now)&&!reloadInput.bypass&&now>=reload.bypassUntil;
    target.pumpPose=pump;
    if(pump&&reload.pumpAimValid&&target.supportPressed&&target.leftValid&&target.rightValid){
        const float weight=reload.pumpAimReturnTick?
            std::fmax(0.f,1.f-static_cast<float>(now>=reload.pumpAimReturnTick?now-reload.pumpAimReturnTick:0)/90.f):1.f;
        target.pumpAimFromRight=reload.pumpAimFromRight;target.pumpAimWeight=weight;
    }
    // Release support only after an actual part interaction was accepted.
    // A trigger press at the front support point must not briefly detach the
    // gun. ApplyReload uses the raw tracked palm for its proximity checks, so
    // it can still acquire a magazine/handle while the support solve is active.
    // A completed handle's stale trigger hold and native Finish also remain
    // eligible for support.
    const bool interaction=ReloadActive()&&reload.phase!=ReloadPhase::Finish;
    const bool free=reload.supported&&reloadInput.enabled&&interaction&&!pump;
    ReleaseSRWLockShared(&reloadLock);return free;
}
bool ReloadBodyArmsHeld(void* owner){
    AcquireSRWLockShared(&reloadLock);
    const auto now=GetTickCount64();
    const bool held=reload.owner==owner&&(ReloadActive()||reload.actionPending||now<reload.actionPoseUntil||now<reload.completedPoseUntil||
        (reload.profile&&reload.profile->reloadAmount&&reload.ammo.state>=2))&&ReloadOwned(now)&&!reloadInput.bypass&&now>=reload.bypassUntil;
    ReleaseSRWLockShared(&reloadLock);return held;
}
bool ReloadContactHeld(void* owner,void* weapon,int kind){
    AcquireSRWLockShared(&reloadLock);
    const bool pump=kind==2&&reload.pumpContact&&reloadInput.supportGrip&&PumpSupportPhase();
    const bool phase=kind==1?(reload.phase==ReloadPhase::Remove||reload.phase==ReloadPhase::Insert||reload.phase==ReloadPhase::ShellInsert):
        kind==3?(reload.phase==ReloadPhase::CoverPull||reload.phase==ReloadPhase::BeltFeed):
        (reload.phase==ReloadPhase::HandlePull||(ManualAction()&&reload.phase==ReloadPhase::HandleReturn));
    const bool held=((phase&&reloadInput.grab)||pump)&&reload.owner==owner&&reload.weapon==weapon&&!reloadInput.bypass&&ReloadOwned(GetTickCount64());
    ReleaseSRWLockShared(&reloadLock);return held;
}
void ReloadHandBasis(const Mat native[70],Mat basis[70]){
    memcpy(basis,native,70*sizeof(Mat));
    AcquireSRWLockShared(&reloadLock);
    const auto now=GetTickCount64();
    if((ReloadActive()||reload.actionPending||now<reload.actionPoseUntil||now<reload.completedPoseUntil||
       (reload.profile&&reload.profile->reloadAmount&&reload.ammo.state>=2))&&reload.handRestValid&&ReloadOwned(now)&&!reloadInput.bypass&&now>=reload.bypassUntil)
        for(int i=2;i<54;++i)basis[i]=Mul(reload.handRest[i],native[1]);
    ReleaseSRWLockShared(&reloadLock);
}
// Only the verified DefaultSoundComp reload notification is intercepted.
// It starts Reload1P/Reload3P; it does not advance ammo or animation state.
// Its final writes are retained exactly as the native no-sound branch does.
using ReloadSoundNotify=void(__thiscall*)(void*,unsigned);
ReloadSoundNotify originalReloadSound=nullptr;
void __fastcall ReloadSoundEvent(void* self,void*,unsigned state){
    bool suppress=false;const auto now=GetTickCount64();
    AcquireSRWLockExclusive(&reloadLock);
    const bool shell=reload.profile&&reload.profile->reloadAmount;
    const bool cycle=state==7&&ManualAction();
    const bool manualSound=cycle||(shell&&state>=3&&state<=6)||(state==3&&!reload.handleOnly);
    if(manualSound&&reloadSoundProfile&&ReloadAudio::Ready()&&ReloadOwned(now)&&reload.restValid&&
       !reloadInput.bypass&&now>=reload.bypassUntil&&reload.ammo.interfaceObject&&
       (cycle||shell||(reload.ammo.rounds<reload.ammo.capacity&&reload.ammo.magazines>1))&&
       Value<void*>(self)==game+0x4F5F30&&Value<void*>(static_cast<const BYTE*>(self)-0x10)==game+0x4F5FC8&&
       Value<void*>(static_cast<const BYTE*>(self)-4)==reload.weapon&&
       (!Value<void*>(self,0x0C)||Value<void*>(self,0x0C)==reload.weapon)){
        const BYTE reloaded=1;const unsigned fireDelay=0;
        suppress=Copy(static_cast<BYTE*>(self)+0x45,&reloaded,sizeof(reloaded))&&
                 Copy(static_cast<BYTE*>(self)+0x40,&fireDelay,sizeof(fireDelay));
        if(suppress&&shell&&(state==4||state==6)){
            const BYTE stage=state==4?1:2;
            suppress=Copy(static_cast<BYTE*>(self)+0x44,&stage,sizeof(stage));
        }
        if(suppress){
            if(!reload.soundSuppressed)Log("MRELOAD21 stock reload sound suppressed weapon=%s component=%p state=%u",reload.profile->name,self,state);
            reload.soundSuppressed=true;
        }
    }
    ReleaseSRWLockExclusive(&reloadLock);
    if(!suppress)originalReloadSound(self,state);
}
bool InstallReloadSound(){
    if(!reloadProfile||!Config().reloadSounds)return false;
    const BYTE entry[]={0x81,0xEC,0xFC,0,0,0};
    const BYTE finish[]={0xC6,0x46,0x45,0x01,0xC7,0x46,0x40,0,0,0,0};
    if(Value<void*>(game,0x4F5F30+18*4)!=game+0x208470||
       !Match(game+0x208470,entry,sizeof(entry))||!Match(game+0x208554,finish,sizeof(finish))||
       !ReloadAudio::Initialize())return false;
    return Jump(game+0x208470,entry,sizeof(entry),reinterpret_cast<void*>(&ReloadSoundEvent),reinterpret_cast<void**>(&originalReloadSound));
}

Mat ReloadBlendPose(const Mat& a,const Mat& b,float t){
    // Adjacent authored keys are close. Re-orthogonalize their blended axes
    // so a rotating handle remains rigid rather than shrinking/shearing.
    const Vec y=Unit(Add(Scale(Row(a,1),1-t),Scale(Row(b,1),t)));
    const Vec rawX=Add(Scale(Row(a,0),1-t),Scale(Row(b,0),t));
    const Vec x=Unit(Sub(rawX,Scale(y,Dot(rawX,y)))),z=Cross(x,y);
    Mat result=Identity();const Vec rows[]={x,y,z};
    for(int i=0;i<3;++i){result.m[i][0]=rows[i].x;result.m[i][1]=rows[i].y;result.m[i][2]=rows[i].z;}
    SetPosition(result,Add(Scale(Position(a),1-t),Scale(Position(b),t)));return result;
}
Mat ReloadHandlePose(float distance,bool follower){
    const auto& p=*reload.profile;
    for(unsigned i=1;i<p.handleKeyCount;++i){
        const auto& a=p.handleKeys[i-1];const auto& b=p.handleKeys[i];
        if(distance<=b.distance||i+1==p.handleKeyCount){
            const float t=std::fmax(0.f,std::fmin(1.f,(distance-a.distance)/(b.distance-a.distance)));
            return ReloadBlendPose(follower?a.follower:a.handle,follower?b.follower:b.handle,t);
        }
    }
    return Identity();
}
void ReloadHandleProgress(Vec delta,float& along,float& side){
    const auto& p=*reload.profile;
    if(!p.handleKeyCount){const Vec axis=Unit(p.handleAxis);along=Dot(delta,axis);side=Length(Sub(delta,Scale(axis,along)));return;}
    const Vec start=Point(p.handleGrab,p.handleKeys[0].handle),q=Add(start,delta);
    float best=1.e9f;along=0;
    for(unsigned i=1;i<p.handleKeyCount;++i){
        const Vec a=Point(p.handleGrab,p.handleKeys[i-1].handle),b=Point(p.handleGrab,p.handleKeys[i].handle),ab=Sub(b,a);
        const float squared=Dot(ab,ab);if(squared<1.e-10f)continue;
        float t=Dot(Sub(q,a),ab)/squared;
        // Keep longitudinal overrun at the two ends for the existing release
        // guards, but clamp intermediate segments to their actual endpoints.
        t=(i==1?std::fmin(1.f,t):std::fmax(0.f,std::fmin(1.f,t)));
        if(i+1==p.handleKeyCount)t=std::fmax(0.f,Dot(Sub(q,a),ab)/squared);
        const float error=Length(Sub(q,Add(a,Scale(ab,t))));
        if(error<best){best=error;along=p.handleKeys[i-1].distance+t*(p.handleKeys[i].distance-p.handleKeys[i-1].distance);}
    }
    side=best;
}
Mat ReloadActionLocal(float distance){
    const auto& p=*reload.profile;
    if(p.handleKeyCount)return Mul(Mul(ReloadHandlePose(distance,false),Inverse(p.handleInstalled)),reload.rest[p.handleBone-54]);
    Mat m=reload.rest[p.handleBone-54];SetPosition(m,Add(Position(m),Scale(Unit(p.handleAxis),distance)));return m;
}
Vec ReloadActionDisplacement(float distance){
    const auto& p=*reload.profile;
    if(p.handleKeyCount)return Sub(Point(p.handleGrab,ReloadHandlePose(distance,false)),Point(p.handleGrab,p.handleKeys[0].handle));
    return Scale(Unit(p.handleAxis),distance);
}
bool ReloadBind(void* owner,void* weapon,void* rig,int slot,const Runtime::Frame& f){
    AcquireSRWLockExclusive(&reloadLock);
    if(owner!=reload.owner||weapon!=reload.weapon||rig!=reload.rig||slot!=reload.slot||f.origin!=reload.origin){
        RememberReloadAction();const bool pending=reload.actionPending;const unsigned shots=reload.actionShots;
        if(ReloadActive())ReloadCancel("weapon/rig/tracking origin changed",f.tick);
        reload.actionPending=pending;reload.actionShots=shots;RememberReloadAction();
        reload={};reload.owner=owner;reload.weapon=weapon;reload.rig=rig;reload.slot=slot;reload.origin=f.origin;
        reload.lastGrab=reloadInput.grab;char name[80]{};
        const bool named=ReloadName(weapon,name);
        reload.profile=named?FindReloadWeapon(name):nullptr;
        reload.lmg=reload.profile?FindLmgReload(name):nullptr;
        reload.supported=reloadProfile&&reload.profile&&Value<void*>(weapon,0x28)==owner&&
            (!reload.lmg||ValidLmgReload(*reload.lmg,*reload.profile));
        Log("MRELOAD21 selected name=%s manual=%d owner=%p weapon=%p",named?name:"unresolved",reload.supported?1:0,owner,weapon);
    }
    reloadWeaponPublished.store(reload.supported?reload.weapon:nullptr,std::memory_order_release);
    // Return a visual-only classification from the existing protected bind.
    // AK-47/GP25 and AK-101/GP30 occupy the fixed profile indices 2 through 5.
    const bool ak=reload.supported&&((reload.profile->soundIndex>=2&&reload.profile->soundIndex<=5)||reload.profile->soundIndex==41);
    reload.poseTick=GetTickCount64();ReleaseSRWLockExclusive(&reloadLock);return ak;
}
Mat ReloadPouch(const HandTargets& target){
    // Accessible seated or standing. Follow the torso heading and tracked
    // head translation, not head pitch/roll. The visible replacement magazine
    // itself marks the pickup location; no invisible inventory-only gesture.
    Mat pouch=target.torso;SetPosition(pouch,Position(target.head));
    const Vec center=Point({-.20f,-.42f,.12f},pouch);
    SetPosition(pouch,Sub(center,Direction(reload.profile->magazineCenter,pouch)));return pouch;
}
bool GrabFreshReloadPart(const HandTargets& target,const Mat solved[70],const Mat& pouch){
    // Row-vector convention: authored wrist = contact * magazine.
    // Solve the magazine from the CURRENT tracked wrist, not the pouch pose.
    // Fingers still use each weapon's authored grasp; the wrist stays put.
    const Mat& contact=reloadContacts[reload.profile->soundIndex].magazine[0];
    if(!Rigid(contact)||!Rigid(solved[7])||!Rigid(target.leftGrip))return false;
    const Mat part=Mul(Inverse(contact),solved[7]);
    const Mat gripLocal=Mul(part,Inverse(target.leftGrip));
    if(!Rigid(part)||!Rigid(gripLocal))return false;
    reload.heldFromGrip=gripLocal;
    Log("MRELOAD31 fresh part to hand weapon=%s partShift=%.3f wristError=%.6f",reload.profile->name,
        Length(Sub(Point(reload.profile->magazineCenter,part),Point(reload.profile->magazineCenter,pouch))),
        Length(Sub(Position(Mul(contact,part)),Position(solved[7]))));
    return true;
}
void ReloadNear(float distance,float radius,ULONGLONG now){
    const bool inRange=distance<radius;
    if(inRange&&!reload.inRange&&now-reload.feedbackTick>500){ReloadFeedback(.18f,25);reload.feedbackTick=now;}
    reload.inRange=inRange;
}
void ReturnReloadHandle(bool cycled,ULONGLONG now){
    reload.handleCycled=cycled;reload.handleReturnFrom=reload.handlePull;reload.handleReturnTick=now;
    ReloadStep(ReloadPhase::HandleReturn);
    ReloadFeedback(cycled?.55f:.18f,cycled?65:30);
}
void CompleteManualAction(ULONGLONG now){
    reload.handlePull=reload.handlePeak=0;reload.actionPending=false;reload.actionShots=0;
    const bool awaitingMagazine=reload.nativeStarted&&!reload.magazineSeated;
    if(!awaitingMagazine)reload.needsCharge=false;
    reload.handleOnly=reload.handleGripValid=false;ReloadCue(ReloadAudio::HandleHome);RememberReloadAction();
    if(awaitingMagazine)ReloadStep(ReloadPhase::GrabInstalled);
    else if(reload.nativeStarted){reload.finishTick=now;ReloadStep(ReloadPhase::Finish);}
    else ReloadStep(ReloadPhase::Idle);
    if(ManualPump()&&reload.pumpAimValid)reload.pumpAimReturnTick=now;
    reload.handleCycled=false;ReloadFeedback(.65f,65);
    Log("MRELOAD21 action closed weapon=%s; assisted physical return accepted",reload.profile->name);
}
void BeginReloadHandle(const HandTargets& target,const Mat solved[70],ULONGLONG now,bool optional,bool supportedPump=false){
    const auto& profile=*reload.profile;
    if(ManualPump()&&supportedPump&&target.supportPressed){
        // Capture the already-solved attitude: entering pump mode has zero
        // rotation jump. Store rotation only, relative to the right aim, so
        // both right-controller translation and rotation stay fully tracked.
        Mat gun=solved[54],aim=target.rightAim;SetPosition(gun,{});SetPosition(aim,{});
        reload.pumpAimFromRight=Mul(gun,Inverse(aim));
        reload.pumpAimValid=Rigid(reload.pumpAimFromRight);reload.pumpAimReturnTick=0;
        Log("MRELOAD33 pump aim latched weapon=%s valid=%d; right controller tracks, left hand slides",profile.name,reload.pumpAimValid?1:0);
    }
    const bool resume=ManualAction()&&reload.phase==ReloadPhase::HandleReady;
    const bool pullSound=reload.handlePullSound;
    const Mat part=Mul(ManualAction()?ReloadActionLocal(reload.handlePull):
        (profile.openBoltReady?profile.handleEmptyInstalled:reload.rest[profile.handleBone-54]),solved[54]);
    const Mat* authored=reloadContacts[profile.soundIndex].handle;
    Mat shaped[70]{};for(int i=0;i<21;++i)shaped[7+i]=Mul(authored[i],part);
    // Match the pad surface more closely than the old terminal-joint center.
    // Corrections are derived from the stock gloves, capped at 8mm from r5.
    // Apply before approach rotation so the refinement follows the hand.
    const Vec visualOffset=reloadHandleVisualOffsets[profile.soundIndex];
    const Vec offset=Direction(visualOffset,part);
    for(int i=7;i<28;++i)SetPosition(shaped[i],Add(Position(shaped[i]),offset));
    const bool m16=profile.soundIndex<2;
    if(!m16&&!ManualPump()){
        // Keep the authored finger shape, but approach the knob with the
        // controller's anatomical palm orientation. The old fixed rotation
        // put the AK/FAMAS wrist across the receiver and into the other hand.
        Mat wristFromPalm{};
        if(PalmBinding(shaped,7,wristFromPalm)){
            Mat palm=Mul(Inverse(wristFromPalm),shaped[7]),tracked=target.leftGrip;
            SetPosition(palm,{});SetPosition(tracked,{});
            const Mat turn=Mul(Inverse(palm),tracked);
            const Vec anchor=Point(profile.handleGrab,part);
            for(int i=7;i<28;++i){
                SetPosition(shaped[i],Sub(Position(shaped[i]),anchor));
                shaped[i]=Mul(shaped[i],turn);
                SetPosition(shaped[i],Add(Position(shaped[i]),anchor));
            }
        }
    }
    reload.handleGripValid=true;const Mat inv=Inverse(part);
    for(int i=0;i<21;++i){reload.handleGrip[i]=Mul(shaped[7+i],inv);if(!Rigid(reload.handleGrip[i]))reload.handleGripValid=false;}
    reload.handleOnly=optional;
    // Track the grabbed point on the controller, not its origin. Wrist roll
    // around the knob should not sweep a 6-9cm lever arm off the action rail.
    const Vec knob=Point(profile.handleGrab,part);
    reload.actionGripPoint=Point(knob,Inverse(target.leftGrip));
    reload.handleHandStart=Point(ManualAction()?knob:Position(target.leftGrip),Inverse(solved[54]));
    if(resume)reload.handleHandStart=Sub(reload.handleHandStart,ReloadActionDisplacement(reload.handlePull));
    else{reload.handlePull=reload.handlePeak=reload.handleReturnFrom=0;reload.handleCycled=false;}
    if(optional&&!reload.nativeStarted){reload.started=now;reload.startRounds=reload.ammo.rounds;reload.startMagazines=reload.ammo.magazines;reload.request=false;}
    ReloadStep(ReloadPhase::HandlePull);ReloadFeedback(.4f,50);
    if(resume)reload.handlePullSound=pullSound;
    Log("MRELOAD21 handle begin weapon=%s optional=%d nativePending=%d approach=%s gripValid=%d support=%d",profile.name,optional?1:0,reload.nativeStarted?1:0,ManualPump()?"support fore-end":(m16?"M16 refined":"controller palm"),reload.handleGripValid?1:0,target.supportPressed?1:0);
    Log("MRELOAD21 grip refinement weapon=%s localMm=(%.2f %.2f %.2f)",profile.name,visualOffset.x*1000.f,visualOffset.y*1000.f,visualOffset.z*1000.f);
}
#include "LmgReload.inl"

// Only the magazine currently held by the local player is considered. The
// controller-to-magazine grip is never overwritten: pulling out returns to it.
struct MagazineGuidePose {
    Mat pose{};float depth=0,side=0;bool enabled=false,guided=false,seated=false;
};
float MagazineGuideClamp(float x){return std::fmax(0.f,std::fmin(1.f,x));}
MagazineGuidePose GuideReloadMagazine(const Mat& raw,const Mat& socket,const Mat& gun,
                                     ULONGLONG now,bool advance,bool grab){
    MagazineGuidePose result{};result.pose=raw;
    result.enabled=Tuning().magazineGuide;
    if(!result.enabled||!Rigid(raw)||!Rigid(socket)||!Rigid(gun)){
        reload.magazineGuided=reload.magazineGuideSeated=false;reload.magazineGuideBlend=0;reload.magazineGuideTick=0;return result;
    }
    const auto& profile=*reload.profile;
    const Vec axis=Unit(Direction(magazineGuideAxes[profile.soundIndex],gun));
    const Vec rawCenter=Point(profile.magazineCenter,raw),seat=Point(profile.magazineCenter,socket);
    const Vec delta=Sub(rawCenter,seat);
    result.depth=Dot(delta,axis);
    result.side=Length(Sub(delta,Scale(axis,result.depth)));
    const float up=Dot(Row(raw,1),Row(socket,1)),forward=Dot(Row(raw,2),Row(socket,2));
    // Broad approach orientation is enough; an upside-down or backwards
    // magazine does not jump through the receiver into a guide.
    const bool cylinder=profile.soundIndex==13||profile.cylindricalMagazine;
    const bool entryAngle=forward>.35f&&(up>.15f||(cylinder&&forward>.85f));
    const bool entry=entryAngle&&result.side<.065f&&result.depth>-.015f&&result.depth<.18f;
    if(advance){
        const ULONGLONG elapsed=reload.magazineGuideTick&&now>=reload.magazineGuideTick?now-reload.magazineGuideTick:11;
        if(elapsed>120){reload.magazineGuided=reload.magazineGuideSeated=false;reload.magazineGuideBlend=0;}
        const bool previous=reload.magazineGuided;
        // A tracked insertion reaches its hard stop before the real hand
        // stops moving. Remember that contact; do not eject an already seated
        // visible magazine because the controller pushed through the receiver.
        // Entry still has to come from the mouth of the well. Pulling back out
        // or sideways releases this checkpoint and returns to the raw hand.
        if(reload.magazineGuideSeated&&result.depth>.065f)reload.magazineGuideSeated=false;
        if(reload.magazineGuided&&result.depth<=.025f&&result.depth>=-.10f&&result.side<.085f&&entryAngle)
            reload.magazineGuideSeated=true;
        const float rearLimit=reload.magazineGuideSeated?-.18f:-.055f;
        const bool outside=result.side>.12f||result.depth<rearLimit||result.depth>.23f||
            forward<-.2f||(!cylinder&&up<-.5f);
        if(outside)reload.magazineGuided=false;
        else if(!reload.magazineGuided&&grab&&entry)reload.magazineGuided=true;
        if(!reload.magazineGuided)reload.magazineGuideSeated=false;
        if(previous!=reload.magazineGuided)
            Log("MRELOAD85 magazine guide weapon=%s engaged=%d depth=%.3f side=%.3f",profile.name,reload.magazineGuided?1:0,result.depth,result.side);
        const float dt=static_cast<float>(std::min<ULONGLONG>(elapsed,50));
        reload.magazineGuideBlend=MagazineGuideClamp(reload.magazineGuideBlend+(reload.magazineGuided?dt/70.f:-dt/50.f));
        reload.magazineGuideTick=now;
    }
    const float rearLimit=reload.magazineGuideSeated?-.18f:-.055f;
    const bool outside=result.side>.12f||result.depth<rearLimit||result.depth>.23f||
        forward<-.2f||(!cylinder&&up<-.5f);
    result.guided=reload.magazineGuided&&!outside;
    result.seated=result.guided&&reload.magazineGuideSeated;
    // Fade at the escape boundary as well as over time. A fast sideways pull
    // must not leave the magazine glued to the gun while the hand moves away.
    const float sideFade=MagazineGuideClamp((.12f-result.side)/.04f);
    const float endFade=MagazineGuideClamp((.23f-result.depth)/.05f)*MagazineGuideClamp((result.depth-rearLimit)/.04f);
    const float amount=reload.magazineGuideBlend*sideFade*endFade;
    if(amount<=0.f)return result;
    Mat rail=socket;
    const Vec railCenter=Add(seat,Scale(axis,std::fmax(0.f,result.depth)));
    SetPosition(rail,Sub(railCenter,Direction(profile.magazineCenter,rail)));
    result.pose=BlendRigidPose(raw,rail,amount);
    // Rotate about the held magazine centre, never about a distant mesh origin.
    const Vec center=Add(rawCenter,Scale(Sub(railCenter,rawCenter),amount));
    SetPosition(result.pose,Sub(center,Direction(profile.magazineCenter,result.pose)));
    return result;
}

void ApplyReload(const Runtime::Frame& f,const HandTargets& target,const Mat native[70],const Mat basis[70],Mat solved[70],int& contactKind,Mat contactLocal[21],bool supporting){
    contactKind=0;
    AcquireSRWLockExclusive(&reloadLock);
    const auto now=GetTickCount64();
    if(!reload.supported){ReleaseSRWLockExclusive(&reloadLock);return;}
    if(!ReloadOwned(now)||!target.leftValid||!target.rightValid){if(ReloadActive())ReloadCancel("tracking/input unavailable",now);ReleaseSRWLockExclusive(&reloadLock);return;}
    const auto& profile=*reload.profile;const Vec magazineCenter=profile.magazineCenter;
    const bool bizon=std::strcmp(profile.name,"rurif_bizon")==0||profile.cylindricalMagazine;const bool pistol=ManualPistol();
    const bool grab=reloadInput.grab,pressed=grab&&!reload.lastGrab,released=!grab&&reload.lastGrab;
    const bool advance=reload.lastFrame!=f.id;
    const bool ammoFresh=reload.ammo.interfaceObject&&now>=reload.ammo.tick&&now-reload.ammo.tick<350;
    if(advance){reload.lastFrame=f.id;reload.lastGrab=grab;}
    if(!ReloadActive()&&now>=reload.completedPoseUntil&&ammoFresh&&reload.ammo.state<2&&binding.rightReady&&f.hands[1].trigger<.2f&&
       (!ManualAction()||!reload.restValid||(!reload.actionPending&&now>=reload.actionPoseUntil))){
        const Mat inv=Inverse(native[54]),mag=Mul(native[profile.magazineBone],inv);
        const Mat handle=profile.handleBone?Mul(native[profile.handleBone],inv):Identity();
        // Do not capture a halfway-ejected native magazine as its socket.
        if((profile.reloadAmount||Near(mag,profile.magazineInstalled,.025f,.025f))&&Near(handle,profile.handleInstalled,.025f,.025f)){
            for(int i=0;i<16;++i)reload.rest[i]=Mul(native[54+i],inv);
            // Bizon's decorative idle2 removes/tosses the cylinder. Even an
            // early blended key can pass the capture tolerance; keep its
            // verified installed transform as the socket, not that drift.
            if(bizon)reload.rest[profile.magazineBone-54]=profile.magazineInstalled;
            const Mat rigInverse=Inverse(native[1]);
            for(int i=2;i<54;++i)reload.handRest[i]=Mul(native[i],rigInverse);
            reload.handRestValid=true;
            if(!reload.restValid)Log("MRELOAD21 sockets captured weapon=%s magBone=%d handleBone=%d stroke=%.4f required=%.4f; original hand solve retained",profile.name,profile.magazineBone,profile.handleBone,profile.handleTravel,ManualAction()?ActionRearThreshold():profile.handleRequired);
            reload.restValid=true;
        }
    }
    if(!reload.restValid){ReleaseSRWLockExclusive(&reloadLock);return;}
    // Break-action shells stay aligned with the rotating breech. Only the
    // opt-in model uses this socket; established magazines keep their path.
    if(profile.magazineFollowsLatch&&advance&&ammoFresh&&!reloadInput.bypass&&now>=reload.bypassUntil){
        const bool open=reload.phase==ReloadPhase::Remove||reload.phase==ReloadPhase::Pouch||reload.phase==ReloadPhase::Insert||
            (profile.revolver&&(reload.phase==ReloadPhase::PistolEject||reload.phase==ReloadPhase::RevolverCloseReady));
        const float dt=reload.magazineLatchTick&&now>=reload.magazineLatchTick?std::fmin(50.f,float(now-reload.magazineLatchTick)):16.f;
        reload.magazineLatchTick=now;
        if(reload.phase!=ReloadPhase::RevolverClose)
            reload.magazineLatch+=std::fmax(-dt/120.f,std::fmin(dt/120.f,(open?1.f:0.f)-reload.magazineLatch));
    }
    Mat socketLocal=profile.reloadAmount?profile.shellSocket:reload.rest[profile.magazineBone-54];
    if(profile.magazineFollowsLatch){
        const Mat delta=ReloadBlendPose(Identity(),profile.magazineLatchOpen,reload.magazineLatch);
        socketLocal=Mul(Mul(Mul(socketLocal,Inverse(reload.rest[profile.magazineLatchBone-54])),delta),reload.rest[profile.magazineLatchBone-54]);
    }
    const Mat socket=Mul(socketLocal,solved[54]);
    const Vec socketCenter=Point(magazineCenter,socket),palm=Position(target.leftGrip);
    const float distance=Length(Sub(palm,socketCenter));
    const bool pullPistol=ManualPullPistolMagazine();
    // G18C lower exposed magazine, measured from its own mesh. Insertion
    // still uses the original magazine center/socket and guide rail.
    const float removeDistance=pullPistol?Length(Sub(palm,Point({-0.0009f,-0.153f,-0.103f},socket))):distance;
    const bool handleEnabled=profile.handleBone&&Config().chargingHandle!=0;
    const float grabRadius=ManualAction()?std::fmax(.15f,profile.handleGrabRadius):profile.handleGrabRadius;
    const Mat handleBase=profile.openBoltReady?profile.handleEmptyInstalled:
        (profile.handleBone?reload.rest[profile.handleBone-54]:Identity());
    const float handleDistance=handleEnabled?Length(Sub(palm,Point(profile.handleGrab,Mul(ManualAction()?ReloadActionLocal(reload.handlePull):handleBase,solved[54])))):100.f;
    MagazineGuidePose magazineGuide{};
    const bool guidingMagazine=reload.phase==ReloadPhase::Insert;
    if(guidingMagazine)
        magazineGuide=GuideReloadMagazine(Mul(reload.heldFromGrip,target.leftGrip),socket,solved[54],now,advance,grab);
    if(advance){
        if(reload.pumpAimValid&&(!target.supportPressed||!PumpSupportPhase()||!ammoFresh||reloadInput.bypass||now<reload.bypassUntil||
           (reload.pumpAimReturnTick&&now>=reload.pumpAimReturnTick+90))){
            reload.pumpAimValid=false;reload.pumpAimReturnTick=0;
        }
        const bool canRemove=!profile.reloadAmount&&reload.ammo.rounds<reload.ammo.capacity&&reload.ammo.magazines>1;
        // A full G18/no spare may be removed and reseated, like B eject,
        // without consuming a reserve or granting extra ammunition.
        const bool canPullMagazine=canRemove||pullPistol;
        const bool optionalWindow=(!ReloadActive()&&(reload.ammo.state<2||profile.reloadAmount)&&(reload.ammo.rounds>0||reload.actionPending))||
                                  (reload.phase==ReloadPhase::Finish&&!reload.needsCharge&&reload.nativeStarted);
        // MAC-11 is already cocked in its native idle pose. Its forward knob
        // is presented during a magazine reload, not as an invisible idle grab.
        const bool optionalHandle=optionalWindow&&ammoFresh&&handleEnabled&&!profile.openBoltReady&&now>=reload.bypassUntil&&(ManualPump()||f.hands[1].trigger<.2f);
        // Revolvers use B to open/eject, then load their own round bundle.
        // A full cylinder/no reserve does not consume ammo or invent a reload.
        const bool eject=reload.ejectPending&&(!profile.revolver||canRemove);reload.ejectPending=false;
        if(ManualMagazineButton()&&eject&&!ReloadActive()&&ammoFresh&&reload.ammo.state<2&&now>=reload.bypassUntil){
            reload.started=reload.ejectTick=now;reload.startRounds=reload.ammo.rounds;reload.startMagazines=reload.ammo.magazines;
            reload.handleOnly=false;reload.nativeStarted=false;reload.magazineSeated=false;reload.originalMagazineHeld=false;
            // With no spare/full magazine, allow removing/reseating the same
            // magazine without a native reload request or invented ammunition.
            reload.pistolReseat=!canRemove;reload.request=canRemove;
            reload.needsCharge=false;reload.handleCycled=false;reload.handlePull=reload.handlePeak=0;
            reload.ejectedMagazine=socket;reload.ejectDirection=Scale(Row(solved[54],profile.revolver?2:1),-1.f);
            if(profile.revolver)ReloadCue(ReloadAudio::HandlePull);
            ReloadCue(ReloadAudio::MagazineOut);ReloadStep(ReloadPhase::PistolEject);ReloadFeedback(.5f,55);
            Log("MRELOAD57 B eject weapon=%s rounds=%d reserves=%d reseat=%d",profile.name,reload.ammo.rounds,reload.ammo.magazines,reload.pistolReseat?1:0);
        }else if(reload.phase==ReloadPhase::PistolEject){
            if(now-reload.ejectTick>=220)ReloadStep(ReloadPhase::Pouch);
        }else if(profile.revolver&&(reload.phase==ReloadPhase::RevolverCloseReady||reload.phase==ReloadPhase::RevolverClose)){
            const Vec hand=Point(palm,Inverse(solved[54]));
            const Mat& closed=reload.rest[profile.magazineBone-54];
            const Mat open=Mul(Mul(Mul(closed,Inverse(reload.rest[profile.magazineLatchBone-54])),profile.magazineLatchOpen),reload.rest[profile.magazineLatchBone-54]);
            const Vec from=Point(magazineCenter,open),to=Point(magazineCenter,closed),path=Sub(to,from);
            const float stroke=Length(path);
            if(reload.phase==ReloadPhase::RevolverCloseReady){
                ReloadNear(Length(Sub(hand,from)),.12f,now);
                if(pressed&&Length(Sub(hand,from))<.14f){
                    reload.handleHandStart=hand;reload.magazineLatch=1.f;ReloadStep(ReloadPhase::RevolverClose);ReloadFeedback(.3f,45);
                }
            }else if(stroke>.01f){
                const Vec delta=Sub(hand,reload.handleHandStart);const Vec axis=Scale(path,1.f/stroke);
                const float along=Dot(delta,axis),side=Length(Sub(delta,Scale(axis,along)));
                if(side<.12f)reload.magazineLatch=1.f-std::fmax(0.f,std::fmin(1.f,along/stroke));
                if(side<.12f&&along>=stroke*.75f){
                    reload.magazineLatch=0;reload.finishTick=now;reload.needsCharge=false;
                    ReloadCue(ReloadAudio::HandleHome);ReloadStep(ReloadPhase::Finish);ReloadFeedback(.6f,70);
                }else if(released||side>=.12f){ReloadStep(ReloadPhase::RevolverCloseReady);}
            }
        }else if(UpdateLmgReload(target,solved,now,grab,pressed,released,ammoFresh,canRemove,distance,optionalHandle?handleDistance:100.f)){
            // LMG cover/belt owns this frame's trigger gesture.
        }else if(profile.pumpAction&&!profile.reloadAmount&&ManualPump()&&supporting&&target.supportPressed&&
           (reload.phase==ReloadPhase::Idle||reload.phase==ReloadPhase::HandleReady)&&
           (pressed||(grab&&reload.actionPending))&&ammoFresh&&!reloadInput.bypass&&now>=reload.bypassUntil){
            BeginReloadHandle(target,solved,now,true,true);
        }else if(profile.reloadAmount&&(reload.phase==ReloadPhase::Idle||reload.phase==ReloadPhase::ShellInsert||reload.phase==ReloadPhase::ShellCommit||
           (ManualPump()&&reload.phase==ReloadPhase::HandleReady))){
            const bool supply=ammoFresh&&reload.ammo.rounds<reload.ammo.capacity&&reload.ammo.magazines>1;
            if((reload.phase==ReloadPhase::Idle||reload.phase==ReloadPhase::HandleReady)&&now>=reload.bypassUntil){
                const Mat pouch=ReloadPouch(target);const float pouchDistance=Length(Sub(palm,Point(magazineCenter,pouch)));
                const bool pumpReady=ManualPump()&&ammoFresh&&reload.phase==ReloadPhase::HandleReady;
                // SolveHands has already accepted this physical support grip.
                // Its wrist anchor and the pump's authored knob are different
                // points: re-testing the controller origin against a 15cm knob
                // sphere rejected a hand visibly latched on the fore-end.
                // Transfer that existing contact to the trigger-operated pump.
                // A free hand still uses the original proximity/pouch checks.
                const bool supportGrab=ManualPump()&&supporting&&target.supportPressed&&!reloadInput.bypass;
                const bool atPump=(optionalHandle||pumpReady)&&(supportGrab||handleDistance<grabRadius);
                const bool atPouch=supply&&!supportGrab&&pouchDistance<.24f&&(!atPump||pouchDistance<handleDistance);
                if(pressed)Log("MRELOAD21 shell/pump grab weapon=%s pouch=%.3f handle=%.3f supply=%d phase=%s support=%d pumpAccepted=%d pending=%d nativeState=%d",profile.name,pouchDistance,handleDistance,supply?1:0,ReloadPhaseName(),supportGrab?1:0,atPump?1:0,reload.actionPending?1:0,reload.ammo.state);
                if(atPouch){
                    ReloadNear(pouchDistance,.20f,now);
                    if(pressed&&GrabFreshReloadPart(target,solved,pouch)){
                        reload.started=now;reload.nativeStarted=false;reload.handleOnly=false;
                        reload.shellClosest=100.f;reload.shellClosestOffset={};
                        ReloadStep(ReloadPhase::ShellInsert);ReloadFeedback(.4f,45);
                    }
                }else if(atPump){
                    ReloadNear(handleDistance,grabRadius*.85f,now);
                    // Holding trigger through the next shot can initiate its
                    // next required pump; a completed cycle cannot retrigger
                    // itself, and an abandoned partial stroke still needs grab.
                    if(pressed||(grab&&ManualPump()&&reload.actionPending&&reload.phase==ReloadPhase::Idle))
                        BeginReloadHandle(target,solved,now,true,supportGrab);
                }else if(supply)ReloadNear(pouchDistance,.20f,now);
            }else if(reload.phase==ReloadPhase::ShellInsert){
                const Mat held=Mul(reload.heldFromGrip,target.leftGrip);
                const Vec error=Sub(Point(magazineCenter,held),socketCenter);
                const float d=Length(error);
                const Vec gunError=Direction(error,Inverse(solved[54]));
                // Cylindrical shells can roll freely; their nose must face the
                // loading direction authored in the original insertion clip.
                const float forward=Dot(Row(held,2),Row(socket,2));
                // r12 Remington feedback: aligned insertions repeatedly ended
                // 96-121mm from the authored single-frame socket. Give this
                // port more positional assistance; keep the direction check,
                // release requirement and one-shell native permit unchanged.
                const bool remington=std::strcmp(profile.name,"usrif_remington11-87")==0;
                const float radius=remington?.14f:.085f,nearRadius=remington?.12f:.07f;
                if(forward>.5f&&d<reload.shellClosest){reload.shellClosest=d;reload.shellClosestOffset=gunError;}
                ReloadNear(supply&&forward>.5f?d:10.f,nearRadius,now);
                if(released){
                    const bool accepted=supply&&d<radius&&forward>.5f;
                    Log("MRELOAD21 shell insert weapon=%s distance=%.3f forward=%.3f accepted=%d radius=%.3f supply=%d errorMm=(%.1f %.1f %.1f) closest=%.3f closestErrorMm=(%.1f %.1f %.1f)",profile.name,d,forward,accepted?1:0,radius,supply?1:0,gunError.x*1000.f,gunError.y*1000.f,gunError.z*1000.f,reload.shellClosest,reload.shellClosestOffset.x*1000.f,reload.shellClosestOffset.y*1000.f,reload.shellClosestOffset.z*1000.f);
                    if(accepted){
                        reload.shellRounds=reload.ammo.rounds;reload.shellReserves=reload.ammo.magazines;
                        reload.shellPermit=true;reload.finishTick=now;reload.request=reload.ammo.state<2;
                        reload.shellRequestTick=reload.request?now:0;
                        ReloadCue(ReloadAudio::MagazineIn);ReloadStep(ReloadPhase::ShellCommit);ReloadFeedback(.45f,45);
                    }else ResumeShellAction();
                }
            }
        }else if(ManualAction()&&reload.phase==ReloadPhase::HandleReady&&canRemove&&!reload.magazineSeated&&reload.handleCycled&&pressed&&distance<.14f&&distance+.04f<handleDistance){
            // The bolt may be opened before changing a magazine. Keep it open
            // through the swap, then let the hand push it forward and lock it.
            if(!reload.nativeStarted){reload.started=now;reload.startRounds=reload.ammo.rounds;reload.startMagazines=reload.ammo.magazines;reload.request=false;reload.originalMagazineHeld=true;}
            reload.handleOnly=false;reload.needsCharge=true;
            reload.heldFromGrip=Mul(socket,Inverse(target.leftGrip));ReloadStep(ReloadPhase::Remove);ReloadFeedback(.4f,50);
        }else if(ManualAction()&&reload.phase==ReloadPhase::GrabInstalled&&pressed&&handleDistance<grabRadius&&handleDistance<distance+.04f){
            BeginReloadHandle(target,solved,now,false);
        }else if(optionalHandle&&pressed&&handleDistance<grabRadius&&
           (reload.phase==ReloadPhase::Finish||!canPullMagazine||removeDistance>=.14f||handleDistance<removeDistance+(ManualAction()?.04f:0.f))){
            BeginReloadHandle(target,solved,now,true);
        }else if(!ReloadActive()&&ammoFresh&&reload.ammo.state<2&&now>=reload.bypassUntil){
            if(optionalHandle&&(!canPullMagazine||handleDistance<removeDistance))ReloadNear(handleDistance,profile.handleGrabRadius*.85f,now);
            else if(canPullMagazine)ReloadNear(removeDistance,.12f,now);
            if(pressed){
                Log("MRELOAD21 grab attempt socketDistance=%.3f ammo=%d/%d",distance,reload.ammo.rounds,reload.ammo.capacity);
                if((!pistol||pullPistol)&&canPullMagazine&&removeDistance<.14f){
                    reload.started=now;reload.startRounds=reload.ammo.rounds;reload.startMagazines=reload.ammo.magazines;
                    reload.handleOnly=false;ResetLmgReload();
                    // Keep native ammunition untouched while the original
                    // magazine can still be returned. Commit only on discard.
                    reload.heldFromGrip=Mul(socket,Inverse(target.leftGrip));reload.request=false;reload.nativeStarted=false;
                    reload.originalMagazineHeld=true;reload.magazineSeated=false;reload.pistolReseat=pullPistol&&!canRemove;
                    if(pullPistol){
                        reload.pistolReseat=!canRemove;reload.magazineSeated=false;
                        reload.needsCharge=reload.handleCycled=false;reload.handlePull=reload.handlePeak=0;
                        Log("MRELOAD115 G18 physical extraction rounds=%d reserves=%d reseat=%d",reload.ammo.rounds,reload.ammo.magazines,reload.pistolReseat?1:0);
                    }
                    ReloadStep(ReloadPhase::Remove);ReloadFeedback(.4f,50);
                }
            }
        }else if(reload.phase==ReloadPhase::GrabInstalled){
            ReloadNear(removeDistance,.12f,now);
            if(pressed&&removeDistance<.14f){reload.heldFromGrip=Mul(socket,Inverse(target.leftGrip));ReloadStep(ReloadPhase::Remove);ReloadFeedback(.4f,50);}
        }else if(reload.phase==ReloadPhase::Remove){
            const Mat held=Mul(reload.heldFromGrip,target.leftGrip);
            const float travel=Length(Sub(Point(magazineCenter,held),socketCenter));
            if(travel>.025f&&!reload.magOutSound){ReloadCue(ReloadAudio::MagazineOut);reload.magOutSound=true;}
            if(released){
                if(travel>=.10f){
                    if(reload.originalMagazineHeld){
                        reload.originalMagazineHeld=false;reload.request=canRemove&&!reload.nativeStarted;
                        if(reload.request)reload.started=now;
                    }
                    ReloadStep(ReloadPhase::Pouch);ReloadFeedback(.45f,55);
                }else{
                    if(reload.magOutSound)ReloadCue(ReloadAudio::MagazineIn);
                    if(reload.originalMagazineHeld&&!reload.nativeStarted){
                        // This is the SAME magazine, not a supply-pouch refill.
                        // Restore handling without touching native counts,
                        // timers, reserve magazines or completion notifications.
                        reload.request=false;reload.originalMagazineHeld=false;reload.pistolReseat=false;reload.magazineSeated=true;
                        const bool openAction=ManualAction()&&(reload.actionPending||reload.handlePull>.001f||reload.handleCycled);
                        reload.needsCharge=openAction;reload.handleOnly=openAction;
                        if(reload.lmg&&reload.lmg->coverBone&&reload.coverOpen>.001f){
                            reload.beltSeated=true;reload.beltProgress=reload.beltPeak=1.f;
                            ReloadStep(ReloadPhase::CoverCloseReady);
                        }else ReloadStep(openAction?ReloadPhase::HandleReady:ReloadPhase::Idle);
                        ReloadFeedback(.45f,55);
                        Log("MRELOAD120 original magazine returned weapon=%s rounds=%d reserves=%d; no native refill",profile.name,reload.ammo.rounds,reload.ammo.magazines);
                    }else ReloadStep(ReloadPhase::GrabInstalled);
                }
            }
        }else if(reload.phase==ReloadPhase::Pouch){
            const Mat pouch=ReloadPouch(target);const float d=Length(Sub(palm,Point(magazineCenter,pouch)));
            ReloadNear(d,.16f,now);
            if(pressed){Log("MRELOAD21 pouch attempt distance=%.3f",d);if(d<.19f&&GrabFreshReloadPart(target,solved,pouch)){ReloadStep(ReloadPhase::Insert);ReloadFeedback(.5f,60);}}
        }else if(reload.phase==ReloadPhase::Insert){
            const Mat raw=Mul(reload.heldFromGrip,target.leftGrip);
            const float d=Length(Sub(Point(magazineCenter,raw),socketCenter));
            const float upright=Dot(Row(raw,1),Row(socket,1)),forward=Dot(Row(raw,2),Row(socket,2));
            const bool originalAlignment=upright>.6f&&forward>.4f;
            const bool rollAssist=bizon&&!originalAlignment&&forward>.85f;
            const bool aligned=originalAlignment||rollAssist;
            // Assistance aligns the visible part but never supplies insertion
            // depth or ammunition. Keep depth/lateral checks on the raw hand
            // pose. A quick already-aligned insertion can seat without waiting
            // for the visual blend; early release along the rail cannot.
            const bool guideSeated=magazineGuide.guided&&magazineGuide.side<.09f&&
                ((magazineGuide.seated&&magazineGuide.depth>=-.14f&&magazineGuide.depth<.045f)||
                 (magazineGuide.depth>=-.035f&&magazineGuide.depth<.045f));
            const bool atSeat=magazineGuide.enabled?(guideSeated||(d<.055f&&aligned)):(d<.11f&&aligned);
            ReloadNear(atSeat?0.f:10.f,.095f,now);
            if(released){
                const bool accepted=atSeat&&(reload.nativeStarted||reload.pistolReseat);
                Log("MRELOAD86 insert weapon=%s distance=%.3f up=%.3f forward=%.3f guide=%d seated=%d depth=%.3f side=%.3f nativeStarted=%d reseat=%d accepted=%d",profile.name,d,upright,forward,magazineGuide.guided?1:0,magazineGuide.seated?1:0,magazineGuide.depth,magazineGuide.side,reload.nativeStarted?1:0,reload.pistolReseat?1:0,accepted?1:0);
                if(accepted){
                    reload.magazineSeated=true;
                    ReloadCue(ReloadAudio::MagazineIn);
                    if(reload.pistolReseat){reload.pistolReseat=false;ReloadStep(ReloadPhase::Idle);}
                    else if(profile.revolver){reload.magazineLatch=1.f;ReloadStep(ReloadPhase::RevolverCloseReady);}
                    else if(reload.lmg){reload.beltProgress=reload.beltPeak=0;reload.beltSeated=false;ReloadStep(ReloadPhase::BeltReady);}
                    else if(reload.needsCharge){if(!ManualAction()){reload.handlePull=reload.handlePeak=0;reload.handleCycled=false;}ReloadStep(ReloadPhase::HandleReady);}
                    else{reload.finishTick=now;ReloadStep(ReloadPhase::Finish);}
                    ReloadFeedback(.65f,90);
                }
                else{ReloadStep(ReloadPhase::Pouch);ReloadFeedback(.15f,35);}
            }
        }else if(reload.phase==ReloadPhase::HandleReady){
            const Mat handle=Mul(ManualAction()?ReloadActionLocal(reload.handlePull):reload.rest[profile.handleBone-54],solved[54]);
            const float d=Length(Sub(palm,Point(profile.handleGrab,handle)));
            ReloadNear(d,grabRadius*.85f,now);
            if(pressed){
                Log("MRELOAD21 handle grab distance=%.3f radius=%.3f",d,grabRadius);
                if(d<grabRadius){
                    BeginReloadHandle(target,solved,now,ManualAction()?reload.handleOnly:false);
                }
            }
        }else if(reload.phase==ReloadPhase::HandlePull){
            const Vec controlPoint=ManualAction()?Point(reload.actionGripPoint,target.leftGrip):palm;
            const Vec delta=Sub(Point(controlPoint,Inverse(solved[54])),reload.handleHandStart);
            float along=0,side=0;ReloadHandleProgress(delta,along,side);
            const bool leftRail=side>profile.handleSideLimit||along>profile.handleTravel+.20f||along<-.20f;
            if(ManualAction()){
                const float sideLimit=ManualPump()?.20f:.14f;
                const bool detached=side>sideLimit||along>profile.handleTravel+.35f||along<-.25f;
                const float rear=ActionRearThreshold(),home=ActionHomeThreshold();
                if(!detached){
                    reload.handlePull=std::fmax(0.f,std::fmin(profile.handleTravel,along));
                    reload.handlePeak=std::fmax(reload.handlePeak,reload.handlePull);
                    if(reload.handlePull>.012f&&!reload.handlePullSound){ReloadCue(ReloadAudio::HandlePull);reload.handlePullSound=true;}
                    if(!reload.handleCycled&&reload.handlePeak>=rear){
                        reload.handleCycled=true;ReloadFeedback(.65f,65);
                        Log("MRELOAD21 action rear stop weapon=%s along=%.3f side=%.3f required=%.3f; return forward",profile.name,along,side,rear);
                    }
                }
                // Latch completion while the hand is still in the home zone.
                // Moving the hand away to release must not erase that result.
                const bool complete=!detached&&reload.handleCycled&&reload.handlePull<=home&&side<=(ManualPump()?.17f:.12f);
                if(complete){
                    Log("MRELOAD21 action close assist weapon=%s along=%.3f side=%.3f home=%.3f peak=%.3f released=%d",profile.name,along,side,home,reload.handlePeak,released?1:0);
                    ReturnReloadHandle(true,now);
                }else if(released||detached){
                    reload.handleGripValid=false;
                    ReloadStep(ReloadPhase::HandleReady);
                    Log("MRELOAD21 action parked weapon=%s distance=%.3f along=%.3f side=%.3f rear=%d detached=%d released=%d",profile.name,reload.handlePull,along,side,reload.handleCycled?1:0,detached?1:0,released?1:0);
                }
            }else if(released&&reload.handlePeak>=profile.handleRequired){
                // A full stroke was observed while held on the rail. The
                // releasing hand may already be moving away in this sample.
                Log("MRELOAD21 handle release peak=%.3f required=%.3f accepted=1",reload.handlePeak,profile.handleRequired);
                ReturnReloadHandle(true,now);
            }else if(leftRail){
                // A full stroke already sampled ON the rail remains complete
                // when the hand peels away before releasing the trigger.
                // Partial/off-rail-only pulls still fail. Pump/bolt actions
                // retain their separate rear-then-forward completion rules.
                const bool cycled=reload.handlePeak>=profile.handleRequired;
                Log("MRELOAD31 spring handle detached weapon=%s along=%.3f side=%.3f peak=%.3f required=%.3f accepted=%d",profile.name,along,side,reload.handlePeak,profile.handleRequired,cycled?1:0);
                ReturnReloadHandle(cycled,now);
            }else{
                reload.handlePull=std::fmax(0.f,std::fmin(profile.handleTravel,along));
                if(reload.handlePull>std::fmin(.015f,profile.handleTravel*.2f)&&!reload.handlePullSound){ReloadCue(ReloadAudio::HandlePull);reload.handlePullSound=true;}
                const bool wasFull=reload.handlePeak>=profile.handleRequired;
                reload.handlePeak=std::fmax(reload.handlePeak,reload.handlePull);
                if(!wasFull&&reload.handlePeak>=profile.handleRequired){
                    Log("MRELOAD21 handle full stroke pull=%.3f required=%.3f side=%.3f; release trigger",reload.handlePull,profile.handleRequired,side);
                    ReloadFeedback(.65f,70);
                }
                if(released){
                    const bool cycled=reload.handlePeak>=profile.handleRequired;
                    Log("MRELOAD21 handle release peak=%.3f required=%.3f accepted=%d",reload.handlePeak,profile.handleRequired,cycled?1:0);
                    ReturnReloadHandle(cycled,now);
                }
            }
        }else if(reload.phase==ReloadPhase::HandleReturn){
            // The handle stays on its rail while returning. Native ammo and
            // firing stay held until the accepted stroke has returned home.
            const float t=std::fmin(1.f,(float)(now-reload.handleReturnTick)/120.f);
            reload.handlePull=reload.handleReturnFrom*(1.f-t)*(1.f-t);
            if(t>=1.f){
                reload.handlePull=0;
                if(ManualAction())CompleteManualAction(now);
                else if(reload.handleOnly){
                    if(reload.handleReturnFrom>.005f)ReloadCue(ReloadAudio::HandleHome);
                    Log("MRELOAD21 optional handle home cycled=%d rounds=%d magazines=%d nativePending=%d; no reload request",reload.handleCycled?1:0,reload.ammo.rounds,reload.ammo.magazines,reload.nativeStarted?1:0);
                    reload.handleOnly=false;reload.handleGripValid=false;
                    if(reload.nativeStarted){reload.finishTick=now;ReloadStep(ReloadPhase::Finish);}
                    else{ReloadStep(ReloadPhase::Idle);reload.handlePeak=0;reload.handleCycled=false;}
                }else if(reload.handleCycled){
                    if(reload.handleReturnFrom>.005f)ReloadCue(ReloadAudio::HandleHome);
                    Log("MRELOAD21 handle home; native reload released after physical cycle");
                    reload.finishTick=now;ReloadStep(ReloadPhase::Finish);ReloadFeedback(.55f,65);
                }else{if(reload.handleReturnFrom>.005f)ReloadCue(ReloadAudio::HandleHome);reload.handlePeak=0;ReloadStep(ReloadPhase::HandleReady);}
            }
        }
        if(!profile.reloadAmount&&ReloadActive()&&!reload.originalMagazineHeld&&!reload.handleOnly&&!reload.pistolReseat&&!reload.nativeStarted&&
           reload.phase!=ReloadPhase::CoverCloseReady&&reload.phase!=ReloadPhase::CoverPull&&now-reload.started>2500)ReloadCancel("native reload did not start",now);
        if(reload.phase==ReloadPhase::Finish&&now-reload.finishTick>7000)ReloadCancel("native completion did not arrive",now);
    }
    // Manual mode owns the seated Bizon magazine during idle too. Native
    // idle2 otherwise animates it independently of the tracked left hand.
    // Restrict this to the validated local profile and fresh non-reload ammo;
    // B-button fallback and ordinary native reloads must retain their poses.
    const bool holdIdleMagazine=bizon&&!ReloadActive()&&ammoFresh&&reload.ammo.state<2&&
                                !reloadInput.bypass&&now>=reload.bypassUntil;
    if(holdIdleMagazine){
        const Mat nativeMag=Mul(native[profile.magazineBone],Inverse(native[54]));
        const bool displaced=!Near(nativeMag,profile.magazineInstalled,.01f,.01f);
        if(displaced&&!reload.idleMagazineSuppressed)
            Log("MRELOAD21 Bizon idle magazine animation suppressed distance=%.3f",Length(Sub(Point(magazineCenter,nativeMag),Point(magazineCenter,profile.magazineInstalled))));
        reload.idleMagazineSuppressed=displaced;
        solved[profile.magazineBone]=socket;
    }else reload.idleMagazineSuppressed=false;
    if(!ReloadActive()&&now<reload.completedPoseUntil&&ammoFresh&&!reloadInput.bypass&&now>=reload.bypassUntil){
        // Preserve the completed installation while the native reload clip
        // drains. Controller motion and native firing continue immediately.
        // Leave other parts (trigger, firing bolt, etc.) to their normal pose.
        solved[profile.magazineBone]=socket;
        if(profile.handleBone)solved[profile.handleBone]=Mul(reload.rest[profile.handleBone-54],solved[54]);
        if(reload.lmg){
            if(reload.lmg->coverBone)solved[reload.lmg->coverBone]=Mul(reload.rest[reload.lmg->coverBone-54],solved[54]);
            for(unsigned i=0;i<reload.lmg->coverFollowerCount;++i){const int bone=reload.lmg->coverFollowerBones[i];solved[bone]=Mul(reload.rest[bone-54],solved[54]);}
            for(int i=0;i<reload.lmg->beltCount;++i){const int bone=LmgBeltBone(*reload.lmg,i);solved[bone]=Mul(reload.rest[bone-54],solved[54]);}
        }
    }
    if(ReloadActive()){
        // Transport the cached rigid weapon parts from the CURRENT tracked
        // gun, then apply the magazine pose and constrained handle travel.
        // Raw target.leftGrip continues driving all interaction measurements.
        for(int i=1;i<16;++i)solved[54+i]=Mul(reload.rest[i],solved[54]);
        if(profile.magazineFollowsLatch)solved[profile.magazineBone]=socket;
        if(profile.openBoltReady&&reload.needsCharge&&!reload.handleCycled)
            solved[profile.handleBone]=Mul(profile.handleEmptyInstalled,solved[54]);
        if(reload.phase==ReloadPhase::Insert)
            solved[profile.magazineBone]=guidingMagazine?magazineGuide.pose:Mul(reload.heldFromGrip,target.leftGrip);
        else if(reload.phase==ReloadPhase::Remove||reload.phase==ReloadPhase::ShellInsert)solved[profile.magazineBone]=Mul(reload.heldFromGrip,target.leftGrip);
        else if(reload.phase==ReloadPhase::Pouch)solved[profile.magazineBone]=ReloadPouch(target);
        else if(reload.phase==ReloadPhase::PistolEject){
            Mat falling=reload.ejectedMagazine;const float t=std::min(.22f,float(now-reload.ejectTick)*.001f);
            SetPosition(falling,Add(Position(falling),Add(Scale(reload.ejectDirection,t*.8f),Vec{0,-4.9f*t*t,0})));
            solved[profile.magazineBone]=falling;
        }
        if(ManualAction()){
            solved[profile.handleBone]=Mul(ReloadActionLocal(reload.handlePull),solved[54]);
            if(profile.followerBone)solved[profile.followerBone]=Mul(Mul(ReloadHandlePose(reload.handlePull,true),reload.rest[profile.followerBone-54]),solved[54]);
        }else if(reload.phase==ReloadPhase::HandlePull||reload.phase==ReloadPhase::HandleReturn){
            if(profile.handleKeyCount){
                const Mat local=Mul(Mul(ReloadHandlePose(reload.handlePull,false),Inverse(profile.handleInstalled)),reload.rest[profile.handleBone-54]);
                solved[profile.handleBone]=Mul(local,solved[54]);
                if(profile.followerBone)solved[profile.followerBone]=Mul(Mul(ReloadHandlePose(reload.handlePull,true),reload.rest[profile.followerBone-54]),solved[54]);
            }else{
                Mat& handle=solved[profile.handleBone];
                SetPosition(handle,Add(Position(handle),Scale(Direction(Unit(profile.handleAxis),solved[54]),reload.handlePull)));
            }
        }
        // Open-bolt MAC-11 stays latched rearward after an accepted charge.
        // The existing completion handshake still owns ammo/fire permission.
        if(profile.openBoltReady&&reload.handleCycled)
            solved[profile.handleBone]=Mul(reload.rest[profile.handleBone-54],solved[54]);
    }
    if(ammoFresh&&!reloadInput.bypass&&now>=reload.bypassUntil){
        if(ManualAction()&&!ReloadActive()){
            solved[profile.handleBone]=Mul(ReloadActionLocal(reload.handlePull),solved[54]);
            if(profile.followerBone)solved[profile.followerBone]=Mul(Mul(ReloadHandlePose(reload.handlePull,true),reload.rest[profile.followerBone-54]),solved[54]);
        }
        if(profile.reloadAmount&&reload.phase!=ReloadPhase::ShellInsert){
            if(reload.ammo.rounds<reload.ammo.capacity&&reload.ammo.magazines>1&&reload.phase!=ReloadPhase::ShellCommit)
                solved[profile.magazineBone]=ReloadPouch(target);
            else{
                // Park the single loose cartridge inside the torso, away from
                // the receiver once loaded. Keep a rigid transform for skinning.
                Mat hidden=target.torso;SetPosition(hidden,Position(target.head));
                const Vec center=Point({0,-.45f,-.25f},hidden);
                SetPosition(hidden,Sub(center,Direction(magazineCenter,hidden)));
                solved[profile.magazineBone]=hidden;
            }
        }
    }
    if(profile.magazineFollowerParts&&(ReloadActive()||now<reload.completedPoseUntil)&&
       ammoFresh&&!reloadInput.bypass&&now>=reload.bypassUntil){
        // These measured parts are rounds inside the magazine. Keep their
        // installed offsets when the player removes, carries or inserts it.
        const Mat fromMagazine=Inverse(reload.rest[profile.magazineBone-54]);
        for(int i=1;i<profile.nativeMeshParts;++i)if(profile.magazineFollowerParts&(1u<<i))
            solved[54+i]=Mul(Mul(reload.rest[i],fromMagazine),solved[profile.magazineBone]);
    }
    if(profile.hiddenReloadParts&&(ReloadActive()||now<reload.completedPoseUntil||reload.ammo.state<2)&&
       ammoFresh&&!reloadInput.bypass&&now>=reload.bypassUntil){
        // Some native clips animate two separate old/new magazine meshes.
        // Manual reload owns one real magazine part. Collapse only the extra
        // visual parts inside the receiver during the override AND idle/fire.
        // Otherwise the spare mesh reappears beside the gun as soon as the
        // completion timer expires. Native fallback reloads keep their parts.
        // This is applied last to rendering output; native input/binding poses
        // and every collision/ammo object keep their original rigid matrices.
        for(int i=1;i<profile.nativeMeshParts;++i)if(profile.hiddenReloadParts&(1u<<i)){
            Mat hidden=solved[54];
            for(int row=0;row<3;++row)for(int col=0;col<3;++col)hidden.m[row][col]*=.0001f;
            solved[54+i]=hidden;
        }
    }
    ApplyLmgParts(solved);
    // Keep the detailed hand and connected sleeve on the moving fore-end
    // while grip remains held, including the handoff back from pump to aim.
    const bool pumpSupport=PumpSupportPhase()&&supporting&&ammoFresh&&!reloadInput.bypass&&now>=reload.bypassUntil;
    reload.pumpContact=false;
    const bool magazine=reload.phase==ReloadPhase::Remove||reload.phase==ReloadPhase::Insert||reload.phase==ReloadPhase::ShellInsert;
    const bool handle=reload.phase==ReloadPhase::HandlePull||(ManualAction()&&reload.phase==ReloadPhase::HandleReturn)||pumpSupport;
    if((grab||pumpSupport)&&(magazine||handle)){
        const auto& contact=reloadContacts[profile.soundIndex];
        const Mat* local=magazine?contact.magazine:(reload.handleGripValid?reload.handleGrip:contact.handle);
        const Mat& part=solved[magazine?profile.magazineBone:profile.handleBone];
        Mat posed[21];for(int i=0;i<21;++i)posed[i]=Mul(local[i],part);
        const float correction=Length(Sub(Position(posed[0]),Position(solved[7])));
        if(SolveLeftContact(basis,target,posed,solved)){
            contactKind=magazine?1:2;
            const Mat inv=Inverse(magazine?target.leftGrip:solved[54]);
            for(int i=0;i<21;++i)contactLocal[i]=Mul(solved[7+i],inv);
        }
        reload.pumpContact=pumpSupport&&contactKind==2;
        const int status=contactKind?contactKind:-1;
        if(reload.visualKind!=status)Log("MRELOAD21 visual latch weapon=%s part=%s active=%d correction=%.3f support=%d",profile.name,magazine?"magazine":"handle",contactKind?1:0,correction,pumpSupport?1:0);
        reload.visualKind=status;
    }else reload.visualKind=0;
    ApplyLmgContact(basis,target,solved,grab,contactKind,contactLocal);
    if(profile.magazineLatchBone&&ammoFresh&&!reloadInput.bypass&&now>=reload.bypassUntil){
        // Jackhammer's fore-end unlocks the drum before it is removed and
        // closes after seating. This is a visual latch coupled to the mag,
        // not a charging stroke or another ammo/refill request.
        const bool open=reload.phase==ReloadPhase::Remove||reload.phase==ReloadPhase::Pouch||reload.phase==ReloadPhase::Insert;
        if(advance&&!profile.magazineFollowsLatch){
            const float dt=reload.magazineLatchTick&&now>=reload.magazineLatchTick?
                std::fmin(50.f,static_cast<float>(now-reload.magazineLatchTick)):16.f;
            reload.magazineLatchTick=now;
            const float goal=open?1.f:0.f,step=dt/120.f;
            reload.magazineLatch+=std::fmax(-step,std::fmin(step,goal-reload.magazineLatch));
        }
        if(ReloadActive()||reload.magazineLatch>.001f){
            const Mat delta=ReloadBlendPose(Identity(),profile.magazineLatchOpen,reload.magazineLatch);
            solved[profile.magazineLatchBone]=Mul(Mul(delta,reload.rest[profile.magazineLatchBone-54]),solved[54]);
        }
    }
    if(profile.revolver&&ammoFresh&&!reloadInput.bypass&&now>=reload.bypassUntil){
        if(ReloadActive()||reload.magazineLatch>.001f||now<reload.completedPoseUntil){
            const Mat delta=ReloadBlendPose(Identity(),profile.magazineLatchOpen,reload.magazineLatch);
            const Mat fromLatch=Inverse(reload.rest[profile.magazineLatchBone-54]);
            for(int i=1;i<profile.nativeMeshParts;++i)if(profile.cylinderFollowerParts&(1u<<i))
                solved[54+i]=Mul(Mul(Mul(Mul(reload.rest[i],fromLatch),delta),reload.rest[profile.magazineLatchBone-54]),solved[54]);
        }
        if(profile.loaderBone){
            if(reload.phase==ReloadPhase::Pouch||reload.phase==ReloadPhase::Insert)
                solved[profile.loaderBone]=Mul(profile.loaderFromMagazine,solved[profile.magazineBone]);
            else{Mat hidden=solved[54];for(int row=0;row<3;++row)for(int col=0;col<3;++col)hidden.m[row][col]*=.0001f;solved[profile.loaderBone]=hidden;}
        }
    }
    ReleaseSRWLockExclusive(&reloadLock);
}
bool InstallReload(){
    // Install validated hooks regardless of the initial setting. Live manual
    // mode is gated by reloadInput.enabled; Off always runs native ammo/audio.
    if(!Config().hands||!originalFinalize)return false;
    const BYTE update[]={0x56,0x8B,0xF1,0x83,0x7E,0x3C,0x02};
    const BYTE canFire[]={0x56,0x8B,0xF1,0x8B,0x06};
    const BYTE refill[]={0x56,0x8B,0xF1,0x8B,0x46,0x5C};
    const BYTE automatic[]={0x56,0x8B,0xF1,0x83,0x7E,0x3C,0x00};
    const BYTE request[]={0x53,0x56,0x8B,0xF1,0x8A,0x5E,0x43};
    const BYTE finish[]={0xC7,0x46,0x3C,0,0,0,0};
    if(Value<void*>(game,0x4F5420+36*4)!=game+0x222040||!Match(game+0x222040,automatic,sizeof(automatic))||
       Value<void*>(game,0x4F5420+6*4)!=game+0x222690||Value<void*>(game,0x4F5420+13*4)!=game+0x222010||
       Value<void*>(game,0x4F5500+15*4)!=game+0x222F80||!Match(game+0x222690,update,sizeof(update))||
       Value<void*>(game,0x4F5420+44*4)!=game+0x2222E0||!Match(game+0x2222E0,request,sizeof(request))||
       !Match(game+0x222010,canFire,sizeof(canFire))||!Match(game+0x222F80,refill,sizeof(refill))||!Match(game+0x2226DB,finish,sizeof(finish)))return false;
    if(!Jump(game+0x222010,canFire,sizeof(canFire),reinterpret_cast<void*>(&ReloadAmmoCanFire),reinterpret_cast<void**>(&originalAmmoCanFire)))return false;
    if(!Jump(game+0x222690,update,sizeof(update),reinterpret_cast<void*>(&ReloadAmmoUpdate),reinterpret_cast<void**>(&originalAmmoUpdate)))return false;
    if(!Jump(game+0x222F80,refill,sizeof(refill),reinterpret_cast<void*>(&ReloadAmmoRefill),reinterpret_cast<void**>(&originalAmmoRefill)))return false;
    if(!Jump(game+0x2222E0,request,sizeof(request),reinterpret_cast<void*>(&ReloadAmmoReloadRequest),reinterpret_cast<void**>(&originalAmmoReloadRequest)))return false;
    if(!Jump(game+0x222040,automatic,sizeof(automatic),reinterpret_cast<void*>(&ReloadAmmoAutomatic),reinterpret_cast<void**>(&originalAmmoAutoReload)))return false;
    Log("MRELOAD57 native empty-auto query hooked; inventory ownership independent of hand tracking");
    // Validate the exact native simulation clock, deadline comparisons and
    // completion notification getter before enabling instance timer writes.
    const BYTE refillDue[]={0x39,0x46,0x1C,0x77,0x0D,0x8B,0x46,0xF0,0x8D,0x4E,0xF0,0xC6,0x46,0x14,0,0xFF,0x50,0x3C};
    const BYTE finishDue[]={0x39,0x46,0x20,0x77,0x22,0x83,0x7E,0x34,0,0xC6,0x46,0x42,1};
    const BYTE notification[]={0x8A,0x41,0x42,0xC6,0x41,0x42,0,0xC3};
    reloadCompletionProfile=Value<BYTE>(game,0xC4420)==0xA1&&Value<void*>(game,0xC4421)==game+0x5A7428&&
        Value<BYTE>(game,0xC4425)==0xC3&&Match(game+0x2226A4,refillDue,sizeof(refillDue))&&
        Match(game+0x2226BB,finishDue,sizeof(finishDue))&&Match(game+0x205250,notification,sizeof(notification))&&
        Value<void*>(game,0x4F5420+43*4)==game+0x205250;
    Log("MRELOAD32 physical completion deadlines=%d; native refill and weapon notifications retained",reloadCompletionProfile?1:0);
    Log("MRELOAD108 magazine grasp corrections; charging required after partial magazine swaps when enabled; duplicate reload parts hidden during idle/fire");
    return true;
}
