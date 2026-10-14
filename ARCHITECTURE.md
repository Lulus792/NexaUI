# Architecture

The Linux Vulkan renderer is a separate private `nexaui_renderer_vulkan`
target. It owns its instance, surface-compatible adapter, graphics/present
queues and an initial Wayland/X11 clear/present swapchain, without adding
Vulkan to the core or installed package. A per-window renderer lease prevents
competing renderers and suppresses Wayland SHM background commits while Vulkan
owns presentation. Selection, allocator, synchronization and owner-thread
rules are documented in [the Vulkan contract](docs/concepts/vulkan.md).
Strict retirement negotiates KHR/EXT present fences and retains resources on
bounded wait failure. The private driver preserves backend ownership across a
failed close. The SPIR-V pipeline draws rectangles, inside strokes, rounded fills,
round-capped lines and triangles with nested clips and integer straight-alpha
composition. An owned canvas/history pair preserves paint order and copies the
completed image to the swapchain. Pixel-center coverage is checked against
headless output. Bitmap/glyph drawing and broader integration remain V06–V09 work;
hardware-GPU and device-removal acceptance remain open.

NexaUI exposes a C17 ABI. Each `nx_context` owns its frame command buffer, hierarchical ID scope, allocator, and counters. No context state is global. A context is mutated by one UI thread at a time; callers may own different contexts on different threads. Command pointers returned by `nx_frame_commands` are borrowed and remain valid until the next `nx_begin_frame` or context destruction. The caller must finish any renderer use before either action.

```text
application state / immutable simulation snapshot
                    |
                    v
             C17 frame API  -> retained hit regions (current)
                            -> accessibility tree (current)
                            -> full widget tree (planned)
                    ^
                    |
          measure/arrange layout tree (current)
                    |
              command stream
                    |
        +-----------+-----------+
        |                       |
 headless RGBA8 (current)    D3D12 shapes/bitmaps (current)
                                |
                         other GPU commands/backends (planned)
```

The core has no dependency on Win32, Cocoa, Wayland, X11, D3D12, Metal or Vulkan headers. Native window implementations live in `platform/windows`, `platform/linux/wayland` and `platform/linux/x11`, each exposing only an opaque `nx_window`; macOS currently returns `NX_ERROR_UNSUPPORTED`, while Linux without a usable Wayland or X11 display returns `NX_ERROR_PLATFORM` from window creation. A Linux dispatcher chooses Wayland first when available and otherwise X11; each Linux window owns one display connection and is polled separately. `platform/common` holds private, OS-neutral event and UTF-8 file-drop queues. Each native adapter translates its OS input into these queues and retains responsibility for window ownership, thread checks, DPI and path encoding. The queues preserve order, retain unread entries after a short output buffer, and grow without changing pending entries on allocation failure. Future native implementations will use private platform and renderer interfaces. The public header will never expose native handles in ordinary widget or layout calls. A context can be created with a caller allocator; every owned allocation is returned to that allocator with the same size and alignment. Allocation failure leaves a context usable or returns an explicit error.

The private `nx_renderer_driver` validates complete command streams before passing them to a backend. It tracks physical target size and scale, skips submit for zero-size surfaces, retries temporary suspension, and latches device loss until explicit recovery. A backend owns all native resources and must finish reading or stage borrowed bitmap bytes before a successful submit returns. The headless renderer shares the command validator. The initial D3D12 backend clears and presents a Win32 swap chain, draws filled rectangles, triangles, RGBA8 images and A8 glyph masks with nested clips and RGBA8 alpha compositing, and rebuilds its device resources after removal. The Linux Vulkan adapter draws core shapes with nested clips and integer straight-alpha composition through embedded SPIR-V; an owned RGBA8/A8 upload atlas supplies images and text masks. Metal remains planned.

`nx_widget_register` gives custom widgets one stable ID and rectangle shared by their semantic node and optional hit region. Applications own widget state, use the standalone layout tree for geometry, and emit frame commands for drawing. Registration rolls back its semantic insertion if the optional hit region cannot be inserted. Built-in controls and native accessibility action dispatch remain future work.
Initial display calls add semantic text, image and separator nodes while appending their rendering commands. A fixed-size layout spacer reserves geometry without a semantic node.
Basic button, checkbox, radio and toggle calls now share the widget registration path, use semantic theme tokens for drawing and update caller-owned values after successful painting. Native accessibility action dispatch and advanced controls remain future work.
Horizontal value controls consume per-region captured pointer motion and focused key events, then publish a copied semantic value and paint the corresponding range position. They leave the caller's value unchanged if registration or painting fails.
A numeric editor helper parses ASCII decimals independently of process locale and filters normalized text events through the existing UTF-8 buffer. The draft remains separate from the caller's committed number until Enter receives a valid, in-range value.
The numeric-field widget owns this editor state, builds a temporary text layout from the current draft, clips selection, glyph and caret commands to its bounds, and copies semantic value and validation state into the frame. The field tracks which queued events it has consumed so a failed paint can be retried without duplicate edits.

Frame construction runs on its designated UI thread. On Win32, a thread message pump dispatches native messages to each window's private queue. The application transfers each window's normalized events into its own context with `nx_window_route_events`; a failed context enqueue leaves the current window event in place for retry. `nx_begin_frame` swaps that context's pending queue into a borrowed frame event stream and computes key/pointer transitions before widgets run. One simulation producer may publish immutable data through `nx_snapshot_exchange`; one UI consumer holds a read-only view through command generation and releases it afterward. Two metadata slots allow the producer to replace unviewed snapshots while a prior UI view remains pinned. A private SRW/POSIX lock protects only slot metadata, never simulation data or UI layout. Destruction requires the producer to have stopped and all views to be released. The simulation never calls into a live UI context or waits for ordinary layout and rendering.

`nx_sim_channel` carries bounded UI commands and simulation completions. An accepted command reserves a ticket until its completion is taken, so a worker can report that completion even when the UI has stopped draining. A full channel rejects a new command before taking its payload. Workers own taken command payloads and may complete tickets in any order; the UI owns taken completion payloads. The channel locks only ring and ticket metadata, never simulation work or frame construction.

`nx_series_view` borrows a stable, immutable sequence through a per-index callback; it can read from an acquired simulation snapshot without copying the full data set. `nx_series_decimate` binary-searches monotonically ordered x coordinates for the inclusive viewport and scans only visible points into caller-owned pixel-column envelopes. It retains first, last and y-extreme source points for the plot renderer. The decimator has no persistent state or internal allocation; a full-range redraw still scans the full source.

`nx_plot` consumes that borrowed view and a caller-owned envelope buffer during frame construction. It paints themed axes and the decimated line into ordinary frame commands, with a clipped trace and focus cursor. The application owns ranges and cursor state; input updates the cursor only after successful painting. Failure rolls back its semantic node, hit region, commands and clip depth.

`nx_telemetry_panel` and `nx_profiler_panel` borrow caller arrays, typically from the same pinned simulation snapshot. They derive a bounded visible row range from panel geometry and `first_row`, then paint themed bars and optional text while adding semantic metric/list nodes. The application owns scroll position and record lifetime. Panel errors roll back commands, clips and every semantic node added by that call. No persistent panel cache or full-array copy is held in the UI context.

An inspector combines a caller-owned preorder hierarchy with `nx_tree_flatten`, `nx_tree` and `nx_tree_item`; expansion and selection stay on the UI side. `nx_property_row` borrows typed values from the current application snapshot, copies the accessible value into the frame, and returns proposals without writing to the source. The caller may dispatch those proposals through `nx_sim_channel` and render the next accepted `nx_snapshot_exchange` view. Row failure rolls back its commands, semantic node, hit region, clip depth and owned glyph masks.

`nx_viewport_surface` wraps an application image producer and tracks one in-flight RGBA8 bitmap lease. A viewport call computes physical dimensions from logical bounds and backing scale, reports changes to the producer, registers a focusable widget and appends a clipped image command. The caller completes the lease only after rendering the command stream. A failed draw releases an acquired image and rolls back UI frame changes. Native GPU texture import needs a separate backend path; no native handles enter this initial C widget interface.

Presentation transitions are caller-owned values tied to one context's frame serial. They advance once per frame from the context's supplied time delta and can be retargeted without delaying input or semantic state. The context holds its own reduced-motion preference; applications may populate it from the native window query between frames.

Paint-only rich-text spans reuse the positioned text layout and its UTF-8 byte offsets. Validation keeps span boundaries at grapheme and shaped-glyph boundaries; the renderer selects per-glyph tint and derives background/decorations from line-aware selection rectangles. The rich label keeps one semantic text node for the entire source. Style-aware shaping and mixed-font metrics are later work.

Coordinate units are logical pixels. Rectangles use half-open bounds `[x, x+w) x [y, y+h)`. Width and height must be finite and nonnegative. Colors are nonpremultiplied RGBA floats in `[0,1]`; the headless renderer blends in deterministic integer RGBA8 space. Its output is a test reference, not a color-managed display renderer. Commands preserve insertion order. Clip commands use an explicit stack and intersect with the target buffer. Bitmap commands borrow source pixels through renderer completion; a future GPU renderer must finish upload or retain its own copy before the caller may release that memory.

The implementation includes core/layout/text/widget foundations and initial native Win32/X11/Wayland rendering; complete multi-OS release acceptance remains open. A standalone measure/arrange tree supports rows, columns, wrapping rows/columns, overlays, fixed/auto/flex grids, two-pane splits, one-child scroll containers and leaves. Frame-retained hit regions provide pointer capture and keyboard focus by stable ID, and inherit nested frame clips for hit testing. A context-owned theme stack resolves semantic color and metric tokens from dark/light/high-contrast/Midnight presets and local overrides; complete widget styling remains pending. A separate semantic tree records accessible roles, strings, state, actions and bounds. The text stack has Unicode boundary and bidi analysis, SFNT and TrueType glyph parsing, grayscale rasterization, fallback font collections, an initial logical-order shaping run path, bounded GSUB single/ligature and GPOS single/pair subsets, multiline layout with caret and selection geometry, and frame-owned A8 text drawing. A contiguous UTF-8 editing buffer provides grapheme-aware edits, boundary navigation and a bounded undo/redo operation log. GPOS mark attachment, advanced script shaping, scalable text storage and full editing integration remain separate tasks. Broader widget integration, incremental layout caches and release acceptance remain in progress. Native accessibility bridges and macOS/Metal remain separate DAG tasks in [ROADMAP.md](ROADMAP.md).
