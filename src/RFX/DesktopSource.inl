// The shared right-eye surface already belongs to a texture. Sample its crop
// directly rather than copying that crop into a second desktop-only texture.
// Explicit texel-center clamp reproduces the old cropped texture's CLAMP edges.
bool PrepareDesktopSource(IDirect3DSurface9* eye,const RECT* crop,IDirect3DTexture9** out,float transform[4],float clamp[4]){
    if(!Config().reuseTransferImages||desktopReuseRejected||!eye||!crop||!out)return false;
    *out=nullptr;D3DSURFACE_DESC d{};
    if(FAILED(eye->GetDesc(&d))||d.MultiSampleType!=D3DMULTISAMPLE_NONE||d.Pool!=D3DPOOL_DEFAULT||
       (d.Format!=D3DFMT_A8R8G8B8&&d.Format!=D3DFMT_X8R8G8B8)||crop->left<0||crop->top<0||crop->right<=crop->left||crop->bottom<=crop->top||crop->right>(LONG)d.Width||crop->bottom>(LONG)d.Height)return false;
    HRESULT hr=eye->GetContainer(__uuidof(IDirect3DTexture9),reinterpret_cast<void**>(out));
    if(FAILED(hr)||!*out)return false; // CPU fallback surfaces may not be textures.
    IDirect3DSurface9* level=nullptr;
    hr=(*out)->GetSurfaceLevel(0,&level);const bool same=SUCCEEDED(hr)&&level==eye;Release(level);
    if(!same){Release(*out);return false;}
    if(!desktopCropShader)hr=device->CreatePixelShader(reinterpret_cast<const DWORD*>(DesktopCropBytecode),&desktopCropShader);
    if(FAILED(hr)){Release(*out);desktopReuseRejected=true;Log("REUSE138 desktop crop shader unavailable hr=%08X; keeping crop copy",(unsigned)hr);return false;}
    const float iw=1.f/d.Width,ih=1.f/d.Height;
    transform[0]=(crop->right-crop->left)*iw;transform[1]=(crop->bottom-crop->top)*ih;transform[2]=crop->left*iw;transform[3]=crop->top*ih;
    clamp[0]=(crop->left+.5f)*iw;clamp[1]=(crop->top+.5f)*ih;clamp[2]=(crop->right-.5f)*iw;clamp[3]=(crop->bottom-.5f)*ih;
    if(!desktopReuseReported){desktopReuseReported=true;Log("REUSE138 desktop samples existing eye texture: crop copy omitted; source=%ux%u crop=%ldx%ld; headset unchanged",d.Width,d.Height,crop->right-crop->left,crop->bottom-crop->top);}
    return true;
}
