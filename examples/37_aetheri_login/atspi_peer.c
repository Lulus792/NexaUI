#define _POSIX_C_SOURCE 200809L
#include <atspi/atspi.h>
#include <errno.h>
#include <fcntl.h>
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

static AtspiAccessible *find_kind(AtspiAccessible *root, const char *name, unsigned depth, int label_only) {
    if (!root || depth > 8) return NULL;
    atspi_accessible_clear_cache(root);
    char *current = atspi_accessible_get_name(root, NULL);
    int match = current && !strcmp(current, name);
    g_free(current);
    if (match && (!label_only || atspi_accessible_get_role(root, NULL) == ATSPI_ROLE_LABEL))
        return g_object_ref(root);
    int count = atspi_accessible_get_child_count(root, NULL);
    if (count > 32) return NULL;
    for (int i = 0; i < count; ++i) {
        AtspiAccessible *child = atspi_accessible_get_child_at_index(root, i, NULL);
        AtspiAccessible *found = find_kind(child, name, depth + 1, label_only);
        if (child) g_object_unref(child);
        if (found) return found;
    }
    return NULL;
}

static AtspiAccessible *find(AtspiAccessible *root, const char *name, unsigned depth) {
    return find_kind(root, name, depth, 0);
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

static int observe_present(AtspiAccessible *app, const char *name) {
    for (unsigned i = 0; i < 200; ++i) {
        AtspiAccessible *node = find(app, name, 0);
        if (node) { g_object_unref(node); return 1; }
        pump();
    }
    return 0;
}

static int focus_control(AtspiAccessible *app, const char *name) {
    AtspiAccessible *node = find(app, name, 0);
    AtspiComponent *component = node ? atspi_accessible_get_component_iface(node) : NULL;
    GError *error = NULL;
    int accepted = component && atspi_component_grab_focus(component, &error) && !error;
    if (error) g_error_free(error);
    if (component) g_object_unref(component);
    if (node) g_object_unref(node);
    return accepted && observe_focus(app, name);
}

static int press_control(AtspiAccessible *app, const char *name) {
    AtspiAccessible *node = find(app, name, 0);
    AtspiAction *action = node ? atspi_accessible_get_action_iface(node) : NULL;
    GError *error = NULL;
    int accepted = 0;
    int count = action ? atspi_action_get_n_actions(action, &error) : 0;
    for (int i = 0; !error && i < count && i < 16; ++i) {
        char *current = atspi_action_get_action_name(action, i, &error);
        int match = current && !strcmp(current, "click");
        g_free(current);
        if (match && !error) {
            accepted = atspi_action_do_action(action, i, &error) && !error;
            break;
        }
    }
    if (error) g_error_free(error);
    if (action) g_object_unref(action);
    if (node) g_object_unref(node);
    return accepted;
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

static int empty_text(AtspiAccessible *node) {
    AtspiText *text = atspi_accessible_get_text_iface(node);
    if (!text) return 0;
    GError *error = NULL;
    char *bytes = atspi_text_get_text(text, 0, -1, &error);
    int empty = bytes && !*bytes && !error;
    g_free(bytes);
    if (error) g_error_free(error);
    g_object_unref(text);
    return empty;
}

static int host_marker(FILE *log, const char *marker) {
    char text[16384];
    for (unsigned i = 0; i < 200; ++i) {
        ssize_t size = pread(fileno(log), text, sizeof(text) - 1, 0);
        if (size < 0 || (size_t)size == sizeof(text) - 1) return 0;
        text[size] = 0;
        if (strstr(text, marker)) return 1;
        pump();
    }
    return 0;
}

static int rejected_action(AtspiAction *action) {
    GError *error = NULL;
    int accepted = atspi_action_do_action(action, 0, &error);
    int rejected = !accepted || error != NULL;
    if (error) g_error_free(error);
    return rejected;
}

static int replacement_checkpoint(AtspiAccessible *app, FILE *log, unsigned index,
    AtspiAccessible *field, AtspiAccessible *answer, AtspiAction *action,
    AtspiAccessible **prior_sending, int control) {
    char marker[96];
    if (snprintf(marker, sizeof(marker), "LOGIN_FIXTURE_REPLACED sending=%u requests=%u peer_empty=1",
                 index, index - 1u) < 1 || !host_marker(log, marker) ||
        !observe_present(app, "Sending request.") || !state(field, ATSPI_STATE_DEFUNCT) ||
        !state(answer, ATSPI_STATE_DEFUNCT) || !rejected_action(action)) return 0;
    if (*prior_sending && !state(*prior_sending, ATSPI_STATE_DEFUNCT)) return 0;
    const char *absent[] = {"Answer", "Secret response", "Response"};
    for (unsigned i = 0; i < 3; ++i) {
        AtspiAccessible *node = find(app, absent[i], 0);
        if (node) { g_object_unref(node); return 0; }
    }
    if (*prior_sending) g_object_unref(*prior_sending);
    *prior_sending = find_kind(app, "Sending request.", 0, 1);
    return *prior_sending && write(control, "R", 1) == 1;
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
    AtspiAccessible *old_answer = NULL, *prior_sending = NULL;
    AtspiAction *old_action = NULL;
    pid_t child = -1;
    int result = 1, initialized = 0;
    int ime = getenv("NEXAUI_LOGIN_TEST_SIMPLE_IME") != NULL;
    int cancel = getenv("NEXAUI_LOGIN_TEST_IME_CANCEL") != NULL;
    FILE *host_log = NULL;
    int controls[2] = {-1, -1};
    const char *loss = getenv("NEXAUI_LOGIN_TEST_CONNECTION_LOSS");
    int replace = getenv("NEXAUI_LOGIN_TEST_REPLACE_VIEW") != NULL;
    char id[32];
    CHECK(x11_output(id));
    CHECK((host_log = tmpfile()));
    if (loss || replace) {
        CHECK(getenv("NEXAUI_LOGIN_TEST_CONNECTION_LOOP"));
        if (loss) {
            CHECK(ime && !cancel && !replace);
            CHECK(!strcmp(loss, "eof") || !strcmp(loss, "admission") || !strcmp(loss, "timeout") || !strcmp(loss, "peer-exit"));
        }
        CHECK(pipe(controls) == 0);
        for (unsigned i = 0; i < 2; ++i) {
            CHECK(fcntl(controls[i], F_SETFD, FD_CLOEXEC) == 0);
            int flags = fcntl(controls[i], F_GETFL);
            CHECK(flags >= 0 && fcntl(controls[i], F_SETFL, flags | O_NONBLOCK) == 0);
        }
    }
    child = fork();
    CHECK(child >= 0);
    if (!child) {
        if (loss || replace) {
            char descriptor[32];
            close(controls[1]);
            if (snprintf(descriptor, sizeof(descriptor), "%d", controls[0]) < 1 ||
                fcntl(controls[0], F_SETFD, 0) ||
                setenv("NEXAUI_LOGIN_TEST_CONTROL_FD", descriptor, 1)) _exit(126);
        }
        if (dup2(fileno(host_log), STDOUT_FILENO) < 0 ||
            dup2(fileno(host_log), STDERR_FILENO) < 0) _exit(126);
        execl(argv[1], argv[1], (char *)NULL);
        _exit(127);
    }
    if (controls[0] >= 0) { close(controls[0]); controls[0] = -1; }
    CHECK(atspi_init() == 0);
    initialized = 1;
    atspi_set_timeout(1000, 1000);
    desktop = atspi_get_desktop(0);
    CHECK(desktop && (app = application(desktop, child)));
    CHECK(xdotool("windowfocus", id));
    CHECK((secret = focused(app, "Secret response")));
    CHECK(protected_field(secret));
    if (ime) CHECK(focus_control(app, "Answer"));
    else CHECK(xdotool("key", "Tab") && observe_focus(app, "Answer"));
    CHECK(xdotool("key", "Tab") && observe_focus(app, "Cancel"));
    CHECK(xdotool("key", "shift+Tab") && observe_focus(app, "Answer"));
    CHECK(xdotool("key", "shift+Tab") && observe_focus(app, "Secret response"));
    if (ime) {
        CHECK(xdotool("key", "108") && xdotool("key", "apostrophe"));
        CHECK(host_marker(host_log, "LOGIN_FIXTURE_PREEDIT secret requests=0"));
        CHECK(protected_field(secret));
        if (loss) {
            if (!strcmp(loss, "timeout")) {
                CHECK(xdotool("key", "a"));
                CHECK(host_marker(host_log, "LOGIN_FIXTURE_PREEDIT_CLEARED secret requests=0"));
                CHECK(press_control(app, "Answer"));
            } else {
                const char command = !strcmp(loss, "eof") ? 'E' : !strcmp(loss, "peer-exit") ? 'D' : 'A';
                CHECK(write(controls[1], &command, 1) == 1);
            }
            CHECK(observe_present(app, "Login state unknown. Close this view to leave the conversation."));
            CHECK(host_marker(host_log, "LOGIN_FIXTURE_CONNECTION_UNKNOWN"));
            if (!strcmp(loss, "peer-exit"))
                CHECK(host_marker(host_log, "LOGIN_FIXTURE_PROCESS_EXIT pidfd_readable=1 socket_eof=0 child_exit=0"));
            CHECK(state(secret, ATSPI_STATE_DEFUNCT));
            AtspiAccessible *answer = find(app, "Answer", 0);
            int missing = answer == NULL;
            if (answer) g_object_unref(answer);
            CHECK(missing);
            CHECK(xdotool("key", "Escape"));
            goto window_closed;
        }
        if (cancel) {
            CHECK(focus_control(app, "Answer"));
            CHECK(host_marker(host_log, "LOGIN_FIXTURE_PREEDIT_CLEARED secret requests=0"));
            CHECK(xdotool("key", "shift+Tab") && observe_focus(app, "Secret response"));
        }
        CHECK(xdotool("key", "a"));
        CHECK(host_marker(host_log, "LOGIN_FIXTURE_PREEDIT_CLEARED secret requests=0"));
        CHECK(protected_field(secret));
    } else {
        CHECK(xdotool("type", "test") && xdotool("key", "Tab") && observe_focus(app, "Answer"));
        CHECK(xdotool("key", "shift+Tab") && observe_focus(app, "Secret response") && protected_field(secret));
    }
    if (replace) {
        CHECK((old_answer = find(app, "Answer", 0)));
        CHECK((old_action = atspi_accessible_get_action_iface(old_answer)));
    }
    CHECK(ime ? press_control(app, "Answer") : xdotool("key", "Return"));
    if (replace) {
        CHECK(replacement_checkpoint(app, host_log, 1, secret, old_answer, old_action, &prior_sending, controls[1]));
        g_object_unref(old_action); old_action = NULL;
        g_object_unref(old_answer); old_answer = NULL;
    }
    CHECK((visible = focused(app, "Response")));
    CHECK(state(secret, ATSPI_STATE_DEFUNCT));
    CHECK(atspi_accessible_get_role(visible, NULL) == ATSPI_ROLE_ENTRY);
    if (ime) {
        CHECK(xdotool("key", "108") && xdotool("key", "apostrophe"));
        CHECK(host_marker(host_log, "LOGIN_FIXTURE_PREEDIT visible requests=1"));
        CHECK(empty_text(visible));
        CHECK(xdotool("key", "a") && observe_value(visible, "\xc3\xa1"));
        CHECK(host_marker(host_log, "LOGIN_FIXTURE_PREEDIT_CLEARED visible requests=1"));
    } else CHECK(xdotool("type", "visible") && observe_value(visible, "visible"));
    if (replace) {
        CHECK((old_answer = find(app, "Answer", 0)));
        CHECK((old_action = atspi_accessible_get_action_iface(old_answer)));
    }
    CHECK(ime ? press_control(app, "Answer") : xdotool("key", "Return"));
    if (replace) {
        CHECK(replacement_checkpoint(app, host_log, 2, visible, old_answer, old_action, &prior_sending, controls[1]));
        g_object_unref(old_action); old_action = NULL;
        g_object_unref(old_answer); old_answer = NULL;
    }
    CHECK((confirmed = focused(app, "Start session")));
    CHECK(state(visible, ATSPI_STATE_DEFUNCT));
    CHECK(xdotool("key", "Escape"));
window_closed: ;
    int status = 0;
    pid_t ended = 0;
    for (unsigned i = 0; i < 300 && !ended; ++i) {
        ended = waitpid(child, &status, WNOHANG);
        if (!ended) pump();
    }
    CHECK(ended == child);
    child = -1;
    CHECK(WIFEXITED(status) && !WEXITSTATUS(status));
    if (replace) puts("Native login replacement: two queued requests survive native context replacement; stale actions rejected");
    else if (loss) printf("Native login connection loop: %s retires protected preedit and closes in unknown state\n", loss);
    else if (ime) puts(cancel ? "Native login IME: focus cancellation and later Unicode commit passed" :
                          "Native login IME: separate secret/visible preedit and Unicode commits passed");
    else puts("Native login: protected password, real keyboard, retired fields and close passed");
    result = 0;
done:
    if (child > 0) { kill(child, SIGKILL); waitpid(child, NULL, 0); }
    if (old_action) g_object_unref(old_action);
    if (old_answer) g_object_unref(old_answer);
    if (prior_sending) g_object_unref(prior_sending);
    if (confirmed) g_object_unref(confirmed);
    if (visible) g_object_unref(visible);
    if (secret) g_object_unref(secret);
    if (app) g_object_unref(app);
    if (desktop) g_object_unref(desktop);
    if (initialized) atspi_exit();
    for (unsigned i = 0; i < 2; ++i) if (controls[i] >= 0) close(controls[i]);
    if (host_log) {
        char *transcript = malloc(128u * 1024u);
        if (!transcript) result = 1;
        else {
            ssize_t count = pread(fileno(host_log), transcript, 128u * 1024u, 0);
            if (count < 0 || count == 128 * 1024) result = 1;
            else if (fwrite(transcript, 1, (size_t)count, stdout) != (size_t)count) result = 1;
            free(transcript);
        }
        fclose(host_log);
    }
    return result;
}
