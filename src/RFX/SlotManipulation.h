#pragma once
#include "XrSessionClient.h"
#include <algorithm>
#include <cmath>

namespace CleanVR { namespace SlotManipulation {
// LH BF2 row vectors; local axes are right/up/forward. Stored angles are
// degrees, applied X then Y then Z before the torso transform. They describe
// an extra holster rotation, not a live weapon/controller aim calibration.
constexpr float radians=.017453292519943295f;
inline Mat Rotation(const float degrees[3]) {
    const float x=degrees[0]*radians,y=degrees[1]*radians,z=degrees[2]*radians;
    Mat a=Identity(),b=Yaw(y),c=Identity();
    a.m[1][1]=a.m[2][2]=std::cos(x);a.m[1][2]=std::sin(x);a.m[2][1]=-std::sin(x);
    c.m[0][0]=c.m[1][1]=std::cos(z);c.m[0][1]=std::sin(z);c.m[1][0]=-std::sin(z);
    return Mul(Mul(a,b),c);
}
inline void Angles(const Mat& r,float out[3]) {
    const float sy=std::max(-1.f,std::min(1.f,-r.m[0][2]));
    const float y=std::asin(sy),cy=std::cos(y);
    const float x=std::abs(cy)>.0005f?std::atan2(r.m[1][2],r.m[2][2]):std::atan2(sy*r.m[1][0],r.m[1][1]);
    const float z=std::abs(cy)>.0005f?std::atan2(r.m[0][1],r.m[0][0]):0.f;
    out[0]=x/radians;out[1]=y/radians;out[2]=z/radians;
}
inline Mat Anchor(Vec base,const float offset[3],const float rotation[3],const Mat& torso) {
    Mat a=Mul(Rotation(rotation),torso);
    SetPosition(a,Point(Add(base,{offset[0],offset[1],offset[2]}),torso));return a;
}
enum class Event { None, Grabbed, Moved, Released, Cancelled };
struct Drag {
    int hand=-1,selection=-1;bool was[2]{true,true},blocked=true;
    std::uint64_t origin=0;ULONGLONG tick=0;Mat relative{};
    void Cancel(){hand=-1;selection=-1;blocked=true;was[0]=was[1]=true;}
    Event Update(const Runtime::Frame& f,int key,const Mat& anchor,const Mat& torso,Vec base,float offset[3],float rotation[3]) {
        const bool gap=tick&&(f.tick<tick||f.tick-tick>150||f.origin!=origin);
        tick=f.tick;origin=f.origin;
        if(gap||!f.valid||!f.focused||!Rigid(torso)||!Rigid(anchor)){
            const bool dragging=hand>=0;Cancel();return dragging?Event::Cancelled:Event::None;
        }
        if(blocked){
            if(f.hands[0].squeeze<.35f&&f.hands[1].squeeze<.35f){blocked=false;was[0]=was[1]=false;}
            return Event::None;
        }
        bool down[2],press[2];for(int i=0;i<2;++i){down[i]=f.hands[i].squeeze>(was[i]?.35f:.60f);press[i]=down[i]&&!was[i];was[i]=down[i];}
        if(hand>=0){
            if(selection!=key||!f.hands[hand].gripValid||!Rigid(f.hands[hand].grip)){Cancel();return Event::Cancelled;}
            // Preserve the initial hand-to-bubble transform: no snapping when
            // touched off-center. All 6DoF motion comes from the grabbing hand.
            Mat local=Mul(Mul(relative,f.hands[hand].grip),Inverse(torso));
            if(!Rigid(local)){Cancel();return Event::Cancelled;}
            const Vec d=Sub(Position(local),base);const float xyz[]={d.x,d.y,d.z};
            for(int i=0;i<3;++i)offset[i]=std::max(-1.25f,std::min(1.25f,xyz[i]));
            Angles(local,rotation);
            if(!down[hand]){hand=-1;selection=-1;return Event::Released;}
            return Event::Moved;
        }
        for(int i=0;i<2;++i)if(press[i]&&f.hands[i].gripValid&&Rigid(f.hands[i].grip)&&Length(Sub(Position(f.hands[i].grip),Position(anchor)))<=.10f){
            hand=i;selection=key;relative=Mul(anchor,Inverse(f.hands[i].grip));return Event::Grabbed;
        }
        return Event::None;
    }
};
}}
