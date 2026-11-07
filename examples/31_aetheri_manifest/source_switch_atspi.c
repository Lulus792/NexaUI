#include "peer_helpers.h"

static char expected_hashes[2][65], large_paths[32][257];
static const char *previous_name(void) { return german ? "Vorheriges Manifest" : "Previous manifest"; }
static const char *next_name(void) { return german ? "Nächstes Manifest" : "Next manifest"; }
static const char *metadata_name(void) { return german ? "Ausgewähltes Manifest: erwartete SHA-256" : "Selected manifest: expected SHA-256"; }
static int hash_file(const char *directory, const char *name, char value[65]) {
    char path[4096]; int n = snprintf(path, sizeof path, "%s/%s.sha256", directory, name);
    if (n < 1 || (size_t)n >= sizeof path) return 0;
    FILE *f = fopen(path, "rb"); if (!f) return 0;
    size_t length = fread(value, 1, 64, f); value[64] = 0;
    int closed = fclose(f); return length == 64 && !closed;
}
static void expect_source(unsigned source) {
    memcpy(paths, large_paths, sizeof paths); path_count = source ? 32 : 2;
    if (!source) { strcpy(paths[0], "bin/probe"); strcpy(paths[1], "share/message"); }
}
static int text_value(AtspiAccessible *node, const char *expected) {
    GError *error = NULL; AtspiText *iface = node ? atspi_accessible_get_text_iface(node) : NULL;
    char *value = iface ? atspi_text_get_text(iface, 0, -1, &error) : NULL;
    int okay = !error && value && !strcmp(value, expected);
    g_free(value); if (error) g_error_free(error); if (iface) g_object_unref(iface); return okay;
}
static int old_text_refused(AtspiText *text) {
    GError *error = NULL; char *value = atspi_text_get_text(text, 0, -1, &error); g_free(value);
    int refused = error != NULL; if (error) g_error_free(error); return refused;
}
static int source_snapshot(AtspiAccessible *app, unsigned source, int wanted_phase) {
    expect_source(source);
    if (!phase(app, wanted_phase, 1)) return 0;
    AtspiAccessible *previous = lookup(app, previous_name(), ATSPI_ROLE_PUSH_BUTTON);
    AtspiAccessible *next = lookup(app, next_name(), ATSPI_ROLE_PUSH_BUTTON);
    AtspiAccessible *meta = lookup(app, metadata_name(), ATSPI_ROLE_LABEL);
    int okay = previous && next && state(previous, ATSPI_STATE_ENABLED) == (source == 1) &&
        state(next, ATSPI_STATE_ENABLED) == (source == 0) && text_value(meta, expected_hashes[source]);
    if (previous) g_object_unref(previous);
    if (next) g_object_unref(next);
    if (meta) g_object_unref(meta);
    return okay;
}
static int header_visible(AtspiAccessible *app, AtspiAccessible *node) {
    AtspiAccessible *root = lookup(app, "Aetheri manifest declared file paths", ATSPI_ROLE_PANEL);
    AtspiComponent *a = node ? atspi_accessible_get_component_iface(node) : NULL;
    AtspiComponent *b = root ? atspi_accessible_get_component_iface(root) : NULL;
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
    if (root) g_object_unref(root);
    return okay;
}
static int read_last(AtspiAccessible *app, AtspiAccessible *node, unsigned step) {
    if (!node || !focus(node)) return 0;
    int pages = 0, edge = read_edge(app, node, 0, &pages); if (!edge) return 0;
    if (edge == 2) {
        if (pages < 1 || pages > 64) return 0;
        printf("READ_TOP %u %d\n", step, pages); char line[64]; unsigned actual = 999;
        if (!fgets(line, sizeof line, stdin) || sscanf(line, "READ %u", &actual) != 1 || actual != step ||
            read_edge(app, node, 1, NULL) != 2 || !state(node, ATSPI_STATE_FOCUSED)) return 0;
        printf("READ_BOTTOM %u\n", step);
    }
    return 1;
}
static int select_wait(AtspiAccessible *node) {
    if (!action(node, "select")) return 0;
    double until = now() + 3;
    while (now() < until && !state(node, ATSPI_STATE_SELECTED)) pump();
    return state(node, ATSPI_STATE_SELECTED);
}
int main(int argc, char **argv) {
    int okay = 0, initialized = 0, automatic = 0;
    AtspiAccessible *desktop = NULL, *app = NULL, *old_next = NULL, *old_branch = NULL;
    AtspiAccessible *old_first_b = NULL, *a_leaf = NULL;
    AtspiAccessible *metas[3] = {0}; AtspiText *texts[3] = {0};
    CHECK(argc >= 4 && argc <= 6 && load_paths(argv[2]) && hash_file(argv[3], "frozen", expected_hashes[0]) &&
        hash_file(argv[3], "maximum", expected_hashes[1]));
    memcpy(large_paths, paths, sizeof paths);
    for (int i = 4; i < argc; ++i) {
        if (!strcmp(argv[i], "--german")) german = 1;
        else if (!strcmp(argv[i], "--automatic")) automatic = 1;
        else CHECK(0);
    }
    char *end; unsigned long pid = strtoul(argv[1], &end, 10); CHECK(pid && !*end && pid <= 2147483647UL);
    CHECK(atspi_init() == 0); initialized = 1; atspi_set_timeout(1000, 1000);
    desktop = atspi_get_desktop(0); CHECK(desktop); double until = now() + 4;
    while (!app && now() < until) { app = target(desktop, pid); if (!app) pump(); }
    CHECK(app); old_branch = lookup(app, branch_name(), ATSPI_ROLE_TREE_ITEM); CHECK(old_branch && focus(old_branch));
    setvbuf(stdout, NULL, _IONBF, 0);
    const unsigned sources[9] = {0,1,1,1,0,0,1,1,1};
    const int phases[9] = {1,1,3,3,1,3,1,3,1};
    for (unsigned step = 0; step < 9; ++step) {
        printf("STEP %u\n", step); char line[64]; unsigned actual = 999;
        CHECK(fgets(line, sizeof line, stdin) && sscanf(line, "CHECK %u", &actual) == 1 && actual == step);
        if (step == 4 || step == 6 || step == 8) {
            AtspiAccessible *button = lookup(app, step == 4 ? previous_name() : step == 6 ? next_name() : refresh_name(), ATSPI_ROLE_PUSH_BUTTON);
            CHECK(button && action(button, "click")); g_object_unref(button);
        }
        int matched = 0; until = now() + 4;
        while (!matched && now() < until) {
            pump();
            if (step == 1 || step == 4 || step == 6) {
                AtspiAccessible *fresh = target(desktop, pid);
                if (fresh) { g_object_unref(app); app = fresh; }
            }
            matched = source_snapshot(app, sources[step], automatic ? 3 : phases[step]);
        }
        if (!matched) fprintf(stderr, "source native step %u did not converge\n", step);
        CHECK(matched);
        if (step == 0 || step == 2 || step == 5) {
            unsigned slot = step == 0 ? 0 : step == 2 ? 1 : 2;
            metas[slot] = lookup(app, metadata_name(), ATSPI_ROLE_LABEL); CHECK(metas[slot]);
            texts[slot] = atspi_accessible_get_text_iface(metas[slot]); CHECK(texts[slot]);
        }
        if (step == 0) { old_next = lookup(app, next_name(), ATSPI_ROLE_PUSH_BUTTON); CHECK(old_next); }
        if (step == 1) CHECK(retired(old_branch) && retired(old_next) && !action(old_next, "click") &&
            retired(metas[0]) && old_text_refused(texts[0]));
        if (step == 1 || step == 4 || step == 6) {
            AtspiAccessible *button = lookup(app, sources[step] ? previous_name() : next_name(), ATSPI_ROLE_PUSH_BUTTON);
            CHECK(button && state(button, ATSPI_STATE_FOCUSED) && header_visible(app, button));
            g_object_unref(button);
        }
        if (step == 2) {
            old_first_b = lookup(app, paths[0], ATSPI_ROLE_TREE_ITEM); CHECK(old_first_b);
            AtspiAccessible *last = lookup(app, paths[31], ATSPI_ROLE_TREE_ITEM); CHECK(read_last(app, last, step) && select_wait(last));
            g_object_unref(last);
        }
        if (step == 3) {
            CHECK(retired(old_next) && !action(old_next, "click"));
            AtspiAccessible *last = lookup(app, paths[31], ATSPI_ROLE_TREE_ITEM);
            CHECK(last && state(last, ATSPI_STATE_SELECTED) && state(last, ATSPI_STATE_FOCUSED)); g_object_unref(last);
        }
        if (step == 4) CHECK(retired(old_first_b) && !action(old_first_b, "select") && retired(metas[1]) && old_text_refused(texts[1]));
        if (step == 5) {
            a_leaf = lookup(app, "share/message", ATSPI_ROLE_TREE_ITEM); CHECK(a_leaf && read_last(app, a_leaf, step) && select_wait(a_leaf));
        }
        if (step == 6) CHECK(retired(a_leaf) && !action(a_leaf, "select") && retired(metas[2]) && old_text_refused(texts[2]));
        if (step == 7) {
            AtspiAccessible *fresh = lookup(app, paths[0], ATSPI_ROLE_TREE_ITEM);
            CHECK(fresh && strcmp(ATSPI_OBJECT(fresh)->path, ATSPI_OBJECT(old_first_b)->path) &&
                retired(old_first_b) && !action(old_first_b, "select") && focus(fresh) && visible_in(app, fresh));
            g_object_unref(fresh);
            AtspiAccessible *next = lookup(app, next_name(), ATSPI_ROLE_PUSH_BUTTON); CHECK(next && !action(next, "click")); g_object_unref(next);
        }
        printf("ACK %u\n", step);
    }
    puts("SOURCE_SWITCH_NATIVE_VERIFIED"); okay = 1;
cleanup:
    for (size_t i = 0; i < 3; ++i) { if (texts[i]) g_object_unref(texts[i]); if (metas[i]) g_object_unref(metas[i]); }
    if (a_leaf) g_object_unref(a_leaf);
    if (old_first_b) g_object_unref(old_first_b);
    if (old_next) g_object_unref(old_next);
    if (old_branch) g_object_unref(old_branch);
    if (app) g_object_unref(app);
    if (desktop) g_object_unref(desktop);
    if (initialized) atspi_exit();
    return okay ? 0 : 1;
}
