#define _POSIX_C_SOURCE 200809L
/* Independent OS client: no NexaUI headers, symbols or application model. */
#include <atspi/atspi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static char *host_bus, *last_focus, *root_path, *child_path, *leaf_path;
static unsigned focus_events, expanded_events, selected_events;
static unsigned root_expansion[2], child_expansion[2], child_selected, leaf_deselected;
static void pump(void) {
    while (g_main_context_iteration(NULL, FALSE)) {}
    struct timespec delay = {0, 10000000};
    (void)nanosleep(&delay, NULL);
}
static double now(void) {
    struct timespec t;
    if (clock_gettime(CLOCK_MONOTONIC, &t)) return 0;
    return (double)t.tv_sec + (double)t.tv_nsec / 1e9;
}
static void changed(AtspiEvent *event, void *data) {
    (void)data;
    if (event->source && host_bus &&
        !strcmp(ATSPI_OBJECT(event->source)->app->bus_name, host_bus)) {
        if (!strcmp(event->type, "object:state-changed:focused") && event->detail1) {
            ++focus_events;
            g_free(last_focus);
            last_focus = g_strdup(ATSPI_OBJECT(event->source)->path);
        }
        const char *path = ATSPI_OBJECT(event->source)->path;
        if (!strcmp(event->type, "object:state-changed:expanded")) {
            ++expanded_events;
            if (root_path && !strcmp(path, root_path)) ++root_expansion[!!event->detail1];
            if (child_path && !strcmp(path, child_path)) ++child_expansion[!!event->detail1];
        }
        if (!strcmp(event->type, "object:state-changed:selected")) {
            ++selected_events;
            if (event->detail1 && child_path && !strcmp(path, child_path)) ++child_selected;
            if (!event->detail1 && leaf_path && !strcmp(path, leaf_path)) ++leaf_deselected;
        }
    }
    g_boxed_free(ATSPI_TYPE_EVENT, event);
}
static AtspiAccessible *application(AtspiAccessible *desktop, guint pid) {
    GError *error = NULL;
    atspi_accessible_clear_cache(desktop);
    int count = atspi_accessible_get_child_count(desktop, &error);
    if (error) { g_error_free(error); return NULL; }
    for (int i = 0; i < count; ++i) {
        AtspiAccessible *app = atspi_accessible_get_child_at_index(desktop, i, &error);
        if (error) { g_error_free(error); error = NULL; continue; }
        if (!app) continue;
        guint actual = atspi_accessible_get_process_id(app, &error);
        if (!error && actual == pid) return app;
        if (error) { g_error_free(error); error = NULL; }
        g_object_unref(app);
    }
    return NULL;
}
static AtspiAccessible *find(AtspiAccessible *root, unsigned id, unsigned depth) {
    if (!root || depth > 16) return NULL;
    GError *error = NULL;
    atspi_accessible_clear_cache(root);
    char wanted[32];
    (void)snprintf(wanted, sizeof(wanted), "Node %u", id);
    char *name = atspi_accessible_get_name(root, &error);
    int match = !error && name && !strcmp(name, wanted);
    if (error) { g_error_free(error); error = NULL; }
    g_free(name);
    AtspiRole role = atspi_accessible_get_role(root, &error);
    if (error) { g_error_free(error); return NULL; }
    if (match && role == ATSPI_ROLE_TREE_ITEM) return g_object_ref(root);
    int count = atspi_accessible_get_child_count(root, &error);
    if (error) { g_error_free(error); return NULL; }
    for (int i = 0; i < count; ++i) {
        AtspiAccessible *child = atspi_accessible_get_child_at_index(root, i, &error);
        if (error) { g_error_free(error); error = NULL; continue; }
        AtspiAccessible *result = find(child, id, depth + 1);
        if (child) g_object_unref(child);
        if (result) return result;
    }
    return NULL;
}
static int has(AtspiAccessible *node, AtspiStateType state) {
    atspi_accessible_clear_cache(node);
    AtspiStateSet *set = atspi_accessible_get_state_set(node);
    int result = set && atspi_state_set_contains(set, state);
    if (set) g_object_unref(set);
    return result;
}
static int retired(AtspiAccessible *node) {
    GError *error = NULL;
    atspi_accessible_clear_cache(node);
    char *name = atspi_accessible_get_name(node, &error);
    int result = error != NULL;
    if (error) g_error_free(error);
    g_free(name);
    return result;
}
static int focus(AtspiAccessible *app, unsigned id) {
    AtspiAccessible *node = find(app, id, 0);
    if (!node) return 0;
    AtspiComponent *component = atspi_accessible_get_component_iface(node);
    GError *error = NULL;
    int requested = component && atspi_component_grab_focus(component, &error) && !error;
    if (error) g_error_free(error);
    if (component) g_object_unref(component);
    double deadline = now() + 5;
    while (requested && now() < deadline && !has(node, ATSPI_STATE_FOCUSED)) pump();
    int result = requested && has(node, ATSPI_STATE_FOCUSED);
    g_object_unref(node);
    return result;
}
static int snapshot(AtspiAccessible *app, unsigned target, int root_open,
    int child_open, unsigned selected, int require_event, unsigned old_focus_events,
    int require_expanded, unsigned old_expanded, int require_selected, unsigned old_selected,
    unsigned old_deselected) {
    AtspiAccessible *row = find(app, target, 0), *root = find(app, 11, 0);
    AtspiAccessible *child = find(app, 13, 0), *leaf = find(app, 15, 0), *other = find(app, 31, 0);
    int okay = row && root && other && has(row, ATSPI_STATE_FOCUSED) &&
        has(root, ATSPI_STATE_EXPANDED) == root_open && has(other, ATSPI_STATE_SELECTED);
    if (child_open < 0) okay = okay && !child && !leaf;
    else okay = okay && child && has(child, ATSPI_STATE_EXPANDED) == child_open &&
        has(child, ATSPI_STATE_SELECTED) == (selected == 13) &&
        (!!leaf == child_open) && (!leaf || has(leaf, ATSPI_STATE_SELECTED) == (selected == 15));
    if (require_event) okay = okay && focus_events > old_focus_events && last_focus && row &&
        !strcmp(last_focus, ATSPI_OBJECT(row)->path);
    if (require_expanded) {
        unsigned actual = require_expanded == 2 ? root_expansion[!!root_open] : child_expansion[!!child_open];
        okay = okay && actual > old_expanded;
    }
    if (require_selected) okay = okay && child_selected > old_selected && leaf_deselected > old_deselected;
    if (row) g_object_unref(row);
    if (root) g_object_unref(root);
    if (child) g_object_unref(child);
    if (leaf) g_object_unref(leaf);
    if (other) g_object_unref(other);
    return okay;
}

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "tree-client:%d: %s\n", __LINE__, #x); goto cleanup; } } while (0)
int main(int argc, char **argv) {
    int okay = 0, initialized = 0;
    AtspiAccessible *desktop = NULL, *app = NULL, *old_leaf = NULL, *new_leaf = NULL, *old_child = NULL;
    AtspiEventListener *listener = NULL;
    const char *events[] = {"object:state-changed:focused", "object:state-changed:expanded", "object:state-changed:selected"};
    unsigned subscribed = 0;
    CHECK(argc == 2 || (argc == 3 && !strcmp(argv[2], "--expect-unavailable")));
    char *end = NULL;
    unsigned long pid = strtoul(argv[1], &end, 10);
    CHECK(end && !*end && pid && pid <= 2147483647UL);
    CHECK(atspi_init() == 0); initialized = 1;
    atspi_set_timeout(1000, 1000);
    desktop = atspi_get_desktop(0); CHECK(desktop);
    double deadline = now() + 5;
    while (now() < deadline && !old_leaf) {
        if (!app) app = application(desktop, (guint)pid);
        if (app) old_leaf = find(app, 15, 0);
        if (!old_leaf) pump();
    }
    if (argc == 3) {
        CHECK(!old_leaf);
        puts("TREE_UNAVAILABLE"); okay = 77; goto cleanup;
    }
    CHECK(app && old_leaf);
    host_bus = g_strdup(ATSPI_OBJECT(app)->app->bus_name); CHECK(host_bus);
    listener = atspi_event_listener_new(changed, NULL, NULL); CHECK(listener);
    for (unsigned i = 0; i < 3; ++i) {
        GError *error = NULL;
        int result = atspi_event_listener_register(listener, events[i], &error);
        if (error) { fprintf(stderr, "%s\n", error->message); g_error_free(error); }
        CHECK(result); ++subscribed;
    }
    CHECK(focus(app, 11));
    old_child = find(app, 13, 0); CHECK(old_child);
    AtspiAccessible *root_node = find(app, 11, 0); CHECK(root_node);
    root_path = g_strdup(ATSPI_OBJECT(root_node)->path); g_object_unref(root_node);
    child_path = g_strdup(ATSPI_OBJECT(old_child)->path);
    leaf_path = g_strdup(ATSPI_OBJECT(old_leaf)->path);
    CHECK(root_path && child_path && leaf_path);
    const struct { unsigned focus; int root, child; unsigned selected; int focus_change, expansion, selection; } steps[] = {
        {13,1,1,15,1,0,0}, {15,1,1,15,1,0,0}, {13,1,1,15,1,0,0},
        {13,1,0,15,0,1,0}, {13,1,1,15,0,1,0}, {15,1,1,15,1,0,0},
        {11,1,1,15,1,0,0}, {17,1,1,15,1,0,0}, {17,1,1,15,0,0,0},
        {32,1,1,15,1,0,0}, {31,1,1,15,1,0,0}, {13,1,1,13,0,0,1},
        {17,0,-1,13,1,2,0}, {11,1,1,13,0,2,0}, {13,1,1,13,1,0,0}, {15,1,1,13,1,0,0}
    };
    setvbuf(stdout, NULL, _IONBF, 0);
    for (unsigned i = 0; i < sizeof(steps)/sizeof(steps[0]); ++i) {
        if (i == 9) CHECK(focus(app, 31));
        if (i == 11) CHECK(focus(app, 13));
        if (i == 12 || i == 13) CHECK(focus(app, 11));
        pump();
        unsigned before_focus = focus_events;
        unsigned before_expanded = steps[i].expansion == 2 ? root_expansion[!!steps[i].root] :
            child_expansion[!!steps[i].child];
        unsigned before_selected = child_selected, before_deselected = leaf_deselected;
        printf("STEP %u\n", i);
        char line[64]; unsigned check = 999;
        CHECK(fgets(line, sizeof(line), stdin) && sscanf(line, "CHECK %u", &check) == 1 && check == i);
        deadline = now() + 5;
        int matched = 0;
        while (now() < deadline && !matched) {
            pump();
            matched = snapshot(app, steps[i].focus, steps[i].root, steps[i].child, steps[i].selected,
                steps[i].focus_change, before_focus, steps[i].expansion, before_expanded,
                steps[i].selection, before_selected, before_deselected);
        }
        if (!matched) fprintf(stderr, "native tree step %u did not converge\n", i);
        CHECK(matched);
        if (i == 3) CHECK(retired(old_leaf));
        if (i == 4) {
            new_leaf = find(app, 15, 0); CHECK(new_leaf);
            CHECK(strcmp(ATSPI_OBJECT(new_leaf)->path, ATSPI_OBJECT(old_leaf)->path));
            g_free(leaf_path); leaf_path = g_strdup(ATSPI_OBJECT(new_leaf)->path); CHECK(leaf_path);
        }
        if (i == 12) CHECK(retired(old_child) && new_leaf && retired(new_leaf));
        if (i == 13) {
            AtspiAccessible *fresh = find(app, 13, 0); CHECK(fresh);
            int different = strcmp(ATSPI_OBJECT(fresh)->path, ATSPI_OBJECT(old_child)->path) != 0;
            g_object_unref(fresh); CHECK(different);
        }
        printf("ACK %u\n", i);
    }
    printf("TREE_VERIFIED focus-events=%u expanded-events=%u selected-events=%u "
        "root-expanded=%u/%u child-expanded=%u/%u child-selected=%u leaf-deselected=%u\n",
        focus_events, expanded_events, selected_events, root_expansion[0], root_expansion[1],
        child_expansion[0], child_expansion[1], child_selected, leaf_deselected);
    okay = 1;
cleanup:
    if (listener) {
        for (unsigned i = 0; i < subscribed; ++i) {
            GError *error = NULL;
            (void)atspi_event_listener_deregister(listener, events[i], &error);
            if (error) g_error_free(error);
        }
        g_object_unref(listener);
    }
    if (old_child) g_object_unref(old_child);
    if (new_leaf) g_object_unref(new_leaf);
    if (old_leaf) g_object_unref(old_leaf);
    if (app) g_object_unref(app);
    if (desktop) g_object_unref(desktop);
    g_free(host_bus); g_free(last_focus); g_free(root_path); g_free(child_path); g_free(leaf_path);
    if (initialized) atspi_exit();
    return okay == 77 ? 77 : okay ? 0 : 1;
}
