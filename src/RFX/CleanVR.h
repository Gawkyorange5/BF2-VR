#pragma once
#include <d3d9.h>
#include "Bf2Adapter.h"
namespace CleanVR {
void AttachDevice(IDirect3DDevice9*);
void BeforeReset();
void AfterReset(IDirect3DDevice9*,HRESULT);
void AfterEndScene(IDirect3DDevice9*);
bool BeforePresent(IDirect3DDevice9*); // true suppresses a stereo replay Present
bool BeforeNativePresent(); // BF2 presents its swap chain without the device wrapper
bool BeginHud();
bool ReuseHud(); // second eye can retain the already-isolated mono canvas
void RecordHudDraw(double milliseconds);
void EndHud();
bool DrawScene(Engine::SceneFunction,void*,unsigned,unsigned,unsigned);
}
