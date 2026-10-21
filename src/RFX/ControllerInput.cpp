#include "ControllerInput.h"
#include "VrSettings.h"
#include "Bf2Adapter.h"
#include "StingerAudio.h"
#include <d3dx9math.h>
#include <cstdio>
#include <cstring>
#include <algorithm>
#include <cmath>

namespace CleanVR { namespace Controls { namespace {
enum KeyId {Forward,Backward,Left,Right,Use,Reload,Jump,Parachute,Sprint,Crouch,Prone,FireMode,PickUp,Map,Score,Squad,Voice,Radio,Spawn,Grenade,Seat1,Seat2,Seat3,Seat4,Seat5,Seat6,Seat7,Seat8,Flares,Select1,Select2,Select3,Select4,Select5,Select6,Select7,Select8,Select9,Escape,KeyCount};
const WORD codes[]={ 'W','S','A','D','E','R',VK_SPACE,'9',VK_LSHIFT,VK_LCONTROL,'Z','F','G','M',VK_TAB,VK_CAPITAL,'B','Q',VK_RETURN,'4',VK_F1,VK_F2,VK_F3,VK_F4,VK_F5,VK_F6,VK_F7,VK_F8,'X','1','2','3','4','5','6','7','8','9',VK_ESCAPE };
const char* names[]={"forward","backward","left","right","use","reload","jump","parachute","sprint","crouch","prone","fire-mode","pickup-kit","map","scoreboard","squad","voice","radio","spawn-screen","grenade","seat-1","seat-2","seat-3","seat-4","seat-5","seat-6","seat-7","seat-8","flares","select-1","select-2","select-3","select-4","select-5","select-6","select-7","select-8","select-9","menu"};
static_assert(sizeof(codes)/sizeof(codes[0])==KeyCount,"Key table size");
static_assert(sizeof(names)/sizeof(names[0])==KeyCount,"Key name table size");
bool held[KeyCount]{},mouse[2]{},trigger[2]{},triggerArmed[2]{},use=false,useArmed=false;
ULONGLONG pulseUntil[KeyCount]{},bothSince=0,lastTick=0,seatRecenterBlockUntil=0;
ULONGLONG kitUntil=0,menuSince=0;bool menuHoldConsumed=false,settingsRequested=false;
bool blocked[2][4]{},havePrevious=false;
bool both=false,chordHoldConsumed=false,menuWas=false,modifierWas=false,modifierUsed=false,gameWas=false,verticalArmed=false,turnArmed=false;
bool cycleWas=false,cycleUsed=false,modifierGripArmed=false;ULONGLONG cycleSince=0;
bool pickupHeld=false,modifierVoiceArmed=false,modifierLeftTriggerArmed=false,modifierRightTriggerArmed=false;
bool crouchLatched=false,crouchWas=false,vehicleWeaponGrip=false,vehicleWeaponGripArmed=false;
bool escapePending=false;float mouseRemainder=0;
struct SmoothTurnClock {LONGLONG counter=0;std::uint64_t frame=0,origin=0;} smoothTurnClock;
float axisRemainderX=0,axisRemainderY=0;bool vehicleAxesArmed=false;
std::uintptr_t occupiedSeat=0;
bool utilityVehicle=false;
ULONGLONG flareSince=0;bool flareWas=false,flareDelivered=false,flareConsumed=false;
std::uint64_t globalFrame=0;unsigned logged=0;

void Key(int id,bool down){
    if(held[id]==down)return;INPUT e{};e.type=INPUT_KEYBOARD;
    e.ki.wScan=(WORD)MapVirtualKeyA(codes[id],MAPVK_VK_TO_VSC);e.ki.dwFlags=KEYEVENTF_SCANCODE|(down?0:KEYEVENTF_KEYUP);
    if(SendInput(1,&e,sizeof(e))){held[id]=down;if(down&&id>=Use&&logged<80){++logged;Log("CONTROL key=%s down",names[id]);}}
}
void Mouse(int id,bool down){
    if(mouse[id]==down)return;INPUT e{};e.type=INPUT_MOUSE;
    e.mi.dwFlags=id?(down?MOUSEEVENTF_RIGHTDOWN:MOUSEEVENTF_RIGHTUP):(down?MOUSEEVENTF_LEFTDOWN:MOUSEEVENTF_LEFTUP);
    const UINT sent=SendInput(1,&e,sizeof(e));const DWORD error=sent?0:GetLastError();if(sent)mouse[id]=down;
    static unsigned reports=0;if(gameWas&&reports<48){++reports;Log("CONTROL mouse=%s down=%d delivered=%u error=%lu",id?"aim":"fire",down?1:0,sent,error);}
}
void Pulse(int id,ULONGLONG now){pulseUntil[id]=now+120;if(id==Seat6)seatRecenterBlockUntil=now+500;}
bool VehicleFlares(bool down,bool inhibit,ULONGLONG now){
    // Brief chord grace prevents a slightly staggered both-stick shortcut
    // from spending a countermeasure. A short single click fires on release.
    if(down&&!flareWas){flareSince=now;flareDelivered=false;flareConsumed=inhibit;}
    if(inhibit){flareConsumed=true;pulseUntil[Flares]=0;}
    const bool fire=down&&!inhibit&&!flareConsumed&&now>=flareSince+80;
    if(fire)flareDelivered=true;
    if(!down&&flareWas&&!flareDelivered&&!flareConsumed&&!inhibit)Pulse(Flares,now);
    flareWas=down;return fire;
}
void Keys(const bool* wanted,ULONGLONG now){for(int i=0;i<KeyCount;++i)Key(i,wanted[i]||pulseUntil[i]>now||(i==Spawn&&kitUntil>now));}
bool Button(const Runtime::Frame& f,int h,int b){const auto& hand=f.hands[h];return b==0?hand.primary:b==1?hand.secondary:b==2?hand.stickClick:hand.menu;}
bool UsableButton(const Runtime::Frame& f,int h,int b){return Button(f,h,b)&&!blocked[h][b];}
void BlockHeld(const Runtime::Frame& f){for(int h=0;h<2;++h)for(int b=0;b<4;++b)blocked[h][b]=Button(f,h,b);}
void CloseSeatWheel();
void ResetGameplay(){
    CloseSeatWheel();
    StingerAudio::Update(0);
    Engine::CancelManualReload();
    Engine::ClearBodySlotControls();
    Engine::ClearDefibControls();Engine::ClearC4Controls();
    Engine::StopLocomotion();
    cycleWas=cycleUsed=modifierGripArmed=false;cycleSince=0;
    pickupHeld=modifierVoiceArmed=modifierLeftTriggerArmed=modifierRightTriggerArmed=false;
    crouchLatched=crouchWas=vehicleWeaponGrip=vehicleWeaponGripArmed=false;
    for(int i=0;i<Escape;++i){Key(i,i==Spawn&&kitUntil>GetTickCount64());pulseUntil[i]=0;}
    Mouse(0,false);Mouse(1,false);trigger[0]=trigger[1]=triggerArmed[0]=triggerArmed[1]=use=useArmed=false;
    lastTick=0;mouseRemainder=0;smoothTurnClock={};verticalArmed=turnArmed=false;modifierWas=modifierUsed=false;gameWas=false;
    axisRemainderX=axisRemainderY=0;vehicleAxesArmed=false;
    flareSince=0;flareWas=flareDelivered=flareConsumed=false;
}
bool Hysteresis(bool prior,float value,float on=.55f,float off=.40f){return std::isfinite(value)&&value>(prior?off:on);}
void Wheel(int direction){INPUT e{};e.type=INPUT_MOUSE;e.mi.dwFlags=MOUSEEVENTF_WHEEL;e.mi.mouseData=(DWORD)(direction*WHEEL_DELTA);SendInput(1,&e,sizeof(e));if(logged<80){++logged;Log("CONTROL inventory wheel=%d",direction);}}
float SmoothTurnSeconds(const Runtime::Frame& f){
    // GetTickCount64 (Frame::tick) is for button deadlines/stale-sample guards.
    // Its coarse ticks made a steady stick produce uneven mouse distances.
    // Measure actual input intervals with QPC, without changing turn speed,
    // deadzone, button timing or the runtime's predicted tracking timestamp.
    static const double inverseFrequency=[](){LARGE_INTEGER hz{};return QueryPerformanceFrequency(&hz)&&hz.QuadPart>0?1.0/static_cast<double>(hz.QuadPart):0.0;}();
    auto& clock=smoothTurnClock;
    if(clock.counter&&clock.frame==f.id&&clock.origin==f.origin)return 0;
    LARGE_INTEGER now{};
    if(!inverseFrequency||!QueryPerformanceCounter(&now)){clock={};mouseRemainder=0;return 0;}
    const auto previous=clock;clock={now.QuadPart,f.id,f.origin};
    if(!previous.counter||previous.origin!=f.origin||f.id<previous.frame||now.QuadPart<=previous.counter){mouseRemainder=0;return 0;}
    const double seconds=static_cast<double>(now.QuadPart-previous.counter)*inverseFrequency;
    // Drop suspended/stale time instead of replaying a catch-up turn. Keep
    // the existing 50ms maximum for valid, slower gameplay frames.
    if(!std::isfinite(seconds)||seconds>.15){mouseRemainder=0;return 0;}
    return static_cast<float>(std::min(seconds,.05));
}
void Turn(float x,float seconds,const TuneSettings& tune){
    if(!tune.smoothTurn){mouseRemainder=0;return;}const float magnitude=std::abs(x),deadzone=Config().turnDeadzone;
    if(magnitude<=deadzone){mouseRemainder=0;return;}
    const float rate=(std::min(magnitude,1.f)-deadzone)/(1-deadzone)*(x<0?-1.f:1.f);
    mouseRemainder+=rate*tune.turnSpeed*seconds;const LONG dx=(LONG)mouseRemainder;if(!dx)return;
    INPUT e{};e.type=INPUT_MOUSE;e.mi.dwFlags=MOUSEEVENTF_MOVE|MOUSEEVENTF_MOVE_NOCOALESCE;e.mi.dx=dx;
    if(SendInput(1,&e,sizeof(e)))mouseRemainder-=dx;else mouseRemainder=0;
    static bool reported=false;if(!reported){reported=true;Log("TURN89 native locomotion unavailable; using legacy QPC mouse fallback");}
}
void RadioCursor(float x,float y,float seconds){
    INPUT e{};e.type=INPUT_MOUSE;e.mi.dwFlags=MOUSEEVENTF_MOVE|MOUSEEVENTF_MOVE_NOCOALESCE;
    e.mi.dx=std::abs(x)>.25f?(LONG)(x*600.f*seconds):0;e.mi.dy=std::abs(y)>.25f?(LONG)(-y*600.f*seconds):0;
    if(e.mi.dx||e.mi.dy)SendInput(1,&e,sizeof(e));
}
float Axis(float value){
    if(!std::isfinite(value))return 0;const float d=Config().turnDeadzone,m=std::abs(value);
    return m<=d?0.f:(std::min(m,1.f)-d)/(1-d)*(value<0?-1.f:1.f);
}
void VehicleMouse(const Runtime::Frame& frame,const Engine::VehicleInput& vehicle,float x,float y,float seconds,const TuneSettings& tune){
    float vx=0,vy=0;
    if(vehicle.role==Engine::VehicleRole::Pilot){
        vx=Axis(x)*tune.flightSpeed;vy=Axis(y)*tune.flightSpeed*tune.flightPitchSign;
    }else if(vehicle.lookValid){
        Engine::Vehicle(frame,Axis(x)*tune.aimStickSpeed*.01745329252f*seconds,Axis(y)*tune.aimStickSpeed*.01745329252f*seconds);
        axisRemainderX=axisRemainderY=0;return;
    }else if(vehicle.aimValid){
        const auto combined=Engine::Vehicle(frame,Axis(x)*tune.aimStickSpeed*.01745329252f*seconds,Axis(y)*tune.aimStickSpeed*.01745329252f*seconds);
        if(!combined.aimValid){axisRemainderX=axisRemainderY=0;return;}
        const auto speed=[&](float error){return std::abs(error)<.003f?0.f:std::max(-tune.headMaxSpeed,std::min(tune.headMaxSpeed,error*tune.headGain));};
        if(vehicle.role==Engine::VehicleRole::Aimed)vx=speed(combined.yawError);
        vy=speed(combined.pitchError)*tune.headPitchSign;
    }else if(!tune.headAim&&vehicle.role!=Engine::VehicleRole::Vehicle){
        // Manual two-axis aiming remains available in other seats and when
        // head aim is explicitly off. A stale head sample sends no correction.
        vx=Axis(x)*tune.turnSpeed;vy=-Axis(y)*tune.turnSpeed;
    }
    if(!vx)axisRemainderX=0;if(!vy)axisRemainderY=0;
    axisRemainderX+=vx*seconds;axisRemainderY+=vy*seconds;
    INPUT e{};e.type=INPUT_MOUSE;e.mi.dwFlags=MOUSEEVENTF_MOVE|MOUSEEVENTF_MOVE_NOCOALESCE;
    e.mi.dx=(LONG)axisRemainderX;e.mi.dy=(LONG)axisRemainderY;
    if(e.mi.dx||e.mi.dy){if(SendInput(1,&e,sizeof(e))){axisRemainderX-=e.mi.dx;axisRemainderY-=e.mi.dy;}else axisRemainderX=axisRemainderY=0;}
}
#include "PhysicalRadio.inl"
#include "VehicleSeatWheel.inl"
} // namespace

bool PhysicalRadio(const Runtime::Frame& f){return UpdatePhysicalRadio(f);}
bool PhysicalRadioPointer(float& u,float& v){if(!shoulderRadio.active)return false;u=shoulderRadio.u;v=shoulderRadio.v;return true;}
bool VehicleSeatWheel(const Runtime::Frame& f){return UpdateSeatWheel(f);}
void PresentSeatWheel(){DrawSeatWheelOverlay();}

void ReleaseAll(){
    ClosePhysicalRadio();shoulderRadio.armed=false;kitUntil=0;ResetGameplay();Key(Escape,false);pulseUntil[Escape]=0;bothSince=0;both=chordHoldConsumed=menuWas=false;
    havePrevious=false;globalFrame=0;escapePending=false;utilityVehicle=false;menuSince=0;menuHoldConsumed=settingsRequested=false;
}
bool TakeSettingsRequest(){const bool request=settingsRequested;settingsRequested=false;return request;}
bool Global(const Runtime::Frame& f){
    if(globalFrame==f.id)return false;globalFrame=f.id;const auto now=f.tick;
    static bool announced=false;if(!announced){announced=true;const auto tune=Tuning();Log("CONTROLS r131 smooth=%d speed=%.1f deadzone=%.2f; infantry X prone, Y toggle crouch; vehicle hold Y seat wheel F1-F8, L-grip weapon switch, R-click flares, L-click boost, L-trigger alt fire, X utility; both-stick tap recenter, hold 600ms VR settings; left menu tap kit, hold pause",tune.smoothTurn?1:0,tune.turnSpeed,Config().turnDeadzone);}
    if(!havePrevious){BlockHeld(f);havePrevious=true;}
    for(int h=0;h<2;++h)for(int b=0;b<4;++b)if(!Button(f,h,b))blocked[h][b]=false;
    const bool menu=UsableButton(f,0,3)&&!Engine::GrenadePrimed();
    const bool chord=UsableButton(f,0,2)&&UsableButton(f,1,2)&&!Engine::GrenadePrimed();
    if(menu&&!menuWas){menuSince=now;menuHoldConsumed=false;}
    if(chord)menuHoldConsumed=true;
    if((menu||menuWas)&&!menuHoldConsumed&&now>=menuSince&&now-menuSince>=600){
        menuHoldConsumed=true;Pulse(Escape,now);escapePending=true;Log("CONTROL44 left-menu hold: pause");
    }
    if(!menu&&menuWas&&!menuHoldConsumed&&!chord&&!both&&!Engine::FrontendActive()&&(Engine::LocalAlive()||Engine::DeploymentActive()||Engine::CameraReady())){
        kitUntil=now+120;Key(Spawn,true);Log("CONTROL44 left-menu tap: kit screen");
    }
    menuWas=menu;
    // Tap is decided on release; a hold opens VR settings once. Checking
    // the release frame too covers a slow frame crossing the threshold.
    if(chord&&!both){bothSince=now;chordHoldConsumed=false;}
    if((chord||both)&&!chordHoldConsumed&&now>=bothSince&&now-bothSince>=600){
        chordHoldConsumed=true;
        settingsRequested=true;Log("CONTROL44 both-stick hold: VR settings");
    }
    bool request=false;
    if(!chord){
        if(both&&!chordHoldConsumed){request=true;Log("CONTROL44 both-stick tap: recenter");}
        if(both){
            // Releasing part of the shortcut must not deploy flares or boost
            // with its remaining held clicks. Release them before rearming.
            blocked[0][2]=Button(f,0,2);blocked[1][2]=Button(f,1,2);
        }
        bothSince=0;chordHoldConsumed=false;
    }both=chord;
    Key(Escape,pulseUntil[Escape]>now);
    if(chord){modifierUsed=true;Key(Sprint,false);Key(Flares,false);pulseUntil[Flares]=0;Key(Voice,false);}
    return request;
}
void Menu(const Runtime::Frame& f){
    CloseSeatWheel();
    StingerAudio::Update(0);
    cycleWas=cycleUsed=pickupHeld=false;cycleSince=0;
    crouchLatched=crouchWas=vehicleWeaponGrip=vehicleWeaponGripArmed=false;
    Engine::CancelManualReload();
    Engine::ClearBodySlotControls();
    Engine::ClearDefibControls();Engine::ClearC4Controls();
    Engine::StopLocomotion();
    // A held radio chord can itself open a native HUD menu. Keep that one key
    // until the chord is released; all walking/fire/stance input is released.
    const bool utility=utilityVehicle?f.hands[0].primary:f.hands[1].stickClick;
    const bool radio=held[Radio]&&utility&&f.hands[0].trigger>.40f;
    const bool voice=held[Voice]&&utility&&f.hands[0].squeeze>.45f&&!both;
    const bool score=held[Score]&&utility&&(utilityVehicle?f.hands[1].stickClick:f.hands[0].primary);
    const bool wasGame=gameWas;
    for(int i=0;i<Escape;++i){const bool keep=(i==Radio&&radio)||(i==Voice&&voice)||(i==Score&&score)||(i==Spawn&&kitUntil>f.tick);if(!keep){Key(i,false);pulseUntil[i]=0;}}
    Mouse(1,false);trigger[0]=trigger[1]=use=false;
    lastTick=0;mouseRemainder=0;smoothTurnClock={};verticalArmed=turnArmed=false;modifierWas=modifierUsed=false;gameWas=false;escapePending=false;
    axisRemainderX=axisRemainderY=0;vehicleAxesArmed=false;
    flareSince=0;flareWas=flareDelivered=flareConsumed=false;
    if(wasGame)BlockHeld(f);
}
void PointerButton(bool down){Mouse(0,down);}
bool RecenterKeyBlocked(){return GetTickCount64()<seatRecenterBlockUntil;}
void Gameplay(const Runtime::Frame& f,const Mat& reference){
    const auto tune=Tuning();
    const auto& l=f.hands[0];const auto& r=f.hands[1];const auto now=f.tick;
    const auto vehicle=Engine::Vehicle(f);const bool inVehicle=vehicle.role!=Engine::VehicleRole::Infantry;
    const bool personalWeapon=!inVehicle||vehicle.personalWeapon;
    utilityVehicle=inVehicle;
    if(!gameWas||occupiedSeat!=vehicle.seat){ResetGameplay();BlockHeld(f);gameWas=true;occupiedSeat=vehicle.seat;}
    if(kitUntil>now){ResetGameplay();return;}
    if(escapePending){escapePending=false;ResetGameplay();return;}
    const float seconds=lastTick&&now>=lastTick?std::min((float)(now-lastTick)*.001f,.05f):0;lastTick=now;
    const float turnSeconds=SmoothTurnSeconds(f);
    bool wanted[KeyCount]{};int cycleDirection=0;
    // One right-stick press: tap retains fire mode/item cycling; hold sends
    // native kit pickup. A both-stick chord or another utility consumes it.
    const bool cycleButton=!inVehicle&&UsableButton(f,1,2);
    if(cycleButton&&!cycleWas){cycleSince=now;cycleUsed=both;pickupHeld=false;modifierUsed=false;}
    if(both){cycleUsed=true;pickupHeld=false;}
    const bool pickupOnRelease=!cycleButton&&cycleWas&&!cycleUsed&&!modifierUsed&&!both&&now>=cycleSince+400;
    if(!cycleButton&&cycleWas&&!cycleUsed&&!modifierUsed&&!both&&!tune.bodySlots&&now>=cycleSince&&now-cycleSince<400)cycleDirection=1;
    if(!cycleButton)pickupHeld=false;
    cycleWas=cycleButton;
    const bool modifier=inVehicle?UsableButton(f,0,0):(UsableButton(f,1,2)&&(tune.bodySlots||now-cycleSince>=250));
    if(modifier&&!modifierWas){
        modifierUsed=false;modifierGripArmed=r.squeeze<.45f;
        // Holding a gun/support grip or trigger before the click must not
        // turn a pickup hold into voice chat, a radio or grenade shortcut.
        modifierVoiceArmed=l.squeeze<.45f;modifierLeftTriggerArmed=l.trigger<.40f;modifierRightTriggerArmed=r.trigger<.40f;
    }
    if(both)modifierUsed=true;
    const bool a=UsableButton(f,1,0),b=UsableButton(f,1,1),x=UsableButton(f,0,0),y=UsableButton(f,0,1),lc=UsableButton(f,0,2),rc=UsableButton(f,1,2);
    const Vec stickMove={l.stickActive&&!both?l.stickX:0,0,l.stickActive&&!both?l.stickY:0};
    const Vec move=inVehicle||!tune.hmdMovement?stickMove:Direction(stickMove,Level(Mul(f.head,Inverse(reference))));
    wanted[Forward]=move.z>Config().moveDeadzone;wanted[Backward]=move.z<-Config().moveDeadzone;
    wanted[Left]=move.x<-Config().moveDeadzone;wanted[Right]=move.x>Config().moveDeadzone;
    if(l.trigger<.40f)triggerArmed[0]=true;if(r.trigger<.40f)triggerArmed[1]=true;if(r.squeeze<.45f)useArmed=true;
    trigger[0]=triggerArmed[0]&&Hysteresis(trigger[0],l.trigger);trigger[1]=triggerArmed[1]&&Hysteresis(trigger[1],r.trigger);use=useArmed&&Hysteresis(use,r.squeeze,.65f,.45f);
    if(modifier&&!use)modifierGripArmed=true;
    if(l.squeeze<.45f)modifierVoiceArmed=true;
    if(l.trigger<.40f)modifierLeftTriggerArmed=true;
    if(r.trigger<.40f)modifierRightTriggerArmed=true;
    const bool utilityLeft=trigger[0]&&(inVehicle||modifierLeftTriggerArmed);
    const bool utilityRight=trigger[1]&&(inVehicle||modifierRightTriggerArmed);
    const bool utilityVoice=l.squeeze>.65f&&(inVehicle||modifierVoiceArmed);
    const bool radio=modifier&&!both&&!pickupHeld&&utilityLeft;
    // A stance press is consumed even inside a utility chord. Releasing the
    // modifier while still holding Y must not unexpectedly toggle crouch.
    const bool crouchPress=y&&!crouchWas;crouchWas=y;
    if(!inVehicle&&!modifier&&!both&&!pickupHeld){
        if(x||a||lc)crouchLatched=false; // prone, jump and sprint leave crouch
        else if(crouchPress){crouchLatched=!crouchLatched;Log("CONTROL129 crouch toggle=%d",crouchLatched?1:0);}
    }
    if(!inVehicle)wanted[Crouch]=crouchLatched;
    // Release-to-arm prevents a foregrip already held while entering a seat
    // from switching weapons. Each squeeze fires once, with hysteresis.
    if(personalWeapon||!l.gripValid||!std::isfinite(l.squeeze)){
        vehicleWeaponGrip=vehicleWeaponGripArmed=false;
    }else{
        const bool down=Hysteresis(vehicleWeaponGrip,l.squeeze,.65f,.45f);
        if(l.squeeze<.45f)vehicleWeaponGripArmed=true;
        if(down&&!vehicleWeaponGrip&&vehicleWeaponGripArmed&&!modifier&&!both)Pulse(FireMode,now);
        vehicleWeaponGrip=down;
    }
    if(modifier&&!both&&!pickupHeld){
        const bool score=inVehicle?rc:x;
        wanted[PickUp]=inVehicle&&a;wanted[Map]=b;wanted[Score]=score;wanted[Squad]=y;wanted[Voice]=utilityVoice;wanted[Radio]=radio;wanted[Spawn]=use&&modifierGripArmed;wanted[Grenade]=utilityRight&&!radio;
        if(a||b||score||y||utilityVoice||utilityLeft||utilityRight||wanted[Spawn])modifierUsed=true;
    }else if(!both&&!pickupHeld){
        wanted[Jump]=a;wanted[Parachute]=a;wanted[Reload]=b&&(!personalWeapon||!tune.manualReload);wanted[Use]=use;
        if(inVehicle)wanted[Sprint]=lc;
        else{wanted[Prone]=x;wanted[Sprint]=lc&&!x&&!crouchLatched;}
    }
    if(inVehicle)wanted[Flares]=VehicleFlares(rc,modifier||both,now);
    const float sx=r.stickActive?r.stickX:0,sy=r.stickActive?r.stickY:0;
    if(std::abs(sy)<.30f)verticalArmed=true;
    if(std::abs(sx)<Config().turnDeadzone)turnArmed=true;
    if(std::abs(sx)<Config().turnDeadzone&&std::abs(sy)<Config().turnDeadzone)vehicleAxesArmed=true;
    const bool vertical=std::abs(sy)>.70f&&std::abs(sy)>std::abs(sx);
    if(vertical&&verticalArmed&&!both&&!radio&&!pickupHeld){verticalArmed=false;
        if(!modifier&&!inVehicle){if(tune.bodySlots)Wheel(sy>0?1:-1);else cycleDirection=sy>0?-1:1;}
    }
    if(!inVehicle&&!both){
        if(pickupOnRelease){Pulse(PickUp,now);modifierUsed=cycleUsed=true;}
        if(cycleButton&&!modifierUsed&&!cycleUsed&&now>=cycleSince+400){pickupHeld=true;modifierUsed=cycleUsed=true;}
        wanted[PickUp]=pickupHeld&&cycleButton;
    }
    float snap=0;
    if(!inVehicle&&tune.snapTurn&&!modifier&&!both&&!vertical&&turnArmed&&std::abs(sx)>.7f){
        snap=std::copysign(tune.snapAngle*.01745329252f,sx);turnArmed=false;
    }
    // Publish a RATE, not an already timed/rounded mouse event. The native
    // local-soldier update integrates it with the same interval as its input.
    const bool smoothAllowed=!inVehicle&&tune.smoothTurn&&!tune.snapTurn&&!modifier&&!both&&!vertical&&turnArmed;
    const float smoothRate=smoothAllowed?Axis(sx)*tune.turnSpeed:0.f;
    const bool nativeMovement=Engine::FootLocomotion(f,stickMove,snap,smoothRate,!inVehicle&&!both&&!modifier);
    if(nativeMovement){
        // BF2 decides sprint before the soldier update (BF2+EE050), and
        // requires a positive native throttle in that earlier input packet.
        // Keep the ordinary directional keys for that gate; the soldier hook
        // replaces the axes with analog values, so movement is not added twice.
        wanted[Forward]=stickMove.z>Config().moveDeadzone;
        wanted[Backward]=stickMove.z<-Config().moveDeadzone;
        wanted[Left]=stickMove.x<-Config().moveDeadzone;
        wanted[Right]=stickMove.x>Config().moveDeadzone;
    }
    if(radio)RadioCursor(sx,sy,seconds);
    if(inVehicle){
        mouseRemainder=0;
        if(!modifier&&!both&&vehicleAxesArmed)VehicleMouse(f,vehicle,sx,sy,vehicle.lookValid?turnSeconds:seconds,tune);else axisRemainderX=axisRemainderY=0;
    }else if(!nativeMovement&&smoothAllowed)Turn(sx,turnSeconds,tune);else mouseRemainder=0;
    const bool fireModeTap=!modifier&&modifierWas&&!modifierUsed&&!both;
    // Vehicle X remains the utility modifier; left grip now switches weapons.
    modifierWas=modifier;
    bool slotConsumed=false;
    const int draw=Engine::BodySlotControls(f,!inVehicle,!modifier&&!both,use,cycleDirection,slotConsumed);
    if(slotConsumed||Engine::GrenadePrimed())wanted[Use]=false;
    if(draw>=1&&draw<=9)Pulse(Select1+draw-1,now);
    if(draw)Engine::CancelManualReload();
    const bool empty=!inVehicle&&Engine::EmptyHands();
    if(fireModeTap&&!inVehicle&&tune.bodySlots&&!empty&&!draw){
        const int item=Engine::FireModeItem();
        if(item>=1&&item<=9){Pulse(Select1+item-1,now);Log("CONTROL119 fire-mode reselect held nativeItem=%d",item);}
    }
    if(empty){
        wanted[Reload]=wanted[Grenade]=false;pulseUntil[Reload]=pulseUntil[Grenade]=0;
        // Grip now carries equipment. B is free when empty-handed and keeps
        // vehicle/ladder interaction available in either inventory mode.
        if(!modifier&&!both&&b&&!Engine::GrenadePrimed())wanted[Use]=true;
    }
    // B is not a fallback in manual mode. Its infantry pistol-magazine
    // ejection is handled by the pistol profiles. Manual gesture
    // reloadPulse below still reaches native Reload; vehicle B is unchanged.
    const auto manual=Engine::ManualReloadControls(f,!empty&&personalWeapon&&!modifier&&!both,trigger[0],wanted[Reload],!empty&&personalWeapon&&!modifier&&!both&&tune.manualReload&&b);
    if(manual.reloadPulse)Pulse(Reload,now);
    if(manual.haptic>0)Runtime::Haptic(0,manual.haptic,manual.hapticMs);
    bool defibFire=false;
    const bool physicalDefib=Engine::DefibControls(f,!empty&&!inVehicle&&!modifier&&!both&&draw==0,defibFire);
    bool knifeFire=false;const bool physicalKnife=Engine::KnifeControls(f,!inVehicle&&!modifier&&!both&&draw==0,knifeFire);
    bool itemFire=false;const bool physicalItem=!inVehicle&&!modifier&&!both&&Engine::PhysicalItemFire(f,itemFire);
    bool c4Fire=false;const bool physicalC4=Engine::C4Controls(f,!inVehicle&&!modifier&&!both&&draw<=0,trigger[0],trigger[1],c4Fire);
    const bool fire=physicalKnife?knifeFire:physicalC4?c4Fire:physicalItem?itemFire:(!empty&&(!modifier||radio)&&!both&&r.aimValid&&r.gripValid&&(physicalDefib?defibFire:trigger[1])&&!manual.blockFire);
    static bool triggerWasDown=false;static unsigned triggerReports=0;const bool triggerDown=r.trigger>.55f;
    if(triggerDown!=triggerWasDown){triggerWasDown=triggerDown;if(triggerReports++<48)Log("TRIGGER right value=%.3f armed=%d aim=%d grip=%d modifier=%d both=%d fire=%d",r.trigger,triggerArmed[1]?1:0,r.aimValid?1:0,r.gripValid?1:0,modifier?1:0,both?1:0,fire?1:0);}
    Mouse(0,fire);Mouse(1,inVehicle&&!personalWeapon&&!modifier&&!both&&trigger[0]);
    Keys(wanted,now);
    StingerAudio::Update(inVehicle?0:Engine::StingerSoundState());
}
}}
