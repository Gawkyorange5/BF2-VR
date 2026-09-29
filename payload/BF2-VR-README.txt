

# BF2-VR

Battlefield 2 in VR: OpenXR stereo, tracked weapons and hands, a visible body, manual reloads, vehicle head aiming, and settings inside the headset.

**First patch your own Battlefield 2 installation to version 1.5 and get it running successfully in flat-screen mode.** Join a match, spawn, move and fire before adding VR.

## Download and install

1. Get **BF2-VR-Setup.exe** from [Releases](https://github.com/Gawkyorange5/BF2-VR/releases).
2. Follow the [installation guide](docs/INSTALL.md), including the headset and runtime prerequisites.
3. Run Setup, choose the folder containing your BF2.exe, select your BF2 profile, and click **Install BF2-VR**.
4. Connect your headset and launch **Battlefield 2 VR** from the new desktop shortcut.
5. Sit or stand in your playing position. **Tap both stick clicks to recenter. Hold both for VR settings.**

No modified BF2.exe is distributed. Setup creates a separate VR executable from your own installed game. SteamVR is **not required** with a compatible OpenXR runtime such as Virtual Desktop's VDXR. No OpenVR library is used.

**Release:** 1.0.0, based on the creator-confirmed r45 gameplay baseline. This is a community mod, not an official EA/DICE product.

## Features

- Stereoscopic OpenXR rendering, headset rotation and position tracking, and tracked motion controllers.
- Motion-controlled weapons, arms and bullet aiming; two-hand weapon support.
- Optional full body with connected arms, detailed hands, body/arm smoothing, and crouch/prone animations. Arms-only mode is also available.
- Physical magazine removal, fresh magazine pickup at the hip, insertion and charging on supported weapons. Fresh magazines and individual shells attach to the tracked hand.
- Partial and empty manual reloads, manual bolt/pump cycling, reload sounds tied to interaction steps, and firing as soon as the required steps finish. Native button reload remains available.
- Pump operation while keeping the support grip held: squeeze the left trigger to operate the pump.
- Gun-following infantry crosshair. Turning it off also hides the original fixed infantry crosshair. Vehicle reticles remain available.
- Physical magnified sniper scopes, usable at either eye, on the five supported stock sniper rifles.
- Head-directed aiming for supported mounted guns and vehicle gunner seats, with right-stick aiming adjustment and a seat-position lock option.
- Aircraft cockpit cameras follow aircraft roll and tilt, with headset look movement relative to the seat.
- Consistent vehicle boost, flares and alternate-fire buttons across land vehicles, planes and helicopters, where the vehicle provides those actions.
- Head-relative locomotion, smooth turning with live speed adjustment, jump/parachute, stance controls, kit cycling and utility shortcuts.
- Anchored cinema/menu presentation and controller pointing/clicking in the main menu and kit/spawn screen. Loading/death presentation uses the existing retained-image handling.
- Stereo HUD with adjustable scale and depth.
- **Inside-headset VR settings:** turning, HUD/crosshair, body/arms, vehicles and scopes. Changes apply while playing. Closing the menu recenters.
- A desktop VR settings app, saved preferences and backups.
- Optimized stereo transport and render paths; optional CPU/GPU diagnostic sampling is off by default. Actual frame rate depends on hardware, scene, settings and runtime.
- Public installer with supported-executable checks, recommended controller profile, desktop shortcut, backups and removal/restoration.

Read [all controller buttons](docs/CONTROLS.md), [manual reloads and supported weapons](docs/MANUAL-RELOADS.md), [VR settings](docs/SETTINGS.md), and [known limitations/troubleshooting](docs/TROUBLESHOOTING.md).

## Planned next work

- LMG manual reloading.
- Physical weapon and gadget slots.
- Physical crouch and prone.
- Infantry launcher scopes.
- Physical gadget actions.
- Physical throwing of medkits, ammo boxes and grenades.
- Physical defibrillator interactions.

These are planned features, not included promises or delivery dates. Current crouch/prone use controller buttons; body pose follows the game stance.

## Requirements and compatibility

Windows 10/11, a complete supported 32-bit BF2 1.5 installation, the x86 Microsoft runtimes, and an OpenXR runtime with **32-bit application support**. The established headset setup is Quest 3 with Touch-style controllers through Virtual Desktop/VDXR. Other runtimes, headsets, mods and controllers need compatibility reports; the installer does not imply they have been tested.

The native hooks target specific BF2/RendDX9 code. Setup rejects unsupported executable variants rather than patching unknown offsets. Start with stock BF2. Mod selection in the shortcut does not guarantee custom weapon, scope or vehicle compatibility. Existing game unlock rules still apply.

## Credits and source

Created by **Gawkyorange5** with development assistance from Codex.

Built on [BadSanta12345's RFX](https://github.com/BadSanta12345/RFX), with substantial reference/adapted work from [justiceofrip's BF2142VR](https://github.com/justiceofrip/BF2142VR) and its BF2 porting guidance. [JayBiggsGMG's Battlefield 1942 VR](https://github.com/JayBiggsGMG/BFVR-Battlefield-1942-VR-Mod) inspired the project. OpenXR loader/headers are from [Khronos](https://github.com/KhronosGroup/OpenXR-SDK).

See [third-party notices](THIRD_PARTY.md), [license scope](LICENSE-NOTICE.md), and [building from source](docs/BUILDING.md). Game executables, meshes, textures, animations, audio, account files and CD keys are not included. Setup derives short reload cues from the player's own installation.


# Installation — start here

## 1. Get Battlefield 2 working flat-screen first

1. Install your own legitimate copy of Battlefield 2.
2. Apply the official **1.5 patch** (and any preceding patch your installation requires).
3. Launch the normal game. Create/select a profile, load a map, spawn, move and fire successfully.
4. Close the game completely.

BF2-VR does not install the game, update it to 1.5, unlock weapons, or repair a broken flat-screen installation. No replacement game executable is supplied. Keep a backup of your game and existing mod configuration if you already use another modification.

## 2. Prepare the headset and runtimes

### Virtual Desktop / Quest

1. Install and set up Virtual Desktop on the headset and its Streamer on the PC.
2. Connect the headset to the PC and confirm that Virtual Desktop works.
3. In the Streamer's OpenXR runtime selection, select **VDXR / VirtualDesktopXR**.
4. Leave the headset connected before launching BF2-VR. SteamVR is not needed on this path.

See the [VDXR project guide](https://github.com/mbucchia/VirtualDesktop-OpenXR/wiki) for runtime setup. If you do not use Virtual Desktop, select your headset's compatible OpenXR runtime. SteamVR may serve as an optional OpenXR runtime; it is not mandatory. BF2-VR is a 32-bit application, so the runtime must support 32-bit OpenXR applications. Alternative runtimes have not all been validated with this mod.

### Windows dependencies

- Windows 10 or 11 with .NET Framework 4.8 for Setup and the desktop settings app.
- Microsoft's latest supported **Visual C++ v14 x86** redistributable. Install x86 even on 64-bit Windows. [Official Microsoft downloads](https://learn.microsoft.com/en-us/cpp/windows/latest-supported-vc-redist?view=msvc-170).
- **DirectX End-User Runtimes (June 2010)** for the legacy D3DX9 library. Having DirectX 12 installed does not supply every legacy helper. Download from [Microsoft](https://www.microsoft.com/en-us/download/details.aspx?id=8109), extract it to a temporary folder, then run DXSETUP.exe there.
- BF2's own MSVCP71/MSVCR71 libraries must already be present. If they are missing, repair the original game installation.

Setup includes the 32-bit OpenXR loader and RFX's STL wrapper. It does not install runtimes or alter the system's active OpenXR setting for you.

## 3. Install BF2-VR

1. Open this repository's **Releases** page. Download **BF2-VR-Setup.exe**. Do not use GitHub's automatic Source code ZIP as the installer.
2. Run Setup. Choose the **Battlefield 2 folder containing BF2.exe**, not Documents and not the mods/bf2 subfolder.
3. Choose the numbered BF2 profile you use. The profiles are found through Windows' Documents location, usually Documents/Battlefield 2/Profiles/0001. If no profile appears, run BF2 flat-screen, create one, close it and reopen Setup.
4. Leave **Apply recommended controller bindings** checked for the documented controls. This replaces that profile's Controls.con and makes a backup. Your previous custom keyboard/mouse bindings can therefore change. Account, video, login, server and other profile files are not copied or replaced.
5. Leave **Create a Battlefield 2 VR desktop shortcut** checked. Choose **bf2** for the initial stock-game setup. Other installed mods appear in the list, but custom content compatibility is not guaranteed.
6. Check the box confirming you have patched to 1.5 and played flat-screen, then click **Install BF2-VR**.
7. Wait for the success message. Do not close Setup while it is installing.

If the game is in a protected folder and Setup reports access denied, close it and use **Run as administrator** on Setup. Use the same Windows account that owns your BF2 Documents profile. Setup never changes folder permissions or disables security software.

Setup checks the code fingerprints of BF2.exe and RendDX9.dll. An unsupported-build message means the exact native hook layout is not recognized. Report it instead of bypassing the check or replacing the game with a random executable.

### What Setup does

- Creates **BF2-VR.exe** from your local BF2.exe and adds the RFX import to the copy. The original BF2.exe is untouched.
- Installs RFX.dll, openxr_loader.dll, stl_8_wrapper.dll and BF2-VR-Settings.exe.
- Generates 116 short reload cues from your own Objects_client.zip, then embeds them in the local RFX.dll. The downloaded DLL contains no extracted game audio.
- Creates vr_stereo.ini if absent. Existing VR settings are retained.
- Optionally installs the recommended Controls.con into the selected Documents profile and creates a desktop shortcut.
- Saves originals and an installation journal inside **.bf2-vr** in the game folder. Keep this folder for restoration.

## 4. First launch

1. Connect the headset and start the chosen OpenXR runtime.
2. Launch the **Battlefield 2 VR** desktop shortcut. Alternatively run BF2-VR.exe in the game folder.
3. Select the profile chosen in Setup. Point the right controller at menu items and squeeze its trigger to click.
4. Load a map, choose a kit/spawn point with the pointer, and spawn.
5. Hold your head at your usual seated or standing height. **Tap and release both stick clicks together to recenter.**
6. **Hold both stick clicks for 0.6 seconds** to open VR settings. Release both, then point/click. Apply saves; closing recenters automatically.

The original BF2.exe remains your normal flat-screen launch when it originally had no VR import. If your pre-existing executable was already modified to import RFX, Setup preserves that original behavior too; it does not remove an existing modification.

## Update, remove or restore

For this first installer version, close BF2 and the settings app. Rerun the downloaded Setup executable, select the same game folder and choose **Remove / Restore** before installing another Setup version. Existing installation journals are never overwritten.

Removal restores backed-up files and removes new files if they still match what Setup installed. Files you edited after installation are preserved, and Setup lists the conflicts and keeps the backups. For example, if you changed a newly installed vr_stereo.ini, copy it to a safe location, remove only that conflicting installed file, then rerun Remove / Restore. If you changed Controls.con, save the version you want before choosing between it and its numbered backup. Do not delete the entire .bf2-vr folder while restoration is incomplete.

Setup does not remove game content or the original BF2.exe. Runtime-generated logs and settings-app backup files may remain. The setup log is **%LOCALAPPDATA%/BF2-VR-Setup.log**.


# Controller guide

For Touch-style controllers: A/B on the right controller, X/Y and the menu button on the left. Grip means the side grip button; trigger means the index-finger trigger. These tables assume the recommended Controls.con was applied to the active BF2 profile by Setup.

## Kit screen, recenter, pause and VR settings

These work on foot and in vehicles, including aircraft and gunner seats.

| Input | Action |
| --- | --- |
| Tap and release both joystick clicks together | Recenter |
| Hold both joystick clicks for 0.6 seconds | VR settings menu |
| Close VR settings | Recenter automatically |
| Tap left controller menu button | Kit / spawn screen |
| Hold left controller menu button for 0.6 seconds | Pause / menu |
| Keyboard F6 | Recenter |

The both-stick shortcut takes priority over boost and flares. After using it,
release both clicks before using an individual click again. No X button is
required for kit selection or recentering. A hold does not also trigger a tap.
The kit screen requires an active match. In VR settings, point with the right
controller and squeeze its trigger. Click Apply to save; Close + recenter, B,
or another both-click hold closes and recenters. See [VR settings](SETTINGS.md).

## Vehicles: land, airplanes and helicopters

| Input | Action |
| --- | --- |
| Left joystick click | Boost, where the vehicle supports it |
| Right joystick click | Flares / countermeasures, where supported |
| Left trigger | Alternate fire / native secondary weapon action |
| Right trigger | Primary fire |
| Left stick up/down | Throttle |
| Left stick left/right | Steering / yaw |
| Right stick | Aircraft pitch/roll; gunner aiming adjustment in other seats |
| Head direction | Aim supported mounted guns; look around as pilot |
| Right grip | Use / exit |
| A | Jump / parachute |
| B | Native reload |
| Tap X alone | Change weapon/fire mode |
| Hold X | Vehicle utility shortcuts below |

A very short right click deploys flares on release. Holding it begins sending
the flare key after an 80 ms grace period, which allows both-stick shortcuts
to be recognized without spending a countermeasure when the clicks are
slightly staggered. Press both together to use VR settings/recenter.

Head aiming retains the existing seat position lock and right-stick aiming
center adjustment. Center the right stick after entering/changing seats.
The inputs request BF2's native actions; they do not add abilities to a vehicle.

## Infantry

| Input | Action |
| --- | --- |
| Left stick | Move relative to level head direction |
| Left joystick click | Sprint |
| Right stick left/right | Smooth turn |
| Flick right stick up/down, then center | Next / previous kit item |
| Right trigger | Fire; tracked pointer click in menus |
| Left trigger | Grab magazine, shell or charging handle on supported weapons |
| Left grip near the support position | Support the gun |
| Left grip held on a pump + left trigger | Operate the pump |
| Right grip | Use / enter vehicle |
| A | Jump and parachute request; press again while falling |
| B | Native button reload |
| Hold X | Crouch |
| Y | Prone |
| Tap right joystick click alone | Change weapon/fire mode |
| Hold right joystick click | Infantry utility shortcuts below |

Manual charging is available after a partial reload as well as an empty
reload. Fresh magazines and individual shells move into the tracked hand
when grabbed. B remains the native reload fallback. Left trigger does not
send infantry ADS.

## Utility shortcuts

Hold **X in vehicles**, or **right joystick click on foot**, then use:

| Additional input | Action |
| --- | --- |
| A | Pick up / exchange kit |
| B | Map size |
| Right joystick click in vehicles; X on foot | Scoreboard |
| Y | Squad screen |
| Right grip | Kit / spawn screen |
| Right trigger | Select slot 4; release utility button before firing |
| Left grip | Push to talk |
| Left trigger | Radio / commo rose |
| Right stick up/down | Cycle seat-selection keys F1–F8 |

Using a utility shortcut consumes the utility button, so releasing it does
not also change fire mode. For radio, use the right stick to move the cursor
and the right trigger to select, or the tracked pointer when a menu appears.

## Profile and transitions

Select the same BF2 profile you chose in Setup. Release held buttons/triggers after spawning, changing seats or closing a menu, then press them again. Center the right stick to rearm its axes. Both-stick shortcuts take priority over individual stick clicks.

The recommended Controls.con contains compatible keyboard/mouse bindings for BF2's infantry and vehicle control maps. If you change those bindings in BF2, the controller's synthesized inputs will follow your changes. Setup does not replace your account or video settings.

In the main menu and deployment screen, use the right-controller ray and trigger click. The left trigger is reserved for physical reload interactions on foot, so it does not request traditional ADS. Aim by raising the weapon; supported physical scopes activate through eye alignment.

The vehicle Y button has no standalone assignment. Left grip has no additional standalone driving action. Boost, flares and alternate fire depend on the vehicle/seat actually having those abilities.


# Manual reloads and physical scopes

## Magazine-fed weapons

1. Bring your left hand to the installed magazine and use the **left trigger** to grab it.
2. Pull it out and release it.
3. Reach to the replacement-ammunition area near your hip. Press the left trigger to take a fresh magazine; it appears in your tracked hand.
4. Bring it to the magazine well and seat it. Release the trigger as needed to let go.
5. On weapons with a profiled charging handle, grab the handle with the left trigger, move it through its travel and return it. Complete the required steps to fire.

Charging is available after partial reloads as well as empty ones. Interaction cues play at their steps. Emptying a manually supported weapon does not complete an automatic reload for you. **B** remains the native reload fallback. A completed manual reload allows firing immediately; BF2's normal fire rate and ammunition rules still apply.

## Shotguns and bolt-action rifles

- **Shell-fed shotguns:** take one shell from the hip area with the left trigger, place it at the loading port, then repeat. Magazine-fed Saiga/Jackhammer use their magazine interaction instead.
- **Pump guns:** keep the **left grip** held at the support/pump location; squeeze the **left trigger** to engage the pump, move back and forward, then release the trigger. Completion has a small assistance threshold.
- **Bolt-action rifles:** after firing, grab and cycle the bolt through the profiled path, including its return/closure. Reload the magazine separately when required.
- Native button reload and physical cycling remain available. If a particular weapon gets stuck, switch weapons or use the native reload fallback and report the weapon, ammo state and exact sequence.

## Profiled weapon list

Availability depends on team, map, installed content and normal unlocks. Some share a profile family; no unlocks are supplied. A profile being included does not mean every map/team combination has been exhaustively tested.

| Family | Included stock profiles |
| --- | --- |
| Assault rifles / carbine | M16A2, M16/M203, AK-47, AK-47/GP-25, AK-101, AK-101/GP-30, G3, L85A1/SA80, L85A2, G36E, F2000, FAMAS, M4 |
| SMGs | MP5, PP-19 Bizon, Type 85, P90, MP7 |
| Magazine shotguns | Saiga-12, Jackhammer |
| Shell-fed shotguns | Norinco 982, Protecta, Benelli M4, Remington 11-87 |
| Snipers | SVD/Dragunov, Type 88, M24, M95 Barrett, L96A1 |

There are **29 internal profiles**. They cover the listed weapons and variants, not every BF2 weapon. LMGs, pistols, launchers and unlisted/custom weapons use native button reload. Some stock assets have no independently profiled charging part; those use their available steps rather than an invented handle.

## Physical sniper scopes

M24, M95 Barrett, L96A1, SVD and Type 88 have profiled physical optics. Raise the scope to either eye and align along the rifle. The view darkens outside the intended eye relief/alignment. Magnification (1–8) is adjustable in VR settings. The reticle is visually zeroed at 100 m; it does not predict bullet drop or remove native spread.

The scope needs an extra world render while aligned, so performance may differ from normal aiming. Infantry launcher scopes and other custom optics are planned/unprofiled.


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


# Troubleshooting and limitations

## Setup

- **Unsupported executable:** use a complete BF2 1.5 installation. Setup validates BF2.exe and RendDX9.dll code, since native hook offsets depend on them. Report the exact legitimate game edition/build if rejected. No bypass or replacement executable is supplied.
- **Missing runtime DLL:** install the current Microsoft Visual C++ v14 **x86** package and DirectX June 2010 runtime from the links in INSTALL.md. Missing MSVCP71/MSVCR71 means the underlying game installation needs attention.
- **Missing reload audio:** a complete original mods/bf2/Objects_client.zip is required. Setup builds its short cues from this archive. It does not download assets.
- **Access denied:** close the game/settings app, then rerun Setup with appropriate access to the game folder. Protected installations may need Run as administrator. Do not alter security software or broad directory permissions.
- **Existing installation:** use Remove / Restore first. Keep .bf2-vr until restoration completes; it contains backups. If files changed after installation, Setup preserves and lists them instead of overwriting them.

## No VR / black headset

Launch **BF2-VR.exe**, not an unmodified BF2.exe. Connect the headset first and select an OpenXR runtime with 32-bit app support. With Virtual Desktop, select VDXR and check the runtime shown in its overlay. Do not add OpenVR DLLs. Read **bf2_clean_vr.log** in the game folder for initialization errors. Confirm the normal flat-screen game still runs.

## Buttons, shooting or movement do not work

Use the same numbered profile selected in Setup and apply its recommended Controls.con. The controller layer relies on these keyboard/mouse bindings. Focus the game window. Release triggers/buttons and center sticks after menus, spawn and seat changes. The VR settings menu suppresses gameplay inputs until closed. Unsupported manual-reload weapons still use **B** for native reload.

## Reloads / arms / scopes

Report the exact weapon, team/map, remaining ammo and action sequence. Custom meshes can differ from the stock profiles. Some AK support-arm sleeve twisting remains a known visual limitation; it is not claimed completely fixed. Scopes are profiled for M24, L96A1, M95, SVD and Type 88 only. Align your eye with the lens, and confirm physical scopes are enabled in VR settings. Launcher scopes are planned.

## Performance

Use the normal launch; no recorder is needed. The r45 runtime keeps the creator's accepted performance work and has optional diagnostics off. No fixed FPS is promised. Plug a portable PC into power for comparable measurements and compare the same map, location, bot count, resolution and graphics settings. Include an overlay showing FPS, Game, Encoding, Network and Decode timing and reprojection/Spacewarp state. Fast head movement can expose delayed/reprojected image edges when frames arrive late. A still screenshot alone cannot establish sustained native FPS.

## Compatibility status

The creator tested the r45 baseline in the headset. Quest 3 / Touch-style controls / VDXR are the established setup. Installer and loader verification results are documented separately in VERIFICATION.md. The release has not been exhaustively validated across hardware, every weapon/map, multiplayer services or custom BF2 mods. Anti-cheat compatibility is not claimed: follow server rules and use allowed environments. The project does not bypass anti-cheat.

## Reporting a problem

Open a GitHub issue with release version, Windows version, GPU, headset, runtime, game edition, mod/map, and exact steps. Attach bf2_clean_vr.log and a short clip if the issue is visual. For setup issues, attach %LOCALAPPDATA%/BF2-VR-Setup.log. Review logs before sharing; remove account identifiers or personal paths you do not want public. Do not upload game executables, full game archives, CD keys or profile/account files.


# License scope

This repository combines original BF2-VR work with third-party components. No single license is asserted over the entire tree.

- RFX base code and its STL wrapper are included with upstream permission confirmed by the BF2-VR maintainer. The supplied RFX archive did not contain a general public license. This permission is not a claim that RFX is MIT-licensed; contact its author regarding separate reuse.
- Adapted BF2142VR code retains the MIT notice in licenses/BF2142VR-LICENSE.txt.
- Khronos OpenXR headers/loader retain their upstream license and copyright notices. See licenses/OpenXR-LICENSE.txt and the source header notices.
- Original BF2-VR contributions and release tooling: copyright 2026 Gawkyorange5. No additional general reuse license is granted in this initial release. Contact the maintainer for reuse permission. This does not restrict rights already granted by third-party licenses.
- Battlefield 2 and its game content belong to their respective owners. The public release contains no game executable or extracted game audio. Local generated executables/audio are for use with the player's own game installation and should not be uploaded as release assets.

This is an unofficial community project, not affiliated with or endorsed by EA or DICE.


# Credits and third-party notices

## RFX

[BadSanta12345/RFX](https://github.com/BadSanta12345/RFX) is the supplied base: Direct3D9 wrapping, Refractor integration, helpers, native BF2 declarations and stl_8_wrapper.lib / stl_8_wrapper.dll. The maintainer confirmed upstream redistribution permission. No blanket license is assigned to that code here.

## BF2142VR

[justiceofrip/BF2142VR](https://github.com/justiceofrip/BF2142VR), reference revision 99e275767cc78379a43c6362e6f250f81d9a5ead. MIT license: licenses/BF2142VR-LICENSE.txt.

Its BF2 porting guidance, arm IK/contact and firing patterns, stereo replay, UI capture, vehicle seat/head aiming and physical optic work informed/adapted into the BF2-specific implementation. BF2 addresses and native layouts were inspected separately; BF2142 offsets are not used as BF2 offsets.

## Battlefield 1942 VR

[JayBiggsGMG's BFVR](https://github.com/JayBiggsGMG/BFVR-Battlefield-1942-VR-Mod) inspired the project and its desired motion-control experience. This release uses OpenXR and does not redistribute that project's OpenVR runtime.

## Khronos OpenXR

[OpenXR-SDK](https://github.com/KhronosGroup/OpenXR-SDK). Headers were taken at f2448a8797c85814aa892efc1ab8707900fbcc78; loader binary reports version 1.1.62.0, x86. License: licenses/OpenXR-LICENSE.txt and per-file notices under src/third_party/openxr. The loader runtime code is used unchanged. The runtime itself must be installed separately.

## Microsoft and game dependencies

DirectX, Visual C++ runtimes and Windows XAudio2 are external dependencies; they are not redistributed by this package. BF2's legacy MSVCP71/MSVCR71 libraries come from the player's working game installation.

Numerical weapon/seat/mesh-mask profiles describe compatibility with stock assets. Meshes, textures, animations, WAV/PCM payloads and game executables are excluded. The installer/audio-recipes.json file contains numerical extraction instructions and hashes only. Setup generates 116 short cues locally from the player's Objects_client.zip.

OpenXR loader includes JsonCpp (Baptiste Lepilleur and The JsonCpp Authors), with the MIT notice in licenses/JsonCpp-LICENSE.txt. Reference: https://github.com/KhronosGroup/OpenXR-SDK/blob/release-1.1.62/.reuse/dep5 . The public loader has only its local PDB filename metadata sanitized; executable code is unchanged.


MIT License

Copyright (c) 2026 BFVR contributors

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.



                                 Apache License
                           Version 2.0, January 2004
                        http://www.apache.org/licenses/

   TERMS AND CONDITIONS FOR USE, REPRODUCTION, AND DISTRIBUTION

   1. Definitions.

      "License" shall mean the terms and conditions for use, reproduction,
      and distribution as defined by Sections 1 through 9 of this document.

      "Licensor" shall mean the copyright owner or entity authorized by
      the copyright owner that is granting the License.

      "Legal Entity" shall mean the union of the acting entity and all
      other entities that control, are controlled by, or are under common
      control with that entity. For the purposes of this definition,
      "control" means (i) the power, direct or indirect, to cause the
      direction or management of such entity, whether by contract or
      otherwise, or (ii) ownership of fifty percent (50%) or more of the
      outstanding shares, or (iii) beneficial ownership of such entity.

      "You" (or "Your") shall mean an individual or Legal Entity
      exercising permissions granted by this License.

      "Source" form shall mean the preferred form for making modifications,
      including but not limited to software source code, documentation
      source, and configuration files.

      "Object" form shall mean any form resulting from mechanical
      transformation or translation of a Source form, including but
      not limited to compiled object code, generated documentation,
      and conversions to other media types.

      "Work" shall mean the work of authorship, whether in Source or
      Object form, made available under the License, as indicated by a
      copyright notice that is included in or attached to the work
      (an example is provided in the Appendix below).

      "Derivative Works" shall mean any work, whether in Source or Object
      form, that is based on (or derived from) the Work and for which the
      editorial revisions, annotations, elaborations, or other modifications
      represent, as a whole, an original work of authorship. For the purposes
      of this License, Derivative Works shall not include works that remain
      separable from, or merely link (or bind by name) to the interfaces of,
      the Work and Derivative Works thereof.

      "Contribution" shall mean any work of authorship, including
      the original version of the Work and any modifications or additions
      to that Work or Derivative Works thereof, that is intentionally
      submitted to Licensor for inclusion in the Work by the copyright owner
      or by an individual or Legal Entity authorized to submit on behalf of
      the copyright owner. For the purposes of this definition, "submitted"
      means any form of electronic, verbal, or written communication sent
      to the Licensor or its representatives, including but not limited to
      communication on electronic mailing lists, source code control systems,
      and issue tracking systems that are managed by, or on behalf of, the
      Licensor for the purpose of discussing and improving the Work, but
      excluding communication that is conspicuously marked or otherwise
      designated in writing by the copyright owner as "Not a Contribution."

      "Contributor" shall mean Licensor and any individual or Legal Entity
      on behalf of whom a Contribution has been received by Licensor and
      subsequently incorporated within the Work.

   2. Grant of Copyright License. Subject to the terms and conditions of
      this License, each Contributor hereby grants to You a perpetual,
      worldwide, non-exclusive, no-charge, royalty-free, irrevocable
      copyright license to reproduce, prepare Derivative Works of,
      publicly display, publicly perform, sublicense, and distribute the
      Work and such Derivative Works in Source or Object form.

   3. Grant of Patent License. Subject to the terms and conditions of
      this License, each Contributor hereby grants to You a perpetual,
      worldwide, non-exclusive, no-charge, royalty-free, irrevocable
      (except as stated in this section) patent license to make, have made,
      use, offer to sell, sell, import, and otherwise transfer the Work,
      where such license applies only to those patent claims licensable
      by such Contributor that are necessarily infringed by their
      Contribution(s) alone or by combination of their Contribution(s)
      with the Work to which such Contribution(s) was submitted. If You
      institute patent litigation against any entity (including a
      cross-claim or counterclaim in a lawsuit) alleging that the Work
      or a Contribution incorporated within the Work constitutes direct
      or contributory patent infringement, then any patent licenses
      granted to You under this License for that Work shall terminate
      as of the date such litigation is filed.

   4. Redistribution. You may reproduce and distribute copies of the
      Work or Derivative Works thereof in any medium, with or without
      modifications, and in Source or Object form, provided that You
      meet the following conditions:

      (a) You must give any other recipients of the Work or
          Derivative Works a copy of this License; and

      (b) You must cause any modified files to carry prominent notices
          stating that You changed the files; and

      (c) You must retain, in the Source form of any Derivative Works
          that You distribute, all copyright, patent, trademark, and
          attribution notices from the Source form of the Work,
          excluding those notices that do not pertain to any part of
          the Derivative Works; and

      (d) If the Work includes a "NOTICE" text file as part of its
          distribution, then any Derivative Works that You distribute must
          include a readable copy of the attribution notices contained
          within such NOTICE file, excluding those notices that do not
          pertain to any part of the Derivative Works, in at least one
          of the following places: within a NOTICE text file distributed
          as part of the Derivative Works; within the Source form or
          documentation, if provided along with the Derivative Works; or,
          within a display generated by the Derivative Works, if and
          wherever such third-party notices normally appear. The contents
          of the NOTICE file are for informational purposes only and
          do not modify the License. You may add Your own attribution
          notices within Derivative Works that You distribute, alongside
          or as an addendum to the NOTICE text from the Work, provided
          that such additional attribution notices cannot be construed
          as modifying the License.

      You may add Your own copyright statement to Your modifications and
      may provide additional or different license terms and conditions
      for use, reproduction, or distribution of Your modifications, or
      for any such Derivative Works as a whole, provided Your use,
      reproduction, and distribution of the Work otherwise complies with
      the conditions stated in this License.

   5. Submission of Contributions. Unless You explicitly state otherwise,
      any Contribution intentionally submitted for inclusion in the Work
      by You to the Licensor shall be under the terms and conditions of
      this License, without any additional terms or conditions.
      Notwithstanding the above, nothing herein shall supersede or modify
      the terms of any separate license agreement you may have executed
      with Licensor regarding such Contributions.

   6. Trademarks. This License does not grant permission to use the trade
      names, trademarks, service marks, or product names of the Licensor,
      except as required for reasonable and customary use in describing the
      origin of the Work and reproducing the content of the NOTICE file.

   7. Disclaimer of Warranty. Unless required by applicable law or
      agreed to in writing, Licensor provides the Work (and each
      Contributor provides its Contributions) on an "AS IS" BASIS,
      WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or
      implied, including, without limitation, any warranties or conditions
      of TITLE, NON-INFRINGEMENT, MERCHANTABILITY, or FITNESS FOR A
      PARTICULAR PURPOSE. You are solely responsible for determining the
      appropriateness of using or redistributing the Work and assume any
      risks associated with Your exercise of permissions under this License.

   8. Limitation of Liability. In no event and under no legal theory,
      whether in tort (including negligence), contract, or otherwise,
      unless required by applicable law (such as deliberate and grossly
      negligent acts) or agreed to in writing, shall any Contributor be
      liable to You for damages, including any direct, indirect, special,
      incidental, or consequential damages of any character arising as a
      result of this License or out of the use or inability to use the
      Work (including but not limited to damages for loss of goodwill,
      work stoppage, computer failure or malfunction, or any and all
      other commercial damages or losses), even if such Contributor
      has been advised of the possibility of such damages.

   9. Accepting Warranty or Additional Liability. While redistributing
      the Work or Derivative Works thereof, You may choose to offer,
      and charge a fee for, acceptance of support, warranty, indemnity,
      or other liability obligations and/or rights consistent with this
      License. However, in accepting such obligations, You may act only
      on Your own behalf and on Your sole responsibility, not on behalf
      of any other Contributor, and only if You agree to indemnify,
      defend, and hold each Contributor harmless for any liability
      incurred by, or claims asserted against, such Contributor by reason
      of your accepting any such warranty or additional liability.

   END OF TERMS AND CONDITIONS

   APPENDIX: How to apply the Apache License to your work.

      To apply the Apache License to your work, attach the following
      boilerplate notice, with the fields enclosed by brackets "[]"
      replaced with your own identifying information. (Don't include
      the brackets!)  The text should be enclosed in the appropriate
      comment syntax for the file format. We also recommend that a
      file or class name and description of purpose be included on the
      same "printed page" as the copyright notice for easier
      identification within third-party archives.

   Copyright [yyyy] [name of copyright owner]

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.


Copyright (c) 2007-2010 Baptiste Lepilleur and The JsonCpp Authors

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
