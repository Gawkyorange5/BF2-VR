#include "D3DWorkProfile.h"
#if BF2_DRAW_DIAGNOSTICS
#include "VrSettings.h"
#include "FrameTiming.h"
#include <algorithm>

namespace CleanVR { namespace D3DWorkProfile {
thread_local EyeSample* current=nullptr;
namespace {
enum Group : unsigned { Draw, Constants, State, Targets, Transfer, Create, GetState, Scene, GroupCount };
struct Description { const char* name; Group group; };
const Description descriptions[methodCount]={
    {"TestCooperativeLevel",Scene},
    {"EvictManagedResources",Create},
    {"GetDeviceCaps",GetState},
    {"GetDisplayMode",GetState},
    {"GetCreationParameters",GetState},
    {"SetCursorProperties",State},
    {"CreateAdditionalSwapChain",Create},
    {"GetSwapChain",GetState},
    {"GetBackBuffer",GetState},
    {"GetRasterStatus",GetState},
    {"SetDialogBoxMode",State},
    {"CreateTexture",Create},
    {"CreateVolumeTexture",Create},
    {"CreateCubeTexture",Create},
    {"CreateVertexBuffer",Create},
    {"CreateIndexBuffer",Create},
    {"CreateRenderTarget",Create},
    {"CreateDepthStencilSurface",Create},
    {"UpdateSurface",Transfer},
    {"UpdateTexture",Transfer},
    {"GetRenderTargetData",Transfer},
    {"GetFrontBufferData",Transfer},
    {"StretchRect",Transfer},
    {"ColorFill",Targets},
    {"CreateOffscreenPlainSurface",Create},
    {"SetRenderTarget",Targets},
    {"GetRenderTarget",GetState},
    {"SetDepthStencilSurface",Targets},
    {"GetDepthStencilSurface",GetState},
    {"BeginScene",Scene},
    {"EndScene",Scene},
    {"Clear",Targets},
    {"SetTransform",State},
    {"GetTransform",GetState},
    {"MultiplyTransform",State},
    {"SetViewport",State},
    {"GetViewport",GetState},
    {"SetMaterial",State},
    {"GetMaterial",GetState},
    {"SetLight",State},
    {"GetLight",GetState},
    {"LightEnable",State},
    {"GetLightEnable",GetState},
    {"SetClipPlane",State},
    {"GetClipPlane",GetState},
    {"SetRenderState",State},
    {"GetRenderState",GetState},
    {"CreateStateBlock",Create},
    {"BeginStateBlock",Create},
    {"EndStateBlock",Create},
    {"SetClipStatus",State},
    {"GetClipStatus",GetState},
    {"GetTexture",GetState},
    {"SetTexture",State},
    {"GetTextureStageState",GetState},
    {"SetTextureStageState",State},
    {"GetSamplerState",GetState},
    {"SetSamplerState",State},
    {"ValidateDevice",Scene},
    {"SetPaletteEntries",State},
    {"GetPaletteEntries",GetState},
    {"SetCurrentTexturePalette",State},
    {"GetCurrentTexturePalette",GetState},
    {"SetScissorRect",State},
    {"GetScissorRect",GetState},
    {"SetSoftwareVertexProcessing",State},
    {"SetNPatchMode",State},
    {"DrawPrimitive",Draw},
    {"DrawIndexedPrimitive",Draw},
    {"DrawPrimitiveUP",Draw},
    {"DrawIndexedPrimitiveUP",Draw},
    {"ProcessVertices",Draw},
    {"CreateVertexDeclaration",Create},
    {"SetVertexDeclaration",State},
    {"GetVertexDeclaration",GetState},
    {"SetFVF",State},
    {"GetFVF",GetState},
    {"CreateVertexShader",Create},
    {"SetVertexShader",State},
    {"GetVertexShader",GetState},
    {"SetVertexShaderConstantF",Constants},
    {"GetVertexShaderConstantF",GetState},
    {"SetVertexShaderConstantI",Constants},
    {"GetVertexShaderConstantI",GetState},
    {"SetVertexShaderConstantB",Constants},
    {"GetVertexShaderConstantB",GetState},
    {"SetStreamSource",State},
    {"GetStreamSource",GetState},
    {"SetStreamSourceFreq",State},
    {"GetStreamSourceFreq",GetState},
    {"SetIndices",State},
    {"GetIndices",GetState},
    {"CreatePixelShader",Create},
    {"SetPixelShader",State},
    {"GetPixelShader",GetState},
    {"SetPixelShaderConstantF",Constants},
    {"GetPixelShaderConstantF",GetState},
    {"SetPixelShaderConstantI",Constants},
    {"GetPixelShaderConstantI",GetState},
    {"SetPixelShaderConstantB",Constants},
    {"GetPixelShaderConstantB",GetState},
    {"DrawRectPatch",Draw},
    {"DrawTriPatch",Draw},
    {"DeletePatch",State},
    {"CreateQuery",Create},
};
struct Profile {
    EyeSample eyes[2]{};
    Cost totals[methodCount]{};
    LONGLONG eyeTicks[2]{},eyeWrapped[2]{};
    unsigned opportunities=0,mask=0,pairs=0,rejected=0;
    bool chosen=false,announced=false;
    double since=0,msPerTick=0,timerPairUs=0;
};
thread_local Profile profile;
LONGLONG Counter(){LARGE_INTEGER t{};QueryPerformanceCounter(&t);return t.QuadPart;}
void Report(){
    const double now=FrameClockMs();
    if(!profile.since){profile.since=now;return;}
    if(now-profile.since<5000||!profile.pairs)return;
    const double n=profile.pairs, factor=profile.msPerTick/n;
    double groupMs[GroupCount]{};unsigned long long calls=0;LONGLONG wrapped=0,maximum=0;
    unsigned ranked[methodCount]{};
    for(unsigned i=0;i<methodCount;++i){
        const auto& c=profile.totals[i];ranked[i]=i;calls+=c.calls;wrapped+=c.ticks;
        maximum=std::max(maximum,c.maximum);groupMs[descriptions[i].group]+=c.ticks*factor;
    }
    std::sort(ranked,ranked+methodCount,[](unsigned a,unsigned b){return profile.totals[a].ticks>profile.totals[b].ticks;});
    const double eye0=profile.eyeTicks[0]*factor,eye1=profile.eyeTicks[1]*factor;
    const double outside=std::max(0.0,eye0+eye1-wrapped*factor);
    Log("PERF drawCPU pairs=%u rejected=%u eye0Ms=%.3f eye1Ms=%.3f wrapped0Ms=%.3f wrapped1Ms=%.3f outsideWrappedMs=%.3f callsPerPair=%.1f maxCallMs=%.3f (sampled CPU wall; NOT GPU busy time)",
        profile.pairs,profile.rejected,eye0,eye1,profile.eyeWrapped[0]*factor,profile.eyeWrapped[1]*factor,outside,(double)calls/n,maximum*profile.msPerTick);
    Log("PERF drawGroupsMs draw=%.3f constants=%.3f state=%.3f targets=%.3f transfer=%.3f create=%.3f getState=%.3f scene=%.3f (means per sampled pair; calls retain original behavior)",
        groupMs[Draw],groupMs[Constants],groupMs[State],groupMs[Targets],groupMs[Transfer],groupMs[Create],groupMs[GetState],groupMs[Scene]);
    for(unsigned j=0;j<3;++j){
        const unsigned i=ranked[j];const auto& c=profile.totals[i];if(!c.calls)continue;
        Log("PERF drawTop rank=%u method=%s callsPerPair=%.1f msPerPair=%.3f maxCallMs=%.3f",j+1,descriptions[i].name,c.calls/n,c.ticks*factor,c.maximum*profile.msPerTick);
    }
    if(!calls)Log("PERF drawCPU coverage=none; native calls bypassed the device wrapper; do not infer engine-only work");
    // Do not let menus contribute to eye time; only completed sampled pairs
    // accumulate. The report window may span a menu, so it is not an FPS rate.
    for(auto& c:profile.totals)c={};
    for(auto& t:profile.eyeTicks)t=0;
    for(auto& t:profile.eyeWrapped)t=0;
    profile.pairs=profile.rejected=0;profile.since=now;
}
}
void BeginPair(){
    current=nullptr;profile.chosen=false;profile.mask=0;
    if(!Config().drawProfiling)return;
    // Offset the sample from GpuTiming's one-in-32 sample to avoid routinely
    // timing instrumented CPU and GPU on the same pair.
    if(++profile.opportunities%32!=17)return;
    if(!profile.msPerTick){
        LARGE_INTEGER f{};QueryPerformanceFrequency(&f);profile.msPerTick=1000.0/f.QuadPart;
        LONGLONG minimum=0;
        for(unsigned i=0;i<64;++i){const auto a=Counter();const auto b=Counter();const auto dt=b-a;if(i==0||dt<minimum)minimum=dt;}
        profile.timerPairUs=minimum*profile.msPerTick*1000.0;
    }
    if(!profile.announced){
        Log("PERF drawCPU active: one native pair in 32; %u HRESULT forwarders; timerPairMinUs=%.3f; raw-device/resource Lock/query GetData/state-block Apply calls are outside wrapper totals; diagnostic build, no promised FPS gain",methodCount,profile.timerPairUs);
        profile.announced=true;
    }
    for(auto& eye:profile.eyes)eye=EyeSample{};
    profile.chosen=true;
}
void BeginEye(IDirect3DDevice9* device,unsigned eye){
    current=nullptr;if(!profile.chosen||eye>1)return;
    auto& s=profile.eyes[eye];s.device=device;s.begin=Counter();current=&s;profile.mask|=1u<<eye;
}
void EndEye(){
    EyeSample* sample=current;current=nullptr;
    if(sample)sample->elapsed=Counter()-sample->begin;
}
void EndPair(bool complete){
    EndEye();
    if(!profile.chosen)return;profile.chosen=false;
    if(!complete||profile.mask!=3){++profile.rejected;return;}
    for(unsigned e=0;e<2;++e){
        const auto& sample=profile.eyes[e];profile.eyeTicks[e]+=sample.elapsed;
        for(unsigned i=0;i<methodCount;++i){
            const auto& from=sample.cost[i];auto& to=profile.totals[i];
            to.calls+=from.calls;to.ticks+=from.ticks;to.maximum=std::max(to.maximum,from.maximum);
            profile.eyeWrapped[e]+=from.ticks;
        }
    }
    ++profile.pairs;Report();
}
void Reset(){current=nullptr;profile=Profile{};}
}}
#endif
