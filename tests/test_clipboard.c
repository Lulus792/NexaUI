#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#define UNICODE
#include <windows.h>

#include "nexaui/nexaui.h"

#include <stdio.h>
#include <string.h>
#include <wchar.h>

typedef struct clipboard_fixture {
    HWINSTA original_station, station;
    HDESK original_desktop, desktop;
} clipboard_fixture;

static int separate_hidden_station(HWINSTA original, HWINSTA candidate) {
    wchar_t before[512], after[512];
    USEROBJECTFLAGS flags = {0};
    DWORD needed;
    return GetUserObjectInformationW(original, UOI_NAME, before,
                                     sizeof(before), &needed) &&
           GetUserObjectInformationW(candidate, UOI_NAME, after,
                                     sizeof(after), &needed) &&
           GetUserObjectInformationW(candidate, UOI_FLAGS, &flags,
                                     sizeof(flags), &needed) &&
           !(flags.dwFlags & WSF_VISIBLE) && _wcsicmp(before, after) != 0;
}

static int fixture_close(clipboard_fixture *fixture) {
    int restored = 1, desktop_closed = 1, station_closed = 1;
    if (fixture->station) {
        restored = SetProcessWindowStation(fixture->original_station) != 0;
        if (fixture->desktop &&
            !SetThreadDesktop(fixture->original_desktop)) restored = 0;
        if (fixture->desktop) desktop_closed = CloseDesktop(fixture->desktop) != 0;
        station_closed = CloseWindowStation(fixture->station) != 0;
    }
    fprintf(stderr, "clipboard cleanup: restored=%d desktop_closed=%d station_closed=%d\n",
            restored, desktop_closed, station_closed);
    return restored && desktop_closed && station_closed;
}

static int fixture_open(clipboard_fixture *fixture) {
    memset(fixture, 0, sizeof(*fixture));
    fixture->original_station = GetProcessWindowStation();
    fixture->original_desktop = GetThreadDesktop(GetCurrentThreadId());
    if (!fixture->original_station || !fixture->original_desktop) return 0;
    fixture->station = CreateWindowStationW(NULL, CWF_CREATE_ONLY,
                                           WINSTA_ALL_ACCESS, NULL);
    DWORD create_error = fixture->station ? 0 : GetLastError();
    /* An unnamed station is session-derived. It can already exist even when
       it is separate from the interactive station. Never request a named
       station or elevated rights, and never fall back to the user's clipboard.
       A reused hidden station is not exclusive; require an empty clipboard. */
    if (!fixture->station && create_error == ERROR_ALREADY_EXISTS)
        fixture->station = CreateWindowStationW(NULL, 0, WINSTA_ALL_ACCESS, NULL);
    if (!fixture->station ||
        !separate_hidden_station(fixture->original_station, fixture->station) ||
        !SetProcessWindowStation(fixture->station)) return 0;
    wchar_t name[96];
    swprintf(name, sizeof(name) / sizeof(*name), L"NexaUIClipboard-%lu-%llu",
             (unsigned long)GetCurrentProcessId(),
             (unsigned long long)GetTickCount64());
    fixture->desktop = CreateDesktopW(name, NULL, NULL, 0, GENERIC_ALL, NULL);
    if (!fixture->desktop || !SetThreadDesktop(fixture->desktop)) return 0;
    fprintf(stderr, "clipboard environment: isolated=1 reused=%d create_error=%lu\n",
            create_error == ERROR_ALREADY_EXISTS, (unsigned long)create_error);
    return 1;
}

static int clipboard_empty(HWND hwnd) {
    if (!OpenClipboard(hwnd)) return 0;
    SetLastError(ERROR_SUCCESS);
    int formats = CountClipboardFormats();
    DWORD error = GetLastError();
    int empty = formats == 0 && error == ERROR_SUCCESS && !GetClipboardOwner();
    return CloseClipboard() && empty;
}

int main(int argc, char **argv) {
    if (argc == 2 && strcmp(argv[1], "reject-current-station") == 0) {
        HWINSTA current = GetProcessWindowStation();
        if (!current || separate_hidden_station(current, current)) return 1;
        puts("current/interactive station rejected before any clipboard access");
        return 0;
    }
    int busy_guard = argc == 2 && strcmp(argv[1], "reject-busy") == 0;
    if (argc > 2 || (argc == 2 && !busy_guard)) return 2;
    clipboard_fixture fixture;
    if (!fixture_open(&fixture)) {
        fprintf(stderr, "clipboard transfer skipped: isolated station/desktop unavailable (Win32 %lu)\n",
                (unsigned long)GetLastError());
        return fixture_close(&fixture) ? 77 : 1;
    }
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
    stage="inspect isolated clipboard availability";
    if (!clipboard_empty(hwnd)) { result = 77; goto cleanup; }
    if (busy_guard) {
        stage="prepare privately owned clipboard sentinel";
        const wchar_t sentinel[] = L"NexaUI privately owned clipboard sentinel";
        HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE, sizeof(sentinel));
        wchar_t *data = memory ? GlobalLock(memory) : NULL;
        if (!data) { if (memory) GlobalFree(memory); goto cleanup; }
        memcpy(data, sentinel, sizeof(sentinel));
        GlobalUnlock(memory);
        if (!OpenClipboard(hwnd)) { GlobalFree(memory); goto cleanup; }
        int written = EmptyClipboard() && SetClipboardData(CF_UNICODETEXT, memory);
        CloseClipboard();
        if (!written) { GlobalFree(memory); goto cleanup; }
        owned = 1;
        stage="reject busy isolated clipboard without changing sentinel";
        if (clipboard_empty(hwnd) || !OpenClipboard(hwnd)) goto cleanup;
        HGLOBAL current = GetClipboardData(CF_UNICODETEXT);
        const wchar_t *contents = current ? GlobalLock(current) : NULL;
        int unchanged = contents && GlobalSize(current) >= sizeof(sentinel) &&
                        memcmp(contents, sentinel, sizeof(sentinel)) == 0 &&
                        GetClipboardOwner() == hwnd;
        if (contents) GlobalUnlock(current);
        CloseClipboard();
        if (!unchanged) goto cleanup;
        result = 0;
        puts("busy isolated clipboard rejected; owned sentinel preserved");
        goto cleanup;
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
    if (!fixture_close(&fixture)) result = 1;
    if (result == 0) puts("clipboard transfer tests passed");
    else if (result == 77) puts("clipboard transfer skipped: clipboard in use");
    else fputs("clipboard transfer test failed\n", stderr);
    return result;
}
