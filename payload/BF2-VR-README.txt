Battlefield 2 VR v1.1.0 — installation and controls

# Installation and restoration — v1.1.0

## 1. Make the normal game work first

Install your own copy of Battlefield 2 and apply the patches required by your
edition to reach **1.5**.
Start the normal game, create/select your local profile, load a map, and spawn.
Reaching only the main menu does not establish that the game installation works.
Close the game before installing VR.

**The Alt-Tab hotfix is not a download required for everyone.** If your renderer
is already compatible, install VR normally. The troubleshooting section below
has the hotfix link for installations that need it.

Setup checks executable code rather than requiring an exact whole-file match;
supported large-address-aware copies are accepted. For an unrecognized build,
use **Compatibility report** and report your edition and whether flat-screen
gameplay works. Setup does not download game executables or the hotfix.

## 2. Prepare PC VR

- Use a Windows 10/11 PC and a connected PC VR headset. This is a 32-bit game;
  its OpenXR runtime must support 32-bit applications. SteamVR is not mandatory
  when your headset connection provides a suitable OpenXR runtime directly.
- Connect the headset before launching the game. Select the runtime in your
  headset connection software. For Virtual Desktop, use the same connection
  and OpenXR runtime that you have configured for PC VR.
- Install Microsoft's [Visual C++ v14 **x86** redistributable](https://learn.microsoft.com/en-us/cpp/windows/latest-supported-vc-redist)
  even on 64-bit Windows if it is missing.
- Install Microsoft's [DirectX End-User Runtimes (June 2010)](https://www.microsoft.com/en-us/download/details.aspx?id=8109)
  if the legacy D3DX libraries are missing. Extract that download to a temporary
  folder and run its DXSETUP program. Having modern DirectX alone does not
  supply all legacy optional libraries.
- If BF2's MSVCP71/MSVCR71 files are missing, repair your working game installation.
  Setup never obtains DLLs from third-party DLL download sites.

## 3. Run BF2-VR Setup

1. Extract the release ZIP, or download its single **BF2-VR-Setup.exe**.
2. Close Battlefield 2, the VR settings application, and any DZB2 VR session.
3. Run Setup. Browse to the installed game folder containing **BF2.exe** and
   **RendDX9.dll**. Choose the game folder, not `mods`, a DVD, or a shortcut.
4. Select your numbered Documents profile. For a first install, leave
   **Apply recommended controller bindings** enabled. This backs up and replaces
   only that profile's `Controls.con`; it does not copy someone else's login,
   Video.con, progress or player name. Existing working controls can be retained
   by clearing this checkbox. Other custom keyboard bindings will be replaced
   if you enable it.
5. Choose an already installed game/mod for the shortcut. The usual choice
   is **bf2**. Choosing DZB2 uses its compatibility launcher. This list does
   not install any additional game content.
6. Choose a render mode:
   - **Keep current**: retain your existing mode. New installs default to
     automatic headset resolution.
   - **Headset recommended**: use the connected OpenXR runtime's requested
     per-eye size. This is not necessarily the headset panel's physical pixel
     count; the runtime accounts for its rendering configuration.
   - **Game resolution**: use BF2's configured render size. A smaller size can
     reduce GPU and transfer cost. This may look softer in the headset.
7. Leave the optional Steam checkbox off for a normal non-Steam installation.
   Steam owners should read the Steam section below.
8. Confirm that flat-screen gameplay worked and click **Install / Update**.
   Wait for the success message. If the folder is protected and Windows denies
   access, close Setup and run it as administrator under the same account.

Setup creates a separate, large-address-aware **BF2-VR.exe** from your own
executable. It installs the approved runtime, settings program, OpenXR loader,
controller bindings if selected, and the DZB2 launcher. It builds the reload
and targeting sounds locally. A base-game-only install does not need DZB2;
optional DZB2 sounds become available when that mod is installed and launched.

## 4. Play

Use the new **Battlefield 2 VR** shortcut. Alternatively run `BF2-VR.exe` from
the game folder, or use `BF2-VR-Settings.exe` to select settings and launch.
Choose your normal local profile and a map, then spawn.

Generated shortcuts append `+restart` to skip BF2's intro movies. The DZB2
launcher includes the same argument when it starts the game.

Tap both stick clicks to recenter. Hold both for about 0.6 seconds to open VR
settings. Point with the right controller and press its trigger to select.
**Apply** saves settings; closing recenters. Tap the left menu button for the
kit/spawn screen, or hold it for pause. See `CONTROLS.md` for the complete layout.

Automatic headset rendering can use a window with a different shape from your
monitor. That desktop preview is not the headset's aspect ratio. Menu-pointer
clicks are contained in the game client area by the included runtime fix.

## 5. DZB2 — now or installed later

1. Install the original DZB2 files into this game's `mods/DZB2` folder.
   The result must contain `mods/DZB2/Mod.desc`. DZB2 is a separate download.
2. Play a map using its normal flat-screen launcher first. Use **Medium Lighting**
   for the tested DZB2 VR configuration. High Lighting has caused frozen images
   while game audio and controls continued on the tested setup.
3. Close BF2. Connect your headset and run **Launch DZB2 VR.cmd** from the game
   folder or this release folder. No editing of command lines is required.
4. The first launch prepares a local compatibility runtime. Allow it to finish.
   Later launches reuse that folder and refresh the runtime or sounds when needed.

The launcher installs no DZB2 assets. It shares your installed `mods` folder
through a directory junction, copies the required game support files locally,
and uses the same shader compatibility helper as the working private launcher.
Your original `d3dx9_25.dll` and game renderer remain intact. An unknown shader
library or a local DXVK `d3d9.dll` stops this native-D3D9 launcher with an explanation.

The prepared files are under:
`%LOCALAPPDATA%/BF2-VR/DZB2/<installation-id>/Game`.
The launcher prints the exact path. This folder contains its **bf2_clean_vr.log**,
**bf2_shader_compat.log**, and **vr_stereo.ini**.

On its first run, the launcher copies your main game's VR preferences. Changes
made inside DZB2 then persist in its compatibility folder. If you later change
the main game's INI, the launcher copies that changed INI on the next launch
and backs up the previous DZB2 INI alongside it. BF2's Documents graphics profile
is shared; the launcher does not reset your graphics settings.

## Steam edition

The optional loader uses **the original BF2.exe through Steam**, avoiding the
renamed-executable launch route that can cause application load error
`5:0000065434`. Enable **Steam edition** in Setup, then start BF2 through Steam
Play or the Steam-aware shortcut. Steam must already be installed and able to
run your game. The loader does not bypass Steam authentication.

This route's loader checks passed locally, but an actual affected Steam
installation has not been verified for this candidate. The DZB2 compatibility
copy is also not confirmed with Steam. Report remaining errors with your game
edition and a compatibility report. Setup refuses to overwrite another mod's
`dinput8.dll`. While this optional loader is installed, the original game's
Steam launch also loads VR; use restoration to return to your prior setup.

## Updating, preserving settings, and returning to the prior build

Run this Setup over public v1.0.0 or the accepted private r134. Keep current
resolution and leave controller bindings unchecked to retain your existing
preferences. The installer refuses an unknown/newer runtime so it cannot
silently overwrite a newer custom DLL build.

The `.bf2-vr` folder contains the current install journal and backups. During an
update, the previous journal is kept in a `.bf2-vr-history-...` folder. Keep both.
Private-patch backup folders are left alone.

To undo this installation, close the game and run the **same Setup**, select
the same game folder, then choose **Restore previous**. This restores exactly
what this install replaced. If you upgraded, that means the previous VR build
and its journal. If this was a clean install, it removes this install's files.
Later user edits are preserved and reported instead of being overwritten.
Restoration may need attention for those reported conflicts; the backups remain.

The DZB2 cache is kept so logs and settings are not lost. Do not recursively
delete a `mods` junction with a utility that follows links into the original
game. The cache is not an independent game installation and needs its source game.

## Troubleshooting and reports

### Only if Setup reports the earlier BF2 1.5 renderer

The reported DVD installation had a valid 1.5 executable but the earlier
August 2009 renderer. If Setup identifies that specific renderer on your copy,
use the [BF2 1.50 Alt-Tab hotfix download page](https://community.pcgamingwiki.com/files/file/1031-battlefield-2-patch-150-alttab-hotfix/).
Close the game, back up RendDX9.dll, follow the hotfix's instructions, then
confirm flat-screen gameplay and retry Setup. Do not rename RendDX9x2.dll to
RendDX9.dll. An already compatible DVD, Steam or other installation does not
need to download or reinstall this hotfix. Setup checks the installed code;
the edition name alone does not determine whether a hotfix is needed.

### Other issues

- A frozen image with continuing sound is still a rendering failure. For DZB2,
  return Lighting to Medium first. AC-130/remote-gunner freezes have been reported;
  this packaging release does not claim to solve all of them.
- Native headset resolution plus 8x AA can be expensive. Choose a render mode
  and AA level that your PC can sustain. This release makes no universal FPS claim.
- Grain/shimmer visible only in a streamed headset can also come from video
  compression. Compare the desktop and headset before changing the VR renderer.
- For a startup problem, save Setup's compatibility report. For a gameplay issue,
  close the game and send the log from the runtime folder actually used, your
  mod/map/vehicle or weapon, graphics settings, headset/runtime and GPU.
- If another project installed a newer RFX.dll, retain it and merge deliberately.
  Do not rename files just to defeat the compatibility check.


# BF2-VR v1.1.0 controller layout

For Touch-style controllers. Install through Setup; existing settings can be retained.

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
or another both-click hold closes and recenters. See SETTINGS.md.

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
| Left grip | Change vehicle weapon/fire mode |
| Hold Y | Open the tilted seat wheel at the left hand; move left hand to Seat 1–8 and release Y to send F1–F8 |
| Hold X | Vehicle utility shortcuts below |

A very short right click deploys flares on release. Holding it begins sending
the flare key after an 80 ms grace period, which allows both-stick shortcuts
to be recognized without spending a countermeasure when the clicks are
slightly staggered. Press both together to use VR settings/recenter.

Head aiming retains the existing seat position lock and right-stick aiming
center adjustment. Center the right stick after entering/changing seats.
The inputs request BF2's native actions; they do not add abilities to a vehicle.

### Vehicle seat wheel (r133)

Hold Y with X released. The circular **CHANGE SEATS** panel opens at your
left controller's tracked grip position. Text is upright and the panel is
tilted 50 degrees above horizontal toward you. It stays at that opening
pose while you move your left hand toward a numbered seat. The
highlight and short vibration identify the selection. Release Y to request
that seat, or move back to the center and release to cancel.

All eight choices are native F1–F8 requests. BF2 decides which seats exist,
are occupied or are otherwise unavailable. The wheel does not eject another
player or bypass the vehicle's rules. It stays inside the stereo VR view.
Vehicle controls are released while choosing a seat; release held controls
and center the right stick afterward to rearm them.

The old X + right-stick up/down seat shortcut has been removed. Hold X
before pressing Y to keep the squad-screen shortcut. Y remains toggle crouch
on foot. Tracking loss, death, seat changes, recenter/settings and game menus
cancel the wheel without selecting a seat.

The panel stays at its opening position while your hand moves to select a seat.

### Open passenger seats with personal weapons (r134)

Audited open passenger seats, including the stock Humvee rear seats, keep
the native equipped handheld weapon glued to the tracked right hand
automatically. No right-grip press is needed to pick it up, including when
entering with empty hands. The left grip
supports that weapon, and the left trigger performs manual reload
interactions. B ejects supported magazines in manual mode, or reloads when
manual reload is off. Gun-following crosshairs and supported sights work
there too. Right grip remains use/exit, and hold Y opens the seat wheel.
On-foot holster state and hold/toggle/glued preferences resume when leaving
the vehicle. Passenger auto-hold does not change those saved settings.

Drivers and mounted gunners retain their existing vehicle controls,
including left-grip vehicle weapon switching and left-trigger alternate fire.

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
| B | Manual mode: eject supported pistol/MP7 magazine. Manual mode off: native reload |
| X | Prone |
| Y | Toggle crouch |
| Tap right joystick click alone | Physical gear: change the held weapon's fire mode; empty-handed: no action. Button inventory: next item |
| Hold right joystick click alone for 0.4 seconds | Pick up / exchange nearby kit |
| Hold right joystick click with another input | Infantry utility shortcuts below |

Manual charging is available after a partial reload as well as an empty
reload. Fresh magazines and individual shells move into the tracked hand
when grabbed. B is not an automatic reload fallback in manual mode.
Left trigger does not send infantry ADS.

## Utility shortcuts

Hold **X in vehicles**, or **right joystick click on foot**, then use:

| Additional input | Action |
| --- | --- |
| A (vehicles only) | Pick up / exchange kit; on foot use right-stick hold alone |
| B | Map size |
| Right joystick click in vehicles; X on foot | Scoreboard |
| Y | Squad screen |
| Right grip | Kit / spawn screen |
| Right trigger | Select slot 4; release utility button before firing |
| Left grip | Push to talk |
| Left trigger | Radio / commo rose |

Using a utility shortcut consumes the utility button, so releasing it does
not also change fire mode. For radio, use the right stick to move the cursor
and the right trigger to select, or the tracked pointer when a menu appears.

## Profile and transitions

Setup can install the included Controls.con for your selected numbered BF2
Documents profile, including land, air and helicopter bindings. It backs up
the previous file. The mapping includes vehicle boost on LeftShift, flares
on X, alternate fire on right mouse, Q radio and keys 1–9 for equipment.

Release held buttons and triggers after spawning, changing seats or closing
a menu, then press them again. Center the right stick to rearm its axes.
Smooth-turn speed now applies live through VR settings or BF2-VR-Settings.exe.

## Physical equipment and manual reload gestures

In physical gear mode, use the **right grip** near a body slot to draw its item.
Hold grip to carry, or use toggle/glued carry according to Gear settings.
The ordinary right-grip use/enter action remains available in the appropriate
context. In supported vehicle passenger seats, the gun is held automatically.

| Slot | Default location |
| --- | --- |
| Gadget 1 | Left shoulder |
| Gadget 2 | Right shoulder |
| Gadget 3, when the kit has it | Behind the head |
| Primary weapon | Right hip |
| Sidearm | Left hip |
| Grenade | Right chest |
| Knife | Middle waistband |

In **Slots**, select an item and adjust left/right, up/down and forward/back.
The highlighted preview identifies the slot. Apply saves positions to the
runtime's vr_stereo.ini. Adjust them once for your seated/standing reach.

Use the left trigger at a supported magazine to remove it; grab a replacement
from the ammunition pickup area and insert it in the magwell. The insertion
guide can align the magazine near the opening. Release to seat it, then work
the charging handle/slide as required by that weapon. Supported pistols and
the MP7 have B eject; the G18's extended magazine also supports a physical pull.
Shotguns, bolt-action rifles, revolvers and belt-fed LMGs use their profiled
shell, bolt, cylinder/speed-loader or cover/belt actions. Disable Manual reloading
to use automatic B reload. Vehicle B reload always remains native.

Letting go while throwing a supported grenade/deployable releases its active
native object. Ordinary thrown weapons tumble and return to their holster
after a delay. Cooking is optional: a supported frag is armed on grab, so throw
before the fuse expires. The left C4 detonator stays available according to the
equipped C4 state. This release does not support arbitrary independent dual wield.

For defibs, rub the paddles together until charged, separate them and bring
both to the downed teammate. Assisted revive increases reach while retaining
target/obstruction checks. Physical knife swings/stabs and throws are supported.

## Physical radio

Raise the **right hand** to the **left shoulder**, hold the right trigger, move
the hand slightly toward the desired commo-rose option and release the trigger
to send it. The pointer/highlight identifies the selection. Gear settings
include radio sensitivity; higher values require less movement. It stays in
the stereo scene on foot and in supported vehicles. Lower/release the hand
before returning to weapon firing.

## Flight, guidance and falling

As a pilot, left stick is throttle/yaw and right stick is pitch/roll. In a
supported mounted gun or tank, head direction aims with the right-stick center
adjustment. Left grip switches native vehicle weapons. Flares are right-stick
click and boost is left-stick click where the vehicle implements them.

Supported wire-guided handheld missiles follow weapon direction; supported
vehicle guidance follows the head-aim route. Lock-on launchers retain native
valid-target and acquisition rules; a VR sight does not grant a lock on an
ineligible target. Keep the target visible and allow acquisition.

If the optional wingsuit is enabled, spread both arms while airborne to glide;
bring them in to stop. Use A to request the parachute before the ground. Normal
fall damage still applies; gliding alone does not give a safe landing.


# Saved VR settings

Hold both stick clicks for about 0.6 seconds. Point with the right controller,
press its trigger, then **Apply** to save. **Close + recenter** or B closes the
menu and recenters. Settings are also available through BF2-VR-Settings.exe.
The runtime stores preferences in **vr_stereo.ini** beside its executable.
The DZB2 compatibility copy has its own persistent INI; see INSTALL.md.

Resolution and custom scale take effect after a normal game restart. Keep
current in Setup preserves them. Headset mode uses OpenXR's recommended eye
size; Game mode uses BF2's rendering size. Neither promises a fixed frame rate.

The catalog below is generated from the r134 settings definitions. Existing
saved values take precedence. Numeric ranges are provided where helpful; slot
offsets are in metres. Graphics switches are existing compatibility controls,
not a recommendation to experiment before testing the accepted baseline.

## Turning

| Setting | Behavior | Range |
| --- | --- | --- |
| Smooth turning | Enabling this switches snap turning off. Both off: physical turns only. | 0–1 |
| VR turn speed | 240 = default; 480 = 2x input rate. BF2 sensitivity also affects turning. | 10–1500 |
| Snap turning | Enabling this switches smooth turning off. Center stick between turns. | 0–1 |
| Snap angle (degrees) | Exact native turn angle. Applies to infantry, not vehicle controls. | 15–90 |

## HUD

| Setting | Behavior | Range |
| --- | --- | --- |
| Controller crosshair | On: gun-following aim marker. Off: no weapon crosshair; fixed infantry crosshair stays hidden. | 0–1 |
| Hide crosshair at sights | Hide gun marker at sights; shotguns keep their marker. Active scopes still hide it. | 0–1 |
| Hide sensitivity (%) | Lower = hide later/closer. Higher = earlier. 100 = r84. Active scopes still hide it. | 25–150 |
| Crosshair size (degrees) | Changes the aiming marker's visible size. | .15–2 |
| Crosshair distance (metres) | Sets the aiming marker's display distance. | 3–100 |
| HUD scale | Smaller values bring the HUD edges into view. Default 0.85. | .35–1.4 |
| HUD depth (metres) | Changes distance while keeping apparent size steady. | 2–30 |

## Body

| Setting | Behavior | Range |
| --- | --- | --- |
| Show full body | Off: tracked first-person arms only. On: torso and legs too. | 0–1 |
| Connected body arms | Full body on: connects arms to the torso. Off: first-person arms. | 0–1 |
| Detailed hands | Uses high-detail hands with connected body arms. | 0–1 |
| Arms hide overlapping weapon | Recommended on: avoids seeing the weapon through your arms. | 0–1 |
| Body clearance (metres) | Moves the visual body back from your eyes. 0 restores the previous position. | 0–.15 |
| Body follows head turns | Full body eases into larger physical turns. Small glances keep it still. | 0–1 |
| On-foot helmet death view | On: follow your downed body. Off: original death camera. Vehicles have a separate switch. | 0–1 |

## Vehicles

| Setting | Behavior | Range |
| --- | --- | --- |
| Vehicle HUD opacity boost | 1 = corrected VR transparency; increase for stronger instruments. Headset only. | 1–4 |
| Vehicle head aiming | Aim supported mounted guns with your head and right stick. | 0–1 |
| Lock position in gunner seat | Keeps your viewpoint at the gunner seat while looking around. | 0–1 |
| Gunner stick speed (degrees/sec) | Moves the aiming center while head aiming stays active. | 10–180 |
| Aircraft flight stick speed | 300 = default pitch/roll input rate. Separate from on-foot turning. | 20–1500 |
| Vehicle crash death view | On: stay in the crash seat and follow its roll. Off: original vehicle death camera. | 0–1 |

## Scopes

| Setting | Behavior | Range |
| --- | --- | --- |
| Physical scopes | See through supported sniper, rifle and launcher optics; reflex sights stay 1x. | 0–1 |
| Scope magnification | Sniper and rifle scope zoom. SRAW / ERYX use Launcher magnification; reflex sights stay 1x. | 1–8 |
| Launcher magnification | SRAW / ERYX zoom. Lower values show more of the scene. Default 1.5x. | 1–4 |

## Move

| Setting | Behavior | Range |
| --- | --- | --- |
| Head-directed movement | Walking and sprint follow your head. Off: body-directed stick. | 0–1 |
| Roomscale body follow | Physical steps move the soldier through normal collision movement. | 0–1 |
| Roomscale lean allowance (metres) | Lean this far before your body follows. Preview: center left stick to step. | .08–.5 |
| Wingsuit (experimental) | Airborne: arms out to glide; arms in to stop. Press A for parachute BEFORE landing; normal fall damage applies. | 0–1 |

## Weapons

| Setting | Behavior | Range |
| --- | --- | --- |
| Virtual shoulder stock | Off: free two-hand aim. On: shoulder assistance. Both keep the foregrip. | 0–1 |
| Aim smoothing | With stock on, softens tiny sight tremors. Stock off always uses raw aim. | 0–1 |
| Manual reloading | On: gestures, no B reload. Off: B reload. Vehicles unchanged. | 0–1 |
| Magazine insertion guide | Aligns nearby magazines with their magwell. Push in, then release to seat. | 0–1 |

## Graphics

| Setting | Behavior | Range |
| --- | --- | --- |
| GPU compatibility mode | 0 Auto; 1 Conservative; 2 Original fast path. Auto protects discrete GPUs. | 0–2 |
| Automatic transfer optimization | Compare transfer methods during play. Off uses the previous method. Keeps image quality. | 0–1 |
| VR resolution mode (restart) | Automatic: headset size. Custom: percentage below. Game: BF2 size. | 0–2 |
| Custom render scale % (restart) | Custom mode only. 100 = headset size; 75 = 56% pixels. Restart normally. | 25–150 |
| Launch through Steam | Settings app Launch VR uses Steam + original BF2.exe. Requires Steam loader. | 0–1 |

## Gear

| Setting | Behavior | Range |
| --- | --- | --- |
| Physical defibrillator | Rub paddles together for 1.25 seconds, then separate and touch a downed teammate. | 0–1 |
| Assisted revive reach | Off: both paddles touch the body. On: both can be up to 1.25 m away, with a clear path. | 0–1 |
| Equipment selection | Buttons: tap right stick to equip. Physical: reach body slots. | 0–1 |
| Glued-to-hand items | Keep held; at another slot, squeeze grip to swap. | 0–1 |
| Grip mode when not glued | Physical gear only, glued off. Hold to carry or toggle with a squeeze. | 0–1 |
| Spawn empty-handed | Physical gear only. Buttons always start with the selected item drawn. | 0–1 |
| Hip distance below head (metres) | Adjust hip and waistband slot reach for seated or standing play. | .45–1.1 |
| Inventory grab radius (metres) | Light pulse: near slot. Strong pulse: weapon draw accepted. | .08–.23 |
| Cook frag grenades on grab | Off: fuse starts on throw. On: grabbing arms a frag; release before its 2.8-second fuse expires. | 0–1 |
| Physical shoulder radio | Right hand at left chest/shoulder: hold trigger, move hand to choose, release to send. | 0–1 |
| Radio selection sensitivity | Higher needs less hand movement. 3x default: about 4 cm to the edge; 1x: original reach. | 1–5 |

## Slots

| Setting | Behavior | Range |
| --- | --- | --- |
| Gadget 1 (left shoulder): left / right | Offset in metres; negative = left, positive = right. Preview in headset Slots tab. | -.65–.65 |
| Gadget 1 (left shoulder): down / up | Offset in metres; negative = down, positive = up. Preview in headset Slots tab. | -.65–.65 |
| Gadget 1 (left shoulder): back / forward | Offset in metres; negative = back, positive = forward. Preview in headset Slots tab. | -.65–.65 |
| Gadget 2 (right shoulder): left / right | Offset in metres; negative = left, positive = right. Preview in headset Slots tab. | -.65–.65 |
| Gadget 2 (right shoulder): down / up | Offset in metres; negative = down, positive = up. Preview in headset Slots tab. | -.65–.65 |
| Gadget 2 (right shoulder): back / forward | Offset in metres; negative = back, positive = forward. Preview in headset Slots tab. | -.65–.65 |
| Primary: left / right | Offset in metres; negative = left, positive = right. Preview in headset Slots tab. | -.65–.65 |
| Primary: down / up | Offset in metres; negative = down, positive = up. Preview in headset Slots tab. | -.65–.65 |
| Primary: back / forward | Offset in metres; negative = back, positive = forward. Preview in headset Slots tab. | -.65–.65 |
| Secondary: left / right | Offset in metres; negative = left, positive = right. Preview in headset Slots tab. | -.65–.65 |
| Secondary: down / up | Offset in metres; negative = down, positive = up. Preview in headset Slots tab. | -.65–.65 |
| Secondary: back / forward | Offset in metres; negative = back, positive = forward. Preview in headset Slots tab. | -.65–.65 |
| Grenade: left / right | Offset in metres; negative = left, positive = right. Preview in headset Slots tab. | -.65–.65 |
| Grenade: down / up | Offset in metres; negative = down, positive = up. Preview in headset Slots tab. | -.65–.65 |
| Grenade: back / forward | Offset in metres; negative = back, positive = forward. Preview in headset Slots tab. | -.65–.65 |
| Knife: left / right | Offset in metres; negative = left, positive = right. Preview in headset Slots tab. | -.65–.65 |
| Knife: down / up | Offset in metres; negative = down, positive = up. Preview in headset Slots tab. | -.65–.65 |
| Knife: back / forward | Offset in metres; negative = back, positive = forward. Preview in headset Slots tab. | -.65–.65 |
| Gadget 3 (behind head): left / right | Offset in metres; negative = left, positive = right. Preview in headset Slots tab. | -.65–.65 |
| Gadget 3 (behind head): down / up | Offset in metres; negative = down, positive = up. Preview in headset Slots tab. | -.65–.65 |
| Gadget 3 (behind head): back / forward | Offset in metres; negative = back, positive = forward. Preview in headset Slots tab. | -.65–.65 |



