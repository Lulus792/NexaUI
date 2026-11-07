#define _GNU_SOURCE
#include "worker.h"
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

struct fp_worker {
    nx_sim_channel *channel;
    pthread_t thread;
    pthread_mutex_t lock;
    pthread_cond_t changed;
    unsigned hints;
    int stop, joined, failed, root, wake[2], show_hidden;
    /* Accessed only by the worker, including construction and destruction. */
    struct ae_files *files;
    uint64_t state;
};
static void free_command(void *user, void *data, size_t bytes) { (void)user; (void)bytes; free(data); }
static void free_reply(void *user, void *data, size_t bytes) {
    (void)user; (void)bytes;
    fp_reply *r = data;
    if (r->directory >= 0) close(r->directory);
    if (r->witness >= 0 && r->witness != r->directory) close(r->witness);
    free(r);
}
static int valid(const fp_command *c) {
    return c && c->key.view && c->key.request && c->operation >= FP_READ && c->operation <= FP_PICK_DIRECTORY &&
        (c->operation == FP_READ || c->expected_state) &&
        c->depth <= AE_FILES_DEPTH && c->ordinal < AE_FILES_ENTRIES &&
        (c->show_hidden == 0 || c->show_hidden == 1) && c->query_bytes <= AE_FILES_QUERY;
}
static enum ae_files_result select_ordinal(struct ae_files *f, size_t ordinal) {
    const struct ae_files_entry *target = ae_files_snapshot_entry(f, ordinal);
    if (!target) return AE_FILES_CHANGED;
    for (size_t i = 0; i < ae_files_count(f); ++i)
        if (ae_files_entry(f, i) == target) return ae_files_select(f, i) ? AE_FILES_OK : AE_FILES_CHANGED;
    return AE_FILES_CHANGED;
}
static enum ae_files_result apply(fp_worker *w, const fp_command *c, fp_reply *r) {
    if (!w->files) {
        if (c->operation != FP_READ) return AE_FILES_CHANGED;
        enum ae_files_result result;
        w->files = ae_files_create(w->root, &result);
        if (w->files) w->state = 1;
        return result;
    }
    if (c->operation == FP_READ) return AE_FILES_OK;
    if (c->expected_state != w->state) return AE_FILES_CHANGED;
    if (w->state == UINT64_MAX) return AE_FILES_EXHAUSTED;
    /* Even a failed admitted attempt supersedes old commands. Only READ may
       rediscover state after lost/retired observations; it performs no replay. */
    ++w->state;
    switch (c->operation) {
    case FP_REFRESH: return ae_files_refresh(w->files);
    case FP_ENTER: {
        enum ae_files_result selected = select_ordinal(w->files, c->ordinal);
        return selected == AE_FILES_OK ? ae_files_enter(w->files) : selected;
    }
    case FP_UP: return ae_files_up(w->files);
    case FP_ANCESTOR: return ae_files_ancestor(w->files, c->depth);
    case FP_FILTER: return ae_files_filter(w->files, c->query, c->query_bytes) ? AE_FILES_OK : AE_FILES_INVALID;
    case FP_HIDDEN: ae_files_hidden(w->files, c->show_hidden != 0); w->show_hidden = c->show_hidden; return AE_FILES_OK;
    case FP_PICK: {
        enum ae_files_result selected = select_ordinal(w->files, c->ordinal);
        return selected == AE_FILES_OK ? ae_files_open_selected(w->files, &r->directory, &r->witness, r->leaf) : selected;
    }
    case FP_PICK_DIRECTORY:
        r->directory = ae_files_directory(w->files);
        if (r->directory < 0) return AE_FILES_IO;
        r->witness = fcntl(r->directory, F_DUPFD_CLOEXEC, 4);
        if (r->witness < 0) { close(r->directory); r->directory = -1; return AE_FILES_IO; }
        r->leaf[0] = '.'; r->leaf[1] = 0;
        return AE_FILES_OK;
    default: return AE_FILES_INVALID;
    }
}
static nx_result project(fp_worker *w, fp_reply *r) {
    if (!w->files) return NX_OK; /* result distinguishes failed/unobserved root. */
    r->state = w->state; r->revision = ae_files_revision(w->files);
    r->show_hidden = w->show_hidden;
    r->depth = ae_files_depth(w->files); r->total = ae_files_total(w->files); r->count = ae_files_count(w->files);
    if (r->depth > AE_FILES_DEPTH || r->total > AE_FILES_ENTRIES || r->count > r->total) return NX_ERROR_STATE;
    memcpy(r->query, ae_files_query(w->files), strlen(ae_files_query(w->files)) + 1);
    for (unsigned d = 0; d <= r->depth; ++d) {
        const char *s = ae_files_ancestor_label(w->files, d); size_t n = strlen(s);
        if (n > AE_FILES_LABEL) return NX_ERROR_STATE;
        memcpy(r->ancestors[d], s, n + 1);
    }
    /* The pinned original filter preserves complete-snapshot order. Match its
       borrowed visible pointers in one pass; no new sort/filter/escape model,
       pointer arithmetic or worker-owned address reaches the UI. */
    size_t visible = 0;
    for (size_t i = 0; i < r->total && visible < r->count; ++i) {
        const struct ae_files_entry *entry = ae_files_snapshot_entry(w->files, i);
        if (entry == ae_files_entry(w->files, visible)) {
            r->rows[visible].ordinal = i; r->rows[visible].entry = *entry; ++visible;
        }
    }
    return visible == r->count ? NX_OK : NX_ERROR_STATE;
}
static void *run(void *data) {
    fp_worker *w = data;
    for (;;) {
        pthread_mutex_lock(&w->lock);
        while (!w->stop && !w->hints) pthread_cond_wait(&w->changed, &w->lock);
        if (!w->hints) { pthread_mutex_unlock(&w->lock); break; }
        --w->hints; int cancelled = w->stop; pthread_mutex_unlock(&w->lock);
        nx_sim_message command = {0};
        nx_result status = nx_sim_channel_take_command(w->channel, &command);
        if (status != NX_OK || !command.ticket) {
            pthread_mutex_lock(&w->lock); w->failed = 1; pthread_mutex_unlock(&w->lock); break;
        }
        fp_reply *reply = NULL; size_t bytes = 0;
        if (cancelled) status = NX_ERROR_STATE;
        else if (command.bytes != sizeof(fp_command) || !valid(command.data)) status = NX_ERROR_ARGUMENT;
        else {
            /* Reserve the full bounded reply BEFORE any original model call.
               Allocation failure therefore cannot change backend state. */
            reply = calloc(1, sizeof(*reply) + AE_FILES_ENTRIES * sizeof(fp_row));
            if (!reply) status = NX_ERROR_MEMORY;
            else {
                const fp_command *c = command.data; reply->key = c->key; reply->directory = reply->witness = -1;
                reply->result = apply(w, c, reply); status = project(w, reply);
                if (status == NX_OK) {
                    bytes = offsetof(fp_reply, rows) + reply->count * sizeof(fp_row);
                    fp_reply *smaller = realloc(reply, bytes);
                    if (smaller) reply = smaller; /* Failure retains the reserved bounded allocation. */
                }
            }
        }
        uint64_t ticket = command.ticket; nx_sim_message_dispose(&command);
        if (status != NX_OK && reply) { free_reply(NULL, reply, bytes); reply = NULL; bytes = 0; }
        nx_result posted = nx_sim_channel_post_completion(w->channel, ticket, status, reply, bytes, reply ? free_reply : NULL, NULL);
        if (posted != NX_OK && reply) free_reply(NULL, reply, bytes);
        ssize_t n; do { n = write(w->wake[1], "W", 1); } while (n < 0 && errno == EINTR);
        if (posted != NX_OK || (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK)) {
            pthread_mutex_lock(&w->lock); w->failed = 1; pthread_mutex_unlock(&w->lock);
        }
    }
    ae_files_destroy(w->files); w->files = NULL;
    return NULL;
}
nx_result fp_worker_start(int root, fp_worker **out) {
    if (!out) return NX_ERROR_ARGUMENT;
    *out = NULL; if (root < 0) return NX_ERROR_ARGUMENT;
    fp_worker *w = calloc(1, sizeof(*w)); if (!w) return NX_ERROR_MEMORY;
    w->root = -1; w->wake[0] = w->wake[1] = -1;
    nx_result result = NX_ERROR_PLATFORM; int lock = 0, cond = 0;
    w->root = fcntl(root, F_DUPFD_CLOEXEC, 4); if (w->root < 0) goto fail;
    /* Same atomic descriptor-creation pattern as the existing Wayland adapter:
       a concurrent application exec must not inherit a temporarily unsealed pipe. */
    if (pipe2(w->wake, O_CLOEXEC | O_NONBLOCK)) goto fail;
    if (pthread_mutex_init(&w->lock, NULL)) goto fail;
    lock = 1; if (pthread_cond_init(&w->changed, NULL)) goto fail;
    cond = 1; result = nx_sim_channel_create(2, NULL, &w->channel); if (result != NX_OK) goto fail;
    result = NX_ERROR_PLATFORM; if (pthread_create(&w->thread, NULL, run, w)) goto fail;
    *out = w; return NX_OK;
fail:
    if (w->channel) (void)nx_sim_channel_destroy(w->channel);
    if (cond) pthread_cond_destroy(&w->changed);
    if (lock) pthread_mutex_destroy(&w->lock);
    if (w->wake[0] >= 0) close(w->wake[0]);
    if (w->wake[1] >= 0) close(w->wake[1]);
    if (w->root >= 0) close(w->root);
    free(w); return result;
}
nx_result fp_worker_submit(fp_worker *w, const fp_command *command, uint64_t *ticket) {
    if (!ticket) return NX_ERROR_ARGUMENT;
    *ticket = 0; if (!w || !valid(command)) return NX_ERROR_ARGUMENT;
    fp_command *copy = malloc(sizeof(*copy)); if (!copy) return NX_ERROR_MEMORY;
    *copy = *command;
    pthread_mutex_lock(&w->lock);
    nx_result r = w->stop || w->failed ? NX_ERROR_STATE : nx_sim_channel_post_command(w->channel, copy, sizeof(*copy), free_command, NULL, ticket);
    if (r == NX_OK) { ++w->hints; pthread_cond_signal(&w->changed); }
    pthread_mutex_unlock(&w->lock);
    if (r != NX_OK) free(copy);
    return r;
}
int fp_worker_wake_fd(const fp_worker *w) { return w ? w->wake[0] : -1; }
nx_result fp_worker_receive(fp_worker *w, nx_sim_message *message) {
    if (!message) return NX_ERROR_ARGUMENT;
    *message = (nx_sim_message){0}; if (!w) return NX_ERROR_ARGUMENT;
    char buffer[64]; ssize_t n;
    do { n = read(w->wake[0], buffer, sizeof buffer); } while (n > 0 || (n < 0 && errno == EINTR));
    if (n >= 0 || (errno != EAGAIN && errno != EWOULDBLOCK)) return NX_ERROR_PLATFORM;
    nx_result result = nx_sim_channel_take_completion(w->channel, message);
    if (result == NX_OK && !message->ticket) {
        pthread_mutex_lock(&w->lock); int failed = w->failed; pthread_mutex_unlock(&w->lock);
        if (failed) result = NX_ERROR_STATE;
    }
    return result;
}
fp_reply *fp_reply_get(const nx_sim_message *m) {
    if (!m || !m->ticket || m->status != NX_OK || !m->data || m->bytes < offsetof(fp_reply, rows)) return NULL;
    fp_reply *r = m->data;
    return r->key.view && r->key.request && (r->result != AE_FILES_OK || (r->state && r->revision)) &&
        r->count <= AE_FILES_ENTRIES && r->total <= AE_FILES_ENTRIES && r->count <= r->total &&
        r->depth <= AE_FILES_DEPTH && m->bytes == offsetof(fp_reply, rows) + r->count * sizeof(fp_row) ? r : NULL;
}
nx_result fp_reply_take(fp_reply *r, int *directory, int *witness, char leaf[AE_FILES_NAME + 1]) {
    if (!directory || !witness || directory == witness || !leaf) return NX_ERROR_ARGUMENT;
    *directory = *witness = -1; leaf[0] = 0;
    if (!r || r->result != AE_FILES_OK || r->directory < 4 || r->witness < 4 || r->directory == r->witness ||
        !memchr(r->leaf, 0, sizeof r->leaf) || !r->leaf[0]) return NX_ERROR_STATE;
    memcpy(leaf, r->leaf, strlen(r->leaf) + 1); *directory = r->directory; *witness = r->witness;
    r->directory = r->witness = -1; return NX_OK;
}
nx_result fp_worker_stop(fp_worker *w) {
    if (!w) return NX_ERROR_ARGUMENT;
    pthread_mutex_lock(&w->lock); w->stop = 1; pthread_cond_signal(&w->changed); pthread_mutex_unlock(&w->lock); return NX_OK;
}
nx_result fp_worker_join(fp_worker *w) {
    if (!w) return NX_ERROR_ARGUMENT;
    pthread_mutex_lock(&w->lock); int stopped = w->stop; pthread_mutex_unlock(&w->lock);
    if (!stopped) return NX_ERROR_STATE;
    if (!w->joined) { if (pthread_join(w->thread, NULL)) return NX_ERROR_PLATFORM; w->joined = 1; }
    return w->failed ? NX_ERROR_STATE : NX_OK;
}
nx_result fp_worker_destroy(fp_worker **worker) {
    if (!worker || !*worker) return NX_ERROR_ARGUMENT;
    fp_worker *w = *worker; (void)fp_worker_stop(w);
    nx_result joined = fp_worker_join(w); if (!w->joined) return joined;
    nx_result result = nx_sim_channel_destroy(w->channel); if (result != NX_OK) return result;
    close(w->wake[0]); close(w->wake[1]); close(w->root);
    pthread_cond_destroy(&w->changed); pthread_mutex_destroy(&w->lock); free(w); *worker = NULL; return joined;
}
