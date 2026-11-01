#pragma once
#include <cstring>
#include <initializer_list>
namespace CleanVR {
// Audited native HUD picture names, not texture/color/position heuristics.
// These are the flat-screen AimPoint/reflex/holographic overlays; the VR
// lens shader draws its own weapon-aligned sight. Never match hit markers,
// sniper masks, vehicle sights or launcher target/connection graphics.
inline bool NativeReflexPicture(const char* name){
    if(!name)return false;
    for(const char* picture:{
        "M4AltCrosshairPictureNode", "ReflexAltCrosshairPictureNode",
        "EotechAltCrosshairPictureNode", "HD33AltCrosshairPictureNode",
        "KobraAltCrosshairPictureNode", "PKArdsPictureNode",
        "PKACrosshairMarksPictureNode", "PKASreddotPictureNode",
        "PKASCrosshairMarksPictureNode", "JGM4RDSPictureNode",
        "JGM4CrosshairMarksPictureNode"})
        if(!std::strcmp(name,picture))return true;
    return false;
}
}
