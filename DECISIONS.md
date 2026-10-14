# Decisions

## 2026-10-14 — Embed Vulkan shaders and evaluate triangle coverage explicitly

The initial V04 path uses a Vulkan 1.0 graphics pipeline, a LOAD/STORE render
pass after transfer clear, per-image views/framebuffers and one serialized,
mapped coherent vertex buffer. Growth preserves the previous buffer until a
replacement is allocated, bound and mapped. Pending graphics fences prevent
overwriting upload storage; swapchain retirement also owns the new targets.
Public headers and the installed core acquire no Vulkan or compiler dependency.

Opaque rectangles and triangles are the first accepted command types; other
shapes, clips, nonopaque paints and textures return unsupported before image
acquisition. This is an initial drawing slice, not complete widget rendering.
Authored GLSL and generated SPIR-V 1.0 arrays are both tracked. The development
generator compiles, validates and checks source provenance; normal builds
need neither the compiler nor shader-file access at runtime.

The headless reference samples half-open bounds at pixel centers. Direct
hardware triangles failed a near-center `10.50001` regression, consistent
with Vulkan's finite viewport subpixel precision. Triangles therefore use
snapped bounding quads with explicit top-left edge tests in the fragment
shader. A second regression with million-coordinate offscreen endpoints
exposed cancellation in a plain float determinant. Compensated differences
and split significand products fix both tested failures without optional
float64 shader features. An Fma-based residual did not fix cancellation on
the tested Lavapipe driver; the split-product version did. These tests do not
prove every hardware driver's precision or performance. The native tests
retain both reproducers and varied fractional/scaled triangles.
The Vulkan SPIR-V environment gives GLSL.std.450 Fma precision inherited from
multiply followed by add, rather than a guaranteed fused residual; see
[the precision table](https://docs.vulkan.org/spec/latest/appendices/spirvenv.html#spirvenv-precision-operation).

See the [pipeline contract and validation](docs/concepts/vulkan.md#shape-pipeline),
[Vulkan viewport precision](https://docs.vulkan.org/spec/latest/chapters/vertexpostproc.html#vertexpostproc-viewport)
and [SPIR-V GLSL instructions](https://registry.khronos.org/SPIR-V/specs/unified1/GLSL.std.450.html).

## 2026-10-14 — Retire Vulkan presentation explicitly and retain failed close

V03 makes the private renderer's default policy require
`VK_KHR_swapchain_maintenance1` or `VK_EXT_swapchain_maintenance1`, with its
matching surface-maintenance and feature-query dependencies. Device selection
checks the actual feature bit and prefers KHR when both variants are available.
Older EXT SDK type spellings are binary aliases of KHR structures, so headers
1.3.250 suffice without requiring the SDK that introduced the KHR names.

Each swapchain image owns a present fence and semaphore. An acquisition fence
tracks availability independently of graphics submission. Teardown/recovery
waits for all tracked work before dropping any native resources; a one-second
timeout retains ownership for retry. An acquired image held by a timed-out
present fence finishes on the old swapchain before any resize. Submission
timeouts retain readback buffers. Present OOM does not enqueue its fence,
whereas an out-of-date result still retires the enqueued presentation.

The private F11 destroy callback and close operation now return `nx_result`.
Failed close preserves the backend table/pointer and prevents new rendering or
recovery. Only a successful retry clears ownership, so callers must also keep
the borrowed native window alive. The public installed C header is unchanged.

Explicit compatible and legacy policies preserve the former unextended path.
Their conventional device-idle teardown retains the specification limitation
recorded below; it is not the strict default. Native fault tests exercise
timeouts after real submissions and distinguish enqueued from unenqueued
present errors. Physical GPU loss remains an acceptance gap. See
[the Vulkan contract and primary sources](docs/concepts/vulkan.md).

## 2026-10-14 — Create Vulkan surfaces before selecting presentation queues

V02 extends the private bootstrap with a surface factory called after instance
creation and before adapter selection. Queue selection queries the actual
surface, prefers one graphics/present family and creates both queues when
families differ. The swapchain uses concurrent sharing for separate families.
Surface ownership stays with the device so destruction orders logical device,
surface and instance. Failure tests cover surface creation/support errors and
every NexaUI allocation with the surface alive.

The renderer leases a native window on its owner thread. Wayland continues
event handling and scaling but suppresses its SHM background while leased,
preventing platform resize handlers from replacing Vulkan content. Release
restores the background. All native handles remain in private headers.

The initial clear path uses transfer operations into UNORM swapchain images,
FIFO presentation, one serialized command buffer and one present semaphore
per image. Bounded acquisition/fence waits avoid reusing unfinished command
storage. Readback compares every RGBA8 byte to headless output, while X11
also checks the displayed window. Native tests include resize, synthetic 2×
Wayland scaling, injected cached-target invalidation and explicit recovery.

The original V02 slice retained conventional device-idle swapchain teardown. The Vulkan
specification does not formally guarantee present completion through that
wait in unextended Vulkan; V03 adds presentation fences and an explicit
retirement/fallback policy before release acceptance. Hardware removal and
hardware GPU behavior are still unverified. See [the contract and sources](docs/concepts/vulkan.md).

## 2026-10-14 — Bootstrap Vulkan outside the core library

V01 creates a Vulkan 1.0 instance and selects a graphics-capable adapter in a
private Linux target. It enables `VK_KHR_surface` before the swapchain device
extension, while requiring no display connection. Hardware is preferred over
CPU adapters; failed logical-device creation can fall back, but allocation
failures stop immediately. Driver lists are capped and retried finitely.
NexaUI allocations use caller-provided allocation/deallocation pairs with
their original byte sizes, including when enumeration shrinks.

Dispatch is immutable and copied per object, allowing deterministic failure
tests without a process-wide production override. The creating thread owns
every operation and borrowed native handle. No Vulkan types or links enter
the public C API or installed core package. Linux builds can disable the
private target with `NEXAUI_BUILD_VULKAN=OFF`.

The local native test submits actual command buffers on Mesa Lavapipe under
Khronos validation, also with Clang ASan/UBSan and leak detection enabled.
This proves device/queue lifetime on a CPU Vulkan implementation, not window
presentation, hardware behavior or GPU performance. V02 will select for
surface presentation compatibility before creating swap chains; V03 owns
in-flight resource and recovery policy. See [the contract](docs/concepts/vulkan.md).

The first clean parallel Linux build also exposed two independent test
targets claiming the same generated Wayland server headers. A single custom
target now owns that command, and both tests depend on it, following the
[CMake shared-output rule](https://cmake.org/cmake/help/latest/command/add_custom_command.html#example-generating-files-for-multiple-targets).

## 2026-10-14 — Receive X11 file drops through Xdnd selections

The X11 target advertises Xdnd version 5, accepts a `text/uri-list` type from inline types or `XdndTypeList`, and reports a copy action with `XdndStatus`. After `XdndDrop`, it converts `XdndSelection` using the source timestamp, handles direct properties or `INCR` chunks, parses local file URIs into the shared drop queue and sends `XdndFinished` only after the transfer. The parser accepts empty, `localhost` and current-machine authorities while rejecting other hosts. A five-second deadline and 32 MiB cap bound the wait; conversion failures send a failed finish. A private Xvfb CTest case uses a separate Xlib source to exercise direct transfer, a split `INCR` stream, extended type lists, local-host URIs and rejection. The protocol sequence follows the [freedesktop Xdnd specification](https://wiki.freedesktop.org/www/Specifications/XDND/). Physical desktop file-manager drag acceptance remains unverified.

## 2026-10-14 — Receive Wayland file drops through URI-list offers

The Wayland data device accepts `text/uri-list` when a drag enters its surface. Its drop callback records the offer and coordinates; the next poll reads the pipe after event dispatch to avoid a nested protocol read inside the callback. The common parser keeps only local `file:` URIs, decodes percent escapes, validates UTF-8 and atomically appends a batch to the existing file-drop queue. A 32 MiB receive cap and five-second deadline follow the clipboard pipe policy. Version-1 data-device offers use `accept` and `receive` without a later `finish` request. Parser and native queue tests cover mixed valid/invalid URIs, coordinates, caller-buffer behavior and allocation rollback. A physical drag from another desktop client remains to be exercised. Protocol and format references: [Wayland data offers](https://wayland.freedesktop.org/docs/html/apa.html), [text/uri-list](https://www.rfc-editor.org/rfc/rfc2483.html), [file URI scheme](https://www.rfc-editor.org/rfc/rfc8089.html).

## 2026-10-14 — Serve the Wayland clipboard through data-device pipes

Each Wayland window binds a version-1 data device for its seat. A pointer-button or keyboard-key press provides the serial required when it publishes a UTF-8 selection; before such input, set returns a state error. The source advertises UTF-8 plain text, and the receiver chooses the best compatible MIME offer. A read pumps every Wayland window on that UI thread, which lets one NexaUI window receive from another without blocking its source callback. Source writes use nonblocking pipes, four concurrent-write slots and a stable source copy that survives cancellation until the last writer closes. Receive buffers are limited to 32 MiB with a five-second deadline. A private WSLg native test exercises the actual send and read callbacks with a transfer larger than a pipe, cancellation during that transfer, sizing and malformed text; direct cross-client selection behavior still needs interactive compositor acceptance.

## 2026-10-14 — Serve the X11 clipboard through selection events

Each X11 window can own the `CLIPBOARD` selection with a copied UTF-8 payload. The backend replies to `TARGETS` and `UTF8_STRING` requests, discards the payload on `SelectionClear`, and synchronously converts foreign UTF-8 selections for the existing caller-buffer API. A thread-local list lets a waiting requestor pump other NexaUI windows on that UI thread, so copy/paste between two windows does not deadlock. Direct properties are used below the server request limit; larger selections use `INCR` in both directions. Incoming transfers share a five-second deadline. Each outgoing transfer holds its own payload copy until the requestor consumes the final zero-length property, so a selection owner change cannot alter bytes already promised. The 32 MiB text cap and four concurrent outgoing transfers bound memory use; inactive transfers are released after 30 seconds of polling. Native tests use a private Xvfb display, separate Xlib requestors, a chunked `INCR` owner, allocation failure and sanitizer instrumentation. Tests on an interactive display leave its clipboard alone.

## 2026-10-14 — Follow entered Wayland outputs for integer backing scale

The Wayland window tracks each bound output and the surface's enter/leave events. It uses the largest entered output scale so a window spanning displays keeps sharp content on the denser one. The xdg-shell size and input coordinates remain logical; each scale change emits a resize event carrying the new scale. The temporary SHM background and self-drawn cursor use physical buffers scaled with `wl_surface.set_buffer_scale` in the same commit. The cursor hotspot remains in logical surface coordinates. A native callback test verifies a 1× to 2× to 1× transition and buffer dimensions under WSLg. Fractional scale protocols and visual review on a physical mixed-DPI setup remain pending.

## 2026-10-14 — Bound Wayland repeats and draw cursor images in NexaUI

Wayland supplies a repeat rate and delay but does not repeat keys at the bound keyboard protocol version. NexaUI schedules repeat events against a monotonic clock, caps the rate at 1,000 per second and emits at most eight overdue repeats per poll to protect frame work after a stall. Release, focus loss, keymap replacement and keyboard removal cancel the timer. A pure timing test and a native integration test with injected protocol callbacks verify the event stream; direct physical keyboard acceptance is still separate work. For cursors, NexaUI rasterizes eleven distinct 32×32 shapes to ARGB shared-memory buffers. This avoids assuming an installed desktop cursor theme and keeps cursor pixels under the framework's control. The native test exercises the buffer submission path with injected pointer entry, while a rendered cursor sheet was inspected for shape and hotspot placement. Physical pointer interaction and multi-scale cursor review remain open.

## 2026-10-14 — Select native Wayland first and retain X11 as a runtime fallback

The Linux public window API now dispatches to independent per-window Wayland or X11 backend objects. It tries Wayland when `WAYLAND_DISPLAY` is set and falls back to X11 only if Wayland reports `NX_ERROR_PLATFORM`, including connection or protocol failure. `NEXAUI_PLATFORM=wayland` or `x11` forces one adapter for testing and deployments. Other errors, including allocation failure, do not trigger a fallback that could hide the original failure. The initial Wayland path follows xdg-shell's empty first commit, acknowledges configure before attaching a shared-memory buffer, and uses libxkbcommon solely to parse the compositor-provided XKB keymap. The visible buffer is a temporary background until a Vulkan renderer owns the surface. One connection per window preserves isolated queues and owner-thread checks, at the cost of repeated registry/keymap setup. The later cursor, repeat and output-scale decisions above extend that path; composition, fractional scale and IME remain separate work; the clipboard and file-drop decisions above extend the data-device path.

## 2026-10-14 — Give each X11 window its own display connection

The initial Linux adapter creates one Xlib `Display` connection per window on its creation thread. This keeps the shared event queue and per-window routing contract explicit: polling a window reads only its connection, and a caller with multiple windows polls each one. The native handle stays private to platform and renderer code. Window dimensions and focus are updated from delivered events rather than queried over the X11 connection on every frame. This first adapter runs under X11 and Xwayland; the later Wayland selection decision above supersedes its earlier planned status. X11 text input currently covers basic key symbols and Unicode fallback; later clipboard and Xdnd decisions above supersede the original transfer backlog. IME composition and monitor scale remain pending.

## 2026-10-14 — Password fields mask the presentation path

The password option is a single-line text-field policy. It shapes one `*` per extended grapheme cluster and maps masked caret positions back to source byte boundaries. This keeps visual width independent of UTF-8 encoding length and preserves pointer editing. The semantic node exposes `NX_SEM_PROTECTED` with an empty value; Ctrl+C and Ctrl+X do not transfer or delete plaintext, while paste remains enabled. A password descriptor now requires `nx_text_field_create_password`, which owns a no-history sensitive buffer. That buffer and paste scratch overwrite NexaUI-owned plaintext before release, including failed edits. This is a bounded, best-effort in-process cleanup: caller input and returned views, OS event queues and IME windows, paging and dumps are outside it. The application still receives the real text and validation callback; native accessibility acceptance remains separate work.

## 2026-09-28 — Start with a headless command path

The first executable slice is a C17 frame command stream and a deterministic software RGBA8 renderer. This validates ownership, memory failure behavior, ordering, clipping and packaging before native integration. D3D12, Metal and Vulkan remain the required display backends in MASTER.md. The headless path is a test renderer, not a replacement for them.

## 2026-09-28 — Pre-1.0 API

The ABI is intentionally versioned `0.x`; source and ABI changes are permitted before 1.0 and must be recorded in CHANGELOG.md. Public calls return `nx_result` for recoverable errors. Public objects are opaque where lifetime matters. There is no hidden process-wide UI context.

## 2026-10-14 — Filesystem drops use a separate per-window queue

Win32 `WM_DROPFILES` carries a variable number of native UTF-16 paths, while `nx_event` is a copyable fixed-size input value consumed by a context. NexaUI converts each complete drop batch to owned UTF-8 paths before queuing it on the receiving window. Applications query and copy paths into caller storage through `nx_window_next_file_drop`; an insufficient buffer does not consume the entry. Batch index/count and logical client coordinates remain attached to each path. The API does not claim ordering against the normalized input queue. Conversion or allocation failure discards only the new batch, keeps earlier queued paths and makes polling report a sticky error. This first Win32 route covers Shell filesystem paths; OLE virtual-file payloads remain separate work.

## 2026-10-14 — Use the Windows-managed IME interface first

The Win32 backend keeps the default IME composition and candidate windows managed by Windows. The text field passes its successfully drawn caret rectangle to `nx_window_set_ime_rect`, which converts logical coordinates to client pixels and positions the IMM windows. `WM_IME_CHAR` flows through the default window procedure into `WM_CHAR`, so committed Unicode enters the existing scalar event path without a second insertion path. This avoids duplicate commits while providing a positioned native composition surface. NexaUI still needs framework-owned preedit/clauses for full T17 support, plus acceptance with installed international IMEs and accessibility review. The local native test verifies IMM window coordinates and a synthetic Unicode commit, not a user-driven IME session.

## 2026-09-28 — Explicit deferred scope

The current milestone has a Win32 window and normalized pointer/key/text events. It does not implement Cocoa, Wayland/X11, interactive widgets, IME composition, font parsing, Unicode shaping, accessibility bridges or GPU rendering. Each remains a named task with an acceptance gate in ROADMAP.md.

## 2026-10-13 — Two-slot simulation snapshot exchange

S01 uses one producer, one consumer and at most one outstanding UI view. The producer publishes an already-built immutable allocation; the exchange copies only pointer metadata. A current slot without a view can be replaced repeatedly, while a held view pins the other slot. Disposal happens outside the metadata lock, possibly on either thread. Shutdown requires producer join and view release. This keeps UI frame work outside synchronization and limits retained slot payloads to two; payloads inside concurrent disposal callbacks can briefly exist beyond those slots. MSVC C17 on the local Windows toolchain failed a `<stdatomic.h>` compile probe, so private Win32 SRW and POSIX mutex adapters provide the lock without exposing native types in the C ABI. Multi-producer publication and arbitrary multi-consumer views remain outside this contract.

## 2026-10-13 — Reserved completion capacity for UI commands

S02 counts every accepted command as outstanding until its completion is taken. A fixed-capacity ticket table tracks queued, taken and completed states; command and completion rings are preallocated. This makes completion insertion possible for every taken ticket even when the UI pauses, while new UI commands receive explicit backpressure. Workers can complete out of order. Posting transfers payload ownership only on success; taking transfers it to the caller. The channel reuses the private S01 metadata lock and invokes disposers outside it. Ticket lookup is currently linear in channel capacity; this is a known scaling limit for very large capacities.

## 2026-10-13 — Borrowed monotonic series and pixel envelopes

S03 uses a callback-backed, immutable data view so AoS, SoA and simulation snapshots can be sampled without a framework-owned copy. Finite, nondecreasing x coordinates make the visible interval searchable in logarithmic time. The decimator scans that interval and emits one caller-owned envelope per occupied pixel column, with source-indexed first/last and y-extreme points. This keeps narrow view work proportional to visible data and retains short spikes for S04. The application guarantees global ordering; only sampled and visible points are validated during a frame. NaN gaps and a persistent multiresolution index remain later extensions. A full-range view still incurs a linear scan.

## 2026-10-13 — Caller-owned real-time plot state

S04 composes S03 envelopes with the ordinary frame command, theme, hit-region and semantic APIs. The application owns ranges, scratch storage and the cursor between frames; painting updates the cursor only after all commands succeed. This keeps plot data lifetime tied to the caller's immutable snapshot and permits frame retry without partial widget state. The first plot supports a single linear-scale series and optional numeric labels. Multi-series legends, axis formatting and native accessibility actions remain separate work.

## 2026-10-13 — Visible-row simulation dashboards

S05 uses borrowed metric and profiler arrays with application-owned `first_row` instead of copying or caching every record in a UI context. Geometry and row height bound each call to visible rows, capped at 4096. Stable item IDs form semantic children, while measured values and warnings remain textual as well as graphical. Panels roll back their semantic and draw work on an invalid visible record or allocation failure. Offscreen records are intentionally not validated during frame construction; the producing application owns full-data validation before publication. Actual text rendering uses supplied fonts and may allocate per frame; the no-font update stress test does not establish a total UI-frame latency target.

## 2026-10-13 — Inspector edits as proposals

S06 composes the U09 hierarchy primitives with typed property rows. Selection and expansion stay in UI-owned state. Each property row reads an immutable application value, returns an edit proposal or editor request, and leaves acceptance to the caller. This allows simulation tools to turn changes into S02 commands while S01 snapshots remain authoritative. Numeric and text editing surfaces are application-owned for now; the row provides keyboard stepping for numbers and activation for an external editor. A successful proposal is shown and exposed semantically in the same frame, but the next frame uses the caller's supplied value.

## 2026-10-13 — Explicit software viewport lease

The first S07 surface contract uses an application-owned RGBA8 image, with resize and acquire callbacks and an explicit completion call. The image remains borrowed after frame construction until the renderer has consumed it; one lease per surface prevents an accidental resize or release while commands still refer to its pixels. Zero-size bounds suspend the surface. This gives software and headless producers a usable 3D preview path and makes lifetime testable now. Direct native GPU texture import cannot be inferred from a CPU pointer and remains a separate backend contract once D3D12, Metal and Vulkan renderer interfaces exist.

## 2026-10-13 — Shared command preflight and renderer state

F11 uses one private command validator for both headless and future native backends. The driver receives a whole frame in paint order rather than callbacks per primitive, so each backend can batch compatible commands without reordering translucent content. Target dimensions and scale are cached; zero-size and temporary suspension skip submission, while device loss requires explicit recovery. The submit callback must consume or stage every borrowed bitmap before returning ready, allowing UI or viewport leases to complete while GPU work continues on backend-owned storage. Native resource allocation, texture caching and GPU fences remain entirely inside backend implementations.

## 2026-10-13 — First D3D12 clear/present path

D01 lives in a separate Windows renderer target so the platform-neutral core has no D3D12 headers or link dependency. A private window adapter lends its HWND only on the owner thread. The renderer tries the default adapter, then DXGI WARP, and owns a two-buffer flip-discard swap chain, render-target views, one command allocator/list and a fence. A fence wait after every submitted frame makes this first slice safe but serial; D06 must introduce bounded frames in flight. The F11 adapter accepts empty command streams and rejects unsupported drawing commands explicitly. Native command pipelines and pixel comparisons remain separate tasks.

## 2026-10-13 — Recreate D3D12 resources after device loss

D02 treats removed/reset/hung device results and the fence's `UINT64_MAX` sentinel as device loss. The direct renderer stops submitting; the F11 adapter reports `NX_RENDER_DEVICE_LOST` and requires explicit recovery. Recovery releases the old swap chain and every device-owned object, then creates a new complete set, preserving the logical renderer and submission counter. A failed rebuild leaves the renderer lost for a later retry. The local smoke test uses `ID3D12Device5::RemoveDevice` where supported to exercise direct and F11 recovery, then injects one partial rebuild failure to verify cleanup and retry. The single allocator remains serialized by a fence; descriptor pooling for textures and bounded frames in flight are D06 work.

## 2026-10-13 — D3D12 geometry with a readback comparison

D03 compiles embedded HLSL on the first geometry submission and creates a reusable triangle PSO with an empty root signature. Rectangles expand to two triangles; vertices carry screen positions converted from logical coordinates using the target scale and a quantized RGBA8 source color. One upload resource per submitted frame remains alive until its fence completes. This preserves command order while keeping the first pipeline small; D06 must replace per-frame allocations with a bounded upload ring. A private readback hook copies the back buffer before present and compares every byte with headless output at 1×, 2× and with layered half-alpha shapes over an opaque clear. A first unquantized alpha test differed by one byte; rounding source colors to RGBA8 before shading matched the tested reference.

## 2026-10-13 — D3D12 clip and straight-alpha parity

D04 maps nested logical clip rectangles to pixel-center scissor rectangles and intersects them on the GPU command path. Fixed-function blending does not give the headless renderer's normalized RGB on translucent destinations, so each partially transparent primitive copies the current back buffer to a shader-readable texture and runs the same integer RGBA8 blend formula in a separate pixel shader. Readback checks match every byte for nested clips at 1×/2× and for layered half-alpha shapes on transparent and partially transparent clear colors. This per-primitive full-buffer copy is expensive; D06 should replace it with a more efficient compositing strategy after D05 adds texture uploads.

## 2026-10-14 — D3D12 image and glyph-mask uploads

D05 stages each borrowed RGBA8 or A8 bitmap into a row-pitch-aligned upload buffer before submission, then copies it to a default-heap texture. A shader-visible two-SRV heap per bitmap holds the previous back buffer and that bitmap; the pixel shader performs nearest-texel sampling, integer tint multiplication and the same straight-alpha blend as headless. Textures, uploads and descriptors live until the frame fence completes. This direct per-command allocation keeps descriptor contents immutable while several bitmaps share one command list and makes borrowing safe. It is costly; D06 needs reusable storage, descriptor management and batching. GPU readback compares images and a rasterized A8 glyph mask with headless at 1×/2×, including source row padding, clipping, source-data updates, resize and device recreation.

## 2026-10-14 — Reuse D3D12 uploads and batch disjoint opaque shapes

D06 deduplicates borrowed bitmap views by pointer, dimensions, stride and format within each frame, while uploading contents again on later frames. Distinct views share one growable row-aligned upload buffer and one growable shader-visible descriptor heap. A texture pool is capped at 256 entries and 64 MiB; over-budget frame textures remain transient. Reused textures transition from shader resource to copy destination before the next upload. Vertex data goes into a growable ring that wraps only after the current frame fence completes. The batcher groups at most 64 adjacent opaque shapes when their bounds do not overlap; clips, translucent draws, bitmaps and overlaps flush it to preserve paint order. GPU readback matches headless for 1,000 opaque shapes and 128/260 distinct images, and a local end-to-end benchmark records draw/upload counts and timings. The single command allocator and upload buffers still require a fence wait after every Present. Bounded frames in flight and a cheaper translucent composite path remain to be implemented.

## 2026-10-14 — Bound opaque D3D12 submissions to two frames

The flip-discard swap chain now has three back buffers. Each slot owns a command allocator and vertex upload ring, with the fence value of its last submission. Before resetting a slot, the renderer waits for that slot's fence; before submitting another opaque or empty frame, it waits for the older of the last two submissions. The command list itself is reset immediately after execution, as allowed by the D3D12 contract. Bitmap and translucent frames still wait for all preceding work and wait for their own completion, because the upload buffer, descriptor heap and compositing texture are shared. Resize, readback and destruction also wait. This bounds in-flight opaque work without letting the GPU read overwritten vertices or commands. The local hidden-window benchmark remains near its VSync-limited median; GPU timestamps and a less expensive translucent path are separate work.

## 2026-10-14 — Measure D3D12 GPU phases and copy clipped composite regions

D07 records start, post-texture-upload and post-render GPU timestamps in three query slots per swap-chain buffer, resolves them to slot-owned readback buffers and reads them only after the corresponding fence completes. The private info structure exposes frequency, latest completed frame ID, upload/render/total milliseconds and copied texels. Query creation is optional so diagnostics do not prevent rendering. D06's straight-alpha shader still requires the previous target color, but each translucent shape or bitmap now copies only its conservatively expanded bounds intersected with the active clip. The full-window copy remains a fallback for empty calculated bounds. Pixel readback tests retain exact headless parity. In one local 640×480 Release run, 128 half-alpha shapes copied 79,389 texels rather than 39,321,600 for 128 full-window copies; one latest-completed GPU-frame sample changed from 1.626 ms before region copying to 1.124 ms after it. These single samples are diagnostic evidence, not a statistical speedup claim. Per-element barriers and draws remain an optimization target.

D06 now batches up to 64 adjacent translucent rectangles and triangles when their logical bounds do not overlap. One copy of the union of their clipped, expanded physical bounds supplies the previous-color shader for one draw. Overlap, clip changes, opaque draws and bitmaps flush the batch so paint order and straight-alpha semantics remain intact. Exact pixel comparisons cover 100 disjoint rectangles, mixed triangles and clips, and overlapping shapes. On the local 640×480 Release benchmark, 128 half-alpha rectangles took 11 draws and copied 2,682,326 texels; 21 distinct completed GPU frames measured 0.141–0.152 ms total (0.143 ms median). The union can transfer substantially more texels than individual shape copies, so this policy favors fewer barriers and draws for this scene rather than minimum copy area. Bitmap compositing remains per element.

D08 first exercises the native window, F11 driver and D3D12 renderer together. A synthetic `WM_DPICHANGED` updates the Win32 adapter to 1.5×, which changes logical size while the test passes actual client pixels and the new scale to F11. It verifies the emitted resize event, a zero-size target that submits no GPU frame, scaled pixel parity, a physical swap-chain resize, and forced device removal/recovery at the resized scale when available. The synthetic message isolates the event and renderer contract; it is not evidence of a real monitor DPI transition. The visible example attempts `nx_renderer_recover` after a device-loss outcome so the recovery path is represented in normal application control flow.

D03 extends native geometry to stroked rectangles and round-capped lines without changing the public command format. Strokes expand into four non-overlapping quads, or one full quad when there is no interior, so translucent corners receive one blend. Lines use one bounding quad and a dedicated pixel shader with segment endpoints and half-width in physical coordinates. Its per-pixel distance test discards outside pixels and reuses the integer straight-alpha formula over the copied previous target. Unlike non-overlapping strokes, lines are not batched and each line copies its bounds; correctness and resource lifetime take priority until a measured optimization is justified. Exact GPU/headless comparisons cover 1×/2× strokes and lines, a zero-length line, and 48 varied overlapping fractional lines on a partially transparent clear.

The same shape shader now handles filled rounded rectangles with physical bounds and a radius capped to half the smaller dimension. It discards pixels whose centers lie beyond the corner arc and uses the previous-color composite path. A zero radius behaves as a filled rectangle; each rounded rectangle currently has its own copy and draw. Exact GPU/headless comparisons at 1×/2× cover zero and oversized radius, and 32 varied overlapping rounded rectangles. All current backend-neutral drawing command types now have a D3D12 implementation; further performance work remains separate.

## 2026-10-14 — Optional Wayland text-input-v1 with explicit activation

The available WSLg compositor advertises `zwp_text_input_manager_v1` but no text-input-v3 manager. NexaUI now binds v1 when present and exposes `nx_window_set_ime_enabled` so an application activates text input only while its editor is focused. The existing `nx_window_set_ime_rect` supplies the logical cursor rectangle. Committed valid UTF-8 becomes scalar text events; XKB text is suppressed only after the text-input object enters, and resumes after deactivation or keyboard focus loss. Serial checks reject replies from an earlier activation. A compositor without v1 continues to use XKB. This is a bounded interoperability step: preedit rendering, surrounding-text edits, content-purpose selection, text-input-v3 and real installed-IME acceptance remain separate work. The WSLg native test and protocol trace confirm bind, activate, cursor state and deactivate requests; a synthetic callback checks text decoding, stale replies and fallback behavior.

## 2026-10-14 — Reconcile text-field focus with Wayland IME at frame end

The text-field widget now records its focused ordinary-field IME target only after a successful paint. `nx_end_frame` applies the final focus decision once: it enables the chosen Wayland window and disables the prior target when focus moves away or the field is omitted. This avoids paint-order-dependent deactivation when two text fields are drawn in different orders. Password fields are excluded until the adapter can request sensitive content handling. The context retains a borrowed native window while it has enabled IME, so callers must end a frame that releases the target or destroy the context before destroying that window. A native WSLg test uses two rendered fields, reverses their paint order during a focus switch, then removes them, focuses a password field and destroys an active context while verifying activation state; the fixture font is shared with the headless text-field tests.

## 2026-10-14 — Prefer Wayland text-input-v3 with v1 fallback

The Wayland adapter binds text-input-v3 when the compositor advertises it and uses the existing v1 path otherwise. It enables v3 only after the protocol enters the window surface, commits cursor and content state with a client serial, buffers `commit_string` until `done`, and disables it when editor focus ends. A private Wayland server test exercises the actual v3 wire protocol and verifies that text stays buffered until `done`; the WSLg native test continues to exercise v1. Preedit presentation, surrounding-text deletion, password content hints and installed-IME acceptance remain separate work.

## 2026-10-14 — Show Wayland v3 preedit without changing committed text

The v3 adapter retains a bounded UTF-8 preedit string and byte-based cursor offsets only after `done`. `nx_window_get_ime_preedit` copies this window-owned state for custom editors. A focused ordinary text field draws the composition in place of its current selection, underlines it, and keeps its committed draft and semantic value unchanged. Preedit changes queue a payload-free `NX_EVENT_IME_PREEDIT_CHANGED` so event-driven applications can redraw and query the current text. Deactivation and focus leave discard both pending and visible preedit. Password fields do not query or display it. The private compositor test checks that preedit stays hidden before `done`, appears in drawing commands afterwards, and leaves the draft intact. Surrounding-text deletion, v1 preedit, cross-backend composition and installed-IME acceptance remain open.

## 2026-10-14 — Bound Wayland v3 surrounding text to the active editor state

The focused ordinary text field advertises at most 4000 UTF-8 bytes around the cursor and complete selection. It sends empty context with its editor identity if the selection alone exceeds that limit or the draft contains NUL. The v3 adapter associates each snapshot with the editor ID, a revision of the text and selection, and the client commit serial. It queues a compositor deletion before committed text only when the corresponding `done` serial and advertised byte bounds are valid. The widget rechecks the ID and revision before applying deletion and committed text through the text buffer's undoable edit path. A changed editor re-enables v3 so the compositor gets a fresh initial state; deactivation clears copied text. Password fields do not advertise surrounding text. The private server test checks UTF-8 byte positions and event order; installed-IME behavior and cross-backend composition remain open.

## 2026-10-14 — Present Wayland v1 preedit through the existing text-field path

The v1 fallback consumes `preedit_cursor` as a byte offset for the following `preedit_string` and publishes the validated composition immediately when its serial matches the active editor state. Without a cursor event, the caret defaults to the text end; a negative cursor hides it. The shared preedit copy-out API and transient text-field painting now work for v1 as well as v3. `commit_string`, leave and deactivation clear the copied composition and queue redraw. Styling spans and the v1 fallback commit string on reset are not yet exposed. A private v1 compositor test checks Unicode text, byte offsets and stale serials; a WSLg test checks visual composition; installed-IME acceptance remains open.

## 2026-10-14 — Keep Wayland v1 cursor-relative deletion distinct from v3

Wayland v1 `delete_surrounding_text` supplies a signed byte index relative to the advertised cursor and a byte length. The adapter stores that request until the following `commit_string`, validates the range against its UTF-8 surrounding-text snapshot, and queues `NX_EVENT_IME_DELETE_RANGE` before tagged committed text. The text field rechecks the editor ID and revision; `nx_text_buffer_delete_relative` validates the same range against the current buffer and applies it as an undoable edit. V3 continues to use selection-relative before/after counts. Withdrawal, malformed ranges and stale revisions suppress the batch. A private v1 server checks the wire exchange and a focused widget edit; text-buffer tests cover UTF-8 boundaries and undo. A real installed IME remains to be accepted.

## 2026-10-14 — Draw Windows IMM composition in the focused text field

The Win32 adapter copies `GCS_COMPSTR` from the window's input context, converts UTF-16 and its character cursor position to UTF-8 byte offsets, and exposes the result through the existing preedit copy-out API. An ordinary focused field enables this presentation at frame end, so password and unfocused fields leave the default IME composition window available. `WM_IME_SETCONTEXT` hides only the default composition window while self-drawing; candidate UI remains native. Start, end, result and focus loss clear copied preedit and queue redraw. The result continues through `DefWindowProc` and the existing `WM_CHAR` commit path. The native test verifies surrogate conversion, invalid cursor rejection, transient field drawing, cancellation, focus withdrawal and committed Unicode. Interaction with an installed non-Latin IME remains an acceptance gap.

## 2026-10-14 — Use a separate X11 XIM callback context for focused editors

The X11 adapter keeps its original XIM context for other fields and creates a second context only if the input method advertises `XIMPreeditCallbacks` with a supported status style. `nx_window_set_ime_enabled` switches the focused context for an ordinary editor; it returns unsupported when the callback style is unavailable. Start, draw, caret and done callbacks maintain a bounded UTF-8 preedit string, map XIM character positions to byte offsets, and queue redraw when the visible state changes. Committed text still comes from `Xutf8LookupString`; focus loss or editor deactivation clears preedit. A private Xvfb test exercises wide and locale multibyte conversion, character-range replacement, malformed inputs, copy-out and clearing. An installed XIM server and real IME interaction remain to be accepted.

## 2026-10-14 — Activate Wayland IME for password fields without surrounding text

`nx_window_set_ime_purpose` selects normal or password input before activation. Wayland v1/v3 send the protocol's sensitive-data hint and password purpose for the latter; Windows and X11 decline password purpose so their existing basic/native path remains. The widget advertises only an empty surrounding-text snapshot carrying editor identity and revision, rejects compositor deletions against it, and masks uncommitted preedit before painting. Accepted commits enter the sensitive text buffer; NexaUI-owned preedit, pending commit and composed drawing scratch are wiped when released. The private v1/v3 server tests verify content hints, empty context and return to normal; a WSLg callback test checks masked composition and committed Unicode. The hint is advisory to the compositor and does not control memory or presentation owned by an installed IME, which still needs acceptance.
