sampler2D scene : register(s0);
sampler2D nativeConnection : register(s1);
sampler2D launcherReticle : register(s2);
sampler2D originalSight : register(s3);
sampler2D originalLockBox : register(s4);
sampler2D originalStatus : register(s5);
float4 custom : register(c4); // native sight ID, art ready, native lock, status ready
float4 flags : register(c5); // native PrimaryHeatBlink / PrimaryClipBlink
float4 params : register(c0); // visibility, reticle half-width, capture coverage, reflex
float4 native : register(c1); // wire launcher, live native missile connection, black etching ready, Stinger

float4 shape : register(c3); // measured convex opening vs legacy circular aperture
float4 seeker : register(c2); // target in reticle coords, native box width, valid target

float4 main(float2 aperture : TEXCOORD0, float2 uv : TEXCOORD1, float2 reticle : TEXCOORD2) : COLOR0 {
    float r = length(aperture);
    clip(lerp(1 - r, 1, shape.x));
    float3 color = tex2D(scene, uv).rgb;
    // Sight coordinates are independent of capture reprojection. Preserve
    // accepted sniper marks and 1x dots. The permanent black launcher marks
    // are separate from the red native HUD's live missile-connection ring.
    float2 d = abs(reticle);
    float dotRadius = params.y * 1.5;
    float dotAA = max(.0001, fwidth(length(reticle)) * .65);
    float center = 1 - smoothstep(max(0, dotRadius-dotAA), dotRadius+dotAA, length(reticle));
    // Keep the accepted saturated red. Give the antialiased reflex dot a
    // stronger opacity so bright scenery cannot wash its small core out.
    // Radius/position stay unchanged; sniper coverage and native HUD gates
    // stay as before. No white/pink centre, halo or additional rendering pass.
    center = saturate(center * (1 + .8 * params.w));
    float crosshair = max(
        (1 - step(params.y, d.x)) * step(params.y * 4, d.y) * (1 - step(.23, d.y)),
        (1 - step(params.y, d.y)) * step(params.y * 4, d.x) * (1 - step(.23, d.x)));
    color = lerp(color, lerp(float3(1, .84, .37), float3(1, .12, .035), params.w),
        saturate(center + crosshair * (1 - params.w)) * (1 - max(native.x, native.w)) * (1-step(.5,custom.x)));
    float blackMarks = tex2D(launcherReticle, reticle + .5).a * native.z;
    color *= 1 - blackMarks;
    // Fit more of the Stinger artwork inside its existing physical lens.
    // Scene magnification and the target's projected centre remain unchanged.
    float stingerScale = .8;
    float2 sightUV = reticle / lerp(1, stingerScale, native.w) + .5;
    float sightInside = step(0, sightUV.x) * step(0, sightUV.y) * step(sightUV.x, 1) * step(sightUV.y, 1);
    float4 connection = tex2D(nativeConnection, sightUV);
    connection.a *= lerp(1, sightInside, native.w);
    color = lerp(color, connection.rgb, connection.a * native.y);
    // Stinger uses its native sight and two native target-box states. The lock
    // flag comes from BF2; no synthetic timer, target search or lock feedback.
    [branch] if (native.w > .5) {
        color = lerp(color, connection.rgb * float3(0, .8, 0), connection.a * .9);
        [branch] if (seeker.w > .5) {
            float2 lockUV = (reticle - seeker.xy) / max(.01, seeker.z * stingerScale) + .5;
            float inside = step(0, lockUV.x) * step(0, lockUV.y) * step(lockUV.x, 1) * step(lockUV.y, 1);
            float4 box = tex2Dlod(launcherReticle, float4(lockUV, 0, 0));
            color = lerp(color, box.rgb * float3(0, .8, 0), box.a * .9 * inside);
        }
    }
    [branch] if(custom.x>.5){
        // Original DZB2 marks replace the generic optic dot/cross. Both eyes
        // use the same finite-bore zero and native target position.
        float2 hudUV=(custom.x>15.5&&custom.x<16.5)?float2(aperture.x,-aperture.y)*.5+.5:reticle+.5;
        float inside=step(0,hudUV.x)*step(0,hudUV.y)*step(hudUV.x,1)*step(hudUV.y,1);
        float4 art=tex2Dlod(originalSight,float4(hudUV,0,0));
        color=lerp(color,art.rgb,art.a*custom.y*inside);
        [branch] if(seeker.w>.5){
            float2 boxUV=(reticle-seeker.xy)/max(.01,seeker.z)+.5;
            float inBox=step(0,boxUV.x)*step(0,boxUV.y)*step(boxUV.x,1)*step(boxUV.y,1);
            float4 box=tex2Dlod(originalLockBox,float4(boxUV,0,0));
            // Keep Javelin target marks inside its transparent viewing window.
            float window=(custom.x>15.5&&custom.x<16.5)?1-art.a:1;
            color=lerp(color,box.rgb,box.a*inBox*window);
        }
        [branch] if(custom.x>15.5&&custom.x<16.5&&custom.w>.5){
            float2 tile=saturate(hudUV)*.5;
            float4 seek=tex2Dlod(originalStatus,float4(tile,0,0));
            float4 heat=tex2Dlod(originalStatus,float4(tile+float2(.5,0),0,0));
            float4 clipState=tex2Dlod(originalStatus,float4(tile+float2(0,.5),0,0));
            float4 battery=tex2Dlod(originalStatus,float4(tile+float2(.5,.5),0,0));
            color=lerp(color,seek.rgb,seek.a*(1-custom.z)*inside);
            color=lerp(color,heat.rgb,heat.a*flags.x*inside);
            color=lerp(color,clipState.rgb,clipState.a*flags.y*inside);
            color=lerp(color,battery.rgb,battery.a*flags.y*inside);
        }else if(custom.x>13.5&&custom.x<14.5&&custom.w>.5&&custom.z>.5){
            float2 statusUV=(reticle-float2(0,.33))/float2(.40,.10)+.5;
            float on=step(0,statusUV.x)*step(0,statusUV.y)*step(statusUV.x,1)*step(statusUV.y,1);
            float4 status=tex2Dlod(originalStatus,float4(statusUV,0,0));
            color=lerp(color,status.rgb*float3(.925,.424,.267),status.a*on);
        }
    }
    // The capture now covers the aperture for both eyes. Only the round
    // physical lens defines its edge; do not stamp a second rectangular mask.
    color *= params.x * lerp(saturate((1 - r) * 35), 1, shape.x);
    return float4(color, 1);
}
