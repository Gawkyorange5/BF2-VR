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
