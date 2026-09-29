#include <Windows.h>
#include <xaudio2.h>
#include "ReloadAudio.h"
#include "VrSettings.h"
#pragma comment(lib,"ole32.lib")

namespace CleanVR { namespace ReloadAudio { namespace {
constexpr unsigned VoiceCount=4;
struct Clip {const BYTE* data=nullptr;UINT32 bytes=0;};
Clip clips[WeaponCount][4];
IXAudio2* engine=nullptr;IXAudio2MasteringVoice* master=nullptr;
IXAudio2SourceVoice* voices[VoiceCount]{};
SRWLOCK audioLock=SRWLOCK_INIT;
volatile LONG available=0;bool attempted=false;HMODULE audioModule=nullptr;
// No callback touches the game, reload lock, voices or resources.
struct EngineEvents final:IXAudio2EngineCallback {
    void STDMETHODCALLTYPE OnProcessingPassStart() override {}
    void STDMETHODCALLTYPE OnProcessingPassEnd() override {}
    void STDMETHODCALLTYPE OnCriticalError(HRESULT) override {InterlockedExchange(&available,0);}
} events;
void Dispose(){
    InterlockedExchange(&available,0);
    for(auto& voice:voices)if(voice){voice->DestroyVoice();voice=nullptr;}
    if(master){master->DestroyVoice();master=nullptr;}
    if(engine){engine->UnregisterForCallbacks(&events);engine->Release();engine=nullptr;}
}
}
bool Ready(){return InterlockedCompareExchange(&available,0,0)!=0;}
bool Initialize(){
    if(attempted)return Ready();attempted=true;
    if(!Config().reloadSounds)return false;
    HMODULE own=nullptr;
    if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        reinterpret_cast<LPCWSTR>(&Initialize),&own))return false;
    for(unsigned weapon=0;weapon<WeaponCount;++weapon)for(unsigned cue=0;cue<4;++cue){
        const auto resource=FindResourceW(own,MAKEINTRESOURCEW(1100+weapon*4+cue),MAKEINTRESOURCEW(10));
        if(!resource)return false;
        const DWORD size=SizeofResource(own,resource);
        const BYTE* data=static_cast<const BYTE*>(LockResource(LoadResource(own,resource)));
        if(!data||!size||size%2||size>44100)return false;
        clips[weapon][cue]={data,size};
    }
    // Windows 10/11 system XAudio2; no redistributable, SteamVR or extra file.
    audioModule=LoadLibraryExW(L"xaudio2_9.dll",nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32);
    if(!audioModule){Log("MRELOAD21 audio unavailable: Windows XAudio2 load failed; stock audio retained");return false;}
    using Create=HRESULT(WINAPI*)(IXAudio2**,UINT32,XAUDIO2_PROCESSOR);
    const auto create=reinterpret_cast<Create>(GetProcAddress(audioModule,"XAudio2Create"));
    if(!create)return false;
    const HRESULT com=CoInitializeEx(nullptr,COINIT_MULTITHREADED);
    HRESULT hr=create(&engine,0,XAUDIO2_DEFAULT_PROCESSOR);
    if(SUCCEEDED(hr))hr=engine->RegisterForCallbacks(&events);
    if(SUCCEEDED(hr))hr=engine->CreateMasteringVoice(&master,2,48000,0,nullptr,nullptr,AudioCategory_GameEffects);
    WAVEFORMATEX format{};format.wFormatTag=WAVE_FORMAT_PCM;format.nChannels=1;format.nSamplesPerSec=22050;
    format.wBitsPerSample=16;format.nBlockAlign=2;format.nAvgBytesPerSec=44100;
    for(unsigned i=0;SUCCEEDED(hr)&&i<VoiceCount;++i){
        hr=engine->CreateSourceVoice(&voices[i],&format,0,1.f);
        if(SUCCEEDED(hr))hr=voices[i]->SetVolume(Config().reloadSoundVolume);
        if(SUCCEEDED(hr))hr=voices[i]->Start();
    }
    if(SUCCEEDED(com))CoUninitialize();
    if(FAILED(hr)){Log("MRELOAD21 audio initialization failed hr=%08X; stock audio retained",hr);Dispose();return false;}
    // Game hooks have process lifetime. Resources and these voices stay alive
    // with the DLL; cancellation flushes voices, never frees submitted PCM.
    InterlockedExchange(&available,1);
    Log("MRELOAD21 audio ready: %u weapon profiles; action-timed stock PCM cues; volume=%.2f",WeaponCount,Config().reloadSoundVolume);
    return true;
}
void Play(unsigned weapon,Cue cue){
    if(!Ready()||weapon>=WeaponCount||static_cast<unsigned>(cue)>=4)return;
    AcquireSRWLockExclusive(&audioLock);
    if(Ready())for(auto* voice:voices){
        XAUDIO2_VOICE_STATE state{};voice->GetState(&state,XAUDIO2_VOICE_NOSAMPLESPLAYED);
        if(state.BuffersQueued)continue;
        const Clip& clip=clips[weapon][cue];XAUDIO2_BUFFER buffer{};
        buffer.Flags=XAUDIO2_END_OF_STREAM;buffer.AudioBytes=clip.bytes;buffer.pAudioData=clip.data;
        const HRESULT hr=voice->SubmitSourceBuffer(&buffer);
        if(FAILED(hr)){InterlockedExchange(&available,0);Log("MRELOAD21 cue submit failed hr=%08X; stock audio restored",hr);}
        break; // Never queue delayed reload sounds behind an earlier gesture.
    }
    ReleaseSRWLockExclusive(&audioLock);
}
void Stop(){
    AcquireSRWLockExclusive(&audioLock);
    for(auto* voice:voices)if(voice){voice->Stop();voice->FlushSourceBuffers();voice->Start();}
    ReleaseSRWLockExclusive(&audioLock);
}
} }
