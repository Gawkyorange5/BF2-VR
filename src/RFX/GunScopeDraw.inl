// Scope replay remains on the D3D9 GPU. The ordinary two-eye atlas transports
// the composited lens; no scope readback, D3D11 upload or XR layer is added.
#include "GunScopeShader.inl"
IDirect3DTexture9* gunScopeTexture=nullptr;
IDirect3DSurface9* gunScopeSurface=nullptr;
IDirect3DPixelShader9* gunScopeShader=nullptr;
IDirect3DStateBlock9* gunScopeState=nullptr;
unsigned gunScopeWidth=0,gunScopeHeight=0;D3DFORMAT gunScopeFormat=D3DFMT_UNKNOWN;
bool gunScopeCaptured=false;
ULONGLONG gunScopeRetryAfter=0;
unsigned gunScopeReplayFailures=0;
void ResetGunScope(){
    Release(gunScopeSurface);Release(gunScopeTexture);Release(gunScopeShader);Release(gunScopeState);
    gunScopeWidth=gunScopeHeight=0;gunScopeFormat=D3DFMT_UNKNOWN;gunScopeCaptured=false;
    gunScopeRetryAfter=0;gunScopeReplayFailures=0;Engine::ScopeAvailable(true);
}
void ScopeFailure(const char* where){
    gunScopeCaptured=false;Engine::ScopeAvailable(false);
    Log("SCOPE40 disabled after %s; ordinary stereo remains active (reset/restart retries)",where);
}
bool EnsureGunScope(){
    IDirect3DSurface9* back=nullptr;D3DSURFACE_DESC desc{};
    bool ok=SUCCEEDED(device->GetBackBuffer(0,0,D3DBACKBUFFER_TYPE_MONO,&back));
    if(ok)ok=SUCCEEDED(back->GetDesc(&desc));Release(back);if(!ok)return false;
    if(gunScopeSurface&&(desc.Width!=gunScopeWidth||desc.Height!=gunScopeHeight||desc.Format!=gunScopeFormat)){
        Release(gunScopeSurface);Release(gunScopeTexture);
    }
    if(!gunScopeTexture){
        ok=SUCCEEDED(device->CreateTexture(desc.Width,desc.Height,1,D3DUSAGE_RENDERTARGET,desc.Format,D3DPOOL_DEFAULT,&gunScopeTexture,nullptr));
        if(ok)ok=SUCCEEDED(gunScopeTexture->GetSurfaceLevel(0,&gunScopeSurface));
        if(!ok){Release(gunScopeSurface);Release(gunScopeTexture);return false;}
        gunScopeWidth=desc.Width;gunScopeHeight=desc.Height;gunScopeFormat=desc.Format;
    }
    if(!gunScopeShader){
        // Shader compiled with the patch; first scope use never runs a shader compiler.
        const HRESULT hr=device->CreatePixelShader(reinterpret_cast<const DWORD*>(gunScopePixelShader),&gunScopeShader);
        if(FAILED(hr)||!gunScopeShader)return false;
    }
    return true;
}
void CaptureGunScope(Engine::SceneFunction original,void* self,unsigned interpolation){
    gunScopeCaptured=false;if(!Engine::ScopeReady()||GetTickCount64()<gunScopeRetryAfter)return;
    if(!EnsureGunScope()){ScopeFailure("GPU resource creation");return;}
    // Always close a camera override, including partial setup or failed native
    // replay. Only the normal first eye advances native renderer time.
    const bool started=Engine::BeginScope();
    const bool drawn=started&&original(self,0,0,interpolation);
    Engine::EndScope();
    if(!drawn){
        // A transient camera/replay rejection must not disable every optic
        // for the rest of the session. Back off; report which stage failed.
        gunScopeRetryAfter=GetTickCount64()+1000;
        if(++gunScopeReplayFailures<=4)Log("SCOPE40 %s failed; retry after 1s",started?"native world render":"camera setup");
        return;
    }
    IDirect3DSurface9* back=nullptr;
    bool ok=SUCCEEDED(device->GetBackBuffer(0,0,D3DBACKBUFFER_TYPE_MONO,&back));
    if(ok)ok=SUCCEEDED(device->StretchRect(back,nullptr,gunScopeSurface,nullptr,D3DTEXF_NONE));
    Release(back);if(!ok){ScopeFailure("GPU resolve");return;}
    gunScopeCaptured=true;
    static bool reported=false;if(!reported){reported=true;Log("SCOPE40 GPU lens captured %ux%u; no CPU pixel copy; scene time advanced only by normal left eye",gunScopeWidth,gunScopeHeight);}
}
void DrawGunScope(){
    if(!gunScopeCaptured)return;
    Engine::ScopeView lens{};Mat intoEye{},projection{};Vec eye{};float visibility=0;
    if(!Engine::ScopeEye(lens,intoEye,projection,eye,visibility))return;
    const float relief=lens.center.z-eye.z;if(relief<=.03f)return;
    struct Vertex{float x,y,z,rhw,u,v,s,t;};Vertex vertices[4];
    // The same finite bore zero used to render the telescope.
    const Vec hit=Add(eye,Scale(Sub({0,0,100},eye),relief/(100-eye.z)));
    // Match the wider capture without zooming or enlarging the metal lens.
    const float scale=lens.relief/(relief*lens.coverage);
    const float corners[4][2]={{-1,-1},{-1,1},{1,-1},{1,1}};
    for(int i=0;i<4;++i){
        const float u=corners[i][0],v=corners[i][1];
        const Vec local=Add(lens.center,{u*lens.radius,v*lens.radius,0}),p=Point(local,intoEye);
        if(!std::isfinite(p.x+p.y+p.z)||p.z<.018f)return;
        const float x=p.x/p.z*projection.m[0][0]+projection.m[2][0],y=p.y/p.z*projection.m[1][1]+projection.m[2][1];
        if(!std::isfinite(x+y)||std::abs(x)>8||std::abs(y)>8)return;
        vertices[i]={(x+1)*.5f*gunScopeWidth-.5f,(1-y)*.5f*gunScopeHeight-.5f,
            projection.m[2][2]+projection.m[3][2]/p.z,1/p.z,u,v,
            .5f+(local.x-hit.x)/lens.radius*.5f*scale,.5f-(local.y-hit.y)/lens.radius*.5f*scale};
    }
    bool ok=true;if(!gunScopeState)ok=SUCCEEDED(device->CreateStateBlock(D3DSBT_ALL,&gunScopeState));
    if(ok)ok=SUCCEEDED(gunScopeState->Capture());
    if(!ok){ScopeFailure("state capture");return;}
    IDirect3DSurface9 *previous=nullptr,*back=nullptr,*depth=nullptr;bool changed=false;
    ok=SUCCEEDED(device->GetRenderTarget(0,&previous))&&SUCCEEDED(device->GetBackBuffer(0,0,D3DBACKBUFFER_TYPE_MONO,&back));
    if(ok)ok=SUCCEEDED(device->GetDepthStencilSurface(&depth));
    D3DSURFACE_DESC backDesc{},depthDesc{};
    if(ok)ok=SUCCEEDED(back->GetDesc(&backDesc))&&SUCCEEDED(depth->GetDesc(&depthDesc))&&
        depthDesc.Width>=backDesc.Width&&depthDesc.Height>=backDesc.Height&&depthDesc.MultiSampleType==backDesc.MultiSampleType&&depthDesc.MultiSampleQuality==backDesc.MultiSampleQuality;
    if(ok){ok=SUCCEEDED(device->SetRenderTarget(0,back));changed=ok;}
    if(ok&&SUCCEEDED(device->BeginScene())){
        const auto check=[&](HRESULT hr){ok=SUCCEEDED(hr)&&ok;};
        D3DVIEWPORT9 vp{0,0,gunScopeWidth,gunScopeHeight,0,1};check(device->SetViewport(&vp));
        check(device->SetVertexShader(nullptr));check(device->SetPixelShader(gunScopeShader));
        check(device->SetFVF(D3DFVF_XYZRHW|D3DFVF_TEX2));check(device->SetStreamSourceFreq(0,1));
        check(device->SetTexture(0,gunScopeTexture));
        const float lensPixels=std::min(gunScopeWidth*projection.m[0][0],gunScopeHeight*projection.m[1][1])*lens.radius/relief;
        const float constants[]={visibility,std::max(.0015f,std::min(.015f,.65f/std::max(1.f,lensPixels))),lens.coverage,0};
        check(device->SetPixelShaderConstantF(0,constants,1));
        for(auto s:{D3DRS_FOGENABLE,D3DRS_LIGHTING,D3DRS_ZWRITEENABLE,D3DRS_ALPHATESTENABLE,
                   D3DRS_SCISSORTESTENABLE,D3DRS_STENCILENABLE,D3DRS_SRGBWRITEENABLE,
                   D3DRS_ALPHABLENDENABLE,D3DRS_SEPARATEALPHABLENDENABLE})check(device->SetRenderState(s,FALSE));
        check(device->SetRenderState(D3DRS_ZENABLE,D3DZB_TRUE));check(device->SetRenderState(D3DRS_ZFUNC,D3DCMP_LESSEQUAL));
        check(device->SetRenderState(D3DRS_DEPTHBIAS,0));check(device->SetRenderState(D3DRS_SLOPESCALEDEPTHBIAS,0));
        check(device->SetRenderState(D3DRS_CULLMODE,D3DCULL_NONE));check(device->SetRenderState(D3DRS_FILLMODE,D3DFILL_SOLID));
        check(device->SetRenderState(D3DRS_COLORWRITEENABLE,15));check(device->SetRenderState(D3DRS_CLIPPLANEENABLE,0));
        check(device->SetRenderState(D3DRS_MULTISAMPLEMASK,0xffffffff));
        check(device->SetSamplerState(0,D3DSAMP_MINFILTER,D3DTEXF_LINEAR));check(device->SetSamplerState(0,D3DSAMP_MAGFILTER,D3DTEXF_LINEAR));
        check(device->SetSamplerState(0,D3DSAMP_MIPFILTER,D3DTEXF_NONE));check(device->SetSamplerState(0,D3DSAMP_SRGBTEXTURE,0));
        check(device->SetSamplerState(0,D3DSAMP_ADDRESSU,D3DTADDRESS_CLAMP));check(device->SetSamplerState(0,D3DSAMP_ADDRESSV,D3DTADDRESS_CLAMP));
        for(unsigned i=0;i<2;++i){check(device->SetTextureStageState(i,D3DTSS_TEXCOORDINDEX,i));check(device->SetTextureStageState(i,D3DTSS_TEXTURETRANSFORMFLAGS,D3DTTFF_DISABLE));}
        if(ok)check(device->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP,2,vertices,sizeof(Vertex)));
        check(device->EndScene());
    }else ok=false;
    if(changed){ok=SUCCEEDED(device->SetRenderTarget(0,previous))&&ok;ok=SUCCEEDED(device->SetDepthStencilSurface(depth))&&ok;}
    ok=SUCCEEDED(gunScopeState->Apply())&&ok;Release(previous);Release(back);Release(depth);
    if(!ok)ScopeFailure("lens draw/depth/state restore");
    else{
        static bool reported=false;if(!reported){reported=true;Log("SCOPE41 lens draw submitted relief=%.4f visibility=%.3f coverage=%.3f depth=(%.6f %.6f); circular mask with native weapon depth",relief,visibility,lens.coverage,vertices[0].z,vertices[3].z);}
    }
}
