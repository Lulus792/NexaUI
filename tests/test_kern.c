#include "nexaui/nexaui.h"

#include <stdio.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "%s:%d: %s\n", \
    __FILE__, __LINE__, #x); return 1; } } while (0)

static void put16(uint8_t *p, uint16_t x) {
    p[0] = (uint8_t)(x >> 8u); p[1] = (uint8_t)x;
}

static void put32(uint8_t *p, uint32_t x) {
    p[0] = (uint8_t)(x >> 24u); p[1] = (uint8_t)(x >> 16u);
    p[2] = (uint8_t)(x >> 8u); p[3] = (uint8_t)x;
}

static void fixture(uint8_t bytes[72]) {
    memset(bytes, 0, 72);
    put32(bytes, UINT32_C(0x00010000));
    put16(bytes + 4u, 1);
    put32(bytes + 12u, NX_SFNT_TAG('k','e','r','n'));
    put32(bytes + 20u, 28);
    put32(bytes + 24u, 44);
    put16(bytes + 30u, 2);
    for (size_t i = 0; i < 2; ++i) {
        uint8_t *sub = bytes + 32u + i * 20u;
        put16(sub + 2u, 20);
        put16(sub + 4u, 1);
        put16(sub + 6u, 1);
        put16(sub + 8u, 6);
        put16(sub + 14u, 3);
        put16(sub + 16u, 7);
        put16(sub + 18u, (uint16_t)(i ? -15 : -40));
    }
}

int main(void) {
    uint8_t bytes[72];
    fixture(bytes);
    nx_font *font = NULL;
    int32_t adjustment = 1234;
    CHECK(nx_font_open(bytes, sizeof(bytes), NULL, &font) == NX_OK);
    CHECK(nx_font_kerning(font, 3, 7, &adjustment) == NX_OK &&
          adjustment == -55);
    CHECK(nx_font_kerning(font, 3, 8, &adjustment) == NX_OK &&
          adjustment == 0);
    nx_font_close(font);
    put16(bytes + 56u, 9);
    CHECK(nx_font_open(bytes, sizeof(bytes), NULL, &font) == NX_OK);
    CHECK(nx_font_kerning(font, 3, 7, &adjustment) == NX_OK &&
          adjustment == -15);
    nx_font_close(font);
    put16(bytes + 56u, 1);
    put16(bytes + 38u, 2);
    adjustment = 1234;
    CHECK(nx_font_open(bytes, sizeof(bytes), NULL, &font) == NX_OK);
    CHECK(nx_font_kerning(font, 3, 7, &adjustment) == NX_ERROR_ARGUMENT &&
          adjustment == 1234);
    nx_font_close(font);
    put16(bytes + 38u, 1);
    put16(bytes + 34u, 5);
    CHECK(nx_font_open(bytes, sizeof(bytes), NULL, &font) == NX_OK);
    CHECK(nx_font_kerning(font, 3, 7, &adjustment) == NX_ERROR_ARGUMENT &&
          adjustment == 1234);
    nx_font_close(font);
    CHECK(nx_font_kerning(NULL, 3, 7, &adjustment) == NX_ERROR_ARGUMENT);
    puts("kern tests passed");
    return 0;
}
