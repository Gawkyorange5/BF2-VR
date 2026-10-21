// Physical aperture/eye-relief approach adapted from BF2142VR GunOptics (MIT).
// BF2 camera layouts are supplied by this adapter; no BF2142 offsets are used.
// kind: 0 retains the accepted sniper projection, 1 magnified optic,
// 2 unmagnified reflex sight, 3 native heat-seeker sight. New lenses use their own aperture frame;
// the ERYX's angled eyepiece redirects the view along the weapon bore.
struct ScopeProfile {const char* name;Vec center;float radius;int kind=0;Vec up{0,1,0};const ScopeGeometry* geometry=nullptr;int nativeHud=0;bool connectionRing=false;};
const ScopeProfile scopeProfiles[]={
    // Measured stock geom0/LOD0 rear glass; inset inside the metal bezel.
    {"usrif_m24",{.00022f,.06772f,-.2485f},.0150f},
    {"gbrif_l96a1",{-.00099f,.08969f,-.2738f},.0103f},
    {"ussni_m95_barret",{.000005f,.09047f,-.1796f},.0169f},
    {"rurif_dragunov",{.00308f,.07962f,-.3025f},.0128f},
    {"chsni_type88",{.00027f,.09587f,-.1748f},.0116f},
    {"usatp_predator",{-.08174f,.08167f,.0805f},.0135f,1},
    {"chat_eryx",{-.18095f,.02273f,.08658f},.0153f,1,{0,.8728f,.4881f}},
    {"insgr_rpg",{-.01097f,.10508f,-.1535f},.0160f,1},
    {"usrif_sa80",{0,.10031f,-.1199f},.0180f,1},
    {"sasrif_fn2000",{-.00895f,.11893f,-.0886f},.0113f,1},
    {"usrif_m4",{0,.08225f,-.0163f},.0125f,2},
    {"rurrif_ak74u",{0,.09165f,.0284f},.0103f,2},
    {"usrif_fnscarl",{-.00005f,.09656f,-.1022f},.0103f,2},
    {"usrif_fnscarh",{-.00005f,.09747f,-.1021f},.0103f,2},
    {"eurif_fnp90",{0,.09176f,.0982f},.0043f,2},
    // DZB2 alternate compact AK uses the identical accepted AK74U optic mesh.
    {"rurrif_ak74u_b",{0,.09165f,.0284f},.0103f,2},
    // DZB2 FIM92: rear circular glass from geom0/LOD0 material jingzi.
    {"emzpj_fim92_stinger",{-.098746f,.110406f,-.0800f},.0120f,3}
};
#include "Dzb2ScopeProfiles.inl"
#include "LauncherScopeProfiles.inl"
bool CustomAimedHandheld(const char* name){
    return Dzb2ScopeProfile(name)||LauncherScopeProfile(name);
}
void ReadLauncherHudRaw(bool& heat,bool& clip){
    void* layer=Class("HudInformationLayer");if(Value<void*>(layer)!=game+0x49E968)return;
    auto* typed=static_cast<DICE::detail::CHudInformationLayer*>(layer);
    DICE::std::string heatName("PrimaryHeatBlink"),clipName("PrimaryClipBlink");
    heat=Value<BYTE>(typed->getBoolByName(&heatName))==1;
    clip=Value<BYTE>(typed->getBoolByName(&clipName))==1;
}
void ReadLauncherHud(bool& heat,bool& clip){__try{ReadLauncherHudRaw(heat,clip);}__except(EXCEPTION_EXECUTE_HANDLER){heat=clip=false;}}
ScopeView scopeView{};bool scopeReady=false,scopeAvailable=true;
void* scopeWeapon=nullptr;
void PrepareGunScope(){
    scopeWeapon=nullptr;
    if(!LocalAlive()){scopeReady=false;return;}
    scopeReady=false;scopeView={};
    if(InventoryStowed(Soldier())||!Tuning().gunScopes||!scopeAvailable||!Config().hands||!pairFrame.focused||!referenceValid||
       !pairFrame.hands[1].gripValid||!pairFrame.hands[1].aimValid)return;
    void* owner=Soldier();void* seat=Value<void*>(Value<void*>(Player(),0x80),4);
    if(!owner||(seat&&seat!=owner&&!PassengerWeaponSeat(owner))||Value<int>(owner,0x2B8)!=0||Value<void*>(owner,0x254))return;
    char name[80]{};if(!handWeapon||!ReloadName(handWeapon,name))return;
    const ScopeProfile* profile=nullptr;
    for(const auto& p:scopeProfiles)if(!strcmp(name,p.name)){profile=&p;break;}
    if(!profile)profile=Dzb2ScopeProfile(name);
    if(!profile)profile=LauncherScopeProfile(name);
    if(!profile)return;
    FireFrame pose{};AcquireSRWLockShared(&fireLock);pose=fire;ReleaseSRWLockShared(&fireLock);
    if(pose.owner!=owner||pose.weapon!=handWeapon||Value<void*>(pose.weapon,0x28)!=owner||
       Value<void*>(owner,0x288)!=pose.rig||Value<int>(owner,0x1E0)!=pose.slot||
       pose.origin!=pairFrame.origin||pose.frame>pairFrame.id||pairFrame.id-pose.frame>1||
       GetTickCount64()-pose.tick>150||!pose.gripBinding.valid)return;
    Mat poseBase=pose.trackingBase;
    if(pose.frame!=pairFrame.id){
        Mat body{},solved[70];bool supporting=false;
        if(!Read(owner,0x88,body)||!Rigid(body))return;
        const HandTargets targets=Targets(pose.native[0],body,pairFrame,pose.rightPalm,pose.leftPalm,pose.palmsValid,pose.supporting);
        if(!SolveHands(pose.native,pose.gripBinding,targets,solved,supporting,&pose.aimState))return;
        pose.trackedWeapon=Mul(solved[54],body);
        poseBase=Compose(HandPresentationBase(Mul(pose.native[0],body),pairFrame),reference,Identity(),Config().units);
    }
    const Mat renderBase=Compose(TrackingBase(views[0].source),reference,Identity(),Config().units);
    if(!Rigid(pose.trackedWeapon)||!Rigid(poseBase)||!Rigid(renderBase))return;
    scopeWeapon=pose.weapon;const Mat simulationGun=pose.trackedWeapon;
    pose.trackedWeapon=Mul(Mul(pose.trackedWeapon,Inverse(poseBase)),renderBase);
    scopeView.gun=pose.trackedWeapon;scopeView.center=profile->center;scopeView.radius=profile->radius;
    const auto scopeTuning=Tuning();
    scopeView.magnification=profile->kind==2?1.f:(profile->nativeHud||pose.wireLauncher||pose.stinger)?scopeTuning.launcherMagnification:scopeTuning.scopeMagnification;
    scopeView.reflex=profile->kind==2;
    scopeView.nativeHud=profile->nativeHud;scopeView.connectionRing=profile->connectionRing;
    scopeView.launcher=pose.wireLauncher&&!profile->nativeHud;
    scopeView.stinger=profile->kind==3&&!profile->nativeHud;
    if(pose.stinger){
        const auto seeker=StingerView(pose.weapon);scopeView.seekerTracking=seeker.tracking;
        scopeView.seekerLocked=seeker.locked;scopeView.seekerDirect=seeker.direct;
        if(seeker.tracking)scopeView.seekerDirection=Point(seeker.target,Inverse(simulationGun));
    }
    scopeView.bore=pose.trackedWeapon;scopeView.aperture=Identity();
    scopeView.connected=(scopeView.launcher||scopeView.connectionRing)&&LauncherConnected(pose.weapon);
    if(profile->kind){
        // Plane lies just ahead of the native glass. Depth testing keeps its
        // bezel/housing in front; no mesh/material replacement is required.
        Mat aperture=Identity();const Vec up=Unit(profile->up),normal=Cross({1,0,0},up);
        aperture.m[1][0]=up.x;aperture.m[1][1]=up.y;aperture.m[1][2]=up.z;
        aperture.m[2][0]=normal.x;aperture.m[2][1]=normal.y;aperture.m[2][2]=normal.z;
        SetPosition(aperture,profile->center);scopeView.aperture=aperture;scopeView.gun=Mul(aperture,pose.trackedWeapon);scopeView.center={};
    }
    if(profile->geometry){
        scopeView.geometry=profile->geometry;scopeView.aperture=profile->geometry->aperture;
        scopeView.gun=Mul(scopeView.aperture,pose.trackedWeapon);scopeView.center={};
    }
    const Vec center=scopeView.center;
    const float farRelief=scopeView.reflex?.75f:.48f;
    const Mat inverse=Inverse(scopeView.gun);float best=0;Vec eyes[2]{};
    for(int i=0;i<2;++i){
        const Mat eye=Compose(TrackingBase(views[0].source),reference,pairFrame.eyes[i],Config().units);
        const Vec e=Point(Position(eye),inverse),forward=Direction(Row(eye,2),inverse);
        eyes[i]=e;const float relief=center.z-e.z;
        if(relief<.035f||relief>farRelief||forward.z<.88f)continue;
        const float radial=ScopeEyeOffset(scopeView,e);
        const auto clamp=[](float v){return std::max(0.f,std::min(1.f,v));};
        const float visibility=clamp((1.2f-radial)*3.f)*clamp((relief-.035f)/.025f)*clamp((farRelief-relief)/.10f);
        scopeView.visibility[i]=visibility;
        if(visibility>best){best=visibility;scopeView.relief=relief;}
    }
    if(best<.02f)return;
    if(scopeView.nativeHud==16)ReadLauncherHud(scopeView.hudHeat,scopeView.hudClip);
    // r40 captured only the centered aperture's image. Eye relief/offset
    // moves its UVs outside that image, exposing a hard rectangular boundary.
    // Bound the ENTIRE lens for every visible eye, using the same finite
    // bore intersection and relief mapping as DrawGunScope. Widen only the
    // capture, then divide its UV displacement by this factor: magnification
    // and physical aperture size stay constant. One shared image, same target.
    for(int i=0;i<2;++i){
        if(scopeView.visibility[i]<.02f)continue;
        const Vec e=eyes[i];const float relief=center.z-e.z;
        const Vec hit=Add(e,Scale(Sub({0,0,100},e),relief/(100-e.z)));
        const float offset=std::max(std::abs(center.x-hit.x),std::abs(center.y-hit.y))/profile->radius;
        float required=(1+offset)*scopeView.relief/relief;
        if(scopeView.geometry){
            required=1;
            for(unsigned j=0;j<scopeView.geometry->count;++j){
                const auto& v=scopeView.geometry->outline[j];Vec ray{};float u=0,vv=0;
                if(!MeasuredScopeRay(scopeView,e,{v[0]*scopeView.radius,v[1]*scopeView.radius,0},ray,u,vv))return;
                required=std::max(required,2*std::max(std::abs(u),std::abs(vv)));
            }
        }
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
    // The aperture can tilt (ERYX), but the captured scene and missile still
    // aim along the bore. This is a periscope, not a downward-firing launcher.
    SetPosition(local,origin);scopeView.camera=Mul(local,pose.trackedWeapon);
    scopeView.tangent=profile->radius*scopeView.coverage/(scopeView.relief*scopeView.magnification);
    scopeReady=Rigid(scopeView.camera)&&std::isfinite(scopeView.tangent)&&scopeView.tangent>.001f&&scopeView.tangent<=1.f;
    static const ScopeProfile* last=nullptr;
    if(scopeReady&&last!=profile){last=profile;Log("SCOPE126 physical optic=%s zoom=%.1fx radius=%.4f relief=%.4f tangent=%.6f coverage=%.3f kind=%d nativeHud=%d; measured aperture, one GPU-only replay",profile->name,scopeView.magnification,profile->radius,scopeView.relief,scopeView.tangent,scopeView.coverage,profile->kind,profile->nativeHud);}
}
