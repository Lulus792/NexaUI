#ifndef NEXAUI_NEXAUI_H
#define NEXAUI_NEXAUI_H

#include <stddef.h>
#include <stdint.h>

#if defined(_WIN32) && defined(NEXAUI_SHARED)
#  if defined(NEXAUI_BUILDING_SHARED)
#    define NX_API __declspec(dllexport)
#  else
#    define NX_API __declspec(dllimport)
#  endif
#elif defined(__GNUC__) && defined(NEXAUI_SHARED)
#  define NX_API __attribute__((visibility("default")))
#else
#  define NX_API
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define NX_VERSION_MAJOR 0
#define NX_VERSION_MINOR 1
#define NX_VERSION_PATCH 0
#define NX_MAX_CLIP_DEPTH 64u
#define NX_MAX_ID_DEPTH 64u
#define NX_MAX_THEME_DEPTH 32u
#define NX_LAYOUT_MAX_DEPTH 64u
#define NX_LAYOUT_NO_PARENT UINT32_MAX
#define NX_SEMANTIC_MAX_DEPTH 64u
#define NX_SFNT_MAX_TABLES 4096u
#define NX_FONT_COLLECTION_MAX_FONTS 64u
#define NX_GLYPH_RASTER_MAX_DIMENSION 4096u
#define NX_GLYPH_RASTER_MAX_PIXELS 4194304u
#define NX_SFNT_TAG(a, b, c, d) \
    (((uint32_t)(uint8_t)(a) << 24u) | ((uint32_t)(uint8_t)(b) << 16u) | \
     ((uint32_t)(uint8_t)(c) << 8u) | (uint32_t)(uint8_t)(d))

typedef struct nx_context nx_context;
typedef struct nx_arena nx_arena;
typedef struct nx_window nx_window;
typedef struct nx_layout nx_layout;
typedef struct nx_font nx_font;
typedef struct nx_glyph_cache nx_glyph_cache;
typedef struct nx_font_collection nx_font_collection;
typedef struct nx_text_layout nx_text_layout;
typedef struct nx_text_stamp nx_text_stamp;
typedef struct nx_text_buffer nx_text_buffer;
typedef struct nx_numeric_field nx_numeric_field;
typedef struct nx_text_field nx_text_field;
typedef struct nx_virtual_list nx_virtual_list;
typedef struct nx_snapshot_exchange nx_snapshot_exchange;
typedef struct nx_sim_channel nx_sim_channel;
typedef struct nx_viewport_surface nx_viewport_surface;

typedef enum nx_result {
    NX_OK = 0,
    NX_ERROR_ARGUMENT = 1,
    NX_ERROR_STATE = 2,
    NX_ERROR_MEMORY = 3,
    NX_ERROR_LIMIT = 4,
    NX_ERROR_COMMAND = 5,
    NX_ERROR_UNSUPPORTED = 6,
    NX_ERROR_PLATFORM = 7
} nx_result;

/* Allocations must be aligned for max_align_t. Supply both callbacks or neither.
   The default allocator uses malloc/free. Deallocation receives the original size. */
typedef struct nx_allocator {
    void *user;
    void *(*allocate)(void *user, size_t size);
    void (*deallocate)(void *user, void *pointer, size_t size);
} nx_allocator;

/* S01: one simulation producer and one UI consumer exchange immutable,
   caller-owned snapshots without copying their contents. Publishing transfers
   ownership only on NX_OK. The disposer runs after the last view is released,
   on either thread, outside the exchange lock; it must not reenter the same
   exchange. The producer must finish writing before publish and never write
   again. At most one view may be open per exchange. Do not copy or modify a
   view's fields; release it before the UI frame ends. No calls may overlap a
   successful destroy, and the producer must be stopped before destroy. */
typedef void (*nx_snapshot_dispose_fn)(void *user, void *data, size_t bytes);
typedef struct nx_snapshot_view {
    const void *data;
    size_t bytes;
    /* Private validation fields. */
    const nx_snapshot_exchange *owner;
    uint64_t generation, ticket;
    unsigned slot;
} nx_snapshot_view;

NX_API nx_result nx_snapshot_exchange_create(const nx_allocator *allocator,
                                              nx_snapshot_exchange **out_exchange);
/* Returns NX_ERROR_STATE and keeps the exchange alive while a view is open. */
NX_API nx_result nx_snapshot_exchange_destroy(nx_snapshot_exchange *exchange);
NX_API nx_result nx_snapshot_exchange_publish(nx_snapshot_exchange *exchange,
                                               void *data, size_t bytes,
                                               nx_snapshot_dispose_fn dispose,
                                               void *dispose_user);
/* No current snapshot returns NX_OK with a zero view; no release is needed. */
NX_API nx_result nx_snapshot_exchange_acquire(nx_snapshot_exchange *exchange,
                                               nx_snapshot_view *out_view);
/* Clears the view on success; stale, copied and foreign views are rejected. */
NX_API nx_result nx_snapshot_exchange_release(nx_snapshot_exchange *exchange,
                                               nx_snapshot_view *view);

/* S02: bounded UI-to-simulation commands and simulation-to-UI completions.
   Each accepted command reserves one completion slot until the UI takes that
   completion. The UI may post commands from one thread; one or more workers
   may take and complete them in any order. Each payload is immutable after a
   successful post. Posting takes ownership only on NX_OK; taking transfers
   ownership to the caller. Dispose each taken message exactly once. A command
   must be taken before its ticket can be completed, and only once. No callback
   runs under the channel lock. Stop all users before destroy; destroy rejects
   commands taken by workers that have not been completed. Disposers invoked
   during destroy must not call back into the same channel. */
typedef struct nx_sim_message {
    uint64_t ticket; /* Nonzero for a real message; zero on an empty take. */
    nx_result status; /* NX_OK for commands; result of work for completions. */
    void *data;
    size_t bytes;
    nx_snapshot_dispose_fn dispose;
    void *dispose_user;
} nx_sim_message;

NX_API nx_result nx_sim_channel_create(size_t capacity,
                                       const nx_allocator *allocator,
                                       nx_sim_channel **out_channel);
NX_API nx_result nx_sim_channel_destroy(nx_sim_channel *channel);
NX_API nx_result nx_sim_channel_post_command(nx_sim_channel *channel,
                                             void *data, size_t bytes,
                                             nx_snapshot_dispose_fn dispose,
                                             void *dispose_user,
                                             uint64_t *out_ticket);
NX_API nx_result nx_sim_channel_take_command(nx_sim_channel *channel,
                                             nx_sim_message *out_message);
/* Completion data may be NULL only when bytes is zero and dispose is NULL. */
NX_API nx_result nx_sim_channel_post_completion(nx_sim_channel *channel,
                                                uint64_t ticket,
                                                nx_result status,
                                                void *data, size_t bytes,
                                                nx_snapshot_dispose_fn dispose,
                                                void *dispose_user);
NX_API nx_result nx_sim_channel_take_completion(nx_sim_channel *channel,
                                                nx_sim_message *out_message);
NX_API void nx_sim_message_dispose(nx_sim_message *message);

/* S03: a borrowed, immutable series with finite points and nondecreasing x.
   The caller keeps its storage stable during each call, for example while an
   S01 snapshot view is open. The callback may read AoS or SoA storage and must
   return NX_OK with a point for every index below count. No callback runs from
   another thread or after this call returns. The library does not allocate. */
typedef struct nx_series_point {
    double x, y;
} nx_series_point;
typedef nx_result (*nx_series_sample_fn)(void *user, size_t index,
                                         nx_series_point *out_point);
typedef struct nx_series_view {
    size_t count;
    void *user;
    nx_series_sample_fn sample;
} nx_series_view;
typedef struct nx_series_mark {
    size_t index;
    nx_series_point point;
} nx_series_mark;
typedef struct nx_series_envelope {
    uint32_t column; /* Zero-based column in the requested viewport. */
    size_t sample_count;
    nx_series_mark first, last, minimum, maximum;
} nx_series_envelope;

/* Produce one envelope per occupied pixel column in increasing column order.
   Each envelope retains the first/last point and both y extrema (first index
   wins ties). x_min and x_max are inclusive and must form a finite positive
   span. columns must be nonzero; output capacity must be at least columns
   whenever view.count is nonzero. Empty views return zero envelopes and need
   no output buffer. On failure *out_count is zero and output contents are
   unspecified. The caller guarantees globally nondecreasing x; a decrease
   observed in the visible scan is rejected, but offscreen order is not checked.
   Cost is O(log count + visible samples), with no internal data copy. */
NX_API nx_result nx_series_decimate(const nx_series_view *view,
                                    double x_min, double x_max,
                                    uint32_t columns,
                                    nx_series_envelope *out_envelopes,
                                    size_t capacity, size_t *out_count);

typedef struct nx_context_desc {
    nx_allocator allocator;
    size_t initial_command_capacity; /* Zero selects a small default. */
    size_t initial_event_capacity;   /* Zero selects a small default. */
    size_t initial_region_capacity;  /* Zero selects a small default. */
    size_t initial_semantic_capacity; /* Zero selects a small default. */
    size_t initial_semantic_text_capacity; /* Zero selects a small default. */
} nx_context_desc;

typedef enum nx_key {
    NX_KEY_UNKNOWN = 0,
    NX_KEY_TAB, NX_KEY_ENTER, NX_KEY_ESCAPE, NX_KEY_SPACE,
    NX_KEY_BACKSPACE, NX_KEY_DELETE, NX_KEY_INSERT,
    NX_KEY_LEFT, NX_KEY_RIGHT, NX_KEY_UP, NX_KEY_DOWN,
    NX_KEY_HOME, NX_KEY_END, NX_KEY_PAGE_UP, NX_KEY_PAGE_DOWN,
    NX_KEY_SHIFT, NX_KEY_CONTROL, NX_KEY_ALT, NX_KEY_SUPER,
    NX_KEY_A, NX_KEY_B, NX_KEY_C, NX_KEY_D, NX_KEY_E, NX_KEY_F,
    NX_KEY_G, NX_KEY_H, NX_KEY_I, NX_KEY_J, NX_KEY_K, NX_KEY_L,
    NX_KEY_M, NX_KEY_N, NX_KEY_O, NX_KEY_P, NX_KEY_Q, NX_KEY_R,
    NX_KEY_S, NX_KEY_T, NX_KEY_U, NX_KEY_V, NX_KEY_W, NX_KEY_X,
    NX_KEY_Y, NX_KEY_Z,
    NX_KEY_0, NX_KEY_1, NX_KEY_2, NX_KEY_3, NX_KEY_4,
    NX_KEY_5, NX_KEY_6, NX_KEY_7, NX_KEY_8, NX_KEY_9,
    NX_KEY_F1, NX_KEY_F2, NX_KEY_F3, NX_KEY_F4, NX_KEY_F5, NX_KEY_F6,
    NX_KEY_F7, NX_KEY_F8, NX_KEY_F9, NX_KEY_F10, NX_KEY_F11, NX_KEY_F12,
    NX_KEY_COUNT
} nx_key;

#define NX_MOD_SHIFT   1u
#define NX_MOD_CONTROL 2u
#define NX_MOD_ALT     4u
#define NX_MOD_SUPER   8u

#define NX_BUTTON_LEFT   1u
#define NX_BUTTON_RIGHT  2u
#define NX_BUTTON_MIDDLE 4u
#define NX_BUTTON_X1     8u
#define NX_BUTTON_X2    16u

/* Text-system actions are distinct from physical keys and their modifiers.
   Native key bindings may produce these without the corresponding key chord.
   Layout-dependent visual movement is handled by rendered text fields. */
typedef enum nx_text_command {
    NX_TEXT_COMMAND_NONE = 0,
    NX_TEXT_COMMAND_MOVE_BACKWARD, NX_TEXT_COMMAND_MOVE_FORWARD,
    NX_TEXT_COMMAND_MOVE_LEFT, NX_TEXT_COMMAND_MOVE_RIGHT,
    NX_TEXT_COMMAND_MOVE_WORD_BACKWARD, NX_TEXT_COMMAND_MOVE_WORD_FORWARD,
    NX_TEXT_COMMAND_MOVE_WORD_LEFT, NX_TEXT_COMMAND_MOVE_WORD_RIGHT,
    NX_TEXT_COMMAND_MOVE_UP, NX_TEXT_COMMAND_MOVE_DOWN,
    NX_TEXT_COMMAND_PAGE_UP, NX_TEXT_COMMAND_PAGE_DOWN,
    NX_TEXT_COMMAND_LINE_START, NX_TEXT_COMMAND_LINE_END,
    NX_TEXT_COMMAND_LINE_LEFT, NX_TEXT_COMMAND_LINE_RIGHT,
    NX_TEXT_COMMAND_PARAGRAPH_START, NX_TEXT_COMMAND_PARAGRAPH_END,
    NX_TEXT_COMMAND_DOCUMENT_START, NX_TEXT_COMMAND_DOCUMENT_END,
    NX_TEXT_COMMAND_DELETE_BACKWARD, NX_TEXT_COMMAND_DELETE_FORWARD,
    NX_TEXT_COMMAND_DELETE_WORD_BACKWARD, NX_TEXT_COMMAND_DELETE_WORD_FORWARD,
    NX_TEXT_COMMAND_DELETE_LINE_START, NX_TEXT_COMMAND_DELETE_LINE_END,
    NX_TEXT_COMMAND_DELETE_PARAGRAPH_START, NX_TEXT_COMMAND_DELETE_PARAGRAPH_END,
    NX_TEXT_COMMAND_SELECT_ALL, NX_TEXT_COMMAND_NEWLINE,
    NX_TEXT_COMMAND_TAB, NX_TEXT_COMMAND_BACKTAB, NX_TEXT_COMMAND_CANCEL,
    NX_TEXT_COMMAND_COPY, NX_TEXT_COMMAND_CUT, NX_TEXT_COMMAND_PASTE,
    NX_TEXT_COMMAND_UNDO, NX_TEXT_COMMAND_REDO,
    NX_TEXT_COMMAND_SCROLL_LINE_UP, NX_TEXT_COMMAND_SCROLL_LINE_DOWN,
    NX_TEXT_COMMAND_SCROLL_DOCUMENT_START, NX_TEXT_COMMAND_SCROLL_DOCUMENT_END,
    NX_TEXT_COMMAND_MOVE_PAGE_UP, NX_TEXT_COMMAND_MOVE_PAGE_DOWN,
    NX_TEXT_COMMAND_COUNT
} nx_text_command;
/* Only movement and page commands accept this flag. It expresses the selector's
   selection behavior independently of the physical Shift modifier. */
#define NX_TEXT_COMMAND_EXTEND_SELECTION UINT32_C(0x80000000)

typedef enum nx_event_type {
    NX_EVENT_POINTER_MOVE = 1,
    NX_EVENT_POINTER_DOWN,
    NX_EVENT_POINTER_UP,
    NX_EVENT_POINTER_CANCEL,
    NX_EVENT_WHEEL,
    NX_EVENT_KEY_DOWN,
    NX_EVENT_KEY_UP,
    NX_EVENT_TEXT,
    NX_EVENT_FOCUS,
    NX_EVENT_RESIZE,
    NX_EVENT_TOUCH_DOWN,
    NX_EVENT_TOUCH_MOVE,
    NX_EVENT_TOUCH_UP,
    NX_EVENT_TOUCH_CANCEL,
    NX_EVENT_IME_PREEDIT_CHANGED, /* Query nx_window_get_ime_preedit. */
    NX_EVENT_IME_DELETE_SURROUNDING,
    NX_EVENT_IME_DELETE_RANGE,
    NX_EVENT_TEXT_COMMAND,
    /* Physical state only: the native text system owns the corresponding
       edit/action. Widgets must not repeat it from this key event. */
    NX_EVENT_KEY_DOWN_CONSUMED
} nx_event_type;

/* Every field is backend-neutral. Unused fields are zero. Mouse and touch
   coordinates and dimensions are logical pixels; wheel deltas are abstract
   scroll units. Touch contacts retain their contact_id until up/cancel.
   Text contains one Unicode scalar, independent of physical key events. */
typedef struct nx_event {
    nx_event_type type;
    uint32_t modifiers;
    float x, y;
    float delta_x, delta_y;
    float width, height, scale;
    uint32_t button;
    uint32_t contact_id; /* Touch contact ID for NX_EVENT_TOUCH_*. */
    nx_key key;
    union {
        uint32_t scalar; /* NX_EVENT_TEXT. */
        uint32_t text_command; /* NX_EVENT_TEXT_COMMAND: command plus flags. */
    };
    int repeat;
    int focused;
    /* Wayland IME TEXT and DELETE events carry the editor target and
       text/selection revision. Match both fields before editing. The v3
       DELETE_SURROUNDING counts UTF-8 bytes outside the selection. The v1
       DELETE_RANGE starts at cursor + ime_index and spans ime_length bytes. */
    uint64_t ime_target, ime_revision;
    uint32_t ime_before, ime_after;
    int32_t ime_index;
    uint32_t ime_length;
} nx_event;

typedef struct nx_input_snapshot {
    float pointer_x, pointer_y;
    float wheel_x, wheel_y;
    uint32_t buttons_down, buttons_pressed, buttons_released;
    uint32_t modifiers;
    int focused;
} nx_input_snapshot;

#define NX_REGION_FOCUSABLE 1u
#define NX_REGION_DISABLED  2u
#define NX_REGION_SCROLLABLE 4u

typedef struct nx_interaction_state {
    int hovered;
    int held;
    int focused;
    unsigned clicked; /* Number of activations delivered this frame. */
    int pointer_pressed, pointer_released; /* Left-button capture transitions. */
    float pointer_x, pointer_y; /* Last position while this region held capture. */
    float drag_delta_x, drag_delta_y; /* Movement accumulated during capture. */
    float wheel_x, wheel_y; /* Wheel units routed to this scrollable region. */
} nx_interaction_state;

#define NX_WINDOW_HIDDEN 1u

typedef struct nx_window_desc {
    const char *title_utf8; /* NUL-terminated; NULL selects "NexaUI". Borrowed. */
    float width, height;    /* Positive finite logical client size. */
    uint32_t flags;         /* NX_WINDOW_HIDDEN or zero. */
    nx_allocator allocator; /* Optional callback pair. */
} nx_window_desc;

typedef struct nx_window_info {
    float width, height; /* Current logical client size. */
    float scale;         /* Native backing pixels per logical pixel. */
    int should_close;    /* Set when the OS requests closure. */
} nx_window_info;

typedef enum nx_cursor_shape {
    NX_CURSOR_DEFAULT = 0,
    NX_CURSOR_TEXT,
    NX_CURSOR_LINK,
    NX_CURSOR_CROSSHAIR,
    NX_CURSOR_RESIZE_HORIZONTAL,
    NX_CURSOR_RESIZE_VERTICAL,
    NX_CURSOR_RESIZE_DIAGONAL_NW_SE,
    NX_CURSOR_RESIZE_DIAGONAL_NE_SW,
    NX_CURSOR_MOVE,
    NX_CURSOR_NOT_ALLOWED,
    NX_CURSOR_BUSY
} nx_cursor_shape;

typedef struct nx_file_drop_info {
    float x, y; /* Logical client coordinates at release. */
    uint32_t index, count; /* Zero-based path index and total paths in this drop. */
} nx_file_drop_info;

/* Native adapters exist on Windows, Linux and initially macOS/AppKit. Linux selects Wayland when a
   compositor is available, then X11 as fallback; NEXAUI_PLATFORM=x11 or
   wayland explicitly selects one adapter. Create, poll, read and destroy a
   window on the same OS event thread; macOS requires the main thread. No native
   handle leaks through this API. Poll waits up to timeout_ms and dispatches
   native events; Linux polls each window connection, while Win32 pumps its
   thread's windows and AppKit dispatches process-wide events. See the platform
   validation reports for native acceptance status. */
NX_API nx_result nx_window_create(const nx_window_desc *desc, nx_window **out_window);
NX_API nx_result nx_window_poll(nx_window *window, uint32_t timeout_ms);
NX_API nx_result nx_window_get_info(nx_window *window, nx_window_info *out_info);
/* Select the cursor for this window's client area. The shape persists until
   changed, is independent for each window, and takes effect on the next native
   cursor update (or immediately while this window owns the pointer). Call on
   the window's creation thread. Invalid shapes return NX_ERROR_ARGUMENT;
   unsupported native backends return NX_ERROR_UNSUPPORTED. */
NX_API nx_result nx_window_set_cursor(nx_window *window, nx_cursor_shape shape);
/* Query the native client-area animation preference on the window's thread.
   On Windows this reads the current system setting; zero means ordinary
   motion is allowed, one requests reduced motion. Call again after a system
   setting change (or before each frame if no change notification is used) and
   pass the result to nx_context_set_reduced_motion between frames. Other
   backends currently return NX_ERROR_UNSUPPORTED. */
NX_API nx_result nx_window_get_reduced_motion(nx_window *window,
                                              int *out_enabled);
/* Pop one normalized event after polling. *available is zero when the queue
   is empty. Event storage belongs to the window and is copied to out_event. */
NX_API nx_result nx_window_next_event(nx_window *window, nx_event *out_event,
                                      int *available);
/* Read one dropped filesystem path from the window's separate drop queue.
   Poll first. Paths within one drop retain their order and share count and
   coordinates. Query with path_utf8=NULL and capacity=0 to obtain the byte
   length excluding NUL without consuming the entry. A short buffer returns
   NX_ERROR_LIMIT and also leaves the entry queued. On success, writes a NUL
   terminated UTF-8 path and consumes it. out_info and out_length are cleared
   when no path is available. Call on the window's creation thread. This API
   currently receives Windows Shell filesystem drops; virtual files and other
   payload types are outside its contract. */
NX_API nx_result nx_window_next_file_drop(nx_window *window,
                                          char *path_utf8, size_t capacity,
                                          size_t *out_length,
                                          nx_file_drop_info *out_info,
                                          int *available);
/* Transfer queued native events to one UI context in order. A successful
   transfer removes each event from the window queue. If the context cannot
   accept an event, that event and all following events remain queued for a
   later retry; *out_count reports the events already transferred. Call on
   the window's creation/UI thread, once per window after polling. Use a
   separate context for each window to keep focus and pointer state isolated. */
NX_API nx_result nx_window_route_events(nx_window *window, nx_context *context,
                                        size_t *out_count);
/* Transfer UTF-8 plain text through the native clipboard on the window's
   creation thread. Set rejects embedded NUL and invalid UTF-8. Get queries
   the byte count excluding NUL when bytes is NULL and capacity is zero;
   otherwise capacity must fit that count plus a terminator. Insufficient
   capacity returns NX_ERROR_LIMIT with the required count and writes no bytes.
   Set converts lone CR and LF to Windows CRLF; get converts CRLF to LF.
   An unavailable text format returns NX_ERROR_STATE. Unsupported backends
   return NX_ERROR_UNSUPPORTED. Native clipboard ownership is process-external. */
NX_API nx_result nx_window_clipboard_set_text(nx_window *window,
                                               const char *bytes,
                                               size_t length);
NX_API nx_result nx_window_clipboard_get_text(nx_window *window,
                                               char *bytes, size_t capacity,
                                               size_t *out_length);
NX_API void nx_window_destroy(nx_window *window);

/* A fixed-capacity scratch arena. It owns one buffer and makes no allocation
   during nx_arena_alloc. Reset invalidates all previously returned pointers. */
NX_API nx_result nx_arena_create(const nx_allocator *allocator, size_t capacity,
                                 nx_arena **out_arena);
NX_API void nx_arena_destroy(nx_arena *arena);
NX_API nx_result nx_arena_alloc(nx_arena *arena, size_t size, size_t alignment,
                                void **out_pointer);
NX_API void nx_arena_reset(nx_arena *arena);
NX_API size_t nx_arena_used(const nx_arena *arena);
NX_API size_t nx_arena_capacity(const nx_arena *arena);

typedef struct nx_rect {
    float x, y, width, height;
} nx_rect;
/* Place native IME UI beside a logical client-area caret rectangle.
   Call on the window's creation thread after laying out the focused editor,
   and again when its caret moves. Windows can draw composition in the field
   while the native IME controls candidates; placement is best effort.
   X11 requires an input method that
   supports the preedit-position style. Other backends may return
   NX_ERROR_UNSUPPORTED. */
NX_API nx_result nx_window_set_ime_rect(nx_window *window, nx_rect caret);
/* Enable or disable focused-editor IME handling. Wayland text-input requires
   explicit protocol activation. Windows and X11 use this to select in-field
   preedit presentation where available. Call again when focus leaves the
   editor. Other backends may return NX_ERROR_UNSUPPORTED. */
NX_API nx_result nx_window_set_ime_enabled(nx_window *window, int enabled);
typedef enum nx_ime_purpose {
    NX_IME_PURPOSE_NORMAL = 0,
    NX_IME_PURPOSE_PASSWORD = 1
} nx_ime_purpose;
/* Set the focused editor's input purpose before enabling its IME.
   Password purpose requests sensitive handling where the backend supports
   it and never authorizes sending password surrounding text. */
NX_API nx_result nx_window_set_ime_purpose(nx_window *window,
                                            nx_ime_purpose purpose);
/* Read the current, uncommitted Windows, X11, Wayland or AppKit composition.
   Query with bytes=NULL and capacity=0 for the UTF-8 byte count; a non-NULL buffer must
   fit the text plus NUL. Cursor offsets are UTF-8 byte offsets into that
   text, or both -1 when the IME hides its caret. This state belongs to the
   window and changes as native messages are dispatched. Other backends may
   return NX_ERROR_UNSUPPORTED. */
NX_API nx_result nx_window_get_ime_preedit(nx_window *window,
    char *bytes, size_t capacity, size_t *out_length,
    int32_t *out_cursor_begin, int32_t *out_cursor_end);
/* Describe the focused editor's UTF-8 text near its cursor to Wayland v3/v1.
   Supply at most 4000 bytes, complete selection, and byte offsets relative
   to bytes. target is the editor's stable nonzero ID; revision must change
   whenever its text or selection changes. A zero target with empty data
   withdraws the editor state until a valid state is supplied; a nonzero
   target with empty bytes keeps the editor identity but gives no context.
   Other backends return NX_ERROR_UNSUPPORTED. */
NX_API nx_result nx_window_set_ime_surrounding_text(nx_window *window,
    uint64_t target, uint64_t revision, const char *bytes, size_t length,
    size_t cursor, size_t anchor);

typedef struct nx_utf8_span {
    const char *data;
    size_t length;
} nx_utf8_span;

typedef struct nx_bytes_view {
    const uint8_t *data;
    size_t length;
    uint32_t checksum; /* SFNT directory value; not verified yet. */
} nx_bytes_view;

typedef struct nx_font_metrics {
    uint16_t units_per_em, glyph_count, number_of_hmetrics;
    int16_t ascender, descender, line_gap;
    int16_t index_to_loc_format;
} nx_font_metrics;

typedef struct nx_glyph_point {
    int32_t x, y; /* Unscaled font design units. */
    uint8_t on_curve;
} nx_glyph_point;

typedef struct nx_glyph_bounds {
    int16_t x_min, y_min, x_max, y_max;
} nx_glyph_bounds;

typedef struct nx_glyph_outline_buffer {
    nx_glyph_point *points;
    size_t point_capacity;
    uint16_t *contour_ends; /* Inclusive point indices, in contour order. */
    size_t contour_capacity;
    size_t point_count, contour_count;
    nx_glyph_bounds bounds;
} nx_glyph_outline_buffer;

typedef struct nx_glyph_mask_view {
    const uint8_t *pixels; /* Borrowed A8 pixels. */
    uint32_t width, height;
    size_t stride_bytes;
    int32_t x_offset, y_offset; /* Mask top-left relative to the glyph baseline. */
    double advance_x; /* Horizontal advance in pixels. */
} nx_glyph_mask_view;

typedef struct nx_glyph_cache_stats {
    size_t used_bytes, max_bytes, entry_count;
    uint64_t hits, misses, evictions;
} nx_glyph_cache_stats;

typedef struct nx_font_match {
    const nx_font *font; /* Borrowed from the collection. */
    uint32_t font_index; /* UINT32_MAX when no font has the scalar. */
    uint16_t glyph_index; /* Zero when no font has the scalar. */
} nx_font_match;

typedef struct nx_shaped_glyph {
    const nx_font *font; /* Borrowed from the collection. */
    uint32_t font_index, script_tag, scalar;
    size_t scalar_index, byte_start, byte_end, cluster_byte_start;
    uint16_t glyph_index; /* Zero denotes .notdef in the chosen font. */
    uint8_t bidi_level;
    double advance_x, offset_x, offset_y; /* Pixels at the requested ppem;
                                            positive offset_y moves up. */
} nx_shaped_glyph;

typedef struct nx_shape_run {
    const nx_font *font;
    uint32_t font_index, script_tag; /* ISO 15924 four-character tag. */
    size_t glyph_start, glyph_count;
    uint8_t bidi_level;
} nx_shape_run;

typedef struct nx_text_line {
    size_t byte_start, byte_end, break_byte_end;
    size_t glyph_start, glyph_count;
    double top, baseline_y, width, height;
    uint8_t paragraph_level, hard_break;
} nx_text_line;

typedef struct nx_text_glyph {
    nx_shaped_glyph shape;
    size_t line_index;
    double x, baseline_y; /* Pen position; apply shape offsets when drawing. */
} nx_text_glyph;

typedef enum nx_caret_affinity {
    NX_CARET_UPSTREAM = 0,
    NX_CARET_DOWNSTREAM = 1
} nx_caret_affinity;

typedef struct nx_text_caret {
    size_t byte_offset, line_index;
    double x, top, height;
    nx_caret_affinity affinity;
} nx_text_caret;

typedef struct nx_text_selection_rect {
    size_t line_index;
    double x, y, width, height;
} nx_text_selection_rect;

typedef enum nx_semantic_role {
    NX_ROLE_WINDOW = 1,
    NX_ROLE_PANEL, NX_ROLE_GROUP, NX_ROLE_TEXT, NX_ROLE_BUTTON,
    NX_ROLE_CHECKBOX, NX_ROLE_RADIO, NX_ROLE_SWITCH, NX_ROLE_SLIDER,
    NX_ROLE_TEXT_FIELD, NX_ROLE_IMAGE, NX_ROLE_LIST, NX_ROLE_LIST_ITEM,
    NX_ROLE_TABLE, NX_ROLE_ROW, NX_ROLE_CELL, NX_ROLE_TAB, NX_ROLE_TAB_PANEL,
    NX_ROLE_MENU, NX_ROLE_MENU_ITEM, NX_ROLE_TOOLBAR, NX_ROLE_STATUS,
    NX_ROLE_TREE, NX_ROLE_TREE_ITEM, NX_ROLE_PROGRESS, NX_ROLE_SEPARATOR,
    NX_ROLE_CUSTOM, NX_ROLE_SCROLL_AREA, NX_ROLE_COLUMN_HEADER,
    NX_ROLE_POPUP, NX_ROLE_DIALOG, NX_ROLE_TOOLTIP, NX_ROLE_MENU_BAR,
    NX_ROLE_TAB_LIST
} nx_semantic_role;

#define NX_SEM_DISABLED   1u
#define NX_SEM_CHECKED    2u
#define NX_SEM_MIXED      4u
#define NX_SEM_EXPANDED   8u
#define NX_SEM_SELECTED  16u
#define NX_SEM_READONLY  32u
#define NX_SEM_REQUIRED  64u
#define NX_SEM_INVALID  128u
#define NX_SEM_BUSY     256u
#define NX_SEM_FOCUSED  512u /* Derived from nx_focus_id; do not set in desc. */
#define NX_SEM_PROTECTED 1024u /* Value is sensitive; semantic value is empty. */
/* Derived by nx_selection_group; not accepted in ordinary semantic descs. */
#define NX_SEM_SELECTION_SINGLE 2048u
#define NX_SEM_SELECTION_REQUIRED 4096u

#define NX_ACTION_PRESS       1u
#define NX_ACTION_FOCUS       2u
#define NX_ACTION_INCREMENT   4u
#define NX_ACTION_DECREMENT   8u
#define NX_ACTION_SET_VALUE  16u
#define NX_ACTION_EXPAND     32u
#define NX_ACTION_COLLAPSE   64u
#define NX_ACTION_SELECT    128u
/* Derived capabilities of explicit selection-group members. */
#define NX_ACTION_ADD_SELECTION 256u
#define NX_ACTION_DESELECT 512u

typedef struct nx_semantic_desc {
    uint64_t id, parent_id; /* parent_id zero makes a root. */
    nx_semantic_role role;
    nx_rect bounds;
    nx_utf8_span name, description, value; /* Borrowed for this call. */
    uint32_t state, actions;
} nx_semantic_desc;

#define NX_WIDGET_INTERACTIVE 1u
#define NX_WIDGET_FOCUSABLE   2u
#define NX_WIDGET_SCROLLABLE  4u

typedef struct nx_widget_desc {
    nx_semantic_desc semantic; /* ID and bounds are shared with hit testing. */
    uint32_t flags; /* NX_WIDGET_INTERACTIVE and optionally NX_WIDGET_FOCUSABLE. */
} nx_widget_desc;

/* S04: a real-time line plot over one borrowed S03 series. The application
   owns state and a reusable envelope buffer of at least one element per
   logical plot pixel, capped at 16384. Ranges are finite, increasing and
   caller-controlled; the widget never changes them. Optional fonts draw
   numeric tick labels at integer pixels_per_em in [1,16384]. Zero-initialize
   state before its first frame. */
typedef struct nx_plot_desc {
    uint64_t id, parent_id;
    nx_rect bounds;
    nx_utf8_span name;
    nx_series_view series;
    double x_min, x_max, y_min, y_max;
    nx_series_envelope *scratch;
    size_t scratch_capacity;
    const nx_font_collection *fonts;
    double pixels_per_em;
    size_t glyph_cache_bytes;
    int disabled;
} nx_plot_desc;

typedef struct nx_plot_state {
    nx_interaction_state interaction;
    double cursor_x;
    int cursor_valid;
} nx_plot_state;

/* Pointer hover/capture places the crosshair; focused Left/Right moves it by
   one logical plot pixel. A successful call updates state after painting. */
NX_API nx_result nx_plot(nx_context *context, const nx_plot_desc *desc,
                         nx_plot_state *state);

/* S05: caller-owned telemetry/profiler records, usually inside an acquired
   immutable simulation snapshot. Panels borrow records only for the call and
   visit at most the rows that fit after first_row; scrolling stays with the
   application. IDs must be nonzero and unique among visible records and the
   panel. Optional fonts draw labels and numbers at integer pixels_per_em in
   [1,16384]. A zero row_height chooses a theme-based minimum; supplied fonts
   may increase it. At most 4096 rows are visited per call. */
typedef enum nx_telemetry_level {
    NX_TELEMETRY_NORMAL = 0,
    NX_TELEMETRY_WARNING = 1,
    NX_TELEMETRY_CRITICAL = 2
} nx_telemetry_level;

typedef struct nx_telemetry_metric {
    uint64_t id;
    nx_utf8_span name, unit, status; /* Unit <=64 bytes; warning/critical
                                       require caller-localized status text. */
    double value, minimum, maximum; /* Finite; minimum < maximum. */
    nx_telemetry_level level;
} nx_telemetry_metric;

typedef struct nx_telemetry_panel_desc {
    uint64_t id, parent_id;
    nx_rect bounds;
    nx_utf8_span name;
    const nx_telemetry_metric *metrics;
    size_t metric_count, first_row;
    float row_height;
    const nx_font_collection *fonts;
    double pixels_per_em;
    size_t glyph_cache_bytes;
} nx_telemetry_panel_desc;

typedef struct nx_profiler_entry {
    uint64_t id;
    nx_utf8_span name;
    double inclusive_ms, self_ms; /* Finite, nonnegative; self <= inclusive. */
    uint64_t calls;
    uint32_t depth; /* Visual indentation, capped to the row width. */
} nx_profiler_entry;

typedef struct nx_profiler_panel_desc {
    uint64_t id, parent_id;
    nx_rect bounds;
    nx_utf8_span name;
    const nx_profiler_entry *entries;
    size_t entry_count, first_row;
    double frame_ms; /* Finite and positive, used for bar scale. */
    float row_height;
    const nx_font_collection *fonts;
    double pixels_per_em;
    size_t glyph_cache_bytes;
} nx_profiler_panel_desc;

NX_API nx_result nx_telemetry_panel(nx_context *context,
                                    const nx_telemetry_panel_desc *desc);
NX_API nx_result nx_profiler_panel(nx_context *context,
                                   const nx_profiler_panel_desc *desc);

/* S06: one inspector property row over caller-owned, immutable values.
   The widget never writes into a simulation snapshot. Boolean activation and
   focused number Left/Right produce a proposed value; activating number/text
   rows requests an external editor. The application owns command dispatch,
   pending state and any text/numeric editor it opens. */
typedef enum nx_property_kind {
    NX_PROPERTY_BOOLEAN = 1,
    NX_PROPERTY_NUMBER = 2,
    NX_PROPERTY_TEXT = 3
} nx_property_kind;

typedef struct nx_property_row_desc {
    uint64_t id, parent_id;
    nx_rect bounds;
    nx_utf8_span name, text_value, unit;
    nx_property_kind kind;
    int boolean_value;
    double number_value, minimum, maximum, step;
    int read_only;
    const nx_font_collection *fonts;
    double pixels_per_em;
    size_t glyph_cache_bytes;
} nx_property_row_desc;

typedef struct nx_property_action {
    nx_interaction_state interaction;
    int changed, edit_requested;
    int boolean_value;
    double number_value;
} nx_property_action;

NX_API nx_result nx_property_row(nx_context *context,
                                 const nx_property_row_desc *desc,
                                 nx_property_action *out_action);

typedef struct nx_semantic_view {
    uint64_t id, parent_id;
    unsigned depth;
    nx_semantic_role role;
    nx_rect bounds;
    nx_utf8_span name, description, value; /* Borrowed until next begin_frame. */
    uint32_t state, actions;
} nx_semantic_view;

typedef struct nx_size {
    float width, height;
} nx_size;

typedef struct nx_insets {
    float left, top, right, bottom;
} nx_insets;

typedef enum nx_layout_kind {
    NX_LAYOUT_LEAF = 1,
    NX_LAYOUT_ROW = 2,
    NX_LAYOUT_COLUMN = 3,
    NX_LAYOUT_OVERLAY = 4,
    NX_LAYOUT_SCROLL = 5,
    NX_LAYOUT_WRAP_ROW = 6,
    NX_LAYOUT_WRAP_COLUMN = 7,
    NX_LAYOUT_GRID = 8,
    NX_LAYOUT_SPLIT = 9
} nx_layout_kind;

typedef enum nx_split_axis {
    NX_SPLIT_X = 1, /* Side by side. */
    NX_SPLIT_Y = 2  /* Top and bottom. */
} nx_split_axis;

typedef enum nx_grid_track_kind {
    NX_GRID_TRACK_FIXED = 1,
    NX_GRID_TRACK_AUTO = 2,
    NX_GRID_TRACK_FLEX = 3
} nx_grid_track_kind;

typedef struct nx_grid_track {
    nx_grid_track_kind kind;
    float value; /* Fixed size or positive flex weight; zero for auto. */
    float minimum;
    float maximum; /* May be positive infinity. */
} nx_grid_track;

#define NX_SCROLL_X 1u
#define NX_SCROLL_Y 2u

typedef enum nx_align {
    NX_ALIGN_START = 1,
    NX_ALIGN_CENTER = 2,
    NX_ALIGN_END = 3,
    NX_ALIGN_STRETCH = 4
} nx_align;

typedef struct nx_axis_spec {
    float minimum;
    float preferred;
    float maximum;   /* May be positive infinity. */
    float grow;
    float shrink;
    int has_preferred; /* Zero uses measured intrinsic size. */
} nx_axis_spec;

typedef struct nx_layout_desc {
    nx_layout_kind kind;
    nx_axis_spec width, height;
    nx_insets padding, border, margin;
    float gap; /* Main-axis gap; horizontal gap for grids. */
    float line_gap; /* Gap between wrap lines; vertical gap for grids. */
    nx_align align_x, align_y;
    uint32_t scroll_axes; /* NX_SCROLL_X and/or NX_SCROLL_Y for scroll nodes. */
    nx_split_axis split_axis; /* Only for split nodes. */
    float split_ratio; /* Initial fraction assigned to the first pane. */
} nx_layout_desc;

typedef struct nx_scroll_info {
    nx_rect viewport; /* Push as a clip while drawing or registering content. */
    nx_size content_size; /* Includes the content child's margins. */
    float offset_x, offset_y; /* Effective clamped offsets. */
    float max_x, max_y;
} nx_scroll_info;

typedef struct nx_virtual_range {
    size_t first, end; /* Half-open row range including bounded overscan. */
    double first_offset; /* Content Y position of row first. */
    double total_extent;
    double effective_offset; /* Clamped viewport offset. */
} nx_virtual_range;

/* A Fenwick height index. Rows keep positive finite heights; updates and
   offset/range queries take O(log count). Overscan is capped at 256 rows. */
NX_API nx_result nx_virtual_list_create(size_t count, double default_height,
                                        const nx_allocator *allocator,
                                        nx_virtual_list **out_list);
NX_API void nx_virtual_list_destroy(nx_virtual_list *list);
NX_API nx_result nx_virtual_list_set_height(nx_virtual_list *list,
                                            size_t index, double height);
NX_API nx_result nx_virtual_list_offset_at(const nx_virtual_list *list,
                                           size_t index, double *out_offset);
NX_API nx_result nx_virtual_list_range(const nx_virtual_list *list,
                                       double offset, double viewport_height,
                                       size_t overscan_rows,
                                       nx_virtual_range *out_range);
/* Called once per row during a rebuild. Keys must be unique and nonzero;
   heights must be finite and positive. The callback may read, but must not
   mutate, the old list. */
typedef nx_result (*nx_virtual_list_row_fn)(void *user, size_t index,
                                             uint64_t *out_key,
                                             double *out_height);
/* Rebuilds keys and measured heights after insertion, removal or reordering.
   Initial keys are index + 1. The visible row at *io_offset is anchored by
   key; if absent, its old absolute offset is clamped to the new extent.
   The list and *io_offset remain unchanged on any failure. O(count) time
   and temporary memory; use set_height for individual height changes. */
NX_API nx_result nx_virtual_list_rebuild(nx_virtual_list *list,
                                          size_t count,
                                          nx_virtual_list_row_fn row,
                                          void *user,
                                          double viewport_height,
                                          double *io_offset);
typedef struct nx_virtual_row {
    size_t index;
    uint64_t key; /* Stable row ID supplied at rebuild; initially index + 1. */
    double content_y; /* Distance from the content origin. */
    double viewport_y; /* content_y minus the effective scroll offset. */
    double height;
} nx_virtual_row;

typedef nx_result (*nx_virtual_list_visit_fn)(void *user,
                                               const nx_virtual_row *row);
/* Visits exactly the rows in nx_virtual_list_range, in content order. The
   callback may register widgets using an ID derived from row.key and the
   viewport_y position, but must not mutate the list. max_rows is a hard
   preflight budget: exceeding it returns
   NX_ERROR_LIMIT without invoking the callback. out_visited is the number of
   successful callbacks, including on callback failure; effects of earlier
   callbacks remain the caller's responsibility. No allocation occurs. */
NX_API nx_result nx_virtual_list_visit(const nx_virtual_list *list,
                                        double offset, double viewport_height,
                                        size_t overscan_rows, size_t max_rows,
                                        nx_virtual_list_visit_fn visit,
                                        void *user, nx_virtual_range *out_range,
                                        size_t *out_visited);

typedef struct nx_split_info {
    nx_rect divider; /* Draw and register this area as a drag handle. */
    float first_extent, second_extent; /* Outer pane sizes, including margins. */
    float effective_ratio;
} nx_split_info;

typedef struct nx_splitter_desc {
    uint64_t id, parent_id;
    nx_utf8_span name;
    nx_split_axis axis;
    nx_split_info split; /* From the most recent nx_layout_get_split_info. */
    float minimum_ratio, maximum_ratio; /* Inclusive; defaults are 0 and 1. */
    float keyboard_step; /* Ratio step; zero selects 0.05. */
    int disabled;
} nx_splitter_desc;

/* Build a reusable, parent-first layout tree. Node handles are indices valid
   until reset or destruction. All calls require exclusive use on one thread.
   A leaf cannot own children. Each measurement/arrangement addresses one root. */
NX_API nx_layout_desc nx_layout_default_desc(nx_layout_kind kind);
NX_API nx_result nx_layout_create(const nx_allocator *allocator,
                                  size_t initial_node_capacity, nx_layout **out_layout);
NX_API void nx_layout_destroy(nx_layout *layout);
NX_API void nx_layout_reset(nx_layout *layout);
NX_API nx_result nx_layout_add(nx_layout *layout, uint32_t parent,
                               const nx_layout_desc *desc, uint32_t *out_node);
/* Add a fixed-size, non-rendering leaf to reserve space in a layout. */
NX_API nx_result nx_layout_spacer(nx_layout *layout, uint32_t parent,
                                  float width, float height, uint32_t *out_node);
/* Configure a grid before adding children. Track arrays are copied. Grid
   children require explicit zero-based row/column placement and spans. */
NX_API nx_result nx_layout_grid_configure(nx_layout *layout, uint32_t grid,
                                          const nx_grid_track *columns, size_t column_count,
                                          const nx_grid_track *rows, size_t row_count);
NX_API nx_result nx_layout_add_grid(nx_layout *layout, uint32_t grid,
                                    const nx_layout_desc *desc,
                                    uint32_t row, uint32_t column,
                                    uint32_t row_span, uint32_t column_span,
                                    uint32_t *out_node);
/* Available dimensions are finite nonnegative values or positive infinity.
   The desired size may exceed available space when minimums require it. */
NX_API nx_result nx_layout_measure(nx_layout *layout, uint32_t root,
                                   float available_width, float available_height,
                                   nx_size *out_desired);
/* Arrange after a successful measure of the same root. The root rectangle is
   the assigned border box; children may overflow when minima cannot fit. */
NX_API nx_result nx_layout_arrange(nx_layout *layout, uint32_t root, nx_rect rect);
NX_API nx_result nx_layout_get_rect(const nx_layout *layout, uint32_t node,
                                    nx_rect *out_rect);
/* A scroll node owns at most one content child. Setting an offset requires a
   new arrange call, but not a new measure call. */
NX_API nx_result nx_layout_set_scroll(nx_layout *layout, uint32_t node,
                                      float offset_x, float offset_y);
NX_API nx_result nx_layout_get_scroll_info(const nx_layout *layout, uint32_t node,
                                           nx_scroll_info *out_info);
/* A split node requires exactly two children. Changing its ratio invalidates
   arrangement but does not require a new measurement. */
NX_API nx_result nx_layout_set_split(nx_layout *layout, uint32_t node, float ratio);
NX_API nx_result nx_layout_get_split_info(const nx_layout *layout, uint32_t node,
                                          nx_split_info *out_info);
/* Draw/register the arranged divider and update a caller-owned requested ratio.
   Apply a changed ratio with nx_layout_set_split and arrange the next frame.
   Dragging starts from split.effective_ratio so constrained panes do not jump. */
NX_API nx_result nx_splitter(nx_context *context, const nx_splitter_desc *desc,
                            float *ratio, nx_interaction_state *out_state);

typedef enum nx_dock_kind {
    NX_DOCK_STACK = 1, NX_DOCK_SPLIT = 2
} nx_dock_kind;

typedef struct nx_dock_node {
    uint64_t id, parent_id;
    nx_dock_kind kind;
    nx_split_axis axis; /* Split only. */
    float ratio; /* First child's requested fraction; split only. */
    uint64_t first_id, second_id; /* Split children. */
    uint64_t selected_panel_id; /* Stack only. */
} nx_dock_node;

typedef struct nx_dock_tab {
    uint64_t panel_id, stack_id;
} nx_dock_tab;

/* Caller-owned buffers. Nodes form a binary tree; tabs appear in array order
   within each stack. IDs are nonzero and unique in their respective spaces. */
typedef struct nx_dock_model {
    nx_dock_node *nodes;
    size_t node_count, node_capacity;
    nx_dock_tab *tabs;
    size_t tab_count, tab_capacity;
    uint64_t root_id;
} nx_dock_model;

NX_API nx_result nx_dock_init(nx_dock_model *model,
                              nx_dock_node *nodes, size_t node_capacity,
                              nx_dock_tab *tabs, size_t tab_capacity,
                              uint64_t root_stack_id, uint64_t first_panel_id);
NX_API nx_result nx_dock_add_tab(nx_dock_model *model, uint64_t stack_id,
                                 uint64_t panel_id);
NX_API nx_result nx_dock_split(nx_dock_model *model, uint64_t stack_id,
                               uint64_t split_id, uint64_t new_stack_id,
                               uint64_t new_panel_id, nx_split_axis axis,
                               float ratio, int new_stack_after);
/* A move appends to the target stack. The source stack must retain a tab. */
NX_API nx_result nx_dock_move(nx_dock_model *model, uint64_t panel_id,
                              uint64_t target_stack_id);
NX_API nx_result nx_dock_select(nx_dock_model *model, uint64_t panel_id);
NX_API nx_result nx_dock_set_ratio(nx_dock_model *model, uint64_t split_id,
                                   float ratio);
NX_API nx_result nx_dock_validate(const nx_dock_model *model);
/* Stable little-endian v1 format. A NULL destination queries required bytes.
   Decode clears node_count/tab_count/root_id on malformed input or capacity error. */
NX_API nx_result nx_dock_encode(const nx_dock_model *model, void *bytes,
                                size_t capacity, size_t *out_size);
NX_API nx_result nx_dock_decode(nx_dock_model *model, const void *bytes,
                                size_t length);

typedef struct nx_color {
    float r, g, b, a;
} nx_color;

typedef struct nx_hsv {
    float h, s, v, a; /* Hue [0,1], where 1 wraps to 0. */
} nx_hsv;

/* Components are finite normalized sRGB values. Outputs are unchanged on
   invalid input. Achromatic colors convert to hue zero. */
NX_API nx_result nx_color_to_hsv(nx_color color, nx_hsv *out_hsv);
NX_API nx_result nx_color_from_hsv(nx_hsv hsv, nx_color *out_color);
/* Parse #RGB, #RGBA, #RRGGBB or #RRGGBBAA. Missing alpha becomes 1.
   Formatting emits #RRGGBB or #RRGGBBAA with nearest 8-bit rounding.
   Passing bytes=NULL queries the non-NUL text length in out_length. */
NX_API nx_result nx_color_parse_hex(nx_utf8_span text, nx_color *out_color);
NX_API nx_result nx_color_format_hex(nx_color color, int include_alpha,
                                     char *bytes, size_t capacity,
                                     size_t *out_length);

typedef enum nx_color_token {
    NX_COLOR_BACKGROUND = 0,
    NX_COLOR_SURFACE,
    NX_COLOR_SURFACE_RAISED,
    NX_COLOR_TEXT,
    NX_COLOR_TEXT_MUTED,
    NX_COLOR_BORDER,
    NX_COLOR_ACCENT,
    NX_COLOR_ACCENT_TEXT,
    NX_COLOR_HOVER,
    NX_COLOR_PRESSED,
    NX_COLOR_DISABLED_BACKGROUND,
    NX_COLOR_DISABLED_TEXT,
    NX_COLOR_FOCUS,
    NX_COLOR_ERROR,
    NX_COLOR_SUCCESS,
    NX_COLOR_WARNING,
    NX_COLOR_SELECTION,
    NX_COLOR_SELECTION_TEXT,
    NX_COLOR_SHADOW,
    NX_COLOR_TOKEN_COUNT
} nx_color_token;

typedef enum nx_metric_token {
    NX_METRIC_SPACE_XS = 0,
    NX_METRIC_SPACE_SM,
    NX_METRIC_SPACE_MD,
    NX_METRIC_SPACE_LG,
    NX_METRIC_CONTROL_HEIGHT,
    NX_METRIC_BORDER_WIDTH,
    NX_METRIC_FOCUS_WIDTH,
    NX_METRIC_RADIUS_SM,
    NX_METRIC_RADIUS_MD,
    NX_METRIC_SHADOW_BLUR,
    NX_METRIC_SHADOW_OFFSET_Y,
    NX_METRIC_FONT_SIZE,
    NX_METRIC_LINE_HEIGHT,
    NX_METRIC_DISABLED_OPACITY,
    NX_METRIC_TOKEN_COUNT
} nx_metric_token;

typedef enum nx_theme_kind {
    NX_THEME_DARK = 1,
    NX_THEME_LIGHT = 2,
    NX_THEME_HIGH_CONTRAST = 3,
    NX_THEME_MIDNIGHT = 4 /* Deep blue surfaces and cyan accents. */
} nx_theme_kind;

typedef struct nx_theme {
    nx_color colors[NX_COLOR_TOKEN_COUNT];
    float metrics[NX_METRIC_TOKEN_COUNT];
    uint64_t color_mask, metric_mask; /* Unset tokens inherit. */
} nx_theme;

NX_API nx_result nx_theme_preset(nx_theme_kind kind, nx_theme *out_theme);
NX_API nx_result nx_theme_set_color(nx_theme *theme, nx_color_token token,
                                    nx_color color);
NX_API nx_result nx_theme_set_metric(nx_theme *theme, nx_metric_token token,
                                     float value);
NX_API nx_result nx_theme_resolve(const nx_theme *parent,
                                  const nx_theme *overrides, nx_theme *out_theme);
/* Set a base theme between frames. NULL selects the dark preset. Within a
   frame, push partial overrides for a subtree and pop before end_frame. */
NX_API nx_result nx_context_set_theme(nx_context *context, const nx_theme *theme);
NX_API nx_result nx_push_theme(nx_context *context, const nx_theme *overrides);
NX_API nx_result nx_pop_theme(nx_context *context);
NX_API nx_result nx_theme_color(const nx_context *context,
                                nx_color_token token, nx_color *out_color);
NX_API nx_result nx_theme_metric(const nx_context *context,
                                 nx_metric_token token, float *out_value);

typedef enum nx_easing {
    NX_EASING_LINEAR = 1,
    NX_EASING_SMOOTHSTEP = 2
} nx_easing;

/* Caller-owned scalar transition state. Zero-initialize before first use or
   call nx_transition_reset to choose an explicit starting value. The fields
   are implementation state and must not be modified directly. Keep each
   transition on one UI context; reset it before moving it to another. */
typedef struct nx_transition {
    double value, start, target, elapsed, duration;
    uint64_t last_frame_serial;
    nx_easing easing;
    int initialized, active;
} nx_transition;

/* Set a context's motion preference between frames; zero is the default.
   Native OS preference discovery is currently the application's job. */
NX_API nx_result nx_context_set_reduced_motion(nx_context *context, int enabled);
NX_API nx_result nx_context_get_reduced_motion(const nx_context *context,
                                               int *out_enabled);
NX_API nx_result nx_transition_reset(nx_transition *transition, double value);
/* Call during frame construction. The first use of a zeroed transition snaps
   to target. Later target changes begin from its current value and progress by
   delta_seconds at most once per frame. A zero duration or reduced motion
   snaps to target. *out_active is true while another frame can change value.
   No input processing, semantics or drawing waits for the transition. */
NX_API nx_result nx_transition_update(nx_context *context,
                                       nx_transition *transition,
                                       double target, double duration_seconds,
                                       nx_easing easing, double *out_value,
                                       int *out_active);

typedef struct nx_point {
    float x, y;
} nx_point;

typedef struct nx_control_desc {
    uint64_t id, parent_id;
    nx_rect bounds;
    nx_utf8_span name; /* Required accessible name; copied during the call. */
    const nx_text_layout *label_layout; /* Optional visible text. */
    nx_point label_origin;
    size_t glyph_cache_bytes; /* Positive when label_layout is present. */
    int disabled;
    nx_transition *switch_transition; /* Optional caller-owned toggle thumb motion. */
    double switch_duration_seconds; /* Finite and nonnegative when used. */
    nx_easing switch_easing; /* Zero selects smoothstep. */
} nx_control_desc;

typedef struct nx_container_desc {
    uint64_t id, parent_id;
    nx_rect bounds;
    nx_utf8_span name; /* Accessible container name. */
    const nx_text_layout *label_layout; /* Optional visible title. */
    nx_point label_origin;
    size_t glyph_cache_bytes; /* Positive when label_layout is present. */
} nx_container_desc;

/* Portable vector icons, drawn with the same commands as other UI content.
   No font, native symbol set or image asset is required. Bounds are logical
   pixels; stroke_width is finite and positive. NONE appends no commands. */
typedef enum nx_icon {
    NX_ICON_NONE = 0, NX_ICON_CLOUD, NX_ICON_HOME, NX_ICON_FOLDER,
    NX_ICON_TRANSFER, NX_ICON_STAR, NX_ICON_TRASH, NX_ICON_SETTINGS,
    NX_ICON_SEARCH, NX_ICON_GRID, NX_ICON_LIST, NX_ICON_MORE,
    NX_ICON_CHEVRON_RIGHT, NX_ICON_CHEVRON_LEFT, NX_ICON_CLOSE,
    NX_ICON_PLUS, NX_ICON_UPLOAD, NX_ICON_DOWNLOAD, NX_ICON_LINK,
    NX_ICON_SHARE, NX_ICON_CHECK, NX_ICON_STORAGE, NX_ICON_THERMOMETER,
    NX_ICON_WIFI, NX_ICON_ACTIVITY, NX_ICON_IMAGE, NX_ICON_EDIT,
    NX_ICON_INFO, NX_ICON_SUN, NX_ICON_COUNT
} nx_icon;

NX_API nx_result nx_draw_icon(nx_context *context, nx_icon icon,
                               nx_rect bounds, float stroke_width,
                               nx_color color);

typedef struct nx_surface_style {
    nx_color background, border;
    float radius, border_width; /* Border lies inside bounds; zero hides it. */
} nx_surface_style;

/* Rounded surface with a matching rounded inside border. Atomic on failure. */
NX_API nx_result nx_draw_surface(nx_context *context, nx_rect bounds,
                                  const nx_surface_style *style);
/* Themed rounded content surface and PANEL semantics. Children use desc.id
   as parent_id. Does not open a scope or clip its children. */
NX_API nx_result nx_card(nx_context *context, const nx_container_desc *desc);

typedef enum nx_action_variant {
    NX_ACTION_PRIMARY = 1, NX_ACTION_SECONDARY, NX_ACTION_QUIET,
    NX_ACTION_NAVIGATION, NX_ACTION_TILE
} nx_action_variant;

typedef struct nx_action_desc {
    nx_control_desc control;
    nx_action_variant variant;
    nx_icon icon;
    nx_rect icon_bounds; /* Ignored for NONE; inside control.bounds otherwise. */
    int selected; /* Caller-owned selection; does not change on activation. */
    const nx_text_stamp *label_stamp; /* Optional cached label, instead of
        control.label_layout; uses control.label_origin. Borrowed through rendering. */
} nx_action_desc;

/* Buttons, navigation entries and selectable tiles share pointer, keyboard,
   disabled and focus behavior. Navigation exposes TAB/SELECT semantics; tiles
   expose LIST_ITEM/SELECT; others BUTTON/PRESS. The accessible name is required
   even for an icon-only action. Paint, hit region and semantics roll back on
   failure. out_state is cleared on failure. */
NX_API nx_result nx_action(nx_context *context, const nx_action_desc *desc,
                            nx_interaction_state *out_state);

typedef struct nx_progress_desc {
    uint64_t id, parent_id;
    nx_rect bounds;
    nx_utf8_span name;
    double value; /* Finite fraction in [0,1]. Semantic value is a percentage. */
} nx_progress_desc;

/* Read-only determinate progress, using selection and accent theme colors. */
NX_API nx_result nx_progress(nx_context *context, const nx_progress_desc *desc);

typedef struct nx_scroll_area_desc {
    uint64_t id, parent_id;
    uint64_t thumb_x_id, thumb_y_id; /* Optional unique focusable scrollbar
                                       IDs; zero leaves that axis wheel-only. */
    nx_rect viewport;
    nx_utf8_span name;
    nx_size content_size; /* Extent before clipping, at least zero. */
    uint32_t axes; /* NX_SCROLL_X and/or NX_SCROLL_Y. */
    float wheel_step; /* Logical pixels per wheel unit; zero selects 40. */
    int disabled;
} nx_scroll_area_desc;

typedef struct nx_table_sort {
    uint64_t column_id; /* Zero means unsorted. */
    int descending;
} nx_table_sort;

typedef struct nx_table_column_desc {
    uint64_t id, resize_id; /* resize_id zero makes the width fixed. */
    nx_utf8_span name;
    float width, minimum_width, maximum_width; /* Finite positive bounds. */
    int sortable;
    const nx_text_layout *label_layout;
    nx_point label_offset; /* Relative to the header rectangle. */
    size_t glyph_cache_bytes;
} nx_table_column_desc;

typedef struct nx_table_desc {
    uint64_t id, parent_id;
    nx_rect bounds; /* Full table semantic bounds. */
    nx_utf8_span name;
    float header_height;
    nx_table_column_desc *columns; /* Widths are updated after successful paint. */
    size_t column_count;
    int disabled;
} nx_table_desc;

typedef struct nx_tree_node {
    size_t parent_index; /* SIZE_MAX for a root; otherwise an active ancestor. */
    int expanded;
} nx_tree_node;

typedef struct nx_tree_item_desc {
    nx_control_desc control; /* Entire selectable item; parent is tree or cell. */
    uint64_t expander_id; /* Required for a branch, zero for a leaf. */
    nx_rect expander_bounds; /* Inside control.bounds for a branch. */
    int has_children;
} nx_tree_item_desc;

typedef struct nx_overlay_desc {
    nx_container_desc panel; /* Popup or modal semantic root. */
    uint64_t backdrop_id; /* Distinct full-window click region. */
    int dismiss_on_outside;
    int dismiss_on_escape;
} nx_overlay_desc;

typedef struct nx_menu_item_desc {
    nx_control_desc control;
    int checkable; /* checked must be supplied for a checkable item. */
} nx_menu_item_desc;

typedef struct nx_search_option {
    uint64_t id;
    nx_utf8_span name; /* Searched and exposed as the accessible row name. */
    const nx_text_layout *label_layout; /* Optional visible label. */
    nx_point label_offset; /* Relative to the row origin. */
    size_t glyph_cache_bytes;
    int disabled;
} nx_search_option;

typedef struct nx_search_state {
    nx_text_field *query; /* Borrowed; application owns its lifetime. */
    uint64_t active_id; /* Keyboard-highlighted result, or zero. */
    uint64_t selected_id; /* Committed selection, or zero. */
    size_t first_visible; /* Index into filtered results. */
} nx_search_state;

/* Stable ASCII-case-insensitive substring matching. Non-ASCII UTF-8 bytes
   match exactly. A too-small buffer is untouched; out_count is still set. */
NX_API nx_result nx_search_filter(const nx_search_option *options,
                                  size_t option_count, nx_utf8_span query,
                                  size_t *indices, size_t capacity,
                                  size_t *out_count);

/* Nodes are in preorder. With indices == NULL, only the visible count is
   returned. A too-small output buffer is untouched and reports NX_ERROR_LIMIT.
   Invalid ancestry and allocation failure leave output untouched. */
NX_API nx_result nx_tree_flatten(const nx_tree_node *nodes, size_t count,
                                 const nx_allocator *allocator,
                                 size_t *indices, size_t capacity,
                                 size_t *out_visible_count);

typedef int (*nx_table_compare_fn)(size_t left, size_t right, void *user);

/* Stable sort of source row indices. Output is untouched on allocation or
   argument failure. Compare receives original indices and must be pure. */
NX_API nx_result nx_table_sort_indices(size_t count,
                                       nx_table_compare_fn compare, void *user,
                                       int descending,
                                       const nx_allocator *allocator,
                                       size_t *indices, size_t capacity);

typedef struct nx_collapsible_desc {
    nx_container_desc container; /* Group; children use container.id. */
    uint64_t header_id; /* Focusable child button ID. */
    nx_rect header_bounds; /* Interactive header within container.bounds. */
    int disabled;
} nx_collapsible_desc;

typedef struct nx_value_control_desc {
    nx_control_desc control;
    double minimum, maximum; /* Finite, with maximum greater than minimum. */
    double step; /* Positive keyboard increment and pointer snap interval. */
    double units_per_pixel; /* Positive horizontal drag scale for drag value. */
} nx_value_control_desc;

typedef struct nx_numeric_field_desc {
    uint64_t id, parent_id;
    nx_rect bounds;
    nx_utf8_span name;
    const nx_font_collection *fonts; /* Borrowed through the widget call. */
    double pixels_per_em; /* Integer in [1, 16384] for text drawing. */
    size_t glyph_cache_bytes;
    double minimum, maximum; /* Finite inclusive validation range. */
    int disabled;
} nx_numeric_field_desc;

/* Return nonzero when the current draft is valid. Called during the widget
   call before and after focused edits; the text is borrowed. */
typedef int (*nx_text_field_validate_fn)(nx_utf8_span text, void *user);

/* Clipboard callbacks follow nx_window_clipboard_get/set_text: read(NULL,0)
   queries UTF-8 byte count, and a later read needs space for a terminating
   NUL. The widget handles Ctrl+C/X/V, and Command+C/X/V on macOS, when the
   relevant callback is present. */
typedef struct nx_text_clipboard {
    void *user;
    nx_result (*read)(void *user, char *bytes, size_t capacity,
                      size_t *out_length);
    nx_result (*write)(void *user, const char *bytes, size_t length);
} nx_text_clipboard;

/* Fill callbacks that forward to a native window clipboard. The window must
   outlive widget calls and stay on its creation thread. */
NX_API nx_result nx_text_clipboard_for_window(nx_window *window,
                                              nx_text_clipboard *out_clipboard);

typedef struct nx_text_field_desc {
    uint64_t id, parent_id;
    nx_rect bounds;
    nx_utf8_span name;
    const nx_font_collection *fonts;
    double pixels_per_em;
    size_t glyph_cache_bytes;
    int multiline, disabled;
    int password; /* Single-line only; requires a field created with
                     nx_text_field_create_password. Masks one glyph per
                     grapheme, suppresses semantic value and Ctrl+C/X. */
    nx_text_field_validate_fn validate; /* NULL accepts any draft. */
    void *validate_user;
    nx_text_clipboard clipboard; /* Optional; zero disables shortcuts. */
    nx_window *ime_window; /* Optional borrowed native window for OS IME
                            placement and Wayland activation. Keep it alive
                            through the last frame using this context, or
                            destroy the context before destroying the window.
                            Both must share the widget's UI thread. */
} nx_text_field_desc;

typedef struct nx_search_popup_desc {
    nx_overlay_desc overlay;
    nx_text_field_desc query; /* A single-line child of overlay.panel. */
    uint64_t list_id, empty_id;
    nx_utf8_span list_name, empty_name;
    const nx_text_layout *empty_layout; /* Optional visible empty state. */
    nx_point empty_offset; /* Relative to list_bounds origin. */
    size_t empty_glyph_cache_bytes;
    nx_rect list_bounds;
    float row_height; /* Positive logical pixels; at least one row must fit. */
    const nx_search_option *options;
    size_t option_count;
} nx_search_popup_desc;

typedef struct nx_color_picker_desc {
    uint64_t id, parent_id; /* Semantic group. */
    uint64_t square_id, hue_id, alpha_id, preview_id;
    nx_utf8_span name, square_name, hue_name, alpha_name, preview_name;
    nx_rect bounds, square_bounds, hue_bounds, alpha_bounds, preview_bounds;
    int show_alpha, disabled;
} nx_color_picker_desc;

typedef struct nx_color_picker_state {
    float hue; /* Remembers hue when saturation is zero. */
    int initialized; /* Zero-initialize before first use. */
} nx_color_picker_state;

typedef struct nx_theme_selector_desc {
    uint64_t id, parent_id; /* Semantic group. */
    nx_utf8_span name;
    nx_rect bounds;
    uint64_t option_ids[3]; /* Dark, light, high contrast. */
    nx_utf8_span option_names[3]; /* Caller-localized accessible labels. */
    const nx_font_collection *fonts; /* Optional visible labels. */
    double pixels_per_em; /* Required when fonts is present. */
    size_t glyph_cache_bytes; /* Required when fonts is present. */
    int disabled;
} nx_theme_selector_desc;

typedef enum nx_bitmap_format {
    NX_BITMAP_RGBA8 = 1, /* Straight alpha, four bytes per pixel. */
    NX_BITMAP_A8 = 2     /* One coverage byte per pixel, tinted on draw. */
} nx_bitmap_format;

typedef struct nx_bitmap_view {
    const uint8_t *pixels;
    uint32_t width, height;
    size_t stride_bytes;
    nx_bitmap_format format;
} nx_bitmap_view;

/* S07: application-rendered RGBA8 viewport surface. Callbacks may render a
   software preview of a 3D scene. resize is called on changed physical pixel
   dimensions, including (0,0) on suspension; a failed resize must leave the
   previous surface usable. acquire returns a borrowed bitmap with exactly the
   requested dimensions. Every successful acquire is matched by release,
   either on drawing failure or after nx_viewport_surface_complete. Pixels must
   remain readable and unchanged until completion. Callbacks run on the caller
   thread; complete only after every renderer has consumed the frame commands. */
typedef struct nx_viewport_source {
    void *user;
    nx_result (*resize)(void *user, uint32_t pixel_width,
                        uint32_t pixel_height);
    nx_result (*acquire)(void *user, nx_bitmap_view *out_bitmap);
    void (*release)(void *user);
} nx_viewport_source;

typedef struct nx_viewport_desc {
    uint64_t id, parent_id;
    nx_rect bounds; /* Logical pixels; a zero dimension suspends the surface. */
    nx_utf8_span name;
    double scale; /* Positive physical pixels per logical pixel. */
    int disabled;
} nx_viewport_desc;

#define NX_VIEWPORT_MAX_DIMENSION 16384u

NX_API nx_result nx_viewport_surface_create(const nx_viewport_source *source,
                                            const nx_allocator *allocator,
                                            nx_viewport_surface **out_surface);
/* Fails with NX_ERROR_STATE while an acquired bitmap is still in flight. */
NX_API nx_result nx_viewport_surface_destroy(nx_viewport_surface *surface);
/* Release one successful viewport draw after rendering its command stream.
   Must be called before reusing or destroying the surface. */
NX_API nx_result nx_viewport_surface_complete(nx_viewport_surface *surface);
/* Register a focusable viewport and append its image in paint order. One draw
   per surface may be in flight. Pixel extents are ceil(bounds * scale).
   Returns NX_ERROR_LIMIT above NX_VIEWPORT_MAX_DIMENSION. On failure, the
   widget's commands, semantic node and hit region are rolled back. */
NX_API nx_result nx_viewport(nx_context *context,
                             nx_viewport_surface *surface,
                             const nx_viewport_desc *desc,
                             nx_interaction_state *out_state);

typedef enum nx_command_type {
    NX_COMMAND_FILL_RECT = 1,
    NX_COMMAND_PUSH_CLIP = 2,
    NX_COMMAND_POP_CLIP = 3,
    NX_COMMAND_STROKE_RECT = 4,
    NX_COMMAND_FILL_ROUND_RECT = 5,
    NX_COMMAND_LINE = 6,
    NX_COMMAND_FILL_TRIANGLE = 7,
    NX_COMMAND_BITMAP = 8
} nx_command_type;

typedef struct nx_command {
    nx_command_type type;
    nx_rect rect;   /* Rectangle, stroke, rounded rectangle or clip. */
    nx_color color; /* Paint color. */
    float thickness; /* Stroke width or line diameter. */
    float radius;    /* Rounded rectangle corner radius. */
    nx_point points[3]; /* Line endpoints or triangle vertices. */
    nx_bitmap_view bitmap; /* Borrowed image or glyph-mask pixels. */
} nx_command;

/* Creates an independent context. *out_context is set to NULL on failure.
   desc may be NULL. Destroy on the same UI thread after renderer use ends. */
NX_API nx_result nx_context_create(const nx_context_desc *desc, nx_context **out_context);
NX_API void nx_context_destroy(nx_context *context);

/* All frame building and ID operations require exclusive access on one UI thread.
   Beginning a frame invalidates the previous command pointer. Logical dimensions
   must be finite and positive; delta_seconds must be finite and nonnegative. */
NX_API nx_result nx_begin_frame(nx_context *context, float width, float height,
                                double delta_seconds);
/* Ends the frame only when all ID/clip scopes are balanced. This also updates
   Wayland IME activation for the focused text field with ime_window. */
NX_API nx_result nx_end_frame(nx_context *context);

/* Queue a copied event for the next frame. Events posted during a frame are
   consumed by the following begin_frame. The current frame's borrowed event
   array and input snapshot remain valid until the next begin_frame. */
NX_API nx_result nx_context_queue_event(nx_context *context, const nx_event *event);
NX_API nx_result nx_frame_events(const nx_context *context,
                                 const nx_event **out_events, size_t *out_count);
NX_API nx_result nx_frame_input(const nx_context *context, nx_input_snapshot *out_input);
NX_API int nx_key_down(const nx_context *context, nx_key key);
/* Physical snapshots include native-consumed keys. Inspect frame event types
   before deriving application actions that could duplicate native text input. */
NX_API int nx_key_pressed(const nx_context *context, nx_key key);
NX_API int nx_key_released(const nx_context *context, nx_key key);

/* Register an interactive hit region during frame construction. The ID must
   be unique in this frame. Input is routed against last frame's regions so
   overlapping regions follow their previous paint order. The caller paints
   the current region based on out_state. Regions not registered in a frame
   lose focus/capture state at end_frame. */
NX_API nx_result nx_interact_region(nx_context *context, uint64_t id,
                                    nx_rect hit_rect, uint32_t flags,
                                    nx_interaction_state *out_state);
/* Register semantics and optional interaction atomically. The caller owns
   measurement, application state and drawing; use one stable ID per widget.
   out_state is required and is zeroed for a noninteractive widget. */
NX_API nx_result nx_widget_register(nx_context *context,
                                    const nx_widget_desc *desc,
                                    nx_interaction_state *out_state);
/* Set logical keyboard focus to a registered, enabled focusable region in
   the current or previous frame. Tab/Shift+Tab traverse registration order. */
NX_API nx_result nx_focus_set(nx_context *context, uint64_t id);
NX_API uint64_t nx_focus_id(const nx_context *context);

/* Build a backend-independent accessibility tree in parent-first order during
   a frame. Strings are copied and validated as UTF-8. IDs must be unique in
   this frame. Query a completed frame by index or ID; returned string pointers
   expire at the next begin_frame or context destruction. */
NX_API nx_result nx_semantics_add(nx_context *context,
                                  const nx_semantic_desc *desc, uint32_t *out_index);
NX_API nx_result nx_semantics_count(const nx_context *context, size_t *out_count);
NX_API nx_result nx_semantics_get(const nx_context *context, uint32_t index,
                                  nx_semantic_view *out_view);
NX_API nx_result nx_semantics_find(const nx_context *context, uint64_t id,
                                   uint32_t *out_index);
/* Queue PRESS, FOCUS or selection actions against a completed frame. PRESS and
   legacy ungrouped SELECT become ordinary interaction; explicit group selection
   is applied through nx_selection_group/apply in the next frame without a click.
   ADD_SELECTION and DESELECT are derived for group members; required groups
   cannot be cleared. Disabled, modal-blocked and unsupported targets are rejected.
   Duplicate/full 64-entry queues return LIMIT. Owner thread, between frames. */
NX_API nx_result nx_semantics_request_action(nx_context *context, uint64_t id, uint32_t action);
/* Publish an owned native accessibility snapshot of a completed context frame.
   Window/context owner thread only. The context may change or be destroyed after
   return. Route window events through this context while it remains published;
   a different context returns STATE before consuming events. Publishing a new
   context invalidates prior element handles and queued native requests.
   NULL context detaches the bridge. Windows supports UIA; macOS supports AppKit AX;
   Linux supports runtime-optional AT-SPI. An unavailable SDK/runtime/bus returns
   UNSUPPORTED while ordinary window operation remains available. */
NX_API nx_result nx_window_publish_semantics(nx_window *window, const nx_context *context);

/* Register an explicit single-selection container (GROUP, LIST, TREE or
   TAB_LIST), apply queued selection requests to the caller-owned ID, then build
   its SELECT-capable descendants using that ID. No model pointer is retained.
   required must be 0/1 (TAB_LIST requires 1). IDs are widget IDs, not domain
   option numbers. End-frame rejects multiple selected members or an empty
   required selection. Selection state uses CHECKED for radios, SELECTED otherwise.
   Desc actions must be zero; container state flags above are derived internally.
   Allocation failure leaves the model and pending selection requests unchanged. */
NX_API nx_result nx_selection_group(nx_context *context, const nx_semantic_desc *desc,
                                    int required, uint64_t *selected_id);
/* Optional early model phase before layout/group registration. Requests for this
   group are consumed once; later group registration preserves the result.
   SELECT replaces, ADD_SELECTION adds only to an empty selection, DESELECT clears
   only its matching optional selection. Context owner thread, during a frame. */
NX_API nx_result nx_selection_apply(nx_context *context, uint64_t group_id,
                                    int required, uint64_t *selected_id);

/* IDs are stable across frames for identical hierarchy and byte sequences.
   Labels are borrowed only for the duration of the call and may contain any
   non-NUL bytes. This is an identity primitive, not Unicode text processing. */
NX_API uint64_t nx_id(nx_context *context, const char *label, size_t length);
NX_API nx_result nx_push_id(nx_context *context, const char *label, size_t length);
NX_API nx_result nx_pop_id(nx_context *context);

/* Commands are appended in paint order. Rectangles use logical half-open bounds.
   Invalid geometry or color returns NX_ERROR_ARGUMENT without appending. */
NX_API nx_result nx_fill_rect(nx_context *context, nx_rect rect, nx_color color);
/* Stroke lies inside rect. Radius is clamped to half the smaller dimension.
   Lines have round caps and thickness is their diameter. Coordinates are
   logical pixels; all geometry and paint values must be finite. */
NX_API nx_result nx_stroke_rect(nx_context *context, nx_rect rect,
                                float thickness, nx_color color);
NX_API nx_result nx_fill_round_rect(nx_context *context, nx_rect rect,
                                    float radius, nx_color color);
NX_API nx_result nx_draw_line(nx_context *context, nx_point a, nx_point b,
                              float thickness, nx_color color);
NX_API nx_result nx_fill_triangle(nx_context *context, nx_point a, nx_point b,
                                  nx_point c, nx_color color);
/* Draw the complete bitmap scaled to dest with nearest-neighbor sampling.
   Tint multiplies RGBA channels for RGBA8; A8 supplies coverage for tint.
   Source pixels must remain readable until every renderer has finished using
   the returned command stream, including after end_frame. A view can refer to
   an atlas subregion by offsetting pixels while keeping its parent stride. */
NX_API nx_result nx_draw_bitmap(nx_context *context, nx_rect dest,
                                nx_bitmap_view bitmap, nx_color tint);
/* Rasterize and append a positioned layout as A8 bitmap commands. The context
   owns copies of the masks until its next begin_frame or destruction, so cache
   eviction cannot invalidate the command stream. The layout's ppem must be an
   integer in [1, 16384]. cache_bytes is a per-font mask-cache budget and must
   be positive. On failure, no commands are appended. Fonts and their source
   bytes must remain valid for the duration of this call. */
NX_API nx_result nx_draw_text_layout(nx_context *context,
                                     const nx_text_layout *layout,
                                     nx_point origin, nx_color tint,
                                     size_t cache_bytes);

/* Rasterize a monochrome layout once into an immutable, tightly bounded A8
   stamp. It owns its pixels and copied text; fonts/layout may be released after
   creation. Empty/whitespace-only layouts produce an empty stamp. Creation
   uses the existing glyph rasterizer and software compositor, with a maximum
   16384 pixels per dimension and 4194304 ink pixels. Allocator is optional.
   This caches one pixel phase/size: recreate after changing font size. Drawing
   is ordinary nearest-neighbor bitmap sampling; tint is applied after glyph
   coverage is combined, unlike per-glyph tinted compositing at overlaps. */
NX_API nx_result nx_text_stamp_create(const nx_text_layout *layout,
                                       size_t glyph_cache_bytes,
                                       const nx_allocator *allocator,
                                       nx_text_stamp **out_stamp);
NX_API void nx_text_stamp_destroy(nx_text_stamp *stamp);
NX_API nx_result nx_text_stamp_text(const nx_text_stamp *stamp,
                                     nx_utf8_span *out_text);
/* View and offset from the original layout origin; a blank stamp returns a
   zero view. Borrowed until destroy. Keep stamps alive until rendering ends. */
NX_API nx_result nx_text_stamp_view(const nx_text_stamp *stamp,
                                     nx_bitmap_view *out_view,
                                     nx_point *out_offset);
NX_API nx_result nx_draw_text_stamp(nx_context *context, const nx_text_stamp *stamp,
                                     nx_point origin, nx_color tint);
NX_API nx_result nx_label_stamp(nx_context *context, uint64_t id, uint64_t parent_id,
                                 nx_rect bounds, const nx_text_stamp *stamp,
                                 nx_point origin, nx_color tint);

#define NX_TEXT_STYLE_FOREGROUND 1u
#define NX_TEXT_STYLE_BACKGROUND 2u
#define NX_TEXT_STYLE_UNDERLINE  4u
#define NX_TEXT_STYLE_STRIKE     8u

typedef struct nx_text_style_span {
    size_t byte_start, byte_end; /* Half-open UTF-8 source range. */
    uint32_t flags;
    nx_color foreground, background; /* Used only when corresponding flag is set. */
} nx_text_style_span;

/* Draw sorted, disjoint inline style spans over an existing layout. Spans are
   borrowed only for the call; gaps use base_tint. Boundaries must be grapheme
   boundaries and may not split a shaped
   glyph (including a ligature). Foreground overrides tint; background and
   decorations use the layout's selection geometry across bidi/line breaks.
   The call is atomic for commands and owned glyph masks on error. */
NX_API nx_result nx_draw_rich_text_layout(nx_context *context,
                                          const nx_text_layout *layout,
                                          nx_point origin, nx_color base_tint,
                                          const nx_text_style_span *spans,
                                          size_t span_count,
                                          size_t cache_bytes);
/* Display widgets register semantic nodes and append their drawing commands.
   The parent semantic node must already exist. IDs are stable across frames.
   nx_label uses the text layout's copied UTF-8 source as its accessible name.
   Image pixels remain borrowed through rendering, as with nx_draw_bitmap. */
NX_API nx_result nx_label(nx_context *context, uint64_t id, uint64_t parent_id,
                          nx_rect bounds, const nx_text_layout *layout,
                          nx_point origin, nx_color tint, size_t cache_bytes);
NX_API nx_result nx_rich_label(nx_context *context, uint64_t id,
                               uint64_t parent_id, nx_rect bounds,
                               const nx_text_layout *layout, nx_point origin,
                               nx_color base_tint,
                               const nx_text_style_span *spans,
                               size_t span_count, size_t cache_bytes);
NX_API nx_result nx_separator(nx_context *context, uint64_t id,
                              uint64_t parent_id, nx_rect bounds, nx_color color);
NX_API nx_result nx_image(nx_context *context, uint64_t id, uint64_t parent_id,
                          nx_rect bounds, nx_utf8_span name,
                          nx_bitmap_view bitmap, nx_color tint);
/* Theme-drawn controls register interaction and semantics under desc->id.
   out_state reports activations. Checkbox/toggle flip their caller-owned bool;
   radio writes its nonzero option ID into *selected on activation. */
NX_API nx_result nx_button(nx_context *context, const nx_control_desc *desc,
                           nx_interaction_state *out_state);
NX_API nx_result nx_checkbox(nx_context *context, const nx_control_desc *desc,
                             int *checked, nx_interaction_state *out_state);
NX_API nx_result nx_radio(nx_context *context, const nx_control_desc *desc,
                          uint64_t option, uint64_t *selected,
                          nx_interaction_state *out_state);
NX_API nx_result nx_toggle(nx_context *context, const nx_control_desc *desc,
                           int *on, nx_interaction_state *out_state);
/* Register and paint a semantic container before its children. The caller
   arranges child rectangles and registers them with desc->id as parent. */
NX_API nx_result nx_panel(nx_context *context,
                          const nx_container_desc *desc);
NX_API nx_result nx_group(nx_context *context,
                          const nx_container_desc *desc);
/* Updates caller-owned offsets from wheel, optional scrollbar drag and
   keyboard input, paints the viewport and pushes its clip. Arrange content at
   the returned offsets, register children while clipped, then call
   nx_scroll_area_end to paint/register scrollbars above the content. */
NX_API nx_result nx_scroll_area_begin(nx_context *context,
                                      const nx_scroll_area_desc *desc,
                                      float *offset_x, float *offset_y,
                                      nx_scroll_info *out_info);
NX_API nx_result nx_scroll_area_end(nx_context *context);
/* Register a group and its focusable header. Draw children only when the
   returned expanded value is nonzero. A failed paint leaves it unchanged. */
NX_API nx_result nx_collapsible_section(nx_context *context,
                                        const nx_collapsible_desc *desc,
                                        int *expanded,
                                        nx_interaction_state *out_state);
/* A tab is selected by click or Enter/Space. Register sibling tabs under the
   same parent and one tab panel for the selected tab. */
NX_API nx_result nx_tab(nx_context *context, const nx_control_desc *desc,
                        uint64_t *selected_id,
                        nx_interaction_state *out_state);
NX_API nx_result nx_tab_panel(nx_context *context,
                              const nx_container_desc *desc);
/* Register a table and interactive column headers. Clicks toggle ascending /
   descending order; resize handles use pointer drag or Left/Right keys.
   Changes to sort state and column widths commit only after painting succeeds. */
NX_API nx_result nx_table_headers(nx_context *context,
                                  const nx_table_desc *desc,
                                  nx_table_sort *sort);
/* Register only rows and cells in the virtual range. Row and cell IDs are
   application-owned stable keys; cells use their row ID as parent. */
NX_API nx_result nx_table_row(nx_context *context,
                              const nx_container_desc *desc);
NX_API nx_result nx_table_cell(nx_context *context,
                               const nx_container_desc *desc);
NX_API nx_result nx_tree(nx_context *context,
                         const nx_container_desc *desc);
/* A branch uses a separate expander hit region; clicking the rest selects it.
   Left/Right set expansion when the item or expander is focused. State and
   selection commit only after semantic registration and painting succeed. */
NX_API nx_result nx_tree_item(nx_context *context,
                              const nx_tree_item_desc *desc,
                              int *expanded, uint64_t *selected_id,
                              nx_interaction_state *out_state);
/* Register an overlay, then its children under panel.id, and end its scope.
   A modal dims the background. Both forms trap Tab navigation while shown.
   out_dismissed signals a backdrop click or Escape; omit the overlay next
   frame to close it. Only one overlay may be built per frame. */
NX_API nx_result nx_popup_begin(nx_context *context,
                                const nx_overlay_desc *desc,
                                int *out_dismissed);
NX_API nx_result nx_modal_begin(nx_context *context,
                                const nx_overlay_desc *desc,
                                int *out_dismissed);
NX_API nx_result nx_overlay_end(nx_context *context);
/* Programmatic close after overlay_end or between frames. Between frames it
   deactivates the previous backdrop before the next event-routing pass. */
NX_API nx_result nx_overlay_close(nx_context *context);
/* Noninteractive painted tooltip with an accessible semantic node. */
NX_API nx_result nx_tooltip(nx_context *context,
                            const nx_container_desc *desc);
/* Menu-bar items navigate horizontally; menu items navigate vertically.
   Home/End select the first/last enabled sibling. Enter/Space activate.
   A checkable item toggles caller-owned state only after painting succeeds. */
NX_API nx_result nx_menu_bar(nx_context *context,
                             const nx_container_desc *desc);
NX_API nx_result nx_menu(nx_context *context,
                         const nx_container_desc *desc);
NX_API nx_result nx_menu_item(nx_context *context,
                              const nx_menu_item_desc *desc,
                              int *checked, nx_interaction_state *out_state);
NX_API nx_result nx_toolbar(nx_context *context,
                            const nx_container_desc *desc);
NX_API nx_result nx_status_bar(nx_context *context,
                               const nx_container_desc *desc);
/* Both functions draw a popup with a search field and a scrollable result
   list. The caller supplies a stable, application-owned query field and may
   register a separate trigger for searchable select. Enter or a row click
   returns an activated ID; Escape/outside click sets out_dismissed. A palette
   reports activation without changing selected_id; searchable select commits
   selected_id. Call only while the popup should be visible. */
NX_API nx_result nx_command_palette(nx_context *context,
                                    const nx_search_popup_desc *desc,
                                    nx_search_state *state,
                                    int *out_dismissed,
                                    uint64_t *out_activated_id);
NX_API nx_result nx_searchable_select(nx_context *context,
                                      const nx_search_popup_desc *desc,
                                      nx_search_state *state,
                                      int *out_dismissed,
                                      uint64_t *out_activated_id);
/* RGB is normalized sRGB. The square edits saturation/value; the horizontal
   strips edit hue and optional alpha. All parts have stable semantic IDs.
   Caller-owned color and hue memory commit only after successful painting. */
NX_API nx_result nx_color_picker(nx_context *context,
                                 const nx_color_picker_desc *desc,
                                 nx_color_picker_state *state,
                                 nx_color *color, int *out_changed);
/* A three-choice semantic radio group. Apply the selected preset to a context
   between frames with nx_theme_preset and nx_context_set_theme. */
NX_API nx_result nx_theme_selector(nx_context *context,
                                   const nx_theme_selector_desc *desc,
                                   nx_theme_kind *selected,
                                   int *out_changed);
/* Horizontal value controls clamp and snap caller-owned values to the range.
   A slider maps captured pointer position to the range. Drag value applies
   captured horizontal motion at units_per_pixel. Arrow, Page, Home and End
   keys operate when focused. The semantic value is updated in the same frame. */
NX_API nx_result nx_slider(nx_context *context,
                           const nx_value_control_desc *desc, double *value,
                           nx_interaction_state *out_state);
NX_API nx_result nx_drag_value(nx_context *context,
                               const nx_value_control_desc *desc, double *value,
                               nx_interaction_state *out_state);
/* A single-line numeric editor. The field owns its draft text and committed
   value. Register once per frame; it consumes focused key/text events,
   paints text, selection and caret, and exposes draft/validation semantics.
   Left-button dragging selects text; Shift-click preserves the anchor.
   Capture permits release outside the field and bounded edge auto-scroll.
   A failed paint may be retried in the same frame without repeating edits.
   Fonts and their bytes remain caller-owned through each widget call. */
NX_API nx_result nx_numeric_field_create(double initial_value,
                                         size_t history_bytes,
                                         nx_numeric_field **out_field);
NX_API void nx_numeric_field_destroy(nx_numeric_field *field);
NX_API nx_result nx_numeric_field_set_value(nx_numeric_field *field,
                                            double value);
NX_API nx_result nx_numeric_field_value(const nx_numeric_field *field,
                                        double *out_value);
NX_API nx_result nx_numeric_field_text(const nx_numeric_field *field,
                                       nx_utf8_span *out_text);
NX_API nx_result nx_numeric_field_widget(nx_context *context,
                                         const nx_numeric_field_desc *desc,
                                         nx_numeric_field *field,
                                         nx_interaction_state *out_state,
                                         int *out_valid);
/* A text editor with owned UTF-8 draft and undo history. Single-line Enter
   reports submission; multiline Enter inserts a line break. Programmatic set
   replaces the draft and clears history. A failed paint may be retried in the
   same frame without applying the same events again. Left-button drag and
   Shift-click select at grapheme boundaries, including password masks;
   a held pointer beyond the visible edge scrolls once per frame. */
NX_API nx_result nx_text_field_create(const char *initial, size_t length,
                                      size_t history_bytes,
                                      nx_text_field **out_field);
/* Create a sensitive single-line field without undo/redo history. Use this
   constructor with nx_text_field_desc.password; regular fields are rejected
   by that mode. The caller still owns its input and borrowed result bytes. */
NX_API nx_result nx_text_field_create_password(const char *initial,
                                               size_t length,
                                               nx_text_field **out_field);
NX_API void nx_text_field_destroy(nx_text_field *field);
NX_API nx_result nx_text_field_set_text(nx_text_field *field,
                                       const char *text, size_t length);
NX_API nx_result nx_text_field_text(const nx_text_field *field,
                                   nx_utf8_span *out_text);
NX_API nx_result nx_text_field_selection(const nx_text_field *field,
                                        size_t *out_anchor, size_t *out_cursor);
NX_API nx_result nx_text_field_widget(nx_context *context,
                                      const nx_text_field_desc *desc,
                                      nx_text_field *field,
                                      nx_interaction_state *out_state,
                                      int *out_valid, int *out_submitted);
NX_API nx_result nx_push_clip(nx_context *context, nx_rect rect);
NX_API nx_result nx_pop_clip(nx_context *context);

/* The returned array is borrowed until the next begin_frame or destruction.
   It is available only after a successful end_frame. Zero commands may return
   NULL. No command may be mutated by the caller. */
NX_API nx_result nx_frame_commands(const nx_context *context,
                                   const nx_command **out_commands, size_t *out_count);

/* Decode one Unicode scalar from a UTF-8 byte span. *offset advances only on
   success. Invalid, truncated, overlong, surrogate and out-of-range sequences
   return NX_ERROR_ARGUMENT. No replacement character is inserted. */
NX_API nx_result nx_utf8_decode(const char *bytes, size_t length,
                                size_t *offset, uint32_t *out_scalar);
/* Validate an entire span. On failure *error_offset is the first invalid
   sequence's byte position; on success it is length. The output is optional. */
NX_API nx_result nx_utf8_validate(const char *bytes, size_t length,
                                  size_t *error_offset);

/* Find the next extended grapheme cluster boundary in a UTF-8 span using
   Unicode 18.0.0 default rules. start must be a cluster boundary below length.
   Only bytes through that cluster are decoded; invalid UTF-8 there returns
   NX_ERROR_ARGUMENT. *out_next changes only on success. */
NX_API nx_result nx_grapheme_next(const char *bytes, size_t length,
                                 size_t start, size_t *out_next);

/* Find the next default word boundary in a UTF-8 span using Unicode 18.0.0.
   start must be a word boundary below length. This yields every UAX #29
   boundary, including punctuation and spaces; it does not classify words.
   Necessary lookahead may be decoded. *out_next changes only on success. */
NX_API nx_result nx_word_next(const char *bytes, size_t length,
                             size_t start, size_t *out_next);

typedef enum nx_line_break_kind {
    NX_LINE_BREAK_NONE = 0,
    NX_LINE_BREAK_OPTIONAL = 1,
    NX_LINE_BREAK_MANDATORY = 2
} nx_line_break_kind;

/* Analyze default Unicode 18.0.0 line break opportunities in a UTF-8 span.
   out_breaks has at least length+1 bytes and may not overlap bytes. Entries
   at non-scalar byte positions and, for nonempty text, at offset zero are
   NONE; the final entry is MANDATORY. Invalid UTF-8 or insufficient capacity
   leaves output intact.
   Opportunities do not choose actual wrap positions or perform hyphenation. */
NX_API nx_result nx_line_breaks(const char *bytes, size_t length,
                               uint8_t *out_breaks, size_t capacity);

typedef enum nx_bidi_direction {
    NX_BIDI_AUTO = 0,
    NX_BIDI_LTR = 1,
    NX_BIDI_RTL = 2
} nx_bidi_direction;

/* Resolve one UTF-8 paragraph as one display line using Unicode 18.0.0.
   levels and visual_order each need one entry per scalar. Levels of X9
   removed formatting characters are UINT8_MAX; visual_order omits them.
   visual_order contains scalar indices in left-to-right display order.
   A paragraph separator is accepted only at the end. Invalid input or
   insufficient capacity leaves all outputs unchanged. */
NX_API nx_result nx_bidi_analyze(const char *bytes, size_t length,
                                nx_bidi_direction direction,
                                uint8_t *levels, size_t levels_capacity,
                                size_t *visual_order, size_t order_capacity,
                                size_t *out_scalar_count,
                                size_t *out_visual_count,
                                uint8_t *out_paragraph_level);

/* Open one SFNT face from a borrowed, immutable byte buffer. The buffer must
   remain readable until nx_font_close. TrueType and OpenType/CFF container
   signatures are accepted; glyph decoding is a separate stage. Directory
   entries are bounds-checked, capped and sorted by tag. Table views borrow the
   same bytes and expire at close. Missing tags return NX_ERROR_ARGUMENT. */
NX_API nx_result nx_font_open(const uint8_t *bytes, size_t length,
                              const nx_allocator *allocator, nx_font **out_font);
NX_API void nx_font_close(nx_font *font);
NX_API nx_result nx_font_table_count(const nx_font *font, size_t *out_count);
NX_API nx_result nx_font_table_at(const nx_font *font, uint32_t index,
                                  uint32_t *out_tag, nx_bytes_view *out_view);
NX_API nx_result nx_font_find_table(const nx_font *font, uint32_t tag,
                                    nx_bytes_view *out_view);
/* Parse TrueType/OpenType horizontal metrics from head/maxp/hhea/hmtx.
   A valid container may still lack these tables and then returns
   NX_ERROR_ARGUMENT. Glyph indices must be below glyph_count. */
NX_API nx_result nx_font_get_metrics(const nx_font *font,
                                     nx_font_metrics *out_metrics);
NX_API nx_result nx_font_glyph_hmetrics(const nx_font *font, uint16_t glyph_index,
                                        uint16_t *out_advance, int16_t *out_lsb);
/* Horizontal legacy 'kern' format-0 pairs, in font design units. Missing or
   unsupported subtables contribute zero; malformed supported data is an error.
   nx_shape_text applies GPOS pair positioning before using this fallback.
   Output changes on success. */
NX_API nx_result nx_font_kerning(const nx_font *font, uint16_t left_glyph,
                                uint16_t right_glyph, int32_t *out_adjustment);
/* Map one Unicode scalar through a supported cmap format 4 or 12 subtable.
   Glyph zero means no mapping. Invalid scalar/subtable returns an error. */
NX_API nx_result nx_font_glyph_index(const nx_font *font, uint32_t scalar,
                                     uint16_t *out_glyph);
/* Decode a TrueType glyf outline using loca. Call with null arrays and zero
   capacities to query counts, then supply caller-owned arrays. Insufficient
   capacity returns NX_ERROR_LIMIT with required counts and writes no elements.
   Composite components support XY offsets, outline-point alignment and F2DOT14
   transforms, with a 32-glyph recursion limit. Fractional transformed points
   round to the nearest design unit, with ties away from zero. Hinting,
   variation deltas, phantom-point alignment and non-glyf outlines are not applied. */
NX_API nx_result nx_font_get_glyph_outline(const nx_font *font, uint16_t glyph_index,
                                           nx_glyph_outline_buffer *out_outline);

/* Rasterize a decoded outline to a caller-owned A8 mask. Positive scale maps
   design units to pixels; (baseline_x, baseline_y) is the bitmap-space origin
   for glyph point (0,0). Bitmap y grows downward. Uses 4x4 coverage sampling
   and nonzero winding. The visible width of every row is overwritten on success;
   row padding is untouched. Any error leaves the pixel buffer unchanged.
   max dimension/pixel constants bound work; glyph instructions are not run. */
NX_API nx_result nx_rasterize_glyph_a8(const nx_glyph_outline_buffer *outline,
                                      double pixels_per_unit,
                                      double baseline_x, double baseline_y,
                                      uint8_t *pixels, uint32_t width,
                                      uint32_t height, size_t stride_bytes,
                                      size_t pixel_buffer_size,
                                      const nx_allocator *allocator);

/* A per-font, single-threaded LRU cache of A8 masks at integer pixels-per-em.
   The font and its source bytes must outlive the cache. max_bytes limits resident
   entries and pixels, excluding the cache object and temporary raster work.
   A returned view remains valid until the next cache miss, clear or destroy.
   An empty glyph has zero mask dimensions but still has an advance. */
NX_API nx_result nx_glyph_cache_create(const nx_font *font, size_t max_bytes,
                                      const nx_allocator *allocator,
                                      nx_glyph_cache **out_cache);
NX_API void nx_glyph_cache_destroy(nx_glyph_cache *cache);
NX_API nx_result nx_glyph_cache_get(nx_glyph_cache *cache,
                                   uint16_t glyph_index, uint16_t pixels_per_em,
                                   nx_glyph_mask_view *out_mask);
NX_API void nx_glyph_cache_clear(nx_glyph_cache *cache);
NX_API nx_result nx_glyph_cache_get_stats(const nx_glyph_cache *cache,
                                         nx_glyph_cache_stats *out_stats);

/* An ordered fallback chain of borrowed font faces. Fonts and their source
   bytes must outlive the collection. Add rejects duplicate faces and faces
   without a supported Unicode cmap. Resolve picks the first nonzero glyph;
   no match returns NX_OK with a null font and zero glyph. Malformed mapping
   errors are propagated. The collection is single-threaded. */
NX_API nx_result nx_font_collection_create(const nx_allocator *allocator,
                                           nx_font_collection **out_collection);
NX_API void nx_font_collection_destroy(nx_font_collection *collection);
NX_API nx_result nx_font_collection_add(nx_font_collection *collection,
                                        const nx_font *font);
NX_API void nx_font_collection_clear(nx_font_collection *collection);
NX_API nx_result nx_font_collection_count(const nx_font_collection *collection,
                                          size_t *out_count);
NX_API nx_result nx_font_collection_font_at(const nx_font_collection *collection,
                                            uint32_t index,
                                            const nx_font **out_font);
NX_API nx_result nx_font_collection_resolve(const nx_font_collection *collection,
                                            uint32_t scalar,
                                            nx_font_match *out_match);

/* Shape one UTF-8 paragraph into logical-order glyphs and homogeneous runs.
   The path applies selected GSUB single, ligature and contextual substitutions and GPOS
   single/pair positioning and mark-to-base/ligature/mark attachment, with legacy
   kern format-0 pairs as a fallback. Supported lookups use GDEF filtering and
   extension tables. Nonspacing marks are centered over their grapheme base
   before GPOS. Ligatures
   span multiple input bytes; their scalar is the first input scalar.
   Default-ignorable and control scalars produce no glyph. Anchor device deltas
   apply at integer ppem; hinted anchor points, variation deltas, contextual GPOS and
   advanced script shaping remain pending. Missing mappings use
   glyph zero of the first font. Both output arrays may be null for a count
   query. Insufficient output capacity returns NX_ERROR_LIMIT with required
   counts but no elements; other failures leave counts and arrays unchanged. */
NX_API nx_result nx_shape_text(const nx_font_collection *collection,
                              const char *bytes, size_t length,
                              nx_bidi_direction direction, double pixels_per_em,
                              nx_shaped_glyph *glyphs, size_t glyph_capacity,
                              nx_shape_run *runs, size_t run_capacity,
                              size_t *out_glyph_count, size_t *out_run_count,
                              uint8_t *out_paragraph_level);

/* Build multiline text in top-down pixel coordinates. The layout copies UTF-8
   bytes but borrows fonts through its glyph records; those fonts must outlive it.
   max_width is positive and finite. Wrapping uses Unicode line opportunities;
   an unbreakable segment may exceed max_width. Hard breaks and Unicode bidi
   paragraph separators make a new line and are excluded from glyph ranges.
   x is the glyph pen position; draw at
   x+offset_x and baseline_y-offset_y. Lines include trailing spaces in their
   byte ranges but measure and position them with zero advance at line ends.
   Carets occur at extended-grapheme boundaries, with two positions where bidi
   runs or soft-wrapped lines meet. Ligature interiors divide advance evenly.
   Advanced script shaping and hinted/variable-font positioning remain pending. */
NX_API nx_result nx_text_layout_create(const nx_font_collection *collection,
                                       const char *bytes, size_t length,
                                       nx_bidi_direction direction,
                                       double pixels_per_em, double max_width,
                                       nx_text_layout **out_layout);
NX_API void nx_text_layout_destroy(nx_text_layout *layout);
NX_API size_t nx_text_layout_line_count(const nx_text_layout *layout);
NX_API size_t nx_text_layout_glyph_count(const nx_text_layout *layout);
NX_API size_t nx_text_layout_caret_count(const nx_text_layout *layout);
NX_API int nx_text_layout_is_grapheme_boundary(const nx_text_layout *layout,
                                                size_t byte_offset);
NX_API double nx_text_layout_width(const nx_text_layout *layout);
NX_API double nx_text_layout_height(const nx_text_layout *layout);
NX_API double nx_text_layout_pixels_per_em(const nx_text_layout *layout);
/* Borrowed until the layout is destroyed; the source is copied at creation. */
NX_API nx_result nx_text_layout_text(const nx_text_layout *layout,
                                     nx_utf8_span *out_text);
NX_API nx_result nx_text_layout_line_at(const nx_text_layout *layout,
                                        size_t index, nx_text_line *out_line);
NX_API nx_result nx_text_layout_glyph_at(const nx_text_layout *layout,
                                         size_t index, nx_text_glyph *out_glyph);
NX_API nx_result nx_text_layout_caret_at(const nx_text_layout *layout,
                                         size_t index, nx_text_caret *out_caret);
/* Choose the nearest caret on the line containing y, clamping outside bounds. */
NX_API nx_result nx_text_layout_hit_test(const nx_text_layout *layout,
                                         double x, double y,
                                         nx_text_caret *out_caret);
/* Return rectangles for a half-open selection at grapheme boundaries. A null
   rect array with zero capacity queries the required count. Insufficient
   capacity returns NX_ERROR_LIMIT and leaves the array untouched. */
NX_API nx_result nx_text_layout_selection(const nx_text_layout *layout,
                                          size_t byte_start, size_t byte_end,
                                          nx_text_selection_rect *rects,
                                          size_t capacity, size_t *out_count);

typedef enum nx_text_move {
    NX_TEXT_MOVE_GRAPHEME_BACKWARD = 1,
    NX_TEXT_MOVE_GRAPHEME_FORWARD,
    NX_TEXT_MOVE_WORD_BACKWARD,
    NX_TEXT_MOVE_WORD_FORWARD,
    NX_TEXT_MOVE_LINE_START,
    NX_TEXT_MOVE_LINE_END,
    NX_TEXT_MOVE_DOCUMENT_START,
    NX_TEXT_MOVE_DOCUMENT_END
} nx_text_move;

/* Own a mutable UTF-8 string. Anchor and cursor are byte offsets at extended
   grapheme boundaries; [min(anchor,cursor), max(...)) is the selection.
   The initial selection is collapsed at the end. Text and boundary indexes
   are rebuilt atomically on edits. history_bytes
   bounds the combined undo/redo operation log; zero disables history. A new
   edit clears redo, and edits larger than the budget clear all history.
   This initial buffer uses contiguous storage and O(text length) edits. */
NX_API nx_result nx_text_buffer_create(const char *bytes, size_t length,
                                       size_t history_bytes,
                                       nx_text_buffer **out_buffer);
/* Own a sensitive UTF-8 buffer without undo/redo history. NexaUI overwrites
   its owned text allocations before freeing them, including failed edits.
   Caller input, OS event storage and other external copies remain caller/OS
   responsibilities. */
NX_API nx_result nx_text_buffer_create_sensitive(const char *bytes,
                                                 size_t length,
                                                 nx_text_buffer **out_buffer);
NX_API void nx_text_buffer_destroy(nx_text_buffer *buffer);
/* Returned bytes are NUL-terminated and borrowed until the next edit or
   destruction; use the returned length because embedded NUL is permitted. */
NX_API nx_result nx_text_buffer_text(const nx_text_buffer *buffer,
                                     const char **out_bytes,
                                     size_t *out_length);
NX_API nx_result nx_text_buffer_selection(const nx_text_buffer *buffer,
                                          size_t *out_anchor,
                                          size_t *out_cursor);
/* A borrowed view of the selected bytes, valid until the next edit or
   destruction. Empty selections have length zero. Clipboard backends must
   copy the bytes before mutating the buffer. */
NX_API nx_result nx_text_buffer_selected_span(const nx_text_buffer *buffer,
                                               nx_utf8_span *out_span);
NX_API nx_result nx_text_buffer_set_selection(nx_text_buffer *buffer,
                                              size_t anchor, size_t cursor);
/* Replace the current selection and collapse at the end of inserted text.
   Replacement must be valid UTF-8. Failure leaves text, selection and history
   unchanged. An empty replacement deletes the selection. */
NX_API nx_result nx_text_buffer_replace(nx_text_buffer *buffer,
                                         const char *bytes, size_t length);
/* Apply a native IME deletion measured in UTF-8 bytes before and after the
   current selection. The selection itself is removed as part of the edit.
   Bounds must fall at Unicode scalar boundaries; invalid edits leave the
   buffer unchanged. */
NX_API nx_result nx_text_buffer_delete_surrounding(nx_text_buffer *buffer,
    size_t before, size_t after);
/* Delete a v1 IME byte range relative to the current cursor, then collapse
   selection to the edit point. The start/end must be UTF-8 scalar boundaries;
   invalid ranges leave the buffer unchanged. */
NX_API nx_result nx_text_buffer_delete_relative(nx_text_buffer *buffer,
    int32_t index, size_t length);
/* Move through grapheme, default UAX #29 word, line or document boundaries.
   With extend_selection=0, backward/forward movement first collapses a nonempty
   selection to its nearer edge. With extend_selection=1, anchor stays fixed. */
NX_API nx_result nx_text_buffer_move_cursor(nx_text_buffer *buffer,
                                             nx_text_move move,
                                             int extend_selection);
NX_API nx_result nx_text_buffer_delete_backward(nx_text_buffer *buffer);
NX_API nx_result nx_text_buffer_delete_forward(nx_text_buffer *buffer);
NX_API int nx_text_buffer_can_undo(const nx_text_buffer *buffer);
NX_API int nx_text_buffer_can_redo(const nx_text_buffer *buffer);
NX_API nx_result nx_text_buffer_undo(nx_text_buffer *buffer);
NX_API nx_result nx_text_buffer_redo(nx_text_buffer *buffer);
/* Apply backend-neutral key, text and text-command events to a focused
   multiline buffer.
   Handles printable text, Enter, grapheme/word navigation and deletion,
   Home/End, Ctrl+A/Z/Y and Ctrl+Shift+Z. macOS additionally accepts Command+A/Z
   and Command+Shift+Z. Text commands handle logical grapheme/word, hard-line,
   paragraph/document movement, deletion, select-all, newline and undo/redo
   independently of physical modifiers. Consumed key presses are unhandled.
   Call only for the editor that owns focus; clipboard, IME composition,
   pointer selection, visual caret geometry and scrolling are handled elsewhere.
   Commands requiring those facilities return NX_OK with out_handled zero.
   On an edit error, buffer state is unchanged and out_handled is zero. */
NX_API nx_result nx_text_buffer_apply_event(nx_text_buffer *buffer,
                                             const nx_event *event,
                                             int *out_handled);
/* Parse an ASCII decimal with optional sign, fraction and exponent. The
   result is locale-independent. Invalid, incomplete, out-of-range,
   unrepresentable or over-1024-byte values return NX_OK with *out_valid zero. */
NX_API nx_result nx_numeric_parse(const char *bytes, size_t length,
                                  double minimum, double maximum,
                                  double *out_value, int *out_valid);
/* Apply one event to a focused numeric editor buffer. Only numeric text
   scalars are inserted; normal selection, deletion and undo keys reuse the
   text-buffer editor. Enter commits a valid draft to *committed_value.
   Invalid drafts remain editable and do not change the committed value. */
NX_API nx_result nx_numeric_input_apply_event(nx_text_buffer *buffer,
                                               const nx_event *event,
                                               double minimum, double maximum,
                                               double *committed_value,
                                               int *out_handled,
                                               int *out_valid);

/* Render to caller-owned, tightly described RGBA8 straight-alpha memory.
   stride_bytes must be at least width*4. The entire visible area is first
   cleared. Commands and pixels may not overlap. No UI context is accessed.
   A malformed command stream returns NX_ERROR_COMMAND before changing pixels. */
NX_API nx_result nx_headless_render(const nx_command *commands, size_t count,
                                    uint8_t *pixels, uint32_t width, uint32_t height,
                                    size_t stride_bytes, nx_color clear_color);

#ifdef __cplusplus
}
#endif

#endif
