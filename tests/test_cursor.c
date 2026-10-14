#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#define UNICODE
#include <windows.h>

#include "nexaui/nexaui.h"

#include <stdio.h>

#define CHECK(expr) do { if (!(expr)) { \
    fprintf(stderr, "%s:%d: %s failed\n", __FILE__, __LINE__, #expr); \
    return 1; \
} } while (0)

typedef struct cursor_case {
    nx_cursor_shape shape;
    LPCWSTR resource;
} cursor_case;

typedef struct thread_case {
    nx_window *window;
    nx_result result;
} thread_case;

static DWORD WINAPI set_on_wrong_thread(void *user) {
    thread_case *test = (thread_case *)user;
    test->result = nx_window_set_cursor(test->window, NX_CURSOR_CROSSHAIR);
    return 0;
}

static int check_native_cursor(HWND hwnd, LPCWSTR resource) {
    HCURSOR expected = (HCURSOR)LoadImageW(NULL, resource, IMAGE_CURSOR, 0, 0,
                                           LR_DEFAULTSIZE | LR_SHARED);
    CHECK(expected != NULL);
    CHECK(SendMessageW(hwnd, WM_SETCURSOR, (WPARAM)hwnd,
                       MAKELPARAM(HTCLIENT, WM_MOUSEMOVE)) == TRUE);
    CHECK(GetCursor() == expected);
    return 0;
}

int main(void) {
    nx_window_desc desc = {0};
    desc.width = 240;
    desc.height = 120;
    desc.flags = NX_WINDOW_HIDDEN;
    desc.title_utf8 = "NexaUI cursor first";
    nx_window *first = NULL, *second = NULL;
    CHECK(nx_window_create(&desc, &first) == NX_OK);
    desc.title_utf8 = "NexaUI cursor second";
    CHECK(nx_window_create(&desc, &second) == NX_OK);
    HWND first_hwnd = FindWindowW(L"NexaUIWindowClass", L"NexaUI cursor first");
    HWND second_hwnd = FindWindowW(L"NexaUIWindowClass", L"NexaUI cursor second");
    CHECK(first_hwnd && second_hwnd && first_hwnd != second_hwnd);
    HCURSOR saved = GetCursor();
    CHECK(check_native_cursor(first_hwnd, IDC_ARROW) == 0);
    CHECK(check_native_cursor(second_hwnd, IDC_ARROW) == 0);

    const cursor_case cases[] = {
        {NX_CURSOR_DEFAULT, IDC_ARROW},
        {NX_CURSOR_TEXT, IDC_IBEAM},
        {NX_CURSOR_LINK, IDC_HAND},
        {NX_CURSOR_CROSSHAIR, IDC_CROSS},
        {NX_CURSOR_RESIZE_HORIZONTAL, IDC_SIZEWE},
        {NX_CURSOR_RESIZE_VERTICAL, IDC_SIZENS},
        {NX_CURSOR_RESIZE_DIAGONAL_NW_SE, IDC_SIZENWSE},
        {NX_CURSOR_RESIZE_DIAGONAL_NE_SW, IDC_SIZENESW},
        {NX_CURSOR_MOVE, IDC_SIZEALL},
        {NX_CURSOR_NOT_ALLOWED, IDC_NO},
        {NX_CURSOR_BUSY, IDC_WAIT}
    };
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        CHECK(nx_window_set_cursor(first, cases[i].shape) == NX_OK);
        CHECK(check_native_cursor(first_hwnd, cases[i].resource) == 0);
    }
    CHECK(nx_window_set_cursor(first, NX_CURSOR_LINK) == NX_OK);
    CHECK(nx_window_set_cursor(second, NX_CURSOR_TEXT) == NX_OK);
    CHECK(check_native_cursor(first_hwnd, IDC_HAND) == 0);
    CHECK(check_native_cursor(second_hwnd, IDC_IBEAM) == 0);
    CHECK(check_native_cursor(first_hwnd, IDC_HAND) == 0);
    CHECK(nx_window_set_cursor(first, (nx_cursor_shape)999) ==
          NX_ERROR_ARGUMENT);
    CHECK(nx_window_set_cursor(first, (nx_cursor_shape)-1) ==
          NX_ERROR_ARGUMENT);
    CHECK(nx_window_set_cursor(NULL, NX_CURSOR_DEFAULT) ==
          NX_ERROR_ARGUMENT);
    CHECK(check_native_cursor(first_hwnd, IDC_HAND) == 0);

    thread_case cross_thread = {first, NX_OK};
    HANDLE worker = CreateThread(NULL, 0, set_on_wrong_thread,
                                 &cross_thread, 0, NULL);
    CHECK(worker != NULL);
    CHECK(WaitForSingleObject(worker, 5000) == WAIT_OBJECT_0);
    CloseHandle(worker);
    CHECK(cross_thread.result == NX_ERROR_STATE);
    CHECK(check_native_cursor(first_hwnd, IDC_HAND) == 0);

    SetCursor(saved);
    nx_window_destroy(second);
    nx_window_destroy(first);
    puts("Win32 cursor mapping, isolation and thread ownership verified");
    return 0;
}
