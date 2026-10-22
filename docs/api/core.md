# Core API (0.1.0)

Public declarations are in `<nexaui/nexaui.h>`. The ABI is pre-1.0. All functions that mutate a context require exclusive access on its UI thread. Separate contexts may be owned by separate threads. `nx_headless_render` accesses no context and can run on another thread after command data is copied or while the producing context and its command storage are kept unchanged.

## Results and ownership

`NX_OK` means success. `NX_ERROR_ARGUMENT` reports a null required pointer, invalid geometry/color, or invalid allocator configuration. `NX_ERROR_STATE` reports an invalid frame/scope order. `NX_ERROR_MEMORY` reports allocator failure. `NX_ERROR_LIMIT` reports an explicit depth or size limit. `NX_ERROR_COMMAND` reports an invalid externally supplied command stream. Calls append no partial command when they fail.

`NX_ERROR_UNSUPPORTED` means the requested native backend is not implemented on this OS. `NX_ERROR_PLATFORM` means a native OS call failed.

## Native window (Windows and initial Linux backends)

`nx_window_create(desc, out_window)` creates a Win32 or Linux window on the calling OS event thread. Linux tries Wayland when `WAYLAND_DISPLAY` is present, then X11 if the Wayland connection is unavailable; `NEXAUI_PLATFORM=wayland` or `x11` forces one adapter. The descriptor owns no memory; the UTF-8 title is borrowed during the call. Positive finite width and height specify logical client dimensions. Zero flags show the window; `NX_WINDOW_HIDDEN` creates it without showing it. An optional allocator pair controls window storage. macOS returns `NX_ERROR_UNSUPPORTED`; Linux returns `NX_ERROR_PLATFORM` when no usable display is available.

`nx_window_poll(window, timeout_ms)` waits up to the given number of milliseconds and dispatches pending messages. Win32 dispatches all windows on that thread; Linux uses one connection per window, so poll each Linux window. `nx_window_get_info(window, out_info)` returns logical client size, backing scale and the close-request flag after polling. A close request leaves the window alive until `nx_window_destroy`. Wayland reports an integer output scale after surface entry and emits a resize event if that scale changes; X11 currently reports 1.0. `nx_window_next_event(window, out_event, available)` copies one normalized event or sets `available` to zero when empty. All operations, including destruction, must run on the creation thread. The window API does not expose a renderer or native handle.

`nx_window_route_events(window, context, out_count)` moves queued events to one context in order. On an allocation failure, the unaccepted event remains in the window queue for retry. Win32 pumps messages for all windows on the thread, so applications route each window to its own context. `nx_window_get_reduced_motion` reads the current Windows client-area animation preference; pass it to `nx_context_set_reduced_motion` between frames. Other native backends currently return `NX_ERROR_UNSUPPORTED` for the preference query.

`nx_window_set_cursor(window, shape)` chooses a persistent client-area cursor for one window. `NX_CURSOR_DEFAULT` is the arrow; the other shapes are text, link, crosshair, horizontal/vertical/diagonal resize, move, unavailable and busy. Set it after interpreting the current pointer position in a frame; for example, use `NX_CURSOR_CROSSHAIR` over a plot and reset to `NX_CURSOR_DEFAULT` elsewhere. Win32 applies the shape at the next native cursor update and immediately when the window currently owns the pointer; X11 defines it on that window. Wayland renders its own 32×32 logical cursor image, scales its physical buffer to the current integer output scale, and applies it on pointer entry or a shape change. Invalid shapes return `NX_ERROR_ARGUMENT`; a call from another thread returns `NX_ERROR_STATE`.

On X11, `nx_window_clipboard_set_text` owns a copy of valid UTF-8 until another client claims the `CLIPBOARD` selection or the window is destroyed. `nx_window_clipboard_get_text` reads a `UTF8_STRING` target from the current owner; requests between NexaUI windows on one UI thread are serviced during the call. Large text transfers use the X11 `INCR` protocol. Text is capped at 32 MiB. An incoming transfer waits at most five seconds; the owner keeps at most four concurrent incremental transfers, each with a copied payload that survives selection changes.

On Wayland, clipboard set requires a recent pointer-button or keyboard-key serial from that window and returns `NX_ERROR_STATE` before such an input event. The adapter offers `text/plain;charset=utf-8` and `text/plain`, owns the UTF-8 copy until the compositor cancels the source, and keeps active pipe writes alive across cancellation. Get accepts a UTF-8 text offer, pumps same-thread Wayland windows while reading, and follows the same 32 MiB input limit and five-second receive deadline. At most four outgoing pipe writes and four retained sources run per window. A missing data-device protocol returns `NX_ERROR_UNSUPPORTED`; no selection or compatible text offer returns `NX_ERROR_STATE`.

`nx_window_next_file_drop(window, path_utf8, capacity, out_length, out_info, available)` reads Win32 filesystem drops or local-file `text/uri-list` drops on Wayland and X11 after polling. All output pointers are required. A null path with zero capacity queries the next UTF-8 byte length without consuming it. A short buffer returns `NX_ERROR_LIMIT` and retains the entry; a successful copy writes the path and NUL and removes it. `out_info` gives zero-based position within its batch, batch count and logical drop coordinates. Empty queues clear outputs and set `available` to zero. The queue is separate from normalized input events. Linux ignores unsupported or malformed URIs, accepts `file:` paths with an empty, `localhost` or local machine authority, and decodes percent escapes before UTF-8 validation; one transfer is capped at 32 MiB and 65,536 paths. X11 accepts Xdnd copy drops with direct or `INCR` selection transfer. macOS currently returns `NX_ERROR_UNSUPPORTED`.

`nx_window_set_ime_rect(window, caret)` positions native IME UI from a logical client-area caret rectangle. Windows requests composition and first-candidate placement; X11 sets the XIM preedit spot at the caret baseline when the active input method supports `XIMPreeditPosition`; Wayland text-input-v3 or v1 sets its cursor rectangle in logical surface coordinates. The call requires a live window on its creation thread and a finite rectangle with positive width and height. It returns `NX_ERROR_UNSUPPORTED` when the required native input context or protocol is unavailable. A text field can set `desc.ime_window` to its borrowed native window; after a successful focused paint it passes the exact drawn caret to this API. At `nx_end_frame`, the context enables Windows or X11 in-field preedit or Wayland text input for the focused ordinary text field and disables its previously enabled window when focus leaves or the field disappears. Wayland password fields request sensitive-data and password content hints, display masked preedit, and send only empty surrounding text; Windows and X11 password fields retain their basic/native input path. Keep the borrowed window alive through the last frame using this context, or destroy the context before destroying the window. Custom editors can call `nx_window_set_ime_purpose` with `NX_IME_PURPOSE_NORMAL` or `NX_IME_PURPOSE_PASSWORD` before `nx_window_set_ime_enabled(window, 1/0)` when their focus changes. The call returns `NX_ERROR_UNSUPPORTED` if the required Windows or X11 input context or Wayland text-input protocol is unavailable. Native candidate placement is best effort. X11 creates optional input contexts from the process locale and `XMODIFIERS`; when a callback preedit style is advertised, focused ordinary editors use a separate context. Committed text comes from `Xutf8LookupString` when a context is available, otherwise the adapter uses its basic keysym lookup. Wayland emits committed text after a v3 `done` event. `nx_window_get_ime_preedit` exposes the current uncommitted Windows, X11 callback-style XIM, or Wayland v3/v1 UTF-8 composition with byte-based cursor offsets; pass a null buffer to query its length, then a buffer large enough for the text and NUL. A focused text field displays this composition without changing its committed draft. `NX_EVENT_IME_PREEDIT_CHANGED` signals redraw and a fresh query when the visible composition changes or clears. `nx_window_set_ime_surrounding_text` gives Wayland v3 or v1 up to 4000 UTF-8 bytes around the entire selection and returns `NX_ERROR_UNSUPPORTED` on other backends. Custom editors match IME deletion events and tagged text to their current target and revision. V3 sends `NX_EVENT_IME_DELETE_SURROUNDING` with UTF-8 byte counts outside the selection, applied by `nx_text_buffer_delete_surrounding`. V1 sends `NX_EVENT_IME_DELETE_RANGE` with a signed byte index relative to the cursor and a byte length, applied by `nx_text_buffer_delete_relative`. Handle the deletion before its following tagged text. Focused ordinary text fields perform this exchange automatically. Password fields never advertise their text.

`nx_window_clipboard_set_text` and `nx_window_clipboard_get_text` transfer plain UTF-8 through Win32 `CF_UNICODETEXT` on that same thread. Set validates UTF-8 and rejects embedded NUL. Get supports a required-byte-count query with a null destination and writes a terminator when the supplied capacity is sufficient. The return value is `NX_ERROR_STATE` when no Unicode text format is available and `NX_ERROR_PLATFORM` if a Win32 clipboard operation fails. macOS returns `NX_ERROR_UNSUPPORTED` until its clipboard backend lands.

## Frame input

`nx_context_queue_event(context, event)` copies a validated `nx_event` into the context's pending queue. It accepts events before or during a frame; each is consumed by the next `nx_begin_frame`. The descriptor's `initial_event_capacity` configures the first queue allocation; zero selects the default. After warmup, the context alternates reusable event buffers. Invalid events return `NX_ERROR_ARGUMENT`; allocator failure returns `NX_ERROR_MEMORY` without appending.

`nx_frame_events(context, out_events, out_count)` returns a borrowed immutable array after begin and before the next begin or destruction. `nx_frame_input(context, out_input)` copies the current pointer/button/modifier/focus snapshot. `nx_key_down`, `nx_key_pressed`, and `nx_key_released` return zero for an invalid key or a context without an active/finished frame. Button and key transitions reset at each begin; the held state persists. Text events contain Unicode scalars and remain separate from keys. Resize events may report zero dimensions while minimized; `nx_begin_frame` still requires positive dimensions, so applications skip frame building while the drawable area is zero. See [Input and frame transitions](../concepts/input.md).

## Interaction and focus

`nx_interact_region(context, id, hit_rect, flags, out_state)` registers one hit region during an active frame. The ID must be nonzero and unique within that frame; `nx_id` can derive it from the current hierarchy. The rectangle must be valid logical geometry. `NX_REGION_FOCUSABLE` participates in keyboard traversal; `NX_REGION_DISABLED` blocks activation and focus. The result is copied into caller-owned `out_state` and reports hover, held, focused and click count. On failure the output is zero and no region is appended. The context owns stored regions and reuses memory across frames.

`nx_focus_set(context, id)` selects an enabled focusable region registered in the current or previous frame. ID zero clears focus. Invalid targets return `NX_ERROR_ARGUMENT`. `nx_focus_id(context)` returns the current logical focus ID or zero. Tab and Shift+Tab move through the previous frame's registered order; Enter and Space activate on release. Registration order also determines hit-test z-order, with the last registered region on top. See [Identity, hit testing and focus](../concepts/focus.md).

## Accessibility tree

`nx_semantics_add(context, desc, out_index)` copies a semantic node into the current frame's parent-first tree. `out_index` is optional and is set to `UINT32_MAX` on failure. The descriptor's ID must be nonzero and unique; `parent_id` zero creates a root, otherwise the parent must already be registered. Bounds are valid logical geometry. Name, description and value spans are borrowed for the call, validated as UTF-8 without embedded NUL bytes, then copied. Interactive roles require a nonempty name. Invalid data returns `NX_ERROR_ARGUMENT`, duplicate IDs return `NX_ERROR_STATE`, depth/size overflow returns `NX_ERROR_LIMIT`, and allocation failure returns `NX_ERROR_MEMORY`; no node is appended on failure. The maximum hierarchy depth is `NX_SEMANTIC_MAX_DEPTH`.

After a successful `nx_end_frame`, `nx_semantics_count(context, out_count)` reports node count, `nx_semantics_get(context, index, out_view)` copies a node view, and `nx_semantics_find(context, id, out_index)` finds its index. Views contain borrowed string spans that expire at the next begin or destruction. The focused state bit is derived from the context's logical focus when queried. Action flags describe capability only; native action dispatch is still pending. These calls use the context's UI thread contract. See [Accessibility semantics](../concepts/accessibility.md).

`NX_SEM_PROTECTED` marks a sensitive field whose semantic value is intentionally empty. `nx_text_field_create_password` makes a no-history field with best-effort wiping of NexaUI-owned text allocations when released. `nx_text_field_desc.password` must be set on each widget call for that field; the mode masks drawing by grapheme cluster and suppresses copy/cut shortcuts while permitting paste. The caller and OS remain responsible for copies outside the editor buffer.

`nx_context_create(desc, out_context)` creates an opaque context. `desc` may be null; `out_context` is required and is set to null on failure. `nx_context_destroy(context)` returns all storage through the context allocator and accepts null. A custom `nx_allocator` must supply both allocation and deallocation callbacks. It must return storage aligned for `max_align_t`. Allocations and frees occur on the context's UI thread. The provided initial command capacity is a hint; zero uses the default.

`nx_arena_create(allocator, capacity, out_arena)` creates a fixed-capacity scratch arena with a caller allocator or the default. Capacity must be positive. `nx_arena_alloc(arena, size, alignment, out_pointer)` returns a borrowed allocation without heap traffic; size must be positive and alignment a power of two no greater than `alignof(max_align_t)`. Exhaustion returns `NX_ERROR_LIMIT` and a null output. `nx_arena_reset` invalidates every allocation at once and reuses the buffer. `nx_arena_used` and `nx_arena_capacity` report byte counts (zero for null). `nx_arena_destroy` frees its storage and accepts null. The arena requires exclusive access on one thread.

## Layout tree

`nx_layout_default_desc(kind)` returns a valid starting descriptor for a leaf, row, column or overlay. Set axis `has_preferred` to 1 to use `preferred`; otherwise measurement derives size from children (zero for a bare leaf). Minimum and preferred must be finite nonnegative values; maximum may be positive infinity. Grow and shrink weights must be finite and nonnegative. Padding, border, margin and gap are nonnegative logical pixels. Invalid descriptors fail `nx_layout_add` without adding a node.

`nx_layout_create(allocator, initial_node_capacity, out_layout)` creates an independent tree; allocator may be null and capacity zero selects a default. `nx_layout_destroy` releases all owned storage and accepts null. `nx_layout_reset` removes every node while retaining capacity. Nodes are added parent first with `nx_layout_add(layout, parent, desc, out_node)`. Use `NX_LAYOUT_NO_PARENT` for a root. Handles are integer indices valid until reset or destruction. A leaf cannot have children. Nesting is limited to `NX_LAYOUT_MAX_DEPTH`; allocation and limit errors leave existing nodes intact.

`nx_layout_measure(layout, root, available_width, available_height, out_desired)` computes desired dimensions for a root with no parent. Available dimensions can be finite nonnegative or positive infinity. `nx_layout_arrange(layout, root, rect)` assigns a final border-box rectangle to that measured root and its descendants. `nx_layout_get_rect(layout, node, out_rect)` returns a node's arranged border box; the caller owns the output value, and failure zeros it. Rebuilding, adding a node or measuring again invalidates prior arranged results. These calls require exclusive access on one thread and do not touch a UI context.

The implemented algorithm distributes row/column free space by grow weight and deficits by shrink weight times preferred size, respecting maximums and minimums. It preserves overflow if minimums cannot fit. Grid, wrapping, scrolling, virtualization, cached measurement and text intrinsic sizing are pending. See [Layout contract](../concepts/layout.md) and `examples/02_layout_headless`.

## Frame and identity

`nx_begin_frame(context, width, height, delta_seconds)` starts a new frame. Width/height are positive finite logical pixels and time is finite and nonnegative. It discards previous commands, resets scopes and invalidates any borrowed command pointer. It cannot be called while a frame is being built. `nx_end_frame(context)` completes only when ID and clip scopes are balanced. If scopes remain open, close them and retry. A finished frame may be read until the next begin or context destruction.

`nx_id(context, label, length)` hashes a byte label in the current hierarchy; it returns zero for invalid input/state and a nonzero ID otherwise. `nx_push_id` enters a stable identity scope and `nx_pop_id` leaves it. The maximum nesting is `NX_MAX_ID_DEPTH`. Labels are copied into the hash during the call and are not retained. These functions do not decode Unicode or render text.

## Commands

`nx_fill_rect`, `nx_stroke_rect`, `nx_fill_round_rect`, `nx_draw_line` and `nx_fill_triangle` append paint commands. Strokes lie inside their rectangle; lines have round caps. `nx_push_clip(context, rect)` starts a nested intersection clip and `nx_pop_clip(context)` ends it. The maximum clip nesting is `NX_MAX_CLIP_DEPTH`. Rectangles require finite coordinates and nonnegative finite dimensions; stroke and line thickness must be positive, corner radius nonnegative, points finite, and color components finite in `[0,1]`. Commands preserve append order. `nx_frame_commands(context, out_commands, out_count)` returns borrowed immutable command memory after a successful end. The pointer may be null when the count is zero.

`nx_utf8_decode(bytes, length, offset, out_scalar)` decodes one Unicode scalar from a borrowed byte span. The offset advances only on success. `nx_utf8_validate(bytes, length, error_offset)` validates an entire span and optionally returns the first invalid byte or the final length. Both reject malformed, overlong, surrogate and out-of-range sequences. These are encoding primitives; shaping, boundaries and bidi remain future text-engine tasks.

## Headless output

`nx_headless_render(commands, count, pixels, width, height, stride_bytes, clear_color)` writes caller-owned RGBA8 straight-alpha pixels. Width and height must be nonzero, stride must cover `width*4` bytes, and pixel memory must extend through the final visible row. The buffer is cleared before commands are applied in order. Malformed streams are checked before any pixel is changed. Clips and shapes use pixel-center coverage without antialiasing; adjacent triangles use a top-left edge rule. The caller retains all command and pixel memory; no buffers are retained. This function allocates no heap storage.

`nx_draw_bitmap(context, dest, bitmap, tint)` records a borrowed RGBA8 image or A8 coverage mask and scales it to `dest` with nearest-neighbor sampling. Source width, height and stride must describe readable rows; atlas subregions can retain the parent atlas stride. The source pointer must remain valid through rendering, even after the frame ends. RGBA8 channels are multiplied by tint; A8 coverage multiplies tint alpha. Bitmap source and destination pixel memory may not overlap during `nx_headless_render`.

`nx_viewport_surface_create(source, allocator, out_surface)` copies the application callbacks into a wrapper. `nx_viewport(context, surface, desc, out_state)` computes physical pixel dimensions from logical bounds and `scale`, reports size changes, registers a focusable semantic region and borrows one exactly sized RGBA8 image. Zero-size bounds suspend the source without a widget. After every renderer has consumed the commands, call `nx_viewport_surface_complete(surface)` to invoke its release callback. Reuse or destruction while the image is in flight returns `NX_ERROR_STATE`; `nx_viewport_surface_destroy` frees only the wrapper. See [the viewport contract](../concepts/viewport.md) for failure and thread rules.

`nx_font_open(bytes, length, allocator, out_font)` opens a single borrowed SFNT font face. It validates the directory and copies only the table index, not the font bytes. `nx_font_table_count`, `nx_font_table_at` and `nx_font_find_table` inspect bounded table views; tags use `NX_SFNT_TAG`. `nx_font_get_metrics` parses the horizontal metrics tables, and `nx_font_glyph_hmetrics` returns per-glyph advance width and left side bearing. All table views expire at `nx_font_close`. See [Text engine status](../concepts/text.md) for supported and pending font features.

`nx_font_glyph_index(font, scalar, out_glyph)` maps one Unicode scalar through a validated format 4 or 12 `cmap` subtable. A missing character returns `NX_OK` with glyph zero. A missing supported Unicode subtable returns `NX_ERROR_UNSUPPORTED`; an invalid scalar or malformed candidate returns `NX_ERROR_ARGUMENT`. The output is reset to zero on failure.

`nx_font_get_glyph_outline(font, glyph_index, out_outline)` decodes simple and composite TrueType outlines. Initialize `nx_glyph_outline_buffer` to zero and call once to receive required point and contour counts; then assign arrays and capacities and call again. The second call writes unscaled design-unit points with on-curve flags and inclusive contour end indices. Too-small arrays return `NX_ERROR_LIMIT` with required counts and remain untouched. Composite components support XY offsets, outline-point alignment and F2DOT14 transforms. Cycles and invalid references return `NX_ERROR_ARGUMENT`; the 32-glyph recursion limit returns `NX_ERROR_LIMIT`. Fractional transformed coordinates round to the nearest design unit, with ties away from zero. Hinting, variable-font deltas and phantom-point alignment are not applied; non-`glyf` fonts return `NX_ERROR_UNSUPPORTED`.

`nx_rasterize_glyph_a8(outline, pixels_per_unit, baseline_x, baseline_y, pixels, width, height, stride_bytes, pixel_buffer_size, allocator)` converts an outline into an A8 mask. Positive scale converts design units to pixels; the baseline coordinates locate glyph point `(0,0)` in bitmap space, whose y axis points down. The routine flattens quadratic curves, applies nonzero winding and uses a 4×4 pixel sample grid. It writes the visible width of every row and preserves padding. Invalid geometry, insufficient storage or failed allocation leaves the output untouched. Dimensions are capped at `NX_GLYPH_RASTER_MAX_DIMENSION` and total pixels at `NX_GLYPH_RASTER_MAX_PIXELS`; excessive contour work returns `NX_ERROR_LIMIT`. `allocator` may be null for the default allocator. The output can be passed as `NX_BITMAP_A8` to the headless renderer. Font hinting and dropout control are not applied.

`nx_glyph_cache_create(font, max_bytes, allocator, out_cache)` makes a per-font, single-threaded LRU cache of A8 masks. The font and its borrowed source bytes must remain alive until `nx_glyph_cache_destroy`. `nx_glyph_cache_get(cache, glyph_index, pixels_per_em, out_mask)` returns a borrowed mask, top-left offsets relative to the glyph baseline and horizontal advance in pixels. Draw at `(pen_x + x_offset, baseline_y + y_offset)` using `NX_BITMAP_A8`, then advance the pen by `advance_x`. A view expires on the next cache miss, clear or destroy. `max_bytes` caps resident entries and pixel data; the cache object and temporary decoding/raster buffers are excluded. Oversized masks return `NX_ERROR_LIMIT`. `nx_glyph_cache_clear` removes entries and retains cumulative hit/miss/eviction counters; `nx_glyph_cache_get_stats` reports them with resident bytes and entry count. This cache stores individual masks; a shared GPU atlas remains planned.

`nx_font_collection_create` makes an ordered, single-threaded fallback chain of borrowed `nx_font` faces. Add supported Unicode-cmap faces with `nx_font_collection_add`; duplicate faces are rejected and the collection is capped at `NX_FONT_COLLECTION_MAX_FONTS`. `nx_font_collection_resolve(collection, scalar, out_match)` scans in order and returns the first nonzero glyph, its font and its collection index. If no font covers the scalar, it returns `NX_OK` with a null font, glyph zero and index `UINT32_MAX`. Invalid scalars and malformed mappings return an error. `nx_font_collection_clear` resets the order; `nx_font_collection_count` and `nx_font_collection_font_at` inspect it. All fonts and their byte buffers must outlive the collection. Selection is by cmap coverage only; shaping, language-aware style matching and outline support are later stages.

Example:

```c
nx_context *ui = NULL;
nx_context_create(NULL, &ui);
nx_begin_frame(ui, 64, 64, 1.0 / 60.0);
nx_fill_rect(ui, (nx_rect){4, 4, 56, 56}, (nx_color){0.2f, 0.6f, 0.9f, 1});
nx_end_frame(ui);
const nx_command *commands;
size_t count;
nx_frame_commands(ui, &commands, &count);
uint8_t pixels[64 * 64 * 4];
nx_headless_render(commands, count, pixels, 64, 64, 64 * 4,
                   (nx_color){0, 0, 0, 1});
nx_context_destroy(ui);
```

Production callers must check each return value and keep command storage valid until rendering finishes. The example under `examples/00_headless` demonstrates error handling and file output.

## Whole-text accessibility edits

`nx_text_buffer_replace_all(buffer, bytes, length)` replaces all text as one
atomic history operation without first changing the selection. Undo restores the
original selection within the configured history budget. Failure preserves text,
selection and history; equal text is a no-op preserving pending redo and selection.
Input may borrow a substring of the current buffer. Like replace, this low-level
buffer operation accepts valid UTF-8 including embedded NUL; SIZE_MAX is rejected
before reading input. Sensitive buffers still have no history and scrub owned
text allocations when freed.

Call `nx_semantics_request_text_value(context, id, value)` on the owner thread
between completed frames to copy a request for a standard owned text/numeric
editor. It admits at most 64 targets and 65,536 UTF-8 bytes per value, rejecting
malformed UTF-8, embedded NUL, oversized payloads, single-line CR/LF, disabled or
modal-blocked targets, read-only/protected fields and unbound custom semantics.
Admission does not edit the field. The next widget submission must still match
the model identity, parent, application revision and edit policy. Then it applies
one undoable replacement; validation still judges the resulting draft and no
submission is synthesized. Missing or invalidated targets expire. Edit allocation
failures leave the model unchanged; a later paint failure retains the existing
retry-without-replaying-input behavior.

`nx_text_field_set_read_only(field, 1/0)` affects the next widget submission
without changing descriptor size. Read-only fields keep focus, selection,
navigation and Copy while suppressing edits, undo/redo, paste/cut and IME input.
Application set_text remains allowed and retains its existing history-reset
contract. Policy changes and application whole-model replacements invalidate
pending accessibility edits even if policy is restored before the next frame.
Keep all field operations on its owner thread. The Windows native adapter copies
and guards these requests as described in the [accessibility contract](../concepts/accessibility.md#windows-owned-editor-values).
