#pragma once
#include <d3d9.h>
namespace CleanVR { namespace NativeEx {
HRESULT CreateDevice(IDirect3D9* classic, UINT adapter, D3DDEVTYPE type, HWND window,
    DWORD flags, D3DPRESENT_PARAMETERS* parameters, IDirect3DDevice9** output);
bool Active(IDirect3DDevice9* device);
HRESULT Reset(IDirect3DDevice9* device, D3DPRESENT_PARAMETERS* parameters);
}}
