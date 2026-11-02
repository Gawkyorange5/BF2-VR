// Physical ADS / visible native LOD approach follows BF2142VR NativeOptics
// (MIT). These addresses and object contracts were independently checked in
// BF2 1.5. Use each equipped weapon's native zoom component, not a sniper-wide
// spread replacement. No template, projectile, recoil or targeting edits.
using ScopeSetZoom=void(__thiscall*)(void*,int,bool);
using ScopeSetLod=void(__thiscall*)(void*,int);
ScopeSetZoom nativeScopeZoom=nullptr;
ScopeSetLod originalScopeLod=nullptr;
bool scopeAdsProfile=false;
struct ScopeZoom {void* base=nullptr;int step=0;float factor=0;};
struct NativeScopeState {
    void* weapon=nullptr;void* owner=nullptr;void* identity=nullptr;
    std::uint64_t origin=0;bool owned=false;ScopeAdsPolicy policy{};
} nativeScopeAds;
std::atomic<void*> scopeAdsVisualWeapon{nullptr};

bool ReadScopeZoom(void* weapon,ScopeZoom& result){
    char name[80]{};
    if(!ReloadName(weapon,name)||(!FindScopeProfile(name)&&!IsAimedHandheld(weapon)))return false;
    // Only GenericFireArm's bounded component fields, with exact interface,
    // base, template and owner identities. Components are resolved afresh:
    // DZB2 can recreate them when switching a launcher's firing mode.
    for(unsigned offset=0x184;offset<=0x1B0;offset+=4){
        auto* component=Value<BYTE*>(weapon,offset);
        if(Value<void*>(component)!=game+0x4F58F8)continue;
        BYTE* base=component-0x10;
        if(Value<void*>(base)!=game+0x4F59B8||Value<void*>(base,0x0C)!=weapon)return false;
        void* def=Value<void*>(component,0x20);
        if(Value<void*>(def)!=game+0x4F5B18)return false;
        const int lod=Value<int>(def,0x2C);
        if(lod<0||lod>1)return false;
        const auto begin=Value<std::uintptr_t>(def,0x18),end=Value<std::uintptr_t>(def,0x1C);
        if(!begin||end<begin||end-begin<8||end-begin>64||(end-begin)%4)return false;
        float factors[16]{};
        if(!Copy(factors,reinterpret_cast<const void*>(begin),end-begin)||
           !std::isfinite(factors[0])||std::abs(factors[0])>.00001f)return false;
        for(unsigned i=1;i<(end-begin)/4;++i)
            if(!std::isfinite(factors[i])||factors[i]<=0||factors[i]>1.f)return false;
        const int step=Value<int>(component,0x0C);
        if(step<0||static_cast<unsigned>(step)>=(end-begin)/4)return false;
        result={base,step,factors[step?step:1]};return true;
    }
    return false;
}
bool CurrentScopeWeapon(void* weapon,void* owner,void* identity){
    if(!weapon||!owner||Soldier()!=owner||Value<void*>(Player(),0xCC)!=identity||
       !LocalAlive()||Value<void*>(weapon,0x28)!=owner||Value<int>(owner,0x2B8)!=0||Value<void*>(owner,0x254))return false;
    void* seat=Value<void*>(Value<void*>(Player(),0x80),4);
    if(seat&&seat!=owner&&!PassengerWeaponSeat(owner))return false;
    return InventoryItem(owner,Value<int>(owner,0x1E0))==weapon;
}
#include "IronSightAds.inl"
#include "IronSightSceneFov.inl"
void ResetScopeAds(){
    ScopeZoom zoom{};
    const auto& s=nativeScopeAds;
    if(s.owned&&nativeScopeZoom&&CurrentScopeWeapon(s.weapon,s.owner,s.identity)&&ReadScopeZoom(s.weapon,zoom)&&zoom.step)
        nativeScopeZoom(zoom.base,0,false);
    nativeScopeAds={};scopeAdsVisualWeapon.store(nullptr,std::memory_order_release);
    ironSightSceneWeapon.store(nullptr,std::memory_order_release);
}
void __fastcall ScopeLodHook(void* self,void*,int lod){
    void* weapon=scopeAdsVisualWeapon.load(std::memory_order_acquire);
    // Remote soldiers reject before player/optic lookups. Only the selected
    // local physical weapon retains its visible first-person LOD0.
    if(lod==1&&weapon&&Value<void*>(self,0x0C)==weapon){
        ScopeZoom zoom{};
        if(Config().hands&&CurrentScopeWeapon(weapon,Soldier(),Value<void*>(Player(),0xCC))&&
           ReadScopeZoom(weapon,zoom)&&zoom.base==self)lod=0;
    }
    originalScopeLod(self,lod);
}
void UpdateScopeAds(){
    void* owner=Soldier();void* identity=Value<void*>(Player(),0xCC);ScopeZoom zoom{};
    if(!scopeAdsProfile||!Config().hands||
       !pairFrame.focused||!referenceValid||!binding.rightReady||MenuActive()||InventoryStowed(owner)||
       !LocalAlive()){
        ResetScopeAds();return;
    }
    void* aimedWeapon=scopeWeapon;float alignment=0.f;bool iron=false;
    if(aimedWeapon){
        // Preserve the accepted optic path, including disabled/unavailable
        // scopes: they must never fall through to iron-sight activation.
        if(!Tuning().gunScopes||!scopeAvailable){ResetScopeAds();return;}
        alignment=scopeReady?std::max(scopeView.visibility[0],scopeView.visibility[1]):0.f;
    }else{
        if(!PrepareIronSightAds(aimedWeapon,alignment)){ResetScopeAds();return;}
        iron=true;
    }
    if(!CurrentScopeWeapon(aimedWeapon,owner,identity)||!ReadScopeZoom(aimedWeapon,zoom)){ResetScopeAds();return;}
    if(nativeScopeAds.weapon!=aimedWeapon||nativeScopeAds.owner!=owner||nativeScopeAds.identity!=identity||nativeScopeAds.origin!=pairFrame.origin){
        ResetScopeAds();nativeScopeAds.weapon=aimedWeapon;nativeScopeAds.owner=owner;
        nativeScopeAds.identity=identity;nativeScopeAds.origin=pairFrame.origin;
        if(!ReadScopeZoom(aimedWeapon,zoom))return;
    }
    scopeAdsVisualWeapon.store(aimedWeapon,std::memory_order_release);
    ironSightSceneWeapon.store(iron?aimedWeapon:nullptr,std::memory_order_release);
    bool reloading=false;
    AcquireSRWLockShared(&reloadLock);
    if(reload.owner==owner&&reload.weapon==aimedWeapon)
        reloading=ReloadBlocksFire()||reload.ammo.state>=2;
    ReleaseSRWLockShared(&reloadLock);
    const bool sprinting=Value<BYTE>(Player(),0x187)!=0;
    auto& s=nativeScopeAds;
    const bool cancelled=s.owned&&!zoom.step;
    if(cancelled)s.owned=false;
    const bool desired=s.policy.Update(alignment,reloading||sprinting,cancelled,GetTickCount64());
    if(desired&&!zoom.step){
        nativeScopeZoom(zoom.base,1,false);
        ScopeZoom after{};
        if(ReadScopeZoom(aimedWeapon,after)&&after.step==1){
            s.owned=true;
            static unsigned reports=0;
            if(reports++<32){char name[80]{};ReloadName(aimedWeapon,name);
                Log("ADS154 native ADS entered weapon=%s sight=%s step=1 factor=%.3f alignment=%.2f; original spread/recoil, visible LOD0",name,iron?"iron":"optic",after.factor,alignment);}
        }
    }else if(!desired&&s.owned){
        if(zoom.step)nativeScopeZoom(zoom.base,0,false);
        s.owned=false;
    }
}
bool InstallScopeAds(){
    const BYTE setZoom[]={0x56,0x8B,0xF1,0x8B,0x46,0x0C,0x8D,0x88,0x70,0x01,0,0};
    const BYTE wrapper[]={0x8B,0x44,0x24,0x04,0x6A,0x01,0x50,0x83,0xC1,0xF0,0xE8};
    const BYTE lod[]={0x53,0x8B,0x5C,0x24,0x08,0x85,0xDB};
    if(!Match(game+0x206C20,setZoom,sizeof(setZoom))||!Match(game+0x206DE0,wrapper,sizeof(wrapper))||
       Value<LONG>(game+0x206DEB)!=-0x1CF||Value<WORD>(game+0x206D0C)!=0x08C2||
       Value<void*>(game+0x4F58F8,0x98)!=game+0x206DE0||Value<void*>(game+0x4F59B8,0x44)!=game+0x206980)return false;
    if(!Jump(game+0x206980,lod,sizeof(lod),reinterpret_cast<void*>(&ScopeLodHook),reinterpret_cast<void**>(&originalScopeLod)))return false;
    nativeScopeZoom=reinterpret_cast<ScopeSetZoom>(game+0x206C20);scopeAdsProfile=true;
    InstallIronSightSceneFov();
    Log("ADS154 verified native zoom/LOD: iron sights and retained supported optics; per-weapon native ADS, no spread overrides");return true;
}
