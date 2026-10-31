// Adapted from justiceofrip/BF2142VR 7fbc90d (MIT); see ../third_party/BF2142VR_LICENSE.txt.
#pragma once
#include <d3d9.h>
#include <wrl/client.h>
namespace CleanVR {
// Capture immediately after device creation, before game or overlay state.
// A client owns this for its single native device. The block references no game
// resources; the reference it holds to the device lasts until client shutdown.
class NativeExReset {
public:
    HRESULT Initialize(IDirect3DDevice9* device);
    HRESULT Restore(IDirect3DDevice9* device, bool autoDepthStencil);
private:
    IDirect3DDevice9* owner_ = nullptr;
    Microsoft::WRL::ComPtr<IDirect3DStateBlock9> defaults_;
    DWORD renderTargets_ = 0;
};
}
