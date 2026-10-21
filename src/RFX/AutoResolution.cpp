#include "AutoResolution.h"
#include "VrSettings.h"
#include "IAT.h"
#include <string>
#include <vector>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <cwchar>

namespace CleanVR { namespace AutoResolution { namespace {
using MainArgs=int(__cdecl*)(int*,char***,char***,int,void*);
MainArgs original=nullptr;
void** rawImport=nullptr;
char* replacement=nullptr;
bool attempted=false;
bool limitedRequest=false;
unsigned wantedW=0,wantedH=0;

void Status(const char* status){
    char path[MAX_PATH];sprintf_s(path,"%sbf2_vr_resolution.ini",GamePath());
    WritePrivateProfileStringA("Startup","status",status,path);
    char value[32];sprintf_s(value,"%u",wantedW);WritePrivateProfileStringA("Startup","requested_width",value,path);
    sprintf_s(value,"%u",wantedH);WritePrivateProfileStringA("Startup","requested_height",value,path);
}
bool SwapImport(void** slot,void* value){
    if(!slot)return false;
    DWORD old=0;if(!VirtualProtect(slot,sizeof(void*),PAGE_READWRITE,&old))return false;
    *slot=value;DWORD unused=0;VirtualProtect(slot,sizeof(void*),old,&unused);return true;
}
struct Span {size_t begin,end;};
bool Space(char c){return c==' '||c=='\t'||c=='\r'||c=='\n';}
// Preserve unrelated launch arguments byte-for-byte, including quoted profile,
// mod and account options. Never write those arguments to a log.
bool ReplaceSize(const char* input,unsigned w,unsigned h,std::string& output){
    if(!input)return false;
    const size_t length=strnlen_s(input,32768);if(!length||length>=32700)return false;
    std::vector<Span> spans;
    for(size_t p=0;p<length;){
        while(p<length&&Space(input[p]))++p;if(p==length)break;
        const size_t start=p;bool quoted=false;unsigned slashes=0;
        for(;p<length;++p){
            const char c=input[p];
            if(c=='"'&&slashes%2==0)quoted=!quoted;
            if(!quoted&&Space(c))break;
            slashes=c=='\\'?slashes+1:0;
        }
        if(quoted)return false;
        spans.push_back({start,p});
    }
    if(spans.empty())return false;
    size_t kept=0;
    for(size_t i=1;i<spans.size();++i){
        const auto s=spans[i];std::string token(input+s.begin,s.end-s.begin);
        if(_stricmp(token.c_str(),"+szx")&&_stricmp(token.c_str(),"+szy")&&_stricmp(token.c_str(),"+fullscreen"))continue;
        if(i+1>=spans.size())return false;
        const auto v=spans[++i];std::string value(input+v.begin,v.end-v.begin);
        char* end=nullptr;strtol(value.c_str(),&end,10);
        if(end==value.c_str()||*end)return false;
        output.append(input+kept,s.begin-kept);kept=v.end;
    }
    output.append(input+kept,length-kept);
    char suffix[96];sprintf_s(suffix," +fullscreen 0 +szx %u +szy %u",w,h);output+=suffix;
    return output.size()<32768;
}

bool Probe(unsigned& w,unsigned& h,unsigned& rw,unsigned& rh,int& mode,int& limited){
    wchar_t folder[MAX_PATH],exe[MAX_PATH],temp[MAX_PATH],result[MAX_PATH];
    const DWORD length=GetModuleFileNameW(nullptr,folder,MAX_PATH);
    if(!length||length>=MAX_PATH)return false;
    wchar_t* slash=wcsrchr(folder,L'\\');if(!slash)return false;*slash=0;
    if(swprintf_s(exe,L"%s\\BF2-VR-Settings.exe",folder)<0)return false;
    if(GetFileAttributesW(exe)==INVALID_FILE_ATTRIBUTES){Log("RESOLUTION60 helper missing; using game size");return false;}
    const DWORD tempLength=GetTempPathW(MAX_PATH,temp);
    if(!tempLength||tempLength>=MAX_PATH||!GetTempFileNameW(temp,L"bvr",0,result))return false;
    wchar_t command[3*MAX_PATH+100];
    if(swprintf_s(command,L"\"%s\" --resolve-openxr \"%s\" \"%s\"",exe,folder,result)<0){DeleteFileW(result);return false;}
    STARTUPINFOW startup{};startup.cb=sizeof(startup);startup.dwFlags=STARTF_USESHOWWINDOW;startup.wShowWindow=SW_HIDE;
    PROCESS_INFORMATION process{};
    if(!CreateProcessW(exe,command,nullptr,nullptr,FALSE,CREATE_NO_WINDOW,nullptr,folder,&startup,&process)){
        Log("RESOLUTION60 helper could not start error=%lu; using game size",GetLastError());DeleteFileW(result);return false;
    }
    CloseHandle(process.hThread);
    const DWORD wait=WaitForSingleObject(process.hProcess,15000);DWORD code=1;
    if(wait==WAIT_OBJECT_0)GetExitCodeProcess(process.hProcess,&code);
    else {TerminateProcess(process.hProcess,1);WaitForSingleObject(process.hProcess,1000);}
    CloseHandle(process.hProcess);
    char answer[512]{};DWORD bytes=0;
    HANDLE file=CreateFileW(result,GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(file!=INVALID_HANDLE_VALUE){ReadFile(file,answer,sizeof(answer)-1,&bytes,nullptr);CloseHandle(file);}
    DeleteFileW(result);
    if(wait!=WAIT_OBJECT_0||code){Log("RESOLUTION60 probe unavailable exit=%lu wait=%lu; using game size (%s)",code,wait,answer);return false;}
    if(sscanf_s(answer,"OK59|%u|%u|%u|%u|%d|%d",&w,&h,&rw,&rh,&mode,&limited)!=6)return false;
    return w>=320&&h>=320&&w<=4096&&h<=4096&&(unsigned long long)w*h<=8388608&&
        rw>=320&&rh>=320&&rw<=16384&&rh<=16384&&(mode==0||mode==1)&&(limited==0||limited==1);
}

void Prepare(){
    if(attempted)return;attempted=true;
    char measured[MAX_PATH];sprintf_s(measured,"%sbf2_vr_resolution.ini",GamePath());
    WritePrivateProfileStringA("Actual",nullptr,nullptr,measured);
    Status("Checking headset");
    char settings[MAX_PATH],number[32];sprintf_s(settings,"%svr_stereo.ini",GamePath());
    GetPrivateProfileStringA("graphics","resolution_mode","0",number,sizeof(number),settings);
    char* end=nullptr;const float mode=strtof(number,&end);
    if(!GetPrivateProfileIntA("openxr","enable",1,settings)||
       (end!=number&&!*end&&std::isfinite(mode)&&mode==2.f)){
        Status("Game resolution selected");Log("RESOLUTION60 game resolution selected; startup options retained");return;
    }
    unsigned w=0,h=0,rw=0,rh=0;int selectedMode=0,limited=0;
    if(!Probe(w,h,rw,rh,selectedMode,limited)){Status("Headset query failed; using game resolution");return;}
    // MSVC71's BF2 WinMain startup reads its imported _acmdln AFTER calling
    // __getmainargs (confirmed in BF2 1.5). Redirect only BF2's data import.
    // Do not modify the shared CRT's command-line storage or the process PEB.
    const char* raw=rawImport?*reinterpret_cast<char**>(*rawImport):nullptr;
    std::string text;
    if(!ReplaceSize(raw,w,h,text)){Status("Launch arguments could not be resized; using game resolution");return;}
    char* buffer=static_cast<char*>(HeapAlloc(GetProcessHeap(),0,text.size()+1));
    if(!buffer){Status("Resolution allocation failed; using game resolution");return;}
    memcpy(buffer,text.c_str(),text.size()+1);replacement=buffer;
    if(!SwapImport(rawImport,&replacement)){replacement=nullptr;HeapFree(GetProcessHeap(),0,buffer);Status("Resolution import unavailable; using game resolution");return;}
    wantedW=w;wantedH=h;limitedRequest=limited!=0;
    Status(limited?"Requested headset size with 32-bit safety limit":"Requested headset size; awaiting game renderer");
    Log("RESOLUTION60 startup mode=%s runtimeRecommended=%ux%u requested=%ux%u limited=%d; BF2 native +szx/+szy before WinMain; original graphics/AA retained",selectedMode==0?"Automatic":"Custom",rw,rh,w,h,limited);
}
int __cdecl GetMainArgs(int* argc,char*** argv,char*** env,int wildcard,void* info){
    const int result=original(argc,argv,env,wildcard,info);
    if(result>=0){try{Prepare();}catch(...){Status("Resolution startup failed; using game resolution");Log("RESOLUTION60 startup exception; native startup continues");}}
    return result;
}
} // namespace
void Install(HMODULE game){
    // DllMain: import lookup/swap only. Both supported RFX loaders initialize
    // before the EXE entry point. All probing happens later in GetMainArgs.
    void** mainImport=IAT::find_function("__getmainargs",game);
    rawImport=IAT::find_function("_acmdln",game);
    if(!mainImport||!rawImport||!*mainImport||!*rawImport)return;
    original=reinterpret_cast<MainArgs>(*mainImport);
    if(!SwapImport(mainImport,reinterpret_cast<void*>(&GetMainArgs)))original=nullptr;
}
bool Requested(){return wantedW&&wantedH;}
void Size(unsigned& w,unsigned& h){w=wantedW;h=wantedH;}
void Confirm(unsigned w,unsigned h){
    if(!w||!h)return;
    char path[MAX_PATH],value[32];sprintf_s(path,"%sbf2_vr_resolution.ini",GamePath());
    sprintf_s(value,"%u",w);WritePrivateProfileStringA("Actual","native_width",value,path);
    sprintf_s(value,"%u",h);WritePrivateProfileStringA("Actual","native_height",value,path);
    if(Requested()){
        const bool match=w==wantedW&&h==wantedH;
        Status(match?(limitedRequest?"Headset resolution active (32-bit size limit applied)":"Headset resolution active"):"Resolution mismatch: game did not use requested size");
        Log("RESOLUTION60 renderer confirmed=%d requested=%ux%u actual=%ux%u",match?1:0,wantedW,wantedH,w,h);
    }else if(!attempted){Status("Automatic startup hook unavailable; game resolution active");Log("RESOLUTION60 startup hook unavailable; actual game size=%ux%u",w,h);}
}
} }
