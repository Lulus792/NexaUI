#include "renderer.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr,"%s:%d: %s\n",__FILE__,__LINE__,#x); return 1; } } while (0)
static nx_command shape(void) {
    nx_command result={0}; result.type=NX_COMMAND_FILL_RECT;
    result.rect=(nx_rect){5,7,83,61}; result.color=(nx_color){.1f,.8f,.5f,.6f};
    return result;
}
static int submit(nx_window *window,nx_vulkan_renderer *renderer,const nx_command *command) {
    int presented=0;
    for (unsigned i=0;i<10 && !presented;++i) {
        CHECK(nx_window_poll(window,10)==NX_OK);
        CHECK(nx_vulkan_render(renderer,command,1,1,(nx_color){.1f,.2f,.3f,1},&presented)==NX_OK);
    }
    CHECK(presented); return 0;
}
static int valid(const nx_vulkan_gpu_timing *value,int supported) {
    CHECK(value->state==(supported ? NX_VK_TIMING_READY : NX_VK_TIMING_UNAVAILABLE));
    CHECK(isfinite(value->gpu_ms) && value->gpu_ms>=0);
    if (supported) CHECK(value->timestamp_valid_bits>=36 && value->timestamp_valid_bits<=64 &&
        isfinite(value->timestamp_period_ns) && value->timestamp_period_ns>0);
    return 0;
}
static int run(nx_window *window) {
    nx_vulkan_renderer *renderer=NULL; nx_vulkan_renderer_info info,before;
    nx_vulkan_gpu_timing value,again;
    CHECK(nx_vulkan_renderer_create(window,NULL,&renderer)==NX_OK);
    CHECK(nx_vulkan_renderer_get_info(renderer,&info)==NX_OK);
    int supported=info.gpu_timing_supported;
    if (getenv("NEXAUI_REQUIRE_VULKAN_TIMING")) CHECK(supported);
    nx_command command=shape();
    CHECK(nx_vulkan_renderer_get_gpu_timing(NULL,1,&value)==NX_ERROR_ARGUMENT && !value.submission);
    CHECK(nx_vulkan_renderer_get_gpu_timing(renderer,0,&value)==NX_ERROR_ARGUMENT);
    CHECK(nx_vulkan_renderer_get_gpu_timing(renderer,1,&value)==NX_ERROR_STATE);
    for (unsigned i=0;i<67;++i) {
        command.rect.x=(float)(i%37u);
        CHECK(submit(window,renderer,&command)==0);
        CHECK(nx_vulkan_renderer_get_info(renderer,&info)==NX_OK);
        CHECK(nx_vulkan_renderer_get_gpu_timing(renderer,info.submitted_frames,&value)==NX_OK);
        CHECK(value.state==NX_VK_TIMING_PENDING && value.frame_slot==info.last_frame.frame_slot);
        CHECK(nx_vulkan_renderer_get_gpu_timing(renderer,info.submitted_frames,&again)==NX_OK);
        CHECK(!memcmp(&value,&again,sizeof(value))); /* Cached lookups never collect. */
    }
    before=info;
    CHECK(nx_vulkan_test_fail_next(renderer,NX_VK_TEST_SUBMISSION_WAIT_TIMEOUT)==NX_OK);
    for (unsigned i=0;i<2;++i) {
        CHECK(nx_vulkan_renderer_wait_gpu(renderer)==NX_ERROR_LIMIT);
        CHECK(nx_vulkan_renderer_get_gpu_timing(renderer,before.submitted_frames,&value)==NX_OK && value.state==NX_VK_TIMING_PENDING);
        CHECK(nx_vulkan_renderer_get_info(renderer,&info)==NX_OK && info.retired_frames==before.retired_frames);
    }
    CHECK(nx_vulkan_renderer_wait_gpu(renderer)==NX_OK);
    CHECK(nx_vulkan_renderer_get_info(renderer,&info)==NX_OK && !info.pending_submissions);
    CHECK(info.last_completed_gpu.submission==info.submitted_frames && valid(&info.last_completed_gpu,supported)==0);
    CHECK(nx_vulkan_renderer_get_gpu_timing(renderer,1,&value)==NX_ERROR_STATE && !value.submission);
    for (uint64_t id=info.submitted_frames-63;id<=info.submitted_frames;++id) {
        CHECK(nx_vulkan_renderer_get_gpu_timing(renderer,id,&value)==NX_OK && value.submission==id);
        CHECK(valid(&value,supported)==0 && !value.includes_readback);
        CHECK(value.pixel_width==info.pixel_width && value.pixel_height==info.pixel_height);
    }
    printf("Vulkan GPU timing journal: 64/64 retained submissions checked, supported=%d bits=%u period=%.9f ns.\n",
        supported,info.timestamp_valid_bits,info.timestamp_period_ns);
    const nx_vulkan_test_fault faults[]={NX_VK_TEST_TIMING_UNAVAILABLE,NX_VK_TEST_TIMING_FAILED};
    for (unsigned i=0;i<2;++i) {
        CHECK(submit(window,renderer,&command)==0);
        CHECK(nx_vulkan_test_fail_next(renderer,faults[i])==NX_OK);
        CHECK(nx_vulkan_renderer_wait_gpu(renderer)==NX_OK);
        CHECK(nx_vulkan_renderer_get_info(renderer,&info)==NX_OK);
        CHECK(info.last_completed_gpu.state==(i ? NX_VK_TIMING_FAILED : NX_VK_TIMING_UNAVAILABLE));
        CHECK(!info.last_completed_gpu.gpu_ms);
    }
    CHECK(nx_vulkan_test_set_gpu_timing(renderer,0)==NX_OK);
    CHECK(submit(window,renderer,&command)==0);
    CHECK(nx_vulkan_test_set_gpu_timing(renderer,1)==NX_OK);
    CHECK(nx_vulkan_renderer_wait_gpu(renderer)==NX_OK);
    CHECK(nx_vulkan_renderer_get_info(renderer,&info)==NX_OK && info.last_completed_gpu.state==NX_VK_TIMING_UNAVAILABLE);
    CHECK(submit(window,renderer,&command)==0 && nx_vulkan_renderer_wait_gpu(renderer)==NX_OK);
    CHECK(nx_vulkan_renderer_get_info(renderer,&info)==NX_OK && valid(&info.last_completed_gpu,supported)==0);
    CHECK(submit(window,renderer,&command)==0);
    CHECK(nx_vulkan_renderer_get_info(renderer,&before)==NX_OK);
    CHECK(nx_vulkan_test_fail_next(renderer,NX_VK_TEST_RETIRE_TIMEOUT)==NX_OK);
    CHECK(nx_vulkan_renderer_recover(renderer)==NX_ERROR_LIMIT);
    CHECK(nx_vulkan_renderer_get_gpu_timing(renderer,before.submitted_frames,&value)==NX_OK && value.state==NX_VK_TIMING_PENDING);
    CHECK(nx_vulkan_renderer_recover(renderer)==NX_OK);
    CHECK(nx_vulkan_renderer_get_gpu_timing(renderer,before.submitted_frames,&value)==NX_ERROR_STATE);
    CHECK(submit(window,renderer,&command)==0 && nx_vulkan_renderer_wait_gpu(renderer)==NX_OK);
    CHECK(nx_vulkan_renderer_get_info(renderer,&info)==NX_OK && info.submitted_frames==before.submitted_frames+1);
    CHECK(valid(&info.last_completed_gpu,supported)==0);
    CHECK(nx_vulkan_test_fail_next(renderer,NX_VK_TEST_PRESENT_OOM)==NX_OK);
    int presented;
    CHECK(nx_vulkan_render(renderer,&command,1,1,(nx_color){0},&presented)==NX_ERROR_MEMORY && !presented);
    CHECK(nx_vulkan_renderer_get_info(renderer,&before)==NX_OK);
    CHECK(nx_vulkan_renderer_get_gpu_timing(renderer,before.submitted_frames,&value)==NX_OK && value.state==NX_VK_TIMING_PENDING);
    CHECK(nx_vulkan_renderer_wait_gpu(renderer)==NX_OK);
    CHECK(nx_vulkan_renderer_get_gpu_timing(renderer,before.submitted_frames,&value)==NX_OK && valid(&value,supported)==0);
    CHECK(nx_vulkan_renderer_recover(renderer)==NX_OK);
    CHECK(submit(window,renderer,&command)==0);
    CHECK(nx_vulkan_renderer_get_info(renderer,&info)==NX_OK);
    size_t stride=(size_t)info.pixel_width*4u+17u;
    uint8_t *pixels=malloc(stride*info.pixel_height); CHECK(pixels);
    CHECK(nx_vulkan_test_render_readback(renderer,&command,1,1,(nx_color){0},pixels,stride)==NX_OK);
    free(pixels);
    CHECK(nx_vulkan_renderer_get_info(renderer,&info)==NX_OK && info.last_completed_gpu.includes_readback);
    CHECK(valid(&info.last_completed_gpu,supported)==0);
    if (supported) {
        /* Rebuilding the third pool fails after two real pools/buffers/fences
           exist. Retry must clean those partial resources and retain the lease. */
        before=info;
        CHECK(nx_vulkan_test_fail_next(renderer,NX_VK_TEST_QUERY_POOL_MEMORY)==NX_OK);
        CHECK(nx_vulkan_renderer_recover(renderer)==NX_ERROR_MEMORY);
        CHECK(nx_vulkan_renderer_get_gpu_timing(renderer,before.submitted_frames,&value)==NX_ERROR_STATE);
        CHECK(nx_vulkan_render(renderer,&command,1,1,(nx_color){0},&presented)==NX_ERROR_STATE);
        CHECK(nx_vulkan_renderer_recover(renderer)==NX_OK);
        CHECK(submit(window,renderer,&command)==0 && nx_vulkan_renderer_wait_gpu(renderer)==NX_OK);
        CHECK(nx_vulkan_renderer_get_info(renderer,&info)==NX_OK && info.submitted_frames==before.submitted_frames+1);
        CHECK(valid(&info.last_completed_gpu,1)==0);
    }
    CHECK(nx_vulkan_renderer_destroy(renderer)==NX_OK);
    puts("Vulkan GPU timing: completed queries, cached pending IDs, timeout, unavailable/failed, disabled sampling, recovery and presentation-failure distinction passed.");
    return 0;
}
int main(void) {
    const char *backend=getenv("NEXAUI_PLATFORM"); if (!backend) return 1;
    int required=getenv("NEXAUI_REQUIRE_VULKAN") &&
        (!strcmp(backend,"x11") || getenv("NEXAUI_REQUIRE_VULKAN_WAYLAND"));
    nx_window *window=NULL;
    nx_window_desc desc={"NexaUI Vulkan GPU timing",160,112,0,{0}};
    nx_result result=nx_window_create(&desc,&window);
    if (result==NX_ERROR_PLATFORM && !required) return 77;
    CHECK(result==NX_OK);
    int status=run(window); nx_window_destroy(window); return status;
}
