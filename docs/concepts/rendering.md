# Rendering boundary

The core records backend-neutral commands in paint order. The headless RGBA8 renderer supports filled and stroked rectangles, rounded rectangles, round-capped lines, filled triangles, RGBA8 images, A8 glyph masks and nested rectangular clips. All coordinates are logical pixels. Strokes lie inside their rectangle; corner radii are clamped to half the smaller rectangle dimension. A zero-length line renders as a disk. Triangles use a top-left edge rule, so adjacent triangles sharing an edge do not double-blend that edge. Geometry is sampled at pixel centers without antialiasing. Bitmap scaling uses nearest-neighbor sampling. Renderer backends will consume the same command stream through a private interface. Widgets and layout may not contain D3D12, Metal or Vulkan branches.

`nx_draw_bitmap` borrows its source pixels. The caller must keep them readable until headless rendering or native submission has completed, including after `nx_end_frame`. RGBA8 pixels use straight alpha, and their channels are multiplied by the tint color. A8 bytes provide coverage for a solid tint, which is the path intended for rasterized glyph masks. A bitmap view may point into an atlas with a stride larger than its visible row. Commands and output pixels must not overlap. Font rasterization produces A8 masks through the [text subsystem](text.md). A general persistent GPU texture API remains planned.

The first [D3D12 path](d3d12.md) owns a Windows device, queue and swap chain, draws all current core shape/bitmap commands, and rebuilds those objects after device removal. The initial [Vulkan path](vulkan.md) draws rectangles, inside strokes, rounded fills, round-capped lines and triangles on Wayland/X11 with nested clips and straight-alpha composition; its owned atlas supplies RGBA8 images and A8 glyph masks. Metal remains planned. A complete native renderer will own upload rings and pipelines for the remaining command types. A renderer may batch adjacent compatible commands, but may not reorder translucent primitives across paint-order boundaries or change clipping results. Resource handles are scoped to a renderer instance and cannot be reused after teardown. All GPU timings must identify the hardware, driver and configuration used.

The headless path is the reference for command order and basic compositing. It is deliberately limited to integer RGBA8 blending and is not a substitute for native color-managed display or GPU performance tests. The [private renderer driver](renderer-backend.md) shares command preflight with the headless path and defines resize, submit, suspension and device-loss behavior. D3D12 currently uses it for empty frames, filled rectangles and triangles. Its readback test compares those primitives with headless RGBA8 at 1× and 2× and checks layered alpha over an opaque clear. Future commands include layers and custom surfaces; each addition needs a headless golden test and a native comparison.

`nx_headless_render_scaled` maps logical command geometry into a caller-owned
physical-pixel RGBA8 buffer at a finite scale from 0.125 to 8. Bitmap source
bytes remain borrowed and unchanged; their destination geometry scales with the
other commands. Text masks must already be generated at the desired density.
For scales other than one, a temporary default-allocator command copy is used;
invalid or unrepresentable geometry and allocation failure leave the output
unchanged. This rasterization call alone does not present a window.
This symbol is additive and is absent from older binaries that may carry the
same pre-1.0 version; consumers must pin a revision containing it.

On Linux, `<nexaui/software.h>` and `NexaUI::NexaUI` also provide a Wayland
software view. It leases an existing window, rasterizes at the observed scale,
converts straight RGBA to native premultiplied ARGB8888, and submits a shared
memory buffer. Three outstanding buffers are permitted, each at most 128 MiB;
BUSY requires polling the window and retrying, not spinning. The view and its
window must be used on their owner thread. Buffers are retained by the window
until compositor release or window destruction, including after view close.
Shared memory uses an unlinked temporary file under `/tmp`. The view reports
submission, not physical presentation; a failed connection after commit has an
unknown outcome. X11 returns unsupported, and no automatic renderer fallback
is selected. Performance, physical displays and production compositor behavior
are separate acceptance gates.

The initial [custom viewport contract](viewport.md) imports an application-owned RGBA8 image as a lease in this command stream. Its resize and completion calls make pixel dimensions and memory lifetime explicit. Direct GPU texture import remains planned alongside the native renderer backends.
