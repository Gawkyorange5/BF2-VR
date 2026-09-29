#pragma once
#include <d3d9.h>

namespace CleanVR { namespace HudCapture {
// Called only around the verified native HudManager draw, on the game thread.
bool Begin(IDirect3DDevice9*);
bool End();
bool Upload(double& readMs,double& uploadMs);
IDirect3DSurface9* ResolvedForTransfer(); // borrowed, valid until Reset/next Begin
void CompositeDesktop();
void Reset();
bool Ready();
unsigned Width();
unsigned Height();
void NewPair();
IDirect3DSurface9* Target(IDirect3DDevice9*,DWORD,IDirect3DSurface9*);
DWORD State(IDirect3DDevice9*,D3DRENDERSTATETYPE,DWORD);
}}
