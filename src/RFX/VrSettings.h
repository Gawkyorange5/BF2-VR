#pragma once
#include <Windows.h>
namespace CleanVR {
struct Settings {
    bool enabled=true,srgb=true,hands=true,reloadSounds=true;
    float reloadSoundVolume=1.f;
    int chargingHandle=1; // 0 legacy disabled, 1/2 required after every manual magazine reload
    float scale=.62f,distance=2.15f,pointerLift=.12f,units=1.f;
    int pointerPixels=9;
    float turnDeadzone=.22f,moveDeadzone=.25f;
    bool nativeEyeTransfer=true,singleHudPass=true,batchTransfer=true;
    bool dynamicUpload=true,fastBodyFilter=true;
    bool directEyeAtlas=true;
    bool umaDirectReadback=true;
    bool drawProfiling=false,gpuTiming=false;
    bool legacyScriptLoop=false;
    unsigned menuPollMs=100;
};
const Settings& Config();
// Called on the serialized game/render path, never on the loading keeper.
struct TuneSettings {
    float gpuCompatibility=0.f; // 0 automatic, 1 conservative, 2 original fast path
    bool adaptiveReadback=true; // bounded dGPU transfer choice; UMA retains its established path
    float resolutionMode=0.f; // 0 automatic OpenXR size, 1 custom %, 2 game setting
    float eyeResolutionPercent=100.f; // custom linear percentage, startup only
    bool steamLaunch=false;
    bool smoothTurn=true;
    bool virtualStock=true;
    bool aimSmoothing=true;
    bool snapTurn=false,hmdMovement=true,roomscale=true;
    bool wingsuit=false;
    float snapAngle=45.f,roomscaleRadius=.18f;
    bool bodyHeadFollow=true;
    bool helmetDeathView=true,vehicleHelmetDeathView=true,bodySlots=true,physicalDefib=true;
    bool assistedRevive=false;
    bool holdGrip=true,spawnEmptyHands=true,gluedItems=false,manualReload=true,grenadeCooking=false;
    bool magazineGuide=true;
    bool physicalRadio=true;
    float radioSensitivity=3.f; // gain over the original shoulder selection travel
    float slotHeight=.72f,slotRadius=.15f;
    float slotOffsets[7][3]{}; // metres in torso axes: right, up, forward
    float turnSpeed=240.f; // relative mouse counts/second; not degrees/second
    float hudScale=.85f,hudDepth=12.f,hudFov=60.f;
    float vehicleHudStrength=1.f;
    bool gunCrosshair=true,hideCrosshairAtSights=true;
    float crosshairHideSensitivity=70.f; // 100 = original r84 sight-hide volume
    bool gunScopes=true;
    float scopeMagnification=4.f;
    float launcherMagnification=1.5f;
    float gunCrosshairDistance=25.f,gunCrosshairDegrees=.6f;
    bool headAim=true;
    bool gunnerPositionLock=true;
    bool showNativeFullBody=false;
    bool useNativeArms=true;
    bool bodyWeaponDepth=true;
    bool highDetailHands=true;
    float bodySetback=.06f;
    float aimStickSpeed=75.f;
    float headGain=4000.f,headMaxSpeed=900.f,headPitchSign=-1.f;
    float flightSpeed=300.f,flightPitchSign=-1.f;
};
TuneSettings Tuning();
const char* GamePath();
void Log(const char* format,...);
bool Foreground(HWND window);
}
