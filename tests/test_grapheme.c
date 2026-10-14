#include "nexaui/nexaui.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static size_t append_utf8(char *buffer, size_t offset, uint32_t scalar) {
    if (scalar <= 0x7fu) {
        buffer[offset++] = (char)scalar;
    } else if (scalar <= 0x7ffu) {
        buffer[offset++] = (char)(0xc0u | (scalar >> 6u));
        buffer[offset++] = (char)(0x80u | (scalar & 0x3fu));
    } else if (scalar <= 0xffffu) {
        buffer[offset++] = (char)(0xe0u | (scalar >> 12u));
        buffer[offset++] = (char)(0x80u | ((scalar >> 6u) & 0x3fu));
        buffer[offset++] = (char)(0x80u | (scalar & 0x3fu));
    } else {
        buffer[offset++] = (char)(0xf0u | (scalar >> 18u));
        buffer[offset++] = (char)(0x80u | ((scalar >> 12u) & 0x3fu));
        buffer[offset++] = (char)(0x80u | ((scalar >> 6u) & 0x3fu));
        buffer[offset++] = (char)(0x80u | (scalar & 0x3fu));
    }
    return offset;
}

static int test_line(char *line, size_t line_number) {
    char bytes[8192];
    size_t boundaries[2048];
    size_t byte_count = 0, boundary_count = 0;
    char *comment = strchr(line, '#');
    if (comment) *comment = '\0';
    char *token = strtok(line, " \t\r\n");
    if (!token) return 0;
    while (token) {
        if (strcmp(token, "\xc3\xb7") == 0) {
            if (boundary_count >= sizeof(boundaries) / sizeof(boundaries[0])) return 1;
            boundaries[boundary_count++] = byte_count;
        } else if (strcmp(token, "\xc3\x97") != 0) {
            char *end = NULL;
            unsigned long scalar = strtoul(token, &end, 16);
            if (!end || *end || scalar > 0x10ffffu ||
                byte_count + 4u > sizeof(bytes)) return 1;
            byte_count = append_utf8(bytes, byte_count, (uint32_t)scalar);
        }
        token = strtok(NULL, " \t\r\n");
    }
    if (boundary_count < 2 || boundaries[0] != 0 ||
        boundaries[boundary_count - 1] != byte_count) return 1;
    for (size_t i = 0; i + 1 < boundary_count; ++i) {
        size_t actual = (size_t)-1;
        nx_result result = nx_grapheme_next(bytes, byte_count,
                                             boundaries[i], &actual);
        if (result != NX_OK || actual != boundaries[i + 1]) {
            fprintf(stderr, "GraphemeBreakTest line %zu, cluster %zu: got %zu, expected %zu\n",
                    line_number, i, actual, boundaries[i + 1]);
            return 1;
        }
    }
    return 0;
}

int main(void) {
    FILE *source = fopen(NEXAUI_GRAPHEME_TEST_FILE, "rb");
    if (!source) {
        fprintf(stderr, "cannot open %s\n", NEXAUI_GRAPHEME_TEST_FILE);
        return 1;
    }
    char line[16384];
    size_t line_number = 0, samples = 0;
    while (fgets(line, sizeof(line), source)) {
        ++line_number;
        if (!strchr(line, '\n') && !feof(source)) {
            fprintf(stderr, "line %zu exceeds test buffer\n", line_number);
            fclose(source);
            return 1;
        }
        if (line[0] != '#' && line[0] != '\r' && line[0] != '\n') {
            if (test_line(line, line_number)) {
                fclose(source);
                return 1;
            }
            ++samples;
        }
    }
    fclose(source);
    if (samples < 500) return 1;

    size_t next = 99;
    if (nx_grapheme_next("", 0, 0, &next) != NX_ERROR_ARGUMENT || next != 99)
        return 1;
    if (nx_grapheme_next("\xc0\xaf", 2, 0, &next) != NX_ERROR_ARGUMENT ||
        next != 99) return 1;
    if (nx_grapheme_next("A\xe2\x82", 3, 0, &next) != NX_ERROR_ARGUMENT ||
        next != 99) return 1;
    if (nx_grapheme_next("A", 1, 1, &next) != NX_ERROR_ARGUMENT || next != 99)
        return 1;
    printf("%zu Unicode grapheme samples passed\n", samples);
    return 0;
}
