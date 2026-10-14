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
    char bytes[65536];
    uint8_t expected[65537], actual[65537];
    size_t byte_count = 0, scalars = 0;
    memset(expected, 0, sizeof(expected));
    char *comment = strchr(line, '#');
    if (comment) *comment = '\0';
    char *token = strtok(line, " \t\r\n");
    while (token) {
        if (strcmp(token, "\xc3\xb7") == 0) {
            expected[byte_count] = NX_LINE_BREAK_OPTIONAL;
        } else if (strcmp(token, "\xc3\x97") != 0) {
            char *end = NULL;
            unsigned long scalar = strtoul(token, &end, 16);
            if (!end || *end || scalar > 0x10ffffu ||
                byte_count + 4u > sizeof(bytes)) return 1;
            byte_count = append_utf8(bytes, byte_count, (uint32_t)scalar);
            ++scalars;
        }
        token = strtok(NULL, " \t\r\n");
    }
    if (!scalars || expected[byte_count] != NX_LINE_BREAK_OPTIONAL)
        return 1;
    expected[byte_count] = NX_LINE_BREAK_MANDATORY;
    if (nx_line_breaks(bytes, byte_count, actual, sizeof(actual)) != NX_OK) {
        fprintf(stderr, "LineBreakTest line %zu: analyzer error\n", line_number);
        return 1;
    }
    for (size_t i = 0; i <= byte_count; ++i) {
        if ((actual[i] != NX_LINE_BREAK_NONE) !=
            (expected[i] != NX_LINE_BREAK_NONE) ||
            (i == byte_count && actual[i] != NX_LINE_BREAK_MANDATORY)) {
            fprintf(stderr, "LineBreakTest line %zu, byte %zu: got %u, expected %u\n",
                    line_number, i, actual[i], expected[i]);
            return 1;
        }
    }
    return 0;
}

int main(void) {
    FILE *source = fopen(NEXAUI_LINE_TEST_FILE, "rb");
    if (!source) {
        fprintf(stderr, "cannot open %s\n", NEXAUI_LINE_TEST_FILE);
        return 1;
    }
    char line[65536];
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
    if (samples < 15000) return 1;

    uint8_t output[4] = {9, 9, 9, 9};
    if (nx_line_breaks("A", 1, output, 1) != NX_ERROR_ARGUMENT ||
        output[0] != 9) return 1;
    if (nx_line_breaks("\xc0\xaf", 2, output, 4) != NX_ERROR_ARGUMENT ||
        output[0] != 9) return 1;
    if (nx_line_breaks(NULL, 0, output, 4) != NX_OK ||
        output[0] != NX_LINE_BREAK_MANDATORY) return 1;
    if (nx_line_breaks("A\r\nB\nC", 6, output, sizeof(output)) !=
        NX_ERROR_ARGUMENT) return 1;
    uint8_t hard[7];
    if (nx_line_breaks("A\r\nB\nC", 6, hard, sizeof(hard)) != NX_OK ||
        hard[0] != NX_LINE_BREAK_NONE || hard[1] != NX_LINE_BREAK_NONE ||
        hard[2] != NX_LINE_BREAK_NONE ||
        hard[3] != NX_LINE_BREAK_MANDATORY ||
        hard[4] != NX_LINE_BREAK_NONE ||
        hard[5] != NX_LINE_BREAK_MANDATORY ||
        hard[6] != NX_LINE_BREAK_MANDATORY) return 1;

    const size_t marks = 30000;
    char *long_text = malloc(1u + marks * 2u + 1u);
    uint8_t *long_breaks = malloc(1u + marks * 2u + 2u);
    if (!long_text || !long_breaks) {
        free(long_text);
        free(long_breaks);
        return 1;
    }
    long_text[0] = 'A';
    for (size_t i = 0; i < marks; ++i) {
        long_text[1u + i * 2u] = (char)0xccu;
        long_text[2u + i * 2u] = (char)0x88u;
    }
    long_text[1u + marks * 2u] = 'B';
    size_t long_length = 2u + marks * 2u;
    if (nx_line_breaks(long_text, long_length, long_breaks,
                       long_length + 1u) != NX_OK ||
        long_breaks[1u] != NX_LINE_BREAK_NONE ||
        long_breaks[long_length - 1u] != NX_LINE_BREAK_NONE ||
        long_breaks[long_length] != NX_LINE_BREAK_MANDATORY) {
        free(long_text);
        free(long_breaks);
        return 1;
    }
    for (size_t i = 0; i < marks; ++i) {
        long_text[i * 2u] = (char)0xccu;
        long_text[i * 2u + 1u] = (char)0x88u;
    }
    long_text[marks * 2u] = 'B';
    long_length = marks * 2u + 1u;
    if (nx_line_breaks(long_text, long_length, long_breaks,
                       long_length + 1u) != NX_OK ||
        long_breaks[2u] != NX_LINE_BREAK_NONE ||
        long_breaks[long_length - 1u] != NX_LINE_BREAK_NONE ||
        long_breaks[long_length] != NX_LINE_BREAK_MANDATORY) {
        free(long_text);
        free(long_breaks);
        return 1;
    }
    memset(long_text, ' ', long_length - 1u);
    long_text[long_length - 1u] = 'B';
    if (nx_line_breaks(long_text, long_length, long_breaks,
                       long_length + 1u) != NX_OK ||
        long_breaks[1u] != NX_LINE_BREAK_NONE ||
        long_breaks[long_length - 1u] != NX_LINE_BREAK_OPTIONAL ||
        long_breaks[long_length] != NX_LINE_BREAK_MANDATORY) {
        free(long_text);
        free(long_breaks);
        return 1;
    }
    free(long_text);
    free(long_breaks);
    printf("%zu Unicode line break samples passed\n", samples);
    return 0;
}
