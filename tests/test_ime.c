#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#define UNICODE
#include <windows.h>
#include <imm.h>

#include "nexaui/nexaui.h"
#include "window_internal.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "search_font_fixture.h"

#define CHECK(x) do { if (!(x)) { \
    fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #x); \
    return 1; \
} } while (0)

int main(void) {
    nx_window_desc window_desc = {0};
    window_desc.title_utf8 = "NexaUI IME integration";
    window_desc.width = 200;
    window_desc.height = 100;
    window_desc.flags = NX_WINDOW_HIDDEN;
    nx_window *window = NULL;
    CHECK(nx_window_create(&window_desc, &window) == NX_OK);
    HWND hwnd = nx_window_native_hwnd(window);
    CHECK(hwnd != NULL);
    HIMC input = ImmGetContext(hwnd);
    if (!input) {
        nx_window_destroy(window);
        puts("No native IME input context; skipping placement check");
        return 77;
    }
    ImmReleaseContext(hwnd, input);

    uint8_t font_bytes[512];
    font_fixture(font_bytes);
    nx_font *font = NULL;
    nx_font_collection *fonts = NULL;
    nx_text_field *field = NULL;
    nx_context *ctx = NULL;
    CHECK(nx_font_open(font_bytes, sizeof(font_bytes), NULL, &font) == NX_OK);
    CHECK(nx_font_collection_create(NULL, &fonts) == NX_OK);
    CHECK(nx_font_collection_add(fonts, font) == NX_OK);
    CHECK(nx_text_field_create("01", 2, 1024, &field) == NX_OK);
    CHECK(nx_context_create(NULL, &ctx) == NX_OK);
    nx_text_field_desc desc = {0};
    desc.id = 1;
    desc.bounds = (nx_rect){10, 12, 65, 26};
    desc.name = (nx_utf8_span){"IME field", 9};
    desc.fonts = fonts;
    desc.pixels_per_em = 18;
    desc.glyph_cache_bytes = 65536;
    desc.ime_window = window;
    nx_interaction_state state;
    int valid, submitted;
    CHECK(nx_begin_frame(ctx, 200, 100, 0) == NX_OK);
    CHECK(nx_text_field_widget(ctx, &desc, field, &state,
                               &valid, &submitted) == NX_OK);
    CHECK(nx_end_frame(ctx) == NX_OK);
    CHECK(nx_focus_set(ctx, 1) == NX_OK);
    CHECK(nx_begin_frame(ctx, 200, 100, 0) == NX_OK);
    CHECK(nx_text_field_widget(ctx, &desc, field, &state,
                               &valid, &submitted) == NX_OK && state.focused);
    CHECK(nx_end_frame(ctx) == NX_OK);
    const nx_command *commands = NULL;
    size_t count = 0;
    CHECK(nx_frame_commands(ctx, &commands, &count) == NX_OK);
    nx_point caret = {0};
    int found = 0;
    for (size_t i = 0; i < count; ++i) {
        if (commands[i].type == NX_COMMAND_LINE) {
            caret = commands[i].points[0];
            found = 1;
            break;
        }
    }
    CHECK(found);
    nx_window_info info;
    CHECK(nx_window_get_info(window, &info) == NX_OK);
    input = ImmGetContext(hwnd);
    CHECK(input != NULL);
    COMPOSITIONFORM form = {0};
    CHECK(ImmGetCompositionWindow(input, &form));
    CHECK(form.dwStyle == CFS_POINT &&
          form.ptCurrentPos.x == (LONG)lround((double)caret.x * info.scale) &&
          form.ptCurrentPos.y == (LONG)lround((double)caret.y * info.scale));
    CANDIDATEFORM candidate = {0};
    CHECK(ImmGetCandidateWindow(input, 0, &candidate));
    CHECK(candidate.dwStyle == CFS_EXCLUDE &&
          candidate.rcArea.left == form.ptCurrentPos.x &&
          candidate.rcArea.top == form.ptCurrentPos.y);
    ImmReleaseContext(hwnd, input);
    RECT suggested;
    CHECK(GetWindowRect(hwnd, &suggested));
    SendMessageW(hwnd, WM_DPICHANGED, MAKEWPARAM(144, 144),
                 (LPARAM)&suggested);
    CHECK(nx_window_get_info(window, &info) == NX_OK &&
          fabsf(info.scale - 1.5f) < 0.001f);
    CHECK(nx_window_set_ime_rect(window,
        (nx_rect){caret.x, caret.y, 1.0f, 18.0f}) == NX_OK);
    input = ImmGetContext(hwnd);
    CHECK(input != NULL);
    CHECK(ImmGetCompositionWindow(input, &form));
    CHECK(form.ptCurrentPos.x == (LONG)lround((double)caret.x * info.scale) &&
          form.ptCurrentPos.y == (LONG)lround((double)caret.y * info.scale));
    ImmReleaseContext(hwnd, input);
    CHECK(nx_window_set_ime_rect(window, (nx_rect){0, 0, 0, 1}) ==
          NX_ERROR_ARGUMENT);
    CHECK(nx_window_set_ime_rect(window,
        (nx_rect){2147483648.0f, 0, 1, 1}) == NX_ERROR_LIMIT);

    const WCHAR supplementary[] = {L'a', 0xd83d, 0xde00, L'b'};
    CHECK(nx_window_windows_test_ime_preedit(window, supplementary, 4, 3)
          == NX_OK);
    size_t preedit_length = 0;
    int32_t preedit_begin = 0, preedit_end = 0;
    CHECK(nx_window_get_ime_preedit(window, NULL, 0, &preedit_length,
        &preedit_begin, &preedit_end) == NX_OK);
    CHECK(preedit_length == 6 && preedit_begin == 5 && preedit_end == 5);
    char preedit[16];
    CHECK(nx_window_get_ime_preedit(window, preedit, 6, &preedit_length,
        &preedit_begin, &preedit_end) == NX_ERROR_LIMIT);
    CHECK(nx_window_get_ime_preedit(window, preedit, sizeof(preedit),
        &preedit_length, &preedit_begin, &preedit_end) == NX_OK);
    CHECK(memcmp(preedit, "a\xf0\x9f\x98\x80" "b", 7) == 0);
    CHECK(nx_window_windows_test_ime_preedit(window, supplementary, 4, 2)
          == NX_ERROR_ARGUMENT);
    const WCHAR broken[] = {0xd83d};
    CHECK(nx_window_windows_test_ime_preedit(window, broken, 1, 1)
          == NX_ERROR_ARGUMENT);
    CHECK(nx_window_get_ime_preedit(window, NULL, 0, &preedit_length,
        &preedit_begin, &preedit_end) == NX_OK &&
        preedit_length == 6 && preedit_begin == 5);
    CHECK(nx_window_windows_test_ime_preedit(window, supplementary, 4, -1)
          == NX_OK);
    CHECK(nx_window_get_ime_preedit(window, NULL, 0, &preedit_length,
        &preedit_begin, &preedit_end) == NX_OK &&
        preedit_begin == -1 && preedit_end == -1);
    SendMessageW(hwnd, WM_IME_STARTCOMPOSITION, 0, 0);
    CHECK(nx_window_windows_test_ime_preedit(window, L"na", 2, 1) == NX_OK);
    CHECK(nx_begin_frame(ctx, 200, 100, 0) == NX_OK);
    CHECK(nx_text_field_widget(ctx, &desc, field, &state,
                               &valid, &submitted) == NX_OK);
    CHECK(nx_end_frame(ctx) == NX_OK);
    CHECK(nx_frame_commands(ctx, &commands, &count) == NX_OK);
    size_t line_count = 0;
    for (size_t i = 0; i < count; ++i)
        if (commands[i].type == NX_COMMAND_LINE) ++line_count;
    CHECK(line_count >= 2);
    nx_utf8_span draft;
    CHECK(nx_text_field_text(field, &draft) == NX_OK &&
          draft.length == 2 && memcmp(draft.data, "01", 2) == 0);
    SendMessageW(hwnd, WM_IME_ENDCOMPOSITION, 0, 0);
    CHECK(nx_window_get_ime_preedit(window, NULL, 0, &preedit_length,
        &preedit_begin, &preedit_end) == NX_OK && preedit_length == 0);
    CHECK(nx_window_windows_test_ime_preedit(window, L"xy", 2, 2) == NX_OK);
    CHECK(nx_focus_set(ctx, 0) == NX_OK);
    CHECK(nx_begin_frame(ctx, 200, 100, 0) == NX_OK);
    CHECK(nx_text_field_widget(ctx, &desc, field, &state,
                               &valid, &submitted) == NX_OK && !state.focused);
    CHECK(nx_end_frame(ctx) == NX_OK);
    CHECK(nx_window_get_ime_preedit(window, NULL, 0, &preedit_length,
        &preedit_begin, &preedit_end) == NX_OK && preedit_length == 0);
    CHECK(nx_focus_set(ctx, 1) == NX_OK);
    CHECK(nx_begin_frame(ctx, 200, 100, 0) == NX_OK);
    CHECK(nx_text_field_widget(ctx, &desc, field, &state,
                               &valid, &submitted) == NX_OK && state.focused);
    CHECK(nx_end_frame(ctx) == NX_OK);
    CHECK(nx_window_set_ime_enabled(window, 2) == NX_ERROR_ARGUMENT);
    SendMessageW(hwnd, WM_IME_COMPOSITION, 0, GCS_COMPSTR);
    CHECK(nx_window_poll(window, 0) == NX_OK);

    nx_event event;
    int available = 0;
    int preedit_events = 0;
    do {
        CHECK(nx_window_next_event(window, &event, &available) == NX_OK);
        if (available && event.type == NX_EVENT_IME_PREEDIT_CHANGED)
            ++preedit_events;
    } while (available);
    CHECK(preedit_events >= 5);
    SendMessageW(hwnd, WM_IME_CHAR, 0x9053, 0);
    CHECK(nx_window_poll(window, 0) == NX_OK);
    int committed = 0;
    do {
        CHECK(nx_window_next_event(window, &event, &available) == NX_OK);
        if (available && event.type == NX_EVENT_TEXT &&
            event.scalar == 0x9053) {
            ++committed;
            CHECK(nx_context_queue_event(ctx, &event) == NX_OK);
        }
    } while (available);
    CHECK(committed == 1);
    CHECK(nx_begin_frame(ctx, 200, 100, 0) == NX_OK);
    CHECK(nx_text_field_widget(ctx, &desc, field, &state,
                               &valid, &submitted) == NX_OK);
    CHECK(nx_end_frame(ctx) == NX_OK);
    nx_utf8_span text;
    CHECK(nx_text_field_text(field, &text) == NX_OK);
    if (text.length != 5 ||
        memcmp(text.data, "01" "\xe9\x81\x93", 5) != 0) {
        fprintf(stderr, "IME field actual length %zu: ", text.length);
        for (size_t i = 0; i < text.length; ++i)
            fprintf(stderr, "%02x ", (unsigned char)text.data[i]);
        fputc('\n', stderr);
    }
    CHECK(text.length == 5 &&
          memcmp(text.data, "01" "\xe9\x81\x93", 5) == 0);

    nx_context_destroy(ctx);
    nx_text_field_destroy(field);
    nx_font_collection_destroy(fonts);
    nx_font_close(font);
    nx_window_destroy(window);
    puts("Win32 IME placement, preedit and committed Unicode verified");
    return 0;
}
