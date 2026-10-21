// BF2's ladder contact callback reads the native camera pitch, not its
// rendered XR replacement. At the bottom it rejects a level/downward look;
// at the top it checks ladder-down-angle-limit. Feed that SAME native check
// the local headset view. No radius, collision, parent or position writes.
bool LadderHeadMatrix(void* owner,const Mat* native,Mat& tracked){
    if(!owner||owner!=Soldier()||!referenceValid||!LocalAlive()||Value<void*>(owner,0x254))return false;
    if(Value<void*>(Value<void*>(Player(),0x80),4)!=owner)return false;
    Runtime::Frame f{};Mat source{};const auto now=GetTickCount64();
    if(!Runtime::Snapshot(f)||!f.valid||!f.focused||!f.tick||now<f.tick||now-f.tick>150||
        !Copy(&source,native,sizeof(source))||!Rigid(source)||!Rigid(f.head))return false;
    DWORD process=0;GetWindowThreadProcessId(GetForegroundWindow(),&process);
    if(process!=GetCurrentProcessId())return false;
    tracked=Compose(Level(source),reference,f.head,Config().units);
    return Rigid(tracked);
}
using LadderEuler=Vec*(__fastcall*)(Vec*,const Mat*);
using LadderRawEuler=void(__fastcall*)(const Mat*,float*,float*,float*);
Vec* __fastcall LadderAttachAngles(Vec* output,const Mat* native,void* owner,void* ladder,float contactHeight){
    Mat tracked{};const bool local=LadderHeadMatrix(owner,native,tracked);
    const auto original=reinterpret_cast<LadderEuler>(game+0xC6040);
    Vec* result=original(output,local?&tracked:native);
    // At the base, stock BF2 insists on upward pitch even when the headset
    // is level and the player deliberately walks into the ladder. Supplying
    // a modest upward intent here retains the native contact/facing/space
    // checks. Never assist stepping off a roof or somebody looking down.
    Runtime::Frame f{};
    const float top=Value<float>(ladder,0x168),margin=Value<float>(game,0x4E7750);
    const bool bottom=std::isfinite(top+margin+contactHeight)&&margin>0&&margin<2&&contactHeight<=top-margin;
    if(local&&result&&bottom&&std::isfinite(result->y)&&result->y>=-.35f&&result->y<=.15f&&
       Runtime::Snapshot(f)&&f.valid&&f.focused&&f.hands[0].stickActive&&f.hands[0].stickY>.35f&&
       std::abs(f.hands[0].stickX)<.65f){
        result->y=-.35f;
        static unsigned assists=0;if(assists++<8)Log("LADDER radio-build: base contact assisted for level HMD + forward stick");
    }
    static unsigned reports=0;
    if(local&&reports<8){
        Vec before{};original(&before,native);++reports;
        Log("LADDER97 attach pitch native=%.2f headset=%.2f; native contact checks retained",before.y,result->y);
    }
    return result;
}
void __fastcall LadderClimbAngles(const Mat* native,float* yaw,float* pitch,float* roll,void* owner){
    Mat tracked{};const bool local=LadderHeadMatrix(owner,native,tracked);
    reinterpret_cast<LadderRawEuler>(game+0x2D9B40)(local?&tracked:native,yaw,pitch,roll);
    static unsigned reports=0;
    if(local&&reports<4){++reports;Log("LADDER97 climb direction uses headset pitch; native forward/back input retained");}
}
// Both thunks are CALL-site replacements, never function-entry detours.
// The profiled attachment caller saved the soldier at [EBP+8]. Its frame
// later reuses one byte of that slot, but only AFTER this exact call site.
__declspec(naked) void LadderAttachThunk(){__asm{
    pop eax
    push dword ptr [ebp-44h]
    push esi
    push dword ptr [ebp+8]
    push eax
    jmp LadderAttachAngles
}}
// The climb caller holds the soldier in ESI. Insert it after the two native
// stack arguments; the fastcall helper pops all three. ECX/EDX stay intact.
__declspec(naked) void LadderClimbThunk(){__asm{
    push ecx
    mov eax,dword ptr [esp+4]
    mov dword ptr [esp],eax
    mov eax,dword ptr [esp+8]
    mov dword ptr [esp+4],eax
    mov eax,dword ptr [esp+12]
    mov dword ptr [esp+8],eax
    mov dword ptr [esp+12],esi
    jmp LadderClimbAngles
}}
bool LadderCall(BYTE* call,void* hook){
    DWORD old=0;if(!VirtualProtect(call,5,PAGE_EXECUTE_READWRITE,&old))return false;
    const LONG displacement=static_cast<LONG>(static_cast<BYTE*>(hook)-(call+5));
    memcpy(call+1,&displacement,sizeof(displacement));
    DWORD ignored=0;VirtualProtect(call,5,old,&ignored);FlushInstructionCache(GetCurrentProcess(),call,5);return true;
}
bool InstallLadderLook(){
    const BYTE attachFrame[]={0x55,0x8B,0xEC,0x83,0xEC,0x78,0x53,0x56,0x8B,0xF1};
    const BYTE height[]={0xD9,0x5D,0xBC};
    const BYTE owner[]={0x05,0xA4,0xFE,0xFF,0xFF,0x89,0x45,0x08};
    const BYTE attach[]={0x8B,0xD0,0x8D,0x4D,0xC8,0xE8,0x4B,0x23,0xF0,0xFF,0x8B,0x40,0x04};
    const BYTE climbFrame[]={0x55,0x8B,0xEC,0x83,0xEC,0x60,0x56,0x8B,0xF1};
    const BYTE climb[]={0x8D,0x4D,0xFC,0x51,0x8D,0x55,0x0C,0x52,0x8D,0x55,0xF8,0x8B,0xC8,0xE8,0x55,0x65,0x11,0,0xD9,0x45,0x0C};
    const BYTE euler[]={0x55,0x8B,0xEC,0x83,0xEC,0x0C,0x56,0x8B,0xF1,0x8B,0xC2};
    if(!Match(game+0x1C3C0D,height,sizeof(height))||!Match(game+0x1C3B90,attachFrame,sizeof(attachFrame))||!Match(game+0x1C3C3E,owner,sizeof(owner))||
       !Match(game+0x1C3CEB,attach,sizeof(attach))||!Match(game+0x1C34F0,climbFrame,sizeof(climbFrame))||
       !Match(game+0x1C35D9,climb,sizeof(climb))||!Match(game+0xC6040,euler,sizeof(euler)))return false;
    const bool attached=LadderCall(game+0x1C3CF0,reinterpret_cast<void*>(&LadderAttachThunk));
    const bool climbing=LadderCall(game+0x1C35E6,reinterpret_cast<void*>(&LadderClimbThunk));
    Log("LADDER97 local headset look profile attach=%d climb=%d",attached?1:0,climbing?1:0);
    return attached&&climbing;
}
