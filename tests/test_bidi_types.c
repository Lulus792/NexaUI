#include "nexaui/nexaui.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct sample { const char *name; uint32_t scalar; } sample;
static const sample samples[] = {
    {"L",0x0041},{"AL",0x0627},{"AN",0x0660},{"B",0x000a},
    {"BN",0x00ad},{"CS",0x002c},{"EN",0x0030},{"ES",0x002b},
    {"ET",0x0024},{"FSI",0x2068},{"LRE",0x202a},{"LRI",0x2066},
    {"LRO",0x202d},{"NSM",0x0300},{"ON",0x0021},{"PDF",0x202c},
    {"PDI",0x2069},{"R",0x05d0},{"RLE",0x202b},{"RLI",0x2067},
    {"RLO",0x202e},{"S",0x0009},{"WS",0x0020}
};

static size_t append(char *bytes, size_t offset, uint32_t scalar) {
    if (scalar < 0x80u) bytes[offset++] = (char)scalar;
    else if (scalar < 0x800u) {
        bytes[offset++] = (char)(0xc0u | (scalar >> 6u));
        bytes[offset++] = (char)(0x80u | (scalar & 63u));
    } else {
        bytes[offset++] = (char)(0xe0u | (scalar >> 12u));
        bytes[offset++] = (char)(0x80u | ((scalar >> 6u) & 63u));
        bytes[offset++] = (char)(0x80u | (scalar & 63u));
    }
    return offset;
}

static int numbers(char *line, uint8_t *out, size_t *count, int levels) {
    *count = 0;
    char *token = strtok(line, " \t\r\n");
    while (token) {
        if (*count >= 256) return 1;
        out[(*count)++] = levels && strcmp(token, "x") == 0 ? UINT8_MAX :
                          (uint8_t)strtoul(token, NULL, 10);
        token = strtok(NULL, " \t\r\n");
    }
    return 0;
}

int main(void) {
    FILE *file = fopen(NEXAUI_BIDI_TYPE_TEST_FILE, "rb");
    if (!file) return 1;
    char line[512];
    uint8_t expected_levels[256], expected_order[256];
    size_t level_count = 0, order_count = 0, cases = 0, line_number = 0;
    while (fgets(line, sizeof(line), file)) {
        ++line_number;
        if (strncmp(line, "@Levels:", 8) == 0) {
            if (numbers(line + 8, expected_levels, &level_count, 1)) return 1;
            continue;
        }
        if (strncmp(line, "@Reorder:", 9) == 0) {
            if (numbers(line + 9, expected_order, &order_count, 0)) return 1;
            continue;
        }
        if (line[0] == '#' || line[0] == '@' || line[0] == '\r' ||
            line[0] == '\n') continue;
        char *separator = strchr(line, ';');
        if (!separator) return 1;
        *separator = '\0';
        unsigned modes = (unsigned)strtoul(separator + 1, NULL, 16);
        char bytes[1024];
        size_t length = 0, count = 0;
        char *token = strtok(line, " \t\r\n");
        while (token) {
            size_t k = 0;
            for (; k < sizeof(samples) / sizeof(samples[0]); ++k)
                if (strcmp(token, samples[k].name) == 0) break;
            if (k == sizeof(samples) / sizeof(samples[0]) || count >= 256)
                return 1;
            length = append(bytes, length, samples[k].scalar);
            ++count;
            token = strtok(NULL, " \t\r\n");
        }
        if (count != level_count) return 1;
        for (unsigned bit = 1; bit <= 4; bit <<= 1u) {
            if (!(modes & bit)) continue;
            uint8_t actual[256], paragraph = 255;
            size_t order[256], actual_count = 0, visual_count = 0;
            nx_bidi_direction direction = bit == 1 ? NX_BIDI_AUTO :
                                          bit == 2 ? NX_BIDI_LTR : NX_BIDI_RTL;
            if (nx_bidi_analyze(bytes, length, direction, actual, 256,
                                order, 256, &actual_count, &visual_count,
                                &paragraph) != NX_OK || actual_count != count ||
                visual_count != order_count) {
                fprintf(stderr,"BidiTest line %zu bit %u: analyzer/count\n",line_number,bit);
                return 1;
            }
            for (size_t i = 0; i < count; ++i)
                if (actual[i] != expected_levels[i]) {
                    fprintf(stderr,"BidiTest line %zu bit %u scalar %zu: level %u expected %u\n",
                            line_number,bit,i,actual[i],expected_levels[i]);
                    return 1;
                }
            for (size_t i = 0; i < order_count; ++i)
                if (order[i] != expected_order[i]) {
                    fprintf(stderr,"BidiTest line %zu bit %u visual %zu: %zu expected %u\n",
                            line_number,bit,i,order[i],expected_order[i]);
                    return 1;
                }
            ++cases;
        }
    }
    fclose(file);
    if (cases < 500000) return 1;
    printf("%zu BidiTest modes passed\n",cases);
    return 0;
}
