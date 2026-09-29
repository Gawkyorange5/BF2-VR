// Included after the verified local fire-pose cache and Targets(). No new
// engine hook or native collision ABI. Use the same tracked weapon +Z axis
// used by MapLaunch; random spread/gravity are deliberately not predicted.
bool IsAimedHandheld(void* weapon){
    char name[80]{};if(!ReloadName(weapon,name))return false;
    // Stock handheld naming covers rifles, SMGs, pistols, MGs, shotguns,
    // snipers and launchers, including expansion/unlock variants. Equipment
    // and hand-thrown grenades keep their own native HUD.
    for(const char* family:{"rif_","lmg_","pis_","sht_","sni_","rgl_","gbgr_","sasgr_","chat_","usatp_"})
        if(strstr(name,family))return true;
    return false;
}
void PrepareGunCrosshair(){
    gunCrosshairReady=hideFixedInfantryCrosshair=false;
    const auto tuning=Tuning();
    if(crosshairProfile&&!tuning.gunCrosshair){
        // r45: explicit OFF means no infantry aiming marker, even when a
        // tracked hand/weapon pose is temporarily unavailable. Resolve only
        // this pair's local soldier; never carry suppression into a vehicle.
        // BeginPair already excludes menus, deployment and dead players.
        void* owner=Soldier();void* seat=Value<void*>(Value<void*>(Player(),0x80),4);
        hideFixedInfantryCrosshair=owner&&(!seat||seat==owner)&&
            Value<int>(owner,0x2B8)==0&&!Value<void*>(owner,0x254);
        return;
    }
    if(!crosshairProfile||!gunCrosshairAvailable||!tuning.gunCrosshair||!Config().hands||!pairFrame.focused||
       !referenceValid||!pairFrame.hands[1].gripValid||!pairFrame.hands[1].aimValid)return;
    void* owner=Soldier();void* seat=Value<void*>(Value<void*>(Player(),0x80),4);
    if(!owner||(seat&&seat!=owner)||Value<int>(owner,0x2B8)!=0||Value<void*>(owner,0x254))return;
    FireFrame pose{};AcquireSRWLockShared(&fireLock);pose=fire;ReleaseSRWLockShared(&fireLock);
    if(pose.owner!=owner||!pose.weapon||Value<void*>(pose.weapon,0x28)!=owner||
       Value<void*>(owner,0x288)!=pose.rig||Value<int>(owner,0x1E0)!=pose.slot||
       pose.origin!=pairFrame.origin||pose.frame>pairFrame.id||pairFrame.id-pose.frame>1||
       GetTickCount64()-pose.tick>150||!pose.gripBinding.valid)return;
    // Classification is tiny, read-only and independent of manual reload
    // support. A new weapon can show an aim marker before its first shot.
    if(!IsAimedHandheld(pose.weapon))return;
    if(pose.frame!=pairFrame.id){
        Mat body{},solved[70];bool supporting=false;
        if(!Read(owner,0x88,body)||!Rigid(body))return;
        const HandTargets targets=Targets(pose.native[0],body,pairFrame,pose.rightPalm,pose.leftPalm,pose.palmsValid,pose.supporting);
        if(!SolveHands(pose.native,pose.gripBinding,targets,solved,supporting,&pose.aimState))return;
        pose.trackedWeapon=Mul(solved[54],body);
    }
    if(!Rigid(pose.trackedWeapon))return;
    // One common point for both eyes. Projection uses each eye's actual
    // asymmetric lens, not the head-centered HUD canvas or a mouse cursor.
    gunCrosshairPoint=Point({0,0,tuning.gunCrosshairDistance*Config().units},pose.trackedWeapon);
    gunCrosshairRadius=std::tan(tuning.gunCrosshairDegrees*.00872664626f);
    gunCrosshairReady=true;
    static bool reported=false;
    if(!reported){reported=true;Log("AIM38 tracked gun crosshair active: %.1fm direction marker, %.2fdeg; shared point/per-eye projection; native infantry marker hidden",tuning.gunCrosshairDistance,tuning.gunCrosshairDegrees);}
}
