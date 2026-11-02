// Only the main renderer's FOV query is intercepted. Native player target/current
// FOV, zoom-component state, deviation, recoil and movement remain untouched.
// BF2+5337F queries Player::GetCurrentFov; +53398 supplies it to the render view.
// Player::SetFov (+FA300) resolves a zero/unzoomed request through the active
// native camera's vtable +58. Use that same camera default for physical irons.
std::atomic<void*> ironSightSceneWeapon{nullptr};
using IronSceneFloat= float(__thiscall*)(void*);
using IronSceneCamera= void*(__thiscall*)(void*);

float __fastcall IronSightSceneFov(void* player,void*){
    void* table=Value<void*>(player);
    auto current=Value<IronSceneFloat>(table,0x8C);
    const float native=current(player);
    void* weapon=ironSightSceneWeapon.load(std::memory_order_acquire);
    if(!weapon||!Config().hands||player!=Player()||!referenceValid||!binding.rightReady||
       !CurrentScopeWeapon(weapon,Soldier(),Value<void*>(player,0xCC)))return native;
    char name[80]{};
    if(!ReloadName(weapon,name)||FindScopeProfile(name))return native;
    // This eligibility persists while lowering an iron sight, so the native
    // zoom-out transition cannot produce a one-frame scenery LOD jump either.
    auto cameraGetter=Value<IronSceneCamera>(table,0x17C);
    void* camera=cameraGetter?cameraGetter(player):nullptr;
    auto defaultGetter=Value<IronSceneFloat>(Value<void*>(camera),0x58);
    if(!defaultGetter)return native;
    const float normal=defaultGetter(camera);
    return std::isfinite(normal)&&normal>0.f&&normal<3.141592654f?normal:native;
}

bool InstallIronSightSceneFov(){
    const BYTE query[]={0x8B,0x07,0x8B,0xCF,0xFF,0x90,0x8C,0,0,0,0xD9,0x5D,0x10};
    const BYTE output[]={0x8B,0x4D,0x10,0x8B,0x10,0x51,0x8B,0xC8,0xFF,0x52,0x18};
    const BYTE getter[]={0xD9,0x81,0xC4,0,0,0,0xC3};
    if(!Match(game+0x5337B,query,sizeof(query))||!Match(game+0x53390,output,sizeof(output))||
       !Match(game+0xFA3C0,getter,sizeof(getter))||
       Value<void*>(game+0x4AC530,0x8C)!=game+0xFA3C0){
        Log("ADS154 iron scene FOV signature unavailable; retaining r153 behavior");return false;
    }
    BYTE patch[6]={0xE8,0,0,0,0,0x90};
    const LONG displacement=static_cast<LONG>(reinterpret_cast<BYTE*>(&IronSightSceneFov)-(game+0x53384));
    memcpy(patch+1,&displacement,4);
    DWORD old=0;if(!VirtualProtect(game+0x5337F,sizeof(patch),PAGE_EXECUTE_READWRITE,&old))return false;
    memcpy(game+0x5337F,patch,sizeof(patch));
    DWORD ignored=0;VirtualProtect(game+0x5337F,sizeof(patch),old,&ignored);
    FlushInstructionCache(GetCurrentProcess(),game+0x5337F,sizeof(patch));
    Log("ADS154 iron sights use native unzoomed scene FOV; native ADS and all optic paths retained");return true;
}
