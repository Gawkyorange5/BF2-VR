// Some legacy renderer resources are reused by the second world replay.
// Compatibility mode waits for their GPU consumers before another replay,
// and uses same-size MSAA resolves instead of resolving into atlas subrects.
// This is a private compatibility experiment, not a confirmed driver diagnosis.
struct GpuReplayGuard {
    IDirect3DQuery9* event=nullptr;
    bool checked=false,uma=false,active=false,reported=false,queryUnavailable=false;
    unsigned failures=0;
    IDirect3DSurface9 *resolve=nullptr,*cpu=nullptr;
    void Reset(){Release(event);Release(resolve);Release(cpu);checked=uma=active=reported=queryUnavailable=false;failures=0;}
    void Configure(){
        if(!checked){
            checked=true;IDirect3D9* api=nullptr;D3DCAPS9 caps{};D3DADAPTER_IDENTIFIER9 id{};
            if(SUCCEEDED(device->GetDeviceCaps(&caps))&&SUCCEEDED(device->GetDirect3D(&api))&&api&&
                SUCCEEDED(api->GetAdapterIdentifier(caps.AdapterOrdinal,0,&id)))
                uma=Runtime::UnifiedMemoryAdapter(id.VendorId,id.DeviceId,id.SubSysId,id.Revision);
            Release(api);
        }
        const int mode=(int)Tuning().gpuCompatibility;
        const bool next=mode==1||(mode==0&&!uma);
        if(!reported||active!=next){
            reported=true;Log("GRAPHICS48 compatibility=%d enabled=%d matchingUMA=%d; GPU replay waits and separate MSAA resolves",mode,next?1:0,uma?1:0);
        }
        active=next;
    }
    bool ReadbackWait(){
        IDirect3DSurface9* back=nullptr;D3DSURFACE_DESC d{},old{};
        HRESULT hr=device->GetBackBuffer(0,0,D3DBACKBUFFER_TYPE_MONO,&back);
        if(SUCCEEDED(hr))hr=back->GetDesc(&d);
        if(SUCCEEDED(hr)&&resolve){resolve->GetDesc(&old);if(old.Width!=d.Width||old.Height!=d.Height||old.Format!=d.Format){Release(resolve);Release(cpu);}}
        if(SUCCEEDED(hr)&&!resolve)hr=device->CreateRenderTarget(d.Width,d.Height,d.Format,D3DMULTISAMPLE_NONE,0,FALSE,&resolve,nullptr);
        if(SUCCEEDED(hr)&&!cpu)hr=device->CreateOffscreenPlainSurface(d.Width,d.Height,d.Format,D3DPOOL_SYSTEMMEM,&cpu,nullptr);
        if(SUCCEEDED(hr))hr=device->StretchRect(back,nullptr,resolve,nullptr,D3DTEXF_NONE);
        if(SUCCEEDED(hr))hr=device->GetRenderTargetData(resolve,cpu);
        Release(back);return SUCCEEDED(hr);
    }
    bool Wait(){
        if(!active)return true;
        if(!event&&!queryUnavailable){
            const HRESULT hr=device->CreateQuery(D3DQUERYTYPE_EVENT,&event);
            if(FAILED(hr)||!event){queryUnavailable=true;Log("GRAPHICS48 event query unavailable hr=%08X; synchronous readback fallback",(unsigned)hr);}
        }
        if(queryUnavailable)return ReadbackWait();
        HRESULT hr=event->Issue(D3DISSUE_END);const double start=FrameClockMs();
        if(SUCCEEDED(hr))do{
            hr=event->GetData(nullptr,0,D3DGETDATA_FLUSH);
            if(hr!=S_FALSE)break;
            if(FrameClockMs()-start>250.)break;
            // Yield after the initial short poll, rather than spinning for a stalled GPU.
            if(FrameClockMs()-start>1.)SwitchToThread();
        }while(true);
        if(hr==S_OK)return true;
        if(++failures<=4)Log("GRAPHICS48 GPU replay wait failed hr=%08X; abandoning this pair",(unsigned)hr);
        Release(event);return false;
    }
} replayGuard;
