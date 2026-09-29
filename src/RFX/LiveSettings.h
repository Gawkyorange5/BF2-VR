#pragma once
#include "VrSettings.h"
namespace CleanVR {
struct LiveOption {unsigned page;bool toggle;const char *section,*key,*label,*help;float minimum,maximum,step,initial;size_t offset;};
const LiveOption* LiveOptions(unsigned& count);
float LiveValue(unsigned index);
// Save only edited values, with a backup before the first write. Called on
// the serialized game/render path; never by the loading keeper.
bool SaveLiveValues(const float* values,const bool* changed);
void InvalidateTuning();
}
