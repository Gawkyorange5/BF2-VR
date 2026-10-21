// r133: desktop input is confined to the focused game's physical client area
// while the VR cinema menu owns it. The render canvas may be much larger.
namespace WindowPointer {
using SetDpi=HANDLE(WINAPI*)(HANDLE);
struct PhysicalPixels {
    SetDpi set=nullptr;HANDLE previous=nullptr;
    PhysicalPixels(){
        set=reinterpret_cast<SetDpi>(GetProcAddress(GetModuleHandleA("user32.dll"),"SetThreadDpiAwarenessContext"));
        if(set){previous=set(reinterpret_cast<HANDLE>(-4));if(!previous)previous=set(reinterpret_cast<HANDLE>(-3));}
    }
    ~PhysicalPixels(){if(set&&previous)set(previous);}
};
HWND target=nullptr;bool confined=false,held=false,armed=false;RECT bounds{};
bool Focused(HWND w){return w&&IsWindow(w)&&!IsIconic(w)&&GetAncestor(GetForegroundWindow(),GA_ROOT)==GetAncestor(w,GA_ROOT);}
void Release(){
    PhysicalPixels dpi;
    if(held){INPUT e{};e.type=INPUT_MOUSE;e.mi.dwFlags=MOUSEEVENTF_LEFTUP;SendInput(1,&e,sizeof(e));}
    // Do not remove a clip installed by another application after Alt-Tab.
    RECT current{};if(confined&&GetClipCursor(&current)&&EqualRect(&current,&bounds))ClipCursor(nullptr);
    held=armed=confined=false;target=nullptr;bounds={};
}
bool Update(HWND w,bool valid,float u,float v,bool pressed){
    PhysicalPixels dpi;
    if(target&&target!=w)Release();
    if(!Focused(w)){Release();return false;}
    RECT client{};POINT corners[2]{};
    if(!GetClientRect(w,&client)||client.right<8||client.bottom<8){Release();return false;}
    corners[1]={client.right,client.bottom};
    if(!ClientToScreen(w,&corners[0])||!ClientToScreen(w,&corners[1])){Release();return false;}
    const LONG sx=GetSystemMetrics(SM_XVIRTUALSCREEN),sy=GetSystemMetrics(SM_YVIRTUALSCREEN);
    const LONG sw=GetSystemMetrics(SM_CXVIRTUALSCREEN),sh=GetSystemMetrics(SM_CYVIRTUALSCREEN);
    if(sw<=1||sh<=1){Release();return false;}
    RECT physical{corners[0].x+2,corners[0].y+2,corners[1].x-2,corners[1].y-2};
    RECT desktop{sx,sy,sx+sw,sy+sh},clip{};
    if(!IntersectRect(&clip,&physical,&desktop)||clip.right-clip.left<4||clip.bottom-clip.top<4){Release();return false;}
    const bool changed=!confined||!EqualRect(&bounds,&clip);
    if(changed&&confined)Release();
    // Reassert each input frame: BF2 can otherwise warp to render-sized menu
    // coordinates, beyond the smaller desktop preview, between hover/click.
    if(!ClipCursor(&clip)){Release();return false;}
    target=w;bounds=clip;confined=true;
    if(changed)Log("POINTER133 menu client=%ldx%ld desktop=(%ld,%ld)-(%ld,%ld); confined physical-pixel clicks",
        corners[1].x-corners[0].x,corners[1].y-corners[0].y,clip.left,clip.top,clip.right,clip.bottom);
    if(!pressed)armed=true;
    valid=valid&&std::isfinite(u+v)&&u>=0&&u<=1&&v>=0&&v<=1;
    POINT point{};
    if(valid){
        point={corners[0].x+(LONG)std::lround(u*(corners[1].x-corners[0].x-1)),
               corners[0].y+(LONG)std::lround(v*(corners[1].y-corners[0].y-1))};
        point.x=(std::max)(clip.left,(std::min)(clip.right-1,point.x));
        point.y=(std::max)(clip.top,(std::min)(clip.bottom-1,point.y));
        const HWND under=WindowFromPoint(point);
        valid=under&&GetAncestor(under,GA_ROOT)==GetAncestor(w,GA_ROOT)&&SetCursorPos(point.x,point.y);
    }
    if(!valid){
        // Never create a click on re-entering the quad with a held trigger.
        if(pressed)armed=false;
        if(held){INPUT e{};e.type=INPUT_MOUSE;e.mi.dwFlags=MOUSEEVENTF_LEFTUP;SendInput(1,&e,sizeof(e));held=false;}
        return false;
    }
    const bool down=pressed&&armed;
    if(down!=held){
        if(!Focused(w)){Release();return false;}
        INPUT e{};e.type=INPUT_MOUSE;
        e.mi.dx=(LONG)(((double)(point.x-sx)+.5)*65536.0/sw);
        e.mi.dy=(LONG)(((double)(point.y-sy)+.5)*65536.0/sh);
        e.mi.dwFlags=MOUSEEVENTF_MOVE|MOUSEEVENTF_ABSOLUTE|MOUSEEVENTF_VIRTUALDESK|
            (down?MOUSEEVENTF_LEFTDOWN:MOUSEEVENTF_LEFTUP);
        // Position and button edge travel in the SAME event. A native cursor
        // warp between SetCursorPos and SendInput cannot redirect the press.
        if(SendInput(1,&e,sizeof(e)))held=down;
        else{Release();return false;}
    }
    return true;
}
}
