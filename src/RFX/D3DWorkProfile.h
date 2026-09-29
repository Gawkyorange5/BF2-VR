#pragma once
#include <d3d9.h>

// Normal-play builds compile the instrumentation out of the forwarding path.
// Define this to 1 only when building a separate diagnostic DLL.
#ifndef BF2_DRAW_DIAGNOSTICS
#define BF2_DRAW_DIAGNOSTICS 0
#endif

namespace CleanVR { namespace D3DWorkProfile {
enum class Method : unsigned {
    TestCooperativeLevel,
    EvictManagedResources,
    GetDeviceCaps,
    GetDisplayMode,
    GetCreationParameters,
    SetCursorProperties,
    CreateAdditionalSwapChain,
    GetSwapChain,
    GetBackBuffer,
    GetRasterStatus,
    SetDialogBoxMode,
    CreateTexture,
    CreateVolumeTexture,
    CreateCubeTexture,
    CreateVertexBuffer,
    CreateIndexBuffer,
    CreateRenderTarget,
    CreateDepthStencilSurface,
    UpdateSurface,
    UpdateTexture,
    GetRenderTargetData,
    GetFrontBufferData,
    StretchRect,
    ColorFill,
    CreateOffscreenPlainSurface,
    SetRenderTarget,
    GetRenderTarget,
    SetDepthStencilSurface,
    GetDepthStencilSurface,
    BeginScene,
    EndScene,
    Clear,
    SetTransform,
    GetTransform,
    MultiplyTransform,
    SetViewport,
    GetViewport,
    SetMaterial,
    GetMaterial,
    SetLight,
    GetLight,
    LightEnable,
    GetLightEnable,
    SetClipPlane,
    GetClipPlane,
    SetRenderState,
    GetRenderState,
    CreateStateBlock,
    BeginStateBlock,
    EndStateBlock,
    SetClipStatus,
    GetClipStatus,
    GetTexture,
    SetTexture,
    GetTextureStageState,
    SetTextureStageState,
    GetSamplerState,
    SetSamplerState,
    ValidateDevice,
    SetPaletteEntries,
    GetPaletteEntries,
    SetCurrentTexturePalette,
    GetCurrentTexturePalette,
    SetScissorRect,
    GetScissorRect,
    SetSoftwareVertexProcessing,
    SetNPatchMode,
    DrawPrimitive,
    DrawIndexedPrimitive,
    DrawPrimitiveUP,
    DrawIndexedPrimitiveUP,
    ProcessVertices,
    CreateVertexDeclaration,
    SetVertexDeclaration,
    GetVertexDeclaration,
    SetFVF,
    GetFVF,
    CreateVertexShader,
    SetVertexShader,
    GetVertexShader,
    SetVertexShaderConstantF,
    GetVertexShaderConstantF,
    SetVertexShaderConstantI,
    GetVertexShaderConstantI,
    SetVertexShaderConstantB,
    GetVertexShaderConstantB,
    SetStreamSource,
    GetStreamSource,
    SetStreamSourceFreq,
    GetStreamSourceFreq,
    SetIndices,
    GetIndices,
    CreatePixelShader,
    SetPixelShader,
    GetPixelShader,
    SetPixelShaderConstantF,
    GetPixelShaderConstantF,
    SetPixelShaderConstantI,
    GetPixelShaderConstantI,
    SetPixelShaderConstantB,
    GetPixelShaderConstantB,
    DrawRectPatch,
    DrawTriPatch,
    DeletePatch,
    CreateQuery,
    Count
};
constexpr unsigned methodCount = static_cast<unsigned>(Method::Count);
#if BF2_DRAW_DIAGNOSTICS
struct Cost { unsigned calls=0; LONGLONG ticks=0, maximum=0; };
struct EyeSample {
    IDirect3DDevice9* device=nullptr;
    Cost cost[methodCount]{};
    LONGLONG begin=0, elapsed=0;
};
// Only the thread running the sampled native scene has a current sample.
// No locks, allocations, GPU queries, waits or logging in the forwarding path.
extern thread_local EyeSample* current;
template<class F> __forceinline HRESULT Invoke(IDirect3DDevice9* device, Method method, F call) {
    EyeSample* sample=current;
    if(!sample||sample->device!=device)return call();
    LARGE_INTEGER begin{},end{};QueryPerformanceCounter(&begin);
    const HRESULT result=call();
    QueryPerformanceCounter(&end);
    const LONGLONG ticks=end.QuadPart-begin.QuadPart;
    auto& cost=sample->cost[static_cast<unsigned>(method)];
    ++cost.calls;cost.ticks+=ticks;if(ticks>cost.maximum)cost.maximum=ticks;
    return result;
}
void BeginPair();
void BeginEye(IDirect3DDevice9* device,unsigned eye);
void EndEye();
void EndPair(bool complete);
void Reset();
#else
template<class F> __forceinline HRESULT Invoke(IDirect3DDevice9*, Method, F call) {
    return call();
}
inline void BeginPair() {}
inline void BeginEye(IDirect3DDevice9*,unsigned) {}
inline void EndEye() {}
inline void EndPair(bool) {}
inline void Reset() {}
#endif
}}
