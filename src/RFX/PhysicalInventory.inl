// Controller release snapshots feed the engine's native projectile creation.
// Ordinary gear uses private render copies; it never becomes a game pickup.
struct InventoryDrop {
    void* owner=nullptr;void* identity=nullptr;void* weapon=nullptr;
    int item=0,slot=-1;ULONGLONG born=0,tick=0;Mat world{};Vec velocity{},angular{};
    bool active=false,resting=false;
} inventoryDrops[10];
struct InventoryMotion {
    struct Sample{Mat pose;ULONGLONG tick;};Sample samples[16]{};
    int count=0;void* owner=nullptr;std::uint64_t origin=0;Mat world{};
} inventoryMotion;
struct ItemThrow {
    void* owner=nullptr;void* identity=nullptr;void* weapon=nullptr;
    int item=0,kind=0;std::uint64_t origin=0;ULONGLONG tick=0;
    Mat launch{};Vec velocity{},angular{};float cookStart=-1.f;bool active=false,pressAccepted=false,c4Started=false;
} itemThrow;
SRWLOCK itemThrowLock=SRWLOCK_INIT;
PVOID volatile throwWeaponPublished=nullptr;
bool physicalThrowProfile=false;
bool grappleThrowProfile=false;
#include "GrenadeReleaseAudio.inl"
struct GrenadeCook {void* owner=nullptr;void* identity=nullptr;void* weapon=nullptr;float start=-1.f;};
GrenadeCook grenadeCook;
bool grenadeCookProfile=false;
float GrenadeClock(){return reinterpret_cast<float(__cdecl*)()>(game+0xC4400)();}
bool CookableFrag(void* weapon){char name[80]{};return ReloadName(weapon,name)&&!strcmp(name,"ushgr_m67");}
float CookingStart(void* weapon){const auto& s=grenadeCook;return weapon&&s.weapon==weapon&&s.owner==Soldier()&&s.identity==Value<void*>(Player(),0xCC)?s.start:-1.f;}
bool RightGrenadePrimed(){return CookingStart(grenadeCook.weapon)>=0;}
void PrimeGrenade(void* owner,void* weapon){
    auto& s=grenadeCook;if(s.weapon==weapon&&s.owner==owner&&s.identity==Value<void*>(Player(),0xCC))return;
    s={};if(!grenadeCookProfile||!Tuning().grenadeCooking||!CookableFrag(weapon))return;
    s.owner=owner;s.identity=Value<void*>(Player(),0xCC);s.weapon=weapon;s.start=GrenadeClock();
    Runtime::Haptic(1,.35f,60);Log("COOK93 frag armed in right hand native-clock=%.3f",s.start);
}
thread_local float projectileCookAge=0;
// Only the verified GenericProjectile initialization callsite is redirected.
// Its result is the duration of the native event 0x1D, not just a HUD timer.
float __fastcall CookDurationHook(void* self,void*,void* argument){
    const float ttl=reinterpret_cast<float(__thiscall*)(void*,void*)>(game+0x1E74B0)(self,argument);
    if(projectileCookAge<=0||!std::isfinite(ttl)||std::abs(ttl-2.8f)>=.005f)return ttl;
    const float remaining=std::max(.001f,ttl-projectileCookAge);
    Log("COOK93 native detonation event duration %.3f -> %.3f seconds",ttl,remaining);return remaining;
}
bool InstallGrenadeCooking(){
    const BYTE call[]={0xE8,0x88,0x42,0xFD,0xFF};
    const BYTE schedule[]={0x50,0x6A,0x1D,0x57,0xFF,0x92,0xD0,0,0,0};
    const BYTE remaining[]={0x55,0x8B,0xEC,0x51,0x56,0x8B,0xF1};
    if(!Match(game+0x213223,call,sizeof(call))||!Match(game+0x21324B,schedule,sizeof(schedule))||
       !Match(game+0x211F30,remaining,sizeof(remaining))||Value<void*>(game,0x4F883C)!=game+0x211F30)return false;
    DWORD old=0;if(!VirtualProtect(game+0x213223,5,PAGE_EXECUTE_READWRITE,&old))return false;
    *reinterpret_cast<LONG*>(game+0x213224)=LONG(reinterpret_cast<BYTE*>(&CookDurationHook)-(game+0x213228));
    DWORD ignored=0;VirtualProtect(game+0x213223,5,old,&ignored);FlushInstructionCache(GetCurrentProcess(),game+0x213223,5);return true;
}
ItemThrow ThrowSnapshot(){AcquireSRWLockShared(&itemThrowLock);const auto t=itemThrow;ReleaseSRWLockShared(&itemThrowLock);return t;}
void ClearItemThrow(){AcquireSRWLockExclusive(&itemThrowLock);InterlockedExchangePointer(&throwWeaponPublished,nullptr);itemThrow={};ReleaseSRWLockExclusive(&itemThrowLock);}
void ClearInventoryCarryDrops(){for(auto& drop:inventoryDrops)drop={};}
void ResetInventoryMotion(){inventoryMotion={};const auto t=ThrowSnapshot();if(!t.active||t.cookStart<0)ClearItemThrow();}
bool ThrowOwned(const ItemThrow& t){
    const auto now=GetTickCount64();
    return t.active&&t.tick&&now>=t.tick&&now-t.tick<2500&&t.owner==Soldier()&&LocalAlive()&&
        t.identity==Value<void*>(Player(),0xCC)&&Value<int>(t.owner,0x1E0)==t.item&&
        InventoryItem(t.owner,t.item)==t.weapon&&Value<void*>(t.weapon,0x28)==t.owner&&
        !Value<void*>(t.owner,0x254)&&Value<int>(t.owner,0x2B8)==0&&!MenuActive();
}
bool InventoryThrowBusy(){return ThrowOwned(ThrowSnapshot());}
bool InventoryThrowInput(void* owner){const auto t=ThrowSnapshot();return t.owner==owner&&ThrowOwned(t);}
bool InventoryThrowPress(const ItemThrow& t,ULONGLONG now){
    // Release after the native throw-start routine accepts the press. A fixed
    // short pulse can be lost while the newly drawn item is still deploying.
    // Supply components that create immediately clear the request on creation.
    return !t.pressAccepted&&now>=t.tick&&now-t.tick<1500;
}
bool C4ReleasePending(void* weapon,bool& press){
    const auto t=ThrowSnapshot();const bool owned=t.weapon==weapon&&ThrowOwned(t);
    press=owned&&InventoryThrowPress(t,GetTickCount64());return owned;
}
void C4ReleaseStarted(void* weapon){
    bool report=false;
    AcquireSRWLockExclusive(&itemThrowLock);
    if(itemThrow.active&&itemThrow.weapon==weapon&&!itemThrow.c4Started){itemThrow.c4Started=true;report=true;}
    ReleaseSRWLockExclusive(&itemThrowLock);
    if(report)Log("C454 native launch delay started; waiting for projectile creation");
}
bool InventoryItemAway(int item,ULONGLONG now){
    if(item<1||item>9)return false;const auto& d=inventoryDrops[item];
    return d.active&&d.owner==inventoryHands.owner&&d.identity==inventoryHands.identity&&now>=d.born&&now-d.born<3000;
}
int PhysicalConsumable(void* weapon){
    char name[80]{};if(!ReloadName(weapon,name))return 0;
    if(HandGrenade(name))return 1;
    if(!strcmp(name,"medikit")||!strcmp(name,"ammokit")||!strcmp(name,"ammobag")||!strcmp(name,"bf3_medikit")||!strcmp(name,"bf3_ammobox"))return 2;
    // Exact audited DZB2 templates: use their ThrownFireComp and native
    // projectiles so adhesion, proximity triggers and ammo stay mod-owned.
    if(!strcmp(name,"at_mine")||!strcmp(name,"usmin_claymore")||!strcmp(name,"at_minev2")||
       !strcmp(name,"bf4_slam")||!strcmp(name,"yun_cod16mine")||!strcmp(name,"antipersonnelmine"))return 3;
    if(PhysicalC4Name(name))return 4;
    // DZB2/Survival retain their own ammo, projectile and deployment logic.
    // Physical release must create each laptop's own native projectile, not a
    // cosmetic inventory drop that returns to the shoulder after 3 seconds.
    if(PhysicalLaptopName(name))return 5;
    // The beacon's SingleFireComp creates hm_point_projectile; its native
    // collision/end-effect spawns point_projectile (the active satellite).
    if(PhysicalBeaconName(name))return 6;
    // GrapplingHookRope has its own launch/rope lifecycle, not rigid-body
    // projectile velocity or angular setters. See CreatePhysicalGrapple.
    if(PhysicalGrappleName(name))return 7;
    return 0;
}
bool InventoryFireControls(const Runtime::Frame& f,bool& down){
    down=false;const auto t=ThrowSnapshot();
    if(t.active){
        if(ThrowOwned(t)&&t.origin==f.origin){down=InventoryThrowPress(t,f.tick);return true;}
        Log("THROW54 request ended without a native projectile (equip/cooldown/ammo or context changed)");ClearItemThrow();
    }
    // Native firing is requested only on release, not by the trigger while
    // holding physical throwable gear. Classic/glued modes retain trigger use.
    return PhysicalReleaseCarry()&&inventoryHands.owner==Soldier()&&
        !inventoryHands.empty&&PhysicalConsumable(InventoryItem(inventoryHands.owner,inventoryHands.selected))!=0;
}
bool InventoryFirePolicy(void* owner,float& primary,float& secondary){
    if(KnifeFirePolicy(owner,primary,secondary))return true;
    if(C4FirePolicy(owner,primary,secondary))return true;
    const auto t=ThrowSnapshot();
    if(t.owner==owner&&ThrowOwned(t)){
        primary=InventoryThrowPress(t,GetTickCount64())?1.f:0.f;secondary=0;return true;
    }
    if(InventoryStowed(owner)||(PhysicalReleaseCarry()&&inventoryHands.owner==owner&&
       PhysicalConsumable(InventoryItem(owner,Value<int>(owner,0x1E0))))){primary=secondary=0;return true;}
    return false;
}
void SampleInventoryMotion(const Runtime::Frame& f,void* owner){
    Mat native{};if(!referenceValid||!Read(views[0].object,0x3C,native)||!Rigid(native)){inventoryMotion={};return;}
    auto& m=inventoryMotion;const Mat world=Compose(Level(native),reference,f.hands[1].grip,Config().units);
    if(!Rigid(world)){m={};return;}
    if(m.owner!=owner||m.origin!=f.origin||!m.count||f.tick<m.samples[m.count-1].tick||f.tick-m.samples[m.count-1].tick>150){
        m={};m.owner=owner;m.origin=f.origin;
    }
    m.world=world;
    if(m.count&&f.tick==m.samples[m.count-1].tick)return;
    if(m.count==16){memmove(m.samples,m.samples+1,15*sizeof(m.samples[0]));m.count=15;}
    m.samples[m.count++]={world,f.tick};
}
Vec InventoryReleaseVelocity(){
    const auto& m=inventoryMotion;if(m.count<2)return {};
    const auto& last=m.samples[m.count-1];int first=m.count-2;
    while(first>0&&last.tick-m.samples[first].tick<65)--first;
    const auto& before=m.samples[first];const auto dt=last.tick-before.tick;
    if(dt<15||dt>150)return {};
    Vec v=Scale(Sub(Position(last.pose),Position(before.pose)),1000.f/float(dt));const float speed=Length(v),limit=12.f*Config().units;
    if(!std::isfinite(speed)||speed>limit*2)return {}; // tracking/snap discontinuity
    // Preserve gentle releases; deliberate throws get up to 3x hand speed.
    const float metres=speed/Config().units;
    const float gain=1.f+2.f*std::max(0.f,std::min(1.f,(metres-.35f)/1.15f));
    v=Scale(v,gain);const float boosted=Length(v),cap=24.f*Config().units;
    if(boosted>cap)v=Scale(v,cap/boosted);return v;
}
// Angular velocity is measured in world space over the same short history.
// No random spin: a straight release stays straight, a wrist flick tumbles.
Vec InventoryReleaseAngular(){
    const auto& m=inventoryMotion;if(m.count<2)return {};
    const auto& last=m.samples[m.count-1];int first=m.count-2;
    while(first>0&&last.tick-m.samples[first].tick<50)--first;
    const auto& before=m.samples[first];const auto dt=last.tick-before.tick;
    if(dt<15||dt>150)return {};
    Vec sine{};float trace=0;
    for(int i=0;i<3;++i){sine=Add(sine,Cross(Row(before.pose,i),Row(last.pose,i)));trace+=Dot(Row(before.pose,i),Row(last.pose,i));}
    sine=Scale(sine,.5f);const float length=Length(sine);
    const float angle=std::atan2(length,std::max(-1.f,std::min(1.f,(trace-1.f)*.5f)));
    if(length<.0001f||angle>1.8f)return {};
    return Scale(sine,std::min(25.f,angle*1000.f/float(dt))/length);
}
Mat ThrowRotation(Vec axis,float angle){
    const float x=axis.x,y=axis.y,z=axis.z,c=std::cos(angle),s=std::sin(angle),t=1-c;
    Mat m=Identity();
    m.m[0][0]=t*x*x+c;m.m[0][1]=t*x*y+s*z;m.m[0][2]=t*x*z-s*y;
    m.m[1][0]=t*x*y-s*z;m.m[1][1]=t*y*y+c;m.m[1][2]=t*y*z+s*x;
    m.m[2][0]=t*x*z+s*y;m.m[2][1]=t*y*z-s*x;m.m[2][2]=t*z*z+c;return m;
}
void ReleaseInventoryItem(const Runtime::Frame& f,void* owner,int item,int slot,bool holster,bool stationary){
    void* weapon=InventoryItem(owner,item);if(!weapon)return;
    if(holster&&CookingStart(weapon)<0){if(IsPhysicalC4(weapon))C4Holstered(weapon);Log("SLOTS51 returned nativeItem=%d to slot=%d",item,slot);return;}
    const Vec velocity=stationary?Vec{}:InventoryReleaseVelocity(),angular=stationary?Vec{}:InventoryReleaseAngular();Mat launch=inventoryMotion.world;
    if(inventoryMotion.owner!=owner||!Rigid(launch))return;
    const int consumable=PhysicalConsumable(weapon);
    if(consumable){
        if(!physicalThrowProfile||(consumable==4&&!c4Profile)||(consumable==7&&!grappleThrowProfile)){Log("THROW54 native profile unavailable; item returned to inventory");return;}
        ItemThrow t{};t.owner=owner;t.identity=inventoryHands.identity;t.weapon=weapon;t.item=item;
        t.kind=consumable;
        t.cookStart=CookingStart(weapon);grenadeCook={};
        t.origin=f.origin;t.tick=f.tick;t.launch=consumable==4?Mul(C4ChargeOffset(weapon),launch):launch;t.velocity=velocity;t.angular=angular;t.active=true;
        if(consumable==5)t.launch=Mul(laptopFromPalm,launch);
        if(consumable==6)t.launch=Mul(dzBeaconFromPalm,launch);
        if(SurvivalSentryItem(weapon)){t.launch=SentryReleasePose(t.launch,weapon);t.angular={};}
        // A small outward offset keeps a gently released object off the hand.
        SetPosition(t.launch,Add(Position(t.launch),Direction({0,0,.045f*Config().units},launch)));
        AcquireSRWLockExclusive(&itemThrowLock);itemThrow=t;InterlockedExchangePointer(&throwWeaponPublished,weapon);ReleaseSRWLockExclusive(&itemThrowLock);
        GrenadeReleaseCue(owner,weapon);
        Log("THROW54 release item=%d kind=%d velocity=(%.2f %.2f %.2f)",item,consumable,velocity.x,velocity.y,velocity.z);
    }else if(item>=1&&item<=9){
        auto& d=inventoryDrops[item];d={};d.owner=owner;d.identity=inventoryHands.identity;d.weapon=weapon;
        d.item=item;d.slot=slot;d.born=d.tick=f.tick;d.world=launch;d.velocity=velocity;d.angular=angular;d.active=true;
        FireFrame pose{};AcquireSRWLockShared(&fireLock);pose=fire;ReleaseSRWLockShared(&fireLock);
        if(pose.owner==owner&&pose.weapon==weapon&&pose.origin==f.origin&&f.tick>=pose.tick&&f.tick-pose.tick<150&&Rigid(pose.trackedWeapon))d.world=pose.trackedWeapon;
        Log("GEAR51 released item=%d speed=%.2f return=3s",item,Length(velocity));
    }
    Runtime::Haptic(1,.18f,35);
}
void UpdateRightCooking(const Runtime::Frame& f){
    const auto& h=inventoryHands;
    void* owner=nullptr;void* identity=nullptr;
    if(!LocalAlive()||h.dead||!FootOwner(owner,identity)||owner!=h.owner||identity!=h.identity){grenadeCook={};return;}
    if(!Tuning().grenadeCooking&&!RightGrenadePrimed())return;
    void* weapon=InventoryItem(h.owner,h.selected);
    if(h.physical&&!h.empty&&CookableFrag(weapon)){
        PrimeGrenade(h.owner,weapon);const float start=CookingStart(weapon);
        if(start>=0&&GrenadeClock()-start>=2.8f&&!InventoryThrowBusy()){
            ReleaseInventoryItem(f,h.owner,h.selected,4,false,true);
            inventoryHands.empty=true;inventoryHands.releaseArmed=false;
        }
    }else if(!ThrowSnapshot().active)grenadeCook={};
}
void UpdateInventoryDrops(ULONGLONG now){
    for(auto& d:inventoryDrops)if(d.active){
        if(now<d.born||now-d.born>=3000||d.owner!=inventoryHands.owner||d.identity!=inventoryHands.identity||
           inventoryHands.dead||InventoryItem(d.owner,d.item)!=d.weapon){d.active=false;continue;}
        if(d.resting){d.tick=now;continue;}
        float elapsed=std::min(.15f,float(now-d.tick)*.001f);d.tick=now;
        while(elapsed>0){
            const float dt=std::min(.016f,elapsed);elapsed-=dt;
            const Vec old=Position(d.world);d.velocity.y-=9.81f*Config().units*dt;
            Vec p=Add(old,Scale(d.velocity,dt)),hit{};float ground=0;
            bool stopped=false;
            if(KnifeThrowContact(d.owner,d.identity,d.weapon,old,p,d.velocity,now)){
                SetPosition(d.world,p);d.velocity={};d.angular={};d.resting=true;break;
            }
            if(WorldSegment(old,p,d.owner,hit)){
                p=Add(hit,Scale(Unit(Sub(old,p)),.04f));
                // Wall contact sheds horizontal speed; gravity continues.
                // A separate downward probe distinguishes floors/rooftops.
                d.velocity.x*=-.15f;d.velocity.z*=-.15f;
                Vec above=p,below=p,surface{};above.y+=.12f;below.y-=.14f;
                if(d.velocity.y<=0&&WorldSegment(above,below,d.owner,surface)){p.y=surface.y+.07f;stopped=true;}
                if(d.velocity.y>0)d.velocity.y=0;
            }
            if(TerrainHeight(p,ground)&&p.y<ground+.06f){p.y=ground+.06f;stopped=true;}
            const float spin=Length(d.angular);
            if(spin>.001f)d.world=Mul(d.world,ThrowRotation(Scale(d.angular,1.f/spin),spin*dt));
            SetPosition(d.world,p);
            if(stopped){d.velocity={};d.angular={};d.resting=true;break;}
        }
    }
}
// BF2+2213D0: native object creation receives a launch matrix and velocity.
// ThrownFireComp +210140 subsequently overwrites velocity, so its scoped
// wrapper reapplies our release velocity AFTER that stock routine finishes.
using ProjectileCreate=void*(__thiscall*)(void*,const Mat*,const Vec*);
ProjectileCreate originalProjectileCreate=nullptr;
using ThrownCommit=int(__thiscall*)(void*);ThrownCommit originalThrownCommit=nullptr;
using ThrownAdvance=bool(__thiscall*)(void*);ThrownAdvance originalThrownAdvance=nullptr;
using ThrownStart=int(__thiscall*)(void*,int,int);ThrownStart originalThrownStart=nullptr;
thread_local void* physicalThrownComponent=nullptr;
thread_local void* physicalThrownObject=nullptr;
thread_local ItemThrow physicalThrownSnapshot{};
void SetThrownSpin(void* object,const Vec& angular){
    void* physics=Value<void*>(object,0x44);void* setter=Value<void*>(Value<void*>(physics),0x68);
    if(physics&&Code(setter,(HMODULE)game))reinterpret_cast<void(__thiscall*)(void*,const Vec*)>(setter)(physics,&angular);
}
void FinishPhysicalThrow(const ItemThrow& t,void* object){
    if(!object)return;
    if(t.kind!=7)SetThrownSpin(object,SurvivalSentryItem(t.weapon)?Vec{}:t.angular);
    AcquireSRWLockExclusive(&itemThrowLock);
    if(itemThrow.weapon==t.weapon&&itemThrow.tick==t.tick){itemThrow.active=false;InterlockedExchangePointer(&throwWeaponPublished,nullptr);}
    ReleaseSRWLockExclusive(&itemThrowLock);
    Log("THROW54 native projectile=%p item=%d releaseDelay=%llums speed=%.2f spin=%.2f",object,t.item,GetTickCount64()-t.tick,Length(t.velocity),Length(t.angular));
    char heldName[80]{},projectileName[80]{};
    if(ReloadName(t.weapon,heldName)&&ReloadName(object,projectileName))
        Log("THROW124 deployed handheld=%s projectile=%s; native ammo/activation retained",heldName,projectileName);
    if(PhysicalLaptop(t.weapon)){
        char name[80]{};ReloadName(object,name);
        char held[80]{};ReloadName(t.weapon,held);
        Log("LAPTOP105 released handheld=%s native projectile=%s slot=%d; original deployment/terminal activation stays with the mod",held,name,t.item);
    }
}
void* CreatePhysicalGrapple(void* self,const ItemThrow& t){
    // BF2+221437 passes ThrownFireComp+138's charge fraction to the native
    // GrapplingHookRope factory (+1D9CC0). That factory reads position and
    // forward from the matrix, ignores ProjectileCreate's velocity argument,
    // and initializes the rope links plus its native network creation event.
    // Supply controller trajectory and a native drop/throw charge for this
    // call only. Never modify the shared template or touch rope physics as
    // though it were a GenericProjectile.
    if(!grappleThrowProfile||Value<void*>(self)!=game+0x4F7DB8)return nullptr;
    float previous=0;if(!Read(self,0x138,previous)||!std::isfinite(previous))return nullptr;
    Mat launch=t.launch;const float speed=Length(t.velocity)/Config().units;
    if(!std::isfinite(speed))return nullptr;
    const bool throwing=speed>.5f;
    if(throwing){
        const Vec forward=Unit(t.velocity);
        const Vec up=std::abs(forward.y)>.98f?Vec{0,0,1}:Vec{0,1,0};
        const Vec right=Unit(Cross(up,forward)),vertical=Cross(forward,right);
        for(int i=0;i<3;++i){const Vec row=i==0?right:i==1?vertical:forward;
            launch.m[i][0]=row.x;launch.m[i][1]=row.y;launch.m[i][2]=row.z;}
    }
    // Below 0.8 the engine uses its ledge-drop branch; 0.8..1 is the
    // authored cast. A gentle release drops, a deliberate swing casts.
    const float charge=throwing?.8f+.2f*std::min(1.f,(speed-.5f)/8.f):0.f;
    if(!Rigid(launch)||!Copy(static_cast<BYTE*>(self)+0x138,&charge,4))return nullptr;
    Vec velocity=t.velocity;void* object=originalProjectileCreate(self,&launch,&velocity);
    Copy(static_cast<BYTE*>(self)+0x138,&previous,4);
    if(object)Log("GRAPPLE128 native rope created speed=%.2f charge=%.3f; attachment/climbing/network retained",speed,charge);
    return object;
}
int __fastcall ThrownStartHook(void* self,void*,int first,int second){
    // This entry receives the fire interface (component + 0x10). Its true
    // result acknowledges starting the native charge/throw state machine.
    void* component=static_cast<BYTE*>(self)-0x10;
    if(Value<void*>(component,0x0C)!=InterlockedCompareExchangePointer(&throwWeaponPublished,nullptr,nullptr))
        return originalThrownStart(self,first,second);
    const auto t=ThrowSnapshot();
    if(Value<void*>(self)!=game+0x4F7C98||Value<void*>(component,0x0C)!=t.weapon||!ThrowOwned(t))
        return originalThrownStart(self,first,second);
    const int result=originalThrownStart(self,first,second);
    if(result){
        AcquireSRWLockExclusive(&itemThrowLock);
        if(itemThrow.active&&itemThrow.weapon==t.weapon&&itemThrow.tick==t.tick)itemThrow.pressAccepted=true;
        ReleaseSRWLockExclusive(&itemThrowLock);
    }
    return result;
}
bool __fastcall ThrownAdvanceHook(void* self,void*){
    void* component=static_cast<BYTE*>(self)-0x10;
    if(Value<void*>(component,0x0C)!=InterlockedCompareExchangePointer(&throwWeaponPublished,nullptr,nullptr))return originalThrownAdvance(self);
    const auto t=ThrowSnapshot();
    if(Value<void*>(self)!=game+0x4F7C98||Value<void*>(component,0x0C)!=t.weapon||!t.pressAccepted||!ThrowOwned(t)||Value<BYTE>(self,0x138))return originalThrownAdvance(self);
    void* getter=Value<void*>(Value<void*>(self),0xD4);void* definition=Value<void*>(self,0x184);
    if(!Code(getter,(HMODULE)game))return originalThrownAdvance(self);
    bool advanced=false;
    // Keep native state transitions, ammo, fuse and network events. Only the
    // local released grenade's two animation wait timestamps are elapsed.
    for(int i=0;i<2;++i){
        const int state=reinterpret_cast<int(__thiscall*)(void*)>(getter)(self);
        if(state!=2&&state!=3)break;
        const unsigned duration=Value<unsigned>(definition,state==2?0x88:0x20);
        const unsigned now=reinterpret_cast<unsigned(__cdecl*)()>(game+0xC4420)();
        if(duration>10000||now<duration)break;
        const unsigned elapsed=now-duration;
        if(!Copy(static_cast<BYTE*>(self)+(state==2?0x12C:0x130),&elapsed,4))break;
        const bool step=originalThrownAdvance(self);advanced|=step;if(!step)break;
    }
    return advanced?true:originalThrownAdvance(self);
}
void* __fastcall ProjectileCreateHook(void* self,void*,const Mat* matrix,const Vec* velocity){
    void* weapon=Value<void*>(self,0x0C);
    Mat knifeLaunch{};Vec knifeVelocity{};const int physicalKnife=weapon==InterlockedCompareExchangePointer(&knifeWeaponPublished,nullptr,nullptr)?KnifeNativeLaunch(weapon,knifeLaunch,knifeVelocity):0;
    if(physicalKnife){
        void* object=physicalKnife>0?originalProjectileCreate(self,&knifeLaunch,&knifeVelocity):nullptr;
        KnifeNativeCreated(weapon,object);return object;
    }
    if(weapon==InterlockedCompareExchangePointer(&defibWeaponPublished,nullptr,nullptr)){
        Mat paddleLaunch{};Vec paddleVelocity{};
        if(DefibNativeLaunch(weapon,paddleLaunch,paddleVelocity)){
            void* object=originalProjectileCreate(self,&paddleLaunch,&paddleVelocity);
            DefibNativeCreated(weapon,object);return object;
        }
    }
    if(weapon!=InterlockedCompareExchangePointer(&throwWeaponPublished,nullptr,nullptr)){
        // Glued mode still permits native trigger throws. Keep the grab fuse.
        const float start=CookingStart(weapon),prior=projectileCookAge;
        if(start>=0&&CookableFrag(weapon))projectileCookAge=std::max(0.f,std::min(30.f,GrenadeClock()-start));
        void* object=originalProjectileCreate(self,matrix,velocity);
        if(object&&start>=0){grenadeCook={};inventoryHands.empty=true;inventoryHands.releaseArmed=false;}
        projectileCookAge=prior;return object;
    }
    const auto t=ThrowSnapshot();
    if(Value<void*>(self,0x0C)!=t.weapon||!ThrowOwned(t))return originalProjectileCreate(self,matrix,velocity);
    if(t.kind==7){
        void* object=CreatePhysicalGrapple(self,t);
        if(self==physicalThrownComponent){physicalThrownObject=object;physicalThrownSnapshot=t;}
        else FinishPhysicalThrow(t,object);
        return object;
    }
    const float savedAge=projectileCookAge;
    projectileCookAge=grenadeCookProfile&&t.cookStart>=0&&CookableFrag(weapon)?std::max(0.f,std::min(30.f,GrenadeClock()-t.cookStart)):0;
    void* object=originalProjectileCreate(self,&t.launch,&t.velocity);
    // Match the per-instance remaining-time query to the scheduled event.
    // Shared projectile templates and other players' grenades are untouched.
    if(object&&projectileCookAge>0&&Value<void*>(object,0x154)==game+0x4F8788){
        const float born=Value<float>(object,0x1B0)-projectileCookAge;
        if(std::isfinite(born))Copy(static_cast<BYTE*>(object)+0x1B0,&born,4);
        Log("COOK93 native grenade age=%.3f seconds",projectileCookAge);
    }
    projectileCookAge=savedAge;
    if(self==physicalThrownComponent){physicalThrownObject=object;physicalThrownSnapshot=t;}
    else FinishPhysicalThrow(t,object);
    return object;
}
int __fastcall ThrownCommitHook(void* self,void*){
    if(Value<void*>(self,0x0C)!=InterlockedCompareExchangePointer(&throwWeaponPublished,nullptr,nullptr))return originalThrownCommit(self);
    const auto t=ThrowSnapshot();
    if(Value<void*>(self)!=game+0x4F7DB8||Value<void*>(self,0x0C)!=t.weapon||!ThrowOwned(t))return originalThrownCommit(self);
    physicalThrownComponent=self;physicalThrownObject=nullptr;physicalThrownSnapshot={};
    const int result=originalThrownCommit(self);void* object=physicalThrownObject;
    physicalThrownComponent=nullptr;physicalThrownObject=nullptr;
    if(object&&physicalThrownSnapshot.tick==t.tick){
        if(t.kind!=7){
            void* physics=Value<void*>(object,0x44);void* setVelocity=Value<void*>(Value<void*>(physics),0x64);
            // Same native physics interface and setter the inspected routine uses.
            if(physics&&Code(setVelocity,(HMODULE)game))reinterpret_cast<void(__thiscall*)(void*,const Vec*)>(setVelocity)(physics,&t.velocity);
        }
        FinishPhysicalThrow(t,object);
    }
    return result;
}
bool InstallPhysicalThrow(){
    const BYTE ropeCharge[]={0x8B,0x86,0x38,1,0,0};
    const BYTE ropeFactory[]={0xE8,0x71,0x88,0xFB,0xFF};
    const BYTE ropeInit[]={0x55,0x8B,0xEC,0x53,0x8B,0x5D,8};
    grappleThrowProfile=Match(game+0x221437,ropeCharge,sizeof(ropeCharge))&&
        Match(game+0x22144A,ropeFactory,sizeof(ropeFactory))&&Match(game+0x1D9CC0,ropeInit,sizeof(ropeInit));
    Log("GRAPPLE128 native rope launch profile=%d",grappleThrowProfile?1:0);
    InstallGrenadeReleaseAudio();
    grenadeCookProfile=InstallGrenadeCooking();Log("COOK93 native event-duration profile=%d; frag cooking defaults off",grenadeCookProfile?1:0);
    const BYTE create[]={0x55,0x8B,0xEC,0x81,0xEC,0x5C,1,0,0};
    const BYTE thrown[]={0x55,0x8B,0xEC,0x81,0xEC,0,1,0,0};
    const BYTE start[]={0x55,0x8B,0xEC,0x51,0x53,0x56,0x8B,0xF1};
    const BYTE advance[]={0x53,0x56,0x8B,0xF1,0x80,0xBE,0x38,1,0,0,0};
    if(!Match(game+0x20FA90,advance,sizeof(advance))||Value<BYTE>(game,0xC4420)!=0xA1||Value<void*>(game,0xC4421)!=game+0x5A7428)return false;
    if(!Match(game+0x2213D0,create,sizeof(create))||!Match(game+0x210140,thrown,sizeof(thrown))||!Match(game+0x210400,start,sizeof(start)))return false;
    if(!Jump(game+0x2213D0,create,sizeof(create),(void*)&ProjectileCreateHook,(void**)&originalProjectileCreate))return false;
    if(!Jump(game+0x210140,thrown,sizeof(thrown),(void*)&ThrownCommitHook,(void**)&originalThrownCommit))return false;
    if(!Jump(game+0x210400,start,sizeof(start),(void*)&ThrownStartHook,(void**)&originalThrownStart))return false;
    return Jump(game+0x20FA90,advance,sizeof(advance),(void*)&ThrownAdvanceHook,(void**)&originalThrownAdvance);
}
