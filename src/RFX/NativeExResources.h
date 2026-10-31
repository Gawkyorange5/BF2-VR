// Adapted from justiceofrip/BF2142VR 7fbc90d (MIT); see ../third_party/BF2142VR_LICENSE.txt.
#pragma once
#include <d3d9.h>
namespace CleanVR {
using ExResourceLog=void(*)(const char*,...);
bool InstallNativeExResources(IDirect3DDevice9*,ExResourceLog);
}
