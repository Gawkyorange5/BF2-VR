#pragma once
#include <Windows.h>
namespace CleanVR {
struct Settings {
    bool enabled=true,srgb=true,hands=true,manualReload=true,reloadSounds=true;
    float reloadSoundVolume=1.f;
    int chargingHandle=1; // 0 off, 1 required when empty + optional otherwise, 2 required after every reload
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
    bool smoothTurn=true;
    float turnSpeed=240.f; // relative mouse counts/second; not degrees/second
    float hudScale=.85f,hudDepth=12.f,hudFov=60.f;
    bool gunCrosshair=true;
    bool gunScopes=true;
    float scopeMagnification=4.f;
    float gunCrosshairDistance=25.f,gunCrosshairDegrees=.6f;
    bool headAim=true;
    bool gunnerPositionLock=true;
    bool showNativeFullBody=false;
    bool useNativeArms=true;
    bool bodyWeaponDepth=true;
    bool highDetailHands=true;
    float aimStickSpeed=75.f;
    float headGain=4000.f,headMaxSpeed=900.f,headPitchSign=-1.f;
    float flightSpeed=300.f,flightPitchSign=-1.f;
};
TuneSettings Tuning();
const char* GamePath();
void Log(const char* format,...);
bool Foreground(HWND window);
}
