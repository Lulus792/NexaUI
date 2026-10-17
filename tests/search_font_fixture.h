#ifndef NEXAUI_SEARCH_FONT_FIXTURE_H
#define NEXAUI_SEARCH_FONT_FIXTURE_H
#include <nexaui/nexaui.h>
#include <string.h>
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
#endif
