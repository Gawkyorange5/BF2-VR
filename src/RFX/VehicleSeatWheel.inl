// r133: upright, tilted, release-to-select vehicle seat wheel.
// Native F1-F8 remain responsible for occupied/missing seats and game rules.
// The wheel and settings share one XR overlay, with mutually exclusive owners.
namespace SeatWheel {
constexpr int Pixels=512;
constexpr float Size=.32f,Pi=3.14159265358979323846f,Step=Pi*.25f;
struct State {
    bool active=false,armed=false,visible=false,dirty=false;
    int choice=-1,shown=-1,cursorX=256,cursorY=256,pending=-1;
    Mat panel{};XrPosef pose{};
    std::uintptr_t seat=0;std::uint64_t origin=0;
    ULONGLONG lastSample=0,lastDraw=0,until=0;
    HDC dc=nullptr;HBITMAP bitmap=nullptr;HGDIOBJ oldBitmap=nullptr;
    HFONT title=nullptr,label=nullptr,smallFont=nullptr;void* pixels=nullptr;
} state;
bool Place(const Runtime::Frame& f){
    auto& s=state;if(!Rigid(f.head)||!Rigid(f.hands[0].grip))return false;
    const Vec position=Position(f.hands[0].grip),delta=Sub(position,Position(f.head));
    Vec away=Unit({delta.x,0,delta.z});
    if(std::hypot(delta.x,delta.z)<.08f)away=Row(Level(f.head),2);
    // Freeze at the opening hand position, with readable world-up text.
    // Tilt 50 degrees above horizontal: front faces the player and upward.
    // Controller roll must not turn labels upside down. Hit testing uses
    // this same frozen frame so moving the hand still chooses each sector.
    const float tilt=50.f*Pi/180.f;
    const Vec x=Unit(Cross({0,1,0},away));
    const Vec z=Add(Scale(away,std::sin(tilt)),Vec{0,-std::cos(tilt),0});
    const Vec rows[]={x,Cross(z,x),z};s.panel=Identity();
    for(int i=0;i<3;++i){s.panel.m[i][0]=rows[i].x;s.panel.m[i][1]=rows[i].y;s.panel.m[i][2]=rows[i].z;}
    SetPosition(s.panel,position);
    if(!Rigid(s.panel))return false;
    D3DXMATRIX rotation{};std::memcpy(&rotation,&s.panel,sizeof(s.panel));
    D3DXQUATERNION q{};D3DXQuaternionRotationMatrix(&q,&rotation);D3DXQuaternionNormalize(&q,&q);
    const Vec p=Position(s.panel);s.pose.orientation={-q.x,-q.y,q.z,q.w};s.pose.position={p.x,p.y,-p.z};
    Log("SEAT133 upright panel tilt=50deg distance=%.3fm; selection plane frozen",Length(delta));
    return true;
}
void FreeCanvas(){
    auto& s=state;
    if(s.dc&&s.oldBitmap)SelectObject(s.dc,s.oldBitmap);
    if(s.bitmap)DeleteObject(s.bitmap);if(s.dc)DeleteDC(s.dc);
    if(s.title)DeleteObject(s.title);if(s.label)DeleteObject(s.label);if(s.smallFont)DeleteObject(s.smallFont);
    s.dc=nullptr;s.bitmap=nullptr;s.oldBitmap=nullptr;s.pixels=nullptr;s.title=s.label=s.smallFont=nullptr;
}
bool Canvas(){
    auto& s=state;s.dc=CreateCompatibleDC(nullptr);if(!s.dc)return false;
    BITMAPINFO info{};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth=Pixels;info.bmiHeader.biHeight=-Pixels;
    info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;info.bmiHeader.biCompression=BI_RGB;
    s.bitmap=CreateDIBSection(s.dc,&info,DIB_RGB_COLORS,&s.pixels,nullptr,0);
    if(!s.bitmap||!s.pixels){FreeCanvas();return false;}
    s.oldBitmap=SelectObject(s.dc,s.bitmap);SetBkMode(s.dc,TRANSPARENT);
    const auto font=[](int size,int weight){return CreateFontA(-size,0,0,0,weight,FALSE,FALSE,FALSE,
        DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,ANTIALIASED_QUALITY,DEFAULT_PITCH,"Segoe UI");};
    s.title=font(27,FW_SEMIBOLD);s.label=font(25,FW_SEMIBOLD);s.smallFont=font(16,FW_NORMAL);
    if(!s.title||!s.label||!s.smallFont){FreeCanvas();return false;}return true;
}
void Text(const char* text,int x,int y,int width,int height,HFONT font,COLORREF color){
    auto& s=state;const auto old=SelectObject(s.dc,font);SetTextColor(s.dc,color);
    RECT rect{x,y,x+width,y+height};DrawTextA(s.dc,text,-1,&rect,DT_CENTER|DT_VCENTER|DT_SINGLELINE|DT_NOPREFIX);
    SelectObject(s.dc,old);
}
void Draw(){
    auto& s=state;GdiFlush();std::memset(s.pixels,0,Pixels*Pixels*4);
    const auto oldPen=SelectObject(s.dc,GetStockObject(NULL_PEN));
    for(int i=0;i<8;++i){
        POINT points[34]{};
        for(int j=0;j<=16;++j){
            const float angle=i*Step-Step*.5f+.024f+(Step-.048f)*j/16.f;
            points[j]={(LONG)std::lround(256+200*std::sin(angle)),(LONG)std::lround(256-200*std::cos(angle))};
            points[33-j]={(LONG)std::lround(256+62*std::sin(angle)),(LONG)std::lround(256-62*std::cos(angle))};
        }
        HBRUSH brush=CreateSolidBrush(i==s.choice?RGB(15,121,153):RGB(23,38,53));
        const auto old=SelectObject(s.dc,brush);Polygon(s.dc,points,34);SelectObject(s.dc,old);DeleteObject(brush);
        const int x=(int)std::lround(256+139*std::sin(i*Step)),y=(int)std::lround(256-139*std::cos(i*Step));
        char label[24];sprintf_s(label,"Seat %d",i+1);
        Text(label,x-49,y-22,98,30,s.label,RGB(238,249,255));
        sprintf_s(label,"F%d",i+1);Text(label,x-26,y+8,52,22,s.smallFont,RGB(158,214,226));
    }
    HBRUSH center=CreateSolidBrush(RGB(13,24,34));const auto oldBrush=SelectObject(s.dc,center);
    Ellipse(s.dc,199,199,313,313);SelectObject(s.dc,oldBrush);DeleteObject(center);
    Text("CHANGE SEATS",72,6,368,37,s.title,RGB(235,249,255));
    Text("Release Y",205,231,102,23,s.smallFont,RGB(193,215,226));
    Text("to cancel",205,254,102,23,s.smallFont,RGB(193,215,226));
    Text("Move left hand  /  Release Y to select",40,474,432,28,s.smallFont,RGB(200,227,237));
    if(s.cursorX>=0){
        HBRUSH cursor=CreateSolidBrush(RGB(109,255,222));const auto old=SelectObject(s.dc,cursor);
        Ellipse(s.dc,s.cursorX-5,s.cursorY-5,s.cursorX+6,s.cursorY+6);SelectObject(s.dc,old);DeleteObject(cursor);
    }
    SelectObject(s.dc,oldPen);GdiFlush();
    // GDI writes BGRX. Only painted pixels are opaque; the quad's corners
    // stay transparent. No extra world render, shader or D3D9 target is used.
    auto* p=static_cast<std::uint32_t*>(s.pixels);
    for(unsigned i=0;i<Pixels*Pixels;++i)p[i]=(p[i]&0xffffffu)?p[i]|0xff000000u:0;
}
}
void CloseSeatWheel(){
    auto& s=SeatWheel::state;
    if(s.active)Runtime::HideSettingsOverlay();
    if(s.active||s.pending>=0)for(int i=Seat1;i<=Seat8;++i){Key(i,false);pulseUntil[i]=0;}
    SeatWheel::FreeCanvas();s={}; // also require a fresh Y release before opening
}
bool UpdateSeatWheel(const Runtime::Frame& f){
    using namespace SeatWheel;auto& s=state;const auto& hand=f.hands[0];
    const auto now=GetTickCount64();const bool y=hand.secondary;
    const bool tracking=f.valid&&f.focused&&f.shouldRender&&hand.gripValid&&Rigid(hand.grip)&&
        f.tick&&now>=f.tick&&now-f.tick<=200;
    if(!s.active&&s.pending<0){
        if(!tracking){s.armed=false;return false;}
        if(!y){s.armed=true;return false;}
        if(!s.armed)return false;
        s.armed=false;
        // X+Y remains squad screen. A Y held during entry, a menu or
        // tracking loss must not turn into a delayed wheel opening.
        if(!UsableButton(f,0,1)||hand.primary||both||menuWas||kitUntil>f.tick||pulseUntil[Escape]>f.tick||
           shoulderRadio.active||shoulderRadio.blockUntilRelease||Engine::MenuActive()||!Engine::LocalAlive())return false;
        const auto vehicle=Engine::Vehicle(f);
        if(vehicle.role==Engine::VehicleRole::Infantry||!vehicle.seat)return false;
        ResetGameplay();
        if(!Canvas()){Log("SEAT131 wheel canvas unavailable; no seat input sent");return true;}
        s.active=true;s.dirty=true;s.seat=vehicle.seat;s.origin=f.origin;s.lastSample=now;
        if(!Place(f)){CloseSeatWheel();return true;}
        Runtime::Haptic(0,.18f,25);
        Log("SEAT133 wheel opened at left grip; upright tilted plane frozen, F1-F8 native selection");
        return true;
    }
    const bool safe=tracking&&f.origin==s.origin&&now>=s.lastSample&&now-s.lastSample<=250&&
        !hand.primary&&!both&&!menuWas&&kitUntil<=f.tick&&pulseUntil[Escape]<=f.tick&&
        !Engine::MenuActive()&&Engine::LocalAlive()&&Engine::Vehicle(f).seat==s.seat;
    if(!safe){CloseSeatWheel();return true;}
    s.lastSample=now;
    if(s.pending>=0){
        // Keep one ordinary 120ms key pulse after the visual wheel closes.
        // The next gameplay Reset must not eat a not-yet-delivered seat key.
        if(now>=s.until){CloseSeatWheel();return true;}
        Key(Seat1+s.pending,true);return true;
    }
    const Vec local=Point(Position(hand.grip),Inverse(s.panel));
    const float radius=std::hypot(local.x,local.y);
    if(!std::isfinite(local.x+local.y+local.z)||Length(local)>.7f){CloseSeatWheel();return true;}
    int choice=-1;
    if(std::abs(local.z)<=.14f&&radius>=(s.choice>=0?.035f:.045f)&&radius<=.15f){
        float angle=std::atan2(local.x,local.y);if(angle<0)angle+=2*Pi;
        choice=((int)std::floor((angle+Step*.5f)/Step))%8;
        if(s.choice>=0){
            const float difference=std::remainder(angle-s.choice*Step,2*Pi);
            if(std::abs(difference)<Step*.5f+.045f)choice=s.choice;
        }
    }
    if(choice!=s.choice){
        s.choice=choice;s.dirty=true;
        if(choice>=0)Runtime::Haptic(0,.12f,18);
    }
    const int cx=(int)std::lround(256+local.x*Pixels/Size),cy=(int)std::lround(256-local.y*Pixels/Size);
    const bool cursor=std::abs(local.z)<=.14f&&cx>=4&&cx<Pixels-4&&cy>=4&&cy<Pixels-4;
    const int x=cursor?cx:-1,yPixel=cursor?cy:-1;
    if(std::abs(x-s.cursorX)>=2||std::abs(yPixel-s.cursorY)>=2){s.cursorX=x;s.cursorY=yPixel;s.dirty=true;}
    if(!y){
        // Select only a highlighted, successfully uploaded choice. A fast
        // release before the wheel appears, or release in the centre, cancels.
        const int selected=s.visible&&s.choice==s.shown?s.choice:-1;
        const auto selectedSeat=s.seat;const auto selectedOrigin=s.origin;
        CloseSeatWheel();
        if(selected>=0){
            s.pending=selected;s.until=now+120;s.lastSample=now;s.seat=selectedSeat;s.origin=selectedOrigin;
            Pulse(Seat1+selected,now);Key(Seat1+selected,true);
            Runtime::Haptic(0,.28f,35);Log("SEAT131 requested native F%d on release",selected+1);
        }else Log("SEAT131 cancelled without seat input");
    }
    return true;
}
void DrawSeatWheelOverlay(){
    using namespace SeatWheel;auto& s=state;if(!s.active)return;
    Runtime::Frame f{};
    if(!Runtime::Snapshot(f)||!f.valid||!f.focused||!f.shouldRender||f.origin!=s.origin){CloseSeatWheel();return;}
    const auto now=GetTickCount64();
    const bool redraw=s.dirty&&(!s.lastDraw||now-s.lastDraw>=33);
    if(redraw)Draw();
    if(Runtime::SettingsOverlay(s.pose,Size,Size,redraw?static_cast<const unsigned char*>(s.pixels):nullptr,Pixels,Pixels,true)){
        s.visible=true;
        if(redraw){s.dirty=false;s.shown=s.choice;s.lastDraw=now;}
    }else{
        Log("SEAT131 wheel overlay unavailable; cancelled without seat input");
        CloseSeatWheel();
    }
}
