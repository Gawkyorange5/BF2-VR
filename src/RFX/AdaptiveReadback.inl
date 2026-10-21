// Bounded calibration on real, current stereo pairs. No extra renders, GPU
// timestamp queries, synthetic frames, or changes to image size/quality.
struct AdaptiveReadback {
    enum { Samples=24, Blocks=7 };
    bool started=false,settled=false,selected=false;
    unsigned warm=120,block=0,count=0,slow=0,attempts=0;
    double work[Samples]{},transfer[Samples]{},workMedian[Blocks]{},transferMedian[Blocks]{},workTail[Blocks]{};
    std::uint64_t previous=0;
    bool WantsDirect()const{return started&&(settled?selected:(block&1u)!=0);}
    static double Median(double* values,unsigned n){
        std::sort(values,values+n);return (values[(n-1)/2]+values[n/2])*.5;
    }
    void Stop(const char* reason){
        if(started&&!settled)Log("PERF64 adaptive readback selected=ordinary reason=%s",reason);
        settled=true;selected=false;
    }
    void Observe(double workMs,double transferMs,bool direct,bool eligible,std::uint64_t frame){
        if(!started||settled)return;
        // Scope changes, menus, failed pairs and cinema gaps cannot establish
        // a comparable block. Keep the current method, discard partial data.
        const bool gap=previous&&frame!=previous+1;previous=frame;
        if(++attempts>2400){Stop("insufficient comparable stereo pairs");return;}
        if(!eligible||gap||!std::isfinite(workMs)||!std::isfinite(transferMs)||workMs<=0||transferMs<0){
            count=slow=0;warm=std::max(warm,4u);return;
        }
        if(direct!=WantsDirect()){Stop("driver fallback");return;}
        if(direct&&workMs>std::max(workMedian[block-1]*2.,workMedian[block-1]+20.)){
            if(++slow>=4){Stop("candidate caused repeated long frames");return;}
        }else slow=0;
        if(warm){--warm;return;}
        work[count]=workMs;transfer[count]=transferMs;
        if(++count<Samples)return;
        workMedian[block]=Median(work,Samples);transferMedian[block]=Median(transfer,Samples);
        workTail[block]=work[(Samples*90-1)/100];
        Log("PERF64 adaptive block=%u method=%s workMedianMs=%.3f workP90Ms=%.3f transferMedianMs=%.3f samples=%u",
            block,direct?"direct":"ordinary",workMedian[block],workTail[block],transferMedian[block],(unsigned)Samples);
        count=slow=0;warm=4;
        if(++block<Blocks)return;
        // A/B/A/B/A/B/A brackets each candidate with ordinary frames. Require
        // repeatable transfer savings and no overall scene-work regression.
        // Large changes in scene cost make the evidence inconclusive.
        double workRatio[3]{},transferRatio[3]{},tailRatio[3]{},savings[3]{};unsigned wins=0;bool comparable=true;
        for(unsigned i=0;i<3;++i){
            const unsigned a=2*i,b=a+1,c=a+2;
            const double baseWork=(workMedian[a]+workMedian[c])*.5;
            const double baseTransfer=(transferMedian[a]+transferMedian[c])*.5;
            if(baseWork<=0||baseTransfer<=0||std::max(workMedian[a],workMedian[c])>
                std::min(workMedian[a],workMedian[c])*1.25){comparable=false;break;}
            workRatio[i]=workMedian[b]/baseWork;transferRatio[i]=transferMedian[b]/baseTransfer;
            tailRatio[i]=workTail[b]/((workTail[a]+workTail[c])*.5);
            savings[i]=baseTransfer-transferMedian[b];
            if(workRatio[i]<=1.03&&tailRatio[i]<=1.15&&transferRatio[i]<.90&&savings[i]>.25)++wins;
        }
        if(!comparable){Stop("scene workload changed too much for a reliable comparison");return;}
        const double wr=Median(workRatio,3),tr=Median(transferRatio,3),tail=Median(tailRatio,3),saved=Median(savings,3);
        selected=wins>=2&&wr<=1.0&&tail<=1.10&&tr<.90&&saved>.25;settled=true;
        Log("PERF64 adaptive readback selected=%s wins=%u/3 workRatio=%.3f workP90Ratio=%.3f transferRatio=%.3f savedMedianMs=%.3f; selection only, not an FPS benchmark",
            selected?"direct":"ordinary",wins,wr,tail,tr,saved);
    }
};
