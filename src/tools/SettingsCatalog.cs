// Generated from RFX/LiveSettingsCatalog.inl. Keep both interfaces in sync.
internal static class SettingsCatalog {
    internal static Option[] Options(){return new Option[]{
            new Option(0,true,"input","smooth_turn","Smooth turning","Off: turn physically with your headset.",0m,1m,1m,1m),
            new Option(0,false,"input","turn_speed","VR turn speed","240 = default; 480 = 2x input rate. BF2 sensitivity also affects turning.",10m,1500m,10m,240m),
            new Option(1,false,"hud","scale","HUD scale","Smaller values bring the HUD edges into view. Default 0.85.",.35m,1.4m,.05m,.85m),
            new Option(1,false,"hud","depth","HUD depth (metres)","Changes distance while keeping apparent size steady.",2m,30m,.5m,12m),
            new Option(1,true,"hud","gun_crosshair","Gun-following crosshair","Crosshair follows the tracked gun.",0m,1m,1m,1m),
            new Option(1,false,"hud","gun_crosshair_distance","Crosshair distance (metres)","Sets the aiming marker's display distance.",3m,100m,1m,25m),
            new Option(1,false,"hud","gun_crosshair_degrees","Crosshair size (degrees)","Changes the aiming marker's visible size.",.15m,2m,.05m,.6m),
            new Option(2,true,"body","show_native_full_body","Show full body","Off: tracked first-person arms only. On: torso and legs too.",0m,1m,1m,0m),
            new Option(2,true,"body","use_native_arms","Connected body arms","Full body on: connects arms to the torso. Off: first-person arms.",0m,1m,1m,1m),
            new Option(2,true,"body","high_detail_hands","Detailed hands","Uses high-detail hands with connected body arms.",0m,1m,1m,1m),
            new Option(2,true,"body","weapon_depth","Arms hide overlapping weapon","Recommended on: avoids seeing the weapon through your arms.",0m,1m,1m,1m),
            new Option(3,true,"vehicle","head_aim","Vehicle head aiming","Aim supported mounted guns with your head and right stick.",0m,1m,1m,1m),
            new Option(3,true,"vehicle","gunner_position_lock","Lock position in gunner seat","Keeps your viewpoint at the gunner seat while looking around.",0m,1m,1m,1m),
            new Option(3,false,"vehicle","aim_stick_speed","Gunner stick speed (degrees/sec)","Moves the aiming center while head aiming stays active.",10m,180m,5m,75m),
            new Option(3,false,"vehicle","flight_speed","Aircraft flight stick speed","300 = default pitch/roll input rate. Separate from on-foot turning.",20m,1500m,10m,300m),
            new Option(4,true,"scopes","enable","Physical scopes","Enables magnified views through supported scopes.",0m,1m,1m,1m),
            new Option(4,false,"scopes","magnification","Scope magnification","Magnification used by the physical scope view.",1m,8m,.25m,4m),
    };}
}
