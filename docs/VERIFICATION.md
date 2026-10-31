# v1.2.0 verification

The gameplay basis is saved r141, without a native runtime rebuild. Code ID
(.text VirtualSize bytes):
`25bda20576a04f0102262e5044ecaf830e1cc83160f7cb9aa35a7d9c0100b724`.

The distributed runtime has zero-filled game audio and sanitized PDB path metadata.
Setup rebuilds 822 sound resources from the owner's installed archives; code and
runtime sections are checked against r141. Stock-only and DZB2 startup probes,
fresh/upgrade installs, saved settings, separate shortcuts, rollback, optional
loader handling and embedded-resource checks are run in isolated fixtures.
See PACKAGE-VERIFICATION.txt and SHA256SUMS.txt in the release download.

The private r141 gameplay was tested and accepted. The final public package
has no separate headset playtest. No universal FPS claim is made; the prior
release's game/mod/hardware limitations still apply.
