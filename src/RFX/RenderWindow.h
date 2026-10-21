#pragma once
#include <Windows.h>
#include <d3d9.h>

namespace CleanVR { namespace RenderWindow {
// Only the main BF2 window is adapted, and only for a successful startup query.
bool Attach(HWND window);
bool TogglePreview();
float DesktopAspect(); // width/height; zero when automatic/custom window is inactive
bool CanvasSize(HWND window,unsigned& width,unsigned& height);
bool MenuPointer(HWND window,bool valid,float u,float v,bool pressed);
void ReleaseMenuPointer();
void ConfigurePresentation(D3DPRESENT_PARAMETERS* params, HWND focus=nullptr);
} }
