// Architecture adapted from BF2142VR 7fbc90d GpuFrameTransfer (MIT).
// BF2 keeps OpenXR in-process, supports distinct eye/HUD sizes, and fences
// both APIs before shared resources are reused. See third_party notices.
#include "SharedGpuTransfer.h"
#include "VrSettings.h"
#include "FrameTiming.h"
#include <dxgi.h>
#include <utility>
namespace CleanVR {
bool SharedGpuTransfer::Fail(const char* operation,HRESULT hr){
    if(!blocked)Log("GPU135 shared transfer disabled until reset: %s hr=%08X; CPU fallback retains resolution",operation,(unsigned)hr);
    blocked=true;return false;
}
void SharedGpuTransfer::Reset(){
    // Call under Runtime::Guard, outside replay. COM releases leave queued GPU
    // references alive; no cross-device write is scheduled after this reset.
    for(auto& image:images)image=Image{};
    resolve.Reset();producer.Reset();consumer.Reset();context.Reset();destination.Reset();
    source=nullptr;blocked=reading=reported=false;staged=0;importRejected=importReported=false;
    directMsaaCopies=legacyMsaaCopies=a8Resolves=plainCopies=0;
    for(unsigned i=0;i<4;++i)directMsaaRejected[i]=directMsaaReported[i]=false;
}
bool SharedGpuTransfer::Ensure(unsigned slot,unsigned w,unsigned h){
    if(slot>=4||!w||!h||!source||!destination)return false;
    auto& image=images[slot];if(image.opened&&!image.imported&&image.width==w&&image.height==h)return true;
    Image next;HANDLE shared=nullptr;
    HRESULT hr=source->CreateTexture(w,h,1,D3DUSAGE_RENDERTARGET,D3DFMT_A8R8G8B8,D3DPOOL_DEFAULT,&next.texture,&shared);
    if(SUCCEEDED(hr))hr=next.texture->GetSurfaceLevel(0,&next.surface);
    if(SUCCEEDED(hr))hr=destination->OpenSharedResource(shared,IID_PPV_ARGS(&next.opened));
    // D3D9 legacy shared handles belong to the allocation, not CloseHandle.
    if(FAILED(hr))return Fail("create/open texture",hr);
    next.width=w;next.height=h;image=std::move(next);return true;
}
bool SharedGpuTransfer::Initialize(IDirect3DDevice9* d,ID3D11Device* out,ID3D11DeviceContext* ctx){
    staged=0;if(blocked||!d||!out||!ctx)return false;
    directMsaaCopies=legacyMsaaCopies=a8Resolves=plainCopies=0;
    if(source&&(source!=d||destination.Get()!=out))Reset();
    if(!source){
        Ptr<IDirect3DDevice9Ex> ex;Ptr<IDirect3D9> api;Ptr<IDirect3D9Ex> apiEx;
        D3DDEVICE_CREATION_PARAMETERS cp{};LUID luid{};
        Ptr<IDXGIDevice> dxgi;Ptr<IDXGIAdapter> adapter;DXGI_ADAPTER_DESC desc{};
        HRESULT hr=d->QueryInterface(IID_PPV_ARGS(&ex));
        if(SUCCEEDED(hr))hr=d->GetDirect3D(&api);
        if(SUCCEEDED(hr))hr=api.As(&apiEx);
        if(SUCCEEDED(hr))hr=d->GetCreationParameters(&cp);
        if(SUCCEEDED(hr))hr=apiEx->GetAdapterLUID(cp.AdapterOrdinal,&luid);
        if(SUCCEEDED(hr))hr=out->QueryInterface(IID_PPV_ARGS(&dxgi));
        if(SUCCEEDED(hr))hr=dxgi->GetAdapter(&adapter);
        if(SUCCEEDED(hr))hr=adapter->GetDesc(&desc);
        if(SUCCEEDED(hr)&&(luid.LowPart!=desc.AdapterLuid.LowPart||luid.HighPart!=desc.AdapterLuid.HighPart))hr=DXGI_ERROR_UNSUPPORTED;
        if(FAILED(hr))return Fail("device/adapter LUID validation",hr);
        source=d;destination=out;context=ctx;
        hr=d->CreateQuery(D3DQUERYTYPE_EVENT,&producer);
        D3D11_QUERY_DESC query{D3D11_QUERY_EVENT,0};
        if(SUCCEEDED(hr))hr=out->CreateQuery(&query,&consumer);
        if(FAILED(hr))return Fail("completion queries",hr);
    }
    if(reading){
        const double start=FrameClockMs();HRESULT hr=S_FALSE;BOOL complete=FALSE;
        // Consumed flushed the context. Do not introduce another flush per poll.
        do{
            hr=context->GetData(consumer.Get(),&complete,sizeof(complete),D3D11_ASYNC_GETDATA_DONOTFLUSH);
            if(hr!=S_FALSE)break;
            if(FrameClockMs()-start>250.)return Fail("D3D11 reuse timeout",HRESULT_FROM_WIN32(ERROR_TIMEOUT));
            SwitchToThread();
        }while(true);
        if(hr!=S_OK||!complete)return Fail("D3D11 reuse event",hr);
        reading=false;
    }
    return true;
}
bool SharedGpuTransfer::BeginCinema(IDirect3DDevice9* d,ID3D11Device* out,ID3D11DeviceContext* ctx){
    // A separate owner: menu allocation never creates/replaces the eye pair.
    return Initialize(d,out,ctx);
}
bool SharedGpuTransfer::Begin(IDirect3DDevice9* d,ID3D11Device* out,ID3D11DeviceContext* ctx,const unsigned widths[2],const unsigned heights[2]){
    if(!Initialize(d,out,ctx))return false;
    if(!Ensure(0,widths[0],heights[0])||!Ensure(1,widths[1],heights[1]))return false;
    if(!reported){reported=true;Log("GPU135 ACTIVE native D3D9Ex shared textures; matching adapter LUID; eyes=%ux%u,%ux%u; CPU pixel copy=0; producer pair fence + consumer reuse fence",widths[0],heights[0],widths[1],heights[1]);}
    return true;
}
bool SharedGpuTransfer::Stage(unsigned slot,IDirect3DSurface9* input,unsigned w,unsigned h){
    if(blocked||!input||reading||!Ensure(slot,w,h))return false;
    D3DSURFACE_DESC desc{};HRESULT hr=input->GetDesc(&desc);
    if(SUCCEEDED(hr)&&desc.Format!=D3DFMT_A8R8G8B8&&desc.Format!=D3DFMT_X8R8G8B8)hr=D3DERR_INVALIDCALL;
    // BF2142VR resolves directly into its shared A8 image. The old BF2 path
    // always inserted an X8 intermediate for an X8 multisampled backbuffer.
    // Keep sizes identical: this changes neither the AA sample count nor the
    // color buffer used by the game. Unsupported drivers keep the old path.
    if(SUCCEEDED(hr)&&desc.MultiSampleType!=D3DMULTISAMPLE_NONE&&desc.Format==D3DFMT_X8R8G8B8&&
       desc.Width==w&&desc.Height==h&&Config().directMsaaResolve&&!directMsaaRejected[slot]){
        const HRESULT direct=source->StretchRect(input,nullptr,images[slot].surface.Get(),nullptr,D3DTEXF_NONE);
        if(SUCCEEDED(direct)){
            ++directMsaaCopies;staged|=1u<<slot;
            if(!directMsaaReported[slot]){directMsaaReported[slot]=true;Log("RESOLVE137 single-step X8-to-A8 MSAA active slot=%u samples=%u size=%ux%u; intermediate copy omitted",slot,unsigned(desc.MultiSampleType),w,h);}
            return true;
        }
        directMsaaRejected[slot]=true;
        Log("RESOLVE137 single-step unsupported slot=%u hr=%08X; retrying established two-step resolve",slot,unsigned(direct));
    }
    if(SUCCEEDED(hr)&&!directMsaaReported[slot]){
        directMsaaReported[slot]=true;
        Log("PATH138 slot=%u srcFormat=%u srcSamples=%u quality=%u src=%ux%u dst=%ux%u existingRoute=%s",slot,(unsigned)desc.Format,(unsigned)desc.MultiSampleType,(unsigned)desc.MultiSampleQuality,desc.Width,desc.Height,w,h,
            desc.MultiSampleType==D3DMULTISAMPLE_NONE?"non-MSAA-copy":(desc.Format==D3DFMT_A8R8G8B8&&desc.Width==w&&desc.Height==h?"existing-one-step-A8-resolve":"legacy-two-step"));
    }
    auto* copy=input;
    // Size changes and drivers rejecting direct conversion use the exact r136
    // resolve-then-scale/convert fallback, preserving selected antialiasing.
    if(SUCCEEDED(hr)&&desc.MultiSampleType!=D3DMULTISAMPLE_NONE&&
        (desc.Width!=w||desc.Height!=h||desc.Format!=D3DFMT_A8R8G8B8)){
        D3DSURFACE_DESC previous{};if(resolve)resolve->GetDesc(&previous);
        if(resolve&&(previous.Width!=desc.Width||previous.Height!=desc.Height||previous.Format!=desc.Format))resolve.Reset();
        if(!resolve)hr=source->CreateRenderTarget(desc.Width,desc.Height,desc.Format,D3DMULTISAMPLE_NONE,0,FALSE,&resolve,nullptr);
        if(SUCCEEDED(hr))hr=source->StretchRect(input,nullptr,resolve.Get(),nullptr,D3DTEXF_NONE);
        if(SUCCEEDED(hr))++legacyMsaaCopies;
        copy=resolve.Get();
    }
    if(SUCCEEDED(hr))hr=source->StretchRect(copy,nullptr,images[slot].surface.Get(),nullptr,desc.Width==w&&desc.Height==h?D3DTEXF_NONE:D3DTEXF_LINEAR);
    if(FAILED(hr))return Fail("resolve/stage",hr);
    if(desc.MultiSampleType==D3DMULTISAMPLE_NONE)++plainCopies;
    else if(copy==input)++a8Resolves;
    staged|=1u<<slot;return true;
}
bool SharedGpuTransfer::ImportHud(IDirect3DSurface9* input,HANDLE handle,unsigned w,unsigned h){
    // Begin has already waited for the previous D3D11 consumer BEFORE native
    // HUD rendering. Retain both COM views; the regular pair fence covers this
    // already-rendered texture exactly as it covers Stage's owned textures.
    if(blocked||reading||importRejected||!destination||!input||!handle||!w||!h)return false;
    D3DSURFACE_DESC desc{};
    if(FAILED(input->GetDesc(&desc))||desc.Width!=w||desc.Height!=h||desc.Format!=D3DFMT_A8R8G8B8||desc.MultiSampleType!=D3DMULTISAMPLE_NONE)return false;
    auto& image=images[3];
    if(!image.imported||image.surface.Get()!=input||image.handle!=handle||image.width!=w||image.height!=h){
        Image next;HRESULT hr=destination->OpenSharedResource(handle,IID_PPV_ARGS(&next.opened));
        D3D11_TEXTURE2D_DESC opened{};if(SUCCEEDED(hr))next.opened->GetDesc(&opened);
        if(SUCCEEDED(hr)&&(opened.Width!=w||opened.Height!=h||opened.Format!=DXGI_FORMAT_B8G8R8A8_UNORM||opened.SampleDesc.Count!=1))hr=E_INVALIDARG;
        if(FAILED(hr)){importRejected=true;Log("REUSE138 HUD import unsupported hr=%08X; keeping established copy",(unsigned)hr);return false;}
        next.surface=input;next.width=w;next.height=h;next.imported=true;next.handle=handle;image=std::move(next);
    }
    staged|=8u;
    if(!importReported){importReported=true;Log("REUSE138 HUD shared directly: intermediate transfer copy omitted; size=%ux%u; pair/reuse fences retained",w,h);}
    return true;
}
bool SharedGpuTransfer::StageBackbuffer(unsigned slot,unsigned w,unsigned h){
    Ptr<IDirect3DSurface9> back;HRESULT hr=source?source->GetBackBuffer(0,0,D3DBACKBUFFER_TYPE_MONO,&back):E_FAIL;
    return SUCCEEDED(hr)?Stage(slot,back.Get(),w,h):Fail("backbuffer",hr);
}
bool SharedGpuTransfer::Complete(unsigned mask,double& waitMs){
    if(blocked||!producer||(staged&mask)!=mask)return false;
    const double start=FrameClockMs();HRESULT hr=producer->Issue(D3DISSUE_END);
    if(SUCCEEDED(hr))do{
        hr=producer->GetData(nullptr,0,D3DGETDATA_FLUSH);
        if(hr!=S_FALSE)break;
        if(FrameClockMs()-start>250.){hr=HRESULT_FROM_WIN32(ERROR_TIMEOUT);break;}
        SwitchToThread();
    }while(true);
    waitMs+=FrameClockMs()-start;
    return hr==S_OK?true:Fail("D3D9 pair completion",hr);
}
void SharedGpuTransfer::Consumed(){
    if(!context||!consumer)return;
    context->End(consumer.Get());context->Flush();reading=true;
}
}
