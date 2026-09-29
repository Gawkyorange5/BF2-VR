#pragma once
#include "XrSessionClient.h"
namespace CleanVR { namespace Engine {
using SceneFunction=bool(__thiscall*)(void*,unsigned,unsigned,unsigned);
bool Install();
void ResetBodyResources();
bool FlushDeferredPresent(); // true when the native deferred present was consumed
bool MenuActive();
bool FrontendActive();
bool DeploymentActive();
void PublishPointer(bool valid,float u,float v,bool pressed,std::uint64_t serial);
bool LocalAlive();
bool CameraReady();
void Recenter(const Runtime::Frame&);
Mat Reference();
struct ManualReloadInput {bool reloadPulse=false,blockFire=false;float haptic=0;unsigned hapticMs=0;};
ManualReloadInput ManualReloadControls(const Runtime::Frame&,bool enabled,bool grab,bool fallback);
void CancelManualReload();
enum class VehicleRole { Infantry,Vehicle,Aimed,PitchAimed,Pilot };
struct VehicleInput {
    VehicleRole role=VehicleRole::Infantry;
    std::uintptr_t seat=0;
    bool aimValid=false;
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
bool GunCrosshair(float& x,float& y,float& halfWidth,float& halfHeight);
void GunCrosshairAvailable(bool);
struct ScopeView {
    Mat gun{},camera{};Vec center{};
    float radius=0,magnification=4,relief=0,tangent=0,coverage=1,visibility[2]{};
};
bool ScopeReady();
void ScopeAvailable(bool);
bool BeginScope();
void EndScope();
bool ScopeEye(ScopeView&,Mat& gunToEye,Mat& projection,Vec& eyeInGun,float& visibility);
bool HudPlacement(Mat&,float& width,float& height);
}}
