#include "peer_helpers.h"
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
            CHECK(item && focus(item));
            int pages = 0, edge = read_edge(app, item, 0, &pages); CHECK(edge);
            if (edge == 2) {
                CHECK(pages > 0 && pages <= 64);
                printf("READ_TOP %u %d\n", step, pages);
                CHECK(fgets(line, sizeof(line), stdin) && sscanf(line, "READ %u", &actual) == 1 && actual == step);
                CHECK(read_edge(app, item, 1, NULL) == 2 && state(item, ATSPI_STATE_FOCUSED));
                printf("READ_BOTTOM %u\n", step);
            }
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
