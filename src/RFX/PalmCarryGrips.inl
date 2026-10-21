// r54 palm attachments from each item's native standing pose.
const Mat knifeFromPalm={{{0.927163533f,-0.323466252f,-0.189043293f,0.000000000f},{0.330632100f,0.943735300f,0.006789481f,0.000000000f},{0.176210661f,-0.068798740f,0.981945282f,0.000000000f},{-0.000282718f,0.007938241f,0.076416012f,1.000000000f}}};
const Mat knifeWristFromPalm={{{0.176206195f,-0.203139404f,-0.963164451f,0.000000000f},{0.277565337f,0.949022566f,-0.149377550f,0.000000000f},{0.944409265f,-0.241019815f,0.223608112f,0.000000000f},{0.024000000f,0.093648207f,-0.011452710f,1.000000000f}}};
const Mat knifeWristFromItem={{{0.411160480f,-0.139989788f,-0.900749643f,-0.000000000f},{-0.021389490f,0.986383911f,-0.163062166f,-0.000000000f},{0.911311994f,0.086311294f,0.402567772f,0.000000000f},{0.011400762f,0.088319584f,-0.087900141f,1.000000000f}}};
const Mat fragFromPalm={{{0.393269030f,0.918715648f,0.036069776f,0.000000000f},{0.127422394f,-0.093313265f,0.987449324f,0.000000000f},{0.910550934f,-0.383737141f,-0.153762164f,0.000000000f},{-0.017752516f,-0.008440697f,0.034993730f,1.000000000f}}};
const Mat fragWristFromPalm={{{0.176206195f,-0.203139404f,-0.963164451f,0.000000000f},{0.277565337f,0.949022566f,-0.149377550f,0.000000000f},{0.944409265f,-0.241019815f,0.223608112f,0.000000000f},{0.024000000f,0.093648207f,-0.011452710f,1.000000000f}}};
const Mat fragWristFromItem={{{-0.152072035f,-0.909667870f,0.386495100f,0.000000000f},{0.975651718f,-0.200691116f,-0.088469214f,0.000000000f},{0.158043734f,0.363630915f,0.918038526f,0.000000000f},{0.108535333f,-0.050069549f,0.005984193f,1.000000000f}}};
const Mat smokeFromPalm={{{0.358427024f,0.932947849f,0.033739872f,0.000000000f},{0.135925142f,-0.087908589f,0.986811246f,0.000000000f},{0.923609455f,-0.349113721f,-0.158319883f,0.000000000f},{-0.034790285f,-0.009580082f,0.012246279f,1.000000000f}}};
const Mat smokeWristFromPalm={{{0.176206195f,-0.203139404f,-0.963164451f,0.000000000f},{0.277565337f,0.949022566f,-0.149377550f,0.000000000f},{0.944409265f,-0.241019815f,0.223608112f,0.000000000f},{0.024000000f,0.093648207f,-0.011452710f,1.000000000f}}};
const Mat smokeWristFromItem={{{-0.158858453f,-0.908652962f,0.386152544f,0.000000000f},{0.979835500f,-0.193106573f,-0.051305394f,0.000000000f},{0.121187393f,0.370215675f,0.921007041f,0.000000000f},{0.116579036f,-0.024470004f,0.022012872f,1.000000000f}}};
const Mat flashFromPalm={{{0.358404184f,0.932954875f,0.033788186f,0.000000000f},{0.135813245f,-0.087913260f,0.986826237f,0.000000000f},{0.923634778f,-0.349093769f,-0.158216110f,0.000000000f},{-0.040170681f,-0.007016997f,0.000582659f,1.000000000f}}};
const Mat flashWristFromPalm={{{0.176206195f,-0.203139404f,-0.963164451f,0.000000000f},{0.277565337f,0.949022566f,-0.149377550f,0.000000000f},{0.944409265f,-0.241019815f,0.223608112f,0.000000000f},{0.024000000f,0.093648207f,-0.011452710f,1.000000000f}}};
const Mat flashWristFromItem={{{-0.158910440f,-0.908686168f,0.386053002f,0.000000000f},{0.979828611f,-0.193144304f,-0.051294932f,0.000000000f},{0.121174933f,0.370114477f,0.921049352f,0.000000000f},{0.116508480f,-0.012011396f,0.026032866f,1.000000000f}}};

#include "LaptopGrips.inl"
#include "Dzb2ThrowGrips.inl"
#include "Dzb2BeaconGrip.inl"
bool PhysicalBeaconName(const char* name){return !strcmp(name,"hm_point")||!strcmp(name,"hm_point_index6");}
bool PhysicalGrappleName(const char* name){return !strcmp(name,"grapplinghook")||!strcmp(name,"nsrif_grapplinghook");}
// r105: these three native gadgets share UAV_PC geometry and both animation
// systems. Keep their actual template names: projectile/deployment differs.
bool PhysicalLaptopName(const char* name){
    return !strcmp(name,"uav_pc")||!strcmp(name,"uav_uav")||!strcmp(name,"uav_bomber");
}
struct PalmCarryBinding {const char* name;const Mat* wristFromPalm;const Mat* wristFromItem;};
const PalmCarryBinding palmCarryBindings[]={
    // Both inventory variants share the same native radio-box model/pose.
    {"hm_point",&dzBeaconWristFromPalm,&dzBeaconWristFromItem},
    {"hm_point_index6",&dzBeaconWristFromPalm,&dzBeaconWristFromItem},
    // ReloadName canonicalizes Survival's uav_pc_sv5 without changing its slot.
    {"uav_pc",&laptopWristFromPalm,&laptopWristFromItem},
    {"uav_uav",&laptopWristFromPalm,&laptopWristFromItem},
    {"uav_bomber",&laptopWristFromPalm,&laptopWristFromItem},
    {"medikit",&medkitWristFromPalm,&medkitWristFromItem},
    // r123: audited DZB2 supplies share the stock medkit stand animation.
    {"bf3_medikit",&medkitWristFromPalm,&medkitWristFromItem},
    {"bf3_ammobox",&medkitWristFromPalm,&medkitWristFromItem},
    // ammokit.tweak explicitly shares medikit/AnimationSystem1p.inc.
    {"ammokit",&medkitWristFromPalm,&medkitWristFromItem},
    {"ammobag",&medkitWristFromPalm,&medkitWristFromItem},
    {"kni_knife",&knifeWristFromPalm,&knifeWristFromItem},
    {"ushgr_m67",&fragWristFromPalm,&fragWristFromItem},
    {"hgr_smoke",&smokeWristFromPalm,&smokeWristFromItem},
    // r124: both canisters explicitly use the stock smoke stand animation.
    // RGO has its own animation path; its measured palm contact is retained.
    {"hgr_incendiary",&smokeWristFromPalm,&smokeWristFromItem},
    {"hgr_teargas",&smokeWristFromPalm,&smokeWristFromItem},
    {"rgo",&dzRgoWristFromPalm,&dzRgoWristFromItem},
    {"nshgr_flashbang",&flashWristFromPalm,&flashWristFromItem}
};
const PalmCarryBinding* PalmCarry(void* weapon){
    char name[80]{};if(!ReloadName(weapon,name))return nullptr;
    for(const auto& grip:palmCarryBindings)if(!strcmp(name,grip.name))return &grip;
    return nullptr;
}
bool PhysicalLaptop(void* weapon){
    char name[80]{};return ReloadName(weapon,name)&&PhysicalLaptopName(name);
}
