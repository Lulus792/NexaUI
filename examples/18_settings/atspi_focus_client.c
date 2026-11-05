#define _POSIX_C_SOURCE 200809L
/* Independent accessibility client; deliberately has no NexaUI linkage. */
#include <atspi/atspi.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static AtspiAccessible *find_name(AtspiAccessible *node, const char *name, unsigned depth) {
    if (!node || depth > 16) return NULL;
    GError *error = NULL;
    char *actual = atspi_accessible_get_name(node, &error);
    int found = !error && actual && !strcmp(actual, name);
    if (error) g_error_free(error);
    g_free(actual);
    if (found) return g_object_ref(node);
    error = NULL;
    int count = atspi_accessible_get_child_count(node, &error);
    if (error) { g_error_free(error); return NULL; }
    for (int i = 0; i < count; ++i) {
        AtspiAccessible *child = atspi_accessible_get_child_at_index(node, i, &error);
        if (error) { g_error_free(error); error = NULL; continue; }
        AtspiAccessible *result = find_name(child, name, depth + 1);
        if (child) g_object_unref(child);
        if (result) return result;
    }
    return NULL;
}

int main(int argc, char **argv) {
    if (argc != 3) return 2;
    char *end = NULL;
    errno = 0;
    unsigned long pid = strtoul(argv[1], &end, 10);
    if (errno || !end || *end || !pid || pid > UINT32_MAX) return 2;
    if (atspi_init() != 0) return 1;
    atspi_set_timeout(1000, 1000);
    AtspiAccessible *desktop = atspi_get_desktop(0), *target = NULL;
    if (!desktop) { atspi_exit(); return 1; }
    for (unsigned attempt = 0; attempt < 150 && !target; ++attempt) {
        while (g_main_context_iteration(NULL, FALSE)) {}
        atspi_accessible_clear_cache(desktop);
        GError *error = NULL;
        int count = atspi_accessible_get_child_count(desktop, &error);
        if (error) { g_error_free(error); error = NULL; count = 0; }
        for (int i = 0; i < count && !target; ++i) {
            AtspiAccessible *app = atspi_accessible_get_child_at_index(desktop, i, &error);
            if (error) { g_error_free(error); error = NULL; continue; }
            if (!app) continue;
            guint actual = atspi_accessible_get_process_id(app, &error);
            if (!error && actual == (guint)pid) target = find_name(app, argv[2], 0);
            if (error) { g_error_free(error); error = NULL; }
            g_object_unref(app);
        }
        if (!target) { struct timespec delay = {0, 20000000}; nanosleep(&delay, NULL); }
    }
    int okay = 0;
    if (target) {
        AtspiComponent *component = atspi_accessible_get_component_iface(target);
        if (component) {
            GError *error = NULL;
            okay = atspi_component_grab_focus(component, &error) && !error;
            if (error) { fprintf(stderr, "GrabFocus: %s\n", error->message); g_error_free(error); }
            g_object_unref(component);
        }
        g_object_unref(target);
    }
    g_object_unref(desktop);
    atspi_exit();
    if (!okay) fprintf(stderr, "Could not focus '%s' in process %lu\n", argv[2], pid);
    return okay ? 0 : 1;
}
