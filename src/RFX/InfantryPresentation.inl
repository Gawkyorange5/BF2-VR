// Presentation-cadence turn policy informed by BF2142VR's MIT-licensed
// InfantryPresentationTurn/D3D8TrackingAnchor. BF2 adapter and ownership here
// are independent. No camera/hand tracking samples are filtered or re-located.
struct FootPresentation {
    void* owner=nullptr;void* identity=nullptr;
    std::uint64_t origin=0,generation=0,frame=0;
    XrTime time=0;ULONGLONG tick=0,mouseUntil=0;
    float yaw=0;bool enabled=false,valid=false;
} footPresentation;

// Caller owns footLock. Integrate once per published XR request, not once per
// simulation update or once per eye. yaw is world-from-tracking heading; it
// excludes the recenter/reference rotation that native HMD movement adjusts.
void AdvanceFootPresentation(const Runtime::Frame& f,void* owner,void* identity,bool enabled,float rate){
    auto& p=footPresentation;
    if(!enabled||!f.shouldRender||!f.focused||f.time<=0){p={};return;}
    if(!p.enabled||p.owner!=owner||p.identity!=identity||p.origin!=f.origin||p.generation!=referenceGeneration){
        p={};p.owner=owner;p.identity=identity;p.origin=f.origin;p.generation=referenceGeneration;
        p.enabled=true;p.frame=f.id;p.time=f.time;p.tick=f.tick;return;
    }
    if(f.id==p.frame)return;
    const double seconds=f.time>p.time?static_cast<double>(f.time-p.time)*1.e-9:0;
    const bool forward=f.id>p.frame;p.frame=f.id;p.time=f.time;p.tick=f.tick;
    if(!p.valid||!forward||seconds<=0||seconds>.1||!std::isfinite(seconds)||!std::isfinite(rate))return;
    // Existing legacy speed scale: base mouse scale .03 * look factor 5.
    // The VR speed slider keeps its calibrated value (540 => 81 degrees/s).
    // Do not carry elapsed time across a pause, or add a native second turn.
    const float delta=std::max(-1500.f,std::min(1500.f,rate))*.15f*.01745329252f*static_cast<float>(seconds);
    p.yaw=Wrapped(p.yaw+delta);
    static unsigned reports=0;
    if(delta!=0&&reports<8){++reports;Log("TURN90 presentation frame=%llu dtMs=%.3f deltaDeg=%.4f; one shared heading for eyes, gun, crosshair, scopes and body",f.id,seconds*1000.,delta*57.295779513f);}
}

Mat FootPresentationBase(const Mat& source,const Runtime::Frame& f){
    Mat base=Level(source);
    if(!f.valid||!f.focused||!referenceValid||!Rigid(source))return base;
    AcquireSRWLockExclusive(&footLock);
    auto& p=footPresentation;
    const bool current=p.enabled&&p.owner==footCommand.soldier&&p.identity==footCommand.identity&&
        p.origin==f.origin&&p.generation==referenceGeneration&&f.id>=p.frame&&f.id-p.frame<=1&&
        f.tick>=p.tick&&f.tick-p.tick<=150&&f.tick>=p.mouseUntil;
    if(current){
        if(!p.valid){p.yaw=Wrapped(Heading(base)-Heading(reference));p.valid=true;}
        const Vec position=Position(base);base=Yaw(Wrapped(p.yaw+Heading(reference)));SetPosition(base,position);
    }
    ReleaseSRWLockExclusive(&footLock);
    return base;
}
