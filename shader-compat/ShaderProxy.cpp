// Local compatibility adapter for the verified BF2 d3dx9_25 runtime.
// All APIs except the three native effect entry points are export forwarders.
#include <windows.h>
#include <d3dx9.h>
#include <cstdio>
#include <cstdarg>
#include <cstring>
#include <algorithm>
#include "LegacyShaderMemory.h"
#include "Forwarders.h"

namespace {
HMODULE selfModule=nullptr, originalModule=nullptr;
INIT_ONCE once=INIT_ONCE_STATIC_INIT;
SRWLOCK logLock=SRWLOCK_INIT;
char logPath[MAX_PATH]{};
using EffectFn=HRESULT(WINAPI*)(LPDIRECT3DDEVICE9,LPCVOID,UINT,CONST D3DXMACRO*,LPD3DXINCLUDE,DWORD,LPD3DXEFFECTPOOL,LPD3DXEFFECT*,LPD3DXBUFFER*);
using CompilerFn=HRESULT(WINAPI*)(LPCSTR,UINT,CONST D3DXMACRO*,LPD3DXINCLUDE,DWORD,LPD3DXEFFECTCOMPILER*,LPD3DXBUFFER*);
using PoolFn=HRESULT(WINAPI*)(LPD3DXEFFECTPOOL*);
EffectFn createEffect=nullptr;
CompilerFn createCompiler=nullptr;
PoolFn createPool=nullptr;
volatile LONG effects=0,compilers=0;

void Log(const char* format, ...){
    if(!logPath[0])return;
    char body[2048],line[2160];va_list args;va_start(args,format);
    _vsnprintf_s(body,sizeof(body),_TRUNCATE,format,args);va_end(args);
    SYSTEMTIME t{};GetLocalTime(&t);
    const int n=_snprintf_s(line,sizeof(line),_TRUNCATE,"[%02u:%02u:%02u.%03u] %s\r\n",t.wHour,t.wMinute,t.wSecond,t.wMilliseconds,body);
    if(n<=0)return;
    AcquireSRWLockExclusive(&logLock);
    HANDLE file=CreateFileA(logPath,FILE_APPEND_DATA,FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(file!=INVALID_HANDLE_VALUE){DWORD written=0;WriteFile(file,line,(DWORD)n,&written,nullptr);CloseHandle(file);}
    ReleaseSRWLockExclusive(&logLock);
}

void Memory(const char* stage,HRESULT result){
    MEMORYSTATUSEX m{};m.dwLength=sizeof(m);BOOL ok=GlobalMemoryStatusEx(&m);
    const auto stats=CleanVR::LegacyShaderMemory::GetLegacyShaderMemoryStats();
    SIZE_T lowerFree=0,largestLower=0,upperFree=0,largestUpper=0;
    uintptr_t cursor=0;
    while(cursor<0xFFFF0000u){
        MEMORY_BASIC_INFORMATION region{};
        if(!VirtualQuery(reinterpret_cast<void*>(cursor),&region,sizeof(region))||!region.RegionSize)break;
        const uintptr_t base=reinterpret_cast<uintptr_t>(region.BaseAddress);
        const unsigned long long end=(unsigned long long)base+region.RegionSize;
        if(region.State==MEM_FREE){
            SIZE_T low=base<0x80000000u?(SIZE_T)((std::min)(end,0x80000000ull)-base):0;
            SIZE_T high=end>0x80000000ull?(SIZE_T)(end-(std::max)((unsigned long long)base,0x80000000ull)):0;
            lowerFree+=low;upperFree+=high;largestLower=(std::max)(largestLower,low);largestUpper=(std::max)(largestUpper,high);
        }
        if(end<=cursor||end>=0xFFFF0000ull)break;cursor=(uintptr_t)end;
    }
    Log("%s hr=%08X guard=%d query=%d availableVA_MiB=%llu availableRAM_MiB=%llu availableCommit_MiB=%llu lowFreeMiB=%zu largestLowMiB=%zu upperFreeMiB=%zu largestUpperMiB=%zu arenaActiveKiB=%zu arenaPeakKiB=%zu arenaFailures=%zu heapFailures=%zu effects=%ld compilers=%ld",
        stage,(unsigned)result,CleanVR::LegacyShaderMemory::Ready(),ok,m.ullAvailVirtual/1048576,m.ullAvailPhys/1048576,m.ullAvailPageFile/1048576,
        lowerFree/1048576,largestLower/1048576,upperFree/1048576,largestUpper/1048576,
        stats.active/1024,stats.peak/1024,stats.failures,stats.heapFailures,effects,compilers);
}

BOOL CALLBACK Initialize(PINIT_ONCE,void*,void**){
    char path[MAX_PATH]{};
    DWORD length=GetModuleFileNameA(selfModule,path,MAX_PATH);
    if(!length||length>=MAX_PATH)return FALSE;
    char* slash=std::strrchr(path,'\\');if(!slash)return FALSE;slash[1]=0;
    if(strcpy_s(logPath,path)||strcat_s(logPath,"bf2_shader_compat.log"))return FALSE;
    if(strcat_s(path,"bf2vr_d3dx25_original.dll"))return FALSE;
    Log("BEGIN DZB2 shader compatibility candidate v1 pid=%lu",GetCurrentProcessId());
    originalModule=LoadLibraryA(path);
    if(!originalModule){Log("Original shader library load failed win32=%lu",GetLastError());return FALSE;}
    createEffect=reinterpret_cast<EffectFn>(GetProcAddress(originalModule,"D3DXCreateEffect"));
    createCompiler=reinterpret_cast<CompilerFn>(GetProcAddress(originalModule,"D3DXCreateEffectCompiler"));
    createPool=reinterpret_cast<PoolFn>(GetProcAddress(originalModule,"D3DXCreateEffectPool"));
    if(!createEffect||!createCompiler||!createPool){Log("Original shader exports unavailable");return FALSE;}
    // Hooks live for the process lifetime. No allocator work runs in DllMain.
    HMODULE pinned=nullptr;
    if(!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_PIN,reinterpret_cast<LPCSTR>(&Initialize),&pinned)){
        Log("Could not pin adapter; allocator guard disabled");Memory("startup diagnostics only",S_OK);return TRUE;
    }
    char setting[8]{};GetEnvironmentVariableA("BF2VR_SHADER_GUARD",setting,sizeof(setting));
    if(std::strcmp(setting,"0"))CleanVR::LegacyShaderMemory::InstallLegacyShaderMemory(originalModule,Log);
    else Log("Allocator guard disabled for diagnostic comparison");
    Memory("startup",S_OK);return TRUE;
}

bool Ready(){return InitOnceExecuteOnce(&once,Initialize,nullptr,nullptr)!=FALSE;}
void Failure(const char* stage,HRESULT result,LPCVOID source,UINT bytes,DWORD flags,ID3DXBuffer* errors){
    Log("%s FAILED hr=%08X sourceBytes=%u flags=%08X",stage,(unsigned)result,bytes,(unsigned)flags);
    Memory(stage,result);
    if(errors && errors->GetBufferPointer()){
        const int n=(int)(std::min)((DWORD)1024,errors->GetBufferSize());
        Log("compilerMessage=%.*s",n,static_cast<const char*>(errors->GetBufferPointer()));
    }
    // Only print printable text from the bounded first line; effects may be binary.
    char label[160]{};unsigned n=0;const char* text=static_cast<const char*>(source);
    while(text&&n<bytes&&n<sizeof(label)-1){unsigned char c=(unsigned char)text[n];if(c<32||c>126)break;label[n++]=(char)c;}
    if(n)Log("sourceFirstLine=%s",label);
}
}

extern "C" HRESULT WINAPI ProxyCreateEffect(LPDIRECT3DDEVICE9 device,LPCVOID source,UINT bytes,CONST D3DXMACRO* defines,LPD3DXINCLUDE include,DWORD flags,LPD3DXEFFECTPOOL pool,LPD3DXEFFECT* effect,LPD3DXBUFFER* errors){
    if(effect)*effect=nullptr;if(errors)*errors=nullptr;
    if(!Ready())return E_FAIL;
    ID3DXBuffer* localErrors=nullptr;auto output=errors?errors:&localErrors;
    const HRESULT hr=createEffect(device,source,bytes,defines,include,flags,pool,effect,output);
    if(FAILED(hr))Failure("effect",hr,source,bytes,flags,*output);
    else if(InterlockedIncrement(&effects)==1)Log("First effect success guard=%d",CleanVR::LegacyShaderMemory::Ready());
    if(localErrors)localErrors->Release();return hr;
}
extern "C" HRESULT WINAPI ProxyCreateCompiler(LPCSTR source,UINT bytes,CONST D3DXMACRO* defines,LPD3DXINCLUDE include,DWORD flags,LPD3DXEFFECTCOMPILER* compiler,LPD3DXBUFFER* errors){
    if(compiler)*compiler=nullptr;if(errors)*errors=nullptr;
    if(!Ready())return E_FAIL;
    ID3DXBuffer* localErrors=nullptr;auto output=errors?errors:&localErrors;
    const HRESULT hr=createCompiler(source,bytes,defines,include,flags,compiler,output);
    if(FAILED(hr))Failure("effect compiler",hr,source,bytes,flags,*output);
    else if(InterlockedIncrement(&compilers)==1)Log("First effect compiler success guard=%d",CleanVR::LegacyShaderMemory::Ready());
    if(localErrors)localErrors->Release();return hr;
}
extern "C" HRESULT WINAPI ProxyCreatePool(LPD3DXEFFECTPOOL* pool){
    if(pool)*pool=nullptr;if(!Ready())return E_FAIL;
    const HRESULT hr=createPool(pool);if(FAILED(hr))Memory("effect pool",hr);return hr;
}
BOOL WINAPI DllMain(HINSTANCE module,DWORD reason,LPVOID){
    if(reason==DLL_PROCESS_ATTACH){selfModule=module;DisableThreadLibraryCalls(module);}return TRUE;
}
