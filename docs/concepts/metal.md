# Initial macOS Metal backend

The private `nexaui_renderer_metal` target implements initial A01/A02 clear/present
and A03–A05 geometry, clipping, composition and RGBA8/A8 uploads for the
[AppKit adapter](macos.md). Shape/alpha/lifecycle tests pass on Apple's virtual
Metal device. Image uploads and the native NexaCloud overview/file/compact runner
also pass the static/shared native CI suite at `fb80bd0` (79/79 tests each).

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

The renderer retains up to three pending command buffers, each in a fixed frame
slot with owned record/bitmap uploads and a private atlas. A full ring retires
its oldest submission before reusing that slot. Resize to different dimensions,
recovery and close drain all slots. Status polling shares a one-second budget
across a drain instead of using an unbounded `waitUntilCompleted`.
No asynchronous callback dereferences the C renderer.
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
split-product operations. The original unbatched path copies per-draw arguments
with `set*Bytes`; the new A06 path below uses owned draw records and keeps that
original path only for native comparisons. The three-slot upload ring is described below.
Color output is quantized RGBA8; fixed-function blending is disabled.
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
composition path retains conservative per-reader history copies; physical
performance acceptance remains open.

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
are multiples of 4096 bytes. Each frame slot owns its upload buffer and atlas;
that slot's command buffer must be terminal before its shared storage is
overwritten or grown. Pending GPU work never reads
caller pixels or command storage. Allocation failure retains the previous atlas
and upload buffer; candidates are installed only after both allocations succeed.
Each slot's atlas texels and staging capacity each stay within 64 MiB; transactional
growth temporarily retains both previous and candidate resources. Borrowed image
storage remains the caller's responsibility until submission returns.

The [NexaCloud macOS runner](../../examples/nexacloud/native_macos.c) uses the same
C17 scene as Windows/Linux. Its authored smoke compares every GPU RGBA8 byte
against headless for overview, file browser and a second compact window, then
writes `*-macos-*.ppm` captures. Normalized pointer/key events test scene state;
physical AppKit input and display acceptance remain separate. CI saves captures
and CTest logs for static/shared macOS jobs. The first three captured views now
pass; see the [native scene report](../nexacloud-validation.md#native-macos-scene--2026-10-15).
The additional compact-details dialog capture and Escape/focus-restoration check
also pass in static/shared CI at `2f70d68`; its GPU image is published in that report.

`tools/embed_metal_shader.py --check` verifies source/header correspondence;
it does not compile MSL. CI runs this check on all three platforms. Pipeline
compilation and GPU byte comparisons still require a Metal device.

## A06 draw-record buffer and ordered batches

The authored production path packs each visible quad, shape and exact scissor
into a 116-byte C/MSL record in a reusable shared Metal buffer. Records contain
no source pointers. Capacity grows transactionally to at most 64 MiB; the prior
buffer is retained on allocation failure. A replacement temporarily coexists
with the old buffer. Each of three slots now owns a record buffer, bitmap staging
buffer and atlas. Only that slot's terminal command buffer permits CPU overwrite
or growth. Two other retained submissions may remain pending while the next slot
is prepared. The test-only serial baseline uses a limit of one; production uses
three. Discarded/unavailable submissions do not advance the cursor or publish a
profile. At `a7c907b`, both macOS variants pass 81/81 native tests, including
the three-slot lifecycle cases below; all six desktop variants and Linux
sanitizers pass. The [ring comparison](../../benchmarks/baselines/metal-a7c907b/README.md)
preserves 768 measured frames and 24 byte-identical mode/block GPU captures.

Canvas and history remain shared GPU-only tracked textures, directly bound on
one legacy `MTLCommandQueue`; Metal's tracked-resource hazards order their GPU
reads/writes across the committed buffers. CPU upload data never shares a busy
slot. This relies on neither Metal-4 queue behavior nor untracked heap resources.
The three record/upload/atlas payload capacities sum to at most 576 MiB at their
individual 64 MiB caps, excluding canvas/history, temporary growth candidates
and Metal allocation overhead. Resources allocate on demand. Diagnostics expose
summed capacities and each slot's capacity/submission. A pending count describes
retained submissions awaiting CPU retirement, not a claim that the GPU is still
executing every one.

New native tests queue distinct mixed-alpha/RGBA/A8 captures, erase/free every
source after submit, and compare all retained GPU snapshots against headless,
including reverse-order reads and ring wrap. They author full-ring backpressure,
busy-peer allocation rollback, resize/recovery timeouts and retryable three-slot
close. These require actual macOS execution. The current diagnostic workload
compares serial/ring/ring/serial with batching and pass-state reuse enabled in
both modes. It records slot, in-flight limit and pending counts, checks allocation
stability after warmup, and preserves the same post-block pixel comparisons.
The measurement shows shorter slot-retirement polling, with wait moving mostly
to drawable acquisition. CPU prepare+encode rises in five of six comparisons;
wall-time p95 worsens in two. This is a bounded ownership/overlap improvement,
not a uniform frame-latency or hardware performance result. A subsequent test-only
shared-event gate holds the GPU before upload/render passes so freed sources and
a real finite readback timeout can be tested deterministically. At `29eb9c1`,
both native macOS variants pass 81/81 with that gate: caller output remains
untouched during an actual one-second polling timeout, all slots stay owned,
and CPU signaling permits every captured frame to complete correctly. The final
native log records 100 submitted/100 retired frames. Both variants' 24 mode/block
captures still match the pre-batching baseline. See the ring report for hashes.

A batch starts with at most one destination-reading record (an image/mask or
quantized alpha below 255), followed by opaque records. The next destination
reader starts a new group and gets the same history snapshot as before. Records
and primitives keep their source order. A batch uses the union scissor, while
its fragment shader also checks each record's own pixel scissor. Vertex IDs
select records at `vertexStart / 6`; two triangles per record keep the same
quad coordinates as the previous path. Opaque followers never read history.

The plain C packer/group iterator is testable without an Apple SDK. Host tests
check mixed geometry/bitmaps/alpha against independent headless output at
1/1.25/1.5/2 scale, capacity rollback, output padding and a 10000-record group.
Windows static/shared and Linux Clang ASan/UBSan/leak checks pass. At `c31c92e`,
both native macOS variants pass 81/81 tests, including MSL compilation and the
10000-draw pixel comparison, allocation rollback/reuse, source
release during a pending submission and an unbatched/batched/batched/unbatched
workload. Each block still warms four frames, records 32 and checks GPU pixels.
The comparison holds per-pass state reuse enabled in both modes.

The ordering contract follows Apple's [resource synchronization](https://developer.apple.com/documentation/metal/resource-synchronization)
primitive-order guarantee for render-pass attachments; the shader reads a
separate history texture, not its render attachment. The [draw command](https://developer.apple.com/documentation/metal/mtlrendercommandencoder/drawprimitives(type:vertexstart:vertexcount:))
defines vertex IDs starting at `vertexStart`. Its DocC source and the relevant
MSL packed-layout rules were inspected during implementation. Native pixel
comparisons remain required on other physical target devices.

The [first native batching measurement](../../benchmarks/baselines/metal-c31c92e/README.md)
preserves 768 samples and artifact digests. Draw calls fall from 343/396/366 to
28/30/9, with byte-identical images. CPU preparation offsets some encode gains;
the shared overview has no net prepare+encode median improvement in that first
run, and wall tails do not improve uniformly. A follow-up preflights count and
atlas mappings together, writes records in a second traversal and reuses the
validated group iterator. It preserves capacity rollback and removes repeated
walks/validation. At `bbf3474`, both native variants again pass 81/81 tests.
The [two-pass measurement](../../benchmarks/baselines/metal-bbf3474/README.md)
preserves another 768 samples and 24 byte-identical GPU captures. Within that
run, pooled median CPU prepare+encode decreases by 7.1–59.8% relative to the
unbatched mode. The static overview's small gain overlaps substantial block
drift, and wall-time tails still do not improve uniformly. Separate CI runs
cannot isolate a preparation speedup by subtracting their timings. At that
revision, concurrent upload rings, GPU timestamps and physical Release acceptance
remained open; the subsequent ring work above addresses upload ownership.

## A07 command-buffer GPU timing

The private C diagnostics now retain a fixed journal of the latest 64 committed
submission IDs. Each entry includes its canvas dimensions and frame slot, and
is `PENDING` until retirement inspects a terminal command buffer. Only successful
`MTLCommandBufferStatusCompleted` work contributes `GPUStartTime`/`GPUEndTime`.
Positive finite ordered timestamps produce a `READY` duration; absent or invalid
pairs produce `UNAVAILABLE`. Command-buffer errors produce `FAILED`, without
reusing an earlier frame's duration. Equal positive timestamps are valid zero
elapsed time; zero timestamps mean unavailable. The kernel start/end pair is
validated independently and describes CPU driver scheduling, not GPU execution.

`nx_metal_renderer_get_timing` is a cached, nonblocking lookup. It does not poll,
retire slots or change pacing. Expired/missing IDs return `STATE` with zero
output. `last_completed_timing` identifies the highest collected submission,
which can lag `last_frame`. The bounded `nx_metal_renderer_wait_idle` collects
the ring tail when an application needs it, without waiting for visible display.
Recovery clears the journal only after pending work is retired; IDs continue
monotonically. No asynchronous callback accesses the C renderer.

The GPU duration covers the entire submitted command buffer: bitmap upload,
clear/composition/history copies and the final drawable blit. Test captures can
also include their readback copy. It is not a per-pass breakdown, GPU busy-only
counter, CPU duration or input-to-display latency. Absolute Metal seconds must
not be subtracted from the `CLOCK_MONOTONIC` CPU profile. GPU timestamps are
optional diagnostics; unavailable timing does not disable rendering.

The native workload still measures serial/ring/ring/serial. After its timed
32-frame block, it drains the last pending frames within the finite budget and
joins every GPU result by submission ID and slot. The drain and file I/O stay
outside timed CPU intervals. Raw CSV adds GPU timestamps/state/duration and
independent kernel timestamps/validity/duration; summaries report availability
instead of averaging unavailable zero placeholders. macOS CI now requires a
Metal device and valid GPU timing for every measured sample through
`NEXAUI_REQUIRE_METAL=1` and `NEXAUI_REQUIRE_METAL_TIMING=1`.

The new portable `nexaui_metal_timing` test checks pending/error/unavailable
states, nonfinite/reversed/overflowing pairs, valid zero duration, delayed frame
identity, bounded history wrap and stale completion rejection. Windows MSVC
Debug and Linux Clang ASan/UBSan/leak checks pass. Authored native cases check
gated pending frames with no premature timing, 67 committed frames over journal
wrap, tail collection, retryable timeouts, recovery reset and thread ownership.
At `e66d5af`, all six desktop core variants and Linux sanitizers pass. Both
macOS variants pass 82/82, each with 64/64 valid retained IDs in the native
journal test. All 768 measured workload rows have valid GPU and kernel pairs,
including the ring tail, with exact CPU/GPU submission/slot association. All
24 mode/block images remain byte-identical to the earlier reference. The
[GPU timing baseline](../../benchmarks/baselines/metal-e66d5af/README.md) preserves
raw data, artifact hashes and distributions. Substantial cross-VM variation and
mixed serial/ring outcomes do not establish a uniform performance gain; physical
GPU/Release and display-latency acceptance remain open.

## CPU cost baseline before A06

The private renderer diagnostics now publish a profile only after a command
buffer is committed. It identifies that submission, canvas size and command
scale, and counts input commands, visible draw records, actual draw calls,
render passes, destination-history copies, one-way copy payload bytes, legacy
`set*Bytes` arguments and new draw-record upload/binding costs. A discarded or suspended
submission leaves the previous profile intact. The counts describe authored
API work; they do not estimate physical memory traffic, driver allocation or
GPU execution time.

Four monotonic CPU intervals separate submit-local validation/pipeline/atlas
preparation, encoder construction, layer flush/drawable acquisition, and final
present-copy encoding plus asynchronous commit. The first API/driver preflight
and application frame construction are outside these intervals. A validity
flag distinguishes clock failure from a measured zero. Retirement exposes
cumulative CPU polling time and sample/error counts, including real timeout
polls. No-pending calls and injected no-wait timeout faults add no sample.
These diagnostics do not change serial retirement or resource ownership.

The native NexaCloud smoke now records three fixed workloads: overview at
100%, files at 100%, and compact files at 200%. Each warms four frames and
collects 32 subsequent frames, without readback/file I/O in the timed interval.
Raw `*-macos-profile-*.csv` artifacts retain per-frame stage, retirement and
whole-call wall times, geometry and payload counts. Log summaries use the
ordinary even-sample median and nearest-rank p95 (sorted index 30 of 32).
They also state the Metal device and actual canvas size. A post-workload
GPU/reference capture verifies output after repeated submission. Atlas/upload
allocations must stay stable after warmup; there is no portable time threshold.

At `f870b29`, both native macOS variants pass 80/80 tests, including profile
counts, discarded-submission behavior and the three measured workloads.
The [preserved CSV baseline](../../benchmarks/baselines/metal-f870b29/README.md)
records actual distributions, payloads, environment and artifact digests.
The portable C17 smoke also compiles on Linux with warnings as errors; this
host compile does not execute the Objective-C renderer. A07 GPU timestamp
work remains separate.

The first A06 change binds pipeline, culling, viewport and the two fragment
textures only before the first draw in each render encoder. Restarting a pass
after a history copy invalidates that local state. Scissors, vertices and shape
arguments remain per draw, and ordering/composition stay unchanged. Diagnostics
count nonempty bound passes and the five invariant API bindings. Empty clear
passes bind no pipeline. A test-only switch retains the old per-draw path for
comparison; production has no baseline switch.

The native smoke now measures A/B/B/A blocks for each workload, with A being
per-draw and B per-pass. Each block warms four frames, records 32 rows and
checks its output against headless afterward. This keeps the comparison on
the same device, build and scene; repeated blocks expose some scheduling drift.
At `a364f2d`, both variants pass 80/80 tests. Across 768 measured frames,
the median CPU encode interval drops by 55.2–73.0% in these virtual-device
workloads. The five invariant calls fall from 1715/1980/1830 to 140/150/45.
All mode/block GPU images match headless, each other and the preceding baseline.
The [raw A/B comparison](../../benchmarks/baselines/metal-a364f2d/README.md)
records distributions, artifact digests and limits. Overall frame times and
tails do not improve uniformly; this is a CPU encode result, not an FPS or
physical Release-latency guarantee. Upload rings and draw batching were subsequent
A06 work. The later batching experiment above compares the previous
per-pass unbatched path against the new draw-record path; its extra CSV fields
identify record upload bytes, buffer bindings and the batching mode.

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
A virtual-device result is not physical hardware acceptance.

The first A05 native jobs at `6140315` compiled the Objective-C/C targets but
stopped at MSL compilation: direct `.x/.y` access on packed-vector members was
rejected. The image branch now converts those arguments to regular `float2`
values before accessing components, matching the already tested geometry path.
The embedded source is regenerated. This correction passes in both macOS jobs
at `fb80bd0`: [static](https://github.com/Lulus792/NexaUI/actions/runs/38773070623/job/116553624423)
and [shared](https://github.com/Lulus792/NexaUI/actions/runs/38773070623/job/116553624411)
each passed 79/79 tests. The RGBA/A8, allocation/retirement and complete native
scene tests ran with Metal validation enabled. The three static/shared PPM
captures are byte-identical and were visually reviewed after verifying artifact
digests. Physical GPU performance, Retina/monitor transitions and assistive
acceptance remain open.

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
- [Triple-buffer ownership](https://developer.apple.com/library/archive/documentation/3DDrawing/Conceptual/MTLBestPracticesGuide/TripleBuffering.html)
- [Command-buffer retained resources](https://developer.apple.com/documentation/metal/mtlcommandbuffer/retainedreferences)
- [Shared-event CPU signaling](https://developer.apple.com/documentation/metal/mtlsharedevent/signaledvalue)
- [GPU start time](https://developer.apple.com/documentation/metal/mtlcommandbuffer/gpustarttime)
- [GPU end time](https://developer.apple.com/documentation/metal/mtlcommandbuffer/gpuendtime)
- [CPU driver scheduling time](https://developer.apple.com/documentation/metal/mtlcommandbuffer/kernelstarttime)
- [Successful terminal command-buffer state](https://developer.apple.com/documentation/metal/mtlcommandbufferstatus/completed)
- [Blit encoder buffer/texture copies](https://developer.apple.com/documentation/metal/mtlblitcommandencoder)
