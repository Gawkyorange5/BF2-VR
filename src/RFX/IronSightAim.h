#pragma once
#include <cmath>
namespace CleanVR {
// Eye position in the held weapon's local frame, in metres. Native weapon
// +Z points down the bore; the eye belongs behind and slightly above it.
// Independent of cosmetic crosshair settings (including shotgun exceptions).
inline float IronSightAlignment(float x,float y,float z,float forwardZ){
    if(!std::isfinite(x)||!std::isfinite(y)||!std::isfinite(z)||!std::isfinite(forwardZ))return 0.f;
    // A small outer band lets the shared ADS policy retain an existing aim
    // without repeatedly entering/exiting at the edge of the sight line.
    if(forwardZ>.983f&&std::abs(x)<.046f&&y>-.015f&&y<.16f&&z>-.70f&&z<-.025f)return 1.f;
    if(forwardZ>.971f&&std::abs(x)<.064f&&y>-.033f&&y<.178f&&z>-.725f&&z<-.007f)return .1f;
    return 0.f;
}
}
