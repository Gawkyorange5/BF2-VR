// BF2 1.50 separates ragdoll simulation (+3EB1D0) from bone reconstruction
// (+3EA390). A first-person corpse can keep its last animated skeleton because
// its normal 3P preparation is skipped. Reconstruct only a PRIVATE snapshot:
// no physics step, native rig write, living pose helper or player IK.
struct CorpsePoseSnapshot {
    void* owner=nullptr;void* rig=nullptr;void* ragdoll=nullptr;
    std::uint64_t frame=0;bool attempted=false,valid=false;
    Mat bones[80]{},world{};
} corpsePose;
bool ReconstructCorpseSnapshot(void* owner,void* rig,void* ragdoll,Mat (&bones)[80],Mat& world,int& stage){
    // POD buffers deliberately keep exception handling separate from objects
    // that need stack unwinding. The native routine only reads the profile,
    // particles and local matrices, then writes rig+10 absolute matrices.
    __try {
        alignas(16) BYTE clone[0x64]{},rigClone[0x18]{},particles[128*0x58]{},locals[80*0x48]{};
        stage=1;
        static const BYTE signature[]={0x55,0x8B,0xEC,0x81,0xEC,0x4C,0x01,0x00,0x00,0x53,0x56,0x8B,0xF1,0x8B,0x46,0x14};
        if(!Match(game+0x3EA390,signature,sizeof(signature))||!BodyTopology(rig)||
           Value<void*>(ragdoll,0x18)!=rig||!Copy(clone,ragdoll,sizeof(clone))||!Copy(rigClone,rig,sizeof(rigClone)))return false;
        stage=2;BYTE* profile=Value<BYTE*>(clone,0x14);
        const auto first=Value<std::uintptr_t>(clone,8),last=Value<std::uintptr_t>(clone,0xC);
        if(!profile||!first||last<=first||(last-first)%0x58||(last-first)>sizeof(particles))return false;
        const int count=int((last-first)/0x58),root=Value<int>(profile,0xEC);
        if(root<0||root>=count)return false;
        // Reconstruction references this complete node map, including the
        // pelvis/shoulder orientation nodes. Reject unsupported definitions.
        stage=3;
        for(int i=0;i<128;++i){int n=0;if(!Read(profile,0xD4+i*4,n)||n<-1||n>=count)return false;}
        if(Value<int>(profile,0xD8)<0||Value<int>(profile,0x150)<0)return false;
        stage=4;const auto links=Value<std::uintptr_t>(profile,0xC),end=Value<std::uintptr_t>(profile,0x10);
        if(!links||end<=links||(end-links)%12||end-links>80*12)return false;
        for(auto at=links;at<end;at+=12){
            const int bone=Value<int>(reinterpret_cast<void*>(at));
            const int a=Value<int>(reinterpret_cast<void*>(at),4),b=Value<int>(reinterpret_cast<void*>(at),8);
            if(bone<0||bone>=80||a<-79||a>=80||b<-79||b>=80)return false;
        }
        stage=5;
        if(!Copy(particles,reinterpret_cast<void*>(first),last-first)||
           !Copy(locals,Value<void*>(rig,4),sizeof(locals))||!Copy(bones,Value<void*>(rig,0x10),sizeof(bones)))return false;
        stage=6;const Vec origin=Value<Vec>(particles,root*0x58);Mat nativeWorld{};
        if(!Read(owner,0x88,nativeWorld)||!Rigid(nativeWorld)||!std::isfinite(Length(origin))||
           Length(Sub(origin,Position(nativeWorld)))>3.f)return false;
        for(int i=0;i<count;++i){const Vec p=Value<Vec>(particles,i*0x58);if(!std::isfinite(Length(p))||Length(Sub(p,origin))>4.f)return false;}
        *reinterpret_cast<void**>(clone+8)=particles;*reinterpret_cast<void**>(clone+0xC)=particles+(last-first);
        *reinterpret_cast<void**>(clone+0x10)=particles+(last-first);*reinterpret_cast<void**>(clone+0x18)=rigClone;
        *reinterpret_cast<void**>(rigClone+4)=locals;*reinterpret_cast<void**>(rigClone+0x10)=bones;
        using BuildPose=void(__thiscall*)(void*);
        stage=7;reinterpret_cast<BuildPose>(game+0x3EA390)(clone);
        // BF2 reconstructs in world-oriented axes relative to the root
        // particle, exactly as the native soldier update sets its world.
        world=Identity();SetPosition(world,origin);
        stage=8;for(const auto& bone:bones)if(!Rigid(bone)||Length(Position(bone))>4.f)return false;
        stage=9;return true;
    }__except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
bool PrepareCorpsePose(void* owner,void* rig,std::uint64_t frame){
    void* ragdoll=Value<void*>(owner,0x254);auto& s=corpsePose;
    if(LocalAlive()||!owner||!rig||!ragdoll||Value<void*>(owner)!=game+0x4E80F8||
       (Value<unsigned>(owner,4)&1)||Value<void*>(owner,0x290)!=rig){s={};return false;}
    if(s.attempted&&s.frame==frame&&s.owner==owner&&s.rig==rig&&s.ragdoll==ragdoll)return s.valid;
    const bool changed=s.owner!=owner||s.rig!=rig||s.ragdoll!=ragdoll;
    const bool wasValid=s.valid;s.owner=owner;s.rig=rig;s.ragdoll=ragdoll;s.frame=frame;s.attempted=true;
    int stage=0;s.valid=ReconstructCorpseSnapshot(owner,rig,ragdoll,s.bones,s.world,stage);
    if(changed||s.valid!=wasValid)Log("DEATH58 native full-corpse pose %s owner=%p rig=%p ragdoll=%p stage=%d (private bones, physics untouched)",s.valid?"ready":"unavailable; r57 fallback",owner,rig,ragdoll,stage);
    return s.valid;
}
