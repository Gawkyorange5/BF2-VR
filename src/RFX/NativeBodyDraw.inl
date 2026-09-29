// Included in the adapter's private namespace after NativeBody's state.
// The native effect draw receives the render packet. Its palette and world
// parameter pointers identify our extra instance without guessing draw sizes.
#include "NativeBodyMasks.inl"
#include "NativeDetailHandMasks.inl"
using EffectDraw=void(__thiscall*)(void*,void*);
EffectDraw originalBodyDraw=nullptr;
IDirect3DIndexBuffer9* bodyIndexBuffers[3][sizeof(bodyMaskProfiles)/sizeof(bodyMaskProfiles[0])]{};
IDirect3DIndexBuffer9* detailHandBuffers[sizeof(detailHandProfiles)/sizeof(detailHandProfiles[0])]{};
const DetailHandProfile* bodyEyeDetailProfiles[4]{};
unsigned bodyDrawCalls=0,bodyEyePackets=0,bodyEyeDraws=0,bodyEyeHandMask=0,bodyEyeHiddenArms=0,bodyEyeKeptArms=0;
unsigned bodyEyeDetailDraws=0;
bool bodyRenderConnected=false,bodyRenderDetails=false,bodyDetailsChecked=false;
struct BodyFilterCounts {unsigned long long draws=0,lookups=0,skipped=0;ULONGLONG since=0;} bodyFilterCounts;
void ReportBodyFilter(){
    const auto now=GetTickCount64();
    if(!bodyFilterCounts.since){bodyFilterCounts.since=now;return;}
    if(now-bodyFilterCounts.since<5000)return;
    if(bodyFilterCounts.draws)Log("PERF body filter eligible=%llu ownerLookups=%llu avoided=%llu fast=%d",bodyFilterCounts.draws,bodyFilterCounts.lookups,bodyFilterCounts.skipped,Config().fastBodyFilter?1:0);
    bodyFilterCounts={0,0,0,now};
}

void ReleaseBodyMasks(){
    for(auto& list:bodyIndexBuffers)for(auto& buffer:list)if(buffer){buffer->Release();buffer=nullptr;}
    for(auto& buffer:detailHandBuffers)if(buffer){buffer->Release();buffer=nullptr;}
}
unsigned BodyPaletteHash(const void* data,unsigned bytes){
    BYTE copy[26*68];if(bytes>sizeof(copy)||!Copy(copy,data,bytes))return 0;
    unsigned h=2166136261u;
    for(unsigned i=0;i<bytes;++i)h=(h^copy[i])*16777619u;
    return h;
}
unsigned SkinMaterials(void* instance,BYTE*& materials){
    // Native prepare +1A854C uses this vector's begin/end and divides by 1C8.
    // Do not probe a guessed four materials beyond a light kit's two entries.
    void* list=Value<void*>(instance,0x38);
    const auto begin=Value<std::uintptr_t>(list,4),end=Value<std::uintptr_t>(list,8);
    materials=nullptr;
    if(!begin||end<=begin||(end-begin)%0x1C8||end-begin>4*0x1C8)return 0;
    materials=reinterpret_cast<BYTE*>(begin);return static_cast<unsigned>((end-begin)/0x1C8);
}
bool SkinPalette(void* instance,unsigned part,unsigned& count,unsigned& hash){
    void* geometry=Value<void*>(instance,0x34);
    const auto begin=Value<std::uintptr_t>(geometry,0x44+part*16);
    const auto end=Value<std::uintptr_t>(geometry,0x48+part*16);
    if(!begin||end<begin||(end-begin)%68||end-begin<68||end-begin>26*68)return false;
    count=static_cast<unsigned>((end-begin)/68);
    hash=BodyPaletteHash(reinterpret_cast<const void*>(begin),count*68);return hash!=0;
}
const BodyMaskProfile* BodyMaskFor(void* material,unsigned part){
    if(Value<void*>(material)!=render+0x21BFC0||Value<unsigned>(material,0x190)!=D3DPT_TRIANGLELIST)return nullptr;
    unsigned count=0,hash=0;if(!SkinPalette(bodyInstance,part,count,hash))return nullptr;
    const unsigned lod=Value<unsigned>(bodyInstance,0x40);
    const unsigned vertices=Value<unsigned>(material,0x24),triangles=Value<unsigned>(material,0x20);
    for(const auto& p:bodyMaskProfiles)
        if(p.lod==lod&&p.material==part&&p.vertices==vertices&&p.triangles==triangles&&p.boneCount==count&&p.paletteHash==hash)return &p;
    static unsigned misses=0;
    if(misses++<12)Log("FULLBODY3 mask profile rejected lod=%u part=%u vertices=%u triangles=%u bones=%u hash=%08X base=%u start=%u",lod,part,vertices,triangles,count,hash,Value<unsigned>(material,0x18),Value<unsigned>(material,0x1C));
    return nullptr;
}
const DetailHandProfile* DetailMaskFor(void* material,unsigned part){
    if(Value<void*>(material)!=render+0x21BFC0||Value<unsigned>(material,0x190)!=D3DPT_TRIANGLELIST)return nullptr;
    unsigned count=0,hash=0;if(!SkinPalette(bodySource,part,count,hash))return nullptr;
    const unsigned lod=Value<unsigned>(bodySource,0x40);
    const unsigned vertices=Value<unsigned>(material,0x24),triangles=Value<unsigned>(material,0x20);
    for(const auto& p:detailHandProfiles)
        if(p.lod==lod&&p.material==part&&p.vertices==vertices&&p.triangles==triangles&&p.boneCount==count&&p.paletteHash==hash)return &p;
    static unsigned misses=0;
    if(misses++<8)Log("FULLBODY8 detail profile rejected lod=%u part=%u vertices=%u triangles=%u bones=%u hash=%08X",lod,part,vertices,triangles,count,hash);
    return nullptr;
}
bool DetailHandsAvailable(const DetailHandProfile** profiles=nullptr){
    if(!bodySource||Value<int>(bodySource,0x3C)!=0)return false;
    BYTE* materials=nullptr;const unsigned parts=SkinMaterials(bodySource,materials);
    if(!parts)return false;
    unsigned hands=0;
    for(unsigned part=0;part<parts;++part){
        void* material=materials+part*0x1C8;
        const auto* p=DetailMaskFor(material,part);
        // Recognize every 1P section, including heavy-kit sleeve armor, before
        // hiding 3P hands. Unknown mesh data retains the working r7 approach.
        if(!p)return false;
        if(profiles)profiles[part]=p;
        if(p->kept)hands|=p->handMask;
    }
    return hands==3;
}
bool ConnectedBodyAvailable(){
    BYTE* materials=nullptr;const unsigned parts=SkinMaterials(bodyInstance,materials);
    if(!parts)return false;
    unsigned hands=0;
    // A reduced LOD can omit the independent wrist/finger skin weights.
    // Require recognizable left AND right hand sections before drawing any
    // connected arms; otherwise this eye uses the torso-only masks and 1P.
    for(unsigned part=0;part<parts;++part){
        void* material=materials+part*0x1C8;
        if(Value<void*>(material)!=render+0x21BFC0)continue;
        const BodyMaskProfile* profile=BodyMaskFor(material,part);
        if(profile&&profile->wholeKept)hands|=profile->handMask;
    }
    return hands==3;
}
IDirect3DIndexBuffer9* MaskBuffer(IDirect3DDevice9* device,IDirect3DIndexBuffer9*& buffer,const unsigned short* indices,unsigned kept){
    if(buffer)return buffer;
    const unsigned bytes=kept*3*sizeof(unsigned short);
    if(!bytes||FAILED(device->CreateIndexBuffer(bytes,D3DUSAGE_WRITEONLY,D3DFMT_INDEX16,D3DPOOL_DEFAULT,&buffer,nullptr)))return nullptr;
    void* dst=nullptr;
    if(FAILED(buffer->Lock(0,bytes,&dst,0))){buffer->Release();buffer=nullptr;return nullptr;}
    memcpy(dst,indices,bytes);
    if(FAILED(buffer->Unlock())){buffer->Release();buffer=nullptr;return nullptr;}
    return buffer;
}
unsigned BodyKept(const BodyMaskProfile& p){return bodyRenderDetails?p.sleeveKept:bodyRenderConnected?p.wholeKept:p.kept;}
IDirect3DIndexBuffer9* BodyIndices(IDirect3DDevice9* device,const BodyMaskProfile& p){
    auto& buffer=bodyIndexBuffers[bodyRenderDetails?2:bodyRenderConnected?1:0][&p-bodyMaskProfiles];
    return MaskBuffer(device,buffer,bodyRenderDetails?p.sleeveIndices:bodyRenderConnected?p.wholeIndices:p.indices,BodyKept(p));
}
IDirect3DIndexBuffer9* DetailIndices(IDirect3DDevice9* device,const DetailHandProfile& p){
    auto& buffer=detailHandBuffers[&p-detailHandProfiles];const bool existed=buffer!=nullptr;
    auto* result=MaskBuffer(device,buffer,p.indices,p.kept);
    if(!existed&&result)Log("FULLBODY8 detailed hand section model=%s lod=%u part=%u kept=%u hands=%u",p.name,p.lod,p.material,p.kept,p.handMask);
    return result;
}
void PrepareDetailHands(IDirect3DDevice9* device){
    if(bodyDetailsChecked)return;
    bodyDetailsChecked=true;
    if(!bodyDetailTargets||!bodyRenderConnected||!DetailHandsAvailable(bodyEyeDetailProfiles))return;
    // Allocate both hand sections BEFORE the body omits either native hand.
    for(const auto* p:bodyEyeDetailProfiles)if(p&&p->kept&&!DetailIndices(device,*p))return;
    bodyRenderDetails=true;
}
bool DrawSkinMask(void* effect,void* material,IDirect3DIndexBuffer9* filtered,unsigned vertices,unsigned kept){
    IDirect3DDevice9* device=Value<IDirect3DDevice9*>(effect,0x14C);
    void* state=Value<void*>(render,0x23B984);if(!device||!state||!filtered)return false;
    // Keep the native vertex stream, shader, texture, palette and base vertex.
    // Only our private index list changes. Never mutate a shared game mesh.
    reinterpret_cast<void(__thiscall*)(void*)>(render+0x26CB0)(state);
    IDirect3DIndexBuffer9* previous=nullptr;
    if(FAILED(device->GetIndices(&previous)))return false;
    HRESULT result=E_FAIL;
    if(SUCCEEDED(device->SetIndices(filtered)))
        result=device->DrawIndexedPrimitive(D3DPT_TRIANGLELIST,Value<int>(material,0x18),0,vertices,0,kept);
    const HRESULT restored=device->SetIndices(previous);if(previous)previous->Release();
    if(FAILED(result)||FAILED(restored)){
        static unsigned errors=0;if(++errors<=8||errors%600==1)
            Log("FULLBODY8 filtered draw failure eye=%d frame=%llu hr=%08X restore=%08X",activeEye,pairFrame.id,(unsigned)result,(unsigned)restored);
    }
    return SUCCEEDED(result);
}
void __fastcall BodyDrawHook(void* effect,void*,void* packet){
    if(scopePass){originalBodyDraw(effect,packet);return;}
    // Packet identity is cheap and immutable for this draw. Reject unrelated
    // world/remote/gun packets before Class("PlayerManager") and ownership
    // traversal, while retaining every ownership/palette check for our hands.
    const void* worldParameter=eyeActive&&bodyReady?Value<void*>(packet,0x118):nullptr;
    const bool connected=eyeActive&&bodyReady&&bodyRenderConnected;
    const bool candidate=connected&&(!Config().fastBodyFilter||
        (bodySource&&worldParameter==static_cast<BYTE*>(bodySource)+0x360));
    if(connected){++bodyFilterCounts.draws;if(candidate)++bodyFilterCounts.lookups;else ++bodyFilterCounts.skipped;}
    if(candidate&&bodyOwner==Soldier()&&bodySource&&
       Value<void*>(bodyOwner,0x38)==bodySource&&Value<int>(bodySource,0x3C)==0&&
       worldParameter==static_cast<BYTE*>(bodySource)+0x360){
        // Exact local soldier 1P instance AND its material/palette. The gun is
        // a different instance, so its packets continue through unchanged.
        const auto palette=Value<std::uintptr_t>(packet,0x4C);
        const auto first=reinterpret_cast<std::uintptr_t>(bodySource)+0x180;
        BYTE* materials=nullptr;const unsigned parts=SkinMaterials(bodySource,materials);
        if(parts&&palette>=first&&palette<first+parts*0x68&&(palette-first)%0x68==0&&
           Value<void*>(packet)==materials+((palette-first)/0x68)*0x1C8){
            const unsigned part=static_cast<unsigned>((palette-first)/0x68);
            if(bodyRenderDetails&&bodyEyeDraws){
                const auto* p=bodyEyeDetailProfiles[part];
                // Recheck the exact current packet; do not crop unknown data.
                if(p&&p==DetailMaskFor(Value<void*>(packet),part)){
                    if(!p->kept){++bodyEyeHiddenArms;return;}
                    auto* device=Value<IDirect3DDevice9*>(effect,0x14C);
                    if(device&&DrawSkinMask(effect,Value<void*>(packet),DetailIndices(device,*p),p->vertices,p->kept)){
                        ++bodyEyeDetailDraws;bodyEyeHandMask|=p->handMask;return;
                    }
                }
                // If a draw fails after preflight, keep its complete native
                // 1P part visible. It is preferable to losing a tracked hand.
                ++bodyEyeKeptArms;originalBodyDraw(effect,packet);return;
            }
            // Native-hand mode only suppresses 1P after both 3P hands drew.
            if(bodyEyeDraws&&bodyEyeHandMask==3){++bodyEyeHiddenArms;return;}
            ++bodyEyeKeptArms;
        }
    }
    if(!eyeActive||!bodyReady||!bodyInstance||worldParameter!=static_cast<BYTE*>(bodyInstance)+0x360){originalBodyDraw(effect,packet);return;}
    ++bodyEyePackets;
    // A packet's +4C points into this instance's per-material palette pointers.
    const auto palette=Value<std::uintptr_t>(packet,0x4C);
    const auto first=reinterpret_cast<std::uintptr_t>(bodyInstance)+0x180;
    if(palette<first||palette>=first+4*0x68||(palette-first)%0x68)return;
    const unsigned part=static_cast<unsigned>((palette-first)/0x68);
    void* material=Value<void*>(packet);
    BYTE* materials=nullptr;const unsigned parts=SkinMaterials(bodyInstance,materials);
    if(part>=parts||material!=materials+part*0x1C8)return;
    const BodyMaskProfile* profile=BodyMaskFor(material,part);
    // Unsupported data affects only this extra local mesh. Never draw its
    // unmasked head or change a shared material/index buffer.
    if(!profile)return;
    IDirect3DDevice9* device=Value<IDirect3DDevice9*>(effect,0x14C);
    if(!device)return;
    PrepareDetailHands(device);
    const unsigned kept=BodyKept(*profile);if(!kept)return;
    const bool drawn=DrawSkinMask(effect,material,BodyIndices(device,*profile),profile->vertices,kept);
    if(drawn){++bodyEyeDraws;if(bodyRenderConnected&&!bodyRenderDetails)bodyEyeHandMask|=profile->handMask;RememberBodyDepth(device);}
    ++bodyDrawCalls;
    if(bodyDrawCalls<=6||bodyDrawCalls%3600==1)
        Log("FULLBODY8 masked draw eye=%d frame=%llu model=%s lod=%u part=%u connected=%d detailHands=%d kept=%u drawn=%d",activeEye,pairFrame.id,profile->name,profile->lod,profile->material,bodyRenderConnected?1:0,bodyRenderDetails?1:0,kept,drawn?1:0);
}
bool InstallBodyDraw(){
    const BYTE entry[]={0x55,0x8B,0xEC,0x51,0x53,0x57};
    const BYTE flush[]={0x55,0x8B,0xEC,0x83,0xEC,8,0xA1};
    const BYTE materialCount[]={0x8B,0x48,8,0x2B,0xCA,0xB8,0xEF,0x23,0xB8,0x8F,0xF7,0xE9};
    // Inspected effect draw +4B720: packet[0] is native material; primitive
    // count / base vertex / vertex count are its getters at slots 4 / 6 / 2.
    if(Value<void*>(render,0x1E2958+6*4)!=render+0x4B720||!Match(render+0x26CB0,flush,sizeof(flush))||!Match(render+0x1A855A,materialCount,sizeof(materialCount))||
       Value<void*>(render,0x21BFC0+2*4)!=render+0x112FA0||Value<void*>(render,0x21BFC0+4*4)!=render+0x112FB0||
       Value<void*>(render,0x21BFC0+5*4)!=render+0x112FC0||Value<void*>(render,0x21BFC0+6*4)!=render+0x52530||
       Value<void*>(render,0x21BFC0+7*4)!=render+0xEDC70||Value<void*>(render,0x21BFC0+8*4)!=render+0x52540)return false;
    return Jump(render+0x4B720,entry,sizeof(entry),reinterpret_cast<void*>(&BodyDrawHook),reinterpret_cast<void**>(&originalBodyDraw));
}
