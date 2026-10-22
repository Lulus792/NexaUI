/* Exercise the real text-buffer TU with deterministic allocation failures.
 * The property classifier remains production code; edits and replay must leave
 * all text, boundary indexes, selections and history unchanged on OOM. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Checks and their edit/selection/replay calls must run with NDEBUG too. */
#define CHECK(condition) do { if (!(condition)) { \
    fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #condition); \
    exit(EXIT_FAILURE); \
} } while (0)
typedef struct allocation { void *pointer; size_t size; int scrub; } allocation;
static allocation allocations[64];
static size_t attempts, fail_at, live;
static int sensitive_allocations;
static void track(void *pointer, size_t size, int scrub) {
    if (!pointer) return;
    for (size_t i = 0; i < 64; ++i) if (!allocations[i].pointer) {
        allocations[i] = (allocation){pointer, size, scrub};
        ++live;
        return;
    }
    CHECK(0 && "allocation tracking exhausted");
}
static void *test_malloc(size_t size) {
    if (++attempts == fail_at) return NULL;
    void *pointer = malloc(size);
    track(pointer, size, sensitive_allocations);
    return pointer;
}
static void *test_calloc(size_t count, size_t size) {
    if (++attempts == fail_at) return NULL;
    void *pointer = calloc(count, size);
    track(pointer, count * size, 0);
    return pointer;
}
static void test_free(void *pointer) {
    if (!pointer) return;
    for (size_t i = 0; i < 64; ++i) if (allocations[i].pointer == pointer) {
        if (allocations[i].scrub) {
            const unsigned char *bytes = pointer;
            for (size_t j = 0; j < allocations[i].size; ++j) CHECK(bytes[j] == 0);
        }
        allocations[i] = (allocation){0};
        --live;
        free(pointer);
        return;
    }
    CHECK(0 && "untracked or duplicate free");
}
#define malloc test_malloc
#define calloc test_calloc
#define free test_free
#include "../src/text_buffer.c"
#undef malloc
#undef calloc
#undef free

typedef struct saved_state {
    char text[128];
    uint8_t grapheme[128], word[128];
    size_t length, anchor, cursor, history_used;
    nx_text_record *undo, *redo;
} saved_state;
static saved_state save(const nx_text_buffer *b) {
    saved_state s = {0};
    CHECK(b->length < sizeof(s.text));
    memcpy(s.text, b->bytes, b->length + 1);
    memcpy(s.grapheme, b->grapheme, b->length + 1);
    memcpy(s.word, b->word, b->length + 1);
    s.length = b->length; s.anchor = b->anchor; s.cursor = b->cursor;
    s.history_used = b->history_used; s.undo = b->undo; s.redo = b->redo;
    return s;
}
static void unchanged(const nx_text_buffer *b, const saved_state *s) {
    CHECK(b->length == s->length && b->anchor == s->anchor && b->cursor == s->cursor);
    CHECK(b->history_used == s->history_used && b->undo == s->undo && b->redo == s->redo);
    CHECK(memcmp(b->bytes, s->text, s->length + 1) == 0);
    CHECK(memcmp(b->grapheme, s->grapheme, s->length + 1) == 0);
    CHECK(memcmp(b->word, s->word, s->length + 1) == 0);
}
static const char initial[] = "Ae\xcc\x81\r\n\xf0\x9f\x91\xa9\xe2\x80\x8d\xf0\x9f\x92\xbb Z";
static void failures(int operation, int sensitive) {
    for (size_t failure = 1; failure <= 8; ++failure) {
        CHECK(live == 0);
        attempts = fail_at = 0; sensitive_allocations = sensitive;
        nx_text_buffer *b = NULL;
        nx_result r = sensitive ? nx_text_buffer_create_sensitive(initial, sizeof(initial)-1, &b) :
            nx_text_buffer_create(initial, sizeof(initial)-1, 4097, &b);
        CHECK(r == NX_OK);
        if (!sensitive) {
            CHECK(nx_text_buffer_set_selection(b, 0, b->length) == NX_OK);
            CHECK(nx_text_buffer_replace(b, "seed", 4) == NX_OK);
            if (operation != 1) CHECK(nx_text_buffer_undo(b) == NX_OK);
        }
        if (operation == 0) CHECK(nx_text_buffer_set_selection(b, 0, b->length) == NX_OK);
        if (operation == 3) CHECK(nx_text_buffer_set_selection(b, b->length, 1) == NX_OK);
        saved_state before = save(b);
        size_t before_live = live;
        attempts = 0; fail_at = failure;
        r = operation == 0 ? nx_text_buffer_replace(b, "ne\xcc\x81w", 5) :
            operation == 1 ? nx_text_buffer_undo(b) :
            operation == 2 ? nx_text_buffer_redo(b) :
            nx_text_buffer_replace_all(b, "ne\xcc\x81w", 5);
        fail_at = 0;
        if (r == NX_ERROR_MEMORY) {
            unchanged(b, &before);
            CHECK(live == before_live);
            /* The preserved record must still be usable, not just linked. */
            if (!sensitive && operation == 1) {
                CHECK(nx_text_buffer_undo(b) == NX_OK);
                CHECK(nx_text_buffer_redo(b) == NX_OK);
                unchanged(b, &before);
            } else if (!sensitive && operation != 1 && operation != 3) {
                CHECK(nx_text_buffer_redo(b) == NX_OK);
                CHECK(nx_text_buffer_undo(b) == NX_OK);
                unchanged(b, &before);
            }
        } else CHECK(r == NX_OK && attempts < failure);
        if (sensitive) {
            for (size_t i = 0; i < 64; ++i)
                if (allocations[i].pointer == b) allocations[i].scrub = 1;
        }
        nx_text_buffer_destroy(b);
        CHECK(live == 0);
        if (r == NX_OK) return;
    }
    CHECK(0 && "expected successful allocation control");
}
int main(void) {
    failures(0, 0); failures(1, 0); failures(2, 0); failures(0, 1);
    failures(3, 0); failures(3, 1);
    puts("text-buffer edit/undo/redo OOM atomicity, boundary/history preservation and sensitive scrubbing passed");
    return 0;
}
