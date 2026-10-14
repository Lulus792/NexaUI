#ifndef NEXAUI_WINDOWS_WINDOW_INTERNAL_H
#define NEXAUI_WINDOWS_WINDOW_INTERNAL_H

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "nexaui/nexaui.h"

#if defined(NEXAUI_BUILDING_SHARED)
#define NX_WINDOW_INTERNAL_API __declspec(dllexport)
#elif defined(NEXAUI_SHARED)
#define NX_WINDOW_INTERNAL_API __declspec(dllimport)
#else
#define NX_WINDOW_INTERNAL_API
#endif

/* Only the window's creation thread may borrow this handle. The nx_window
   must outlive every native renderer that uses it. Never expose this header
   in the installed public C API. */
NX_WINDOW_INTERNAL_API HWND nx_window_native_hwnd(nx_window *window);

/* Native-test seam for UTF-16 composition conversion and window-owned state. */
NX_WINDOW_INTERNAL_API nx_result nx_window_windows_test_ime_preedit(
    nx_window *window, const WCHAR *wide, size_t units, int32_t cursor_units);

#endif
