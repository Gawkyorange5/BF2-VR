// Local visual arm simulation for the helmet death view. BF2 retains ownership
// of the corpse, torso, legs and camera; living rigs are never written here.
bool WorldSegment(const Vec& from,const Vec& to,void* ignore,Vec& hit);
bool TerrainHeight(const Vec& p,float& height);
struct DownedArms {
    void* owner=nullptr;void* rig=nullptr;void* ragdoll=nullptr;
    std::int64_t time=0;Vec root{};
    Vec point[2][2]{},velocity[2][2]{};
} downedArms;
void DownedCollision(Vec previous,Vec& p,void* owner){
    Vec hit{};const float radius=.055f*Config().units;
    if(Length(Sub(p,previous))>.001f&&WorldSegment(previous,p,owner,hit))
        p=Add(hit,Scale(Unit(Sub(previous,p)),radius));
    float floor=0;if(TerrainHeight(p,floor)&&p.y<floor+radius)p.y=floor+radius;
    // Roofs and floors above terrain also support the arm. Avoid a long ray
    // that could pull a corpse's arm onto an unrelated level overhead.
    Vec above=p,below=p;above.y+=radius;below.y-=radius;
    if(WorldSegment(above,below,owner,hit)&&p.y<hit.y+radius)p.y=hit.y+radius;
}
void RelaxDownedArms(void* owner,void* rig){
    auto& s=downedArms;void* ragdoll=Value<void*>(owner,0x254);
    const auto now=pairFrame.time;const Vec root=Position(bodyWorld);
    const bool reset=s.owner!=owner||s.rig!=rig||s.ragdoll!=ragdoll||!s.time||now<s.time||now-s.time>150000000||Length(Sub(root,s.root))>2.f;
    const Mat intoBody=Inverse(bodyWorld);Mat source[80];memcpy(source,bodyBones,sizeof(source));
    if(reset){s={};s.owner=owner;s.rig=rig;s.ragdoll=ragdoll;s.time=now;s.root=root;
        for(int side=0;side<2;++side){s.point[side][0]=Point(Position(source[side?33:17]),bodyWorld);s.point[side][1]=Point(Position(source[side?35:20]),bodyWorld);}
        Log("DEATH57 passive visual arms bound to native corpse=%p ragdoll=%p",owner,ragdoll);
    }
    const float seconds=std::min(.06f,float((now-s.time)*1.e-9));s.time=now;
    const Vec travel=Sub(root,s.root);s.root=root;
    for(int side=0;side<2;++side){
        const int shoulder=side?31:15,elbow=side?33:17,wrist=side?35:20,last=side?44:29;
        const Vec a=Point(Position(source[shoulder]),bodyWorld),b=Point(Position(source[elbow]),bodyWorld),c=Point(Position(source[wrist]),bodyWorld);
        const float upper=Length(Sub(b,a)),lower=Length(Sub(c,b));
        if(upper<.08f||upper>.7f||lower<.08f||lower>.7f)continue;
        // Most corpse translation carries the arms with it; a small residual
        // produces inertia while the native torso falls or slides.
        for(int j=0;j<2;++j)s.point[side][j]=Add(s.point[side][j],Scale(travel,.85f));
        float remaining=seconds;
        while(remaining>0){
            const float dt=std::min(.016f,remaining);remaining-=dt;
            Vec old[2]={s.point[side][0],s.point[side][1]};
            for(int j=0;j<2;++j){
                s.velocity[side][j].y-=9.81f*Config().units*dt;
                s.velocity[side][j]=Scale(s.velocity[side][j],std::exp(-3.5f*dt));
                const float speed=Length(s.velocity[side][j]);if(speed>4.f)s.velocity[side][j]=Scale(s.velocity[side][j],4.f/speed);
                s.point[side][j]=Add(s.point[side][j],Scale(s.velocity[side][j],dt));
            }
            for(int k=0;k<5;++k){
                Vec& e=s.point[side][0];Vec& w=s.point[side][1];
                Vec arm=Sub(e,a);if(Length(arm)>.0001f)e=Add(a,Scale(Unit(arm),upper));
                arm=Sub(w,e);const float length=Length(arm);
                if(length>.0001f){const Vec correction=Scale(arm,(length-lower)/length*.5f);e=Add(e,correction);w=Sub(w,correction);}
            }
            for(int j=0;j<2;++j){
                DownedCollision(old[j],s.point[side][j],owner);
                s.velocity[side][j]=Scale(Sub(s.point[side][j],old[j]),1.f/dt);
            }
        }
        Vec e=s.point[side][0],w=s.point[side][1];
        // Preserve the native lengths when reconstructing the mesh, even if
        // collision briefly constrains both endpoints more tightly.
        e=Add(a,Scale(Unit(Sub(e,a)),upper));w=Add(e,Scale(Unit(Sub(w,e)),lower));
        const Mat upperTurn=RotateBetween(Sub(b,a),Sub(e,a)),lowerTurn=RotateBetween(Sub(c,b),Sub(w,e));
        for(int i=shoulder;i<=last;++i){
            const bool forearm=i>=elbow;const Vec pivot=forearm?b:a,target=forearm?e:a;
            Mat bone=Mul(source[i],bodyWorld);const Vec offset=Sub(Position(bone),pivot);
            const Mat rotation=forearm?lowerTurn:upperTurn;bone=Mul(bone,rotation);
            SetPosition(bone,Add(target,Direction(offset,rotation)));bodyBones[i]=Mul(bone,intoBody);
            if(!Rigid(bodyBones[i])){memcpy(bodyBones,source,sizeof(source));s={};return;}
        }
    }
}
