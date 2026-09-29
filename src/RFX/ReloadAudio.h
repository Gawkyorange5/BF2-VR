#pragma once
namespace CleanVR { namespace ReloadAudio {
constexpr unsigned WeaponCount=29;
enum Cue { MagazineOut,MagazineIn,HandlePull,HandleHome };
bool Initialize();
bool Ready();
void Play(unsigned weapon,Cue cue);
void Stop();
} }
