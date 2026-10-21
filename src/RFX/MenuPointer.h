#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace CleanVR { namespace MenuPointer {
// One glyph definition for the CPU cinema canvas and the stereo radio HUD.
constexpr std::uint32_t Border=0xff000000u,Fill=0xff50ff3cu;
struct Rect { int left,top,right,bottom; };
struct Glyph { Rect border[2]{},fill[2]{};unsigned borderCount=0,fillCount=0; };
inline Glyph Make(float u,float v,unsigned width,unsigned height,int radius){
    Glyph g{};
    if(!width||!height||!std::isfinite(u)||!std::isfinite(v)||u<0||u>1||v<0||v>1)return g;
    const int x=int(u*(width-1)),y=int(v*(height-1)),r=std::max(1,std::min(64,radius));
    const auto add=[&](Rect* rect,unsigned& count,int halfX,int halfY){
        Rect q{std::max(0,x-halfX),std::max(0,y-halfY),std::min(int(width),x+halfX+1),std::min(int(height),y+halfY+1)};
        if(q.left<q.right&&q.top<q.bottom)rect[count++]=q;
    };
    add(g.border,g.borderCount,2,r+1);add(g.border,g.borderCount,r+1,2);
    add(g.fill,g.fillCount,1,r);add(g.fill,g.fillCount,r,1);
    return g;
}
}}
