// Right-hand shoulder radio. This is input routing only: BF2 owns the
// displayed wheel, hover, command, recipient, voice and cooldown behavior.
struct ShoulderRadio {
    bool active=false,armed=false,ready=false,confirming=false,blockUntilRelease=false;
    Mat axes{};Vec anchor{};
    float u=.5f,v=.5f;
    std::uint64_t origin=0,pressSerial=0;
    ULONGLONG started=0,confirmedAt=0,lastSample=0;
    std::uintptr_t seat=0;
} shoulderRadio;

void ClosePhysicalRadio(){
    if(shoulderRadio.active){
        Engine::PublishPointer(false,0,0,false,0);
        Key(Radio,false);Mouse(0,false);Mouse(1,false);
        shoulderRadio.active=false;shoulderRadio.ready=false;shoulderRadio.confirming=false;
        shoulderRadio.blockUntilRelease=true;
        shoulderRadio.armed=false;
        triggerArmed[0]=triggerArmed[1]=false;trigger[0]=trigger[1]=false;
        gameWas=false;lastTick=0;smoothTurnClock={};
    }
}
bool UpdatePhysicalRadio(const Runtime::Frame& f){
    auto& state=shoulderRadio;const auto& hand=f.hands[1];
    const auto now=GetTickCount64();
    const bool tracking=f.valid&&f.focused&&hand.gripValid&&hand.aimValid&&
        Rigid(f.head)&&Rigid(hand.grip)&&std::isfinite(hand.trigger)&&
        f.tick&&now>=f.tick&&now-f.tick<=150;
    const bool triggerDown=tracking&&hand.trigger>(state.active?.40f:.55f);
    if(!state.active){
        if(state.blockUntilRelease){
            Mouse(0,false);Mouse(1,false);Engine::PublishPointer(false,0,0,false,f.id);
            if(tracking&&hand.trigger<.40f)state.blockUntilRelease=false;
            return true;
        }
        if(!tracking||!Tuning().physicalRadio){state.armed=false;return false;}
        const bool rising=triggerDown&&state.armed;
        if(hand.trigger<.40f)state.armed=true;
        else if(triggerDown)state.armed=false;
        if(!rising||both||menuWas||kitUntil>f.tick||pulseUntil[Escape]>f.tick||
           Engine::MenuActive()||!Engine::LocalAlive()||Engine::GrenadePrimed())return false;
        const Mat axes=Level(f.head);const Vec local=Point(Position(hand.grip),Inverse(axes));
        // Broad left collarbone/chest area; the normal right-side gun grip
        // and a controller aimed beside the face do not start the gesture.
        if(local.x<-.44f||local.x>-.06f||local.y<-.50f||local.y>-.055f||local.z<-.20f||local.z>.32f)return false;
        ResetGameplay();bool ignored=false;Engine::KnifeControls(f,false,ignored);
        const auto vehicle=Engine::Vehicle(f);
        state={};state.active=true;state.axes=axes;state.origin=f.origin;
        state.anchor=Direction(Sub(Position(hand.grip),Position(f.head)),Inverse(axes));
        state.started=state.lastSample=now;state.seat=vehicle.seat;
        Key(Radio,true);Engine::PublishRadioPointer(.5f,.5f,false,f.id);
        Runtime::Haptic(1,.22f,35);
        Log("RADIO shoulder gesture opened; right trigger release selects native wheel");
        return true;
    }
    const bool safe=tracking&&f.origin==state.origin&&now>=state.lastSample&&now-state.lastSample<=200&&
        Tuning().physicalRadio&&Engine::LocalAlive()&&!Engine::FrontendActive()&&!both&&!menuWas&&
        kitUntil<=f.tick&&pulseUntil[Escape]<=f.tick&&Engine::Vehicle(f).seat==state.seat;
    if(!safe){ClosePhysicalRadio();return true;}
    state.lastSample=now;
    Mouse(0,false);Mouse(1,false);Engine::StopLocomotion();
    // Acknowledge delivery on the native HUD update, not a guessed delay.
    // Closing Q before that update would silently discard a quick release.
    if(state.confirming){
        if(Engine::RadioPointerConsumed(state.pressSerial)){
            Log("RADIO native choice delivered u=%.3f v=%.3f",state.u,state.v);
            Runtime::Haptic(1,.35f,40);ClosePhysicalRadio();
        }else if(now-state.confirmedAt>750||!Engine::RadioMenuActive()){
            Log("RADIO confirmation cancelled: native wheel closed or unavailable");ClosePhysicalRadio();
        }else Engine::PublishRadioPointer(state.u,state.v,true,state.pressSerial);
        return true;
    }
    const bool visible=Engine::RadioMenuActive();
    if(visible)state.ready=true;
    if((state.ready&&!visible)||now-state.started>30000){ClosePhysicalRadio();return true;}
    const Vec relative=Direction(Sub(Position(hand.grip),Position(f.head)),Inverse(state.axes));
    const Vec delta=Sub(relative,state.anchor);
    // Relative to the torso translation, with axes frozen at opening. Small
    // head rotations cannot drag the selector to another request.
    if(!std::isfinite(delta.x+delta.y+delta.z)||Length(delta)>.65f){ClosePhysicalRadio();return true;}
    float x=delta.x,y=delta.y;const float distance=std::hypot(x,y);
    // Scale the complete response, including the centre deadzone. Default
    // 3x reaches the same native wheel edge in 3.93 cm instead of 11.8 cm.
    // The activation zone and the release-to-select handshake are unchanged.
    const float gain=Tuning().radioSensitivity;
    const float deadzone=.018f/gain,travel=.10f/gain;
    if(distance<deadzone)x=y=0;
    else{
        const float amount=std::min(1.f,(distance-deadzone)/travel);
        x=x/distance*amount*180.f;y=y/distance*amount*180.f;
    }
    // BF2's native rose is authored around (400,300) on an 800x600
    // canvas. Use normalized coordinates so desktop/headset resolution
    // changes do not alter either reach distance or input sensitivity.
    state.u=.5f+x/800.f;state.v=.5f-y/600.f;
    if(!triggerDown){
        if(state.ready&&visible&&now-state.started>=120){
            state.confirming=true;state.confirmedAt=now;state.pressSerial=f.id;
            Engine::PublishRadioPointer(state.u,state.v,true,state.pressSerial);
        }else ClosePhysicalRadio();
        return true;
    }
    if(!state.ready&&now-state.started>1500){Log("RADIO native Q wheel unavailable; gesture cancelled");ClosePhysicalRadio();return true;}
    Key(Radio,true);Engine::PublishRadioPointer(state.u,state.v,false,f.id);
    return true;
}
