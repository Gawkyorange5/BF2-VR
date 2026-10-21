# Building v1.1.0

## Rebuild the installer

On Windows with Python 3 and .NET Framework 4.8, run:

    python tools/build_setup.py

Output: `build/BF2-VR-Setup.exe`. It embeds the reviewed, asset-free runtime
payload, current settings application, controls, local audio recipes and DZB2
launcher. This build does not download or modify a game installation.

## Runtime source

The source in `src/` is the accepted r134 runtime. Before building, run:

    python tools/prepare_audio_placeholders.py

This creates 822 zero-filled `.pcm` resource inputs with the reserved sizes;
they contain no game audio. Do not replace existing locally generated audio.
Open `src/RFX.sln` with Visual Studio's C++ desktop tools, toolset v145,
Win32 / RunWDebugger (historical name; optimized). Supply a DirectX SDK June
2010 installation through DXSDK_DIR. OpenXR headers and the established STL
import library are included. Preserve the project optimization/ABI boundaries.

For distribution, run `python tools/prepare_runtime.py <built-RFX.dll> payload/RFX.dll`.
It removes local PDB path metadata and zeroes audio while preserving resource
layout and the Windows manifest. Never ship an installed runtime with game audio.

The v1.1.0 release payload uses the accepted binary without recompiling its
gameplay code. Rebuilding native source may produce different machine code;
review the installer's accepted code IDs and rerun compatibility/loader checks
before distributing a custom build. Do not bypass unknown-build rejection.

## Local sound generation

`installer/audio-recipes.json` contains only numeric recipes and source archive
entry names/hashes. Setup rebuilds sound resources from the owner's stock and
optional DZB2 installation. Missing optional DZB2 sounds are silent; the DZB2
launcher refreshes them when the mod becomes available. Resource updates retain
the original resource VirtualSize, relocation address and image size.

## Other sources

Desktop settings: `src/tools/`. Installer/DZB2 launcher: `installer/`.
Shader compatibility adapter: `shader-compat/`. Optional DirectInput loader:
`steam-loader/`. Their third-party notices are in `THIRD_PARTY.md` and `licenses/`.

## Verification

See `VERIFICATION.md`. The installer harness is compiled separately with
INSTALLER_VERIFY for isolated fixture profiles and without registry updates.
It requires the maintainer's own supported game and private baseline inputs.
Those fixtures, executables, archives, audio, personal profiles and logs must
never be committed. Native loader probes do not establish in-headset gameplay.
