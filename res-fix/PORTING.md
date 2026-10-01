# Adapting the resolution approach to BF2142

## Transferable design

- Query the active runtime's recommendation at launch, using the same process architecture/runtime as the game.
- Select a bounded size with an explicit Game-mode fallback.
- Apply it early enough that the engine initializes all dependent render resources coherently.
- Keep physical monitor size separate from the native render canvas.
- Confirm actual source dimensions and submitted eye dimensions independently.
- Keep XR poses and FOV independent of texture and desktop aspect ratios.

## Items to inspect in BF2142

1. **Startup:** When is the VR DLL loaded? Where are launch arguments consumed? Does the native engine support width/height launch options, and when does it create color/depth/postprocess targets? The BF2 import hook must not be presumed compatible.
2. **Window:** Does a resize notification overwrite engine screen dimensions? Is the wrapper forcing monitor-sized windows or backbuffers? Does a windowed zero-sized backbuffer inherit the client size?
3. **Render/capture:** Which surface actually receives the world draw? Is each eye replay full resolution? Are any intermediate color, depth, MSAA, postprocess, scope, HUD, or capture surfaces still using a cached game size?
4. **Transfer:** Does the upload path resize, crop or use an obsolete row pitch? Are source rectangles and atlas eye offsets correct at the new size? Are swapchain replacements safe with its ownership model?
5. **Projection:** Do both eyes retain the runtime FOV and eye poses? Does any projection calculation incorrectly derive its aspect from the monitor window?
6. **Limits:** What allocation budget is appropriate for its 32-bit process and rendering path? BF2's 4096-per-dimension and 8,388,608-pixel caps are implementation choices.
7. **Launch compatibility:** Does the normal launch route, including Steam where applicable, reach the early size setup? Does failure leave the original game command line intact?

## Suggested verification after adapting it

These are proposed BF2142 checks, not tests already performed on that project.

| Scenario | Evidence to collect |
| --- | --- |
| Automatic, connected headset | Current recommendation; requested size; actual world target/viewport; captured dimensions; both submitted rectangles |
| A different runtime quality preset, then restart | Recommendation is queried again and the new request reaches the renderer |
| Custom percentage | Linear width/height scaling, reported clamp if needed |
| Game mode | Original game options retained; no automatic headset-size override |
| Disconnected/broken runtime or missing helper | Bounded fallback, clear diagnostic, no claim that headset size is active |
| Different display size / preview toggle | Native canvas remains correct; desktop menu clicking remains accurate |
| Minimize/restore and device reset | No stale targets, blank eye or loss of renderer-size confirmation |
| Different AA setting | Color/depth/resolve surfaces remain compatible |
| Main menu, kit screen, infantry and vehicles | Correct hit testing, FOV, image coverage and usable HUD |
| Save settings again / rerun installer | Preference change works without a runtime reinstall or overwriting the first recovery backup |

Compare a fixed scene and text/geometry edges. A screenshot of the desktop window alone does not prove the resolution of the headset image. A large swapchain alone also does not prove the scene was rendered at that size.

### Illustrative log, not a captured benchmark

```text
RESOLUTION60 startup mode=Automatic runtimeRecommended=2064x2208 requested=2064x2208 limited=0
RESOLUTION60 device request=0x0 explicit=2064x2208 samples=2
RESOLUTION60 renderer confirmed=1 requested=2064x2208 actual=2064x2208
RESOLUTION49 confirmed native eye=2064x2208 submitted L=2064x2208 R=2064x2208
```

The important result is consistency from the actual scene render through capture to submission. If `actual` remains, for example, `1280x720` while submission is `2064x2208`, the source-resolution problem remains. If a limit or intentional Custom/Game mode produces a smaller image, report that accurately.

## Limits of the supplied reference

- The BF2 maintainer reported the image was crisp after the automatic-resolution work. No BF2142 build or headset run was performed for this publication.
- Source snapshots were taken from the private r63 tree. Core startup/canvas work was established in r60/r61; the r63 material relevant here is repeatable preference saving. This does not publish the rest of r63 or change the public runtime.
- The x86 managed OpenXR helper is tied to the ABI documented by its paired C++ assertions. A native helper avoids manually marshaling those offsets when porting.
- Fallback handles query/startup failures. This is not a guarantee that any arbitrarily large native render allocation will succeed on every GPU/driver; limits and allocation failures still need evaluation in the target game.
- The existing transfer function records the last observed native size before attempting replacement allocation. If allocation fails, it keeps the working chains; this exact implementation does not retry unchanged dimensions every frame. Review retry policy for the destination renderer.
- Threading, device-loss recovery and safe destruction of old XR chains must match the destination implementation.
- Thin vehicle HUD alpha, terrain flicker and UI proportions can have causes independent of render size. Diagnose those independently instead of assuming one resolution change solves all of them.

## Short handoff for the other Codex

Read this folder's README, integration guide and source snapshots first. Trace BF2142's actual launch-to-render path before editing it. The essential BF2 fix is an early runtime-recommended native render size plus coherent window/device/reset/capture handling; increasing only the XR swapchain is insufficient. Adapt BF2142's own startup mechanism, preserve normal launch/authentication, and log recommendation, request, actual source and submitted sizes. Do not copy BF2 executable addresses, ship game files, or represent this BF2 reference as already verified in BF2142.
