#include "VrPoseMath.h"
#include <algorithm>
#include <cmath>
#include <cstring>

// Arm/binding policy adapted from BF2142VR (MIT); see THIRD_PARTY.md.
namespace CleanVR {
Vec Add(Vec a, Vec b) { return {a.x+b.x,a.y+b.y,a.z+b.z}; }
Vec Sub(Vec a, Vec b) { return {a.x-b.x,a.y-b.y,a.z-b.z}; }
Vec Scale(Vec a,float s) { return {a.x*s,a.y*s,a.z*s}; }
float Dot(Vec a,Vec b) { return a.x*b.x+a.y*b.y+a.z*b.z; }
Vec Cross(Vec a,Vec b) { return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x}; }
float Length(Vec a) { return std::sqrt(Dot(a,a)); }
Vec Unit(Vec a) { const float l=Length(a); return std::isfinite(l)&&l>1.e-6f?Scale(a,1.f/l):Vec{}; }
Vec Row(const Mat& a,int i) { return {a.m[i][0],a.m[i][1],a.m[i][2]}; }
Vec Position(const Mat& a) { return Row(a,3); }
void SetPosition(Mat& a,Vec p) { a.m[3][0]=p.x;a.m[3][1]=p.y;a.m[3][2]=p.z;a.m[3][3]=1; }
Mat Identity() { Mat a{};for(int i=0;i<4;++i)a.m[i][i]=1;return a; }
Mat Mul(const Mat& a,const Mat& b) { Mat c{};for(int i=0;i<4;++i)for(int j=0;j<4;++j)for(int k=0;k<4;++k)c.m[i][j]+=a.m[i][k]*b.m[k][j];return c; }
Vec Direction(Vec p,const Mat& a) { return {p.x*a.m[0][0]+p.y*a.m[1][0]+p.z*a.m[2][0],p.x*a.m[0][1]+p.y*a.m[1][1]+p.z*a.m[2][1],p.x*a.m[0][2]+p.y*a.m[1][2]+p.z*a.m[2][2]}; }
Vec Point(Vec p,const Mat& a) { return Add(Direction(p,a),Position(a)); }
Mat Inverse(const Mat& a) { Mat b=Identity();for(int i=0;i<3;++i)for(int j=0;j<3;++j)b.m[i][j]=a.m[j][i];SetPosition(b,Scale(Direction(Position(a),b),-1));return b; }
bool Rigid(const Mat& a) {
    for(int i=0;i<4;++i)for(int j=0;j<4;++j)if(!std::isfinite(a.m[i][j])||std::abs(a.m[i][j])>16000)return false;
    if(std::abs(a.m[3][3]-1)>.02f)return false;
    for(int i=0;i<3;++i){if(std::abs(a.m[i][3])>.02f)return false;for(int j=0;j<3;++j)if(std::abs(Dot(Row(a,i),Row(a,j))-(i==j?1.f:0.f))>.03f)return false;}
    return Dot(Row(a,0),Cross(Row(a,1),Row(a,2)))>.95f;
}
bool Near(const Mat& a,const Mat& b,float p,float r) {
    if(Length(Sub(Position(a),Position(b)))>p)return false;
    for(int i=0;i<3;++i)if(Dot(Row(a,i),Row(b,i))<1-r)return false;
    return true;
}
Mat FromOpenXR(Vec p,Quat q) {
    const float n=std::sqrt(q.x*q.x+q.y*q.y+q.z*q.z+q.w*q.w);
    if(!std::isfinite(n)||n<.5f)return Mat{};
    q.x/=n;q.y/=n;q.z/=n;q.w/=n;
    Mat a=Identity();const float x=q.x,y=q.y,z=q.z,w=q.w;
    // Transposed quaternion matrix, then S*R*S with S=diag(1,1,-1).
    a.m[0][0]=1-2*(y*y+z*z);a.m[0][1]=2*(x*y+w*z);a.m[0][2]=-2*(x*z-w*y);
    a.m[1][0]=2*(x*y-w*z);a.m[1][1]=1-2*(x*x+z*z);a.m[1][2]=-2*(y*z+w*x);
    a.m[2][0]=-2*(x*z+w*y);a.m[2][1]=-2*(y*z-w*x);a.m[2][2]=1-2*(x*x+y*y);
    SetPosition(a,{p.x,p.y,-p.z});return a;
}
Mat Yaw(float r) { Mat a=Identity();a.m[0][0]=a.m[2][2]=std::cos(r);a.m[0][2]=-std::sin(r);a.m[2][0]=std::sin(r);return a; }
Mat Level(const Mat& a) { Mat b=Yaw(std::atan2(a.m[2][0],a.m[2][2]));SetPosition(b,Position(a));return b; }
Mat LevelInRig(const Mat& camera,const Mat& body) { return Mul(Level(Mul(camera,body)),Inverse(body)); }
Mat ShoulderFrame(const Mat& head,const Mat& torso) {
    Mat frame=torso;SetPosition(frame,Add(Position(head),Direction({0.f,-.22f,-.015f},torso)));return frame;
}
Mat Compose(const Mat& camera,const Mat& reference,const Mat& tracked,float scale) {
    Mat relative=Mul(tracked,Inverse(reference));SetPosition(relative,Scale(Position(relative),scale));return Mul(relative,camera);
}
bool Projection(float l,float r,float u,float d,float n,float f,Mat& p) {
    if(!std::isfinite(l+r+u+d+n+f)||r-l<.05f||u-d<.05f||n<=0||f<=n)return false;
    p={};p.m[0][0]=2/(r-l);p.m[1][1]=2/(u-d);
    // BF2's native projection is LH (+Z), confirmed in RendDX9's builder.
    p.m[2][0]=-(r+l)/(r-l);p.m[2][1]=-(u+d)/(u-d);
    p.m[2][2]=f/(f-n);p.m[2][3]=1;p.m[3][2]=-n*f/(f-n);return true;
}
Mat Facing(const Mat& prior,Vec forward) {
    Vec z=Unit(forward),x=Unit(Cross(Row(prior,1),z));
    if(Length(x)<.5f)x=Unit(Cross({0,0,1},z));if(Length(x)<.5f)x=Unit(Cross({1,0,0},z));
    const Vec y=Cross(z,x);Mat a=Identity();const Vec rows[]={x,y,z};
    for(int i=0;i<3;++i){a.m[i][0]=rows[i].x;a.m[i][1]=rows[i].y;a.m[i][2]=rows[i].z;}SetPosition(a,Position(prior));return a;
}
Mat RotateBetween(Vec a,Vec b) {
    a=Unit(a);b=Unit(b);const float c=std::max(-1.f,std::min(1.f,Dot(a,b)));Vec axis=Cross(a,b);float s=Length(axis);
    if(s<1.e-5f){if(c>0)return Identity();axis=Unit(Cross(a,{0,1,0}));if(Length(axis)<.5f)axis=Unit(Cross(a,{1,0,0}));}else axis=Scale(axis,1/s);
    const float x=axis.x,y=axis.y,z=axis.z,t=1-c;Mat m=Identity();
    m.m[0][0]=t*x*x+c;m.m[0][1]=t*x*y+s*z;m.m[0][2]=t*x*z-s*y;
    m.m[1][0]=t*x*y-s*z;m.m[1][1]=t*y*y+c;m.m[1][2]=t*y*z+s*x;
    m.m[2][0]=t*x*z+s*y;m.m[2][1]=t*y*z-s*x;m.m[2][2]=t*z*z+c;return m;
}
void HandBinding::Update(const Mat b[70],std::uint64_t now) {
    if(!Rigid(b[54])||!Rigid(b[33])||!Rigid(b[7]))return;
    const Mat r=Mul(b[33],Inverse(b[54])),l=Mul(b[7],Inverse(b[54]));
    if(Length(Position(r))>1||Length(Position(l))>1)return;
    if(!valid){right=r;left=l;started=now;valid=true;}
    const bool gap=now<lastSeen||(lastSeen&&now-lastSeen>150);lastSeen=now;
    if(!rightReady){
        right=r;
        if(gap||!rightPending||!Near(r,rightCandidate,.012f,.002f)){rightCandidate=r;rightSince=now;rightPending=true;}
        rightReady=now>=started+750&&now>=rightSince+250;
    }
    const Vec between=Sub(Position(l),Position(r));const float separation=Length(between);
    if(!leftReady){
        left=l;
        const bool foregrip=between.z>.04f&&separation>.12f&&separation<.8f&&std::abs(between.x)<.3f&&std::abs(between.y)<.3f;
        if(!foregrip){leftPending=false;return;}
        if(gap||!leftPending||!Near(l,leftCandidate,.012f,.002f)){leftCandidate=l;leftSince=now;leftPending=true;}
        leftReady=rightReady&&now>=leftSince+250;
    }
}
bool PalmBinding(const Mat b[70],int w,Mat& out) {
    if(w!=7&&w!=33)return false;
    const Vec origin=Position(b[w]),across=Sub(Position(b[w+5]),Position(b[w+17]));
    const Vec along=Sub(Scale(Add(Position(b[w+9]),Position(b[w+13])),.5f),origin);
    if(Length(across)<.025f||Length(across)>.12f||Length(along)<.035f||Length(along)>.16f)return false;
    const Vec z=Unit(across),x=Unit(Cross(Scale(along,-1),z)),y=Cross(z,x);Mat palm=Identity();const Vec rows[]={x,y,z};
    for(int i=0;i<3;++i){palm.m[i][0]=rows[i].x;palm.m[i][1]=rows[i].y;palm.m[i][2]=rows[i].z;}
    SetPosition(palm,Add(Add(origin,Scale(along,.9f)),Scale(x,w==33?-.024f:.024f)));
    if(!Rigid(palm))return false;out=Mul(b[w],Inverse(palm));return Rigid(out);
}
namespace {
float Clamp(float x){return std::max(0.f,std::min(1.f,x));}
Vec Pole(Vec d,bool left){
    d=Unit(d);const float side=left?-1.f:1.f;
    Vec p={side*(.45f+.25f*Clamp(d.z)+.20f*Clamp(-side*d.x)),-.70f-.15f*Clamp(d.y),-.40f-.35f*Clamp(d.z)};
    const float blend=std::max(Clamp((.35f-std::sqrt(d.x*d.x+d.z*d.z))/.25f),Clamp((-d.z-.1f)/.5f));
    p=Add(Scale(p,1-blend),Scale({side*.133f,-.443f,-.886f},blend));p=Unit(Sub(p,Scale(d,Dot(p,d))));
    if(Length(p)<.5f)p=Unit(Cross(d,{0,0,1}));if(Length(p)<.5f)p=Unit(Cross(d,{1,0,0}));return p;
}
Mat Segment(const Mat& bone,Vec a,Vec b,Vec c,Vec d){
    Mat result=bone;SetPosition(result,Sub(Position(bone),a));result=Mul(result,RotateBetween(Sub(b,a),Sub(d,c)));
    SetPosition(result,Add(c,Scale(Position(result),Length(Sub(d,c))/Length(Sub(b,a)))));return result;
}
Mat LimbFrame(Vec origin,Vec direction,Vec plane){
    const Vec z=Unit(direction),x=Unit(Cross(plane,z)),y=Cross(z,x);
    Mat m=Identity();const Vec rows[]={x,y,z};
    for(int i=0;i<3;++i){m.m[i][0]=rows[i].x;m.m[i][1]=rows[i].y;m.m[i][2]=rows[i].z;}
    SetPosition(m,origin);return m;
}
Mat PlaneSegment(const Mat& bone,Vec a,Vec b,Vec c,Vec d,Vec nativePlane,Vec solvedPlane){
    if(Length(nativePlane)<.5f||Length(solvedPlane)<.5f)return Segment(bone,a,b,c,d);
    const Mat oldFrame=LimbFrame(a,Sub(b,a),nativePlane),newFrame=LimbFrame(c,Sub(d,c),solvedPlane);
    if(!Rigid(oldFrame)||!Rigid(newFrame))return Segment(bone,a,b,c,d);
    Mat local=Mul(bone,Inverse(oldFrame));Vec p=Position(local);
    // Retain lateral offsets; stretching a limb only changes its length.
    p.z*=Length(Sub(d,c))/Length(Sub(b,a));SetPosition(local,p);return Mul(local,newFrame);
}
Mat AxisTurn(Vec axis,float angle){
    const float x=axis.x,y=axis.y,z=axis.z,c=std::cos(angle),s=std::sin(angle),t=1-c;
    Mat m=Identity();
    m.m[0][0]=t*x*x+c;m.m[0][1]=t*x*y+s*z;m.m[0][2]=t*x*z-s*y;
    m.m[1][0]=t*x*y-s*z;m.m[1][1]=t*y*y+c;m.m[1][2]=t*y*z+s*x;
    m.m[2][0]=t*x*z+s*y;m.m[2][1]=t*y*z-s*x;m.m[2][2]=t*z*z+c;return m;
}
Quat RelativeRotation(const Mat& from,const Mat& to){
    Mat a=from,b=to;SetPosition(a,{});SetPosition(b,{});const Mat d=Mul(Inverse(a),b);
    // Quaternion of a row-vector rotation; ignore translations.
    Quat q{};const float trace=d.m[0][0]+d.m[1][1]+d.m[2][2];
    if(trace>0){const float s=2*std::sqrt(trace+1);q={(d.m[1][2]-d.m[2][1])/s,(d.m[2][0]-d.m[0][2])/s,(d.m[0][1]-d.m[1][0])/s,s*.25f};}
    else{
        int i=0;if(d.m[1][1]>d.m[i][i])i=1;if(d.m[2][2]>d.m[i][i])i=2;
        const int j=(i+1)%3,k=(i+2)%3;const float s=2*std::sqrt(std::max(0.f,1+d.m[i][i]-d.m[j][j]-d.m[k][k]));
        if(s<1.e-5f)return {0,0,0,1};float v[3]{};v[i]=s*.25f;v[j]=(d.m[i][j]+d.m[j][i])/s;v[k]=(d.m[i][k]+d.m[k][i])/s;
        q={v[0],v[1],v[2],(d.m[j][k]-d.m[k][j])/s};
    }
    return q;
}
float WristTwist(const Mat& from,const Mat& to,Vec axis){
    // Project the vector part onto the forearm axis to isolate roll. Wrist
    // flexion is separate and must not bend the ulna.
    const Quat q=RelativeRotation(from,to);
    const float along=Dot({q.x,q.y,q.z},axis);if(along*along+q.w*q.w<1.e-8f)return 0;
    return std::remainder(2*std::atan2(along,q.w),6.283185307f);
}
Mat PartialRotation(const Mat& from,const Mat& to,float fraction){
    Quat q=RelativeRotation(from,to);
    if(q.w<0)q={-q.x,-q.y,-q.z,-q.w};
    const Vec v={q.x,q.y,q.z};const float sine=Length(v);
    return sine<1.e-6f?Identity():AxisTurn(Scale(v,1/sine),2*std::atan2(sine,q.w)*fraction);
}
Vec ChargingElbowPole(const Mat b[80],int s,int e,int w,const Mat& target,
                      const Mat& torso,Vec direction,float along,float bend,float weight,float& turn,Vec nativeElbow){
    const Vec base=Pole(direction,true);turn=0;
    if(weight<=0||bend<.015f)return base;
    const Vec shoulder=Position(b[s]),wrist=Position(target);
    const Vec nativePlane=Unit(Cross(Sub(nativeElbow,shoulder),Sub(Position(b[w]),nativeElbow)));
    if(Length(nativePlane)<.5f)return base;
    // A close cross-body handle grab needs some elbow swivel. Evaluate a
    // bounded set around the anatomical down/out pole, preferring less wrist
    // torsion without changing either endpoint. Soft weights avoid switching
    // abruptly between discrete candidates or at the +/- pi roll boundary.
    float weighted=0,total=0;
    for(int i=-4;i<=4;++i){
        const float angle=i*.261799388f; // +/- 60 degrees
        const Vec pole=Direction(base,AxisTurn(direction,angle));
        const Vec local=Add(Scale(direction,along),Scale(pole,bend));
        const Vec elbow=Add(shoulder,Direction(local,torso));
        const Vec plane=Unit(Cross(Sub(elbow,shoulder),Sub(wrist,elbow)));
        const Mat swung=PlaneSegment(b[w],nativeElbow,Position(b[w]),elbow,wrist,nativePlane,plane);
        const float roll=WristTwist(swung,target,Unit(Sub(wrist,elbow)));
        const float excess=std::max(0.f,std::abs(roll)-.785398163f);
        const float raised=std::max(0.f,local.y-.015f);
        const float cost=excess*excess+.65f*angle*angle+40.f*raised*raised;
        const float preference=std::exp(-2.f*cost);
        weighted+=angle*preference;total+=preference;
    }
    if(total>1.e-8f)turn=weighted/total*Clamp(weight);
    return Direction(base,AxisTurn(direction,turn));
}
bool Arm(Mat out[70],const Mat b[70],int s,int e,int w,const Mat& target,const Mat& head,const Mat& torso,bool left,bool allowFold=false){
    const Mat frame=ShoulderFrame(head,torso);
    const Vec root={left?-ShoulderHalfWidth:ShoulderHalfWidth,0.f,0.f},hand=Point(Position(target),Inverse(frame)),reach=Sub(hand,root);
    float a=Length(Sub(Position(b[e]),Position(b[s]))),c=Length(Sub(Position(b[w]),Position(b[e]))),distance=Length(reach);
    if(a<.05f||c<.05f||a>.6f||c>.6f||!std::isfinite(distance)||(!allowFold&&distance<.015f)||distance>(a+c)*1.65f)return false;
    if(allowFold)distance=std::max(.001f,distance);
    const float stretch=std::max(1.f,distance/(a+c)*1.002f);a*=stretch;c*=stretch;
    if(distance<std::abs(a-c)+.002f&&allowFold){
        // Same inner-reach treatment as the connected body. Bringing a gun
        // toward the face must not discard the entire tracked gun/arm pose.
        if(a>c)a=std::min(a,c+distance*.98f);else c=std::min(c,a+distance*.98f);
    }
    if(!allowFold&&distance<std::abs(a-c)+.001f)return false;
    const float along=(a*a-c*c+distance*distance)/(2*distance),bend=std::sqrt(std::max(0.f,a*a-along*along));
    Vec direction=Unit(reach);
    if(allowFold&&Length(direction)<.5f)direction=Unit(Direction(Row(target,2),Inverse(frame)));
    const Vec elbow=Point(Add(root,Add(Scale(direction,along),Scale(Pole(direction,left),bend))),frame),shoulder=Point(root,frame),wrist=Position(target);
    // Keep the established 1P solve for the weapon and fallback arms.
    for(int i=s;i<e;++i)out[i]=Segment(b[i],Position(b[s]),Position(b[e]),shoulder,elbow);
    for(int i=e;i<w;++i)out[i]=Segment(b[i],Position(b[e]),Position(b[w]),elbow,wrist);
    const Mat delta=Mul(Inverse(b[w]),target);for(int i=w;i<w+21;++i)out[i]=Mul(b[i],delta);
    out[s-1]=Mul(b[s-1],Mul(Inverse(b[s]),out[s]));return true;
}
bool BodyPalm(const Mat b[80],int w,Mat& wristFromPalm){
    // Stock 3P hands have ring/index/thumb chains, not the five 1P chains.
    // The ring chain represents the outer fingers. Construct the same palm
    // axes as PalmBinding from anatomical landmarks, not bone-axis guesses.
    const Vec origin=Position(b[w]),ring=Position(b[w+1]),index=Position(b[w+4]);
    const Vec across=Sub(index,ring),along=Sub(Scale(Add(index,ring),.5f),origin);
    if(Length(across)<.015f||Length(across)>.12f||Length(along)<.025f||Length(along)>.18f)return false;
    const Vec z=Unit(across),x=Unit(Cross(Scale(along,-1),z)),y=Cross(z,x);
    Mat palm=Identity();const Vec rows[]={x,y,z};
    for(int i=0;i<3;++i){palm.m[i][0]=rows[i].x;palm.m[i][1]=rows[i].y;palm.m[i][2]=rows[i].z;}
    SetPosition(palm,Add(Add(origin,Scale(along,.9f)),Scale(x,w==35?-.024f:.024f)));
    if(!Rigid(palm))return false;wristFromPalm=Mul(b[w],Inverse(palm));return Rigid(wristFromPalm);
}
bool BodyArm(Mat out[80],const Mat b[80],int s,int e,int w,const Mat& target,const Mat& torso,bool left,BodyArmStats& stats,float contactWeight,BodyArmSmoothing* smoothing,float elbowWeight=0){
    // The shoulder comes from this body's posed collar, so moving a tracked
    // hand cannot pull a detached 1P sleeve into the neck opening.
    const Vec shoulder=Position(b[s]),wrist=Position(target);
    Vec nativeElbow=Position(b[e]);
    if(left&&elbowWeight>0){
        // Stock left_elbow (16) and left_low_arm (17) straddle a short
        // half-bend segment. Bone 17 is not the hinge. Intersect the authored
        // upper/lower axes to recover that hinge instead of putting a hard
        // corner six centimetres down the sleeve. Guard nearly straight or
        // non-anatomical poses; ease only the AK support arm into this model.
        const Vec u=Unit(Sub(Position(b[e-1]),shoulder)),v=Unit(Sub(Position(b[w]),Position(b[e])));
        const Vec d=Sub(Position(b[e]),shoulder);const float cosine=Dot(u,v),denom=1-cosine*cosine;
        if(denom>.015f){
            const float upper=(Dot(d,u)-cosine*Dot(d,v))/denom;
            const float lower=(cosine*Dot(d,u)-Dot(d,v))/denom;
            const Vec pivot=Add(shoulder,Scale(u,upper));
            const float gap=Length(Sub(pivot,Add(Position(b[e]),Scale(v,lower))));
            if(upper>.12f&&upper<.4f&&lower<.02f&&lower>-.12f&&gap<.012f)
                nativeElbow=Add(nativeElbow,Scale(Sub(pivot,nativeElbow),Clamp(elbowWeight)));
        }
    }
    const Vec reach=Direction(Sub(wrist,shoulder),Inverse(torso));
    float a=Length(Sub(nativeElbow,shoulder)),c=Length(Sub(Position(b[w]),nativeElbow));
    const float measured=Length(reach);const int side=left?0:1;
    stats.reach[side]=measured;
    if(a<.05f||c<.05f||a>.6f||c>.6f||!std::isfinite(measured)||measured>2.f)return false;
    // These palms have already passed the established tracked-hand solve.
    // Stock 3P limbs are shorter, and their posed shoulders can sit farther
    // back. Its old 1.65x cutoff swapped the entire arm model on extension.
    // Stretch continuously to the same palm endpoint instead of changing mesh.
    const float distance=std::max(.001f,measured);
    const float stretch=std::max(1.f,distance/(a+c)*1.002f);a*=stretch;c*=stretch;
    stats.stretch[side]=stretch;
    // A wrist close to its shoulder needs the unequal limb lengths to fold.
    // Shorten only the longer segment enough to admit the triangle. Keep the
    // tracked wrist exact rather than rejecting this small inner reach region.
    if(distance<std::abs(a-c)+.002f){
        if(a>c)a=std::min(a,c+distance*.98f);else c=std::min(c,a+distance*.98f);
        stats.folded|=1u<<side;
    }
    const float along=(a*a-c*c+distance*distance)/(2*distance),bend=std::sqrt(std::max(0.f,a*a-along*along));
    Vec direction=Unit(reach);
    if(Length(direction)<.5f)direction=Unit(Direction(Sub(Position(b[w]),shoulder),Inverse(torso)));
    if(Length(direction)<.5f)direction={0,0,1};
    float swivel=0;
    const float wristAwareWeight=std::max(contactWeight,elbowWeight);
    Vec pole=left&&wristAwareWeight>0?ChargingElbowPole(b,s,e,w,target,torso,direction,along,bend,wristAwareWeight,swivel,nativeElbow):Pole(direction,left);
    if(smoothing&&smoothing->valid[side]){
        // Carry elbow bend in torso space and project onto this frame's reach
        // plane. Smooth the swivel, then reconstruct the triangle with both
        // shoulder and tracked wrist exact; never lerp detached arm endpoints.
        Vec old=Unit(Sub(smoothing->pole[side],Scale(direction,Dot(smoothing->pole[side],direction))));
        if(Length(old)>.5f){
            const float angle=std::atan2(Dot(Cross(old,pole),direction),Dot(old,pole));
            const float tau=.035f-(.025f*Clamp((std::abs(angle)-.08f)/.7f));
            const float alpha=smoothing->seconds/(smoothing->seconds+tau);
            pole=Direction(old,AxisTurn(direction,angle*alpha));
        }
    }
    const Vec elbow=Add(shoulder,Direction(Add(Scale(direction,along),Scale(pole,bend)),torso));
    const Vec nativePlane=Unit(Cross(Sub(nativeElbow,shoulder),Sub(Position(b[w]),nativeElbow)));
    const Vec solvedPlane=Unit(Cross(Sub(elbow,shoulder),Sub(wrist,elbow)));
    for(int i=s;i<e;++i)out[i]=PlaneSegment(b[i],shoulder,nativeElbow,shoulder,elbow,nativePlane,solvedPlane);
    for(int i=e;i<w;++i)out[i]=PlaneSegment(b[i],nativeElbow,Position(b[w]),elbow,wrist,nativePlane,solvedPlane);
    const float helperWeight=std::max(contactWeight,elbowWeight);
    if(helperWeight>0){
        // The middle elbow helper is skinned into both sides of the bend.
        // Carry half the CHANGE in bend, instead of leaving it rigidly on the
        // upper arm. Preserve the authored helper's offset/rotation at rest.
        const Mat upper=PlaneSegment(b[e],shoulder,nativeElbow,shoulder,elbow,nativePlane,solvedPlane);
        const Mat half=PartialRotation(upper,out[e],.5f*Clamp(helperWeight));
        Mat helper=out[e-1];SetPosition(helper,Sub(Position(helper),elbow));
        helper=Mul(helper,half);SetPosition(helper,Add(Position(helper),elbow));
        // The elbow helper may rotate halfway, but its origin must stay on
        // the upper-arm axis. Rotating that origin around bone 17 made a second
        // visible hinge. Preserve established charging poses at zero weight.
        SetPosition(helper,Add(Position(helper),Scale(Sub(Position(out[e-1]),Position(helper)),Clamp(elbowWeight))));
        out[e-1]=helper;
    }
    const Mat swungWrist=PlaneSegment(b[w],nativeElbow,Position(b[w]),elbow,wrist,nativePlane,solvedPlane);
    const Vec axis=Unit(Sub(wrist,elbow));const float roll=WristTwist(swungWrist,target,axis);
    stats.roll[side]=roll;stats.swivel[side]=swivel;
    for(int i=e+1;i<w;++i){
        const float fraction=Clamp(Dot(Sub(Position(out[i]),elbow),axis)/Length(Sub(wrist,elbow)));
        // Split the principal wrist rotation BEFORE filtering. An unwrapped
        // 340-degree wrist is equivalent to -20, but half of those rotations
        // is 170 versus -10: r35 accumulated an extra turn inside the sleeve.
        // Each helper now filters its own final rotation on the circle. A
        // full turn in its history is harmless because it is never scaled.
        float twist=roll*fraction;
        if(smoothing){
            const int helper=i-e-1;
            if(smoothing->valid[side]){
                const float old=smoothing->twist[side][helper];
                const float delta=std::remainder(twist-old,6.283185307f);
                // Soften a branch change near +/-pi without delaying normal
                // wrist tracking; only the visual sleeve helpers are filtered.
                const float tau=.025f*(1-.75f*Clamp((std::abs(delta)-.08f)/.7f))+
                    .070f*Clamp((std::abs(delta)-.8f)/1.2f);
                twist=old+delta*smoothing->seconds/(smoothing->seconds+tau);
            }
            twist=std::remainder(twist,6.283185307f);
            smoothing->twist[side][helper]=twist;
        }
        const Vec p=Position(out[i]);out[i]=Mul(out[i],AxisTurn(axis,twist));SetPosition(out[i],p);
    }
    if(smoothing){smoothing->pole[side]=pole;smoothing->valid[side]=true;}
    const Mat delta=Mul(Inverse(b[w]),target);
    for(int i=w;i<w+10;++i)out[i]=Mul(b[i],delta);
    // Collar and torso stay native; both arm roots remain at the shoulder.
    return true;
}
}
Mat BlendRigidPose(const Mat& from,const Mat& to,float fraction){
    const float t=Clamp(fraction);
    Mat result=Mul(from,PartialRotation(from,to,t));
    SetPosition(result,Add(Position(from),Scale(Sub(Position(to),Position(from)),t)));
    return result;
}
bool SolveLeftContact(const Mat b[70],const HandTargets& t,const Mat contact[21],Mat out[70]){
    // Solve the sleeve to the authored wrist, then put the detailed fingers
    // on the part. Keep right arm, weapon and all controller samples intact.
    for(int i=0;i<21;++i)if(!Rigid(contact[i]))return false;
    if(Length(Sub(Position(contact[0]),Position(out[7])))>.40f)return false;
    Mat candidate[70];std::memcpy(candidate,out,sizeof(candidate));
    if(!Arm(candidate,b,3,5,7,contact[0],t.head,t.torso,true,true))return false;
    for(int i=0;i<21;++i)candidate[7+i]=contact[i];
    for(int i=2;i<28;++i)if(!Rigid(candidate[i]))return false;
    std::memcpy(out+2,candidate+2,26*sizeof(Mat));return true;
}
namespace {
void StabilizeSupportedAim(const HandTargets& t,Mat& weapon,Vec& wrist,HandAimState& state){
    // Smooth only small movements near the face. The hand/weapon/fire solve
    // shares this result; raw controller poses still measure reload gestures.
    Mat pose=weapon;SetPosition(pose,wrist);
    pose=Mul(pose,Inverse(t.torso));
    const bool fresh=state.valid&&state.origin==t.origin&&t.frame>=state.frame&&
        t.time>=state.time&&t.time-state.time<100000000&&Rigid(state.pose);
    if(fresh&&t.frame==state.frame){pose=state.pose;}
    else if(fresh&&t.time>state.time){
        const float dt=static_cast<float>((t.time-state.time)*1.e-9);
        const float close=Clamp((.45f-Length(Sub(wrist,Position(t.head))))/.25f);
        if(close>0){
            const Quat q=RelativeRotation(state.pose,pose);
            const float angle=2*std::atan2(Length({q.x,q.y,q.z}),std::abs(q.w));
            const float shift=Length(Sub(Position(pose),Position(state.pose)));
            // Full response for deliberate changes; roughly 12ms time
            // constant only for sub-degree/sub-centimeter sight tremor.
            const float rotateTau=.012f*close*(1-Clamp((angle-.0044f)/.048f));
            const float moveTau=.012f*close*(1-Clamp((shift-.002f)/.018f));
            const Vec position=Add(Position(state.pose),Scale(Sub(Position(pose),Position(state.pose)),dt/(dt+moveTau)));
            if(rotateTau>0)pose=BlendRigidPose(state.pose,pose,dt/(dt+rotateTau));
            SetPosition(pose,position);
        }
    }
    state.pose=pose;state.frame=t.frame;state.origin=t.origin;state.time=t.time;state.valid=true;
    pose=Mul(pose,t.torso);wrist=Position(pose);SetPosition(pose,Position(weapon));weapon=pose;
}
}
bool SolveHands(const Mat b[70],const HandBinding& binding,const HandTargets& t,Mat out[70],bool& support,HandAimState* aimState){
    support=false;if(!binding.valid||!t.rightValid||!Rigid(t.rightGrip)||!Rigid(t.rightAim)||!Rigid(t.head)||!Rigid(t.torso))return false;
    for(int i=0;i<70;++i)if(!Rigid(b[i]))return false;
    HandAimState filtered=aimState?*aimState:HandAimState{};
    Mat weapon=t.rightAim,right=Mul(binding.right,weapon);
    SetPosition(right,t.palmsValid?Position(Mul(t.rightPalm,t.rightGrip)):Position(t.rightGrip));
    weapon=Mul(Inverse(binding.right),right);Mat left=Mul(binding.left,weapon);
    if(t.leftValid){
        if(!Rigid(t.leftGrip)||!Rigid(t.leftAim))return false;
        const float separation=Length(Sub(Position(t.leftGrip),Position(t.rightGrip))),distance=Length(Sub(Position(left),Position(t.leftGrip)));
        // Distance gates acquisition only. Once attached, keep the hand on
        // the foregrip until grip release, a reload interaction or tracking loss.
        support=t.supportPressed&&binding.leftReady&&(t.wasSupporting||(distance<.20f&&separation>.12f&&separation<.8f));
        const bool pumping=t.pumpPose&&t.supportPressed&&binding.leftReady&&t.pumpAimWeight>0;
        const Mat oneHand=weapon;
        if(support){
            const Vec authored=Sub(Position(binding.left),Position(binding.right));
            const Vec tracked=Sub(Position(t.leftGrip),Position(t.rightGrip));
            // Swing the current right-hand attitude onto the support line.
            // A projected-up-vector basis can flip when swept past its pole.
            // Fade the line's influence for nearly coincident controllers,
            // without opening the support hand or normalizing a zero vector.
            if(separation>.04f){
                const Mat aimed=Mul(oneHand,RotateBetween(Direction(authored,oneHand),tracked));
                weapon=separation>=.12f?aimed:BlendRigidPose(oneHand,aimed,Clamp((separation-.04f)/.08f));
            }
        }
        if(pumping){
            if(!Rigid(t.pumpAimFromRight))return false;
            const Mat held=Mul(t.pumpAimFromRight,t.rightAim);
            // Bypass the ordinary support-entry distance/12cm lever cutoff
            // while gripping the pump. Axial travel cannot drop/reacquire aim.
            weapon=t.pumpAimWeight>=1.f?held:BlendRigidPose(support?weapon:oneHand,held,t.pumpAimWeight);
            support=true;
        }
        if(support){
            Vec wrist=t.palmsValid?Position(Mul(t.rightPalm,t.rightGrip)):Position(t.rightGrip);
            if(aimState&&!pumping)StabilizeSupportedAim(t,weapon,wrist,filtered);
            else filtered.valid=false;
            right=Mul(binding.right,weapon);SetPosition(right,wrist);
            weapon=Mul(Inverse(binding.right),right);left=Mul(binding.left,weapon);
        }else if(t.palmsValid)left=Mul(t.leftPalm,t.leftGrip);
        else{left=Mul(binding.left,t.leftAim);SetPosition(left,Position(t.leftGrip));}
    }
    if(!support)filtered.valid=false;
    Mat candidate[70];std::memcpy(candidate,b,sizeof(candidate));
    if(!Arm(candidate,b,29,31,33,right,t.head,t.torso,false,true)||!Arm(candidate,b,3,5,7,left,t.head,t.torso,true,true))return false;
    const Mat delta=Mul(Inverse(b[54]),weapon);for(int i=54;i<70;++i)candidate[i]=Mul(b[i],delta);
    for(int i=2;i<70;++i)if(!Rigid(candidate[i]))return false;
    std::memcpy(out,candidate,sizeof(candidate));if(aimState)*aimState=filtered;return true;
}
bool SolveBodyArms(const Mat b[80],const Mat& leftPalm,const Mat& rightPalm,const Mat& torso,Mat out[80],BodyArmStats& stats,const Vec* wristPositions,float leftContactWeight,BodyArmSmoothing* smoothing,float leftElbowWeight){
    stats={};
    if(!Rigid(leftPalm)||!Rigid(rightPalm)||!Rigid(torso)){stats.rejected=1;return false;}
    for(int i=0;i<80;++i)if(!Rigid(b[i])){stats.rejected=2;return false;}
    Mat lp{},rp{};if(!BodyPalm(b,20,lp)||!BodyPalm(b,35,rp)){stats.rejected=3;return false;}
    Mat left=Mul(lp,leftPalm),right=Mul(rp,rightPalm);
    if(wristPositions){SetPosition(left,wristPositions[0]);SetPosition(right,wristPositions[1]);}
    if(!Rigid(left)||!Rigid(right)){stats.rejected=1;return false;}
    BodyArmSmoothing filtered=smoothing?*smoothing:BodyArmSmoothing{};
    Mat candidate[80];std::memcpy(candidate,b,sizeof(candidate));
    if(!BodyArm(candidate,b,15,17,20,left,torso,true,stats,leftContactWeight,smoothing?&filtered:nullptr,leftElbowWeight)){stats.rejected=4;return false;}
    if(!BodyArm(candidate,b,31,33,35,right,torso,false,stats,0,smoothing?&filtered:nullptr)){stats.rejected=5;return false;}
    for(int i=0;i<80;++i)if(!Rigid(candidate[i])){stats.rejected=6;return false;}
    std::memcpy(out,candidate,sizeof(candidate));if(smoothing)*smoothing=filtered;return true;
}
bool MapLaunch(const Mat& source,const Mat& camera,const Mat& nativeWeapon,const Mat& tracked,bool weaponSpace,Mat& out){
    const Mat& anchor=weaponSpace?nativeWeapon:camera;
    if(!Rigid(source)||!Rigid(anchor)||!Rigid(tracked)||Length(Sub(Position(source),Position(anchor)))>2)return false;
    out=Mul(Mul(source,Inverse(anchor)),tracked);return Rigid(out);
}
}
