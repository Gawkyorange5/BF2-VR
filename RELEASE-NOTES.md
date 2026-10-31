# BF2-VR v1.2.0 — performance and physical gear update

This release uses the accepted **r141 gameplay code**, with the retained
rendering improvements from r135–r139 and the physical-gear changes from
r140–r141. It preserves the existing hold-to-sprint controls.

**Compatible with [DZB2](https://www.moddb.com/mods/dzb2).** Install DZB2 separately,
then use the included DZB2 VR launcher. This update retains its weapon, gadget,
scope and vehicle profiles. Use Medium Lighting; known limits are listed below.

## Performance

- Native Direct3D 9Ex and shared GPU texture transfer avoid the CPU readback/upload
  round trip on the supported path. Adapter matching and fallback handling remain.
- Shared transfer for 2D menus and loading/cinema screens.
- Improved multisample resolve handling for supported AA formats, including the
  native A8 surface path used in the tested 8x AA configuration.
- Reuse of existing HUD and eye images avoids redundant copies and resolves.
- The desktop preview reuses an eye image and combines it with the HUD in one
  composition pass; it does not render another world view.
- Preserves OpenXR-recommended per-eye resolution, the game-resolution option,
  and the player's AA setting. Supported paths also benefit 2x/4x AA; AA Off
  retains the transfer and composition improvements without an MSAA resolve.

These changes reduce specific rendering costs. FPS still depends on GPU, CPU,
OpenXR render size, AA, map, mod and streaming settings. No universal FPS or
percentage gain is promised. Later experimental performance builds are excluded.
GPU timing is off by default and draw instrumentation is compiled out.

## Gameplay and usability

- Primary weapons and pistols briefly fly/tumble when dropped or thrown, then
  rebound to their holsters much sooner. Reaching for the holster can reclaim
  the weapon during its return.
- Hit-confirmation markers remain visible while aiming through sights, even
  when the ordinary gun-following crosshair is hidden.
- Corrected physical C4 release so a quick close-to-body throw is less likely
  to be mistaken for putting the item back in its holster.
- Pistol chest holster by default, with a Chest/Hip choice on the first VR settings
  page. Each placement has separately saved position and rotation.
- In Slots, select a slot, squeeze either controller's grip near its highlighted
  bubble, then move and rotate it physically. Release and choose **Apply** to save.
  This replaces the old directional adjustment controls in the headset menu.

## Installation

- Updates public v1.0.0, v1.1.0 and the accepted private r141 installation.
- Keeps existing VR preferences and custom slot layouts. Select **Keep current
  resolution setting** to retain your rendering choice; graphics/AA profiles
  are not reset. Controller bindings are optional on existing installations.
- Selecting DZB2 creates **Battlefield 2 VR** and **Battlefield 2 DZB2 VR** shortcuts
  separately. Selecting regular BF2 does not overwrite an existing DZB2 shortcut.
- Includes **Launch DZB2 VR.cmd**. DZB2 is installed separately; use Medium Lighting
  for its tested VR configuration. Launch arguments retain `+restart`.
- The BF2 1.50 Alt-Tab hotfix is needed only for the specifically identified older
  renderer. Compatible installations do not need another download.

## What remains included

The v1.1.0 features remain: physical reloads and gadgets, optional grenade cooking,
C4 in one hand with the detonator in the other, the vehicle seat wheel, shoulder
radio, optional helmet death views, DZB2 profiles and optics, and existing controls.
See [previous release features](docs/CHANGES-v1.1.0.md) and [fixes](docs/FIXES.md).

## Verification and limits

The saved r141 gameplay DLL is not recompiled for this release. Distribution
removes local debug-path metadata and game sound data; Setup rebuilds sounds
from the player's own archives. Executable code and runtime data are checked
against r141, alongside installation, upgrade, rollback and startup checks.
The r141 private build was played and accepted; this final public packaging has
not had a separate headset playtest. The maintainer authorized publication.

Existing limits remain: DZB2 High Lighting and some remote-camera freezes,
imperfect passenger poses, and native wreck disappearance. This release does
not claim to fix every map, GPU, runtime or Steam installation. Keep Setup for
**Restore previous**, and see [installation](docs/INSTALL.md) for troubleshooting.
