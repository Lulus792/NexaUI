#include "nexaui/nexaui.h"
#include "font_internal.h"

#include <stdint.h>
#include <stdlib.h>
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

/* The fuzzer mutates the GSUB bytes inside an otherwise valid SFNT face. */
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (size > (1u << 20u)) return 0;
    uint8_t *bytes = malloc(248u + size);
    if (!bytes) return 0;
    memset(bytes, 0, 248u);
    put32(bytes, UINT32_C(0x00010000));
    put16(bytes + 4u, 5);
    const uint32_t tags[5] = {
        NX_SFNT_TAG('G','S','U','B'), NX_SFNT_TAG('h','e','a','d'),
        NX_SFNT_TAG('h','h','e','a'), NX_SFNT_TAG('h','m','t','x'),
        NX_SFNT_TAG('m','a','x','p')
    };
    const uint32_t offsets[5] = {248,92,180,216,148};
    const uint32_t lengths[5] = {(uint32_t)size,54,36,32,32};
    for (size_t i = 0; i < 5; ++i) {
        put32(bytes + 12u + 16u * i, tags[i]);
        put32(bytes + 20u + 16u * i, offsets[i]);
        put32(bytes + 24u + 16u * i, lengths[i]);
    }
    put32(bytes + 92u, UINT32_C(0x00010000));
    put32(bytes + 104u, UINT32_C(0x5f0f3cf5));
    put16(bytes + 110u, 1000);
    put32(bytes + 148u, UINT32_C(0x00010000));
    put16(bytes + 152u, 8);
    put32(bytes + 180u, UINT32_C(0x00010000));
    put16(bytes + 184u, 800);
    put16(bytes + 186u, (uint16_t)-200);
    put16(bytes + 214u, 8);
    for (size_t i = 0; i < 8; ++i) put16(bytes + 216u + 4u * i, 600);
    if (size) memcpy(bytes + 248u, data, size);
    nx_font *font = NULL;
    if (nx_font_open(bytes, 248u + size, NULL, &font) == NX_OK) {
        nx_shaped_glyph glyphs[32] = {0};
        size_t count = size < 32u ? size : 32u;
        for (size_t i = 0; i < count; ++i)
            glyphs[i].glyph_index = data[i] & 7u;
        /* Exercise both seeded substitutions even before mutations reach them. */
        if (count >= 3u) {
            glyphs[0].glyph_index = 5;
            glyphs[1].glyph_index = 3;
            glyphs[2].glyph_index = 4;
        }
        for (unsigned variant=0;variant<4;++variant) {
            nx_shaped_glyph work[32]; memcpy(work,glyphs,sizeof(work));
            if (variant==2 && count>=3) {
                work[0].glyph_index=1; work[1].glyph_index=2; work[2].glyph_index=3;
            }
            if (variant==3 && count>=6) {
                const uint16_t chain[]={6,1,2,3,4,5};
                for (size_t i=0;i<6;++i) work[i].glyph_index=chain[i];
            }
            size_t work_count=count;
            nx_result result=nx_gsub_apply(font,variant==1?NX_SFNT_TAG('A','r','a','b'):NX_SFNT_TAG('L','a','t','n'),work,&work_count);
            if (result==NX_OK) {
                if (work_count>count) abort();
                for (size_t i=0;i<work_count;++i) if (work[i].glyph_index>=8) abort();
            }
        }
        nx_font_close(font);
    }
    free(bytes);
    return 0;
}
