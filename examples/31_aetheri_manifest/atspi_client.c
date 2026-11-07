/* Reuse only the independent OS-peer transport helpers, never toolkit objects. */
#define main async_example_peer_main
#include "../30_async_hierarchy/atspi_client.c"
#undef main

static char paths[32][257];
static int load_paths(const char *file) {
    FILE *f = fopen(file, "rb"); if (!f) return 0;
    char line[259]; size_t count = 0; int okay = 1;
    while (fgets(line, sizeof line, f)) {
        size_t n = strlen(line);
        if (count == 32 || n < 2 || line[n - 1] != '\n' || n > 257) { okay = 0; break; }
        line[--n] = 0; memcpy(paths[count++], line, n + 1);
    }
    if (ferror(f) || count != 32 || strlen(paths[31]) != 256) okay = 0;
    if (fclose(f)) okay = 0;
    return okay;
}
static int collect(AtspiAccessible *node, unsigned depth, unsigned *seen, unsigned *nodes) {
    if (!node || depth > 16 || ++*nodes > 512) return 0;
    atspi_accessible_clear_cache(node); GError *error = NULL;
    AtspiRole role = atspi_accessible_get_role(node, &error);
    char *name = !error ? atspi_accessible_get_name(node, &error) : NULL;
    int okay = !error && name;
    if (okay && role == ATSPI_ROLE_TREE_ITEM && strcmp(name, branch_name())) {
        size_t i = 0; while (i < 32 && strcmp(name, paths[i])) ++i;
        if (i == 32 || (*seen & (1u << i))) okay = 0;
        else *seen |= 1u << i;
    }
    g_free(name);
    int count = okay ? atspi_accessible_get_child_count(node, &error) : 0;
    okay = okay && !error && count >= 0 && count <= 128;
    for (int i = 0; okay && i < count; ++i) {
        AtspiAccessible *child = atspi_accessible_get_child_at_index(node, i, &error);
        okay = !error && child && collect(child, depth + 1, seen, nodes);
        if (child) g_object_unref(child);
    }
    if (error) g_error_free(error);
    return okay;
}
static int phase(AtspiAccessible *app, int value, int open) {
    AtspiAccessible *branch = lookup(app, branch_name(), ATSPI_ROLE_TREE_ITEM);
    int okay = branch && state(branch, ATSPI_STATE_EXPANDED) == open;
    if (branch) g_object_unref(branch);
    AtspiAccessible *status = lookup(app, status_name(value), ATSPI_ROLE_STATUS_BAR);
    if (open) okay = okay && status && state(status, ATSPI_STATE_BUSY) == (value == 1);
    else okay = okay && !status && !scan_failed;
    if (status) g_object_unref(status);
    AtspiAccessible *retry = lookup(app, retry_name(), ATSPI_ROLE_PUSH_BUTTON);
    okay = okay && ((open && value == 2) ? retry != NULL : !retry && !scan_failed);
    if (retry) g_object_unref(retry);
    unsigned seen = 0, nodes = 0;
    okay = okay && collect(app, 0, &seen, &nodes) && seen == (open && value == 3 ? 0xffffffffu : 0u);
    return okay;
}
int main(int argc, char **argv) {
    int okay = 0, initialized = 0;
    AtspiAccessible *desktop = NULL, *app = NULL, *branch = NULL, *old_retry = NULL, *last = NULL, *old_last = NULL;
    CHECK(argc >= 3 && argc <= 5 && load_paths(argv[2]));
    int automatic = 0;
    for (int i = 3; i < argc; ++i) {
        if (!strcmp(argv[i], "--german")) german = 1;
        else if (!strcmp(argv[i], "--automatic")) automatic = 1;
        else CHECK(0);
    }
    char *end; unsigned long pid = strtoul(argv[1], &end, 10); CHECK(pid && !*end && pid <= 2147483647UL);
    CHECK(atspi_init() == 0); initialized = 1; atspi_set_timeout(1000, 1000);
    desktop = atspi_get_desktop(0); CHECK(desktop);
    double until = now() + 4;
    while (!app && now() < until) { app = target(desktop, pid); if (!app) pump(); }
    CHECK(app); branch = lookup(app, branch_name(), ATSPI_ROLE_TREE_ITEM); CHECK(branch && focus(branch));
    setvbuf(stdout, NULL, _IONBF, 0);
    const int phases[11] = {1,2,1,3,1,0,1,3,3,0,1};
    for (unsigned step = 0; step < (automatic ? 1u : 11u); ++step) {
        printf("STEP %u\n", step); char line[64]; unsigned actual = 999;
        CHECK(fgets(line, sizeof line, stdin) && sscanf(line, "CHECK %u", &actual) == 1 && actual == step);
        if (step == 9) {
            CHECK(retired(branch) && !action(branch, "expand") && retired(last) && !action(last, "select"));
            g_object_unref(app); app = NULL; until = now() + 4;
            while (!app && now() < until) { app = target(desktop, pid); if (!app) pump(); }
            CHECK(app);
        }
        int matched = 0, open = step != 5 && step != 9;
        until = now() + 4;
        while (!matched && now() < until) { pump(); matched = phase(app, automatic ? 3 : phases[step], open); }
        if (!matched) fprintf(stderr, "manifest native step %u did not converge\n", step);
        CHECK(matched);
        if (step == 1) {
            old_retry = lookup(app, retry_name(), ATSPI_ROLE_PUSH_BUTTON);
            CHECK(old_retry && focus(old_retry) && visible_in(app, old_retry));
        }
        if (step == 2) CHECK(retired(old_retry) && !action(old_retry, "click"));
        if (step == 3 || step == 7 || automatic) {
            AtspiAccessible *item = lookup(app, paths[31], ATSPI_ROLE_TREE_ITEM);
            CHECK(item && focus(item) && visible_in(app, item));
            if (step == 3 || automatic) {
                CHECK(action(item, "select"));
                until = now() + 3; while (now() < until && !state(item, ATSPI_STATE_SELECTED)) pump();
                CHECK(state(item, ATSPI_STATE_SELECTED)); old_last = item;
            } else {
                CHECK(state(item, ATSPI_STATE_SELECTED) && strcmp(ATSPI_OBJECT(item)->path, ATSPI_OBJECT(old_last)->path));
                last = item;
            }
        }
        if (step == 4) CHECK(retired(old_last) && !action(old_last, "select"));
        if (step == 8) CHECK(state(last, ATSPI_STATE_SELECTED) && state(last, ATSPI_STATE_FOCUSED));
        if (step == 9) {
            AtspiAccessible *fresh = lookup(app, branch_name(), ATSPI_ROLE_TREE_ITEM);
            CHECK(fresh && focus(fresh)); g_object_unref(fresh);
        }
        printf("ACK %u\n", step);
    }
    puts("MANIFEST_NATIVE_VERIFIED"); okay = 1;
cleanup:
    if (last) g_object_unref(last);
    if (old_last) g_object_unref(old_last);
    if (old_retry) g_object_unref(old_retry);
    if (branch) g_object_unref(branch);
    if (app) g_object_unref(app);
    if (desktop) g_object_unref(desktop);
    if (initialized) atspi_exit();
    return okay ? 0 : 1;
}
