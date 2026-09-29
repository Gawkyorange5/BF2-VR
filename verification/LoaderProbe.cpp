#include <windows.h>
// Force all three import slots used by RFX's process-attach hook to exist.
static bool InRfx(const void* p, HMODULE rfx) { MEMORY_BASIC_INFORMATION m={}; return VirtualQuery(p,&m,sizeof(m)) && m.AllocationBase==rfx; }
extern "C" void __stdcall ProbeEntry() {
 HMODULE rfx=GetModuleHandleA("RFX.dll");bool ok=rfx && GetProcAddress(rfx,"main");
 // Use actual IAT values after process attach, without creating a game window.
 ok=ok && InRfx((const void*)PeekMessageA,rfx) && InRfx((const void*)CreateWindowExA,rfx) && InRfx((const void*)LoadLibraryA,rfx);
 const char* message=ok?"PASS: RFX loaded at process startup; all three import hooks point into RFX.\r\n":"FAIL: startup import/hook verification.\r\n";
 DWORD n;WriteFile(GetStdHandle(STD_OUTPUT_HANDLE),message,lstrlenA(message),&n,0);ExitProcess(ok?0:1);
}
