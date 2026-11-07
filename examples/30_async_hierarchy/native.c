#define _POSIX_C_SOURCE 200809L
#include "hierarchy_view.h"
#include "nexaui/software.h"
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <limits.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#ifdef AH_MANIFEST
#include "manifest_source.h"
#endif

/* Two real workers, never given a view/context/window. The neutral fixture
   uses driver-selected observations. AH_MANIFEST reads the original manifest
   contract automatically; optional test permits only hold its real result. */
typedef struct job { uint64_t ticket; ah_key key; int released; nx_result status; size_t count; } job;
typedef struct producer {
    nx_sim_channel *channel;
    pthread_mutex_t lock;
    pthread_cond_t changed;
    job jobs[2];
    unsigned hints, posted;
    int stop, failed, wake;
#ifdef AH_MANIFEST
    const char *manifest;
    unsigned char expected[32];
    int controlled;
#endif
} producer;
static void payload_free(void *user, void *data, size_t bytes) { (void)user; (void)bytes; free(data); }
static void *worker(void *data) {
    producer *p = data;
    for (;;) {
        pthread_mutex_lock(&p->lock);
        while (!p->stop && !p->hints) pthread_cond_wait(&p->changed, &p->lock);
        if (p->stop) { pthread_mutex_unlock(&p->lock); break; }
        --p->hints; pthread_mutex_unlock(&p->lock);
        nx_sim_message command = {0};
        if (nx_sim_channel_take_command(p->channel, &command) != NX_OK || !command.ticket) {
            pthread_mutex_lock(&p->lock); p->failed = 1; pthread_mutex_unlock(&p->lock); break;
        }
        uint64_t ticket = command.ticket; ah_key key = {0};
        int valid = command.bytes == sizeof(key) && command.data;
        if (valid) memcpy(&key, command.data, sizeof(key));
        nx_sim_message_dispose(&command);
#ifdef AH_MANIFEST
        ah_response *response = calloc(1, sizeof(*response));
        nx_result loaded = !response ? NX_ERROR_MEMORY : !valid ? NX_ERROR_ARGUMENT :
            amh_read(p->manifest, p->expected, key, response);
#endif
        pthread_mutex_lock(&p->lock);
        unsigned slot = p->jobs[0].ticket ? 1u : 0u;
        if (p->jobs[slot].ticket) { p->failed = 1; pthread_mutex_unlock(&p->lock); break; }
        p->jobs[slot] = (job){ticket, key, !valid, NX_ERROR_ARGUMENT, 0};
#ifdef AH_MANIFEST
        p->jobs[slot].released = !p->controlled;
        p->jobs[slot].status = loaded;
        printf("MANIFEST ticket=%" PRIu64 " status=%d count=%zu\n", ticket, (int)loaded,
            loaded == NX_OK ? response->count : 0);
#endif
        printf("TAKEN ticket=%" PRIu64 " view=%" PRIu64 " generation=%" PRIu64 "\n", ticket, key.view, key.generation);
        while (!p->stop && !p->jobs[slot].released) pthread_cond_wait(&p->changed, &p->lock);
        nx_result status = p->stop ? NX_ERROR_STATE : p->jobs[slot].status;
        size_t count = p->stop ? 0 : p->jobs[slot].count;
        p->jobs[slot] = (job){0}; pthread_mutex_unlock(&p->lock);
#ifndef AH_MANIFEST
        ah_response *response = calloc(1, sizeof(*response));
        if (!response) status = NX_ERROR_MEMORY;
        else {
            response->key = key; response->count = count;
            for (size_t i = 0; i < count; ++i) {
                response->children[i].id = 100 + i;
                (void)snprintf(response->children[i].name, AH_NAME_BYTES, "Child %c", (int)('A' + i));
            }
        }
#else
        (void)count;
#endif
        nx_result sent = nx_sim_channel_post_completion(p->channel, ticket, status, response,
            response ? sizeof(*response) : 0, response ? payload_free : NULL, NULL);
        if (sent != NX_OK) free(response);
        pthread_mutex_lock(&p->lock);
        if (sent != NX_OK) p->failed = 1;
        else ++p->posted;
        printf("POSTED ticket=%" PRIu64 " status=%d accepted=%d\n", ticket, (int)status, sent == NX_OK);
        pthread_mutex_unlock(&p->lock);
        ssize_t n;
        do { n = write(p->wake, "W\n", 2); } while (n < 0 && errno == EINTR);
        if (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
            pthread_mutex_lock(&p->lock); p->failed = 1; pthread_mutex_unlock(&p->lock);
        }
        if (sent != NX_OK) break;
    }
    return NULL;
}
static int permission(producer *p, const char *line) {
    if (!strcmp(line, "W")) return 1;
    unsigned long long ticket; char extra;
#ifndef AH_MANIFEST
    char outcome[8]; size_t count;
    if (sscanf(line, "F %llu %7s %zu %c", &ticket, outcome, &count, &extra) != 3 ||
        !ticket || (strcmp(outcome, "ok") && strcmp(outcome, "fail")) || count > 2) return 0;
    nx_result status = !strcmp(outcome, "ok") ? NX_OK : NX_ERROR_PLATFORM;
#else
    if (sscanf(line, "F %llu %c", &ticket, &extra) != 1 || !ticket || !p->controlled) return 0;
#endif
    int found = 0;
    pthread_mutex_lock(&p->lock);
    for (unsigned i = 0; i < 2; ++i) if (p->jobs[i].ticket == ticket && !p->jobs[i].released) {
        p->jobs[i].released = 1; found = 1;
#ifndef AH_MANIFEST
        p->jobs[i].status = status; p->jobs[i].count = count;
#endif
    }
    if (found) pthread_cond_broadcast(&p->changed);
    pthread_mutex_unlock(&p->lock);
    return found;
}
static int request(ah_view *v, producer *p, unsigned *submitted) {
    nx_result r = ah_request(v);
    if (r == NX_OK) {
        ++*submitted;
        pthread_mutex_lock(&p->lock); ++p->hints; pthread_cond_broadcast(&p->changed); pthread_mutex_unlock(&p->lock);
    }
    printf("REQUEST ticket=%" PRIu64 " view=%" PRIu64 " generation=%" PRIu64 " status=%d submitted=%u\n",
        v->ticket, v->epoch, v->generation, (int)r, *submitted);
    return r == NX_OK || r == NX_ERROR_LIMIT || r == NX_ERROR_MEMORY;
}
static int fd_argument(const char *s) {
    char *end; errno = 0; long value = strtol(s, &end, 10);
    return !errno && *s && !*end && value >= 0 && value <= INT_MAX ? (int)value : -1;
}
static int set_theme(nx_context *ctx, int kind) {
    nx_theme theme;
    return nx_theme_preset((nx_theme_kind)kind, &theme) == NX_OK &&
        nx_context_set_theme(ctx, &theme) == NX_OK && nx_context_set_reduced_motion(ctx, 1) == NX_OK;
}
#define REQUIRE(x) do { if (!(x)) { fprintf(stderr, "async-host:%d: %s\n", __LINE__, #x); okay = 0; goto cleanup; } } while (0)
int main(int argc, char **argv) {
    int okay = 1, read_fd = -1, write_fd = -1, lock_ready = 0, cond_ready = 0;
    unsigned started = 0, submitted = 0, adopted = 0, ignored = 0, drained = 0;
    pthread_t threads[2]; producer p = {0}; ah_view view = {0};
    nx_sim_channel *channel = NULL; nx_context *ctx = NULL; nx_window *window = NULL; nx_software_view *renderer = NULL;
    nx_font *font = NULL; nx_font_collection *fonts = NULL; unsigned char *font_bytes = NULL; FILE *file = NULL;
    int done = 0, dirty = 1, announced = 0;
    uint64_t epoch = 1, frames = 0, polls = 0, keys = 0;
    int german = getenv("AH_TEST_GERMAN") != NULL, large = getenv("AH_TEST_LARGE") != NULL;
    int theme = getenv("AH_TEST_CONTRAST") ? NX_THEME_HIGH_CONTRAST : getenv("AH_TEST_LIGHT") ? NX_THEME_LIGHT : NX_THEME_DARK;
    int small = getenv("AH_TEST_SMALL") != NULL;
    char pending[256]; size_t used = 0;
    setvbuf(stdout, NULL, _IONBF, 0);
#ifndef AH_MANIFEST
    REQUIRE(argc == 4 && (read_fd = fd_argument(argv[2])) >= 0 &&
        (write_fd = fd_argument(argv[3])) >= 0 && read_fd != write_fd);
#else
    REQUIRE((argc == 4 || argc == 6) && amh_digest(argv[3], p.expected));
    p.manifest = argv[2]; p.controlled = argc == 6;
    if (p.controlled) {
        REQUIRE((read_fd = fd_argument(argv[4])) >= 0 && (write_fd = fd_argument(argv[5])) >= 0 && read_fd != write_fd);
    } else {
        int wake_pipe[2]; REQUIRE(pipe(wake_pipe) == 0);
        read_fd = wake_pipe[0]; write_fd = wake_pipe[1];
        REQUIRE(fcntl(read_fd, F_SETFD, FD_CLOEXEC) == 0 && fcntl(write_fd, F_SETFD, FD_CLOEXEC) == 0);
    }
#endif
    REQUIRE(fcntl(read_fd, F_SETFL, fcntl(read_fd, F_GETFL) | O_NONBLOCK) == 0 &&
        fcntl(write_fd, F_SETFL, fcntl(write_fd, F_GETFL) | O_NONBLOCK) == 0);
    REQUIRE((file = fopen(argv[1], "rb")) != NULL && !fseek(file, 0, SEEK_END));
    long length = ftell(file); REQUIRE(length > 0 && length < 64000000 && !fseek(file, 0, SEEK_SET));
    font_bytes = malloc((size_t)length); REQUIRE(font_bytes && fread(font_bytes, 1, (size_t)length, file) == (size_t)length);
    int closed = fclose(file); file = NULL; REQUIRE(!closed);
    REQUIRE(nx_font_open(font_bytes, (size_t)length, NULL, &font) == NX_OK &&
        nx_font_collection_create(NULL, &fonts) == NX_OK && nx_font_collection_add(fonts, font) == NX_OK);
    REQUIRE(nx_sim_channel_create(2, NULL, &channel) == NX_OK && nx_context_create(NULL, &ctx) == NX_OK && set_theme(ctx, theme));
    REQUIRE(ah_init(&view, channel, fonts, epoch) == NX_OK); view.german = german; view.text_size = large ? 32 : 16;
    nx_window_desc desc = {"NexaUI asynchronous hierarchy", small ? 360 : 680, small ? 260 : 400, 0, {0}};
#ifdef AH_MANIFEST
    desc.title_utf8 = "Aetheri manifest declared file paths";
#endif
    REQUIRE(nx_window_create(&desc, &window) == NX_OK && nx_software_view_open(window, &renderer) == NX_OK);
    p.channel = channel; p.wake = write_fd;
    REQUIRE(pthread_mutex_init(&p.lock, NULL) == 0); lock_ready = 1;
    REQUIRE(pthread_cond_init(&p.changed, NULL) == 0); cond_ready = 1;
    for (unsigned i = 0; i < 2; ++i) { REQUIRE(pthread_create(&threads[i], NULL, worker, &p) == 0); ++started; }
    while (!done) {
        nx_sim_message completion = {0};
        for (;;) {
            REQUIRE(nx_sim_channel_take_completion(channel, &completion) == NX_OK);
            if (!completion.ticket) break;
            int accepted = ah_observe(&view, &completion);
            if (accepted) { ++adopted; dirty = 1; } else ++ignored;
            printf("OBSERVE ticket=%" PRIu64 " accepted=%d current=%" PRIu64 " phase=%d revision=%" PRIu64 "\n",
                completion.ticket, accepted, view.ticket, (int)view.phase, view.revision);
            nx_sim_message_dispose(&completion);
        }
        nx_window_info info; REQUIRE(nx_window_get_info(window, &info) == NX_OK);
        if (info.should_close) break;
        if (dirty) {
            REQUIRE(nx_begin_frame(ctx, info.width, info.height, 0) == NX_OK);
            nx_semantic_desc root = {0}; root.id = 1; root.role = NX_ROLE_WINDOW;
            root.name = (nx_utf8_span){desc.title_utf8, strlen(desc.title_utf8)}; root.bounds = (nx_rect){0, 0, info.width, info.height};
            int intent, redraw;
            REQUIRE(nx_semantics_add(ctx, &root, NULL) == NX_OK &&
                ah_build(&view, ctx, 1, (nx_rect){8, 8, info.width - 16, info.height - 16}, &intent, &redraw) == NX_OK && nx_end_frame(ctx) == NX_OK);
            if (intent) { REQUIRE(request(&view, &p, &submitted)); continue; }
            if (redraw) continue;
            REQUIRE(nx_window_publish_semantics(window, ctx) == NX_OK);
            const nx_command *commands; size_t count; nx_color background; nx_software_view_outcome outcome;
            REQUIRE(nx_frame_commands(ctx, &commands, &count) == NX_OK && nx_theme_color(ctx, NX_COLOR_BACKGROUND, &background) == NX_OK &&
                nx_software_view_render(renderer, commands, count, background, &outcome) == NX_OK);
            dirty = outcome != NX_SOFTWARE_VIEW_READY; ++frames;
            printf("VIEW phase=%d expanded=%d ticket=%" PRIu64 " view=%" PRIu64 " generation=%" PRIu64 " revision=%" PRIu64
                " count=%zu selected=%" PRIu64 " focus=%" PRIu64 " frames=%" PRIu64 " polls=%" PRIu64 " keys=%" PRIu64 "\n",
                (int)view.phase, view.expanded, view.ticket, view.epoch, view.generation, view.revision,
                view.count, view.selected, nx_focus_id(ctx), frames, polls, keys);
            if (!dirty && !announced) { puts("ASYNC_READY"); announced = 1; }
        }
        uint32_t ready = 0; ++polls;
        REQUIRE(nx_window_poll_with_fd(window, UINT32_MAX, read_fd, &ready) == NX_OK);
        REQUIRE(!(ready & NX_WINDOW_FD_ERROR));
        if (ready & (NX_WINDOW_FD_READABLE | NX_WINDOW_FD_HANGUP)) {
            char bytes[128]; ssize_t n;
            while ((n = read(read_fd, bytes, sizeof(bytes))) > 0) for (ssize_t i = 0; i < n; ++i) {
                if (bytes[i] == '\n') { pending[used] = 0; REQUIRE(permission(&p, pending)); used = 0; }
                else { REQUIRE(used + 1 < sizeof(pending)); pending[used++] = bytes[i]; }
            }
            REQUIRE(n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK));
        }
        nx_event event; int available;
        do {
            REQUIRE(nx_window_next_event(window, &event, &available) == NX_OK);
            if (!available) break;
            if (event.type == NX_EVENT_KEY_DOWN || event.type == NX_EVENT_KEY_UP) ++keys;
            int special = event.type == NX_EVENT_KEY_DOWN && !event.repeat;
            if (special && event.key == NX_KEY_ESCAPE) { done = 1; break; }
            if (special && event.key == NX_KEY_F4) {
                REQUIRE(epoch != UINT64_MAX && nx_window_publish_semantics(window, NULL) == NX_OK);
                ah_destroy(&view); nx_context_destroy(ctx); ctx = NULL;
                REQUIRE(nx_context_create(NULL, &ctx) == NX_OK && set_theme(ctx, theme));
                REQUIRE(ah_init(&view, channel, fonts, ++epoch) == NX_OK); view.german = german; view.text_size = large ? 32 : 16;
                printf("REPLACED view=%" PRIu64 "\n", epoch); dirty = 1; continue;
            }
            if (special && event.key == NX_KEY_F5 && view.expanded) { REQUIRE(request(&view, &p, &submitted)); dirty = 1; continue; }
            REQUIRE(nx_context_queue_event(ctx, &event) == NX_OK); dirty = 1;
        } while (available);
        size_t routed = 0; REQUIRE(nx_window_route_events(window, ctx, &routed) == NX_OK);
        if (routed) dirty = 1;
    }
cleanup:
    if (window && nx_window_publish_semantics(window, NULL) != NX_OK) okay = 0;
    if (renderer && nx_software_view_close(&renderer) != NX_OK) okay = 0;
    ah_destroy(&view); nx_context_destroy(ctx); if (window) nx_window_destroy(window);
    puts("GUI_GONE");
    if (lock_ready && cond_ready) {
        pthread_mutex_lock(&p.lock); p.stop = 1; pthread_cond_broadcast(&p.changed); pthread_mutex_unlock(&p.lock);
    }
    for (unsigned i = 0; i < started; ++i) if (pthread_join(threads[i], NULL)) okay = 0;
    if (p.failed) okay = 0;
    if (channel) {
        nx_sim_message m = {0};
        while (nx_sim_channel_take_completion(channel, &m) == NX_OK && m.ticket) { nx_sim_message_dispose(&m); ++drained; }
        if (nx_sim_channel_destroy(channel) != NX_OK) okay = 0;
    }
    if (cond_ready) pthread_cond_destroy(&p.changed);
    if (lock_ready) pthread_mutex_destroy(&p.lock);
    if (read_fd >= 0) close(read_fd);
    if (write_fd >= 0) close(write_fd);
    nx_font_collection_destroy(fonts); nx_font_close(font); free(font_bytes); if (file) fclose(file);
    printf("ASYNC_CLOSED submitted=%u posted=%u adopted=%u ignored=%u drained=%u joined=%u failed=%d\n",
        submitted, p.posted, adopted, ignored, drained, started, !okay);
    return okay ? 0 : 1;
}
