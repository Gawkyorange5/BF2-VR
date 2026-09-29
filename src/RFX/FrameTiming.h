#pragma once
#include <Windows.h>

namespace CleanVR {
// CPU wall time, including driver waits. These are not GPU timestamp queries.
inline double FrameClockMs(){
    static const double scale=[](){LARGE_INTEGER f{};QueryPerformanceFrequency(&f);return 1000.0/(double)f.QuadPart;}();
    LARGE_INTEGER t{};QueryPerformanceCounter(&t);return (double)t.QuadPart*scale;
}
}
