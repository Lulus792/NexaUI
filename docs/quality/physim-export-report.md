# Offscreen export of the published Physim view

Example source `a14cfda4d8c85fa6bb07cbbce773c5412d7192fe` exports a published
pendulum snapshot and its original plot range through the existing Physim
view and NexaUI headless renderer. It uses the installed Wayland-only SDK
`b881dd2c0156bdfe1196f743adf4d9177b647985` and the pinned original Physim
source `01ccc27a94ca611c9e9cdd8ccde0da426aee2378`.

The caller supplies integer logical dimensions and scale. An export of
641×401 at 1.25× returns an owned, packed 802×502 RGBA8 image. The export
copies the published model and plot state, constructs an independent context
and releases its viewport lease after rasterization. The caller can encode
the pixels after the context and lease have retired. The live view can retain
an outstanding frame during export.

All 20 installed consumer CTests pass across static Release, shared Release,
ASan/UBSan/LSan and the default native-fixture-off build. The export test
compares every output channel against a separately recorded frame, checks
dimensions at 1.25× and 0.5× and verifies that the live frame pixels, model and
plot range remain intact. Invalid size, nonfinite scale and an already-owned
output are rejected. Both compiled counterexamples fail the full-image
comparison when they discard the plot range or rasterize text at 1×.

The first strict build caught the implicit double-to-float conversion in the
text-density setter. The accepted source makes the conversion explicit at
that API boundary. All four profiles produce the same saved RGB image.
The image was visually inspected. The reference plot still shows a clipped
near-zero tick in scientific notation, and the numeric draft displays long
floating-point precision. Both need presentation review before using the
reference view as a production export layout.

The [evidence manifest](physim-export-evidence.json) binds profile logs,
complete RGB images and the earlier failed build. Local archive
`nexaui-physim-export346.tar.gz` has 173,795 bytes and SHA-256
`d51ecf2f5cc42923c46fa4364fea58084674df698eb24f8a5d22c3c4ce794cb3`.
The lossless local image is `physim-export346.png`.

This advances R12 for the installed Physim CPU view. Exporting other Aetheri
views, native compositor screenshot capture, printing, color management and
engine-specific encoding still have separate acceptance work.
