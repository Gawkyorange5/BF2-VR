// Extra native BundledMesh instances. Template references and matrices have
// session lifetime because queued BF2 draw packets borrow their addresses.
// Never reposition the game-owned mesh of an equipped or dropped weapon.
using BundlePrepare=void(__thiscall*)(void*,unsigned,unsigned,unsigned);
BundlePrepare originalBundlePrepare=nullptr;
bool gearProfile=false,gearSubmitted=false;
bool InventoryGearSupported(){return gearProfile;}
struct GearMesh {
    void* definition=nullptr;void* instance=nullptr;int slot=-1,parts=0;
    Mat world{},rest[32]{},matrices[32]{};bool ready=false;
    GearMesh* attachment=nullptr;void* attachmentWeaponDefinition=nullptr;Mat attachmentLocal{};bool attachmentChecked=false;
};
GearMesh gearMeshes[96]{};GearMesh* gearSlots[7]{};GearMesh* droppedGear[10]{};
void* hiddenHeldMesh=nullptr;void* hiddenC4Child=nullptr;void* hiddenSuppressor=nullptr;GearMesh* c4Held[2]{};

bool BundleLayout(void* definition,int& sub,int& parts){
    if(Value<void*>(definition)!=render+0x21A760||!Value<BYTE>(definition,0x4C))return false;
    const auto g=Value<std::uintptr_t>(definition,0x114),e=Value<std::uintptr_t>(definition,0x118);
    const auto m=Value<std::uintptr_t>(definition,0xD0),me=Value<std::uintptr_t>(definition,0xD4);
    parts=Value<int>(definition,0x120);
    if(!g||e<g||e-g<32||e-g>1024||(e-g)%32||!m||me<m||me-m<16||me-m>512||(me-m)%16||parts<1||parts>32)return false;
    sub=e-g>=64&&me-m>=32?1:0;
    const auto lod=Value<std::uintptr_t>(reinterpret_cast<void*>(g+sub*32),4),le=Value<std::uintptr_t>(reinterpret_cast<void*>(g+sub*32),8);
    const auto mat=Value<std::uintptr_t>(reinterpret_cast<void*>(m+sub*16),4),mate=Value<std::uintptr_t>(reinterpret_cast<void*>(m+sub*16),8);
    return lod&&le>lod&&le-lod<=72*16&&(le-lod)%72==0&&mat&&mate>mat&&mate-mat<=16*16&&(mate-mat)%16==0;
}
GearMesh* GearFor(void* source,int slot){
    if(Value<void*>(source)!=render+0x21B350)return nullptr;
    void* definition=Value<void*>(source,0x258);int sub=0,parts=0;
    if(Value<void*>(definition)!=render+0x21A760||!Value<BYTE>(definition,0x4C))return nullptr;
    // Each cached definition owns a native reference for the session, and
    // its instance already has validated layout/part counts. Reuse it before
    // walking the immutable LOD/material arrays again for every carried item.
    for(auto& mesh:gearMeshes)if(mesh.definition==definition&&mesh.slot==slot)return mesh.ready?&mesh:nullptr;
    if(!BundleLayout(definition,sub,parts))return nullptr;
    if(slot==32||slot==33)sub=0; // held C4 copies use detailed first-person geometry
    GearMesh* result=nullptr;for(auto& mesh:gearMeshes)if(!mesh.definition){result=&mesh;break;}
    if(!result)return nullptr;
    Mat rest[32];for(auto& m:rest)m=Identity();
    // Source transforms are optional for inactive items; factory identity is
    // BF2's own default. Only capture part articulation from the third-person
    // layout, so a pulled 1P reload magazine cannot become the holstered pose.
    if(Value<int>(source,0x28C)==sub){
        void* array=Value<void*>(source,0x25C);Mat root{};
        if(Read(Value<void*>(array),0,root)&&Rigid(root))for(int i=1;i<parts;++i){
            Mat part{};if(Read(Value<void*>(array,i*4),0,part)&&Rigid(part)){
                Mat relative=Mul(part,Inverse(root));
                if(Rigid(relative)&&Length(Position(relative))<1.5f)rest[i]=relative;
            }
        }
    }
    reinterpret_cast<NativeRef>(Value<void*>(Value<void*>(definition),0))(definition);
    void* instance=reinterpret_cast<MeshFactory>(render+0xE96E0)(definition);
    if(!instance){reinterpret_cast<NativeRef>(Value<void*>(Value<void*>(definition),8))(definition);return nullptr;}
    auto& mesh=*result;mesh.definition=definition;mesh.instance=instance;mesh.slot=slot;mesh.parts=parts;
    if(Value<void*>(instance)!=render+0x21B350)return nullptr;
    memcpy(mesh.rest,rest,sizeof(rest));mesh.world=Identity();for(auto& m:mesh.matrices)m=Identity();
    reinterpret_cast<MeshPart>(render+0x104F90)(instance,sub);
    reinterpret_cast<MeshOpacity>(render+0x1077F0)(instance,1.f);
    if(Value<int>(instance,0x28C)!=sub||!Value<void*>(instance,0x284)||!Value<void*>(instance,0x288))return nullptr;
    mesh.ready=true;Log("GEAR51 native mesh created slot=%d template=%p instance=%p geometry=%d parts=%d",slot,definition,instance,sub,parts);
    return &mesh;
}
void* GearChildNamed(void* weapon,const char* wanted){
    void* getter=Value<void*>(Value<void*>(weapon),0xEC);if(!Code(getter,(HMODULE)game))return nullptr;
    void* node=reinterpret_cast<void*(__thiscall*)(void*)>(getter)(weapon);
    for(int i=0;node&&i<64;++i){
        char name[80]{};
        if(Value<void*>(node,0x28)==weapon&&ReloadName(node,name)&&!strcmp(name,wanted))return node;
        void* next=Value<void*>(node,0x2C);if(next==node)break;node=next;
    }
    return nullptr;
}
void* C4DetonatorObject(void* weapon){return GearChildNamed(weapon,Dzb2C4(weapon)?"c4_ex_d":"c4_detonator");}
void PrepareGearSuppressor(void* weapon,GearMesh& parent){
    // Suppressed and unsuppressed weapons can share the same root geometry.
    // Resolve attachments again when its native weapon definition changes.
    void* definition=Value<void*>(weapon,0x14);
    if(parent.attachmentWeaponDefinition!=definition){
        parent.attachmentWeaponDefinition=definition;parent.attachment=nullptr;parent.attachmentChecked=false;
    }
    if(!parent.attachmentChecked){
        void* child=GearChildNamed(weapon,"silencer");
        if(!child){parent.attachmentChecked=true;return;}
        Mat local{};
        // Object+CC is the local transform used by BF2+31DB50 to compose
        // this direct child's world from its parent. Read it, never move the
        // live attachment or borrow its mutable world matrix for a draw.
        if(!VehicleObject(child)||Value<void*>(Value<void*>(child),0x80)!=game+0x31DB50||
           !Read(Value<void*>(child,0xCC),0,local)||!Rigid(local)||Length(Position(local))>1.f)return;
        auto* extra=GearFor(Value<void*>(child,0x38),64+parent.slot);
        if(!extra)return;
        parent.attachment=extra;parent.attachmentLocal=local;parent.attachmentChecked=true;
        Log("GEAR120 suppressor attached to private gear slot=%d",parent.slot);
    }
    if(auto* extra=parent.attachment){
        extra->world=Mul(parent.attachmentLocal,parent.world);
        for(int p=0;p<extra->parts;++p)extra->matrices[p]=Mul(extra->rest[p],extra->world);
        reinterpret_cast<MeshWorld>(render+0x104390)(extra->instance,&extra->world);
        reinterpret_cast<MeshBones>(render+0x1043E0)(extra->instance,extra->matrices,extra->parts);
    }
}
void SubmitGearMesh(GearMesh* mesh,unsigned a,unsigned b,unsigned c){
    if(!mesh)return;
    const DWORD stale=0xffffffff;Copy(static_cast<BYTE*>(mesh->instance)+0x20,&stale,4);
    originalBundlePrepare(mesh->instance,a,b,c);
    if(auto* extra=mesh->attachment){
        Copy(static_cast<BYTE*>(extra->instance)+0x20,&stale,4);
        originalBundlePrepare(extra->instance,a,b,c);
    }
}
void PrepareC4Gear(void* owner){
    if(!c4Profile)return;
    void* weapon=InventoryItem(owner,Value<int>(owner,0x1E0));if(!IsPhysicalC4(weapon))return;
    void* detonator=C4DetonatorObject(weapon);if(!detonator)return;
    void* sources[2]={Value<void*>(detonator,0x38),Value<void*>(weapon,0x38)};
    GearMesh* meshes[2]={GearFor(sources[0],33),GearFor(sources[1],32)};
    if(!meshes[0]||!meshes[1])return;
    const bool dz=Dzb2C4(weapon);
    // c4_ex has one charge part and a three-part authored detonator.
    // Refuse an unexpected mesh layout instead of applying another model's pose.
    if(dz&&(meshes[0]->parts!=3||meshes[1]->parts!=1))return;
    const bool visible[2]={C4DetonatorVisible(owner,weapon),!InventoryStowed(owner)};
    for(int h=0;h<2;++h){
        if(!visible[h]||!pairFrame.hands[h].gripValid)continue;
        auto& mesh=*meshes[h];
        const Mat grip=Compose(TrackingBase(views[0].source),reference,pairFrame.hands[h].grip,Config().units);
        Mat offset=h?(dz?dzC4ChargeFromPalm:c4ChargeFromPalm):
            (dz?dzC4DetonatorLeftFromPalm:c4DetonatorLeftFromPalm);
        mesh.world=Mul(offset,grip);
        for(int p=0;p<mesh.parts;++p){
            Mat rest=Identity();
            if(!h&&p==1)rest=dz?dzC4DetonatorPart1:c4DetonatorPart1;
            if(!h&&dz&&p==2)rest=dzC4DetonatorPart2;
            mesh.matrices[p]=Mul(rest,mesh.world);
        }
        reinterpret_cast<MeshWorld>(render+0x104390)(mesh.instance,&mesh.world);
        reinterpret_cast<MeshBones>(render+0x1043E0)(mesh.instance,mesh.matrices,mesh.parts);
        c4Held[h]=&mesh;
    }
    hiddenHeldMesh=sources[1];hiddenC4Child=sources[0];
    static unsigned reports=0;if(reports++<4)Log("C452 held meshes right charge=%p left detonator=%p",meshes[1]->instance,meshes[0]->instance);
}
void BeginInventoryGear(){
    for(auto& slot:gearSlots)slot=nullptr;for(auto& slot:droppedGear)slot=nullptr;hiddenHeldMesh=hiddenC4Child=hiddenSuppressor=nullptr;c4Held[0]=c4Held[1]=nullptr;gearSubmitted=false;
    if(!gearProfile||!LocalAlive())return;
    void* owner=nullptr;void* identity=nullptr;if(!FootOwner(owner,identity))return;
    if(InventoryStowed(owner)){
        void* held=InventoryItem(owner,Value<int>(owner,0x1E0));hiddenHeldMesh=Value<void*>(held,0x38);
        if(IsPhysicalC4(held))hiddenC4Child=Value<void*>(C4DetonatorObject(held),0x38);
        hiddenSuppressor=Value<void*>(GearChildNamed(held,"silencer"),0x38);
    }
    PrepareC4Gear(owner);
    UpdateInventoryDrops(pairFrame.tick);
    for(auto& d:inventoryDrops)if(d.active&&d.owner==owner&&d.identity==identity){
        auto* mesh=GearFor(Value<void*>(d.weapon,0x38),16+d.item);if(!mesh)continue;
        mesh->world=d.world;
        for(int p=0;p<mesh->parts;++p)mesh->matrices[p]=Mul(mesh->rest[p],mesh->world);
        reinterpret_cast<MeshWorld>(render+0x104390)(mesh->instance,&mesh->world);
        reinterpret_cast<MeshBones>(render+0x1043E0)(mesh->instance,mesh->matrices,mesh->parts);
        PrepareGearSuppressor(d.weapon,*mesh);
        droppedGear[d.item]=mesh;
    }
    const bool preview=SlotPreviewCurrent();const auto& slots=preview?slotLayoutPreview:bodySlots;
    if((!preview&&!Tuning().bodySlots)||!slots.valid||slots.owner!=owner||slots.identity!=identity||slots.origin!=pairFrame.origin)return;
    const int equipped=Value<int>(owner,0x1E0);
    for(int i=0;i<7;++i){
        const int item=slots.items[i];if(!item||(!preview&&(InventoryItemAway(item,pairFrame.tick)||(!InventoryStowed(owner)&&equipped==item))))continue;
        void* weapon=InventoryItem(owner,item);if(!weapon)continue;
        auto* mesh=GearFor(Value<void*>(weapon,0x38),i);if(!mesh)continue;
        const Mat anchor=Compose(TrackingBase(views[0].source),reference,slots.anchors[i],Config().units);
        const Vec direction=i==5?Vec{1,0,0}:i==4?Vec{0,1,0}:(i<2||i==6)?Vec{0,1,0}:Vec{0,-1,0};
        Mat local=RotateBetween({0,0,1},direction);
        // Grip is at the reachable slot, barrel points down at the hip. The
        // full native geometry supplies materials and per-model proportions.
        mesh->world=Mul(local,anchor);
        for(int p=0;p<mesh->parts;++p)mesh->matrices[p]=Mul(mesh->rest[p],mesh->world);
        reinterpret_cast<MeshWorld>(render+0x104390)(mesh->instance,&mesh->world);
        reinterpret_cast<MeshBones>(render+0x1043E0)(mesh->instance,mesh->matrices,mesh->parts);
        PrepareGearSuppressor(weapon,*mesh);
        gearSlots[i]=mesh;
    }
    static void* lastOwner=nullptr;static unsigned reports=0;
    if(owner!=lastOwner||reports<3){
        unsigned available=0;for(auto* slot:gearSlots)available+=slot!=nullptr;
        Log("GEAR51 prepared owner=%p meshes=%u hiddenHeld=%p empty=%d",owner,available,hiddenHeldMesh,InventoryStowed(owner)?1:0);lastOwner=owner;++reports;
    }
}
void __fastcall BundlePrepareHook(void* self,void*,unsigned a,unsigned b,unsigned c){
    if(eyeActive&&!scopePass){
        SubmitDownedBody(a,b,c);
        if(!gearSubmitted){
            gearSubmitted=true;
            for(auto* mesh:gearSlots)SubmitGearMesh(mesh,a,b,c);
            for(auto* mesh:c4Held)SubmitGearMesh(mesh,a,b,c);
            for(auto* mesh:droppedGear)SubmitGearMesh(mesh,a,b,c);
        }
        if((hiddenHeldMesh&&self==hiddenHeldMesh)||(hiddenC4Child&&self==hiddenC4Child)||(hiddenSuppressor&&self==hiddenSuppressor))return;
    }
    originalBundlePrepare(self,a,b,c);
}
bool InstallInventoryGear(){
    const BYTE prepare[]={0x55,0x8B,0xEC,0x81,0xEC,0x4C,1,0,0};
    if(Value<void*>(render,0x21A760+9*4)!=render+0xE96E0||
       Value<void*>(render,0x21B350+6*4)!=render+0x105550||
       Value<void*>(render,0x21B350+20*4)!=render+0x104390||
       Value<void*>(render,0x21B350+21*4)!=render+0x1043E0||
       Value<void*>(render,0x21B350+27*4)!=render+0x104F90||
       Value<void*>(render,0x21B350+61*4)!=render+0x1077F0)return false;
    return Jump(render+0x105550,prepare,sizeof(prepare),reinterpret_cast<void*>(&BundlePrepareHook),reinterpret_cast<void**>(&originalBundlePrepare));
}
