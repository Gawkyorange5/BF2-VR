// r55 belt-fed interactions. Called under reloadLock for the owned local rig.
// No renderer, global weapon, inventory count, or server ammo patches here.
float LmgClamp(float x){return std::fmax(0.f,std::fmin(1.f,x));}
Mat LmgCoverPose(float progress,int follower=-1){
    const auto& p=*reload.lmg;
    if(!p.coverBone||p.coverKeyCount<2)return Identity();
    for(unsigned i=1;i<p.coverKeyCount;++i){
        const auto& a=p.coverKeys[i-1];const auto& b=p.coverKeys[i];
        if(progress<=b.progress||i+1==p.coverKeyCount)
            return ReloadBlendPose(follower<0?a.pose:a.followers[follower],follower<0?b.pose:b.followers[follower],LmgClamp((progress-a.progress)/(b.progress-a.progress)));
    }
    return follower<0?p.coverKeys[0].pose:p.coverKeys[0].followers[follower];
}
float LmgCoverProgress(Vec q,float& side){
    const auto& p=*reload.lmg;float best=1.e9f,result=reload.coverOpen;
    for(unsigned i=1;i<p.coverKeyCount;++i){
        const auto& a=p.coverKeys[i-1];const auto& b=p.coverKeys[i];
        const Vec from=Point(p.coverGrab,a.pose),to=Point(p.coverGrab,b.pose),delta=Sub(to,from);
        const float squared=Dot(delta,delta);if(squared<1.e-10f)continue;
        const float t=LmgClamp(Dot(Sub(q,from),delta)/squared);
        const float error=Length(Sub(q,Add(from,Scale(delta,t))));
        if(error<best){best=error;result=a.progress+t*(b.progress-a.progress);}
    }
    side=best;return result;
}
Mat LmgBeltPose(int i,float progress){
    const auto& p=*reload.lmg;
    return ReloadBlendPose(p.beltLoose[i],p.beltSeated[i],progress);
}
void LmgCue(ReloadAudio::LmgCue cue){
    if(reloadSoundProfile&&reload.profile&&ReloadAudio::Ready()){
        ReloadAudio::PlayLmg(reload.profile->soundIndex,cue);
        Log("MRELOAD55 cue weapon=%s event=%u",reload.profile->name,static_cast<unsigned>(cue));
    }
}
void LmgReadyToCharge(ULONGLONG now){
    reload.lmgGripValid=false;
    if(reload.needsCharge){
        reload.handlePull=reload.handlePeak=0;reload.handleCycled=false;
        ReloadStep(ReloadPhase::HandleReady);
    }else if(!reload.nativeStarted){reload.originalMagazineHeld=false;ReloadStep(ReloadPhase::Idle);}
    else{reload.finishTick=now;ReloadStep(ReloadPhase::Finish);}
}
void BeginLmgContact(const HandTargets& target,const Mat& part,const Mat local[21],Vec anchor){
    Mat shaped[70]{};for(int i=0;i<21;++i)shaped[7+i]=Mul(local[i],part);
    const Vec world=Point(anchor,part);
    Mat wristFromPalm{};
    if(PalmBinding(shaped,7,wristFromPalm)){
        Mat palm=Mul(Inverse(wristFromPalm),shaped[7]),tracked=target.leftGrip;
        SetPosition(palm,{});SetPosition(tracked,{});
        const Mat turn=Mul(Inverse(palm),tracked);
        for(int i=7;i<28;++i){
            SetPosition(shaped[i],Sub(Position(shaped[i]),world));
            shaped[i]=Mul(shaped[i],turn);
            SetPosition(shaped[i],Add(Position(shaped[i]),world));
        }
    }
    reload.lmgGripValid=true;const Mat inv=Inverse(part);
    for(int i=0;i<21;++i){reload.lmgGrip[i]=Mul(shaped[7+i],inv);if(!Rigid(reload.lmgGrip[i]))reload.lmgGripValid=false;}
    // Track the contact point, so wrist rotation alone cannot open a cover.
    reload.lmgGripPoint=Point(world,Inverse(target.leftGrip));
}
bool UpdateLmgReload(const HandTargets& target,const Mat solved[70],ULONGLONG now,
                     bool grab,bool pressed,bool released,bool ammoFresh,bool canRemove,
                     float magazineDistance,float optionalHandleDistance){
    if(!reload.lmg)return false;
    const auto& p=*reload.lmg;const Mat& gun=solved[54];
    const Vec palm=Position(target.leftGrip);
    if(reload.phase==ReloadPhase::Idle&&p.coverBone&&canRemove&&ammoFresh&&
       reload.ammo.state<2&&!reloadInput.bypass&&now>=reload.bypassUntil){
        const Mat part=Mul(LmgCoverPose(0),gun);
        const float d=Length(Sub(palm,Point(p.coverGrab,part)));
        // An optional rack still works when explicitly grabbing the handle.
        if(pressed&&optionalHandleDistance<reload.profile->handleGrabRadius&&
           optionalHandleDistance<d&&optionalHandleDistance<magazineDistance)return false;
        ReloadNear(d,.13f,now);
        if(pressed){
            Log("MRELOAD55 cover grab weapon=%s coverDistance=%.3f boxDistance=%.3f",p.name,d,magazineDistance);
            if(d<.16f){
                ResetLmgReload();reload.started=now;reload.startRounds=reload.ammo.rounds;reload.startMagazines=reload.ammo.magazines;
                reload.handleOnly=false;reload.magazineSeated=false;reload.request=false;reload.nativeStarted=false;
                reload.originalMagazineHeld=true;reload.needsCharge=false;
                BeginLmgContact(target,part,p.coverHand,p.coverGrab);
                ReloadStep(ReloadPhase::CoverPull);ReloadFeedback(.4f,45);
            }
        }
        return true; // Closed cover gates magazine removal.
    }
    if(reload.phase==ReloadPhase::CoverReady||reload.phase==ReloadPhase::CoverCloseReady){
        const Mat part=Mul(LmgCoverPose(reload.coverOpen),gun);
        const float d=Length(Sub(palm,Point(p.coverGrab,part)));
        ReloadNear(d,.13f,now);
        if(pressed&&d<.16f){
            reload.coverClosing=reload.phase==ReloadPhase::CoverCloseReady;
            BeginLmgContact(target,part,p.coverHand,p.coverGrab);
            ReloadStep(ReloadPhase::CoverPull);ReloadFeedback(.4f,45);
        }
        return true;
    }
    if(reload.phase==ReloadPhase::CoverPull){
        const Vec q=Point(Point(reload.lmgGripPoint,target.leftGrip),Inverse(gun));
        float side=0;const float progress=LmgCoverProgress(q,side);
        if(side<=.18f)reload.coverOpen=progress;
        const bool complete=side<=.18f&&(reload.coverClosing?progress<=.18f:progress>=.82f);
        if(complete){
            const bool closing=reload.coverClosing;
            reload.coverOpen=closing?0.f:1.f;reload.lmgGripValid=false;
            Log("MRELOAD55 cover %s weapon=%s progress=%.3f side=%.3f",closing?"closed":"opened",p.name,progress,side);
            LmgCue(closing?ReloadAudio::CoverClose:ReloadAudio::CoverOpen);ReloadFeedback(.6f,65);
            if(closing)LmgReadyToCharge(now);else ReloadStep(ReloadPhase::GrabInstalled);
        }else if(released||side>.22f){
            reload.lmgGripValid=false;
            ReloadStep(reload.coverClosing?ReloadPhase::CoverCloseReady:ReloadPhase::CoverReady);
            Log("MRELOAD55 cover parked weapon=%s progress=%.3f side=%.3f",p.name,reload.coverOpen,side);
        }
        return true;
    }
    if(reload.phase==ReloadPhase::BeltReady){
        const Mat part=Mul(LmgBeltPose(p.beltCount-1,reload.beltProgress),gun);
        const float d=Length(Sub(palm,Point(p.beltTip,part)));
        ReloadNear(d,.10f,now);
        if(pressed&&d<.13f){
            BeginLmgContact(target,part,p.beltHand,p.beltTip);
            const Vec from=Point(p.beltTip,p.beltLoose[p.beltCount-1]),to=Point(p.beltTip,p.beltSeated[p.beltCount-1]);
            // Anchor controller movement at the current belt contact, with
            // progress retained after a partial release/re-grab.
            reload.lmgHandStart=Sub(Point(Point(reload.lmgGripPoint,target.leftGrip),Inverse(gun)),Scale(Sub(to,from),reload.beltProgress));
            ReloadStep(ReloadPhase::BeltFeed);ReloadFeedback(.4f,45);
        }
        return true;
    }
    if(reload.phase==ReloadPhase::BeltFeed){
        const Vec from=Point(p.beltTip,p.beltLoose[p.beltCount-1]),to=Point(p.beltTip,p.beltSeated[p.beltCount-1]);
        const Vec path=Sub(to,from);const float length=Length(path);const Vec axis=Unit(path);
        const Vec delta=Sub(Point(Point(reload.lmgGripPoint,target.leftGrip),Inverse(gun)),reload.lmgHandStart);
        const float along=Dot(delta,axis),side=Length(Sub(delta,Scale(axis,along)));
        const bool onPath=side<.14f&&along>-.12f&&along<length+.18f;
        if(onPath){
            reload.beltProgress=LmgClamp(along/std::fmax(.001f,length));
            const bool wasReady=reload.beltPeak>=.75f;
            reload.beltPeak=std::fmax(reload.beltPeak,reload.beltProgress);
            if(!wasReady&&reload.beltPeak>=.75f)ReloadFeedback(.6f,65);
        }
        if(released||!onPath){
            reload.lmgGripValid=false;
            const bool accepted=reload.beltPeak>=.75f;
            Log("MRELOAD55 belt release weapon=%s along=%.3f side=%.3f peak=%.3f accepted=%d",p.name,along,side,reload.beltPeak,accepted?1:0);
            if(accepted){
                reload.beltProgress=1;reload.beltSeated=true;LmgCue(ReloadAudio::BeltSeat);ReloadFeedback(.6f,65);
                if(p.coverBone)ReloadStep(ReloadPhase::CoverCloseReady);else LmgReadyToCharge(now);
            }else ReloadStep(ReloadPhase::BeltReady);
        }
        return true;
    }
    return false;
}
void ApplyLmgParts(Mat solved[70]){
    if(!reload.lmg||!ReloadActive()||reload.handleOnly)return;
    const auto& p=*reload.lmg;
    if(p.coverBone)solved[p.coverBone]=Mul(LmgCoverPose(reload.coverOpen),solved[54]);
    for(unsigned i=0;i<p.coverFollowerCount;++i)
        solved[p.coverFollowerBones[i]]=Mul(LmgCoverPose(reload.coverOpen,int(i)),solved[54]);
    const bool carried=reload.phase==ReloadPhase::Remove||reload.phase==ReloadPhase::Pouch||reload.phase==ReloadPhase::Insert;
    const Mat box=carried?Mul(Inverse(reload.profile->magazineInstalled),solved[reload.profile->magazineBone]):solved[54];
    for(int i=0;i<p.beltCount;++i){
        // Box and belt move together. The stock animation must never leave
        // detached cartridges floating at the receiver or the supply pouch.
        const Mat local=carried?p.beltLoose[i]:(reload.magazineSeated?LmgBeltPose(i,reload.beltProgress):p.beltSeated[i]);
        solved[LmgBeltBone(p,i)]=Mul(local,box);
    }
}
void ApplyLmgContact(const Mat basis[70],const HandTargets& target,Mat solved[70],bool grab,int& kind,Mat contactLocal[21]){
    if(!reload.lmg||!grab||!reload.lmgGripValid)return;
    const bool cover=reload.phase==ReloadPhase::CoverPull,belt=reload.phase==ReloadPhase::BeltFeed;
    if(!cover&&!belt)return;
    const auto& p=*reload.lmg;
    const Mat& part=solved[cover?p.coverBone:LmgBeltBone(p,p.beltCount-1)];
    Mat posed[21];for(int i=0;i<21;++i)posed[i]=Mul(reload.lmgGrip[i],part);
    if(SolveLeftContact(basis,target,posed,solved)){
        kind=3;const Mat inv=Inverse(solved[54]);
        for(int i=0;i<21;++i)contactLocal[i]=Mul(solved[7+i],inv);
    }
}
