#include "XrSessionClient.h"
#include "VrSettings.h"
#include "FrameTiming.h"
#include <d3d11.h>
#include <dxgi.h>
#define XR_USE_PLATFORM_WIN32
#define XR_USE_GRAPHICS_API_D3D11
#include <openxr/openxr_platform.h>
#include <vector>
#include <algorithm>
#include <cstring>
#include <cmath>
#include <cstdio>

namespace CleanVR { namespace Runtime { namespace {
#define XR_CALLS(X) \
 X(xrDestroyInstance) X(xrGetSystem) X(xrGetSystemProperties) X(xrGetInstanceProperties) X(xrPollEvent) \
 X(xrGetD3D11GraphicsRequirementsKHR) X(xrCreateSession) X(xrDestroySession) \
 X(xrBeginSession) X(xrEndSession) X(xrCreateReferenceSpace) X(xrDestroySpace) \
 X(xrEnumerateViewConfigurationViews) X(xrEnumerateEnvironmentBlendModes) \
 X(xrEnumerateSwapchainFormats) X(xrCreateSwapchain) X(xrDestroySwapchain) \
 X(xrEnumerateSwapchainImages) X(xrAcquireSwapchainImage) X(xrWaitSwapchainImage) X(xrReleaseSwapchainImage) \
 X(xrWaitFrame) X(xrBeginFrame) X(xrEndFrame) X(xrLocateViews) X(xrLocateSpace) \
 X(xrStringToPath) X(xrPathToString) X(xrGetCurrentInteractionProfile) X(xrCreateActionSet) X(xrDestroyActionSet) X(xrCreateAction) \
 X(xrSuggestInteractionProfileBindings) X(xrAttachSessionActionSets) X(xrCreateActionSpace) X(xrSyncActions) \
 X(xrGetActionStateFloat) X(xrGetActionStateBoolean) X(xrGetActionStateVector2f) X(xrGetActionStatePose)
#define DECLARE(name) PFN_##name name=nullptr;
XR_CALLS(DECLARE)
#undef DECLARE
PFN_xrGetInstanceProcAddr getProc=nullptr;
PFN_xrApplyHapticFeedback applyHaptic=nullptr;
HMODULE loader=nullptr;XrInstance instance=XR_NULL_HANDLE;XrSystemId systemId=XR_NULL_SYSTEM_ID;
XrSession session=XR_NULL_HANDLE;XrSpace localSpace=XR_NULL_HANDLE;XrSessionState state=XR_SESSION_STATE_UNKNOWN;
bool initialized=false,running=false,open=false,available=false;std::uint64_t serial=0;
std::uint64_t originSerial=0;XrTime originChangeTime=0;
XrEnvironmentBlendMode blend=XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
ID3D11Device* device=nullptr;ID3D11DeviceContext* context=nullptr;
bool unifiedMemory=false;DXGI_ADAPTER_DESC1 graphicsAdapter{};
XrActionSet actionSet=XR_NULL_HANDLE;XrPath handPaths[2]{};
XrAction aimAction{},gripAction{},triggerAction{},squeezeAction{},stickAction{},clickAction{},primaryAction{},secondaryAction{},menuAction{};
XrAction hapticAction{};bool hapticBound=false;
XrSpace aimSpace[2]{},gripSpace[2]{};
Frame current{};SRWLOCK frameLock=SRWLOCK_INIT;
struct Chain {
    XrSwapchain handle=XR_NULL_HANDLE;unsigned width=0,height=0;
    std::vector<XrSwapchainImageD3D11KHR> images;std::vector<unsigned char> rgba;
    bool uploaded=false,acquired=false,waited=false;uint32_t image=0;
} chains[5];
unsigned maxLayers=2;
struct SettingsLayer {bool visible=false,valid=false;XrPosef pose{};float width=0,height=0;std::uint64_t origin=0;ULONGLONG updated=0;} settingsLayer;
ID3D11Texture2D* transferAtlas=nullptr;unsigned atlasWidth=0,atlasHeight=0;
bool atlasDynamic=false,dynamicUnavailable=false;
XrViewConfigurationView eyeRecommendations[2]{};
unsigned lastNativeWidth=0,lastNativeHeight=0;
TransferTiming transferTiming{};
INIT_ONCE gateOnce=INIT_ONCE_STATIC_INIT;CRITICAL_SECTION gate;
BOOL CALLBACK InitGate(PINIT_ONCE,PVOID,PVOID*){InitializeCriticalSection(&gate);return TRUE;}
HANDLE keeperStop=nullptr,keeperThread=nullptr;
struct RetainedQuad {bool valid=false;XrPosef pose{};float width=0,height=0;std::uint64_t origin=0;} retained;
bool lastWasCinema=false;ULONGLONG lastGameSubmission=0;
DXGI_FORMAT format=DXGI_FORMAT_UNKNOWN;
bool Check(XrResult r,const char* what){if(XR_FAILED(r)){Log("OpenXR %s failed (%d)",what,(int)r);return false;}return true;}
bool AcquireImage(Chain& c){
    c.uploaded=false;
    // A failed wait/release may leave an acquisition outstanding. Retry that
    // ownership operation, never pile up a second acquire on the same chain.
    if(!c.acquired){
        XrSwapchainImageAcquireInfo ai{XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO};
        if(!Check(xrAcquireSwapchainImage(c.handle,&ai,&c.image),"acquire image"))return false;
        c.acquired=true;c.waited=false;
    }
    if(!c.waited){
        XrSwapchainImageWaitInfo wi{XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};wi.timeout=XR_INFINITE_DURATION;
        const XrResult result=xrWaitSwapchainImage(c.handle,&wi);
        if(!Check(result,"wait image")||result==XR_TIMEOUT_EXPIRED)return false;
        c.waited=true;
    }
    if(c.image>=c.images.size()||!c.images[c.image].texture){Log("OpenXR acquired image index/texture invalid");return false;}
    return true;
}
bool ReleaseImage(Chain& c,bool copied){
    c.uploaded=false;if(!c.acquired||!c.waited)return false;
    XrSwapchainImageReleaseInfo ri{XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};
    if(!Check(xrReleaseSwapchainImage(c.handle,&ri),"release image"))return false;
    c.acquired=c.waited=false;c.uploaded=copied;return true;
}
XrPath Path(const char* name){XrPath p=XR_NULL_PATH;return Check(xrStringToPath(instance,name,&p),"path")?p:XR_NULL_PATH;}
Mat Matrix(const XrPosef& p){return FromOpenXR({p.position.x,p.position.y,p.position.z},{p.orientation.x,p.orientation.y,p.orientation.z,p.orientation.w});}
void CloseFrame(){AcquireSRWLockExclusive(&frameLock);open=false;ReleaseSRWLockExclusive(&frameLock);}
bool MakeChain(Chain& c,unsigned w,unsigned h){
    if(c.handle&&c.width==w&&c.height==h)return true;
    if(&c==&chains[2])retained.valid=false;c.uploaded=false;
    if(c.handle){xrDestroySwapchain(c.handle);c.handle=XR_NULL_HANDLE;c.images.clear();}c.acquired=c.waited=false;
    XrSwapchainCreateInfo ci{XR_TYPE_SWAPCHAIN_CREATE_INFO};ci.usageFlags=XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT|XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT|XR_SWAPCHAIN_USAGE_SAMPLED_BIT;
    ci.format=format;ci.sampleCount=1;ci.width=w;ci.height=h;ci.faceCount=1;ci.arraySize=1;ci.mipCount=1;
    if(!Check(xrCreateSwapchain(session,&ci,&c.handle),"create swapchain"))return false;
    uint32_t n=0;if(!Check(xrEnumerateSwapchainImages(c.handle,0,&n,nullptr),"image count")||!n)return false;
    c.images.assign(n,{XR_TYPE_SWAPCHAIN_IMAGE_D3D11_KHR});
    if(!Check(xrEnumerateSwapchainImages(c.handle,n,&n,reinterpret_cast<XrSwapchainImageBaseHeader*>(c.images.data())),"images"))return false;
    c.width=w;c.height=h;Log("OpenXR swapchain %ux%u format=%d images=%u",w,h,(int)format,n);return true;
}
bool CreateGraphics(const XrGraphicsRequirementsD3D11KHR& requirements){
    unifiedMemory=false;graphicsAdapter={};
    IDXGIFactory1* factory=nullptr;if(FAILED(CreateDXGIFactory1(__uuidof(IDXGIFactory1),(void**)&factory)))return false;
    IDXGIAdapter1* chosen=nullptr;
    for(unsigned i=0;;++i){IDXGIAdapter1* a=nullptr;if(factory->EnumAdapters1(i,&a)==DXGI_ERROR_NOT_FOUND)break;if(!a)break;
        DXGI_ADAPTER_DESC1 d{};a->GetDesc1(&d);if(d.AdapterLuid.HighPart==requirements.adapterLuid.HighPart&&d.AdapterLuid.LowPart==requirements.adapterLuid.LowPart){chosen=a;break;}a->Release();}
    factory->Release();if(!chosen){Log("OpenXR runtime adapter not found");return false;}
    DXGI_ADAPTER_DESC1 adapter{};if(SUCCEEDED(chosen->GetDesc1(&adapter)))Log("PERF GPU=%ls dedicatedMB=%llu",adapter.Description,(unsigned long long)(adapter.DedicatedVideoMemory/(1024*1024)));
    const D3D_FEATURE_LEVEL options[]={D3D_FEATURE_LEVEL_11_1,D3D_FEATURE_LEVEL_11_0,D3D_FEATURE_LEVEL_10_1,D3D_FEATURE_LEVEL_10_0};
    std::vector<D3D_FEATURE_LEVEL> levels;for(auto f:options)if(f>=requirements.minFeatureLevel)levels.push_back(f);
    D3D_FEATURE_LEVEL used{};HRESULT hr=levels.empty()?E_FAIL:D3D11CreateDevice(chosen,D3D_DRIVER_TYPE_UNKNOWN,nullptr,D3D11_CREATE_DEVICE_BGRA_SUPPORT,levels.data(),(UINT)levels.size(),D3D11_SDK_VERSION,&device,&used,&context);
    if(SUCCEEDED(hr)){
        D3D11_FEATURE_DATA_D3D11_OPTIONS2 options2{};
        const HRESULT capability=device->CheckFeatureSupport(D3D11_FEATURE_D3D11_OPTIONS2,&options2,sizeof(options2));
        unifiedMemory=SUCCEEDED(capability)&&options2.UnifiedMemoryArchitecture!=FALSE;
        graphicsAdapter=adapter;
        Log("PERF adapter UMA=%d featureQuery=%08X vendor=%04X device=%04X",unifiedMemory?1:0,(unsigned)capability,adapter.VendorId,adapter.DeviceId);
    }
    chosen->Release();Log("D3D11 runtime adapter=%08X:%08X device=%08X level=%X",requirements.adapterLuid.HighPart,requirements.adapterLuid.LowPart,(unsigned)hr,(unsigned)used);return SUCCEEDED(hr);
}
bool Action(const char* name,XrActionType type,XrAction& out,bool both=true){
    XrActionCreateInfo ci{XR_TYPE_ACTION_CREATE_INFO};strcpy_s(ci.actionName,name);strcpy_s(ci.localizedActionName,name);ci.actionType=type;
    if(both){ci.countSubactionPaths=2;ci.subactionPaths=handPaths;}
    return Check(xrCreateAction(actionSet,&ci,&out),name);
}
bool SetupInput(){
    XrActionSetCreateInfo si{XR_TYPE_ACTION_SET_CREATE_INFO};strcpy_s(si.actionSetName,"bf2_gameplay");strcpy_s(si.localizedActionSetName,"Battlefield 2");
    if(!Check(xrCreateActionSet(instance,&si,&actionSet),"actions"))return false;
    handPaths[0]=Path("/user/hand/left");handPaths[1]=Path("/user/hand/right");
    if(!Action("aim",XR_ACTION_TYPE_POSE_INPUT,aimAction)||!Action("grip_pose",XR_ACTION_TYPE_POSE_INPUT,gripAction)||
       !Action("trigger",XR_ACTION_TYPE_FLOAT_INPUT,triggerAction)||!Action("squeeze",XR_ACTION_TYPE_FLOAT_INPUT,squeezeAction)||
       !Action("stick",XR_ACTION_TYPE_VECTOR2F_INPUT,stickAction)||!Action("stick_click",XR_ACTION_TYPE_BOOLEAN_INPUT,clickAction)||
       !Action("primary_button",XR_ACTION_TYPE_BOOLEAN_INPUT,primaryAction)||!Action("secondary_button",XR_ACTION_TYPE_BOOLEAN_INPUT,secondaryAction)||!Action("menu",XR_ACTION_TYPE_BOOLEAN_INPUT,menuAction,false))return false;
    std::vector<XrActionSuggestedBinding> bindings;
    const auto bind=[&](XrAction action,const char* hand,const char* part){char name[160];sprintf_s(name,"/user/hand/%s/input/%s",hand,part);bindings.push_back({action,Path(name)});};
    for(const char* hand:{"left","right"}){bind(aimAction,hand,"aim/pose");bind(gripAction,hand,"grip/pose");bind(triggerAction,hand,"trigger/value");bind(squeezeAction,hand,"squeeze/value");bind(stickAction,hand,"thumbstick");bind(clickAction,hand,"thumbstick/click");}
    bind(primaryAction,"left","x/click");bind(primaryAction,"right","a/click");
    bind(secondaryAction,"left","y/click");bind(secondaryAction,"right","b/click");bind(menuAction,"left","menu/click");
    const size_t inputBindingCount=bindings.size();
    if(applyHaptic&&Action("reload_feedback",XR_ACTION_TYPE_VIBRATION_OUTPUT,hapticAction)){
        bindings.push_back({hapticAction,Path("/user/hand/left/output/haptic")});
        bindings.push_back({hapticAction,Path("/user/hand/right/output/haptic")});
    }
    XrInteractionProfileSuggestedBinding sb{XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING};sb.interactionProfile=Path("/interaction_profiles/oculus/touch_controller");sb.countSuggestedBindings=(uint32_t)bindings.size();sb.suggestedBindings=bindings.data();
    XrResult bindingResult=xrSuggestInteractionProfileBindings(instance,&sb);
    hapticBound=XR_SUCCEEDED(bindingResult)&&bindings.size()>inputBindingCount;
    if(XR_FAILED(bindingResult)&&bindings.size()>inputBindingCount){
        Log("OpenXR optional reload haptics unavailable (%d); retrying existing controller bindings",(int)bindingResult);
        sb.countSuggestedBindings=(uint32_t)inputBindingCount;bindingResult=xrSuggestInteractionProfileBindings(instance,&sb);
    }
    if(!Check(bindingResult,"Touch bindings"))return false;
    Log("OpenXR reload haptics=%d",hapticBound?1:0);
    XrSessionActionSetsAttachInfo ai{XR_TYPE_SESSION_ACTION_SETS_ATTACH_INFO};ai.countActionSets=1;ai.actionSets=&actionSet;
    if(!Check(xrAttachSessionActionSets(session,&ai),"attach actions"))return false;
    for(int h=0;h<2;++h){XrActionSpaceCreateInfo as{XR_TYPE_ACTION_SPACE_CREATE_INFO};as.subactionPath=handPaths[h];as.poseInActionSpace.orientation.w=1;as.action=aimAction;
        if(!Check(xrCreateActionSpace(session,&as,&aimSpace[h]),"aim space"))return false;as.action=gripAction;if(!Check(xrCreateActionSpace(session,&as,&gripSpace[h]),"grip space"))return false;}
    return true;
}
void Poll(){
    if(!instance)return;XrEventDataBuffer ev{XR_TYPE_EVENT_DATA_BUFFER};
    while(xrPollEvent(instance,&ev)==XR_SUCCESS){
        if(ev.type==XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED){const auto& change=*reinterpret_cast<XrEventDataSessionStateChanged*>(&ev);state=change.state;Log("OpenXR session state=%d",(int)state);
            if(state==XR_SESSION_STATE_READY&&!running){XrSessionBeginInfo bi{XR_TYPE_SESSION_BEGIN_INFO};bi.primaryViewConfigurationType=XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;running=Check(xrBeginSession(session,&bi),"begin session");}
            if(state==XR_SESSION_STATE_STOPPING){if(open)SubmitEmpty();if(running)xrEndSession(session);running=false;}
            if(state==XR_SESSION_STATE_EXITING||state==XR_SESSION_STATE_LOSS_PENDING){if(open)SubmitEmpty();running=false;available=false;}
        }
        if(ev.type==XR_TYPE_EVENT_DATA_REFERENCE_SPACE_CHANGE_PENDING){const auto& change=*reinterpret_cast<XrEventDataReferenceSpaceChangePending*>(&ev);if(change.referenceSpaceType==XR_REFERENCE_SPACE_TYPE_LOCAL)originChangeTime=change.changeTime;}
        ev={XR_TYPE_EVENT_DATA_BUFFER};
    }
}
float FloatState(XrAction a,int h){XrActionStateGetInfo gi{XR_TYPE_ACTION_STATE_GET_INFO};gi.action=a;gi.subactionPath=handPaths[h];XrActionStateFloat s{XR_TYPE_ACTION_STATE_FLOAT};return XR_SUCCEEDED(xrGetActionStateFloat(session,&gi,&s))&&s.isActive?s.currentState:0;}
bool BoolState(XrAction a,XrPath hand){XrActionStateGetInfo gi{XR_TYPE_ACTION_STATE_GET_INFO};gi.action=a;gi.subactionPath=hand;XrActionStateBoolean s{XR_TYPE_ACTION_STATE_BOOLEAN};return XR_SUCCEEDED(xrGetActionStateBoolean(session,&gi,&s))&&s.isActive&&s.currentState;}
bool PoseState(XrAction a,XrSpace space,int h,XrTime time,Mat& out){
    XrActionStateGetInfo gi{XR_TYPE_ACTION_STATE_GET_INFO};gi.action=a;gi.subactionPath=handPaths[h];XrActionStatePose active{XR_TYPE_ACTION_STATE_POSE};
    const XrResult actionResult=xrGetActionStatePose(session,&gi,&active);
    XrSpaceLocation loc{XR_TYPE_SPACE_LOCATION};XrResult locationResult=XR_ERROR_POSE_INVALID;
    if(XR_SUCCEEDED(actionResult)&&active.isActive)locationResult=xrLocateSpace(space,localSpace,time,&loc);
    // VALID is the OpenXR contract for a usable pose. TRACKED may be unset
    // during runtime inference; it is diagnostic, not a second validity test.
    constexpr XrSpaceLocationFlags required=XR_SPACE_LOCATION_POSITION_VALID_BIT|XR_SPACE_LOCATION_ORIENTATION_VALID_BIT;
    const bool usable=XR_SUCCEEDED(actionResult)&&active.isActive&&XR_SUCCEEDED(locationResult)&&(loc.locationFlags&required)==required;
    struct Status{XrResult action=XR_ERROR_RUNTIME_FAILURE,locate=XR_ERROR_RUNTIME_FAILURE;XrBool32 active=0;XrSpaceLocationFlags flags=~0ull;ULONGLONG tick=0;};
    static Status last[2][2];auto& s=last[h][a==gripAction?1:0];const auto now=GetTickCount64();
    const bool changed=s.action!=actionResult||s.locate!=locationResult||s.active!=active.isActive||s.flags!=loc.locationFlags;
    if((changed&&now-s.tick>500)||now-s.tick>30000){Log("ACTION %s %s active=%u flags=0x%llX action=%d locate=%d usable=%d",h?"right":"left",a==gripAction?"grip":"aim",active.isActive,(unsigned long long)loc.locationFlags,(int)actionResult,(int)locationResult,usable?1:0);s={actionResult,locationResult,active.isActive,loc.locationFlags,now};}
    if(!usable)return false;out=Matrix(loc.pose);return Rigid(out);
}
bool Finish(const XrCompositionLayerBaseHeader* layer,const XrCompositionLayerBaseHeader* hud=nullptr){
    if(!open)return false;
    const bool menu=settingsLayer.visible&&settingsLayer.valid&&current.valid&&current.focused&&current.shouldRender&&
        settingsLayer.origin==current.origin&&GetTickCount64()-settingsLayer.updated<500;
    XrCompositionLayerQuad overlay{XR_TYPE_COMPOSITION_LAYER_QUAD};
    const XrCompositionLayerBaseHeader* layers[3]{};unsigned n=0;
    if(layer)layers[n++]=layer;
    if(layer&&hud&&n+(menu?1u:0u)<maxLayers)layers[n++]=hud;
    if(menu&&n<maxLayers){
        overlay.space=localSpace;overlay.eyeVisibility=XR_EYE_VISIBILITY_BOTH;
        overlay.pose=settingsLayer.pose;overlay.size={settingsLayer.width,settingsLayer.height};
        overlay.subImage.swapchain=chains[4].handle;overlay.subImage.imageRect.extent={(int)chains[4].width,(int)chains[4].height};
        layers[n++]=reinterpret_cast<const XrCompositionLayerBaseHeader*>(&overlay);
    }
    XrFrameEndInfo ei{XR_TYPE_FRAME_END_INFO};ei.displayTime=current.time;ei.environmentBlendMode=blend;ei.layerCount=n;ei.layers=n?layers:nullptr;
    const bool ok=Check(xrEndFrame(session,&ei),"end frame");
    static int lastLayer=-1;static unsigned reports=0;const int kind=layer?(layer->type==XR_TYPE_COMPOSITION_LAYER_QUAD?1:2):0;
    if(reports<6||kind!=lastLayer||current.id%1800==1){Log("SUBMIT frame=%llu layer=%s shouldRender=%d result=%d",current.id,kind==1?"cinema":kind==2?"stereo":"empty",current.shouldRender?1:0,ok?1:0);++reports;lastLayer=kind;}
    CloseFrame();for(auto& c:chains)c.uploaded=false;return ok;
}
DWORD WINAPI KeepCinema(LPVOID){
    unsigned held=0;bool keeping=false;
    while(WaitForSingleObject(keeperStop,20)==WAIT_TIMEOUT){
        Guard lock(true);if(!lock.Acquired())continue;
        if(!available||!running||!lastWasCinema||!retained.valid||GetTickCount64()-lastGameSubmission<150){
            if(keeping){Log("LOADING keeper returned to game presentation after %u frames",held);keeping=false;}continue;
        }
        if(!keeping){keeping=true;held=0;Log("LOADING keeper retaining last cinema image while native rendering is idle");}
        // Serialize wait/begin/end with every game submission. No D3D9, game
        // memory reads or injected inputs on this thread. The released XR
        // image can be referenced by subsequent xrEndFrame calls unchanged.
        if(Begin()){SubmitRetained();++held;Begin();}
    }
    return 0;
}
}
Guard::Guard(bool tryOnly){InitOnceExecuteOnce(&gateOnce,InitGate,nullptr,nullptr);if(tryOnly)acquired=TryEnterCriticalSection(&gate)!=0;else{EnterCriticalSection(&gate);acquired=true;}}
void Guard::Unlock(){if(acquired){LeaveCriticalSection(&gate);acquired=false;}}
Guard::~Guard(){Unlock();}
bool Initialize(){
    Guard lock;
    if(initialized)return available;initialized=true;if(!Config().enabled)return false;
    Log("CLEAN-2142-VR 62: crosshair OFF also hides fixed infantry reticle; headset settings menu and live turn speed; both-stick hold VR settings, tap recenter; left-menu tap kit, hold pause; r43 aircraft, r41 scopes and AK rollback; accepted r38 crosshair/r37 controls/r36 smoothing/r34 aiming/r30 renderer, OpenXR");
    char path[MAX_PATH];sprintf_s(path,"%sopenxr_loader.dll",GamePath());loader=LoadLibraryExA(path,nullptr,LOAD_WITH_ALTERED_SEARCH_PATH);
    if(!loader){Log("32-bit openxr_loader.dll load failed %lu",GetLastError());return false;}
    getProc=reinterpret_cast<PFN_xrGetInstanceProcAddr>(GetProcAddress(loader,"xrGetInstanceProcAddr"));if(!getProc)return false;
    PFN_xrCreateInstance create=nullptr;if(XR_FAILED(getProc(XR_NULL_HANDLE,"xrCreateInstance",reinterpret_cast<PFN_xrVoidFunction*>(&create)))||!create)return false;
    const char* extension=XR_KHR_D3D11_ENABLE_EXTENSION_NAME;XrInstanceCreateInfo ci{XR_TYPE_INSTANCE_CREATE_INFO};strcpy_s(ci.applicationInfo.applicationName,"BF2 Clean OpenXR");ci.applicationInfo.applicationVersion=1;ci.applicationInfo.apiVersion=XR_MAKE_VERSION(1,0,0);ci.enabledExtensionCount=1;ci.enabledExtensionNames=&extension;
    if(!Check(create(&ci,&instance),"instance"))return false;
#define LOAD(name) if(XR_FAILED(getProc(instance,#name,reinterpret_cast<PFN_xrVoidFunction*>(&name)))||!name){Log("Missing OpenXR function %s",#name);return false;}
    XR_CALLS(LOAD)
#undef LOAD
    getProc(instance,"xrApplyHapticFeedback",reinterpret_cast<PFN_xrVoidFunction*>(&applyHaptic));
    XrInstanceProperties properties{XR_TYPE_INSTANCE_PROPERTIES};xrGetInstanceProperties(instance,&properties);Log("OpenXR runtime=%s version=%llu",properties.runtimeName,(unsigned long long)properties.runtimeVersion);
    XrSystemGetInfo gi{XR_TYPE_SYSTEM_GET_INFO};gi.formFactor=XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;if(!Check(xrGetSystem(instance,&gi,&systemId),"system"))return false;
    XrSystemProperties props{XR_TYPE_SYSTEM_PROPERTIES};
    if(XR_SUCCEEDED(xrGetSystemProperties(instance,systemId,&props)))maxLayers=props.graphicsProperties.maxLayerCount;
    XrGraphicsRequirementsD3D11KHR req{XR_TYPE_GRAPHICS_REQUIREMENTS_D3D11_KHR};if(!Check(xrGetD3D11GraphicsRequirementsKHR(instance,systemId,&req),"graphics requirements")||!CreateGraphics(req))return false;
    XrGraphicsBindingD3D11KHR binding{XR_TYPE_GRAPHICS_BINDING_D3D11_KHR};binding.device=device;XrSessionCreateInfo sc{XR_TYPE_SESSION_CREATE_INFO};sc.next=&binding;sc.systemId=systemId;if(!Check(xrCreateSession(instance,&sc,&session),"session"))return false;
    XrReferenceSpaceCreateInfo rs{XR_TYPE_REFERENCE_SPACE_CREATE_INFO};rs.referenceSpaceType=XR_REFERENCE_SPACE_TYPE_LOCAL;rs.poseInReferenceSpace.orientation.w=1;if(!Check(xrCreateReferenceSpace(session,&rs,&localSpace),"LOCAL space"))return false;
    uint32_t count=0;if(!Check(xrEnumerateEnvironmentBlendModes(instance,systemId,XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO,0,&count,nullptr),"blend modes")||!count)return false;
    std::vector<XrEnvironmentBlendMode> modes(count);xrEnumerateEnvironmentBlendModes(instance,systemId,XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO,count,&count,modes.data());blend=modes[0];for(auto m:modes)if(m==XR_ENVIRONMENT_BLEND_MODE_OPAQUE)blend=m;
    if(!Check(xrEnumerateSwapchainFormats(session,0,&count,nullptr),"formats")||!count)return false;
    std::vector<int64_t> formats(count);xrEnumerateSwapchainFormats(session,count,&count,formats.data());
    const DXGI_FORMAT preferences[]={Config().srgb?DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:DXGI_FORMAT_B8G8R8A8_UNORM,DXGI_FORMAT_B8G8R8A8_UNORM,DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,DXGI_FORMAT_R8G8B8A8_UNORM};
    for(auto candidate:preferences)if(format==DXGI_FORMAT_UNKNOWN&&std::find(formats.begin(),formats.end(),(int64_t)candidate)!=formats.end())format=candidate;
    if(format==DXGI_FORMAT_UNKNOWN){Log("No supported four-byte OpenXR color format");return false;}
    XrViewConfigurationView sizes[2]={{XR_TYPE_VIEW_CONFIGURATION_VIEW},{XR_TYPE_VIEW_CONFIGURATION_VIEW}};
    if(!Check(xrEnumerateViewConfigurationViews(instance,systemId,XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO,2,&count,sizes),"view sizes")||count!=2)return false;
    for(int e=0;e<2;++e){eyeRecommendations[e]=sizes[e];if(!MakeChain(chains[e],sizes[e].recommendedImageRectWidth,sizes[e].recommendedImageRectHeight))return false;}
    if(!SetupInput())return false;available=true;Poll();
    // RFX hooks have process lifetime. Pin their module before starting a
    // background callback, so an explicit FreeLibrary cannot unload its code.
    HMODULE pinned=nullptr;
    if(GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_PIN,reinterpret_cast<LPCSTR>(&KeepCinema),&pinned)){
        keeperStop=CreateEventA(nullptr,TRUE,FALSE,nullptr);
        if(keeperStop)keeperThread=CreateThread(nullptr,0,KeepCinema,nullptr,0,nullptr);
    }
    Log("LOADING cinema keeper=%s",keeperThread?"ready":"unavailable; game-thread retention remains active");return true;
}
bool Running(){Guard lock;return running&&available;}
bool Begin(){
    Guard lock;
    if(!available)return false;if(open)return true;Poll();if(!running)return false;
    transferTiming={};
    if(serial==0)Log("FRAME entering first xrWaitFrame");
    const double waitStart=FrameClockMs();
    XrFrameWaitInfo wi{XR_TYPE_FRAME_WAIT_INFO};XrFrameState fs{XR_TYPE_FRAME_STATE};if(!Check(xrWaitFrame(session,&wi,&fs),"wait frame"))return false;
    const double waitMs=FrameClockMs()-waitStart;
    XrFrameBeginInfo bi{XR_TYPE_FRAME_BEGIN_INFO};if(!Check(xrBeginFrame(session,&bi),"begin frame"))return false;
    if(serial==0)Log("FRAME first xrWaitFrame/xrBeginFrame completed; shouldRender=%d",fs.shouldRender?1:0);
    if(originChangeTime&&fs.predictedDisplayTime>=originChangeTime){++originSerial;originChangeTime=0;retained.valid=false;Log("OpenXR runtime recentered: rebuilding shared tracking reference");}
    Frame next{};next.time=fs.predictedDisplayTime;next.period=fs.predictedDisplayPeriod;next.waitMs=waitMs;next.id=++serial;next.origin=originSerial;next.tick=GetTickCount64();next.shouldRender=fs.shouldRender!=0;next.focused=state==XR_SESSION_STATE_FOCUSED;
    next.views[0].type=next.views[1].type=XR_TYPE_VIEW;XrViewLocateInfo li{XR_TYPE_VIEW_LOCATE_INFO};li.viewConfigurationType=XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;li.displayTime=next.time;li.space=localSpace;
    XrViewState vs{XR_TYPE_VIEW_STATE};uint32_t n=0;const auto vr=xrLocateViews(session,&li,&vs,2,&n,next.views);
    next.valid=XR_SUCCEEDED(vr)&&n==2&&(vs.viewStateFlags&(XR_VIEW_STATE_POSITION_VALID_BIT|XR_VIEW_STATE_ORIENTATION_VALID_BIT))==(XR_VIEW_STATE_POSITION_VALID_BIT|XR_VIEW_STATE_ORIENTATION_VALID_BIT);
    if(next.valid){
        const auto& a=next.views[0].pose;const auto& b=next.views[1].pose;
        next.headPose.position={(a.position.x+b.position.x)*.5f,(a.position.y+b.position.y)*.5f,(a.position.z+b.position.z)*.5f};
        const float sign=a.orientation.x*b.orientation.x+a.orientation.y*b.orientation.y+a.orientation.z*b.orientation.z+a.orientation.w*b.orientation.w<0?-1.f:1.f;
        auto& q=next.headPose.orientation;q={a.orientation.x+sign*b.orientation.x,a.orientation.y+sign*b.orientation.y,a.orientation.z+sign*b.orientation.z,a.orientation.w+sign*b.orientation.w};
        const float len=std::sqrt(q.x*q.x+q.y*q.y+q.z*q.z+q.w*q.w);if(len>.1f){q.x/=len;q.y/=len;q.z/=len;q.w/=len;}else next.valid=false;
        next.head=Matrix(next.headPose);for(int e=0;e<2;++e)next.eyes[e]=Matrix(next.views[e].pose);
        if(next.id==1||next.id%1800==1)Log("POSE frame=%llu ipd=%.5f FOV L=(%.3f %.3f %.3f %.3f) R=(%.3f %.3f %.3f %.3f)",next.id,Length(Sub(Position(next.eyes[1]),Position(next.eyes[0]))),next.views[0].fov.angleLeft,next.views[0].fov.angleRight,next.views[0].fov.angleUp,next.views[0].fov.angleDown,next.views[1].fov.angleLeft,next.views[1].fov.angleRight,next.views[1].fov.angleUp,next.views[1].fov.angleDown);
    }
    XrActiveActionSet active{actionSet,XR_NULL_PATH};XrActionsSyncInfo sync{XR_TYPE_ACTIONS_SYNC_INFO};sync.countActiveActionSets=1;sync.activeActionSets=&active;
    const XrResult syncResult=xrSyncActions(session,&sync);
    if(syncResult==XR_SUCCESS){
        for(int h=0;h<2;++h){auto& hand=next.hands[h];hand.aimValid=PoseState(aimAction,aimSpace[h],h,next.time,hand.aim);hand.gripValid=PoseState(gripAction,gripSpace[h],h,next.time,hand.grip);
            hand.trigger=FloatState(triggerAction,h);hand.squeeze=FloatState(squeezeAction,h);hand.stickClick=BoolState(clickAction,handPaths[h]);
            hand.primary=BoolState(primaryAction,handPaths[h]);hand.secondary=BoolState(secondaryAction,handPaths[h]);
            XrActionStateGetInfo sg{XR_TYPE_ACTION_STATE_GET_INFO};sg.action=stickAction;sg.subactionPath=handPaths[h];XrActionStateVector2f st{XR_TYPE_ACTION_STATE_VECTOR2F};if(XR_SUCCEEDED(xrGetActionStateVector2f(session,&sg,&st))&&st.isActive){hand.stickActive=true;hand.stickX=st.currentState.x;hand.stickY=st.currentState.y;}
            static XrPath profiles[2]{};XrInteractionProfileState profile{XR_TYPE_INTERACTION_PROFILE_STATE};
            if(XR_SUCCEEDED(xrGetCurrentInteractionProfile(session,handPaths[h],&profile))&&profile.interactionProfile!=profiles[h]){profiles[h]=profile.interactionProfile;char name[XR_MAX_PATH_LENGTH]{};uint32_t count=0;if(profile.interactionProfile)xrPathToString(instance,profile.interactionProfile,sizeof(name),&count,name);Log("ACTION profile %s=%s",h?"right":"left",name);}}
        next.hands[0].menu=BoolState(menuAction,XR_NULL_PATH);
    }
    AcquireSRWLockExclusive(&frameLock);current=next;open=true;ReleaseSRWLockExclusive(&frameLock);return true;
}
bool Snapshot(Frame& f){AcquireSRWLockShared(&frameLock);f=current;const bool ok=open&&f.valid&&GetTickCount64()-f.tick<250;ReleaseSRWLockShared(&frameLock);return ok;}
void Haptic(int hand,float strength,unsigned milliseconds){
    Guard lock(true);if(!lock.Acquired()||!available||!running||state!=XR_SESSION_STATE_FOCUSED||!hapticBound||!applyHaptic||hand<0||hand>1||!std::isfinite(strength))return;
    XrHapticActionInfo info{XR_TYPE_HAPTIC_ACTION_INFO};info.action=hapticAction;info.subactionPath=handPaths[hand];
    XrHapticVibration vibration{XR_TYPE_HAPTIC_VIBRATION};vibration.amplitude=std::max(0.f,std::min(strength,1.f));
    vibration.duration=(XrDuration)std::max(1u,std::min(milliseconds,250u))*1000000;vibration.frequency=XR_FREQUENCY_UNSPECIFIED;
    const auto result=applyHaptic(session,&info,reinterpret_cast<const XrHapticBaseHeader*>(&vibration));
    static unsigned reports=0;if(XR_FAILED(result)&&reports++<3)Log("OpenXR reload haptic failed (%d)",(int)result);
}
void EyeSize(int eye,unsigned& w,unsigned& h){Guard lock;if(eye<0||eye>1){w=h=0;return;}w=chains[eye].width;h=chains[eye].height;}
void MatchEyeTransfer(unsigned w,unsigned h){
    Guard lock;
    if(!available||!open||!w||!h||!Config().nativeEyeTransfer||(w==lastNativeWidth&&h==lastNativeHeight))return;
    lastNativeWidth=w;lastNativeHeight=h;
    unsigned widths[2]{},heights[2]{};bool changed=false;
    for(int e=0;e<2;++e){
        const auto& r=eyeRecommendations[e];
        const bool smaller=w<=r.maxImageRectWidth&&h<=r.maxImageRectHeight&&
            (std::uint64_t)w*h<(std::uint64_t)r.recommendedImageRectWidth*r.recommendedImageRectHeight;
        widths[e]=smaller?w:r.recommendedImageRectWidth;heights[e]=smaller?h:r.recommendedImageRectHeight;
        changed=changed||widths[e]!=chains[e].width||heights[e]!=chains[e].height;
    }
    if(!changed)return;
    // Allocate both replacements first. If either fails, keep the working
    // stereo pair instead of leaving only one resized eye or a blank image.
    Chain replacement[2];bool ready=true;
    for(int e=0;e<2&&ready;++e)ready=MakeChain(replacement[e],widths[e],heights[e]);
    if(ready){
        for(int e=0;e<2;++e)std::swap(chains[e],replacement[e]);
        Log("PERF native eye transfer source=%ux%u eyes=L%ux%u R%ux%u; original XR FOV/poses retained",w,h,chains[0].width,chains[0].height,chains[1].width,chains[1].height);
    }else Log("PERF native eye transfer allocation failed; keeping previous stereo sizes");
    for(auto& old:replacement)if(old.handle)xrDestroySwapchain(old.handle);
}
TransferTiming UploadTiming(){Guard lock;return transferTiming;}
bool QuadSize(unsigned w,unsigned h){Guard lock;return available&&w>0&&h>0&&MakeChain(chains[2],w,h);}
bool HudSize(unsigned w,unsigned h){Guard lock;return available&&w>0&&h>0&&MakeChain(chains[3],w,h);}
void HideSettingsOverlay(){Guard lock;settingsLayer.visible=false;}
bool SettingsOverlay(const XrPosef& pose,float width,float height,const unsigned char* bgra,unsigned w,unsigned h){
    Guard lock;if(!open||!current.valid||!current.focused||!current.shouldRender||maxLayers<2)return false;
    if(bgra){
        if(!w||!h||width<=0||height<=0)return false;
        if(chains[4].width!=w||chains[4].height!=h)settingsLayer.valid=false;
        if(!MakeChain(chains[4],w,h)||!Upload(4,bgra,w*4))return false;
        settingsLayer.valid=true;
    }
    if(!settingsLayer.valid)return false;
    settingsLayer.visible=true;settingsLayer.pose=pose;settingsLayer.width=width;settingsLayer.height=height;
    settingsLayer.origin=current.origin;settingsLayer.updated=GetTickCount64();return true;
}
bool Upload(int index,const unsigned char* bgra,unsigned pitch){
    Guard lock;
    if(!open||index<0||index>4||!bgra||!current.shouldRender)return false;auto& c=chains[index];if(!c.handle||pitch<c.width*4)return false;
    const double acquireStart=FrameClockMs();
    if(!AcquireImage(c)){ReleaseImage(c,false);transferTiming.wait+=FrameClockMs()-acquireStart;return false;}
    const double copyStart=FrameClockMs();transferTiming.wait+=copyStart-acquireStart;
    const bool swap=format==DXGI_FORMAT_R8G8B8A8_UNORM||format==DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    const unsigned char* source=bgra;unsigned sourcePitch=pitch;
    if(swap){
        // Uncommon RGBA runtimes still need channel conversion. Reuse storage.
        c.rgba.resize((size_t)c.width*c.height*4);
        for(unsigned y=0;y<c.height;++y)for(unsigned x=0;x<c.width;++x){const auto* in=bgra+(size_t)y*pitch+x*4;auto* out=c.rgba.data()+((size_t)y*c.width+x)*4;out[0]=in[2];out[1]=in[1];out[2]=in[0];out[3]=index==3?in[3]:255;}
        source=c.rgba.data();sourcePitch=c.width*4;
    }
    // D3D9 already supplies BGRA. Projection/cinema layers do not enable
    // source-alpha blending, so X8 alpha need not be rewritten to 255. The
    // alpha HUD retains its original premultiplied channels. UpdateSubresource
    // snapshots source bytes before returning; the caller can then unlock.
    context->UpdateSubresource(c.images[c.image].texture,0,nullptr,source,sourcePitch,0);
    const double flushStart=FrameClockMs();transferTiming.write+=flushStart-copyStart;context->Flush();
    const double finished=FrameClockMs();transferTiming.submit+=finished-flushStart;transferTiming.copy+=finished-copyStart;
    return ReleaseImage(c,true);
}
void ReleaseTransferAtlas(){if(transferAtlas){transferAtlas->Release();transferAtlas=nullptr;}atlasWidth=atlasHeight=0;atlasDynamic=false;}
void ResetTransfer(){Guard lock;ReleaseTransferAtlas();dynamicUnavailable=false;}
bool UnifiedMemoryAdapter(unsigned vendor,unsigned adapter,unsigned subsystem,unsigned revision){
    // The caller already owns Guard for the current stereo pair. Query once
    // during graphics creation, never per draw or per transferred row.
    return device&&unifiedMemory&&vendor!=0&&graphicsAdapter.VendorId==vendor&&
        graphicsAdapter.DeviceId==adapter&&graphicsAdapter.SubSysId==subsystem&&graphicsAdapter.Revision==revision;
}
BatchResult UploadAtlas(const unsigned char* bgra,unsigned pitch,unsigned width,unsigned height,const TransferRegion* regions,unsigned count){
    Guard lock;
    if(!open||!current.shouldRender||!device||!context)return BatchResult::Failed;
    // Other runtime formats retain the established per-surface conversion.
    const unsigned limit=device->GetFeatureLevel()>=D3D_FEATURE_LEVEL_11_0?16384u:8192u;
    if(!bgra||!regions||count<2||count>3||!width||!height||width>limit||height>limit||pitch<width*4||
       (format!=DXGI_FORMAT_B8G8R8A8_UNORM&&format!=DXGI_FORMAT_B8G8R8A8_UNORM_SRGB))return BatchResult::Unavailable;
    unsigned mask=0;
    for(unsigned i=0;i<count;++i){
        const auto& r=regions[i];if(r.surface!=0&&r.surface!=1&&r.surface!=3)return BatchResult::Unavailable;
        const auto& c=chains[r.surface];const unsigned bit=1u<<r.surface;
        if((mask&bit)||!c.handle||!r.width||!r.height||r.width!=c.width||r.height!=c.height||
           r.x>width||r.width>width-r.x||r.y>height||r.height>height-r.y)return BatchResult::Unavailable;
        mask|=bit;
    }
    if((mask&3)!=3)return BatchResult::Unavailable;
    const auto allocate=[&](bool dynamic){
        ReleaseTransferAtlas();D3D11_TEXTURE2D_DESC desc{};desc.Width=width;desc.Height=height;
        desc.MipLevels=desc.ArraySize=1;desc.Format=format;desc.SampleDesc.Count=1;
        desc.Usage=dynamic?D3D11_USAGE_DYNAMIC:D3D11_USAGE_DEFAULT;
        desc.CPUAccessFlags=dynamic?D3D11_CPU_ACCESS_WRITE:0;
        desc.BindFlags=dynamic?D3D11_BIND_SHADER_RESOURCE:0;
        const HRESULT hr=device->CreateTexture2D(&desc,nullptr,&transferAtlas);
        if(FAILED(hr)){Log("PERF atlas allocation dynamic=%d hr=%08X",dynamic?1:0,(unsigned)hr);return false;}
        atlasWidth=width;atlasHeight=height;atlasDynamic=dynamic;
        Log("PERF atlas upload mode=%s size=%ux%u",dynamic?"dynamic-discard":"update-subresource",width,height);return true;
    };
    if(!transferAtlas||atlasWidth!=width||atlasHeight!=height){
        ReleaseTransferAtlas();
        if(Config().dynamicUpload&&!dynamicUnavailable&&!allocate(true))dynamicUnavailable=true;
        if(!transferAtlas&&!allocate(false))return BatchResult::Unavailable;
    }
    // Fill our atlas before acquiring XR images. If mapping is unsupported,
    // the same current pixels can use the r17 DEFAULT atlas immediately.
    const double uploadStart=FrameClockMs();bool written=false;
    if(atlasDynamic){
        D3D11_MAPPED_SUBRESOURCE mapped{};
        const HRESULT hr=context->Map(transferAtlas,0,D3D11_MAP_WRITE_DISCARD,0,&mapped);
        const double writeStart=FrameClockMs();transferTiming.map+=writeStart-uploadStart;
        if(SUCCEEDED(hr)){
            if(mapped.pData&&mapped.RowPitch>=width*4){
                auto* destination=static_cast<unsigned char*>(mapped.pData);
                // Only write mapped memory; never read write-combined pages.
                // Row pitches belong to separate APIs and need not match.
                if(pitch==width*4&&mapped.RowPitch==pitch)memcpy(destination,bgra,(size_t)pitch*height);
                else for(unsigned y=0;y<height;++y)memcpy(destination+(size_t)y*mapped.RowPitch,bgra+(size_t)y*pitch,width*4);
                written=true;
            }
            context->Unmap(transferAtlas,0);
        }
        transferTiming.write+=FrameClockMs()-writeStart;
        if(!written){
            dynamicUnavailable=true;Log("PERF dynamic upload unavailable hr=%08X; using r17 atlas upload until reset",(unsigned)hr);
            if(!allocate(false)){transferTiming.copy+=FrameClockMs()-uploadStart;return BatchResult::Unavailable;}
        }
    }
    if(!written){const double writeStart=FrameClockMs();context->UpdateSubresource(transferAtlas,0,nullptr,bgra,pitch,0);transferTiming.write+=FrameClockMs()-writeStart;}
    transferTiming.copy+=FrameClockMs()-uploadStart;
    const double acquireStart=FrameClockMs();bool acquired=true;
    for(unsigned i=0;i<count;++i)if(!AcquireImage(chains[regions[i].surface])){acquired=false;break;}
    const double copyStart=FrameClockMs();transferTiming.wait+=copyStart-acquireStart;
    if(!acquired){
        // No XR image copies were queued. Return every waited image, but
        // retain failed waits for a legal retry on the next frame.
        for(unsigned i=0;i<count;++i)ReleaseImage(chains[regions[i].surface],false);
        return BatchResult::Failed;
    }
    for(unsigned i=0;i<count;++i){
        const auto& r=regions[i];auto& c=chains[r.surface];
        const D3D11_BOX box{r.x,r.y,0,r.x+r.width,r.y+r.height,1};
        context->CopySubresourceRegion(c.images[c.image].texture,0,0,0,0,transferAtlas,0,&box);
    }
    // Submit ALL GPU writes before giving ANY of these images back to XR.
    context->Flush();const double finished=FrameClockMs();transferTiming.copy+=finished-copyStart;transferTiming.submit+=finished-copyStart;
    bool released=true;
    for(unsigned i=0;i<count;++i)if(!ReleaseImage(chains[regions[i].surface],true))released=false;
    return released?BatchResult::Uploaded:BatchResult::Failed;
}
bool SubmitEyes(const XrPosef* hudPose,float hudWidth,float hudHeight){
    Guard lock;lastWasCinema=false;lastGameSubmission=GetTickCount64();
    if(!open||!current.valid||!current.shouldRender||!chains[0].uploaded||!chains[1].uploaded){SubmitEmpty();return false;}
    XrCompositionLayerProjectionView pv[2]={{XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW},{XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW}};
    for(int e=0;e<2;++e){pv[e].pose=current.views[e].pose;pv[e].fov=current.views[e].fov;pv[e].subImage.swapchain=chains[e].handle;pv[e].subImage.imageRect.extent={(int)chains[e].width,(int)chains[e].height};}
    XrCompositionLayerQuad hud{XR_TYPE_COMPOSITION_LAYER_QUAD};
    const bool haveHud=hudPose&&hudWidth>0&&hudHeight>0&&chains[3].uploaded;
    if(haveHud){hud.space=localSpace;hud.eyeVisibility=XR_EYE_VISIBILITY_BOTH;hud.layerFlags=XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT;
        hud.pose=*hudPose;hud.size={hudWidth,hudHeight};hud.subImage.swapchain=chains[3].handle;hud.subImage.imageRect.extent={(int)chains[3].width,(int)chains[3].height};}
    XrCompositionLayerProjection layer{XR_TYPE_COMPOSITION_LAYER_PROJECTION};layer.space=localSpace;layer.viewCount=2;layer.views=pv;
    return Finish(reinterpret_cast<XrCompositionLayerBaseHeader*>(&layer),haveHud?reinterpret_cast<XrCompositionLayerBaseHeader*>(&hud):nullptr);
}
bool SubmitQuad(const XrPosef& pose,float width,float height){
    Guard lock;lastGameSubmission=GetTickCount64();
    if(!open||!current.valid||!current.shouldRender||!chains[2].uploaded){SubmitRetained();return false;}
    retained={true,pose,width,height,current.origin};lastWasCinema=true;return SubmitRetained();
}
bool HasCinemaImage(){Guard lock;return retained.valid&&retained.origin==originSerial;}
bool SubmitRetained(){
    Guard lock;if(!open)return false;
    if(!current.shouldRender||!retained.valid||retained.origin!=current.origin){SubmitEmpty();return false;}
    XrCompositionLayerQuad layer{XR_TYPE_COMPOSITION_LAYER_QUAD};layer.space=localSpace;layer.eyeVisibility=XR_EYE_VISIBILITY_BOTH;
    layer.pose=retained.pose;layer.size={retained.width,retained.height};layer.subImage.swapchain=chains[2].handle;layer.subImage.imageRect.extent={(int)chains[2].width,(int)chains[2].height};
    lastWasCinema=true;
    return Finish(reinterpret_cast<XrCompositionLayerBaseHeader*>(&layer));
}
bool RefreshCinemaFrame(){
    Guard lock;
    if(open&&GetTickCount64()-current.tick>=250){
        Log("LOADING refresh stale frame=%llu age=%llu ms; retained=%d",current.id,GetTickCount64()-current.tick,retained.valid?1:0);
        SubmitRetained();
    }
    return Begin();
}
void SubmitEmpty(){Guard lock;if(open)Finish(nullptr);}
void Shutdown(){
    // Called outside DllMain only. Never wait on the keeper under the XR gate.
    if(keeperStop)SetEvent(keeperStop);
    if(keeperThread){WaitForSingleObject(keeperThread,INFINITE);CloseHandle(keeperThread);keeperThread=nullptr;}
    if(keeperStop){CloseHandle(keeperStop);keeperStop=nullptr;}Guard lock;retained.valid=false;settingsLayer={};
    SubmitEmpty();for(auto& c:chains){if(c.handle&&xrDestroySwapchain)xrDestroySwapchain(c.handle);c=Chain{};}
    for(int i=0;i<2;++i){if(aimSpace[i]&&xrDestroySpace)xrDestroySpace(aimSpace[i]);if(gripSpace[i]&&xrDestroySpace)xrDestroySpace(gripSpace[i]);aimSpace[i]=gripSpace[i]=XR_NULL_HANDLE;}
    if(localSpace&&xrDestroySpace)xrDestroySpace(localSpace);localSpace=XR_NULL_HANDLE;
    if(session&&xrDestroySession)xrDestroySession(session);session=XR_NULL_HANDLE;
    if(actionSet&&xrDestroyActionSet)xrDestroyActionSet(actionSet);actionSet=XR_NULL_HANDLE;
    if(instance&&xrDestroyInstance)xrDestroyInstance(instance);instance=XR_NULL_HANDLE;
    ResetTransfer();if(context){context->Release();context=nullptr;}if(device){device->Release();device=nullptr;}
    if(loader){FreeLibrary(loader);loader=nullptr;}running=available=false;
}
}} // namespace
