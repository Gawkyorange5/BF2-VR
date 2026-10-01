// Reference excerpts only: these blocks belong to different existing scopes.
// Do not compile this file as a standalone translation unit.

// Within Runtime::Initialize, after system/session/format setup.
// Origin: RFX/XrSessionClient.cpp, source line 264
    XrViewConfigurationView sizes[2]={{XR_TYPE_VIEW_CONFIGURATION_VIEW},{XR_TYPE_VIEW_CONFIGURATION_VIEW}};
    if(!Check(xrEnumerateViewConfigurationViews(instance,systemId,XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO,2,&count,sizes),"view sizes")||count!=2)return false;
    unsigned requestedW=0,requestedH=0;AutoResolution::Size(requestedW,requestedH);
    for(int e=0;e<2;++e){
        eyeRecommendations[e]=sizes[e];const bool useRequested=requestedW&&requestedH&&requestedW<=sizes[e].maxImageRectWidth&&requestedH<=sizes[e].maxImageRectHeight;
        if(!MakeChain(chains[e],useRequested?requestedW:sizes[e].recommendedImageRectWidth,useRequested?requestedH:sizes[e].recommendedImageRectHeight))return false;
    }
    char resolutionPath[MAX_PATH];sprintf_s(resolutionPath,"%sbf2_vr_resolution.ini",GamePath());
    const unsigned recommendedW=std::max(sizes[0].recommendedImageRectWidth,sizes[1].recommendedImageRectWidth);
    const unsigned recommendedH=std::max(sizes[0].recommendedImageRectHeight,sizes[1].recommendedImageRectHeight);
    char resolutionNumber[32];sprintf_s(resolutionNumber,"%u",recommendedW);
    WritePrivateProfileStringA("OpenXR","width",resolutionNumber,resolutionPath);
    sprintf_s(resolutionNumber,"%u",recommendedH);WritePrivateProfileStringA("OpenXR","height",resolutionNumber,resolutionPath);
    WritePrivateProfileStringA("OpenXR","runtime",properties.runtimeName,resolutionPath);
    Log("RESOLUTION49 OpenXR recommended per-eye=%ux%u; game source logged when stereo starts",recommendedW,recommendedH);

// Runtime functions using the existing chain/frame-lock implementation.
// Origin: RFX/XrSessionClient.cpp, source line 337
void RecordEyeSize(unsigned w,unsigned h){
    char path[MAX_PATH],n[32];sprintf_s(path,"%sbf2_vr_resolution.ini",GamePath());
    sprintf_s(n,"%u",w);WritePrivateProfileStringA("Actual","native_width",n,path);
    sprintf_s(n,"%u",h);WritePrivateProfileStringA("Actual","native_height",n,path);
    sprintf_s(n,"%u",chains[0].width);WritePrivateProfileStringA("Actual","transfer_width",n,path);
    sprintf_s(n,"%u",chains[0].height);WritePrivateProfileStringA("Actual","transfer_height",n,path);
    Log("RESOLUTION49 confirmed native eye=%ux%u submitted L=%ux%u R=%ux%u",w,h,chains[0].width,chains[0].height,chains[1].width,chains[1].height);
}
void MatchEyeTransfer(unsigned w,unsigned h){
    Guard lock;
    if(!available||!open||!w||!h||!Config().nativeEyeTransfer||(w==lastNativeWidth&&h==lastNativeHeight))return;
    lastNativeWidth=w;lastNativeHeight=h;
    unsigned widths[2]{},heights[2]{};bool changed=false;
    for(int e=0;e<2;++e){
        const auto& r=eyeRecommendations[e];
        const bool smaller=w<=r.maxImageRectWidth&&h<=r.maxImageRectHeight&&
            (AutoResolution::Requested()||(std::uint64_t)w*h<(std::uint64_t)r.recommendedImageRectWidth*r.recommendedImageRectHeight);
        widths[e]=smaller?w:r.recommendedImageRectWidth;heights[e]=smaller?h:r.recommendedImageRectHeight;
        changed=changed||widths[e]!=chains[e].width||heights[e]!=chains[e].height;
    }
    Log("RESOLUTION49 actual game render/eye=%ux%u; OpenXR recommended=%ux%u; transfer L=%ux%u R=%ux%u (transfer resize alone adds no detail)",w,h,eyeRecommendations[0].recommendedImageRectWidth,eyeRecommendations[0].recommendedImageRectHeight,widths[0],heights[0],widths[1],heights[1]);
    if(!changed){RecordEyeSize(w,h);return;}
    // Allocate both replacements first. If either fails, keep the working
    // stereo pair instead of leaving only one resized eye or a blank image.
    Chain replacement[2];bool ready=true;
    for(int e=0;e<2&&ready;++e)ready=MakeChain(replacement[e],widths[e],heights[e]);
    if(ready){
        for(int e=0;e<2;++e)std::swap(chains[e],replacement[e]);
        Log("PERF native eye transfer source=%ux%u eyes=L%ux%u R%ux%u; original XR FOV/poses retained",w,h,chains[0].width,chains[0].height,chains[1].width,chains[1].height);
    }else Log("PERF native eye transfer allocation failed; keeping previous stereo sizes");
    for(auto& old:replacement)if(old.handle)xrDestroySwapchain(old.handle);
    RecordEyeSize(w,h);
}
