#include "CinemaGpu.h"
#include "CinemaGpuShaders.inl"
#include "VrSettings.h"
#include <algorithm>
namespace CleanVR {
bool CinemaGpu::Fail(HRESULT hr){
    if(!failed)Log("MENU136 GPU cinema unavailable hr=%08X; retaining CPU fallback",unsigned(hr));
    failed=true;return false;
}
void CinemaGpu::Reset(){
    input.Reset();view.Reset();retained.Reset();target.Reset();probe.Reset();
    vertex.Reset();pixel.Reset();constants.Reset();nonblank.Reset();raster.Reset();depth.Reset();blend.Reset();device.Reset();
    width=height=0;ready=failed=false;
}
bool CinemaGpu::Prepare(ID3D11Device* d,ID3D11Texture2D* image){
    if(!d||!image)return false;if(device&&device.Get()!=d)Reset();if(failed)return false;
    D3D11_TEXTURE2D_DESC desc{};image->GetDesc(&desc);
    if(desc.Format!=DXGI_FORMAT_B8G8R8A8_UNORM||desc.SampleDesc.Count!=1||desc.MipLevels!=1||desc.ArraySize!=1)return Fail(E_INVALIDARG);
    HRESULT hr=S_OK;
    if(!device){
        device=d;
        hr=d->CreateVertexShader(CinemaVS,sizeof(CinemaVS),nullptr,&vertex);
        if(SUCCEEDED(hr))hr=d->CreatePixelShader(CinemaPS,sizeof(CinemaPS),nullptr,&pixel);
        D3D11_BUFFER_DESC cb{};cb.ByteWidth=16;cb.Usage=D3D11_USAGE_DEFAULT;cb.BindFlags=D3D11_BIND_CONSTANT_BUFFER;
        if(SUCCEEDED(hr))hr=d->CreateBuffer(&cb,nullptr,&constants);
        D3D11_QUERY_DESC q{D3D11_QUERY_OCCLUSION_PREDICATE,0}; // Exact predicate, NOT the optional/hint mode.
        if(SUCCEEDED(hr))hr=d->CreatePredicate(&q,&nonblank);
        D3D11_RASTERIZER_DESC rs{};rs.FillMode=D3D11_FILL_SOLID;rs.CullMode=D3D11_CULL_NONE;rs.DepthClipEnable=TRUE;
        if(SUCCEEDED(hr))hr=d->CreateRasterizerState(&rs,&raster);
        D3D11_DEPTH_STENCIL_DESC ds{};ds.DepthEnable=FALSE;ds.StencilEnable=FALSE;
        if(SUCCEEDED(hr))hr=d->CreateDepthStencilState(&ds,&depth);
        D3D11_BLEND_DESC bs{};bs.RenderTarget[0].RenderTargetWriteMask=0;
        if(SUCCEEDED(hr))hr=d->CreateBlendState(&bs,&blend);
        D3D11_TEXTURE2D_DESC probeDesc{};probeDesc.Width=probeDesc.Height=256;probeDesc.MipLevels=probeDesc.ArraySize=1;probeDesc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;
        probeDesc.SampleDesc.Count=1;probeDesc.Usage=D3D11_USAGE_DEFAULT;probeDesc.BindFlags=D3D11_BIND_RENDER_TARGET;
        if(SUCCEEDED(hr))hr=d->CreateTexture2D(&probeDesc,nullptr,&probe);
        if(SUCCEEDED(hr))hr=d->CreateRenderTargetView(probe.Get(),nullptr,&target);
        if(FAILED(hr))return Fail(hr);
    }
    if(!retained||width!=desc.Width||height!=desc.Height){
        ready=false;retained.Reset();width=desc.Width;height=desc.Height;
        desc.BindFlags=desc.MiscFlags=desc.CPUAccessFlags=0;desc.Usage=D3D11_USAGE_DEFAULT;
        hr=d->CreateTexture2D(&desc,nullptr,&retained);if(FAILED(hr))return Fail(hr);
    }
    if(input.Get()!=image){
        view.Reset();input=image;
        hr=d->CreateShaderResourceView(image,nullptr,&view);if(FAILED(hr))return Fail(hr);
    }
    return true;
}
void CinemaGpu::Seed(ID3D11DeviceContext* ctx,ID3D11Texture2D* image){
    if(!ctx||!image||!retained||failed)return;
    D3D11_TEXTURE2D_DESC desc{};image->GetDesc(&desc);
    if(desc.Width!=width||desc.Height!=height||desc.MipLevels!=1||desc.ArraySize!=1||desc.SampleDesc.Count!=1||
       (desc.Format!=DXGI_FORMAT_B8G8R8A8_UNORM&&desc.Format!=DXGI_FORMAT_B8G8R8A8_UNORM_SRGB))return;
    ctx->CopyResource(retained.Get(),image);ready=true;
}
bool CinemaGpu::Compose(ID3D11DeviceContext* ctx,ID3D11Texture2D* image,bool retainBlank){
    if(!ctx||!retained||!view||failed||input.Get()!=image)return false;
    // If CPU cinema preceded GPU initialization, let one CPU upload seed the
    // cache. Never replace a valid loading screen with an uninitialized image.
    if(retainBlank&&!ready)return false;
    if(!retainBlank){ctx->CopyResource(retained.Get(),image);ready=true;return true;}
    const unsigned sx=std::max(1u,width/128),sy=std::max(1u,height/128);
    const unsigned cb[]={sx,sy,0,0};ctx->UpdateSubresource(constants.Get(),0,nullptr,cb,0,0);
    D3D11_VIEWPORT vp{0,0,float((width+sx-1)/sx),float((height+sy-1)/sy),0,1};
    auto* rt=target.Get();auto* srv=view.Get();auto* buffer=constants.Get();
    ctx->SetPredication(nullptr,FALSE);ctx->IASetInputLayout(nullptr);ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    ctx->VSSetShader(vertex.Get(),nullptr,0);ctx->PSSetShader(pixel.Get(),nullptr,0);
    ctx->GSSetShader(nullptr,nullptr,0);ctx->HSSetShader(nullptr,nullptr,0);ctx->DSSetShader(nullptr,nullptr,0);
    ctx->PSSetShaderResources(0,1,&srv);ctx->PSSetConstantBuffers(0,1,&buffer);
    ctx->RSSetState(raster.Get());ctx->RSSetViewports(1,&vp);ctx->OMSetRenderTargets(1,&rt,nullptr);
    ctx->OMSetDepthStencilState(depth.Get(),0);ctx->OMSetBlendState(blend.Get(),nullptr,~0u);
    ctx->Begin(nonblank.Get());ctx->Draw(3,0);ctx->End(nonblank.Get());
    srv=nullptr;buffer=nullptr;ctx->PSSetShaderResources(0,1,&srv);ctx->PSSetConstantBuffers(0,1,&buffer);ctx->OMSetRenderTargets(0,nullptr,nullptr);
    // A FALSE predicate suppresses the copy only for an entirely black sample
    // grid. No GetData, CPU readback or CPU wait: ordered entirely on the GPU.
    ctx->SetPredication(nonblank.Get(),FALSE);ctx->CopyResource(retained.Get(),image);ctx->SetPredication(nullptr,FALSE);
    ctx->VSSetShader(nullptr,nullptr,0);ctx->PSSetShader(nullptr,nullptr,0);ctx->RSSetState(nullptr);
    ctx->OMSetDepthStencilState(nullptr,0);ctx->OMSetBlendState(nullptr,nullptr,~0u);
    return true;
}
}
