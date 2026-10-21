// Only affects the overlay; controller, gun and firing poses stay untouched.
// A little exit hysteresis prevents a blinking marker at the sight boundary.
struct SightCrosshairState {void* weapon=nullptr;std::uint64_t origin=0;ULONGLONG tick=0,lastAimed=0;float sensitivity=0;bool aimed=false,shotgun=false;} sightCrosshair;
bool CrosshairAtSights(const FireFrame& pose,float sensitivity){
    auto& s=sightCrosshair;const auto now=pairFrame.tick;
    if(s.weapon!=pose.weapon||s.origin!=pairFrame.origin||now<s.tick||now-s.tick>150||s.sensitivity!=sensitivity){
        s={};s.weapon=pose.weapon;s.origin=pairFrame.origin;s.sensitivity=sensitivity;
        char name[80]{};s.shotgun=ReloadName(pose.weapon,name)&&(strstr(name,"sht_")!=nullptr||
            !strcmp(name,"gbrif_benelli_m4")||!strcmp(name,"usrif_remington11-87"));
    }
    s.tick=now;if(s.shotgun){s.aimed=false;s.lastAimed=0;return false;}bool aligned=false;
    const Mat inverse=Inverse(pose.trackedWeapon);
    const float unit=Config().units;
    if(unit<=0||!std::isfinite(sensitivity))return false;
    const float amount=std::max(.25f,std::min(1.5f,sensitivity*.01f));
    // Lower settings require closer, more precisely aligned sights. Keep the
    // authored vertical sight-height allowance across different gun models.
    // At 100% the entry/exit thresholds are exactly the original r84 values.
    const float maximumDistance=.95f*amount,lateralTolerance=.065f*amount;
    const float entryForward=1.f-.035f*amount*amount,exitForward=1.f-.06f*amount*amount;
    for(int i=0;i<2;++i){
        const Mat eye=Compose(TrackingBase(views[0].source),reference,pairFrame.eyes[i],unit);
        const Vec p=Scale(Point(Position(eye),inverse),1.f/unit);
        const Vec forward=Direction(Row(eye,2),inverse);
        // Native handheld +Z is the bore. Eyes sit just above it and behind
        // the receiver when looking through irons, including extended pistols.
        const float margin=s.aimed?.025f*std::min(1.f,amount):0.f;
        if(forward.z>(s.aimed?exitForward:entryForward)&&std::abs(p.x)<lateralTolerance+margin&&
           p.y>-.015f-margin&&p.y<.16f+margin&&p.z>-maximumDistance-margin&&p.z<-.025f+margin)
            aligned=true;
    }
    if(aligned){s.aimed=true;s.lastAimed=now;}
    else if(!s.lastAimed||now-s.lastAimed>100)s.aimed=false;
    return s.aimed;
}
// Included after the verified local fire-pose cache and Targets(). No new
// engine hook or native collision ABI. Use the same tracked weapon +Z axis
// used by MapLaunch; random spread/gravity are deliberately not predicted.
bool CustomAimedHandheld(const char* name);
bool IsAimedHandheld(void* weapon){
    // Reclassify only on item/template changes. Custom gun names do not use
    // BF2's rif_/sht_ prefixes, but their audited reload/optic profiles do.
    static void* previous=nullptr;static void* definition=nullptr;static bool aimed=false;
    void* def=Value<void*>(weapon,0x14);
    if(weapon==previous&&def==definition)return aimed;
    previous=weapon;definition=def;aimed=false;
    char name[80]{};if(!ReloadName(weapon,name))return false;
    if(FindReloadWeapon(name)||CustomAimedHandheld(name))return aimed=true;
    for(const char* n:{"aix_portableminigun","aix_portableminigun_mec","emzpj_terror4000_minigun","p220"})
        if(!strcmp(name,n))return aimed=true;
    // Stock handheld naming covers rifles, SMGs, pistols, MGs, shotguns,
    // snipers and launchers, including expansion/unlock variants. Equipment
    // and hand-thrown grenades have no replacement aiming marker.
    for(const char* family:{"rif_","lmg_","pis_","sht_","sni_","rgl_","gbgr_","sasgr_","chat_","usatp_"})
        if(strstr(name,family))return aimed=true;
    return false;
}
void PrepareGunCrosshair(){
    gunCrosshairReady=gunCrosshairAiming=hideFixedInfantryCrosshair=false;gunHitSampled=false;gunHitAlpha=0;
    const auto tuning=Tuning();if(!LocalAlive())return;
    if(crosshairProfile){
        // Hide the fixed infantry marker independently of weapon type and
        // replacement availability: knives, grenades, gadgets and weapon
        // switches must not bring the original screen-centred marker back.
        void* owner=Soldier();void* seat=Value<void*>(Value<void*>(Player(),0x80),4);
        hideFixedInfantryCrosshair=owner&&(!seat||seat==owner||PassengerWeaponSeat(owner))&&
            Value<int>(owner,0x2B8)==0&&!Value<void*>(owner,0x254);
    }
    if(InventoryStowed(Soldier()))return;
    if(!crosshairProfile||!gunCrosshairAvailable||!tuning.gunCrosshair||!Config().hands||!pairFrame.focused||
       !referenceValid||!pairFrame.hands[1].gripValid||!pairFrame.hands[1].aimValid)return;
    void* owner=Soldier();void* seat=Value<void*>(Value<void*>(Player(),0x80),4);
    if(!owner||(seat&&seat!=owner&&!PassengerWeaponSeat(owner))||Value<int>(owner,0x2B8)!=0||Value<void*>(owner,0x254))return;
    FireFrame pose{};AcquireSRWLockShared(&fireLock);pose=fire;ReleaseSRWLockShared(&fireLock);
    if(pose.owner!=owner||!pose.weapon||Value<void*>(pose.weapon,0x28)!=owner||
       Value<void*>(owner,0x288)!=pose.rig||Value<int>(owner,0x1E0)!=pose.slot||
       pose.origin!=pairFrame.origin||pose.frame>pairFrame.id||pairFrame.id-pose.frame>1||
       GetTickCount64()-pose.tick>150||!pose.gripBinding.valid)return;
    // Classification is tiny, read-only and independent of manual reload
    // support. A new weapon can show an aim marker before its first shot.
    if(!IsAimedHandheld(pose.weapon))return;
    if(!Rigid(pose.camera))return;
    Mat poseBase=pose.trackingBase;
    if(pose.frame!=pairFrame.id){
        Mat body{},solved[70];bool supporting=false;
        if(!Read(owner,0x88,body)||!Rigid(body))return;
        const HandTargets targets=Targets(pose.native[0],body,pairFrame,pose.rightPalm,pose.leftPalm,pose.palmsValid,pose.supporting);
        if(!SolveHands(pose.native,pose.gripBinding,targets,solved,supporting,&pose.aimState))return;
        pose.trackedWeapon=Mul(solved[54],body);
        // Targets levels this exact camera/body combination. Do not use the
        // older cached world camera when the body has advanced since capture.
        poseBase=Compose(HandPresentationBase(Mul(pose.native[0],body),pairFrame),reference,Identity(),Config().units);
    }
    if(!Rigid(pose.trackedWeapon))return;
    // The OpenXR sample ID only timestamps tracking, not BF2's native camera.
    // Stick turning can advance that camera after the local hand finalizer,
    // including while the sample ID still matches. Carry the solved weapon's
    // tracking-relative pose into the same current base used by BOTH eyes.
    // Otherwise old weapon yaw minus new eye yaw makes the marker jump during
    // smooth turns. This is a visual copy; never mutate the firing/IK cache.
    // Include the reference used at solve time. HMD-directed locomotion can
    // rotate that reference between the native pose and the stereo pair.
    const Mat renderBase=Compose(TrackingBase(views[0].source),reference,Identity(),Config().units);
    if(!Rigid(poseBase)||!Rigid(renderBase))return;
    pose.trackedWeapon=Mul(Mul(pose.trackedWeapon,Inverse(poseBase)),renderBase);
    if(!Rigid(pose.trackedWeapon))return;
    // One common point for both eyes. Projection uses each eye's actual
    // asymmetric lens, not the head-centered HUD canvas or a mouse cursor.
    gunCrosshairPoint=Point({0,0,tuning.gunCrosshairDistance*Config().units},pose.trackedWeapon);
    gunCrosshairRadius=std::tan(tuning.gunCrosshairDegrees*.00872664626f);
    gunCrosshairReady=true;
    gunCrosshairAiming=tuning.hideCrosshairAtSights&&CrosshairAtSights(pose,tuning.crosshairHideSensitivity);
    static bool reported=false;
    if(!reported){reported=true;Log("AIM87 tracked gun crosshair active: %.1fm direction marker, %.2fdeg; cached pose rebased to current stereo camera for both tracking-age paths; shared point/per-eye projection; native infantry marker hidden",tuning.gunCrosshairDistance,tuning.gunCrosshairDegrees);}
}
