#include "CleanVR.h"
#include "AutoResolution.h"
#include "RenderWindow.h"
#include "VrSettings.h"
#include "ControllerInput.h"
#include "VrSettingsMenu.h"
#include "HudCapture.h"
#include "MenuPointer.h"
#include "FrameTiming.h"
#include "D3DWorkProfile.h"
#include <d3dx9math.h>
#include <algorithm>
#include <cmath>
#include <vector>
#include <cstring>

namespace CleanVR { namespace {
IDirect3DDevice9* device=nullptr;
HWND window=nullptr;
bool initialized=false,inPair=false,pairReady=false,flatFallback=false,referenceSet=false,resetting=false;
bool hudReady=false,hudFailed=false,hudUnpremultiplied=false;XrPosef hudPose{};float hudWidth=0,hudHeight=0;
unsigned pairCount=0,suppressedPresents=0;
unsigned nativeWidth=0,nativeHeight=0;double hudDrawMs=0;
std::uint64_t sceneSerial=0,submittedScene=~0ull;
ULONGLONG lastPresentationTick=0;bool presentationSeen=false;
template<class T> void Release(T*& p){if(p){p->Release();p=nullptr;}}
#include "GunCrosshairDraw.inl"
#include "GunScopeDraw.inl"
struct PairTiming {
    double setup=0,eyes[2]{};
    double render=0,stage=0,read=0,upload=0,hud=0,finish=0,desktop=0,total=0;
    double hudDraw=0,uploadWait=0,uploadCopy=0;
    double uploadMap=0,uploadWrite=0,uploadSubmit=0;
    unsigned batches=0,directEyes=0,directReads=0;
};
struct Performance {
    std::uint64_t previousFrame=0;double previousStart=0,windowStart=0;
    unsigned count=0,successful=0,intervalCount=0;double intervals[512]{};
    double intervalSum=0,intervalMax=0,wait=0,budget=0;PairTiming sum{};
    void Add(const Runtime::Frame& f,double start,const PairTiming& t,bool rendered){
        // Cinema/loading frames are not stereo samples. Start a new window
        // after a gap in XR frame IDs rather than counting menu time as FPS.
        if(previousFrame&&f.id!=previousFrame+1)*this=Performance{};
        if(!windowStart)windowStart=start;
        if(previousStart){const double dt=start-previousStart;intervals[intervalCount%512]=dt;++intervalCount;intervalSum+=dt;intervalMax=std::max(intervalMax,dt);}
        previousFrame=f.id;previousStart=start;++count;if(rendered)++successful;
        sum.render+=t.render;sum.stage+=t.stage;sum.read+=t.read;sum.upload+=t.upload;sum.hud+=t.hud;
        sum.setup+=t.setup;for(unsigned i=0;i<2;++i)sum.eyes[i]+=t.eyes[i];
        sum.finish+=t.finish;sum.desktop+=t.desktop;sum.total+=t.total;wait+=f.waitMs;budget+=(double)f.period/1000000.0;
        sum.hudDraw+=t.hudDraw;sum.uploadWait+=t.uploadWait;sum.uploadCopy+=t.uploadCopy;
        sum.uploadMap+=t.uploadMap;sum.uploadWrite+=t.uploadWrite;sum.uploadSubmit+=t.uploadSubmit;
        sum.batches+=t.batches;sum.directEyes+=t.directEyes;sum.directReads+=t.directReads;
        if(start-windowStart<5000||!intervalCount)return;
        const unsigned n=std::min(intervalCount,512u);double ordered[512];std::copy(intervals,intervals+n,ordered);std::sort(ordered,ordered+n);
        const double divisor=(double)count;
        Log("PERF stereo frames=%u ok=%u fps=%.2f intervalMs=%.2f p95=%.2f max=%.2f budgetMs=%.2f sceneMs=%.2f xrWaitMs=%.2f",count,successful,intervalCount*1000.0/intervalSum,intervalSum/intervalCount,ordered[(n*95-1)/100],intervalMax,budget/divisor,sum.total/divisor,wait/divisor);
        Log("PERF workMs render=%.2f stage=%.2f readback=%.2f upload=%.2f hudOther=%.2f submitPrepare=%.2f desktop=%.2f (CPU wall times; readback includes GPU wait; xrWait is included in submitPrepare across frames)",sum.render/divisor,sum.stage/divisor,sum.read/divisor,sum.upload/divisor,sum.hud/divisor,sum.finish/divisor,sum.desktop/divisor);
        Log("PERF subsetsMs hudDraw=%.2f uploadWait=%.2f uploadCopyFlush=%.2f (included in render/upload above)",sum.hudDraw/divisor,sum.uploadWait/divisor,sum.uploadCopy/divisor);
        Log("PERF transfer batches=%u/%u (one readback/upload/flush per successful batch)",sum.batches,count);
        Log("PERF directAtlas eyes=%u/%u (successful resolve-and-pack copies; one intermediate eye copy omitted per eye)",sum.directEyes,count*2);
        Log("PERF directReadback frames=%u/%u (completed current-frame atlas reads without GetRenderTargetData)",sum.directReads,count);
        Log("PERF uploadDetailMs map=%.2f write=%.2f copyFlush=%.2f (subsets of upload, CPU wall times)",sum.uploadMap/divisor,sum.uploadWrite/divisor,sum.uploadSubmit/divisor);
        Log("PERF sceneDetailMs setup=%.2f eye0=%.2f eye1=%.2f (CPU wall times; eyes included in render)",sum.setup/divisor,sum.eyes[0]/divisor,sum.eyes[1]/divisor);
        *this=Performance{};
    }
} performance;
#include "GpuTiming.inl"
struct Capture {
    IDirect3DSurface9* target=nullptr;IDirect3DSurface9* cpu=nullptr;IDirect3DSurface9* msaaResolve=nullptr;
    unsigned width=0,height=0;D3DFORMAT format=D3DFMT_UNKNOWN;
    std::vector<unsigned char> pixels;
    void Clear(){Release(target);Release(cpu);Release(msaaResolve);width=height=0;pixels.clear();}
    bool EnsureCpu(){return cpu||SUCCEEDED(device->CreateOffscreenPlainSurface(width,height,format,D3DPOOL_SYSTEMMEM,&cpu,nullptr));}
    bool Stage(unsigned w,unsigned h){
        IDirect3DSurface9* back=nullptr;if(!device||FAILED(device->GetBackBuffer(0,0,D3DBACKBUFFER_TYPE_MONO,&back)))return false;
        D3DSURFACE_DESC desc{};back->GetDesc(&desc);
        if(desc.Format!=D3DFMT_X8R8G8B8&&desc.Format!=D3DFMT_A8R8G8B8){Release(back);return false;}
        if(width!=w||height!=h||format!=desc.Format||!target){
            Clear();format=desc.Format;
            if(FAILED(device->CreateRenderTarget(w,h,format,D3DMULTISAMPLE_NONE,0,FALSE,&target,nullptr))){Release(back);Clear();return false;}
            width=w;height=h;
        }
        const auto filter=desc.Width==w&&desc.Height==h?D3DTEXF_NONE:D3DTEXF_LINEAR;
        HRESULT hr=S_OK;
        if(desc.MultiSampleType!=D3DMULTISAMPLE_NONE&&(desc.Width!=w||desc.Height!=h)){
            D3DSURFACE_DESC old{};if(msaaResolve)msaaResolve->GetDesc(&old);
            if(msaaResolve&&(old.Width!=desc.Width||old.Height!=desc.Height||old.Format!=desc.Format))Release(msaaResolve);
            if(!msaaResolve)hr=device->CreateRenderTarget(desc.Width,desc.Height,desc.Format,D3DMULTISAMPLE_NONE,0,FALSE,&msaaResolve,nullptr);
            if(SUCCEEDED(hr))hr=device->StretchRect(back,nullptr,msaaResolve,nullptr,D3DTEXF_NONE);
            if(SUCCEEDED(hr))hr=device->StretchRect(msaaResolve,nullptr,target,nullptr,filter);
        }else hr=device->StretchRect(back,nullptr,target,nullptr,filter);
        Release(back);return SUCCEEDED(hr);
    }
    bool FromAtlas(IDirect3DSurface9* atlas,const Runtime::TransferRegion& region){
        // Recover a current eye before abandoning the direct atlas. A stale
        // per-eye surface from an earlier frame must never reach fallback.
        if(!atlas||!device||!region.width||!region.height)return false;
        if(width!=region.width||height!=region.height||format!=D3DFMT_A8R8G8B8||!target){
            Clear();format=D3DFMT_A8R8G8B8;
            if(FAILED(device->CreateRenderTarget(region.width,region.height,format,D3DMULTISAMPLE_NONE,0,FALSE,&target,nullptr))){Clear();return false;}
            width=region.width;height=region.height;
        }
        const RECT rect{(LONG)region.x,(LONG)region.y,(LONG)(region.x+region.width),(LONG)(region.y+region.height)};
        return SUCCEEDED(device->StretchRect(atlas,&rect,target,nullptr,D3DTEXF_NONE));
    }
    bool Upload(int eye,double& readMs,double& uploadMs){
        const double begin=FrameClockMs();
        if(!target||!EnsureCpu()||FAILED(device->GetRenderTargetData(target,cpu))){readMs+=FrameClockMs()-begin;return false;}
        D3DLOCKED_RECT mapped{};
        if(FAILED(cpu->LockRect(&mapped,nullptr,D3DLOCK_READONLY))){readMs+=FrameClockMs()-begin;return false;}
        const double copied=FrameClockMs();readMs+=copied-begin;
        // Feed the mapped readback directly to D3D11. It snapshots these bytes
        // synchronously, so no extra per-eye CPU vector/copy is necessary.
        const bool ok=mapped.Pitch>0&&Runtime::Upload(eye,static_cast<const BYTE*>(mapped.pBits),(unsigned)mapped.Pitch);
        uploadMs+=FrameClockMs()-copied;cpu->UnlockRect();return ok;
    }
    bool Read(unsigned w,unsigned h){
        // Cinema still needs writable CPU pixels for its pointer and blank
        // loading-screen detection. Keep that path separate from stereo.
        if(!Stage(w,h)||!EnsureCpu()||FAILED(device->GetRenderTargetData(target,cpu)))return false;
        pixels.resize((size_t)w*h*4);
        D3DLOCKED_RECT lock{};if(FAILED(cpu->LockRect(&lock,nullptr,D3DLOCK_READONLY)))return false;
        for(unsigned y=0;y<h;++y)memcpy(pixels.data()+(size_t)y*w*4,static_cast<const BYTE*>(lock.pBits)+(size_t)y*lock.Pitch,w*4);
        cpu->UnlockRect();return true;
    }
} captures[3];
#include "GpuReplayGuard.inl"
#include "DirectAtlasCapture.inl"
void ReleaseInput(){RenderWindow::ReleaseMenuPointer();Controls::ReleaseAll();Engine::PublishPointer(false,0,0,false,0);}
bool Cinema(){
    // Refresh the last living helmet pose even while the kit menu is open.
    // Short-circuiting on FlatMenuActive stranded suicide deaths in cinema.
    const bool camera=Engine::CameraReady();
    return Engine::FlatMenuActive()||!camera||flatFallback;
}
float CinemaAspect(unsigned w,unsigned h){
    const float desktop=RenderWindow::DesktopAspect();
    // BF2's normalized menu layout is authored for a flat widescreen canvas.
    // Changing the quad shape does not resample its texture or the stereo eyes.
    return desktop>0&&Engine::MenuActive()?1.f/desktop:(w?(float)h/w:0);
}
Mat panel=Identity();XrPosef panelPose{};bool panelValid=false,panelWasVisible=false;std::uint64_t panelOrigin=0;
void PlacePanel(const Runtime::Frame& f){
    panel=Level(f.head);const Vec center=Point({0,0,Config().distance},panel);
    const float yaw=std::atan2(panel.m[2][0],panel.m[2][2]);
    panelPose.orientation={0,-std::sin(yaw*.5f),0,std::cos(yaw*.5f)};panelPose.position={center.x,center.y,-center.z};
    panelOrigin=f.origin;panelValid=true;Log("CINEMA anchored in LOCAL space at (%.3f %.3f %.3f)",center.x,center.y,center.z);
}
void UpdatePanel(const Runtime::Frame& f){
    const bool visible=Cinema();
    if(visible!=panelWasVisible)Log("DISPLAY mode=%s localAlive=%d fallback=%d",visible?"cinema":"stereo",Engine::LocalAlive()?1:0,flatFallback?1:0);
    if(visible&&(!panelWasVisible||!panelValid||panelOrigin!=f.origin))PlacePanel(f);panelWasVisible=visible;
}
struct Hit{bool valid=false;float u=0,v=0;};
Hit Pointer(const Runtime::Frame& f,float aspect){
    Hit hit{};if(!panelValid||!f.hands[1].aimValid||!f.valid||aspect<=0)return hit;
    const Vec center=Point({0,0,Config().distance},panel),normal=Row(panel,2),origin=Position(f.hands[1].aim);
    const Vec ray=Unit(Add(Row(f.hands[1].aim,2),Scale(Row(panel,1),Config().pointerLift)));
    const float denominator=Dot(ray,normal);if(denominator<.05f)return hit;
    const float t=Dot(Sub(center,origin),normal)/denominator;if(t<=0)return hit;
    const Vec delta=Sub(Add(origin,Scale(ray,t)),center);const float width=1.15f+1.55f*Config().scale;
    hit.u=.5f+Dot(delta,Row(panel,0))/width;hit.v=.5f-Dot(delta,Row(panel,1))/(width*aspect);
    hit.valid=hit.u>=0&&hit.u<=1&&hit.v>=0&&hit.v<=1;return hit;
}
void InputFrame(const Runtime::Frame& f){
    static std::uint64_t origin=0;static bool f6=false;
    const bool enabled=f.valid&&f.focused&&Foreground(window);
    if(!enabled){if(SettingsMenu::Active()){SettingsMenu::Close();referenceSet=false;}ReleaseInput();f6=false;return;}
    const bool keyF6=(GetAsyncKeyState(VK_F6)&0x8000)!=0&&!Controls::RecenterKeyBlocked();
    if(!referenceSet||origin!=f.origin||(keyF6&&!f6)){Engine::Recenter(f);referenceSet=true;origin=f.origin;panelValid=false;}f6=keyF6;
    if(SettingsMenu::Active()){
        SettingsMenu::Input(f);Engine::PublishPointer(false,0,0,false,f.id);
        if(!SettingsMenu::Active()){ReleaseInput();Engine::Recenter(f);panelValid=false;}
        return;
    }
    if(Controls::Global(f)){Engine::Recenter(f);panelValid=false;}
    if(Controls::TakeSettingsRequest()){
        ReleaseInput();
        if(SettingsMenu::Open(f))return;
        Log("SETTINGS44 headset menu could not open");
    }
    UpdatePanel(f);
    if(Controls::VehicleSeatWheel(f)){RenderWindow::ReleaseMenuPointer();return;}
    if(Controls::PhysicalRadio(f)){RenderWindow::ReleaseMenuPointer();return;}
    static int previous=-1;const int mode=Engine::FrontendActive()?1:Engine::DeploymentActive()?2:Cinema()?3:4;
    if(mode!=previous){previous=mode;Log("INPUT mode=%s localAlive=%d",mode==1?"frontend":mode==2?"deployment":mode==3?"cinema":"gameplay",Engine::LocalAlive()?1:0);}
    RECT client{};GetClientRect(window,&client);const int w=client.right,h=client.bottom;
    if(Cinema()){
        Controls::Menu(f);
        // Pointer intersection and visible menu quad must use the same shape.
        Hit hit=Pointer(f,nativeWidth?CinemaAspect(nativeWidth,nativeHeight):CinemaAspect(w,h));
        Controls::PointerButton(false);
        if(Engine::DeploymentActive()){
            RenderWindow::ReleaseMenuPointer();Engine::PublishPointer(hit.valid,hit.u,hit.v,f.hands[1].trigger>.55f,f.id);
        }else{
            Engine::PublishPointer(false,0,0,false,f.id);
            RenderWindow::MenuPointer(window,hit.valid,hit.u,hit.v,f.hands[1].trigger>.55f);
        }return;
    }
    RenderWindow::ReleaseMenuPointer();
    Engine::PublishPointer(false,0,0,false,f.id);
    if(!Engine::LocalAlive()){Controls::Menu(f);Controls::PointerButton(false);return;}
    Controls::Gameplay(f,Engine::Reference());
}
void Prepare(){
    Runtime::Guard lock;
    if(resetting||!Runtime::Begin()){ReleaseInput();return;}Runtime::Frame f{};
    if(Runtime::Snapshot(f))InputFrame(f);else ReleaseInput();
}
void DrawPointer(Capture& c,const Hit& hit){
    if(!hit.valid)return;const auto glyph=MenuPointer::Make(hit.u,hit.v,c.width,c.height,Config().pointerPixels);
    const auto draw=[&](const MenuPointer::Rect* rectangles,unsigned count,std::uint32_t color){
        for(unsigned i=0;i<count;++i){const auto& r=rectangles[i];
            for(int y=r.top;y<r.bottom;++y)for(int x=r.left;x<r.right;++x)
                std::memcpy(c.pixels.data()+((size_t)y*c.width+x)*4,&color,4);
        }
    };
    draw(glyph.border,glyph.borderCount,MenuPointer::Border);draw(glyph.fill,glyph.fillCount,MenuPointer::Fill);
}
bool HasPicture(const Capture& c){
    // Some loading Presents contain just a cleared backbuffer. Keep the last
    // visible loading/menu image instead of replacing it with solid black.
    for(unsigned y=0;y<c.height;y+=std::max(1u,c.height/128))for(unsigned x=0;x<c.width;x+=std::max(1u,c.width/128)){
        const auto* p=c.pixels.data()+((size_t)y*c.width+x)*4;if(p[0]>8||p[1]>8||p[2]>8)return true;
    }return false;
}
void Finish(){
    SettingsMenu::Present();
    if(!SettingsMenu::Active())Controls::PresentSeatWheel();
    // A completed eye pair retains its own located poses even if rendering
    // took >250 ms. Never re-label those pixels with a newer pose.
    if(pairReady&&!Cinema()){
        Runtime::SubmitEyes(hudReady?&hudPose:nullptr,hudWidth,hudHeight,hudUnpremultiplied);pairReady=false;Prepare();return;
    }
    const bool cinema=Cinema();if(cinema)Runtime::RefreshCinemaFrame();
    Runtime::Frame f{};if(!Runtime::Snapshot(f)){if(cinema)Runtime::SubmitRetained();else Runtime::SubmitEmpty();pairReady=false;Prepare();return;}
    UpdatePanel(f);
    if(cinema&&f.shouldRender){
        IDirect3DSurface9* back=nullptr;D3DSURFACE_DESC desc{};
        if(SUCCEEDED(device->GetBackBuffer(0,0,D3DBACKBUFFER_TYPE_MONO,&back))){back->GetDesc(&desc);Release(back);}
        auto& c=captures[2];
        if(desc.Width&&desc.Height&&Runtime::QuadSize(desc.Width,desc.Height)&&c.Read(desc.Width,desc.Height)){
            const float aspect=CinemaAspect(c.width,c.height),width=1.15f+1.55f*Config().scale;
            if(!HasPicture(c)&&Runtime::HasCinemaImage()){
                static unsigned blanks=0;if(++blanks<=4||blanks%300==0)Log("LOADING ignored cleared native backbuffer; retaining cinema image #%u",blanks);
                Runtime::SubmitRetained();
            }else{
                Hit pointer=Pointer(f,aspect);
                if(Controls::PhysicalRadioPointer(pointer.u,pointer.v))pointer.valid=true;
                DrawPointer(c,pointer);
                if(Runtime::Upload(2,c.pixels.data(),c.width*4))Runtime::SubmitQuad(panelPose,width,width*aspect);else Runtime::SubmitRetained();
            }
        }else Runtime::SubmitRetained();
    }else Runtime::SubmitEmpty();
    pairReady=false;Prepare();
}
void Start(){if(!initialized){initialized=true;Engine::Install();Runtime::Initialize();}}
void Pump(bool completedNativeFrame=false){
    Runtime::Guard lock;
    // EndScene can provide the startup fallback, followed by a native/device
    // Present for that same image. Do not consume the next prepared XR frame.
    if(!pairReady&&submittedScene==sceneSerial)return;
    if(!Runtime::Begin()){ReleaseInput();return;}
    // Native Present outside replay means BF2 completed a normal desktop
    // frame. Death/spectator/pause views may not invoke our stereo scene hook.
    // Present that frame in cinema rather than waiting for a pair that cannot
    // arrive. Keep the fallback anchored until a real stereo pair resumes.
    if(completedNativeFrame&&!pairReady&&!Cinema()){
        flatFallback=true;Log("DISPLAY completed native frame has no stereo pair; cinema fallback");
    }
    if(pairReady||Cinema()){Finish();submittedScene=sceneSerial;}
    else {
        Runtime::Frame f{};
        if(!Runtime::Snapshot(f)||!f.shouldRender){Runtime::SubmitEmpty();Prepare();submittedScene=sceneSerial;}
    }
}
bool PresentationBoundary(IDirect3DDevice9* d,const char* source){
    if(resetting||!Config().enabled)return false;if(!device)AttachDevice(d);
    if(!d||d!=device)return false;
    if(!presentationSeen)Log("BOUNDARY %s connected",source);
    presentationSeen=true;lastPresentationTick=GetTickCount64();
    if(inPair){++suppressedPresents;return true;}
    Start();Pump(true);return false;
}
} // namespace
void ReportRenderFailure(const char* operation,HRESULT result){
    // Error-only diagnostic: successful native draws do not take a clock,
    // query the driver, acquire a lock or write a log entry.
    static unsigned count=0;static ULONGLONG last=0;
    const auto now=GetTickCount64();++count;
    if(count<=16||now-last>=5000){
        last=now;Log("CAPTURE113 native %s failed hr=%08X count=%u scene=%llu inPair=%d",operation,(unsigned)result,count,sceneSerial,inPair?1:0);
    }
}
void AttachDevice(IDirect3DDevice9* d){
    device=d;D3DDEVICE_CREATION_PARAMETERS creation{};if(d&&SUCCEEDED(d->GetCreationParameters(&creation)))window=creation.hFocusWindow;
    IDirect3DSwapChain9* chain=nullptr;
    if(d&&SUCCEEDED(d->GetSwapChain(0,&chain))){D3DPRESENT_PARAMETERS p{};if(SUCCEEDED(chain->GetPresentParameters(&p)))Log("PERF D3D9 backbuffer=%ux%u samples=%u quality=%u presentInterval=0x%08X windowed=%d",p.BackBufferWidth,p.BackBufferHeight,(unsigned)p.MultiSampleType,p.MultiSampleQuality,p.PresentationInterval,p.Windowed?1:0);Release(chain);}
    nativeWidth=nativeHeight=0;IDirect3DSurface9* back=nullptr;
    if(d&&SUCCEEDED(d->GetBackBuffer(0,0,D3DBACKBUFFER_TYPE_MONO,&back))){D3DSURFACE_DESC desc{};if(SUCCEEDED(back->GetDesc(&desc))){nativeWidth=desc.Width;nativeHeight=desc.Height;}Release(back);}
    AutoResolution::Confirm(nativeWidth,nativeHeight);
}
void BeforeReset(){Runtime::Guard lock;if(SettingsMenu::Active()){SettingsMenu::Close();referenceSet=false;}resetting=true;replayGuard.Reset();performance=Performance{};gpuTiming.Reset();D3DWorkProfile::Reset();ResetGunCrosshair();ResetGunScope();ReleaseInput();Runtime::SubmitRetained();pairReady=false;hudReady=hudFailed=hudUnpremultiplied=false;batchCapture.Reset();HudCapture::Reset();Engine::ResetBodyResources();flatFallback=false;submittedScene=~0ull;presentationSeen=false;for(auto& c:captures)c.Clear();}
void AfterReset(IDirect3DDevice9* d,HRESULT result){resetting=FAILED(result);if(!resetting)AttachDevice(d);else Log("RESOLUTION60 D3D9 reset failed hr=0x%08X",(unsigned)result);}
void AfterEndScene(IDirect3DDevice9* d){
    if(resetting||!Config().enabled)return;if(!device)AttachDevice(d);
    if(d!=device)return;++sceneSerial;
    if(inPair)return;
    Start();
    // r2 only initialized here and waited forever for Device::Present, which
    // BF2 bypasses. Keep this known-working callback as a startup/watchdog
    // frame pump, even when no presentation hook has fired yet.
    if(!presentationSeen||GetTickCount64()-lastPresentationTick>250){
        static unsigned fallbacks=0;if(++fallbacks<=3||fallbacks%1800==1)Log("BOUNDARY EndScene fallback #%u",fallbacks);
        Pump(true);
    }
}
bool BeforePresent(IDirect3DDevice9* d){
    return PresentationBoundary(d,"device Present");
}
bool BeforeNativePresent(){return PresentationBoundary(device,"native swap-chain Present");}
bool BeginHud(){return inPair&&!resetting&&!hudFailed&&HudCapture::Begin(device);}
bool ReuseHud(){return inPair&&!resetting&&!hudFailed&&Config().singleHudPass&&HudCapture::Ready();}
void RecordHudDraw(double ms){if(inPair)hudDrawMs+=ms;}
void EndHud(){HudCapture::End();}
bool DrawScene(Engine::SceneFunction original,void* self,unsigned a,unsigned b,unsigned c){
    if(inPair||resetting||!device)return original(self,a,b,c);
    // A prior pair can await the outer Present. Do not overwrite its images.
    if(pairReady)return original(self,a,b,c);
    Runtime::Guard frameGuard;
    const double start=FrameClockMs();PairTiming timing{};
    Runtime::Frame f{};
    if(!Runtime::Snapshot(f)||!Engine::BeginPair(f)){frameGuard.Unlock();return original(self,a,b,c);}
    Runtime::MatchEyeTransfer(nativeWidth,nativeHeight);
    flatFallback=false;
    static bool radioStereoWas=false;const bool radioStereo=Engine::RadioOverlayActive();
    if(radioStereo!=radioStereoWas){radioStereoWas=radioStereo;Log("RADIO100 display=%s: native wheel shares the stereo HUD canvas",radioStereo?"VR overlay":"closed");}
    hudReady=hudUnpremultiplied=false;hudDrawMs=0;HudCapture::NewPair();batchCapture.NewPair();
    inPair=true;suppressedPresents=0;bool rendered=true,result=true;
    gpuTiming.Begin(device);D3DWorkProfile::BeginPair();timing.setup=FrameClockMs()-start;
    replayGuard.Configure();
    const double workStart=batchCapture.Measuring()?FrameClockMs():0;
    // The prior frame's desktop composite can also still be in flight.
    bool replayReady=replayGuard.Wait();
    const bool scopeAttempt=replayReady&&Engine::ScopeReady();
    if(replayReady)CaptureGunScope(original,self,c);
    for(int eye=0;eye<2;++eye){
        const double renderStart=FrameClockMs();
        // Scope and eye zero can reuse native geometry before eye one.
        if(!replayReady||((eye>0||scopeAttempt)&&!replayGuard.Wait())){rendered=false;break;}
        if(!Engine::BeginEye(eye)){rendered=false;break;}
        // Keep BF2's graphics cache and device in agreement. Native rendering
        // owns state/targets; readback does not bind anything. Only the first
        // eye advances renderer-side time (a,b are the native double's words).
        D3DWorkProfile::BeginEye(device,(unsigned)eye);
        const bool ok=original(self,eye==0?a:0,eye==0?b:0,c);
        D3DWorkProfile::EndEye();if(ok){DrawGunCrosshair();DrawGunScope();}Engine::EndEye();if(eye==0)result=ok;
        timing.eyes[eye]=FrameClockMs()-renderStart;timing.render+=timing.eyes[eye];
        gpuTiming.Mark(1+2*eye);
        unsigned width=0,height=0;Runtime::EyeSize(eye,width,height);
        const double stageStart=FrameClockMs();
        const bool staged=ok&&width&&(batchCapture.StageEye((unsigned)eye,width,height)||captures[eye].Stage(width,height));timing.stage+=FrameClockMs()-stageStart;
        gpuTiming.Mark(2+2*eye);
        if(!staged){rendered=false;break;}
    }
    gpuTiming.End();
    // Prepare the HUD before acquiring images so it can share the eyes'
    // transfer. Vehicle HUD encoding is corrected before this shared transfer.
    const double hudStart=FrameClockMs();Mat hud{};
    const bool wantHud=rendered&&HudCapture::Ready()&&Engine::HudPlacement(hud,hudWidth,hudHeight)&&Runtime::HudSize(HudCapture::Width(),HudCapture::Height());
    if(wantHud){
        // D3DX uses the same row-vector matrix as BF2. Reflect Z when
        // converting the rotation and position back to OpenXR's RH space.
        D3DXQUATERNION q{};D3DXMATRIX rotation{};memcpy(&rotation,&hud,sizeof(hud));D3DXQuaternionRotationMatrix(&q,&rotation);
        hudPose.orientation={-q.x,-q.y,q.z,q.w};const Vec p=Position(hud);hudPose.position={p.x,p.y,-p.z};
    }
    timing.hud=FrameClockMs()-hudStart;
    if(rendered){
        const auto batch=batchCapture.Upload(wantHud,timing,f);
        if(batch==Runtime::BatchResult::Uploaded)hudReady=wantHud;
        else if(batch==Runtime::BatchResult::Failed)rendered=false;
        else{
            // Unsupported packing falls back to the already-resolved SAME
            // eye pair; never reuse yesterday's image with today's pose.
            for(int eye=0;eye<2;++eye)if(!captures[eye].Upload(eye,timing.read,timing.upload)){rendered=false;break;}
            if(rendered&&wantHud)hudReady=HudCapture::Upload(timing.read,timing.upload);
        }
    }
    hudUnpremultiplied=hudReady&&HudCapture::Unpremultiplied();
    static unsigned reports=0;if(hudReady&&reports++<3)Log("HUD shared canvas submitted once for both eyes: %.2fm x %.2fm depth=%.2fm scale=%.2f",hudWidth,hudHeight,Tuning().hudDepth,Tuning().hudScale);
    if(rendered&&HudCapture::Ready()&&!hudReady){hudFailed=true;Log("HUD readback/placement/upload failed; native HUD restored from next frame");}
    timing.hudDraw=hudDrawMs;const auto transfer=Runtime::UploadTiming();timing.uploadWait=transfer.wait;timing.uploadCopy=transfer.copy;
    timing.uploadMap=transfer.map;timing.uploadWrite=transfer.write;timing.uploadSubmit=transfer.submit;
    Engine::EndPair();inPair=false;pairReady=rendered;
    batchCapture.Observe(workStart?FrameClockMs()-workStart:0,timing,rendered,!scopeAttempt,f.id);
    if(rendered){if(++pairCount==1||pairCount%1800==1)Log("SAMEFRAME pair=%u frame=%llu ipd=%.5f suppressedPresents=%u native views restored",pairCount,f.id,Length(Sub(Position(f.eyes[0]),Position(f.eyes[1]))),suppressedPresents);}
    else{static unsigned failures=0;if(++failures<=8)Log("STEREO pair capture failed #%u; submitting no world layer",failures);}
    // A completed pair can submit immediately; it must not depend on a later
    // device callback. Preserve BF2's selected native swap chain/mode when
    // flushing the deferred desktop Present, including its error handling.
    const double finishStart=FrameClockMs();if(!rendered)Runtime::SubmitEmpty();
    Pump();
    timing.finish=FrameClockMs()-finishStart;const double desktopStart=FrameClockMs();
    // Reuse an already resolved eye after its headset upload. Crop only the
    // desktop world; the complete isolated HUD is composited on top afterward.
    IDirect3DSurface9* mirror=nullptr;RECT crop{};
    const float desktopAspect=RenderWindow::DesktopAspect();
    if(rendered&&desktopAspect>0){
        if(batchCapture.mirrorReady){
            const auto& r=batchCapture.mirrorRegion;mirror=batchCapture.target;
            crop={(LONG)r.x,(LONG)r.y,(LONG)(r.x+r.width),(LONG)(r.y+r.height)};
        }else if(captures[1].target){
            mirror=captures[1].target;crop={0,0,(LONG)captures[1].width,(LONG)captures[1].height};
        }
        const auto& fov=f.views[1].fov;
        const float eyeAspect=(std::tan(fov.angleRight)-std::tan(fov.angleLeft))/
            (std::tan(fov.angleUp)-std::tan(fov.angleDown));
        if(mirror&&std::isfinite(eyeAspect)&&eyeAspect>.1f&&eyeAspect<10.f){
            // Use the angular projection aspect, not the eye texture's pixel
            // aspect; headset render pixels need not have equal angular scale.
            const LONG w=crop.right-crop.left,h=crop.bottom-crop.top;
            const LONG cw=std::max(1L,std::min(w,(LONG)std::floor(w*desktopAspect/eyeAspect)));
            const LONG ch=std::max(1L,std::min(h,(LONG)std::floor(h*eyeAspect/desktopAspect)));
            crop.left+=(w-cw)/2;crop.top+=(h-ch)/2;crop.right=crop.left+cw;crop.bottom=crop.top+ch;
        }else mirror=nullptr;
    }
    HudCapture::CompositeDesktop(mirror,mirror?&crop:nullptr);
    if(suppressedPresents&&!Engine::FlushDeferredPresent())device->Present(nullptr,nullptr,nullptr,nullptr);
    timing.desktop=FrameClockMs()-desktopStart;timing.total=FrameClockMs()-start;performance.Add(f,start,timing,rendered);
    D3DWorkProfile::EndPair(rendered);
    return result;
}
}
