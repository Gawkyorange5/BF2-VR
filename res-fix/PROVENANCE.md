# Snapshot provenance

Published at Gawkyorange5's request to help justiceofrip and the BF2142VR Codex understand and adapt BF2-VR's automatic headset-resolution implementation.

- Snapshot date: **1 October 2026**.
- Source lineage: BF2-VR private r63 development tree, carrying the r60 startup/canvas implementation and r61 desktop presentation work. The preference-save helper is from r63.
- `reference/RFX/AutoResolution.*`, `reference/RFX/RenderWindow.*`, `reference/tools/VrResolution.cs`, `reference/tools/VrResolutionPlan.cs`, and `reference/installer/ResolutionChoice.cs` are complete copies of those individual files, with line endings normalized to LF.
- `reference/integration/*.excerpts.*` contains selected verbatim blocks, with headings explaining where each belongs. Those files are reference excerpts, not independently buildable programs.
- `snapshot-manifest.json` records source-relative names, SHA-256 values and excerpt origins. `SHA256SUMS.txt` covers the published reference documents and source files.
- No DLL/EXE, extracted BF2 asset, live settings/profile, account details, local path-bearing log, or user video is included.
- No BF2142 executable inspection, build, installation or headset verification was performed for this reference publication.

## Dependencies and notices

The copies use existing BF2-VR/RFX helpers and renderer types. Integration dependencies are identified in [INTEGRATION.md](INTEGRATION.md). They are supplied for inspection and adaptation; copying the files into BF2142 without providing the appropriate dependencies and startup integration will not produce a working port.

Existing [repository license scope](../LICENSE-NOTICE.md), [third-party notices](../THIRD_PARTY.md), and component notices continue to apply. In particular, [justiceofrip/BF2142VR](https://github.com/justiceofrip/BF2142VR) is credited in this project, with its [MIT notice](../licenses/BF2142VR-LICENSE.txt). This documentation addition does not assign a new blanket license to RFX or other upstream code.

Original BF2-VR contributions are credited to Gawkyorange5, with Codex development assistance. The standalone resolution files and integration notes are shared here in response to the maintainer's explicit request to help the BF2142VR author.

## Official API references

- [Khronos: XrViewConfigurationView](https://registry.khronos.org/OpenXR/specs/1.1/man/html/XrViewConfigurationView.html)
- [Khronos: xrEnumerateViewConfigurationViews](https://registry.khronos.org/OpenXR/specs/1.1/man/html/xrEnumerateViewConfigurationViews.html)
- [Microsoft: Dynamic-Link Library Best Practices](https://learn.microsoft.com/en-us/windows/win32/dlls/dynamic-link-library-best-practices)

The guide explains the local implementation; these API links support the OpenXR view-size contract and Windows loader-lock constraints.
