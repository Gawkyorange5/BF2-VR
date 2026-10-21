#include "HudCapture.h"
#include "VrSettings.h"
#include "XrSessionClient.h"
#include "FrameTiming.h"
#include <initializer_list>
#include <algorithm>
#include "Bf2Adapter.h"
#include "VehicleHudBytecode.h"

// NativeUiCapture in BF2142VR (MIT) supplies the isolation/alpha policy.
// BF2 uses the existing RFX device wrapper for the two state interceptions.
namespace CleanVR { namespace HudCapture { namespace {
IDirect3DDevice9* device=nullptr;
IDirect3DSurface9 *back=nullptr,*surface=nullptr,*resolved=nullptr,*cpu=nullptr;
IDirect3DTexture9* texture=nullptr;
IDirect3DTexture9* desktopWorld=nullptr;
IDirect3DSurface9* desktopWorldSurface=nullptr;
unsigned desktopWorldW=0,desktopWorldH=0;D3DFORMAT desktopWorldFormat=D3DFMT_UNKNOWN;
bool desktopWarned=false,desktopReported=false,desktopBlocked=false;
bool desktopDirectBlocked=false,desktopDirectReported=false,desktopCapsReady=false;
DWORD desktopStretchCaps=0;
IDirect3DStateBlock9* desktopState=nullptr;
unsigned width=0,height=0;
D3DMULTISAMPLE_TYPE samples=D3DMULTISAMPLE_NONE;DWORD quality=0;
bool active=false,ready=false,warned=false,resolvedReady=false;
constexpr D3DRENDERSTATETYPE types[]={D3DRS_SEPARATEALPHABLENDENABLE,D3DRS_SRCBLENDALPHA,D3DRS_DESTBLENDALPHA,D3DRS_BLENDOPALPHA,D3DRS_COLORWRITEENABLE};
constexpr DWORD values[]={TRUE,D3DBLEND_ONE,D3DBLEND_INVSRCALPHA,D3DBLENDOP_ADD,15};
DWORD states[5]{};
template<class T>void Release(T*& p){if(p){p->Release();p=nullptr;}}
#include "VehicleHud.inl"
bool Failure(const char* step,HRESULT hr){if(!warned){warned=true;Log("HUD capture failed: %s hr=%08X; retaining native HUD",step,(unsigned)hr);}return false;}
void ClearDesktopWorld(){Release(desktopWorldSurface);Release(desktopWorld);desktopWorldW=desktopWorldH=0;desktopWorldFormat=D3DFMT_UNKNOWN;}
void Surfaces(){ClearVehicleHud();ClearDesktopWorld();Release(surface);Release(resolved);Release(texture);Release(cpu);width=height=0;resolvedReady=false;}
bool DirectDesktopWorld(IDirect3DSurface9* eye,const RECT* crop,IDirect3DSurface9* desktop){
    if(desktopDirectBlocked||!eye||!crop||!desktop)return false;
    D3DSURFACE_DESC src{},dst{};
    const HRESULT sourceDesc=eye->GetDesc(&src),destDesc=desktop->GetDesc(&dst);
    if(FAILED(sourceDesc)||FAILED(destDesc))return false;
    if(crop->left<0||crop->top<0||crop->right<=crop->left||crop->bottom<=crop->top||
       crop->right>(LONG)src.Width||crop->bottom>(LONG)src.Height)return false;
    // Match the established preview canvas and format. In particular a
    // multisampled backbuffer still needs the original textured-draw path.
    if(eye==desktop||src.Pool!=D3DPOOL_DEFAULT||dst.Pool!=D3DPOOL_DEFAULT||
       src.MultiSampleType!=D3DMULTISAMPLE_NONE||dst.MultiSampleType!=D3DMULTISAMPLE_NONE||
       src.Format!=dst.Format||dst.Width!=width||dst.Height!=height){
        desktopDirectBlocked=true;return false;
    }
    if(!desktopCapsReady){
        D3DCAPS9 caps{};
        if(FAILED(device->GetDeviceCaps(&caps))){desktopDirectBlocked=true;return false;}
        desktopStretchCaps=caps.StretchRectFilterCaps;desktopCapsReady=true;
    }
    const bool scale=(unsigned)(crop->right-crop->left)!=dst.Width||(unsigned)(crop->bottom-crop->top)!=dst.Height;
    const DWORD linear=D3DPTFILTERCAPS_MINFLINEAR|D3DPTFILTERCAPS_MAGFLINEAR;
    if(scale&&(desktopStretchCaps&linear)!=linear){desktopDirectBlocked=true;return false;}
    // Called outside BeginScene. Crop/scale straight into the desktop rather
    // than copying to a temporary texture and then drawing that texture again.
    // The eye is only read; the headset transfer and HUD overlay are unchanged.
    const HRESULT hr=device->StretchRect(eye,crop,desktop,nullptr,scale?D3DTEXF_LINEAR:D3DTEXF_NONE);
    if(FAILED(hr)){
        desktopDirectBlocked=true;
        Log("PREVIEW98 direct desktop copy unavailable hr=%08X; using existing preview path",(unsigned)hr);
        return false;
    }
    ClearDesktopWorld();
    if(!desktopDirectReported){desktopDirectReported=true;Log("PREVIEW98 direct desktop crop active: intermediate world texture and draw removed; headset pixels unchanged");}
    return true;
}
bool CopyDesktopWorld(IDirect3DSurface9* eye,const RECT* crop){
    if(desktopBlocked||!eye||!crop||crop->right<=crop->left||crop->bottom<=crop->top)return false;
    D3DSURFACE_DESC desc{};HRESULT hr=eye->GetDesc(&desc);
    if(FAILED(hr)||desc.MultiSampleType!=D3DMULTISAMPLE_NONE||crop->left<0||crop->top<0||
       crop->right>(LONG)desc.Width||crop->bottom>(LONG)desc.Height)return false;
    const unsigned w=crop->right-crop->left,h=crop->bottom-crop->top;
    if(!desktopWorld||w!=desktopWorldW||h!=desktopWorldH||desc.Format!=desktopWorldFormat){
        ClearDesktopWorld();
        hr=device->CreateTexture(w,h,1,D3DUSAGE_RENDERTARGET,desc.Format,D3DPOOL_DEFAULT,&desktopWorld,nullptr);
        if(SUCCEEDED(hr))hr=desktopWorld->GetSurfaceLevel(0,&desktopWorldSurface);
        if(SUCCEEDED(hr)){desktopWorldW=w;desktopWorldH=h;desktopWorldFormat=desc.Format;}
        else ClearDesktopWorld();
    }
    // The source was already resolved/read for XR. This is a small GPU-only
    // copy, outside BeginScene, with no extra world replay or CPU readback.
    if(SUCCEEDED(hr))hr=device->StretchRect(eye,crop,desktopWorldSurface,nullptr,D3DTEXF_NONE);
    if(FAILED(hr)){
        desktopBlocked=true;
        if(!desktopWarned){desktopWarned=true;Log("PREVIEW61 desktop crop unavailable hr=%08X; retaining native desktop image",(unsigned)hr);}
        return false;
    }
    if(!desktopReported){desktopReported=true;Log("PREVIEW61 desktop world crop=%ux%u from current resolved eye; complete HUD overlaid; headset pixels unchanged",w,h);}
    return true;
}
bool Resolve(){
    if(resolvedReady)return true;
    if(surface!=resolved&&FAILED(device->StretchRect(surface,nullptr,resolved,nullptr,D3DTEXF_NONE)))return false;
    if(!EnhanceVehicleHud())return false;
    resolvedReady=true;return true;
}
}
void NewPair(){ready=false;resolvedReady=false;vehicleReady=false;}
bool Ready(){return ready;}
bool Unpremultiplied(){return ready&&resolvedReady&&vehicleReady;}
unsigned Width(){return width;}
unsigned Height(){return height;}
IDirect3DSurface9* Target(IDirect3DDevice9* d,DWORD index,IDirect3DSurface9* target){return active&&d==device&&index==0&&target==back?surface:target;}
DWORD State(IDirect3DDevice9* d,D3DRENDERSTATETYPE type,DWORD value){
    if(active&&d==device)for(unsigned i=0;i<5;++i)if(type==types[i]){states[i]=value;return i==4?value|D3DCOLORWRITEENABLE_ALPHA:values[i];}return value;
}
bool Begin(IDirect3DDevice9* d){
    if(active||!d)return false;device=d;ready=false;resolvedReady=false;vehicleReady=false;
    vehicleStrength=Engine::VehicleHud()?Tuning().vehicleHudStrength:0.f;
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
    D3DVIEWPORT9 viewport{};RECT scissor{};DWORD scissorEnabled=FALSE;
    hr=device->GetViewport(&viewport);
    if(SUCCEEDED(hr))hr=device->GetScissorRect(&scissor);
    if(SUCCEEDED(hr))hr=device->GetRenderState(D3DRS_SCISSORTESTENABLE,&scissorEnabled);
    if(FAILED(hr)){Release(back);return Failure("read viewport",hr);}
    hr=device->SetRenderTarget(0,surface);
    if(FAILED(hr)){Release(back);return Failure("bind target",hr);}
    // SetRenderTarget resets both viewport and scissor rectangle. Clear the
    // complete alpha target, then reinstate BF2's clipping region explicitly.
    hr=device->SetRenderState(D3DRS_SCISSORTESTENABLE,FALSE);
    if(SUCCEEDED(hr))hr=device->Clear(0,nullptr,D3DCLEAR_TARGET,0,1,0);
    const HRESULT restoreScissor=device->SetScissorRect(&scissor);
    const HRESULT restoreEnable=device->SetRenderState(D3DRS_SCISSORTESTENABLE,scissorEnabled);
    const HRESULT restoreViewport=device->SetViewport(&viewport);
    if(FAILED(restoreScissor))hr=restoreScissor;
    if(FAILED(restoreEnable))hr=restoreEnable;
    if(FAILED(restoreViewport))hr=restoreViewport;
    if(SUCCEEDED(hr))for(unsigned i=0;i<5;++i){hr=device->SetRenderState(types[i],i==4?states[i]|D3DCOLORWRITEENABLE_ALPHA:values[i]);if(FAILED(hr))break;}
    if(FAILED(hr)){
        device->SetRenderTarget(0,back);device->SetViewport(&viewport);device->SetScissorRect(&scissor);
        for(unsigned i=0;i<5;++i)device->SetRenderState(types[i],states[i]);Release(back);return Failure("initialize alpha target",hr);
    }
    active=true;return true;
}
bool End(){
    if(!active)return false;active=false;
    D3DVIEWPORT9 viewport{};RECT scissor{};
    const HRESULT vp=device->GetViewport(&viewport),sr=device->GetScissorRect(&scissor);
    const HRESULT target=device->SetRenderTarget(0,back);if(SUCCEEDED(vp))device->SetViewport(&viewport);
    const HRESULT clip=SUCCEEDED(sr)?device->SetScissorRect(&scissor):sr;
    bool restored=SUCCEEDED(vp)&&SUCCEEDED(target)&&SUCCEEDED(clip);
    // Restore the last values REQUESTED by BF2, keeping its state cache valid.
    for(unsigned i=0;i<5;++i)restored=SUCCEEDED(device->SetRenderState(types[i],states[i]))&&restored;
    Release(back);ready=restored;return ready;
}
IDirect3DSurface9* ResolvedForTransfer(){return ready&&!active&&Resolve()?(vehicleReady?vehicleSurface:resolved):nullptr;}
bool Upload(double& readMs,double& uploadMs){
    if(!ready||active)return false;
    const double start=FrameClockMs();
    // The packed path uses its own single readback surface. Allocate this
    // separate fallback only when needed, avoiding another full HUD buffer.
    if(!cpu&&FAILED(device->CreateOffscreenPlainSurface(width,height,D3DFMT_A8R8G8B8,D3DPOOL_SYSTEMMEM,&cpu,nullptr))){readMs+=FrameClockMs()-start;return false;}
    if(!Resolve()||FAILED(device->GetRenderTargetData(vehicleReady?vehicleSurface:resolved,cpu))){readMs+=FrameClockMs()-start;return false;}
    D3DLOCKED_RECT lock{};if(FAILED(cpu->LockRect(&lock,nullptr,D3DLOCK_READONLY))){readMs+=FrameClockMs()-start;return false;}
    const double copied=FrameClockMs();readMs+=copied-start;
    const bool ok=lock.Pitch>0&&Runtime::Upload(3,static_cast<const BYTE*>(lock.pBits),(unsigned)lock.Pitch);
    uploadMs+=FrameClockMs()-copied;cpu->UnlockRect();return ok;
}
void CompositeDesktop(IDirect3DSurface9* eye,const RECT* crop){
    if(!ready||active||!texture)return;
    if(!Resolve())return;
    // Reuse the state-block object, but capture CURRENT game state each time.
    // BeforeReset releases its references to native DEFAULT-pool resources.
    if(!desktopState&&FAILED(device->CreateStateBlock(D3DSBT_ALL,&desktopState)))return;
    if(FAILED(desktopState->Capture())){Release(desktopState);return;}
    IDirect3DSurface9 *previous=nullptr,*desktop=nullptr,*depth=nullptr;
    if(FAILED(device->GetRenderTarget(0,&previous))||FAILED(device->GetBackBuffer(0,0,D3DBACKBUFFER_TYPE_MONO,&desktop))){Release(previous);Release(desktop);return;}
    const bool directWorld=DirectDesktopWorld(eye,crop,desktop);
    const bool drawWorld=!directWorld&&CopyDesktopWorld(eye,crop);
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
            if(drawWorld){
                device->SetTexture(0,desktopWorld);device->SetRenderState(D3DRS_ALPHABLENDENABLE,FALSE);
                device->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP,2,v,sizeof(Vertex));
                device->SetTexture(0,texture);device->SetRenderState(D3DRS_ALPHABLENDENABLE,TRUE);
            }
            device->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP,2,v,sizeof(Vertex));device->EndScene();
        }
    }
    device->SetRenderTarget(0,previous);device->SetDepthStencilSurface(depth);desktopState->Apply();Release(previous);Release(desktop);Release(depth);
}
void Reset(){if(active)End();Release(vehicleState);Release(vehicleShader);vehicleBlocked=vehicleReported=false;Release(desktopState);Release(back);Surfaces();ready=false;warned=false;desktopWarned=desktopReported=desktopBlocked=false;desktopDirectBlocked=desktopDirectReported=desktopCapsReady=false;desktopStretchCaps=0;}
}}
