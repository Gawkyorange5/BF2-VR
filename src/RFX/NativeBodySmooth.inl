// Filter only the private visual body's native animation, before anatomical
// alignment and controller IK. No filtering of eyes, hands, gun or gameplay.
struct VisualBodyFilter {
    void* owner=nullptr;void* rig=nullptr;void* definition=nullptr;void* weapon=nullptr;
    std::uint64_t frame=0,origin=0,recenter=0;std::int64_t time=0;int stance=-1;
    BodyArmSmoothing arms{};float akElbowWeight=0,headYaw=0;
    Mat reference{},world{},local[80]{},output[80]{};
    Vec nativePosition{};bool valid=false;
} visualBodyFilter;

bool SmoothVisualBody(void* owner,void* rig,void* definition){
    auto& state=visualBodyFilter;
    const auto now=pairFrame.time;
    const int stance=Value<int>(owner,0x230);
    const Vec nativePosition=Position(bodyWorld);
    const bool reset=!state.valid||state.owner!=owner||state.rig!=rig||state.definition!=definition||
        state.stance!=stance||state.origin!=pairFrame.origin||pairFrame.id<state.frame||pairFrame.id>state.frame+2||
        now<=0||now<state.time||now-state.time>150000000||Length(Sub(nativePosition,state.nativePosition))>.75f||
        state.recenter!=referenceGeneration;
    if(!reset&&state.frame==pairFrame.id){
        bodyWorld=state.world;memcpy(bodyBones,state.output,sizeof(bodyBones));return true;
    }
    const float dt=reset?0.f:std::min(.05f,static_cast<float>((now-state.time)*1.e-9));
    if(reset){state.arms={};state.akElbowWeight=0;state.headYaw=0;}
    state.recenter=referenceGeneration;
    state.arms.seconds=dt;
    const float poseAlpha=reset?1.f:1.f-std::exp(-dt/.045f);
    const float legAlpha=reset?1.f:1.f-std::exp(-dt/.025f);
    const float turnAlpha=reset?1.f:1.f-std::exp(-dt/.050f);
    Mat candidate[80],local[80];
    BYTE* meta=Value<BYTE*>(rig,8);
    // Work in parent space so interpolation does not shrink a bent spine or
    // pull shoulders off it. A shorter lower-body filter retains walking response.
    // Head alignment and exact tracked wrist endpoints are applied afterward.
    for(int i=0;i<80;++i){
        const short parent=Value<short>(meta,i*0x24+6);
        if(parent<-1||parent>=i){state.valid=false;return false;}
        const Mat nativeLocal=parent<0?bodyBones[i]:Mul(bodyBones[i],Inverse(bodyBones[parent]));
        local[i]=i<=74&&!reset?BlendRigidPose(state.local[i],nativeLocal,i<11?legAlpha:poseAlpha):nativeLocal;
        candidate[i]=parent<0?local[i]:Mul(local[i],candidate[parent]);
        if(!Rigid(candidate[i])){state.valid=false;return false;}
    }
    Mat world=reset?bodyWorld:BlendRigidPose(state.world,bodyWorld,turnAlpha);
    // Follow locomotion immediately; only the visual body's turn is softened.
    SetPosition(world,nativePosition);
    if(!Rigid(world)){state.valid=false;return false;}
    state.owner=owner;state.rig=rig;state.definition=definition;state.stance=stance;
    state.frame=pairFrame.id;state.origin=pairFrame.origin;state.time=now;
    state.reference=reference;state.world=world;state.nativePosition=nativePosition;
    memcpy(state.local,local,sizeof(local));memcpy(state.output,candidate,sizeof(candidate));state.valid=true;
    bodyWorld=world;memcpy(bodyBones,candidate,sizeof(bodyBones));
    static bool reported=false;
    if(!reported){reported=true;Log("FULLBODY36 smoothing active: XR clock; torso=45ms legs=25ms turn=50ms elbow=35ms; bounded per-helper twist; exact wrist endpoints");}
    return true;
}
