// Original assets are decoded once per used sight, never once per frame.
#include "LauncherArtwork.inl"
enum class LauncherArt { Sraw,At4,Pf89,Pf98,Metis,Maaws,M32,Mgl,Xm25,Xm53,
    Stiletto,Eotech,Pld,Igla,IglaTrack,IglaLock,IglaState,GroundTrack,GroundLock,
    JavelinTop,JavelinDirect,JavelinTrack,JavelinLock,JavelinStates,Underbarrel,Count };
struct LauncherArtData {const unsigned char* data;unsigned size;};
#define LA(name) {launcherArt##name,sizeof(launcherArt##name)}
const LauncherArtData launcherArtData[]={LA(Sraw),LA(At4),LA(Pf89),LA(Pf98),LA(Metis),LA(Maaws),LA(M32),LA(Mgl),LA(Xm25),LA(Xm53),
    LA(Stiletto),LA(Eotech),LA(Pld),LA(Igla),LA(IglaTrack),LA(IglaLock),LA(IglaState),LA(GroundTrack),LA(GroundLock),
    LA(JavelinTop),LA(JavelinDirect),LA(JavelinTrack),LA(JavelinLock),LA(JavelinStates),LA(Underbarrel)};
#undef LA
static_assert(sizeof(launcherArtData)/sizeof(launcherArtData[0])==static_cast<unsigned>(LauncherArt::Count),"Launcher artwork inventory");
IDirect3DTexture9* launcherArtTextures[static_cast<unsigned>(LauncherArt::Count)]{};
bool launcherArtAttempted[static_cast<unsigned>(LauncherArt::Count)]{};
IDirect3DTexture9* LauncherTexture(LauncherArt art){
    const unsigned i=static_cast<unsigned>(art);
    if(i>=static_cast<unsigned>(LauncherArt::Count))return nullptr;
    if(!launcherArtAttempted[i]){
        launcherArtAttempted[i]=true;const auto& resource=launcherArtData[i];
        const HRESULT hr=D3DXCreateTextureFromFileInMemoryEx(device,resource.data,resource.size,D3DX_DEFAULT_NONPOW2,D3DX_DEFAULT_NONPOW2,
            1,0,D3DFMT_A8R8G8B8,D3DPOOL_MANAGED,D3DX_FILTER_NONE,D3DX_FILTER_NONE,0,nullptr,nullptr,&launcherArtTextures[i]);
        Log("OPTIC126 native artwork=%u loaded=%d hr=%08X",i,!!launcherArtTextures[i],unsigned(hr));
    }
    return launcherArtTextures[i];
}
void ResetLauncherArtwork(){
    for(unsigned i=0;i<static_cast<unsigned>(LauncherArt::Count);++i){Release(launcherArtTextures[i]);launcherArtAttempted[i]=false;}
}
void LauncherSightTextures(const Engine::ScopeView& lens,IDirect3DTexture9*& base,IDirect3DTexture9*& box,IDirect3DTexture9*& status,float& boxWidth){
    if(lens.nativeHud>=1&&lens.nativeHud<=14)base=LauncherTexture(static_cast<LauncherArt>(lens.nativeHud-1));
    if(lens.nativeHud==17)base=LauncherTexture(LauncherArt::Underbarrel);
    if(lens.nativeHud==14){
        box=LauncherTexture(lens.seekerLocked?LauncherArt::IglaLock:LauncherArt::IglaTrack);
        status=LauncherTexture(LauncherArt::IglaState);boxWidth=lens.seekerLocked?.20f:.10f;
    }else if(lens.nativeHud==15){
        box=LauncherTexture(lens.seekerLocked?LauncherArt::GroundLock:LauncherArt::GroundTrack);boxWidth=.10f;
    }else if(lens.nativeHud==16){
        base=LauncherTexture(lens.seekerDirect?LauncherArt::JavelinDirect:LauncherArt::JavelinTop);
        box=LauncherTexture(lens.seekerLocked?LauncherArt::JavelinLock:LauncherArt::JavelinTrack);
        status=LauncherTexture(LauncherArt::JavelinStates);
        // Native HUD uses a 48px acquire box and a 1536px locked cross on a
        // 1068px canvas. Retain those different states and their proportions.
        boxWidth=(lens.seekerLocked?1536.f:48.f)/1068.f*(500.f/512.f)*(492.f/382.f);
    }
}
