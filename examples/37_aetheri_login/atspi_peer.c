#define _POSIX_C_SOURCE 200809L
#include <atspi/atspi.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "login AT-SPI probe line %d: %s\n", __LINE__, #x); goto done; } } while (0)

static void pump(void) {
    while (g_main_context_iteration(NULL, FALSE)) {}
    const struct timespec delay = {0, 10000000};
    nanosleep(&delay, NULL);
}

static int state(AtspiAccessible *node, AtspiStateType flag) {
    atspi_accessible_clear_cache(node);
    AtspiStateSet *states = atspi_accessible_get_state_set(node);
    int present = states && atspi_state_set_contains(states, flag);
    if (states) g_object_unref(states);
    return present;
}

static AtspiAccessible *find(AtspiAccessible *root, const char *name, unsigned depth) {
    if (!root || depth > 8) return NULL;
    atspi_accessible_clear_cache(root);
    char *current = atspi_accessible_get_name(root, NULL);
    int match = current && !strcmp(current, name);
    g_free(current);
    if (match) return g_object_ref(root);
    int count = atspi_accessible_get_child_count(root, NULL);
    if (count > 32) return NULL;
    for (int i = 0; i < count; ++i) {
        AtspiAccessible *child = atspi_accessible_get_child_at_index(root, i, NULL);
        AtspiAccessible *found = find(child, name, depth + 1);
        if (child) g_object_unref(child);
        if (found) return found;
    }
    return NULL;
}

static AtspiAccessible *application(AtspiAccessible *desktop, pid_t child) {
    for (unsigned i = 0; i < 200; ++i) {
        atspi_accessible_clear_cache(desktop);
        int count = atspi_accessible_get_child_count(desktop, NULL);
        if (count > 16) return NULL;
        for (int j = 0; j < count; ++j) {
            AtspiAccessible *candidate = atspi_accessible_get_child_at_index(desktop, j, NULL);
            if (!candidate) continue;
            if (atspi_accessible_get_process_id(candidate, NULL) == (guint)child) return candidate;
            g_object_unref(candidate);
        }
        pump();
    }
    return NULL;
}

static AtspiAccessible *focused(AtspiAccessible *app, const char *name) {
    for (unsigned i = 0; i < 200; ++i) {
        AtspiAccessible *node = find(app, name, 0);
        if (node && state(node, ATSPI_STATE_FOCUSED)) return node;
        if (node) g_object_unref(node);
        pump();
    }
    return NULL;
}

static int observe_focus(AtspiAccessible *app, const char *name) {
    AtspiAccessible *node = focused(app, name);
    if (!node) return 0;
    g_object_unref(node);
    return 1;
}

static int protected_field(AtspiAccessible *node) {
    GError *error = NULL;
    if (atspi_accessible_get_role(node, &error) != ATSPI_ROLE_PASSWORD_TEXT || error) {
        if (error) g_error_free(error);
        return 0;
    }
    GHashTable *attributes = atspi_accessible_get_attributes(node, &error);
    int hidden = attributes && !error && !g_hash_table_contains(attributes, "nexaui-value");
    if (error) g_error_free(error);
    if (attributes) g_hash_table_unref(attributes);
    AtspiText *text = atspi_accessible_get_text_iface(node);
    AtspiEditableText *edit = atspi_accessible_get_editable_text_iface(node);
    int protected = hidden && !text && !edit;
    if (text) g_object_unref(text);
    if (edit) g_object_unref(edit);
    return protected;
}

static int observe_value(AtspiAccessible *node, const char *expected) {
    for (unsigned i = 0; i < 200; ++i) {
        atspi_accessible_clear_cache(node);
        GHashTable *attributes = atspi_accessible_get_attributes(node, NULL);
        const char *value = attributes ? g_hash_table_lookup(attributes, "nexaui-value") : NULL;
        int good = value && !strcmp(value, expected);
        if (attributes) g_hash_table_unref(attributes);
        if (good) return 1;
        pump();
    }
    return 0;
}

static int x11_output(char id[32]) {
    const char *path = getenv("NEXAUI_TEST_WESTON_LOG");
    FILE *log = path ? fopen(path, "r") : NULL;
    if (!log) return 0;
    char line[1024];
    unsigned count = 0;
    while (fgets(line, sizeof(line), log)) {
        const char *label = strstr(line, "x11 output ");
        const char *window = label ? strstr(label, "window id ") : NULL;
        if (!window) continue;
        window += strlen("window id ");
        size_t length = strspn(window, "0123456789");
        if (!length || length >= 32) {
            count = 0;
            break;
        }
        memcpy(id, window, length);
        id[length] = 0;
        ++count;
    }
    fclose(log);
    return count == 1;
}

static int xdotool(const char *command, const char *value) {
    const char *driver = getenv("NEXAUI_LOGIN_TEST_XDOTOOL");
    if (!driver || !*driver) return 0;
    pid_t child = fork();
    if (!child) {
        if (!strcmp(command, "type"))
            execl(driver, driver, command, "--delay", "30", value, (char *)NULL);
        else execl(driver, driver, command, value, (char *)NULL);
        _exit(127);
    }
    if (child < 0) return 0;
    int status = 0;
    pid_t ended;
    do { ended = waitpid(child, &status, 0); } while (ended < 0 && errno == EINTR);
    return ended == child && WIFEXITED(status) && !WEXITSTATUS(status);
}

int main(int argc, char **argv) {
    const char *isolated = getenv("NEXAUI_TEST_PRIVATE_WAYLAND");
    const char *display = getenv("WAYLAND_DISPLAY");
    if (argc != 2 || !isolated || strcmp(isolated, "1") || !display ||
        strcmp(display, "nexaui-test") || !getenv("NEXAUI_TEST_ATSPI_ADDRESS")) return 2;
    AtspiAccessible *desktop = NULL, *app = NULL, *secret = NULL, *visible = NULL, *confirmed = NULL;
    pid_t child = -1;
    int result = 1, initialized = 0;
    char id[32];
    CHECK(x11_output(id));
    child = fork();
    CHECK(child >= 0);
    if (!child) {
        execl(argv[1], argv[1], (char *)NULL);
        _exit(127);
    }
    CHECK(atspi_init() == 0);
    initialized = 1;
    atspi_set_timeout(1000, 1000);
    desktop = atspi_get_desktop(0);
    CHECK(desktop && (app = application(desktop, child)));
    CHECK(xdotool("windowfocus", id));
    CHECK((secret = focused(app, "Secret response")));
    CHECK(protected_field(secret));
    CHECK(xdotool("key", "Tab") && observe_focus(app, "Answer"));
    CHECK(xdotool("key", "Tab") && observe_focus(app, "Cancel"));
    CHECK(xdotool("key", "shift+Tab") && observe_focus(app, "Answer"));
    CHECK(xdotool("key", "shift+Tab") && observe_focus(app, "Secret response"));
    CHECK(xdotool("type", "test") && xdotool("key", "Tab") && observe_focus(app, "Answer"));
    CHECK(xdotool("key", "shift+Tab") && observe_focus(app, "Secret response") && protected_field(secret));
    CHECK(xdotool("key", "Return") && (visible = focused(app, "Response")));
    CHECK(state(secret, ATSPI_STATE_DEFUNCT));
    CHECK(atspi_accessible_get_role(visible, NULL) == ATSPI_ROLE_ENTRY);
    CHECK(xdotool("type", "visible") && observe_value(visible, "visible"));
    CHECK(xdotool("key", "Return") && (confirmed = focused(app, "Start session")));
    CHECK(state(visible, ATSPI_STATE_DEFUNCT));
    CHECK(xdotool("key", "Escape"));
    int status = 0;
    pid_t ended = 0;
    for (unsigned i = 0; i < 300 && !ended; ++i) {
        ended = waitpid(child, &status, WNOHANG);
        if (!ended) pump();
    }
    CHECK(ended == child);
    child = -1;
    CHECK(WIFEXITED(status) && !WEXITSTATUS(status));
    puts("Native login: protected password, real keyboard, retired fields and close passed");
    result = 0;
done:
    if (child > 0) { kill(child, SIGKILL); waitpid(child, NULL, 0); }
    if (confirmed) g_object_unref(confirmed);
    if (visible) g_object_unref(visible);
    if (secret) g_object_unref(secret);
    if (app) g_object_unref(app);
    if (desktop) g_object_unref(desktop);
    if (initialized) atspi_exit();
    return result;
}
