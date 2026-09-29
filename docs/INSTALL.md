# Installation — start here

## 1. Get Battlefield 2 working flat-screen first

1. Install your own legitimate copy of Battlefield 2.
2. Apply the official **1.5 patch** (and any preceding patch your installation requires).
3. Launch the normal game. Create/select a profile, load a map, spawn, move and fire successfully.
4. Close the game completely.

BF2-VR does not install the game, update it to 1.5, unlock weapons, or repair a broken flat-screen installation. No replacement game executable is supplied. Keep a backup of your game and existing mod configuration if you already use another modification.

## 2. Prepare the headset and runtimes

### Virtual Desktop / Quest

1. Install and set up Virtual Desktop on the headset and its Streamer on the PC.
2. Connect the headset to the PC and confirm that Virtual Desktop works.
3. In the Streamer's OpenXR runtime selection, select **VDXR / VirtualDesktopXR**.
4. Leave the headset connected before launching BF2-VR. SteamVR is not needed on this path.

See the [VDXR project guide](https://github.com/mbucchia/VirtualDesktop-OpenXR/wiki) for runtime setup. If you do not use Virtual Desktop, select your headset's compatible OpenXR runtime. SteamVR may serve as an optional OpenXR runtime; it is not mandatory. BF2-VR is a 32-bit application, so the runtime must support 32-bit OpenXR applications. Alternative runtimes have not all been validated with this mod.

### Windows dependencies

- Windows 10 or 11 with .NET Framework 4.8 for Setup and the desktop settings app.
- Microsoft's latest supported **Visual C++ v14 x86** redistributable. Install x86 even on 64-bit Windows. [Official Microsoft downloads](https://learn.microsoft.com/en-us/cpp/windows/latest-supported-vc-redist?view=msvc-170).
- **DirectX End-User Runtimes (June 2010)** for the legacy D3DX9 library. Having DirectX 12 installed does not supply every legacy helper. Download from [Microsoft](https://www.microsoft.com/en-us/download/details.aspx?id=8109), extract it to a temporary folder, then run DXSETUP.exe there.
- BF2's own MSVCP71/MSVCR71 libraries must already be present. If they are missing, repair the original game installation.

Setup includes the 32-bit OpenXR loader and RFX's STL wrapper. It does not install runtimes or alter the system's active OpenXR setting for you.

## 3. Install BF2-VR

1. Open this repository's **Releases** page. Download **BF2-VR-Setup.exe**. Do not use GitHub's automatic Source code ZIP as the installer.
2. Run Setup. Choose the **Battlefield 2 folder containing BF2.exe**, not Documents and not the mods/bf2 subfolder.
3. Choose the numbered BF2 profile you use. The profiles are found through Windows' Documents location, usually Documents/Battlefield 2/Profiles/0001. If no profile appears, run BF2 flat-screen, create one, close it and reopen Setup.
4. Leave **Apply recommended controller bindings** checked for the documented controls. This replaces that profile's Controls.con and makes a backup. Your previous custom keyboard/mouse bindings can therefore change. Account, video, login, server and other profile files are not copied or replaced.
5. Leave **Create a Battlefield 2 VR desktop shortcut** checked. Choose **bf2** for the initial stock-game setup. Other installed mods appear in the list, but custom content compatibility is not guaranteed.
6. Check the box confirming you have patched to 1.5 and played flat-screen, then click **Install BF2-VR**.
7. Wait for the success message. Do not close Setup while it is installing.

If the game is in a protected folder and Setup reports access denied, close it and use **Run as administrator** on Setup. Use the same Windows account that owns your BF2 Documents profile. Setup never changes folder permissions or disables security software.

Setup checks the code fingerprints of BF2.exe and RendDX9.dll. An unsupported-build message means the exact native hook layout is not recognized. Report it instead of bypassing the check or replacing the game with a random executable.

### What Setup does

- Creates **BF2-VR.exe** from your local BF2.exe and adds the RFX import to the copy. The original BF2.exe is untouched.
- Installs RFX.dll, openxr_loader.dll, stl_8_wrapper.dll and BF2-VR-Settings.exe.
- Generates 116 short reload cues from your own Objects_client.zip, then embeds them in the local RFX.dll. The downloaded DLL contains no extracted game audio.
- Creates vr_stereo.ini if absent. Existing VR settings are retained.
- Optionally installs the recommended Controls.con into the selected Documents profile and creates a desktop shortcut.
- Saves originals and an installation journal inside **.bf2-vr** in the game folder. Keep this folder for restoration.

## 4. First launch

1. Connect the headset and start the chosen OpenXR runtime.
2. Launch the **Battlefield 2 VR** desktop shortcut. Alternatively run BF2-VR.exe in the game folder.
3. Select the profile chosen in Setup. Point the right controller at menu items and squeeze its trigger to click.
4. Load a map, choose a kit/spawn point with the pointer, and spawn.
5. Hold your head at your usual seated or standing height. **Tap and release both stick clicks together to recenter.**
6. **Hold both stick clicks for 0.6 seconds** to open VR settings. Release both, then point/click. Apply saves; closing recenters automatically.

The original BF2.exe remains your normal flat-screen launch when it originally had no VR import. If your pre-existing executable was already modified to import RFX, Setup preserves that original behavior too; it does not remove an existing modification.

## Update, remove or restore

For this first installer version, close BF2 and the settings app. Rerun the downloaded Setup executable, select the same game folder and choose **Remove / Restore** before installing another Setup version. Existing installation journals are never overwritten.

Removal restores backed-up files and removes new files if they still match what Setup installed. Files you edited after installation are preserved, and Setup lists the conflicts and keeps the backups. For example, if you changed a newly installed vr_stereo.ini, copy it to a safe location, remove only that conflicting installed file, then rerun Remove / Restore. If you changed Controls.con, save the version you want before choosing between it and its numbered backup. Do not delete the entire .bf2-vr folder while restoration is incomplete.

Setup does not remove game content or the original BF2.exe. Runtime-generated logs and settings-app backup files may remain. The setup log is **%LOCALAPPDATA%/BF2-VR-Setup.log**.
