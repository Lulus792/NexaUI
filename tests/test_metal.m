#import <AppKit/AppKit.h>
#include "renderer.h"
#include "window_internal.h"
#include <math.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr,"%s:%d: %s\n",__FILE__,__LINE__,#x); return 1; } } while (0)
typedef struct heap { void *p; size_t size, live; int fail; } heap;
static void *allocate(void *user, size_t size) {
    heap *h = user; if (h->fail) return NULL; if (h->live) abort();
    h->p = malloc(size); if (h->p) { h->size = size; h->live = 1; } return h->p;
}
static void deallocate(void *user, void *p, size_t size) {
    heap *h = user; if (h->p != p || h->size != size || h->live != 1) abort();
    free(p); h->p = NULL; h->live = 0;
}
static int compare_commands(nx_window *w, nx_metal_renderer *r, const nx_command *commands,
    size_t count, double scale, nx_color clear) {
    nx_window_info info; CHECK(nx_window_poll(w,0) == NX_OK && nx_window_get_info(w,&info) == NX_OK);
    uint32_t width = (uint32_t)ceil((double)info.width*info.scale), height = (uint32_t)ceil((double)info.height*info.scale);
    size_t stride = (size_t)width * 4u + 17u, bytes = stride * height;
    uint8_t *actual = malloc(bytes), *expected = malloc(bytes); CHECK(actual && expected);
    nx_command *scaled = malloc((count ? count : 1)*sizeof(*scaled)); CHECK(scaled);
    for (size_t i = 0; i < count; ++i) {
        scaled[i] = commands[i]; nx_command *c = &scaled[i];
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
    memset(actual,0xa7,bytes); memset(expected,0xa7,bytes);
    CHECK(nx_headless_render(scaled,count,expected,width,height,stride,clear) == NX_OK);
    nx_result result = NX_ERROR_STATE;
    for (unsigned retry = 0; retry < 10 && result == NX_ERROR_STATE; ++retry) {
        CHECK(nx_window_poll(w,10) == NX_OK);
        result = nx_metal_test_render_readback(r,commands,count,scale,clear,actual,stride);
    }
    CHECK(result == NX_OK);
    if (memcmp(actual,expected,bytes)) {
        for (size_t i = 0; i < bytes; ++i) if (actual[i] != expected[i]) {
            fprintf(stderr,"Metal byte %zu: %u != %u\n",i,actual[i],expected[i]); break;
        }
        return 1;
    }
    free(scaled); free(actual); free(expected); return 0;
}
static int compare(nx_window *w, nx_metal_renderer *r, nx_color clear) {
    return compare_commands(w,r,NULL,0,1,clear);
}
static int test_shapes(nx_window *w, nx_metal_renderer *r) {
    nx_command commands[132] = {0}; size_t n = 0;
    commands[n++] = (nx_command){.type=NX_COMMAND_PUSH_CLIP,.rect={3.5f,4.5f,135,95}};
    commands[n++] = (nx_command){.type=NX_COMMAND_PUSH_CLIP,.rect={8.5f,7.5f,109,83}};
    for (unsigned i = 0; i < 120; ++i) {
        nx_command c = {0};
        c.type = i%5 == 0 ? NX_COMMAND_FILL_RECT : i%5 == 1 ? NX_COMMAND_STROKE_RECT :
            i%5 == 2 ? NX_COMMAND_FILL_ROUND_RECT : i%5 == 3 ? NX_COMMAND_LINE : NX_COMMAND_FILL_TRIANGLE;
        c.rect = (nx_rect){(float)((i*17)%151)-12.5f,(float)((i*11)%109)-10.25f,25.5f,20.25f};
        c.color = (nx_color){(i%17)/16.f,(i%23)/22.f,(i%29)/28.f,1};
        c.radius = 4.5f; c.thickness = 2.5f;
        c.points[0] = (nx_point){c.rect.x,c.rect.y}; c.points[1] = (nx_point){c.rect.x+19,c.rect.y+3};
        c.points[2] = (nx_point){c.rect.x+4,c.rect.y+20};
        if (i%2) { nx_point swap = c.points[1]; c.points[1] = c.points[2]; c.points[2] = swap; }
        commands[n++] = c;
    }
    commands[n++] = (nx_command){.type=NX_COMMAND_POP_CLIP};
    commands[n++] = (nx_command){.type=NX_COMMAND_POP_CLIP};
    int presented = 1;
    CHECK(nx_metal_test_fail_next(r,NX_MTL_TEST_PIPELINE_MEMORY) == NX_OK);
    CHECK(nx_metal_render(r,commands,n,1,(nx_color){0},&presented) == NX_ERROR_MEMORY && !presented);
    const double scales[] = {1,1.25,1.5,2};
    for (unsigned i = 0; i < 4; ++i)
        CHECK(compare_commands(w,r,commands,n,scales[i],(nx_color){.03f,.07f,.11f,.26f}) == 0);
    nx_command distant = {.type=NX_COMMAND_FILL_TRIANGLE,.color={1,1,1,1},
        .points={{-1000000,-1000000.0625f},{1000000,999999.9375f},{-1000000,999999.9375f}}};
    CHECK(compare_commands(w,r,&distant,1,1,(nx_color){0}) == 0);
    nx_command diagonal[] = {
        {.type=NX_COMMAND_FILL_TRIANGLE,.color={0,1,1,1},.points={{40.5f,52.5f},{68.5f,52.5f},{40.5f,80.5f}}},
        {.type=NX_COMMAND_FILL_TRIANGLE,.color={1,0,1,1},.points={{68.5f,80.5f},{40.5f,80.5f},{68.5f,52.5f}}},
        {.type=NX_COMMAND_FILL_TRIANGLE,.color={1,1,1,1},.points={{4,4},{4,4},{4,4}}}
    };
    CHECK(compare_commands(w,r,diagonal,3,1,(nx_color){0}) == 0);
    /* Near-edge triangles must not become dependent on optional float64. */
    for (unsigned i = 0; i < 12; ++i) {
        float offset = (float)i/1000.f;
        nx_command triangle = {.type=NX_COMMAND_FILL_TRIANGLE,.color={1,0,1,1},
            .points={{3.1401f+offset,2.7182f-offset},{152.319f-offset,11.1781f+offset},{35.409f+offset,105.079f-offset}}};
        CHECK(compare_commands(w,r,&triangle,1,scales[i%4],(nx_color){0}) == 0);
    }
    return 0;
}
static int test_alpha(nx_window *w, nx_metal_renderer *r) {
    nx_command commands[41] = {0}; size_t n = 0;
    commands[n++] = (nx_command){.type=NX_COMMAND_PUSH_CLIP,.rect={3.5f,4.5f,135,95}};
    for (unsigned i = 0; i < 36; ++i) {
        nx_command c = {.type=NX_COMMAND_FILL_RECT,.color={(i%7)/6.f,(i%11)/10.f,(i%13)/12.f,(i%9)/8.f}};
        c.rect = (nx_rect){(float)((i*17)%121)-12.5f,(float)((i*11)%89)-10.25f,53.5f,41.25f};
        if (i%3 == 1) { c.type = NX_COMMAND_FILL_ROUND_RECT; c.radius = 7.5f; }
        if (i%3 == 2) {
            c.type = NX_COMMAND_FILL_TRIANGLE;
            c.points[0] = (nx_point){c.rect.x,c.rect.y}; c.points[1] = (nx_point){c.rect.x+51,c.rect.y+7};
            c.points[2] = (nx_point){c.rect.x+8,c.rect.y+43};
        }
        commands[n++] = c;
    }
    commands[n++] = (nx_command){.type=NX_COMMAND_POP_CLIP};
    const double scales[] = {1,1.5,2};
    const nx_color clear[] = {{0},{.03f,.07f,.11f,.26f},{.2f,.3f,.4f,1}};
    for (unsigned i = 0; i < 3; ++i)
        CHECK(compare_commands(w,r,commands,n,scales[i],clear[i]) == 0);
    nx_command low = {.type=NX_COMMAND_FILL_RECT,.rect={.5f,.5f,79,61},.color={.5f,.2f,.7f,1.f/255.f}};
    CHECK(compare_commands(w,r,&low,1,1,(nx_color){1,.4f,.2f,1.f/255.f}) == 0);
    return 0;
}
typedef struct wrong { nx_metal_renderer *renderer; nx_result query, render, destroy; } wrong;
static void *wrong_thread(void *user) {
    wrong *w = user; nx_metal_renderer_info info; int presented;
    w->query = nx_metal_renderer_get_info(w->renderer,&info);
    w->render = nx_metal_render(w->renderer,NULL,0,1,(nx_color){0,0,0,1},&presented);
    w->destroy = nx_metal_renderer_destroy(w->renderer); return NULL;
}
static int test_driver(nx_window *window) {
    nx_renderer_driver driver = {0}; CHECK(nx_metal_driver_open(window,&driver) == NX_OK);
    nx_metal_renderer *r = driver.backend.user;
    nx_window_info info; CHECK(nx_window_get_info(window,&info) == NX_OK);
    nx_render_target target = {(uint32_t)ceil((double)info.width*info.scale),
        (uint32_t)ceil((double)info.height*info.scale),info.scale};
    nx_render_outcome outcome;
    CHECK(nx_renderer_run(&driver,(nx_render_target){0,0,1},NULL,0,(nx_color){0},&outcome) == NX_OK && outcome == NX_RENDER_SUSPENDED);
    CHECK(nx_renderer_run(&driver,(nx_render_target){UINT32_MAX,1,1},NULL,0,(nx_color){0},&outcome) == NX_ERROR_LIMIT);
    CHECK(nx_renderer_run(&driver,target,NULL,0,(nx_color){.1f,.2f,.3f,1},&outcome) == NX_OK && outcome == NX_RENDER_READY);
    CHECK(nx_metal_test_invalidate(r) == NX_OK);
    CHECK(nx_renderer_run(&driver,target,NULL,0,(nx_color){0},&outcome) == NX_OK && outcome == NX_RENDER_DEVICE_LOST);
    CHECK(nx_renderer_recover(&driver) == NX_OK);
    CHECK(nx_renderer_run(&driver,target,NULL,0,(nx_color){0,0,1,1},&outcome) == NX_OK && outcome == NX_RENDER_READY);
    CHECK(nx_metal_test_fail_next(r,NX_MTL_TEST_WAIT_TIMEOUT) == NX_OK);
    CHECK(nx_renderer_close(&driver) == NX_ERROR_LIMIT && driver.open && driver.closing);
    CHECK(nx_renderer_close(&driver) == NX_ERROR_LIMIT);
    CHECK(nx_renderer_close(&driver) == NX_OK && !driver.open);
    return 0;
}
int main(void) {
    @autoreleasepool {
        nx_window *window = NULL;
        nx_window_desc desc = {"NexaUI native Metal test",160,112,0,{0}};
        CHECK(nx_window_create(&desc,&window) == NX_OK);
        NSView *view = nx_window_native_nsview(window);
        CALayer *original_layer = [[view layer] retain];
        BOOL original_wants_layer = [view wantsLayer];
        heap h = {0}; nx_allocator a = {&h,allocate,deallocate}; nx_metal_renderer *r = NULL;
        h.fail = 1; CHECK(nx_metal_renderer_create(window,&a,&r) == NX_ERROR_MEMORY && !r && !h.live); h.fail = 0;
        nx_result result = nx_metal_renderer_create(window,&a,&r);
        if (result == NX_ERROR_UNSUPPORTED && !getenv("NEXAUI_REQUIRE_METAL")) {
            CHECK(!r && !h.live);
            CHECK([view layer] == original_layer && [view wantsLayer] == original_wants_layer);
            [original_layer release]; nx_window_destroy(window);
            puts("No Metal device on this runner; GPU execution skipped (not accepted)."); return 77;
        }
        CHECK(result == NX_OK && r && h.live == 1);
        CHECK([view layer] && [view layer] != original_layer && [view wantsLayer]);
        nx_metal_renderer *competing = NULL;
        CHECK(nx_metal_renderer_create(window,NULL,&competing) == NX_ERROR_STATE && !competing);
        nx_window_destroy(window); nx_window_info wi; CHECK(nx_window_get_info(window,&wi) == NX_OK);
        nx_metal_renderer_info before, after;
        CHECK(nx_metal_renderer_get_info(r,&before) == NX_OK && !before.pending && !before.submitted_frames);
        const uint8_t pixel[] = {255,255,255,255};
        nx_command shape = {.type=NX_COMMAND_BITMAP,.rect={1,2,3,4},.color={1,1,1,1},
            .bitmap={pixel,1,1,4,NX_BITMAP_RGBA8}};
        int presented;
        CHECK(nx_metal_render(r,&shape,1,1,(nx_color){0},&presented) == NX_ERROR_UNSUPPORTED && !presented);
        shape.type = NX_COMMAND_POP_CLIP;
        CHECK(nx_metal_render(r,&shape,1,1,(nx_color){0},&presented) == NX_ERROR_COMMAND);
        CHECK(nx_metal_render(r,NULL,1,1,(nx_color){0},&presented) == NX_ERROR_ARGUMENT);
        CHECK(nx_metal_render(r,NULL,0,NAN,(nx_color){0},&presented) == NX_ERROR_ARGUMENT);
        CHECK(nx_metal_test_fail_next(r,NX_MTL_TEST_CANVAS_MEMORY) == NX_OK);
        CHECK(nx_metal_render(r,NULL,0,1,(nx_color){0},&presented) == NX_ERROR_MEMORY && !presented);
        CHECK(nx_metal_test_fail_next(r,NX_MTL_TEST_COMMAND_MEMORY) == NX_OK);
        CHECK(nx_metal_render(r,NULL,0,1,(nx_color){0},&presented) == NX_ERROR_MEMORY && !presented);
        CHECK(nx_metal_test_fail_next(r,NX_MTL_TEST_DRAWABLE_UNAVAILABLE) == NX_OK);
        CHECK(nx_metal_render(r,NULL,0,1,(nx_color){0},&presented) == NX_OK && !presented);
        CHECK(nx_metal_renderer_get_info(r,&after) == NX_OK && !after.submitted_frames && !after.pending);
        nx_color colors[] = {{0},{1,0,0,1},{0,1,0,.26f},{.1f,.3f,.8f,0},
            {.5f/255.f,82.5f/255.f,190.5f/255.f,92.5f/255.f}};
        for (unsigned i = 0; i < 15; ++i) CHECK(compare(window,r,colors[i%5]) == 0);
        CHECK(test_shapes(window,r) == 0);
        CHECK(test_alpha(window,r) == 0);
        CHECK(nx_metal_renderer_get_info(r,&before) == NX_OK);
        [nx_window_native_nswindow(window) setContentSize:NSMakeSize(281,193)];
        CHECK(nx_window_poll(window,0) == NX_OK);
        CHECK(nx_metal_test_fail_next(r,NX_MTL_TEST_CANVAS_MEMORY) == NX_OK);
        CHECK(nx_metal_render(r,NULL,0,1,(nx_color){0},&presented) == NX_ERROR_MEMORY && !presented);
        CHECK(nx_metal_test_fail_next(r,NX_MTL_TEST_HISTORY_MEMORY) == NX_OK);
        CHECK(nx_metal_render(r,NULL,0,1,(nx_color){0},&presented) == NX_ERROR_MEMORY && !presented);
        CHECK(nx_metal_renderer_get_info(r,&after) == NX_OK && after.pixel_width == before.pixel_width && after.pixel_height == before.pixel_height);
        CHECK(compare(window,r,colors[4]) == 0);
        wrong w = {r,0,0,0}; pthread_t thread;
        CHECK(!pthread_create(&thread,NULL,wrong_thread,&w) && !pthread_join(thread,NULL));
        CHECK(w.query == NX_ERROR_STATE && w.render == NX_ERROR_STATE && w.destroy == NX_ERROR_STATE);
        CHECK(nx_metal_renderer_get_info(r,&after) == NX_OK);
        size_t stride = (size_t)after.pixel_width*4u, bytes = stride*after.pixel_height;
        uint8_t *untouched = malloc(bytes); CHECK(untouched); memset(untouched,0x6b,bytes);
        CHECK(nx_metal_test_fail_next(r,NX_MTL_TEST_WAIT_TIMEOUT) == NX_OK);
        CHECK(nx_metal_test_readback(r,colors[0],untouched,stride) == NX_ERROR_LIMIT);
        for (size_t i = 0; i < bytes; ++i) CHECK(untouched[i] == 0x6b);
        free(untouched);
        CHECK(nx_metal_renderer_get_info(r,&after) == NX_OK && after.pending && after.pending_readback);
        CHECK(nx_metal_render(r,NULL,0,1,colors[0],&presented) == NX_OK && !presented);
        CHECK(compare(window,r,colors[1]) == 0);
        CHECK(nx_metal_renderer_get_info(r,&after) == NX_OK && !after.pending_readback);
        CHECK(nx_metal_test_invalidate(r) == NX_OK);
        CHECK(nx_metal_render(r,NULL,0,1,colors[0],&presented) == NX_ERROR_STATE);
        CHECK(nx_metal_renderer_recover(r) == NX_OK && compare(window,r,colors[2]) == 0);
        nx_command recovered = {.type=NX_COMMAND_FILL_ROUND_RECT,.rect={9.5f,7.5f,91,67},.radius=12,.color={0,1,1,.5f}};
        CHECK(compare_commands(window,r,&recovered,1,1.5,(nx_color){0}) == 0);
        printf("Metal %s: %ux%u; %llu submitted, %llu retired\n",after.device_name,after.pixel_width,after.pixel_height,
            (unsigned long long)after.submitted_frames,(unsigned long long)after.completed_frames);
        CHECK(nx_metal_renderer_destroy(r) == NX_OK && !h.live);
        CHECK([view layer] == original_layer && [view wantsLayer] == original_wants_layer);
        CHECK(test_driver(window) == 0);
        CHECK([view layer] == original_layer && [view wantsLayer] == original_wants_layer);
        [original_layer release];
        nx_window_destroy(window);
        puts("Metal clear/shapes/clips/readback, finite retirement, recovery and driver checks passed.");
    }
    return 0;
}
