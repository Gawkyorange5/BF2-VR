#include "HudCapture.h"
#include "VrSettings.h"
#include "XrSessionClient.h"
#include "FrameTiming.h"
#include <initializer_list>

// NativeUiCapture in BF2142VR (MIT) supplies the isolation/alpha policy.
// BF2 uses the existing RFX device wrapper for the two state interceptions.
namespace CleanVR { namespace HudCapture { namespace {
IDirect3DDevice9* device=nullptr;
IDirect3DSurface9 *back=nullptr,*surface=nullptr,*resolved=nullptr,*cpu=nullptr;
IDirect3DTexture9* texture=nullptr;
IDirect3DStateBlock9* desktopState=nullptr;
unsigned width=0,height=0;
D3DMULTISAMPLE_TYPE samples=D3DMULTISAMPLE_NONE;DWORD quality=0;
bool active=false,ready=false,warned=false,resolvedReady=false;
constexpr D3DRENDERSTATETYPE types[]={D3DRS_SEPARATEALPHABLENDENABLE,D3DRS_SRCBLENDALPHA,D3DRS_DESTBLENDALPHA,D3DRS_BLENDOPALPHA,D3DRS_COLORWRITEENABLE};
constexpr DWORD values[]={TRUE,D3DBLEND_ONE,D3DBLEND_INVSRCALPHA,D3DBLENDOP_ADD,15};
DWORD states[5]{};
template<class T>void Release(T*& p){if(p){p->Release();p=nullptr;}}
bool Failure(const char* step,HRESULT hr){if(!warned){warned=true;Log("HUD capture failed: %s hr=%08X; retaining native HUD",step,(unsigned)hr);}return false;}
void Surfaces(){Release(surface);Release(resolved);Release(texture);Release(cpu);width=height=0;resolvedReady=false;}
bool Resolve(){
    if(resolvedReady)return true;
    if(surface!=resolved&&FAILED(device->StretchRect(surface,nullptr,resolved,nullptr,D3DTEXF_NONE)))return false;
    resolvedReady=true;return true;
}
}
void NewPair(){ready=false;resolvedReady=false;}
bool Ready(){return ready;}
unsigned Width(){return width;}
unsigned Height(){return height;}
IDirect3DSurface9* Target(IDirect3DDevice9* d,DWORD index,IDirect3DSurface9* target){return active&&d==device&&index==0&&target==back?surface:target;}
DWORD State(IDirect3DDevice9* d,D3DRENDERSTATETYPE type,DWORD value){
    if(active&&d==device)for(unsigned i=0;i<5;++i)if(type==types[i]){states[i]=value;return i==4?value|D3DCOLORWRITEENABLE_ALPHA:values[i];}return value;
}
bool Begin(IDirect3DDevice9* d){
    if(active||!d)return false;device=d;ready=false;resolvedReady=false;
    HRESULT hr=device->GetRenderTarget(0,&back);if(FAILED(hr))return Failure("get target",hr);
    D3DSURFACE_DESC desc{};hr=back->GetDesc(&desc);
    if(FAILED(hr)){Release(back);return Failure("target description",hr);}
    if(!surface||width!=desc.Width||height!=desc.Height||samples!=desc.MultiSampleType||quality!=desc.MultiSampleQuality){
        Surfaces();width=desc.Width;height=desc.Height;samples=desc.MultiSampleType;quality=desc.MultiSampleQuality;
        hr=device->CreateTexture(width,height,1,D3DUSAGE_RENDERTARGET,D3DFMT_A8R8G8B8,D3DPOOL_DEFAULT,&texture,nullptr);
        if(SUCCEEDED(hr))hr=texture->GetSurfaceLevel(0,&resolved);
        if(SUCCEEDED(hr)){if(samples==D3DMULTISAMPLE_NONE){surface=resolved;surface->AddRef();}else hr=device->CreateRenderTarget(width,height,D3DFMT_A8R8G8B8,samples,quality,FALSE,&surface,nullptr);}
        if(FAILED(hr)){Release(back);Surfaces();return Failure("create surfaces",hr);}
        Log("HUD isolated target %ux%u samples=%u",width,height,(unsigned)samples);
    }
    for(unsigned i=0;i<5;++i){hr=device->GetRenderState(types[i],&states[i]);if(FAILED(hr)){Release(back);return Failure("read alpha state",hr);}}
    D3DVIEWPORT9 viewport{};hr=device->GetViewport(&viewport);
    if(FAILED(hr)){Release(back);return Failure("read viewport",hr);}
    hr=device->SetRenderTarget(0,surface);
    if(FAILED(hr)){Release(back);return Failure("bind target",hr);}
    hr=device->Clear(0,nullptr,D3DCLEAR_TARGET,0,1,0);
    device->SetViewport(&viewport);
    if(SUCCEEDED(hr))for(unsigned i=0;i<5;++i){hr=device->SetRenderState(types[i],i==4?states[i]|D3DCOLORWRITEENABLE_ALPHA:values[i]);if(FAILED(hr))break;}
    if(FAILED(hr)){
        device->SetRenderTarget(0,back);device->SetViewport(&viewport);
        for(unsigned i=0;i<5;++i)device->SetRenderState(types[i],states[i]);Release(back);return Failure("initialize alpha target",hr);
    }
    active=true;return true;
}
bool End(){
    if(!active)return false;active=false;
    D3DVIEWPORT9 viewport{};const HRESULT vp=device->GetViewport(&viewport);
    const HRESULT target=device->SetRenderTarget(0,back);if(SUCCEEDED(vp))device->SetViewport(&viewport);
    bool restored=SUCCEEDED(vp)&&SUCCEEDED(target);
    // Restore the last values REQUESTED by BF2, keeping its state cache valid.
    for(unsigned i=0;i<5;++i)restored=SUCCEEDED(device->SetRenderState(types[i],states[i]))&&restored;
    Release(back);ready=restored;return ready;
}
IDirect3DSurface9* ResolvedForTransfer(){return ready&&!active&&Resolve()?resolved:nullptr;}
bool Upload(double& readMs,double& uploadMs){
    if(!ready||active)return false;
    const double start=FrameClockMs();
    // The packed path uses its own single readback surface. Allocate this
    // separate fallback only when needed, avoiding another full HUD buffer.
    if(!cpu&&FAILED(device->CreateOffscreenPlainSurface(width,height,D3DFMT_A8R8G8B8,D3DPOOL_SYSTEMMEM,&cpu,nullptr))){readMs+=FrameClockMs()-start;return false;}
    if(!Resolve()||FAILED(device->GetRenderTargetData(resolved,cpu))){readMs+=FrameClockMs()-start;return false;}
    D3DLOCKED_RECT lock{};if(FAILED(cpu->LockRect(&lock,nullptr,D3DLOCK_READONLY))){readMs+=FrameClockMs()-start;return false;}
    const double copied=FrameClockMs();readMs+=copied-start;
    const bool ok=lock.Pitch>0&&Runtime::Upload(3,static_cast<const BYTE*>(lock.pBits),(unsigned)lock.Pitch);
    uploadMs+=FrameClockMs()-copied;cpu->UnlockRect();return ok;
}
void CompositeDesktop(){
    if(!ready||active||!texture)return;
    if(!Resolve())return;
    // Reuse the state-block object, but capture CURRENT game state each time.
    // BeforeReset releases its references to native DEFAULT-pool resources.
    if(!desktopState&&FAILED(device->CreateStateBlock(D3DSBT_ALL,&desktopState)))return;
    if(FAILED(desktopState->Capture())){Release(desktopState);return;}
    IDirect3DSurface9 *previous=nullptr,*desktop=nullptr,*depth=nullptr;
    if(FAILED(device->GetRenderTarget(0,&previous))||FAILED(device->GetBackBuffer(0,0,D3DBACKBUFFER_TYPE_MONO,&desktop))){Release(previous);Release(desktop);return;}
    device->GetDepthStencilSurface(&depth);
    const bool bound=SUCCEEDED(device->SetRenderTarget(0,desktop));
    if(bound){device->SetDepthStencilSurface(nullptr);
        if(SUCCEEDED(device->BeginScene())){
            device->SetVertexShader(nullptr);device->SetPixelShader(nullptr);device->SetFVF(D3DFVF_XYZRHW|D3DFVF_TEX1);device->SetTexture(0,texture);
            for(auto s:{D3DRS_FOGENABLE,D3DRS_LIGHTING,D3DRS_ZENABLE,D3DRS_ZWRITEENABLE,D3DRS_ALPHATESTENABLE,D3DRS_SCISSORTESTENABLE,D3DRS_STENCILENABLE,D3DRS_SRGBWRITEENABLE,D3DRS_SEPARATEALPHABLENDENABLE})device->SetRenderState(s,FALSE);
            device->SetRenderState(D3DRS_CULLMODE,D3DCULL_NONE);device->SetRenderState(D3DRS_COLORWRITEENABLE,15);
            device->SetRenderState(D3DRS_ALPHABLENDENABLE,TRUE);device->SetRenderState(D3DRS_SRCBLEND,D3DBLEND_ONE);device->SetRenderState(D3DRS_DESTBLEND,D3DBLEND_INVSRCALPHA);device->SetRenderState(D3DRS_BLENDOP,D3DBLENDOP_ADD);
            device->SetTextureStageState(0,D3DTSS_COLOROP,D3DTOP_SELECTARG1);device->SetTextureStageState(0,D3DTSS_COLORARG1,D3DTA_TEXTURE);
            device->SetTextureStageState(0,D3DTSS_ALPHAOP,D3DTOP_SELECTARG1);device->SetTextureStageState(0,D3DTSS_ALPHAARG1,D3DTA_TEXTURE);
            device->SetTextureStageState(0,D3DTSS_TEXCOORDINDEX,0);device->SetTextureStageState(0,D3DTSS_TEXTURETRANSFORMFLAGS,D3DTTFF_DISABLE);device->SetTextureStageState(1,D3DTSS_COLOROP,D3DTOP_DISABLE);
            device->SetSamplerState(0,D3DSAMP_ADDRESSU,D3DTADDRESS_CLAMP);device->SetSamplerState(0,D3DSAMP_ADDRESSV,D3DTADDRESS_CLAMP);
            device->SetSamplerState(0,D3DSAMP_SRGBTEXTURE,FALSE);device->SetSamplerState(0,D3DSAMP_MIPFILTER,D3DTEXF_NONE);device->SetSamplerState(0,D3DSAMP_MINFILTER,D3DTEXF_LINEAR);device->SetSamplerState(0,D3DSAMP_MAGFILTER,D3DTEXF_LINEAR);
            D3DVIEWPORT9 vp{0,0,width,height,0,1};device->SetViewport(&vp);
            struct Vertex{float x,y,z,w,u,v;};const float w=(float)width-.5f,h=(float)height-.5f;
            const Vertex v[]={{-.5f,-.5f,0,1,0,0},{w,-.5f,0,1,1,0},{-.5f,h,0,1,0,1},{w,h,0,1,1,1}};
            device->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP,2,v,sizeof(Vertex));device->EndScene();
        }
    }
    device->SetRenderTarget(0,previous);device->SetDepthStencilSurface(depth);desktopState->Apply();Release(previous);Release(desktop);Release(depth);
}
void Reset(){if(active)End();Release(desktopState);Release(back);Surfaces();ready=false;warned=false;}
}}
