// Adapted from justiceofrip/BF2142VR 7fbc90d (MIT); see ../third_party/BF2142VR_LICENSE.txt.
#include "NativeExReset.h"
namespace CleanVR {
HRESULT NativeExReset::Initialize(IDirect3DDevice9* device) {
    defaults_.Reset(); owner_ = nullptr; renderTargets_ = 0;
    Microsoft::WRL::ComPtr<IDirect3DDevice9Ex> ex;
    if (!device || FAILED(device->QueryInterface(IID_PPV_ARGS(&ex)))) return E_INVALIDARG;
    D3DCAPS9 caps{};
    HRESULT hr = device->GetDeviceCaps(&caps);
    if (SUCCEEDED(hr)) hr = device->CreateStateBlock(D3DSBT_ALL, &defaults_);
    if (SUCCEEDED(hr)) { owner_ = device; renderTargets_ = caps.NumSimultaneousRTs; }
    return hr;
}
HRESULT NativeExReset::Restore(IDirect3DDevice9* device, bool autoDepthStencil) {
    if (!defaults_ || device != owner_) return E_INVALIDARG;
    // ResetEx keeps pipeline state, while the game's legacy Reset contract
    // clears it. Apply ONLY at that boundary, never between native eye draws:
    // the engine clears its state cache on Reset but not between stereo eyes.
    HRESULT hr = defaults_->Apply();
    if (FAILED(hr)) return hr;
    // State blocks omit render targets. ResetEx owns the new primary target and
    // automatic depth surface, but additional render-target bindings must go.
    for (DWORD i = 1; i < renderTargets_; ++i) {
        hr = device->SetRenderTarget(i, nullptr);
        if (FAILED(hr)) return hr;
    }
    if (!autoDepthStencil && FAILED(hr = device->SetDepthStencilSurface(nullptr))) return hr;
    if (FAILED(hr = device->SetRenderState(D3DRS_ZENABLE, autoDepthStencil ? D3DZB_TRUE : D3DZB_FALSE))) return hr;
    Microsoft::WRL::ComPtr<IDirect3DSurface9> back;
    D3DSURFACE_DESC desc{};
    if (FAILED(hr = device->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &back))) return hr;
    if (FAILED(hr = back->GetDesc(&desc))) return hr;
    // The captured block contains the OLD viewport and scissor dimensions.
    D3DVIEWPORT9 viewport{0, 0, desc.Width, desc.Height, 0.f, 1.f};
    RECT scissor{0, 0, LONG(desc.Width), LONG(desc.Height)};
    if (FAILED(hr = device->SetViewport(&viewport))) return hr;
    return device->SetScissorRect(&scissor);
}
}
