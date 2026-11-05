#define _POSIX_C_SOURCE 200809L
/* Independent AT-SPI consumer. The host owns the synthetic list and selection. */
#include <atspi/atspi.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "virtual-client:%d: %s\n", __LINE__, #x); goto cleanup; } } while (0)
static void pause_ms(void) { struct timespec t = {0, 10000000}; (void)nanosleep(&t, NULL); }
static AtspiAccessible *find(AtspiAccessible *node, const char *name, unsigned depth) {
    if (!node || depth > 10) return NULL;
    GError *error = NULL;
    atspi_accessible_clear_cache(node);
    char *text = atspi_accessible_get_name(node, &error);
    int match = !error && text && !strcmp(text, name);
    if (error) g_error_free(error);
    g_free(text);
    if (match) return g_object_ref(node);
    error = NULL;
    int count = atspi_accessible_get_child_count(node, &error);
    if (error) { g_error_free(error); return NULL; }
    for (int i = 0; i < count; ++i) {
        AtspiAccessible *child = atspi_accessible_get_child_at_index(node, i, &error);
        if (error) { g_error_free(error); error = NULL; continue; }
        AtspiAccessible *result = find(child, name, depth + 1);
        if (child) g_object_unref(child);
        if (result) return result;
    }
    return NULL;
}
static AtspiAccessible *app_by_pid(AtspiAccessible *desktop, pid_t pid) {
    GError *error = NULL; atspi_accessible_clear_cache(desktop);
    int count = atspi_accessible_get_child_count(desktop, &error);
    if (error) { g_error_free(error); return NULL; }
    for (int i = 0; i < count; ++i) {
        AtspiAccessible *app = atspi_accessible_get_child_at_index(desktop, i, &error);
        if (error) { g_error_free(error); error = NULL; continue; }
        if (!app) continue;
        guint actual = atspi_accessible_get_process_id(app, &error);
        if (!error && actual == (guint)pid) return app;
        if (error) { g_error_free(error); error = NULL; }
        g_object_unref(app);
    }
    return NULL;
}
static int selected(AtspiAccessible *node) {
    atspi_accessible_clear_cache(node);
    AtspiStateSet *states = atspi_accessible_get_state_set(node);
    if (!states) return 0;
    int result = atspi_state_set_contains(states, ATSPI_STATE_SELECTED);
    g_object_unref(states); return result;
}
static int absent(AtspiAccessible *node) {
    GError *error = NULL; atspi_accessible_clear_cache(node);
    char *name = atspi_accessible_get_name(node, &error);
    int result = error != NULL;
    if (error) g_error_free(error);
    g_free(name); return result;
}
static int bounded(AtspiAccessible *tree) {
    GError *error = NULL; atspi_accessible_clear_cache(tree);
    int count = atspi_accessible_get_child_count(tree, &error);
    int okay = !error && count > 0 && count <= 21;
    if (error) g_error_free(error);
    return okay;
}
int main(int argc, char **argv) {
    int okay = 0; pid_t child = -1; GError *error = NULL;
    AtspiAccessible *desktop = NULL, *app = NULL, *tree = NULL, *old = NULL, *anchor = NULL, *removed = NULL;
    AtspiAction *action = NULL;
    CHECK(argc == 2); child = fork(); CHECK(child >= 0);
    if (!child) { execl(argv[1], argv[1], (char *)NULL); _exit(127); }
    CHECK(atspi_init() == 0); atspi_set_timeout(1000, 1000);
    desktop = atspi_get_desktop(0); CHECK(desktop);
    for (unsigned i = 0; i < 500 && !old; ++i) {
        while (g_main_context_iteration(NULL, FALSE)) {}
        if (!app) app = app_by_pid(desktop, child);
        if (app) old = find(app, "file-000000", 0);
        if (!old) pause_ms();
    }
    CHECK(old && atspi_accessible_get_role(old, &error) == ATSPI_ROLE_TREE_ITEM && !error);
    tree = atspi_accessible_get_parent(old, &error); CHECK(tree && !error && bounded(tree));
    CHECK(kill(child, SIGUSR1) == 0);
    for (unsigned i = 0; i < 500 && (!anchor || !removed); ++i) {
        while (g_main_context_iteration(NULL, FALSE)) {}
        if (!anchor) anchor = find(tree, "file-050000", 0);
        if (!removed) removed = find(tree, "file-050001", 0);
        if (!anchor || !removed) pause_ms();
    }
    CHECK(anchor && removed && bounded(tree) && absent(old));
    action = atspi_accessible_get_action_iface(anchor); CHECK(action);
    int count = atspi_action_get_n_actions(action, &error), index = -1; CHECK(!error);
    for (int i = 0; i < count; ++i) {
        char *name = atspi_action_get_name(action, i, &error); CHECK(!error);
        if (name && !strcmp(name, "select")) index = i;
        g_free(name);
    }
    CHECK(index >= 0 && atspi_action_do_action(action, index, &error) && !error);
    for (unsigned i = 0; i < 500 && !selected(anchor); ++i) { while (g_main_context_iteration(NULL, FALSE)) {} pause_ms(); }
    CHECK(selected(anchor) && kill(child, SIGUSR2) == 0);
    for (unsigned i = 0; i < 500 && !absent(removed); ++i) { while (g_main_context_iteration(NULL, FALSE)) {} pause_ms(); }
    CHECK(absent(removed) && bounded(tree));
    atspi_accessible_clear_cache(anchor);
    char *name = atspi_accessible_get_name(anchor, &error);
    int retained = !error && name && !strcmp(name, "file-050000"); g_free(name);
    CHECK(retained && selected(anchor));
    AtspiAccessible *next = find(tree, "file-050002", 0); CHECK(next); g_object_unref(next);
    CHECK(kill(child, SIGTERM) == 0);
    int status = 0; pid_t waited = 0;
    for (unsigned i = 0; i < 500 && !waited; ++i) { waited = waitpid(child, &status, WNOHANG); if (!waited) pause_ms(); }
    CHECK(waited == child && WIFEXITED(status) && WEXITSTATUS(status) == 0); child = -1; okay = 1;
cleanup:
    if (child > 0) { (void)kill(child, SIGKILL); (void)waitpid(child, NULL, 0); }
    if (error) g_error_free(error);
    if (action) g_object_unref(action);
    if (removed) g_object_unref(removed);
    if (anchor) g_object_unref(anchor);
    if (old) g_object_unref(old);
    if (tree) g_object_unref(tree);
    if (app) g_object_unref(app);
    if (desktop) g_object_unref(desktop);
    if (okay) puts("Independent AT-SPI virtual 100k: scroll, selection, filter and stale references passed");
    return okay ? 0 : 1;
}
