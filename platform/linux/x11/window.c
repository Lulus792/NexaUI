#define _POSIX_C_SOURCE 200809L
#include "nexaui/nexaui.h"
#include "drop_queue.h"
#include "event_queue.h"
#include "backend.h"
#include "uri_list.h"

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/XKBlib.h>
#include <X11/keysym.h>
#include <X11/cursorfont.h>
#include <errno.h>
#include <limits.h>
#include <math.h>
#include <poll.h>
#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <wchar.h>

typedef struct nx_x11_incr_transfer {
    struct nx_x11_incr_transfer *next;
    Window requestor;
    Atom property;
    char *text;
    size_t length, offset;
    uint64_t last_progress_ns;
    int final_sent;
} nx_x11_incr_transfer;

struct nx_x11_window {
    struct nx_x11_window *thread_next;
    nx_allocator allocator;
    pthread_t owner_thread;
    Display *display;
    Window xid;
    XIM input_method;
    XIC input_context;
    XIC fallback_context;
    XIC callback_context;
    XIMStyle input_style;
    XIMStyle fallback_style;
    XIMStyle callback_style;
    Atom wm_protocols, wm_delete_window;
    Atom clipboard, clipboard_targets, clipboard_utf8;
    Atom clipboard_property, clipboard_incr;
    Atom xdnd_aware, xdnd_enter, xdnd_position, xdnd_status;
    Atom xdnd_leave, xdnd_drop, xdnd_finished, xdnd_selection;
    Atom xdnd_type_list, xdnd_action_copy, xdnd_uri_list, xdnd_property;
    Window xdnd_source;
    unsigned xdnd_version;
    int xdnd_uri_offered, xdnd_accept, xdnd_pending;
    int xdnd_waiting, xdnd_done, xdnd_property_changed;
    Atom xdnd_received;
    Time xdnd_time;
    float xdnd_x, xdnd_y;
    nx_platform_drop_queue drop_queue;
    char *clipboard_text;
    size_t clipboard_length;
    nx_x11_incr_transfer *incr_transfers;
    unsigned incr_transfer_count;
    int clipboard_waiting, clipboard_done;
    int clipboard_property_changed;
    Atom clipboard_received;
    Cursor cursor;
    nx_cursor_shape cursor_shape;
    nx_platform_event_queue event_queue;
    float scale;
    float width, height;
    int should_close, alive;
    int detectable_repeat;
    unsigned char keys_down[256];
    char *ime_preedit;
    size_t ime_preedit_length, ime_preedit_chars;
    size_t ime_preedit_caret_chars;
    int32_t ime_preedit_cursor;
    int ime_preedit_active, ime_enabled, ime_focused;
};

static _Thread_local nx_x11_window *nx_x11_thread_windows;
static uint64_t nx_x11_now_ns(void);

static void nx_x11_preedit_clear(nx_x11_window *window) {
    int changed = window->ime_preedit != NULL;
    if (changed)
        window->allocator.deallocate(window->allocator.user,
            window->ime_preedit, window->ime_preedit_length + 1u);
    window->ime_preedit = NULL;
    window->ime_preedit_length = window->ime_preedit_chars = 0;
    window->ime_preedit_caret_chars = 0;
    window->ime_preedit_cursor = 0;
    if (changed) {
        nx_event event = {0};
        event.type = NX_EVENT_IME_PREEDIT_CHANGED;
        nx_platform_event_queue_push(&window->event_queue, event);
    }
}

static size_t nx_x11_encode_scalar(uint32_t scalar, char *bytes) {
    if (scalar < 0x80u) {
        bytes[0] = (char)scalar;
        return 1;
    }
    if (scalar < 0x800u) {
        bytes[0] = (char)(0xc0u | (scalar >> 6u));
        bytes[1] = (char)(0x80u | (scalar & 0x3fu));
        return 2;
    }
    if (scalar < 0x10000u) {
        bytes[0] = (char)(0xe0u | (scalar >> 12u));
        bytes[1] = (char)(0x80u | ((scalar >> 6u) & 0x3fu));
        bytes[2] = (char)(0x80u | (scalar & 0x3fu));
        return 3;
    }
    bytes[0] = (char)(0xf0u | (scalar >> 18u));
    bytes[1] = (char)(0x80u | ((scalar >> 12u) & 0x3fu));
    bytes[2] = (char)(0x80u | ((scalar >> 6u) & 0x3fu));
    bytes[3] = (char)(0x80u | (scalar & 0x3fu));
    return 4;
}

static nx_result nx_x11_preedit_convert(nx_x11_window *window,
    const XIMText *text, char **out_bytes, size_t *out_length) {
    *out_bytes = NULL;
    *out_length = 0;
    if (!text || !text->length) return NX_OK;
    if (text->encoding_is_wchar ? !text->string.wide_char :
                                  !text->string.multi_byte)
        return NX_ERROR_ARGUMENT;
    size_t capacity = (size_t)text->length * 4u + 1u;
    char *bytes = window->allocator.allocate(window->allocator.user,
                                              capacity);
    if (!bytes) return NX_ERROR_MEMORY;
    size_t length = 0;
    const char *source = text->encoding_is_wchar ? NULL :
                         text->string.multi_byte;
    mbstate_t state = {0};
    for (size_t i = 0; i < text->length; ++i) {
        wchar_t wide = 0;
        if (text->encoding_is_wchar) wide = text->string.wide_char[i];
        else {
            size_t used = mbrtowc(&wide, source, MB_CUR_MAX, &state);
            if (used == (size_t)-1 || used == (size_t)-2 || !used) {
                window->allocator.deallocate(window->allocator.user,
                    bytes, capacity);
                return NX_ERROR_ARGUMENT;
            }
            source += used;
        }
        uint32_t scalar = (uint32_t)wide;
        if (!scalar || scalar > 0x10ffffu ||
            (scalar >= 0xd800u && scalar <= 0xdfffu)) {
            window->allocator.deallocate(window->allocator.user,
                bytes, capacity);
            return NX_ERROR_ARGUMENT;
        }
        length += nx_x11_encode_scalar(scalar, bytes + length);
    }
    bytes[length] = '\0';
    *out_bytes = bytes;
    *out_length = length;
    return NX_OK;
}

static nx_result nx_x11_preedit_byte_offset(const char *bytes,
    size_t length, size_t characters, size_t *out_offset) {
    size_t offset = 0;
    for (size_t i = 0; i < characters; ++i) {
        uint32_t scalar = 0;
        if (nx_utf8_decode(bytes, length, &offset, &scalar) != NX_OK)
            return NX_ERROR_ARGUMENT;
    }
    *out_offset = offset;
    return NX_OK;
}

static nx_result nx_x11_preedit_apply(nx_x11_window *window,
    int first, int removed, const char *insert, size_t inserted_length,
    size_t inserted_chars, int caret) {
    if (first < 0 || removed < 0 || caret < 0 ||
        (size_t)first > window->ime_preedit_chars ||
        (size_t)removed > window->ime_preedit_chars - (size_t)first)
        return NX_ERROR_ARGUMENT;
    size_t new_chars = window->ime_preedit_chars - (size_t)removed +
                       inserted_chars;
    if ((size_t)caret > new_chars) return NX_ERROR_ARGUMENT;
    size_t start = 0, end = 0;
    nx_result result = nx_x11_preedit_byte_offset(window->ime_preedit,
        window->ime_preedit_length, (size_t)first, &start);
    if (result != NX_OK) return result;
    result = nx_x11_preedit_byte_offset(window->ime_preedit,
        window->ime_preedit_length, (size_t)first + (size_t)removed, &end);
    if (result != NX_OK) return result;
    size_t length = window->ime_preedit_length - (end - start) +
                    inserted_length;
    if (length > 1048576u) return NX_ERROR_LIMIT;
    if (!length) {
        nx_x11_preedit_clear(window);
        return NX_OK;
    }
    char *copy = window->allocator.allocate(window->allocator.user,
                                            length + 1u);
    if (!copy) return NX_ERROR_MEMORY;
    if (start) memcpy(copy, window->ime_preedit, start);
    if (inserted_length) memcpy(copy + start, insert, inserted_length);
    size_t tail = window->ime_preedit_length - end;
    if (tail) memcpy(copy + start + inserted_length,
                     window->ime_preedit + end, tail);
    copy[length] = '\0';
    size_t cursor = 0;
    result = nx_x11_preedit_byte_offset(copy, length, (size_t)caret,
                                         &cursor);
    if (result != NX_OK) {
        window->allocator.deallocate(window->allocator.user, copy,
            length + 1u);
        return result;
    }
    if (window->ime_preedit_length == length &&
        window->ime_preedit_cursor == (int32_t)cursor &&
        window->ime_preedit &&
        memcmp(window->ime_preedit, copy, length) == 0) {
        window->allocator.deallocate(window->allocator.user, copy,
            length + 1u);
        return NX_OK;
    }
    if (window->ime_preedit)
        window->allocator.deallocate(window->allocator.user,
            window->ime_preedit, window->ime_preedit_length + 1u);
    window->ime_preedit = copy;
    window->ime_preedit_length = length;
    window->ime_preedit_chars = new_chars;
    window->ime_preedit_caret_chars = (size_t)caret;
    window->ime_preedit_cursor = (int32_t)cursor;
    nx_event event = {0};
    event.type = NX_EVENT_IME_PREEDIT_CHANGED;
    nx_platform_event_queue_push(&window->event_queue, event);
    return NX_OK;
}

static nx_result nx_x11_preedit_set_caret(nx_x11_window *window,
    size_t characters, int visible) {
    if (characters > window->ime_preedit_chars)
        return NX_ERROR_ARGUMENT;
    size_t offset = 0;
    nx_result result = nx_x11_preedit_byte_offset(window->ime_preedit,
        window->ime_preedit_length, characters, &offset);
    if (result != NX_OK) return result;
    int32_t cursor = visible ? (int32_t)offset : -1;
    window->ime_preedit_caret_chars = characters;
    if (cursor == window->ime_preedit_cursor) return NX_OK;
    window->ime_preedit_cursor = cursor;
    if (window->ime_preedit) {
        nx_event event = {0};
        event.type = NX_EVENT_IME_PREEDIT_CHANGED;
        nx_platform_event_queue_push(&window->event_queue, event);
    }
    return NX_OK;
}

static int nx_x11_preedit_start(XIC input, XPointer data,
                                XPointer call_data) {
    nx_x11_window *window = (nx_x11_window *)data;
    (void)input; (void)call_data;
    nx_x11_preedit_clear(window);
    window->ime_preedit_active = 1;
    return 1048576;
}

static void nx_x11_preedit_done(XIC input, XPointer data,
                                 XPointer call_data) {
    nx_x11_window *window = (nx_x11_window *)data;
    (void)input; (void)call_data;
    window->ime_preedit_active = 0;
    nx_x11_preedit_clear(window);
}

static void nx_x11_preedit_draw(XIC input, XPointer data,
    XIMPreeditDrawCallbackStruct *change) {
    nx_x11_window *window = (nx_x11_window *)data;
    (void)input;
    if (!change || !window->ime_enabled || !window->ime_focused ||
        !window->ime_preedit_active)
        return;
    nx_result result = NX_OK;
    if (change->text &&
        (change->text->encoding_is_wchar ?
            !change->text->string.wide_char :
            !change->text->string.multi_byte)) {
        result = nx_x11_preedit_set_caret(window,
            change->caret >= 0 ? (size_t)change->caret :
                window->ime_preedit_caret_chars, 1);
    } else {
        char *insert = NULL;
        size_t length = 0;
        result = nx_x11_preedit_convert(window, change->text,
                                         &insert, &length);
        if (result == NX_OK)
            result = nx_x11_preedit_apply(window, change->chg_first,
                change->chg_length, insert, length,
                change->text ? change->text->length : 0u,
                change->caret);
        if (insert)
            window->allocator.deallocate(window->allocator.user, insert,
                (size_t)change->text->length * 4u + 1u);
    }
    if (result == NX_ERROR_MEMORY && window->event_queue.error == NX_OK)
        window->event_queue.error = result;
    else if (result != NX_OK)
        nx_x11_preedit_clear(window);
}

static void nx_x11_preedit_caret(XIC input, XPointer data,
    XIMPreeditCaretCallbackStruct *move) {
    nx_x11_window *window = (nx_x11_window *)data;
    (void)input;
    if (!move || !window->ime_enabled || !window->ime_focused ||
        !window->ime_preedit_active)
        return;
    size_t caret = window->ime_preedit_caret_chars;
    switch (move->direction) {
    case XIMForwardChar:
        if (caret < window->ime_preedit_chars) ++caret;
        break;
    case XIMBackwardChar:
        if (caret) --caret;
        break;
    case XIMLineStart: caret = 0; break;
    case XIMLineEnd: caret = window->ime_preedit_chars; break;
    case XIMAbsolutePosition:
        if (move->position >= 0 &&
            (size_t)move->position <= window->ime_preedit_chars)
            caret = (size_t)move->position;
        break;
    default: break;
    }
    nx_result result = nx_x11_preedit_set_caret(window, caret,
        move->style != XIMIsInvisible);
    if (result == NX_ERROR_MEMORY && window->event_queue.error == NX_OK)
        window->event_queue.error = result;
    move->position = (int)caret;
}

static void nx_x11_remove_incr_transfer(nx_x11_window *window,
                                         nx_x11_incr_transfer **slot) {
    nx_x11_incr_transfer *transfer = *slot;
    *slot = transfer->next;
    window->allocator.deallocate(window->allocator.user, transfer->text,
                                 transfer->length);
    window->allocator.deallocate(window->allocator.user, transfer,
                                 sizeof(*transfer));
    --window->incr_transfer_count;
}

static void nx_x11_prune_incr_transfers(nx_x11_window *window) {
    uint64_t now = nx_x11_now_ns();
    if (!now) return;
    nx_x11_incr_transfer **slot = &window->incr_transfers;
    while (*slot) {
        nx_x11_incr_transfer *transfer = *slot;
        if (now >= transfer->last_progress_ns &&
            now - transfer->last_progress_ns >= 30000000000ull)
            nx_x11_remove_incr_transfer(window, slot);
        else slot = &transfer->next;
    }
}

static void nx_x11_unlink_thread_window(nx_x11_window *window) {
    nx_x11_window **slot = &nx_x11_thread_windows;
    while (*slot && *slot != window) slot = &(*slot)->thread_next;
    if (*slot) *slot = window->thread_next;
}

static void nx_x11_free_clipboard(nx_x11_window *window) {
    if (!window->clipboard_text) return;
    window->allocator.deallocate(window->allocator.user,
                                 window->clipboard_text,
                                 window->clipboard_length + 1u);
    window->clipboard_text = NULL;
    window->clipboard_length = 0;
}

static pthread_once_t nx_x11_threads_once = PTHREAD_ONCE_INIT;
static int nx_x11_threads_ready;

static void nx_x11_init_threads(void) {
    nx_x11_threads_ready = XInitThreads() != 0;
}

static void *nx_x11_allocate(void *user, size_t size) {
    (void)user;
    return malloc(size);
}

static void nx_x11_deallocate(void *user, void *pointer, size_t size) {
    (void)user;
    (void)size;
    free(pointer);
}

static int nx_x11_on_owner_thread(const nx_x11_window *window) {
    return pthread_equal(pthread_self(), window->owner_thread);
}

static uint32_t nx_x11_modifiers(unsigned int state) {
    uint32_t modifiers = 0;
    if (state & ShiftMask) modifiers |= NX_MOD_SHIFT;
    if (state & ControlMask) modifiers |= NX_MOD_CONTROL;
    if (state & Mod1Mask) modifiers |= NX_MOD_ALT;
    if (state & Mod4Mask) modifiers |= NX_MOD_SUPER;
    return modifiers;
}

static nx_key nx_x11_key(KeySym sym) {
    if (sym >= XK_a && sym <= XK_z) return (nx_key)(NX_KEY_A + sym - XK_a);
    if (sym >= XK_A && sym <= XK_Z) return (nx_key)(NX_KEY_A + sym - XK_A);
    if (sym >= XK_0 && sym <= XK_9) return (nx_key)(NX_KEY_0 + sym - XK_0);
    if (sym >= XK_KP_0 && sym <= XK_KP_9)
        return (nx_key)(NX_KEY_0 + sym - XK_KP_0);
    if (sym >= XK_F1 && sym <= XK_F12)
        return (nx_key)(NX_KEY_F1 + sym - XK_F1);
    switch (sym) {
    case XK_Tab: case XK_ISO_Left_Tab: return NX_KEY_TAB;
    case XK_Return: case XK_KP_Enter: return NX_KEY_ENTER;
    case XK_Escape: return NX_KEY_ESCAPE;
    case XK_space: return NX_KEY_SPACE;
    case XK_BackSpace: return NX_KEY_BACKSPACE;
    case XK_Delete: return NX_KEY_DELETE;
    case XK_Insert: return NX_KEY_INSERT;
    case XK_Left: return NX_KEY_LEFT;
    case XK_Right: return NX_KEY_RIGHT;
    case XK_Up: return NX_KEY_UP;
    case XK_Down: return NX_KEY_DOWN;
    case XK_Home: return NX_KEY_HOME;
    case XK_End: return NX_KEY_END;
    case XK_Prior: return NX_KEY_PAGE_UP;
    case XK_Next: return NX_KEY_PAGE_DOWN;
    case XK_Shift_L: case XK_Shift_R: return NX_KEY_SHIFT;
    case XK_Control_L: case XK_Control_R: return NX_KEY_CONTROL;
    case XK_Alt_L: case XK_Alt_R: return NX_KEY_ALT;
    case XK_Super_L: case XK_Super_R: return NX_KEY_SUPER;
    default: return NX_KEY_UNKNOWN;
    }
}

static uint32_t nx_x11_button(unsigned int button) {
    switch (button) {
    case Button1: return NX_BUTTON_LEFT;
    case Button2: return NX_BUTTON_MIDDLE;
    case Button3: return NX_BUTTON_RIGHT;
    case 8u: return NX_BUTTON_X1;
    case 9u: return NX_BUTTON_X2;
    default: return 0;
    }
}

static unsigned int nx_x11_cursor_glyph(nx_cursor_shape shape) {
    switch (shape) {
    case NX_CURSOR_DEFAULT: return XC_left_ptr;
    case NX_CURSOR_TEXT: return XC_xterm;
    case NX_CURSOR_LINK: return XC_hand2;
    case NX_CURSOR_CROSSHAIR: return XC_crosshair;
    case NX_CURSOR_RESIZE_HORIZONTAL: return XC_sb_h_double_arrow;
    case NX_CURSOR_RESIZE_VERTICAL: return XC_sb_v_double_arrow;
    case NX_CURSOR_RESIZE_DIAGONAL_NW_SE: return XC_bottom_right_corner;
    case NX_CURSOR_RESIZE_DIAGONAL_NE_SW: return XC_bottom_left_corner;
    case NX_CURSOR_MOVE: return XC_fleur;
    case NX_CURSOR_NOT_ALLOWED: return XC_X_cursor;
    case NX_CURSOR_BUSY: return XC_watch;
    default: return 0;
    }
}

static void nx_x11_emit_text(nx_x11_window *window, uint32_t scalar) {
    if (scalar < 0x20u || scalar == 0x7fu || scalar > 0x10ffffu ||
        (scalar >= 0xd800u && scalar <= 0xdfffu)) return;
    nx_event event = {0};
    event.type = NX_EVENT_TEXT;
    event.scalar = scalar;
    nx_platform_event_queue_push(&window->event_queue, event);
}

static void nx_x11_emit_utf8(nx_x11_window *window, const char *bytes,
                             size_t length) {
    if (nx_utf8_validate(bytes, length, NULL) != NX_OK) return;
    if (window->ime_preedit) nx_x11_preedit_clear(window);
    size_t offset = 0;
    while (offset < length) {
        uint32_t scalar = 0;
        if (nx_utf8_decode(bytes, length, &offset, &scalar) != NX_OK) break;
        nx_x11_emit_text(window, scalar);
    }
}

static void nx_x11_lookup_input_context(nx_x11_window *window,
                                         XKeyEvent *key) {
    char stack[64];
    char *bytes = stack;
    size_t capacity = sizeof(stack);
    KeySym sym = NoSymbol;
    Status status = XLookupNone;
    int length = Xutf8LookupString(window->input_context, key, stack,
                                   (int)sizeof(stack), &sym, &status);
    if (status == XBufferOverflow && length > 1048576) {
        window->event_queue.error = NX_ERROR_LIMIT;
        return;
    }
    if (status == XBufferOverflow && length > 0) {
        capacity = (size_t)length;
        bytes = (char *)window->allocator.allocate(window->allocator.user,
                                                   capacity);
        if (!bytes) {
            window->event_queue.error = NX_ERROR_MEMORY;
            return;
        }
        length = Xutf8LookupString(window->input_context, key, bytes,
                                    length, &sym, &status);
    }
    if ((status == XLookupChars || status == XLookupBoth) && length > 0)
        nx_x11_emit_utf8(window, bytes, (size_t)length);
    if (bytes != stack)
        window->allocator.deallocate(window->allocator.user, bytes,
                                     capacity);
}

static void nx_x11_open_input_method(nx_x11_window *window) {
    if (!XSupportsLocale() || !XSetLocaleModifiers("")) return;
    window->input_method = XOpenIM(window->display, NULL, NULL, NULL);
    if (!window->input_method) return;
    XIMStyles *styles = NULL;
    if (XGetIMValues(window->input_method, XNQueryInputStyle, &styles,
                     NULL) || !styles) {
        if (styles) XFree(styles);
        goto fail;
    }
    const XIMStyle preferred[] = {
        XIMPreeditPosition | XIMStatusNothing,
        XIMPreeditPosition | XIMStatusNone,
        XIMPreeditNothing | XIMStatusNothing,
        XIMPreeditNothing | XIMStatusNone,
        XIMPreeditNone | XIMStatusNone
    };
    for (size_t choice = 0; choice < sizeof(preferred) / sizeof(preferred[0]);
         ++choice) {
        int supported = 0;
        for (unsigned short i = 0; i < styles->count_styles; ++i)
            if (styles->supported_styles[i] == preferred[choice])
                supported = 1;
        if (!supported) continue;
        if (preferred[choice] & XIMPreeditPosition) {
            XPoint spot = {0, 0};
            XVaNestedList preedit = XVaCreateNestedList(0, XNSpotLocation,
                                                        &spot, NULL);
            if (!preedit) continue;
            window->input_context = XCreateIC(window->input_method,
                XNInputStyle, preferred[choice], XNClientWindow, window->xid,
                XNFocusWindow, window->xid, XNPreeditAttributes, preedit,
                NULL);
            XFree(preedit);
        } else {
            window->input_context = XCreateIC(window->input_method,
                XNInputStyle, preferred[choice], XNClientWindow, window->xid,
                XNFocusWindow, window->xid, NULL);
        }
        if (window->input_context) {
            window->input_style = preferred[choice];
            break;
        }
    }
    if (window->input_context) {
        window->fallback_context = window->input_context;
        window->fallback_style = window->input_style;
    }
    {
        const XIMStyle callback_styles[] = {
            XIMPreeditCallbacks | XIMStatusNothing,
            XIMPreeditCallbacks | XIMStatusNone
        };
        /* Xlib declares XIMProc as void, but preedit-start returns int. */
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-function-type"
#endif
        XIMCallback start = {(XPointer)window, (XIMProc)nx_x11_preedit_start};
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
        XIMCallback done = {(XPointer)window, (XIMProc)nx_x11_preedit_done};
        XIMCallback draw = {(XPointer)window, (XIMProc)nx_x11_preedit_draw};
        XIMCallback caret = {(XPointer)window, (XIMProc)nx_x11_preedit_caret};
        for (size_t choice = 0; choice < sizeof(callback_styles) /
             sizeof(callback_styles[0]); ++choice) {
            int supported = 0;
            for (unsigned short i = 0; i < styles->count_styles; ++i)
                if (styles->supported_styles[i] == callback_styles[choice])
                    supported = 1;
            if (!supported) continue;
            XVaNestedList preedit = XVaCreateNestedList(0,
                XNPreeditStartCallback, &start,
                XNPreeditDoneCallback, &done,
                XNPreeditDrawCallback, &draw,
                XNPreeditCaretCallback, &caret, NULL);
            if (!preedit) continue;
            window->callback_context = XCreateIC(window->input_method,
                XNInputStyle, callback_styles[choice],
                XNClientWindow, window->xid, XNFocusWindow, window->xid,
                XNPreeditAttributes, preedit, NULL);
            XFree(preedit);
            if (window->callback_context) {
                window->callback_style = callback_styles[choice];
                break;
            }
        }
    }
    XFree(styles);
    if (window->fallback_context || window->callback_context) return;
fail:
    XCloseIM(window->input_method);
    window->input_method = NULL;
}

static void nx_x11_close_input_method(nx_x11_window *window) {
    if (window->callback_context) XDestroyIC(window->callback_context);
    if (window->fallback_context) XDestroyIC(window->fallback_context);
    window->callback_context = window->fallback_context = NULL;
    window->input_context = NULL;
    if (window->input_method) XCloseIM(window->input_method);
    window->input_method = NULL;
}

static void nx_x11_emit_key(nx_x11_window *window, XKeyEvent *key, int down) {
    KeySym sym = XLookupKeysym(key, 0);
    nx_key logical = nx_x11_key(sym);
    unsigned int code = key->keycode;
    int repeat = down && code < sizeof(window->keys_down) &&
                 window->keys_down[code];
    if (code < sizeof(window->keys_down)) window->keys_down[code] = down;
    if (logical != NX_KEY_UNKNOWN) {
        nx_event event = {0};
        event.type = down ? NX_EVENT_KEY_DOWN : NX_EVENT_KEY_UP;
        event.key = logical;
        event.repeat = repeat;
        event.modifiers = nx_x11_modifiers(key->state);
        nx_platform_event_queue_push(&window->event_queue, event);
    }
    if (!down) return;
    if (window->input_context) {
        nx_x11_lookup_input_context(window, key);
        return;
    }
    if (key->state & (ControlMask | Mod1Mask | Mod4Mask)) return;
    char bytes[64];
    KeySym text_sym = NoSymbol;
    int length = XLookupString(key, bytes, (int)sizeof(bytes), &text_sym, NULL);
    if (length > 0 && nx_utf8_validate(bytes, (size_t)length, NULL) == NX_OK) {
        nx_x11_emit_utf8(window, bytes, (size_t)length);
    } else {
        /* XLookupString may return Latin-1 under a non-UTF-8 process locale. */
        if (text_sym == NoSymbol)
            text_sym = XLookupKeysym(key, (key->state & ShiftMask) ? 1 : 0);
        uint32_t scalar = 0;
        if (text_sym >= 0x20 && text_sym <= 0xff)
            scalar = (uint32_t)text_sym;
        else if ((text_sym & 0xff000000u) == 0x01000000u)
            scalar = (uint32_t)(text_sym & 0x00ffffffu);
        if (scalar) nx_x11_emit_text(window, scalar);
    }
}

static size_t nx_x11_direct_property_limit(Display *display) {
    long units = XMaxRequestSize(display);
    if (units <= 64) return 0;
    size_t payload_units = (size_t)(units - 64);
    return payload_units > SIZE_MAX / 4u ? SIZE_MAX : payload_units * 4u;
}

static int nx_x11_start_incr_transfer(nx_x11_window *window,
                                       const XSelectionRequestEvent *request,
                                       Atom property) {
    nx_x11_prune_incr_transfers(window);
    if (window->incr_transfer_count >= 4u) return 0;
    for (nx_x11_incr_transfer *current = window->incr_transfers;
         current; current = current->next)
        if (current->requestor == request->requestor &&
            current->property == property) return 0;
    nx_x11_incr_transfer *transfer = window->allocator.allocate(
        window->allocator.user, sizeof(*transfer));
    if (!transfer) return 0;
    memset(transfer, 0, sizeof(*transfer));
    transfer->text = window->allocator.allocate(window->allocator.user,
                                                window->clipboard_length);
    if (!transfer->text) {
        window->allocator.deallocate(window->allocator.user, transfer,
                                     sizeof(*transfer));
        return 0;
    }
    memcpy(transfer->text, window->clipboard_text,
           window->clipboard_length);
    transfer->requestor = request->requestor;
    transfer->property = property;
    transfer->length = window->clipboard_length;
    transfer->last_progress_ns = nx_x11_now_ns();
    transfer->next = window->incr_transfers;
    window->incr_transfers = transfer;
    ++window->incr_transfer_count;
    if (request->requestor != window->xid)
        XSelectInput(window->display, request->requestor,
                     PropertyChangeMask);
    unsigned long announced = (unsigned long)transfer->length;
    XChangeProperty(window->display, request->requestor, property,
                    window->clipboard_incr, 32, PropModeReplace,
                    (const unsigned char *)&announced, 1);
    return 1;
}

static void nx_x11_incr_property_deleted(nx_x11_window *window,
                                          Window requestor, Atom property) {
    nx_x11_incr_transfer **slot = &window->incr_transfers;
    while (*slot) {
        nx_x11_incr_transfer *transfer = *slot;
        if (transfer->requestor != requestor ||
            transfer->property != property) {
            slot = &transfer->next;
            continue;
        }
        if (transfer->final_sent) {
            nx_x11_remove_incr_transfer(window, slot);
            return;
        }
        size_t remaining = transfer->length - transfer->offset;
        size_t chunk = remaining < 65536u ? remaining : 65536u;
        size_t direct_limit = nx_x11_direct_property_limit(window->display);
        if (chunk > direct_limit) chunk = direct_limit;
        XChangeProperty(window->display, requestor, property,
                        window->clipboard_utf8, 8, PropModeReplace,
                        (const unsigned char *)(transfer->text +
                                                transfer->offset), (int)chunk);
        transfer->offset += chunk;
        transfer->final_sent = chunk == 0;
        transfer->last_progress_ns = nx_x11_now_ns();
        XFlush(window->display);
        return;
    }
}

static void nx_x11_selection_request(nx_x11_window *window,
                                      const XSelectionRequestEvent *request) {
    if (request->owner != window->xid ||
        request->selection != window->clipboard) return;
    Atom property = request->property != None ?
                    request->property : request->target;
    XEvent reply = {0};
    reply.xselection.type = SelectionNotify;
    reply.xselection.display = window->display;
    reply.xselection.requestor = request->requestor;
    reply.xselection.selection = request->selection;
    reply.xselection.target = request->target;
    reply.xselection.time = request->time;
    reply.xselection.property = None;
    if (window->clipboard_text && request->target == window->clipboard_utf8) {
        if (window->clipboard_length <=
            nx_x11_direct_property_limit(window->display)) {
            XChangeProperty(window->display, request->requestor, property,
                            window->clipboard_utf8, 8, PropModeReplace,
                            (const unsigned char *)window->clipboard_text,
                            (int)window->clipboard_length);
            reply.xselection.property = property;
        } else if (nx_x11_start_incr_transfer(window, request, property))
            reply.xselection.property = property;
    } else if (window->clipboard_text &&
               request->target == window->clipboard_targets) {
        Atom targets[] = {window->clipboard_targets, window->clipboard_utf8};
        XChangeProperty(window->display, request->requestor, property,
                        XA_ATOM, 32, PropModeReplace,
                        (const unsigned char *)targets, 2);
        reply.xselection.property = property;
    }
    XSendEvent(window->display, request->requestor, False, NoEventMask,
               &reply);
    XFlush(window->display);
}

static void nx_x11_dnd_reset(nx_x11_window *window) {
    window->xdnd_source = None;
    window->xdnd_version = 0;
    window->xdnd_uri_offered = 0;
    window->xdnd_accept = 0;
    window->xdnd_pending = 0;
}

static void nx_x11_dnd_send(nx_x11_window *window, Atom message,
                             int accepted) {
    if (window->xdnd_source == None) return;
    XEvent response = {0};
    response.xclient.type = ClientMessage;
    response.xclient.display = window->display;
    response.xclient.window = window->xdnd_source;
    response.xclient.message_type = message;
    response.xclient.format = 32;
    response.xclient.data.l[0] = (long)window->xid;
    if (message == window->xdnd_status) {
        response.xclient.data.l[1] = accepted ? 3L : 2L;
        response.xclient.data.l[4] = accepted ?
            (long)window->xdnd_action_copy : (long)None;
    } else if (message == window->xdnd_finished) {
        response.xclient.data.l[1] = accepted ? 1L : 0L;
        response.xclient.data.l[2] = accepted ?
            (long)window->xdnd_action_copy : (long)None;
    }
    XSendEvent(window->display, window->xdnd_source, False,
               NoEventMask, &response);
    XFlush(window->display);
}

static void nx_x11_dnd_enter(nx_x11_window *window,
                              const XClientMessageEvent *message) {
    Window source = (Window)message->data.l[0];
    unsigned version = ((unsigned long)message->data.l[1] >> 24) & 0xffu;
    if (source == None || version < 3u || version > 5u ||
        window->xdnd_waiting || window->xdnd_pending ||
        (window->xdnd_source != None && window->xdnd_source != source))
        return;
    nx_x11_dnd_reset(window);
    window->xdnd_source = source;
    window->xdnd_version = version;
    if (message->data.l[1] & 1L) {
        Atom actual = None;
        int format = 0;
        unsigned long count = 0, after = 0;
        unsigned char *types = NULL;
        if (XGetWindowProperty(window->display, source,
                window->xdnd_type_list, 0, 65536L, False, XA_ATOM,
                &actual, &format, &count, &after, &types) == Success &&
            actual == XA_ATOM && format == 32 && after == 0) {
            Atom *atoms = (Atom *)types;
            for (unsigned long i = 0; i < count; ++i)
                if (atoms[i] == window->xdnd_uri_list)
                    window->xdnd_uri_offered = 1;
        }
        if (types) XFree(types);
    } else {
        for (int i = 2; i <= 4; ++i)
            if ((Atom)message->data.l[i] == window->xdnd_uri_list)
                window->xdnd_uri_offered = 1;
    }
}

static void nx_x11_dnd_position(nx_x11_window *window,
                                 const XClientMessageEvent *message) {
    if ((Window)message->data.l[0] != window->xdnd_source ||
        window->xdnd_source == None || window->xdnd_pending) return;
    uint32_t packed = (uint32_t)message->data.l[2];
    int root_x = (int16_t)(packed >> 16);
    int root_y = (int16_t)(packed & 0xffffu);
    int local_x = 0, local_y = 0;
    Window child = None;
    int translated = XTranslateCoordinates(window->display,
            DefaultRootWindow(window->display), window->xid,
            root_x, root_y, &local_x, &local_y, &child);
    if (translated) {
        window->xdnd_x = (float)local_x / window->scale;
        window->xdnd_y = (float)local_y / window->scale;
    }
    window->xdnd_time = (Time)message->data.l[3];
    window->xdnd_accept = window->xdnd_uri_offered && translated;
    nx_x11_dnd_send(window, window->xdnd_status, window->xdnd_accept);
}

static void nx_x11_dnd_drop(nx_x11_window *window,
                             const XClientMessageEvent *message) {
    if ((Window)message->data.l[0] != window->xdnd_source ||
        window->xdnd_source == None || window->xdnd_pending) return;
    if (!window->xdnd_accept) {
        nx_x11_dnd_send(window, window->xdnd_finished, 0);
        nx_x11_dnd_reset(window);
        return;
    }
    window->xdnd_time = (Time)message->data.l[2];
    window->xdnd_pending = 1;
}

static nx_result nx_x11_process_drop(nx_x11_window *window);

static void nx_x11_dispatch(nx_x11_window *window, XEvent *native) {
    if (native->type == PropertyNotify &&
        native->xproperty.state == PropertyDelete &&
        native->xproperty.window != window->xid) {
        nx_x11_incr_property_deleted(window, native->xproperty.window,
                                     native->xproperty.atom);
        return;
    }
    if (native->type == SelectionRequest) {
        nx_x11_selection_request(window, &native->xselectionrequest);
        return;
    }
    if (native->type == SelectionClear &&
        native->xselectionclear.window == window->xid &&
        native->xselectionclear.selection == window->clipboard) {
        nx_x11_free_clipboard(window);
        return;
    }
    if (native->type == SelectionNotify && window->clipboard_waiting &&
        native->xselection.requestor == window->xid &&
        native->xselection.selection == window->clipboard &&
        native->xselection.target == window->clipboard_utf8) {
        window->clipboard_received = native->xselection.property;
        window->clipboard_done = 1;
        return;
    }
    if (native->type == SelectionNotify && window->xdnd_waiting &&
        native->xselection.requestor == window->xid &&
        native->xselection.selection == window->xdnd_selection &&
        native->xselection.target == window->xdnd_uri_list) {
        window->xdnd_received = native->xselection.property;
        window->xdnd_done = 1;
        return;
    }
    if (native->type == PropertyNotify && window->clipboard_waiting &&
        native->xproperty.window == window->xid &&
        native->xproperty.atom == window->clipboard_property &&
        native->xproperty.state == PropertyNewValue) {
        window->clipboard_property_changed = 1;
        return;
    }
    if (native->type == PropertyNotify && window->xdnd_waiting &&
        native->xproperty.window == window->xid &&
        native->xproperty.atom == window->xdnd_property &&
        native->xproperty.state == PropertyNewValue) {
        window->xdnd_property_changed = 1;
        return;
    }
    if (native->xany.window != window->xid) return;
    nx_event event = {0};
    switch (native->type) {
    case ClientMessage:
        if (native->xclient.message_type == window->wm_protocols &&
            native->xclient.format == 32 &&
            (Atom)native->xclient.data.l[0] == window->wm_delete_window)
            window->should_close = 1;
        else if (native->xclient.format == 32) {
            Atom message = native->xclient.message_type;
            if (message == window->xdnd_enter)
                nx_x11_dnd_enter(window, &native->xclient);
            else if (message == window->xdnd_position)
                nx_x11_dnd_position(window, &native->xclient);
            else if (message == window->xdnd_leave &&
                     (Window)native->xclient.data.l[0] ==
                         window->xdnd_source && !window->xdnd_pending &&
                     !window->xdnd_waiting)
                nx_x11_dnd_reset(window);
            else if (message == window->xdnd_drop)
                nx_x11_dnd_drop(window, &native->xclient);
        }
        break;
    case DestroyNotify:
        window->alive = 0;
        window->should_close = 1;
        break;
    case ConfigureNotify:
        if (native->xconfigure.width != window->width ||
            native->xconfigure.height != window->height) {
            window->width = (float)native->xconfigure.width;
            window->height = (float)native->xconfigure.height;
            event.type = NX_EVENT_RESIZE;
            event.width = window->width;
            event.height = window->height;
            event.scale = window->scale;
            nx_platform_event_queue_push(&window->event_queue, event);
        }
        break;
    case FocusIn: case FocusOut:
        event.type = NX_EVENT_FOCUS;
        event.focused = native->type == FocusIn;
        window->ime_focused = event.focused;
        if (window->input_context) {
            if (event.focused) XSetICFocus(window->input_context);
            else XUnsetICFocus(window->input_context);
        }
        if (!event.focused) {
            window->ime_preedit_active = 0;
            nx_x11_preedit_clear(window);
        }
        if (!event.focused) memset(window->keys_down, 0,
                                   sizeof(window->keys_down));
        nx_platform_event_queue_push(&window->event_queue, event);
        break;
    case MotionNotify:
        event.type = NX_EVENT_POINTER_MOVE;
        event.x = (float)native->xmotion.x / window->scale;
        event.y = (float)native->xmotion.y / window->scale;
        event.modifiers = nx_x11_modifiers(native->xmotion.state);
        nx_platform_event_queue_push(&window->event_queue, event);
        break;
    case ButtonPress: case ButtonRelease: {
        unsigned int button = native->xbutton.button;
        if (button >= Button4 && button <= 7u) {
            if (native->type == ButtonPress) {
                event.type = NX_EVENT_WHEEL;
                event.x = (float)native->xbutton.x / window->scale;
                event.y = (float)native->xbutton.y / window->scale;
                event.modifiers = nx_x11_modifiers(native->xbutton.state);
                if (button == Button4) event.delta_y = 1.0f;
                if (button == Button5) event.delta_y = -1.0f;
                if (button == 6u) event.delta_x = -1.0f;
                if (button == 7u) event.delta_x = 1.0f;
                nx_platform_event_queue_push(&window->event_queue, event);
            }
            break;
        }
        event.button = nx_x11_button(button);
        if (!event.button) break;
        event.type = native->type == ButtonPress ?
            NX_EVENT_POINTER_DOWN : NX_EVENT_POINTER_UP;
        event.x = (float)native->xbutton.x / window->scale;
        event.y = (float)native->xbutton.y / window->scale;
        event.modifiers = nx_x11_modifiers(native->xbutton.state);
        nx_platform_event_queue_push(&window->event_queue, event);
        break;
    }
    case KeyPress: nx_x11_emit_key(window, &native->xkey, 1); break;
    case KeyRelease: nx_x11_emit_key(window, &native->xkey, 0); break;
    default: break;
    }
}

nx_result nx_x11_window_create(const nx_window_desc *desc, nx_x11_window **out_window) {
    if (!out_window) return NX_ERROR_ARGUMENT;
    *out_window = NULL;
    if (!desc || !isfinite(desc->width) || !isfinite(desc->height) ||
        desc->width <= 0.0f || desc->height <= 0.0f ||
        (desc->flags & ~NX_WINDOW_HIDDEN)) return NX_ERROR_ARGUMENT;
    double width = ceil((double)desc->width);
    double height = ceil((double)desc->height);
    if (width > UINT16_MAX || height > UINT16_MAX) return NX_ERROR_LIMIT;
    const char *title = desc->title_utf8 ? desc->title_utf8 : "NexaUI";
    size_t title_length = strlen(title);
    if (title_length > INT_MAX ||
        nx_utf8_validate(title, title_length, NULL) != NX_OK)
        return NX_ERROR_ARGUMENT;
    nx_allocator allocator = desc->allocator;
    if ((allocator.allocate == NULL) != (allocator.deallocate == NULL))
        return NX_ERROR_ARGUMENT;
    if (!allocator.allocate) {
        allocator = (nx_allocator){NULL, nx_x11_allocate, nx_x11_deallocate};
    }
    if (pthread_once(&nx_x11_threads_once, nx_x11_init_threads) != 0 ||
        !nx_x11_threads_ready) return NX_ERROR_PLATFORM;
    nx_x11_window *window = (nx_x11_window *)allocator.allocate(allocator.user,
                                                        sizeof(nx_x11_window));
    if (!window) return NX_ERROR_MEMORY;
    memset(window, 0, sizeof(*window));
    window->allocator = allocator;
    window->owner_thread = pthread_self();
    window->scale = 1.0f;
    window->width = (float)width;
    window->height = (float)height;
    nx_result result = nx_platform_event_queue_init(&window->event_queue,
                                                     allocator, 64u);
    if (result != NX_OK) goto fail;
    nx_platform_drop_queue_init(&window->drop_queue, allocator);
    window->display = XOpenDisplay(NULL);
    if (!window->display) { result = NX_ERROR_PLATFORM; goto fail; }
    Bool repeat_supported = False;
    window->detectable_repeat =
        XkbSetDetectableAutoRepeat(window->display, True,
                                   &repeat_supported) && repeat_supported;
    window->xid = XCreateSimpleWindow(window->display,
                  DefaultRootWindow(window->display), 0, 0,
                  (unsigned int)width, (unsigned int)height, 0,
                  BlackPixel(window->display, DefaultScreen(window->display)),
                  WhitePixel(window->display, DefaultScreen(window->display)));
    if (!window->xid) { result = NX_ERROR_PLATFORM; goto fail; }
    window->alive = 1;
    XSelectInput(window->display, window->xid,
        StructureNotifyMask | FocusChangeMask | PointerMotionMask |
        ButtonPressMask | ButtonReleaseMask | KeyPressMask | KeyReleaseMask |
        PropertyChangeMask);
    nx_x11_open_input_method(window);
    window->wm_protocols = XInternAtom(window->display, "WM_PROTOCOLS", False);
    window->wm_delete_window = XInternAtom(window->display,
                                            "WM_DELETE_WINDOW", False);
    window->clipboard = XInternAtom(window->display, "CLIPBOARD", False);
    window->clipboard_targets = XInternAtom(window->display, "TARGETS", False);
    window->clipboard_utf8 = XInternAtom(window->display, "UTF8_STRING", False);
    window->clipboard_property = XInternAtom(window->display,
                                              "NEXAUI_CLIPBOARD_DATA", False);
    window->clipboard_incr = XInternAtom(window->display, "INCR", False);
    window->xdnd_aware = XInternAtom(window->display, "XdndAware", False);
    window->xdnd_enter = XInternAtom(window->display, "XdndEnter", False);
    window->xdnd_position = XInternAtom(window->display, "XdndPosition", False);
    window->xdnd_status = XInternAtom(window->display, "XdndStatus", False);
    window->xdnd_leave = XInternAtom(window->display, "XdndLeave", False);
    window->xdnd_drop = XInternAtom(window->display, "XdndDrop", False);
    window->xdnd_finished = XInternAtom(window->display, "XdndFinished", False);
    window->xdnd_selection = XInternAtom(window->display,
                                          "XdndSelection", False);
    window->xdnd_type_list = XInternAtom(window->display,
                                          "XdndTypeList", False);
    window->xdnd_action_copy = XInternAtom(window->display,
                                            "XdndActionCopy", False);
    window->xdnd_uri_list = XInternAtom(window->display,
                                         "text/uri-list", False);
    window->xdnd_property = XInternAtom(window->display,
                                         "NEXAUI_XDND_DATA", False);
    if (window->wm_protocols == None || window->wm_delete_window == None ||
        window->clipboard == None || window->clipboard_targets == None ||
        window->clipboard_utf8 == None ||
        window->clipboard_property == None || window->clipboard_incr == None ||
        window->xdnd_aware == None || window->xdnd_enter == None ||
        window->xdnd_position == None || window->xdnd_status == None ||
        window->xdnd_leave == None || window->xdnd_drop == None ||
        window->xdnd_finished == None || window->xdnd_selection == None ||
        window->xdnd_type_list == None ||
        window->xdnd_action_copy == None ||
        window->xdnd_uri_list == None || window->xdnd_property == None ||
        !XSetWMProtocols(window->display, window->xid,
                          &window->wm_delete_window, 1)) {
        result = NX_ERROR_PLATFORM;
        goto fail;
    }
    unsigned long xdnd_version = 5;
    XChangeProperty(window->display, window->xid, window->xdnd_aware,
                    XA_ATOM, 32, PropModeReplace,
                    (const unsigned char *)&xdnd_version, 1);
    Atom utf8 = XInternAtom(window->display, "UTF8_STRING", False);
    Atom net_name = XInternAtom(window->display, "_NET_WM_NAME", False);
    if (utf8 != None && net_name != None)
        XChangeProperty(window->display, window->xid, net_name, utf8, 8,
                        PropModeReplace, (const unsigned char *)title,
                        (int)title_length);
    XStoreName(window->display, window->xid, title);
    window->cursor = XCreateFontCursor(window->display, XC_left_ptr);
    if (window->cursor == None) { result = NX_ERROR_PLATFORM; goto fail; }
    XDefineCursor(window->display, window->xid, window->cursor);
    if (!(desc->flags & NX_WINDOW_HIDDEN))
        XMapWindow(window->display, window->xid);
    XFlush(window->display);
    window->thread_next = nx_x11_thread_windows;
    nx_x11_thread_windows = window;
    *out_window = window;
    return NX_OK;
fail:
    if (window->display) {
        nx_x11_close_input_method(window);
        if (window->cursor != None) XFreeCursor(window->display, window->cursor);
        if (window->alive) XDestroyWindow(window->display, window->xid);
        XCloseDisplay(window->display);
    }
    nx_platform_event_queue_destroy(&window->event_queue);
    nx_platform_drop_queue_destroy(&window->drop_queue);
    allocator.deallocate(allocator.user, window, sizeof(*window));
    return result;
}

static void nx_x11_dispatch_pending(nx_x11_window *window) {
    nx_x11_prune_incr_transfers(window);
    while (XPending(window->display)) {
        XEvent event;
        XNextEvent(window->display, &event);
        if (!window->detectable_repeat && event.type == KeyRelease &&
            XPending(window->display)) {
            XEvent next;
            XPeekEvent(window->display, &next);
            if (next.type == KeyPress &&
                next.xkey.window == event.xkey.window &&
                next.xkey.keycode == event.xkey.keycode &&
                next.xkey.time == event.xkey.time) {
                XNextEvent(window->display, &event);
            }
        }
        if (window->input_context && XFilterEvent(&event, None)) continue;
        nx_x11_dispatch(window, &event);
    }
}

nx_result nx_x11_window_poll(nx_x11_window *window, uint32_t timeout_ms) {
    if (!window) return NX_ERROR_ARGUMENT;
    if (!nx_x11_on_owner_thread(window) || !window->alive)
        return NX_ERROR_STATE;
    if (!XPending(window->display)) {
        struct pollfd fd = {ConnectionNumber(window->display), POLLIN, 0};
        int timeout = timeout_ms > INT_MAX ? INT_MAX : (int)timeout_ms;
        int ready = poll(&fd, 1, timeout);
        if (ready < 0 && errno != EINTR) return NX_ERROR_PLATFORM;
        if (ready > 0 && (fd.revents & (POLLERR | POLLHUP | POLLNVAL)))
            return NX_ERROR_PLATFORM;
    }
    nx_x11_dispatch_pending(window);
    nx_result drop_result = nx_x11_process_drop(window);
    if (drop_result != NX_OK) return drop_result;
    return window->event_queue.error;
}

nx_result nx_x11_window_get_info(nx_x11_window *window, nx_window_info *out_info) {
    if (!window || !out_info) return NX_ERROR_ARGUMENT;
    if (!nx_x11_on_owner_thread(window) || !window->alive)
        return NX_ERROR_STATE;
    *out_info = (nx_window_info){window->width / window->scale,
                                 window->height / window->scale,
                                 window->scale, window->should_close};
    return NX_OK;
}

nx_result nx_x11_window_set_cursor(nx_x11_window *window, nx_cursor_shape shape) {
    if (!window || shape < NX_CURSOR_DEFAULT || shape > NX_CURSOR_BUSY)
        return NX_ERROR_ARGUMENT;
    if (!nx_x11_on_owner_thread(window) || !window->alive)
        return NX_ERROR_STATE;
    if (shape == window->cursor_shape) return NX_OK;
    Cursor cursor = XCreateFontCursor(window->display,
                                      nx_x11_cursor_glyph(shape));
    if (cursor == None) return NX_ERROR_PLATFORM;
    XDefineCursor(window->display, window->xid, cursor);
    XFlush(window->display);
    XFreeCursor(window->display, window->cursor);
    window->cursor = cursor;
    window->cursor_shape = shape;
    return NX_OK;
}

nx_result nx_x11_window_get_reduced_motion(nx_x11_window *window, int *out_enabled) {
    if (!out_enabled) return NX_ERROR_ARGUMENT;
    *out_enabled = 0;
    if (!window) return NX_ERROR_ARGUMENT;
    if (!nx_x11_on_owner_thread(window) || !window->alive)
        return NX_ERROR_STATE;
    return NX_ERROR_UNSUPPORTED;
}

nx_result nx_x11_window_set_ime_rect(nx_x11_window *window, nx_rect caret) {
    if (!window || !isfinite(caret.x) || !isfinite(caret.y) ||
        !isfinite(caret.width) || !isfinite(caret.height) ||
        caret.width <= 0.0f || caret.height <= 0.0f)
        return NX_ERROR_ARGUMENT;
    if (!nx_x11_on_owner_thread(window) || !window->alive)
        return NX_ERROR_STATE;
    if (!window->input_context ||
        !(window->input_style & XIMPreeditPosition))
        return NX_ERROR_UNSUPPORTED;
    double x = round((double)caret.x * window->scale);
    double y = round(((double)caret.y + caret.height) * window->scale);
    if (x < SHRT_MIN || x > SHRT_MAX || y < SHRT_MIN || y > SHRT_MAX)
        return NX_ERROR_LIMIT;
    XPoint spot = {(short)x, (short)y};
    XVaNestedList preedit = XVaCreateNestedList(0, XNSpotLocation, &spot, NULL);
    if (!preedit) return NX_ERROR_MEMORY;
    char *failed = XSetICValues(window->input_context,
                                 XNPreeditAttributes, preedit, NULL);
    XFree(preedit);
    if (failed) return NX_ERROR_PLATFORM;
    XFlush(window->display);
    return NX_OK;
}

nx_result nx_x11_window_set_ime_enabled(nx_x11_window *window, int enabled) {
    if (!window || (enabled != 0 && enabled != 1)) return NX_ERROR_ARGUMENT;
    if (!nx_x11_on_owner_thread(window) || !window->alive)
        return NX_ERROR_STATE;
    if (enabled && !window->callback_context)
        return NX_ERROR_UNSUPPORTED;
    if (window->ime_enabled == enabled) return NX_OK;
    if (window->input_context && window->ime_focused)
        XUnsetICFocus(window->input_context);
    window->ime_enabled = enabled;
    window->input_context = enabled ? window->callback_context :
                                      window->fallback_context;
    window->input_style = enabled ? window->callback_style :
                                      window->fallback_style;
    if (window->input_context && window->ime_focused)
        XSetICFocus(window->input_context);
    if (!enabled) {
        window->ime_preedit_active = 0;
        nx_x11_preedit_clear(window);
    }
    XFlush(window->display);
    return NX_OK;
}

nx_result nx_x11_window_get_ime_preedit(nx_x11_window *window,
    char *bytes, size_t capacity, size_t *out_length,
    int32_t *out_cursor_begin, int32_t *out_cursor_end) {
    if (!out_length || !out_cursor_begin || !out_cursor_end ||
        (!bytes && capacity)) return NX_ERROR_ARGUMENT;
    *out_length = 0;
    *out_cursor_begin = *out_cursor_end = 0;
    if (!window) return NX_ERROR_ARGUMENT;
    if (!nx_x11_on_owner_thread(window) || !window->alive)
        return NX_ERROR_STATE;
    if (!window->callback_context && !window->ime_preedit)
        return NX_ERROR_UNSUPPORTED;
    *out_length = window->ime_preedit_length;
    *out_cursor_begin = *out_cursor_end = window->ime_preedit_cursor;
    if (!bytes) return NX_OK;
    if (capacity <= window->ime_preedit_length) return NX_ERROR_LIMIT;
    if (window->ime_preedit_length)
        memcpy(bytes, window->ime_preedit, window->ime_preedit_length);
    bytes[window->ime_preedit_length] = '\0';
    return NX_OK;
}

nx_result nx_x11_window_next_event(nx_x11_window *window, nx_event *out_event,
                               int *available) {
    if (!out_event || !available) return NX_ERROR_ARGUMENT;
    *out_event = (nx_event){0};
    *available = 0;
    if (!window) return NX_ERROR_ARGUMENT;
    if (!nx_x11_on_owner_thread(window) || !window->alive)
        return NX_ERROR_STATE;
    return nx_platform_event_queue_next(&window->event_queue,
                                         out_event, available);
}

nx_result nx_x11_window_route_events(nx_x11_window *window, nx_context *context,
                                 size_t *out_count) {
    if (!out_count) return NX_ERROR_ARGUMENT;
    *out_count = 0;
    if (!window || !context) return NX_ERROR_ARGUMENT;
    if (!nx_x11_on_owner_thread(window) || !window->alive)
        return NX_ERROR_STATE;
    return nx_platform_event_queue_route(&window->event_queue,
                                          context, out_count);
}

nx_result nx_x11_window_next_file_drop(nx_x11_window *window,
                                   char *path_utf8, size_t capacity,
                                   size_t *out_length,
                                   nx_file_drop_info *out_info,
                                   int *available) {
    if (!out_length || !out_info || !available || (!path_utf8 && capacity))
        return NX_ERROR_ARGUMENT;
    *out_length = 0;
    *out_info = (nx_file_drop_info){0};
    *available = 0;
    if (!window) return NX_ERROR_ARGUMENT;
    if (!nx_x11_on_owner_thread(window) || !window->alive)
        return NX_ERROR_STATE;
    return nx_platform_drop_queue_next(&window->drop_queue, path_utf8,
                                        capacity, out_length, out_info,
                                        available);
}

nx_result nx_x11_window_clipboard_set_text(nx_x11_window *window,
                                        const char *bytes, size_t length) {
    if (!window || (!bytes && length) ||
        (length && memchr(bytes, 0, length)) ||
        nx_utf8_validate(bytes, length, NULL) != NX_OK)
        return NX_ERROR_ARGUMENT;
    if (!nx_x11_on_owner_thread(window) || !window->alive)
        return NX_ERROR_STATE;
    if (!nx_x11_direct_property_limit(window->display))
        return NX_ERROR_PLATFORM;
    if (length > 32u * 1024u * 1024u || length > INT_MAX ||
        length == SIZE_MAX) return NX_ERROR_LIMIT;
    char *copy = window->allocator.allocate(window->allocator.user,
                                            length + 1u);
    if (!copy) return NX_ERROR_MEMORY;
    if (length) memcpy(copy, bytes, length);
    copy[length] = '\0';
    XSetSelectionOwner(window->display, window->clipboard,
                       window->xid, CurrentTime);
    if (XGetSelectionOwner(window->display, window->clipboard) !=
        window->xid) {
        window->allocator.deallocate(window->allocator.user, copy,
                                     length + 1u);
        return NX_ERROR_PLATFORM;
    }
    nx_x11_free_clipboard(window);
    window->clipboard_text = copy;
    window->clipboard_length = length;
    return NX_OK;
}

static nx_result nx_x11_clipboard_copy(const unsigned char *source,
                                       size_t length, char *bytes,
                                       size_t capacity, size_t *out_length) {
    if ((length && memchr(source, 0, length)) ||
        nx_utf8_validate((const char *)source, length, NULL) != NX_OK)
        return NX_ERROR_STATE;
    *out_length = length;
    if (!bytes && !capacity) return NX_OK;
    if (capacity <= length) return NX_ERROR_LIMIT;
    if (length) memcpy(bytes, source, length);
    bytes[length] = '\0';
    return NX_OK;
}

static uint64_t nx_x11_now_ns(void) {
    struct timespec now;
    if (clock_gettime(CLOCK_MONOTONIC, &now) != 0) return 0;
    return (uint64_t)now.tv_sec * 1000000000ull + (uint64_t)now.tv_nsec;
}

static nx_result nx_x11_wait_clipboard(nx_x11_window *window,
                                        const int *complete,
                                        uint64_t deadline_ns) {
    while (!*complete) {
        for (nx_x11_window *candidate = nx_x11_thread_windows;
             candidate; candidate = candidate->thread_next)
            nx_x11_dispatch_pending(candidate);
        if (*complete) break;
        uint64_t now = nx_x11_now_ns();
        if (!now || now >= deadline_ns)
            return NX_ERROR_PLATFORM;
        struct pollfd fd = {ConnectionNumber(window->display), POLLIN, 0};
        int ready = poll(&fd, 1, 10);
        if (ready < 0 && errno != EINTR) return NX_ERROR_PLATFORM;
        if (ready > 0 && (fd.revents & (POLLERR | POLLHUP | POLLNVAL)))
            return NX_ERROR_PLATFORM;
    }
    return NX_OK;
}

static nx_result nx_x11_process_drop(nx_x11_window *window) {
    if (!window->xdnd_pending) return NX_OK;
    window->xdnd_pending = 0;
    nx_result result = NX_ERROR_PLATFORM;
    char *collected = NULL;
    size_t used = 0, allocated = 0;
    char local_host[256] = {0};
    if (gethostname(local_host, sizeof(local_host)) != 0)
        local_host[0] = '\0';
    local_host[sizeof(local_host) - 1u] = '\0';
    const char *host = local_host[0] ? local_host : NULL;
    size_t before_count = window->drop_queue.count;
    uint64_t started = nx_x11_now_ns();
    if (!started || XGetSelectionOwner(window->display,
                                       window->xdnd_selection) == None)
        goto done;
    uint64_t deadline = started + 5000000000ull;
    XDeleteProperty(window->display, window->xid, window->xdnd_property);
    window->xdnd_waiting = 1;
    window->xdnd_done = 0;
    window->xdnd_received = None;
    XConvertSelection(window->display, window->xdnd_selection,
                      window->xdnd_uri_list, window->xdnd_property,
                      window->xid, window->xdnd_time);
    XFlush(window->display);
    result = nx_x11_wait_clipboard(window, &window->xdnd_done, deadline);
    if (result != NX_OK) goto done;
    if (window->xdnd_received != window->xdnd_property) {
        result = NX_ERROR_STATE;
        goto done;
    }
    Atom actual = None;
    int format = 0;
    unsigned long count = 0, after = 0;
    unsigned char *data = NULL;
    if (XGetWindowProperty(window->display, window->xid,
            window->xdnd_property, 0, 0, False, AnyPropertyType,
            &actual, &format, &count, &after, &data) != Success)
        goto done;
    if (data) XFree(data);
    if (actual == window->clipboard_incr && format == 32) {
        window->xdnd_property_changed = 0;
        XDeleteProperty(window->display, window->xid,
                        window->xdnd_property);
        XFlush(window->display);
        for (;;) {
            result = nx_x11_wait_clipboard(window,
                &window->xdnd_property_changed, deadline);
            if (result != NX_OK) break;
            window->xdnd_property_changed = 0;
            data = NULL;
            if (XGetWindowProperty(window->display, window->xid,
                    window->xdnd_property, 0, 8L * 1024L * 1024L,
                    True, AnyPropertyType, &actual, &format, &count,
                    &after, &data) != Success) {
                result = NX_ERROR_PLATFORM;
            } else if (actual != window->xdnd_uri_list || format != 8) {
                result = NX_ERROR_STATE;
            } else if (after || count > 32u * 1024u * 1024u - used) {
                result = NX_ERROR_LIMIT;
            } else if (count == 0) {
                result = nx_platform_uri_list_append(&window->drop_queue,
                    collected, used, window->xdnd_x, window->xdnd_y, host);
            } else {
                size_t needed = used + (size_t)count;
                if (needed > allocated) {
                    size_t grown = allocated ? allocated * 2u : 4096u;
                    if (grown < needed) grown = needed;
                    if (grown > 32u * 1024u * 1024u)
                        grown = 32u * 1024u * 1024u;
                    char *replacement = window->allocator.allocate(
                        window->allocator.user, grown);
                    if (!replacement) result = NX_ERROR_MEMORY;
                    else {
                        if (used) memcpy(replacement, collected, used);
                        if (collected) window->allocator.deallocate(
                            window->allocator.user, collected, allocated);
                        collected = replacement;
                        allocated = grown;
                    }
                }
                if (result == NX_OK) {
                    memcpy(collected + used, data, (size_t)count);
                    used = needed;
                }
            }
            if (data) XFree(data);
            if (result != NX_OK || count == 0) break;
        }
    } else if (actual == window->xdnd_uri_list && format == 8 &&
               after <= 32u * 1024u * 1024u) {
        data = NULL;
        unsigned long remainder = 0;
        if (XGetWindowProperty(window->display, window->xid,
                window->xdnd_property, 0, (long)((after + 3u) / 4u),
                True, window->xdnd_uri_list, &actual, &format, &count,
                &remainder, &data) == Success &&
            actual == window->xdnd_uri_list && format == 8 &&
            remainder == 0 && count == after)
            result = nx_platform_uri_list_append(&window->drop_queue,
                (const char *)data, (size_t)count,
                window->xdnd_x, window->xdnd_y, host);
        else result = NX_ERROR_PLATFORM;
        if (data) XFree(data);
    } else result = after > 32u * 1024u * 1024u ?
        NX_ERROR_LIMIT : NX_ERROR_STATE;
done:
    window->xdnd_waiting = 0;
    if (result != NX_OK)
        XDeleteProperty(window->display, window->xid,
                        window->xdnd_property);
    if (collected) window->allocator.deallocate(window->allocator.user,
                                                 collected, allocated);
    nx_x11_dnd_send(window, window->xdnd_finished,
                      result == NX_OK &&
                      window->drop_queue.count > before_count);
    nx_x11_dnd_reset(window);
    return result;
}

static nx_result nx_x11_read_incremental(nx_x11_window *window,
                                          char *bytes, size_t capacity,
                                          size_t *out_length,
                                          uint64_t deadline_ns) {
    char *collected = NULL;
    size_t used = 0, allocated = 0;
    nx_result result = NX_ERROR_PLATFORM;
    window->clipboard_property_changed = 0;
    XDeleteProperty(window->display, window->xid,
                    window->clipboard_property);
    XFlush(window->display);
    for (;;) {
        result = nx_x11_wait_clipboard(window,
            &window->clipboard_property_changed, deadline_ns);
        if (result != NX_OK) break;
        window->clipboard_property_changed = 0;
        Atom actual = None;
        int format = 0;
        unsigned long count = 0, after = 0;
        unsigned char *chunk = NULL;
        if (XGetWindowProperty(window->display, window->xid,
                window->clipboard_property, 0, 8L * 1024L * 1024L, True,
                AnyPropertyType, &actual, &format, &count,
                &after, &chunk) != Success) {
            result = NX_ERROR_PLATFORM;
        } else if (actual != window->clipboard_utf8 || format != 8) {
            result = NX_ERROR_STATE;
        } else if (after || count > 32u * 1024u * 1024u - used) {
            result = NX_ERROR_LIMIT;
        } else if (count == 0) {
            result = nx_x11_clipboard_copy((const unsigned char *)collected,
                                            used, bytes, capacity, out_length);
        } else {
            size_t needed = used + (size_t)count;
            if (needed > allocated) {
                size_t grown = allocated ? allocated * 2u : 4096u;
                if (grown < needed) grown = needed;
                if (grown > 32u * 1024u * 1024u)
                    grown = 32u * 1024u * 1024u;
                char *replacement = window->allocator.allocate(
                    window->allocator.user, grown);
                if (!replacement) result = NX_ERROR_MEMORY;
                else {
                    if (used) memcpy(replacement, collected, used);
                    if (collected) window->allocator.deallocate(
                        window->allocator.user, collected, allocated);
                    collected = replacement;
                    allocated = grown;
                }
            }
            if (result == NX_OK) {
                memcpy(collected + used, chunk, (size_t)count);
                used = needed;
            }
        }
        if (chunk) XFree(chunk);
        if (result != NX_OK || count == 0) break;
    }
    if (collected) window->allocator.deallocate(window->allocator.user,
                                                 collected, allocated);
    if (result != NX_OK)
        XDeleteProperty(window->display, window->xid,
                        window->clipboard_property);
    return result;
}

nx_result nx_x11_window_clipboard_get_text(nx_x11_window *window,
                                        char *bytes, size_t capacity,
                                        size_t *out_length) {
    if (!out_length || (!bytes && capacity)) return NX_ERROR_ARGUMENT;
    *out_length = 0;
    if (!window) return NX_ERROR_ARGUMENT;
    if (!nx_x11_on_owner_thread(window) || !window->alive)
        return NX_ERROR_STATE;
    Window owner = XGetSelectionOwner(window->display, window->clipboard);
    if (owner == None) return NX_ERROR_STATE;
    if (owner == window->xid && window->clipboard_text)
        return nx_x11_clipboard_copy(
            (const unsigned char *)window->clipboard_text,
            window->clipboard_length, bytes, capacity, out_length);
    XDeleteProperty(window->display, window->xid,
                    window->clipboard_property);
    window->clipboard_waiting = 1;
    window->clipboard_done = 0;
    window->clipboard_received = None;
    XConvertSelection(window->display, window->clipboard,
                      window->clipboard_utf8, window->clipboard_property,
                      window->xid, CurrentTime);
    XFlush(window->display);
    uint64_t started = nx_x11_now_ns();
    if (!started) { window->clipboard_waiting = 0; return NX_ERROR_PLATFORM; }
    uint64_t deadline = started + 5000000000ull;
    nx_result result = nx_x11_wait_clipboard(window,
                            &window->clipboard_done, deadline);
    if (result != NX_OK) {
        window->clipboard_waiting = 0;
        return result;
    }
    if (window->clipboard_received != window->clipboard_property)
        { window->clipboard_waiting = 0; return NX_ERROR_STATE; }
    Atom actual = None;
    int format = 0;
    unsigned long count = 0, after = 0;
    unsigned char *data = NULL;
    if (XGetWindowProperty(window->display, window->xid,
            window->clipboard_property, 0, 0, False, AnyPropertyType,
            &actual, &format, &count, &after, &data) != Success)
        { window->clipboard_waiting = 0; return NX_ERROR_PLATFORM; }
    if (data) XFree(data);
    if (actual == window->clipboard_incr && format == 32) {
        result = nx_x11_read_incremental(window, bytes, capacity,
                                          out_length, deadline);
        window->clipboard_waiting = 0;
        return result;
    }
    window->clipboard_waiting = 0;
    if (actual != window->clipboard_utf8 || format != 8) {
        XDeleteProperty(window->display, window->xid,
                        window->clipboard_property);
        return actual == window->clipboard_incr ?
            NX_ERROR_LIMIT : NX_ERROR_STATE;
    }
    if (after > 32u * 1024u * 1024u) {
        XDeleteProperty(window->display, window->xid,
                        window->clipboard_property);
        return NX_ERROR_LIMIT;
    }
    data = NULL;
    count = 0;
    unsigned long remainder = 0;
    nx_result read_result = NX_ERROR_PLATFORM;
    if (XGetWindowProperty(window->display, window->xid,
            window->clipboard_property, 0, (long)((after + 3u) / 4u), True,
            window->clipboard_utf8, &actual, &format, &count,
            &remainder, &data) == Success &&
        actual == window->clipboard_utf8 && format == 8 &&
        remainder == 0 && count == after)
        read_result = nx_x11_clipboard_copy(data, (size_t)count,
                                             bytes, capacity, out_length);
    if (data) XFree(data);
    return read_result;
}

void nx_x11_window_destroy(nx_x11_window *window) {
    if (!window || !nx_x11_on_owner_thread(window)) return;
    nx_x11_unlink_thread_window(window);
    nx_x11_free_clipboard(window);
    while (window->incr_transfers)
        nx_x11_remove_incr_transfer(window, &window->incr_transfers);
    if (window->display) {
        nx_x11_close_input_method(window);
        if (window->cursor != None) XFreeCursor(window->display, window->cursor);
        if (window->alive) XDestroyWindow(window->display, window->xid);
        XCloseDisplay(window->display);
    }
    if (window->ime_preedit)
        window->allocator.deallocate(window->allocator.user,
            window->ime_preedit, window->ime_preedit_length + 1u);
    nx_platform_event_queue_destroy(&window->event_queue);
    nx_platform_drop_queue_destroy(&window->drop_queue);
    nx_allocator allocator = window->allocator;
    allocator.deallocate(allocator.user, window, sizeof(*window));
}

Display *nx_x11_window_x11_display(nx_x11_window *window) {
    return window ? window->display : NULL;
}

Window nx_x11_window_x11_id(nx_x11_window *window) {
    return window ? window->xid : None;
}

#ifdef NEXAUI_X11_TEST_HOOKS
nx_result nx_x11_window_test_ime_edit(nx_x11_window *window,
    const XIMText *text, int first, int removed, int caret) {
    if (!window) return NX_ERROR_ARGUMENT;
    if (!nx_x11_on_owner_thread(window) || !window->alive)
        return NX_ERROR_STATE;
    char *insert = NULL;
    size_t length = 0;
    nx_result result = nx_x11_preedit_convert(window, text, &insert, &length);
    if (result == NX_OK)
        result = nx_x11_preedit_apply(window, first, removed, insert,
            length, text ? text->length : 0u, caret);
    if (insert)
        window->allocator.deallocate(window->allocator.user, insert,
            (size_t)text->length * 4u + 1u);
    return result;
}

nx_result nx_x11_window_test_ime_clear(nx_x11_window *window) {
    if (!window) return NX_ERROR_ARGUMENT;
    if (!nx_x11_on_owner_thread(window) || !window->alive)
        return NX_ERROR_STATE;
    nx_x11_preedit_clear(window);
    return NX_OK;
}
#endif
