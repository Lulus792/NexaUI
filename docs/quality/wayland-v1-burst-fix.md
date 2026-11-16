# Consecutive Wayland v1 IME insertions

SDK candidate `b881dd2c0156bdfe1196f743adf4d9177b647985` fixes the
[recorded input loss](wayland-v1-burst-gap.md) for built-in text and numeric
fields. The editor reports the revisions immediately before and after each
accepted native edit. Wayland v1 can keep accepting plain commit strings from
an earlier serial only when the next surrounding state matches the end of
that chain for the same target and activation.

Changing the value or selection independently establishes a new serial floor.
Preedit and byte-range deletions still require the current surrounding
snapshot. Old normalized events retain their original editor revision. The
feedback is private and uses integers on the window owner thread. The
follow-up to `ab60c929` hides two symbols that the first shared build exposed.
The corrected shared library has the same 546 exported names as the previous
installed SDK at `368cd38292498bd538964ba514ff02e0e9924358`. Public headers
are byte-identical between these SDK revisions.

The old SDK lost the period in the deterministic regression in all three
profiles. In the candidate, each profile passes both burst test modes,
covering 18 editor scenarios across ordinary and atomic event delivery.
These include multi-scalar replacement followed by another insertion, external
value replacement, a caret command ordered after a native edit in the same
frame, and stale preedit and deletion. The existing v1, v3, password-storage,
key-handoff, field and transaction cases pass in the same build. In total,
84 selected SDK CTests and 12 installed consumer CTests execute successfully
in Wayland-only static Release, shared Release and Clang ASan/UBSan/LSan.

The unchanged Physim fixture from `795812b4f09f469677c2c3e33a763ed9986623f2`
runs the original pendulum from `01ccc27a94ca611c9e9cdd8ccde0da426aee2378`.
All 12 native executions pass on private Weston 14.0.2, including focus
cancellation, view replacement and close while composing. The independent
AT-SPI and pixel checks run with the original input and deadlines. In the
sanitized focus-cancellation case, the native trace shows the exact failing
ordering from the earlier report. The client publishes surrounding text
`2` at serial 14, receives the period at serial 12, then publishes `2.` and
`2.5`. Apply advances the original physics model to generation 2 with
length 2.5. Two other native runs also observe a period older than the latest
surrounding serial.

Local archive `nexaui-v1-ime-330.tar.gz` contains 146 members, including the
source archive, installed libraries, test receipts, traces and 24 native
pixel captures. Every member was read back and checked against its source
hash. The archive has 27,157,213 bytes and SHA-256
`b7c45412b5100e4ce188fae2cb3b83309a428592f4509e8fa6b7b68c2fcc9d40`.
The [archive manifest](wayland-v1-burst-fix-evidence.json) binds the result.

The [cross-platform CI run](https://github.com/Lulus792/NexaUI/actions/runs/43965126697)
tracks the exact SDK candidate for main publication. App-owned v1 editors,
additional compositors and physical input methods still need coverage. The
Physim fixture covers the original model and selected native tasks.
Production runner integration, the full scene and camera/picking,
object accessibility, approved design and performance acceptance remain open.
