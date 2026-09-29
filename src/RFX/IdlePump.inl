// The old RFX sample ran a console script on EVERY empty PeekMessage poll.
// Clean OpenXR input/rendering does not use that script. Keep an explicit
// compatibility option for users who have their own RFX script loop.
namespace {
void UpdateIdlePump(){
    struct Stats {
        ULONGLONG window=0,lastMenu=0;
        unsigned polls=0,scripts=0,scriptSkips=0,menus=0,menuSkips=0;
        double scriptMs=0,menuMs=0,maxMs=0;
        bool announced=false,menuSeen=false,inUpdate=false;
    };
    static thread_local Stats stats;
    // A console command can pump messages recursively. Do not re-enter the
    // optional script or attempt to patch the same menu object recursively.
    if(stats.inUpdate)return;
    struct Busy {bool& flag;Busy(bool& f):flag(f){flag=true;}~Busy(){flag=false;}} busy(stats.inUpdate);
    const auto& config=CleanVR::Config();
    const ULONGLONG now=GetTickCount64();
    if(!stats.announced){
        stats.announced=true;stats.window=now;
        CleanVR::Log("PERF idle pump legacyScript=%d menuPollMs=%u; OpenXR controls remain on the frame path",config.legacyScriptLoop?1:0,config.menuPollMs);
    }
    ++stats.polls;double spent=0;
    if(config.legacyScriptLoop){
        const double start=CleanVR::FrameClockMs();
        DICE::invoke("run Common/Scripts/MainLoop.con");
        const double elapsed=CleanVR::FrameClockMs()-start;
        ++stats.scripts;stats.scriptMs+=elapsed;spent+=elapsed;
    }else ++stats.scriptSkips;

    // DICE::update only discovers/re-hooks newly created SwiffPlayer menu
    // instances. Once installed, its mouse hook still runs for EVERY native
    // mouse update; this interval does not throttle pointing or clicking.
    if(!stats.menuSeen||!config.menuPollMs||now-stats.lastMenu>=config.menuPollMs){
        stats.menuSeen=true;stats.lastMenu=now;
        const double start=CleanVR::FrameClockMs();DICE::update();
        const double elapsed=CleanVR::FrameClockMs()-start;
        ++stats.menus;stats.menuMs+=elapsed;spent+=elapsed;
    }else ++stats.menuSkips;
    if(spent>stats.maxMs)stats.maxMs=spent;
    if(now-stats.window>=5000){
        CleanVR::Log("PERF idle windowMs=%llu polls=%u scriptRuns=%u scriptSkipped=%u menuChecks=%u menuSkipped=%u scriptMs=%.3f menuMs=%.3f maxWorkMs=%.3f (window totals; saved time is not measured)",
            now-stats.window,stats.polls,stats.scripts,stats.scriptSkips,stats.menus,stats.menuSkips,stats.scriptMs,stats.menuMs,stats.maxMs);
        stats.window=now;stats.polls=stats.scripts=stats.scriptSkips=stats.menus=stats.menuSkips=0;
        stats.scriptMs=stats.menuMs=stats.maxMs=0;
    }
}
}
