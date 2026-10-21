// Observe pixels that are already CPU-readable for XR upload. This adds no
// readback, GPU query, full-image scan or per-frame allocation. S_OK alone
// cannot establish that Direct3D produced a new image.
struct CaptureFreshness {
    ULONGLONG lastProbe=0,lastChanged=0,lastReport=0;
    std::uint64_t previous=0,origin=0;
    unsigned oldWidth=0,oldHeight=0;
    Mat anchor{};float turn=0,move=0;bool ready=false,reported=false;
    bool Observe(const BYTE* pixels,unsigned pitch,const Runtime::TransferRegion* regions,
                 const Runtime::Frame& frame,bool direct){
        const auto now=GetTickCount64();
        if(!pixels||!frame.valid||!frame.focused||!frame.shouldRender||!Rigid(frame.head)){
            ready=false;return false;
        }
        if(lastProbe&&now>=lastProbe&&now-lastProbe<100)return false;
        const bool gap=lastProbe&&(now<lastProbe||now-lastProbe>500);
        lastProbe=now;
        std::uint64_t hash=14695981039346656037ull;unsigned contrast=0;
        for(unsigned eye=0;eye<2;++eye){
            const auto& r=regions[eye];if(r.width<16||r.height<16)return false;
            unsigned previousColor=0;
            for(unsigned y=0;y<8;++y)for(unsigned x=0;x<8;++x){
                const unsigned sx=r.x+(2*x+1)*r.width/16,sy=r.y+(2*y+1)*r.height/16;
                unsigned color=0;memcpy(&color,pixels+(size_t)sy*pitch+sx*4,4);color&=0x00ffffffu;
                hash^=color;hash*=1099511628211ull;
                if((x||y)&&color!=previousColor)++contrast;previousColor=color;
            }
        }
        const auto& first=regions[0];
        if(!ready||gap||origin!=frame.origin||oldWidth!=first.width||oldHeight!=first.height||hash!=previous){
            if(ready&&reported&&hash!=previous)Log("CAPTURE113 image samples changing again frame=%llu direct=%d",frame.id,direct?1:0);
            ready=true;reported=false;lastChanged=now;previous=hash;origin=frame.origin;
            oldWidth=first.width;oldHeight=first.height;anchor=frame.head;turn=move=0;
        }else{
            turn=std::max(turn,1.f-std::min(Dot(Row(anchor,2),Row(frame.head,2)),Dot(Row(anchor,1),Row(frame.head,1))));
            move=std::max(move,Length(Sub(Position(anchor),Position(frame.head))));
        }
        // A stopped death camera or an open settings screen is not evidence
        // of failed capture. Recovery is limited to focused, living play.
        const bool stale=!reported&&now-lastChanged>=1500&&contrast>=16&&(turn>.004f||move>.035f)&&
            !SettingsMenu::Active()&&Engine::LocalAlive();
        if(stale||!lastReport||now-lastReport>=5000){
            lastReport=now;
            const HRESULT gameStatus=device->TestCooperativeLevel(),xrStatus=Runtime::GraphicsStatus();
            D3DVIEWPORT9 viewport{};DWORD colorWrite=0;
            const HRESULT vp=device->GetViewport(&viewport),cw=device->GetRenderState(D3DRS_COLORWRITEENABLE,&colorWrite);
            Log("CAPTURE113 frame=%llu hash=%016llX unchangedMs=%llu headTurn=%.5f headMove=%.3f contrast=%u direct=%d stale=%d d3d9=%08X d3d11=%08X viewport=%u,%u,%u,%u vpHr=%08X colorWrite=%X cwHr=%08X",
                frame.id,hash,now-lastChanged,turn,move,contrast,direct?1:0,stale?1:0,(unsigned)gameStatus,(unsigned)xrStatus,
                viewport.X,viewport.Y,viewport.Width,viewport.Height,(unsigned)vp,(unsigned)colorWrite,(unsigned)cw);
        }
        if(stale)reported=true;
        return stale;
    }
};
