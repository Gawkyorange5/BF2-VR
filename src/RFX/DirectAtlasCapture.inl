// The established path resolves each eye, then copies it into an atlas.
// Here a supported color StretchRect resolves straight into that eye's atlas
// rectangle. No render target is rebound; BF2's device-state cache is intact.
// Driver rejection uses the original resolve followed by an ordinary pack.
struct BatchCapture {
    IDirect3DSurface9 *target=nullptr,*cpu=nullptr;
    unsigned width=0,height=0,directMask=0;
    Runtime::TransferRegion directRegions[2]{};
    bool disabled=false,reported=false,directBlocked=false,directReported=false;
    bool lockable=false,lockableBlocked=false,architectureChecked=false,preferLockable=false,readReported=false;
    void Clear(){Release(target);Release(cpu);width=height=directMask=0;lockable=false;}
    void Reset(){
        Clear();disabled=reported=directBlocked=directReported=false;
        lockableBlocked=architectureChecked=preferLockable=readReported=false;Runtime::ResetTransfer();
    }
    void NewPair(){directMask=0;}
    void ReadbackPreference(const D3DCAPS9& caps){
        if(architectureChecked)return;
        architectureChecked=true;
        if(!Config().umaDirectReadback)return;
        IDirect3D9* api=nullptr;D3DADAPTER_IDENTIFIER9 adapter{};
        HRESULT hr=device->GetDirect3D(&api);
        if(SUCCEEDED(hr)&&api)hr=api->GetAdapterIdentifier(caps.AdapterOrdinal,0,&adapter);
        else hr=E_FAIL;
        Release(api);
        preferLockable=SUCCEEDED(hr)&&caps.DeviceType==D3DDEVTYPE_HAL&&
            Runtime::UnifiedMemoryAdapter(adapter.VendorId,adapter.DeviceId,adapter.SubSysId,adapter.Revision);
        Log("PERF directReadback candidate=%d adapterQuery=%08X; driver UMA and matching game adapter required",preferLockable?1:0,(unsigned)hr);
    }
    bool RecoverEyes(){
        for(unsigned eye=0;eye<2;++eye){
            const unsigned bit=1u<<eye;
            if((directMask&bit)&&!captures[eye].FromAtlas(target,directRegions[eye]))return false;
            directMask&=~bit;
        }
        return true;
    }
    Runtime::BatchResult Disable(const char* reason,HRESULT hr=E_FAIL){
        // Preserve any direct eyes before clearing the atlas; the caller's
        // established per-eye fallback then receives this frame's pixels.
        const bool recovered=RecoverEyes();
        Log("PERF batch transfer fallback: %s hr=%08X recovery=%d; independent transfers until device reset",reason,(unsigned)hr,recovered?1:0);
        Clear();Runtime::ResetTransfer();disabled=true;
        return recovered?Runtime::BatchResult::Unavailable:Runtime::BatchResult::Failed;
    }
    HRESULT Ensure(unsigned w,unsigned h){
        if(target&&width==w&&height==h&&!(lockable&&lockableBlocked))return S_OK;
        // Never release an eye's only current copy during a layout change.
        if(directMask)return D3DERR_INVALIDCALL;
        D3DCAPS9 caps{};HRESULT hr=device->GetDeviceCaps(&caps);
        if(SUCCEEDED(hr)&&(!w||!h||w>caps.MaxTextureWidth||h>caps.MaxTextureHeight))hr=D3DERR_NOTAVAILABLE;
        if(FAILED(hr))return hr;
        ReadbackPreference(caps);
        IDirect3DSurface9* nextTarget=nullptr;
        bool nextLockable=preferLockable&&!lockableBlocked;
        hr=device->CreateRenderTarget(w,h,D3DFMT_A8R8G8B8,D3DMULTISAMPLE_NONE,0,nextLockable?TRUE:FALSE,&nextTarget,nullptr);
        if(FAILED(hr)&&nextLockable){
            Log("PERF directReadback allocation rejected hr=%08X; ordinary atlas until reset",(unsigned)hr);
            Release(nextTarget);lockableBlocked=true;nextLockable=false;
            hr=device->CreateRenderTarget(w,h,D3DFMT_A8R8G8B8,D3DMULTISAMPLE_NONE,0,FALSE,&nextTarget,nullptr);
        }
        if(FAILED(hr)){Release(nextTarget);return hr;}
        Clear();target=nextTarget;lockable=nextLockable;width=w;height=h;return S_OK;
    }
    HRESULT LockImage(D3DLOCKED_RECT& mapped,IDirect3DSurface9*& mappedSurface,bool& direct){
        direct=false;mappedSurface=nullptr;
        if(lockable&&!lockableBlocked){
            const HRESULT hr=target->LockRect(&mapped,nullptr,D3DLOCK_READONLY);
            if(SUCCEEDED(hr)){
                mappedSurface=target;direct=true;return hr;
            }
            // Lock failure has not acquired XR images or modified the atlas.
            // Read the SAME current pixels through the established path, then
            // use an ordinary atlas starting with the next stereo pair.
            lockableBlocked=true;
            Log("PERF directReadback lock rejected hr=%08X; same-frame readback fallback",(unsigned)hr);
        }
        HRESULT hr=S_OK;
        if(!cpu)hr=device->CreateOffscreenPlainSurface(width,height,D3DFMT_A8R8G8B8,D3DPOOL_SYSTEMMEM,&cpu,nullptr);
        if(SUCCEEDED(hr))hr=device->GetRenderTargetData(target,cpu);
        if(SUCCEEDED(hr))hr=cpu->LockRect(&mapped,nullptr,D3DLOCK_READONLY);
        if(SUCCEEDED(hr))mappedSurface=cpu;
        return hr;
    }
    HRESULT PackImages(IDirect3DSurface9** sources,const Runtime::TransferRegion* regions,unsigned count,unsigned w,unsigned h){
        for(unsigned attempt=0;attempt<2;++attempt){
            HRESULT hr=S_OK;
            for(unsigned i=0;i<count;++i){
                if(i<2&&(directMask&(1u<<i)))continue;
                const auto& r=regions[i];const RECT rect{(LONG)r.x,(LONG)r.y,(LONG)(r.x+r.width),(LONG)(r.y+r.height)};
                hr=device->StretchRect(sources[i],nullptr,target,&rect,D3DTEXF_NONE);
                if(FAILED(hr))break;
            }
            if(SUCCEEDED(hr)||!lockable)return hr;
            lockableBlocked=true;
            Log("PERF directReadback packing rejected hr=%08X; recovering to ordinary atlas",(unsigned)hr);
            if(!RecoverEyes())return E_FAIL;
            sources[0]=captures[0].target;sources[1]=captures[1].target;
            hr=Ensure(w,h);if(FAILED(hr))return hr;
        }
        return E_FAIL;
    }
    bool StageEye(unsigned eye,unsigned eyeWidth,unsigned eyeHeight){
        if(eye>1||disabled||directBlocked||!Config().batchTransfer||!Config().directEyeAtlas)return false;
        // The native HUD exists after eye zero. Its dimensions let us reserve
        // the final layout without another whole-atlas resize/copy afterward.
        // A missing HUD keeps the original two-eye path with no unused rows.
        if(!HudCapture::Ready()||!HudCapture::Width()||!HudCapture::Height())return false;
        Runtime::TransferRegion plan[2]{};unsigned w=HudCapture::Width(),h=0;
        for(unsigned e=0;e<2;++e){
            unsigned ew=0,eh=0;Runtime::EyeSize((int)e,ew,eh);
            if(!ew||!eh||ew>16384||eh>16384||h>16384-eh)return false;
            plan[e]={(int)e,0,h,ew,eh};w=std::max(w,ew);h+=eh;
        }
        if(HudCapture::Height()>16384||h>16384-HudCapture::Height())return false;
        h+=HudCapture::Height();
        if(w>16384||plan[eye].width!=eyeWidth||plan[eye].height!=eyeHeight)return false;
        if(directMask&&(width!=w||height!=h))return false;
        for(unsigned e=0;e<2;++e)if((directMask&(1u<<e))&&
            (directRegions[e].y!=plan[e].y||directRegions[e].width!=plan[e].width||directRegions[e].height!=plan[e].height))return false;
        IDirect3DSurface9* back=nullptr;D3DSURFACE_DESC desc{};
        HRESULT hr=device->GetBackBuffer(0,0,D3DBACKBUFFER_TYPE_MONO,&back);
        if(SUCCEEDED(hr))hr=back->GetDesc(&desc);
        // Only combine same-size color resolves. Resampling and other formats
        // retain the original per-eye path. X8 alpha is not used by the world
        // projection layer; the separate A8 HUD retains its own alpha.
        if(FAILED(hr)||desc.Width!=eyeWidth||desc.Height!=eyeHeight||
            (desc.Format!=D3DFMT_X8R8G8B8&&desc.Format!=D3DFMT_A8R8G8B8)){Release(back);return false;}
        hr=Ensure(w,h);
        if(SUCCEEDED(hr)){
            const auto& r=plan[eye];const RECT rect{0,(LONG)r.y,(LONG)r.width,(LONG)(r.y+r.height)};
            hr=device->StretchRect(back,nullptr,target,&rect,D3DTEXF_NONE);
        }
        Release(back);
        if(FAILED(hr)){
            if(lockable){
                lockableBlocked=true;
                Log("PERF directReadback resolve rejected hr=%08X; recovering this pair to ordinary atlas",(unsigned)hr);
                return false;
            }
            directBlocked=true;
            Log("PERF directAtlas unavailable hr=%08X samples=%u; original separate resolves until reset",(unsigned)hr,(unsigned)desc.MultiSampleType);
            return false;
        }
        directRegions[eye]=plan[eye];directMask|=1u<<eye;
        if(!directReported){directReported=true;Log("PERF directAtlas active source=%ux%u samples=%u atlas=%ux%u; native-resolution color resolve directly into eye region",eyeWidth,eyeHeight,(unsigned)desc.MultiSampleType,w,h);}
        return true;
    }
    Runtime::BatchResult Upload(bool includeHud,PairTiming& timing){
        if(disabled||!Config().batchTransfer)return Runtime::BatchResult::Unavailable;
        const double stageStart=FrameClockMs();
        IDirect3DSurface9* sources[3]={captures[0].target,captures[1].target,includeHud?HudCapture::ResolvedForTransfer():nullptr};
        const unsigned count=includeHud?3u:2u;unsigned w=0,h=0;
        Runtime::TransferRegion regions[3]{};
        for(unsigned i=0;i<count;++i){
            D3DSURFACE_DESC desc{};
            if(i<2&&(directMask&(1u<<i))){desc.Width=directRegions[i].width;desc.Height=directRegions[i].height;}
            else if(!sources[i]||FAILED(sources[i]->GetDesc(&desc))){
                timing.stage+=FrameClockMs()-stageStart;return Disable("invalid resolved source");
            }
            if(!desc.Width||!desc.Height||desc.Width>16384||desc.Height>16384||h>16384-desc.Height){
                timing.stage+=FrameClockMs()-stageStart;return Disable("invalid resolved extent");
            }
            regions[i]={i==2?3:(int)i,0,h,desc.Width,desc.Height};w=std::max(w,desc.Width);h+=desc.Height;
        }
        bool layoutMatches=width==w&&height==h&&!(lockable&&lockableBlocked);
        for(unsigned e=0;e<2;++e)if((directMask&(1u<<e))&&
            (directRegions[e].y!=regions[e].y||directRegions[e].width!=regions[e].width||directRegions[e].height!=regions[e].height))layoutMatches=false;
        if(directMask&&!layoutMatches){
            if(!RecoverEyes()){timing.stage+=FrameClockMs()-stageStart;Disable("layout recovery failed");return Runtime::BatchResult::Failed;}
            sources[0]=captures[0].target;sources[1]=captures[1].target;
        }
        const HRESULT allocation=Ensure(w,h);
        if(FAILED(allocation)){timing.stage+=FrameClockMs()-stageStart;return Disable("create D3D9 atlas",allocation);}
        const HRESULT packed=PackImages(sources,regions,count,w,h);
        if(FAILED(packed)){timing.stage+=FrameClockMs()-stageStart;return Disable("pack resolved image",packed);}
        const double readStart=FrameClockMs();timing.stage+=readStart-stageStart;
        D3DLOCKED_RECT mapped{};IDirect3DSurface9* mappedSurface=nullptr;bool directRead=false;
        const HRESULT hr=LockImage(mapped,mappedSurface,directRead);
        if(FAILED(hr)){timing.read+=FrameClockMs()-readStart;return Disable("read packed image",hr);}
        const double uploadStart=FrameClockMs();timing.read+=uploadStart-readStart;
        const auto result=mapped.pBits&&mapped.Pitch>0&&(unsigned)mapped.Pitch>=width*4u?
            Runtime::UploadAtlas(static_cast<const BYTE*>(mapped.pBits),(unsigned)mapped.Pitch,width,height,regions,count):Runtime::BatchResult::Unavailable;
        const HRESULT unlock=mappedSurface->UnlockRect();timing.upload+=FrameClockMs()-uploadStart;
        if(FAILED(unlock)){
            // A failed unlock makes further use of this resource unsafe. Do
            // not recover from it or retry after XR images may have been used.
            Log("PERF atlas unlock failed hr=%08X; dropping pair and disabling batch transfer",(unsigned)unlock);
            directMask=0;Clear();Runtime::ResetTransfer();disabled=true;
            return Runtime::BatchResult::Failed;
        }
        if(result==Runtime::BatchResult::Uploaded){
            ++timing.batches;timing.directEyes+=(directMask&1u)+((directMask>>1)&1u);
            if(directRead){
                ++timing.directReads;
                if(!readReported){readReported=true;Log("PERF directReadback active atlas=%ux%u; LockRect on completed staging target, no GetRenderTargetData or separate CPU atlas",width,height);}
            }
            if(!reported){reported=true;Log("PERF batch transfer active atlas=%ux%u surfaces=%u; one readback, one upload, one flush; original per-eye pixels/poses",width,height,count);}
            directMask=0;
        }else if(result==Runtime::BatchResult::Failed){
            // XR acquisition may have started. Drop this pair; do not retry
            // through independent uploads after a partial XR operation.
            directMask=0;Disable("XR transfer failed; dropping current pair");
        }else return Disable("D3D11 batch unsupported");
        return result;
    }
} batchCapture;
