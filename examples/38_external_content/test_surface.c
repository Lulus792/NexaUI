#define _POSIX_C_SOURCE 200809L
#include "surface.h"
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
typedef HANDLE test_thread;
#else
#include <pthread.h>
#include <time.h>
typedef pthread_t test_thread;
#endif

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "content line %d: %s\n", __LINE__, #x); goto done; } } while (0)
typedef struct allocation_probe { size_t live, calls; int fail_next, bad; } allocation_probe;
static void *probe_allocate(void *user, size_t bytes) {
    allocation_probe *probe = user;
    if (probe->fail_next) { probe->fail_next = 0; return NULL; }
    void *data = malloc(bytes);
    if (data) { probe->live += bytes; ++probe->calls; }
    return data;
}
static void probe_free(void *user, void *data, size_t bytes) {
    allocation_probe *probe = user;
    if (bytes > probe->live) probe->bad = 1;
    else probe->live -= bytes;
    free(data);
}
static void dispose(void *user, void *data, size_t bytes) {
    (void)bytes;
    atomic_fetch_add((_Atomic unsigned *)user, 1u);
    free(data);
}
static void color(uint8_t *pixel, uint8_t value) {
    pixel[0] = value; pixel[1] = value ^ 0x55u; pixel[2] = value ^ 0xa3u; pixel[3] = 255;
}
static void *make_frame(content_request request, uint64_t sequence, uint32_t stride,
    uint8_t value, size_t *bytes) {
    *bytes = sizeof(content_frame_header) + (size_t)stride * request.height;
    uint8_t *data = malloc(*bytes);
    if (!data) return NULL;
    memset(data, 0xee, *bytes);
    content_frame_header header = {
        .request = request, .sequence = sequence, .format = NX_BITMAP_RGBA8, .stride = stride};
    memcpy(data, &header, sizeof(header));
    for (uint32_t y = 0; y < request.height; ++y)
        for (uint32_t x = 0; x < request.width; ++x)
            color(data + sizeof(header) + (size_t)y * stride + (size_t)x * 4u, value);
    return data;
}
static int publish(nx_snapshot_exchange *exchange, content_request request, uint64_t sequence,
    uint8_t value, _Atomic unsigned *disposals) {
    size_t bytes = 0;
    void *data = make_frame(request, sequence, request.width * 4u + 8u, value, &bytes);
    if (!data) return 0;
    nx_result result = nx_snapshot_exchange_publish(exchange, data, bytes, dispose, disposals);
    if (result != NX_OK) free(data);
    return result == NX_OK;
}
static int host_button(nx_context *context) {
    nx_control_desc button = {.id = 1, .bounds = {0, 0, 32, 12}, .name = {"Host controls", 13}};
    nx_interaction_state state;
    return nx_button(context, &button, &state) == NX_OK;
}
static int record(content_surface *surface, nx_snapshot_exchange *exchange, nx_context *context,
    const nx_viewport_desc *desc, nx_result wanted, int *leased) {
    *leased = 0;
    if (nx_begin_frame(context, 64, 64, 0) != NX_OK || !host_button(context)) return 0;
    nx_interaction_state interaction;
    nx_result result = content_surface_draw(surface, context, desc, &interaction);
    if (result == NX_OK && desc->bounds.width > 0 && desc->bounds.height > 0) *leased = 1;
    if (result != wanted) { fprintf(stderr, "draw result %d wanted %d\n", (int)result, (int)wanted); return 0; }
    /* Even while viewport pixels remain leased, no snapshot borrow may
       survive to end_frame. A second acquire must work RIGHT NOW. */
    nx_snapshot_view borrow;
    if (nx_snapshot_exchange_acquire(exchange, &borrow) != NX_OK) {
        fputs("Snapshot borrow still open before end_frame\n", stderr);
        return 0;
    }
    if (borrow.data && nx_snapshot_exchange_release(exchange, &borrow) != NX_OK) return 0;
    if (nx_end_frame(context) != NX_OK) return 0;
    size_t semantics = 0;
    return nx_semantics_count(context, &semantics) == NX_OK && semantics == (*leased ? 2u : 1u);
}
static int raster(nx_context *context, uint8_t *pixels) {
    const nx_command *commands = NULL; size_t count = 0;
    return nx_frame_commands(context, &commands, &count) == NX_OK &&
        nx_headless_render(commands, count, pixels, 64, 64, 64u * 4u, (nx_color){0, 0, 0, 1}) == NX_OK;
}
static int image_matches_bitmap(nx_context *context, const nx_viewport_desc *desc,
    nx_bitmap_view bitmap) {
    uint8_t actual[64u * 64u * 4u], expected[64u * 64u * 4u];
    nx_context *reference = NULL;
    int okay = nx_context_create(NULL, &reference) == NX_OK;
    okay = okay && nx_begin_frame(reference, 64, 64, 0) == NX_OK && host_button(reference) &&
        nx_draw_bitmap(reference, desc->bounds, bitmap, (nx_color){1, 1, 1, 1}) == NX_OK &&
        nx_end_frame(reference) == NX_OK && raster(context, actual) && raster(reference, expected) &&
        !memcmp(actual, expected, sizeof(actual));
    nx_context_destroy(reference);
    return okay;
}
static int image_matches(nx_context *context, const nx_viewport_desc *desc, uint8_t value) {
    uint8_t source[64u * 64u * 4u];
    for (size_t i = 0; i < sizeof(source); i += 4u) color(source + i, value);
    nx_bitmap_view bitmap = {source, 64, 64, 64u * 4u, NX_BITMAP_RGBA8};
    return image_matches_bitmap(context, desc, bitmap);
}

static int publish_corner(nx_snapshot_exchange *exchange, content_request request,
    uint64_t sequence, uint64_t base, uint8_t background, uint8_t corner,
    _Atomic unsigned *disposals) {
    size_t bytes;
    uint32_t stride = request.width * 4u + 12u;
    uint8_t *data = make_frame(request, sequence, stride, background, &bytes);
    if (!data) return 0;
    content_frame_header *header = (content_frame_header *)data;
    header->damage_base = base;
    header->damage_x = request.width - 3u;
    header->damage_y = request.height - 2u;
    header->damage_width = 3;
    header->damage_height = 2;
    for (uint32_t y = header->damage_y; y < request.height; ++y)
        for (uint32_t x = header->damage_x; x < request.width; ++x)
            color(data + sizeof(*header) + (size_t)y * stride + (size_t)x * 4u, corner);
    nx_result result = nx_snapshot_exchange_publish(exchange, data, bytes, dispose, disposals);
    if (result != NX_OK) free(data);
    return result == NX_OK;
}

static int image_matches_corner(nx_context *context, const nx_viewport_desc *desc,
    uint8_t background, uint8_t corner) {
    uint8_t expected[16u * 12u * 4u];
    for (unsigned i = 0; i < 16u * 12u; ++i)
        color(expected + i * 4u, (i >= 160u && i % 16u >= 13u) ? corner : background);
    nx_bitmap_view bitmap = {expected, 16, 12, 64, NX_BITMAP_RGBA8};
    return image_matches_bitmap(context, desc, bitmap);
}
static void pause_ms(void) {
#ifdef _WIN32
    Sleep(1);
#else
    const struct timespec delay = {0, 1000000};
    nanosleep(&delay, NULL);
#endif
}
typedef struct worker_probe {
    nx_snapshot_exchange *exchange;
    content_request request;
    _Atomic unsigned *disposals;
    atomic_int ready, go;
    nx_result result;
} worker_probe;
static void worker_run(worker_probe *worker) {
    size_t bytes = 0;
    void *data = make_frame(worker->request, 1, worker->request.width * 4u + 8u, 70, &bytes);
    worker->result = NX_ERROR_MEMORY;
    atomic_store(&worker->ready, 1);
    for (unsigned i = 0; i < 2000 && !atomic_load(&worker->go); ++i) pause_ms();
    if (data) {
        worker->result = atomic_load(&worker->go) ?
            nx_snapshot_exchange_publish(worker->exchange, data, bytes, dispose, worker->disposals) : NX_ERROR_STATE;
        if (worker->result != NX_OK) free(data);
    }
}
#ifdef _WIN32
static DWORD WINAPI worker_entry(void *user) { worker_run(user); return 0; }
static int start(test_thread *thread, worker_probe *worker) {
    *thread = CreateThread(NULL, 0, worker_entry, worker, 0, NULL); return *thread != NULL;
}
static int join(test_thread thread) {
    DWORD result = WaitForSingleObject(thread, 5000);
    if (result != WAIT_OBJECT_0) {
        /* A failed test cannot free the stack/exchange under a live worker. */
        fputs("Owned content worker did not join\n", stderr);
        _Exit(1);
    }
    CloseHandle(thread);
    return 1;
}
#else
static void *worker_entry(void *user) { worker_run(user); return NULL; }
static int start(test_thread *thread, worker_probe *worker) { return pthread_create(thread, NULL, worker_entry, worker) == 0; }
static int join(test_thread thread) { return pthread_join(thread, NULL) == 0; }
#endif

int main(int argc, char **argv) {
    int result = 1, leased = 0, started = 0;
    nx_snapshot_exchange *exchange = NULL;
    nx_context *context = NULL;
    content_surface *surface = NULL;
    allocation_probe probe = {0};
    nx_allocator allocator = {&probe, probe_allocate, probe_free};
    _Atomic unsigned disposals = 0;
    worker_probe worker = {0}; atomic_init(&worker.ready, 0); atomic_init(&worker.go, 0);
    test_thread thread;
    content_surface_status status = {0};
    nx_viewport_desc desc = {.id = 9, .bounds = {8, 24, 16, 12}, .name = {"Fixture content", 15}, .scale = 1};
    CHECK(argc == 2);
    CHECK(nx_snapshot_exchange_create(NULL, &exchange) == NX_OK && nx_context_create(NULL, &context) == NX_OK);
    CHECK(content_surface_create(exchange, 11, 65536, &allocator, &surface) == NX_OK);
    CHECK(record(surface, exchange, context, &desc, NX_ERROR_STATE, &leased));
    CHECK(content_surface_get_status(surface, &status) == NX_OK && status.request.resize == 1);
    content_request request = status.request;
    if (!strcmp(argv[1], "flow")) {
        CHECK(publish(exchange, request, 1, 30, &disposals));
        CHECK(record(surface, exchange, context, &desc, NX_OK, &leased));
        /* Producer retirement is legal now. Command pixels must survive it. */
        CHECK(publish(exchange, request, 2, 90, &disposals) && atomic_load(&disposals) == 1);
        CHECK(image_matches(context, &desc, 30));
        CHECK(content_surface_complete(surface) == NX_OK); leased = 0;
        CHECK(record(surface, exchange, context, &desc, NX_OK, &leased) && image_matches(context, &desc, 90));
        CHECK(content_surface_complete(surface) == NX_OK); leased = 0;
        size_t allocations = probe.calls;
        CHECK(record(surface, exchange, context, &desc, NX_OK, &leased));
        CHECK(content_surface_get_status(surface, &status) == NX_OK && status.copied_frames == 2 && probe.calls == allocations);
        CHECK(content_surface_complete(surface) == NX_OK); leased = 0;
        desc.scale = 2;
        CHECK(record(surface, exchange, context, &desc, NX_ERROR_STATE, &leased));
        CHECK(content_surface_get_status(surface, &status) == NX_OK && status.request.resize == 2 && status.request.width == 32);
        request = status.request;
        CHECK(publish(exchange, request, 1, 140, &disposals));
        CHECK(record(surface, exchange, context, &desc, NX_OK, &leased) && image_matches(context, &desc, 140));
        CHECK(content_surface_complete(surface) == NX_OK); leased = 0;
        desc.bounds.width = 0;
        CHECK(record(surface, exchange, context, &desc, NX_OK, &leased));
        CHECK(content_surface_get_status(surface, &status) == NX_OK && status.request.resize == 3 && !status.staging_bytes);
        CHECK(publish(exchange, request, 2, 160, &disposals));
        desc.bounds.width = 16; desc.scale = 1;
        CHECK(record(surface, exchange, context, &desc, NX_ERROR_STATE, &leased));
        CHECK(content_surface_get_status(surface, &status) == NX_OK && status.request.resize == 4);
        request = status.request;
        CHECK(publish(exchange, request, 1, 200, &disposals));
        CHECK(record(surface, exchange, context, &desc, NX_OK, &leased) && image_matches(context, &desc, 200));
        CHECK(content_surface_complete(surface) == NX_OK); leased = 0;
        desc.bounds.width = 4096;
        CHECK(record(surface, exchange, context, &desc, NX_ERROR_LIMIT, &leased));
        CHECK(content_surface_get_status(surface, &status) == NX_OK && status.request.resize == 4 && status.sequence == 1);
        desc.bounds.width = 16;
        CHECK(record(surface, exchange, context, &desc, NX_OK, &leased) && image_matches(context, &desc, 200));
    } else if (!strcmp(argv[1], "damage")) {
        CHECK(publish(exchange, request, 1, 30, &disposals));
        CHECK(record(surface, exchange, context, &desc, NX_OK, &leased));
        CHECK(content_surface_complete(surface) == NX_OK); leased = 0;
        CHECK(content_surface_get_status(surface, &status) == NX_OK && status.last_copy_bytes == 768);
        size_t allocations = probe.calls;
        CHECK(publish_corner(exchange, request, 2, 1, 30, 70, &disposals));
        CHECK(record(surface, exchange, context, &desc, NX_OK, &leased));
        CHECK(content_surface_get_status(surface, &status) == NX_OK &&
            status.sequence == 2 && status.copied_frames == 2 &&
            status.last_copy_bytes == 24 && probe.calls == allocations);
        CHECK(image_matches_corner(context, &desc, 30, 70));
        CHECK(content_surface_complete(surface) == NX_OK); leased = 0;
        CHECK(record(surface, exchange, context, &desc, NX_OK, &leased));
        CHECK(content_surface_get_status(surface, &status) == NX_OK && status.copied_frames == 2);
        /* Two producer publications overtake the renderer while its old
           staging lease is live. The last rectangle refers to unseen frame 3. */
        CHECK(publish(exchange, request, 3, 90, &disposals));
        CHECK(publish_corner(exchange, request, 4, 3, 90, 120, &disposals));
        CHECK(image_matches_corner(context, &desc, 30, 70));
        CHECK(content_surface_complete(surface) == NX_OK); leased = 0;
        CHECK(record(surface, exchange, context, &desc, NX_OK, &leased));
        CHECK(content_surface_get_status(surface, &status) == NX_OK &&
            status.sequence == 4 && status.copied_frames == 3 &&
            status.last_copy_bytes == 768 && probe.calls == allocations);
        CHECK(image_matches_corner(context, &desc, 90, 120));
        CHECK(atomic_load(&disposals) == 3);
        printf("staging full=768 bytes, matching 3x2 patch=24 bytes, missed base=768 bytes\n");
    } else if (!strcmp(argv[1], "malformed")) {
        for (unsigned variant = 0; variant < 14; ++variant) {
            size_t bytes = 0;
            void *data = make_frame(request, 1, request.width * 4u, 90, &bytes);
            CHECK(data);
            content_frame_header *header = data;
            nx_result wanted = NX_ERROR_ARGUMENT;
            switch (variant) {
            case 0: bytes = 1; break;
            case 1: ++header->request.generation; wanted = NX_ERROR_STATE; break;
            case 2: ++header->request.resize; wanted = NX_ERROR_STATE; break;
            case 3: ++header->request.width; break;
            case 4: header->sequence = 0; break;
            case 5: header->format = NX_BITMAP_A8; wanted = NX_ERROR_UNSUPPORTED; break;
            case 6: header->stride = 0; break;
            case 7: header->stride = UINT32_MAX; wanted = NX_ERROR_LIMIT; break;
            case 8: --bytes; break;
            case 9: header->damage_width = 1; break;
            case 10: header->damage_base = 1; header->damage_width = 1; header->damage_height = 1; break;
            default:
                header->sequence = 2; header->damage_base = 1;
                header->damage_width = 1; header->damage_height = 1;
                if (variant == 11) header->damage_x = request.width;
                else if (variant == 12) header->damage_width = UINT32_MAX;
                else header->damage_height = 0;
                break;
            }
            nx_result published = nx_snapshot_exchange_publish(exchange, data, bytes, dispose, &disposals);
            if (published != NX_OK) free(data);
            CHECK(published == NX_OK && record(surface, exchange, context, &desc, wanted, &leased));
        }
        CHECK(publish(exchange, request, 10, 90, &disposals));
        CHECK(record(surface, exchange, context, &desc, NX_OK, &leased) && image_matches(context, &desc, 90));
        CHECK(content_surface_complete(surface) == NX_OK); leased = 0;
        CHECK(publish(exchange, request, 9, 100, &disposals));
        CHECK(record(surface, exchange, context, &desc, NX_ERROR_STATE, &leased));
        CHECK(content_surface_get_status(surface, &status) == NX_OK && status.sequence == 10 && status.copied_frames == 1);
        CHECK(publish(exchange, request, 11, 110, &disposals));
        CHECK(record(surface, exchange, context, &desc, NX_OK, &leased) && image_matches(context, &desc, 110));
    } else if (!strcmp(argv[1], "lifetime")) {
        probe.fail_next = 1; desc.scale = 2;
        CHECK(record(surface, exchange, context, &desc, NX_ERROR_MEMORY, &leased));
        CHECK(content_surface_get_status(surface, &status) == NX_OK && status.request.resize == 1 && status.request.width == 16);
        desc.scale = 1;
        CHECK(publish(exchange, request, 1, 50, &disposals));
        CHECK(record(surface, exchange, context, &desc, NX_OK, &leased));
        content_surface *original = surface;
        CHECK(content_surface_destroy(&surface) == NX_ERROR_STATE && surface == original);
        CHECK(image_matches(context, &desc, 50) && content_surface_complete(surface) == NX_OK); leased = 0;
        CHECK(content_surface_complete(surface) == NX_ERROR_STATE && content_surface_destroy(&surface) == NX_OK);
        CHECK(!surface && probe.live == 0);
        CHECK(publish(exchange, request, 2, 70, &disposals));
        CHECK(content_surface_create(exchange, 12, 65536, &allocator, &surface) == NX_OK);
        CHECK(record(surface, exchange, context, &desc, NX_ERROR_STATE, &leased));
        CHECK(content_surface_get_status(surface, &status) == NX_OK && status.request.generation == 12);
        CHECK(publish(exchange, status.request, 1, 100, &disposals));
        CHECK(record(surface, exchange, context, &desc, NX_OK, &leased) && image_matches(context, &desc, 100));
    } else if (!strcmp(argv[1], "worker")) {
        worker.exchange = exchange; worker.request = request; worker.disposals = &disposals;
        CHECK(start(&thread, &worker)); started = 1;
        for (unsigned i = 0; i < 2000 && !atomic_load(&worker.ready); ++i) pause_ms();
        CHECK(atomic_load(&worker.ready));
        CHECK(content_surface_destroy(&surface) == NX_OK);
        nx_context_destroy(context); context = NULL;
        CHECK(nx_context_create(NULL, &context) == NX_OK && content_surface_create(exchange, 12, 65536, &allocator, &surface) == NX_OK);
        CHECK(record(surface, exchange, context, &desc, NX_ERROR_STATE, &leased));
        atomic_store(&worker.go, 1);
        int joined = join(thread); started = 0;
        CHECK(joined && worker.result == NX_OK);
        CHECK(record(surface, exchange, context, &desc, NX_ERROR_STATE, &leased));
        CHECK(content_surface_get_status(surface, &status) == NX_OK && status.request.generation == 12 && !status.copied_frames);
        CHECK(publish(exchange, status.request, 1, 110, &disposals));
        CHECK(record(surface, exchange, context, &desc, NX_OK, &leased) && image_matches(context, &desc, 110));
    } else CHECK(0);
    result = 0;
done:
    if (started) { atomic_store(&worker.go, 1); if (!join(thread)) result = 1; }
    if (leased && content_surface_complete(surface) != NX_OK) result = 1;
    if (content_surface_destroy(&surface) != NX_OK) result = 1;
    nx_context_destroy(context);
    if (exchange && nx_snapshot_exchange_destroy(exchange) != NX_OK) result = 1;
    if (probe.live || probe.bad) result = 1;
    if (!result) printf("External CPU content: %s passed; bounded staging, original APIs, no engine admission\n", argv[1]);
    return result;
}
