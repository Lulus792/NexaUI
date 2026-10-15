# Initial macOS Metal backend

The private `nexaui_renderer_metal` target implements initial A01/A02 clear/present
and A03–A05 geometry, clipping, composition and RGBA8/A8 uploads for the
[AppKit adapter](macos.md). Shape/alpha/lifecycle tests pass on Apple's virtual
Metal device. Image uploads and the new native NexaCloud runner have an
implementation with native execution pending.

## Build and ownership

Apple builds enable `NEXAUI_BUILD_METAL`; disable it for a core/window-only build.
The target directly links OS Metal, QuartzCore, CoreGraphics and Foundation. Its build-tree
C header exposes an opaque renderer and diagnostics, with no Objective-C/Metal
types. The installed core has no direct Metal dependency.

The main-thread renderer leases an AppKit window and attaches a CAMetalLayer,
retaining its previous layer state for restoration. Competing leases and
wrong-thread calls fail. Destroying the window while leased leaves it alive.
The view receives the custom layer before enabling `wantsLayer`, following
AppKit's layer-hosting order; destruction restores the prior view state.
An owned BGRA8 UNORM canvas receives a quantized RGBA8 clear; a GPU blit copies
it to the drawable. The layer declares sRGB color space. Resize allocates the
replacement before dropping the prior canvas and checks an initial conservative
8192-pixel axis cap. Zero-size targets suspend.

## Finite retirement

The renderer retains the pending command buffer, which retains its encoded
resources. It polls status for at most one second instead of an unbounded
`waitUntilCompleted`. No asynchronous callback dereferences the C renderer.
A timeout suspends rendering; close returns `NX_ERROR_LIMIT` with resources and
the window lease intact. Retry close rather than freeing them. Terminal command
errors latch the renderer until explicit recovery, which retires work before
recreating the device/queue and canvas state.

`allowsNextDrawableTimeout=YES` bounds CAMetalLayer acquisition to its documented
one-second wait. An unavailable drawable suspends the frame. A render autorelease
pool promptly releases the caller's drawable reference after commit; Metal/Core
Animation retain their own presentation resources. GPU completion is not proof
that a frame has become visible on screen.

## Initial geometry and composition

The first command path handles filled rectangles and triangles, inside strokes,
rounded fills and round-capped lines. A plain C draw iterator validates the full
stream before drawable acquisition, bounds float32 intermediate ranges, applies
up to `NX_MAX_CLIP_DEPTH` nested clips and emits pixel-aligned bounding quads.
Its NDC Y axis is inverted relative to the Vulkan path: Metal's viewport maps
the top of the drawable to NDC +1. The fragment shader tests analytic coverage
at pixel centers, with compensated float32 arithmetic for near-edge triangles,
rounds and lines. It uses no optional double-precision shader support.

Authored MSL is embedded as source; Apple's OS compiler creates the pipeline
once per device, without an external shader translation library or runtime
source-file lookup. `fastMathEnabled=NO` and `FP_CONTRACT OFF` preserve the
split-product operations. This initial path copies small per-draw vertex and
shape arguments with Metal's `set*Bytes` API. Upload rings and batching are A06
work. Color output is quantized RGBA8; fixed-function blending is disabled.
Alpha values that quantize to zero are skipped, and those that quantize to 255
are opaque. Bitmap draws sample a private RGBA8 atlas and apply integer tint
and the same straight-alpha formula.

For each translucent draw the encoder first ends the previous render pass,
copies the clipped destination rectangle to an owned history texture, then
loads the canvas and draws with integer straight-alpha composition in MSL.
No shader simultaneously reads and writes the same texture. Standalone device
textures use Metal's default hazard tracking with the existing `MTLCommandQueue`;
it orders the render/blit/read transitions between encoders. Resize allocates
both replacement textures before releasing either old texture. History-allocation
failure releases the candidate canvas and preserves the previous pair. This
simple first composition path still needs A06 batching/performance work.

## Image and glyph uploads

The A05 atlas shares RGBA8 images and A8 glyph masks. A plain C planner deduplicates
complete source views (pointer, dimensions, stride and format), packs shelves,
widens/retries within an 8192-pixel axis cap and enforces a 64 MiB pixel budget.
Metadata has a fixed limit of 4096 distinct views per frame. Zero-size destinations
and zero quantized alpha need no source entry. The planner validates metadata
without reading pixel bytes; copying requires valid pixels from a successful plan.
RGBA8 copies exclude source row padding. A8 becomes white RGB plus source coverage;
the shader applies the command's RGBA tint. Compensated nearest-neighbor sampling
preserves fractional-scale source-cell boundaries.

Before encoding, the renderer copies the packed bytes to its own shared Metal
buffer, then a blit uploads them into a private shader-read texture. Atlas rows
are multiples of 4096 bytes. The prior command buffer must be terminal before
the shared staging storage is overwritten or grown. Pending GPU work never reads
caller pixels or command storage. Allocation failure retains the previous atlas
and upload buffer; candidates are installed only after both allocations succeed.
Current atlas storage and staging capacity each stay within 64 MiB; transactional
growth temporarily retains both previous and candidate resources. Borrowed image
storage remains the caller's responsibility until submission returns.

The [NexaCloud macOS runner](../../examples/nexacloud/native_macos.c) uses the same
C17 scene as Windows/Linux. Its authored smoke compares every GPU RGBA8 byte
against headless for overview, file browser and a second compact window, then
writes `*-macos-*.ppm` captures. Normalized pointer/key events test scene state;
physical AppKit input and display acceptance remain separate. CI saves captures
and CTest logs for static/shared macOS jobs. A native run is still required
before these new full-scene images can be claimed as validated.

`tools/embed_metal_shader.py --check` verifies source/header correspondence;
it does not compile MSL. CI runs this check on all three platforms. Pipeline
compilation and GPU byte comparisons still require a Metal device.

## Validation status

The Windows/Linux host cannot compile or execute Objective-C Metal code. Its C17
example entry point compiles as portable C; host configurations still build.
The initial clear/present renderer from `aa42f01` compiled with AppleClang 21.0.0
in [macOS CI](https://github.com/Lulus792/NexaUI/actions/runs/38656947313/job/116168845559).
The overall build stopped at the test target's missing public-header include
path; its direct `NexaUI::NexaUI` dependency is now explicit. That build did not
execute Metal tests.

At `083cf94`, the [static macOS job](https://github.com/Lulus792/NexaUI/actions/runs/38767168136/job/116533287565)
compiled the MSL with Apple's OS compiler and executed on `Apple Paravirtual
device`. The clear/opaque geometry/clip/readback and recovery comparisons all
reached completion (36 submitted/retired frames) before the test failed its
post-destruction layer-state assertion. The destructor now disables layer
backing before restoring the saved layer, avoiding an automatically created
replacement when restoring nil. The Metal clear/present example passed.
The [shared job](https://github.com/Lulus792/NexaUI/actions/runs/38767168136/job/116533287566)
failed to link the C example's Foundation symbols: the Metal target now names
its own Foundation dependency explicitly instead of relying on the static core's
AppKit link closure. These failures are resolved at `74aa8ec`: both
[static](https://github.com/Lulus792/NexaUI/actions/runs/38768396552/job/116537519079)
and [shared](https://github.com/Lulus792/NexaUI/actions/runs/38768396552/job/116537519309)
macOS 26 ARM64 / AppleClang 21 CI jobs passed all 77 tests, including Metal
shape/alpha/readback/retirement/recovery and the native clear example. Metal ran
rather than skipping. Installed and subdirectory C consumers also built; the
installed consumer ran successfully.
The newer A05 bitmap/full-scene GPU tests are pending. A virtual-device result
is not physical hardware acceptance.

The first A05 native jobs at `6140315` compiled the Objective-C/C targets but
stopped at MSL compilation: direct `.x/.y` access on packed-vector members was
rejected. The image branch now converts those arguments to regular `float2`
values before accessing components, matching the already tested geometry path.
The embedded source is regenerated. Native compilation/execution of this
correction remains pending; the failed jobs produced no full-scene GPU captures.

The separate `nexaui_metal_plan` test runs on Windows and Linux. It interprets
prepared geometry through the independent headless renderer and checks every
RGBA byte and padding byte at 1x/1.25x/1.5x/2x, including empty/intersecting clips,
all five shape types, overlapping alpha and a distant triangle. It also checks the Metal
NDC orientation, invalid streams, image metadata, range limits and
zero-size suspension. Windows MSVC Debug passed; Linux Clang ASan/UBSan with
leak detection passed. These are CPU preparation results, not Metal execution.

The new `nexaui_metal_bitmaps` CPU test also passes on both hosts, with Linux
ASan/UBSan/LeakSanitizer. It checks deduplication, padded RGBA/A8 copies, untouched
output padding, shelf widening, maximum count/budget failures and invalid views.
The macOS runner's entry point compiles as portable C17 on Linux; that compile
does not link AppKit/Metal or establish native rendering.

`nexaui_metal` is authored to compare every GPU canvas RGBA8 byte with the software
reference, including alpha, quantization ties and row padding. It checks resize,
allocation faults, drawable unavailability, thread/lease rules, retained readback,
recovery and retryable driver close. Authored shape comparisons cover mixed
opaque shapes, fractional scales, nested clips, both triangle windings, shared
diagonals, degenerate geometry, large-coordinate cancellation and pipeline
allocation failure/recreation. Additional alpha cases cover overlapping rects,
rounded fills and triangles over transparent/translucent/opaque destinations,
one-byte alpha, history allocation failure and recovery. New authored A05 cases
cover tinted RGBA/A8, repeated views, fractional sampling, reusable capacities,
atlas/upload allocation rollback and freeing borrowed pixels while an owned
upload remains pending. They and the native examples skip explicitly only if
there is no Metal device. `NEXAUI_REQUIRE_METAL=1` makes absence fail on required
hardware runs. Tests enable the Metal debug layer; actual results will be recorded
here. A skip never establishes native GPU support.

Timed-out readback keeps an owned shared buffer and leaves caller output
untouched. It never retains the caller's output pointer. After completion the
helper converts BGRA to RGBA, preserving row padding. Readback is test-only;
production does not upload a CPU-rendered frame. Display color management,
hardware performance, physical DPI/input and native NexaCloud remain open.

## Primary references

- [AppKit layer-hosting order](https://developer.apple.com/documentation/appkit/nsview/wantslayer)
- [Drawable ownership](https://developer.apple.com/documentation/quartzcore/cametallayer)
- [Bounded drawable acquisition](https://developer.apple.com/documentation/quartzcore/cametallayer/allowsnextdrawabletimeout)
- [Command buffer status](https://developer.apple.com/documentation/metal/mtlcommandbuffer)
- [Unbounded waitUntilCompleted](https://developer.apple.com/documentation/metal/mtlcommandbuffer/waituntilcompleted%28%29?language=objc)
- [Texture readback into a shared buffer](https://developer.apple.com/documentation/metal/reading-pixel-data-from-a-drawable-texture)
- [Metal math compile options](https://developer.apple.com/documentation/metal/mtlcompileoptions/fastmathenabled)
- [MSL specification, section 1.6.3: FP contraction](https://developer.apple.com/metal/Metal-Shading-Language-Specification.pdf)
- [Metal coordinate systems](https://developer.apple.com/library/archive/documentation/Miscellaneous/Conceptual/MetalProgrammingGuide/Render-Ctx/Render-Ctx.html)
- [Metal resource synchronization](https://developer.apple.com/documentation/metal/resource-synchronization)
- [Shared storage access](https://developer.apple.com/documentation/metal/mtlstoragemode/shared)
- [Blit encoder buffer/texture copies](https://developer.apple.com/documentation/metal/mtlblitcommandencoder)
