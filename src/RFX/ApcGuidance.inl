// r133: only audited APC / IFV wire launchers. No laptop, aircraft, TV or
// heat-seeking weapon is admitted. Native target updates still own physics.
bool ApcTargetRay(void* self,Vec& from,Vec& to){
    if(!vehicleViewValid||!AimedSeat(vehicleSample.role)||vehicleSample.remoteCamera||
       !vehicleTick||GetTickCount64()-vehicleTick>150||!referenceValid||!Tuning().headAim)return false;
    if(!self||Value<void*>(self)!=game+0x4F73F0||Value<void*>(self,0x10)!=game+0x4F7328||
       Value<int>(Value<void*>(self,0x5C),0x80)!=3||!Value<void*>(Value<void*>(self,0x54),4))return false;
    void* weapon=Value<void*>(self,0x0C);if(!weapon)return false;
    // Walk only this launcher's parents to its FIRST PCO. Never authorize
    // another occupant's missile merely because it shares the same hull.
    void* owner=weapon;bool belongs=false;
    for(unsigned i=0;i<12;++i){
        void* parent=Value<void*>(owner,0x28);
        if(!parent||parent==owner||!VehicleObject(parent))return false;
        owner=parent;
        if(Value<void*>(owner)==game+0x4F2430){belongs=owner==vehicleSample.seat;break;}
    }
    if(!belongs)return false;
    char name[80]{};if(!VehicleTemplateName(Value<void*>(weapon,0x14),name))return false;
    struct Entry{const char* seat;const char* weapon;};static const Entry entries[]={
#include "ApcGuidanceProfiles.inl"
    };
    bool profile=false;
    for(const auto& entry:entries)if(!strcmp(name,entry.weapon)&&!strcmp(vehicleSample.name,entry.seat)){profile=true;break;}
    if(!profile||!LocalAlive()||MenuActive()||
       Value<void*>(Player(),0xCC)!=vehicleSample.weak||
       Value<void*>(Value<void*>(Player(),0x80),4)!=vehicleSample.seat)return false;
    const float distance=Length(Sub(to,from));
    if(!std::isfinite(from.x+from.y+from.z+to.x+to.y+to.z+distance)||distance<.001f||distance>100000.f)return false;
    Runtime::Frame sample{};DWORD process=0;GetWindowThreadProcessId(GetForegroundWindow(),&process);
    const auto now=GetTickCount64();
    if(process!=GetCurrentProcessId()||!Runtime::Snapshot(sample)||!sample.valid||!sample.focused||
       !sample.shouldRender||!sample.tick||now<sample.tick||now-sample.tick>150||
       sample.origin!=pairFrame.origin||!Rigid(sample.head))return false;
    // Use the same hull/stick/reference basis as the headset view, without
    // waiting for the slower native turret feedback servo to reach that aim.
    Mat head=sample.head;if(Tuning().gunnerPositionLock)SetPosition(head,Position(reference));
    const Mat aim=Compose(vehicleBase,reference,head,Config().units);
    if(!Rigid(aim))return false;
    const Vec target=Point({0,0,distance},aim),direction=Unit(Sub(target,from));
    if(Length(direction)<.9f)return false;
    // Keep the original muzzle origin and native ray length. The game's LOS,
    // obstruction, wire break, ammo, projectile steering and lifetime remain.
    to=Add(from,Scale(direction,distance));
    static void* reportedWeapon=nullptr;static ULONGLONG reportTick=0;
    if(reportedWeapon!=weapon||now-reportTick>5000){
        reportedWeapon=weapon;reportTick=now;
        Log("GUIDANCE133 APC seat=%s weapon=%s live wire target follows HMD; native steering retained",vehicleSample.name,name);
    }
    return true;
}
