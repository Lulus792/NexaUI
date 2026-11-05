#define _POSIX_C_SOURCE 200809L
/* Independent AT-SPI client: deliberately no NexaUI or Aetheri linkage. */
#include <atspi/atspi.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

static pid_t host = -1;
static void cleanup(void) {
    if (host > 0) { kill(host, SIGTERM); waitpid(host, NULL, 0); host = -1; }
}
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "model-atspi:%d: %s\n", __LINE__, #x); cleanup(); return 1; } } while (0)
static void pump(unsigned ms) {
    for (unsigned i = 0; i < ms; ++i) {
        while (g_main_context_iteration(NULL, FALSE)) {}
        struct timespec t = {0, 1000000}; nanosleep(&t, NULL);
    }
}
static AtspiAccessible *find(AtspiAccessible *node, const char *name, unsigned depth) {
    if (!node || depth > 16) return NULL;
    GError *error = NULL; char *actual = atspi_accessible_get_name(node, &error);
    int match = !error && actual && !strcmp(actual, name);
    if (error) g_error_free(error);
    g_free(actual);
    if (match) return g_object_ref(node);
    error = NULL; int count = atspi_accessible_get_child_count(node, &error);
    if (error) { g_error_free(error); return NULL; }
    for (int i = 0; i < count; ++i) {
        error = NULL; AtspiAccessible *child = atspi_accessible_get_child_at_index(node, i, &error);
        if (error) { g_error_free(error); continue; }
        AtspiAccessible *result = find(child, name, depth + 1);
        if (child) g_object_unref(child);
        if (result) return result;
    }
    return NULL;
}
static AtspiAccessible *app_for_pid(AtspiAccessible *desktop) {
    for (unsigned n = 0; n < 300; ++n) {
        pump(10); atspi_accessible_clear_cache(desktop); GError *error = NULL;
        int count = atspi_accessible_get_child_count(desktop, &error);
        if (error) { g_error_free(error); continue; }
        for (int i = 0; i < count; ++i) {
            error = NULL; AtspiAccessible *app = atspi_accessible_get_child_at_index(desktop, i, &error);
            if (error) { g_error_free(error); continue; }
            if (!app) continue;
            atspi_accessible_clear_cache(app); guint pid = atspi_accessible_get_process_id(app, &error);
            if (!error && pid == (guint)host && atspi_accessible_get_child_count(app, &error) > 0 && !error) return app;
            if (error) g_error_free(error);
            g_object_unref(app);
        }
    }
    return NULL;
}
static int checked(AtspiAccessible *item) {
    atspi_accessible_clear_cache(item);
    AtspiStateSet *state = atspi_accessible_get_state_set(item);
    int value = state && atspi_state_set_contains(state, ATSPI_STATE_CHECKED);
    if (state) g_object_unref(state);
    return value;
}
static int activate(AtspiAccessible *item) {
    AtspiAction *action = atspi_accessible_get_action_iface(item);
    if (!action) return 0;
    GError *error = NULL; int ok = atspi_action_do_action(action, 0, &error) && !error;
    if (error) { fprintf(stderr, "action: %s\n", error->message); g_error_free(error); }
    g_object_unref(action); return ok;
}
static int await_checked(AtspiAccessible *item) {
    for (unsigned i = 0; i < 200; ++i) { pump(10); if (checked(item)) return 1; }
    return 0;
}
static int signal_file(const char *variable) {
    const char *path=getenv(variable); if(!path || !*path) return 0;
    FILE *f=fopen(path,"wx"); if(!f) return 0;
    int ok=fputs("ready\n",f)>=0; return fclose(f)==0 && ok;
}
static int await_file(const char *variable) {
    const char *path=getenv(variable); if(!path || !*path) return 0;
    for(unsigned i=0;i<800;++i) { if(access(path,F_OK)==0) return 1; pump(10); }
    return 0;
}
static int await_size(AtspiAccessible *item,int width,int height) {
    AtspiComponent *component=atspi_accessible_get_component_iface(item);
    if(!component) return 0;
    int matched=0;
    for(unsigned i=0;i<200 && !matched;++i) {
        GError *error=NULL; atspi_accessible_clear_cache(item);
        AtspiPoint *size=atspi_component_get_size(component,&error);
        matched=!error && size && size->x==width && size->y==height;
        if(error)g_error_free(error);
        g_free(size);
        if(!matched)pump(10);
    }
    g_object_unref(component); return matched;
}
int main(int argc, char **argv) {
    const char *font = getenv("NEXAUI_SETTINGS_FONT"); CHECK(argc == 2 && font && *font);
    host = fork(); CHECK(host >= 0);
    if (!host) {
        if (getenv("NEXAUI_MODEL_RESIZE_READY"))
            execl(argv[1], argv[1], font, "--diagnostics", "--fixture-displays", (char *)NULL);
        else if (getenv("NEXAUI_MODEL_DISPLAY_FIXTURE"))
            execl(argv[1], argv[1], font, "--verify", "--fixture-displays", (char *)NULL);
        else execl(argv[1], argv[1], font, "--verify", (char *)NULL);
        _exit(127);
    }
    CHECK(atspi_init() == 0); atspi_set_timeout(1000, 1000);
    AtspiAccessible *desktop = atspi_get_desktop(0); CHECK(desktop);
    AtspiAccessible *app = app_for_pid(desktop); CHECK(app);
    AtspiAccessible *contrast = find(app, "High contrast", 0);
    AtspiAccessible *german = find(app, "German", 0);
    AtspiAccessible *size = find(app, "200%", 0);
    CHECK(contrast && german && size);
    GError *error = NULL;
    CHECK(atspi_accessible_get_role(contrast, &error) == ATSPI_ROLE_RADIO_BUTTON && !error);
    CHECK(!checked(contrast) && !checked(german) && !checked(size));
    CHECK(activate(contrast) && await_checked(contrast));
    CHECK(activate(german) && await_checked(german));
    CHECK(activate(size) && await_checked(size));
    CHECK(checked(contrast) && checked(german) && checked(size));
    AtspiAccessible *medium=NULL,*root=NULL;
    if(getenv("NEXAUI_MODEL_RESIZE_READY")) {
        CHECK(signal_file("NEXAUI_MODEL_RESIZE_READY") && await_file("NEXAUI_MODEL_RESIZE_SMALL"));
        root=find(app,"Aetheri Einstellungen",0); CHECK(root && await_size(root,320,240));
        CHECK(checked(contrast) && checked(german) && checked(size));
        medium=find(app,"150%",0); CHECK(medium && activate(medium) && await_checked(medium));
        CHECK(!checked(size));
        CHECK(signal_file("NEXAUI_MODEL_RESIZE_CHANGED") && await_file("NEXAUI_MODEL_RESIZE_LARGE"));
        CHECK(await_size(root,800,480) && checked(medium) && checked(contrast) && checked(german));
    }
    if (getenv("NEXAUI_MODEL_DISPLAY_FIXTURE")) {
        AtspiAccessible *display=find(app,"Monitor B",0); CHECK(display && activate(display));
        AtspiAccessible *selected=NULL;
        for(unsigned i=0;i<200 && !selected;++i) {pump(10); selected=find(app,"Monitor B (ausgew\xc3\xa4hlt)",0);}
        CHECK(selected); g_object_unref(selected); g_object_unref(display);
    }
    AtspiAccessible *close = find(app, "Schlie\xc3\x9f" "en", 0); CHECK(close && activate(close));
    int status = 0, finished = 0;
    for (unsigned i = 0; i < 300 && !finished; ++i) {
        pump(10); pid_t result = waitpid(host, &status, WNOHANG);
        if (result == host) { finished = 1; host = -1; } else if (result < 0) break;
    }
    CHECK(finished && WIFEXITED(status) && WEXITSTATUS(status) == 0);
    g_object_unref(close); g_object_unref(size); g_object_unref(german); g_object_unref(contrast);
    if(medium)g_object_unref(medium);
    if(root)g_object_unref(root);
    g_object_unref(app); g_object_unref(desktop); atspi_exit();
    puts("Independent AT-SPI actions changed three Aetheri model groups and closed the view.");
    return 0;
}
