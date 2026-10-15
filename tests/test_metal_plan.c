#include "draw_plan.h"
#include <float.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr,"%s:%d: %s\n",__FILE__,__LINE__,#x); return 1; } } while (0)

static nx_command rect(nx_command_type type, nx_rect r, nx_color color) {
    nx_command c = {0}; c.type = type; c.rect = r; c.color = color; return c;
}
/* Interpret prepared shape arguments with the independent software renderer.
   This verifies CPU preparation, not execution of the MSL or Metal rasterizer. */
static int compare(const nx_command *stream, size_t count, double scale) {
    enum { W = 97, H = 83, STRIDE = W*4+13 };
    uint8_t reference[STRIDE*H], actual[STRIDE*H];
    nx_command scaled[512], rebuilt[1536];
    CHECK(count <= 512);
    for (size_t i = 0; i < count; ++i) {
        scaled[i] = stream[i]; nx_command *c = &scaled[i];
        if (c->type == NX_COMMAND_FILL_TRIANGLE || c->type == NX_COMMAND_LINE) {
            for (unsigned j = 0; j < 3; ++j) {
                c->points[j].x = (float)(c->points[j].x*scale); c->points[j].y = (float)(c->points[j].y*scale);
            }
        } else if (c->type != NX_COMMAND_POP_CLIP) {
            c->rect.x = (float)(c->rect.x*scale); c->rect.y = (float)(c->rect.y*scale);
            c->rect.width = (float)(c->rect.width*scale); c->rect.height = (float)(c->rect.height*scale);
        }
        if (c->type == NX_COMMAND_LINE || c->type == NX_COMMAND_STROKE_RECT) c->thickness = (float)(c->thickness*scale);
        if (c->type == NX_COMMAND_FILL_ROUND_RECT) c->radius = (float)(c->radius*scale);
    }
    nx_mtl_plan p; CHECK(nx_mtl_plan_begin(&p,stream,count,scale,W,H) == NX_OK);
    nx_mtl_draw d; size_t n = 0;
    while (nx_mtl_plan_next(&p,&d)) {
        CHECK(n+3 <= 1536 && d.scissor.width && d.scissor.height);
        CHECK(d.scissor.x+d.scissor.width <= W && d.scissor.y+d.scissor.height <= H);
        CHECK(fabs((d.vertices[0].x+1)*W*.5-d.scissor.x) < .0001);
        CHECK(fabs((1-d.vertices[0].y)*H*.5-d.scissor.y) < .0001);
        CHECK(fabs((d.vertices[4].x+1)*W*.5-d.scissor.x-d.scissor.width) < .0001);
        CHECK(fabs((1-d.vertices[4].y)*H*.5-d.scissor.y-d.scissor.height) < .0001);
        nx_rect box = {(float)d.scissor.x,(float)d.scissor.y,(float)d.scissor.width,(float)d.scissor.height};
        rebuilt[n++] = rect(NX_COMMAND_PUSH_CLIP,box,(nx_color){0});
        nx_command c = {0};
        c.color = (nx_color){d.shape.color[0]/255.f,d.shape.color[1]/255.f,d.shape.color[2]/255.f,d.shape.color[3]/255.f};
        if (d.shape.kind == 0) { c.type = NX_COMMAND_FILL_RECT; c.rect = box; }
        else if (d.shape.kind == 1) {
            c.type = NX_COMMAND_FILL_TRIANGLE;
            c.points[0] = d.shape.a; c.points[1] = d.shape.b; c.points[2] = d.shape.c;
        } else if (d.shape.kind == 4) {
            c.type = NX_COMMAND_LINE; c.points[0] = d.shape.a; c.points[1] = d.shape.b; c.thickness = d.shape.c.x*2;
        } else {
            c.type = d.shape.kind == 2 ? NX_COMMAND_STROKE_RECT : NX_COMMAND_FILL_ROUND_RECT;
            c.rect = (nx_rect){d.shape.a.x,d.shape.a.y,d.shape.b.x,d.shape.b.y};
            c.thickness = c.radius = d.shape.c.x;
        }
        rebuilt[n++] = c; rebuilt[n++] = (nx_command){.type=NX_COMMAND_POP_CLIP};
    }
    memset(reference,0xa7,sizeof(reference)); memset(actual,0xa7,sizeof(actual));
    nx_color clear = {.03f,.07f,.11f,.26f};
    CHECK(nx_headless_render(scaled,count,reference,W,H,STRIDE,clear) == NX_OK);
    CHECK(nx_headless_render(rebuilt,n,actual,W,H,STRIDE,clear) == NX_OK);
    for (size_t i = 0; i < sizeof(actual); ++i) if (actual[i] != reference[i]) {
        fprintf(stderr,"plan scale=%g pixel %zu,%zu channel %zu: %u != %u\n",
            scale,(i%STRIDE)/4,i/STRIDE,(i%STRIDE)%4,actual[i],reference[i]); return 1;
    }
    return 0;
}
int main(void) {
    nx_command stream[512]; size_t n = 0;
    stream[n++] = rect(NX_COMMAND_FILL_RECT,(nx_rect){-4,.5f,30.5f,25.5f},(nx_color){.5f,.2f,.7f,1});
    stream[n++] = rect(NX_COMMAND_PUSH_CLIP,(nx_rect){5.5f,4.5f,69,62},(nx_color){0});
    stream[n++] = rect(NX_COMMAND_PUSH_CLIP,(nx_rect){8.5f,7.5f,47,48},(nx_color){0});
    for (unsigned i = 0; i < 120; ++i) {
        nx_command c = rect((nx_command_type)(i%5 == 0 ? NX_COMMAND_FILL_RECT : i%5 == 1 ?
            NX_COMMAND_STROKE_RECT : i%5 == 2 ? NX_COMMAND_FILL_ROUND_RECT : i%5 == 3 ? NX_COMMAND_LINE : NX_COMMAND_FILL_TRIANGLE),
            (nx_rect){(float)((i*17)%105)-12.5f,(float)((i*11)%90)-10.25f,19.5f,16.25f},
            (nx_color){(i%17)/16.f,(i%23)/22.f,(i%29)/28.f,1});
        c.radius = 4.5f; c.thickness = 2.5f;
        c.points[0] = (nx_point){c.rect.x,c.rect.y}; c.points[1] = (nx_point){c.rect.x+19,c.rect.y+3};
        c.points[2] = (nx_point){c.rect.x+4,c.rect.y+20};
        stream[n++] = c;
    }
    stream[n++] = (nx_command){.type=NX_COMMAND_POP_CLIP};
    stream[n++] = rect(NX_COMMAND_PUSH_CLIP,(nx_rect){150,130,10,10},(nx_color){0});
    stream[n++] = rect(NX_COMMAND_FILL_RECT,(nx_rect){0,0,97,83},(nx_color){1,1,1,1});
    stream[n++] = (nx_command){.type=NX_COMMAND_POP_CLIP};
    stream[n++] = (nx_command){.type=NX_COMMAND_POP_CLIP};
    stream[n++] = rect(NX_COMMAND_FILL_RECT,(nx_rect){0,0,97,83},(nx_color){1,1,1,0});
    const double scales[] = {1,1.25,1.5,2};
    for (unsigned i = 0; i < 4; ++i) CHECK(compare(stream,n,scales[i]) == 0);
    nx_mtl_plan p; nx_mtl_draw d;
    CHECK(nx_mtl_plan_begin(&p,stream,n,1,0,0) == NX_OK && !nx_mtl_plan_next(&p,&d));
    CHECK(nx_mtl_plan_begin(NULL,NULL,0,1,1,1) == NX_ERROR_ARGUMENT);
    CHECK(nx_mtl_plan_begin(&p,NULL,1,1,1,1) == NX_ERROR_ARGUMENT);
    CHECK(nx_mtl_plan_begin(&p,NULL,0,NAN,1,1) == NX_ERROR_ARGUMENT);
    CHECK(nx_mtl_plan_begin(&p,NULL,0,1,8193,1) == NX_ERROR_LIMIT);
    nx_command bad = {.type=NX_COMMAND_POP_CLIP};
    CHECK(nx_mtl_plan_begin(&p,&bad,1,1,1,1) == NX_ERROR_COMMAND);
    bad = stream[0]; bad.color.a = .5f;
    CHECK(nx_mtl_plan_begin(&p,&bad,1,1,1,1) == NX_ERROR_UNSUPPORTED);
    bad.color.a = 1; bad.rect.x = FLT_MAX;
    CHECK(nx_mtl_plan_begin(&p,&bad,1,4,1,1) == NX_ERROR_LIMIT);
    const uint8_t pixel[] = {255,255,255,255};
    bad = (nx_command){.type=NX_COMMAND_BITMAP,.rect={0,0,1,1},.color={1,1,1,1},
        .bitmap={pixel,1,1,4,NX_BITMAP_RGBA8}};
    CHECK(nx_mtl_plan_begin(&p,&bad,1,1,1,1) == NX_ERROR_UNSUPPORTED);
    bad = (nx_command){.type=NX_COMMAND_FILL_TRIANGLE,.color={1,1,1,1}};
    CHECK(nx_mtl_plan_begin(&p,&bad,1,1,1,1) == NX_OK && !nx_mtl_plan_next(&p,&d));
    bad.points[0] = (nx_point){-1000000,-1000000.0625f};
    bad.points[1] = (nx_point){1000000,999999.9375f}; bad.points[2] = (nx_point){-1000000,999999.9375f};
    CHECK(compare(&bad,1,1) == 0);
    n = 0;
    for (unsigned i = 0; i < NX_MAX_CLIP_DEPTH; ++i)
        stream[n++] = rect(NX_COMMAND_PUSH_CLIP,(nx_rect){i*.25f,i*.25f,61,53},(nx_color){0});
    stream[n++] = rect(NX_COMMAND_FILL_RECT,(nx_rect){0,0,97,83},(nx_color){1,1,1,1});
    for (unsigned i = 0; i < NX_MAX_CLIP_DEPTH; ++i) stream[n++] = (nx_command){.type=NX_COMMAND_POP_CLIP};
    CHECK(compare(stream,n,1.5) == 0);
    puts("Metal CPU draw preparation matches independent headless interpretation; MSL/GPU validation is separate.");
    return 0;
}
