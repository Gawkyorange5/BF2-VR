bool ScopeReady(){return pair&&scopeReady&&scopeAvailable;}
void ScopeAvailable(bool available){scopeAvailable=available;if(!available)scopeReady=false;}
bool BeginScope(){
    if(!ScopeReady()||scopePass)return false;
    // The stereo projection validator intentionally rejects near-zero headset
    // FOVs. A telescope needs a much narrower, but still finite, frustum.
    // Validate and prepare BOTH projections before touching either camera.
    const float t=scopeView.tangent;
    Mat projections[2]{};
    for(int i=0;i<2;++i){
        const float n=i==1?4.f:views[i].nearZ,f=i==1?5.f:views[i].farZ;
        if(!std::isfinite(t+n+f)||t<.001f||t>1.f||n<=0||f<=n){
            static unsigned reports=0;if(reports++<4)Log("SCOPE40 camera rejected view=%d tangent=%.6f near=%.5f far=%.2f",i,t,n,f);
            return false;
        }
        Mat& p=projections[i];p.m[0][0]=p.m[1][1]=1/t;
        p.m[2][2]=f/(f-n);p.m[2][3]=1;p.m[3][2]=-n*f/(f-n);
    }
    eyeActive=false;activeEye=2;ReleaseBodyDepth();
    for(auto& v:views)if(!Copy(static_cast<BYTE*>(v.object)+0x18,v.bytes,sizeof(v.bytes)))return false;
    for(int i=0;i<2;++i){auto& v=views[i];v.eye=scopeView.camera;
        const float nearZ=i==1?4.f:v.nearZ,farZ=i==1?5.f:v.farZ;
        v.projection=projections[i];
        v.eyeNear=nearZ;v.cullFov=2*std::atan(scopeView.tangent);v.aspect=1;
        // ApplyLens derives far delta from farZ. Preserve normal pair data.
        const float savedFar=v.farZ;v.farZ=farZ;ApplyLens(v);v.farZ=savedFar;
    }
    scopePass=eyeActive=true;
    for(auto& v:views){originalSetter(v.object,&v.eye);BuildProjection(v.object,nullptr);}
    return true;
}
void EndScope(){
    scopePass=eyeActive=false;activeEye=-1;
    if(pair)for(auto& v:views)Copy(static_cast<BYTE*>(v.object)+0x18,v.bytes,sizeof(v.bytes));
}
bool ScopeEye(ScopeView& out,Mat& gunToEye,Mat& projection,Vec& eyeInGun,float& visibility){
    if(!ScopeReady()||scopePass||!eyeActive||activeEye<0||activeEye>1)return false;
    visibility=scopeView.visibility[activeEye];if(visibility<.02f)return false;
    out=scopeView;gunToEye=Mul(scopeView.gun,Inverse(views[1].eye));projection=views[1].projection;
    eyeInGun=Point(Position(views[1].eye),Inverse(scopeView.gun));return true;
}
