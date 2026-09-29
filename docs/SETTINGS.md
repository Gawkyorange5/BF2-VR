# VR settings

## Inside the headset

Hold both stick clicks for **0.6 seconds**, then release them. Point the right controller at a tab, slider or button and squeeze the right trigger. Use plus/minus or drag sliders. Click **Apply** to save and activate changes.

Close with **Close + recenter**, **B**, or another both-stick hold after releasing the clicks. Closing recenters automatically, so hold your head in your usual playing position. Closing without Apply discards pending edits. The game keeps running while this menu is open; gameplay controller input is suppressed.

| Page | Options |
| --- | --- |
| Turning | Smooth turning and VR turn speed |
| HUD | Scale, depth, gun-following crosshair, crosshair distance and size |
| Body / arms | Full body or arms only, connected body arms, detailed hands, weapon/body occlusion |
| Vehicles | Head aiming, gunner seat position lock, gunner stick speed, aircraft flight stick speed |
| Scopes | Physical scopes and magnification |

The gun crosshair follows weapon aim. Turning it **off** hides both it and the old fixed infantry reticle. Vehicle reticles and other HUD elements are retained.

Turn speed is an **input rate**, not degrees per second; BF2 mouse sensitivity also matters. The settings app's reset default is 240; 480 doubles that input rate. The confirmed release INI uses the creator's faster 670 preference, adjustable from 10–1500. Turning smooth turning off leaves physical headset rotation available. Gunner and flight stick speeds have their own controls.

Full body follows the game's stance and lower-body animation; it is not full-body tracking. Body appearance options take effect while alive on foot. Physical crouch/prone are planned; use X/Y for the current stance controls.

## Desktop app and saved files

Run **BF2-VR-Settings.exe** in the game folder. It offers the same categories and can apply changes live. It identifies the target vr_stereo.ini and saves only edited settings. If another editor changed the file, use **Reload saved values** before applying conflicting edits. **Reset this tab** resets that page only.

Settings are stored beside BF2-VR.exe in **vr_stereo.ini**. Existing settings are retained during Setup. Both settings interfaces back up the INI before their first save. A read-only game folder prevents saving; the app reports the error.

## Performance

CPU/GPU diagnostic sampling is disabled in the release INI. No external recorder is needed. The accepted stereo transport optimizations remain in the baseline. Do not assume the same FPS on different machines or in different scenes. Scopes, combat density, graphics options, headset resolution, power mode and runtime reprojection affect the result. Reports should include the runtime overlay and bf2_clean_vr.log, with hardware and the scene described. A displayed FPS value with Spacewarp active is not proof of that many freshly rendered native game frames.
