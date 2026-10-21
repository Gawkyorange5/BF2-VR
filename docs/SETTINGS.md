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

