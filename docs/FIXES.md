# Fixes since public v1.0.0 / r45

This is a user-facing summary of fixes retained in the accepted r134 build.
It covers reported problems and their retained corrections, alongside the
features described in RELEASE-NOTES.md. It is not a claim that every mod,
headset, graphics driver, or vehicle has been exhaustively tested.

## Movement, turning and menus

- Fixed sprint stopping or ignoring head-directed movement.
- Made smooth and snap turning mutually exclusive in the settings menu.
- Corrected duplicated gun crosshairs during smooth turning.
- Corrected the regular stepping/skipping seen during steady smooth turns.
- Improved ladder acquisition when the player could not get onto a ladder.
- Kept the menu pointer's clicks within the game client area at headset resolution,
  addressing clicks outside the window that stole focus.
- Reorganized settings, including separate infantry and vehicle death-view options.
- Saved physical-slot offsets between sessions.
- Fixed radio selection requiring excessive arm travel.
- Kept the radio overlay and menu pointer in stereo in supported infantry/vehicle contexts.
- Returned radio activation to the left shoulder to avoid accidental activation while firing.
- Corrected outstretched/frozen arm poses when using the radio.

## HUD, crosshairs and sights

- Stopped the HUD climbing upward with successive shots.
- Restored head-following HUD behavior and corrected vehicle HUD positioning.
- Corrected faint/transparent vehicle HUDs in the headset at headset resolution.
- Suppressed residual flat-screen crosshairs on knives, grenades, defibs and other gadgets.
- Moved hit feedback to the gun-following crosshair and restored its native game color.
- Hid crosshairs when aiming through sights, with adjustable sensitivity and an off option.
- Excluded shotguns from proximity-based crosshair hiding.
- Corrected ghost/stale optic lenses while moving weapons such as the M4 and compact AK.
- Restored missing red dots in profiled DZB2 sights, including the Vector and other guns.
- Expanded gun-following crosshair support to the profiled DZB2 weapons.
- Added the missing native black reticles and firing/guidance indicators in launcher optics.
- Corrected launcher magnification, sight alignment and Stinger HUD visibility.

## Gripping, inventory and reloading

- Corrected virtual-stock-off behavior while retaining two-hand support.
- Fixed empty hands and failed weapon switching in button-based equipment mode.
- Preserved physical slot switching while glued-to-hand carry is enabled.
- Made shoulder gadget pickup regions more forgiving.
- Corrected the physical-slot selection/equip path and simultaneous holstered item display.
- Disabled B's automatic infantry reload fallback in manual mode and prevented ordinary
  automatic empty-magazine reloads there; vehicle reload behavior remains native.
- Corrected partial-magazine reload completion/charging behavior.
- Fixed reinserting the same magazine leaving the weapon unable to fire, rack or use support grip.
- Corrected magwell seating and insertion-guide pop-out/completion animation behavior.
- Added missing profiles/aliases, including the compact MEC AK and alternate DZB2 templates.
- Corrected rifle, pistol, revolver speed-loader and shotgun drum magazine hand poses.
- Added MP7 B-eject and G18 physical magazine removal while retaining B-eject.
- Corrected right-stick-click equipment switching/flicker in physical gear mode;
  tap performs the intended fire-mode action and hold requests kit pickup.
- Stopped left-trigger interaction from causing a flick/jump while supporting a weapon.
- Fixed the silenced pistol's suppressor remaining suspended after the weapon was thrown.
- Restored physical knife stabs/swings while retaining throwing-knife behavior.

## Physical gadgets and launchers

- Corrected medkit, ammo, knife, grenade, smoke and other profiled gadget wrist/model alignment.
- Replaced visual-only throws with native active deployment for supported grenades, medkits,
  ammo, C4, mines, claymores, beacon, sensor, grapple and DZB2 variants.
- Corrected weak throw velocity and reduced grenade release delay.
- Corrected C4 disappearing on physical release and retained its active native projectile.
- Kept the C4 detonator available after throwing, with corrected off-hand placement and cleanup.
- Corrected grenade pin-sound timing while preserving optional cooking.
- Refined defib paddle positions, rubbing/charge timing and revive detection; assisted reach
  supports seated use. DZB2 defib variants use the profiled VR interaction.
- Corrected laptop handle alignment, stuck arms during holding, and stuck holding poses after
  release. The original DZB2 laptop family and Survival aliases retain active deployment.
- Corrected the alternate Javelin's stuck arms, support-handle pickup and sight presentation.
- Restored controller-directed guidance for the profiled SRAW/ERYX/DZB2 guided launchers.
- Preserved native targeting, lock HUD and sound behavior for supported Stinger/Javelin profiles.

## Death views and vehicles

- Improved helmet death arms/body movement and camera clearance near slopes/impacts.
- Added helmet death handling for kit-menu suicide and corrected its respawn-flow issue.
- Kept vehicle death view attached to the seat/crash roll while a valid wreck remains.
- Removed inappropriate horizon locking from supported land-vehicle views.
- Corrected tank/APC head aiming and expanded seat roles/controls for DZB2 factions.
- Corrected unsupported helicopter controls on additional factions/templates.
- Restored right-stick look-around in supported transport drivers.
- Corrected missing/backwards seat-wheel rendering and its viewing angle.
- Automatically held personal weapons in audited Humvee/open passenger seats, avoiding the
  conflict where right grip tried to pick up the floating gun but instead exited the vehicle.
- Retained seat-specific support/reload controls and APC guided-missile aiming profiles.

## Installation and packaging

- Added both headset-recommended and game-resolution installation choices; Keep current
  preserves existing preferences.
- Replaced fragile whole-file version checks with supported code identification where appropriate.
- Added explicit guidance for the identified earlier 1.5 renderer. The Alt-Tab hotfix is
  **only needed on affected installations**; compatible copies do not need that download.
- Added an optional Steam original-executable loader for the reported renamed-EXE load error.
  Its loader startup is checked; actual affected Steam gameplay still needs confirmation.
- Made the locally created VR executable large-address-aware; the original BF2.exe stays intact.
- Added tested install/update rollback, backups, protected later user edits and newer-build rejection.
- Preserved the tested runtime code and all locally reconstructed sound resources in public packaging.
- Included the DZB2 compatibility launcher, later-mod discovery and persistent local settings.
- Added `+restart` to generated game launch arguments to skip the intro movies.

## Issues not claimed solved

DZB2 High Lighting and some AC-130/remote-camera sessions have produced frozen
rendering. Use Medium Lighting for the tested DZB2 configuration. The native
game still removes wreck models. Passenger poses can be imperfect. Performance,
out-of-memory behavior, and GPU-specific rendering problems are not universally
solved; no fixed frame rate or maximum-settings guarantee is made. Reverted
renderer/body-cache experiments are not counted as fixes in this release.
