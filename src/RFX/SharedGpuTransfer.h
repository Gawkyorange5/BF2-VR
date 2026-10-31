#pragma once
#include <d3d9.h>
#include <d3d11.h>
#include <wrl/client.h>
namespace CleanVR {
// Same-adapter D3D9Ex -> D3D11 transport. Images stay on the GPU. The
// producer event covers the whole eye/HUD pair; the consumer event prevents
// the next pair from overwriting a texture that D3D11 is still reading.
class SharedGpuTransfer {
public:
    bool Begin(IDirect3DDevice9*,ID3D11Device*,ID3D11DeviceContext*,const unsigned widths[2],const unsigned heights[2]);
    bool BeginCinema(IDirect3DDevice9*,ID3D11Device*,ID3D11DeviceContext*);
    bool Stage(unsigned slot,IDirect3DSurface9*,unsigned width,unsigned height);
    bool ImportHud(IDirect3DSurface9*,HANDLE,unsigned,unsigned);
    bool StageBackbuffer(unsigned slot,unsigned width,unsigned height);
    bool Complete(unsigned mask,double& waitMs);
    void Consumed();
    void Reset();
    ID3D11Texture2D* Texture(unsigned slot)const{return slot<4?images[slot].opened.Get():nullptr;}
    IDirect3DSurface9* Surface(unsigned slot)const{return slot<4?images[slot].surface.Get():nullptr;}
    bool Failed()const{return blocked;}
    unsigned DirectMsaaCopies()const{return directMsaaCopies;}
    unsigned LegacyMsaaCopies()const{return legacyMsaaCopies;}
    unsigned ExistingA8Resolves()const{return a8Resolves;}
    unsigned PlainCopies()const{return plainCopies;}
    void Disable(){Fail("runtime copy layout unsupported",D3DERR_NOTAVAILABLE);}
private:
    template<class T> using Ptr=Microsoft::WRL::ComPtr<T>;
    struct Image {Ptr<IDirect3DTexture9> texture;Ptr<IDirect3DSurface9> surface;Ptr<ID3D11Texture2D> opened;unsigned width=0,height=0;
        bool imported=false;HANDLE handle=nullptr;} images[4];
    Ptr<IDirect3DSurface9> resolve;
    Ptr<IDirect3DQuery9> producer;
    Ptr<ID3D11Query> consumer;
    Ptr<ID3D11Device> destination;
    Ptr<ID3D11DeviceContext> context;
    IDirect3DDevice9* source=nullptr;
    bool blocked=false,reading=false,reported=false;
    unsigned staged=0;
    unsigned directMsaaCopies=0,legacyMsaaCopies=0,a8Resolves=0,plainCopies=0;
    bool importRejected=false,importReported=false;
    // Driver support is cached independently by slot and reset with resources.
    bool directMsaaRejected[4]{},directMsaaReported[4]{};
    bool Initialize(IDirect3DDevice9*,ID3D11Device*,ID3D11DeviceContext*);
    bool Ensure(unsigned,unsigned,unsigned);
    bool Fail(const char*,HRESULT);
};
}
