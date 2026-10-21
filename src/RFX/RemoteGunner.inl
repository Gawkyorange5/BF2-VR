// r130: DZB2's laptops put native gun/camera bundles 300-400 metres from
// the occupied PCO. Resolve ONLY these exact local-seat hierarchies. The
// orbit bundle is the aiming frame; turret rotation remains feedback.
// No template edits, native function calls, or all-object scans.
struct RemoteGunnerProfile {
    const char* seat;const char* root;int occupiedNode;
    const char* path[5];
};
const RemoteGunnerProfile* RemoteGunnerFor(const char* name){
    static const RemoteGunnerProfile profiles[]={
        {"uav_pcbase","uav_pcbase",-1,{"uav_pcbase_baserotational","uav_pcbase_controller","uav_pcbase_weaponsrotational","uav_pcbase_camera",nullptr}},
        {"uav_pcbase_gunner","uav_pcbase",1,{"uav_pcbase_baserotational","uav_pcbase_gunner","uav_pcbase_gunner_weaponsrotational","uav_pcbase_gunner_camera",nullptr}},
        {"uav_uavbase","uav_uavbase",-1,{"uav_uavbase_baserotational","uav_uavbase_controller","uav_uavbase_weaponsrotational","uav_uavbase_camera",nullptr}},
        {"uav_uavbase_gunner","uav_uavbase",1,{"uav_uavbase_baserotational","uav_uavbase_gunner","uav_uavbase_gunner_controller","uav_uavbase_gunner_weaponsrotational","uav_uavbase_gunner_camera"}},
        {"uav_bomberbase","uav_bomberbase",-1,{"uav_bomberbase_baserotational","uav_bomberbase_weaponsrotational","uav_bomberbase_camera",nullptr,nullptr}},
        {"uav_bomberbase_copilot","uav_bomberbase",1,{"uav_bomberbase_baserotational","uav_bomberbase_copilot","uav_bomberbase_copilot_weaponsrotational","uav_bomberbase_copilot_camera",nullptr}},
    };
    for(const auto& p:profiles)if(!strcmp(name,p.seat))return &p;
    return nullptr; // sv_cg_laptop is a Survival activation terminal.
}
struct RemoteGunnerNode {void* object=nullptr;void* parent=nullptr;void* definition=nullptr;void* table=nullptr;};
struct RemoteGunnerCache {
    const RemoteGunnerProfile* profile=nullptr;
    void* owner=nullptr;void* weak=nullptr;void* seat=nullptr;void* root=nullptr;void* definition=nullptr;
    RemoteGunnerNode nodes[5]{};unsigned count=0;ULONGLONG attempt=0;bool ready=false;
} remoteGunner;
void* RemoteGunnerChild(void* parent,const char* wanted){
    void* child=Value<void*>(parent,0x30);
    for(unsigned i=0;child&&i<256;++i){
        if(Value<void*>(child,0x28)!=parent)return nullptr;
        char name[80]{};
        if(!(Value<unsigned>(child,4)&1)&&VehicleTemplateName(Value<void*>(child,0x14),name)&&!strcmp(name,wanted))return child;
        void* next=Value<void*>(child,0x2C);if(next==child)return nullptr;child=next;
    }
    return nullptr;
}
bool ReadRemoteGunner(VehicleSample& sample){
    const auto* profile=RemoteGunnerFor(sample.name);
    if(!profile)return false;
    auto& c=remoteGunner;const auto now=GetTickCount64();
    const bool same=c.profile==profile&&c.owner==sample.owner&&c.weak==sample.weak&&c.seat==sample.seat&&
        c.root==sample.root&&c.definition==sample.definition;
    if(!same){c={};c.profile=profile;c.owner=sample.owner;c.weak=sample.weak;c.seat=sample.seat;c.root=sample.root;c.definition=sample.definition;}
    if(c.ready){
        // Validate every cached edge before using the two live transforms.
        // A destroyed/replaced native bundle invalidates the entire path.
        for(unsigned i=0;i<c.count;++i){const auto& n=c.nodes[i];
            if(Value<void*>(n.object)!=n.table||Value<void*>(n.object,0x14)!=n.definition||
               Value<void*>(n.object,0x28)!=n.parent||(Value<unsigned>(n.object,4)&1)){c.ready=false;c.attempt=now;break;}
        }
    }
    if(!c.ready){
        if(c.attempt&&now>=c.attempt&&now-c.attempt<1000)return false;
        c.attempt=now;c.count=0;
        char rootName[80]{};
        if(!VehicleName(Value<void*>(sample.root,0x14),rootName)||strcmp(rootName,profile->root))return false;
        if(profile->occupiedNode<0&&sample.seat!=sample.root)return false;
        void* parent=sample.root;
        for(unsigned i=0;i<5&&profile->path[i];++i){
            void* object=RemoteGunnerChild(parent,profile->path[i]);if(!object)return false;
            if(profile->occupiedNode==int(i)&&object!=sample.seat)return false;
            c.nodes[i]={object,parent,Value<void*>(object,0x14),Value<void*>(object)};parent=object;++c.count;
        }
        if(c.count<3)return false;
        c.ready=true;
        Log("REMOTE130 resolved seat=%s nodes=%u orbit=%p camera=%p; native weapons retained",sample.name,c.count,c.nodes[0].object,c.nodes[c.count-1].object);
    }
    Mat orbit{},camera{};
    if(!Read(c.nodes[0].object,0x88,orbit)||!Rigid(orbit)||
       !Read(c.nodes[c.count-1].object,0x88,camera)||!Rigid(camera))return false;
    sample.world=orbit;sample.remoteCamera=c.nodes[c.count-1].object;sample.remoteWorld=camera;
    sample.role=VehicleRole::Aimed;sample.aircraft=true;sample.freeLook=false;
    sample.rolePending=false;sample.roleEvidence=profile->path[c.count-1];
    return true;
}
