#define _POSIX_C_SOURCE 200809L
#include "manifest_source.h"
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "manifest-test:%d: %s\n", __LINE__, #x); return 1; } } while (0)
static char directory[4096];
static nx_font_collection *test_fonts;
static int path(char out[4096], const char *name) {
    int n = snprintf(out, 4096, "%s/%s", directory, name); return n > 0 && n < 4096;
}
static int digest(const char *name, unsigned char out[32]) {
    char file[4096], text[65] = {0}; if (!path(file, name)) return 0;
    FILE *f = fopen(file, "rb"); if (!f) return 0;
    size_t n = fread(text, 1, 64, f); int r = fclose(f);
    return n == 64 && !r && amh_digest(text, out);
}
static unsigned fd_count(void) {
    DIR *d = opendir("/proc/self/fd"); if (!d) return 0;
    unsigned n = 0; while (readdir(d)) ++n; closedir(d); return n;
}
static void dispose(void *user, void *data, size_t bytes) { (void)user; (void)bytes; free(data); }
static int build(nx_context *ctx, ah_view *v) {
    int intent, redraw;
    return nx_begin_frame(ctx, 680, 400, 0) == NX_OK &&
        ah_build(v, ctx, 0, (nx_rect){8, 8, 664, 384}, &intent, &redraw) == NX_OK && nx_end_frame(ctx) == NX_OK;
}
static int projection(void) {
    ah_key key = {12, 3, AH_BRANCH}; unsigned char expected[32]; char file[4096];
    ah_response output, before; memset(&output, 0xa5, sizeof output); before = output;
    CHECK(digest("frozen.sha256", expected) && path(file, "frozen.bin"));
    CHECK(amh_read(file, expected, key, &output) == NX_OK && output.count == 2 &&
        output.key.view == 12 && output.key.generation == 3 && output.children[0].id == 100 &&
        !strcmp(output.children[0].name, "bin/probe") && !strcmp(output.children[1].name, "share/message"));
    unsigned char bytes[131072]; FILE *f = fopen(file, "rb"); CHECK(f);
    size_t used = fread(bytes, 1, sizeof bytes, f); CHECK(!ferror(f) && !fclose(f));
    CHECK(amh_project(bytes, used, expected, key, &output) == NX_OK);
    memset(bytes, 0, used); CHECK(!strcmp(output.children[1].name, "share/message"));
    output = before; expected[0] ^= 1;
    CHECK(amh_read(file, expected, key, &output) == NX_ERROR_ARGUMENT && !memcmp(&output, &before, sizeof output));
    const char *names[] = {"malformed", "oversize"};
    for (unsigned i = 0; i < 2; ++i) {
        char name[64]; snprintf(name, sizeof name, "%s.sha256", names[i]); CHECK(digest(name, expected));
        snprintf(name, sizeof name, "%s.bin", names[i]); CHECK(path(file, name));
        CHECK(amh_read(file, expected, key, &output) == (i ? NX_ERROR_LIMIT : NX_ERROR_ARGUMENT));
        CHECK(!memcmp(&output, &before, sizeof output));
    }
    CHECK(digest("frozen.sha256", expected));
    const char *bad[] = {"link", "fifo", "absent", "."};
    unsigned descriptors = fd_count(); CHECK(descriptors);
    for (unsigned n = 0; n < 25; ++n) for (unsigned i = 0; i < 4; ++i) {
        CHECK(path(file, bad[i]) && amh_read(file, expected, key, &output) == NX_ERROR_PLATFORM);
        CHECK(!memcmp(&output, &before, sizeof output));
    }
    CHECK(fd_count() == descriptors);
    CHECK(amh_read(NULL, expected, key, &output) == NX_ERROR_ARGUMENT &&
        amh_project(bytes, 0, expected, (ah_key){0}, &output) == NX_ERROR_ARGUMENT);
    unsigned char unchanged[32]; memset(unchanged, 0xa5, sizeof unchanged); memcpy(expected, unchanged, sizeof expected);
    CHECK(!amh_digest("wrong", expected) && !memcmp(expected, unchanged, sizeof expected));
    puts("MANIFEST_PROJECTION_VERIFIED"); return 0;
}
static int whole_model(void) {
    nx_sim_channel *channel = NULL; nx_context *ctx = NULL; ah_view v; unsigned char expected[32]; char file[4096];
    CHECK(digest("maximum.sha256", expected) && path(file, "maximum.bin"));
    CHECK(nx_context_create(NULL, &ctx) == NX_OK && nx_sim_channel_create(2, NULL, &channel) == NX_OK &&
        ah_init(&v, channel, test_fonts, 4) == NX_OK);
    v.expanded = 1; CHECK(ah_request(&v) == NX_OK);
    nx_sim_message command = {0}; CHECK(nx_sim_channel_take_command(channel, &command) == NX_OK && command.ticket);
    ah_key key; memcpy(&key, command.data, sizeof key);
    ah_response *response = malloc(sizeof(*response)); CHECK(response && amh_read(file, expected, key, response) == NX_OK);
    CHECK(response->count == 32 && strlen(response->children[31].name) == 256);
    for (size_t i = 0; i < 32; ++i) CHECK(response->children[i].id == 100 + i);
    uint64_t ticket = command.ticket; nx_sim_message_dispose(&command);
    CHECK(nx_sim_channel_post_completion(channel, ticket, NX_OK, response, sizeof(*response), dispose, NULL) == NX_OK);
    nx_sim_message completion = {0}; CHECK(nx_sim_channel_take_completion(channel, &completion) == NX_OK && ah_observe(&v, &completion));
    nx_sim_message_dispose(&completion); CHECK(build(ctx, &v) && v.phase == AH_READY && v.count == 32);
    uint32_t index; nx_semantic_view item;
    CHECK(nx_semantics_find(ctx, 131, &index) == NX_OK && nx_semantics_get(ctx, index, &item) == NX_OK && item.name.length == 256);
    CHECK(nx_focus_set(ctx, 131) == NX_OK && build(ctx, &v) && v.scroll_y > 0);
    CHECK(nx_semantics_find(ctx, 131, &index) == NX_OK && nx_semantics_get(ctx, index, &item) == NX_OK &&
        item.bounds.y >= 0 && item.bounds.y + item.bounds.height <= 400);
    if (test_fonts) {
        CHECK(v.layouts[35] && nx_text_layout_height(v.layouts[35]) > 32 &&
            item.bounds.height >= nx_text_layout_height(v.layouts[35]) + 8);
        for (size_t i = 0; i < 32; ++i) {
            CHECK(nx_semantics_find(ctx, 100 + i, &index) == NX_OK && nx_semantics_get(ctx, index, &item) == NX_OK &&
                item.bounds.height >= nx_text_layout_height(v.layouts[4 + i]) + 8 &&
                nx_text_layout_width(v.layouts[4 + i]) <= item.bounds.width - 36);
        }
        v.text_size = 32;
        int intent, redraw;
        CHECK(nx_begin_frame(ctx, 360, 260, 0) == NX_OK &&
            ah_build(&v, ctx, 0, (nx_rect){8, 8, 344, 244}, &intent, &redraw) == NX_OK && nx_end_frame(ctx) == NX_OK);
        CHECK(nx_text_layout_height(v.layouts[35]) > 260);
        float before_scroll = v.scroll_y;
        nx_event page = {0}; page.type = NX_EVENT_KEY_DOWN; page.key = NX_KEY_PAGE_DOWN;
        CHECK(nx_context_queue_event(ctx, &page) == NX_OK);
        CHECK(nx_begin_frame(ctx, 360, 260, 0) == NX_OK &&
            ah_build(&v, ctx, 0, (nx_rect){8, 8, 344, 244}, &intent, &redraw) == NX_OK && nx_end_frame(ctx) == NX_OK &&
            v.scroll_y > before_scroll && nx_focus_id(ctx) == 131);
        float scrolled = v.scroll_y;
        CHECK(nx_begin_frame(ctx, 360, 260, 0) == NX_OK &&
            ah_build(&v, ctx, 0, (nx_rect){8, 8, 344, 244}, &intent, &redraw) == NX_OK && nx_end_frame(ctx) == NX_OK &&
            v.scroll_y == scrolled);
        puts("MANIFEST_WRAPPED_ROW_HEIGHTS_VERIFIED");
    }
    ah_destroy(&v); nx_context_destroy(ctx); CHECK(nx_sim_channel_destroy(channel) == NX_OK);
    puts("MANIFEST_ALL_32_PATHS_VERIFIED"); return 0;
}
int main(int argc, char **argv) {
    CHECK((argc == 2 || argc == 3) && strlen(argv[1]) < sizeof directory); strcpy(directory, argv[1]);
    nx_font *font = NULL; unsigned char *bytes = NULL;
    if (argc == 3) {
        FILE *f = fopen(argv[2], "rb"); CHECK(f && !fseek(f, 0, SEEK_END));
        long size = ftell(f); CHECK(size > 0 && size < 64000000 && !fseek(f, 0, SEEK_SET));
        bytes = malloc((size_t)size); CHECK(bytes && fread(bytes, 1, (size_t)size, f) == (size_t)size && !fclose(f));
        CHECK(nx_font_open(bytes, (size_t)size, NULL, &font) == NX_OK && nx_font_collection_create(NULL, &test_fonts) == NX_OK &&
            nx_font_collection_add(test_fonts, font) == NX_OK);
    }
    int result = projection(); if (!result) result = whole_model();
    nx_font_collection_destroy(test_fonts); nx_font_close(font); free(bytes); return result;
}
