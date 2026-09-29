# Release verification — 1.0.0 / r45

Checked on 2026-09-29 using isolated workspace copies. The live game installation and Documents profile were not changed by these checks.

| Check | Result |
| --- | --- |
| Supported stock BF2 executable starts without an RFX import | Passed |
| Add RFX import to separate executable copy | Passed |
| Preserve original game .text bytes and original file | Passed |
| Repeat import operation without duplicate import | Passed |
| Reject changed/unsupported code and truncated executable | Passed |
| Generate all 116 reload cues from local game WAV files | Passed; 116/116 bit-identical to r45 PCM |
| Simulated failure after three installed files | Passed; prior files restored |
| Retain existing vr_stereo.ini | Passed |
| Apply recommended Controls.con with backup | Passed |
| Refuse overwrite of existing install journal | Passed |
| Uninstall restores prior mod DLL and controls | Passed |
| New preferences installed when INI absent | Passed |
| Preserve files edited after installation and retain backups | Passed |
| Resolve conflict and complete restoration | Passed |
| Shortcut creation and target / working directory / mod arguments | Passed in isolated folder |
| Setup window and bundled help | Opened and inspected |
| Native x86 process automatically loads RFX before entry | Passed |
| RFX hooks all three expected import slots in native probe | Passed |
| Public RFX executable code matches confirmed r45 | Passed; SHA-256 below |

RFX .text (virtual bytes): f824ba53583c621c68533a0c94ef8808d8abcd6d16ab136a32eaa17fcc19ad38.

The creator confirmed the r45 gameplay baseline in the headset before packaging. Installer checks do not establish compatibility with every installation or constitute a new headset/FPS benchmark. The native probe verifies loading; a full BF2 match under the newly generated executable name still needs user/hardware compatibility feedback. The source and installer do not promise a fixed frame rate.
