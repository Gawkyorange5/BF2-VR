#include "VrSettingsMenu.h"
#include "LiveSettings.h"
#include "Bf2Adapter.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace CleanVR { namespace SettingsMenu { namespace {
constexpr int width=1024,height=800,firstRow=140,rowHeight=96,rowsPerPage=5;
constexpr float panelWidth=1.65f,panelHeight=panelWidth*height/width,distance=1.7f;
const char* pages[]={"Turning","HUD","Body","Vehicles","Scopes","Move","Weapons","Graphics","Gear","Slots"};
constexpr int pageCount=sizeof(pages)/sizeof(pages[0]),tabStep=97,tabWidth=91;
bool active=false,dirty=true,triggerWas=false,closeWas=false,chordArmed=false,chordWas=false;
ULONGLONG chordSince=0,lastDraw=0,repeatAt=0;std::uint64_t frameId=0,origin=0;
Mat panel{};XrPosef pose{};int page=0,pagePart=0,px=-1,py=-1,drag=-1,repeat=-1,repeatDirection=0;
unsigned count=0;const LiveOption* options=nullptr;
constexpr unsigned optionCapacity=128;
float values[optionCapacity]{},loaded[optionCapacity]{};bool changed[optionCapacity]{};
char status[180]="";
int selectedSlot=0,slotOption[7][3]{};bool slotPreviewVisible=false;
const char* slotNames[]={"Gadget 1 - left shoulder","Gadget 2 - right shoulder","Primary weapon - right hip","Secondary weapon - left hip","Grenade - right chest","Knife - waistband","Gadget 3 - behind head"};
const char* slotKeys[]={"left_shoulder","right_shoulder","primary","secondary","grenade","knife","rear_head"};
HDC dc=nullptr;HBITMAP bitmap=nullptr;HGDIOBJ originalBitmap=nullptr;void* pixels=nullptr;
HFONT normalFont=nullptr,smallFont=nullptr,titleFont=nullptr;

void Text(const char* value,int x,int y,int w,HFONT font,COLORREF color){
    const auto old=SelectObject(dc,font);SetTextColor(dc,color);RECT r{x,y,x+w,y+36};
    DrawTextA(dc,value,-1,&r,DT_LEFT|DT_SINGLELINE|DT_END_ELLIPSIS|DT_NOPREFIX);SelectObject(dc,old);
}
void Box(int x,int y,int w,int h,COLORREF color){RECT r{x,y,x+w,y+h};HBRUSH brush=CreateSolidBrush(color);FillRect(dc,&r,brush);DeleteObject(brush);}
bool Inside(int x,int y,int w,int h){return px>=x&&px<x+w&&py>=y&&py<y+h;}
void Button(const char* label,int x,int y,int w,int h,bool selected=false,bool tab=false){
    Box(x,y,w,h,selected?RGB(24,108,132):Inside(x,y,w,h)?RGB(55,91,110):RGB(38,53,70));
    Text(label,x+(tab?8:14),y+(h-(tab?22:28))/2,w-(tab?12:24),tab?smallFont:normalFont,RGB(235,245,255));
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
    if(value>.5f&&o.toggle&&(!strcmp(o.key,"snap_turn")||!strcmp(o.key,"smooth_turn"))){
        const char* opposite=!strcmp(o.key,"snap_turn")?"smooth_turn":"snap_turn";
        for(unsigned j=0;j<count;++j)if(!strcmp(options[j].key,opposite)){
            values[j]=0;changed[j]=loaded[j]!=0;
        }
    }
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
    bool launchChange=false;
    for(unsigned i=0;i<count;++i)if(changed[i]&&(!strcmp(options[i].key,"resolution_mode")||!strcmp(options[i].key,"eye_resolution_percent")||!strcmp(options[i].key,"steam_launch")))launchChange=true;
    if(SaveLiveValues(values,changed)){
        for(unsigned i=0;i<count;++i){loaded[i]=values[i];changed[i]=false;}
        strcpy_s(status,launchChange?"Saved. Resolution: restart BF2 normally. Steam launch choice uses desktop Settings.":"Saved. Settings are now active.");Runtime::Haptic(1,.25f,40);
    }else strcpy_s(status,"Save failed; some changes may have saved. Check game folder write access.");
    dirty=true;
}
int PageParts(){
    if(page==9)return 1;
    int rows=0;for(unsigned i=0;i<count;++i)if(options[i].page==(unsigned)page)++rows;
    return std::max(1,(rows+rowsPerPage-1)/rowsPerPage);
}
void SlotClick(ULONGLONG now){
    if(Inside(24,142,165,50)){selectedSlot=(selectedSlot+6)%7;dirty=true;return;}
    if(Inside(835,142,165,50)){selectedSlot=(selectedSlot+1)%7;dirty=true;return;}
    for(int axis=0;axis<3;++axis){
        const int y=220+axis*120;
        if(Inside(560,y+12,140,52)||Inside(850,y+12,140,52)){
            repeat=slotOption[selectedSlot][axis];repeatDirection=px<710?-1:1;
            if(repeat>=0){Step((unsigned)repeat,repeatDirection);repeatAt=now+400;}
        }
    }
}
void DrawSlots(){
    Button("Previous",24,142,165,50);Button("Next",835,142,165,50);
    char label[128];sprintf_s(label,"%d / 7   %s",selectedSlot+1,slotNames[selectedSlot]);
    Text(label,214,153,615,normalFont,RGB(110,191,255));
    const char* names[]={"Horizontal position","Vertical position","Depth from body"};
    const char* minus[]={"Left","Down","Back"};const char* plus[]={"Right","Up","Forward"};
    for(int axis=0;axis<3;++axis){
        const int y=220+axis*120,index=slotOption[selectedSlot][axis];
        Box(24,y,976,106,RGB(22,34,47));Text(names[axis],36,y+12,495,normalFont,RGB(236,243,250));
        Text("1 cm per click. Hold the button to move further.",36,y+53,500,smallFont,RGB(171,192,207));
        Button(minus[axis],560,y+12,140,52);Button(plus[axis],850,y+12,140,52);
        if(index>=0){sprintf_s(label,"%+.0f cm%s",values[index]*100,changed[index]?" *":"");Text(label,712,y+27,135,normalFont,RGB(95,218,227));}
    }
    Text(slotPreviewVisible?"Blue bubble = selected grab point. Look down or over your shoulder to see it.":"Spawn on foot to preview the blue bubble and your kit. Offsets can be saved now.",24,586,976,smallFont,RGB(110,191,255));
    Text("Preview heading stays fixed while you look around. Apply saves all edited slots.",24,612,976,smallFont,RGB(171,192,207));
}
void UpdateSlotPreview(const Runtime::Frame& f){
    bool visible=false;
    if(page==9){
        float offsets[7][3]{},height=Tuning().slotHeight;
        for(unsigned i=0;i<count;++i)if(!strcmp(options[i].key,"hip_below_head"))height=values[i];
        for(int i=0;i<7;++i)for(int axis=0;axis<3;++axis)if(slotOption[i][axis]>=0)offsets[i][axis]=values[slotOption[i][axis]];
        visible=Engine::PreviewBodySlotLayout(f,offsets,height,selectedSlot);
    }else Engine::ClearBodySlotLayoutPreview();
    if(visible!=slotPreviewVisible){slotPreviewVisible=visible;dirty=true;}
}
void Click(ULONGLONG now){
    for(int i=0;i<pageCount;++i)if(Inside(24+i*tabStep,82,tabWidth,44)){page=i;pagePart=0;drag=repeat=-1;dirty=true;return;}
    if(PageParts()>1){
        if(pagePart>0&&Inside(680,616,145,30)){--pagePart;dirty=true;return;}
        if(pagePart+1<PageParts()&&Inside(842,616,145,30)){++pagePart;dirty=true;return;}
    }
    if(Inside(24,650,200,48)){Apply();return;}
    if(Inside(240,650,250,48)){
        if(page==9){for(int axis=0;axis<3;++axis){const int i=slotOption[selectedSlot][axis];if(i>=0)Change(i,options[i].initial);}}
        else for(unsigned i=0;i<count;++i)if(options[i].page==(unsigned)page)Change(i,options[i].initial);return;
    }
    if(Inside(780,650,220,48)){Close();return;}
    if(page==9){SlotClick(now);return;}
    int row=0;
    for(unsigned i=0;i<count;++i)if(options[i].page==(unsigned)page){
        const int visible=row++-pagePart*rowsPerPage;
        if(visible<0||visible>=rowsPerPage)continue;
        const int y=firstRow+visible*rowHeight;
        if(options[i].toggle){if(Inside(738,y+8,236,48))Step(i,1);}
        else if(Inside(690,y+8,60,48)||Inside(930,y+8,60,48)){
            repeat=(int)i;repeatDirection=px<800?-1:1;Step(i,repeatDirection);repeatAt=now+400;
        }else if(Inside(32,y+64,632,24)){drag=(int)i;Slide(i);}
    }
}
void Draw(){
    Box(0,0,width,height,RGB(15,24,35));Text("BATTLEFIELD 2 VR SETTINGS",24,20,760,titleFont,RGB(237,247,255));
    Text("r98",924,28,70,smallFont,RGB(135,174,196));
    for(int i=0;i<pageCount;++i)Button(pages[i],24+i*tabStep,82,tabWidth,44,page==i,true);
    int row=0;char value[80];
    if(page==9)DrawSlots();
    else for(unsigned i=0;i<count;++i)if(options[i].page==(unsigned)page){
        const int visible=row++-pagePart*rowsPerPage;
        if(visible<0||visible>=rowsPerPage)continue;
        const auto& o=options[i];const int y=firstRow+visible*rowHeight;
        Box(24,y,976,90,RGB(22,34,47));Text(o.label,36,y+6,642,normalFont,RGB(236,243,250));
        Text(o.help,36,y+38,646,smallFont,RGB(171,192,207));
        if(o.toggle){const bool on=values[i]>.5f;const char* label=!strcmp(o.key,"body_slots")?(on?"Physical gear":"Button cycling"):!strcmp(o.key,"hold_grip")?(on?"Hold grip":"Toggle grip"):(on?"On":"Off");Button(label,738,y+8,236,48,on);}
        else{
            Button("-",690,y+8,60,48);Button("+",930,y+8,60,48);
            if(!strcmp(o.key,"resolution_mode"))sprintf_s(value,"%s%s",values[i]<.5f?"Automatic":values[i]<1.5f?"Custom":"Game",changed[i]?" *":"");
            else sprintf_s(value,"%.2f%s",values[i],changed[i]?" *":"");Text(value,770,y+18,158,normalFont,RGB(95,218,227));
            const int end=32+(int)(632*(values[i]-o.minimum)/(o.maximum-o.minimum));
            Box(32,y+74,632,6,RGB(49,65,83));Box(32,y+74,end-32,6,RGB(46,179,199));Box(end-3,y+68,7,18,RGB(213,247,255));
        }
    }
    if(PageParts()>1){
        sprintf_s(value,"Page %d / %d",pagePart+1,PageParts());Text(value,440,620,200,smallFont,RGB(171,192,207));
        if(pagePart>0)Button("Previous",680,616,145,30);
        if(pagePart+1<PageParts())Button("Next",842,616,145,30);
    }
    Button("Apply",24,650,200,48,HasChanges());Button(page==9?"Reset selected slot":"Reset this tab",240,650,250,48);Button("Close + recenter",780,650,220,48);
    Text(status,24,714,974,smallFont,RGB(115,223,211));
    Text("Point + right trigger. B closes (unapplied changes discarded). Game continues while open.",24,752,976,smallFont,RGB(170,187,202));
    if(px>=0){Box(px-7,py-2,15,5,RGB(83,247,228));Box(px-2,py-7,5,15,RGB(83,247,228));}
    GdiFlush();
}
}
bool Active(){return active;}
bool Open(const Runtime::Frame& f){
    if(active)return true;options=LiveOptions(count);if(count>optionCapacity||!Canvas())return false;
    for(unsigned i=0;i<count;++i){values[i]=loaded[i]=LiveValue(i);changed[i]=false;}
    for(int i=0;i<7;++i)for(int axis=0;axis<3;++axis){
        slotOption[i][axis]=-1;char key[48];sprintf_s(key,"%s_%c",slotKeys[i],"xyz"[axis]);
        for(unsigned j=0;j<count;++j)if(!strcmp(options[j].section,"inventory_slots")&&!strcmp(options[j].key,key))slotOption[i][axis]=(int)j;
    }
    Engine::ClearBodySlotLayoutPreview();slotPreviewVisible=false;
    active=true;dirty=true;pagePart=0;Place(f);triggerWas=f.hands[1].trigger>.4f;closeWas=f.hands[1].secondary;
    chordArmed=false;chordWas=false;chordSince=lastDraw=0;frameId=0;drag=repeat=-1;px=py=-1;
    strcpy_s(status,"Choose settings, then Apply. Closing will recenter your view.");
    Log("SETTINGS44 headset menu opened");return true;
}
void Close(){
    if(!active)return;active=false;Engine::ClearBodySlotLayoutPreview();Runtime::HideSettingsOverlay();FreeCanvas();
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
        bool hovering=false;
        if(page==9){for(int axis=0;axis<3;++axis)if(slotOption[selectedSlot][axis]==repeat)
            hovering=Inside(repeatDirection<0?560:850,220+axis*120+12,140,52);}
        else hovering=Inside(repeatDirection<0?690:930,firstRow+(row-pagePart*rowsPerPage)*rowHeight+8,60,48);
        if(!hovering)repeat=-1;
        else if(f.tick>=repeatAt){Step((unsigned)repeat,repeatDirection);repeatAt=f.tick+90;}
    }
    if(!pressed||px<0){drag=repeat=-1;}triggerWas=pressed;
    UpdateSlotPreview(f);
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
