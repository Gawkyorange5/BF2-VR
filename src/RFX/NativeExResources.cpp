// Adapted from justiceofrip/BF2142VR 7fbc90d (MIT); see ../third_party/BF2142VR_LICENSE.txt.
#include "NativeExResources.h"
#include <MinHook.h>
#include <wrl/client.h>
#include <array>
#include <atomic>
namespace CleanVR {
namespace {
using Microsoft::WRL::ComPtr;
IDirect3DDevice9* owner=nullptr;ExResourceLog logLine=nullptr;
constexpr GUID managedTag={0xc99af705,0x3d84,0x4aad,{0xad,0xd4,0xe0,0x55,0xa1,0xc1,0x90,0x8d}};
struct Metadata{DWORD magic=0x3945584d,usage=0;};
struct HookEntry{void* address;void* detour;void* original;};
std::array<HookEntry,48> hooks{};size_t hookCount=0;
SRWLOCK hookLock=SRWLOCK_INIT;
struct HookGuard{HookGuard(){AcquireSRWLockExclusive(&hookLock);}~HookGuard(){ReleaseSRWLockExclusive(&hookLock);}};
bool debugResources=false,managedUpload=false;
constexpr GUID textureShadowTag={0xfa92d182,0x15a9,0x428d,{0x9c,0x66,0x84,0xa2,0xe7,0x33,0x97,0x82}};
constexpr GUID surfaceShadowTag={0x26e3134b,0xe1f4,0x4da9,{0x9a,0x74,0xf8,0x85,0x42,0x34,0x24,0xa1}};
template<class T,class R> bool Shadow(R* resource,const GUID& key,ComPtr<T>& shadow){
 DWORD bytes=sizeof(T*);return resource&&SUCCEEDED(resource->GetPrivateData(key,shadow.GetAddressOf(),&bytes))&&shadow;
}
constexpr GUID volumeShadowTag={0x2ebf7011,0xaad5,0x4dda,{0x93,0x9d,0xc7,0xc6,0x1a,0xda,0x6b,0x31}};
HRESULT DirtyAll(IDirect3DBaseTexture9* texture){
 switch(texture->GetType()){
 case D3DRTYPE_TEXTURE:{ComPtr<IDirect3DTexture9> t;auto hr=texture->QueryInterface(IID_PPV_ARGS(&t));return SUCCEEDED(hr)?t->AddDirtyRect(nullptr):hr;}
 case D3DRTYPE_CUBETEXTURE:{ComPtr<IDirect3DCubeTexture9> t;auto hr=texture->QueryInterface(IID_PPV_ARGS(&t));for(unsigned face=0;SUCCEEDED(hr)&&face<6;++face)hr=t->AddDirtyRect(static_cast<D3DCUBEMAP_FACES>(face),nullptr);return hr;}
 case D3DRTYPE_VOLUMETEXTURE:{ComPtr<IDirect3DVolumeTexture9> t;auto hr=texture->QueryInterface(IID_PPV_ARGS(&t));return SUCCEEDED(hr)?t->AddDirtyBox(nullptr):hr;}
 default:return E_INVALIDARG;
 }
}
HRESULT UploadManaged(IDirect3DBaseTexture9* texture){
 ComPtr<IDirect3DBaseTexture9> shadow;ComPtr<IDirect3DDevice9> device;
 if(!Shadow(texture,textureShadowTag,shadow))return E_INVALIDARG;
 HRESULT hr=DirtyAll(shadow.Get());
 if(SUCCEEDED(hr))hr=texture->GetDevice(&device);
 if(SUCCEEDED(hr))hr=device->UpdateTexture(shadow.Get(),texture);
 if(FAILED(hr)&&logLine)logLine("NativeEx managed upload failed: hr=0x%08lX.",hr);
 return hr;
}
bool Hook(void* address,void* detour,void** original){
 HookGuard guard;
 for(size_t i=0;i<hookCount;++i){const auto& h=hooks[i];if(h.address==address){if(h.detour!=detour)return false;*original=h.original;return true;}}
 if(hookCount==hooks.size())return false;
 if(MH_CreateHook(address,detour,original)!=MH_OK)return false;
 if(MH_EnableHook(address)!=MH_OK){MH_RemoveHook(address);return false;}
 hooks[hookCount++]={address,detour,*original};return true;
}
// A D3D9 runtime may expose different implementations of one COM method
// for different resource classes. Never dispatch an older object's hook via
// the trampoline most recently registered for a different class.
template<class F,class T> F Original(T* object,unsigned slot){
 void* address=(*reinterpret_cast<void***>(object))[slot];F result=nullptr;
 AcquireSRWLockShared(&hookLock);
 for(size_t i=0;i<hookCount;++i)if(hooks[i].address==address){result=reinterpret_cast<F>(hooks[i].original);break;}
 ReleaseSRWLockShared(&hookLock);return result;
}
template<class T> bool ReadTag(T* resource,Metadata& value){DWORD bytes=sizeof(value);return resource&&SUCCEEDED(resource->GetPrivateData(managedTag,&value,&bytes))&&bytes==sizeof(value)&&value.magic==Metadata{}.magic;}
template<class T> HRESULT Tag(T* resource,DWORD usage){Metadata value;value.usage=usage;return resource->SetPrivateData(managedTag,&value,sizeof(value),0);}
template<class D> void Correct(D* desc,const Metadata& m){desc->Usage=m.usage;desc->Pool=D3DPOOL_MANAGED;}
using CreateTexture=HRESULT(STDMETHODCALLTYPE*)(IDirect3DDevice9*,UINT,UINT,UINT,DWORD,D3DFORMAT,D3DPOOL,IDirect3DTexture9**,HANDLE*);
using CreateVolume=HRESULT(STDMETHODCALLTYPE*)(IDirect3DDevice9*,UINT,UINT,UINT,UINT,DWORD,D3DFORMAT,D3DPOOL,IDirect3DVolumeTexture9**,HANDLE*);
using CreateCube=HRESULT(STDMETHODCALLTYPE*)(IDirect3DDevice9*,UINT,UINT,DWORD,D3DFORMAT,D3DPOOL,IDirect3DCubeTexture9**,HANDLE*);
using CreateVB=HRESULT(STDMETHODCALLTYPE*)(IDirect3DDevice9*,UINT,DWORD,DWORD,D3DPOOL,IDirect3DVertexBuffer9**,HANDLE*);
using CreateIB=HRESULT(STDMETHODCALLTYPE*)(IDirect3DDevice9*,UINT,DWORD,D3DFORMAT,D3DPOOL,IDirect3DIndexBuffer9**,HANDLE*);
CreateTexture createTexture=nullptr;CreateVolume createVolume=nullptr;CreateCube createCube=nullptr;CreateVB createVB=nullptr;CreateIB createIB=nullptr;
using LevelDesc=HRESULT(STDMETHODCALLTYPE*)(IDirect3DBaseTexture9*,UINT,D3DSURFACE_DESC*);
LevelDesc textureDesc=nullptr,cubeDesc=nullptr;
using VolumeLevelDesc=HRESULT(STDMETHODCALLTYPE*)(IDirect3DVolumeTexture9*,UINT,D3DVOLUME_DESC*);VolumeLevelDesc volumeLevelDesc=nullptr;
using SurfaceDesc=HRESULT(STDMETHODCALLTYPE*)(IDirect3DSurface9*,D3DSURFACE_DESC*);SurfaceDesc surfaceDesc=nullptr;
using VolumeDesc=HRESULT(STDMETHODCALLTYPE*)(IDirect3DVolume9*,D3DVOLUME_DESC*);VolumeDesc volumeDesc=nullptr;
using BufferDesc=HRESULT(STDMETHODCALLTYPE*)(IDirect3DResource9*,void*);BufferDesc vbDesc=nullptr,ibDesc=nullptr;
HRESULT STDMETHODCALLTYPE DescribeLevel(IDirect3DBaseTexture9* t,UINT level,D3DSURFACE_DESC* d){
 const auto original=Original<LevelDesc>(t,17);if(!original)return E_FAIL;
 const auto hr=original(t,level,d);Metadata m;if(SUCCEEDED(hr)&&d&&ReadTag(t,m))Correct(d,m);return hr;
}
HRESULT STDMETHODCALLTYPE DescribeVolumeLevel(IDirect3DVolumeTexture9* t,UINT level,D3DVOLUME_DESC* d){const auto original=Original<VolumeLevelDesc>(t,17);if(!original)return E_FAIL;auto hr=original(t,level,d);Metadata m;if(SUCCEEDED(hr)&&d&&ReadTag(t,m))Correct(d,m);return hr;}
HRESULT STDMETHODCALLTYPE DescribeSurface(IDirect3DSurface9* s,D3DSURFACE_DESC* d){
 const auto original=Original<SurfaceDesc>(s,12);if(!original)return E_FAIL;auto hr=original(s,d);ComPtr<IDirect3DResource9> container;Metadata m;
 if(SUCCEEDED(hr)&&d&&SUCCEEDED(s->GetContainer(IID_PPV_ARGS(&container)))&&ReadTag(container.Get(),m))Correct(d,m);return hr;
}
HRESULT STDMETHODCALLTYPE DescribeVolume(IDirect3DVolume9* s,D3DVOLUME_DESC* d){
 const auto original=Original<VolumeDesc>(s,8);if(!original)return E_FAIL;auto hr=original(s,d);ComPtr<IDirect3DResource9> container;Metadata m;
 if(SUCCEEDED(hr)&&d&&SUCCEEDED(s->GetContainer(IID_PPV_ARGS(&container)))&&ReadTag(container.Get(),m))Correct(d,m);return hr;
}
HRESULT STDMETHODCALLTYPE DescribeBuffer(IDirect3DResource9* b,void* d){
 const bool vertex=b->GetType()==D3DRTYPE_VERTEXBUFFER;const auto original=Original<BufferDesc>(b,13);if(!original)return E_FAIL;const auto hr=original(b,d);Metadata m;
 if(SUCCEEDED(hr)&&d&&ReadTag(b,m)){if(vertex)Correct(static_cast<D3DVERTEXBUFFER_DESC*>(d),m);else Correct(static_cast<D3DINDEXBUFFER_DESC*>(d),m);}return hr;
}
template<class T,class F> bool Connect(T* object,unsigned slot,F detour,F* original){return Hook((*reinterpret_cast<void***>(object))[slot],reinterpret_cast<void*>(detour),reinterpret_cast<void**>(original));}
using TextureLock=HRESULT(STDMETHODCALLTYPE*)(IDirect3DTexture9*,UINT,D3DLOCKED_RECT*,const RECT*,DWORD);
using BufferLock=HRESULT(STDMETHODCALLTYPE*)(IDirect3DResource9*,UINT,UINT,void**,DWORD);
using TextureLod=DWORD(STDMETHODCALLTYPE*)(IDirect3DBaseTexture9*,DWORD);
using SurfaceLock=HRESULT(STDMETHODCALLTYPE*)(IDirect3DSurface9*,D3DLOCKED_RECT*,const RECT*,DWORD);
using CubeLock=HRESULT(STDMETHODCALLTYPE*)(IDirect3DCubeTexture9*,D3DCUBEMAP_FACES,UINT,D3DLOCKED_RECT*,const RECT*,DWORD);
using VolumeLock=HRESULT(STDMETHODCALLTYPE*)(IDirect3DVolumeTexture9*,UINT,D3DLOCKED_BOX*,const D3DBOX*,DWORD);
SurfaceLock surfaceLock=nullptr;CubeLock cubeLock=nullptr;VolumeLock volumeLock=nullptr;
using SurfaceUnlock=HRESULT(STDMETHODCALLTYPE*)(IDirect3DSurface9*);
using TextureUnlock=HRESULT(STDMETHODCALLTYPE*)(IDirect3DTexture9*,UINT);
SurfaceUnlock surfaceUnlock=nullptr;TextureUnlock textureUnlock=nullptr;
using CubeUnlock=HRESULT(STDMETHODCALLTYPE*)(IDirect3DCubeTexture9*,D3DCUBEMAP_FACES,UINT);
using VolumeUnlock=HRESULT(STDMETHODCALLTYPE*)(IDirect3DVolumeTexture9*,UINT);
using VolumeLevelLock=HRESULT(STDMETHODCALLTYPE*)(IDirect3DVolume9*,D3DLOCKED_BOX*,const D3DBOX*,DWORD);
using VolumeLevelUnlock=HRESULT(STDMETHODCALLTYPE*)(IDirect3DVolume9*);
CubeUnlock cubeUnlock=nullptr;VolumeUnlock volumeUnlock=nullptr;
VolumeLevelLock volumeLevelLock=nullptr;VolumeLevelUnlock volumeLevelUnlock=nullptr;
HRESULT STDMETHODCALLTYPE UnlockCube(IDirect3DCubeTexture9* texture,D3DCUBEMAP_FACES face,UINT level){
 ComPtr<IDirect3DCubeTexture9> shadow;
 if(!Shadow(texture,textureShadowTag,shadow)){const auto original=Original<CubeUnlock>(texture,20);return original?original(texture,face,level):E_FAIL;}
 auto hr=shadow->UnlockRect(face,level);return SUCCEEDED(hr)?UploadManaged(texture):hr;
}
HRESULT STDMETHODCALLTYPE UnlockVolume(IDirect3DVolumeTexture9* texture,UINT level){
 ComPtr<IDirect3DVolumeTexture9> shadow;
 if(!Shadow(texture,textureShadowTag,shadow)){const auto original=Original<VolumeUnlock>(texture,20);return original?original(texture,level):E_FAIL;}
 auto hr=shadow->UnlockBox(level);return SUCCEEDED(hr)?UploadManaged(texture):hr;
}
HRESULT STDMETHODCALLTYPE LockVolumeLevel(IDirect3DVolume9* volume,D3DLOCKED_BOX* result,const D3DBOX* box,DWORD flags){
 ComPtr<IDirect3DVolume9> shadow;
 if(Shadow(volume,volumeShadowTag,shadow))return shadow->LockBox(result,box,flags);
 const auto original=Original<VolumeLevelLock>(volume,9);return original?original(volume,result,box,flags):E_FAIL;
}
HRESULT STDMETHODCALLTYPE UnlockVolumeLevel(IDirect3DVolume9* volume){
 ComPtr<IDirect3DVolume9> shadow;
 if(!Shadow(volume,volumeShadowTag,shadow)){const auto original=Original<VolumeLevelUnlock>(volume,10);return original?original(volume):E_FAIL;}
 auto hr=shadow->UnlockBox();ComPtr<IDirect3DVolumeTexture9> parent;
 if(SUCCEEDED(hr))hr=volume->GetContainer(IID_PPV_ARGS(&parent));
 return SUCCEEDED(hr)?UploadManaged(parent.Get()):hr;
}
HRESULT STDMETHODCALLTYPE UnlockTexture(IDirect3DTexture9* texture,UINT level){
 ComPtr<IDirect3DTexture9> shadow;if(!Shadow(texture,textureShadowTag,shadow)){const auto original=Original<TextureUnlock>(texture,20);return original?original(texture,level):E_FAIL;}
 const auto hr=shadow->UnlockRect(level);return SUCCEEDED(hr)?UploadManaged(texture):hr;
}
HRESULT STDMETHODCALLTYPE UnlockSurface(IDirect3DSurface9* surface){
 ComPtr<IDirect3DSurface9> shadow;if(!Shadow(surface,surfaceShadowTag,shadow)){const auto original=Original<SurfaceUnlock>(surface,14);return original?original(surface):E_FAIL;}
 auto hr=shadow->UnlockRect();ComPtr<IDirect3DBaseTexture9> parent;
 if(SUCCEEDED(hr))hr=surface->GetContainer(IID_PPV_ARGS(&parent));
 return SUCCEEDED(hr)?UploadManaged(parent.Get()):hr;
}
std::atomic<unsigned> subresourceWarnings{0};
HRESULT STDMETHODCALLTYPE LockSurface(IDirect3DSurface9* resource,D3DLOCKED_RECT* result,const RECT* rect,DWORD flags){
 ComPtr<IDirect3DSurface9> shadow;
 const auto original=Original<SurfaceLock>(resource,13);if(!original)return E_FAIL;
 const auto hr=Shadow(resource,surfaceShadowTag,shadow)?shadow->LockRect(result,rect,flags):original(resource,result,rect,flags);
 if(FAILED(hr)||(flags&(D3DLOCK_DISCARD|D3DLOCK_NOOVERWRITE))){
  ComPtr<IDirect3DResource9> parent;Metadata m;
  if(SUCCEEDED(resource->GetContainer(IID_PPV_ARGS(&parent)))&&ReadTag(parent.Get(),m)&&subresourceWarnings.fetch_add(1)<32&&logLine)
   logLine("NativeEx surface lock: flags=0x%08lX subrect=%d hr=0x%08lX.",flags,rect!=nullptr,hr);
 }
 return hr;
}
HRESULT STDMETHODCALLTYPE LockCube(IDirect3DCubeTexture9* resource,D3DCUBEMAP_FACES face,UINT level,D3DLOCKED_RECT* result,const RECT* rect,DWORD flags){
 ComPtr<IDirect3DCubeTexture9> shadow;
 const auto original=Original<CubeLock>(resource,19);if(!original)return E_FAIL;
 const auto hr=Shadow(resource,textureShadowTag,shadow)?shadow->LockRect(face,level,result,rect,flags):original(resource,face,level,result,rect,flags);Metadata m;
 if((FAILED(hr)||(flags&(D3DLOCK_DISCARD|D3DLOCK_NOOVERWRITE)))&&ReadTag(resource,m)&&subresourceWarnings.fetch_add(1)<32&&logLine)
  logLine("NativeEx cube lock: level=%u flags=0x%08lX hr=0x%08lX.",level,flags,hr);
 return hr;
}
HRESULT STDMETHODCALLTYPE LockVolume(IDirect3DVolumeTexture9* resource,UINT level,D3DLOCKED_BOX* result,const D3DBOX* box,DWORD flags){
 ComPtr<IDirect3DVolumeTexture9> shadow;
 const auto original=Original<VolumeLock>(resource,19);if(!original)return E_FAIL;
 const auto hr=Shadow(resource,textureShadowTag,shadow)?shadow->LockBox(level,result,box,flags):original(resource,level,result,box,flags);Metadata m;
 if((FAILED(hr)||(flags&(D3DLOCK_DISCARD|D3DLOCK_NOOVERWRITE)))&&ReadTag(resource,m)&&subresourceWarnings.fetch_add(1)<32&&logLine)
  logLine("NativeEx volume lock: level=%u flags=0x%08lX hr=0x%08lX.",level,flags,hr);
 return hr;
}
TextureLock textureLock=nullptr;BufferLock vertexLock=nullptr,indexLock=nullptr;TextureLod textureLod=nullptr;
std::atomic<unsigned> textureWarnings{0},bufferWarnings{0},lodWarnings{0};
HRESULT STDMETHODCALLTYPE LockTexture(IDirect3DTexture9* t,UINT level,D3DLOCKED_RECT* result,const RECT* rect,DWORD flags){
 ComPtr<IDirect3DTexture9> shadow;
 const auto original=Original<TextureLock>(t,19);if(!original)return E_FAIL;
 const auto hr=Shadow(t,textureShadowTag,shadow)?shadow->LockRect(level,result,rect,flags):original(t,level,result,rect,flags);Metadata m;
 if((FAILED(hr)||(flags&(D3DLOCK_DISCARD|D3DLOCK_NOOVERWRITE)))&&ReadTag(t,m)&&textureWarnings.fetch_add(1)<24&&logLine)
  logLine("NativeEx texture lock: level=%u flags=0x%08lX subrect=%d hr=0x%08lX.",level,flags,rect!=nullptr,hr);
 return hr;
}
HRESULT STDMETHODCALLTYPE LockBuffer(IDirect3DResource9* b,UINT offset,UINT bytes,void** out,DWORD flags){
 const auto original=Original<BufferLock>(b,11);if(!original)return E_FAIL;const auto hr=original(b,offset,bytes,out,flags);Metadata m;
 if((FAILED(hr)||(flags&(D3DLOCK_DISCARD|D3DLOCK_NOOVERWRITE)))&&ReadTag(b,m)&&bufferWarnings.fetch_add(1)<24&&logLine)
  logLine("NativeEx buffer lock: type=%u offset=%u bytes=%u flags=0x%08lX hr=0x%08lX.",unsigned(b->GetType()),offset,bytes,flags,hr);
 return hr;
}
DWORD STDMETHODCALLTYPE SetTextureLod(IDirect3DBaseTexture9* t,DWORD lod){
 const auto original=Original<TextureLod>(t,11);if(!original)return 0;const auto result=original(t,lod);Metadata m;
 if(lod&&ReadTag(t,m)&&lodWarnings.fetch_add(1)<24&&logLine)logLine("NativeEx managed texture SetLOD: requested=%lu returned=%lu.",lod,result);
 return result;
}
bool Track(IDirect3DTexture9* t,DWORD usage){
 if((debugResources||managedUpload)&&(!Connect(t,19,LockTexture,&textureLock)||!Connect(t,20,UnlockTexture,&textureUnlock)))return false;
 if(debugResources&&!Connect<IDirect3DTexture9,TextureLod>(t,11,SetTextureLod,&textureLod))return false;
 ComPtr<IDirect3DSurface9> s;return SUCCEEDED(Tag(t,usage))&&Connect<IDirect3DTexture9,LevelDesc>(t,17,DescribeLevel,&textureDesc)&&
 SUCCEEDED(t->GetSurfaceLevel(0,&s))&&Connect(s.Get(),12,DescribeSurface,&surfaceDesc)&&(!(debugResources||managedUpload)||(Connect(s.Get(),13,LockSurface,&surfaceLock)&&Connect(s.Get(),14,UnlockSurface,&surfaceUnlock)));
}
bool Track(IDirect3DCubeTexture9* t,DWORD usage){
 if((debugResources||managedUpload)&&(!Connect(t,19,LockCube,&cubeLock)||!Connect(t,20,UnlockCube,&cubeUnlock)))return false;
 ComPtr<IDirect3DSurface9> s;return SUCCEEDED(Tag(t,usage))&&Connect<IDirect3DCubeTexture9,LevelDesc>(t,17,DescribeLevel,&cubeDesc)&&
 SUCCEEDED(t->GetCubeMapSurface(D3DCUBEMAP_FACE_POSITIVE_X,0,&s))&&Connect(s.Get(),12,DescribeSurface,&surfaceDesc)&&(!(debugResources||managedUpload)||(Connect(s.Get(),13,LockSurface,&surfaceLock)&&Connect(s.Get(),14,UnlockSurface,&surfaceUnlock)));
}
bool Track(IDirect3DVolumeTexture9* t,DWORD usage){
 if((debugResources||managedUpload)&&(!Connect(t,19,LockVolume,&volumeLock)||!Connect(t,20,UnlockVolume,&volumeUnlock)))return false;
 ComPtr<IDirect3DVolume9> v;return SUCCEEDED(Tag(t,usage))&&Connect(t,17,DescribeVolumeLevel,&volumeLevelDesc)&&
 SUCCEEDED(t->GetVolumeLevel(0,&v))&&Connect(v.Get(),8,DescribeVolume,&volumeDesc)&&
 (!(debugResources||managedUpload)||(Connect(v.Get(),9,LockVolumeLevel,&volumeLevelLock)&&Connect(v.Get(),10,UnlockVolumeLevel,&volumeLevelUnlock)));
}
bool Track(IDirect3DVertexBuffer9* b,DWORD usage){if(debugResources&&!Connect<IDirect3DVertexBuffer9,BufferLock>(b,11,LockBuffer,&vertexLock))return false;return SUCCEEDED(Tag(b,usage))&&Connect<IDirect3DVertexBuffer9,BufferDesc>(b,13,DescribeBuffer,&vbDesc);}
bool Track(IDirect3DIndexBuffer9* b,DWORD usage){if(debugResources&&!Connect<IDirect3DIndexBuffer9,BufferLock>(b,11,LockBuffer,&indexLock))return false;return SUCCEEDED(Tag(b,usage))&&Connect<IDirect3DIndexBuffer9,BufferDesc>(b,13,DescribeBuffer,&ibDesc);}
template<class T> HRESULT Finish(HRESULT hr,T** out,DWORD usage,bool translated){
 if(SUCCEEDED(hr)&&translated&&out&&*out&&!Track(*out,usage)){(*out)->Release();*out=nullptr;hr=E_FAIL;}
 if(FAILED(hr)&&translated&&logLine)logLine("Native D3D9Ex managed resource failed: hr=0x%08lX usage=0x%08lX.",hr,usage);return hr;
}
HRESULT STDMETHODCALLTYPE Texture(IDirect3DDevice9* d,UINT w,UINT h,UINT levels,DWORD usage,D3DFORMAT f,D3DPOOL p,IDirect3DTexture9** out,HANDLE* handle){
 const bool convert=d==owner&&p==D3DPOOL_MANAGED;
 const bool staged=convert&&managedUpload&&usage==0;
 auto hr=createTexture(d,w,h,levels,convert&&!staged?usage|D3DUSAGE_DYNAMIC:usage,f,convert?D3DPOOL_DEFAULT:p,out,handle);
 if(SUCCEEDED(hr)&&staged&&out&&*out){
  // Native managed locks use tightly laid-out system memory, whereas Ex dynamic
  // textures have driver-aligned rows. Preserve the managed CPU representation
  // and upload explicitly; shadow references are owned by resource private data.
  ComPtr<IDirect3DTexture9> shadow;
  hr=createTexture(d,w,h,(*out)->GetLevelCount(),0,f,D3DPOOL_SYSTEMMEM,&shadow,nullptr);
  if(SUCCEEDED(hr))hr=(*out)->SetPrivateData(textureShadowTag,shadow.Get(),sizeof(IUnknown*),D3DSPD_IUNKNOWN);
  for(UINT level=0;SUCCEEDED(hr)&&level<(*out)->GetLevelCount();++level){
   ComPtr<IDirect3DSurface9> surface,backing;
   hr=(*out)->GetSurfaceLevel(level,&surface);
   if(SUCCEEDED(hr))hr=shadow->GetSurfaceLevel(level,&backing);
   if(SUCCEEDED(hr))hr=surface->SetPrivateData(surfaceShadowTag,backing.Get(),sizeof(IUnknown*),D3DSPD_IUNKNOWN);
  }
  if(FAILED(hr)){(*out)->Release();*out=nullptr;}
 }
 return Finish(hr,out,usage,convert);
}
HRESULT STDMETHODCALLTYPE Volume(IDirect3DDevice9* d,UINT w,UINT h,UINT depth,UINT levels,DWORD usage,D3DFORMAT f,D3DPOOL p,IDirect3DVolumeTexture9** out,HANDLE* handle){
 const bool convert=d==owner&&p==D3DPOOL_MANAGED,staged=convert&&managedUpload&&usage==0;
 auto hr=createVolume(d,w,h,depth,levels,convert&&!staged?usage|D3DUSAGE_DYNAMIC:usage,f,convert?D3DPOOL_DEFAULT:p,out,handle);
 if(SUCCEEDED(hr)&&staged&&out&&*out){
  // Water's volume-normal mip chain must retain native row AND slice pitch.
  ComPtr<IDirect3DVolumeTexture9> shadow;
  hr=createVolume(d,w,h,depth,(*out)->GetLevelCount(),0,f,D3DPOOL_SYSTEMMEM,&shadow,nullptr);
  if(SUCCEEDED(hr))hr=(*out)->SetPrivateData(textureShadowTag,shadow.Get(),sizeof(IUnknown*),D3DSPD_IUNKNOWN);
  for(UINT level=0;SUCCEEDED(hr)&&level<(*out)->GetLevelCount();++level){
   ComPtr<IDirect3DVolume9> volume,backing;hr=(*out)->GetVolumeLevel(level,&volume);
   if(SUCCEEDED(hr))hr=shadow->GetVolumeLevel(level,&backing);
   if(SUCCEEDED(hr))hr=volume->SetPrivateData(volumeShadowTag,backing.Get(),sizeof(IUnknown*),D3DSPD_IUNKNOWN);
  }
  if(FAILED(hr)){(*out)->Release();*out=nullptr;}
 }
 return Finish(hr,out,usage,convert);
}
HRESULT STDMETHODCALLTYPE Cube(IDirect3DDevice9* d,UINT w,UINT levels,DWORD usage,D3DFORMAT f,D3DPOOL p,IDirect3DCubeTexture9** out,HANDLE* handle){
 const bool convert=d==owner&&p==D3DPOOL_MANAGED,staged=convert&&managedUpload&&usage==0;
 auto hr=createCube(d,w,levels,convert&&!staged?usage|D3DUSAGE_DYNAMIC:usage,f,convert?D3DPOOL_DEFAULT:p,out,handle);
 if(SUCCEEDED(hr)&&staged&&out&&*out){
  ComPtr<IDirect3DCubeTexture9> shadow;hr=createCube(d,w,(*out)->GetLevelCount(),0,f,D3DPOOL_SYSTEMMEM,&shadow,nullptr);
  if(SUCCEEDED(hr))hr=(*out)->SetPrivateData(textureShadowTag,shadow.Get(),sizeof(IUnknown*),D3DSPD_IUNKNOWN);
  for(unsigned face=0;SUCCEEDED(hr)&&face<6;++face)for(UINT level=0;SUCCEEDED(hr)&&level<(*out)->GetLevelCount();++level){
   auto side=static_cast<D3DCUBEMAP_FACES>(face);ComPtr<IDirect3DSurface9> surface,backing;
   hr=(*out)->GetCubeMapSurface(side,level,&surface);
   if(SUCCEEDED(hr))hr=shadow->GetCubeMapSurface(side,level,&backing);
   if(SUCCEEDED(hr))hr=surface->SetPrivateData(surfaceShadowTag,backing.Get(),sizeof(IUnknown*),D3DSPD_IUNKNOWN);
  }
  if(FAILED(hr)){(*out)->Release();*out=nullptr;}
 }
 return Finish(hr,out,usage,convert);
}
HRESULT STDMETHODCALLTYPE VB(IDirect3DDevice9* d,UINT bytes,DWORD usage,DWORD f,D3DPOOL p,IDirect3DVertexBuffer9** out,HANDLE* handle){
 // Buffers in DEFAULT are lockable without DYNAMIC. Preserve native usage.
 const bool convert=d==owner&&p==D3DPOOL_MANAGED;return Finish(createVB(d,bytes,usage,f,convert?D3DPOOL_DEFAULT:p,out,handle),out,usage,convert);
}
HRESULT STDMETHODCALLTYPE IB(IDirect3DDevice9* d,UINT bytes,DWORD usage,D3DFORMAT f,D3DPOOL p,IDirect3DIndexBuffer9** out,HANDLE* handle){
 const bool convert=d==owner&&p==D3DPOOL_MANAGED;return Finish(createIB(d,bytes,usage,f,convert?D3DPOOL_DEFAULT:p,out,handle),out,usage,convert);
}
}
bool InstallNativeExResources(IDirect3DDevice9* d,ExResourceLog log){
 if(owner)return owner==d;ComPtr<IDirect3DDevice9Ex> ex;if(!d||FAILED(d->QueryInterface(IID_PPV_ARGS(&ex))))return false;
 logLine=log;wchar_t flag[8]{};debugResources=GetEnvironmentVariableW(L"BF2VR_GPU_DEBUG",flag,8)==1&&flag[0]==L'1';
 managedUpload=true; // Required for legacy row/slice pitch, including cube/volume mips.
 if(!Connect(d,23,Texture,&createTexture)||!Connect(d,24,Volume,&createVolume)||!Connect(d,25,Cube,&createCube)||!Connect(d,26,VB,&createVB)||!Connect(d,27,IB,&createIB))return false;
 owner=d;if(logLine)logLine("Native D3D9Ex managed resource compatibility installed (experimental; system-memory upload=%d).",managedUpload);return true;
}
}
