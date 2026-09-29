// Sparse D3D9 GPU timestamps. This measures elapsed GPU timeline intervals,
// including possible submission gaps, not exclusive GPU execution time.
// Read older samples once per sampling opportunity; never wait or force-flush.
struct GpuTiming {
    struct Sample {
        IDirect3DQuery9 *ticks[5]{},*frequency=nullptr,*disjoint=nullptr;
        bool pending=false,open=false;unsigned mask=0;double issued=0;
        void Clear(){for(auto& q:ticks)Release(q);Release(frequency);Release(disjoint);pending=open=false;mask=0;issued=0;}
        HRESULT Create(IDirect3DDevice9* d){
            if(frequency)return S_OK;
            HRESULT hr=d->CreateQuery(D3DQUERYTYPE_TIMESTAMPDISJOINT,&disjoint);
            if(SUCCEEDED(hr))hr=d->CreateQuery(D3DQUERYTYPE_TIMESTAMPFREQ,&frequency);
            for(auto& q:ticks)if(SUCCEEDED(hr))hr=d->CreateQuery(D3DQUERYTYPE_TIMESTAMP,&q);
            return hr;
        }
    } samples[3];
    Sample* active=nullptr;bool disabled=false,announced=false;
    unsigned opportunities=0,count=0,discarded=0;
    double window=0,eye0=0,eye1=0,resolve=0,total=0,maximum=0;
    void Close(){
        if(active&&active->open){active->disjoint->Issue(D3DISSUE_END);active->open=false;}
        active=nullptr;
    }
    void Reset(){Close();for(auto& s:samples)s.Clear();*this=GpuTiming{};}
    void Disable(const char* step,HRESULT hr){
        Close();for(auto& s:samples)s.Clear();disabled=true;
        Log("PERF GPU timestamps disabled: %s hr=%08X; rendering continues unchanged",step,(unsigned)hr);
    }
    void Poll(double now){
        for(auto& s:samples){
            if(!s.pending)continue;
            BOOL disjoint=TRUE;UINT64 freq=0,t[5]{};
            HRESULT hr=s.disjoint->GetData(&disjoint,sizeof(disjoint),0);
            if(hr==S_OK)hr=s.frequency->GetData(&freq,sizeof(freq),0);
            for(unsigned i=0;i<5&&hr==S_OK;++i)hr=s.ticks[i]->GetData(&t[i],sizeof(t[i]),0);
            if(FAILED(hr)){Disable("read",hr);return;}
            if(hr!=S_OK){
                // A driver may never return an unflushed query. Give up on
                // profiling instead of adding a flush or blocking the frame.
                if(now-s.issued>10000){Disable("result timeout",S_FALSE);return;}
                continue;
            }
            s.pending=false;
            bool valid=!disjoint&&freq;
            for(unsigned i=1;i<5;++i)valid=valid&&t[i]>=t[i-1];
            if(!valid){++discarded;continue;}
            const double factor=1000.0/(double)freq;
            const double duration=(double)(t[4]-t[0])*factor;
            if(!std::isfinite(duration)||duration>1000){++discarded;continue;}
            ++count;eye0+=(double)(t[1]-t[0])*factor;eye1+=(double)(t[3]-t[2])*factor;
            resolve+=(double)((t[2]-t[1])+(t[4]-t[3]))*factor;
            total+=duration;maximum=std::max(maximum,duration);
        }
        if(!window)window=now;
        if(now-window>=5000&&(count||discarded)){
            const double n=count?(double)count:1.0;
            Log("PERF gpuMs samples=%u discarded=%u eye0=%.2f eye1=%.2f resolves=%.2f total=%.2f max=%.2f (sparse GPU timeline; includes submission gaps; excludes atlas/readback/upload)",count,discarded,eye0/n,eye1/n,resolve/n,total/n,maximum);
            count=discarded=0;eye0=eye1=resolve=total=maximum=0;window=now;
        }
    }
    void Begin(IDirect3DDevice9* d){
        if(!Config().gpuTiming||disabled||++opportunities%32)return;
        const double now=FrameClockMs();Poll(now);if(disabled)return;
        for(auto& s:samples)if(!s.pending){
            const HRESULT created=s.Create(d);
            if(FAILED(created)){Disable("create",created);return;}
            active=&s;s.mask=0;s.issued=now;
            const HRESULT begun=s.disjoint->Issue(D3DISSUE_BEGIN);
            if(FAILED(begun)){Disable("begin",begun);return;}
            s.open=true;Mark(0);
            if(!announced&&!disabled){announced=true;Log("PERF GPU timestamps active: one pair in 32, later-frame polling, no forced flush");}
            return;
        }
        // All slots are pending. Skip this sample; never recycle an in-flight query.
    }
    void Mark(unsigned index){
        if(!active||index>=5)return;
        const HRESULT hr=active->ticks[index]->Issue(D3DISSUE_END);
        if(FAILED(hr)){Disable("timestamp",hr);return;}
        active->mask|=1u<<index;
    }
    void End(){
        if(!active)return;
        Sample* s=active;
        HRESULT hr=s->frequency->Issue(D3DISSUE_END);
        const HRESULT ended=s->disjoint->Issue(D3DISSUE_END);s->open=false;
        if(SUCCEEDED(hr))hr=ended;
        if(FAILED(hr)){Disable("end",hr);return;}
        s->pending=s->mask==31;
        if(!s->pending)++discarded;
        active=nullptr;
    }
} gpuTiming;
