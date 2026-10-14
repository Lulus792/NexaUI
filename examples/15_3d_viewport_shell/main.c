#include "nexaui/nexaui.h"

#include <stdio.h>
#include <stdlib.h>

#define WIDTH 360u
#define HEIGHT 260u

typedef struct scene_snapshot {
    float cosine, sine;
} scene_snapshot;

typedef struct cube_surface {
    uint8_t *pixels;
    uint32_t width, height;
    int leased;
    const scene_snapshot *snapshot;
} cube_surface;

static void pixel(cube_surface *surface, int x, int y,
                  uint8_t red, uint8_t green, uint8_t blue) {
    if (x < 0 || y < 0 || (uint32_t)x >= surface->width ||
        (uint32_t)y >= surface->height) return;
    size_t i = ((size_t)y * surface->width + (size_t)x) * 4u;
    surface->pixels[i] = red;
    surface->pixels[i + 1u] = green;
    surface->pixels[i + 2u] = blue;
    surface->pixels[i + 3u] = 255;
}

static void line(cube_surface *surface, int x0, int y0, int x1, int y1,
                 uint8_t red, uint8_t green, uint8_t blue) {
    int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int error = dx + dy;
    unsigned thickness = surface->width / 320u;
    if (!thickness) thickness = 1;
    for (;;) {
        for (unsigned oy = 0; oy < thickness; ++oy)
            for (unsigned ox = 0; ox < thickness; ++ox)
                pixel(surface, x0 + (int)ox, y0 + (int)oy,
                      red, green, blue);
        if (x0 == x1 && y0 == y1) break;
        int twice = error * 2;
        if (twice >= dy) { error += dy; x0 += sx; }
        if (twice <= dx) { error += dx; y0 += sy; }
    }
}

static void draw_cube(cube_surface *surface) {
    for (uint32_t y = 0; y < surface->height; ++y)
        for (uint32_t x = 0; x < surface->width; ++x)
            pixel(surface, (int)x, (int)y, 16, 28, 47);
    static const int corners[8][3] = {
        {-1,-1,-1}, {1,-1,-1}, {1,1,-1}, {-1,1,-1},
        {-1,-1, 1}, {1,-1, 1}, {1,1, 1}, {-1,1, 1}
    };
    static const unsigned edges[12][2] = {
        {0,1}, {1,2}, {2,3}, {3,0}, {4,5}, {5,6},
        {6,7}, {7,4}, {0,4}, {1,5}, {2,6}, {3,7}
    };
    int projected[8][2];
    for (unsigned i = 0; i < 8; ++i) {
        float x = (float)corners[i][0];
        float y = (float)corners[i][1];
        float z = (float)corners[i][2];
        float rx = surface->snapshot->cosine * x +
                   surface->snapshot->sine * z;
        float rz = -surface->snapshot->sine * x +
                    surface->snapshot->cosine * z;
        float depth = 4.0f + rz;
        float scale = (float)surface->height * 0.78f / depth;
        projected[i][0] = (int)((float)surface->width * 0.5f + rx * scale);
        projected[i][1] = (int)((float)surface->height * 0.5f - y * scale);
    }
    for (unsigned i = 0; i < 12; ++i) {
        unsigned a = edges[i][0], b = edges[i][1];
        line(surface, projected[a][0], projected[a][1],
             projected[b][0], projected[b][1], 110, 224, 216);
    }
}

static nx_result resize_surface(void *user, uint32_t width, uint32_t height) {
    cube_surface *surface = user;
    if (surface->leased) return NX_ERROR_STATE;
    if (!width || !height) {
        free(surface->pixels);
        surface->pixels = NULL;
        surface->width = surface->height = 0;
        return NX_OK;
    }
    if ((size_t)width > SIZE_MAX / 4u / height) return NX_ERROR_LIMIT;
    size_t bytes = (size_t)width * height * 4u;
    uint8_t *replacement = malloc(bytes);
    if (!replacement) return NX_ERROR_MEMORY;
    free(surface->pixels);
    surface->pixels = replacement;
    surface->width = width;
    surface->height = height;
    return NX_OK;
}

static nx_result acquire_surface(void *user, nx_bitmap_view *out) {
    cube_surface *surface = user;
    if (surface->leased || !surface->pixels || !surface->snapshot)
        return NX_ERROR_STATE;
    draw_cube(surface);
    surface->leased = 1;
    *out = (nx_bitmap_view){surface->pixels, surface->width,
                            surface->height, (size_t)surface->width * 4u,
                            NX_BITMAP_RGBA8};
    return NX_OK;
}

static void release_surface(void *user) {
    ((cube_surface *)user)->leased = 0;
}

static int save_frame(nx_context *ui, const char *path) {
    const nx_command *commands = NULL;
    size_t count = 0;
    if (nx_frame_commands(ui, &commands, &count) != NX_OK) return 0;
    uint8_t *pixels = malloc((size_t)WIDTH * HEIGHT * 4u);
    if (!pixels) return 0;
    int okay = nx_headless_render(commands, count, pixels, WIDTH, HEIGHT,
                                  WIDTH * 4u, (nx_color){0.04f, 0.06f, 0.10f, 1}) == NX_OK;
    FILE *file = okay ? fopen(path, "wb") : NULL;
    if (!file) okay = 0;
    if (okay && fprintf(file, "P6\n%u %u\n255\n", WIDTH, HEIGHT) < 0) okay = 0;
    for (size_t i = 0; okay && i < (size_t)WIDTH * HEIGHT; ++i)
        if (fwrite(pixels + i * 4u, 1, 3, file) != 3) okay = 0;
    if (file && fclose(file) != 0) okay = 0;
    free(pixels);
    return okay;
}

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "usage: nexaui_3d_viewport_shell OUTPUT.ppm\n");
        return 2;
    }
    int status = 1;
    cube_surface image = {0};
    nx_viewport_source source = {&image, resize_surface, acquire_surface,
                                  release_surface};
    nx_viewport_surface *surface = NULL;
    nx_context *ui = NULL;
    if (nx_viewport_surface_create(&source, NULL, &surface) != NX_OK ||
        nx_context_create(NULL, &ui) != NX_OK) goto cleanup;
    nx_viewport_desc desc = {0};
    desc.id = 7;
    desc.bounds = (nx_rect){20, 20, 320, 220};
    desc.name = (nx_utf8_span){"Rotating cube", 13};
    desc.scale = 1;
    scene_snapshot first = {1, 0};
    scene_snapshot second = {0.8660254f, 0.5f};
    nx_interaction_state interaction;
    image.snapshot = &first;
    if (nx_begin_frame(ui, WIDTH, HEIGHT, 0) != NX_OK ||
        nx_viewport(ui, surface, &desc, &interaction) != NX_OK ||
        nx_end_frame(ui) != NX_OK ||
        nx_viewport_surface_complete(surface) != NX_OK ||
        nx_focus_set(ui, desc.id) != NX_OK) goto cleanup;
    image.snapshot = &second;
    desc.scale = 2;
    if (nx_begin_frame(ui, WIDTH, HEIGHT, 1.0 / 60.0) != NX_OK ||
        nx_viewport(ui, surface, &desc, &interaction) != NX_OK ||
        !interaction.focused || nx_end_frame(ui) != NX_OK ||
        !save_frame(ui, argv[1]) ||
        nx_viewport_surface_complete(surface) != NX_OK) goto cleanup;
    printf("wrote %s (%u x %u surface pixels)\n", argv[1],
           image.width, image.height);
    status = 0;
cleanup:
    if (status) fprintf(stderr, "viewport shell failed\n");
    if (image.leased && surface) (void)nx_viewport_surface_complete(surface);
    if (surface) (void)nx_viewport_surface_destroy(surface);
    nx_context_destroy(ui);
    free(image.pixels);
    return status;
}
