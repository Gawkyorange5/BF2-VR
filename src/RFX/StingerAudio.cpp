#include <Windows.h>
#include <xaudio2.h>
#include "StingerAudio.h"
#include "VrSettings.h"
#pragma comment(lib,"ole32.lib")

namespace CleanVR { namespace StingerAudio { namespace {
struct Cue {const BYTE* data=nullptr;UINT32 bytes=0,loops=0;};
Cue cues[2];
IXAudio2* engine=nullptr;IXAudio2MasteringVoice* master=nullptr;IXAudio2SourceVoice* voice=nullptr;
SRWLOCK lock=SRWLOCK_INIT;
volatile LONG available=0;bool attempted=false;int playing=0;
// Audio callbacks never read game objects or touch the input/render locks.
struct Events final:IXAudio2EngineCallback {
    void STDMETHODCALLTYPE OnProcessingPassStart() override {}
    void STDMETHODCALLTYPE OnProcessingPassEnd() override {}
    void STDMETHODCALLTYPE OnCriticalError(HRESULT) override {InterlockedExchange(&available,0);}
} events;
bool Initialize(){
    if(attempted)return InterlockedCompareExchange(&available,0,0)!=0;
    attempted=true;HMODULE own=nullptr;
    if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        reinterpret_cast<LPCWSTR>(&Initialize),&own))return false;
    for(unsigned i=0;i<2;++i){
        const auto resource=FindResourceW(own,MAKEINTRESOURCEW(4000+i),MAKEINTRESOURCEW(10));
        if(!resource)return false;
        const auto bytes=SizeofResource(own,resource);
        const auto data=static_cast<const BYTE*>(LockResource(LoadResource(own,resource)));
        if(!data||bytes<2||bytes%2||bytes>10000)return false;
        // Repeat whole native waveforms for ~0.1 second per submission.
        // No infinite loop: two queued buffers expire within ~230ms even if
        // rendering pauses, focus is lost or native game updates stop.
        const UINT32 repeats=(4410+bytes/2-1)/(bytes/2);
        cues[i]={data,bytes,repeats-1};
    }
    HMODULE module=LoadLibraryExW(L"xaudio2_9.dll",nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32);
    if(!module){Log("STINGER118 audio unavailable: system XAudio2 missing");return false;}
    using Create=HRESULT(WINAPI*)(IXAudio2**,UINT32,XAUDIO2_PROCESSOR);
    const auto create=reinterpret_cast<Create>(GetProcAddress(module,"XAudio2Create"));
    if(!create)return false;
    const HRESULT com=CoInitializeEx(nullptr,COINIT_MULTITHREADED);
    HRESULT hr=create(&engine,0,XAUDIO2_DEFAULT_PROCESSOR);
    if(SUCCEEDED(hr))hr=engine->RegisterForCallbacks(&events);
    if(SUCCEEDED(hr))hr=engine->CreateMasteringVoice(&master,2,48000,0,nullptr,nullptr,AudioCategory_GameEffects);
    WAVEFORMATEX format{};format.wFormatTag=WAVE_FORMAT_PCM;format.nChannels=1;format.nSamplesPerSec=44100;
    format.wBitsPerSample=16;format.nBlockAlign=2;format.nAvgBytesPerSec=88200;
    if(SUCCEEDED(hr))hr=engine->CreateSourceVoice(&voice,&format,0,1.f);
    if(SUCCEEDED(hr))hr=voice->Start();
    if(SUCCEEDED(com))CoUninitialize();
    if(FAILED(hr)){
        if(voice){voice->DestroyVoice();voice=nullptr;}
        if(master){master->DestroyVoice();master=nullptr;}
        if(engine){engine->UnregisterForCallbacks(&events);engine->Release();engine=nullptr;}
        Log("STINGER118 audio unavailable hr=%08X",hr);return false;
    }
    InterlockedExchange(&available,1);
    Log("STINGER118 native TargetTrack/TargetLock audio ready; independent of manual reload audio");return true;
}
}
void Update(int state){
    if(state<0||state>2)state=0;
    AcquireSRWLockExclusive(&lock);
    if(state&&!Initialize())state=0;
    if(!InterlockedCompareExchange(&available,0,0))state=0;
    if(voice&&state!=playing){
        voice->Stop();voice->FlushSourceBuffers();
        if(state){voice->SetVolume(state==1?.35f:.4f);voice->Start();}
        static unsigned reports=0;if(reports++<24)Log("STINGER118 audio state=%d",state);
    }
    playing=state;
    if(voice&&state){
        XAUDIO2_VOICE_STATE current{};voice->GetState(&current,XAUDIO2_VOICE_NOSAMPLESPLAYED);
        const Cue& cue=cues[state-1];
        for(UINT32 queued=current.BuffersQueued;queued<2;++queued){
            XAUDIO2_BUFFER buffer{};buffer.AudioBytes=cue.bytes;buffer.pAudioData=cue.data;buffer.LoopCount=cue.loops;
            const HRESULT hr=voice->SubmitSourceBuffer(&buffer);
            if(FAILED(hr)){
                voice->Stop();voice->FlushSourceBuffers();playing=0;InterlockedExchange(&available,0);
                Log("STINGER118 audio stopped hr=%08X",hr);break;
            }
        }
    }
    ReleaseSRWLockExclusive(&lock);
}
}}
