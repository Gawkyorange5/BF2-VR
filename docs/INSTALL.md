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
5. Choose the shortcut's game/mod. The usual choice is **bf2**. Choosing DZB2
   makes the shortcut use its compatibility launcher.
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

The same r134 code retains Survival's weapon aliases, laptop deployment and
activation support. This release does not install, replace or modify Survival
scripts, maps, loadouts or progression. Launch Survival using its existing
configured launcher. The DZB2 command specifically selects `mods/DZB2`.

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
silently overwrite newer Survival DLL work.

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
