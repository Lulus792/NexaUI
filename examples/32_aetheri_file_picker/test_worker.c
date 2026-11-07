#define _GNU_SOURCE
#include "worker.h"
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "picker-worker:%d: %s\n", __LINE__, #x); return 1; } } while (0)
static pthread_t ui_thread;
static atomic_uint calls, wrong_thread, picks_completed;
static pthread_mutex_t gate_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t gate_changed = PTHREAD_COND_INITIALIZER;
static int armed, entered, released, ui_dead, after_ui;
static uint64_t next_request;
static void note(void) {
    atomic_fetch_add(&calls, 1);
    if (pthread_equal(pthread_self(), ui_thread)) atomic_fetch_add(&wrong_thread, 1);
}
#define WRAP(type, name, args, values) type __real_##name args; type __wrap_##name args { note(); return __real_##name values; }
WRAP(struct ae_files *, ae_files_create, (int fd, enum ae_files_result *r), (fd, r))
WRAP(enum ae_files_result, ae_files_enter, (struct ae_files *f), (f))
WRAP(enum ae_files_result, ae_files_up, (struct ae_files *f), (f))
WRAP(enum ae_files_result, ae_files_ancestor, (struct ae_files *f, unsigned d), (f, d))
WRAP(bool, ae_files_filter, (struct ae_files *f, const char *q, size_t n), (f, q, n))
WRAP(bool, ae_files_select, (struct ae_files *f, size_t n), (f, n))
WRAP(const struct ae_files_entry *, ae_files_entry, (const struct ae_files *f, size_t n), (f, n))
WRAP(const struct ae_files_entry *, ae_files_snapshot_entry, (const struct ae_files *f, size_t n), (f, n))
WRAP(size_t, ae_files_count, (const struct ae_files *f), (f))
WRAP(size_t, ae_files_total, (const struct ae_files *f), (f))
WRAP(unsigned, ae_files_depth, (const struct ae_files *f), (f))
WRAP(uint64_t, ae_files_revision, (const struct ae_files *f), (f))
WRAP(const char *, ae_files_query, (const struct ae_files *f), (f))
WRAP(const char *, ae_files_ancestor_label, (const struct ae_files *f, unsigned d), (f, d))
enum ae_files_result __real_ae_files_open_selected(const struct ae_files *, int *, int *, char *);
enum ae_files_result __wrap_ae_files_open_selected(const struct ae_files *f, int *d, int *w, char *n) {
    note(); enum ae_files_result result = __real_ae_files_open_selected(f, d, w, n);
    if (result == AE_FILES_OK) atomic_fetch_add(&picks_completed, 1);
    return result;
}
void __real_ae_files_destroy(struct ae_files *);
void __wrap_ae_files_destroy(struct ae_files *f) { note(); __real_ae_files_destroy(f); }
void __real_ae_files_hidden(struct ae_files *, bool);
void __wrap_ae_files_hidden(struct ae_files *f, bool show) { note(); __real_ae_files_hidden(f, show); }
enum ae_files_result __real_ae_files_refresh(struct ae_files *);
enum ae_files_result __wrap_ae_files_refresh(struct ae_files *f) {
    note(); pthread_mutex_lock(&gate_lock);
    if (armed) {
        armed = 0; entered = 1; pthread_cond_broadcast(&gate_changed);
        while (!released) pthread_cond_wait(&gate_changed, &gate_lock);
        after_ui = ui_dead;
    }
    pthread_mutex_unlock(&gate_lock); return __real_ae_files_refresh(f);
}
static double now(void) { struct timespec t; if (clock_gettime(CLOCK_MONOTONIC, &t)) exit(2); return (double)t.tv_sec + (double)t.tv_nsec / 1e9; }
static unsigned descriptors(void) {
    DIR *d = opendir("/proc/self/fd"); if (!d) return 0;
    unsigned n = 0; while (readdir(d)) ++n; closedir(d); return n;
}
static int file_at(int root, const char *name) {
    int fd = openat(root, name, O_CREAT | O_EXCL | O_WRONLY | O_CLOEXEC, 0600);
    return fd >= 0 && !close(fd);
}
static fp_command command(fp_operation op, uint64_t state) {
    fp_command c = {0}; c.key = (fp_key){1, ++next_request}; c.operation = op; c.expected_state = state; return c;
}
static int receive(fp_worker *w, uint64_t ticket, nx_sim_message *out) {
    double deadline = now() + 5;
    while (now() < deadline) {
        if (fp_worker_receive(w, out) != NX_OK) return 0;
        if (out->ticket) return out->ticket == ticket;
        struct pollfd fd = {fp_worker_wake_fd(w), POLLIN, 0};
        int r = poll(&fd, 1, 100); if (r < 0 && errno != EINTR) return 0;
    }
    return 0;
}
static int ask(fp_worker *w, const fp_command *c, nx_sim_message *m) {
    uint64_t ticket;
    return fp_worker_submit(w, c, &ticket) == NX_OK && receive(w, ticket, m) && fp_reply_get(m) &&
        fp_reply_get(m)->key.view == c->key.view && fp_reply_get(m)->key.request == c->key.request;
}
static size_t ordinal(const fp_reply *r, const char *name) {
    for (size_t i = 0; i < r->count; ++i) if (!strcmp(r->rows[i].entry.name, name)) return r->rows[i].ordinal;
    return SIZE_MAX;
}
static int navigation(int base) {
    CHECK(!mkdirat(base, "navigation", 0700)); int root = openat(base, "navigation", O_DIRECTORY | O_CLOEXEC); CHECK(root >= 0);
    CHECK(!mkdirat(root, "existing", 0700) && !mkdirat(root, "other", 0700));
    int child = openat(root, "existing", O_DIRECTORY | O_CLOEXEC); CHECK(child >= 0 && !mkdirat(child, "nested", 0700));
    int nested = openat(child, "nested", O_DIRECTORY | O_CLOEXEC); CHECK(nested >= 0 && file_at(nested, "inside.txt")); close(nested); close(child);
    char raw[256]; memset(raw, 0xff, 255); raw[255] = 0;
    CHECK(file_at(root, "document.txt") && file_at(root, "line\nbreak") && file_at(root, ".hidden") && file_at(root, raw) && !symlinkat("existing", root, "linked"));
    unsigned before = descriptors(); fp_worker *w = NULL; CHECK(fp_worker_start(root, &w) == NX_OK);
    nx_sim_message first = {0}, m = {0}; fp_command c = command(FP_READ, 0); CHECK(ask(w, &c, &first));
    fp_reply *r = fp_reply_get(&first); CHECK(r->result == AE_FILES_OK && r->state == 1 && r->revision == 1 && r->total == 7 && r->count == 6);
    size_t doc = ordinal(r, "document.txt"), existing = ordinal(r, "existing"), link = ordinal(r, "linked"), escaped = ordinal(r, raw);
    CHECK(doc != SIZE_MAX && existing != SIZE_MAX && link != SIZE_MAX && escaped != SIZE_MAX);
    for (size_t i = 0; i < r->count; ++i) if (r->rows[i].ordinal == escaped)
        CHECK(strlen(r->rows[i].entry.name) == 255 && strlen(r->rows[i].entry.label) == 1020 &&
            nx_utf8_validate(r->rows[i].entry.label, 1020, NULL) == NX_OK);
    c = command(FP_FILTER, 1); strcpy(c.query, "document"); c.query_bytes = 8; CHECK(ask(w, &c, &m)); r = fp_reply_get(&m);
    CHECK(r->state == 2 && r->revision == 1 && r->total == 7 && r->count == 1 && r->rows[0].ordinal == doc); nx_sim_message_dispose(&m);
    c = command(FP_ENTER, 1); c.ordinal = existing; CHECK(ask(w, &c, &m)); r = fp_reply_get(&m);
    CHECK(r->result == AE_FILES_CHANGED && r->state == 2 && r->depth == 0 && !strcmp(r->query, "document")); nx_sim_message_dispose(&m);
    c = command(FP_FILTER, 2); CHECK(ask(w, &c, &m)); r = fp_reply_get(&m); CHECK(r->state == 3 && r->count == 6); nx_sim_message_dispose(&m);
    c = command(FP_HIDDEN, 3); c.show_hidden = 1; CHECK(ask(w, &c, &m)); r = fp_reply_get(&m);
    CHECK(r->state == 4 && r->count == 7 && r->show_hidden && ordinal(r, ".hidden") != SIZE_MAX); nx_sim_message_dispose(&m);
    c = command(FP_ENTER, 4); c.ordinal = link; CHECK(ask(w, &c, &m)); r = fp_reply_get(&m);
    CHECK(r->result == AE_FILES_UNSUPPORTED && r->depth == 0 && r->revision == 1 && r->state == 5); nx_sim_message_dispose(&m);
    c = command(FP_ENTER, 5); c.ordinal = existing; CHECK(ask(w, &c, &m)); r = fp_reply_get(&m);
    CHECK(r->result == AE_FILES_OK && r->depth == 1 && r->revision == 2 && r->state == 6 && !strcmp(r->ancestors[1], "existing"));
    size_t sub = ordinal(r, "nested"); CHECK(sub != SIZE_MAX); nx_sim_message_dispose(&m);
    CHECK(ordinal(fp_reply_get(&first), "document.txt") == doc); nx_sim_message_dispose(&first);
    c = command(FP_ENTER, 6); c.ordinal = sub; CHECK(ask(w, &c, &m)); r = fp_reply_get(&m);
    CHECK(r->result == AE_FILES_OK && r->depth == 2 && r->state == 7 && !strcmp(r->ancestors[2], "nested")); nx_sim_message_dispose(&m);
    CHECK(!renameat(root, "existing", root, "renamed") && !mkdirat(root, "existing", 0700));
    child = openat(root, "existing", O_DIRECTORY | O_CLOEXEC); CHECK(child >= 0 && file_at(child, "decoy")); close(child);
    c = command(FP_ANCESTOR, 7); c.depth = 1; CHECK(ask(w, &c, &m)); r = fp_reply_get(&m);
    CHECK(r->state == 8 && r->depth == 1 && ordinal(r, "nested") != SIZE_MAX && ordinal(r, "decoy") == SIZE_MAX); nx_sim_message_dispose(&m);
    c = command(FP_UP, 8); CHECK(ask(w, &c, &m)); r = fp_reply_get(&m); existing = ordinal(r, "existing");
    CHECK(r->state == 9 && r->depth == 0 && existing != SIZE_MAX); uint64_t revision = r->revision; nx_sim_message_dispose(&m);
    CHECK(!renameat(root, "existing", root, "moved") && !mkdirat(root, "existing", 0700));
    c = command(FP_ENTER, 9); c.ordinal = existing; CHECK(ask(w, &c, &m)); r = fp_reply_get(&m);
    CHECK(r->result == AE_FILES_CHANGED && r->state == 10 && r->revision == revision && r->depth == 0); nx_sim_message_dispose(&m);
    c = command(FP_REFRESH, 10); CHECK(ask(w, &c, &m)); r = fp_reply_get(&m); doc = ordinal(r, "document.txt");
    CHECK(r->result == AE_FILES_OK && r->state == 11 && doc != SIZE_MAX); nx_sim_message_dispose(&m);
    c = command(FP_PICK, 11); c.ordinal = doc; CHECK(ask(w, &c, &m)); r = fp_reply_get(&m);
    CHECK(r->result == AE_FILES_OK && r->directory >= 4 && r->witness >= 4 && !strcmp(r->leaf, "document.txt"));
    int directory = -1, witness = -1, x = -1, y = -1; char leaf[256], again[256];
    CHECK(fp_reply_take(r, &directory, &witness, leaf) == NX_OK && fp_reply_take(r, &x, &y, again) == NX_ERROR_STATE && x == -1 && y == -1);
    nx_sim_message_dispose(&m); CHECK(fp_worker_destroy(&w) == NX_OK && !w);
    struct stat st, expected; CHECK(!fstat(witness, &st) && !fstatat(root, leaf, &expected, AT_SYMLINK_NOFOLLOW) &&
        st.st_dev == expected.st_dev && st.st_ino == expected.st_ino && !fstat(directory, &st));
    close(directory); close(witness); CHECK(descriptors() == before); close(root);
    puts("PICKER_ORIGINAL_NAVIGATION_AND_OWNED_WITNESS_VERIFIED"); return 0;
}
static int limits(int base) {
    CHECK(!mkdirat(base, "limits", 0700)); int root = openat(base, "limits", O_DIRECTORY | O_CLOEXEC); CHECK(root >= 0 && !mkdirat(root, "overflow", 0700));
    int folder = openat(root, "overflow", O_DIRECTORY | O_CLOEXEC); CHECK(folder >= 0);
    for (unsigned i = 0; i <= AE_FILES_ENTRIES; ++i) { char name[32]; snprintf(name, sizeof name, "item-%04u", i); CHECK(file_at(folder, name)); }
    fp_worker *w = NULL; nx_sim_message m = {0}; CHECK(fp_worker_start(root, &w) == NX_OK);
    fp_command c = command(FP_READ, 0); CHECK(ask(w, &c, &m)); nx_sim_message_dispose(&m);
    c = command(FP_ENTER, 1); CHECK(ask(w, &c, &m)); fp_reply *r = fp_reply_get(&m);
    CHECK(r->result == AE_FILES_LIMIT && r->depth == 0 && r->revision == 1 && r->count == 1 && r->state == 2); nx_sim_message_dispose(&m);
    CHECK(!unlinkat(folder, "item-4096", 0)); close(folder);
    c = command(FP_ENTER, 2); CHECK(ask(w, &c, &m)); r = fp_reply_get(&m);
    CHECK(r->result == AE_FILES_OK && r->count == AE_FILES_ENTRIES && r->total == AE_FILES_ENTRIES && r->depth == 1 && r->revision == 2 && r->state == 3);
    for (size_t i = 0; i < r->count; ++i) CHECK(r->rows[i].ordinal == i);
    nx_sim_message broken = m; ++broken.bytes; CHECK(!fp_reply_get(&broken)); nx_sim_message_dispose(&m);
    c = command(FP_FILTER, 3); strcpy(c.query, "item-4095"); c.query_bytes = 9; CHECK(ask(w, &c, &m)); r = fp_reply_get(&m);
    CHECK(r->count == 1 && r->rows[0].ordinal == 4095 && !strcmp(r->rows[0].entry.name, "item-4095")); nx_sim_message_dispose(&m);
    c = command(FP_PICK, 4); c.ordinal = 0; CHECK(ask(w, &c, &m)); r = fp_reply_get(&m);
    CHECK(r->result == AE_FILES_CHANGED && r->directory == -1 && r->witness == -1 && r->state == 5); nx_sim_message_dispose(&m);
    c = command(FP_PICK, 5); c.ordinal = 4095; CHECK(ask(w, &c, &m)); r = fp_reply_get(&m);
    CHECK(r->result == AE_FILES_OK); int directory = r->directory, witness = r->witness; nx_sim_message_dispose(&m);
    CHECK(fcntl(directory, F_GETFD) == -1 && errno == EBADF && fcntl(witness, F_GETFD) == -1 && errno == EBADF);
    CHECK(fp_worker_destroy(&w) == NX_OK); close(root);
    puts("PICKER_COMPLETE_4096_AND_FILTER_ORDINAL_VERIFIED"); return 0;
}
static int depth(int base) {
    CHECK(!mkdirat(base, "deep", 0700)); int root = openat(base, "deep", O_DIRECTORY | O_CLOEXEC), cursor = dup(root); CHECK(root >= 0 && cursor >= 0);
    for (unsigned d = 1; d <= AE_FILES_DEPTH + 1; ++d) {
        char name[16]; snprintf(name, sizeof name, "d%02u", d); CHECK(!mkdirat(cursor, name, 0700));
        int next = openat(cursor, name, O_DIRECTORY | O_CLOEXEC); CHECK(next >= 0); close(cursor); cursor = next;
    }
    close(cursor); unsigned before = descriptors(); fp_worker *w = NULL; CHECK(fp_worker_start(root, &w) == NX_OK);
    nx_sim_message m = {0}; fp_command c = command(FP_READ, 0); CHECK(ask(w, &c, &m)); nx_sim_message_dispose(&m);
    for (unsigned d = 1; d <= AE_FILES_DEPTH; ++d) {
        c = command(FP_ENTER, d); CHECK(ask(w, &c, &m)); fp_reply *r = fp_reply_get(&m); char name[16]; snprintf(name, sizeof name, "d%02u", d);
        CHECK(r->result == AE_FILES_OK && r->depth == d && r->state == (uint64_t)d + 1 && !strcmp(r->ancestors[d], name)); nx_sim_message_dispose(&m);
    }
    c = command(FP_ENTER, AE_FILES_DEPTH + 1); CHECK(ask(w, &c, &m)); fp_reply *r = fp_reply_get(&m);
    CHECK(r->result == AE_FILES_LIMIT && r->depth == AE_FILES_DEPTH && r->revision == AE_FILES_DEPTH + 1); uint64_t state = r->state; nx_sim_message_dispose(&m);
    c = command(FP_ANCESTOR, state); CHECK(ask(w, &c, &m)); r = fp_reply_get(&m); CHECK(r->result == AE_FILES_OK && r->depth == 0); nx_sim_message_dispose(&m);
    CHECK(fp_worker_destroy(&w) == NX_OK && descriptors() == before); close(root);
    puts("PICKER_HELD_64_LEVELS_AND_ROOT_BOUND_VERIFIED"); return 0;
}
static void arm(void) { pthread_mutex_lock(&gate_lock); armed = 1; entered = released = ui_dead = after_ui = 0; pthread_mutex_unlock(&gate_lock); }
static int validation(int base) {
    CHECK(!mkdirat(base, "validation", 0700)); int root = openat(base, "validation", O_DIRECTORY | O_CLOEXEC);
    CHECK(root >= 0 && file_at(root, "file") && file_at(root, ".hidden"));
    unsigned before = descriptors(); fp_worker *w = NULL; CHECK(fp_worker_start(root, &w) == NX_OK);
    nx_sim_message m = {0}; fp_command c = command(FP_READ, 0); CHECK(ask(w, &c, &m)); nx_sim_message_dispose(&m);
    c = command(FP_HIDDEN, 1); c.show_hidden = 1; CHECK(ask(w, &c, &m)); nx_sim_message_dispose(&m);
    c = command(FP_READ, 0); CHECK(ask(w, &c, &m)); fp_reply *r = fp_reply_get(&m);
    CHECK(r->state == 2 && r->show_hidden && r->count == 2); nx_sim_message_dispose(&m);
    c = command(FP_FILTER, 2); memcpy(c.query, "file", 4); c.query_bytes = 4; CHECK(ask(w, &c, &m)); nx_sim_message_dispose(&m);
    c = command(FP_FILTER, 3); memcpy(c.query, "fi\0le", 5); c.query_bytes = 5; CHECK(ask(w, &c, &m)); r = fp_reply_get(&m);
    CHECK(r->result == AE_FILES_INVALID && r->state == 4 && r->count == 1 && !strcmp(r->query, "file")); nx_sim_message_dispose(&m);
    c = command(FP_FILTER, 4); c.query[0] = (char)0xff; c.query_bytes = 1; CHECK(ask(w, &c, &m)); r = fp_reply_get(&m);
    CHECK(r->result == AE_FILES_INVALID && r->state == 5 && r->count == 1 && !strcmp(r->query, "file")); nx_sim_message_dispose(&m);
    c = command(FP_FILTER, 5); memset(c.query, 'x', AE_FILES_QUERY); c.query_bytes = AE_FILES_QUERY; CHECK(ask(w, &c, &m)); r = fp_reply_get(&m);
    CHECK(r->result == AE_FILES_OK && r->state == 6 && !r->count && r->total == 2 && strlen(r->query) == AE_FILES_QUERY); nx_sim_message_dispose(&m);
    c = command(FP_FILTER, 6); c.query_bytes = AE_FILES_QUERY + 1; uint64_t ticket;
    CHECK(fp_worker_submit(w, &c, &ticket) == NX_ERROR_ARGUMENT && !ticket);
    c = command(FP_READ, 0); c.key.view = 0; CHECK(fp_worker_submit(w, &c, &ticket) == NX_ERROR_ARGUMENT && !ticket);
    c = command(FP_READ, 0); CHECK(ask(w, &c, &m)); CHECK(fp_reply_get(&m)->state == 6); nx_sim_message_dispose(&m);
    CHECK(fp_worker_destroy(&w) == NX_OK && descriptors() == before); close(root);
    puts("PICKER_FILTER_BYTES_HIDDEN_REDISCOVERY_AND_NO_TRUNCATION_VERIFIED"); return 0;
}
static int wait_gate(void) {
    double until = now() + 5;
    while (now() < until) {
        pthread_mutex_lock(&gate_lock); int held = entered; pthread_mutex_unlock(&gate_lock); if (held) return 1;
        struct timespec t = {0, 1000000}; nanosleep(&t, NULL);
    }
    return 0;
}
static void release(int dead) { pthread_mutex_lock(&gate_lock); ui_dead = dead; released = 1; pthread_cond_broadcast(&gate_changed); pthread_mutex_unlock(&gate_lock); }
static int busy_frame(nx_context *ctx, int *cancelled) {
    if (nx_begin_frame(ctx, 320, 240, 0) != NX_OK) return 0;
    nx_semantic_desc busy = {0}; busy.id = 2; busy.role = NX_ROLE_STATUS;
    busy.name = (nx_utf8_span){"Loading directory", 17}; busy.state = NX_SEM_BUSY;
    busy.bounds = (nx_rect){16, 16, 288, 40};
    nx_control_desc cancel = {0}; cancel.id = 3; cancel.name = (nx_utf8_span){"Cancel", 6};
    cancel.bounds = (nx_rect){16, 160, 120, 48}; nx_interaction_state state;
    if (nx_semantics_add(ctx, &busy, NULL) != NX_OK || nx_button(ctx, &cancel, &state) != NX_OK || nx_end_frame(ctx) != NX_OK) return 0;
    uint32_t index; nx_semantic_view status;
    if (nx_semantics_find(ctx, 2, &index) != NX_OK || nx_semantics_get(ctx, index, &status) != NX_OK || !(status.state & NX_SEM_BUSY)) return 0;
    *cancelled = state.clicked != 0; return 1;
}
static int lifetime(int base) {
    CHECK(!mkdirat(base, "lifetime", 0700)); int root = openat(base, "lifetime", O_DIRECTORY | O_CLOEXEC); CHECK(root >= 0 && !mkdirat(root, "child", 0700));
    unsigned before = descriptors(), initial_calls = atomic_load(&calls); fp_worker *w = NULL; CHECK(fp_worker_start(root, &w) == NX_OK && atomic_load(&calls) == initial_calls);
    CHECK(fp_worker_join(w) == NX_ERROR_STATE);
    CHECK((fcntl(fp_worker_wake_fd(w), F_GETFD) & FD_CLOEXEC) && (fcntl(fp_worker_wake_fd(w), F_GETFL) & O_NONBLOCK));
    nx_sim_message m = {0}; fp_command c = command(FP_READ, 0); CHECK(ask(w, &c, &m)); nx_sim_message_dispose(&m);
    for (unsigned i = 0; i < 64; ++i) { c = command(FP_READ, 0); CHECK(ask(w, &c, &m)); nx_sim_message_dispose(&m); }
    arm(); uint64_t first, second, extra;
    c = command(FP_REFRESH, 1); CHECK(fp_worker_submit(w, &c, &first) == NX_OK && wait_gate());
    c = command(FP_ENTER, 1); CHECK(fp_worker_submit(w, &c, &second) == NX_OK);
    CHECK(fp_worker_submit(w, &c, &extra) == NX_ERROR_LIMIT && !extra);
    nx_context *ctx = NULL; CHECK(nx_context_create(NULL, &ctx) == NX_OK);
    int cancelled = 0;
    for (unsigned i = 0; i < 16; ++i) CHECK(busy_frame(ctx, &cancelled) && !cancelled);
    release(0); CHECK(receive(w, first, &m)); fp_reply *r = fp_reply_get(&m); CHECK(r && r->state == 2 && r->depth == 0); nx_sim_message_dispose(&m);
    CHECK(receive(w, second, &m)); r = fp_reply_get(&m); CHECK(r && r->result == AE_FILES_CHANGED && r->state == 2 && r->depth == 0); nx_sim_message_dispose(&m);
    arm(); c = command(FP_REFRESH, 2); CHECK(fp_worker_submit(w, &c, &first) == NX_OK && wait_gate());
    c = command(FP_READ, 0); c.key.view = 99; CHECK(fp_worker_submit(w, &c, &second) == NX_OK);
    CHECK(nx_focus_set(ctx, 3) == NX_OK);
    nx_event enter = {0}; enter.type = NX_EVENT_KEY_DOWN; enter.key = NX_KEY_ENTER;
    CHECK(nx_context_queue_event(ctx, &enter) == NX_OK); enter.type = NX_EVENT_KEY_UP;
    CHECK(nx_context_queue_event(ctx, &enter) == NX_OK && busy_frame(ctx, &cancelled) && cancelled);
    nx_context_destroy(ctx); CHECK(fp_worker_stop(w) == NX_OK); release(1); CHECK(fp_worker_join(w) == NX_OK && after_ui);
    CHECK(receive(w, first, &m) && fp_reply_get(&m) && fp_reply_get(&m)->state == 3); nx_sim_message_dispose(&m);
    CHECK(receive(w, second, &m) && m.status == NX_ERROR_STATE && !m.data); nx_sim_message_dispose(&m);
    CHECK(fp_worker_submit(w, &c, &extra) == NX_ERROR_STATE && !extra && fp_worker_destroy(&w) == NX_OK && descriptors() == before);
    close(root); puts("PICKER_WAKEUP_BACKPRESSURE_STALE_STATE_AND_POST_UI_JOIN_VERIFIED"); return 0;
}
static int orphan(int base) {
    CHECK(!mkdirat(base, "orphan", 0700)); int root = openat(base, "orphan", O_DIRECTORY | O_CLOEXEC); CHECK(root >= 0 && file_at(root, "selected"));
    unsigned before = descriptors() - 1; fp_worker *w = NULL; CHECK(fp_worker_start(root, &w) == NX_OK);
    close(root); /* The original model is not created yet; the owned duplicate suffices. */
    nx_sim_message m = {0}; fp_command c = command(FP_READ, 0); CHECK(ask(w, &c, &m)); nx_sim_message_dispose(&m);
    unsigned picked = atomic_load(&picks_completed);
    c = command(FP_PICK, 1); uint64_t ticket; CHECK(fp_worker_submit(w, &c, &ticket) == NX_OK);
    double until = now() + 5;
    while (now() < until && atomic_load(&picks_completed) == picked) { struct timespec t = {0, 1000000}; nanosleep(&t, NULL); }
    CHECK(atomic_load(&picks_completed) == picked + 1 && descriptors() >= before + 6);
    struct pollfd fd = {fp_worker_wake_fd(w), POLLIN, 0}; CHECK(poll(&fd, 1, 5000) == 1);
    CHECK(fp_worker_destroy(&w) == NX_OK && descriptors() == before);
    /* Invalid original root is an explicit failure, never confirmed empty. */
    root = openat(base, "regular-root", O_CREAT | O_EXCL | O_RDWR | O_CLOEXEC, 0600); CHECK(root >= 0 && fp_worker_start(root, &w) == NX_OK);
    c = command(FP_READ, 0); CHECK(ask(w, &c, &m)); fp_reply *r = fp_reply_get(&m);
    CHECK(r->result == AE_FILES_INVALID && !r->state && !r->revision && !r->count); nx_sim_message_dispose(&m);
    CHECK(fp_worker_destroy(&w) == NX_OK); close(root);
    puts("PICKER_ABANDONED_WITNESSES_AND_FAILED_ROOT_VERIFIED"); return 0;
}
int main(int argc, char **argv) {
    CHECK(argc == 2 || (argc == 3 && !strcmp(argv[2], "lifetime")));
    ui_thread = pthread_self(); int base = open(argv[1], O_DIRECTORY | O_CLOEXEC); CHECK(base >= 0);
    if (argc == 3) CHECK(!lifetime(base));
    else CHECK(!navigation(base) && !limits(base) && !depth(base) && !validation(base) && !lifetime(base) && !orphan(base));
    CHECK(!atomic_load(&wrong_thread)); close(base);
    printf("PICKER_WORKER_VERIFIED model_calls=%u wrong_thread=%u header_bytes=%zu row_bytes=%zu max_reply_bytes=%zu\n",
        atomic_load(&calls), atomic_load(&wrong_thread), offsetof(fp_reply, rows), sizeof(fp_row),
        sizeof(fp_reply) + AE_FILES_ENTRIES * sizeof(fp_row)); return 0;
}
