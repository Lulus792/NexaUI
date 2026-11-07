#define _POSIX_C_SOURCE 200809L
/* Independent OS peer. No NexaUI headers, state object or toolkit linkage. */
#include <atspi/atspi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static int german;
static unsigned visits;
static int scan_failed;
static double scan_deadline;
static double now(void) { struct timespec t; if (clock_gettime(CLOCK_MONOTONIC, &t)) exit(2); return (double)t.tv_sec + t.tv_nsec / 1e9; }
static void pump(void) { while (g_main_context_iteration(NULL, FALSE)) {} struct timespec t = {0, 1000000}; (void)nanosleep(&t, NULL); }
static const char *branch_name(void) { return german ? "Zweig" : "Branch"; }
static const char *retry_name(void) { return german ? "Erneut versuchen" : "Retry"; }
static const char *refresh_name(void) { return german ? "Aktualisieren" : "Refresh"; }
static const char *status_name(int phase) {
    static const char *const names[2][5] = {
        {"Children not requested", "Loading children", "Could not load children", "Children ready", "No children"},
        {"Unterelemente noch nicht angefordert", "Unterelemente werden geladen", "Unterelemente konnten nicht geladen werden", "Unterelemente bereit", "Keine Unterelemente"}
    };
    return names[german][phase];
}
static AtspiAccessible *target(AtspiAccessible *desktop, unsigned long pid) {
    atspi_accessible_clear_cache(desktop); GError *error = NULL;
    int count = atspi_accessible_get_child_count(desktop, &error);
    if (error) { g_error_free(error); return NULL; }
    for (int i = 0; i < count && count <= 64; ++i) {
        AtspiAccessible *app = atspi_accessible_get_child_at_index(desktop, i, &error);
        guint actual = app && !error ? atspi_accessible_get_process_id(app, &error) : 0;
        int same = !error && actual == pid;
        if (error) { g_error_free(error); error = NULL; }
        if (same) return app;
        if (app) g_object_unref(app);
    }
    return NULL;
}
static AtspiAccessible *find(AtspiAccessible *node, const char *name, AtspiRole role, unsigned depth) {
    if (!node || depth > 16 || ++visits > 512 || now() >= scan_deadline) { scan_failed = 1; return NULL; }
    atspi_accessible_clear_cache(node); GError *error = NULL;
    char *value = atspi_accessible_get_name(node, &error);
    int match = !error && value && !strcmp(value, name); g_free(value);
    AtspiRole actual = !error ? atspi_accessible_get_role(node, &error) : ATSPI_ROLE_INVALID;
    if (error) { g_error_free(error); scan_failed = 1; return NULL; }
    if (match && actual == role) return g_object_ref(node);
    int count = atspi_accessible_get_child_count(node, &error);
    if (error) { g_error_free(error); scan_failed = 1; return NULL; }
    if (count < 0 || count > 128) { scan_failed = 1; return NULL; }
    for (int i = 0; i < count && count <= 128; ++i) {
        AtspiAccessible *child = atspi_accessible_get_child_at_index(node, i, &error);
        if (error) { g_error_free(error); scan_failed = 1; return NULL; }
        AtspiAccessible *found = find(child, name, role, depth + 1);
        if (child) g_object_unref(child);
        if (found) return found;
    }
    return NULL;
}
static AtspiAccessible *lookup(AtspiAccessible *app, const char *name, AtspiRole role) {
    visits = 0; scan_failed = 0; scan_deadline = now() + 1; return find(app, name, role, 0);
}
static int state(AtspiAccessible *node, AtspiStateType wanted) {
    if (!node) return 0;
    atspi_accessible_clear_cache(node); AtspiStateSet *set = atspi_accessible_get_state_set(node);
    int yes = set && atspi_state_set_contains(set, wanted); if (set) g_object_unref(set); return yes;
}
static int retired(AtspiAccessible *node) {
    if (!node) return 0;
    GError *error = NULL; atspi_accessible_clear_cache(node);
    char *name = atspi_accessible_get_name(node, &error); g_free(name);
    int yes = error != NULL; if (error) g_error_free(error); return yes || state(node, ATSPI_STATE_DEFUNCT);
}
static int action(AtspiAccessible *node, const char *wanted) {
    GError *error = NULL; AtspiAction *iface = node ? atspi_accessible_get_action_iface(node) : NULL;
    int count = iface ? atspi_action_get_n_actions(iface, &error) : 0, yes = 0;
    for (int i = 0; !error && i < count && count <= 16; ++i) {
        char *name = atspi_action_get_action_name(iface, i, &error);
        int same = !error && name && !strcmp(name, wanted); g_free(name);
        if (same) { yes = atspi_action_do_action(iface, i, &error) && !error; break; }
    }
    if (error) g_error_free(error);
    if (iface) g_object_unref(iface);
    return yes;
}
static int focus(AtspiAccessible *node) {
    GError *error = NULL; AtspiComponent *iface = node ? atspi_accessible_get_component_iface(node) : NULL;
    int okay = iface && atspi_component_grab_focus(iface, &error) && !error;
    if (error) g_error_free(error);
    if (iface) g_object_unref(iface);
    double end = now() + 3;
    while (okay && now() < end && !state(node, ATSPI_STATE_FOCUSED)) pump();
    return okay && state(node, ATSPI_STATE_FOCUSED);
}
static int visible_in(AtspiAccessible *app, AtspiAccessible *node) {
    AtspiAccessible *area = lookup(app, german ? "Unterelemente" : "Children", ATSPI_ROLE_SCROLL_PANE);
    AtspiComponent *a = node ? atspi_accessible_get_component_iface(node) : NULL;
    AtspiComponent *b = area ? atspi_accessible_get_component_iface(area) : NULL;
    GError *error = NULL;
    AtspiRect *inner = a ? atspi_component_get_extents(a, ATSPI_COORD_TYPE_WINDOW, &error) : NULL;
    AtspiRect *outer = b && !error ? atspi_component_get_extents(b, ATSPI_COORD_TYPE_WINDOW, &error) : NULL;
    int okay = !error && inner && outer && inner->width > 0 && inner->height > 0 &&
        inner->x >= outer->x && inner->y >= outer->y &&
        (long long)inner->x + inner->width <= (long long)outer->x + outer->width &&
        (long long)inner->y + inner->height <= (long long)outer->y + outer->height;
    if (error) g_error_free(error);
    if (inner) g_boxed_free(ATSPI_TYPE_RECT, inner);
    if (outer) g_boxed_free(ATSPI_TYPE_RECT, outer);
    if (a) g_object_unref(a);
    if (b) g_object_unref(b);
    if (area) g_object_unref(area);
    return okay;
}
static int snapshot(AtspiAccessible *app, int phase, int open) {
    AtspiAccessible *branch = lookup(app, branch_name(), ATSPI_ROLE_TREE_ITEM);
    int scanned = !scan_failed;
    AtspiAccessible *status = open ? lookup(app, status_name(phase), ATSPI_ROLE_STATUS_BAR) : NULL;
    if (open) scanned = scanned && !scan_failed;
    AtspiAccessible *a = lookup(app, "Child A", ATSPI_ROLE_TREE_ITEM);
    scanned = scanned && !scan_failed;
    AtspiAccessible *b = lookup(app, "Child B", ATSPI_ROLE_TREE_ITEM);
    scanned = scanned && !scan_failed;
    int okay = scanned && branch && state(branch, ATSPI_STATE_EXPANDED) == open &&
        (!open || (status && state(status, ATSPI_STATE_BUSY) == (phase == 1))) &&
        (!!a == (open && phase == 3)) && (!!b == (open && phase == 3));
    if (branch) g_object_unref(branch);
    if (status) g_object_unref(status);
    if (a) g_object_unref(a);
    if (b) g_object_unref(b);
    return okay;
}
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "async-peer:%d: %s\n", __LINE__, #x); goto cleanup; } } while (0)
int main(int argc, char **argv) {
    int okay = 0, initialized = 0;
    AtspiAccessible *desktop = NULL, *app = NULL, *old_branch = NULL, *old_retry = NULL, *old_child = NULL, *fresh_child = NULL;
    CHECK(argc == 2 || (argc == 3 && !strcmp(argv[2], "--german"))); german = argc == 3;
    char *end; unsigned long pid = strtoul(argv[1], &end, 10); CHECK(pid && !*end && pid <= 2147483647UL);
    CHECK(atspi_init() == 0); initialized = 1; atspi_set_timeout(1000, 1000);
    desktop = atspi_get_desktop(0); CHECK(desktop);
    double until = now() + 4;
    while (!app && now() < until) { app = target(desktop, pid); if (!app) pump(); }
    CHECK(app); old_branch = lookup(app, branch_name(), ATSPI_ROLE_TREE_ITEM); CHECK(old_branch && focus(old_branch));
    setvbuf(stdout, NULL, _IONBF, 0);
    const int phases[18] = {1,1,2,1,3,3,3,1,0,1,4,4,1,0,1,1,3,1};
    for (unsigned step = 0; step < 18; ++step) {
        printf("STEP %u\n", step); char line[64]; unsigned actual = 999;
        CHECK(fgets(line, sizeof(line), stdin) && sscanf(line, "CHECK %u", &actual) == 1 && actual == step);
        if (step == 13) {
            CHECK(retired(old_branch) && !action(old_branch, "expand") && !action(old_branch, "collapse"));
            g_object_unref(app); app = NULL; until = now() + 4;
            while (!app && now() < until) { app = target(desktop, pid); if (!app) pump(); }
            CHECK(app);
        }
        if (step == 17) {
            AtspiAccessible *refresh = lookup(app, refresh_name(), ATSPI_ROLE_PUSH_BUTTON);
            int requested = refresh && action(refresh, "click"); if (refresh) g_object_unref(refresh); CHECK(requested);
        }
        int open = step != 5 && step != 8 && step != 13, matched = 0;
        until = now() + 4;
        while (now() < until && !matched) { pump(); matched = snapshot(app, phases[step], open); }
        if (!matched) fprintf(stderr, "async native step %u did not converge\n", step);
        CHECK(matched);
        if (step == 2) {
            old_retry = lookup(app, retry_name(), ATSPI_ROLE_PUSH_BUTTON);
            CHECK(old_retry && focus(old_retry) && visible_in(app, old_retry));
        }
        if (step == 3) CHECK(retired(old_retry) && !action(old_retry, "click"));
        if (step == 4) {
            old_child = lookup(app, "Child B", ATSPI_ROLE_TREE_ITEM);
            CHECK(old_child && focus(old_child) && visible_in(app, old_child) && action(old_child, "select"));
            until = now() + 3; while (now() < until && !state(old_child, ATSPI_STATE_SELECTED)) pump();
            CHECK(state(old_child, ATSPI_STATE_SELECTED));
        }
        if (step == 5) CHECK(retired(old_child) && !action(old_child, "select"));
        if (step == 6) {
            fresh_child = lookup(app, "Child B", ATSPI_ROLE_TREE_ITEM);
            CHECK(fresh_child && strcmp(ATSPI_OBJECT(fresh_child)->path, ATSPI_OBJECT(old_child)->path) &&
                state(fresh_child, ATSPI_STATE_SELECTED) && focus(fresh_child) && visible_in(app, fresh_child));
        }
        if (step == 7) CHECK(retired(fresh_child));
        if (step == 13) {
            AtspiAccessible *fresh = lookup(app, branch_name(), ATSPI_ROLE_TREE_ITEM); CHECK(fresh);
            int distinct = strcmp(ATSPI_OBJECT(fresh)->path, ATSPI_OBJECT(old_branch)->path) ||
                strcmp(ATSPI_OBJECT(fresh)->app->bus_name, ATSPI_OBJECT(old_branch)->app->bus_name);
            int focused = focus(fresh); g_object_unref(fresh); CHECK(distinct && focused);
        }
        if (step != 2 && step != 4 && step != 6) {
            AtspiAccessible *branch = lookup(app, branch_name(), ATSPI_ROLE_TREE_ITEM);
            int focused = branch && state(branch, ATSPI_STATE_FOCUSED); if (branch) g_object_unref(branch); CHECK(focused);
        }
        if (step == 16) {
            AtspiAccessible *refresh = lookup(app, refresh_name(), ATSPI_ROLE_PUSH_BUTTON);
            int focused = refresh && focus(refresh) && visible_in(app, refresh); if (refresh) g_object_unref(refresh); CHECK(focused);
        }
        printf("ACK %u\n", step);
    }
    puts("ASYNC_NATIVE_VERIFIED"); okay = 1;
cleanup:
    if (fresh_child) g_object_unref(fresh_child);
    if (old_child) g_object_unref(old_child);
    if (old_retry) g_object_unref(old_retry);
    if (old_branch) g_object_unref(old_branch);
    if (app) g_object_unref(app);
    if (desktop) g_object_unref(desktop);
    if (initialized) atspi_exit();
    return okay ? 0 : 1;
}
