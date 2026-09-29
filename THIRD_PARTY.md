# Credits and third-party notices

## RFX

[BadSanta12345/RFX](https://github.com/BadSanta12345/RFX) is the supplied base: Direct3D9 wrapping, Refractor integration, helpers, native BF2 declarations and stl_8_wrapper.lib / stl_8_wrapper.dll. The maintainer confirmed upstream redistribution permission. No blanket license is assigned to that code here.

## BF2142VR

[justiceofrip/BF2142VR](https://github.com/justiceofrip/BF2142VR), reference revision 99e275767cc78379a43c6362e6f250f81d9a5ead. MIT license: licenses/BF2142VR-LICENSE.txt.

Its BF2 porting guidance, arm IK/contact and firing patterns, stereo replay, UI capture, vehicle seat/head aiming and physical optic work informed/adapted into the BF2-specific implementation. BF2 addresses and native layouts were inspected separately; BF2142 offsets are not used as BF2 offsets.

## Battlefield 1942 VR

[JayBiggsGMG's BFVR](https://github.com/JayBiggsGMG/BFVR-Battlefield-1942-VR-Mod) inspired the project and its desired motion-control experience. This release uses OpenXR and does not redistribute that project's OpenVR runtime.

## Khronos OpenXR

[OpenXR-SDK](https://github.com/KhronosGroup/OpenXR-SDK). Headers were taken at f2448a8797c85814aa892efc1ab8707900fbcc78; loader binary reports version 1.1.62.0, x86. License: licenses/OpenXR-LICENSE.txt and per-file notices under src/third_party/openxr. The loader runtime code is used unchanged. The runtime itself must be installed separately.

## Microsoft and game dependencies

DirectX, Visual C++ runtimes and Windows XAudio2 are external dependencies; they are not redistributed by this package. BF2's legacy MSVCP71/MSVCR71 libraries come from the player's working game installation.

Numerical weapon/seat/mesh-mask profiles describe compatibility with stock assets. Meshes, textures, animations, WAV/PCM payloads and game executables are excluded. The installer/audio-recipes.json file contains numerical extraction instructions and hashes only. Setup generates 116 short cues locally from the player's Objects_client.zip.

OpenXR loader includes JsonCpp (Baptiste Lepilleur and The JsonCpp Authors), with the MIT notice in licenses/JsonCpp-LICENSE.txt. Reference: https://github.com/KhronosGroup/OpenXR-SDK/blob/release-1.1.62/.reuse/dep5 . The public loader has only its local PDB filename metadata sanitized; executable code is unchanged.
