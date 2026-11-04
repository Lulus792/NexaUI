#include "wayland_v3_fixture.h"

int main(void) {
    test_server server = {0};
    char runtime[] = "/tmp/nexaui-wayland-v3-XXXXXX";
    CHECK(mkdtemp(runtime));
    CHECK(setenv("XDG_RUNTIME_DIR", runtime, 1) == 0);
    CHECK(setenv("NEXAUI_PLATFORM", "wayland", 1) == 0);
    server.display = wl_display_create();
    CHECK(server.display);
    CHECK(wl_global_create(server.display, &wl_compositor_interface, 1,
                           &server, bind_compositor));
    CHECK(wl_global_create(server.display, &wl_shm_interface, 1,
                           &server, bind_shm));
    CHECK(wl_global_create(server.display, &wl_seat_interface, 5,
                           &server, bind_seat));
    CHECK(wl_global_create(server.display, &wl_data_device_manager_interface, 3,
                           &server, bind_data_manager));
    CHECK(wl_global_create(server.display, &xdg_wm_base_interface, 1,
                           &server, bind_wm));
    CHECK(wl_global_create(server.display,
                           &zwp_text_input_manager_v3_interface, 1,
                           &server, bind_manager));
    const char *socket_name = wl_display_add_socket_auto(server.display);
    CHECK(socket_name);
    char socket_path[256];
    CHECK(snprintf(socket_path, sizeof(socket_path), "%s/%s", runtime,
                   socket_name) > 0);
    CHECK(setenv("WAYLAND_DISPLAY", socket_name, 1) == 0);
    pthread_t thread;
    CHECK(pthread_create(&thread, NULL, run_server, &server) == 0);

    nx_window_desc desc = {0};
    window_heap allocations = {0};
    desc.allocator = (nx_allocator){&allocations, window_allocate, window_free};
    desc.width = 240;
    desc.height = 120;
    desc.flags = NX_WINDOW_HIDDEN;
    nx_window *window = NULL;
    CHECK(nx_window_create(&desc, &window) == NX_OK && window);
    CHECK(nx_window_set_ime_rect(window, (nx_rect){12, 34, 2, 18}) == NX_OK);
    CHECK(nx_window_set_ime_enabled(window, 1) == NX_OK);
    CHECK(wl_display_roundtrip(nx_window_wayland_display(window)) >= 0);
    CHECK(atomic_load(&server.enable_commits) == 1);
    CHECK(atomic_load(&server.caret_ok) == 1);
    size_t preedit_length = 99;
    int32_t preedit_begin = -2, preedit_end = -2;
    CHECK(nx_window_get_ime_preedit(window, NULL, 0, &preedit_length,
        &preedit_begin, &preedit_end) == NX_OK);
    CHECK(preedit_length == 0 && preedit_begin == 0 && preedit_end == 0);
    nx_event event;
    int available = 0;
    CHECK(nx_window_next_event(window, &event, &available) == NX_OK);
    CHECK(!available);
    wl_surface_commit(nx_window_wayland_surface(window));
    CHECK(wl_display_roundtrip(nx_window_wayland_display(window)) >= 0);
    CHECK(nx_window_get_ime_preedit(window, NULL, 0, &preedit_length,
        &preedit_begin, &preedit_end) == NX_OK);
    CHECK(preedit_length == 4 && preedit_begin == 2 && preedit_end == 2);
    char preedit[5] = {0};
    CHECK(nx_window_get_ime_preedit(window, preedit, 4, &preedit_length,
        &preedit_begin, &preedit_end) == NX_ERROR_LIMIT);
    CHECK(nx_window_get_ime_preedit(window, preedit, sizeof(preedit),
        &preedit_length, &preedit_begin, &preedit_end) == NX_OK);
    CHECK(strcmp(preedit, "na\xc3\xaf") == 0);
    const uint32_t expected[] = {'c', 'a', 'f', 0xe9};
    for (size_t i = 0; i < 4; ++i) {
        available = 0;
        CHECK(nx_window_next_event(window, &event, &available) == NX_OK);
        CHECK(available && event.type == NX_EVENT_TEXT &&
              event.scalar == expected[i]);
    }
    CHECK(nx_window_next_event(window, &event, &available) == NX_OK);
    CHECK(available && event.type == NX_EVENT_IME_PREEDIT_CHANGED);
    uint8_t font_bytes[512];
    font_fixture(font_bytes);
    nx_font *font = NULL;
    nx_font_collection *fonts = NULL;
    nx_text_field *field = NULL;
    nx_context *context = NULL;
    CHECK(nx_font_open(font_bytes, sizeof(font_bytes), NULL, &font) == NX_OK);
    CHECK(nx_font_collection_create(NULL, &fonts) == NX_OK);
    CHECK(nx_font_collection_add(fonts, font) == NX_OK);
    CHECK(nx_text_field_create("x", 1, 1024, &field) == NX_OK);
    CHECK(nx_context_create(NULL, &context) == NX_OK);
    nx_text_field_desc field_desc = {0};
    field_desc.id = 81;
    field_desc.bounds = (nx_rect){0, 0, 120, 28};
    field_desc.name = (nx_utf8_span){"Editor", 6};
    field_desc.fonts = fonts;
    field_desc.pixels_per_em = 16;
    field_desc.glyph_cache_bytes = 4096;
    field_desc.ime_window = window;
    nx_interaction_state interaction;
    int valid = 0, submitted = 0;
    CHECK(nx_begin_frame(context, 160, 60, 0) == NX_OK);
    CHECK(nx_text_field_widget(context, &field_desc, field, &interaction,
        &valid, &submitted) == NX_OK);
    CHECK(nx_end_frame(context) == NX_OK);
    CHECK(nx_focus_set(context, 81) == NX_OK);
    CHECK(nx_begin_frame(context, 160, 60, 0) == NX_OK);
    CHECK(nx_text_field_widget(context, &field_desc, field, &interaction,
        &valid, &submitted) == NX_OK && interaction.focused);
    CHECK(nx_end_frame(context) == NX_OK);
    const nx_command *commands = NULL;
    size_t command_count = 0, line_count = 0;
    CHECK(nx_frame_commands(context, &commands, &command_count) == NX_OK);
    for (size_t i = 0; i < command_count; ++i)
        if (commands[i].type == NX_COMMAND_LINE) ++line_count;
    CHECK(line_count >= 2); /* Composition underline and insertion caret. */
    nx_utf8_span draft;
    CHECK(nx_text_field_text(field, &draft) == NX_OK);
    CHECK(draft.length == 1 && draft.data[0] == 'x');
    CHECK(wl_display_roundtrip(nx_window_wayland_display(window)) >= 0);
    wl_surface_commit(nx_window_wayland_surface(window));
    CHECK(wl_display_roundtrip(nx_window_wayland_display(window)) >= 0);
    CHECK(nx_window_get_ime_preedit(window, NULL, 0, &preedit_length,
        &preedit_begin, &preedit_end) == NX_OK);
    CHECK(preedit_length == 4 && preedit_begin == -1 && preedit_end == -1);
    size_t routed = 0;
    CHECK(nx_window_route_events(window, context, &routed) == NX_OK);
    CHECK(routed >= 1);
    CHECK(nx_begin_frame(context, 160, 60, 0) == NX_OK);
    CHECK(nx_text_field_widget(context, &field_desc, field, &interaction,
        &valid, &submitted) == NX_OK && interaction.focused);
    CHECK(nx_end_frame(context) == NX_OK);
    CHECK(nx_frame_commands(context, &commands, &command_count) == NX_OK);
    line_count = 0;
    for (size_t i = 0; i < command_count; ++i)
        if (commands[i].type == NX_COMMAND_LINE) ++line_count;
    CHECK(line_count == 1); /* The IME hid the caret, not the composition. */
    CHECK(nx_text_field_set_text(field, "abcd", 4) == NX_OK);
    CHECK(nx_begin_frame(context, 160, 60, 0) == NX_OK);
    CHECK(nx_text_field_widget(context, &field_desc, field, &interaction,
        &valid, &submitted) == NX_OK && interaction.focused);
    CHECK(nx_end_frame(context) == NX_OK);
    CHECK(wl_display_roundtrip(nx_window_wayland_display(window)) >= 0);
    CHECK(atomic_load(&server.field_surrounding_ok));
    wl_surface_commit(nx_window_wayland_surface(window));
    CHECK(wl_display_roundtrip(nx_window_wayland_display(window)) >= 0);
    CHECK(nx_window_route_events(window, context, &routed) == NX_OK);
    CHECK(routed >= 2);
    CHECK(nx_begin_frame(context, 160, 60, 0) == NX_OK);
    const nx_event *frame_events = NULL;
    size_t frame_event_count = 0;
    CHECK(nx_frame_events(context, &frame_events, &frame_event_count) == NX_OK);
    nx_event stale_delete = {0}, stale_commit = {0};
    for (size_t i = 0; i < frame_event_count; ++i) {
        if (frame_events[i].type == NX_EVENT_IME_DELETE_SURROUNDING)
            stale_delete = frame_events[i];
        if (frame_events[i].type == NX_EVENT_TEXT &&
            frame_events[i].ime_target == 81)
            stale_commit = frame_events[i];
    }
    CHECK(stale_delete.ime_target == 81 && stale_commit.ime_target == 81);
    CHECK(nx_text_field_widget(context, &field_desc, field, &interaction,
        &valid, &submitted) == NX_OK && interaction.focused);
    CHECK(nx_end_frame(context) == NX_OK);
    CHECK(nx_text_field_text(field, &draft) == NX_OK);
    CHECK(draft.length == 4 && memcmp(draft.data, "abc!", 4) == 0);
    CHECK(nx_context_queue_event(context, &stale_delete) == NX_OK);
    CHECK(nx_context_queue_event(context, &stale_commit) == NX_OK);
    CHECK(nx_begin_frame(context, 160, 60, 0) == NX_OK);
    CHECK(nx_text_field_widget(context, &field_desc, field, &interaction,
        &valid, &submitted) == NX_OK && interaction.focused);
    CHECK(nx_end_frame(context) == NX_OK);
    CHECK(nx_text_field_text(field, &draft) == NX_OK);
    CHECK(draft.length == 4 && memcmp(draft.data, "abc!", 4) == 0);
    CHECK(nx_window_set_ime_surrounding_text(window, 91, 7,
        "ab\xc3\xa9" "cd", 6, 6, 6) == NX_OK);
    CHECK(wl_display_roundtrip(nx_window_wayland_display(window)) >= 0);
    CHECK(atomic_load(&server.surrounding_ok));
    wl_surface_commit(nx_window_wayland_surface(window));
    CHECK(wl_display_roundtrip(nx_window_wayland_display(window)) >= 0);
    CHECK(nx_window_next_event(window, &event, &available) == NX_OK);
    if (available && event.type == NX_EVENT_IME_PREEDIT_CHANGED)
        CHECK(nx_window_next_event(window, &event, &available) == NX_OK);
    CHECK(available && event.type == NX_EVENT_IME_DELETE_SURROUNDING &&
          event.ime_target == 91 && event.ime_revision == 7 &&
          event.ime_before == 1 && event.ime_after == 0);
    CHECK(nx_window_next_event(window, &event, &available) == NX_OK);
    CHECK(available && event.type == NX_EVENT_TEXT && event.scalar == '!' &&
          event.ime_target == 91 && event.ime_revision == 7);
    CHECK(nx_window_next_event(window, &event, &available) == NX_OK);
    CHECK(!available);
    CHECK(nx_window_set_ime_surrounding_text(window, 91, 7,
        "ab\xc3\xa9" "cd", 6, 3, 6) == NX_ERROR_ARGUMENT);
    CHECK(nx_window_set_ime_surrounding_text(window, 0, 0,
        NULL, 0, 0, 0) == NX_OK);
    nx_context_destroy(context);
    nx_text_field_destroy(field);
    nx_font_collection_destroy(fonts);
    nx_font_close(font);
    CHECK(nx_window_set_ime_enabled(window, 0) == NX_OK);
    CHECK(wl_display_roundtrip(nx_window_wayland_display(window)) >= 0);
    CHECK(atomic_load(&server.disable_commits) == 1);
    CHECK(nx_window_get_ime_preedit(window, NULL, 0, &preedit_length,
        &preedit_begin, &preedit_end) == NX_OK && preedit_length == 0);
    CHECK(atomic_load(&server.normal_content) > 0);
    CHECK(nx_window_set_ime_purpose(window, NX_IME_PURPOSE_PASSWORD) == NX_OK);
    CHECK(nx_window_set_ime_surrounding_text(window, 92, 1,
        "secret", 6, 6, 6) == NX_ERROR_ARGUMENT);
    CHECK(nx_window_set_ime_surrounding_text(window, 92, 1,
        "", 0, 0, 0) == NX_OK);
    CHECK(nx_window_set_ime_enabled(window, 1) == NX_OK);
    CHECK(wl_display_roundtrip(nx_window_wayland_display(window)) >= 0);
    CHECK(atomic_load(&server.password_content) > 0 &&
          server.surrounding_length == 0);
    int normal_before = atomic_load(&server.normal_content);
    CHECK(nx_window_set_ime_purpose(window, NX_IME_PURPOSE_NORMAL) == NX_OK);
    CHECK(nx_window_set_ime_surrounding_text(window, 93, 2,
        "abc", 3, 3, 3) == NX_OK);
    CHECK(wl_display_roundtrip(nx_window_wayland_display(window)) >= 0);
    CHECK(atomic_load(&server.normal_content) > normal_before &&
          server.surrounding_length == 3 &&
          memcmp(server.surrounding, "abc", 3) == 0);
    CHECK(nx_window_set_ime_enabled(window, 0) == NX_OK);
    CHECK(wl_display_roundtrip(nx_window_wayland_display(window)) >= 0);
    /* A different client of the same display (such as Vulkan WSI) can read
       new-id events into the default queue without dispatching that queue. */
    struct wl_display *display = nx_window_wayland_display(window);
    struct wl_event_queue *queue = wl_display_create_queue(display);
    CHECK(queue);
    size_t before_offer = allocations.calls;
    atomic_store(&server.send_offer, 1);
    wl_surface_commit(nx_window_wayland_surface(window));
    CHECK(wl_display_roundtrip_queue(display, queue) >= 0);
    CHECK(allocations.calls == before_offer); /* Offer callback is still pending. */
    wl_event_queue_destroy(queue);
    nx_window_destroy(window);
    CHECK(allocations.calls > before_offer && allocations.live == 0);
    wl_display_terminate(server.display);
    CHECK(pthread_join(thread, NULL) == 0);
    wl_display_destroy(server.display);
    CHECK(unlink(socket_path) == 0 || access(socket_path, F_OK) != 0);
    CHECK(rmdir(runtime) == 0);
    return 0;
}
