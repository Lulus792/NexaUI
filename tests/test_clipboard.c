#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#define UNICODE
#include <windows.h>

#include "nexaui/nexaui.h"

#include <stdio.h>
#include <string.h>
#include <wchar.h>

int main(void) {
    /* Prefer a private clipboard; otherwise run only if the interactive
       clipboard starts empty. */
    int isolated = 0;
    HWINSTA station = CreateWindowStationW(NULL, CWF_CREATE_ONLY,
                                            WINSTA_ALL_ACCESS, NULL);
    DWORD create_error=station?0:GetLastError(),station_error=0;
    if (station && SetProcessWindowStation(station)) {
        HDESK desktop = CreateDesktopW(L"NexaUIClipboardTest", NULL, NULL,
                                        0, GENERIC_ALL, NULL);
        if (!desktop || !SetThreadDesktop(desktop)) {
            fprintf(stderr,"clipboard transfer skipped: private desktop unavailable (Win32 %lu)\n",(unsigned long)GetLastError());
            return 77;
        }
        isolated = 1;
    } else if (station) {
        station_error=GetLastError();
        CloseWindowStation(station);
    }
    fprintf(stderr,"clipboard environment: isolated=%d create_error=%lu station_error=%lu\n",isolated,
        (unsigned long)create_error,(unsigned long)station_error);
    int result = 1, owned = 0;
    const char *stage="create window";
    nx_window *window = NULL;
    HWND hwnd = NULL;
    nx_text_buffer *source = NULL, *pasted = NULL;
    const char sample[] = "A\xc3\xa9\xf0\x9f\x98\x80\nB";
    nx_window_desc desc = {0};
    desc.title_utf8 = "NexaUI clipboard test";
    desc.width = 100.0f;
    desc.height = 50.0f;
    desc.flags = NX_WINDOW_HIDDEN;
    if (nx_window_create(&desc, &window) != NX_OK) goto cleanup;
    stage="find owned HWND";
    hwnd = FindWindowW(L"NexaUIWindowClass", L"NexaUI clipboard test");
    if (!hwnd) goto cleanup;
    stage="reject invalid text/arguments";
    if (nx_window_clipboard_set_text(window, "\xc0", 1u) !=
        NX_ERROR_ARGUMENT) goto cleanup;
    size_t length = 99u;
    if (nx_window_clipboard_get_text(window, NULL, 1u, &length) !=
        NX_ERROR_ARGUMENT) goto cleanup;
    if (!isolated) {
        stage="inspect interactive clipboard availability";
        if (!OpenClipboard(hwnd)) { result = 77; goto cleanup; }
        SetLastError(ERROR_SUCCESS);
        int formats = CountClipboardFormats();
        DWORD formats_error = GetLastError();
        CloseClipboard();
        if (formats != 0 || formats_error != ERROR_SUCCESS) {
            result = 77;
            goto cleanup;
        }
    }
    stage="read initially empty clipboard";
    if (nx_window_clipboard_get_text(window, NULL, 0u, &length) !=
        NX_ERROR_STATE) goto cleanup;

    nx_text_clipboard adapter = {0};
    stage="create window clipboard adapter";
    if (nx_text_clipboard_for_window(window, &adapter) != NX_OK ||
        adapter.user != window || !adapter.read || !adapter.write)
        goto cleanup;

    stage="create/select source text";
    if (nx_text_buffer_create(sample, sizeof(sample) - 1u, 4096u,
                              &source) != NX_OK ||
        nx_text_buffer_set_selection(source, 0u,
                                     sizeof(sample) - 1u) != NX_OK)
        goto cleanup;
    nx_utf8_span span = {0};
    stage="write selected sample";
    if (nx_text_buffer_selected_span(source, &span) != NX_OK ||
        span.length != sizeof(sample) - 1u ||
        adapter.write(adapter.user, span.data, span.length) != NX_OK)
        goto cleanup;
    owned = 1;
    stage="inspect private native Unicode newline";
    if (!OpenClipboard(hwnd)) goto cleanup;
    HGLOBAL handle = GetClipboardData(CF_UNICODETEXT);
    const WCHAR *wide = handle ? GlobalLock(handle) : NULL;
    int correct_newline = wide && wcsstr(wide, L"\r\nB") != NULL;
    if (wide) GlobalUnlock(handle);
    CloseClipboard();
    if (!correct_newline) goto cleanup;
    stage="query sample UTF-8 length";
    if (adapter.read(adapter.user, NULL, 0u, &length) != NX_OK ||
        length != span.length) goto cleanup;
    char bytes[sizeof(sample)] = {'z'};
    stage="reject short output without writing";
    if (nx_window_clipboard_get_text(window, bytes, length,
                                      &length) != NX_ERROR_LIMIT ||
        bytes[0] != 'z') goto cleanup;
    stage="read sample UTF-8 roundtrip";
    if (nx_window_clipboard_get_text(window, bytes, sizeof(bytes),
                                      &length) != NX_OK ||
        length != span.length || memcmp(bytes, sample, length) != 0 ||
        bytes[length] != '\0') goto cleanup;
    stage="paste sample into text buffer";
    if (nx_text_buffer_create("", 0u, 4096u, &pasted) != NX_OK ||
        nx_text_buffer_replace(pasted, bytes, length) != NX_OK)
        goto cleanup;
    const char *pasted_bytes = NULL;
    size_t pasted_length = 0;
    if (nx_text_buffer_text(pasted, &pasted_bytes, &pasted_length) != NX_OK ||
        pasted_length != sizeof(sample) - 1u ||
        memcmp(pasted_bytes, sample, pasted_length) != 0)
        goto cleanup;
    stage="roundtrip empty string";
    if (nx_window_clipboard_set_text(window, NULL, 0u) != NX_OK ||
        nx_window_clipboard_get_text(window, bytes, sizeof(bytes),
                                      &length) != NX_OK ||
        length != 0u || bytes[0] != '\0') goto cleanup;
    result = 0;
cleanup:
    if(result==1)fprintf(stderr,"clipboard failure stage: %s; last Win32 error %lu\n",stage,(unsigned long)GetLastError());
    if (owned && OpenClipboard(hwnd)) {
        if (GetClipboardOwner() == hwnd) EmptyClipboard();
        CloseClipboard();
    }
    nx_text_buffer_destroy(pasted);
    nx_text_buffer_destroy(source);
    nx_window_destroy(window);
    if (result == 0) puts("clipboard transfer tests passed");
    else if (result == 77) puts("clipboard transfer skipped: clipboard in use");
    else fputs("clipboard transfer test failed\n", stderr);
    return result;
}
