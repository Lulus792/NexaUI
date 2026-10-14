#include "cursor.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define CHECK(expr) do { if (!(expr)) { \
    fprintf(stderr, "%s:%d: %s failed\n", __FILE__, __LINE__, #expr); \
    return 1; \
} } while (0)

int main(int argc, char **argv) {
    uint32_t sheet[11][NX_WAYLAND_CURSOR_SIZE * NX_WAYLAND_CURSOR_SIZE];
    uint64_t hashes[11] = {0};
    for (int shape = NX_CURSOR_DEFAULT; shape <= NX_CURSOR_BUSY; ++shape) {
        int hot_x = -1, hot_y = -1;
        nx_wayland_cursor_render((nx_cursor_shape)shape, sheet[shape],
                                  &hot_x, &hot_y);
        CHECK(hot_x >= 0 && hot_x < NX_WAYLAND_CURSOR_SIZE &&
              hot_y >= 0 && hot_y < NX_WAYLAND_CURSOR_SIZE);
        unsigned ink = 0, clear = 0;
        uint64_t hash = 1469598103934665603ull;
        for (size_t i = 0; i < NX_WAYLAND_CURSOR_SIZE *
                               NX_WAYLAND_CURSOR_SIZE; ++i) {
            uint32_t pixel = sheet[shape][i];
            if (pixel == 0xff151515u) ++ink;
            if (pixel == 0) ++clear;
            hash ^= pixel;
            hash *= 1099511628211ull;
        }
        CHECK(ink > 20 && ink < 800 && clear > 20);
        hashes[shape] = hash;
        for (int previous = NX_CURSOR_DEFAULT; previous < shape; ++previous)
            CHECK(hash != hashes[previous]);
    }
    if (argc == 3 && !strcmp(argv[1], "--dump")) {
        FILE *file = fopen(argv[2], "wb");
        CHECK(file);
        const int zoom = 4;
        fprintf(file, "P6\n%d %d\n255\n",
                11 * NX_WAYLAND_CURSOR_SIZE * zoom,
                NX_WAYLAND_CURSOR_SIZE * zoom);
        for (int y = 0; y < NX_WAYLAND_CURSOR_SIZE; ++y)
            for (int zy = 0; zy < zoom; ++zy)
                for (int shape = NX_CURSOR_DEFAULT;
                     shape <= NX_CURSOR_BUSY; ++shape)
                    for (int x = 0; x < NX_WAYLAND_CURSOR_SIZE; ++x) {
                        uint32_t pixel = sheet[shape][y *
                                                NX_WAYLAND_CURSOR_SIZE + x];
                        unsigned char value = pixel == 0 ? 224 :
                                              pixel == 0xff151515u ? 21 : 255;
                        unsigned char rgb[3] = {value, value, value};
                        for (int zx = 0; zx < zoom; ++zx)
                            CHECK(fwrite(rgb, 1, sizeof(rgb), file) ==
                                  sizeof(rgb));
                    }
        CHECK(fclose(file) == 0);
    }
    puts("Wayland cursor shapes and hotspots verified");
    return 0;
}
