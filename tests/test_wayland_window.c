#define _POSIX_C_SOURCE 200809L
#include "nexaui/nexaui.h"
#include "wayland/window_internal.h"
#include "x11/window_internal.h"

#include <pthread.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wayland-client.h>
#include "fixture_font.h"

#define CHECK(expr) do { if (!(expr)) { \
    fprintf(stderr, "%s:%d: %s failed\n", __FILE__, __LINE__, #expr); \
    return 1; \
} } while (0)

typedef struct allocation_probe {
    unsigned calls, live, fail_on;
} allocation_probe;

static void *probe_allocate(void *user, size_t size) {
    allocation_probe *probe = user;
    if (++probe->calls == probe->fail_on) return NULL;
    void *pointer = malloc(size);
    if (pointer) ++probe->live;
    return pointer;
}

static void probe_deallocate(void *user, void *pointer, size_t size) {
    allocation_probe *probe = user;
    (void)size;
    if (pointer) --probe->live;
    free(pointer);
}

typedef struct wrong_thread_case {
    nx_window *window;
    nx_result poll_result, info_result;
} wrong_thread_case;

static void *wrong_thread(void *user) {
    wrong_thread_case *test = user;
    nx_window_info info;
    test->poll_result = nx_window_poll(test->window, 0);
    test->info_result = nx_window_get_info(test->window, &info);
    return NULL;
}

static int next(nx_window *window, nx_event_type type,
                nx_event *out_event) {
    int available = 0;
    CHECK(nx_window_next_event(window, out_event, &available) == NX_OK);
    CHECK(available && out_event->type == type);
    return 0;
}

static int drain(nx_window *window) {
    nx_event event;
    int available;
    do {
        CHECK(nx_window_next_event(window, &event, &available) == NX_OK);
    } while (available);
    return 0;
}

static int test_widget_ime_focus(nx_window *window) {
    nx_context *context = NULL;
    nx_font_collection *fonts = NULL;
    nx_font *font = NULL;
    nx_text_field *first = NULL, *second = NULL, *password = NULL;
    uint8_t font_bytes[512];
    font_fixture(font_bytes);
    CHECK(nx_context_create(NULL, &context) == NX_OK);
    CHECK(nx_font_open(font_bytes, sizeof(font_bytes), NULL, &font) == NX_OK);
    CHECK(nx_font_collection_create(NULL, &fonts) == NX_OK);
    CHECK(nx_font_collection_add(fonts, font) == NX_OK);
    CHECK(nx_text_field_create("", 0, 1024, &first) == NX_OK);
    CHECK(nx_text_field_create("", 0, 1024, &second) == NX_OK);
    CHECK(nx_text_field_create_password("pw", 2, &password) == NX_OK);
    CHECK(nx_window_wayland_test_focus(window, 1) == NX_OK);
    CHECK(drain(window) == 0);
    nx_text_field_desc desc = {0};
    desc.id = 81;
    desc.bounds = (nx_rect){0, 0, 100, 24};
    desc.name = (nx_utf8_span){"First", 5};
    desc.fonts = fonts;
    desc.pixels_per_em = 16;
    desc.glyph_cache_bytes = 4096;
    desc.ime_window = window;
    nx_interaction_state state;
    int valid = 0, submitted = 0;
    int requested = -1, activated = -1;
    CHECK(nx_begin_frame(context, 200, 100, 0) == NX_OK);
    CHECK(nx_text_field_widget(context, &desc, first, &state,
                                &valid, &submitted) == NX_OK);
    desc.id = 82;
    desc.bounds.y = 30;
    desc.name = (nx_utf8_span){"Second", 6};
    CHECK(nx_text_field_widget(context, &desc, second, &state,
                                &valid, &submitted) == NX_OK);
    CHECK(nx_end_frame(context) == NX_OK);
    CHECK(nx_focus_set(context, 81) == NX_OK);
    CHECK(nx_begin_frame(context, 200, 100, 0) == NX_OK);
    desc.id = 81;
    desc.bounds.y = 0;
    desc.name = (nx_utf8_span){"First", 5};
    CHECK(nx_text_field_widget(context, &desc, first, &state,
                                &valid, &submitted) == NX_OK);
    desc.id = 82;
    desc.bounds.y = 30;
    desc.name = (nx_utf8_span){"Second", 6};
    CHECK(nx_text_field_widget(context, &desc, second, &state,
                                &valid, &submitted) == NX_OK);
    CHECK(nx_end_frame(context) == NX_OK);
    CHECK(nx_window_wayland_test_ime_state(window, &requested,
                                            &activated) == NX_OK &&
          requested && activated);
    nx_result preedit = nx_window_wayland_test_ime_preedit(window,
        "na\xc3\xaf", 2, 0);
    CHECK(preedit == NX_OK || preedit == NX_ERROR_UNSUPPORTED);
    if (preedit == NX_OK) {
        nx_event event;
        CHECK(next(window, NX_EVENT_IME_PREEDIT_CHANGED, &event) == 0);
        CHECK(nx_begin_frame(context, 200, 100, 0) == NX_OK);
        desc.id = 81;
        desc.bounds.y = 0;
        desc.name = (nx_utf8_span){"First", 5};
        CHECK(nx_text_field_widget(context, &desc, first, &state,
                                    &valid, &submitted) == NX_OK);
        desc.id = 82;
        desc.bounds.y = 30;
        desc.name = (nx_utf8_span){"Second", 6};
        CHECK(nx_text_field_widget(context, &desc, second, &state,
                                    &valid, &submitted) == NX_OK);
        CHECK(nx_end_frame(context) == NX_OK);
        const nx_command *commands = NULL;
        size_t command_count = 0, line_count = 0;
        CHECK(nx_frame_commands(context, &commands, &command_count) == NX_OK);
        for (size_t i = 0; i < command_count; ++i)
            if (commands[i].type == NX_COMMAND_LINE) ++line_count;
        CHECK(line_count >= 2);
        nx_utf8_span draft;
        CHECK(nx_text_field_text(first, &draft) == NX_OK && draft.length == 0);
        CHECK(nx_window_wayland_test_ime_commit(window, "", 0) == NX_OK);
        CHECK(next(window, NX_EVENT_IME_PREEDIT_CHANGED, &event) == 0);
    }
    CHECK(nx_focus_set(context, 82) == NX_OK);
    CHECK(nx_begin_frame(context, 200, 100, 0) == NX_OK);
    desc.id = 82;
    desc.bounds.y = 30;
    desc.name = (nx_utf8_span){"Second", 6};
    CHECK(nx_text_field_widget(context, &desc, second, &state,
                                &valid, &submitted) == NX_OK);
    desc.id = 81;
    desc.bounds.y = 0;
    desc.name = (nx_utf8_span){"First", 5};
    CHECK(nx_text_field_widget(context, &desc, first, &state,
                                &valid, &submitted) == NX_OK);
    CHECK(nx_end_frame(context) == NX_OK);
    CHECK(nx_window_wayland_test_ime_state(window, &requested,
                                            &activated) == NX_OK &&
          requested && activated);
    CHECK(nx_begin_frame(context, 200, 100, 0) == NX_OK);
    CHECK(nx_end_frame(context) == NX_OK);
    CHECK(nx_window_wayland_test_ime_state(window, &requested,
                                            &activated) == NX_OK &&
          !requested && !activated);
    desc.id = 83;
    desc.password = 1;
    desc.name = (nx_utf8_span){"Password", 8};
    CHECK(nx_begin_frame(context, 200, 100, 0) == NX_OK);
    CHECK(nx_text_field_widget(context, &desc, password, &state,
                                &valid, &submitted) == NX_OK);
    CHECK(nx_end_frame(context) == NX_OK);
    CHECK(nx_focus_set(context, 83) == NX_OK);
    CHECK(nx_begin_frame(context, 200, 100, 0) == NX_OK);
    CHECK(nx_text_field_widget(context, &desc, password, &state,
                                &valid, &submitted) == NX_OK);
    CHECK(nx_end_frame(context) == NX_OK);
    CHECK(nx_window_wayland_test_ime_state(window, &requested,
                                            &activated) == NX_OK &&
          requested && activated);
    CHECK(drain(window) == 0);
    nx_result secret_preedit = nx_window_wayland_test_ime_preedit(window,
        "\xc3\xa9", 0, 0);
    CHECK(secret_preedit == NX_OK ||
          secret_preedit == NX_ERROR_UNSUPPORTED);
    if (secret_preedit == NX_OK) {
        nx_event event;
        CHECK(next(window, NX_EVENT_IME_PREEDIT_CHANGED, &event) == 0);
        CHECK(nx_begin_frame(context, 200, 100, 0) == NX_OK);
        CHECK(nx_text_field_widget(context, &desc, password, &state,
                                    &valid, &submitted) == NX_OK);
        CHECK(nx_end_frame(context) == NX_OK);
        const nx_command *commands = NULL;
        size_t command_count = 0, line_count = 0;
        CHECK(nx_frame_commands(context, &commands, &command_count) == NX_OK);
        for (size_t i = 0; i < command_count; ++i)
            if (commands[i].type == NX_COMMAND_LINE) ++line_count;
        CHECK(line_count >= 2);
        nx_utf8_span draft;
        CHECK(nx_text_field_text(password, &draft) == NX_OK &&
              draft.length == 2 && memcmp(draft.data, "pw", 2) == 0);
        CHECK(nx_window_wayland_test_ime_commit(window, "\xc3\xa9", 0) ==
              NX_OK);
        size_t routed = 0;
        CHECK(nx_window_route_events(window, context, &routed) == NX_OK &&
              routed >= 2);
        CHECK(nx_begin_frame(context, 200, 100, 0) == NX_OK);
        CHECK(nx_text_field_widget(context, &desc, password, &state,
                                    &valid, &submitted) == NX_OK);
        CHECK(nx_end_frame(context) == NX_OK);
        CHECK(nx_text_field_text(password, &draft) == NX_OK &&
              draft.length == 4 &&
              memcmp(draft.data, "pw\xc3\xa9", 4) == 0);
    }
    desc.id = 81;
    desc.password = 0;
    desc.name = (nx_utf8_span){"First", 5};
    CHECK(nx_begin_frame(context, 200, 100, 0) == NX_OK);
    CHECK(nx_text_field_widget(context, &desc, first, &state,
                                &valid, &submitted) == NX_OK);
    CHECK(nx_end_frame(context) == NX_OK);
    CHECK(nx_focus_set(context, 81) == NX_OK);
    CHECK(nx_begin_frame(context, 200, 100, 0) == NX_OK);
    CHECK(nx_text_field_widget(context, &desc, first, &state,
                                &valid, &submitted) == NX_OK);
    CHECK(nx_end_frame(context) == NX_OK);
    CHECK(nx_window_wayland_test_ime_state(window, &requested,
                                            &activated) == NX_OK &&
          requested && activated);
    nx_context_destroy(context);
    CHECK(nx_window_wayland_test_ime_state(window, &requested,
                                            &activated) == NX_OK &&
          !requested && !activated);
    nx_text_field_destroy(second);
    nx_text_field_destroy(first);
    nx_text_field_destroy(password);
    nx_font_collection_destroy(fonts);
    nx_font_close(font);
    return 0;
}

int main(void) {
    const char *name = getenv("WAYLAND_DISPLAY");
    if (!name || !*name) {
        fputs("Wayland display unavailable; native test skipped\n", stderr);
        return 77;
    }
    CHECK(setenv("NEXAUI_PLATFORM", "wayland", 1) == 0);
    nx_window_desc desc = {0};
    desc.title_utf8 = "NexaUI Wayland caf\xc3\xa9";
    desc.width = 240;
    desc.height = 120;
    desc.flags = NX_WINDOW_HIDDEN;
    nx_window *hidden = NULL, *visible = NULL;
    CHECK(nx_window_create(&desc, &hidden) == NX_OK && hidden);
    desc.flags = 0;
    CHECK(nx_window_create(&desc, &visible) == NX_OK && visible);
    struct wl_display *first = nx_window_wayland_display(hidden);
    struct wl_display *second = nx_window_wayland_display(visible);
    CHECK(first && second && first != second &&
          nx_window_wayland_surface(hidden) &&
          nx_window_wayland_surface(visible) &&
          !nx_window_x11_display(hidden));
    CHECK(wl_display_roundtrip(first) >= 0 &&
          wl_display_roundtrip(second) >= 0 &&
          wl_display_get_error(first) == 0 &&
          wl_display_get_error(second) == 0);
    for (int attempt = 0; attempt < 80 &&
            (nx_window_wayland_ready(visible) & 7u) != 7u; ++attempt)
        CHECK(nx_window_poll(visible, 25) == NX_OK);
    if ((nx_window_wayland_ready(visible) & 7u) != 7u)
        fprintf(stderr, "Wayland ready flags: hidden=%u visible=%u\n",
            nx_window_wayland_ready(hidden), nx_window_wayland_ready(visible));
    CHECK((nx_window_wayland_ready(hidden) & 1u) == 0 &&
          (nx_window_wayland_ready(visible) & 1u) != 0 &&
          (nx_window_wayland_ready(visible) & 6u) == 6u);
    nx_window_info info = {0};
    CHECK(nx_window_get_info(hidden, &info) == NX_OK &&
          info.width == 240 && info.height == 120 &&
          info.scale == 1.0f && !info.should_close);
    CHECK(nx_window_get_info(visible, &info) == NX_OK &&
          info.width > 0 && info.height > 0 && info.scale >= 1.0f);
    CHECK(nx_window_poll(hidden, 0) == NX_OK &&
          nx_window_poll(visible, 0) == NX_OK);
    CHECK(drain(visible) == 0);
    CHECK(nx_window_wayland_test_pointer_enter(visible, 11, 17) == NX_OK);
    nx_event event;
    CHECK(next(visible, NX_EVENT_POINTER_MOVE, &event) == 0 &&
          event.x == 11 && event.y == 17);
    for (int shape = NX_CURSOR_DEFAULT; shape <= NX_CURSOR_BUSY; ++shape)
        CHECK(nx_window_set_cursor(visible, (nx_cursor_shape)shape) == NX_OK);
    CHECK(nx_window_set_cursor(visible, (nx_cursor_shape)999) ==
          NX_ERROR_ARGUMENT);
    CHECK(wl_display_roundtrip(second) >= 0 &&
          wl_display_get_error(second) == 0);
    const char uri_drop[] = "file:///tmp/one%20file\r\n"
                            "file:///tmp/two\r\n";
    CHECK(nx_window_wayland_test_uri_drop(visible, uri_drop,
          sizeof(uri_drop) - 1u, 20.5f, 30.25f) == NX_OK);
    char drop_path[64] = {0};
    size_t drop_length = 0;
    nx_file_drop_info drop_info = {0};
    int drop_available = 0;
    CHECK(nx_window_next_file_drop(visible, NULL, 0, &drop_length,
          &drop_info, &drop_available) == NX_OK && drop_available &&
          drop_length == strlen("/tmp/one file") && drop_info.index == 0 &&
          drop_info.count == 2 && drop_info.x == 20.5f &&
          drop_info.y == 30.25f);
    CHECK(nx_window_next_file_drop(visible, drop_path, drop_length,
          &drop_length, &drop_info, &drop_available) == NX_ERROR_LIMIT &&
          drop_available);
    CHECK(nx_window_next_file_drop(visible, drop_path, sizeof(drop_path),
          &drop_length, &drop_info, &drop_available) == NX_OK &&
          !strcmp(drop_path, "/tmp/one file"));
    CHECK(nx_window_next_file_drop(visible, drop_path, sizeof(drop_path),
          &drop_length, &drop_info, &drop_available) == NX_OK &&
          !strcmp(drop_path, "/tmp/two") && drop_info.index == 1);
    CHECK(nx_window_next_file_drop(visible, drop_path, sizeof(drop_path),
          &drop_length, &drop_info, &drop_available) == NX_OK &&
          !drop_available);
    const size_t clipboard_size = 256u * 1024u + 2u;
    char *clipboard_text = malloc(clipboard_size);
    char *clipboard_output = malloc(clipboard_size + 1u);
    CHECK(clipboard_text && clipboard_output);
    memset(clipboard_text, 'A', clipboard_size);
    clipboard_text[clipboard_size - 2u] = (char)0xc3;
    clipboard_text[clipboard_size - 1u] = (char)0xa9;
    size_t clipboard_length = 0;
    CHECK(nx_window_wayland_test_clipboard_roundtrip(visible,
          clipboard_text, clipboard_size, NULL, 0,
          &clipboard_length) == NX_OK &&
          clipboard_length == clipboard_size);
    clipboard_output[0] = 'Z';
    CHECK(nx_window_wayland_test_clipboard_roundtrip(visible,
          clipboard_text, clipboard_size, clipboard_output, clipboard_size,
          &clipboard_length) == NX_ERROR_LIMIT &&
          clipboard_length == clipboard_size && clipboard_output[0] == 'Z');
    CHECK(nx_window_wayland_test_clipboard_roundtrip(visible,
          clipboard_text, clipboard_size, clipboard_output,
          clipboard_size + 1u, &clipboard_length) == NX_OK &&
          clipboard_length == clipboard_size &&
          !memcmp(clipboard_text, clipboard_output, clipboard_size) &&
          clipboard_output[clipboard_size] == '\0');
    CHECK(nx_window_wayland_test_clipboard_roundtrip(visible, "\xc0", 1,
          clipboard_output, clipboard_size + 1u,
          &clipboard_length) == NX_ERROR_STATE && clipboard_length == 0);
    CHECK(nx_window_wayland_test_clipboard_roundtrip(visible, NULL, 0,
          clipboard_output, clipboard_size + 1u,
          &clipboard_length) == NX_OK && clipboard_length == 0 &&
          clipboard_output[0] == '\0');
    free(clipboard_output);
    free(clipboard_text);
    CHECK(nx_window_clipboard_set_text(hidden, "\xc0", 1) ==
          NX_ERROR_ARGUMENT);
    if (nx_window_wayland_ready(hidden) & 8u)
        CHECK(nx_window_clipboard_set_text(hidden, "test", 4) ==
              NX_ERROR_STATE);
    CHECK(nx_window_wayland_test_output(visible, 1, 0) == NX_OK);
    CHECK(drain(visible) == 0);
    CHECK(nx_window_get_info(visible, &info) == NX_OK &&
          info.scale == 1.0f);
    int logical_width = (int)info.width, logical_height = (int)info.height;
    CHECK(nx_window_wayland_test_output(visible, 2, 1) == NX_OK);
    CHECK(next(visible, NX_EVENT_RESIZE, &event) == 0 &&
          event.width == (float)logical_width &&
          event.height == (float)logical_height && event.scale == 2.0f);
    CHECK(nx_window_get_info(visible, &info) == NX_OK &&
          info.scale == 2.0f);
    int bw = 0, bh = 0, cw = 0, ch = 0;
    CHECK(nx_window_wayland_test_extents(visible, &bw, &bh, &cw, &ch) == NX_OK &&
          bw == logical_width * 2 && bh == logical_height * 2 &&
          cw == 64 && ch == 64);
    CHECK(nx_window_wayland_test_output(visible, 2, 0) == NX_OK);
    CHECK(next(visible, NX_EVENT_RESIZE, &event) == 0 &&
          event.scale == 1.0f);
    CHECK(nx_window_wayland_test_extents(visible, &bw, &bh, &cw, &ch) == NX_OK &&
          bw == logical_width && bh == logical_height &&
          cw == 32 && ch == 32);
    CHECK(wl_display_roundtrip(second) >= 0 &&
          wl_display_get_error(second) == 0);
    for (int attempt = 0; attempt < 80 &&
            (nx_window_wayland_ready(hidden) & 4u) == 0; ++attempt)
        CHECK(nx_window_poll(hidden, 25) == NX_OK);
    CHECK((nx_window_wayland_ready(hidden) & 4u) != 0);
    CHECK(drain(hidden) == 0);
    CHECK(nx_window_wayland_test_repeat_info(hidden, 50, 25) == NX_OK);
    CHECK(nx_window_wayland_test_focus(hidden, 1) == NX_OK);
    CHECK(next(hidden, NX_EVENT_FOCUS, &event) == 0 && event.focused);
    CHECK(nx_window_wayland_test_key(hidden, 'a', 1) == NX_OK);
    CHECK(next(hidden, NX_EVENT_KEY_DOWN, &event) == 0 &&
          event.key == NX_KEY_A && !event.repeat);
    CHECK(next(hidden, NX_EVENT_TEXT, &event) == 0 && event.scalar == 'a');
    int saw_repeat = 0;
    for (int attempt = 0; attempt < 10 && !saw_repeat; ++attempt) {
        CHECK(nx_window_poll(hidden, 100) == NX_OK);
        int available = 0;
        do {
            CHECK(nx_window_next_event(hidden, &event, &available) == NX_OK);
            if (available && event.type == NX_EVENT_KEY_DOWN &&
                event.key == NX_KEY_A && event.repeat) saw_repeat = 1;
        } while (available);
    }
    CHECK(saw_repeat);
    CHECK(nx_window_wayland_test_key(hidden, 'a', 0) == NX_OK);
    CHECK(next(hidden, NX_EVENT_KEY_UP, &event) == 0 &&
          event.key == NX_KEY_A);
    CHECK(nx_window_poll(hidden, 80) == NX_OK);
    int available = 0;
    do {
        CHECK(nx_window_next_event(hidden, &event, &available) == NX_OK);
        CHECK(!available || event.type != NX_EVENT_KEY_DOWN || !event.repeat);
    } while (available);
    CHECK(nx_window_wayland_test_focus(hidden, 1) == NX_OK);
    CHECK(next(hidden, NX_EVENT_FOCUS, &event) == 0 && event.focused);
    nx_result ime = nx_window_set_ime_enabled(hidden, 1);
    CHECK(ime == NX_OK || ime == NX_ERROR_UNSUPPORTED);
    if (ime == NX_OK) {
        CHECK(nx_window_set_ime_rect(hidden,
              (nx_rect){12, 14, 1, 20}) == NX_OK);
        CHECK(nx_window_set_ime_rect(hidden,
              (nx_rect){(float)INT_MAX, 14, 1, 20}) == NX_ERROR_LIMIT);
        CHECK(nx_window_wayland_test_ime_commit(hidden,
              "a\xc3\xa9\xf0\x9f\x98\x80", 0) == NX_OK);
        CHECK(next(hidden, NX_EVENT_TEXT, &event) == 0 && event.scalar == 'a');
        CHECK(next(hidden, NX_EVENT_TEXT, &event) == 0 && event.scalar == 0xe9);
        CHECK(next(hidden, NX_EVENT_TEXT, &event) == 0 &&
              event.scalar == 0x1f600);
        nx_result preedit = nx_window_wayland_test_ime_preedit(hidden,
            "na\xc3\xaf", 2, 0);
        CHECK(preedit == NX_OK || preedit == NX_ERROR_UNSUPPORTED);
        if (preedit == NX_OK) {
            CHECK(next(hidden, NX_EVENT_IME_PREEDIT_CHANGED, &event) == 0);
            char composition[5] = {0};
            size_t composition_length = 0;
            int32_t begin = 0, end = 0;
            CHECK(nx_window_get_ime_preedit(hidden, composition,
                sizeof(composition), &composition_length, &begin, &end) == NX_OK);
            CHECK(composition_length == 4 && begin == 2 && end == 2 &&
                  strcmp(composition, "na\xc3\xaf") == 0);
            CHECK(nx_window_wayland_test_ime_preedit(hidden,
                "stale", 0, 1) == NX_OK);
            CHECK(nx_window_next_event(hidden, &event, &available) == NX_OK &&
                  !available);
            CHECK(nx_window_wayland_test_ime_preedit(hidden,
                "na\xc3\xaf", -1, 0) == NX_OK);
            CHECK(next(hidden, NX_EVENT_IME_PREEDIT_CHANGED, &event) == 0);
            CHECK(nx_window_get_ime_preedit(hidden, NULL, 0,
                &composition_length, &begin, &end) == NX_OK &&
                composition_length == 4 && begin == -1 && end == -1);
            CHECK(nx_window_wayland_test_ime_commit(hidden, "", 0) == NX_OK);
            CHECK(next(hidden, NX_EVENT_IME_PREEDIT_CHANGED, &event) == 0);
            CHECK(nx_window_get_ime_preedit(hidden, NULL, 0,
                &composition_length, &begin, &end) == NX_OK &&
                composition_length == 0);
        }
        CHECK(nx_window_wayland_test_ime_commit(hidden, "stale", 1) == NX_OK);
        CHECK(nx_window_wayland_test_ime_commit(hidden, "\xc0", 0) == NX_OK);
        CHECK(nx_window_next_event(hidden, &event, &available) == NX_OK &&
              !available);
        CHECK(nx_window_wayland_test_key(hidden, 'b', 1) == NX_OK);
        CHECK(next(hidden, NX_EVENT_KEY_DOWN, &event) == 0 &&
              event.key == NX_KEY_B);
        CHECK(nx_window_next_event(hidden, &event, &available) == NX_OK &&
              !available);
        CHECK(nx_window_wayland_test_key(hidden, 'b', 0) == NX_OK);
        CHECK(next(hidden, NX_EVENT_KEY_UP, &event) == 0);
        CHECK(nx_window_set_ime_enabled(hidden, 0) == NX_OK);
        CHECK(nx_window_wayland_test_key(hidden, 'b', 1) == NX_OK);
        CHECK(next(hidden, NX_EVENT_KEY_DOWN, &event) == 0);
        CHECK(next(hidden, NX_EVENT_TEXT, &event) == 0 && event.scalar == 'b');
        CHECK(nx_window_wayland_test_key(hidden, 'b', 0) == NX_OK);
        CHECK(next(hidden, NX_EVENT_KEY_UP, &event) == 0);
        CHECK(drain(hidden) == 0);
        CHECK(nx_window_set_ime_enabled(hidden, 1) == NX_OK);
        CHECK(nx_window_wayland_test_focus(hidden, 0) == NX_OK);
        CHECK(next(hidden, NX_EVENT_FOCUS, &event) == 0 && !event.focused);
        CHECK(nx_window_wayland_test_ime_commit(hidden, "x", 0) ==
              NX_ERROR_STATE);
        CHECK(nx_window_wayland_test_focus(hidden, 1) == NX_OK);
        CHECK(next(hidden, NX_EVENT_FOCUS, &event) == 0 && event.focused);
        CHECK(nx_window_wayland_test_ime_commit(hidden, "z", 0) == NX_OK);
        CHECK(next(hidden, NX_EVENT_TEXT, &event) == 0 && event.scalar == 'z');
        CHECK(nx_window_set_ime_enabled(hidden, 0) == NX_OK);
    }
    CHECK(nx_window_set_ime_enabled(hidden, 2) == NX_ERROR_ARGUMENT);
    if (ime == NX_OK) CHECK(test_widget_ime_focus(hidden) == 0);
    CHECK(nx_window_wayland_test_focus(hidden, 0) == NX_OK);
    CHECK(next(hidden, NX_EVENT_FOCUS, &event) == 0 && !event.focused);
    CHECK(wl_display_roundtrip(first) >= 0 && wl_display_get_error(first) == 0);
    CHECK(nx_window_wayland_test_key(hidden, 'a', 1) == NX_OK);
    CHECK(drain(hidden) == 0);
    CHECK(nx_window_wayland_test_focus(hidden, 0) == NX_OK);
    CHECK(next(hidden, NX_EVENT_FOCUS, &event) == 0 && !event.focused);
    CHECK(nx_window_poll(hidden, 80) == NX_OK);
    do {
        CHECK(nx_window_next_event(hidden, &event, &available) == NX_OK);
        CHECK(!available || event.type != NX_EVENT_KEY_DOWN || !event.repeat);
    } while (available);
    wrong_thread_case wrong = {visible, NX_OK, NX_OK};
    pthread_t thread;
    CHECK(pthread_create(&thread, NULL, wrong_thread, &wrong) == 0);
    CHECK(pthread_join(thread, NULL) == 0);
    CHECK(wrong.poll_result == NX_ERROR_STATE &&
          wrong.info_result == NX_ERROR_STATE);
    CHECK(nx_window_wayland_test_output(visible, INT_MAX, 1) ==
          NX_ERROR_LIMIT);
    CHECK(nx_window_get_info(visible, &info) == NX_OK &&
          info.scale == 1.0f);
    nx_window_destroy(visible);
    nx_window_destroy(hidden);

    allocation_probe allocation = {0};
    allocation.fail_on = 2;
    desc.flags = NX_WINDOW_HIDDEN;
    desc.allocator = (nx_allocator){&allocation, probe_allocate,
                                    probe_deallocate};
    nx_window *failed = NULL;
    CHECK(nx_window_create(&desc, &failed) == NX_ERROR_MEMORY &&
          !failed && allocation.live == 0);
    allocation = (allocation_probe){0};
    allocation.fail_on = 3;
    desc.flags = 0;
    CHECK(nx_window_create(&desc, &failed) == NX_ERROR_MEMORY &&
          !failed && allocation.live == 0);
    allocation = (allocation_probe){0};
    allocation.fail_on = 4;
    CHECK(nx_window_create(&desc, &failed) == NX_ERROR_MEMORY &&
          !failed && allocation.live == 0);
    desc.allocator = (nx_allocator){0};
    desc.flags = NX_WINDOW_HIDDEN;

    CHECK(setenv("WAYLAND_DISPLAY", "nexaui-no-such-display", 1) == 0);
    CHECK(nx_window_create(&desc, &failed) == NX_ERROR_PLATFORM && !failed);
    CHECK(unsetenv("NEXAUI_PLATFORM") == 0);
    if (getenv("DISPLAY") && *getenv("DISPLAY")) {
        CHECK(nx_window_create(&desc, &failed) == NX_OK && failed);
        CHECK(nx_window_x11_display(failed) &&
              !nx_window_wayland_display(failed));
        nx_window_destroy(failed);
    }
    puts("Wayland lifecycle, scale, cursor, key repeat and X11 fallback verified");
    return 0;
}
