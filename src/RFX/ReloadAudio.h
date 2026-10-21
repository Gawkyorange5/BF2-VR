#pragma once
namespace CleanVR { namespace ReloadAudio {
constexpr unsigned WeaponCount=189;
enum Cue { MagazineOut,MagazineIn,HandlePull,HandleHome };
bool Initialize();
bool Ready();
enum LmgCue { CoverOpen,BeltSeat,CoverClose };
void Play(unsigned weapon,Cue cue);
void PlayLmg(unsigned weapon,LmgCue cue);
void DefibReady();
bool GrenadePin();
void Stop();
} }
