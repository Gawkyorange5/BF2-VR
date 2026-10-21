#include "VehicleDeath.inl"
// Keep the death camera on the same local soldier's head. Read the engine's
// reconstructed ragdoll snapshot; never advance physics or write native rigs.
struct HelmetViewState {
    void* player=nullptr;void* weak=nullptr;void* owner=nullptr;void* rig=nullptr;void* retainedBody=nullptr;
    Mat head{},headAxes{},camera{},tracking{};
    std::uint64_t frame=0,origin=0;ULONGLONG tick=0,floorTick=0;
    Vec floorPoint{};float floor=-10000;
    bool captured=false,active=false,vehicle=false,bodyAttached=false,headMapped=false;
} helmet;
void ResetHelmetView(){ResetWreckView();WreckRelease(helmet.retainedBody);helmet={};}
bool HelmetHead(void* owner,void* rig,Mat& head){
    if(!owner||Value<void*>(owner)!=game+0x4E80F8||(Value<unsigned>(owner,4)&1)||
       Value<void*>(owner,0x290)!=rig||!BodyTopology(rig))return false;
    Mat bone{},world{};Mat* bones=Value<Mat*>(rig,0x10);
    if(!Read(bones,47*sizeof(Mat),bone)||!Rigid(bone)||!Read(owner,0x88,world)||!Rigid(world))return false;
    head=Mul(bone,world);return Rigid(head)&&Length(Sub(Position(head),Position(world)))<3.f;
}
bool UpdateHelmetView(const Runtime::Frame& frame,const Mat& native,bool kitMenu=false){
    const auto tune=Tuning();
    if((!tune.helmetDeathView&&!tune.vehicleHelmetDeathView)||!frame.valid||!referenceValid||!Rigid(native)){ResetHelmetView();return false;}
    if(helmet.frame==frame.id)return helmet.active;
    const auto now=frame.tick;void* player=Player();void* weak=Value<void*>(player,0xCC);void* owner=Value<void*>(weak,4);
    if(LocalAlive()){
        helmet.active=false;
        // Kit-menu cameras/poses are not the last in-world helmet view. Keep
        // that mapping while the same soldier is alive in the menu, so Suicide
        // can use it even when BF2 clears its player-to-soldier link on death.
        if(kitMenu&&helmet.captured&&helmet.player==player&&helmet.owner==owner&&
           Value<void*>(helmet.retainedBody,4)==owner){
            helmet.frame=frame.id;helmet.tick=now;return false;
        }
        void* seat=Value<void*>(Value<void*>(player,0x80),4);void* rig=Value<void*>(owner,0x290);Mat head{};
        const bool inVehicle=owner&&seat&&owner!=seat;
        if(inVehicle?!tune.vehicleHelmetDeathView:!tune.helmetDeathView){ResetHelmetView();return false;}
        const bool headValid=HelmetHead(owner,rig,head);
        Mat base=Level(native),trackedHead=frame.head;VehicleSample occupied{};
        if(inVehicle){
            if(Value<void*>(owner)!=game+0x4E80F8||(Value<unsigned>(owner,4)&1)||
               !ReadVehicle(occupied)||occupied.owner!=owner||occupied.seat!=seat){ResetHelmetView();return false;}
            // Use the live hull transform and the same gunner/driver policy as
            // the living camera. Never level a rolling aircraft death pose.
            base=native;
            if(vehicleViewValid&&SameVehicle(occupied,vehicleSample)){
                const Mat attached=Mul(vehicleLocal,occupied.world);
                base=AimedSeat(occupied.role)?VehicleHeading(occupied.world):attached;
                SetPosition(base,AimedSeat(occupied.role)?Position(native):Position(attached));
            }
            if(AimedSeat(occupied.role)&&Tuning().gunnerPositionLock)SetPosition(trackedHead,Position(reference));
            if(!Rigid(base)){ResetHelmetView();return false;}
        }else if(owner!=seat||!headValid){ResetHelmetView();return false;}
        // Save the mapping from this model's head axes to the actual tracked
        // view while alive. Models need not share the same bone orientation.
        Mat actual=Compose(base,reference,trackedHead,Config().units);
        if(inVehicle)CaptureWreckSeat(occupied,actual,now);else if(wreck.root)ResetWreckView();
        if(helmet.owner!=owner||Value<void*>(helmet.retainedBody,4)!=owner){
            WreckRelease(helmet.retainedBody);helmet.retainedBody=WreckRetain(owner);
        }
        helmet.player=player;helmet.weak=weak;helmet.owner=owner;helmet.rig=rig;
        helmet.head=head;helmet.camera=actual;helmet.floorTick=0;helmet.floor=-10000;
        helmet.headAxes=headValid?Mul(actual,Inverse(head)):Identity();SetPosition(helmet.headAxes,{});
        helmet.vehicle=inVehicle;helmet.headMapped=headValid;helmet.bodyAttached=false;
        helmet.tracking=frame.head;helmet.origin=frame.origin;helmet.frame=frame.id;helmet.tick=now;helmet.captured=true;return false;
    }
    if((helmet.vehicle?!tune.vehicleHelmetDeathView:!tune.helmetDeathView)||
       !helmet.captured||player!=helmet.player||now<helmet.tick){ResetHelmetView();return false;}
    // The retained native weak handle survives removal of the player's link,
    // but never extends the corpse lifetime. A deleted corpse clears +4.
    void* retained=Value<void*>(helmet.retainedBody,4);
    const bool retainedValid=retained&&retained==helmet.owner&&Value<void*>(retained,0x7C)==helmet.retainedBody;
    const bool sameBody=(weak==helmet.weak&&owner==helmet.owner&&owner)||retainedValid;
    // Destroyed vehicles may delete their occupants outright. Do not follow a
    // stale soldier/seat pointer or another soldier selected for spectating.
    if(owner&&owner!=helmet.owner){ResetHelmetView();return false;}
    owner=sameBody?helmet.owner:nullptr;
    Mat attached{},head{};
    const bool seatValid=helmet.vehicle&&FollowWreckSeat(now,attached);
    bool bodyValid=false;
    if(sameBody&&!seatValid){
        if(PrepareCorpsePose(owner,helmet.rig,frame.id)){
            head=Mul(corpsePose.bones[47],corpsePose.world);bodyValid=Rigid(head);
        }else if(!helmet.vehicle)bodyValid=HelmetHead(owner,helmet.rig,head);
    }
    if(!helmet.active){
        if(now-helmet.tick>500){ResetHelmetView();return false;}
        helmet.active=true;
        if(!helmet.vehicle)Log("HELMET120 infantry death owner=%p retained=%d corpse=%d; helmet view retained",owner,retainedValid?1:0,bodyValid?1:0);
        if(helmet.vehicle)Log("HELMET63 vehicle death: %s; stereo head tracking retained",seatValid?"attached to crash seat":bodyValid?"local ragdoll":"waiting for wreck / retaining final helmet position");
    }
    if(seatValid){
        helmet.camera=attached;helmet.bodyAttached=false;helmet.floorTick=0;
        if(helmet.origin!=frame.origin){helmet.tracking=frame.head;helmet.origin=frame.origin;}
        helmet.frame=frame.id;helmet.tick=now;return true;
    }
    if(helmet.vehicle&&bodyValid!=helmet.bodyAttached)Log("HELMET63 vehicle corpse %s; owner=%p",bodyValid?"attached":"unavailable, retaining last helmet position",sameBody?owner:nullptr);
    helmet.bodyAttached=bodyValid;
    const Vec previous=Position(helmet.camera);
    if(bodyValid){
        if(!helmet.headMapped){helmet.headAxes=Mul(helmet.camera,Inverse(head));SetPosition(helmet.headAxes,{});helmet.headMapped=true;}
        helmet.camera=Mul(helmet.headAxes,head);
    }
    // Anatomical eye offset already used by the living full-body alignment.
    Vec position=bodyValid?Point({0,.07304f,.07993f},head):previous;
    // A ragdoll head bone can penetrate terrain, especially after an impact.
    // Correct the eye center only; preserve the native body's roll and tilt.
    // Sample around the stereo footprint so a rolled eye/near plane also fits.
    if(!helmet.floorTick||now-helmet.floorTick>=50||Length(Sub(position,helmet.floorPoint))>.04f){
        float floor=-10000,h=0;const Vec offsets[]={{0,0,0},{.12f,0,0},{-.12f,0,0},{0,0,.12f},{0,0,-.12f}};
        for(const auto& offset:offsets)if(TerrainHeight(Add(position,offset),h))floor=std::max(floor,h);
        Vec start=position,end=position,hit{};
        start.y=std::min(position.y+3.f,std::max(position.y+.55f,previous.y+.3f));end.y-=1.f;
        if(WorldSegment(start,end,bodyValid?owner:nullptr,hit))floor=std::max(floor,hit.y);
        helmet.floor=floor;helmet.floorPoint=position;helmet.floorTick=now;
    }
    if(helmet.floor>-9999&&position.y<helmet.floor+.18f){
        position.y=helmet.floor+.18f;
        static ULONGLONG report=0;if(now-report>3000){report=now;Log("HELMET51 ground clearance applied floor=%.3f eye=%.3f",helmet.floor,position.y);}
    }
    SetPosition(helmet.camera,position);
    if(helmet.origin!=frame.origin){helmet.tracking=frame.head;helmet.origin=frame.origin;}
    if(bodyValid)helmet.head=head;helmet.frame=frame.id;helmet.tick=now;return Rigid(helmet.camera);
}
void* HelmetBodyOwner(){return helmet.active&&helmet.bodyAttached?helmet.owner:nullptr;}
void* HelmetBodySource(){return Value<void*>(HelmetBodyOwner(),0x38);}
