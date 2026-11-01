#pragma once
#include <cstdint>
namespace CleanVR {
// Physical sight alignment must settle briefly; native reload/fire cancellation
// gets a backoff instead of being forced into ADS again on the next frame.
struct ScopeAdsPolicy {
    std::uint64_t enter=0,lastVisible=0,lastTick=0,retryAfter=0;
    bool desired=false;
    bool Update(float visibility,bool blocked,bool cancelled,std::uint64_t now){
        if(lastTick&&(now<lastTick||now-lastTick>150))*this={};
        lastTick=now;
        if(cancelled){desired=false;enter=0;retryAfter=now+300;}
        if(blocked){desired=false;enter=lastVisible=0;return false;}
        if(now<retryAfter)return false;
        if(visibility>=.25f){
            lastVisible=now;if(!enter)enter=now;
            if(now>=enter+80)desired=true;
        }else{
            enter=0;if(visibility>=.05f)lastVisible=now;
            if(!lastVisible||now-lastVisible>100)desired=false;
        }
        return desired;
    }
};
}
