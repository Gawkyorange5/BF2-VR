#pragma once
#include "XrSessionClient.h"
namespace CleanVR { namespace Controls {
// Global buttons are processed once per XR sample, before choosing UI/gameplay.
bool Global(const Runtime::Frame&); // true requests recenter
bool TakeSettingsRequest(); // consumes a both-stick hold request
bool RecenterKeyBlocked(); // injected seat F6 must not also recenter
void Gameplay(const Runtime::Frame&,const Mat& reference);
void Menu(const Runtime::Frame&);
void PointerButton(bool down);
void ReleaseAll();
}}
