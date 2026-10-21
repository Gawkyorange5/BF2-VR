// Optional local-soldier glide. Uses the same physics interface as BF2's
// soldier input update: +44 physics, vslot +5C velocity, +64 set velocity.
// Geometry/collision, gravity, falling history and damage stay native.
bool WorldSegment(const Vec&,const Vec&,void*,Vec&);
bool TerrainHeight(const Vec&,float&);
bool wingProfile=false;
struct WingGesture {std::uint64_t origin=0;ULONGLONG since=0,tick=0;bool out=false;} wingGesture;
struct WingFlight {void* owner=nullptr;void* identity=nullptr;bool active=false;ULONGLONG tick=0;} wingFlight;
bool UpdateWingGesture(const Runtime::Frame& f,bool enabled){
    auto& s=wingGesture;
    if(!enabled||!f.valid||!f.focused||!f.hands[0].gripValid||!f.hands[1].gripValid){s={};return false;}
    if(s.origin!=f.origin||f.tick<s.tick||f.tick-s.tick>150){s={};s.origin=f.origin;}
    s.tick=f.tick;
    const Mat head=Inverse(Level(f.head));
    const Vec left=Point(Position(f.hands[0].grip),head),right=Point(Position(f.hands[1].grip),head);
    const float spread=s.out?.30f:.40f;
    const bool arms=left.x< -spread&&right.x>spread&&left.y>-.65f&&right.y>-.65f&&
        left.y<.25f&&right.y<.25f&&std::abs(left.z)<.65f&&std::abs(right.z)<.65f;
    if(!arms){s.out=false;s.since=0;return false;}
    if(!s.since)s.since=f.tick;
    if(f.tick-s.since>=180)s.out=true;
    return s.out;
}
bool WingPhysics(void* owner,void*& physics,void*& setter,Vec& velocity){
    physics=Value<void*>(owner,0x44);void* table=Value<void*>(physics);
    void* getter=Value<void*>(table,0x5C);setter=Value<void*>(table,0x64);
    if(!physics||!Code(getter,(HMODULE)game)||!Code(setter,(HMODULE)game))return false;
    __try{
        const Vec* v=reinterpret_cast<const Vec*(__thiscall*)(void*)>(getter)(physics);
        return Copy(&velocity,v,sizeof(velocity))&&std::isfinite(velocity.x)&&std::isfinite(velocity.y)&&
            std::isfinite(velocity.z)&&Length(velocity)<150.f;
    }__except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
bool WingSetVelocity(void* physics,void* setter,const Vec& velocity){
    __try{reinterpret_cast<void(__thiscall*)(void*,const Vec*)>(setter)(physics,&velocity);return true;}
    __except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
void ApplyWingsuit(void* owner,void* identity,const FootCommand& command,float seconds){
    auto& flight=wingFlight;bool active=false;
    do{
        void* liveOwner=nullptr;void* liveIdentity=nullptr;
        if(!wingProfile||!command.wingPose||!FootOwner(liveOwner,liveIdentity)||liveOwner!=owner||liveIdentity!=identity)break;
        // Native +1AEA5E/+1AEC0D separates grounded/water movement from air.
        // Never change those contact bytes to force flight or reset a fall.
        void* node=Value<void*>(owner,0x48);BYTE ground=1,water=1;
        if(!Read(node,0xFA,ground)||!Read(node,0xFB,water)||ground||water)break;
        Mat world{};Vec velocity{};void* physics=nullptr;void* setter=nullptr;
        if(!Read(owner,0x88,world)||!Rigid(world)||!WingPhysics(owner,physics,setter,velocity)||velocity.y> -2.f)break;
        Vec from=Position(world),below=from,hit{};from.y+=.15f;below.y-=2.f;
        float terrain=0;
        // No ordinary jump/ground skating. Stop lift before surfaces/rooftops;
        // momentum remains, and BF2 handles the eventual impact and damage.
        if(!TerrainHeight(from,terrain)||from.y-terrain<2.2f||WorldSegment(from,below,owner,hit))break;
        const float yaw=(Value<float>(owner,0x224)+Value<float>(owner,0x240))*.01745329252f;
        if(!std::isfinite(yaw))break;
        Vec direction=Direction(Row(command.head,2),Yaw(yaw-Heading(reference)));direction.y=0;
        if(Length(direction)<.2f)break;direction=Unit(direction);
        const float dive=std::max(0.f,std::min(.7f,-command.head.m[2][1]));
        const Vec desired=Scale(direction,24.f+12.f*dive);
        Vec change=Sub(desired,{velocity.x,0,velocity.z});
        const float amount=Length(change),maximum=18.f*seconds;
        if(amount>maximum)change=Scale(change,maximum/amount);
        velocity.x+=change.x;velocity.z+=change.z;
        // Glide always descends rapidly: no hover, upward boost or soft landing.
        // Wings reduce a fast fall toward 12 m/s (steeper while looking down).
        const float descent=12.f+14.f*dive;
        if(velocity.y< -descent)velocity.y=std::min(-descent,velocity.y+22.f*seconds);
        active=WingSetVelocity(physics,setter,velocity);
    }while(false);
    if(flight.owner!=owner||flight.identity!=identity){flight={};flight.owner=owner;flight.identity=identity;}
    if(active!=flight.active){
        Log("WING94 %s; native collision/fall damage retained; A requests parachute",active?"gliding":"released");
        if(active){Runtime::Haptic(0,.18f,35);Runtime::Haptic(1,.18f,35);}
    }
    flight.active=active;flight.tick=GetTickCount64();
}
bool InstallWingProfile(){
    const BYTE get[]={0x8B,0x17,0x8B,0xCF,0xFF,0x52,0x5C};
    const BYTE set[]={0x8B,0x17,0x8D,0x45,0xA0,0x50,0x8B,0xCF,0xFF,0x52,0x64};
    const BYTE air[]={0x8B,0x55,0xF8,0x80,0xBA,0xFA,0,0,0,0,0x75,9};
    const bool ok=Match(game+0x1AE9EB,get,sizeof(get))&&Match(game+0x1AEBD0,set,sizeof(set))&&Match(game+0x1AEC0A,air,sizeof(air));
    Log("WING94 native soldier physics profile=%d; optional glide defaults off",ok?1:0);return ok;
}
