// Keep the camera's seat offset through vehicle death. BF2 can replace an
// aircraft with a short-lived Particle hull; keep a WEAK reference to that
// hull, not the deleted vehicle address. No physics or lifetime is extended.
bool wreckProfile=false;
using WreckFactory=void*(__thiscall*)(void*);
WreckFactory originalWreckFactory=nullptr;
struct WreckCandidate {void* weak=nullptr;ULONGLONG born=0;};
struct WreckView {
    void* root=nullptr;void* rootWeak=nullptr;void* definition=nullptr;void* debrisWeak=nullptr;
    Mat seat=Identity(),rootWorld=Identity(),lastWorld=Identity();
    Vec emitterOffset{};const char* particle=nullptr;
    WreckCandidate candidates[4]{};
    ULONGLONG liveTick=0,followTick=0;DWORD thread=0;
    bool debrisSelected=false,following=false;
} wreck;
// BF2 1.5 Object weak handle: +7C points to {refcount,object}. Native removal
// (+3207E0) clears object and drops its ref. Our ref keeps only these 8 bytes
// valid, preventing allocator reuse from attaching the camera to a new object.
void* WreckRetain(void* object){
    __try{
        if(!wreckProfile||!VehicleObject(object))return nullptr;
        void* weak=nullptr;
        reinterpret_cast<void*(__thiscall*)(void*,void**)>(game+0x320890)(object,&weak);
        return weak;
    }__except(EXCEPTION_EXECUTE_HANDLER){return nullptr;}
}
void WreckRelease(void*& weak){
    void* old=weak;weak=nullptr;if(!old)return;
    __try{
        auto* count=static_cast<LONG*>(old);
        if(*count>0&&*count<1000000&&--*count==0)
            reinterpret_cast<void(__cdecl*)(void*)>(game+0x2D10)(old);
    }__except(EXCEPTION_EXECUTE_HANDLER){}
}
void ResetWreckView(){
    WreckRelease(wreck.rootWeak);WreckRelease(wreck.debrisWeak);
    for(auto& c:wreck.candidates)WreckRelease(c.weak);
    wreck={};
}
bool WreckWorld(void* weak,void* definition,Mat& world){
    void* object=Value<void*>(weak,4);
    return object&&Value<void*>(object,0x7C)==weak&&VehicleObject(object)&&
        (!definition||Value<void*>(object,0x14)==definition)&&Read(object,0x88,world)&&Rigid(world);
}
void ChooseWreckParticle(void* root){
    char name[80]{};if(!VehicleName(Value<void*>(root,0x14),name))return;
    struct Entry {const char* root;const char* particle;Vec offset;};
    // Particle and emitter origins from the user's stock Objects_server.zip.
    // Other vehicles still follow their original hull for as long as it lives.
    static const Entry entries[]={
        {"ahe_ah1z","p_wreck_cobra_hull",{}},
        {"ahe_z10","p_wreck_z10_fuselage",{0,.245707f,.913753f}},
        {"the_mi17","p_wreck_mi17_cockpit",{1.401f,2.146f,-.077f}},
        {"usthe_uh60","p_wreck_uh60_mainbody",{-.535807f,-.665382f,1.40795f}},
        {"chthe_z8","p_wreck_z8_mainbody",{.00000925592f,1.9858f,.0000118967f}},
        {"ruair_mig29","p_wreck_mig29_cockpit",{-.485151f,0,0}},
        {"usair_f15","p_f15_hullpart",{.224f,.495f,-4.357f}}
    };
    for(const auto& e:entries)if(!strcmp(name,e.root)){wreck.particle=e.particle;wreck.emitterOffset=e.offset;break;}
}
bool IsWreckTemplate(void* definition){
    if(!wreck.particle||Value<void*>(definition)!=game+0x5016C8)return false;
    const unsigned length=Value<unsigned>(definition,0x20),capacity=Value<unsigned>(definition,0x24);
    if(length!=strlen(wreck.particle)||capacity<length||capacity>4096)return false;
    const char* text=capacity<16?static_cast<const char*>(definition)+0x10:Value<const char*>(definition,0x10);
    char name[80]{};if(length>=sizeof(name)||!Copy(name,text,length+1)||name[length])return false;
    return !_stricmp(name,wreck.particle);
}
void* __fastcall WreckFactoryHook(void* self,void*){
    void* result=originalWreckFactory(self);
    // Only the particle factory is observed, not the hot world draw/getter.
    // Native refcounts belong to the game thread that captured the seat.
    if(!result||!wreck.thread||GetCurrentThreadId()!=wreck.thread||wreck.debrisSelected||!IsWreckTemplate(self))return result;
    const auto now=GetTickCount64();if(now<wreck.liveTick||now-wreck.liveTick>1000)return result;
    WreckCandidate* slot=&wreck.candidates[0];
    for(auto& c:wreck.candidates)if(!c.weak||c.born<slot->born)slot=&c;
    WreckRelease(slot->weak);slot->weak=WreckRetain(result);slot->born=now;
    return result;
}
void CaptureWreckSeat(const VehicleSample& occupied,const Mat& actual,ULONGLONG now){
    if(!wreckProfile)return;
    if(wreck.root!=occupied.root||Value<void*>(wreck.rootWeak,4)!=occupied.root){
        ResetWreckView();wreck.root=occupied.root;wreck.rootWeak=WreckRetain(occupied.root);
        wreck.definition=Value<void*>(occupied.root,0x14);wreck.thread=GetCurrentThreadId();
        ChooseWreckParticle(occupied.root);
        Log("HELMET63 seat captured root=%p weak=%p debris=%s",wreck.root,wreck.rootWeak,wreck.particle?wreck.particle:"original hull");
    }
    wreck.seat=Mul(actual,Inverse(occupied.world));wreck.rootWorld=occupied.world;
    wreck.lastWorld=occupied.world;wreck.liveTick=now;wreck.followTick=now;wreck.following=false;
    for(auto& c:wreck.candidates)if(c.weak&&(now<c.born||now-c.born>1000))WreckRelease(c.weak);
}
bool FollowWreckSeat(ULONGLONG now,Mat& camera){
    if(!wreckProfile||!wreck.root||now<wreck.liveTick)return false;
    if(!wreck.debrisSelected&&now-wreck.liveTick<=1000){
        // Match only newly emitted, model-specific fuselage pieces near this
        // seat's last hull origin. Reject ambiguous simultaneous nearby wrecks.
        int best=-1;float distance=25.f,second=10000.f;Mat chosen{};
        const Vec expected=Point(wreck.emitterOffset,wreck.rootWorld);
        for(int i=0;i<4;++i){auto& c=wreck.candidates[i];Mat world{};
            if(!c.weak||now<c.born||now-c.born>750||c.born+200<wreck.liveTick||!WreckWorld(c.weak,nullptr,world))continue;
            const float d=Length(Sub(Position(world),expected));
            if(d<distance){second=distance;distance=d;best=i;chosen=world;}else second=std::min(second,d);
        }
        if(best>=0&&second-distance>2.f){
            wreck.debrisWeak=wreck.candidates[best].weak;wreck.candidates[best].weak=nullptr;
            wreck.debrisSelected=true;
            // The particle's local origin is the emitter origin, not always
            // the original vehicle origin. Preserve the actual seat offset.
            Mat emitter=Identity();SetPosition(emitter,wreck.emitterOffset);
            wreck.seat=Mul(wreck.seat,Inverse(emitter));wreck.lastWorld=chosen;wreck.followTick=now;
            Log("HELMET63 attached to emitted hull %s distance=%.2f",wreck.particle,distance);
            WreckRelease(wreck.rootWeak);for(auto& c:wreck.candidates)WreckRelease(c.weak);
        }
    }
    Mat world{};bool valid=WreckWorld(wreck.debrisSelected?wreck.debrisWeak:wreck.rootWeak,
        wreck.debrisSelected?nullptr:wreck.definition,world);
    if(valid){
        const float seconds=now>=wreck.followTick?float(now-wreck.followTick)*.001f:0.f;
        valid=seconds<=2.f&&Length(Sub(Position(world),Position(wreck.lastWorld)))<=15.f+600.f*seconds;
    }
    if(!valid){
        if(wreck.following)Log("HELMET63 wreck removed/unavailable; retaining final helmet position");
        wreck.following=false;return false;
    }
    camera=Mul(wreck.seat,world);if(!Rigid(camera))return false;
    if(!wreck.following)Log("HELMET63 following %s with full seat rotation",wreck.debrisSelected?"emitted hull":"vehicle hull");
    wreck.following=true;wreck.lastWorld=world;wreck.followTick=now;return true;
}
bool InstallWreckView(){
    const BYTE weak[]={0x55,0x8B,0xEC,0x56,0x8B,0xF1,0x83,0x7E,0x7C,0};
    const BYTE release[]={0x55,0x8B,0xEC,0x5D,0xFF,0x25};
    const BYTE particle[]={0x55,0x8B,0xEC,0x81,0xEC,0x4C,1,0,0};
    if(!vehicleProfile||!Match(game+0x320890,weak,sizeof(weak))||!Match(game+0x2D10,release,sizeof(release))||
       !Match(game+0x249CA0,particle,sizeof(particle))||Value<void*>(game,0x5016C8+20*4)!=game+0x24A250||
       Value<void*>(game,0x5016C8+10*4)!=game+0x25DDD0||Value<void*>(game,0x5012E0+32*4)!=game+0x31DB50)return false;
    void** slot=reinterpret_cast<void**>(game+0x5016C8+20*4);DWORD protection=0;
    if(!VirtualProtect(slot,sizeof(void*),PAGE_READWRITE,&protection))return false;
    originalWreckFactory=reinterpret_cast<WreckFactory>(*slot);*slot=reinterpret_cast<void*>(&WreckFactoryHook);
    DWORD ignored=0;VirtualProtect(slot,sizeof(void*),protection,&ignored);return true;
}
