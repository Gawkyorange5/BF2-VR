# BF2-VR v1.1.0 — changes since public v1.0.0 / r45

Gameplay basis: accepted **r134**. This release gathers the retained changes
from the private development series. Reverted experiments are not advertised
as features. The maintainer tested and accepted the public-package candidate.
The final launcher adds `+restart` to skip intro movies.

## Bug fixes, not just new features

The sections below include retained fixes as well as additions. For the dedicated
fix list, see [Fixes since the first public release](docs/FIXES.md): movement and
smooth turning; HUD/crosshairs/optics; gripping and manual reloads; physical
gadgets; death/respawn and vehicles; installation and compatibility.

## Motion, view and interface

- Head-directed movement, including sprint, with a menu option.
- Roomscale body follow through native collision movement and adjustable lean allowance.
- Snap turning alongside smooth turning; the modes exclude each other.
- Smooth-turn timing and camera improvements that removed the reported regular stepping.
- Head-following HUD behavior and correction of the HUD climbing with gun recoil.
- Vehicle HUD visibility/alpha fixes at headset resolution.
- Gun-following crosshairs and hit feedback in the original game's color.
- Fixed flat-screen infantry crosshair leakage for knife, grenade, defibs and other gadgets.
- Optional gun crosshairs, optional sight proximity hiding, adjustable hiding sensitivity,
  and shotgun exclusion from sight proximity hiding.
- Smooth-turn crosshair duplication and stale optic lens fixes.
- In-headset settings organization, persistent physical-slot adjustment and selected-slot preview.
- Windowed headset-resolution pointer correction to keep menu clicks inside the game window.

## Hands, inventory and gadgets

- Adjustable virtual shoulder stock. Turning it off keeps two-hand support with free aim;
  stock smoothing is available when enabled.
- Physical body equipment or button-based equipment selection.
- Hold-grip, toggle-grip and glued-to-hand carry modes; physical slot swaps remain available
  while glued. Optional empty-handed spawn in physical mode.
- Simultaneous visible holstered gear, configurable slot positions, and persistent offsets.
- Gadget 1 on the left shoulder, gadget 2 on the right shoulder, and optional gadget 3
  behind the head when the kit provides it. Primary right hip, sidearm left hip,
  grenade right chest and knife at the waistband.
- Physical throwing with greater release velocity and tumbling for dropped display items;
  ordinary dropped gear returns to its slot. Supported deployables use native active objects.
- Active physical grenade, medkit, ammo, C4, mine and claymore release paths.
- Optional grenade cooking and corrected pin-sound timing.
- C4 block/detonator handling, off-hand detonator placement and lifecycle fixes.
- Physical knife stabs/swings and throwing-knife hits.
- Two physical defib paddles, deliberate rubbing charge, whole-body revive detection,
  and an optional assisted reach mode for seated play.
- Medkit/ammo/grenade/knife wrist and item alignment corrections.
- Physical shoulder radio: right hand near the left shoulder, hold right trigger,
  make a small directional selection, release to choose. Menu-style pointer feedback;
  infantry and supported vehicle radio remain in stereo.
- Ladder acquisition corrections and crouch toggle/button layout changes.
- Optional arms-out airborne wingsuit glide; parachute still needed for a safe landing.

## Manual reloading

- **189 reload profiles** across stock and DZB2 weapons, including aliases and the XM25.
  Coverage expanded through SMGs, assault rifles, LMGs, snipers, shotguns and pistols.
- Native magazine/handle contacts, detachable magazines, shells, pumps, bolt actions,
  LMG cover/belt interactions and revolver speed loaders where profiled.
- B ejects supported pistol/MP7 magazines in manual mode; G18 also permits a physical pull.
- Manual reload on/off option. B performs normal reload with manual mode off;
  manual mode prevents ordinary automatic empty-magazine reloads. Vehicle reload unchanged.
- Optional magwell alignment guide, stable seating and corrected completion-animation behavior.
- Partial reload charging behavior and reinserting the same magazine corrected.
- Weapon-specific magazine grips, speed-loader grips and drum alignment refinements.
- Action-timed audio reconstructed locally, expanded from 116 to **822 resources**
  including reload, pin and targeting cues. No game audio is bundled in the public download.

## Death views and vehicles

- Optional on-foot helmet death view following the downed body, with looser arms/body,
  terrain-clearance fixes, kit-menu suicide handling and respawn-flow correction.
- Separate optional vehicle helmet death view following a crash's roll from the seat.
  Native wreck disappearance still limits how long the cockpit can remain visible.
- Land-vehicle attitude follows the chassis, with role-specific HUD/view handling.
- Expanded seat classification and flight controls for DZB2 factions, tanks/APCs,
  helicopters, jets, mounted guns and remote gunner systems.
- Head aiming for supported tank/turret/gunner seats and APC wire-missile guidance profiles.
- Right-stick look-around support in transport drivers, vehicle left-grip weapon switching,
  and hold-Y **CHANGE SEATS** wheel requesting F1–F8.
- Upright tilted seat-wheel presentation and fixes for missing panel rendering.
- Audited personal-weapon passenger seats automatically hold the gun in the right hand;
  left grip supports it and right grip remains exit.

## Sights and DZB2 support

- Expanded physical rifle/sniper optics, 1x reflex/red dots and launcher optics.
- SRAW/ERYX native reticle and firing/guidance indicators, controller-directed guidance,
  and separate adjustable launcher magnification.
- DZB2 Stinger/Javelin sight, targeting HUD and native lock-on behavior/audio support.
- Javelin alternate-template motion support, left support handle, and sight alignment fixes.
- DZB2 laptop family motion controls with handle alignment, active deployment, and
  post-throw arm-state fixes. Survival laptop aliases and enter-to-activate logic retained.
- Active throws for profiled DZB2 mines/explosives, smoke/black grenades, beacon,
  sensor and grapple, plus relevant hand-alignment fixes.
- Compatible original-DZB2 launcher included, with local shader compatibility preparation.
- Survival's existing aliases and runtime integrations remain in the shared DLL.

## Rendering, installation and retained optimizations

- Automatic OpenXR-recommended per-eye rendering, game-resolution alternative,
  and custom percentage setting; existing preferences survive installation.
- Compatibility handling for shared/transferred images and discrete-GPU reports.
- Retained HUD-pass/transfer, menu lookup and reload/body filtering improvements.
  The rejected later body-rendering cache and abandoned renderer experiments are excluded.
- Supported 1.5 code identification, explicit pre-hotfix DVD guidance, optional Steam
  original-executable loader, and a separate large-address-aware VR executable.
- Transactional install/update, rollback, protected user edits, and backed-up controls.
- Clean public packaging preserves r134 code and rebuilds sounds from local assets.

## Known limits and future work

- Performance varies by hardware, runtime render size, AA, map and mod. No fixed FPS
  or maximum-settings guarantee is made, and packaging adds no new performance experiment.
- Use Medium Lighting for the tested DZB2 VR setup. High Lighting froze rendering in
  prior testing. AC-130/remote-camera freezes remain a reported issue, not a solved claim.
- Steam and diverse GPUs/headsets still need public testing; a successful installer
  check does not prove gameplay on every system.
- Vehicle passengers are supported through audited templates. Their pose can still
  be imperfect. Wreck models disappearing remains a native game limitation.
- Physical crouch/prone, physical steering wheels/flight controls and independent
  dual-wield equipment are not part of this release. Crouch/prone use buttons.
- Other mods can inherit compatible behavior, but unknown weapons/vehicles may need
  explicit profiles. DZB2 and Survival game content are separate installations.
