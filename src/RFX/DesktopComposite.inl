// One desktop-only full-screen draw instead of world then alpha HUD.
// The headset still receives the exact same eye/HUD textures as r138.
IDirect3DPixelShader9* desktopCompositeShader=nullptr;
bool desktopCompositeBlocked=false,desktopCompositeReported=false;
unsigned desktopDrawsAvoided=0;
bool CompositeDesktopOnce(IDirect3DTexture9* eye,const float transform[4],const float clamp[4],const void* vertices,unsigned stride){
    if(!Config().singlePassDesktop||desktopCompositeBlocked||!eye||!texture)return false;
    HRESULT hr=S_OK;
    if(!desktopCompositeShader)hr=device->CreatePixelShader(reinterpret_cast<const DWORD*>(DesktopCompositeBytecode),&desktopCompositeShader);
    const auto set=[&](HRESULT r){if(FAILED(r)&&SUCCEEDED(hr))hr=r;};
    if(SUCCEEDED(hr)){
        set(device->SetPixelShader(desktopCompositeShader));set(device->SetTexture(0,eye));set(device->SetTexture(1,texture));
        set(device->SetPixelShaderConstantF(0,transform,1));set(device->SetPixelShaderConstantF(1,clamp,1));
        set(device->SetRenderState(D3DRS_ALPHABLENDENABLE,FALSE));
        set(device->SetSamplerState(1,D3DSAMP_ADDRESSU,D3DTADDRESS_CLAMP));set(device->SetSamplerState(1,D3DSAMP_ADDRESSV,D3DTADDRESS_CLAMP));
        set(device->SetSamplerState(1,D3DSAMP_SRGBTEXTURE,FALSE));set(device->SetSamplerState(1,D3DSAMP_MIPFILTER,D3DTEXF_NONE));
        set(device->SetSamplerState(1,D3DSAMP_MINFILTER,D3DTEXF_LINEAR));set(device->SetSamplerState(1,D3DSAMP_MAGFILTER,D3DTEXF_LINEAR));
        if(SUCCEEDED(hr))hr=device->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP,2,vertices,stride);
    }
    // Restore the state expected by the existing two-pass fallback. The outer
    // state block restores the native caller's complete state after either path.
    device->SetPixelShader(nullptr);device->SetTexture(1,nullptr);
    if(FAILED(hr)){
        desktopCompositeBlocked=true;Log("COMPOSITE139 one-pass desktop unavailable hr=%08X; using r138 two-pass path",(unsigned)hr);return false;
    }
    ++desktopDrawsAvoided;
    if(!desktopCompositeReported){desktopCompositeReported=true;Log("COMPOSITE139 ACTIVE world + original alpha HUD in one desktop draw; samples=%u size=%ux%u; eye/HUD headset images unchanged",(unsigned)samples,width,height);}
    return true;
}
void ResetDesktopComposite(){Release(desktopCompositeShader);desktopCompositeBlocked=desktopCompositeReported=false;desktopDrawsAvoided=0;}
