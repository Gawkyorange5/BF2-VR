#include "LiveSettings.h"
#include <cstddef>
#include <cstdio>
#include <cmath>
namespace CleanVR { namespace {
#define VR_OPTION(page,member,toggle,section,key,label,help,lo,hi,step,initial) \
    {page,toggle,section,key,label,help,(float)lo,(float)hi,(float)step,(float)initial,offsetof(TuneSettings,member)},
const LiveOption options[]={
#include "LiveSettingsCatalog.inl"
};
#undef VR_OPTION
constexpr unsigned count=sizeof(options)/sizeof(options[0]);
}
const LiveOption* LiveOptions(unsigned& n){n=count;return options;}
float LiveValue(unsigned index){
    if(index>=count)return 0;const auto t=Tuning();const auto& o=options[index];
    const auto p=reinterpret_cast<const unsigned char*>(&t)+o.offset;
    return o.toggle?(*reinterpret_cast<const bool*>(p)?1.f:0.f):*reinterpret_cast<const float*>(p);
}
bool SaveLiveValues(const float* values,const bool* changed){
    if(!values||!changed)return false;bool any=false;
    for(unsigned i=0;i<count;++i)if(changed[i]){
        any=true;const auto& o=options[i];const float v=values[i];
        if(!std::isfinite(v)||v<o.minimum||v>o.maximum||(o.toggle&&v!=0&&v!=1))return false;
    }
    if(!any)return true;
    char path[MAX_PATH];sprintf_s(path,"%svr_stereo.ini",GamePath());
    static bool backedUp=false;
    if(!backedUp){
        if(GetFileAttributesA(path)!=INVALID_FILE_ATTRIBUTES){
            SYSTEMTIME t;GetLocalTime(&t);char backup[MAX_PATH];
            sprintf_s(backup,"%svr_stereo.ini.before-r44-%04u%02u%02u-%02u%02u%02u-%03u.bak",GamePath(),t.wYear,t.wMonth,t.wDay,t.wHour,t.wMinute,t.wSecond,t.wMilliseconds);
            if(!CopyFileA(path,backup,TRUE)){Log("SETTINGS44 backup failed error=%lu",GetLastError());return false;}
        }
        backedUp=true;
    }
    bool ok=true;
    for(unsigned i=0;i<count;++i)if(changed[i]){
        char value[32];sprintf_s(value,"%.4f",values[i]);
        if(!WritePrivateProfileStringA(options[i].section,options[i].key,value,path)){
            Log("SETTINGS44 save failed key=%s error=%lu",options[i].key,GetLastError());ok=false;break;
        }
    }
    InvalidateTuning();return ok;
}
}
