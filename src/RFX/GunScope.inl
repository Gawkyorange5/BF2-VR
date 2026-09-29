// Physical aperture/eye-relief approach adapted from BF2142VR GunOptics (MIT).
// BF2 camera layouts are supplied by this adapter; no BF2142 offsets are used.
struct ScopeProfile {const char* name;Vec center;float radius;};
const ScopeProfile scopeProfiles[]={
    // Measured stock geom0/LOD0 rear glass; inset inside the metal bezel.
    {"usrif_m24",{.00022f,.06772f,-.2485f},.0150f},
    {"gbrif_l96a1",{-.00099f,.08969f,-.2738f},.0103f},
    {"ussni_m95_barret",{.000005f,.09047f,-.1796f},.0169f},
    {"rurif_dragunov",{.00308f,.07962f,-.3025f},.0128f},
    {"chsni_type88",{.00027f,.09587f,-.1748f},.0116f}
};
ScopeView scopeView{};bool scopeReady=false,scopeAvailable=true;
void PrepareGunScope(){
    scopeReady=false;scopeView={};
    if(!Tuning().gunScopes||!scopeAvailable||!Config().hands||!pairFrame.focused||!referenceValid||
       !pairFrame.hands[1].gripValid||!pairFrame.hands[1].aimValid)return;
    void* owner=Soldier();void* seat=Value<void*>(Value<void*>(Player(),0x80),4);
    if(!owner||(seat&&seat!=owner)||Value<int>(owner,0x2B8)!=0||Value<void*>(owner,0x254))return;
    char name[80]{};if(!handWeapon||!ReloadName(handWeapon,name))return;
    const ScopeProfile* profile=nullptr;
    for(const auto& p:scopeProfiles)if(!strcmp(name,p.name)){profile=&p;break;}
    if(!profile)return;
    FireFrame pose{};AcquireSRWLockShared(&fireLock);pose=fire;ReleaseSRWLockShared(&fireLock);
    if(pose.owner!=owner||pose.weapon!=handWeapon||Value<void*>(pose.weapon,0x28)!=owner||
       Value<void*>(owner,0x288)!=pose.rig||Value<int>(owner,0x1E0)!=pose.slot||
       pose.origin!=pairFrame.origin||pose.frame>pairFrame.id||pairFrame.id-pose.frame>1||
       GetTickCount64()-pose.tick>150||!pose.gripBinding.valid)return;
    if(pose.frame!=pairFrame.id){
        Mat body{},solved[70];bool supporting=false;
        if(!Read(owner,0x88,body)||!Rigid(body))return;
        const HandTargets targets=Targets(pose.native[0],body,pairFrame,pose.rightPalm,pose.leftPalm,pose.palmsValid,pose.supporting);
        if(!SolveHands(pose.native,pose.gripBinding,targets,solved,supporting,&pose.aimState))return;
        pose.trackedWeapon=Mul(solved[54],body);
    }
    if(!Rigid(pose.trackedWeapon))return;
    scopeView.gun=pose.trackedWeapon;scopeView.center=profile->center;scopeView.radius=profile->radius;
    scopeView.magnification=Tuning().scopeMagnification;
    const Mat inverse=Inverse(scopeView.gun);float best=0;Vec eyes[2]{};
    for(int i=0;i<2;++i){
        const Mat eye=Compose(TrackingBase(views[0].source),reference,pairFrame.eyes[i],Config().units);
        const Vec e=Point(Position(eye),inverse),forward=Direction(Row(eye,2),inverse);
        eyes[i]=e;const float relief=profile->center.z-e.z;
        if(relief<.035f||relief>.48f||forward.z<.88f)continue;
        const float radial=std::hypot(e.x-profile->center.x,e.y-profile->center.y)/(profile->radius+.014f);
        const auto clamp=[](float v){return std::max(0.f,std::min(1.f,v));};
        const float visibility=clamp((1.2f-radial)*3.f)*clamp((relief-.035f)/.025f)*clamp((.48f-relief)/.10f);
        scopeView.visibility[i]=visibility;
        if(visibility>best){best=visibility;scopeView.relief=relief;}
    }
    if(best<.02f)return;
    // r40 captured only the centered aperture's image. Eye relief/offset
    // moves its UVs outside that image, exposing a hard rectangular boundary.
    // Bound the ENTIRE lens for every visible eye, using the same finite
    // bore intersection and relief mapping as DrawGunScope. Widen only the
    // capture, then divide its UV displacement by this factor: magnification
    // and physical aperture size stay constant. One shared image, same target.
    for(int i=0;i<2;++i){
        if(scopeView.visibility[i]<.02f)continue;
        const Vec e=eyes[i];const float relief=profile->center.z-e.z;
        const Vec hit=Add(e,Scale(Sub({0,0,100},e),relief/(100-e.z)));
        const float offset=std::max(std::abs(profile->center.x-hit.x),std::abs(profile->center.y-hit.y))/profile->radius;
        const float required=(1+offset)*scopeView.relief/relief;
        if(!std::isfinite(required)||required<=0)return;
        scopeView.coverage=std::max(scopeView.coverage,required);
    }
    scopeView.coverage*=1.03f; // sampling/roundoff margin outside the round lens
    // Visual 100m bore zero, shared by the telescope image and its reticle.
    // Native projectile spread, drop and launch hooks are unchanged.
    const Vec origin=Add(profile->center,{0,0,.025f});
    const Vec z=Unit(Sub({0,0,100},origin)),x=Unit(Cross({0,1,0},z)),y=Cross(z,x);
    Mat local=Identity();const Vec rows[]={x,y,z};
    for(int i=0;i<3;++i){local.m[i][0]=rows[i].x;local.m[i][1]=rows[i].y;local.m[i][2]=rows[i].z;}
    SetPosition(local,origin);scopeView.camera=Mul(local,scopeView.gun);
    scopeView.tangent=profile->radius*scopeView.coverage/(scopeView.relief*scopeView.magnification);
    scopeReady=Rigid(scopeView.camera)&&std::isfinite(scopeView.tangent)&&scopeView.tangent>.001f&&scopeView.tangent<=1.f;
    static const ScopeProfile* last=nullptr;
    if(scopeReady&&last!=profile){last=profile;Log("SCOPE41 physical optic=%s zoom=%.1fx radius=%.4f relief=%.4f tangent=%.6f coverage=%.3f; full circular aperture, one GPU-only replay",profile->name,scopeView.magnification,profile->radius,scopeView.relief,scopeView.tangent,scopeView.coverage);}
}
