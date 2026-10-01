// Reference excerpts only: these blocks belong to different existing scopes.
// Do not compile this file as a standalone translation unit.

// Cinema/menu quad aspect, separate from eye projection.
// Origin: RFX/CleanVR.cpp, source line 130
float CinemaAspect(unsigned w,unsigned h){
    const float desktop=RenderWindow::DesktopAspect();
    // BF2's normalized menu layout is authored for a flat widescreen canvas.
    // Changing the quad shape does not resample its texture or the stereo eyes.
    return desktop>0&&Engine::MenuActive()?1.f/desktop:(w?(float)h/w:0);
}

// Controller ray to normalized menu coordinates.
// Origin: RFX/CleanVR.cpp, source line 148
struct Hit{bool valid=false;float u=0,v=0;};
Hit Pointer(const Runtime::Frame& f,float aspect){
    Hit hit{};if(!panelValid||!f.hands[1].aimValid||!f.valid||aspect<=0)return hit;
    const Vec center=Point({0,0,Config().distance},panel),normal=Row(panel,2),origin=Position(f.hands[1].aim);
    const Vec ray=Unit(Add(Row(f.hands[1].aim,2),Scale(Row(panel,1),Config().pointerLift)));
    const float denominator=Dot(ray,normal);if(denominator<.05f)return hit;
    const float t=Dot(Sub(center,origin),normal)/denominator;if(t<=0)return hit;
    const Vec delta=Sub(Add(origin,Scale(ray,t)),center);const float width=1.15f+1.55f*Config().scale;
    hit.u=.5f+Dot(delta,Row(panel,0))/width;hit.v=.5f-Dot(delta,Row(panel,1))/(width*aspect);
    hit.valid=hit.u>=0&&hit.u<=1&&hit.v>=0&&hit.v<=1;return hit;
}

// Within InputFrame: map normalized hit to the desktop cursor; publish kit pointer separately.
// Origin: RFX/CleanVR.cpp, source line 179
    RECT client{};GetClientRect(window,&client);const int w=client.right,h=client.bottom;
    if(Cinema()){
        Controls::Menu(f);
        // Pointer intersection and visible menu quad must use the same shape.
        Hit hit=Pointer(f,nativeWidth?CinemaAspect(nativeWidth,nativeHeight):CinemaAspect(w,h));
        if(hit.valid){POINT p{(LONG)(hit.u*(w-1)),(LONG)(hit.v*(h-1))};ClientToScreen(window,&p);SetCursorPos(p.x,p.y);}
        if(Engine::DeploymentActive()){
            Controls::PointerButton(false);Engine::PublishPointer(hit.valid,hit.u,hit.v,f.hands[1].trigger>.55f,f.id);
        }else{Engine::PublishPointer(false,0,0,false,f.id);Controls::PointerButton(hit.valid&&f.hands[1].trigger>.55f);}return;
    }

// Inside DrawScene after eye upload; desktop crop only.
// Origin: RFX/CleanVR.cpp, source line 375
    // Reuse an already resolved eye after its headset upload. Crop only the
    // desktop world; the complete isolated HUD is composited on top afterward.
    IDirect3DSurface9* mirror=nullptr;RECT crop{};
    const float desktopAspect=RenderWindow::DesktopAspect();
    if(rendered&&desktopAspect>0){
        if(batchCapture.mirrorReady){
            const auto& r=batchCapture.mirrorRegion;mirror=batchCapture.target;
            crop={(LONG)r.x,(LONG)r.y,(LONG)(r.x+r.width),(LONG)(r.y+r.height)};
        }else if(captures[1].target){
            mirror=captures[1].target;crop={0,0,(LONG)captures[1].width,(LONG)captures[1].height};
        }
        const auto& fov=f.views[1].fov;
        const float eyeAspect=(std::tan(fov.angleRight)-std::tan(fov.angleLeft))/
            (std::tan(fov.angleUp)-std::tan(fov.angleDown));
        if(mirror&&std::isfinite(eyeAspect)&&eyeAspect>.1f&&eyeAspect<10.f){
            // Use the angular projection aspect, not the eye texture's pixel
            // aspect; headset render pixels need not have equal angular scale.
            const LONG w=crop.right-crop.left,h=crop.bottom-crop.top;
            const LONG cw=std::max(1L,std::min(w,(LONG)std::floor(w*desktopAspect/eyeAspect)));
            const LONG ch=std::max(1L,std::min(h,(LONG)std::floor(h*eyeAspect/desktopAspect)));
            crop.left+=(w-cw)/2;crop.top+=(h-ch)/2;crop.right=crop.left+cw;crop.bottom=crop.top+ch;
        }else mirror=nullptr;
    }
    HudCapture::CompositeDesktop(mirror,mirror?&crop:nullptr);
