/* Reuse the independent OS helpers and native fixture; no SDK linkage. */
#define main tree_keyboard_peer_main
#include "client.c"
#undef main

static int action(AtspiAccessible *row, int expander, const char *wanted) {
    GError *error = NULL;
    AtspiAccessible *target = row ? g_object_ref(row) : NULL;
    if (expander && target) {
        int count = atspi_accessible_get_child_count(row, &error);
        g_object_unref(target); target = NULL;
        for (int i = 0; !error && i < count && count <= 32; ++i) {
            AtspiAccessible *child = atspi_accessible_get_child_at_index(row, i, &error);
            AtspiRole role = child && !error ? atspi_accessible_get_role(child, &error) : ATSPI_ROLE_INVALID;
            if (!error && role == ATSPI_ROLE_PUSH_BUTTON) { target = child; break; }
            if (child) g_object_unref(child);
        }
    }
    AtspiAction *iface = target && !error ? atspi_accessible_get_action_iface(target) : NULL;
    int count = iface ? atspi_action_get_n_actions(iface, &error) : 0, okay = 0;
    for (int i = 0; !error && i < count && count <= 16; ++i) {
        char *name = atspi_action_get_action_name(iface, i, &error);
        int match = !error && name && !strcmp(name, wanted);
        g_free(name);
        if (match) { okay = atspi_action_do_action(iface, i, &error) && !error; break; }
    }
    if (error) g_error_free(error);
    if (iface) g_object_unref(iface);
    if (target) g_object_unref(target);
    return okay;
}

int main(int argc, char **argv) {
    int okay = 0, initialized = 0;
    AtspiAccessible *desktop = NULL, *app = NULL, *old_leaf = NULL, *old_child = NULL, *fresh_leaf = NULL;
    AtspiEventListener *listener = NULL;
    const char *events[] = {"object:state-changed:focused", "object:state-changed:expanded", "object:state-changed:selected"};
    unsigned subscribed = 0;
    CHECK(argc == 2);
    char *end = NULL; unsigned long pid = strtoul(argv[1], &end, 10);
    CHECK(end && !*end && pid && pid <= 2147483647UL);
    CHECK(atspi_init() == 0); initialized = 1; atspi_set_timeout(1000, 1000);
    desktop = atspi_get_desktop(0); CHECK(desktop);
    double deadline = now() + 5;
    while (now() < deadline && !old_leaf) {
        if (!app) app = application(desktop, (guint)pid);
        if (app) old_leaf = find(app, 15, 0);
        if (!old_leaf) pump();
    }
    CHECK(app && old_leaf);
    host_bus = g_strdup(ATSPI_OBJECT(app)->app->bus_name); CHECK(host_bus);
    listener = atspi_event_listener_new(changed, NULL, NULL); CHECK(listener);
    for (unsigned i = 0; i < 3; ++i) {
        GError *error = NULL; int registered = atspi_event_listener_register(listener, events[i], &error);
        if (error) g_error_free(error);
        CHECK(registered); ++subscribed;
    }
    CHECK(focus(app, 17));
    old_child = find(app, 13, 0); CHECK(old_child);
    AtspiAccessible *root = find(app, 11, 0); CHECK(root);
    root_path = g_strdup(ATSPI_OBJECT(root)->path); g_object_unref(root);
    child_path = g_strdup(ATSPI_OBJECT(old_child)->path);
    leaf_path = g_strdup(ATSPI_OBJECT(old_leaf)->path);
    CHECK(root_path && child_path && leaf_path);
    setvbuf(stdout, NULL, _IONBF, 0);
    for (unsigned step = 0; step < 8; ++step) {
        int is_root = step >= 4, open = (step & 1u) != 0;
        int handle = step == 2 || step == 3 || step == 5 || step == 6;
        AtspiAccessible *node = find(app, is_root ? 11 : 13, 0); CHECK(node);
        pump();
        unsigned before = is_root ? root_expansion[open] : child_expansion[open];
        unsigned before_focus = focus_events;
        int requested = action(node, handle, open ? "expand" : "collapse");
        g_object_unref(node); CHECK(requested);
        printf("STEP %u\n", step);
        char line[64]; unsigned check = 999;
        CHECK(fgets(line, sizeof(line), stdin) && sscanf(line, "CHECK %u", &check) == 1 && check == step);
        int matched = 0; deadline = now() + 5;
        while (now() < deadline && !matched) {
            pump();
            matched = snapshot(app, 17, is_root ? open : 1, is_root ? (open ? 1 : -1) : open,
                15, 0, 0, is_root ? 2 : 1, before, 0, 0, 0) &&
                focus_events == before_focus && selected_events == 0;
        }
        if (!matched) fprintf(stderr, "native expansion action step %u did not converge\n", step);
        CHECK(matched);
        if (step == 0) CHECK(retired(old_leaf));
        if (step == 1) {
            fresh_leaf = find(app, 15, 0); CHECK(fresh_leaf);
            CHECK(strcmp(ATSPI_OBJECT(old_leaf)->path, ATSPI_OBJECT(fresh_leaf)->path));
        }
        if (step == 4) CHECK(retired(old_child) && fresh_leaf && retired(fresh_leaf));
        if (step == 5) {
            AtspiAccessible *fresh = find(app, 13, 0); CHECK(fresh);
            int different = strcmp(ATSPI_OBJECT(old_child)->path, ATSPI_OBJECT(fresh)->path) != 0;
            g_object_unref(fresh); CHECK(different);
        }
        printf("ACK %u\n", step);
    }
    printf("TREE_ACTIONS_VERIFIED root=%u/%u child=%u/%u selection-events=%u\n",
        root_expansion[0], root_expansion[1], child_expansion[0], child_expansion[1], selected_events);
    okay = 1;
cleanup:
    if (listener) {
        for (unsigned i = 0; i < subscribed; ++i) {
            GError *error = NULL; (void)atspi_event_listener_deregister(listener, events[i], &error);
            if (error) g_error_free(error);
        }
        g_object_unref(listener);
    }
    if (fresh_leaf) g_object_unref(fresh_leaf);
    if (old_child) g_object_unref(old_child);
    if (old_leaf) g_object_unref(old_leaf);
    if (app) g_object_unref(app);
    if (desktop) g_object_unref(desktop);
    g_free(host_bus); g_free(last_focus); g_free(root_path); g_free(child_path); g_free(leaf_path);
    if (initialized) atspi_exit();
    return okay ? 0 : 1;
}
