# v1.1.0 verification

The maintainer tested the public-package candidate in VR and accepted it.
The final requested change adds +restart to shortcut/DZB2 game arguments.
The runtime payload is unchanged from that candidate and retains accepted r134
gameplay code. Its executable code SHA-256 is:
`8c500e1b3be73b111525b00f574aa403c1006283caa203589cc3ebb4d10bafe5`.

Isolated checks passed for clean installation, upgrades from public v1 and r134,
both render choices, unchanged saved preferences, rollback after injected failure,
restoring exact prior runtime/journal, protected later edits/newer code/other
loaders, and original BF2.exe/renderer preservation. All 822 generated sounds
match the private baseline with the tested installed archives. Native startup
probes passed for stock/DZB2 audio and direct/original-executable loading.

DZB2 checks cover local runtime preparation, shader adapter, mods junction,
later-installed assets and persistent settings. Final embedded resources and
shortcut arguments were checked. The Alt-Tab hotfix message is conditional on
the specifically identified older renderer; compatible installations pass.

The probes do not simulate a BF2 match. Actual affected Steam installations,
other GPUs/headsets and reported remote-camera freezes are not universally
verified. See the release notes for known limits. SHA256SUMS accompanies downloads.
