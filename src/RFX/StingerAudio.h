#pragma once
namespace CleanVR { namespace StingerAudio {
// Called from the VR input path. Only the engine's confirmed seeker state
// selects a cue; zero also cancels any queued audio without initializing it.
void Update(int nativeState);
}}
