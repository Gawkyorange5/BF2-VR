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
    if(!scopeWeapon||handWeapon!=scopeWeapon||Value<void*>(scopeWeapon,0x28)!=handOwner)return false;
    out=scopeView;
    // The lens must use the SAME native instance transform as the visible
    // housing. Predicting it again from FireFrame before renderer interpolation
    // gave the M4/AK74U and angled ERYX a second, moving "ghost" glass disc.
    // Keep the accepted sniper path unchanged (its aperture remains implicit).
    if(out.reflex||Length(Position(out.aperture))>.001f){
        void* mesh=Value<void*>(handWeapon,0x38);Mat drawn{};
        if(Value<void*>(mesh)!=render+0x21B350||Value<int>(mesh,0x28C)!=0||
           !Read(Value<void*>(Value<void*>(mesh,0x25C)),0,drawn)||!Rigid(drawn))return false;
        out.bore=drawn;out.gun=Mul(out.aperture,drawn);
        if(out.geometry){
            const auto& g=*out.geometry;void* definition=Value<void*>(mesh,0x258);
            if(g.count<3||g.count>32||g.part>=16||Value<void*>(definition)!=render+0x21A760)return false;
            const int parts=Value<int>(definition,0x120);
            if(parts<1||parts>32||g.part>=static_cast<unsigned>(parts))return false;
            Mat attached=drawn;
            if(g.part&&(!Read(Value<void*>(Value<void*>(mesh,0x25C),g.part*4),0,attached)||!Rigid(attached)))return false;
            // Follow the actual cover/slide palette, not an independently
            // predicted pose. A reload can articulate this part away from root.
            out.gun=Mul(g.local,attached);out.aperture=Mul(out.gun,Inverse(drawn));
            if(!Rigid(out.gun)||Length(Position(out.aperture))>1.5f)return false;
        }
        const Vec e=Point(Position(views[1].eye),Inverse(out.gun));
        const Vec forward=Direction(Row(views[1].eye,2),Inverse(out.gun));
        const float relief=out.center.z-e.z,farRelief=out.reflex?.75f:.48f;
        if(relief<.035f||relief>farRelief||forward.z<.88f)return false;
        const float radial=ScopeEyeOffset(out,e);
        const auto clamp=[](float v){return std::max(0.f,std::min(1.f,v));};
        out.visibility[activeEye]=clamp((1.2f-radial)*3.f)*clamp((relief-.035f)/.025f)*clamp((farRelief-relief)/.10f);
        static unsigned reports=0;if(reports++<4)Log("SCOPE96 lens uses drawn native mesh; prediction difference=%.4fm",Length(Sub(Position(out.gun),Position(scopeView.gun))));
    }
    visibility=out.visibility[activeEye];if(visibility<.02f)return false;
    out.connected=(out.launcher||out.connectionRing)&&LauncherConnected(handWeapon);
    gunToEye=Mul(out.gun,Inverse(views[1].eye));projection=views[1].projection;
    eyeInGun=Point(Position(views[1].eye),Inverse(out.gun));return true;
}
