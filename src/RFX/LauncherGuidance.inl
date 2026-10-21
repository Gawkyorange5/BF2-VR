// BF2 1.5 wire guidance +20E3D0 calls +20D940 directly each native tick.
// r95 intercepted +20E0A0, which that update bypasses. Hook the shared
// ray consumer, with its BASE this pointer, only for the local held launcher.
// Original collision, range, wire break and projectile ownership stay native.
struct WireConnectionState {void* weapon=nullptr;ULONGLONG tick=0;bool live=false;} wireConnection;
SRWLOCK wireConnectionLock=SRWLOCK_INIT;
std::atomic<void*> wireLiveWeapon{nullptr};
bool LauncherConnected(void* weapon){
    AcquireSRWLockShared(&wireConnectionLock);const auto s=wireConnection;ReleaseSRWLockShared(&wireConnectionLock);
    return weapon&&s.weapon==weapon&&s.live&&GetTickCount64()-s.tick<150;
}
bool GuidanceReject(unsigned reason){
    static unsigned seen=0;const unsigned bit=1u<<reason;
    if(!(seen&bit)){seen|=bit;Log("GUIDANCE96 local ray retained native aim: guard=%u",reason);}return false;
}
bool WireLauncherName(void* weapon){
    char name[80]{};if(!ReloadName(weapon,name))return false;
    // Exact installed DZB2 definitions with LPTargetComp / TSWireGuided.
    // Their missile types, steering rates and connection lifetime stay native.
    for(const char* n:{"usatp_predator","chat_eryx","bf4_sraw","ch_pf98",
        "mohammed_at_metis_m","rsa_stiletto","at4_m136","bf4_pld","fow_zeus_aa"})
        if(!strcmp(name,n))return true;
    return false;
}
using TargetRay=void(__thiscall*)(void*,Vec,Vec);
TargetRay originalTargetRay=nullptr;
bool ControllerTargetRay(void* self,Vec& from,Vec& to){
    if(!self||Value<void*>(self)!=game+0x4F73F0||Value<void*>(self,0x10)!=game+0x4F7328)return false;
    void* weapon=Value<void*>(self,0x0C);
    AcquireSRWLockShared(&fireLock);
    if(!fire.wireLauncher||!weapon||fire.weapon!=weapon){ReleaseSRWLockShared(&fireLock);return false;}
    FireFrame pose=fire;ReleaseSRWLockShared(&fireLock);
    // Enum 3 is TSWireGuided (verified BF2 string/enum switch +1069C9).
    if(Value<int>(Value<void*>(self,0x5C),0x80)!=3)return GuidanceReject(0);
    if(!Config().hands||!referenceValid||!pose.gripBinding.valid||pose.owner!=Soldier()||
       Value<void*>(pose.weapon,0x28)!=pose.owner||Value<int>(pose.owner,0x1E0)!=pose.slot||
       Value<void*>(pose.owner,0x288)!=pose.rig||Value<int>(pose.owner,0x2B8)!=0||
       Value<void*>(pose.owner,0x254)||GetTickCount64()-pose.tick>150||
       !LocalAlive()||MenuActive()||InventoryStowed(pose.owner))return GuidanceReject(1);
    void* seat=Value<void*>(Value<void*>(Player(),0x80),4);
    if(seat&&seat!=pose.owner)return false;
    // Keep native zero/invalid ray handling, including guidance cancellation.
    const float distance=Length(Sub(to,from));
    if(!std::isfinite(from.x+from.y+from.z+to.x+to.y+to.z+distance)||distance<.001f||distance>100000.f)return false;
    Runtime::Frame sample{};DWORD process=0;GetWindowThreadProcessId(GetForegroundWindow(),&process);
    if(process!=GetCurrentProcessId()||!Runtime::Snapshot(sample)||!sample.focused||
       !sample.hands[1].aimValid||!sample.hands[1].gripValid||sample.origin!=pose.origin||
       sample.id<pose.frame||sample.id-pose.frame>2)return GuidanceReject(2);
    // Reuse the located frame and the accepted hand solver, including its
    // two-hand support and stock setting. Never sample OpenXR a second time.
    if(sample.id!=pose.frame){
        Mat body{},solved[70];bool supporting=false;
        if(!Read(pose.owner,0x88,body)||!Rigid(body))return false;
        const HandTargets targets=Targets(pose.native[0],body,sample,pose.rightPalm,pose.leftPalm,pose.palmsValid,pose.supporting);
        if(!SolveHands(pose.native,pose.gripBinding,targets,solved,supporting,&pose.aimState))return false;
        pose.trackedWeapon=Mul(solved[54],body);
    }
    if(!Rigid(pose.trackedWeapon))return false;
    Vec origin=Position(pose.trackedWeapon);const Vec direction=Unit(Row(pose.trackedWeapon,2));
    // DZB2 SRAW's native launch point is forward of the held receiver. Start
    // the guidance LOS at that same point rather than inside the soldier.
    char name[80]{};if(ReloadName(weapon,name)&&!strcmp(name,"bf4_sraw"))
        origin=Point({.17f,-.005f,.47691f},pose.trackedWeapon);
    if(!std::isfinite(origin.x+origin.y+origin.z+direction.x+direction.y+direction.z))return false;
    from=origin;to=Add(origin,Scale(direction,distance));
    return true;
}
#include "ApcGuidance.inl"
void __fastcall TargetRayHook(void* self,void*,Vec from,Vec to){
    const bool tracked=ControllerTargetRay(self,from,to);
    if(!tracked)ApcTargetRay(self,from,to);
    originalTargetRay(self,from,to);
    if(!tracked)return;
    const WireConnectionState next{Value<void*>(self,0x0C),GetTickCount64(),Value<void*>(Value<void*>(self,0x54),4)!=nullptr};
    AcquireSRWLockExclusive(&wireConnectionLock);
    const bool transition=wireConnection.weapon!=next.weapon||wireConnection.live!=next.live;
    wireConnection=next;ReleaseSRWLockExclusive(&wireConnectionLock);
    wireLiveWeapon.store(next.live?next.weapon:nullptr,std::memory_order_release);
    static unsigned reports=0;
    if(transition&&reports++<48){char name[80]{};ReloadName(next.weapon,name);
        Log("GUIDANCE127 weapon=%s controller ray accepted; native connection after LOS=%d",name,next.live?1:0);}
}
bool HoldWireAutomaticReload(void* self){
    // The physical sight supplies aim without BF2's right-mouse zoom hold.
    // Don't let empty-mag auto reload cancel a live wire missile. Explicit
    // reload, holster, collision, wire break and impact remain native.
    if(Value<void*>(self)!=game+0x4F5420)return false;
    const BYTE* base=static_cast<const BYTE*>(self)-0x10;
    if(Value<void*>(base)!=game+0x4F5500)return false;
    void* weapon=Value<void*>(base,0x0C);
    // Other soldiers reject before any lock, player lookup or name scan.
    if(!weapon||wireLiveWeapon.load(std::memory_order_acquire)!=weapon||!LauncherConnected(weapon))return false;
    void* owner=Soldier();
    if(!owner||Value<void*>(weapon,0x28)!=owner||!LocalAlive()||InventoryStowed(owner)||!Config().hands||MenuActive())return false;
    char name[80]{};if(!ReloadName(weapon,name)||strcmp(name,"bf4_sraw"))return false;
    void* seat=Value<void*>(Value<void*>(Player(),0x80),4);
    if(seat!=owner||Value<int>(owner,0x2B8)!=0||Value<void*>(owner,0x254))return false;
    AcquireSRWLockShared(&fireLock);
    const bool held=fire.owner==owner&&fire.weapon==weapon&&fire.wireLauncher&&fire.gripBinding.valid&&GetTickCount64()-fire.tick<150;
    ReleaseSRWLockShared(&fireLock);
    return held&&LauncherConnected(weapon);
}
bool InstallLauncherGuidance(){
    // Whole instructions only; no relative operands in the trampoline.
    const BYTE entry[]={0x55,0x8B,0xEC,0x81,0xEC,0xBC,0,0,0};
    if(Value<void*>(game+0x4F7328,0xA4)!=game+0x20E0A0)return false;
    return Jump(game+0x20D940,entry,sizeof(entry),reinterpret_cast<void*>(&TargetRayHook),reinterpret_cast<void**>(&originalTargetRay));
}
