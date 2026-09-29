#pragma once
#include "XrSessionClient.h"
namespace CleanVR { namespace SettingsMenu {
bool Open(const Runtime::Frame&);
bool Active();
void Close();
void Input(const Runtime::Frame&);
void Present();
}}
