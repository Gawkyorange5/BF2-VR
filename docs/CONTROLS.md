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
