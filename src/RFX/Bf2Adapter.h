#pragma once
#include <cmath>
#include "XrSessionClient.h"
namespace CleanVR { namespace Engine {
using SceneFunction=bool(__thiscall*)(void*,unsigned,unsigned,unsigned);
bool Install();
void ResetBodyResources();
bool FlushDeferredPresent(); // true when the native deferred present was consumed
bool MenuActive();
bool FrontendActive();
bool DeploymentActive();
bool RadioMenuActive();
bool RadioOverlayActive();
bool FlatMenuActive(); // full-screen menus only; shoulder radio remains stereo
void PublishRadioPointer(float u,float v,bool pressed,std::uint64_t serial);
bool RadioPointerConsumed(std::uint64_t serial);
void PublishPointer(bool valid,float u,float v,bool pressed,std::uint64_t serial);
bool LocalAlive();
bool CameraReady();
void Recenter(const Runtime::Frame&);
Mat Reference();
bool FootLocomotion(const Runtime::Frame&,Vec stick,float snapRadians,float smoothCountsPerSecond,bool enabled);
void StopLocomotion();
int BodySlotControls(const Runtime::Frame&,bool enabled,bool actions,bool grip,int cycle,bool& consume);
int FireModeItem(); // native reselect of the item actually held; zero when unavailable
void ClearBodySlotControls();
bool GrenadePrimed();
bool PreviewBodySlotLayout(const Runtime::Frame&,const float offsets[7][3],float height,int selected);
void ClearBodySlotLayoutPreview();
bool EmptyHands();
bool C4Controls(const Runtime::Frame&,bool enabled,bool left,bool right,bool& down);
void ClearC4Controls();
bool PhysicalItemFire(const Runtime::Frame&,bool& down);
bool KnifeControls(const Runtime::Frame&,bool enabled,bool& firing);
bool DefibControls(const Runtime::Frame&,bool enabled,bool& firing);
void ClearDefibControls();
bool DefibIndicator(float& x,float& y,float& sx,float& sy,float& progress,bool& ready);
struct BodySlotMarker {float x,y,sx,sy;unsigned item;bool hovered,editing;};
unsigned BodySlotMarkers(BodySlotMarker (&markers)[7]);
struct ManualReloadInput {bool reloadPulse=false,blockFire=false;float haptic=0;unsigned hapticMs=0;};
ManualReloadInput ManualReloadControls(const Runtime::Frame&,bool enabled,bool grab,bool fallback,bool eject);
void CancelManualReload();
enum class VehicleRole { Infantry,Vehicle,Aimed,PitchAimed,Pilot };
struct VehicleInput {
    VehicleRole role=VehicleRole::Infantry;
    std::uintptr_t seat=0;
    bool aimValid=false,lookValid=false,personalWeapon=false;
    float yawError=0,pitchError=0;
};
VehicleInput Vehicle(const Runtime::Frame&,float yawStep=0,float pitchStep=0);
bool BeginPair(const Runtime::Frame&);
bool BeginEye(int);
void EndEye();
void EndPair();
bool EyeActive();
Mat EyeProjection();
// NDC center and angular half size of one shared world-space aim point.
// Direction marker only: no collision, spread or bullet-drop prediction.
bool GunCrosshair(float& x,float& y,float& halfWidth,float& halfHeight,float& hitAlpha,DWORD& hitColor);
void GunCrosshairAvailable(bool);
// Immutable, premeasured aperture; data is compiled into the DLL.
struct ScopeGeometry {
    Mat aperture{},local{}; // weapon-root rest frame and attached-part frame
    unsigned part=0,count=0;float outline[32][2]{};
};
struct ScopeView {
    Mat gun{},camera{};Vec center{};
    float radius=0,magnification=4,relief=0,tangent=0,coverage=1,visibility[2]{};
    bool reflex=false,launcher=false,connected=false;
    bool stinger=false,seekerTracking=false,seekerLocked=false;Vec seekerDirection{};
    // 0 preserves accepted optics. Nonzero selects original DZB2 sight art.
    int nativeHud=0;bool connectionRing=false,seekerDirect=false,hudHeat=false,hudClip=false;
    Mat aperture{}; // local plane relative to the actual drawn native mesh
    Mat bore{}; // current drawn gun, distinct from the captured scene camera
    const ScopeGeometry* geometry=nullptr;
};
// A canted reflex window is not the optical axis. Project the bore-zero ray
// onto its plane for eye-box visibility; otherwise the Vector's 12-degree
// glass tilt rejects an eye aligned behind the gun at normal arm's length.
inline float ScopeEyeOffset(const ScopeView& lens,Vec eye){
    Vec hit=eye;
    if(lens.geometry){
        const Vec zero=Point({0,0,100},Inverse(lens.aperture));
        const float dz=zero.z-eye.z;
        if(!std::isfinite(dz)||dz<1.f)return 100.f;
        hit=Add(eye,Scale(Sub(zero,eye),(lens.center.z-eye.z)/dz));
    }
    return std::hypot(hit.x-lens.center.x,hit.y-lens.center.y)/(lens.radius+.014f);
}
// Ray through a measured (possibly canted) glass plane, expressed along the
// gun's bore. Use the identical mapping for capture bounds and both eye draws.
inline bool MeasuredScopeRay(const ScopeView& lens,Vec eye,Vec point,Vec& ray,float& u,float& v){
    const Vec e=Point(eye,lens.aperture),p=Point(point,lens.aperture);
    const Vec d=Sub(p,e),zero=Sub({0,0,100},e);
    if(d.z<=.01f||zero.z<=1.f||lens.radius<=0||lens.magnification<=0)return false;
    const float dx=d.x/d.z-zero.x/zero.z,dy=d.y/d.z-zero.y/zero.z;
    const Vec origin=Add(Position(lens.aperture),{0,0,.025f}),f=Sub({0,0,100},origin);
    if(f.z<=1.f)return false;
    ray={f.x/f.z+dx/lens.magnification,f.y/f.z+dy/lens.magnification,1};
    u=dx*lens.relief/(2*lens.radius);v=-dy*lens.relief/(2*lens.radius);
    return std::isfinite(ray.x+ray.y+u+v);
}
bool ScopeReady();
int StingerSoundState(); // 0 silent, 1 native target tracking, 2 confirmed lock
void ScopeAvailable(bool);
bool BeginScope();
void EndScope();
bool ScopeEye(ScopeView&,Mat& gunToEye,Mat& projection,Vec& eyeInGun,float& visibility);
bool VehicleHud();
bool HudPlacement(Mat&,float& width,float& height);
}}
