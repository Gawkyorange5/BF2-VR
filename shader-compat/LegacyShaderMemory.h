// Adapted from BF2142VR c2c254d (MIT); see licenses/BF2142VR-MIT.txt.
#pragma once
#include <windows.h>
namespace CleanVR { namespace LegacyShaderMemory {
// BF2's verified d3dx9_25 tags the high address bit in compiler/effect pointers. Its heap
// and VirtualAlloc arenas must remain below 2 GiB in a large-address-aware host.
// Installed before the first game effect; retained for the runtime's lifetime.
using ShaderMemoryLog = void (*)(const char*, ...);
bool InstallLegacyShaderMemory(HMODULE module, ShaderMemoryLog log);
bool Ready();
struct ShaderMemoryStats { size_t reserved, active, peak, allocations, failures, heapFailures; };
ShaderMemoryStats GetLegacyShaderMemoryStats();
}
}
