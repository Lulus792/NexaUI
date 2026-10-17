#include "internal.h"
#include <float.h>
#include <stdlib.h>
#include <string.h>

struct nx_text_stamp {
    nx_allocator allocator;
    uint8_t *pixels;
    char *text;
    size_t length, bytes;
    nx_bitmap_view view;
    nx_point offset;
    nx_rect bounds;
};
static void *nx_stamp_alloc(void *user, size_t bytes) { (void)user; return malloc(bytes); }
static void nx_stamp_free(void *user, void *p, size_t bytes) { (void)user; (void)bytes; free(p); }

typedef struct nx_stamp_work {
    nx_allocator base;
    size_t live;
    int limited;
} nx_stamp_work;
static void *nx_stamp_work_alloc(void *user,size_t bytes) {
    nx_stamp_work *work=user;
    if(bytes>64u*1024u*1024u-work->live){work->limited=1;return NULL;}
    void *p=work->base.allocate(work->base.user,bytes);
    if(p)work->live+=bytes;
    return p;
}
static void nx_stamp_work_free(void *user,void *p,size_t bytes) {
    nx_stamp_work *work=user;
    work->live-=bytes;
    work->base.deallocate(work->base.user,p,bytes);
}

void nx_text_stamp_destroy(nx_text_stamp *stamp) {
    if (!stamp) return;
    nx_allocator a = stamp->allocator;
    if (stamp->pixels) a.deallocate(a.user, stamp->pixels, stamp->bytes);
    if (stamp->text) a.deallocate(a.user, stamp->text, stamp->length + 1u);
    a.deallocate(a.user, stamp, sizeof(*stamp));
}

nx_result nx_text_stamp_create(const nx_text_layout *layout, size_t cache_bytes,
                                const nx_allocator *allocator, nx_text_stamp **out) {
    return nx_text_stamp_create_scaled(layout,1,cache_bytes,allocator,out);
}

nx_result nx_text_stamp_create_scaled(const nx_text_layout *layout,float raster_scale,
    size_t cache_bytes,const nx_allocator *allocator,nx_text_stamp **out) {
    if (!out) return NX_ERROR_ARGUMENT;
    *out = NULL;
    if (!layout || !cache_bytes || !isfinite(raster_scale) ||
        raster_scale<.125f || raster_scale>8 || (allocator &&
        (!!allocator->allocate != !!allocator->deallocate))) return NX_ERROR_ARGUMENT;
    double ppem=nx_text_layout_pixels_per_em(layout),physical=ppem*raster_scale;
    if(ppem<.125||ppem>16384||!isfinite(physical)||physical<.125||physical>16384)
        return NX_ERROR_ARGUMENT;
    nx_allocator base = allocator && allocator->allocate ? *allocator :
        (nx_allocator){NULL, nx_stamp_alloc, nx_stamp_free};
    nx_stamp_work work={base,0,0};
    nx_allocator a={&work,nx_stamp_work_alloc,nx_stamp_work_free};
    nx_text_stamp *stamp = a.allocate(a.user, sizeof(*stamp));
    if (!stamp) return NX_ERROR_MEMORY;
    memset(stamp, 0, sizeof(*stamp)); stamp->allocator = a;
    nx_utf8_span source;
    nx_result result = nx_text_layout_text(layout, &source);
    nx_context *ui = NULL;
    nx_command *translated = NULL;
    uint8_t *rgba = NULL;
    size_t command_count = 0;
    if (result != NX_OK) goto done;
    if (source.length == SIZE_MAX) { result = NX_ERROR_LIMIT; goto done; }
    stamp->length = source.length;
    stamp->text = a.allocate(a.user, source.length + 1u);
    if (!stamp->text) { result = NX_ERROR_MEMORY; goto done; }
    if (source.length) memcpy(stamp->text, source.data, source.length);
    stamp->text[source.length] = 0;
    nx_context_desc desc = {0}; desc.allocator = a;
    result = nx_context_create(&desc, &ui);
    if (result != NX_OK) goto done;
    result = nx_begin_frame(ui, 1, 1, 0);
    if (result == NX_OK) result = nx_draw_text_layout_pixels(ui, layout, raster_scale,cache_bytes);
    if (result == NX_OK) result = nx_end_frame(ui);
    const nx_command *commands = NULL;
    if (result == NX_OK) result = nx_frame_commands(ui, &commands, &command_count);
    if (result != NX_OK || !command_count) goto done;
    double x0 = DBL_MAX, y0 = DBL_MAX, x1 = -DBL_MAX, y1 = -DBL_MAX;
    for (size_t i = 0; i < command_count; ++i) {
        nx_rect r = commands[i].rect;
        x0 = fmin(x0, ceil((double)r.x - .5));
        y0 = fmin(y0, ceil((double)r.y - .5));
        x1 = fmax(x1, ceil((double)r.x + r.width - .5));
        y1 = fmax(y1, ceil((double)r.y + r.height - .5));
    }
    double w = x1 - x0, h = y1 - y0;
    if (w <= 0 || h <= 0) goto done;
    if (w > 16384 || h > 16384 || w * h > 4194304 ||
        fabs(x0) > 16777216 || fabs(y0) > 16777216 ||
        command_count > SIZE_MAX / sizeof(nx_command)) { result = NX_ERROR_LIMIT; goto done; }
    stamp->bytes = (size_t)w * (size_t)h;
    stamp->pixels = a.allocate(a.user, stamp->bytes);
    rgba = a.allocate(a.user, stamp->bytes * 4u);
    translated = a.allocate(a.user, command_count * sizeof(nx_command));
    if (!stamp->pixels || !rgba || !translated) { result = NX_ERROR_MEMORY; goto done; }
    for (size_t i = 0; i < command_count; ++i) {
        translated[i] = commands[i];
        translated[i].rect.x -= (float)x0; translated[i].rect.y -= (float)y0;
    }
    result = nx_headless_render(translated, command_count, rgba, (uint32_t)w,
                                (uint32_t)h, (size_t)w * 4u, (nx_color){0});
    if (result != NX_OK) goto done;
    for (size_t i = 0; i < stamp->bytes; ++i) stamp->pixels[i] = rgba[i * 4u + 3];
    stamp->view = (nx_bitmap_view){stamp->pixels, (uint32_t)w, (uint32_t)h, (size_t)w, NX_BITMAP_A8};
    stamp->offset = (nx_point){(float)(x0/raster_scale),(float)(y0/raster_scale)};
    stamp->bounds=(nx_rect){stamp->offset.x,stamp->offset.y,
        (float)(w/raster_scale),(float)(h/raster_scale)};
done:
    if (translated) a.deallocate(a.user, translated, command_count * sizeof(nx_command));
    if (rgba) a.deallocate(a.user, rgba, stamp->bytes * 4u);
    nx_context_destroy(ui);
    if (result == NX_OK) {stamp->allocator=base;*out = stamp;}
    else {nx_text_stamp_destroy(stamp);if(result==NX_ERROR_MEMORY&&work.limited)result=NX_ERROR_LIMIT;}
    return result;
}

nx_result nx_text_stamp_text(const nx_text_stamp *stamp, nx_utf8_span *out) {
    if (!stamp || !out) return NX_ERROR_ARGUMENT;
    *out = (nx_utf8_span){stamp->text, stamp->length}; return NX_OK;
}
nx_result nx_text_stamp_view(const nx_text_stamp *stamp, nx_bitmap_view *view, nx_point *offset) {
    if (!stamp || !view || !offset) return NX_ERROR_ARGUMENT;
    *view = stamp->view; *offset = stamp->offset; return NX_OK;
}
nx_result nx_text_stamp_bounds(const nx_text_stamp *stamp,nx_rect *bounds) {
    if(!stamp||!bounds)return NX_ERROR_ARGUMENT;
    *bounds=stamp->bounds;return NX_OK;
}
