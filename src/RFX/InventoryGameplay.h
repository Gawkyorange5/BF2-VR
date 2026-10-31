#pragma once
#include "VrPoseMath.h"
#include <algorithm>
#include <cmath>

namespace CleanVR { namespace InventoryGameplay {
constexpr unsigned WeaponFlightMs=100,WeaponReturnMs=220;
constexpr unsigned WeaponAwayMs=WeaponFlightMs+WeaponReturnMs;
inline bool Rebounds(int slot,int consumable){return !consumable&&(slot==2||slot==3);}
inline bool C4Stow(Vec hand,Vec anchor,float bodyHandSpeed){
    // Broad shoulder volumes are for grabbing. Stowing a live charge requires
    // a slow release close to its actual saved anchor, not the grab volume.
    return std::isfinite(bodyHandSpeed)&&bodyHandSpeed<.45f&&Length(Sub(hand,anchor))<=.12f;
}
inline Vec SlotBase(int slot,float height,bool pistolChest){
    const Vec p[]={{-.25f,-.20f,-.10f},{.25f,-.20f,-.10f},{.23f,-height,0},
        pistolChest?Vec{-.08f,-.34f,.20f}:Vec{-.23f,-height,0},
        {.18f,-.36f,.18f},{0,-height+.05f,.20f},{0,-.10f,-.28f}};
    return p[slot];
}
inline Vec BarrelDirection(int slot,bool pistolChest){
    if(slot==3&&pistolChest)return {-1,0,0};
    return slot==5?Vec{1,0,0}:slot==4?Vec{0,1,0}:(slot<2||slot==6)?Vec{0,1,0}:Vec{0,-1,0};
}
inline Mat ReturnPose(const Mat& from,const Mat& target,Vec velocity,float fraction){
    const float t=std::max(0.f,std::min(1.f,fraction));
    const float smooth=t*t*(3.f-2.f*t);
    Mat pose=BlendRigidPose(from,target,smooth);
    // Hermite release tangent preserves the outward motion at the turnaround;
    // both the position and velocity settle into the moving holster at t=1.
    const Vec tangent=Scale(velocity,(WeaponReturnMs*.001f)*t*(1-t)*(1-t));
    SetPosition(pose,Add(Position(pose),tangent));return pose;
}
}}
