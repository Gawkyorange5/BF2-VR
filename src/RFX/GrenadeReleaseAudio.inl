// BF2 1.5 DefaultSoundComp fire notification, interface at component+10.
// Stock grenade Fire1P's pin recording has an animation-length lead-in.
struct GrenadeCueState {void* owner=nullptr;void* identity=nullptr;void* weapon=nullptr;ULONGLONG until=0;} grenadeCue;
SRWLOCK grenadeCueLock=SRWLOCK_INIT;
using GrenadeSoundNotify=void(__thiscall*)(void*,const void*);
GrenadeSoundNotify originalGrenadeSound=nullptr;
bool grenadeSoundProfile=false;
void GrenadeReleaseCue(void* owner,void* weapon){
    char name[80]{};
    if(!grenadeSoundProfile||owner!=Soldier()||!LocalAlive()||!ReloadName(weapon,name)||strcmp(name,"ushgr_m67"))return;
    if(!ReloadAudio::GrenadePin())return; // Never mute stock audio without a replacement.
    AcquireSRWLockExclusive(&grenadeCueLock);
    grenadeCue={owner,Value<void*>(Player(),0xCC),weapon,GetTickCount64()+3000};
    ReleaseSRWLockExclusive(&grenadeCueLock);
    Log("PIN94 physical release: immediate stock pin transient; delayed fire cue suppressed");
}
void __fastcall GrenadeSoundHook(void* self,void*,const void* event){
    GrenadeCueState cue{};AcquireSRWLockShared(&grenadeCueLock);cue=grenadeCue;ReleaseSRWLockShared(&grenadeCueLock);
    if(cue.weapon&&GetTickCount64()<cue.until&&cue.owner==Soldier()&&LocalAlive()&&
       cue.identity==Value<void*>(Player(),0xCC)&&Value<void*>(self)==game+0x4F5F30&&
       Value<void*>(static_cast<BYTE*>(self)-0x10)==game+0x4F5FC8&&
       Value<void*>(static_cast<BYTE*>(self)-4)==cue.weapon&&
       (!Value<void*>(self,0x0C)||Value<void*>(self,0x0C)==cue.weapon)){
        // This notification only plays Fire1P/Outdoor/3P. Native thrown state,
        // ammo, detonation, impacts, voices and other soldiers are untouched.
        const unsigned delay=0;const BYTE released=0;
        if(Copy(static_cast<BYTE*>(self)+0x40,&delay,4)&&Copy(static_cast<BYTE*>(self)+0x48,&released,1))return;
    }
    originalGrenadeSound(self,event);
}
void InstallGrenadeReleaseAudio(){
    const BYTE entry[]={0x83,0xEC,0x10,0x56,0x8B,0xF1};
    const BYTE pin[]={0xB8,1,0,0,0,0x88,0x46,0x48};
    if(!ReloadAudio::Ready()||Value<void*>(game,0x4F5F30+14*4)!=game+0x208260||
       !Match(game+0x208416,pin,sizeof(pin)))return;
    grenadeSoundProfile=Jump(game+0x208260,entry,sizeof(entry),(void*)&GrenadeSoundHook,(void**)&originalGrenadeSound);
    Log("PIN94 native release audio profile=%d",grenadeSoundProfile?1:0);
}
