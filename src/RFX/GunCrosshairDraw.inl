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
    float x=0,y=0,sx=0,sy=0;if(!Engine::GunCrosshair(x,y,sx,sy))return;
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
            if(ok){
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
