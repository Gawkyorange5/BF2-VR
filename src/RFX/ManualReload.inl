// Local-player magazine/charging prototype. Included inside Bf2Adapter's private
// namespace, after the owned-hand state. BF2 1.50 profile, OpenXR only.
// Native ammo owns all counts and completion notifications. Accepted manual
// steps expire only this local component's refill/finish deadlines.
#include "ReloadProfiles.inl"
#include "ReloadContacts.inl"
#include "ReloadGripRefinements.inl"
static_assert(sizeof(reloadContacts)/sizeof(reloadContacts[0])==sizeof(reloadWeapons)/sizeof(reloadWeapons[0]),"Every rifle needs visual contact poses");
static_assert(sizeof(reloadHandleVisualOffsets)/sizeof(reloadHandleVisualOffsets[0])==sizeof(reloadWeapons)/sizeof(reloadWeapons[0]),"Every rifle needs a visual offset entry");
static_assert(ReloadAudio::WeaponCount==sizeof(reloadWeapons)/sizeof(reloadWeapons[0]),"Audio and interaction profile counts must match");
enum class ReloadPhase { Idle, GrabInstalled, Remove, Pouch, Insert, HandleReady, HandlePull, HandleReturn, Finish, ShellInsert, ShellCommit };
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
    bool supported=false,restValid=false,lastGrab=false,request=false,nativeStarted=false,inRange=false;
    ReloadPhase phase=ReloadPhase::Idle;ReloadAmmo ammo{};
    bool buttonReload=false,buttonReloadSeen=false;ULONGLONG buttonReloadTick=0,buttonIdleTick=0;
    Mat rest[16]{},heldFromGrip{},handRest[54]{};
    bool handRestValid=false,idleMagazineSuppressed=false;int visualKind=0;
    float magazineLatch=0;ULONGLONG magazineLatchTick=0;
    bool shellPermit=false,actionPending=false,magazineSeated=false;int shellRounds=0,shellReserves=0;
    float shellClosest=100.f;Vec shellClosestOffset{};
    ULONGLONG actionPoseUntil=0,shellRequestTick=0,shellChainReportTick=0;
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
bool reloadProfile=false,reloadSoundProfile=false,reloadCompletionProfile=false;
bool ManualAction(){return reload.profile&&reload.profile->manualReturn&&Config().chargingHandle!=0;}
bool ManualPump(){return ManualAction()&&reload.profile->reloadAmount==1;}
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
struct ReloadActionMemory {void* owner=nullptr;void* weapon=nullptr;void* definition=nullptr;int rounds=0;bool pending=false;};
ReloadActionMemory reloadActionMemory[8]{};unsigned reloadActionCursor=0;
void RememberReloadAction(){
    if(!ManualAction()||!reload.ammo.interfaceObject)return;
    ReloadActionMemory* entry=nullptr;
    for(auto& m:reloadActionMemory)if(m.owner==reload.owner&&m.weapon==reload.weapon){entry=&m;break;}
    if(!entry)entry=&reloadActionMemory[reloadActionCursor++%8];
    *entry={reload.owner,reload.weapon,Value<void*>(reload.weapon,0x14),reload.ammo.rounds,reload.actionPending};
}
void ReloadCue(ReloadAudio::Cue cue){
    const bool rackCue=reload.handleOnly&&(cue==ReloadAudio::HandlePull||cue==ReloadAudio::HandleHome);
    const bool direct=reload.profile&&(reload.profile->reloadAmount||ManualAction());
    if(reloadSoundProfile&&(reload.soundSuppressed||rackCue||direct)&&reload.profile&&ReloadAudio::Ready()){
        ReloadAudio::Play(reload.profile->soundIndex,cue);
        Log("MRELOAD21 cue weapon=%s event=%u",reload.profile->name,static_cast<unsigned>(cue));
    }
}

bool ReloadActive(){return reload.phase!=ReloadPhase::Idle;}
// Finish now lasts only until the next native update commits the reload.
// Keep trigger input gated until then: sending its press to a still-reloading
// semi-auto weapon can consume that press before native completion is seen.
bool ReloadBlocksFire(){return ReloadActive()||reload.actionPending;}
bool ReloadHolding(){return ReloadActive()&&!reload.handleOnly&&reload.phase!=ReloadPhase::Finish;}
const char* ReloadPhaseName(){
    switch(reload.phase){case ReloadPhase::GrabInstalled:return "grab installed magazine";
    case ReloadPhase::Remove:return "pull magazine out and release";case ReloadPhase::Pouch:return "grab fresh magazine at left chest";
    case ReloadPhase::Insert:return "insert and release magazine";case ReloadPhase::HandleReady:return "grab charging handle";
    case ReloadPhase::HandlePull:return "pull charging handle back";case ReloadPhase::HandleReturn:return "charging handle returning";
    case ReloadPhase::ShellInsert:return "insert one shell and release";
    case ReloadPhase::ShellCommit:return "waiting for one native shell refill";
    case ReloadPhase::Finish:return "native completion";default:return "idle";}
}
void ReloadFeedback(float strength,unsigned ms){
    if(strength>=reload.feedback){reload.feedback=strength;reload.feedbackMs=ms;}
}
void ReloadStep(ReloadPhase phase){
    reload.phase=phase;reload.inRange=false;
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
    reload.phase=ReloadPhase::Idle;reload.request=false;reload.nativeStarted=false;reload.inRange=false;
    reload.handlePull=reload.handlePeak=reload.handleReturnFrom=0;reload.needsCharge=reload.handleCycled=false;
    reload.handleOnly=reload.handleGripValid=reload.pumpContact=false;reload.visualKind=0;
    reload.pumpAimValid=false;reload.pumpAimReturnTick=0;
    reload.magazineLatch=0;reload.magazineLatchTick=0;
    reload.shellPermit=false;reload.actionPending=false;reload.magazineSeated=false;reload.actionPoseUntil=0;RememberReloadAction();
    // Let an already-started native animation complete without re-catching it.
    reload.bypassUntil=now+(reload.profile&&reload.profile->reloadAmount?15000:7000);reload.lastGrab=true;
    reload.buttonReload=reload.buttonReloadSeen=false;reload.buttonIdleTick=0;
}
bool ReloadName(void* weapon,char (&name)[80]){
    void* def=Value<void*>(weapon,0x14);const unsigned n=Value<unsigned>(def,0x20),cap=Value<unsigned>(def,0x24);
    if(!n||n>=sizeof(name)||cap<n||cap>4096)return false;
    const char* data=cap<16?static_cast<const char*>(def)+0x10:Value<const char*>(def,0x10);
    if(!Copy(name,data,n+1)||name[n])return false;
    for(unsigned i=0;i<n;++i){char& c=name[i];if(c>='A'&&c<='Z')c+=32;if(!((c>='a'&&c<='z')||(c>='0'&&c<='9')||c=='_'||c=='-'))return false;}return true;
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
    if(Value<void*>(def)!=game+0x4F5558||Value<int>(def,0x3C)!=reload.profile->reloadAmount||Value<int>(def,0x38)!=reload.profile->capacity||
       !std::isfinite(seconds)||std::abs(seconds-reload.profile->reloadSeconds)>.001f)return false;
    const auto begin=Value<std::uintptr_t>(self,8),end=Value<std::uintptr_t>(self,12),capacity=Value<std::uintptr_t>(self,16);
    if(!begin||end<=begin||end>capacity||(end-begin)%4||capacity-begin>512)return false;
    int rounds=0;if(!Read(reinterpret_cast<void*>(begin),0,rounds)||rounds<0||rounds>reload.profile->capacity)return false;
    const int state=Value<int>(self,0x3C);if(state<0||state>6)return false;
    out.interfaceObject=self;out.definition=def;out.rounds=rounds;out.capacity=reload.profile->capacity;
    out.magazines=(int)((end-begin)/4);out.state=state;out.refilled=Value<BYTE>(self,0x40)!=0;out.tick=GetTickCount64();return true;
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
        reload.nativeStarted=false;reload.request=false;reload.soundSuppressed=false;reload.needsCharge=false;
        // A partial reload can complete while its OPTIONAL handle is still
        // held. Finish ammo accounting without dropping that physical grip.
        if(!reload.handleOnly){ReloadStep(ReloadPhase::Idle);reload.handlePull=reload.handlePeak=0;reload.handleCycled=false;}
        ReloadFeedback(.7f,100);
    }
}
void __fastcall ReloadAmmoRefill(void* base,void*){
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
    AcquireSRWLockExclusive(&reloadLock);
    ReloadAmmo ammo{};const auto now=GetTickCount64();
    const bool block=OwnedShellAmmo(self,ammo,now)&&!ShellRefillPermitted(ammo);
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
                    for(const auto& m:reloadActionMemory)if(m.pending&&m.owner==reload.owner&&m.weapon==reload.weapon&&
                        m.definition==Value<void*>(reload.weapon,0x14)&&m.rounds==ammo.rounds){reload.actionPending=true;break;}
                }else if(reload.restValid&&reload.profile->cycleAfterShot&&previous.interfaceObject==self&&previous.rounds==ammo.rounds+1&&previous.magazines==ammo.magazines){
                    reload.actionPending=true;reload.actionPoseUntil=now+2500;RememberReloadAction();
                    Log("MRELOAD21 cycle required weapon=%s rounds=%d; native ammo retained",reload.profile->name,ammo.rounds);
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
                        ReloadStep(ReloadPhase::GrabInstalled);ReloadFeedback(.35f,65);
                    }
                    if(!reload.nativeStarted){
                        reload.nativeStarted=true;
                        reload.magazineSeated=false;
                        reload.needsCharge=reload.profile->handleBone&&(Config().chargingHandle==2||(Config().chargingHandle==1&&ammo.rounds==0)||reload.actionPending||(ManualAction()&&reload.handlePull>.008f));
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
    // Release support for a NEW trigger grab or an actual part interaction.
    // A completed handle may auto-detach while trigger remains held; that
    // stale hold and native Finish must not prevent the next support grip.
    const bool interaction=ReloadActive()&&reload.phase!=ReloadPhase::Finish;
    const bool free=reload.supported&&reloadInput.enabled&&
        ((reloadInput.grab&&!reload.lastGrab)||interaction)&&!pump;
    ReleaseSRWLockShared(&reloadLock);return free;
}
bool ReloadBodyArmsHeld(void* owner){
    AcquireSRWLockShared(&reloadLock);
    const bool held=reload.owner==owner&&(ReloadActive()||reload.actionPending||GetTickCount64()<reload.actionPoseUntil||
        (reload.profile&&reload.profile->reloadAmount&&reload.ammo.state>=2))&&ReloadOwned(GetTickCount64())&&!reloadInput.bypass&&GetTickCount64()>=reload.bypassUntil;
    ReleaseSRWLockShared(&reloadLock);return held;
}
bool ReloadContactHeld(void* owner,void* weapon,int kind){
    AcquireSRWLockShared(&reloadLock);
    const bool pump=kind==2&&reload.pumpContact&&reloadInput.supportGrip&&PumpSupportPhase();
    const bool phase=kind==1?(reload.phase==ReloadPhase::Remove||reload.phase==ReloadPhase::Insert||reload.phase==ReloadPhase::ShellInsert):
        (reload.phase==ReloadPhase::HandlePull||(ManualAction()&&reload.phase==ReloadPhase::HandleReturn));
    const bool held=((phase&&reloadInput.grab)||pump)&&reload.owner==owner&&reload.weapon==weapon&&!reloadInput.bypass&&ReloadOwned(GetTickCount64());
    ReleaseSRWLockShared(&reloadLock);return held;
}
void ReloadHandBasis(const Mat native[70],Mat basis[70]){
    memcpy(basis,native,70*sizeof(Mat));
    AcquireSRWLockShared(&reloadLock);
    if((ReloadActive()||reload.actionPending||GetTickCount64()<reload.actionPoseUntil||
       (reload.profile&&reload.profile->reloadAmount&&reload.ammo.state>=2))&&reload.handRestValid&&ReloadOwned(GetTickCount64())&&!reloadInput.bypass&&GetTickCount64()>=reload.bypassUntil)
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
        RememberReloadAction();const bool pending=reload.actionPending;
        if(ReloadActive())ReloadCancel("weapon/rig/tracking origin changed",f.tick);
        reload.actionPending=pending;RememberReloadAction();
        reload={};reload.owner=owner;reload.weapon=weapon;reload.rig=rig;reload.slot=slot;reload.origin=f.origin;
        reload.lastGrab=reloadInput.grab;char name[80]{};
        const bool named=ReloadName(weapon,name);
        reload.profile=named?FindReloadWeapon(name):nullptr;
        reload.supported=reloadProfile&&reload.profile&&Value<void*>(weapon,0x28)==owner;
        Log("MRELOAD21 selected name=%s manual=%d owner=%p weapon=%p",named?name:"unresolved",reload.supported?1:0,owner,weapon);
    }
    // Return a visual-only classification from the existing protected bind.
    // AK-47/GP25 and AK-101/GP30 occupy the fixed profile indices 2 through 5.
    const bool ak=reload.supported&&reload.profile->soundIndex>=2&&reload.profile->soundIndex<=5;
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
    reload.handlePull=reload.handlePeak=0;reload.actionPending=false;
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
    const Mat part=Mul(ManualAction()?ReloadActionLocal(reload.handlePull):reload.rest[profile.handleBone-54],solved[54]);
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
void ApplyReload(const Runtime::Frame& f,const HandTargets& target,const Mat native[70],const Mat basis[70],Mat solved[70],int& contactKind,Mat contactLocal[21],bool supporting){
    contactKind=0;
    AcquireSRWLockExclusive(&reloadLock);
    const auto now=GetTickCount64();
    if(!reload.supported){ReleaseSRWLockExclusive(&reloadLock);return;}
    if(!ReloadOwned(now)||!target.leftValid||!target.rightValid){if(ReloadActive())ReloadCancel("tracking/input unavailable",now);ReleaseSRWLockExclusive(&reloadLock);return;}
    const auto& profile=*reload.profile;const Vec magazineCenter=profile.magazineCenter;
    const bool bizon=std::strcmp(profile.name,"rurif_bizon")==0;
    const bool grab=reloadInput.grab,pressed=grab&&!reload.lastGrab,released=!grab&&reload.lastGrab;
    const bool advance=reload.lastFrame!=f.id;
    const bool ammoFresh=reload.ammo.interfaceObject&&now>=reload.ammo.tick&&now-reload.ammo.tick<350;
    if(advance){reload.lastFrame=f.id;reload.lastGrab=grab;}
    if(!ReloadActive()&&ammoFresh&&reload.ammo.state<2&&binding.rightReady&&f.hands[1].trigger<.2f&&
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
    const Mat socket=Mul(profile.reloadAmount?profile.shellSocket:reload.rest[profile.magazineBone-54],solved[54]);
    const Vec socketCenter=Point(magazineCenter,socket),palm=Position(target.leftGrip);
    const float distance=Length(Sub(palm,socketCenter));
    const bool handleEnabled=profile.handleBone&&Config().chargingHandle!=0;
    const float grabRadius=ManualAction()?std::fmax(.15f,profile.handleGrabRadius):profile.handleGrabRadius;
    const float handleDistance=handleEnabled?Length(Sub(palm,Point(profile.handleGrab,Mul(ManualAction()?ReloadActionLocal(reload.handlePull):reload.rest[profile.handleBone-54],solved[54])))):100.f;
    if(advance){
        if(reload.pumpAimValid&&(!target.supportPressed||!PumpSupportPhase()||!ammoFresh||reloadInput.bypass||now<reload.bypassUntil||
           (reload.pumpAimReturnTick&&now>=reload.pumpAimReturnTick+90))){
            reload.pumpAimValid=false;reload.pumpAimReturnTick=0;
        }
        const bool canRemove=!profile.reloadAmount&&reload.ammo.rounds<reload.ammo.capacity&&reload.ammo.magazines>1;
        const bool optionalWindow=(!ReloadActive()&&(reload.ammo.state<2||profile.reloadAmount)&&(reload.ammo.rounds>0||reload.actionPending))||
                                  (reload.phase==ReloadPhase::Finish&&!reload.needsCharge&&reload.nativeStarted);
        const bool optionalHandle=optionalWindow&&ammoFresh&&handleEnabled&&now>=reload.bypassUntil&&(ManualPump()||f.hands[1].trigger<.2f);
        if(profile.reloadAmount&&(reload.phase==ReloadPhase::Idle||reload.phase==ReloadPhase::ShellInsert||reload.phase==ReloadPhase::ShellCommit||
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
            if(!reload.nativeStarted){reload.started=now;reload.startRounds=reload.ammo.rounds;reload.startMagazines=reload.ammo.magazines;reload.request=true;}
            reload.handleOnly=false;reload.needsCharge=true;
            reload.heldFromGrip=Mul(socket,Inverse(target.leftGrip));ReloadStep(ReloadPhase::Remove);ReloadFeedback(.4f,50);
        }else if(ManualAction()&&reload.phase==ReloadPhase::GrabInstalled&&pressed&&handleDistance<grabRadius&&handleDistance<distance+.04f){
            BeginReloadHandle(target,solved,now,false);
        }else if(optionalHandle&&pressed&&handleDistance<grabRadius&&
           (reload.phase==ReloadPhase::Finish||!canRemove||distance>=.14f||handleDistance<distance+(ManualAction()?.04f:0.f))){
            BeginReloadHandle(target,solved,now,true);
        }else if(!ReloadActive()&&ammoFresh&&reload.ammo.state<2&&now>=reload.bypassUntil){
            if(optionalHandle&&(!canRemove||handleDistance<distance))ReloadNear(handleDistance,profile.handleGrabRadius*.85f,now);
            else if(canRemove)ReloadNear(distance,.12f,now);
            if(pressed){
                Log("MRELOAD21 grab attempt socketDistance=%.3f ammo=%d/%d",distance,reload.ammo.rounds,reload.ammo.capacity);
                if(canRemove&&distance<.14f){
                    reload.started=now;reload.startRounds=reload.ammo.rounds;reload.startMagazines=reload.ammo.magazines;
                    reload.handleOnly=false;
                    reload.heldFromGrip=Mul(socket,Inverse(target.leftGrip));reload.request=true;reload.nativeStarted=false;
                    ReloadStep(ReloadPhase::Remove);ReloadFeedback(.4f,50);
                }
            }
        }else if(reload.phase==ReloadPhase::GrabInstalled){
            ReloadNear(distance,.12f,now);
            if(pressed&&distance<.14f){reload.heldFromGrip=Mul(socket,Inverse(target.leftGrip));ReloadStep(ReloadPhase::Remove);ReloadFeedback(.4f,50);}
        }else if(reload.phase==ReloadPhase::Remove){
            const Mat held=Mul(reload.heldFromGrip,target.leftGrip);
            const float travel=Length(Sub(Point(magazineCenter,held),socketCenter));
            if(travel>.025f&&!reload.magOutSound){ReloadCue(ReloadAudio::MagazineOut);reload.magOutSound=true;}
            if(released){
                if(travel>=.10f){ReloadStep(ReloadPhase::Pouch);ReloadFeedback(.45f,55);}
                else{if(reload.magOutSound)ReloadCue(ReloadAudio::MagazineIn);ReloadStep(ReloadPhase::GrabInstalled);}
            }
        }else if(reload.phase==ReloadPhase::Pouch){
            const Mat pouch=ReloadPouch(target);const float d=Length(Sub(palm,Point(magazineCenter,pouch)));
            ReloadNear(d,.16f,now);
            if(pressed){Log("MRELOAD21 pouch attempt distance=%.3f",d);if(d<.19f&&GrabFreshReloadPart(target,solved,pouch)){ReloadStep(ReloadPhase::Insert);ReloadFeedback(.5f,60);}}
        }else if(reload.phase==ReloadPhase::Insert){
            const Mat held=Mul(reload.heldFromGrip,target.leftGrip);
            const float d=Length(Sub(Point(magazineCenter,held),socketCenter));
            const float upright=Dot(Row(held,1),Row(socket,1)),forward=Dot(Row(held,2),Row(socket,2));
            // The cylinder's long axis is local +Z. A near, forward-facing
            // cylinder may seat with roll assistance on RELEASE, after the
            // hand unlatches. Do not twist the held hand/magazine or accept
            // a backwards cylinder. Other profiles keep the original rule.
            const bool originalAlignment=upright>.6f&&forward>.4f;
            const bool rollAssist=bizon&&!originalAlignment&&forward>.85f;
            const bool aligned=originalAlignment||rollAssist;ReloadNear(aligned?d:10.f,.095f,now);
            if(released){
                Log("MRELOAD21 insert attempt weapon=%s distance=%.3f up=%.3f forward=%.3f nativeStarted=%d rollAssist=%d accepted=%d",profile.name,d,upright,forward,reload.nativeStarted?1:0,rollAssist?1:0,(d<.11f&&aligned&&reload.nativeStarted)?1:0);
                if(d<.11f&&aligned&&reload.nativeStarted){
                    reload.magazineSeated=true;
                    ReloadCue(ReloadAudio::MagazineIn);
                    if(reload.needsCharge){if(!ManualAction()){reload.handlePull=reload.handlePeak=0;reload.handleCycled=false;}ReloadStep(ReloadPhase::HandleReady);}
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
        if(!profile.reloadAmount&&ReloadActive()&&!reload.handleOnly&&!reload.nativeStarted&&now-reload.started>2500)ReloadCancel("native reload did not start",now);
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
    if(ReloadActive()){
        // Transport the cached rigid weapon parts from the CURRENT tracked
        // gun, then apply the magazine pose and constrained handle travel.
        // Raw target.leftGrip continues driving all interaction measurements.
        for(int i=1;i<16;++i)solved[54+i]=Mul(reload.rest[i],solved[54]);
        if(reload.phase==ReloadPhase::Remove||reload.phase==ReloadPhase::Insert||reload.phase==ReloadPhase::ShellInsert)solved[profile.magazineBone]=Mul(reload.heldFromGrip,target.leftGrip);
        else if(reload.phase==ReloadPhase::Pouch)solved[profile.magazineBone]=ReloadPouch(target);
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
    if(profile.magazineLatchBone&&ammoFresh&&!reloadInput.bypass&&now>=reload.bypassUntil){
        // Jackhammer's fore-end unlocks the drum before it is removed and
        // closes after seating. This is a visual latch coupled to the mag,
        // not a charging stroke or another ammo/refill request.
        const bool open=reload.phase==ReloadPhase::Remove||reload.phase==ReloadPhase::Pouch||reload.phase==ReloadPhase::Insert;
        if(advance){
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
    ReleaseSRWLockExclusive(&reloadLock);
}
bool InstallReload(){
    if(!Config().manualReload||!Config().hands||!originalFinalize)return false;
    const BYTE update[]={0x56,0x8B,0xF1,0x83,0x7E,0x3C,0x02};
    const BYTE canFire[]={0x56,0x8B,0xF1,0x8B,0x06};
    const BYTE refill[]={0x56,0x8B,0xF1,0x8B,0x46,0x5C};
    const BYTE request[]={0x53,0x56,0x8B,0xF1,0x8A,0x5E,0x43};
    const BYTE finish[]={0xC7,0x46,0x3C,0,0,0,0};
    if(Value<void*>(game,0x4F5420+6*4)!=game+0x222690||Value<void*>(game,0x4F5420+13*4)!=game+0x222010||
       Value<void*>(game,0x4F5500+15*4)!=game+0x222F80||!Match(game+0x222690,update,sizeof(update))||
       Value<void*>(game,0x4F5420+44*4)!=game+0x2222E0||!Match(game+0x2222E0,request,sizeof(request))||
       !Match(game+0x222010,canFire,sizeof(canFire))||!Match(game+0x222F80,refill,sizeof(refill))||!Match(game+0x2226DB,finish,sizeof(finish)))return false;
    if(!Jump(game+0x222010,canFire,sizeof(canFire),reinterpret_cast<void*>(&ReloadAmmoCanFire),reinterpret_cast<void**>(&originalAmmoCanFire)))return false;
    if(!Jump(game+0x222690,update,sizeof(update),reinterpret_cast<void*>(&ReloadAmmoUpdate),reinterpret_cast<void**>(&originalAmmoUpdate)))return false;
    if(!Jump(game+0x222F80,refill,sizeof(refill),reinterpret_cast<void*>(&ReloadAmmoRefill),reinterpret_cast<void**>(&originalAmmoRefill)))return false;
    if(!Jump(game+0x2222E0,request,sizeof(request),reinterpret_cast<void*>(&ReloadAmmoReloadRequest),reinterpret_cast<void**>(&originalAmmoReloadRequest)))return false;
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
    return true;
}
