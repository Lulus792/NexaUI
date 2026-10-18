#include "draw_plan.h"
#include <float.h>
#include <math.h>
#include <stddef.h>

/* The MSL struct uses packed_float2 and uint[4], not SIMD-aligned float2/uint4. */
_Static_assert(sizeof(nx_point) == 8, "Metal vertex layout");
_Static_assert(sizeof(nx_mtl_shape) == 52 && offsetof(nx_mtl_shape,color) == 28 &&
    offsetof(nx_mtl_shape,bitmap_size) == 44,
    "Metal shape argument layout");

nx_result nx_mtl_plan_begin(nx_mtl_plan *p, const nx_command *commands,
    size_t count, double scale, uint32_t width, uint32_t height) {
    if (!p || !isfinite(scale) || scale <= 0) return NX_ERROR_ARGUMENT;
    *p = (nx_mtl_plan){0};
    if (width > 8192u || height > 8192u || count > UINT32_MAX / 6u) return NX_ERROR_LIMIT;
    nx_result result = nx_render_validate_commands(commands,count);
    if (result != NX_OK) return result;
    for (size_t i = 0; i < count; ++i) {
        const nx_command *c = &commands[i];
        if (c->type == NX_COMMAND_POP_CLIP) continue;
        if (c->type != NX_COMMAND_PUSH_CLIP && c->type != NX_COMMAND_FILL_RECT &&
            c->type != NX_COMMAND_FILL_TRIANGLE && c->type != NX_COMMAND_STROKE_RECT &&
            c->type != NX_COMMAND_FILL_ROUND_RECT && c->type != NX_COMMAND_LINE && c->type != NX_COMMAND_BITMAP) return NX_ERROR_UNSUPPORTED;
        if (c->type == NX_COMMAND_BITMAP && (c->bitmap.width > 8192u || c->bitmap.height > 8192u ||
            (uint64_t)c->bitmap.width*c->bitmap.height > (64u*1024u*1024u)/4u)) return NX_ERROR_LIMIT;
        double values[8] = {0};
        if (c->type == NX_COMMAND_FILL_TRIANGLE || c->type == NX_COMMAND_LINE) {
            unsigned n = c->type == NX_COMMAND_LINE ? 2u : 3u;
            for (unsigned j = 0; j < n; ++j) { values[j*2] = c->points[j].x; values[j*2+1] = c->points[j].y; }
        } else {
            values[0] = c->rect.x; values[1] = c->rect.y;
            values[2] = (double)c->rect.x + c->rect.width; values[3] = (double)c->rect.y + c->rect.height;
            values[4] = c->rect.width; values[5] = c->rect.height;
        }
        if (c->type == NX_COMMAND_LINE || c->type == NX_COMMAND_STROKE_RECT) values[6] = c->thickness;
        if (c->type == NX_COMMAND_FILL_ROUND_RECT) values[7] = c->radius;
        for (unsigned j = 0; j < 8; ++j) {
            double scaled = values[j] * scale * 2.0;
            if (!isfinite(scaled) || fabs(scaled) > (double)FLT_MAX / 2.0) return NX_ERROR_LIMIT;
            /* Float32 split products need finite intermediate headroom. */
            if (c->type == NX_COMMAND_FILL_TRIANGLE && fabs(scaled) > sqrt((double)FLT_MAX)/4) return NX_ERROR_LIMIT;
            if (c->type == NX_COMMAND_LINE && fabs(scaled) > sqrt(sqrt((double)FLT_MAX))/16) return NX_ERROR_LIMIT;
            if (c->type == NX_COMMAND_FILL_ROUND_RECT && fabs(scaled) > sqrt((double)FLT_MAX)/8) return NX_ERROR_LIMIT;
            if (c->type == NX_COMMAND_BITMAP && fabs(scaled) > sqrt((double)FLT_MAX)/32768) return NX_ERROR_LIMIT;
        }
    }
    p->commands = commands; p->count = count; p->scale = scale; p->width = width; p->height = height;
    p->clips[0] = (nx_mtl_box){0,0,width,height}; return NX_OK;
}

static double edge(double coordinate, double scale, uint32_t limit) {
    double pixel = ceil(coordinate * scale - .5);
    return pixel < 0 ? 0 : pixel > limit ? limit : pixel;
}
int nx_mtl_plan_next(nx_mtl_plan *p, nx_mtl_draw *out) {
    if (!p || !out || !p->width || !p->height) return 0;
    while (p->index < p->count) {
        const nx_command *c = &p->commands[p->index++];
        if (c->type == NX_COMMAND_POP_CLIP) { --p->depth; continue; }
        nx_mtl_shape s = {0};
        double x0, y0, x1, y1;
        if (c->type == NX_COMMAND_LINE) {
            s.kind = 4;
            s.a = (nx_point){(float)(c->points[0].x*p->scale),(float)(c->points[0].y*p->scale)};
            s.b = (nx_point){(float)(c->points[1].x*p->scale),(float)(c->points[1].y*p->scale)};
            s.c.x = (float)(c->thickness*p->scale)*.5f;
            x0 = edge(fmin(s.a.x,s.b.x)-s.c.x,1,p->width); y0 = edge(fmin(s.a.y,s.b.y)-s.c.x,1,p->height);
            x1 = edge(fmax(s.a.x,s.b.x)+s.c.x,1,p->width); y1 = edge(fmax(s.a.y,s.b.y)+s.c.x,1,p->height);
        } else if (c->type == NX_COMMAND_FILL_TRIANGLE) {
            s.kind = 1;
            s.a = (nx_point){(float)(c->points[0].x*p->scale),(float)(c->points[0].y*p->scale)};
            s.b = (nx_point){(float)(c->points[1].x*p->scale),(float)(c->points[1].y*p->scale)};
            s.c = (nx_point){(float)(c->points[2].x*p->scale),(float)(c->points[2].y*p->scale)};
            double orient = ((double)s.b.x-s.a.x)*((double)s.c.y-s.a.y)-((double)s.b.y-s.a.y)*((double)s.c.x-s.a.x);
            if (!orient) continue;
            if (orient < 0) { nx_point swap = s.b; s.b = s.c; s.c = swap; }
            x0 = edge(fmin(s.a.x,fmin(s.b.x,s.c.x)),1,p->width); y0 = edge(fmin(s.a.y,fmin(s.b.y,s.c.y)),1,p->height);
            x1 = edge(fmax(s.a.x,fmax(s.b.x,s.c.x)),1,p->width); y1 = edge(fmax(s.a.y,fmax(s.b.y,s.c.y)),1,p->height);
        } else if (c->type == NX_COMMAND_STROKE_RECT || c->type == NX_COMMAND_FILL_ROUND_RECT || c->type == NX_COMMAND_BITMAP) {
            s.kind = c->type == NX_COMMAND_STROKE_RECT ? 2u : c->type == NX_COMMAND_FILL_ROUND_RECT ? 3u : 5u;
            s.a = (nx_point){(float)(c->rect.x*p->scale),(float)(c->rect.y*p->scale)};
            s.b = (nx_point){(float)(c->rect.width*p->scale),(float)(c->rect.height*p->scale)};
            if (s.kind == 5) {
                if (!s.b.x || !s.b.y) continue;
                s.bitmap_size = (nx_point){(float)c->bitmap.width,(float)c->bitmap.height};
            } else s.c.x = s.kind == 2 ? (float)(c->thickness*p->scale) : fminf((float)(c->radius*p->scale),fminf(s.b.x,s.b.y)*.5f);
            x0 = edge(s.a.x,1,p->width); y0 = edge(s.a.y,1,p->height);
            x1 = edge((double)s.a.x+s.b.x,1,p->width); y1 = edge((double)s.a.y+s.b.y,1,p->height);
        } else {
            /* Match the physical float command contract before adding edges.
               A logical sum multiplied in double can cross a different pixel
               center than separately scaled origin/extent (HiDPI ink clips). */
            nx_point origin = {(float)(c->rect.x*p->scale),(float)(c->rect.y*p->scale)};
            nx_point extent = {(float)(c->rect.width*p->scale),(float)(c->rect.height*p->scale)};
            x0 = edge(origin.x,1,p->width); y0 = edge(origin.y,1,p->height);
            x1 = edge((double)origin.x+extent.x,1,p->width);
            y1 = edge((double)origin.y+extent.y,1,p->height);
        }
        nx_mtl_box clip = p->clips[p->depth];
        x0 = fmax(x0,clip.x); y0 = fmax(y0,clip.y);
        x1 = fmin(x1,(double)clip.x+clip.width); y1 = fmin(y1,(double)clip.y+clip.height);
        nx_mtl_box box = {(uint32_t)x0,(uint32_t)y0,(uint32_t)fmax(0,x1-x0),(uint32_t)fmax(0,y1-y0)};
        if (c->type == NX_COMMAND_PUSH_CLIP) { p->clips[++p->depth] = box; continue; }
        if (!box.width || !box.height || floorf(c->color.a*255.f+.5f) == 0) continue;
        *out = (nx_mtl_draw){0}; out->scissor = box; out->shape = s;
        if (s.kind == 5) out->bitmap = c->bitmap;
        const float channels[] = {c->color.r,c->color.g,c->color.b,c->color.a};
        for (unsigned j = 0; j < 4; ++j) out->shape.color[j] = (uint32_t)floorf(channels[j]*255.f+.5f);
        double xy[6][2] = {{x0,y0},{x1,y0},{x0,y1},{x1,y0},{x1,y1},{x0,y1}};
        for (unsigned j = 0; j < 6; ++j) out->vertices[j] = (nx_point){
            (float)(xy[j][0]*2.0/p->width-1.0),(float)(1.0-xy[j][1]*2.0/p->height)};
        return 1;
    }
    return 0;
}
