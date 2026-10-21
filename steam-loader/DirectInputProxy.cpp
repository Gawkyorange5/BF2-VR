// Optional BF2-VR loader: preserve the original BF2.exe and its Steam launch.
// RFX is a normal static dependency, initialized before the game entry point.
// No injection, Steam emulation, executable edits, or license checks changed.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#define DIRECTINPUT_VERSION 0x0800
#include <dinput.h>
extern "C" __declspec(dllimport) int __cdecl main();
namespace {
INIT_ONCE once=INIT_ONCE_STATIC_INIT;
HMODULE systemInput=nullptr;
BOOL CALLBACK LoadSystemInput(PINIT_ONCE,void*,void**){
    wchar_t path[MAX_PATH];const UINT n=GetSystemDirectoryW(path,MAX_PATH);
    if(!n||n+13>=MAX_PATH)return FALSE;
    lstrcatW(path,L"\\dinput8.dll");
    systemInput=LoadLibraryExW(path,nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32);
    return systemInput!=nullptr;
}
FARPROC Function(const char* name){
    if(!InitOnceExecuteOnce(&once,LoadSystemInput,nullptr,nullptr))return nullptr;
    return GetProcAddress(systemInput,name);
}
}
extern "C" HRESULT WINAPI ProxyCreate(HINSTANCE instance,DWORD version,REFIID iid,void** out,IUnknown* outer){
    if(out)*out=nullptr;
    using Fn=HRESULT(WINAPI*)(HINSTANCE,DWORD,REFIID,void**,IUnknown*);
    const auto fn=reinterpret_cast<Fn>(Function("DirectInput8Create"));
    return fn?fn(instance,version,iid,out,outer):HRESULT_FROM_WIN32(ERROR_MOD_NOT_FOUND);
}
extern "C" HRESULT WINAPI ProxyCanUnload(){
    using Fn=HRESULT(WINAPI*)();const auto fn=reinterpret_cast<Fn>(Function("DllCanUnloadNow"));return fn?fn():S_FALSE;
}
extern "C" HRESULT WINAPI ProxyClass(REFCLSID cls,REFIID iid,void** out){
    if(out)*out=nullptr;
    using Fn=HRESULT(WINAPI*)(REFCLSID,REFIID,void**);const auto fn=reinterpret_cast<Fn>(Function("DllGetClassObject"));return fn?fn(cls,iid,out):CLASS_E_CLASSNOTAVAILABLE;
}
extern "C" HRESULT WINAPI ProxyRegister(){
    using Fn=HRESULT(WINAPI*)();const auto fn=reinterpret_cast<Fn>(Function("DllRegisterServer"));return fn?fn():E_NOTIMPL;
}
extern "C" HRESULT WINAPI ProxyUnregister(){
    using Fn=HRESULT(WINAPI*)();const auto fn=reinterpret_cast<Fn>(Function("DllUnregisterServer"));return fn?fn():E_NOTIMPL;
}
extern "C" const DIDATAFORMAT* WINAPI ProxyJoystick(){
    using Fn=const DIDATAFORMAT*(WINAPI*)();const auto fn=reinterpret_cast<Fn>(Function("GetdfDIJoystick"));return fn?fn():nullptr;
}
BOOL WINAPI DllMain(HINSTANCE module,DWORD reason,void*){
    if(reason==DLL_PROCESS_ATTACH){DisableThreadLibraryCalls(module);return main()==0;}
    return TRUE;
}
