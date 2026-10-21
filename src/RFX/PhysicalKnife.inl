// Physical contact requests a normal local knife attack. The stock projectile
// decides damage, armour, friendly fire, attribution and server acceptance.
// No health writes, remote soldier calls, or synthetic instant kills.
struct KnifeContact {
    void* player=nullptr;void* identity=nullptr;void* body=nullptr;
    int bone=13;Vec local{},approach{},start{},localApproach{};
};
struct PhysicalKnifeState {
    void* owner=nullptr;void* identity=nullptr;void* weapon=nullptr;
    std::uint64_t origin=0;ULONGLONG tick=0,cooldown=0,pulse=0,expires=0;
    Vec tip{},edge{},localTip{},localEdge{};KnifeContact contact{};
    bool sampled=false,pending=false,created=false,thrown=false;
} knife;
SRWLOCK knifeLock=SRWLOCK_INIT;
bool IsKnife(void* weapon){char name[80]{};return ReloadName(weapon,name)&&!strcmp(name,"kni_knife");}
void ResetKnife(){AcquireSRWLockExclusive(&knifeLock);InterlockedExchangePointer(&knifeWeaponPublished,nullptr);knife={};ReleaseSRWLockExclusive(&knifeLock);}
bool KnifeOwned(const PhysicalKnifeState& s){
    return s.owner&&s.owner==Soldier()&&LocalAlive()&&!MenuActive()&&
        s.identity==Value<void*>(Player(),0xCC)&&Value<void*>(s.weapon,0x28)==s.owner&&
        InventoryItem(s.owner,Value<int>(s.owner,0x1E0))==s.weapon&&!Value<void*>(s.owner,0x254)&&
        Value<int>(s.owner,0x2B8)==0;
}
float KnifeClamp(float x){return std::max(0.f,std::min(1.f,x));}
// Closest points on two finite segments, including degenerate/parallel cases.
float KnifeSegments(Vec a,Vec b,Vec c,Vec d,Vec& p,Vec& q){
    const Vec u=Sub(b,a),v=Sub(d,c),w=Sub(a,c);
    const float aa=Dot(u,u),ee=Dot(v,v),ff=Dot(v,w);float s=0,t=0;
    if(aa<1.e-8f)t=ee>1.e-8f?KnifeClamp(ff/ee):0;
    else{const float cc=Dot(u,w);
        if(ee<1.e-8f)s=KnifeClamp(-cc/aa);
        else{const float bb=Dot(u,v),den=aa*ee-bb*bb;
            if(den>1.e-8f)s=KnifeClamp((bb*ff-cc*ee)/den);
            t=(bb*s+ff)/ee;
            if(t<0){t=0;s=KnifeClamp(-cc/aa);}else if(t>1){t=1;s=KnifeClamp((bb-cc)/aa);}
        }
    }
    p=Add(a,Scale(u,s));q=Add(c,Scale(v,t));return Length(Sub(p,q));
}
bool KnifeClearPath(void* owner,void* target,Vec from,Vec to){
    Vec hit{};void* object=nullptr;
    if(!WorldSegmentObject(from,to,owner,hit,object))return inventoryWorldProfile;
    // The soldier itself is a valid first hit. A world surface or a different
    // soldier in front of the candidate blocks the contact request.
    return object==target||(object&&Value<void*>(object,0x28)==target)||Length(Sub(hit,to))<.01f*Config().units;
}
bool KnifeFindContact(void* owner,Vec from,Vec to,KnifeContact& out){
    void* manager=Class("PlayerManager");void* table=Value<void*>(manager);
    if(table!=game+0x4AA5A8||Value<void*>(table,0x30)!=game+0x24E850||Value<void*>(table,0x40)!=game+0x25DDD0)return false;
    void* sentinel=Value<void*>(manager,0x10);const unsigned count=Value<unsigned>(manager,0x14);
    if(!sentinel||!count||count>256)return false;
    void* node=Value<void*>(sentinel);float nearest=1000.f;bool found=false;
    const int joints[]={1,2,3,6,7,8,11,12,13,15,17,20,31,33,35,45,46,47};
    for(unsigned i=0;i<count&&node&&node!=sentinel;++i){
        void* p=Value<void*>(node,8);void* weak=Value<void*>(p,0xCC);void* body=Value<void*>(weak,4);
        Mat world{};
        if(p!=Player()&&Value<void*>(p)==Value<void*>(Player())&&!Value<BYTE>(p,0xE8)&&
           body!=owner&&Value<void*>(body)==game+0x4E80F8&&!(Value<unsigned>(body,4)&1)&&
           Read(body,0x88,world)&&Rigid(world)&&Length(Sub(Position(world),ClosestBodySegment(Position(world),from,to)))<2.5f*Config().units){
            void* rig=Value<void*>(body,0x290);Mat bones[80];BYTE* meta=Value<BYTE*>(rig,8);
            if(BodyTopology(rig)&&Copy(bones,Value<void*>(rig,0x10),sizeof(bones))){
                for(int bone:joints){
                    const int parent=Value<short>(meta,bone*0x24+6);
                    if(parent<0||parent>=80||!Rigid(bones[bone])||!Rigid(bones[parent]))continue;
                    const Vec a=Point(Position(bones[parent]),world),b=Point(Position(bones[bone]),world);
                    if(Length(Sub(a,Position(world)))>3.f||Length(Sub(b,a))>1.2f)continue;
                    const float radius=(bone>=11&&bone<=13?.20f:bone>=45?.14f:.105f)*Config().units;
                    Vec path{},centre{};const float distance=KnifeSegments(from,to,a,b,path,centre);
                    if(distance>radius+.015f*Config().units)continue;
                    const float along=Length(Sub(path,from));if(along>=nearest)continue;
                    // Keep the whole movement segment unobstructed. A body
                    // capsule is only a candidate; never reach through a wall.
                    if(!KnifeClearPath(owner,body,from,path))continue;
                    Vec approach=Sub(centre,path);if(Length(approach)<.005f)approach=Sub(to,from);
                    if(Length(approach)<.005f)continue;
                    approach=Unit(approach);
                    const Vec start=Sub(centre,Scale(approach,radius+.045f*Config().units));
                    if(!KnifeClearPath(owner,body,path,start))continue;
                    out.player=p;out.identity=weak;out.body=body;out.bone=bone;
                    const Mat inverseBone=Inverse(Mul(bones[bone],world));
                    out.start=start;out.local=Point(start,inverseBone);out.approach=approach;
                    out.localApproach=Direction(approach,inverseBone);
                    nearest=along;found=true;
                }
            }
        }
        void* next=Value<void*>(node);if(next==node)break;node=next;
    }
    return found;
}
void KnifeRequest(PhysicalKnifeState& s,const KnifeContact& contact,ULONGLONG now,bool thrown){
    s.contact=contact;s.pending=true;s.created=false;s.thrown=thrown;
    // Held contacts need a pulse longer than the stock 130ms launch delay.
    // Stop at the first launch callback; 300ms remains below the 500ms fire
    // period. The working thrown-knife input timing is retained.
    s.pulse=now+(thrown?120:300);s.expires=now+600;s.cooldown=now+550;
    InterlockedExchangePointer(&knifeWeaponPublished,s.weapon);
    Runtime::Haptic(1,.4f,45);Log("KNIFE57 %s contact; requesting native knife attack body=%p",thrown?"throw":"stab",contact.body);
}
bool KnifeThrowContact(void* owner,void* identity,void* weapon,Vec from,Vec to,Vec velocity,ULONGLONG now){
    if(!IsKnife(weapon)||Length(velocity)<1.1f*Config().units)return false;
    AcquireSRWLockExclusive(&knifeLock);auto& s=knife;bool hit=false;
    // BF2's input channel belongs to its currently selected weapon. Never
    // redirect another gun's shot when switching equipment during flight.
    if(s.owner==owner&&s.identity==identity&&s.weapon==weapon&&KnifeOwned(s)&&now>=s.cooldown){
        KnifeContact contact{};
        if(KnifeFindContact(owner,from,to,contact)){KnifeRequest(s,contact,now,true);hit=true;}
    }
    ReleaseSRWLockExclusive(&knifeLock);return hit;
}
bool KnifeFirePolicy(void* owner,float& primary,float& secondary){
    AcquireSRWLockShared(&knifeLock);const auto& s=knife;const auto now=GetTickCount64();
    const bool owned=s.owner==owner&&s.pending&&now<s.expires&&KnifeOwned(s);
    if(owned){primary=!s.created&&now<s.pulse?1.f:0.f;secondary=0;}
    ReleaseSRWLockShared(&knifeLock);return owned;
}
bool UpdateKnife(const Runtime::Frame& f,bool enabled,bool& firing){
    firing=false;
    if(!enabled||!f.valid||!f.focused||MenuActive()||!LocalAlive()||!f.hands[1].gripValid){ResetKnife();return false;}
    void* owner=Soldier();void* weapon=InventoryItem(owner,Value<int>(owner,0x1E0));
    if(!IsKnife(weapon)){ResetKnife();return false;}
    AcquireSRWLockExclusive(&knifeLock);auto& s=knife;
    if(s.owner!=owner||s.weapon!=weapon||s.identity!=Value<void*>(Player(),0xCC)||s.origin!=f.origin){
        InterlockedExchangePointer(&knifeWeaponPublished,nullptr);s={};s.owner=owner;s.weapon=weapon;s.identity=Value<void*>(Player(),0xCC);s.origin=f.origin;
    }
    if(s.pending&&f.tick>=s.expires){
        static LONG reports=0;
        if(!s.created&&InterlockedIncrement(&reports)<=24)Log("KNIFE91 %s request expired without a native launch callback",s.thrown?"throw":"stab");
        s.pending=false;InterlockedExchangePointer(&knifeWeaponPublished,nullptr);
    }
    if(InventoryStowed(owner)){s.sampled=false;s.tick=f.tick;
        const bool active=s.pending;firing=active&&!s.created&&f.tick<s.pulse;ReleaseSRWLockExclusive(&knifeLock);return active;}
    FireFrame pose{};AcquireSRWLockShared(&fireLock);pose=fire;ReleaseSRWLockShared(&fireLock);
    const auto now=GetTickCount64();Mat body{};
    if(pose.owner!=owner||pose.weapon!=weapon||pose.origin!=f.origin||f.id<pose.frame||now<pose.tick||now-pose.tick>150||
       pose.slot!=Value<int>(owner,0x1E0)||pose.rig!=Value<void*>(owner,0x288)||!referenceValid||
       !pose.palmsValid||!pose.gripBinding.valid||!Rigid(pose.gripBinding.right)||!Rigid(pose.rightPalm)||
       !Rigid(pose.native[0])||!Read(owner,0x88,body)||!Rigid(body)||!Rigid(f.hands[1].grip)){
        s.sampled=false;s.tick=f.tick;ReleaseSRWLockExclusive(&knifeLock);return false;
    }
    // The free-hand knife uses the same fixed palm attachment as SolveHands.
    // Compose it from this input frame instead of testing the older render
    // pose. No second OpenXR locate or full 70-bone IK solve is necessary.
    const Mat attachment=Mul(Inverse(pose.gripBinding.right),pose.rightPalm);
    const Mat base=FootPresentationBase(Mul(pose.native[0],body),f);
    const Mat tracked=Mul(attachment,Compose(base,reference,f.hands[1].grip,Config().units));
    Mat grip=f.hands[1].grip;SetPosition(grip,Scale(Position(grip),Config().units));
    const Mat local=Mul(attachment,grip);
    // Actual knife mesh runs from z=-.127 (handle) to +.186 (tip).
    const Vec tip=Point({0,0,.186f},tracked),edge=Point({0,0,.07f},tracked);
    const Vec localTip=Point({0,0,.186f},local),localEdge=Point({0,0,.07f},local);
    const auto elapsed=f.tick-s.tick;
    if(s.sampled&&elapsed>=25&&elapsed<=100&&f.tick>=s.cooldown&&(!s.pending||s.created)){
        // Include wrist rotation while excluding artificial stick turning and
        // locomotion from the physical speed requirement.
        const float speed=std::max(Length(Sub(localTip,s.localTip)),Length(Sub(localEdge,s.localEdge)))*1000.f/(float(elapsed)*Config().units);
        const float travel=Length(Sub(tip,s.tip));
        if(speed>.55f&&speed<10.f&&travel<.65f*Config().units){
            KnifeContact contact{};
            if(KnifeFindContact(owner,s.tip,tip,contact)||KnifeFindContact(owner,s.edge,edge,contact)||KnifeFindContact(owner,edge,tip,contact))
                KnifeRequest(s,contact,f.tick,false);
        }
    }
    // Retain the previous scan endpoint until the next scan. Updating it on
    // skipped VR frames used to discard most of a fast swing's swept path.
    if(!s.sampled||f.tick<s.tick||elapsed>=25){
        s.tip=tip;s.edge=edge;s.localTip=localTip;s.localEdge=localEdge;s.tick=f.tick;s.sampled=true;
    }
    const bool active=s.pending;firing=active&&!s.created&&f.tick<s.pulse;
    ReleaseSRWLockExclusive(&knifeLock);return active;
}
// 0 = ordinary trigger attack, 1 = physical attack, -1 = cancelled physical
// attack. A cancelled request must not fall back to a shot at the crosshair.
int KnifeNativeLaunch(void* weapon,Mat& launch,Vec& velocity){
    AcquireSRWLockShared(&knifeLock);const auto s=knife;ReleaseSRWLockShared(&knifeLock);
    if(s.weapon!=weapon||!s.pending)return 0;
    const auto now=GetTickCount64();const auto& c=s.contact;
    auto reject=[&](const char* reason){
        static LONG reports=0;
        if(InterlockedIncrement(&reports)<=32)Log("KNIFE91 %s launch cancelled: %s",s.thrown?"throw":"stab",reason);
        return -1;
    };
    if(s.created)return -1;
    if(now>=s.expires)return reject("contact expired");
    if(!KnifeOwned(s))return reject("local ownership, life, seat or menu changed");
    if(Value<void*>(c.player,0xCC)!=c.identity||Value<void*>(c.identity,4)!=c.body||
       Value<BYTE>(c.player,0xE8)||Value<void*>(c.body)!=game+0x4E80F8)return reject("target identity or life changed");
    void* rig=Value<void*>(c.body,0x290);Mat bone{},world{};
    if(!BodyTopology(rig)||!Read(Value<void*>(rig,0x10),c.bone*sizeof(Mat),bone)||!Rigid(bone)||!Read(c.body,0x88,world)||!Rigid(world))return reject("target pose unavailable");
    const Mat boneWorld=Mul(bone,world);const Vec start=Point(c.local,boneWorld);
    if(Length(Sub(start,c.start))>.45f*Config().units)return reject("contact moved over 45cm during native windup");
    if(!KnifeClearPath(s.owner,c.body,c.start,start))return reject("contact path became obstructed");
    // Start and direction must follow the same bone through the launch delay.
    // Otherwise an animated limb rotates the start but leaves the stab aimed
    // in the old world direction. Keep the accepted throw path unchanged.
    const Vec approach=s.thrown?c.approach:Unit(Direction(c.localApproach,boneWorld));
    launch=Facing(Identity(),approach);SetPosition(launch,start);
    // Short stock lifetime, short contact ray: avoid hitting a second player
    // metres behind a missed candidate. BF2 keeps the original damage values.
    velocity=Scale(approach,12.f*Config().units);return Rigid(launch)?1:reject("invalid contact transform");
}
void KnifeNativeCreated(void* weapon,void* object){
    AcquireSRWLockExclusive(&knifeLock);
    if(knife.weapon==weapon&&knife.pending){knife.created=true;knife.pulse=0;
        Log("KNIFE91 %s native callback completed projectile=%p; %s",knife.thrown?"throw":"stab",object,
            object?"damage and hit result remain native":"no projectile; see preceding cancellation reason, otherwise native creation returned null");}
    ReleaseSRWLockExclusive(&knifeLock);
}
