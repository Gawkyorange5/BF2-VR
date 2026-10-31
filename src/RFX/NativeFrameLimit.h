#pragma once
#include <Windows.h>
#include <cstring>
namespace CleanVR { namespace NativeFrameLimit {
// Read-only diagnostic, independently recovered from the supported BF2 1.5
// console registration. Never call or patch the setter, and never write the
// frame limit. BF2142's addresses/field offsets differ.
inline int Read(const BYTE* image) noexcept {
    __try {
        if(!image)return -1;
        const BYTE getter[]={0x8b,0x81,0x94,0,0,0,0xc3};
        const BYTE setter[]={0x55,0x8b,0xec,0x8b,0x45,8,0x85,0xc0,0x56,0x8b,0xf1,0x7e,0x0b,0x89,0x86,0x94,0,0,0,0x5e,0x5d,0xc2,4,0,0xe8};
        if(std::memcmp(image+0x1d1fa0,getter,sizeof(getter))||std::memcmp(image+0x3100,setter,sizeof(setter))||std::memcmp(image+0x4933c4,"lockFps",8))return -1;
        DWORD name=0,table=0,global1=0,global2=0;LONG getCall=0,setCall=0;
        std::memcpy(&name,image+0x34e21,4);std::memcpy(&table,image+0x49339c,4);
        std::memcpy(&global1,image+0x34ee5,4);std::memcpy(&global2,image+0x34f07,4);
        std::memcpy(&getCall,image+0x34eea,4);std::memcpy(&setCall,image+0x34f0d,4);
        if(name!=DWORD(image+0x4933c4)||table!=DWORD(image+0x34ece)||global1!=DWORD(image+0x586f48)||global2!=global1||
           image[0x34ee9]!=0xe8||image[0x34f0c]!=0xe8||image+0x34eee+getCall!=image+0x1d1fa0||image+0x34f11+setCall!=image+0x3100)return -1;
        const BYTE* object=nullptr;std::memcpy(&object,image+0x586f48,sizeof(object));if(!object)return -1;
        int limit=-1;std::memcpy(&limit,object+0x94,4);return limit>=0&&limit<=1000?limit:-1;
    }__except(EXCEPTION_EXECUTE_HANDLER){return -1;}
}
inline int Current() noexcept {return Read(reinterpret_cast<const BYTE*>(GetModuleHandleW(nullptr)));}
}}
