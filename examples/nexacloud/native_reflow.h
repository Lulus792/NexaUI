/* Private smoke helper, included only by native test builds after capture(). */
static int reflow_key(cloud_scene *scene, nx_key key) {
    nx_event event = {0}; event.type = NX_EVENT_KEY_DOWN; event.key = key;
    if (nx_context_queue_event(cloud_context(scene), &event) != NX_OK) return 0;
    event.type = NX_EVENT_KEY_UP;
    return nx_context_queue_event(cloud_context(scene), &event) == NX_OK;
}

static int reflow_frames(nx_window *window, cloud_scene *scene, nx_renderer_driver *renderer) {
    for (unsigned i = 0; i < 3; ++i) if (!reflow_frame(window, scene, renderer)) return 0;
    return 1;
}

static int reflow_capture(nx_window *window, cloud_scene *scene, nx_renderer_driver *renderer,
                          const char *prefix, const char *tag, const char *view) {
    char path[1024];
    return snprintf(path, sizeof(path), "%s-%s-%s-200.ppm", prefix, tag, view) < (int)sizeof(path) &&
        capture(window, scene, renderer, path);
}

static int native_reflow(nx_window *window, cloud_scene *scene, nx_renderer_driver *renderer,
                         const char *prefix, const char *tag) {
    if (!cloud_set_text_scale(scene, 2) || !reflow_frames(window, scene, renderer) ||
        !reflow_capture(window, scene, renderer, prefix, tag, "files") ||
        nx_focus_set(cloud_context(scene), 203) != NX_OK || !reflow_key(scene, NX_KEY_ENTER) ||
        !reflow_frames(window, scene, renderer)) return 0;
    uint32_t index;
    if (nx_semantics_find(cloud_context(scene), 500, &index) != NX_OK ||
        !reflow_capture(window, scene, renderer, prefix, tag, "details") ||
        !reflow_key(scene, NX_KEY_ESCAPE) || !reflow_frames(window, scene, renderer) ||
        nx_semantics_find(cloud_context(scene), 500, &index) == NX_OK ||
        nx_focus_id(cloud_context(scene)) != 203 ||
        nx_focus_set(cloud_context(scene), 15) != NX_OK || !reflow_key(scene, NX_KEY_ENTER) ||
        !reflow_frames(window, scene, renderer) ||
        nx_focus_set(cloud_context(scene), 404) != NX_OK || !reflow_frames(window, scene, renderer)) return 0;
    return reflow_capture(window, scene, renderer, prefix, tag, "settings");
}
