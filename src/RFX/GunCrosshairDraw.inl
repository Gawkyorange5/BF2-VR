// Tiny D3D9 overlay in the existing eye image. No new eye rendering, texture,
// readback, upload or OpenXR layer. Device state is restored to BF2's cache.
IDirect3DStateBlock9* gunCrosshairState=nullptr;
void ResetGunCrosshair(){Release(gunCrosshairState);Engine::GunCrosshairAvailable(true);}
bool SetGunCrosshairState(){
    bool ok=true;
    const auto check=[&](HRESULT hr){ok=SUCCEEDED(hr)&&ok;};
    check(device->SetVertexShader(nullptr));check(device->SetPixelShader(nullptr));
    check(device->SetFVF(D3DFVF_XYZRHW|D3DFVF_DIFFUSE));check(device->SetTexture(0,nullptr));
    check(device->SetStreamSourceFreq(0,1));
    for(auto s:{D3DRS_FOGENABLE,D3DRS_LIGHTING,D3DRS_ZENABLE,D3DRS_ZWRITEENABLE,
               D3DRS_ALPHATESTENABLE,D3DRS_SCISSORTESTENABLE,D3DRS_STENCILENABLE,
               D3DRS_SRGBWRITEENABLE,D3DRS_ALPHABLENDENABLE,D3DRS_SEPARATEALPHABLENDENABLE})
        check(device->SetRenderState(s,FALSE));
    check(device->SetRenderState(D3DRS_CULLMODE,D3DCULL_NONE));
    check(device->SetRenderState(D3DRS_FILLMODE,D3DFILL_SOLID));
    check(device->SetRenderState(D3DRS_COLORWRITEENABLE,15));
    check(device->SetRenderState(D3DRS_CLIPPLANEENABLE,0));
    check(device->SetRenderState(D3DRS_MULTISAMPLEMASK,0xffffffff));
    check(device->SetTextureStageState(0,D3DTSS_COLOROP,D3DTOP_SELECTARG1));
    check(device->SetTextureStageState(0,D3DTSS_COLORARG1,D3DTA_DIFFUSE));
    check(device->SetTextureStageState(0,D3DTSS_ALPHAOP,D3DTOP_SELECTARG1));
    check(device->SetTextureStageState(0,D3DTSS_ALPHAARG1,D3DTA_DIFFUSE));
    check(device->SetTextureStageState(0,D3DTSS_TEXTURETRANSFORMFLAGS,D3DTTFF_DISABLE));
    check(device->SetTextureStageState(1,D3DTSS_COLOROP,D3DTOP_DISABLE));
    return ok;
}
void DrawGunCrosshair(){
    float x=0,y=0,sx=0,sy=0,hitAlpha=0;DWORD hitColor=0x00ffffff;const bool crosshair=Engine::GunCrosshair(x,y,sx,sy,hitAlpha,hitColor);
    Engine::BodySlotMarker slots[7];const unsigned slotCount=Engine::BodySlotMarkers(slots);
    float dx=0,dy=0,dsx=0,dsy=0,progress=0;bool ready=false;
    const bool defib=Engine::DefibIndicator(dx,dy,dsx,dsy,progress,ready);
    if(!crosshair&&!slotCount&&!defib)return;
    bool ok=true;
    if(!gunCrosshairState)ok=SUCCEEDED(device->CreateStateBlock(D3DSBT_ALL,&gunCrosshairState));
    if(ok)ok=SUCCEEDED(gunCrosshairState->Capture());
    if(!ok){Release(gunCrosshairState);Engine::GunCrosshairAvailable(false);Log("AIM38 state capture failed; retaining native crosshair from next pair");return;}
    IDirect3DSurface9 *previous=nullptr,*back=nullptr,*depth=nullptr;bool targetChanged=false;
    ok=SUCCEEDED(device->GetRenderTarget(0,&previous))&&SUCCEEDED(device->GetBackBuffer(0,0,D3DBACKBUFFER_TYPE_MONO,&back));
    D3DSURFACE_DESC desc{};if(ok)ok=SUCCEEDED(back->GetDesc(&desc));
    if(ok){
        const HRESULT depthResult=device->GetDepthStencilSurface(&depth);
        ok=SUCCEEDED(depthResult)||depthResult==D3DERR_NOTFOUND;
    }
    if(ok){
        ok=SUCCEEDED(device->SetRenderTarget(0,back));targetChanged=ok;
        if(ok)ok=SUCCEEDED(device->SetDepthStencilSurface(nullptr));
        if(ok&&SUCCEEDED(device->BeginScene())){
            D3DVIEWPORT9 vp{0,0,desc.Width,desc.Height,0,1};
            ok=SUCCEEDED(device->SetViewport(&vp))&&SetGunCrosshairState();
            if(ok&&crosshair){
                // Screen-space stroke widths, world-space center. Rounded
                // pixels would make the marker step during slow aiming.
                x=(x+1)*.5f*desc.Width-.5f;y=(1-y)*.5f*desc.Height-.5f;
                sx=std::max(3.f,sx*.5f*desc.Width);sy=std::max(3.f,sy*.5f*desc.Height);
                const float stroke=std::max(1.f,std::min(sx,sy)*.22f),outline=std::max(.8f,stroke*.7f);
                const float gap=std::max(1.6f,std::min(sx,sy)*.36f);
                struct Vertex{float x,y,z,w;DWORD color;};Vertex vertices[48];unsigned count=0;
                const auto rect=[&](float l,float t,float r,float b,DWORD color){
                    const Vertex a{x+l,y+t,0,1,color},c{x+r,y+b,0,1,color};
                    vertices[count++]=a;vertices[count++]={c.x,a.y,0,1,color};vertices[count++]=c;
                    vertices[count++]=a;vertices[count++]=c;vertices[count++]={a.x,c.y,0,1,color};
                };
                for(int pass=0;pass<2;++pass){
                    const float border=pass?0:outline,h=stroke*.5f+border;const DWORD color=pass?0xffffffff:0xff101010;
                    rect(-sx-border,-h,-gap+border,h,color);rect(gap-border,-h,sx+border,h,color);
                    rect(-h,-sy-border,h,-gap+border,color);rect(-h,gap-border,h,sy+border,color);
                }
                ok=SUCCEEDED(device->DrawPrimitiveUP(D3DPT_TRIANGLELIST,count/3,vertices,sizeof(Vertex)));
                if(ok&&hitAlpha>.001f){
                    // Four diagonal ticks around the SAME per-eye gun point.
                    // Native hit strength controls the fade; no new texture,
                    // scene render, transfer, or persistent GPU resource.
                    count=0;
                    const DWORD alpha=static_cast<DWORD>(std::min(1.f,hitAlpha)*255.f+.5f)<<24;
                    const auto line=[&](float ax,float ay,float bx,float by,float width,DWORD color){
                        const float length=std::hypot(bx-ax,by-ay);
                        const float nx=-(by-ay)/length*width*.5f,ny=(bx-ax)/length*width*.5f;
                        const Vertex a{x+ax+nx,y+ay+ny,0,1,color},b{x+bx+nx,y+by+ny,0,1,color};
                        const Vertex c{x+bx-nx,y+by-ny,0,1,color},d{x+ax-nx,y+ay-ny,0,1,color};
                        vertices[count++]=a;vertices[count++]=b;vertices[count++]=c;
                        vertices[count++]=a;vertices[count++]=c;vertices[count++]=d;
                    };
                    for(int pass=0;pass<2;++pass){
                        const DWORD color=alpha|(pass?hitColor:0x00101010);
                        for(int vx:{-1,1})for(int vy:{-1,1})
                            line(vx*sx*.55f,vy*sy*.55f,vx*sx*1.25f,vy*sy*1.25f,
                                stroke+(pass?0:outline*2),color);
                    }
                    ok=SUCCEEDED(device->SetRenderState(D3DRS_ALPHABLENDENABLE,TRUE))&&
                       SUCCEEDED(device->SetRenderState(D3DRS_BLENDOP,D3DBLENDOP_ADD))&&
                       SUCCEEDED(device->SetRenderState(D3DRS_SRCBLEND,D3DBLEND_SRCALPHA))&&
                       SUCCEEDED(device->SetRenderState(D3DRS_DESTBLEND,D3DBLEND_INVSRCALPHA));
                    if(ok)ok=SUCCEEDED(device->DrawPrimitiveUP(D3DPT_TRIANGLELIST,count/3,vertices,sizeof(Vertex)));
                    ok=SUCCEEDED(device->SetRenderState(D3DRS_ALPHABLENDENABLE,FALSE))&&ok;
                }
            }
            if(ok&&slotCount){
                // Reuse this state capture and target for all nearby inventory
                // markers. Digits are BF2's actual equipped-kit item numbers.
                struct V{float x,y,z,w;DWORD color;};V v[400];unsigned n=0;
                const unsigned digits[]={0x3f,0x06,0x5b,0x4f,0x66,0x6d,0x7d,0x07,0x7f,0x6f};
                const float segments[7][4]={{-.35f,-.6f,.35f,-.6f},{.35f,-.6f,.35f,0},{.35f,0,.35f,.6f},{-.35f,.6f,.35f,.6f},{-.35f,0,-.35f,.6f},{-.35f,-.6f,-.35f,0},{-.35f,0,.35f,0}};
                for(unsigned i=0;i<slotCount;++i){
                    const auto& m=slots[i];const float cx=(m.x+1)*.5f*desc.Width-.5f,cy=(1-m.y)*.5f*desc.Height-.5f;
                    const float rx=std::max(6.f,m.sx*.5f*desc.Width),ry=std::max(6.f,m.sy*.5f*desc.Height);
                    const DWORD color=m.editing?0xff409fff:m.hovered?0xff60ff90:0xffc0e8f0;
                    const auto line=[&](float a,float b,float c,float d){v[n++]={cx+a*rx,cy+b*ry,0,1,color};v[n++]={cx+c*rx,cy+d*ry,0,1,color};};
                    if(m.editing){
                        V bubble[34];bubble[0]={cx,cy,0,1,0x504090ff};
                        for(unsigned j=0;j<=32;++j){
                            const float angle=j*.19634954085f;
                            bubble[j+1]={cx+std::cos(angle)*rx,cy+std::sin(angle)*ry,0,1,0x504090ff};
                            if(j<32){const float next=angle+.19634954085f;
                                line(std::cos(angle),std::sin(angle),std::cos(next),std::sin(next));
                                line(.45f*std::cos(angle),std::sin(angle),.45f*std::cos(next),std::sin(next));
                            }
                        }
                        ok=SUCCEEDED(device->SetRenderState(D3DRS_ALPHABLENDENABLE,TRUE))&&
                           SUCCEEDED(device->SetRenderState(D3DRS_BLENDOP,D3DBLENDOP_ADD))&&
                           SUCCEEDED(device->SetRenderState(D3DRS_SRCBLEND,D3DBLEND_SRCALPHA))&&
                           SUCCEEDED(device->SetRenderState(D3DRS_DESTBLEND,D3DBLEND_INVSRCALPHA))&&ok;
                        if(ok)ok=SUCCEEDED(device->DrawPrimitiveUP(D3DPT_TRIANGLEFAN,32,bubble,sizeof(V)));
                        ok=SUCCEEDED(device->SetRenderState(D3DRS_ALPHABLENDENABLE,FALSE))&&ok;
                        continue;
                    }
                    line(-1,-1,1,-1);line(1,-1,1,1);line(1,1,-1,1);line(-1,1,-1,-1);
                    if(m.item<10)for(unsigned j=0;j<7;++j)if(digits[m.item]&(1<<j))line(segments[j][0],segments[j][1],segments[j][2],segments[j][3]);
                }
                if(n)ok=SUCCEEDED(device->DrawPrimitiveUP(D3DPT_LINELIST,n/2,v,sizeof(V)));
            }
            if(ok&&defib){
                struct V{float x,y,z,w;DWORD color;};V v[12];unsigned n=0;
                const float cx=(dx+1)*.5f*desc.Width-.5f,cy=(1-dy)*.5f*desc.Height-.5f;
                const float rx=std::max(10.f,dsx*.5f*desc.Width),ry=std::max(3.f,dsy*.5f*desc.Height);
                const auto rect=[&](float l,float t,float r,float b,DWORD color){
                    v[n++]={l,t,0,1,color};v[n++]={r,t,0,1,color};v[n++]={r,b,0,1,color};
                    v[n++]={l,t,0,1,color};v[n++]={r,b,0,1,color};v[n++]={l,b,0,1,color};
                };
                rect(cx-rx-2,cy-ry-2,cx+rx+2,cy+ry+2,0xff101820);
                rect(cx-rx,cy-ry,cx-rx+2*rx*progress,cy+ry,ready?0xff50ff85:0xffffb840);
                ok=SUCCEEDED(device->DrawPrimitiveUP(D3DPT_TRIANGLELIST,n/3,v,sizeof(V)));
            }
            ok=SUCCEEDED(device->EndScene())&&ok;
        }else ok=false;
    }
    // ALL state blocks do not store render targets/depth. Restore those
    // explicitly before restoring viewport, shaders and stream 0 (UP clears it).
    if(targetChanged){
        ok=SUCCEEDED(device->SetRenderTarget(0,previous))&&ok;
        ok=SUCCEEDED(device->SetDepthStencilSurface(depth))&&ok;
    }
    ok=SUCCEEDED(gunCrosshairState->Apply())&&ok;
    Release(previous);Release(back);Release(depth);
    if(!ok){Engine::GunCrosshairAvailable(false);Log("AIM38 draw/restore failed; retaining native crosshair from next pair");}
}
