#pragma once
#include <d3d11.h>
#include <wrl/client.h>
namespace CleanVR {
// Used only with the private XR D3D11 context, which otherwise performs copies.
// The native BF2 D3D9 pipeline is never rebound or drawn into here.
class CinemaGpu {
public:
    bool Prepare(ID3D11Device*,ID3D11Texture2D*);
    bool Compose(ID3D11DeviceContext*,ID3D11Texture2D*,bool retainBlank);
    void Seed(ID3D11DeviceContext*,ID3D11Texture2D*);
    ID3D11Texture2D* Image()const{return retained.Get();}
    bool Failed()const{return failed;}
    void Reset();
private:
    template<class T>using Ptr=Microsoft::WRL::ComPtr<T>;
    Ptr<ID3D11Device> device;
    Ptr<ID3D11Texture2D> retained,probe;
    Ptr<ID3D11ShaderResourceView> view;
    Ptr<ID3D11RenderTargetView> target;
    Ptr<ID3D11VertexShader> vertex;
    Ptr<ID3D11PixelShader> pixel;
    Ptr<ID3D11Buffer> constants;
    Ptr<ID3D11Predicate> nonblank;
    Ptr<ID3D11RasterizerState> raster;
    Ptr<ID3D11DepthStencilState> depth;
    Ptr<ID3D11BlendState> blend;
    Ptr<ID3D11Texture2D> input;
    unsigned width=0,height=0;bool ready=false,failed=false;
    bool Fail(HRESULT);
};
}
