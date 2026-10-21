// Native C4 retains its placement, ammo, detonations and multiplayer events.
// Controller triggers choose its stock mode before sending normal primary fire.
bool c4Profile=false;
bool C4ReleasePending(void* weapon,bool& press);
void C4ReleaseStarted(void* weapon);
bool PhysicalC4Name(const char* name){return !strcmp(name,"c4_explosives")||!strcmp(name,"c4_ex");}
bool IsPhysicalC4(void* weapon){char name[80]{};return ReloadName(weapon,name)&&PhysicalC4Name(name);}
bool Dzb2C4(void* weapon){char name[80]{};return ReloadName(weapon,name)&&!strcmp(name,"c4_ex");}
const Mat& C4ChargeOffset(void* weapon){return Dzb2C4(weapon)?dzC4ChargeFromPalm:c4ChargeFromPalm;}
struct C4Command {
    void* owner=nullptr;void* weapon=nullptr;void* identity=nullptr;std::uint64_t origin=0;
    ULONGLONG tick=0,requested=0,pulseUntil=0;
    int desired=0,mode=0,pulseMode=0;bool leftWas=false,rightWas=false,fire=false,detonator=false,rightHeld=false,enabled=false,releaseQueued=false;
} c4Command;
SRWLOCK c4Lock=SRWLOCK_INIT;
PVOID volatile c4WeaponPublished=nullptr;
using C4Update=void(__thiscall*)(void*,int,int);C4Update originalC4Update=nullptr;
using C4Start=int(__thiscall*)(void*,int,int);C4Start originalC4Start=nullptr;
void ResetC4(){
    AcquireSRWLockExclusive(&c4Lock);InterlockedExchangePointer(&c4WeaponPublished,nullptr);c4Command={};ReleaseSRWLockExclusive(&c4Lock);
}
bool C4Owned(const C4Command& s){
    const auto now=GetTickCount64();void* owner=s.owner;
    return owner&&s.weapon&&owner==Soldier()&&LocalAlive()&&!MenuActive()&&s.enabled&&s.identity==Value<void*>(Player(),0xCC)&&
        !Value<void*>(owner,0x254)&&Value<int>(owner,0x2B8)==0&&
        InventoryItem(owner,Value<int>(owner,0x1E0))==s.weapon&&Value<void*>(s.weapon,0x28)==owner&&
        now>=s.tick&&now-s.tick<150;
}
void SuspendC4(){
    AcquireSRWLockExclusive(&c4Lock);c4Command.enabled=false;c4Command.desired=0;c4Command.fire=false;c4Command.pulseUntil=0;
    ReleaseSRWLockExclusive(&c4Lock);
}
bool C4DetonatorVisible(void* owner,void* weapon){
    AcquireSRWLockShared(&c4Lock);const auto s=c4Command;ReleaseSRWLockShared(&c4Lock);
    return s.owner==owner&&s.weapon==weapon&&s.identity==Value<void*>(Player(),0xCC)&&s.detonator&&
        !(inventoryHands.pendingWeapon&&inventoryHands.pendingWeapon!=weapon);
}
void C4Holstered(void* weapon){
    AcquireSRWLockExclusive(&c4Lock);
    if(c4Command.weapon==weapon){c4Command.detonator=false;c4Command.desired=0;c4Command.pulseUntil=0;c4Command.fire=false;}
    ReleaseSRWLockExclusive(&c4Lock);
}
bool UpdateC4(const Runtime::Frame& f,bool enabled,bool left,bool right,bool& down){
    down=false;void* owner=Soldier();void* weapon=InventoryItem(owner,Value<int>(owner,0x1E0));
    if(!c4Profile||!LocalAlive()||Value<void*>(owner,0x254)||!IsPhysicalC4(weapon)||
       (inventoryHands.pendingWeapon&&inventoryHands.pendingWeapon!=weapon)){ResetC4();return false;}
    const bool held=!InventoryStowed(owner);bool releasePress=false;
    const bool released=C4ReleasePending(weapon,releasePress);
    AcquireSRWLockExclusive(&c4Lock);auto& s=c4Command;
    if(s.owner!=owner||s.weapon!=weapon||s.identity!=Value<void*>(Player(),0xCC)){
        s={};s.owner=owner;s.weapon=weapon;s.identity=Value<void*>(Player(),0xCC);
        s.detonator=held;s.leftWas=left;s.rightWas=right;
    }
    if(held&&!s.rightHeld)s.detonator=true;
    s.rightHeld=held;s.origin=f.origin;s.tick=f.tick;
    s.enabled=enabled&&f.valid&&f.focused&&f.hands[0].gripValid&&f.hands[1].gripValid&&!MenuActive();
    if(!s.enabled){s.desired=0;s.pulseUntil=0;s.fire=false;s.leftWas=left;s.rightWas=right;}
    else if(released){
        // A grip release owns the placement request until BF2 accepts it.
        // Never let a left-trigger mode switch turn that throw into detonation.
        s.releaseQueued=true;s.detonator=true;s.desired=1;s.requested=f.tick;s.pulseMode=1;
        s.fire=releasePress&&s.mode==1;s.pulseUntil=0;
    }else{
        if(s.releaseQueued){s.releaseQueued=false;s.desired=0;s.pulseUntil=0;s.fire=false;}
        if((left&&!s.leftWas&&s.detonator)||(right&&!s.rightWas&&held)){
            s.desired=left?2:1;s.requested=f.tick;s.pulseUntil=0;
            Log("C454 requested %s",s.desired==2?"left detonator":"right charge");
        }
        if(s.desired&&f.tick-s.requested>=1500){s.desired=0;Log("C454 native mode wait expired");}
        if(s.desired&&s.mode==s.desired){s.pulseMode=s.desired;s.desired=0;s.pulseUntil=f.tick+120;}
        s.fire=f.tick<s.pulseUntil;
    }
    s.leftWas=left;s.rightWas=right;down=s.fire;
    InterlockedExchangePointer(&c4WeaponPublished,weapon);
    ReleaseSRWLockExclusive(&c4Lock);return true;
}
bool C4FirePolicy(void* owner,float& primary,float& secondary){
    AcquireSRWLockShared(&c4Lock);const auto s=c4Command;ReleaseSRWLockShared(&c4Lock);
    if(s.owner!=owner||!C4Owned(s))return false;
    primary=s.fire?1.f:0.f;secondary=0;return true;
}
void __fastcall C4UpdateHook(void* self,void*,int a,int b){
    void* component=static_cast<BYTE*>(self)-0x10;void* weapon=Value<void*>(component,0x0C);
    if(weapon!=InterlockedCompareExchangePointer(&c4WeaponPublished,nullptr,nullptr)||Value<void*>(self)!=game+0x4F7EE0){originalC4Update(self,a,b);return;}
    AcquireSRWLockShared(&c4Lock);const auto s=c4Command;ReleaseSRWLockShared(&c4Lock);
    if(!C4Owned(s)||s.weapon!=weapon){originalC4Update(self,a,b);return;}
    const int current=Value<int>(self,0x134),pending=Value<int>(self,0x138);
    if(s.desired>=1&&s.desired<=2&&current!=s.desired&&pending!=s.desired)
        reinterpret_cast<void(__thiscall*)(void*,int)>(game+0x2108B0)(component,s.desired);
    const bool detonating=Value<BYTE>(self,0x158)!=0;
    originalC4Update(self,a,b);
    const bool detonated=detonating&&!Value<BYTE>(self,0x158);
    const int mode=Value<int>(self,0x134);
    AcquireSRWLockExclusive(&c4Lock);
    if(c4Command.weapon==weapon&&c4Command.origin==s.origin){
        c4Command.mode=mode;
        if(detonated){c4Command.detonator=false;c4Command.fire=false;c4Command.desired=0;c4Command.pulseUntil=0;Log("C454 native detonation completed; left device stowed");}
    }
    ReleaseSRWLockExclusive(&c4Lock);
}
int __fastcall C4StartHook(void* self,void*,int a,int b){
    void* weapon=Value<void*>(static_cast<BYTE*>(self)-0x10,0x0C);
    if(weapon!=InterlockedCompareExchangePointer(&c4WeaponPublished,nullptr,nullptr)||Value<void*>(self)!=game+0x4F7EE0)return originalC4Start(self,a,b);
    AcquireSRWLockShared(&c4Lock);const auto s=c4Command;ReleaseSRWLockShared(&c4Lock);
    if(s.weapon!=weapon||!C4Owned(s))return originalC4Start(self,a,b);
    bool releasePress=false;
    if(C4ReleasePending(weapon,releasePress)){
        if(!releasePress||Value<int>(self,0x134)!=1)return 0;
        const int result=originalC4Start(self,a,b);
        // A true result can mean only that the 50ms native launch delay began.
        // The subsequent call performs the actual projectile/ammo operation.
        // Keep the owned request alive until ProjectileCreate acknowledges it.
        if(result)C4ReleaseStarted(weapon);
        return result;
    }
    // Native detonation schedules an automatic switch back to placement.
    // Never let a remaining left-trigger pulse place a charge after that switch.
    if(!s.fire||GetTickCount64()>=s.pulseUntil||Value<int>(self,0x134)!=s.pulseMode)return 0;
    return originalC4Start(self,a,b);
}
bool InstallPhysicalC4(){
    const BYTE update[]={0x55,0x8B,0xEC,0x83,0xEC,8};
    const BYTE start[]={0x55,0x8B,0xEC,0x51,0x53,0x33,0xDB,0x56};
    const BYTE mode[]={0x55,0x8B,0xEC,0x8B,0x45,8,0x56,0x8B,0xF1,0x89,0x86,0x48,1,0,0};
    if(Value<void*>(game,0x4F7EE0+6*4)!=game+0x2110B0||Value<void*>(game,0x4F7EE0+35*4)!=game+0x2111F0||
       !Match(game+0x2108B0,mode,sizeof(mode))||!Match(game+0x2111F0,start,sizeof(start)))return false;
    if(!Jump(game+0x2110B0,update,sizeof(update),(void*)&C4UpdateHook,(void**)&originalC4Update))return false;
    return Jump(game+0x2111F0,start,sizeof(start),(void*)&C4StartHook,(void**)&originalC4Start);
}
