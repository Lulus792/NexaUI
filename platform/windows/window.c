#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#define UNICODE
#include <windows.h>
#include <windowsx.h>
#include <shellapi.h>
#include <imm.h>

#include "nexaui/nexaui.h"
#include "window_internal.h"
#include "accessibility.h"
#include "event_queue.h"
#include "drop_queue.h"

#include <limits.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

struct nx_window {
    nx_allocator allocator;
    HWND hwnd;
    nx_uia_bridge *accessibility;
    DWORD owner_thread;
    int should_close;
    float scale;
    HCURSOR cursor;
    nx_platform_event_queue event_queue;
    nx_platform_drop_queue drop_queue;
    uint32_t pointer_buttons;
    uint16_t pending_high_surrogate;
    char *ime_preedit;
    size_t ime_preedit_length;
    int32_t ime_preedit_cursor;
    int ime_self_draw;
    int ime_context_active;
    LPARAM ime_context_options;
};

static nx_result nx_window_collect_file_drop(nx_window *window, HDROP drop) {
    UINT count = DragQueryFileW(drop, UINT_MAX, NULL, 0);
    if (!count) return NX_OK;
    if ((size_t)count > SIZE_MAX / sizeof(nx_platform_drop_entry))
        return NX_ERROR_LIMIT;
    nx_platform_drop_entry *batch = (nx_platform_drop_entry *)window->allocator.allocate(
        window->allocator.user, (size_t)count * sizeof(*batch));
    if (!batch) return NX_ERROR_MEMORY;
    memset(batch, 0, (size_t)count * sizeof(*batch));
    POINT point = {0};
    DragQueryPoint(drop, &point);
    nx_result result = NX_OK;
    for (UINT i = 0; i < count; ++i) {
        UINT wide_length = DragQueryFileW(drop, i, NULL, 0);
        if (!wide_length || wide_length >= INT_MAX ||
            (size_t)wide_length + 1u > SIZE_MAX / sizeof(WCHAR)) {
            result = NX_ERROR_LIMIT;
            break;
        }
        size_t wide_bytes = ((size_t)wide_length + 1u) * sizeof(WCHAR);
        WCHAR *wide = (WCHAR *)window->allocator.allocate(window->allocator.user,
                                                           wide_bytes);
        if (!wide) { result = NX_ERROR_MEMORY; break; }
        UINT copied = DragQueryFileW(drop, i, wide, wide_length + 1u);
        if (copied != wide_length || wide[copied] != 0) {
            result = NX_ERROR_PLATFORM;
        } else {
            int byte_length = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS,
                wide, (int)wide_length, NULL, 0, NULL, NULL);
            if (!byte_length) {
                result = NX_ERROR_PLATFORM;
            } else {
                batch[i].path = (char *)window->allocator.allocate(
                    window->allocator.user, (size_t)byte_length + 1u);
                if (!batch[i].path) {
                    result = NX_ERROR_MEMORY;
                } else {
                    batch[i].length = (size_t)byte_length;
                    if (WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS,
                    wide, (int)wide_length, batch[i].path, byte_length,
                    NULL, NULL) != byte_length) {
                        result = NX_ERROR_PLATFORM;
                    } else {
                        batch[i].path[byte_length] = '\0';
                        batch[i].info = (nx_file_drop_info){
                            (float)point.x / window->scale,
                            (float)point.y / window->scale, i, count};
                    }
                }
            }
        }
        window->allocator.deallocate(window->allocator.user, wide, wide_bytes);
        if (result != NX_OK) break;
    }
    if (result == NX_OK)
        result = nx_platform_drop_queue_append_batch(&window->drop_queue,
                                                      batch, count);
    if (result != NX_OK) {
        for (size_t i = 0; i < count; ++i)
            if (batch[i].path)
                window->allocator.deallocate(window->allocator.user,
                    batch[i].path, batch[i].length + 1u);
    }
    window->allocator.deallocate(window->allocator.user, batch,
                                  (size_t)count * sizeof(*batch));
    return result;
}

static void *nx_window_default_allocate(void *user, size_t size) {
    (void)user;
    return malloc(size);
}

static void nx_window_default_deallocate(void *user, void *pointer, size_t size) {
    (void)user;
    (void)size;
    free(pointer);
}

static void nx_window_push_event(nx_window *window, nx_event event) {
    nx_platform_event_queue_push(&window->event_queue, event);
}

static void nx_window_clear_preedit(nx_window *window) {
    if (!window->ime_preedit) return;
    window->allocator.deallocate(window->allocator.user,
        window->ime_preedit, window->ime_preedit_length + 1u);
    window->ime_preedit = NULL;
    window->ime_preedit_length = 0;
    window->ime_preedit_cursor = 0;
    nx_event event = {0};
    event.type = NX_EVENT_IME_PREEDIT_CHANGED;
    nx_window_push_event(window, event);
}

static nx_result nx_window_set_preedit_utf16(nx_window *window,
    const WCHAR *wide, size_t units, int32_t cursor_units) {
    if ((!wide && units) || units > 524288u ||
        cursor_units < -1 || (cursor_units >= 0 &&
        (size_t)cursor_units > units)) return NX_ERROR_ARGUMENT;
    if (!units) {
        nx_window_clear_preedit(window);
        return NX_OK;
    }
    int length = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS,
        wide, (int)units, NULL, 0, NULL, NULL);
    if (length <= 0) return NX_ERROR_ARGUMENT;
    if (length > 1048576) return NX_ERROR_LIMIT;
    int32_t cursor = -1;
    if (cursor_units >= 0) {
        cursor = 0;
        if (cursor_units) {
            int prefix = WideCharToMultiByte(CP_UTF8,
                WC_ERR_INVALID_CHARS, wide, cursor_units,
                NULL, 0, NULL, NULL);
            if (prefix <= 0) return NX_ERROR_ARGUMENT;
            cursor = prefix;
        }
    }
    char *copy = window->allocator.allocate(window->allocator.user,
        (size_t)length + 1u);
    if (!copy) return NX_ERROR_MEMORY;
    if (WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS,
            wide, (int)units, copy, length, NULL, NULL) != length) {
        window->allocator.deallocate(window->allocator.user, copy,
            (size_t)length + 1u);
        return NX_ERROR_PLATFORM;
    }
    copy[length] = '\0';
    if (window->ime_preedit_length == (size_t)length &&
        window->ime_preedit_cursor == cursor && window->ime_preedit &&
        memcmp(window->ime_preedit, copy, (size_t)length) == 0) {
        window->allocator.deallocate(window->allocator.user, copy,
            (size_t)length + 1u);
        return NX_OK;
    }
    if (window->ime_preedit)
        window->allocator.deallocate(window->allocator.user,
            window->ime_preedit, window->ime_preedit_length + 1u);
    window->ime_preedit = copy;
    window->ime_preedit_length = (size_t)length;
    window->ime_preedit_cursor = cursor;
    nx_event event = {0};
    event.type = NX_EVENT_IME_PREEDIT_CHANGED;
    nx_window_push_event(window, event);
    return NX_OK;
}

static nx_result nx_window_read_ime_preedit(nx_window *window) {
    HIMC context = ImmGetContext(window->hwnd);
    if (!context) {
        nx_window_clear_preedit(window);
        return NX_ERROR_UNSUPPORTED;
    }
    LONG needed = ImmGetCompositionStringW(context, GCS_COMPSTR, NULL, 0);
    if (needed == IMM_ERROR_NODATA) {
        ImmReleaseContext(window->hwnd, context);
        nx_window_clear_preedit(window);
        return NX_OK;
    }
    if (needed < 0 || (needed & 1) || needed > 1048576) {
        ImmReleaseContext(window->hwnd, context);
        return needed > 1048576 ? NX_ERROR_LIMIT : NX_ERROR_PLATFORM;
    }
    if (!needed) {
        ImmReleaseContext(window->hwnd, context);
        nx_window_clear_preedit(window);
        return NX_OK;
    }
    WCHAR *wide = window->allocator.allocate(window->allocator.user,
        (size_t)needed + sizeof(WCHAR));
    if (!wide) {
        ImmReleaseContext(window->hwnd, context);
        return NX_ERROR_MEMORY;
    }
    LONG copied = ImmGetCompositionStringW(context, GCS_COMPSTR,
        wide, (DWORD)needed);
    LONG position = ImmGetCompositionStringW(context, GCS_CURSORPOS,
        NULL, 0);
    ImmReleaseContext(window->hwnd, context);
    nx_result result = copied == needed ?
        nx_window_set_preedit_utf16(window, wide,
            (size_t)needed / sizeof(WCHAR),
            position >= 0 && position <= needed / (LONG)sizeof(WCHAR) ?
                (int32_t)position : -1) : NX_ERROR_PLATFORM;
    window->allocator.deallocate(window->allocator.user, wide,
        (size_t)needed + sizeof(WCHAR));
    return result;
}

static HCURSOR nx_window_load_cursor(nx_cursor_shape shape) {
    LPCWSTR resource = NULL;
    switch (shape) {
    case NX_CURSOR_DEFAULT: resource = IDC_ARROW; break;
    case NX_CURSOR_TEXT: resource = IDC_IBEAM; break;
    case NX_CURSOR_LINK: resource = IDC_HAND; break;
    case NX_CURSOR_CROSSHAIR: resource = IDC_CROSS; break;
    case NX_CURSOR_RESIZE_HORIZONTAL: resource = IDC_SIZEWE; break;
    case NX_CURSOR_RESIZE_VERTICAL: resource = IDC_SIZENS; break;
    case NX_CURSOR_RESIZE_DIAGONAL_NW_SE: resource = IDC_SIZENWSE; break;
    case NX_CURSOR_RESIZE_DIAGONAL_NE_SW: resource = IDC_SIZENESW; break;
    case NX_CURSOR_MOVE: resource = IDC_SIZEALL; break;
    case NX_CURSOR_NOT_ALLOWED: resource = IDC_NO; break;
    case NX_CURSOR_BUSY: resource = IDC_WAIT; break;
    default: return NULL;
    }
    return (HCURSOR)LoadImageW(NULL, resource, IMAGE_CURSOR, 0, 0,
                               LR_DEFAULTSIZE | LR_SHARED);
}

static uint32_t nx_window_modifiers(void) {
    uint32_t modifiers = 0;
    if (GetKeyState(VK_SHIFT) & 0x8000) modifiers |= NX_MOD_SHIFT;
    if (GetKeyState(VK_CONTROL) & 0x8000) modifiers |= NX_MOD_CONTROL;
    if (GetKeyState(VK_MENU) & 0x8000) modifiers |= NX_MOD_ALT;
    if ((GetKeyState(VK_LWIN) | GetKeyState(VK_RWIN)) & 0x8000)
        modifiers |= NX_MOD_SUPER;
    return modifiers;
}

static nx_key nx_window_map_key(WPARAM key) {
    if (key >= 'A' && key <= 'Z') return (nx_key)(NX_KEY_A + key - 'A');
    if (key >= '0' && key <= '9') return (nx_key)(NX_KEY_0 + key - '0');
    if (key >= VK_F1 && key <= VK_F12) return (nx_key)(NX_KEY_F1 + key - VK_F1);
    switch (key) {
    case VK_TAB: return NX_KEY_TAB;
    case VK_RETURN: return NX_KEY_ENTER;
    case VK_ESCAPE: return NX_KEY_ESCAPE;
    case VK_SPACE: return NX_KEY_SPACE;
    case VK_BACK: return NX_KEY_BACKSPACE;
    case VK_DELETE: return NX_KEY_DELETE;
    case VK_INSERT: return NX_KEY_INSERT;
    case VK_LEFT: return NX_KEY_LEFT;
    case VK_RIGHT: return NX_KEY_RIGHT;
    case VK_UP: return NX_KEY_UP;
    case VK_DOWN: return NX_KEY_DOWN;
    case VK_HOME: return NX_KEY_HOME;
    case VK_END: return NX_KEY_END;
    case VK_PRIOR: return NX_KEY_PAGE_UP;
    case VK_NEXT: return NX_KEY_PAGE_DOWN;
    case VK_SHIFT: case VK_LSHIFT: case VK_RSHIFT: return NX_KEY_SHIFT;
    case VK_CONTROL: case VK_LCONTROL: case VK_RCONTROL: return NX_KEY_CONTROL;
    case VK_MENU: case VK_LMENU: case VK_RMENU: return NX_KEY_ALT;
    case VK_LWIN: case VK_RWIN: return NX_KEY_SUPER;
    default: return NX_KEY_UNKNOWN;
    }
}

static void nx_window_emit_pointer(nx_window *window, nx_event_type type,
                                   LPARAM lparam, uint32_t button) {
    nx_event event = {0};
    event.type = type;
    event.x = (float)GET_X_LPARAM(lparam) / window->scale;
    event.y = (float)GET_Y_LPARAM(lparam) / window->scale;
    event.button = button;
    event.modifiers = nx_window_modifiers();
    nx_window_push_event(window, event);
}

static void nx_window_emit_touch(nx_window *window, nx_event_type type,
                                  UINT32 contact_id, LPARAM lparam) {
    nx_event event = {0};
    event.type = type;
    event.contact_id = contact_id;
    event.modifiers = nx_window_modifiers();
    if (type != NX_EVENT_TOUCH_CANCEL) {
        POINTER_INFO info;
        POINT point = {GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
        BOOL has_info = GetPointerInfo(contact_id, &info);
        if (has_info) point = info.ptPixelLocation;
        if (!ScreenToClient(window->hwnd, &point)) {
            event.type = NX_EVENT_TOUCH_CANCEL;
        } else {
            event.x = (float)point.x / window->scale;
            event.y = (float)point.y / window->scale;
            if (has_info && (info.pointerFlags & POINTER_FLAG_CANCELED))
                event.type = NX_EVENT_TOUCH_CANCEL;
        }
    }
    nx_window_push_event(window, event);
}

static void nx_window_emit_resize(nx_window *window) {
    RECT rect;
    if (!GetClientRect(window->hwnd, &rect)) return;
    nx_event event = {0};
    event.type = NX_EVENT_RESIZE;
    event.width = (float)(rect.right - rect.left) / window->scale;
    event.height = (float)(rect.bottom - rect.top) / window->scale;
    event.scale = window->scale;
    nx_window_push_event(window, event);
}

static void nx_window_emit_text(nx_window *window, uint32_t scalar) {
    if (scalar < 0x20u || scalar == 0x7fu || scalar > 0x10ffffu ||
        (scalar >= 0xd800u && scalar <= 0xdfffu)) return;
    nx_event event = {0};
    event.type = NX_EVENT_TEXT;
    event.scalar = scalar;
    nx_window_push_event(window, event);
}

static void nx_window_emit_utf16(nx_window *window, uint16_t unit) {
    if (window->pending_high_surrogate) {
        if (unit >= 0xdc00u && unit <= 0xdfffu) {
            uint32_t high = window->pending_high_surrogate - 0xd800u;
            nx_window_emit_text(window, 0x10000u + (high << 10u) + unit - 0xdc00u);
            window->pending_high_surrogate = 0;
            return;
        }
        nx_window_emit_text(window, 0xfffdu);
        window->pending_high_surrogate = 0;
    }
    if (unit >= 0xd800u && unit <= 0xdbffu) {
        window->pending_high_surrogate = unit;
    } else if (unit >= 0xdc00u && unit <= 0xdfffu) {
        nx_window_emit_text(window, 0xfffdu);
    } else {
        nx_window_emit_text(window, unit);
    }
}

static LRESULT CALLBACK nx_window_proc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
    nx_window *window = (nx_window *)GetWindowLongPtrW(hwnd, GWLP_USERDATA);
    if (message == WM_NCCREATE) {
        const CREATESTRUCTW *create = (const CREATESTRUCTW *)lparam;
        window = (nx_window *)create->lpCreateParams;
        window->hwnd = hwnd;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)window);
    }
    if (window) {
        if (message == WM_IME_SETCONTEXT) {
            window->ime_context_active = !!wparam;
            window->ime_context_options = lparam;
            return DefWindowProcW(hwnd, message, wparam,
                window->ime_self_draw && wparam ?
                    lparam & ~ISC_SHOWUICOMPOSITIONWINDOW : lparam);
        }
        if (message == WM_SETCURSOR && LOWORD(lparam) == HTCLIENT) {
            SetCursor(window->cursor);
            return TRUE;
        }
        if (message == WM_DROPFILES) {
            HDROP drop = (HDROP)wparam;
            if (window->event_queue.error == NX_OK)
                window->event_queue.error = nx_window_collect_file_drop(window, drop);
            DragFinish(drop);
            return 0;
        }
        if (message == WM_SIZE) {
            nx_uia_geometry(window->accessibility);
            nx_window_emit_resize(window);
            return 0;
        }
        if (message == WM_CLOSE) {
            window->should_close = 1;
            return 0;
        }
        if (message == WM_DPICHANGED) {
            window->scale = (float)HIWORD(wparam) / 96.0f;
            const RECT *suggested = (const RECT *)lparam;
            SetWindowPos(hwnd, NULL, suggested->left, suggested->top,
                         suggested->right - suggested->left,
                         suggested->bottom - suggested->top,
                         SWP_NOZORDER | SWP_NOACTIVATE);
            nx_uia_geometry(window->accessibility);
            nx_window_emit_resize(window);
            return 0;
        }
        if (message == WM_SETFOCUS || message == WM_KILLFOCUS) {
            nx_uia_geometry(window->accessibility);
            nx_event event = {0};
            event.type = NX_EVENT_FOCUS;
            event.focused = message == WM_SETFOCUS;
            nx_window_push_event(window, event);
            if (message == WM_KILLFOCUS) {
                window->pending_high_surrogate = 0;
                nx_window_clear_preedit(window);
            }
            return 0;
        }
        if (message == WM_IME_STARTCOMPOSITION && window->ime_self_draw) {
            nx_window_clear_preedit(window);
            return 0;
        }
        if (message == WM_IME_ENDCOMPOSITION && window->ime_self_draw) {
            nx_window_clear_preedit(window);
            return 0;
        }
        if (message == WM_IME_COMPOSITION && window->ime_self_draw) {
            if (lparam & GCS_RESULTSTR) nx_window_clear_preedit(window);
            if (lparam & (GCS_COMPSTR | GCS_CURSORPOS)) {
                nx_result result = nx_window_read_ime_preedit(window);
                if (result != NX_OK && result != NX_ERROR_UNSUPPORTED &&
                    window->event_queue.error == NX_OK)
                    window->event_queue.error = result;
            } else if (!(lparam & GCS_RESULTSTR)) {
                nx_window_clear_preedit(window);
            }
            if (lparam & GCS_RESULTSTR)
                return DefWindowProcW(hwnd, message, wparam, lparam);
            return 0;
        }
        if (message == WM_POINTERDOWN || message == WM_POINTERUPDATE ||
            message == WM_POINTERUP) {
            UINT32 contact_id = GET_POINTERID_WPARAM(wparam);
            POINTER_INPUT_TYPE pointer_type = PT_POINTER;
            if (GetPointerType(contact_id, &pointer_type) &&
                pointer_type == PT_TOUCH) {
                if (IS_POINTER_CANCELED_WPARAM(wparam)) {
                    nx_window_emit_touch(window, NX_EVENT_TOUCH_CANCEL,
                                          contact_id, 0);
                } else if (message != WM_POINTERUPDATE ||
                    IS_POINTER_INCONTACT_WPARAM(wparam)) {
                    nx_event_type type = message == WM_POINTERDOWN ?
                        NX_EVENT_TOUCH_DOWN : message == WM_POINTERUP ?
                        NX_EVENT_TOUCH_UP : NX_EVENT_TOUCH_MOVE;
                    nx_window_emit_touch(window, type, contact_id, lparam);
                }
                return 0;
            }
        }
        if (message == WM_POINTERCAPTURECHANGED) {
            UINT32 contact_id = GET_POINTERID_WPARAM(wparam);
            POINTER_INPUT_TYPE pointer_type = PT_POINTER;
            if (!GetPointerType(contact_id, &pointer_type) ||
                pointer_type == PT_TOUCH) {
                nx_window_emit_touch(window, NX_EVENT_TOUCH_CANCEL,
                                      contact_id, 0);
                return 0;
            }
        }
        if (message == WM_MOUSEMOVE) {
            nx_window_emit_pointer(window, NX_EVENT_POINTER_MOVE, lparam, 0);
            return 0;
        }
        if (message == WM_LBUTTONDOWN || message == WM_RBUTTONDOWN ||
            message == WM_MBUTTONDOWN || message == WM_XBUTTONDOWN ||
            message == WM_LBUTTONUP || message == WM_RBUTTONUP ||
            message == WM_MBUTTONUP || message == WM_XBUTTONUP) {
            uint32_t button = message == WM_LBUTTONDOWN || message == WM_LBUTTONUP
                ? NX_BUTTON_LEFT : message == WM_RBUTTONDOWN || message == WM_RBUTTONUP
                ? NX_BUTTON_RIGHT : message == WM_MBUTTONDOWN || message == WM_MBUTTONUP
                ? NX_BUTTON_MIDDLE : HIWORD(wparam) == XBUTTON1 ? NX_BUTTON_X1 : NX_BUTTON_X2;
            int down = message == WM_LBUTTONDOWN || message == WM_RBUTTONDOWN ||
                       message == WM_MBUTTONDOWN || message == WM_XBUTTONDOWN;
            if (down) {
                window->pointer_buttons |= button;
                SetCapture(hwnd);
            } else {
                window->pointer_buttons &= ~button;
            }
            nx_window_emit_pointer(window, down ? NX_EVENT_POINTER_DOWN : NX_EVENT_POINTER_UP,
                                   lparam, button);
            if (!down && !window->pointer_buttons && GetCapture() == hwnd)
                ReleaseCapture();
            return message == WM_XBUTTONDOWN || message == WM_XBUTTONUP ? TRUE : 0;
        }
        if (message == WM_CAPTURECHANGED && window->pointer_buttons) {
            window->pointer_buttons = 0;
            nx_event event = {0};
            event.type = NX_EVENT_POINTER_CANCEL;
            nx_window_push_event(window, event);
            return 0;
        }
        if (message == WM_MOUSEWHEEL || message == WM_MOUSEHWHEEL) {
            POINT point = {GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
            ScreenToClient(hwnd, &point);
            nx_event event = {0};
            event.type = NX_EVENT_WHEEL;
            event.x = (float)point.x / window->scale;
            event.y = (float)point.y / window->scale;
            event.modifiers = nx_window_modifiers();
            float delta = (float)GET_WHEEL_DELTA_WPARAM(wparam) / WHEEL_DELTA;
            if (message == WM_MOUSEWHEEL) event.delta_y = delta;
            else event.delta_x = delta;
            nx_window_push_event(window, event);
            return 0;
        }
        if (message == WM_KEYDOWN || message == WM_SYSKEYDOWN ||
            message == WM_KEYUP || message == WM_SYSKEYUP) {
            nx_key key = nx_window_map_key(wparam);
            if (key != NX_KEY_UNKNOWN) {
                nx_event event = {0};
                event.type = message == WM_KEYDOWN || message == WM_SYSKEYDOWN
                             ? NX_EVENT_KEY_DOWN : NX_EVENT_KEY_UP;
                event.key = key;
                event.repeat = event.type == NX_EVENT_KEY_DOWN && (lparam & (1L << 30)) != 0;
                event.modifiers = nx_window_modifiers();
                nx_window_push_event(window, event);
            }
            if (message == WM_SYSKEYDOWN || message == WM_SYSKEYUP)
                return DefWindowProcW(hwnd, message, wparam, lparam);
            return 0;
        }
        if (message == WM_CHAR || message == WM_SYSCHAR) {
            nx_window_emit_utf16(window, (uint16_t)wparam);
            return 0;
        }
        if (message == WM_UNICHAR) {
            if (wparam == UNICODE_NOCHAR) return TRUE;
            nx_window_emit_text(window, (uint32_t)wparam);
            return 0;
        }
        if (message == WM_GETOBJECT && window->accessibility && nx_uia_object_id(lparam))
            return nx_uia_get_object(window->accessibility,wparam,lparam);
        if (message == WM_WINDOWPOSCHANGED || message == WM_SHOWWINDOW || message == WM_ACTIVATE || message == WM_ENABLE)
            nx_uia_geometry(window->accessibility);
        if (message == WM_DESTROY && window->accessibility) {
            nx_uia_bridge *bridge=window->accessibility; window->accessibility=NULL;
            nx_uia_destroy(bridge);
        }
        if (message == WM_NCDESTROY) {
            window->hwnd = NULL;
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
        }
    }
    return DefWindowProcW(hwnd, message, wparam, lparam);
}

static nx_result nx_register_class(HINSTANCE instance) {
    WNDCLASSEXW cls = {0};
    cls.cbSize = sizeof(cls);
    cls.style = CS_HREDRAW | CS_VREDRAW;
    cls.lpfnWndProc = nx_window_proc;
    cls.hInstance = instance;
    cls.hCursor = NULL;
    cls.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    cls.lpszClassName = L"NexaUIWindowClass";
    if (RegisterClassExW(&cls)) return NX_OK;
    if (GetLastError() == ERROR_CLASS_ALREADY_EXISTS) {
        WNDCLASSEXW existing = {0};
        existing.cbSize = sizeof(existing);
        if (GetClassInfoExW(instance, cls.lpszClassName, &existing) &&
            existing.lpfnWndProc == nx_window_proc) return NX_OK;
    }
    return NX_ERROR_PLATFORM;
}

nx_result nx_window_create(const nx_window_desc *desc, nx_window **out_window) {
    if (!out_window) return NX_ERROR_ARGUMENT;
    *out_window = NULL;
    if (!desc || !isfinite(desc->width) || !isfinite(desc->height) ||
        desc->width <= 0.0f || desc->height <= 0.0f ||
        (desc->flags & ~NX_WINDOW_HIDDEN)) return NX_ERROR_ARGUMENT;
    nx_allocator a = desc->allocator;
    if ((a.allocate == NULL) != (a.deallocate == NULL)) return NX_ERROR_ARGUMENT;
    if (!a.allocate) {
        a.user = NULL;
        a.allocate = nx_window_default_allocate;
        a.deallocate = nx_window_default_deallocate;
    }
    const char *title = desc->title_utf8 ? desc->title_utf8 : "NexaUI";
    size_t title_length = strlen(title);
    if (title_length > INT_MAX - 1 ||
        nx_utf8_validate(title, title_length, NULL) != NX_OK) return NX_ERROR_ARGUMENT;
    int wide_length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, title,
                                          (int)title_length + 1, NULL, 0);
    if (!wide_length || (size_t)wide_length > SIZE_MAX / sizeof(wchar_t))
        return NX_ERROR_PLATFORM;
    wchar_t *wide_title = (wchar_t *)a.allocate(a.user, (size_t)wide_length * sizeof(wchar_t));
    if (!wide_title) return NX_ERROR_MEMORY;
    if (!MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, title,
                             (int)title_length + 1, wide_title, wide_length)) {
        a.deallocate(a.user, wide_title, (size_t)wide_length * sizeof(wchar_t));
        return NX_ERROR_PLATFORM;
    }
    nx_window *window = (nx_window *)a.allocate(a.user, sizeof(nx_window));
    if (!window) {
        a.deallocate(a.user, wide_title, (size_t)wide_length * sizeof(wchar_t));
        return NX_ERROR_MEMORY;
    }
    memset(window, 0, sizeof(*window));
    window->allocator = a;
    nx_platform_drop_queue_init(&window->drop_queue, a);
    window->owner_thread = GetCurrentThreadId();
    if (nx_platform_event_queue_init(&window->event_queue, a, 64u) != NX_OK) {
        a.deallocate(a.user, window, sizeof(nx_window));
        a.deallocate(a.user, wide_title, (size_t)wide_length * sizeof(wchar_t));
        return NX_ERROR_MEMORY;
    }
    window->cursor = nx_window_load_cursor(NX_CURSOR_DEFAULT);
    if (!window->cursor) {
        nx_platform_event_queue_destroy(&window->event_queue);
        a.deallocate(a.user, window, sizeof(nx_window));
        a.deallocate(a.user, wide_title,
                     (size_t)wide_length * sizeof(wchar_t));
        return NX_ERROR_PLATFORM;
    }
    UINT dpi = GetDpiForSystem();
    if (!dpi) dpi = 96u;
    window->scale = (float)dpi / 96.0f;
    double pixel_width = ceil((double)desc->width * window->scale);
    double pixel_height = ceil((double)desc->height * window->scale);
    nx_result result = NX_ERROR_PLATFORM;
    if (pixel_width <= INT_MAX && pixel_height <= INT_MAX) {
        RECT bounds = {0, 0, (LONG)pixel_width, (LONG)pixel_height};
        DWORD style = WS_OVERLAPPEDWINDOW;
        if (AdjustWindowRectExForDpi(&bounds, style, FALSE, 0, dpi)) {
            long long outer_width = (long long)bounds.right - bounds.left;
            long long outer_height = (long long)bounds.bottom - bounds.top;
            HINSTANCE instance = GetModuleHandleW(NULL);
            if (outer_width > 0 && outer_width <= INT_MAX &&
                outer_height > 0 && outer_height <= INT_MAX &&
                nx_register_class(instance) == NX_OK) {
                HWND hwnd = CreateWindowExW(0, L"NexaUIWindowClass", wide_title,
                                             style, CW_USEDEFAULT, CW_USEDEFAULT,
                                             (int)outer_width, (int)outer_height, NULL, NULL,
                                             instance, window);
                if (hwnd) {
                    DragAcceptFiles(hwnd, TRUE);
                    UINT actual_dpi = GetDpiForWindow(hwnd);
                    if (actual_dpi) window->scale = (float)actual_dpi / 96.0f;
                    if (!(desc->flags & NX_WINDOW_HIDDEN)) ShowWindow(hwnd, SW_SHOW);
                    result = NX_OK;
                }
            }
        }
    } else {
        result = NX_ERROR_LIMIT;
    }
    a.deallocate(a.user, wide_title, (size_t)wide_length * sizeof(wchar_t));
    if (result != NX_OK) {
        nx_platform_event_queue_destroy(&window->event_queue);
        a.deallocate(a.user, window, sizeof(nx_window));
        return result;
    }
    *out_window = window;
    return NX_OK;
}

nx_result nx_window_poll(nx_window *window, uint32_t timeout_ms) {
    if (!window) return NX_ERROR_ARGUMENT;
    if (GetCurrentThreadId() != window->owner_thread || !window->hwnd)
        return NX_ERROR_STATE;
    DWORD wait = MsgWaitForMultipleObjects(0, NULL, FALSE, timeout_ms, QS_ALLINPUT);
    if (wait == WAIT_FAILED) return NX_ERROR_PLATFORM;
    MSG message;
    while (PeekMessageW(&message, NULL, 0, 0, PM_REMOVE)) {
        if (message.message == WM_QUIT) {
            window->should_close = 1;
            continue;
        }
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    return window->event_queue.error;
}

nx_result nx_window_get_info(nx_window *window, nx_window_info *out_info) {
    if (!window || !out_info) return NX_ERROR_ARGUMENT;
    if (GetCurrentThreadId() != window->owner_thread || !window->hwnd)
        return NX_ERROR_STATE;
    RECT rect;
    if (!GetClientRect(window->hwnd, &rect)) return NX_ERROR_PLATFORM;
    out_info->width = (float)(rect.right - rect.left) / window->scale;
    out_info->height = (float)(rect.bottom - rect.top) / window->scale;
    out_info->scale = window->scale;
    out_info->should_close = window->should_close;
    return NX_OK;
}

nx_result nx_window_set_cursor(nx_window *window, nx_cursor_shape shape) {
    if (!window || shape < NX_CURSOR_DEFAULT || shape > NX_CURSOR_BUSY)
        return NX_ERROR_ARGUMENT;
    if (GetCurrentThreadId() != window->owner_thread || !window->hwnd)
        return NX_ERROR_STATE;
    HCURSOR cursor = nx_window_load_cursor(shape);
    if (!cursor) return NX_ERROR_PLATFORM;
    window->cursor = cursor;
    if (GetCapture() == window->hwnd) {
        SetCursor(cursor);
    } else {
        POINT point;
        RECT client;
        if (GetCursorPos(&point) && WindowFromPoint(point) == window->hwnd &&
            ScreenToClient(window->hwnd, &point) &&
            GetClientRect(window->hwnd, &client) && PtInRect(&client, point))
            SetCursor(cursor);
    }
    return NX_OK;
}

nx_result nx_window_get_reduced_motion(nx_window *window, int *out_enabled) {
    if (!out_enabled) return NX_ERROR_ARGUMENT;
    *out_enabled = 0;
    if (!window) return NX_ERROR_ARGUMENT;
    if (GetCurrentThreadId() != window->owner_thread || !window->hwnd)
        return NX_ERROR_STATE;
    BOOL animations_enabled = TRUE;
    if (!SystemParametersInfoW(SPI_GETCLIENTAREAANIMATION, 0,
                               &animations_enabled, 0))
        return NX_ERROR_PLATFORM;
    *out_enabled = !animations_enabled;
    return NX_OK;
}

nx_result nx_window_set_ime_rect(nx_window *window, nx_rect caret) {
    if (!window || !isfinite(caret.x) || !isfinite(caret.y) ||
        !isfinite(caret.width) || !isfinite(caret.height) ||
        caret.width <= 0.0f || caret.height <= 0.0f)
        return NX_ERROR_ARGUMENT;
    if (GetCurrentThreadId() != window->owner_thread || !window->hwnd)
        return NX_ERROR_STATE;
    double left = (double)caret.x * window->scale;
    double top = (double)caret.y * window->scale;
    double right = ((double)caret.x + caret.width) * window->scale;
    double bottom = ((double)caret.y + caret.height) * window->scale;
    if (left < INT_MIN || top < INT_MIN || left > INT_MAX ||
        top > INT_MAX || right > INT_MAX ||
        bottom > INT_MAX || !isfinite(left) || !isfinite(top) ||
        !isfinite(right) || !isfinite(bottom)) return NX_ERROR_LIMIT;
    HIMC context = ImmGetContext(window->hwnd);
    if (!context) return NX_ERROR_UNSUPPORTED;
    COMPOSITIONFORM composition = {0};
    composition.dwStyle = CFS_POINT;
    composition.ptCurrentPos.x = (LONG)lround(left);
    composition.ptCurrentPos.y = (LONG)lround(top);
    BOOL positioned = ImmSetCompositionWindow(context, &composition);
    if (positioned) {
        CANDIDATEFORM candidate = {0};
        candidate.dwIndex = 0;
        candidate.dwStyle = CFS_EXCLUDE;
        candidate.ptCurrentPos.x = composition.ptCurrentPos.x;
        candidate.ptCurrentPos.y = (LONG)lround(bottom);
        candidate.rcArea.left = composition.ptCurrentPos.x;
        candidate.rcArea.top = composition.ptCurrentPos.y;
        candidate.rcArea.right = (LONG)lround(right);
        candidate.rcArea.bottom = candidate.ptCurrentPos.y;
        ImmSetCandidateWindow(context, &candidate);
    }
    ImmReleaseContext(window->hwnd, context);
    return positioned ? NX_OK : NX_ERROR_PLATFORM;
}

nx_result nx_window_set_ime_enabled(nx_window *window, int enabled) {
    if (!window || (enabled != 0 && enabled != 1)) return NX_ERROR_ARGUMENT;
    if (GetCurrentThreadId() != window->owner_thread || !window->hwnd)
        return NX_ERROR_STATE;
    if (enabled && !window->ime_self_draw) {
        HIMC context = ImmGetContext(window->hwnd);
        if (!context) return NX_ERROR_UNSUPPORTED;
        ImmReleaseContext(window->hwnd, context);
    }
    if (window->ime_self_draw != enabled) {
        window->ime_self_draw = enabled;
        if (window->ime_context_active)
            DefWindowProcW(window->hwnd, WM_IME_SETCONTEXT, TRUE,
                enabled ? window->ime_context_options &
                    ~ISC_SHOWUICOMPOSITIONWINDOW :
                    window->ime_context_options);
    }
    if (!enabled) nx_window_clear_preedit(window);
    return NX_OK;
}

nx_result nx_window_set_ime_purpose(nx_window *window,
                                    nx_ime_purpose purpose) {
    if (!window || (purpose != NX_IME_PURPOSE_NORMAL &&
                    purpose != NX_IME_PURPOSE_PASSWORD))
        return NX_ERROR_ARGUMENT;
    if (GetCurrentThreadId() != window->owner_thread || !window->hwnd)
        return NX_ERROR_STATE;
    return purpose == NX_IME_PURPOSE_NORMAL ? NX_OK : NX_ERROR_UNSUPPORTED;
}

nx_result nx_window_get_ime_preedit(nx_window *window,
    char *bytes, size_t capacity, size_t *out_length,
    int32_t *out_cursor_begin, int32_t *out_cursor_end) {
    if (!out_length || !out_cursor_begin || !out_cursor_end ||
        (!bytes && capacity)) return NX_ERROR_ARGUMENT;
    *out_length = 0;
    *out_cursor_begin = *out_cursor_end = 0;
    if (!window) return NX_ERROR_ARGUMENT;
    if (GetCurrentThreadId() != window->owner_thread || !window->hwnd)
        return NX_ERROR_STATE;
    *out_length = window->ime_preedit_length;
    *out_cursor_begin = *out_cursor_end = window->ime_preedit_cursor;
    if (!bytes) return NX_OK;
    if (capacity <= window->ime_preedit_length) return NX_ERROR_LIMIT;
    if (window->ime_preedit_length)
        memcpy(bytes, window->ime_preedit, window->ime_preedit_length);
    bytes[window->ime_preedit_length] = '\0';
    return NX_OK;
}

nx_result nx_window_set_ime_surrounding_text(nx_window *window,
    uint64_t target, uint64_t revision, const char *bytes, size_t length,
    size_t cursor, size_t anchor) {
    (void)window; (void)target; (void)revision; (void)bytes;
    (void)length; (void)cursor; (void)anchor;
    return NX_ERROR_UNSUPPORTED;
}

nx_result nx_window_next_event(nx_window *window, nx_event *out_event, int *available) {
    if (!out_event || !available) return NX_ERROR_ARGUMENT;
    *out_event = (nx_event){0};
    *available = 0;
    if (!window) return NX_ERROR_ARGUMENT;
    if (GetCurrentThreadId() != window->owner_thread || !window->hwnd)
        return NX_ERROR_STATE;
    return nx_platform_event_queue_next(&window->event_queue,
                                         out_event, available);
}

nx_result nx_window_next_file_drop(nx_window *window,
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
    if (GetCurrentThreadId() != window->owner_thread || !window->hwnd)
        return NX_ERROR_STATE;
    return nx_platform_drop_queue_next(&window->drop_queue,
                                        path_utf8, capacity, out_length,
                                        out_info, available);
}

nx_result nx_window_route_events(nx_window *window, nx_context *context,
                                 size_t *out_count) {
    if (!out_count) return NX_ERROR_ARGUMENT;
    *out_count = 0;
    if (!window || !context) return NX_ERROR_ARGUMENT;
    if (GetCurrentThreadId() != window->owner_thread || !window->hwnd)
        return NX_ERROR_STATE;
    if (!nx_uia_matches_context(window->accessibility,context)) return NX_ERROR_STATE;
    nx_result result=nx_platform_event_queue_route(&window->event_queue,context,out_count);
    if (result==NX_OK) {
        size_t actions=0;
        result=nx_uia_route(window->accessibility,context,&actions);
        *out_count+=actions;
    }
    return result;
}

nx_result nx_window_publish_semantics(nx_window *window,const nx_context *context) {
    if (!window) return NX_ERROR_ARGUMENT;
    if (GetCurrentThreadId()!=window->owner_thread || !window->hwnd) return NX_ERROR_STATE;
    if (!context) {
        nx_uia_bridge *bridge=window->accessibility; window->accessibility=NULL;
        nx_uia_destroy(bridge); return NX_OK;
    }
    if (!window->accessibility) {
        nx_uia_bridge *candidate=NULL;
        nx_result result=nx_uia_create(window->hwnd,window->allocator,&candidate);
        if (result!=NX_OK) return result;
        window->accessibility=candidate;
        result=nx_uia_publish(candidate,context);
        if (result!=NX_OK) { window->accessibility=NULL; nx_uia_destroy(candidate); return result; }
        return NX_OK;
    }
    return nx_uia_publish(window->accessibility,context);
}

nx_result nx_window_clipboard_set_text(nx_window *window,
                                        const char *bytes, size_t length) {
    if (!window || (!bytes && length) ||
        (length && memchr(bytes, 0, length)) ||
        nx_utf8_validate(bytes, length, NULL) != NX_OK)
        return NX_ERROR_ARGUMENT;
    if (length > INT_MAX) return NX_ERROR_LIMIT;
    if (GetCurrentThreadId() != window->owner_thread || !window->hwnd)
        return NX_ERROR_STATE;
    int wide_length = 0;
    if (length) {
        wide_length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
                                          bytes, (int)length, NULL, 0);
        if (!wide_length) return NX_ERROR_PLATFORM;
    }
    size_t extra = 0;
    for (size_t i = 0; i < length; ++i) {
        if ((bytes[i] == '\n' && (i == 0u || bytes[i - 1u] != '\r')) ||
            (bytes[i] == '\r' && (i + 1u == length ||
                                  bytes[i + 1u] != '\n')))
            ++extra;
    }
    if ((size_t)wide_length + extra >= SIZE_MAX / sizeof(WCHAR))
        return NX_ERROR_LIMIT;
    size_t clipboard_units = (size_t)wide_length + extra;
    HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE,
                                  (clipboard_units + 1u) * sizeof(WCHAR));
    if (!memory) return NX_ERROR_MEMORY;
    WCHAR *wide = GlobalLock(memory);
    if (!wide) { GlobalFree(memory); return NX_ERROR_PLATFORM; }
    int converted = length ? MultiByteToWideChar(CP_UTF8,
        MB_ERR_INVALID_CHARS, bytes, (int)length, wide, wide_length) : 0;
    if (length && converted != wide_length) {
        GlobalUnlock(memory);
        GlobalFree(memory);
        return NX_ERROR_PLATFORM;
    }
    size_t read = (size_t)wide_length, write = clipboard_units;
    wide[write] = 0;
    int next_original_lf = 0;
    while (read) {
        WCHAR current = wide[--read];
        int expand = (current == L'\n' &&
                      (read == 0u || wide[read - 1u] != L'\r')) ||
                     (current == L'\r' && !next_original_lf);
        if (expand) {
            wide[--write] = L'\n';
            wide[--write] = L'\r';
        } else wide[--write] = current;
        next_original_lf = current == L'\n';
    }
    GlobalUnlock(memory);
    if (!OpenClipboard(window->hwnd)) {
        GlobalFree(memory);
        return NX_ERROR_PLATFORM;
    }
    nx_result result = NX_ERROR_PLATFORM;
    if (EmptyClipboard() && SetClipboardData(CF_UNICODETEXT, memory)) {
        memory = NULL; /* The OS owns this handle now. */
        result = NX_OK;
    }
    if (!CloseClipboard()) result = NX_ERROR_PLATFORM;
    if (memory) GlobalFree(memory);
    return result;
}

nx_result nx_window_clipboard_get_text(nx_window *window,
                                        char *bytes, size_t capacity,
                                        size_t *out_length) {
    if (!out_length || (!bytes && capacity)) return NX_ERROR_ARGUMENT;
    *out_length = 0;
    if (!window) return NX_ERROR_ARGUMENT;
    if (GetCurrentThreadId() != window->owner_thread || !window->hwnd)
        return NX_ERROR_STATE;
    if (!OpenClipboard(window->hwnd)) return NX_ERROR_PLATFORM;
    nx_result result = NX_ERROR_PLATFORM;
    HGLOBAL memory = NULL;
    WCHAR *wide = NULL;
    char *converted = NULL;
    size_t converted_size = 0;
    if (!IsClipboardFormatAvailable(CF_UNICODETEXT)) {
        result = NX_ERROR_STATE;
        goto cleanup;
    }
    memory = GetClipboardData(CF_UNICODETEXT);
    if (!memory) goto cleanup;
    size_t units = GlobalSize(memory) / sizeof(WCHAR);
    if (!units) goto cleanup;
    wide = GlobalLock(memory);
    if (!wide) goto cleanup;
    size_t wide_length = 0;
    while (wide_length < units && wide[wide_length]) ++wide_length;
    if (wide_length == units || wide_length > INT_MAX) {
        result = NX_ERROR_LIMIT;
        goto cleanup;
    }
    int needed = wide_length ? WideCharToMultiByte(CP_UTF8,
        WC_ERR_INVALID_CHARS, wide, (int)wide_length, NULL, 0,
        NULL, NULL) : 0;
    if (wide_length && !needed) goto cleanup;
    size_t pairs = 0;
    for (size_t i = 0; i + 1u < wide_length; ++i)
        if (wide[i] == L'\r' && wide[i + 1u] == L'\n') {
            ++pairs;
            ++i;
        }
    *out_length = (size_t)needed - pairs;
    if (!bytes && !capacity) {
        result = NX_OK;
        goto cleanup;
    }
    if (capacity <= *out_length) {
        result = NX_ERROR_LIMIT;
        goto cleanup;
    }
    if (pairs) {
        converted_size = (size_t)needed;
        converted = window->allocator.allocate(window->allocator.user,
                                                converted_size);
        if (!converted) { result = NX_ERROR_MEMORY; goto cleanup; }
    }
    char *target = converted ? converted : bytes;
    if (wide_length && WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS,
        wide, (int)wide_length, target, needed, NULL, NULL) != needed)
        goto cleanup;
    if (pairs) {
        size_t written = 0;
        for (int i = 0; i < needed; ++i) {
            if (target[i] == '\r' && i + 1 < needed &&
                target[i + 1] == '\n') continue;
            bytes[written++] = target[i];
        }
    }
    bytes[*out_length] = '\0';
    result = NX_OK;
cleanup:
    if (converted)
        window->allocator.deallocate(window->allocator.user, converted,
                                      converted_size);
    if (wide) GlobalUnlock(memory);
    if (!CloseClipboard()) result = NX_ERROR_PLATFORM;
    return result;
}

void nx_window_destroy(nx_window *window) {
    if (!window) return;
    if (GetCurrentThreadId() != window->owner_thread) return;
    if (window->hwnd) {
        DragAcceptFiles(window->hwnd, FALSE);
        DestroyWindow(window->hwnd);
    }
    if (window->accessibility) { nx_uia_destroy(window->accessibility); window->accessibility=NULL; }
    nx_allocator a = window->allocator;
    if (window->ime_preedit)
        a.deallocate(a.user, window->ime_preedit,
            window->ime_preedit_length + 1u);
    nx_platform_drop_queue_destroy(&window->drop_queue);
    nx_platform_event_queue_destroy(&window->event_queue);
    a.deallocate(a.user, window, sizeof(nx_window));
}

HWND nx_window_native_hwnd(nx_window *window) {
    if (!window || GetCurrentThreadId() != window->owner_thread)
        return NULL;
    return window->hwnd;
}

nx_result nx_window_windows_test_ime_preedit(nx_window *window,
    const WCHAR *wide, size_t units, int32_t cursor_units) {
    if (!window) return NX_ERROR_ARGUMENT;
    if (GetCurrentThreadId() != window->owner_thread || !window->hwnd)
        return NX_ERROR_STATE;
    return nx_window_set_preedit_utf16(window, wide, units, cursor_units);
}
