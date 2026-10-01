#include "RenderWindow.h"
#include "AutoResolution.h"
#include "VrSettings.h"
#include <algorithm>
#include <cmath>

namespace CleanVR { namespace RenderWindow { namespace {
HWND mainWindow=nullptr;
WNDPROC original=nullptr;
unsigned renderW=0,renderH=0;
bool compact=false;
constexpr double previewAspect=16.0/9.0;

RECT PreviewRect(HWND window){
    MONITORINFO monitor{};monitor.cbSize=sizeof(monitor);
    RECT work{0,0,GetSystemMetrics(SM_CXSCREEN),GetSystemMetrics(SM_CYSCREEN)};
    if(GetMonitorInfoA(MonitorFromWindow(window,MONITOR_DEFAULTTONEAREST),&monitor))work=monitor.rcWork;
    RECT border{};
    AdjustWindowRectEx(&border,(DWORD)GetWindowLongPtrA(window,GWL_STYLE),GetMenu(window)!=nullptr,
        (DWORD)GetWindowLongPtrA(window,GWL_EXSTYLE));
    const LONG frameW=border.right-border.left,frameH=border.bottom-border.top;
    const LONG availableW=(std::max)(64L,work.right-work.left-frameW-16);
    const LONG availableH=(std::max)(64L,work.bottom-work.top-frameH-16);
    // The monitor preview has its own aspect, independent of one headset eye.
    // Native WM_SIZE and D3D resources still use the full renderW/renderH.
    double clientH=(std::min)(double(availableH),double(availableW)/previewAspect);
    if(compact)clientH*=0.65;
    const LONG w=(std::max)(1L,(LONG)std::floor(clientH*previewAspect))+frameW;
    const LONG h=(std::max)(1L,(LONG)std::floor(clientH))+frameH;
    const LONG x=work.left+(work.right-work.left-w)/2,y=work.top+(work.bottom-work.top-h)/2;
    return RECT{x,y,x+w,y+h};
}
void Fit(){
    if(!mainWindow||IsIconic(mainWindow))return;
    const RECT r=PreviewRect(mainWindow);
    SetWindowPos(mainWindow,nullptr,r.left,r.top,r.right-r.left,r.bottom-r.top,
        SWP_NOZORDER|SWP_NOACTIVATE|SWP_FRAMECHANGED);
}
bool ClientMouse(UINT message){
    switch(message){
    case WM_MOUSEMOVE:case WM_LBUTTONDOWN:case WM_LBUTTONUP:case WM_LBUTTONDBLCLK:
    case WM_RBUTTONDOWN:case WM_RBUTTONUP:case WM_RBUTTONDBLCLK:
    case WM_MBUTTONDOWN:case WM_MBUTTONUP:case WM_MBUTTONDBLCLK:
    case WM_XBUTTONDOWN:case WM_XBUTTONUP:case WM_XBUTTONDBLCLK:return true;
    default:return false; // Wheel and non-client messages contain screen coordinates.
    }
}
LRESULT CALLBACK WindowProc(HWND window,UINT message,WPARAM w,LPARAM l){
    const WNDPROC next=original;
    if(window!=mainWindow||!next)return DefWindowProcA(window,message,w,l);
    if(message==WM_WINDOWPOSCHANGING&&l&&!IsIconic(window)){
        auto* position=reinterpret_cast<WINDOWPOS*>(l);
        if(!(position->flags&SWP_NOSIZE)&&position->cx>0&&position->cy>0){
            const RECT fit=PreviewRect(window);
            position->cx=fit.right-fit.left;position->cy=fit.bottom-fit.top;
            if(!(position->flags&SWP_NOMOVE)){position->x=fit.left;position->y=fit.top;}
        }
    }
    // BF2 1.5's native WM_SIZE handler stores LOWORD/HIWORD in its screen
    // dimensions. Feed the render canvas, never the smaller desktop preview.
    // Keep minimized notifications intact for the native focus/lost-device path.
    if(message==WM_SIZE&&w!=SIZE_MINIMIZED)l=MAKELPARAM(renderW,renderH);
    if(ClientMouse(message)){
        RECT client{};
        if(GetClientRect(window,&client)&&client.right>1&&client.bottom>1){
            const int x=(short)LOWORD(l),y=(short)HIWORD(l);
            const int rx=MulDiv(x,(int)renderW-1,client.right-1);
            const int ry=MulDiv(y,(int)renderH-1,client.bottom-1);
            l=MAKELPARAM(rx,ry);
        }
    }
    const LRESULT result=CallWindowProcA(next,window,message,w,l);
    if(message==WM_STYLECHANGED||message==WM_DISPLAYCHANGE)Fit();
    if(message==WM_NCDESTROY){
        SetWindowLongPtrA(window,GWLP_WNDPROC,reinterpret_cast<LONG_PTR>(next));
        mainWindow=nullptr;original=nullptr;renderW=renderH=0;
    }
    return result;
}
} // namespace
bool Attach(HWND window){
    if(!window||mainWindow||!AutoResolution::Requested()||GetParent(window))return false;
    AutoResolution::Size(renderW,renderH);
    SetLastError(0);
    const LONG_PTR previous=SetWindowLongPtrA(window,GWLP_WNDPROC,reinterpret_cast<LONG_PTR>(&WindowProc));
    if(!previous){Log("RESOLUTION60 preview window subclass failed error=%lu",GetLastError());return false;}
    mainWindow=window;original=reinterpret_cast<WNDPROC>(previous);
    Fit();
    // WM_SIZE during CreateWindow preceded the subclass. Publish the logical
    // size once now, even if fitting the desktop produced no physical resize.
    SendMessageA(window,WM_SIZE,SIZE_RESTORED,MAKELPARAM(renderW,renderH));
    RECT client{};GetClientRect(window,&client);
    Log("PREVIEW61 render canvas=%ux%u widescreen desktop=%ldx%ld; menus fit full image, stereo mirror cropped; client mouse coordinates mapped",renderW,renderH,client.right,client.bottom);
    return true;
}
bool TogglePreview(){if(!mainWindow)return false;compact=!compact;Fit();return true;}
float DesktopAspect(){return mainWindow?(float)previewAspect:0;}
bool CanvasSize(HWND window,unsigned& width,unsigned& height){
    if(!mainWindow||window!=mainWindow)return false;
    width=renderW;height=renderH;return true;
}
void ConfigurePresentation(D3DPRESENT_PARAMETERS* p,HWND focus){
    if(!p||!mainWindow)return;
    const HWND target=p->hDeviceWindow?p->hDeviceWindow:(focus?focus:mainWindow);
    if(target!=mainWindow)return;
    // A zero windowed dimension makes D3D9 use the physical client size. BF2's
    // startup and WM_SIZE canvas already use renderW/H; bind that same size on
    // both CreateDevice and Reset, including the automatically created depth.
    Log("RESOLUTION60 device request=%ux%u explicit=%ux%u samples=%u",p->BackBufferWidth,p->BackBufferHeight,renderW,renderH,(unsigned)p->MultiSampleType);
    p->BackBufferWidth=renderW;p->BackBufferHeight=renderH;
    p->Windowed=TRUE;p->FullScreen_RefreshRateInHz=0;
}
} }
