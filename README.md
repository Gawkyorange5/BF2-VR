# Battlefield 2 VR — v1.1.0

PC VR for Battlefield 2, with OpenXR head tracking, tracked hands and weapons,
physical reloading and equipment, vehicle controls, and DZB2 compatibility.
This package uses the accepted **r134 gameplay code**.

[Download v1.1.0](https://github.com/Gawkyorange5/BF2-VR/releases/tag/v1.1.0) · [Features and changes](RELEASE-NOTES.md) · [Bug fixes](docs/FIXES.md)

## Start here

1. **Patch Battlefield 2 to 1.5 and play a map successfully in flat-screen mode
   first.** Create a local player profile. The Alt-Tab hotfix is only needed if
   Setup identifies the older renderer; see troubleshooting in `docs/INSTALL.md`.
2. Connect your headset and select its OpenXR runtime. Install the required
   Microsoft x86 runtimes listed in `docs/INSTALL.md`.
3. Run **BF2-VR-Setup.exe**. Select the folder containing `BF2.exe`, choose your
   profile and resolution preference, then click **Install / Update BF2-VR**.
4. Start **Battlefield 2 VR** from the created shortcut. For DZB2, install that
   mod separately and use the included **Launch DZB2 VR.cmd**.

The ZIP can be extracted anywhere. You do not need to copy its files into the
game manually. Setup installs the launcher and its helper into your chosen
game folder. The command in this ZIP also locates the most recent installation
made by this Setup under the same Windows account.

On an existing installation, **Keep current resolution setting** preserves
your preference. Leave controller bindings unchecked if your controls already
work and you want to retain custom keyboard bindings. A fresh installation
should apply the included controller profile.

## Read more

- `docs/INSTALL.md`: complete installation, DZB2, updating and restoration.
- `docs/CONTROLS.md`: on-foot, reload, equipment, radio and vehicle controls.
- `docs/SETTINGS.md`: saved options, resolution choices and slot adjustment.
- `RELEASE-NOTES.md`: changes since the first public release and known limits.
- `docs/FIXES.md`: the retained bug fixes since the first public release.
- `THIRD_PARTY.md` and `licenses/`: credits and dependency notices.

This is a PC VR mod, not a standalone Quest APK. Battlefield 2 and DZB2 assets
are not included. Setup creates the VR executable from your own BF2.exe and
generates sound cues from your own installed archives. Original game archives, mod scripts, and the original BF2.exe are not overwritten.
