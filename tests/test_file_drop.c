#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#define UNICODE
#include <windows.h>
#include <shellapi.h>
#include <shlobj_core.h>

#include "nexaui/nexaui.h"
#include "window_internal.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { \
    fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #x); \
    return 1; \
} } while (0)

typedef struct alloc_state {
    size_t calls, limit, live;
} alloc_state;

static void *test_allocate(void *user, size_t size) {
    alloc_state *state = (alloc_state *)user;
    if (++state->calls > state->limit) return NULL;
    void *pointer = malloc(size);
    if (pointer) ++state->live;
    return pointer;
}

static void test_deallocate(void *user, void *pointer, size_t size) {
    alloc_state *state = (alloc_state *)user;
    (void)size;
    if (pointer) --state->live;
    free(pointer);
}

static HDROP make_drop(const WCHAR *paths, size_t path_bytes, LONG x, LONG y) {
    if (path_bytes > SIZE_MAX - sizeof(DROPFILES)) return NULL;
    HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE | GMEM_ZEROINIT,
                                 sizeof(DROPFILES) + path_bytes);
    if (!memory) return NULL;
    DROPFILES *drop = (DROPFILES *)GlobalLock(memory);
    if (!drop) { GlobalFree(memory); return NULL; }
    drop->pFiles = sizeof(DROPFILES);
    drop->pt.x = x;
    drop->pt.y = y;
    drop->fNC = FALSE;
    drop->fWide = TRUE;
    memcpy((uint8_t *)drop + sizeof(DROPFILES), paths, path_bytes);
    GlobalUnlock(memory);
    return (HDROP)memory;
}

int main(void) {
    const WCHAR paths[] = L"C:\\Test\\caf\u00e9.txt\0C:\\Test\\\u9053.txt\0";
    const char *expected[] = {
        "C:\\Test\\caf\xc3\xa9.txt",
        "C:\\Test\\\xe9\x81\x93.txt"
    };
    nx_window_desc desc = {0};
    desc.width = 220; desc.height = 120;
    desc.flags = NX_WINDOW_HIDDEN;
    desc.title_utf8 = "NexaUI file drop first";
    nx_window *first = NULL, *second = NULL;
    CHECK(nx_window_create(&desc, &first) == NX_OK);
    desc.title_utf8 = "NexaUI file drop second";
    CHECK(nx_window_create(&desc, &second) == NX_OK);
    HWND hwnd = nx_window_native_hwnd(first);
    CHECK(hwnd != NULL);
    CHECK((GetWindowLongPtrW(hwnd, GWL_EXSTYLE) & WS_EX_ACCEPTFILES) != 0);
    nx_window_info window_info;
    CHECK(nx_window_get_info(first, &window_info) == NX_OK);
    HDROP drop = make_drop(paths, sizeof(paths), 36, 24);
    CHECK(drop != NULL);
    CHECK(PostMessageW(hwnd, WM_DROPFILES, (WPARAM)drop, 0));
    CHECK(nx_window_poll(first, 0) == NX_OK);

    size_t length = 99;
    nx_file_drop_info info = {0};
    int available = 0;
    CHECK(nx_window_next_file_drop(second, NULL, 0, &length, &info,
                                   &available) == NX_OK);
    CHECK(!available && length == 0 && info.count == 0);
    CHECK(nx_window_next_file_drop(first, NULL, 0, &length, &info,
                                   &available) == NX_OK);
    CHECK(available && length == strlen(expected[0]));
    CHECK(info.index == 0 && info.count == 2);
    CHECK(fabsf(info.x - 36.0f / window_info.scale) < 0.001f &&
          fabsf(info.y - 24.0f / window_info.scale) < 0.001f);
    char short_buffer[4] = {'x', 'x', 'x', 'x'};
    CHECK(nx_window_next_file_drop(first, short_buffer, sizeof(short_buffer),
                                   &length, &info, &available) == NX_ERROR_LIMIT);
    CHECK(available && length == strlen(expected[0]) &&
          short_buffer[0] == 'x' && short_buffer[3] == 'x');
    char exact_buffer[128];
    memset(exact_buffer, 'x', sizeof(exact_buffer));
    CHECK(nx_window_next_file_drop(first, exact_buffer, length,
                                   &length, &info, &available) == NX_ERROR_LIMIT);
    CHECK(available && exact_buffer[0] == 'x');
    char path[128];
    CHECK(nx_window_next_file_drop(first, path, sizeof(path), &length,
                                   &info, &available) == NX_OK);
    CHECK(available && strcmp(path, expected[0]) == 0 &&
          length == strlen(expected[0]) && info.index == 0);
    CHECK(nx_window_next_file_drop(first, path, sizeof(path), &length,
                                   &info, &available) == NX_OK);
    CHECK(available && strcmp(path, expected[1]) == 0 &&
          length == strlen(expected[1]) && info.index == 1 && info.count == 2);
    CHECK(nx_window_next_file_drop(first, path, sizeof(path), &length,
                                   &info, &available) == NX_OK);
    CHECK(!available && length == 0 && info.count == 0);
    CHECK(nx_window_next_file_drop(NULL, path, sizeof(path), &length,
                                   &info, &available) == NX_ERROR_ARGUMENT);
    CHECK(nx_window_next_file_drop(first, NULL, 1, &length,
                                   &info, &available) == NX_ERROR_ARGUMENT);
    nx_window_destroy(second);
    nx_window_destroy(first);

    alloc_state allocator = {0};
    allocator.limit = SIZE_MAX;
    desc.allocator = (nx_allocator){&allocator, test_allocate, test_deallocate};
    desc.title_utf8 = "NexaUI file drop allocation failure";
    nx_window *limited = NULL;
    CHECK(nx_window_create(&desc, &limited) == NX_OK);
    CHECK(allocator.live >= 2);
    HWND limited_hwnd = nx_window_native_hwnd(limited);
    CHECK(limited_hwnd != NULL);
    const WCHAR prior_path[] = L"C:\\Test\\prior.txt\0";
    drop = make_drop(prior_path, sizeof(prior_path), 3, 4);
    CHECK(drop != NULL);
    CHECK(PostMessageW(limited_hwnd, WM_DROPFILES, (WPARAM)drop, 0));
    CHECK(nx_window_poll(limited, 0) == NX_OK);
    size_t baseline_live = allocator.live;
    allocator.limit = allocator.calls + 3u;
    drop = make_drop(paths, sizeof(paths), 12, 10);
    CHECK(drop != NULL);
    CHECK(PostMessageW(limited_hwnd, WM_DROPFILES, (WPARAM)drop, 0));
    CHECK(nx_window_poll(limited, 0) == NX_ERROR_MEMORY);
    CHECK(allocator.live == baseline_live);
    CHECK(nx_window_next_file_drop(limited, NULL, 0, &length, &info,
                                   &available) == NX_OK && available &&
          length == strlen("C:\\Test\\prior.txt") && info.count == 1);
    CHECK(nx_window_next_file_drop(limited, path, sizeof(path), &length,
                                   &info, &available) == NX_OK && available &&
          strcmp(path, "C:\\Test\\prior.txt") == 0);
    CHECK(nx_window_next_file_drop(limited, NULL, 0, &length, &info,
                                   &available) == NX_OK && !available);
    nx_window_destroy(limited);
    CHECK(allocator.live == 0);
    puts("Win32 Unicode file drops, isolation and atomic failure verified");
    return 0;
}
