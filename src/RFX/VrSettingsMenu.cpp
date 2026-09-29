#include "VrSettingsMenu.h"
#include "LiveSettings.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace CleanVR { namespace SettingsMenu { namespace {
constexpr int width=1024,height=800,firstRow=140,rowHeight=96;
constexpr float panelWidth=1.65f,panelHeight=panelWidth*height/width,distance=1.7f;
const char* pages[]={"Turning","HUD","Body / arms","Vehicles","Scopes"};
bool active=false,dirty=true,triggerWas=false,closeWas=false,chordArmed=false,chordWas=false;
ULONGLONG chordSince=0,lastDraw=0,repeatAt=0;std::uint64_t frameId=0,origin=0;
Mat panel{};XrPosef pose{};int page=0,px=-1,py=-1,drag=-1,repeat=-1,repeatDirection=0;
unsigned count=0;const LiveOption* options=nullptr;
float values[32]{},loaded[32]{};bool changed[32]{};
char status[180]="";
HDC dc=nullptr;HBITMAP bitmap=nullptr;HGDIOBJ originalBitmap=nullptr;void* pixels=nullptr;
HFONT normalFont=nullptr,smallFont=nullptr,titleFont=nullptr;

void Text(const char* value,int x,int y,int w,HFONT font,COLORREF color){
    const auto old=SelectObject(dc,font);SetTextColor(dc,color);RECT r{x,y,x+w,y+36};
    DrawTextA(dc,value,-1,&r,DT_LEFT|DT_SINGLELINE|DT_END_ELLIPSIS|DT_NOPREFIX);SelectObject(dc,old);
}
void Box(int x,int y,int w,int h,COLORREF color){RECT r{x,y,x+w,y+h};HBRUSH brush=CreateSolidBrush(color);FillRect(dc,&r,brush);DeleteObject(brush);}
bool Inside(int x,int y,int w,int h){return px>=x&&px<x+w&&py>=y&&py<y+h;}
void Button(const char* label,int x,int y,int w,int h,bool selected=false){
    Box(x,y,w,h,selected?RGB(24,108,132):Inside(x,y,w,h)?RGB(55,91,110):RGB(38,53,70));
    Text(label,x+14,y+(h-28)/2,w-24,normalFont,RGB(235,245,255));
}
void FreeCanvas(){
    if(dc&&originalBitmap)SelectObject(dc,originalBitmap);
    if(bitmap)DeleteObject(bitmap);if(dc)DeleteDC(dc);
    if(normalFont)DeleteObject(normalFont);if(smallFont)DeleteObject(smallFont);if(titleFont)DeleteObject(titleFont);
    dc=nullptr;bitmap=nullptr;originalBitmap=nullptr;normalFont=smallFont=titleFont=nullptr;pixels=nullptr;
}
bool Canvas(){
    if(dc)return true;
    dc=CreateCompatibleDC(nullptr);if(!dc)return false;
    BITMAPINFO bi{};bi.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);bi.bmiHeader.biWidth=width;bi.bmiHeader.biHeight=-height;
    bi.bmiHeader.biPlanes=1;bi.bmiHeader.biBitCount=32;bi.bmiHeader.biCompression=BI_RGB;
    bitmap=CreateDIBSection(dc,&bi,DIB_RGB_COLORS,&pixels,nullptr,0);
    if(!bitmap||!pixels){FreeCanvas();return false;}originalBitmap=SelectObject(dc,bitmap);SetBkMode(dc,TRANSPARENT);
    normalFont=CreateFontA(-24,0,0,0,FW_MEDIUM,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,ANTIALIASED_QUALITY,DEFAULT_PITCH,"Segoe UI");
    smallFont=CreateFontA(-18,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,ANTIALIASED_QUALITY,DEFAULT_PITCH,"Segoe UI");
    titleFont=CreateFontA(-32,0,0,0,FW_SEMIBOLD,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,ANTIALIASED_QUALITY,DEFAULT_PITCH,"Segoe UI");
    if(!normalFont||!smallFont||!titleFont){FreeCanvas();return false;}return true;
}
void Place(const Runtime::Frame& f){
    panel=Level(f.head);const Vec p=Point({0,0,distance},panel);SetPosition(panel,p);
    const float yaw=std::atan2(panel.m[2][0],panel.m[2][2]);
    pose.orientation={0,-std::sin(yaw*.5f),0,std::cos(yaw*.5f)};pose.position={p.x,p.y,-p.z};origin=f.origin;
}
void LocatePointer(const Runtime::Frame& f){
    int x=-1,y=-1;
    if(f.hands[1].aimValid){
        const Vec ray=Row(f.hands[1].aim,2),start=Position(f.hands[1].aim),normal=Row(panel,2);
        const float denominator=Dot(ray,normal);
        if(denominator>.05f){
            const float t=Dot(Sub(Position(panel),start),normal)/denominator;
            if(t>0){const Vec delta=Sub(Add(start,Scale(ray,t)),Position(panel));
                const float u=.5f+Dot(delta,Row(panel,0))/panelWidth,v=.5f-Dot(delta,Row(panel,1))/panelHeight;
                if(u>=0&&u<=1&&v>=0&&v<=1){x=(int)(u*(width-1));y=(int)(v*(height-1));}
            }
        }
    }
    if(x!=px||y!=py){dirty=true;px=x;py=y;}
}
void Change(unsigned i,float value){
    const auto& o=options[i];value=std::max(o.minimum,std::min(o.maximum,value));
    if(values[i]==value)return;values[i]=value;changed[i]=std::abs(value-loaded[i])>.0001f;dirty=true;
    strcpy_s(status,"Changes selected. Click Apply to save and use them.");
}
void Step(unsigned i,int direction){Change(i,options[i].toggle?(values[i]?0.f:1.f):values[i]+options[i].step*direction);}
void Slide(unsigned i){
    const auto& o=options[i];float v=o.minimum+(o.maximum-o.minimum)*std::max(0.f,std::min(1.f,(px-32)/632.f));
    v=std::round(v/o.step)*o.step;Change(i,v);
}
bool HasChanges(){for(unsigned i=0;i<count;++i)if(changed[i])return true;return false;}
void Apply(){
    if(!HasChanges()){strcpy_s(status,"No changes to save.");dirty=true;return;}
    if(SaveLiveValues(values,changed)){
        for(unsigned i=0;i<count;++i){loaded[i]=values[i];changed[i]=false;}
        strcpy_s(status,"Saved. Settings are now active.");Runtime::Haptic(1,.25f,40);
    }else strcpy_s(status,"Save failed; some changes may have saved. Check game folder write access.");
    dirty=true;
}
void Click(ULONGLONG now){
    for(int i=0;i<5;++i)if(Inside(24+i*196,82,188,44)){page=i;dirty=true;return;}
    if(Inside(24,650,200,48)){Apply();return;}
    if(Inside(240,650,250,48)){
        for(unsigned i=0;i<count;++i)if(options[i].page==(unsigned)page)Change(i,options[i].initial);return;
    }
    if(Inside(780,650,220,48)){Close();return;}
    int row=0;
    for(unsigned i=0;i<count;++i)if(options[i].page==(unsigned)page){
        const int y=firstRow+row++*rowHeight;
        if(options[i].toggle){if(Inside(738,y+8,236,48))Step(i,1);}
        else if(Inside(690,y+8,60,48)||Inside(930,y+8,60,48)){
            repeat=(int)i;repeatDirection=px<800?-1:1;Step(i,repeatDirection);repeatAt=now+400;
        }else if(Inside(32,y+64,632,24)){drag=(int)i;Slide(i);}
    }
}
void Draw(){
    Box(0,0,width,height,RGB(15,24,35));Text("BATTLEFIELD 2 VR SETTINGS",24,20,760,titleFont,RGB(237,247,255));
    Text("r44",924,28,70,smallFont,RGB(135,174,196));
    for(int i=0;i<5;++i)Button(pages[i],24+i*196,82,188,44,page==i);
    int row=0;char value[80];
    for(unsigned i=0;i<count;++i)if(options[i].page==(unsigned)page){
        const auto& o=options[i];const int y=firstRow+row++*rowHeight;
        Box(24,y,976,90,RGB(22,34,47));Text(o.label,36,y+6,642,normalFont,RGB(236,243,250));
        Text(o.help,36,y+38,646,smallFont,RGB(171,192,207));
        if(o.toggle)Button(values[i]>.5f?"On":"Off",738,y+8,236,48,values[i]>.5f);
        else{
            Button("-",690,y+8,60,48);Button("+",930,y+8,60,48);
            sprintf_s(value,"%.2f%s",values[i],changed[i]?" *":"");Text(value,770,y+18,158,normalFont,RGB(95,218,227));
            const int end=32+(int)(632*(values[i]-o.minimum)/(o.maximum-o.minimum));
            Box(32,y+74,632,6,RGB(49,65,83));Box(32,y+74,end-32,6,RGB(46,179,199));Box(end-3,y+68,7,18,RGB(213,247,255));
        }
    }
    Button("Apply",24,650,200,48,HasChanges());Button("Reset this tab",240,650,250,48);Button("Close + recenter",780,650,220,48);
    Text(status,24,714,974,smallFont,RGB(115,223,211));
    Text("Point + right trigger. B closes (unapplied changes discarded). Game continues while open.",24,752,976,smallFont,RGB(170,187,202));
    if(px>=0){Box(px-7,py-2,15,5,RGB(83,247,228));Box(px-2,py-7,5,15,RGB(83,247,228));}
    GdiFlush();
}
}
bool Active(){return active;}
bool Open(const Runtime::Frame& f){
    if(active)return true;options=LiveOptions(count);if(count>32||!Canvas())return false;
    for(unsigned i=0;i<count;++i){values[i]=loaded[i]=LiveValue(i);changed[i]=false;}
    active=true;dirty=true;Place(f);triggerWas=f.hands[1].trigger>.4f;closeWas=f.hands[1].secondary;
    chordArmed=false;chordWas=false;chordSince=lastDraw=0;frameId=0;drag=repeat=-1;px=py=-1;
    strcpy_s(status,"Choose settings, then Apply. Closing will recenter your view.");
    Log("SETTINGS44 headset menu opened");return true;
}
void Close(){
    if(!active)return;active=false;Runtime::HideSettingsOverlay();FreeCanvas();
    Log("SETTINGS44 headset menu closed");
}
void Input(const Runtime::Frame& f){
    if(!active||frameId==f.id)return;frameId=f.id;
    if(origin!=f.origin){Place(f);drag=repeat=-1;triggerWas=true;dirty=true;}
    const bool chord=f.hands[0].stickClick&&f.hands[1].stickClick;
    if(!f.hands[0].stickClick&&!f.hands[1].stickClick)chordArmed=true;
    if(chord&&chordArmed&&!chordWas)chordSince=f.tick;
    const bool close=f.hands[1].secondary;
    if((close&&!closeWas)||(chord&&chordArmed&&f.tick>=chordSince+600)){Close();return;}
    closeWas=close;chordWas=chord;
    LocatePointer(f);
    const bool pressed=f.hands[1].trigger>(triggerWas?.4f:.55f);
    if(pressed&&!triggerWas){Click(f.tick);if(!active)return;}
    if(pressed&&drag>=0&&px>=0)Slide((unsigned)drag);
    if(pressed&&repeat>=0){
        int row=0;for(int i=0;i<repeat;++i)if(options[i].page==(unsigned)page)++row;
        if(!Inside(repeatDirection<0?690:930,firstRow+row*rowHeight+8,60,48))repeat=-1;
        else if(f.tick>=repeatAt){Step((unsigned)repeat,repeatDirection);repeatAt=f.tick+90;}
    }
    if(!pressed||px<0){drag=repeat=-1;}triggerWas=pressed;
}
void Present(){
    if(!active)return;
    const auto now=GetTickCount64();const bool redraw=dirty&&(!lastDraw||now-lastDraw>=33);
    if(redraw)Draw();
    if(Runtime::SettingsOverlay(pose,panelWidth,panelHeight,redraw?static_cast<const unsigned char*>(pixels):nullptr,width,height)){
        if(redraw){dirty=false;lastDraw=now;}
    }else{dirty=true;strcpy_s(status,"VR menu image unavailable; release and hold both clicks to close.");}
}
}}
