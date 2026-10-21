// BF2 1.5 ground services, used only by private gear and the downed camera.
// The stock placement routine at +52A0B calls HeightmapCluster slot 27 with
// (x,z); +52AA1 calls the segment query with these exact arguments.
bool inventoryWorldProfile=false;
bool TerrainHeight(const Vec& p,float& height){
    if(!inventoryWorldProfile||!std::isfinite(p.x)||!std::isfinite(p.z))return false;
    __try{
        void* cluster=Value<void*>(game,0x607E28);void* table=Value<void*>(cluster);
        void* fn=Value<void*>(table,0x6C);
        static void* verified=nullptr;
        if(!cluster||!fn)return false;
        if(table!=verified){if(cluster!=Class("HeightmapCluster")||!Code(fn,(HMODULE)game))return false;verified=table;}
        height=reinterpret_cast<float(__thiscall*)(void*,float,float)>(fn)(cluster,p.x,p.z);
        return std::isfinite(height)&&height>-10000&&height<10000;
    }__except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
bool WorldSegmentObject(const Vec& start,const Vec& end,void* ignore,Vec& hit,void*& object){
    object=nullptr;
    if(!inventoryWorldProfile||!Value<void*>(game,0x608F58))return false;
    using Query=bool(__fastcall*)(const Vec*,const Vec*,void*,void**,unsigned,unsigned,bool,Vec*);
    __try{
        hit={};
        const bool found=reinterpret_cast<Query>(game+0x31B300)(&start,&end,ignore,&object,0,0,true,&hit);
        return found&&std::isfinite(hit.x)&&std::isfinite(hit.y)&&std::isfinite(hit.z)&&
            Length(Sub(hit,start))<=Length(Sub(end,start))+.1f;
    }__except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
bool WorldSegment(const Vec& start,const Vec& end,void* ignore,Vec& hit){
    void* object=nullptr;return WorldSegmentObject(start,end,ignore,hit,object);
}
bool InstallInventoryWorld(){
    const BYTE ray[]={0x55,0x8B,0xEC,0x83,0xEC,0x38,0x53,0x56,0x57};
    const BYTE ground[]={0x8B,0x0D,0x28,0x7E,0xA0,0x00};
    return Match(game+0x31B300,ray,sizeof(ray))&&Match(game+0x52A0B,ground,sizeof(ground));
}
