#include <nexaui/nexaui.h>
#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "search_font_fixture.h"
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"%d: %s\n",__LINE__,#x); return 1; } } while (0)

typedef struct accounting {
    size_t calls, fail, live; int bad;
    void *pointers[128]; size_t sizes[128];
} accounting;
static void *allocate(void *user, size_t bytes) {
    accounting *a = user; if (++a->calls == a->fail) return NULL;
    void *p = malloc(bytes); if (!p) return NULL;
    for (size_t i = 0; i < 128; ++i) if (!a->pointers[i]) {
        a->pointers[i] = p; a->sizes[i] = bytes; ++a->live; return p;
    }
    a->bad = 1; free(p); return NULL;
}
static void deallocate(void *user, void *p, size_t bytes) {
    accounting *a = user;
    for (size_t i = 0; i < 128; ++i) if (a->pointers[i] == p) {
        if (a->sizes[i] != bytes || !a->live) a->bad = 1;
        a->pointers[i] = NULL; --a->live; free(p); return;
    }
    a->bad = 1;
}
int main(void) {
    uint8_t bytes[512]; font_fixture(bytes);
    nx_font *font; nx_font_collection *fonts; nx_text_layout *layout, *blank;
    CHECK(nx_font_open(bytes,sizeof(bytes),NULL,&font) == NX_OK);
    CHECK(nx_font_collection_create(NULL,&fonts) == NX_OK);
    CHECK(nx_font_collection_add(fonts,font) == NX_OK);
    CHECK(nx_text_layout_create(fonts,"010\n1",5,NX_BIDI_LTR,18,100,&layout) == NX_OK);
    CHECK(nx_text_layout_create(fonts,"",0,NX_BIDI_LTR,18,100,&blank) == NX_OK);
    size_t successes = 0, failures = 0;
    for (size_t fail = 1; fail < 200; ++fail) {
        accounting a = {0}; a.fail = fail; nx_allocator allocator = {&a,allocate,deallocate};
        nx_text_stamp *stamp = (nx_text_stamp *)(uintptr_t)1;
        nx_result result = nx_text_stamp_create(layout,65536,&allocator,&stamp);
        if (result == NX_ERROR_MEMORY) { CHECK(!stamp); ++failures; }
        else { CHECK(result == NX_OK && stamp); ++successes; nx_text_stamp_destroy(stamp); }
        CHECK(!a.bad && a.live == 0);
        if (successes) break;
    }
    CHECK(successes == 1 && failures >= 8);
    nx_text_stamp *stamp, *empty;
    CHECK(nx_text_stamp_create(layout,65536,NULL,&stamp) == NX_OK);
    CHECK(nx_text_stamp_create(blank,65536,NULL,&empty) == NX_OK);
    nx_bitmap_view view; nx_point offset;
    CHECK(nx_text_stamp_view(stamp,&view,&offset) == NX_OK && view.pixels && view.width && view.format == NX_BITMAP_A8);
    CHECK(nx_text_stamp_view(empty,&view,&offset) == NX_OK && !view.pixels && !view.width);
    nx_context *ui; CHECK(nx_context_create(NULL,&ui) == NX_OK);
    CHECK(nx_begin_frame(ui,100,80,0) == NX_OK);
    CHECK(nx_draw_text_layout(ui,layout,(nx_point){10,5},(nx_color){1,1,1,1},65536) == NX_OK);
    CHECK(nx_end_frame(ui) == NX_OK);
    const nx_command *commands; size_t count;
    uint8_t direct[100*80*4], cached[100*80*4];
    CHECK(nx_frame_commands(ui,&commands,&count) == NX_OK);
    CHECK(nx_headless_render(commands,count,direct,100,80,400,(nx_color){0}) == NX_OK);
    nx_text_layout_destroy(layout); nx_text_layout_destroy(blank);
    nx_font_collection_destroy(fonts); nx_font_close(font); memset(bytes,0,sizeof(bytes));
    CHECK(nx_begin_frame(ui,100,80,0) == NX_OK);
    CHECK(nx_draw_text_stamp(ui,empty,(nx_point){0},(nx_color){1,1,1,1}) == NX_OK);
    CHECK(nx_label_stamp(ui,1,0,(nx_rect){0,0,100,80},stamp,(nx_point){10,5},(nx_color){1,1,1,1}) == NX_OK);
    CHECK(nx_end_frame(ui) == NX_OK);
    CHECK(nx_draw_text_stamp(ui,empty,(nx_point){0},(nx_color){1,1,1,1}) == NX_ERROR_STATE);
    CHECK(nx_frame_commands(ui,&commands,&count) == NX_OK && count == 1);
    CHECK(nx_headless_render(commands,count,cached,100,80,400,(nx_color){0}) == NX_OK);
    CHECK(memcmp(direct,cached,sizeof(direct)) == 0);
    uint32_t index; nx_semantic_view semantic;
    CHECK(nx_semantics_find(ui,1,&index) == NX_OK && nx_semantics_get(ui,index,&semantic) == NX_OK);
    CHECK(semantic.role == NX_ROLE_TEXT && semantic.name.length == 5 && memcmp(semantic.name.data,"010\n1",5) == 0);
    nx_text_stamp_destroy(stamp); nx_text_stamp_destroy(empty); nx_context_destroy(ui);
    return 0;
}
