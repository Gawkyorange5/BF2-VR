// Survival-only world-object grip bridge. No inventory object is fabricated.
#include "SurvivalSentryHand.inl"
// Fixed tripod spars, measured in the approved BF2 meshes (+Y up).
// Grip Z runs along the spar; the same contact drives hand IK and root movement.
Mat SentryContact(int kind){
    Mat result=RotateBetween({0,0,1},kind?Vec{0,-.48436f,.64130f}:Vec{0,-.631f,-.517f});
    // Reverse the carrying orientation around the palm, keeping the spar
    // inside the fingers. The M3 uses its right rear leg (120 degrees from
    // the front leg); SG-2 retains the exact previously approved contact.
    Mat flip=Identity();flip.m[1][1]=-1;flip.m[2][2]=-1;
    result=Mul(flip,result);
    if(kind)result=Mul(result,Yaw(2.09439510239f));
    SetPosition(result,kind?Vec{.23417729f,.43478830f,-.07623000f}:Vec{0,.5578f,-.1764f});
    return result;
}
Mat SentryReleasePose(const Mat& hand,void* weapon){
    char name[80]{};ReloadName(weapon,name);
    const Mat contact=SentryContact(!strcmp(name,"ammokit")?1:0);
    Mat root=Level(Mul(Inverse(contact),hand));
    SetPosition(root,Sub(Position(hand),Direction(Position(contact),root)));
    return root;
}
struct SurvivalSentryGrip {
    char session[40]{};unsigned long long sequence=0,serial=0;
    ULONGLONG scanned=0,changed=0,written=0;
    struct Placed {int id=-1;Vec position{};float yaw=0;bool held=false;int kind=0;} placed[2];
    int count=0,carried=-1;bool pressed=true;
} survivalSentryGrip;
bool SentryRead(char (&text)[2048]){
    HANDLE h=CreateFileA("mods/survival/Logs/sentry-world.txt",GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(h==INVALID_HANDLE_VALUE)return false;DWORD n=0;
    const bool ok=ReadFile(h,text,sizeof(text)-1,&n,nullptr)!=0;CloseHandle(h);text[n]=0;return ok;
}
void RefreshSentryWorld(ULONGLONG now){
    auto& s=survivalSentryGrip;if(now-s.scanned<100)return;s.scanned=now;
    char text[2048]{},session[40]{};unsigned long long sequence=0;int used=0;
    if(!SentryRead(text)||sscanf_s(text,"survival-sentries-v1 %39s %llu%n",session,(unsigned)sizeof(session),&sequence,&used)!=2||strlen(session)!=32)return;
    for(const char* p=session;*p;++p)if(!((*p>='0'&&*p<='9')||(*p>='a'&&*p<='f')))return;
    if(strcmp(s.session,session)){strcpy_s(s.session,session);s.carried=-1;s.sequence=0;s.serial=now;s.pressed=true;}
    if(sequence!=s.sequence){s.sequence=sequence;s.changed=now;}
    s.count=0;const char* row=text+used;
    while(*row&&s.count<2){
        int id=-1,held=0,n=0;char kind[12]{};float x=0,y=0,z=0,yaw=0;
        if(sscanf_s(row," %d %11s %f %f %f %f %d%n",&id,kind,(unsigned)sizeof(kind),&x,&y,&z,&yaw,&held,&n)!=7||n<=0)break;
        if(id>=0&&id<4&&(!strcmp(kind,"sg2")||!strcmp(kind,"m3"))&&std::isfinite(x+y+z+yaw))
            s.placed[s.count++]={id,{x,y,z},yaw,held!=0,!strcmp(kind,"m3")?1:0};
        row+=n;
    }
}
bool SurvivalSentryItem(void* weapon){
    const auto& s=survivalSentryGrip;
    if(!s.changed||GetTickCount64()-s.changed>1500)return false;
    char name[80]{};return ReloadName(weapon,name)&&(!strcmp(name,"medikit")||!strcmp(name,"ammokit"));
}
bool SentryCarried(){
    const auto& s=survivalSentryGrip;
    return s.carried>=0&&s.changed&&GetTickCount64()-s.changed<=1500&&LocalAlive()&&!MenuActive();
}
// The entire authored carrying pose is retained, including all five fingers.
// Never reuse the equipped rifle's trigger finger for an empty-hand world grip.
void SentryHandBasis(void* weapon,Mat basis[70]){
    if(!SentryCarried()&&!SurvivalSentryItem(weapon))return;
    const Mat wrist=basis[33];
    for(int i=0;i<21;++i)basis[33+i]=Mul(sentryCarryFingers[i],wrist);
}
void SentryHandBinding(void* weapon){
    if(!SentryCarried()&&!SurvivalSentryItem(weapon))return;
    palmRight=medkitWristFromPalm;
    if(SurvivalSentryItem(weapon)){
        // First-person vertices are baked into the stock medikit item frame.
        binding.right=medkitWristFromItem;
        binding.rightReady=true;
    }
}
void SentryHandTarget(HandTargets& target,const Mat& body){
    if(!SentryCarried())return;
    const auto& s=survivalSentryGrip;
    for(int i=0;i<s.count;++i)if(s.placed[i].id==s.carried&&s.placed[i].held){
        Mat root=Yaw(s.placed[i].yaw*.01745329252f);SetPosition(root,s.placed[i].position);
        // Use the actual published sentry position, including the server ack.
        // Sleeve and fingers share this contact, so neither floats beside it.
        target.rightGrip=Mul(Mul(SentryContact(s.placed[i].kind),root),Inverse(body));
        target.rightPalm=medkitWristFromPalm;target.palmsValid=true;
        target.freeHands=true;target.supportPressed=false;target.virtualStock=false;
        return;
    }
}
void SendSentryGrip(const char* action,int id,Vec p,float yaw){
    auto& s=survivalSentryGrip;char text[256]{};
    const int n=sprintf_s(text,"%s %llu %s %d %.5f %.5f %.5f %.3f\n",s.session,++s.serial,action,id,p.x,p.y,p.z,yaw);
    HANDLE h=CreateFileA("mods/survival/Logs/sentry-input.txt",GENERIC_WRITE,FILE_SHARE_READ,nullptr,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(h!=INVALID_HANDLE_VALUE){DWORD written=0;WriteFile(h,text,n,&written,nullptr);CloseHandle(h);}
}
bool SentryFloor(Vec from,Vec& floor){
    Vec end=from;end.y-=4.f;void* object=nullptr;
    if(WorldSegmentObject(from,end,Soldier(),floor,object)){
        char name[80]{};void* root=object;
        for(int i=0;i<12&&VehicleObject(root)&&Value<void*>(root,0x28);++i)root=Value<void*>(root,0x28);
        if(VehicleName(Value<void*>(root,0x14),name)&&(!strcmp(name,"survival_sentry_base")||!strcmp(name,"m3_grenade_sentry")))
            return WorldSegment(from,end,root,floor);
        return true;
    }
    float ground=0;if(TerrainHeight(from,ground)&&from.y-ground>=0&&from.y-ground<4){floor=from;floor.y=ground;return true;}return false;
}
bool UpdateSurvivalSentryGrip(const Runtime::Frame& f,bool enabled,bool grip){
    auto& s=survivalSentryGrip;RefreshSentryWorld(f.tick);
    if(!enabled||!f.valid||!f.focused||!f.hands[1].gripValid||!LocalAlive()||MenuActive()||!s.changed||f.tick-s.changed>1500){s.carried=-1;s.pressed=grip;return false;}
    Mat camera{};if(!referenceValid||!Read(views[0].object,0x3C,camera)||!Rigid(camera))return false;
    const Mat hand=Compose(Level(camera),reference,f.hands[1].grip,Config().units);if(!Rigid(hand))return false;
    const Vec palm=Position(hand);const bool pressed=grip&&!s.pressed;s.pressed=grip;
    int selected=-1;
    for(int i=0;i<s.count;++i)if(s.placed[i].id==s.carried)selected=i;
    if(s.carried>=0&&selected<0)s.carried=-1;
    if(s.carried<0&&pressed){
        float best=.22f*Config().units;
        for(int i=0;i<s.count;++i){Mat root=Yaw(s.placed[i].yaw*.01745329252f);SetPosition(root,s.placed[i].position);
            Vec handle=Point(Position(SentryContact(s.placed[i].kind)),root);
            const float distance=Length(Sub(palm,handle));if(!s.placed[i].held&&distance<best){best=distance;selected=i;}}
        if(selected>=0){s.carried=s.placed[selected].id;s.written=0;Runtime::Haptic(1,.6f,60);}
    }
    if(s.carried<0)return false;
    inventoryHands.empty=true;inventoryHands.pending=0;inventoryHands.pendingWeapon=nullptr;inventoryHands.releaseArmed=false;
    bodySlots.pressed=true;bodySlots.consumed=true;
    // The contact offset is rotated with the root, never a fixed world offset.
    // Keep the heavy base upright while the hand wraps the support spar.
    const Mat rootRotation=Level(Mul(Inverse(SentryContact(s.placed[selected].kind)),hand));
    float yaw=Heading(rootRotation)*57.295779513f;
    Vec position=Sub(palm,Direction(Position(SentryContact(s.placed[selected].kind)),rootRotation));
    if(!grip){
        Vec floor{};
        Vec above=position;above.y+=.4f;
        if(SentryFloor(above,floor)){SendSentryGrip("drop",s.carried,floor,yaw);Runtime::Haptic(1,.25f,40);}
        else SendSentryGrip("cancel",s.carried,position,yaw);
        s.carried=-1;
        return true;
    }
    if(f.tick-s.written>=50){
        bool accepted=false;for(int i=0;i<s.count;++i)if(s.placed[i].id==s.carried)accepted=s.placed[i].held;
        SendSentryGrip(accepted?"hold":"grab",s.carried,accepted?position:palm,yaw);s.written=f.tick;
    }
    return true;
}
