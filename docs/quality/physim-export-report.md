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
The [later SDK correction](plot-tick-label-report.md) resolves the plot tick
with a separate source and image receipt.

The [evidence manifest](physim-export-evidence.json) binds profile logs,
complete RGB images and the earlier failed build. Local archive
`nexaui-physim-export346.tar.gz` has 173,795 bytes and SHA-256
`d51ecf2f5cc42923c46fa4364fea58084674df698eb24f8a5d22c3c4ce794cb3`.
The lossless local image is `physim-export346.png`.

This advances R12 for the installed Physim CPU view. Exporting other Aetheri
views, native compositor screenshot capture, printing, color management and
engine-specific encoding still have separate acceptance work.

## Current camera export, 2026-11-17

Consumer `e110397cee6ffc45c97e18792274665c47e94cff` uses installed NexaUI
`b53513b390dbf135050a858fbdfd434edc04ad08` and the same 27 pinned Physim
inputs. `physim_view_export` takes the published model, original plot range
and current camera from the view on its owner thread. The existing
`physim_export_render` still provides a front-pose entry point for callers
with just the model and plot. Both use the same separate render context.
The caller owns packed RGBA8 and chooses the encoder.

The installed test renders 641×401 at 1.25× into 802×502, verifies plot-region
RGB equality and scene changes, and exports while an interactive command
stream retains its viewport lease. It rerenders that stream and compares
every pixel, checks the model and plot bytes, and then reads the owned export
after destroying the interactive view. A compiled counterexample substitutes
the front camera and fails the scene change assertion.

The independent Wayland/AT-SPI peer changes yaw, pitch and zoom, then
activates **Export view**. The native host writes a create-only PPM in the
private fixture directory. The peer reads the file and finds 186 original
sphere pixels at the independently computed tilted position. A second request
to the occupied path preserves the file and the native workflow continues.
At the same pose and dimensions, the installed and native exports have
identical scene pixels. Their plot ranges differ because the workflows
intentionally request different ranges.

All 21 installed CTests and twelve private Wayland cases passed across static
Release, shared Release and ASan/UBSan/LSan on Alpine with Clang 22.1.3. Each
export workflow produced byte-identical RGB across profiles. The archive
`nexaui-physim-camera-export381.tar.gz` binds exact source, logs, images and
the rejected counterexample. Readback verified SHA-256
`fe7899178dbf7d7aaa4f853b41a17c3679a601125a67cf95e3f323fb904f7c70`.
Local receipt `physim-camera-export381-final.json` lists every member hash.

Production Physim integration, other Aetheri surfaces, compositor screenshot
capture, printing and color management remain open.
