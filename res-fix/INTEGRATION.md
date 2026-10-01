# BF2 integration points

This is the wiring around the source snapshots. The excerpt files are **not standalone translation units**. The root public release source predates the private resolution implementation; do not assume the root `src/` already contains these calls.

## Files

| File | Responsibility / dependencies |
| --- | --- |
| [AutoResolution.cpp](reference/RFX/AutoResolution.cpp), [.h](reference/RFX/AutoResolution.h) | Startup interception, helper timeout, argument replacement and actual-size status. Uses `GamePath()`/`Log()` from `VrSettings.h` and `IAT::find_function` from the existing RFX IAT helper. |
| [VrResolution.cs](reference/tools/VrResolution.cs) | Same-architecture OpenXR query. Uses x86 OpenXR ABI; its isolated UI query helper invokes the executable's `--probe-openxr` entry point. |
| [VrResolutionPlan.cs](reference/tools/VrResolutionPlan.cs) | Reads current resolution mode and scale; calls the probe; returns a validated size plan. |
| [RenderWindow.cpp](reference/RFX/RenderWindow.cpp), [.h](reference/RFX/RenderWindow.h) | Logical canvas versus physical client window; requires `AutoResolution`, `GamePath`/`Log`, Win32 and D3D9. |
| [ProbeEntryPoint.excerpts.cs](reference/integration/ProbeEntryPoint.excerpts.cs) | Actual command-line branches from the settings app. They must run before its normal Forms UI. |
| [OpenXRAbiChecks.excerpts.cpp](reference/integration/OpenXRAbiChecks.excerpts.cpp) | Compile-time checks paired with the managed x86 layout. Uses the OpenXR SDK headers. |
| [RuntimeSizing.excerpts.cpp](reference/integration/RuntimeSizing.excerpts.cpp) | Initial eye chains, native-size recording and transfer adjustment. Requires the port's existing OpenXR session/chain/copy machinery. |
| [DesktopPresentation.excerpts.cpp](reference/integration/DesktopPresentation.excerpts.cpp) | BF2 menu aspect / controller pointer mapping and desktop mirror crop integration. Requires its existing capture/HUD machinery. |
| [ResolutionChoice.cs](reference/installer/ResolutionChoice.cs) | Optional installer preference helper; requires `PeImage.Hash`. Does not implement rendering. |

Existing RFX IAT support is in [`src/RFX/IAT.h`](../src/RFX/IAT.h) and [`src/RFX/IAT.cpp`](../src/RFX/IAT.cpp). Review [license scope](../LICENSE-NOTICE.md) before reusing upstream components.

## Native DLL startup

Add the two new native `.cpp` files to the project. In the early DLL entry path:

```cpp
// DLL_PROCESS_ATTACH: install only; no headset query or child process here.
CleanVR::AutoResolution::Install(GetModuleHandle(nullptr));
```

The BF2 implementation requires the DLL to be initialized before the EXE's CRT startup consumes its command line. Its two lookups are for `__getmainargs` and `_acmdln`. Verify both import semantics and ordering in BF2142 instead of copying addresses or assuming a late injection has the same timing.

## Settings/helper executable

Compile `VrResolution.cs`, `VrResolutionPlan.cs` and the normal settings app as **x86**, with the early branches in [ProbeEntryPoint.excerpts.cs](reference/integration/ProbeEntryPoint.excerpts.cs). Use the matching 32-bit OpenXR loader at `<game folder>/openxr_loader.dll`. The DLL expects `<game folder>/BF2-VR-Settings.exe`; rename both sides consistently in another project.

The automatic startup uses:

```text
BF2-VR-Settings.exe --resolve-openxr "<game folder>" "<temporary result file>"
```

The successful response, encoded as UTF-8 **without BOM**, is:

```text
OK59|requestedWidth|requestedHeight|recommendedWidth|recommendedHeight|mode|limited
```

`OK59` is the existing protocol marker; it is not the current build number. Native parsing checks sizes, pixel count, mode and limit flag. The helper's exit code is nonzero on failure. Native startup waits at most 15 seconds for its own child, then uses the game's original size. The separate `--probe-openxr` branch is for the settings UI's read-only query.

The current copied helper uses manual unmanaged layouts. The companion C++ static assertions document the x86 ABI it was compiled against. Do not change it to AnyCPU, use 32-bit-sized handles in these delegates, or reuse its numeric offsets in an x64 process. A port can instead implement the helper in native C++ using SDK structures.

## Main game window

Immediately after creating the actual main HWND, before normal engine window integration continues:

```cpp
CleanVR::RenderWindow::Attach(mainHwnd);
```

The BF2 wrapper also routes its Alt+Enter preview-size toggle through `RenderWindow::TogglePreview()`. This toggles monitor preview size, not VR scene resolution.

The callback maps client mouse messages; wheel/non-client messages are left alone because they use different coordinates. Inspect your existing controller pointer conversion too: BF2's native cursor positions are calculated in backbuffer coordinates. Scaling them a second time would make menu clicks inaccurate.

## D3D9 device creation and reset

Before forwarding the native calls:

```cpp
// CreateDevice wrapper
CleanVR::RenderWindow::ConfigurePresentation(params, focusWindow);
HRESULT hr = realD3D->CreateDevice(adapter, type, focusWindow,
                                 behaviorFlags, params, returnedDevice);

// Reset wrapper: retain your existing resource-release/recreation ordering.
CleanVR::BeforeReset();
CleanVR::RenderWindow::ConfigurePresentation(params);
// Update wrapper cached width/height from params here, if present.
HRESULT resetResult = realDevice->Reset(params);
CleanVR::AfterReset(realDevice, resetResult);
```

`BeforeReset` in BF2 releases frame/capture/HUD resources and marks the device as resetting. `AfterReset` only reattaches on success. This resource lifecycle is part of the integration; the short excerpt is not a replacement for the port's reset handling.

On successful device attachment/reset, BF2 calls `GetBackBuffer`, then `GetDesc`, stores the actual width/height and calls:

```cpp
CleanVR::AutoResolution::Confirm(actualWidth, actualHeight);
```

Before rendering a stereo pair it calls:

```cpp
CleanVR::Runtime::MatchEyeTransfer(actualWidth, actualHeight);
```

Source capture, MSAA resolve and transfer must use the real dimensions. The BF2 path preserves the player's existing game AA settings. OpenXR image allocation and game MSAA are separate parts of the renderer; do not blindly replace native AA with the runtime's swapchain sample recommendation.

## OpenXR initialization and submission

[RuntimeSizing.excerpts.cpp](reference/integration/RuntimeSizing.excerpts.cpp) contains the actual private-line blocks for:

1. Saving both `XrViewConfigurationView` entries and allocating initial eye chains using the selected request when within runtime limits.
2. Recording actual source and submitted dimensions.
3. Matching eye transfer size, allocating both new chains before swapping them into use, retaining existing chains if allocation fails.

The code assumes the port's existing frame lock, chain image enumeration, synchronization, full image upload and projection layer submission are correct. A port must ensure no acquired images or pending GPU work still reference a chain it destroys. Do not copy this function into a different ownership model without adapting that lifecycle.

When size changes, update all copy rectangles, row pitches, atlas eye regions and `XrSwapchainSubImage.imageRect.extent`. Keep `xrLocateViews` poses and FOV. The captured viewport and submitted subimage must cover the intended full eye image.

## UI settings and installer

Expose `graphics/resolution_mode` as Automatic / Custom / Game and `graphics/eye_resolution_percent` as 25–150, with 100 default. Mark both as requiring restart. Avoid implying that saving the preference resizes an active engine session.

The original updater mixed applying a binary patch with changing a preference and rejected its own installed build. The r63 approach is a dedicated **Save resolution only** action; `ResolutionChoice.Save` stages just the INI edit, checks for intervening edits and retains a unique backup. Repeating the same mode is a no-op. The updater also detects its own runtime before checking for an existing patch backup. These installer changes are separate from headset resolution measurement.

## Desktop and HUD follow-up

BF2 uses a wide desktop preview independently of its eye texture shape. The actual menu canvas and hit testing remain tied to native dimensions. [DesktopPresentation.excerpts.cpp](reference/integration/DesktopPresentation.excerpts.cpp) records this integration, including using angular FOV aspect for mirror cropping.

A higher-resolution render exposed other presentation differences during BF2 development. The vehicle HUD transparency fix is a separate premultiplied-alpha/compositor correction and is not included here. Do not treat changing resolution as an alpha fix. Native menu/kit layout may still look narrow at a portrait-like eye aspect; this reference does not claim that every UI layout was redesigned.
