#include "bitmap_plan.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr,"%s:%d: %s\n",__FILE__,__LINE__,#x); return 1; } } while (0)
static nx_command image(nx_bitmap_view bitmap) {
    nx_command c = {.type=NX_COMMAND_BITMAP,.rect={0,0,17,23},.color={1,1,1,1}};
    c.bitmap = bitmap; return c;
}
int main(void) {
    nx_mtl_bitmap_plan *p = calloc(1,sizeof(*p)); CHECK(p);
    uint8_t rgba[24] = {1,2,3,4, 91,82,73,64, 0xee,0xee,0xee,0xee,
        201,202,203,204, 81,72,63,54, 0xee,0xee,0xee,0xee};
    uint8_t a8[8] = {0,93,255,0xee, 1,128,213,0xee};
    nx_command commands[] = {image((nx_bitmap_view){rgba,2,2,12,NX_BITMAP_RGBA8}),
        image((nx_bitmap_view){a8,3,2,4,NX_BITMAP_A8}), image((nx_bitmap_view){rgba,2,2,12,NX_BITMAP_RGBA8})};
    CHECK(nx_mtl_bitmaps_build(p,commands,3,0,0) == NX_OK && p->count == 2 && p->width == 1024 && p->height == 2);
    CHECK(nx_mtl_bitmap_find(p,commands[2].bitmap) == 0);
    CHECK(nx_mtl_bitmap_find(p,(nx_bitmap_view){rgba,1,1,4,NX_BITMAP_RGBA8}) == p->count);
    size_t stride = (size_t)p->width*4u+13u, bytes = stride*p->height;
    uint8_t *pixels = malloc(bytes); CHECK(pixels); memset(pixels,0xa7,bytes);
    CHECK(nx_mtl_bitmaps_copy(p,pixels,bytes,stride) == NX_OK);
    for (uint32_t y = 0; y < p->height; ++y) {
        for (uint32_t x = 0; x < p->width; ++x) {
            const uint8_t *q = pixels+(size_t)y*stride+x*4u;
            if (x < 2) CHECK(!memcmp(q,rgba+(size_t)y*12+x*4u,4));
            else if (x < 5) CHECK(q[0] == 255 && q[1] == 255 && q[2] == 255 && q[3] == a8[y*4+x-2]);
            else CHECK(!q[0] && !q[1] && !q[2] && !q[3]);
        }
        for (size_t x = (size_t)p->width*4u; x < stride; ++x) CHECK(pixels[(size_t)y*stride+x] == 0xa7);
    }
    CHECK(nx_mtl_bitmaps_copy(p,pixels,bytes-1,stride) == NX_ERROR_ARGUMENT);
    CHECK(nx_mtl_bitmaps_copy(p,pixels,bytes,p->width*4u-1u) == NX_ERROR_ARGUMENT);
    commands[1].color.a = 0; commands[2].rect.width = 0;
    CHECK(nx_mtl_bitmaps_build(p,commands,3,2048,8) == NX_OK && p->count == 1 && p->width == 2048 && p->height == 8);
    CHECK(nx_mtl_bitmaps_build(p,NULL,0,0,0) == NX_OK && !p->count && !p->width && !p->height);
    CHECK(nx_mtl_bitmaps_copy(p,NULL,0,0) == NX_OK);
    CHECK(nx_mtl_bitmaps_build(p,commands,3,1234,0) == NX_ERROR_ARGUMENT);
    CHECK(nx_mtl_bitmaps_build(NULL,NULL,0,0,0) == NX_ERROR_ARGUMENT);
    CHECK(nx_mtl_bitmaps_build(p,NULL,1,0,0) == NX_ERROR_ARGUMENT);
    nx_command invalid = commands[0]; invalid.bitmap.stride_bytes = 1;
    CHECK(nx_mtl_bitmaps_build(p,&invalid,1,0,0) == NX_ERROR_COMMAND);
    /* Exercise row wrapping and widening without reading these large views. */
    nx_command shelves[3];
    for (unsigned i = 0; i < 3; ++i) shelves[i] = image((nx_bitmap_view){rgba+i,1024,4096,4096,NX_BITMAP_RGBA8});
    CHECK(nx_mtl_bitmaps_build(p,shelves,3,0,0) == NX_OK && p->width == 2048 && p->height == 8192);
    CHECK(p->entries[0].x == 0 && p->entries[1].x == 1024 && p->entries[2].y == 4096);
    nx_command huge = image((nx_bitmap_view){rgba,8192,8192,32768,NX_BITMAP_RGBA8});
    CHECK(nx_mtl_bitmaps_build(p,&huge,1,0,0) == NX_ERROR_LIMIT);
    uint8_t *sources = malloc(NX_MTL_BITMAP_LIMIT+1u);
    nx_command *many = calloc(NX_MTL_BITMAP_LIMIT+1u,sizeof(*many)); CHECK(sources && many);
    for (unsigned i = 0; i <= NX_MTL_BITMAP_LIMIT; ++i)
        many[i] = image((nx_bitmap_view){sources+i,1,1,1,NX_BITMAP_A8});
    CHECK(nx_mtl_bitmaps_build(p,many,NX_MTL_BITMAP_LIMIT,0,0) == NX_OK && p->count == NX_MTL_BITMAP_LIMIT);
    CHECK(nx_mtl_bitmaps_build(p,many,NX_MTL_BITMAP_LIMIT+1u,0,0) == NX_ERROR_LIMIT);
    free(many); free(sources); free(pixels); free(p);
    puts("Metal atlas deduplication, shelf bounds, RGBA/A8 copies and padding checks passed.");
    return 0;
}
