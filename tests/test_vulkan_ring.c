#define _POSIX_C_SOURCE 200809L
#include "renderer.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define CHECK(expr) do { if (!(expr)) { \
    fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #expr); return 1; \
} } while (0)

static double now(void) {
    struct timespec time;
    if (clock_gettime(CLOCK_MONOTONIC,&time)) return -1;
    return (double)time.tv_sec+(double)time.tv_nsec/1.e9;
}
static nx_command rectangle(float x,float y,float width,float height,nx_color color) {
    nx_command result={0}; result.type=NX_COMMAND_FILL_RECT;
    result.rect=(nx_rect){x,y,width,height}; result.color=color; return result;
}
static int queue_image(nx_vulkan_renderer *renderer,unsigned variant,
    const nx_vulkan_renderer_info *info,uint64_t *id,uint8_t *expected,size_t stride) {
    /* Source/command storage does not survive this helper. Both bitmap paths
       vary per frame, with source padding and nontrivial alpha. */
    unsigned width=19u+variant%3u*7u,height=13u+variant%2u*5u;
    size_t source_stride=(size_t)width*4u+11u;
    uint8_t *rgba=malloc(source_stride*height),*mask=malloc(35u*21u);
    CHECK(rgba && mask);
    for (unsigned y=0;y<height;++y) for (unsigned x=0;x<width;++x) {
        uint8_t *pixel=rgba+y*source_stride+x*4u;
        pixel[0]=(uint8_t)(x*17u+variant*29u); pixel[1]=(uint8_t)(y*23u+variant*41u);
        pixel[2]=(uint8_t)(x+y*9u+variant*13u); pixel[3]=(uint8_t)(60u+(x+y)%190u);
    }
    for (unsigned i=0;i<35u*21u;++i) mask[i]=(uint8_t)(i*31u+variant*17u);
    nx_command commands[134]={rectangle(2,3,120,87,(nx_color){.1f,.3f,.6f,1})};
    commands[1].type=NX_COMMAND_PUSH_CLIP; commands[1].rect=(nx_rect){9,7,119,84};
    commands[2].type=NX_COMMAND_BITMAP; commands[2].rect=(nx_rect){5,4,95,62};
    commands[2].color=(nx_color){.8f,.7f,.9f,.8f};
    commands[2].bitmap=(nx_bitmap_view){rgba,width,height,source_stride,NX_BITMAP_RGBA8};
    commands[3]=rectangle(38,23,59,41,(nx_color){.9f,.1f,.5f,.4f});
    commands[4].type=NX_COMMAND_BITMAP; commands[4].rect=(nx_rect){61,39,58,42};
    commands[4].color=(nx_color){.1f,.9f,.4f,.7f};
    commands[4].bitmap=(nx_bitmap_view){mask,35,21,35,NX_BITMAP_A8};
    commands[5].type=NX_COMMAND_POP_CLIP;
    size_t count=6;
    if (variant==1) {
        /* Successful geometry growth after a busy-peer allocation failure. */
        for (unsigned i=6;i<134;++i)
            commands[i]=rectangle((float)(i%113u),(float)(i%79u),7,5,(nx_color){.2f,.7f,.3f,1});
        count=134;
    }
    nx_color clear={(float)(variant%5u)*.1f,.15f,.2f,.6f};
    CHECK(nx_headless_render(commands,count,expected,info->pixel_width,info->pixel_height,stride,clear)==NX_OK);
    nx_result result=nx_vulkan_test_queue_capture(renderer,commands,count,1,clear,id);
    memset(rgba,0xe1,source_stride*height); memset(mask,0x13,35u*21u);
    free(rgba); free(mask); memset(commands,0,sizeof(commands));
    CHECK(result==NX_OK && *id);
    return 0;
}
static int run_ring(nx_window *window,int required) {
    nx_vulkan_renderer *renderer=NULL;
    nx_result created=nx_vulkan_renderer_create(window,NULL,&renderer);
    if (created==NX_ERROR_UNSUPPORTED && !required) {
        puts("Vulkan timeline gate unavailable; gated ring case skipped.");
        return 77;
    }
    CHECK(created==NX_OK);
    nx_command command=rectangle(2,3,31,25,(nx_color){1,0,0,1});
    int presented=0;
    for (unsigned i=0;i<10 && !presented;++i) {
        CHECK(nx_window_poll(window,10)==NX_OK);
        CHECK(nx_vulkan_render(renderer,&command,1,1,(nx_color){0},&presented)==NX_OK);
    }
    CHECK(presented);
    nx_vulkan_renderer_info before,after;
    CHECK(nx_vulkan_renderer_get_info(renderer,&before)==NX_OK);
    CHECK(before.frame_slots==3 && before.pixel_width && before.pixel_height);
    size_t stride=(size_t)before.pixel_width*4u+17u,bytes=stride*before.pixel_height;
    uint8_t *actual=malloc(bytes),*expected[3];
    CHECK(actual);
    for (unsigned i=0;i<3;++i) { expected[i]=malloc(bytes); CHECK(expected[i]); }
    CHECK(nx_vulkan_test_render_readback(renderer,&command,1,1,(nx_color){0},actual,stride)==NX_OK);
    CHECK(nx_vulkan_test_set_frame_slots(renderer,2)==NX_ERROR_ARGUMENT);
    /* Lavapipe can synchronize vkCreateImageView with prior queue work. Warm
       all three atlas capacities before holding that queue with a timeline. */
    for (unsigned i=0;i<3;++i) {
        uint64_t id;
        CHECK(queue_image(renderer,0,&before,&id,expected[i],stride)==0);
        CHECK(nx_vulkan_test_collect_capture(renderer,id,actual,stride)==NX_OK);
    }
    for (unsigned round=0;round<6;++round) {
        CHECK(nx_vulkan_test_gate_begin(renderer)==NX_OK);
        CHECK(nx_vulkan_renderer_get_info(renderer,&before)==NX_OK);
        uint64_t ids[3];
        for (unsigned i=0;i<3;++i) {
            memset(expected[i],0xa7,bytes);
            if (!round && i==1) {
                /* A failed allocation with a busy peer must preserve its slot,
                   committed profile and retained capture. */
                CHECK(nx_vulkan_test_fail_next(renderer,NX_VK_TEST_VERTEX_MEMORY)==NX_OK);
                nx_command growth[128];
                for (unsigned j=0;j<128;++j) growth[j]=command;
                uint64_t failed=99;
                CHECK(nx_vulkan_test_queue_capture(renderer,growth,128,1,(nx_color){0},&failed)==NX_ERROR_MEMORY);
                CHECK(!failed);
                CHECK(nx_vulkan_renderer_get_info(renderer,&after)==NX_OK);
                CHECK(after.submitted_frames==before.submitted_frames+1 && after.pending_submissions==1 && after.pending_captures==1);
            }
            CHECK(queue_image(renderer,round*3u+i,&before,&ids[i],expected[i],stride)==0);
            CHECK(nx_vulkan_renderer_get_info(renderer,&after)==NX_OK);
            CHECK(after.last_frame.frame_slot<3 && after.last_frame.frames_in_flight==3);
            CHECK(after.submitted_frames==before.submitted_frames+i+1 && after.pending_submissions==i+1);
            CHECK(after.pending_captures==i+1 && after.retired_frames==before.retired_frames);
            nx_vulkan_gpu_timing timing;
            CHECK(nx_vulkan_renderer_get_gpu_timing(renderer,ids[i],&timing)==NX_OK && timing.state==NX_VK_TIMING_PENDING);
        }
        CHECK(nx_vulkan_test_gate_begin(renderer)==NX_ERROR_STATE);
        if (!round) {
            CHECK(nx_vulkan_renderer_wait_gpu(renderer)==NX_ERROR_LIMIT);
            CHECK(nx_vulkan_test_set_frame_slots(renderer,1)==NX_ERROR_LIMIT);
            memset(actual,0xa7,bytes);
            double start=now();
            CHECK(nx_vulkan_test_collect_capture(renderer,ids[0],actual,stride)==NX_ERROR_LIMIT);
            CHECK(now()-start>=.8 && now()-start<5.0);
            for (size_t i=0;i<bytes;++i) CHECK(actual[i]==0xa7);
            uint64_t failed=99;
            CHECK(nx_vulkan_test_queue_capture(renderer,&command,1,1,(nx_color){0},&failed)==NX_ERROR_LIMIT && !failed);
            CHECK(nx_vulkan_renderer_get_info(renderer,&after)==NX_OK);
            CHECK(after.pending_submissions==3 && after.pending_captures==3 &&
                after.submitted_frames==before.submitted_frames+3 && after.retired_frames==before.retired_frames);
        }
        CHECK(nx_vulkan_test_gate_signal(renderer)==NX_OK);
        /* Reverse collection checks submission identity independent of next slot.
           GPU copies precede subsequent shared-canvas clears. */
        for (unsigned i=3;i-- >0;) {
            memset(actual,0xa7,bytes);
            CHECK(nx_vulkan_test_collect_capture(renderer,ids[i],actual,stride)==NX_OK);
            CHECK(memcmp(actual,expected[i],bytes)==0);
            nx_vulkan_gpu_timing timing;
            CHECK(nx_vulkan_renderer_get_gpu_timing(renderer,ids[i],&timing)==NX_OK && timing.includes_readback);
            CHECK(timing.state==(after.gpu_timing_supported ? NX_VK_TIMING_READY : NX_VK_TIMING_UNAVAILABLE));
            CHECK(isfinite(timing.gpu_ms) && timing.gpu_ms>=0);
            CHECK(nx_vulkan_test_collect_capture(renderer,ids[i],actual,stride)==NX_ERROR_STATE);
        }
        CHECK(nx_vulkan_renderer_get_info(renderer,&after)==NX_OK);
        CHECK(!after.pending_submissions && !after.pending_captures && after.retired_frames==after.submitted_frames);
    }
    CHECK(nx_vulkan_renderer_get_info(renderer,&before)==NX_OK);
    uint64_t allocations=before.vertex_buffer_allocations,uploads=before.bitmap_upload_allocations,atlases=before.atlas_allocations;
    for (unsigned i=0;i<9;++i)
        CHECK(nx_vulkan_render(renderer,&command,1,1,(nx_color){0},&presented)==NX_OK && presented);
    CHECK(nx_vulkan_renderer_get_info(renderer,&after)==NX_OK);
    CHECK(after.vertex_buffer_allocations==allocations && after.bitmap_upload_allocations==uploads && after.atlas_allocations==atlases);
    CHECK(nx_vulkan_test_set_frame_slots(renderer,1)==NX_OK);
    CHECK(nx_vulkan_test_set_frame_slots(renderer,3)==NX_OK);
    /* Dirty swapchain, recovery and close retain three busy slots on a real
       timeout, each with one aggregate budget instead of one wait per slot. */
    for (unsigned phase=0;phase<3;++phase) {
        CHECK(nx_vulkan_renderer_get_info(renderer,&before)==NX_OK);
        for (unsigned i=0;i<3;++i) {
            uint64_t id;
            CHECK(queue_image(renderer,0,&before,&id,expected[i],stride)==0);
            CHECK(nx_vulkan_test_collect_capture(renderer,id,actual,stride)==NX_OK);
        }
        CHECK(nx_vulkan_test_gate_begin(renderer)==NX_OK);
        CHECK(nx_vulkan_renderer_get_info(renderer,&before)==NX_OK);
        uint64_t ids[3];
        for (unsigned i=0;i<3;++i) {
            memset(expected[i],0xa7,bytes);
            CHECK(queue_image(renderer,phase*3u+i,&before,&ids[i],expected[i],stride)==0);
        }
        double start=now();
        if (!phase) {
            CHECK(nx_vulkan_test_invalidate(renderer,0)==NX_OK);
            CHECK(nx_vulkan_render(renderer,&command,1,1,(nx_color){0},&presented)==NX_OK && !presented);
        } else if (phase==1) CHECK(nx_vulkan_renderer_recover(renderer)==NX_ERROR_LIMIT);
        else CHECK(nx_vulkan_renderer_destroy(renderer)==NX_ERROR_LIMIT);
        CHECK(now()-start>=.8 && now()-start<5.0);
        CHECK(nx_vulkan_renderer_get_info(renderer,&after)==NX_OK);
        CHECK(after.pending_submissions==3 && after.pending_captures==3 &&
            after.swapchain_generation==before.swapchain_generation && after.retired_frames==before.retired_frames);
        CHECK(nx_vulkan_test_gate_signal(renderer)==NX_OK);
        if (phase==2) break;
        for (unsigned i=0;i<3;++i) {
            memset(actual,0xa7,bytes);
            CHECK(nx_vulkan_test_collect_capture(renderer,ids[i],actual,stride)==NX_OK);
            CHECK(memcmp(actual,expected[i],bytes)==0);
        }
        if (!phase) {
            CHECK(nx_vulkan_render(renderer,&command,1,1,(nx_color){0},&presented)==NX_OK && presented);
            CHECK(nx_vulkan_test_set_frame_slots(renderer,3)==NX_OK);
        } else {
            CHECK(nx_vulkan_renderer_recover(renderer)==NX_OK);
            CHECK(nx_vulkan_render(renderer,&command,1,1,(nx_color){0},&presented)==NX_OK && presented);
            CHECK(nx_vulkan_test_set_frame_slots(renderer,3)==NX_OK);
        }
    }
    CHECK(nx_vulkan_renderer_destroy(renderer)==NX_OK);
    for (unsigned i=0;i<3;++i) free(expected[i]);
    free(actual);
    CHECK(nx_vulkan_renderer_create(window,NULL,&renderer)==NX_OK);
    CHECK(nx_vulkan_renderer_destroy(renderer)==NX_OK);
    puts("Vulkan ring: three timeline-gated GPU submissions, freed RGBA8/A8 sources, 24 full-frame comparisons, real bounded reuse/resize/recovery/close timeouts passed.");
    return 0;
}
int main(void) {
    const char *backend=getenv("NEXAUI_PLATFORM");
    if (!backend) return 1;
    int required=getenv("NEXAUI_REQUIRE_VULKAN") &&
        (!strcmp(backend,"x11") || getenv("NEXAUI_REQUIRE_VULKAN_WAYLAND"));
    nx_window *window=NULL;
    nx_window_desc desc={"NexaUI Vulkan frame ring",160,112,0,{0}};
    nx_result result=nx_window_create(&desc,&window);
    if (result==NX_ERROR_PLATFORM && !required) return 77;
    CHECK(result==NX_OK);
    int status=run_ring(window,required);
    nx_window_destroy(window);
    return status;
}
