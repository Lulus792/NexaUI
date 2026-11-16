#ifndef CONTENT_NATIVE_WORKER_H
#define CONTENT_NATIVE_WORKER_H
#include "surface.h"
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* Private deterministic producer. Only copied requests cross this boundary;
   neither the surface nor UI context belongs to this worker. */
typedef struct native_job { content_request request; uint64_t sequence; unsigned kind; int held; } native_job;
typedef struct native_worker {
    nx_snapshot_exchange *exchange;
    pthread_mutex_t mutex;
    pthread_cond_t condition;
    pthread_t thread;
    int mutex_ready, condition_ready, started, wake[2];
    int stop, pending, working, prepared, allowed;
    native_job job;
    unsigned publications, discarded;
    nx_result result;
    _Atomic unsigned disposals;
} native_worker;
typedef struct native_worker_state {
    int working, pending, prepared;
    unsigned publications;
    nx_result result;
} native_worker_state;

static void native_dispose(void *user, void *data, size_t bytes) {
    (void)bytes;
    atomic_fetch_add((_Atomic unsigned *)user, 1u);
    free(data);
}
static void native_wake(native_worker *worker) {
    char byte = 'W';
    ssize_t count;
    do { count = write(worker->wake[1], &byte, 1); } while (count < 0 && errno == EINTR);
    if (count != 1 && errno != EAGAIN && errno != EWOULDBLOCK) {
        pthread_mutex_lock(&worker->mutex);
        worker->result = NX_ERROR_PLATFORM;
        pthread_mutex_unlock(&worker->mutex);
    }
}
static void *native_pixels(native_job job, size_t *bytes) {
    if (!job.request.width || !job.request.height || job.request.width > NX_VIEWPORT_MAX_DIMENSION ||
        job.request.height > NX_VIEWPORT_MAX_DIMENSION) return NULL;
    size_t stride = (size_t)job.request.width * 4u;
    if (stride > (4u * 1024u * 1024u) / job.request.height) return NULL;
    *bytes = sizeof(content_frame_header) + stride * job.request.height;
    uint8_t *data = malloc(*bytes);
    if (!data) return NULL;
    content_frame_header header = {.request = job.request, .sequence = job.sequence,
        .format = job.kind == 2 ? NX_BITMAP_A8 : NX_BITMAP_RGBA8, .stride = (uint32_t)stride};
    if (job.kind == 3 && job.request.width >= 4u && job.request.height >= 4u) {
        /* Restore after the rejected frame. The last valid frame was blue. */
        header.damage_base = 1;
        header.damage_x = job.request.width / 2u;
        header.damage_y = job.request.height / 2u;
        header.damage_width = job.request.width / 4u;
        header.damage_height = job.request.height / 4u;
    }
    memcpy(data, &header, sizeof(header));
    static const uint8_t colors[5][3] = {{211,79,31},{23,83,197},{127,127,127},{71,163,31},{191,63,137}};
    if (job.kind > 4) { free(data); return NULL; }
    for (size_t i = sizeof(header); i < *bytes; i += 4u) {
        unsigned kind = job.kind;
        if (header.damage_base) {
            size_t x = ((i - sizeof(header)) % stride) / 4u;
            size_t y = (i - sizeof(header)) / stride;
            if (x < header.damage_x || x >= header.damage_x + header.damage_width ||
                y < header.damage_y || y >= header.damage_y + header.damage_height)
                kind = 1;
        }
        memcpy(data + i, colors[kind], 3);
        data[i + 3u] = 255;
    }
    return data;
}
static void *native_worker_entry(void *user) {
    native_worker *worker = user;
    for (;;) {
        pthread_mutex_lock(&worker->mutex);
        while (!worker->stop && !worker->pending) pthread_cond_wait(&worker->condition, &worker->mutex);
        if (worker->stop) { pthread_mutex_unlock(&worker->mutex); break; }
        native_job job = worker->job;
        worker->pending = 0; worker->working = 1;
        pthread_mutex_unlock(&worker->mutex);
        size_t bytes = 0;
        void *data = native_pixels(job, &bytes);
        pthread_mutex_lock(&worker->mutex);
        worker->prepared = 1;
        pthread_mutex_unlock(&worker->mutex);
        native_wake(worker);
        pthread_mutex_lock(&worker->mutex);
        while (!worker->stop && job.held && !worker->allowed)
            pthread_cond_wait(&worker->condition, &worker->mutex);
        int stop = worker->stop;
        if (stop && data) ++worker->discarded;
        pthread_mutex_unlock(&worker->mutex);
        if (stop) { free(data); break; }
        nx_result result = data ? nx_snapshot_exchange_publish(worker->exchange, data, bytes,
            native_dispose, &worker->disposals) : NX_ERROR_MEMORY;
        if (result != NX_OK) free(data);
        pthread_mutex_lock(&worker->mutex);
        worker->prepared = 0; worker->working = 0; ++worker->publications; worker->result = result;
        pthread_mutex_unlock(&worker->mutex);
        native_wake(worker);
    }
    return NULL;
}
static int native_worker_close(native_worker *worker) {
    int result = 0;
    if (worker->started) {
        pthread_mutex_lock(&worker->mutex);
        worker->stop = 1;
        pthread_cond_broadcast(&worker->condition);
        pthread_mutex_unlock(&worker->mutex);
        /* No UI/engine call remains in this worker; stop releases its gate. */
        if (pthread_join(worker->thread, NULL)) _Exit(1);
        worker->started = 0;
    }
    for (unsigned i = 0; i < 2; ++i) if (worker->wake[i] >= 0) {
        if (close(worker->wake[i])) result = -1;
        worker->wake[i] = -1;
    }
    if (worker->condition_ready) { if (pthread_cond_destroy(&worker->condition)) result = -1; worker->condition_ready = 0; }
    if (worker->mutex_ready) { if (pthread_mutex_destroy(&worker->mutex)) result = -1; worker->mutex_ready = 0; }
    return result;
}
static int native_worker_open(native_worker *worker, nx_snapshot_exchange *exchange) {
    memset(worker, 0, sizeof(*worker)); worker->wake[0] = worker->wake[1] = -1;
    atomic_init(&worker->disposals, 0); worker->exchange = exchange;
    if (pthread_mutex_init(&worker->mutex, NULL)) return -1;
    worker->mutex_ready = 1;
    if (pthread_cond_init(&worker->condition, NULL)) return -1;
    worker->condition_ready = 1;
    if (pipe2(worker->wake, O_CLOEXEC | O_NONBLOCK) ||
        pthread_create(&worker->thread, NULL, native_worker_entry, worker)) return -1;
    worker->started = 1;
    return 0;
}
static native_worker_state native_state(native_worker *worker) {
    pthread_mutex_lock(&worker->mutex);
    native_worker_state state = {worker->working, worker->pending, worker->prepared, worker->publications, worker->result};
    pthread_mutex_unlock(&worker->mutex);
    return state;
}
static int native_schedule(native_worker *worker, native_job job) {
    pthread_mutex_lock(&worker->mutex);
    int accepted = !worker->stop && !worker->pending && !worker->working;
    if (accepted) {
        worker->job = job; worker->pending = 1; worker->prepared = 0; worker->allowed = !job.held;
        pthread_cond_signal(&worker->condition);
    }
    pthread_mutex_unlock(&worker->mutex);
    return accepted;
}
static void native_release(native_worker *worker) {
    pthread_mutex_lock(&worker->mutex); worker->allowed = 1;
    pthread_cond_signal(&worker->condition); pthread_mutex_unlock(&worker->mutex);
}
#endif
