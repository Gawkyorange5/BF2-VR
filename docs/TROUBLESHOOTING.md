# Troubleshooting and limitations

## Setup

- **Unsupported executable:** use a complete BF2 1.5 installation. Setup validates BF2.exe and RendDX9.dll code, since native hook offsets depend on them. Report the exact legitimate game edition/build if rejected. No bypass or replacement executable is supplied.
- **Missing runtime DLL:** install the current Microsoft Visual C++ v14 **x86** package and DirectX June 2010 runtime from the links in INSTALL.md. Missing MSVCP71/MSVCR71 means the underlying game installation needs attention.
- **Missing reload audio:** a complete original mods/bf2/Objects_client.zip is required. Setup builds its short cues from this archive. It does not download assets.
- **Access denied:** close the game/settings app, then rerun Setup with appropriate access to the game folder. Protected installations may need Run as administrator. Do not alter security software or broad directory permissions.
- **Existing installation:** use Remove / Restore first. Keep .bf2-vr until restoration completes; it contains backups. If files changed after installation, Setup preserves and lists them instead of overwriting them.

## No VR / black headset

Launch **BF2-VR.exe**, not an unmodified BF2.exe. Connect the headset first and select an OpenXR runtime with 32-bit app support. With Virtual Desktop, select VDXR and check the runtime shown in its overlay. Do not add OpenVR DLLs. Read **bf2_clean_vr.log** in the game folder for initialization errors. Confirm the normal flat-screen game still runs.

## Buttons, shooting or movement do not work

Use the same numbered profile selected in Setup and apply its recommended Controls.con. The controller layer relies on these keyboard/mouse bindings. Focus the game window. Release triggers/buttons and center sticks after menus, spawn and seat changes. The VR settings menu suppresses gameplay inputs until closed. Unsupported manual-reload weapons still use **B** for native reload.

## Reloads / arms / scopes

Report the exact weapon, team/map, remaining ammo and action sequence. Custom meshes can differ from the stock profiles. Some AK support-arm sleeve twisting remains a known visual limitation; it is not claimed completely fixed. Scopes are profiled for M24, L96A1, M95, SVD and Type 88 only. Align your eye with the lens, and confirm physical scopes are enabled in VR settings. Launcher scopes are planned.

## Performance

Use the normal launch; no recorder is needed. The r45 runtime keeps the creator's accepted performance work and has optional diagnostics off. No fixed FPS is promised. Plug a portable PC into power for comparable measurements and compare the same map, location, bot count, resolution and graphics settings. Include an overlay showing FPS, Game, Encoding, Network and Decode timing and reprojection/Spacewarp state. Fast head movement can expose delayed/reprojected image edges when frames arrive late. A still screenshot alone cannot establish sustained native FPS.

## Compatibility status

The creator tested the r45 baseline in the headset. Quest 3 / Touch-style controls / VDXR are the established setup. Installer and loader verification results are documented separately in VERIFICATION.md. The release has not been exhaustively validated across hardware, every weapon/map, multiplayer services or custom BF2 mods. Anti-cheat compatibility is not claimed: follow server rules and use allowed environments. The project does not bypass anti-cheat.

## Reporting a problem

Open a GitHub issue with release version, Windows version, GPU, headset, runtime, game edition, mod/map, and exact steps. Attach bf2_clean_vr.log and a short clip if the issue is visual. For setup issues, attach %LOCALAPPDATA%/BF2-VR-Setup.log. Review logs before sharing; remove account identifiers or personal paths you do not want public. Do not upload game executables, full game archives, CD keys or profile/account files.
