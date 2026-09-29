# BF2-VR 1.0.0 — r45 public release

The first public package of the creator-confirmed r45 OpenXR gameplay baseline.

## Start here

**Patch Battlefield 2 to 1.5 and successfully play a flat-screen match first.** Then download **BF2-VR-Setup.exe**, run it, select your game folder and numbered BF2 profile, and install. Connect the headset and start the **Battlefield 2 VR** shortcut.

[Complete installation instructions](https://github.com/Gawkyorange5/BF2-VR/blob/main/docs/INSTALL.md) · [All controller buttons](https://github.com/Gawkyorange5/BF2-VR/blob/main/docs/CONTROLS.md) · [Supported manual reloads](https://github.com/Gawkyorange5/BF2-VR/blob/main/docs/MANUAL-RELOADS.md)

The ZIP contains the same installer plus offline guides and notices. You only need one download. GitHub's automatic source-code archives are for development.

## Included

- OpenXR stereo and motion-controlled weapons, hands and bullet aiming.
- Full-body or arms-only presentation, smoothing and two-hand weapon support.
- Manual magazines, shells, pumps and bolts across 29 supported weapon profiles, with interaction sounds and immediate firing after the required steps.
- Gun-following crosshair, five stock sniper scopes, stereo HUD scale/depth controls.
- Vehicle head aiming, right-stick adjustment, seat anchoring and aircraft roll/tilt.
- Controller menu/spawn pointing, locomotion, smooth turning, parachute and vehicle controls.
- Inside-headset settings and the desktop settings app.
- A single-file installer with backups and removal/restoration.

**Tap both stick clicks to recenter; hold both to open VR settings.** Left menu button tap opens the kit screen; hold opens pause.

SteamVR is not required with a compatible 32-bit OpenXR runtime such as VDXR. No game executable or game media is distributed: Setup builds a separate VR launcher from your own executable and creates reload sound cues from your installed game files. The original BF2.exe is retained.

## Verification and current limits

Isolated install, rollback, profile/shortcut and uninstall checks passed. All 116 reconstructed reload cues match the r45 baseline exactly, and a native x86 probe loaded RFX at process startup and installed all three expected hooks. Gameplay code matches r45. See [verification details](https://github.com/Gawkyorange5/BF2-VR/blob/main/docs/VERIFICATION.md).

A full BF2 match under the new launcher filename has not yet been verified. The established headset setup is Quest 3 / Touch-style controllers / VDXR. Other installations need compatibility feedback. AK support-arm poses still need refinement. Frame rate depends on the system, scene, graphics settings and runtime; no fixed FPS is promised.

## Planned next work

LMG manual reloads; physical weapon/gadget slots; physical crouch/prone; infantry launcher scopes; physical gadget actions; throwing medkits, ammo boxes and grenades; physical defibrillator interactions.

Credits and third-party licensing are included in the repository and offline guide.
