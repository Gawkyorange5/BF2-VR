# BF2-VR v1.2.1 — native aiming, scope alignment and terrain fixes

This release uses the **tested and accepted r154 runtime**. It includes all
accepted fixes since v1.2.0 and keeps the previous performance and gameplay
improvements. The gameplay DLL has not been recompiled for public packaging.

## Aiming and sights

- **Pulling a supported weapon up to your face and lining up its iron sights,
  scope or red-dot/holographic sight now activates Battlefield 2's native aiming behavior.** You get the
  weapon's normal aimed accuracy and slower movement while aiming, just like
  aiming in the original game. Previously, looking through a VR scope could
  leave the weapon using its hip-fire accuracy.
- Includes supported iron-sight pistols, SMGs, rifles, LMGs and shotguns in
  stock BF2 and DZB2. Iron-sight aiming keeps scenery detail stable instead of
  applying the flat-screen camera zoom. Existing scoped-weapon magnification,
  red-dot/holographic sights and launcher optics retain their previous behavior.
- The weapon's native spread, recoil and projectile behavior remain intact;
  this activates its existing aimed state rather than giving every gun perfect
  accuracy. Reloading and sprinting still use the native cancellation behavior.
- Scope images and reticles stay aligned with the visible weapon while moving,
  including the M24 and other supported stock/DZB2 optics.
- Scopes become visible farther from your face, so you do not need to pull them
  as close to look through them.
- Removed the extra floating flat-screen red-dot/holographic HUD reticle that
  could appear when raising the weapon. The correct sight stays on the gun.
- Stronger, saturated red dots are easier to pick out against scenery. The
  final version keeps the original red with stronger opacity, without the
  washed-out pale center from the earlier private test.
- Existing sniper reticles, launcher targeting artwork, guidance, lock feedback
  and sounds are retained. Aim points and scope magnification are unchanged.

## Terrain fix

- Corrected a terrain-shader depth mismatch associated with black ground
  flickering on Medium/High Terrain settings. The affected RTX 4090 tester
  confirmed that the flickering was gone and the M24 scope stayed aligned while
  moving. Low Terrain is no longer the required workaround for that reported
  issue. Unknown custom terrain shaders are left unchanged.

## Installation and compatibility

Download **BF2-VR-Setup.exe** below, or the ZIP containing Setup and the guides.
Close the game and VR Settings, run Setup, and select your existing game folder.
**Keep current resolution setting** preserves your rendering choice; existing
VR settings and physical-slot layouts are retained. Leave controller bindings
unchecked when you want to keep your current bindings.

- Supports updates from public v1.0.0, v1.1.0, v1.2.0 and the earlier v1.2.1 files, plus accepted r141 and
  the r147–r154 private previews. Setup keeps exact previous-file backups.
- Separate regular BF2 and DZB2 shortcuts remain, with intro skipping via
  `+restart`. The package includes **Launch DZB2 VR.cmd**.
- **Compatible with [DZB2](https://www.moddb.com/mods/dzb2).** Install DZB2
  separately. **DZB2 itself requests Medium Lighting** in its graphics setup;
  follow that recommendation. This is Lighting, not the Terrain setting above.
- The BF2 1.50 Alt-Tab hotfix remains conditional: only use it if Setup identifies
  the older incompatible renderer. It is not required for everyone.

## Previous improvements remain

All accepted v1.2.0 rendering optimizations and gameplay features are included:
GPU texture transfer and AA resolve improvements, weapon rebound, hit markers
while aiming, physical slot positioning, chest/hip pistol holsters, manual
reloads, physical gadgets, grenade cooking, C4/detonator controls, shoulder radio,
vehicle seat wheel, death cameras and DZB2 support. No new experimental
performance path or sprint-toggle behavior was added.

See [v1.2.0 changes](https://github.com/Gawkyorange5/BF2-VR/blob/main/docs/CHANGES-v1.2.0.md),
[earlier features](https://github.com/Gawkyorange5/BF2-VR/blob/main/docs/CHANGES-v1.1.0.md)
and [fixes](https://github.com/Gawkyorange5/BF2-VR/blob/main/docs/FIXES.md).

## Verification and existing limits

Public packaging preserves r154 executable code and runtime data. Only game
sound resources and local debug-path metadata are sanitized for distribution;
Setup rebuilds sounds from the player's installed game files. Installation,
upgrade, exact restoration, settings, shortcuts, audio reconstruction and x86
startup checks were run on the final package. The private iron-sight update was
tested in-game; scoped paths were preserved and checked automatically; the final public packaging has not had a separate headset test.

Existing DZB2 High Lighting/remote-camera freezes, imperfect passenger poses
and native wreck disappearance are not claimed fixed. Performance still varies
with hardware, map, AA and headset resolution. Keep Setup and its backup folders
for **Restore previous**. See the [installation guide](https://github.com/Gawkyorange5/BF2-VR/blob/main/docs/INSTALL.md).
