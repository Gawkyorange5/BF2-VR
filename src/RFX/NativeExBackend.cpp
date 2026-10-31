#include "NativeExBackend.h"
#include "NativeExResources.h"
#include "NativeExReset.h"
#include "LegacyShaderMemory.h"
#include "VrSettings.h"
#include <MinHook.h>
#include <d3dx9.h>
namespace CleanVR { namespace NativeEx { namespace {
IDirect3DDevice9* owner=nullptr;
// Owned for the native device's process lifetime; never release COM under DllMain.
NativeExReset* resetState=nullptr;
}
bool Active(IDirect3DDevice9* device){return device&&device==owner;}
HRESULT CreateDevice(IDirect3D9* classic, UINT adapter, D3DDEVTYPE type, HWND window,
    DWORD flags, D3DPRESENT_PARAMETERS* parameters, IDirect3DDevice9** output){
    static const bool requested=Config().enabled&&Config().sharedGpuTransfer;
    if(!requested||!parameters||!output||owner)
        return classic->CreateDevice(adapter,type,window,flags,parameters,output);
    // A different legacy D3DX build must be profiled before enabling this backend.
    // Fall back before resources exist; never replace the renderer in mid-frame.
    const HMODULE shader=GetModuleHandleA("d3dx9_25.dll");
    bool guarded=LegacyShaderMemory::Ready();
    if(!guarded){
        // The public DZB2 launcher supplies a proxy. Initialize its existing
        // guard through its normal export, then recognize all import owners.
        // Never install a second allocator over the proxy's live allocations.
        HMODULE original=GetModuleHandleA("bf2vr_d3dx25_original.dll");
        if(!original)guarded=LegacyShaderMemory::InstallLegacyShaderMemory(shader,Log);
        if(!guarded&&shader){
            using PoolFn=HRESULT(WINAPI*)(ID3DXEffectPool**);
            const auto poolFn=reinterpret_cast<PoolFn>(GetProcAddress(shader,"D3DXCreateEffectPool"));
            ID3DXEffectPool* pool=nullptr;if(poolFn&&SUCCEEDED(poolFn(&pool))&&pool)pool->Release();
            original=GetModuleHandleA("bf2vr_d3dx25_original.dll");
            guarded=LegacyShaderMemory::HasExternalGuard(original,shader);
            if(guarded)Log("GPU135 existing DZB2 shader guard verified; original allocator retained");
        }
    }
    if(!guarded){
        Log("SHADER135 guard unavailable; using classic D3D9 before game resources; requested graphics and resolution retained");
        return classic->CreateDevice(adapter,type,window,flags,parameters,output);
    }
    const D3DPRESENT_PARAMETERS saved=*parameters;
    parameters->PresentationInterval=D3DPRESENT_INTERVAL_IMMEDIATE;
    *output=nullptr;
    static HMODULE system=LoadLibraryExW(L"d3d9.dll",nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32);
    using CreateEx=HRESULT(WINAPI*)(UINT,IDirect3D9Ex**);
    auto create=system?reinterpret_cast<CreateEx>(GetProcAddress(system,"Direct3DCreate9Ex")):nullptr;
    IDirect3D9Ex* api=nullptr;IDirect3DDevice9Ex* ex=nullptr;
    HRESULT hr=create?create(D3D_SDK_VERSION,&api):D3DERR_NOTAVAILABLE;
    if(SUCCEEDED(hr)){
        D3DADAPTER_IDENTIFIER9 a{},b{};
        hr=classic->GetAdapterIdentifier(adapter,0,&a);
        if(SUCCEEDED(hr))hr=api->GetAdapterIdentifier(adapter,0,&b);
        if(SUCCEEDED(hr)&&(a.VendorId!=b.VendorId||a.DeviceId!=b.DeviceId||a.SubSysId!=b.SubSysId||a.Revision!=b.Revision))hr=D3DERR_NOTAVAILABLE;
    }
    D3DDISPLAYMODEEX mode{};mode.Size=sizeof(mode);
    mode.Width=parameters->BackBufferWidth;mode.Height=parameters->BackBufferHeight;
    mode.RefreshRate=parameters->FullScreen_RefreshRateInHz;mode.Format=parameters->BackBufferFormat;
    mode.ScanLineOrdering=D3DSCANLINEORDERING_UNKNOWN;
    if(SUCCEEDED(hr))hr=api->CreateDeviceEx(adapter,type,window,flags,parameters,
        parameters->Windowed?nullptr:&mode,&ex);
    if(SUCCEEDED(hr)){
        const auto mh=MH_Initialize();
        if(mh!=MH_OK&&mh!=MH_ERROR_ALREADY_INITIALIZED)hr=E_FAIL;
    }
    NativeExReset* initial=nullptr;
    if(SUCCEEDED(hr)){initial=new NativeExReset;hr=initial->Initialize(ex);}
    if(SUCCEEDED(hr)&&!InstallNativeExResources(ex,Log))hr=E_FAIL;
    if(SUCCEEDED(hr)){
        owner=ex;resetState=initial;*output=ex;
        Log("GPU135 renderer=native-D3D9Ex managed-shadow-upload=1 adapter=%u; shared GPU transfer candidate",adapter);
    }else{
        delete initial;if(ex)ex->Release();
        *parameters=saved;
        Log("GPU135 Ex creation/compatibility unavailable hr=%08X; creating classic D3D9 before game resources",(unsigned)hr);
        hr=classic->CreateDevice(adapter,type,window,flags,parameters,output);
    }
    if(api)api->Release();return hr;
}
HRESULT Reset(IDirect3DDevice9* device,D3DPRESENT_PARAMETERS* parameters){
    if(!Active(device))return device->Reset(parameters);
    IDirect3DDevice9Ex* ex=nullptr;
    HRESULT hr=device->QueryInterface(IID_PPV_ARGS(&ex));
    if(FAILED(hr))return hr;
    D3DDISPLAYMODEEX mode{};mode.Size=sizeof(mode);
    if(parameters){mode.Width=parameters->BackBufferWidth;mode.Height=parameters->BackBufferHeight;
        mode.RefreshRate=parameters->FullScreen_RefreshRateInHz;mode.Format=parameters->BackBufferFormat;
        mode.ScanLineOrdering=D3DSCANLINEORDERING_UNKNOWN;}
    if(parameters)parameters->PresentationInterval=D3DPRESENT_INTERVAL_IMMEDIATE;
    hr=ex->ResetEx(parameters,parameters&&!parameters->Windowed?&mode:nullptr);
    if(SUCCEEDED(hr))hr=resetState->Restore(device,parameters&&parameters->EnableAutoDepthStencil);
    ex->Release();Log("GPU135 legacy Reset via Ex, default pipeline restored hr=%08X",(unsigned)hr);return hr;
}
}}
