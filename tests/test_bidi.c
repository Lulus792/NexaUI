#include "nexaui/nexaui.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static size_t append_utf8(char *bytes, size_t offset, uint32_t scalar) {
    if (scalar < 0x80u) bytes[offset++] = (char)scalar;
    else if (scalar < 0x800u) {
        bytes[offset++] = (char)(0xc0u | (scalar >> 6u));
        bytes[offset++] = (char)(0x80u | (scalar & 63u));
    } else if (scalar < 0x10000u) {
        bytes[offset++] = (char)(0xe0u | (scalar >> 12u));
        bytes[offset++] = (char)(0x80u | ((scalar >> 6u) & 63u));
        bytes[offset++] = (char)(0x80u | (scalar & 63u));
    } else {
        bytes[offset++] = (char)(0xf0u | (scalar >> 18u));
        bytes[offset++] = (char)(0x80u | ((scalar >> 12u) & 63u));
        bytes[offset++] = (char)(0x80u | ((scalar >> 6u) & 63u));
        bytes[offset++] = (char)(0x80u | (scalar & 63u));
    }
    return offset;
}

static int test_character_line(char *line, size_t number) {
    char *fields[5];
    fields[0] = line;
    for (int i = 1; i < 5; ++i) {
        char *mark = strchr(fields[i - 1], ';');
        if (!mark) return 1;
        *mark = '\0';
        fields[i] = mark + 1;
    }
    char bytes[1024];
    uint8_t expected[256], actual[256], paragraph = 255;
    size_t wanted_order[256], actual_order[256];
    size_t length = 0, count = 0, order_count = 0;
    char *token = strtok(fields[0], " \t\r\n");
    while (token) {
        if (count >= 256 || length + 4u > sizeof(bytes)) return 1;
        length = append_utf8(bytes, length, (uint32_t)strtoul(token, NULL, 16));
        ++count;
        token = strtok(NULL, " \t\r\n");
    }
    token = strtok(fields[3], " \t\r\n");
    for (size_t i = 0; i < count; ++i) {
        if (!token) return 1;
        expected[i] = strcmp(token, "x") == 0 ? UINT8_MAX :
                      (uint8_t)strtoul(token, NULL, 10);
        token = strtok(NULL, " \t\r\n");
    }
    if (token) return 1;
    token = strtok(fields[4], " \t\r\n");
    while (token) {
        if (order_count >= count) return 1;
        wanted_order[order_count++] = (size_t)strtoul(token, NULL, 10);
        token = strtok(NULL, " \t\r\n");
    }
    size_t actual_count = 0, actual_visual = 0;
    int mode = (int)strtol(fields[1], NULL, 10);
    nx_bidi_direction direction = mode == 0 ? NX_BIDI_LTR :
                                  mode == 1 ? NX_BIDI_RTL : NX_BIDI_AUTO;
    if (nx_bidi_analyze(bytes, length, direction, actual, 256, actual_order, 256,
                        &actual_count, &actual_visual, &paragraph) != NX_OK ||
        actual_count != count || actual_visual != order_count ||
        paragraph != (uint8_t)strtoul(fields[2], NULL, 10)) {
        fprintf(stderr, "BidiCharacterTest line %zu: counts or paragraph\n", number);
        return 1;
    }
    for (size_t i = 0; i < count; ++i)
        if (actual[i] != expected[i]) {
            fprintf(stderr, "BidiCharacterTest line %zu scalar %zu: level %u expected %u\n",
                    number, i, actual[i], expected[i]);
            for (size_t j = 0; j < count; ++j)
                fprintf(stderr, "%zu:%u/%u ", j, actual[j], expected[j]);
            fputc('\n', stderr);
            return 1;
        }
    for (size_t i = 0; i < order_count; ++i)
        if (actual_order[i] != wanted_order[i]) {
            fprintf(stderr, "BidiCharacterTest line %zu visual %zu: %zu expected %zu\n",
                    number, i, actual_order[i], wanted_order[i]);
            return 1;
        }
    return 0;
}

int main(void) {
    FILE *file = fopen(NEXAUI_BIDI_CHARACTER_TEST_FILE, "rb");
    if (!file) return 1;
    char line[4096];
    size_t number = 0, samples = 0;
    while (fgets(line, sizeof(line), file)) {
        ++number;
        if (line[0] == '#' || line[0] == '\r' || line[0] == '\n') continue;
        if (test_character_line(line, number)) { fclose(file); return 1; }
        ++samples;
    }
    fclose(file);
    if (samples < 90000) return 1;
    uint8_t levels[4] = {9,9,9,9}, paragraph = 9;
    size_t order[4] = {9,9,9,9}, scalars = 9, visual = 9;
    if (nx_bidi_analyze("A", 1, NX_BIDI_AUTO, levels, 0, order, 4,
                        &scalars, &visual, &paragraph) != NX_ERROR_ARGUMENT ||
        levels[0] != 9 || order[0] != 9 || scalars != 9 || visual != 9 ||
        paragraph != 9) return 1;
    if (nx_bidi_analyze("\xc0\xaf", 2, NX_BIDI_AUTO, levels, 4,
                        order, 4, &scalars, &visual, &paragraph) !=
        NX_ERROR_ARGUMENT || levels[0] != 9 || order[0] != 9) return 1;
    if (nx_bidi_analyze(NULL, 0, NX_BIDI_AUTO, NULL, 0, NULL, 0,
                        &scalars, &visual, &paragraph) != NX_OK ||
        scalars != 0 || visual != 0 || paragraph != 0) return 1;
    printf("%zu BidiCharacterTest cases passed\n", samples);
    return 0;
}
