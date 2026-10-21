// One GPU pass over the isolated HUD, once per pair, only in occupied vehicles.
// The original resolved texture remains available if any setup/draw fails.
IDirect3DTexture9* vehicleTexture=nullptr;
IDirect3DSurface9* vehicleSurface=nullptr;
IDirect3DPixelShader9* vehicleShader=nullptr;
IDirect3DStateBlock9* vehicleState=nullptr;
float vehicleStrength=0.f;
bool vehicleReady=false,vehicleBlocked=false,vehicleReported=false;
void ClearVehicleHud(){Release(vehicleSurface);Release(vehicleTexture);vehicleReady=false;}
bool EnhanceVehicleHud(){
    vehicleReady=false;
    if(vehicleStrength<1.f||vehicleBlocked)return true;
    HRESULT hr=S_OK;
    if(!vehicleShader)hr=device->CreatePixelShader(reinterpret_cast<const DWORD*>(VehicleHudBytecode),&vehicleShader);
    if(SUCCEEDED(hr)&&!vehicleTexture){
        hr=device->CreateTexture(width,height,1,D3DUSAGE_RENDERTARGET,D3DFMT_A8R8G8B8,D3DPOOL_DEFAULT,&vehicleTexture,nullptr);
        if(SUCCEEDED(hr))hr=vehicleTexture->GetSurfaceLevel(0,&vehicleSurface);
        if(FAILED(hr))ClearVehicleHud();
    }
    if(SUCCEEDED(hr)&&!vehicleState)hr=device->CreateStateBlock(D3DSBT_ALL,&vehicleState);
    if(SUCCEEDED(hr))hr=vehicleState->Capture();
    if(FAILED(hr)){vehicleBlocked=true;Log("HUD62 vehicle opacity unavailable hr=%08X; original HUD retained",(unsigned)hr);return true;}
    IDirect3DSurface9* targets[4]{};IDirect3DSurface9* depth=nullptr;
    D3DCAPS9 caps{};hr=device->GetDeviceCaps(&caps);
    const unsigned count=SUCCEEDED(hr)?std::min(4u,(unsigned)caps.NumSimultaneousRTs):0;
    for(unsigned i=0;i<count&&SUCCEEDED(hr);++i){const auto r=device->GetRenderTarget(i,&targets[i]);if(FAILED(r)&&!(i&&r==D3DERR_NOTFOUND))hr=r;}
    if(SUCCEEDED(hr)){const auto r=device->GetDepthStencilSurface(&depth);if(FAILED(r)&&r!=D3DERR_NOTFOUND)hr=r;}
    if(!count&&SUCCEEDED(hr))hr=E_FAIL;
    const bool saved=SUCCEEDED(hr);
    const auto set=[&](HRESULT result){if(FAILED(result)&&SUCCEEDED(hr))hr=result;};
    if(saved){
        set(device->SetDepthStencilSurface(nullptr));
        for(unsigned i=1;i<count;++i)set(device->SetRenderTarget(i,nullptr));
        set(device->SetRenderTarget(0,vehicleSurface));
        bool began=false;if(SUCCEEDED(hr)){hr=device->BeginScene();began=SUCCEEDED(hr);}
        if(began){
            set(device->SetVertexShader(nullptr));set(device->SetPixelShader(vehicleShader));
            set(device->SetFVF(D3DFVF_XYZRHW|D3DFVF_TEX1));set(device->SetTexture(0,texture));
            for(auto s:{D3DRS_FOGENABLE,D3DRS_LIGHTING,D3DRS_ZENABLE,D3DRS_ZWRITEENABLE,D3DRS_ALPHATESTENABLE,
                        D3DRS_ALPHABLENDENABLE,D3DRS_SCISSORTESTENABLE,D3DRS_STENCILENABLE,D3DRS_SRGBWRITEENABLE,
                        D3DRS_SEPARATEALPHABLENDENABLE})set(device->SetRenderState(s,FALSE));
            set(device->SetRenderState(D3DRS_CULLMODE,D3DCULL_NONE));set(device->SetRenderState(D3DRS_COLORWRITEENABLE,15));
            set(device->SetRenderState(D3DRS_FILLMODE,D3DFILL_SOLID));
            set(device->SetSamplerState(0,D3DSAMP_ADDRESSU,D3DTADDRESS_CLAMP));set(device->SetSamplerState(0,D3DSAMP_ADDRESSV,D3DTADDRESS_CLAMP));
            set(device->SetSamplerState(0,D3DSAMP_SRGBTEXTURE,FALSE));set(device->SetSamplerState(0,D3DSAMP_MIPFILTER,D3DTEXF_NONE));
            set(device->SetSamplerState(0,D3DSAMP_MINFILTER,D3DTEXF_POINT));set(device->SetSamplerState(0,D3DSAMP_MAGFILTER,D3DTEXF_POINT));
            D3DVIEWPORT9 vp{0,0,width,height,0,1};set(device->SetViewport(&vp));
            const float factor[]={vehicleStrength,0,0,0};set(device->SetPixelShaderConstantF(0,factor,1));
            struct Vertex{float x,y,z,w,u,v;};const float w=(float)width-.5f,h=(float)height-.5f;
            const Vertex v[]={{-.5f,-.5f,0,1,0,0},{w,-.5f,0,1,1,0},{-.5f,h,0,1,0,1},{w,h,0,1,1,1}};
            if(SUCCEEDED(hr))hr=device->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP,2,v,sizeof(Vertex));
            set(device->EndScene());
        }
        bool restored=true;
        // Targets/depth are not part of a D3D9 state block. Restore them first;
        // applying the block last also restores the viewport reset by RT0.
        for(unsigned i=0;i<count;++i)restored=SUCCEEDED(device->SetRenderTarget(i,targets[i]))&&restored;
        restored=SUCCEEDED(device->SetDepthStencilSurface(depth))&&restored;
        restored=SUCCEEDED(vehicleState->Apply())&&restored;
        for(auto& p:targets)Release(p);Release(depth);
        if(!restored){Log("HUD62 vehicle opacity state restore failed; HUD transfer cancelled");return false;}
    }else{for(auto& p:targets)Release(p);Release(depth);}
    if(FAILED(hr)){vehicleBlocked=true;Log("HUD62 vehicle opacity draw unavailable hr=%08X; original HUD retained",(unsigned)hr);return true;}
    vehicleReady=true;
    if(!vehicleReported){vehicleReported=true;Log("HUD62 vehicle opacity active strength=%.2f; straight-alpha XR correction; original desktop HUD; one GPU pass per pair",vehicleStrength);}
    return true;
}
