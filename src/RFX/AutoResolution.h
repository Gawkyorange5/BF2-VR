#pragma once
#include <Windows.h>
namespace CleanVR { namespace AutoResolution {
// Only installs an import hook; never starts a process or OpenXR in DllMain.
void Install(HMODULE game);
void Confirm(unsigned width,unsigned height);
bool Requested();
void Size(unsigned& w,unsigned& h);
} }
