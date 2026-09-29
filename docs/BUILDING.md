# Building

## Setup only

Use Windows 10/11, .NET Framework 4.8 and Python 3. From the repository root:

    python tools/build_setup.py

Output: build/BF2-VR-Setup.exe. No network access or game installation is used by this command. It embeds the checked release payload, configuration, controls and audio recipes. Do not substitute an installed RFX.dll directly: it contains locally generated game audio.

## Runtime

Open src/RFX.sln with Visual Studio's C++ desktop tools and a Windows SDK. The checked project uses toolset v145, Win32, configuration RunWDebugger (the historical configuration name; optimization is enabled). Set DXSDK_DIR to a DirectX SDK June 2010 installation if necessary. OpenXR headers and the STL wrapper import library are included. Keep the selective whole-program optimization boundaries in the project; the native BF2 hook translation units have ABI-sensitive code.

The public ReloadAudio.rc intentionally contains no game audio. Build the DLL, then use tools/prepare_runtime.py before placing it in payload/RFX.dll for packaging. A DLL with no .rsrc section can receive a new resource section during Setup; the release baseline preserves its original resource address space. The public r45 payload keeps the confirmed runtime's executable code; only resource payloads and local PDB path metadata are removed.

Desktop settings sources are in src/tools. They compile as an x86 .NET Framework Windows Forms app referencing System.Windows.Forms and System.Drawing.

## Verification

verification/InstallerChecks.cs compiles with the installer core and the same embedded resources, with INSTALLER_VERIFY defined, as a separate console executable. This define allows isolated profile directories only for the checks; it is absent from the production installer. It accepts a fresh fixture folder, a local supported BF2.exe, the local game directory, repository directory and a private baseline source directory containing the locally generated PCM. It copies required inputs into the fixture and never launches or edits the live game. Do not commit the fixture or game inputs.

verification/LoaderProbe.cpp is a minimal x86 native executable with the three import slots used by RFX. Applying the same PE import function to its copy verifies startup loading and hook installation without invoking BF2 rendering/game initialization. This is a loader smoke check, not an in-headset gameplay test.

See VERIFICATION.md for the scope/results of the release checks. The public package must exclude BF2.exe, locally generated BF2-VR.exe, game archives, extracted assets, PCM/WAV, PDBs, personal logs and profiles other than the reviewed Controls.con.
