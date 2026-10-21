// Audited DZB2 MultiFireArmTargetComp launchers: heat OR laser targeting.
// The only redirected call is its existing native acquisition query. BF2 keeps
// eligibility, occlusion, angle, range, lock timers, flares and missile ownership.
struct SeekerViewState {void* weapon=nullptr;ULONGLONG tick=0;Vec target{};bool tracking=false,locked=false,direct=false;int system=0;} seekerView;
SRWLOCK seekerLock=SRWLOCK_INIT;
bool stingerHooksReady=false;
bool StingerName(void* weapon){
    char name[80]{};if(!ReloadName(weapon,name))return false;
    for(const char* n:{"emzpj_fim92_stinger","aix_fim92a","aix_strela2","chaa_qw2",
        "chat_eryx_lt","javelin","javelin_v2","nlaw"})if(!strcmp(name,n))return true;
    return false;
}
bool NativeDirectMissile(void* weapon){
    // BF2+1E1610 resolves GenericFireArm's current projectile template from
    // interface +170 -> definition +78 -> name +288. Read the same string;
    // do not invoke the resolver or change templates. DZB2 mode scripts may
    // recreate components, so no component/template pointer is retained.
    if(Value<void*>(weapon,0x170)!=game+0x4ED558)return false;
    void* def=Value<void*>(weapon,0x1E8);
    const unsigned n=Value<unsigned>(def,0x298),cap=Value<unsigned>(def,0x29C);
    if(n!=22||cap<n||cap>4096)return false;
    char name[24]{};const char* data=Value<const char*>(def,0x288);
    return Copy(name,data,n+1)&&!_stricmp(name,"javelin_direct_missile");
}
bool StingerPose(void* weapon,Mat& gun){
    AcquireSRWLockShared(&fireLock);
    if(!weapon||!fire.stinger||fire.weapon!=weapon){ReleaseSRWLockShared(&fireLock);return false;}
    FireFrame pose=fire;ReleaseSRWLockShared(&fireLock);
    if(!Config().hands||!referenceValid||!pose.gripBinding.valid||pose.owner!=Soldier()||
       Value<void*>(weapon,0x28)!=pose.owner||Value<int>(pose.owner,0x1E0)!=pose.slot||
       Value<void*>(pose.owner,0x288)!=pose.rig||Value<int>(pose.owner,0x2B8)!=0||
       Value<void*>(pose.owner,0x254)||GetTickCount64()-pose.tick>150||
       !LocalAlive()||MenuActive()||InventoryStowed(pose.owner))return false;
    void* seat=Value<void*>(Value<void*>(Player(),0x80),4);if(seat&&seat!=pose.owner)return false;
    Runtime::Frame sample{};DWORD process=0;GetWindowThreadProcessId(GetForegroundWindow(),&process);
    if(process!=GetCurrentProcessId()||!Runtime::Snapshot(sample)||!sample.focused||
       !sample.hands[1].aimValid||!sample.hands[1].gripValid||sample.origin!=pose.origin||
       sample.id<pose.frame||sample.id-pose.frame>2)return false;
    if(sample.id!=pose.frame){
        Mat body{},solved[70];bool supporting=false;
        if(!Read(pose.owner,0x88,body)||!Rigid(body))return false;
        const HandTargets targets=Targets(pose.native[0],body,sample,pose.rightPalm,pose.leftPalm,pose.palmsValid,pose.supporting);
        if(!SolveHands(pose.native,pose.gripBinding,targets,solved,supporting,&pose.aimState))return false;
        pose.trackedWeapon=Mul(solved[54],body);
    }
    gun=pose.trackedWeapon;return Rigid(gun);
}
// Exact stack contract at BF2+20D00A: two Vec pointers, projectile target type,
// range from target-interface +B4, angle, four object arguments and three flags.
// Twelve 32-bit arguments / 48 bytes. r116 omitted range: native callee cleanup
// then consumed a saved register and corrupted ESI before BF2+20D028.
using SeekerQuery=void*(__thiscall*)(void*,const Vec*,const Vec*,void*,float,float,void*,void*,void*,void*,int,int,int);
void* __fastcall SeekerQueryHook(void* self,void*,const Vec* position,const Vec* direction,void* type,float range,float angle,
                                void* owner,void* weapon,void* ignored,void* firing,int a,int b,int c){
    const auto query=reinterpret_cast<SeekerQuery>(Value<void*>(Value<void*>(self),0x1BC));
    Mat gun{};Vec start{},ray{};
    if(stingerHooksReady&&weapon==firing&&StingerPose(weapon,gun)){
        start=Position(gun);ray=Unit(Row(gun,2));position=&start;direction=&ray;
        static unsigned reports=0;if(reports++<6)Log("SEEKER126 native query uses controller bore; range=%.3f angle=%.3f; original 12-argument contract",range,angle);
    }
    return query(self,position,direction,type,range,angle,owner,weapon,ignored,firing,a,b,c);
}
using SeekerUpdate=void(__thiscall*)(void*,unsigned,unsigned);
SeekerUpdate originalSeekerUpdate=nullptr;
void __fastcall SeekerUpdateHook(void* self,void*,unsigned a,unsigned b){
    void* weapon=Value<void*>(static_cast<BYTE*>(self)-4);Mat gun{};
    const int system=Value<int>(Value<void*>(self,0x4C),0x80);
    const bool local=stingerHooksReady&&Value<void*>(self)==game+0x4F7040&&
        (system==1||system==2)&&StingerPose(weapon,gun);
    // Native 2D ObjectMarker enables seeking via interface slot +94. Our
    // physical optic takes that role only for the local equipped launcher.
    // Restore the original gate immediately; never edit a weapon template.
    const BYTE saved=local?Value<BYTE>(self,0x35):0,enabled=1;
    const bool changed=local&&Copy(static_cast<BYTE*>(self)+0x35,&enabled,1);
    originalSeekerUpdate(self,a,b);
    if(changed)Copy(static_cast<BYTE*>(self)+0x35,&saved,1);
    if(!local)return;
    SeekerViewState view{};view.weapon=weapon;view.tick=GetTickCount64();view.system=system;
    view.direct=system==1||NativeDirectMissile(weapon);
    void* target=Value<void*>(Value<void*>(self,0x1C),4);Mat world{};
    // +1C is the native weak target, +20 is its confirmed lock flag (the
    // engine's own +223D60 getter uses these same fields).
    if(target&&Read(target,0x88,world)&&Rigid(world)){
        view.target=Position(world);view.tracking=true;view.locked=Value<BYTE>(self,0x20)!=0;
    }
    static void* lastWeapon=nullptr;static int lastSystem=-1;static bool lastDirect=false;static unsigned reports=0;
    if(reports<16&&(weapon!=lastWeapon||system!=lastSystem||view.direct!=lastDirect)){
        char name[80]{};ReloadName(weapon,name);
        Log("SEEKER126 weapon=%s nativeSystem=%d directOrAA=%d; native target rules/timers retained",name,system,view.direct?1:0);
        lastWeapon=weapon;lastSystem=system;lastDirect=view.direct;++reports;
    }
    AcquireSRWLockExclusive(&seekerLock);seekerView=view;ReleaseSRWLockExclusive(&seekerLock);
}
SeekerViewState StingerView(void* weapon){
    AcquireSRWLockShared(&seekerLock);auto view=seekerView;ReleaseSRWLockShared(&seekerLock);
    if(view.weapon!=weapon||GetTickCount64()-view.tick>200)return {};
    return view;
}
bool InstallStingerGuidance(){
    const BYTE query[]={0xFF,0x93,0xBC,0x01,0x00,0x00};
    const BYTE update[]={0x55,0x8B,0xEC,0x83,0xEC,0x2C};
    if(!Match(game+0x20D00A,query,sizeof(query))||!Match(game+0x20CE00,update,sizeof(update))||
       Value<void*>(game+0x4F7040,0x18)!=game+0x20CE00||
       Value<void*>(game+0x4F7040,0x9C)!=game+0x223D60)return false;
    if(!Jump(game+0x20CE00,update,sizeof(update),reinterpret_cast<void*>(&SeekerUpdateHook),reinterpret_cast<void**>(&originalSeekerUpdate)))return false;
    DWORD old=0;BYTE* site=game+0x20D00A;
    if(!VirtualProtect(site,6,PAGE_EXECUTE_READWRITE,&old))return false;
    site[0]=0xE8;*reinterpret_cast<LONG*>(site+1)=LONG(reinterpret_cast<BYTE*>(&SeekerQueryHook)-(site+5));site[5]=0x90;
    DWORD ignored;VirtualProtect(site,6,old,&ignored);FlushInstructionCache(GetCurrentProcess(),site,6);
    stingerHooksReady=true;return true;
}
