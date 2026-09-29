#include "VrSettings.h"
#include <cstdio>
#include <cstdarg>
#include <cstdlib>
#include <cmath>
#include <cstring>
namespace CleanVR {
const char* GamePath(){static char path[MAX_PATH]={};if(!path[0]){GetModuleFileNameA(nullptr,path,MAX_PATH);char* end=strrchr(path,'\\');if(end)end[1]=0;}return path;}
const Settings& Config(){
    static Settings s;static bool loaded=false;if(loaded)return s;loaded=true;
    char path[MAX_PATH];sprintf_s(path,"%svr_stereo.ini",GamePath());
    s.enabled=GetPrivateProfileIntA("openxr","enable",1,path)!=0;s.srgb=GetPrivateProfileIntA("openxr","srgb",1,path)!=0;s.hands=GetPrivateProfileIntA("hands","enable",1,path)!=0;
    s.manualReload=GetPrivateProfileIntA("reload","manual",1,path)!=0;
    s.reloadSounds=GetPrivateProfileIntA("reload","sounds",1,path)!=0;
    s.chargingHandle=(int)GetPrivateProfileIntA("reload","charging_handle",1,path);
    if(s.chargingHandle<0||s.chargingHandle>2)s.chargingHandle=1;
    const auto number=[&](const char* section,const char* key,float def,float lo,float hi){char d[32],b[32];sprintf_s(d,"%.4f",def);GetPrivateProfileStringA(section,key,d,b,32,path);float f=(float)atof(b);return std::isfinite(f)&&f>=lo&&f<=hi?f:def;};
    s.reloadSoundVolume=number("reload","sound_volume",1.f,0.f,2.f);
    s.scale=number("cinema","scale",.62f,.1f,2.f);s.distance=number("cinema","distance",2.15f,.5f,6.f);s.pointerLift=number("cinema","pointer_lift",.12f,-.35f,.35f);
    s.units=number("tracking","units_per_meter",1.f,.25f,4.f);s.pointerPixels=(int)number("cinema","pointer_pixels",9,3,32);
    s.nativeEyeTransfer=GetPrivateProfileIntA("performance","native_eye_transfer",1,path)!=0;
    s.singleHudPass=GetPrivateProfileIntA("performance","single_hud_pass",1,path)!=0;
    s.batchTransfer=GetPrivateProfileIntA("performance","batch_transfer",1,path)!=0;
    s.directEyeAtlas=GetPrivateProfileIntA("performance","direct_eye_atlas",1,path)!=0;
    s.umaDirectReadback=GetPrivateProfileIntA("performance","uma_direct_readback",1,path)!=0;
    s.dynamicUpload=GetPrivateProfileIntA("performance","dynamic_upload",1,path)!=0;
    s.fastBodyFilter=GetPrivateProfileIntA("performance","fast_body_filter",1,path)!=0;
    s.drawProfiling=GetPrivateProfileIntA("performance","draw_profiling",0,path)!=0;
    s.gpuTiming=GetPrivateProfileIntA("performance","gpu_timing",0,path)!=0;
    s.legacyScriptLoop=GetPrivateProfileIntA("performance","legacy_script_loop",s.enabled?0:1,path)!=0;
    s.menuPollMs=GetPrivateProfileIntA("performance","menu_poll_ms",s.enabled?100:0,path);
    if(s.menuPollMs>250)s.menuPollMs=s.enabled?100:0;
    s.turnDeadzone=number("input","turn_deadzone",.22f,.05f,.8f);
    s.moveDeadzone=number("input","move_deadzone",.25f,.05f,.8f);return s;
}
namespace { ULONGLONG tuningLast=0; }
void InvalidateTuning(){tuningLast=0;}
TuneSettings Tuning(){
    static TuneSettings s;auto& last=tuningLast;const auto now=GetTickCount64();
    if(last&&now-last<500)return s;last=now;
    char path[MAX_PATH];sprintf_s(path,"%svr_stereo.ini",GamePath());
    const auto number=[&](const char* section,const char* key,float def,float lo,float hi){
        char d[32],b[32];sprintf_s(d,"%.4f",def);GetPrivateProfileStringA(section,key,d,b,32,path);
        char* end=nullptr;const float f=strtof(b,&end);
        return end!=b&&!*end&&std::isfinite(f)&&f>=lo&&f<=hi?f:def;
    };
    const bool smooth=GetPrivateProfileIntA("input","smooth_turn",1,path)!=0;
    const float turn=number("input","turn_speed",240.f,10.f,1500.f);
    static bool turnReported=false;
    if(!turnReported||smooth!=s.smoothTurn||turn!=s.turnSpeed){
        turnReported=true;Log("SETTINGS44 live turn smooth=%d speed=%.1f counts/s (%.2fx default)",smooth?1:0,turn,turn/240.f);
    }
    s.smoothTurn=smooth;s.turnSpeed=turn;
    s.hudScale=number("hud","scale",.85f,.35f,1.4f);
    s.hudDepth=number("hud","depth",12.f,2.f,30.f);
    s.hudFov=number("hud","vertical_fov",60.f,30.f,90.f);
    s.gunCrosshair=GetPrivateProfileIntA("hud","gun_crosshair",1,path)!=0;
    s.gunScopes=GetPrivateProfileIntA("scopes","enable",1,path)!=0;
    s.scopeMagnification=number("scopes","magnification",4.f,1.f,8.f);
    s.gunCrosshairDistance=number("hud","gun_crosshair_distance",25.f,3.f,100.f);
    s.gunCrosshairDegrees=number("hud","gun_crosshair_degrees",.6f,.15f,2.f);
    s.headAim=GetPrivateProfileIntA("vehicle","head_aim",1,path)!=0;
    s.gunnerPositionLock=GetPrivateProfileIntA("vehicle","gunner_position_lock",1,path)!=0;
    s.showNativeFullBody=GetPrivateProfileIntA("body","show_native_full_body",0,path)!=0;
    s.useNativeArms=GetPrivateProfileIntA("body","use_native_arms",1,path)!=0;
    s.bodyWeaponDepth=GetPrivateProfileIntA("body","weapon_depth",1,path)!=0;
    s.highDetailHands=GetPrivateProfileIntA("body","high_detail_hands",1,path)!=0;
    s.aimStickSpeed=number("vehicle","aim_stick_speed",75.f,10.f,180.f);
    s.headGain=number("vehicle","head_gain",4000.f,100.f,10000.f);
    s.headMaxSpeed=number("vehicle","head_max_speed",900.f,50.f,2000.f);
    s.headPitchSign=GetPrivateProfileIntA("vehicle","invert_head_pitch",0,path)?1.f:-1.f;
    s.flightSpeed=number("vehicle","flight_speed",300.f,20.f,1500.f);
    s.flightPitchSign=GetPrivateProfileIntA("vehicle","invert_flight_pitch",0,path)?1.f:-1.f;
    return s;
}
void Log(const char* format,...){
    static SRWLOCK lock=SRWLOCK_INIT;AcquireSRWLockExclusive(&lock);
    char path[MAX_PATH];sprintf_s(path,"%sbf2_clean_vr.log",GamePath());FILE* f=nullptr;
    if(fopen_s(&f,path,"a")==0&&f){SYSTEMTIME t;GetLocalTime(&t);fprintf(f,"[%02u:%02u:%02u.%03u] ",t.wHour,t.wMinute,t.wSecond,t.wMilliseconds);va_list a;va_start(a,format);vfprintf(f,format,a);va_end(a);fputc('\n',f);fclose(f);}ReleaseSRWLockExclusive(&lock);
}
bool Foreground(HWND w){DWORD pid=0;GetWindowThreadProcessId(GetForegroundWindow(),&pid);return w&&pid==GetCurrentProcessId();}
}
