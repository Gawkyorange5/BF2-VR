#pragma once
#include "VrPoseMath.h"
#include <Windows.h>
#define XR_NO_PROTOTYPES
#include <openxr/openxr.h>
namespace CleanVR { namespace Runtime {
// Serializes OpenXR and its D3D11 context, including an entire stereo replay.
// The loading keeper never touches BF2 objects or the D3D9 game device.
class Guard {
public: explicit Guard(bool tryOnly=false);~Guard();void Unlock();bool Acquired()const{return acquired;}
private: bool acquired;Guard(const Guard&)=delete;Guard& operator=(const Guard&)=delete;
};
struct HandSample { Mat grip{},aim{};bool gripValid=false,aimValid=false,stickActive=false,stickClick=false,primary=false,secondary=false,menu=false;float trigger=0,squeeze=0,stickX=0,stickY=0; };
struct Frame {
    XrTime time=0;XrDuration period=0;double waitMs=0;std::uint64_t id=0,origin=0;ULONGLONG tick=0;bool valid=false,focused=false,shouldRender=false;
    XrView views[2]{};XrPosef headPose{};Mat head{},eyes[2]{};HandSample hands[2]{};
};
bool Initialize();
bool Begin(); // idempotent until Submit: exactly one locate/sync per pair
bool Snapshot(Frame&);
void Haptic(int hand,float strength,unsigned milliseconds); // optional short grip feedback
bool Running();
// Cached driver-reported UMA capability, matched to the native game's adapter.
// Missing feature support or an adapter mismatch returns false.
bool UnifiedMemoryAdapter(unsigned vendor,unsigned adapter,unsigned subsystem,unsigned revision);
bool MatchingGraphicsAdapter(unsigned vendor,unsigned adapter,unsigned subsystem,unsigned revision);
HRESULT GraphicsStatus(); // diagnostic device-removal status; no GPU wait
void EyeSize(int eye,unsigned& width,unsigned& height);
void MatchEyeTransfer(unsigned nativeWidth,unsigned nativeHeight); // before acquiring either eye image
struct TransferTiming {double wait=0,copy=0,map=0,write=0,submit=0;};
TransferTiming UploadTiming(); // subsets of total upload wall time, current frame
bool QuadSize(unsigned width,unsigned height);
bool HudSize(unsigned width,unsigned height);
// A separately retained menu image; allocated/uploaded only while open.
bool SettingsOverlay(const XrPosef&,float width,float height,const unsigned char* bgra,unsigned pixelsWide,unsigned pixelsHigh,bool transparent=false);
void HideSettingsOverlay();
bool Upload(int surface,const unsigned char* bgra,unsigned pitch); // 0/1 eyes, 2 cinema, 3 alpha HUD
struct TransferRegion {int surface;unsigned x,y,width,height;};
// Unavailable means no XR images were touched: callers may use Upload instead.
// Failed means abandon this frame; do not acquire the same chains again.
enum class BatchResult {Unavailable,Failed,Uploaded};
BatchResult UploadAtlas(const unsigned char* bgra,unsigned pitch,unsigned width,unsigned height,const TransferRegion* regions,unsigned count);
void ResetTransfer(); // release the reusable D3D11 transfer atlas
bool SubmitEyes(const XrPosef* hudPose=nullptr,float hudWidth=0,float hudHeight=0,bool hudUnpremultiplied=false);
bool SubmitQuad(const XrPosef& pose,float width,float height);
bool SubmitRetained(); // resubmit the last released cinema image through a loading stall
bool RefreshCinemaFrame(); // end stale timing without sending a black/empty frame
bool HasCinemaImage();
void SubmitEmpty();
void Shutdown();
}}
