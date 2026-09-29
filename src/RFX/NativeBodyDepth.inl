// BF2's scene entry clears Z at +30F51 before the first-person render queues.
// Retain the local body's Z only at that profiled boundary, with identical
// world/weapon depth projections. All other native clears remain unchanged.
using BodyNativeClear=HRESULT(__thiscall*)(void*,DWORD,D3DCOLOR,float,DWORD,DWORD,const D3DRECT*);
BodyNativeClear originalBodyClear=nullptr;
IDirect3DSurface9* bodyDepthSurface=nullptr;
D3DVIEWPORT9 bodyDepthViewport{};

void ReleaseBodyDepth(){
    if(bodyDepthSurface){bodyDepthSurface->Release();bodyDepthSurface=nullptr;}
    bodyDepthViewport={};
}
void RememberBodyDepth(IDirect3DDevice9* device){
    if(!bodyDepthShared||!device)return;
    DWORD enabled=0,writes=0;D3DVIEWPORT9 viewport{};IDirect3DSurface9* surface=nullptr;
    if(FAILED(device->GetRenderState(D3DRS_ZENABLE,&enabled))||enabled!=D3DZB_TRUE||
       FAILED(device->GetRenderState(D3DRS_ZWRITEENABLE,&writes))||!writes||
       FAILED(device->GetViewport(&viewport))||FAILED(device->GetDepthStencilSurface(&surface)))return;
    if(!surface)return;
    ReleaseBodyDepth();bodyDepthSurface=surface;bodyDepthViewport=viewport;
}
__declspec(noinline) HRESULT __fastcall BodyClearHook(void* self,void*,DWORD flags,D3DCOLOR color,float z,DWORD stencil,DWORD count,const D3DRECT* rects){
    if(!scopePass&&_ReturnAddress()==render+0x30F56&&eyeActive&&bodyReady&&bodyDepthShared&&bodyEyeDraws&&bodyDepthSurface&&
       bodyOwner==Soldier()&&flags==D3DCLEAR_ZBUFFER&&z==1.f&&!count&&!rects){
        void* state=Value<void*>(render,0x23B984);
        IDirect3DDevice9* device=Value<IDirect3DDevice9*>(self,0x0C);
        if(state&&device){
            // Retain the original clear helper's pending render-target/depth
            // surface commit; bypass only the device Clear itself.
            reinterpret_cast<void(__thiscall*)(void*)>(render+0x26670)(state);
            IDirect3DSurface9* current=nullptr;D3DVIEWPORT9 viewport{};
            const bool readable=SUCCEEDED(device->GetDepthStencilSurface(&current))&&SUCCEEDED(device->GetViewport(&viewport));
            const bool same=readable&&current==bodyDepthSurface&&
                viewport.X==bodyDepthViewport.X&&viewport.Y==bodyDepthViewport.Y&&
                viewport.Width==bodyDepthViewport.Width&&viewport.Height==bodyDepthViewport.Height&&
                viewport.MinZ==bodyDepthViewport.MinZ&&viewport.MaxZ==bodyDepthViewport.MaxZ;
            if(current)current->Release();
            if(same){
                ++bodyEyeDepthClears;
                static unsigned preserved=0;if(++preserved<=6||preserved%3600==1)
                    Log("FULLBODY7 depth preserved eye=%d frame=%llu near=%.4f far=%.1f viewportZ=(%.3f %.3f)",activeEye,pairFrame.id,views[0].eyeNear,views[0].farZ,viewport.MinZ,viewport.MaxZ);
                return D3D_OK;
            }
            static unsigned mismatches=0;if(++mismatches<=6||mismatches%3600==1)
                Log("FULLBODY7 depth not preserved eye=%d frame=%llu surfaceOrViewportMismatch=1",activeEye,pairFrame.id);
        }
    }
    return originalBodyClear(self,flags,color,z,stencil,count,rects);
}
bool InstallBodyDepth(){
    const BYTE entry[]={0x55,0x8B,0xEC,0x56,0x8B,0xF1};
    const BYTE boundary[]={0x6A,0,0x6A,0,0x6A,0,0x68,0,0,0x80,0x3F,0x6A,0,0x6A,2,0xE8,0xFA,0x47,0xFD,0xFF};
    const BYTE deviceClear[]={0x50,0xFF,0x91,0xAC,0,0,0,0x5E,0x5D,0xC2,0x18,0};
    const BYTE targets[]={0x55,0x8B,0xEC,0x83,0xEC,0x14,0x53,0x56,0x8B,0xD9};
    if(!Match(render+0x30F42,boundary,sizeof(boundary))||!Match(render+0x577E,deviceClear,sizeof(deviceClear))||
       !Match(render+0x26670,targets,sizeof(targets)))return false;
    return Jump(render+0x5750,entry,sizeof(entry),reinterpret_cast<void*>(&BodyClearHook),reinterpret_cast<void**>(&originalBodyClear));
}
