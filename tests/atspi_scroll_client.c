#define _POSIX_C_SOURCE 200809L
/* Independent libatspi client; does not include or link NexaUI. */
#include <atspi/atspi.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "scroll-atspi:%d: %s\n", __LINE__, #x); goto cleanup; } } while (0)
static void pause_ms(void) { struct timespec t = {0, 10000000}; (void)nanosleep(&t, NULL); }
static AtspiAccessible *find(AtspiAccessible *node, const char *name, unsigned depth) {
    if (!node || depth > 20) return NULL;
    GError *error = NULL; char *text = atspi_accessible_get_name(node, &error);
    int match = !error && text && !strcmp(text, name);
    if (error) g_error_free(error);
    g_free(text);
    if (match) {
        error = NULL; AtspiRole role = atspi_accessible_get_role(node, &error);
        match = !error && role != ATSPI_ROLE_APPLICATION && role != ATSPI_ROLE_FRAME;
        if (error) g_error_free(error);
    }
    if (match) return g_object_ref(node);
    error = NULL; int count = atspi_accessible_get_child_count(node, &error);
    if (error) { g_error_free(error); return NULL; }
    for (int i = 0; i < count; ++i) {
        AtspiAccessible *child = atspi_accessible_get_child_at_index(node, i, &error);
        if (error) { g_error_free(error); error = NULL; continue; }
        AtspiAccessible *found = find(child, name, depth + 1);
        if (child) g_object_unref(child);
        if (found) return found;
    }
    return NULL;
}
static AtspiAccessible *by_pid(AtspiAccessible *desktop, pid_t pid) {
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
static int percent(AtspiAccessible *node) {
    GError *error = NULL; atspi_accessible_clear_cache(node);
    GHashTable *attrs = atspi_accessible_get_attributes(node, &error);
    const char *text = attrs ? g_hash_table_lookup(attrs, "nexaui-value") : NULL;
    int result = -1;
    if (!error && text) { char *end = NULL; long n = strtol(text, &end, 10);
        if (end && !strcmp(end, "%") && n >= 0 && n <= 100) result = (int)n; }
    if (attrs) g_hash_table_unref(attrs);
    if (error) g_error_free(error);
    return result;
}
static int action_index(AtspiAction *action, const char *wanted) {
    GError *error = NULL; int count = atspi_action_get_n_actions(action, &error);
    if (error) { g_error_free(error); return -1; }
    for (int i = 0; i < count; ++i) {
        char *name = atspi_action_get_name(action, i, &error);
        if (error) { g_error_free(error); g_free(name); return -1; }
        int match = name && !strcmp(name, wanted); g_free(name);
        if (match) return i;
    }
    return -1;
}
static int wait_values(AtspiAccessible *horizontal, AtspiAccessible *vertical, int x, int y) {
    for (unsigned i = 0; i < 200; ++i) {
        while (g_main_context_iteration(NULL, FALSE)) {}
        if (percent(horizontal) == x && percent(vertical) == y) return 1;
        pause_ms();
    }
    fprintf(stderr, "Expected %d%%/%d%%, observed %d%%/%d%%\n", x, y, percent(horizontal), percent(vertical));
    return 0;
}
int main(int argc, char **argv) {
    int ok = 0, pipes[2] = {-1, -1}; pid_t child = -1;
    AtspiAccessible *desktop = NULL, *app = NULL, *horizontal = NULL, *vertical = NULL, *close_node = NULL;
    AtspiAction *ha = NULL, *va = NULL, *ca = NULL; GError *error = NULL;
    CHECK(argc == 2 && !pipe(pipes)); child = fork(); CHECK(child >= 0);
    if (!child) { close(pipes[0]); if (dup2(pipes[1], STDOUT_FILENO) < 0) _exit(126);
        close(pipes[1]); execl(argv[1], argv[1], (char *)NULL); _exit(127); }
    close(pipes[1]); pipes[1] = -1;
    CHECK(atspi_init() == 0); atspi_set_timeout(1000, 1000); desktop = atspi_get_desktop(0); CHECK(desktop);
    for (unsigned i = 0; i < 500 && (!horizontal || !vertical || !close_node); ++i) {
        while (g_main_context_iteration(NULL, FALSE)) {}
        if (!app) app = by_pid(desktop, child);
        if (app) { atspi_accessible_clear_cache(app);
            if (!horizontal) horizontal = find(app, "Horizontal scrollbar", 0);
            if (!vertical) vertical = find(app, "Vertical scrollbar", 0);
            if (!close_node) close_node = find(app, "Close", 0); }
        if (!horizontal || !vertical || !close_node) pause_ms();
    }
    CHECK(horizontal && vertical && close_node);
    CHECK(atspi_accessible_get_role(horizontal, &error) == ATSPI_ROLE_SLIDER && !error);
    CHECK(atspi_accessible_get_role(vertical, &error) == ATSPI_ROLE_SLIDER && !error);
    ha = atspi_accessible_get_action_iface(horizontal); va = atspi_accessible_get_action_iface(vertical);
    ca = atspi_accessible_get_action_iface(close_node); CHECK(ha && va && ca);
    int hi = action_index(ha, "increment"), hd = action_index(ha, "decrement");
    int vi = action_index(va, "increment"), vd = action_index(va, "decrement");
    CHECK(hi >= 0 && hd >= 0 && vi >= 0 && vd >= 0 && wait_values(horizontal, vertical, 0, 0));
    CHECK(atspi_action_do_action(ha, hi, &error) && !error && wait_values(horizontal, vertical, 10, 0));
    CHECK(atspi_action_do_action(va, vi, &error) && !error && wait_values(horizontal, vertical, 10, 10));
    CHECK(atspi_action_do_action(ha, hd, &error) && !error && wait_values(horizontal, vertical, 0, 10));
    CHECK(atspi_action_do_action(va, vd, &error) && !error && wait_values(horizontal, vertical, 0, 0));
    CHECK(atspi_action_get_n_actions(ca, &error) > 0 && !error);
    CHECK(atspi_action_do_action(ca, 0, &error) && !error);
    int status = 0; pid_t waited = 0;
    for (unsigned i = 0; i < 500 && !waited; ++i) { waited = waitpid(child, &status, WNOHANG); if (!waited) pause_ms(); }
    CHECK(waited == child && WIFEXITED(status) && WEXITSTATUS(status) == 0); child = -1;
    char output[128] = {0}; CHECK(read(pipes[0], output, sizeof(output) - 1) > 0);
    CHECK(strstr(output, "SCROLL_CLOSED 0 0\n")); ok = 1;
cleanup:
    if (child > 0) { (void)kill(child, SIGKILL); (void)waitpid(child, NULL, 0); }
    if (error) g_error_free(error);
    if (ha) g_object_unref(ha);
    if (va) g_object_unref(va);
    if (ca) g_object_unref(ca);
    if (horizontal) g_object_unref(horizontal);
    if (vertical) g_object_unref(vertical);
    if (close_node) g_object_unref(close_node);
    if (app) g_object_unref(app);
    if (desktop) g_object_unref(desktop);
    if (pipes[0] >= 0) close(pipes[0]);
    if (pipes[1] >= 0) close(pipes[1]);
    if (ok) puts("Native AT-SPI scroll: independent horizontal and vertical steps passed");
    return ok ? 0 : 1;
}
