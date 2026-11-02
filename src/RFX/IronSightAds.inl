// Use the same presented weapon frame as the accepted crosshair/scope paths.
// Never write the firing/IK pose, and never use a previous game's world pose
// directly against current eyes during locomotion or smooth turning.
bool PrepareIronSightAds(void*& weapon,float& alignment){
    weapon=nullptr;alignment=0.f;
    if(!pairFrame.hands[1].gripValid||!pairFrame.hands[1].aimValid)return false;
    char name[80]{};
    if(!handWeapon||!ReloadName(handWeapon,name)||FindScopeProfile(name)||!IsAimedHandheld(handWeapon))return false;
    void* owner=Soldier();void* identity=Value<void*>(Player(),0xCC);
    if(!CurrentScopeWeapon(handWeapon,owner,identity))return false;
    FireFrame pose{};AcquireSRWLockShared(&fireLock);pose=fire;ReleaseSRWLockShared(&fireLock);
    if(pose.owner!=owner||pose.weapon!=handWeapon||pose.identity!=identity||pose.emptyHands||
       Value<void*>(owner,0x288)!=pose.rig||Value<int>(owner,0x1E0)!=pose.slot||
       pose.origin!=pairFrame.origin||pose.frame>pairFrame.id||pairFrame.id-pose.frame>1||
       GetTickCount64()-pose.tick>150||!pose.gripBinding.valid||!Rigid(pose.camera))return false;
    Mat poseBase=pose.trackingBase;
    if(pose.frame!=pairFrame.id){
        Mat body{},solved[70];bool supporting=false;
        if(!Read(owner,0x88,body)||!Rigid(body))return false;
        const HandTargets targets=Targets(pose.native[0],body,pairFrame,pose.rightPalm,pose.leftPalm,pose.palmsValid,pose.supporting);
        if(!SolveHands(pose.native,pose.gripBinding,targets,solved,supporting,&pose.aimState))return false;
        pose.trackedWeapon=Mul(solved[54],body);
        poseBase=Compose(HandPresentationBase(Mul(pose.native[0],body),pairFrame),reference,Identity(),Config().units);
    }
    const float units=Config().units;
    if(!std::isfinite(units)||units<=0)return false;
    const Mat renderBase=Compose(TrackingBase(views[0].source),reference,Identity(),units);
    if(!Rigid(pose.trackedWeapon)||!Rigid(poseBase)||!Rigid(renderBase))return false;
    pose.trackedWeapon=Mul(Mul(pose.trackedWeapon,Inverse(poseBase)),renderBase);
    if(!Rigid(pose.trackedWeapon))return false;
    const Mat inverse=Inverse(pose.trackedWeapon);
    for(int i=0;i<2;++i){
        const Mat eye=Compose(TrackingBase(views[0].source),reference,pairFrame.eyes[i],units);
        if(!Rigid(eye))continue;
        const Vec p=Scale(Point(Position(eye),inverse),1.f/units);
        const Vec direction=Direction(Row(eye,2),inverse);
        alignment=std::max(alignment,IronSightAlignment(p.x,p.y,p.z,direction.z));
    }
    weapon=pose.weapon;return true;
}
