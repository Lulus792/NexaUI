#include "nexaui/nexaui.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(expr) do { if (!(expr)) { \
    fprintf(stderr, "%s:%d: %s failed\n", __FILE__, __LINE__, #expr); \
    return 1; \
} } while (0)

static void put16(uint8_t *p, uint16_t value) {
    p[0] = (uint8_t)(value >> 8u);
    p[1] = (uint8_t)value;
}

static void put32(uint8_t *p, uint32_t value) {
    p[0] = (uint8_t)(value >> 24u);
    p[1] = (uint8_t)(value >> 16u);
    p[2] = (uint8_t)(value >> 8u);
    p[3] = (uint8_t)value;
}

static void font_fixture(uint8_t bytes[512]) {
    uint8_t source[376] = {0};
    put32(source, UINT32_C(0x00010000));
    put16(source + 4u, 6);
    const uint32_t tags[6] = {
        NX_SFNT_TAG('c','m','a','p'), NX_SFNT_TAG('h','e','a','d'),
        NX_SFNT_TAG('h','h','e','a'), NX_SFNT_TAG('h','m','t','x'),
        NX_SFNT_TAG('k','e','r','n'), NX_SFNT_TAG('m','a','x','p')
    };
    const uint32_t offsets[6] = {264,108,196,232,352,164};
    const uint32_t lengths[6] = {88,54,36,32,24,32};
    for (size_t i = 0; i < 6; ++i) {
        put32(source + 12u + 16u * i, tags[i]);
        put32(source + 20u + 16u * i, offsets[i]);
        put32(source + 24u + 16u * i, lengths[i]);
    }
    put32(source + 108u, UINT32_C(0x00010000));
    put32(source + 120u, UINT32_C(0x5f0f3cf5));
    put16(source + 126u, 1000);
    put32(source + 164u, UINT32_C(0x00010000));
    put16(source + 168u, 8);
    put32(source + 196u, UINT32_C(0x00010000));
    put16(source + 200u, 800);
    put16(source + 202u, (uint16_t)-200);
    put16(source + 230u, 8);
    for (size_t i = 0; i < 8; ++i) put16(source + 232u + 4u * i, 600);
    put16(source + 236u, 300);
    uint8_t *cmap = source + 264u;
    put16(cmap + 2u, 1);
    put16(cmap + 4u, 3);
    put16(cmap + 6u, 10);
    put32(cmap + 8u, 12);
    uint8_t *sub = cmap + 12u;
    put16(sub, 12);
    put32(sub + 4u, 64);
    put32(sub + 12u, 4);
    const uint32_t mappings[4] = {'-', '0', '1', '5'};
    for (size_t i = 0; i < 4; ++i) {
        put32(sub + 16u + 12u * i, mappings[i]);
        put32(sub + 20u + 12u * i, mappings[i]);
        put32(sub + 24u + 12u * i, 2);
    }
    put16(source + 354u, 1);
    put16(source + 358u, 20);
    put16(source + 360u, 1);
    put16(source + 362u, 1);
    memset(bytes, 0, 512);
    memcpy(bytes, source, 12u);
    put16(bytes + 4u, 8);
    memcpy(bytes + 140u, source + 108u, 268u);
    for (size_t i = 0; i < 6; ++i) {
        const uint8_t *old_entry = source + 12u + 16u * i;
        uint8_t *entry = bytes + 12u + 16u * i;
        memcpy(entry, old_entry, 16u);
        uint32_t offset = ((uint32_t)old_entry[8] << 24u) |
                          ((uint32_t)old_entry[9] << 16u) |
                          ((uint32_t)old_entry[10] << 8u) | old_entry[11];
        put32(entry + 8u, offset + 32u);
    }
    put32(bytes + 12u + 16u * 6u, NX_SFNT_TAG('l','o','c','a'));
    put32(bytes + 20u + 16u * 6u, 408u);
    put32(bytes + 24u + 16u * 6u, 18u);
    put32(bytes + 12u + 16u * 7u, NX_SFNT_TAG('g','l','y','f'));
    put32(bytes + 20u + 16u * 7u, 428u);
    put32(bytes + 24u + 16u * 7u, 20u);
    for (size_t i = 3u; i < 9u; ++i)
        put16(bytes + 408u + i * 2u, 10u);
    uint8_t *glyph = bytes + 428u;
    put16(glyph, 1u);
    put16(glyph + 6u, 100u);
    put16(glyph + 8u, 100u);
    put16(glyph + 10u, 2u);
    glyph[14] = 0x31;
    glyph[15] = 0x33;
    glyph[16] = 0x27;
    glyph[17] = glyph[18] = glyph[19] = 100u;
}

static int queue_key(nx_context *ctx, nx_key key, uint32_t modifiers) {
    nx_event event = {0};
    event.type = NX_EVENT_KEY_DOWN;
    event.key = key;
    event.modifiers = modifiers;
    return nx_context_queue_event(ctx, &event) == NX_OK;
}

static int queue_text(nx_context *ctx, uint32_t scalar) {
    nx_event event = {0};
    event.type = NX_EVENT_TEXT;
    event.scalar = scalar;
    return nx_context_queue_event(ctx, &event) == NX_OK;
}

static int semantic_draft(nx_context *ctx, const char *draft, int invalid) {
    uint32_t index = UINT32_MAX;
    nx_semantic_view view;
    size_t length = strlen(draft);
    return nx_semantics_find(ctx, 1, &index) == NX_OK &&
           nx_semantics_get(ctx, index, &view) == NX_OK &&
           view.role == NX_ROLE_TEXT_FIELD &&
           view.value.length == length &&
           (!length || memcmp(view.value.data, draft, length) == 0) &&
           !!(view.state & NX_SEM_INVALID) == !!invalid;
}

static int expect_field(nx_numeric_field *field, const char *draft,
                        double committed) {
    nx_utf8_span text;
    double value = 0;
    return nx_numeric_field_text(field, &text) == NX_OK &&
           text.length == strlen(draft) &&
           memcmp(text.data, draft, text.length) == 0 &&
           nx_numeric_field_value(field, &value) == NX_OK &&
           value == committed;
}

static int test_numeric_field(void) {
    uint8_t bytes[512];
    font_fixture(bytes);
    nx_font *font = NULL;
    nx_font_collection *collection = NULL;
    nx_numeric_field *field = NULL;
    nx_context *ctx = NULL;
    CHECK(nx_font_open(bytes, sizeof(bytes), NULL, &font) == NX_OK);
    CHECK(nx_font_collection_create(NULL, &collection) == NX_OK);
    CHECK(nx_font_collection_add(collection, font) == NX_OK);
    CHECK(nx_numeric_field_create(0, 4096, &field) == NX_OK);
    CHECK(nx_context_create(NULL, &ctx) == NX_OK);
    nx_numeric_field_desc desc = {0};
    desc.id = 1;
    desc.bounds = (nx_rect){0, 0, 80, 24};
    desc.name = (nx_utf8_span){"Number", 6};
    desc.fonts = collection;
    desc.pixels_per_em = 20;
    desc.glyph_cache_bytes = 65536;
    desc.minimum = -10;
    desc.maximum = 10;
    for (unsigned frame = 0; frame < 9; ++frame) {
        if (frame == 1) {
            CHECK(nx_focus_set(ctx, 1) == NX_OK);
            CHECK(queue_key(ctx, NX_KEY_A, NX_MOD_CONTROL));
            CHECK(queue_text(ctx, '-'));
        }
        if (frame == 2) {
            CHECK(queue_text(ctx, '1'));
            CHECK(queue_key(ctx, NX_KEY_ENTER, 0));
        }
        if (frame == 3) {
            nx_event down = {0}, up = {0};
            down.type = NX_EVENT_POINTER_DOWN;
            down.button = NX_BUTTON_LEFT;
            down.x = 5;
            down.y = 12;
            up = down;
            up.type = NX_EVENT_POINTER_UP;
            CHECK(nx_context_queue_event(ctx, &down) == NX_OK);
            CHECK(nx_context_queue_event(ctx, &up) == NX_OK);
        }
        if (frame == 4) CHECK(queue_text(ctx, '0'));
        if (frame == 5) {
            CHECK(nx_numeric_field_set_value(field, 5) == NX_OK);
            desc.disabled = 1;
            CHECK(queue_text(ctx, '1'));
        }
        if (frame == 6) desc.disabled = 0;
        if (frame == 7) {
            CHECK(nx_focus_set(ctx, 1) == NX_OK);
            CHECK(queue_key(ctx, NX_KEY_A, NX_MOD_CONTROL));
        }
        if (frame == 8) {
            CHECK(queue_text(ctx, '1'));
            CHECK(queue_key(ctx, NX_KEY_ENTER, 0));
        }
        CHECK(nx_begin_frame(ctx, 100, 30, 0) == NX_OK);
        nx_interaction_state state;
        int valid = -1;
        CHECK(nx_numeric_field_widget(ctx, &desc, field, &state,
                                      &valid) == NX_OK);
        if (frame == 1 || frame == 4) CHECK(!valid);
        else CHECK(valid);
        if (frame == 3) CHECK(state.pointer_pressed && state.focused);
        CHECK(nx_end_frame(ctx) == NX_OK);
        const char *drafts[] = {"0", "-", "-1", "-1", "0-1",
                                "5", "5", "5", "1"};
        double values[] = {0, 0, -1, -1, -1, 5, 5, 5, 1};
        CHECK(expect_field(field, drafts[frame], values[frame]));
        CHECK(semantic_draft(ctx, drafts[frame], !valid));
        const nx_command *commands = NULL;
        size_t count = 0;
        CHECK(nx_frame_commands(ctx, &commands, &count) == NX_OK && count >= 4);
        int has_bitmap = 0;
        int has_selection = 0;
        for (size_t i = 0; i < count; ++i)
            if (commands[i].type == NX_COMMAND_BITMAP) has_bitmap = 1;
            else if (commands[i].type == NX_COMMAND_FILL_RECT)
                has_selection = 1;
        if (frame != 1) CHECK(has_bitmap);
        if (frame == 7) CHECK(has_selection);
        uint8_t pixels[100u * 30u * 4u];
        CHECK(nx_headless_render(commands, count, pixels, 100, 30,
                                 100u * 4u, (nx_color){0, 0, 0, 0}) == NX_OK);
        CHECK(pixels[((size_t)12 * 100u + 40u) * 4u + 3u] != 0);
    }
    nx_context_destroy(ctx);
    nx_numeric_field_destroy(field);
    nx_font_collection_destroy(collection);
    nx_font_close(font);
    return 0;
}

typedef struct allocation_probe {
    unsigned calls, frees, fail_on;
} allocation_probe;

static void *probe_allocate(void *user, size_t size) {
    allocation_probe *probe = (allocation_probe *)user;
    ++probe->calls;
    return probe->calls == probe->fail_on ? NULL : malloc(size);
}

static void probe_deallocate(void *user, void *pointer, size_t size) {
    allocation_probe *probe = (allocation_probe *)user;
    (void)size;
    ++probe->frees;
    free(pointer);
}

static int test_retry_after_paint_failure(void) {
    uint8_t bytes[512];
    font_fixture(bytes);
    nx_font *font = NULL;
    nx_font_collection *collection = NULL;
    nx_numeric_field *field = NULL;
    nx_context *ctx = NULL;
    allocation_probe probe = {0};
    nx_context_desc context_desc = {0};
    context_desc.allocator = (nx_allocator){&probe, probe_allocate,
                                             probe_deallocate};
    CHECK(nx_font_open(bytes, sizeof(bytes), NULL, &font) == NX_OK);
    CHECK(nx_font_collection_create(NULL, &collection) == NX_OK);
    CHECK(nx_font_collection_add(collection, font) == NX_OK);
    CHECK(nx_numeric_field_create(0, 4096, &field) == NX_OK);
    CHECK(nx_context_create(&context_desc, &ctx) == NX_OK);
    nx_numeric_field_desc desc = {0};
    desc.id = 1;
    desc.bounds = (nx_rect){0, 0, 80, 24};
    desc.name = (nx_utf8_span){"Number", 6};
    desc.fonts = collection;
    desc.pixels_per_em = 20;
    desc.glyph_cache_bytes = 65536;
    desc.minimum = 0;
    desc.maximum = 10;
    nx_interaction_state state;
    int valid = 0;
    CHECK(nx_begin_frame(ctx, 100, 30, 0) == NX_OK);
    CHECK(nx_numeric_field_widget(ctx, &desc, field, &state,
                                  &valid) == NX_OK);
    CHECK(nx_end_frame(ctx) == NX_OK);
    CHECK(nx_begin_frame(ctx, 100, 30, 0) == NX_OK);
    CHECK(nx_numeric_field_widget(ctx, &desc, field, &state,
                                  &valid) == NX_OK);
    CHECK(nx_end_frame(ctx) == NX_OK);
    CHECK(nx_focus_set(ctx, 1) == NX_OK);
    CHECK(queue_key(ctx, NX_KEY_A, NX_MOD_CONTROL));
    CHECK(queue_text(ctx, '1'));
    CHECK(nx_begin_frame(ctx, 100, 30, 0) == NX_OK);
    probe.fail_on = probe.calls + 1u;
    CHECK(nx_numeric_field_widget(ctx, &desc, field, &state,
                                  &valid) == NX_ERROR_MEMORY);
    CHECK(expect_field(field, "1", 0));
    probe.fail_on = 0;
    CHECK(nx_numeric_field_widget(ctx, &desc, field, &state,
                                  &valid) == NX_OK && valid);
    CHECK(nx_end_frame(ctx) == NX_OK);
    CHECK(expect_field(field, "1", 0));
    CHECK(semantic_draft(ctx, "1", 0));
    nx_context_destroy(ctx);
    nx_numeric_field_destroy(field);
    nx_font_collection_destroy(collection);
    nx_font_close(font);
    CHECK(probe.frees == probe.calls - 1u);
    return 0;
}

int main(void) {
    if (test_numeric_field()) return 1;
    if (test_retry_after_paint_failure()) return 1;
    puts("numeric field tests passed");
    return 0;
}
