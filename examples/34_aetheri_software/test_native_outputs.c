/* Optional observer around the ordinary native host; no alternate UI loop. */
#define main software_ordinary_main
#include "native.c"
#undef main
#include <nexaui/wayland_outputs.h>
static int output_probe_okay=1;
nx_result __real_nx_window_get_info(nx_window *,nx_window_info *);
nx_result __wrap_nx_window_get_info(nx_window *window,nx_window_info *info){
    nx_result r=__real_nx_window_get_info(window,info);if(r!=NX_OK)return r;
    nx_wayland_output_info items[16];size_t count=0;uint64_t revision=0;
    nx_result observed=nx_window_get_wayland_outputs(window,items,16,&count,&revision);
    if(observed==NX_ERROR_STATE){puts("OUTPUTS_PENDING");return r;}
    if(observed!=NX_OK){fprintf(stderr,"OUTPUTS_ERROR status=%d\n",(int)observed);output_probe_okay=0;return observed;}
    printf("OUTPUTS revision=%" PRIu64 " count=%zu items=",revision,count);
    for(size_t i=0;i<count;++i)printf("%s%u:%d:%d:%d",i?",":"",items[i].id,items[i].width,items[i].height,items[i].scale);
    putchar('\n');return r;
}
nx_result __real_nx_window_poll_with_fd(nx_window *,uint32_t,int,uint32_t *);
nx_result __wrap_nx_window_poll_with_fd(nx_window *window,uint32_t timeout,int fd,uint32_t *ready){
    printf("OUTPUT_WAIT_ENTER fd=%d timeout=%u\n",fd,timeout);
    nx_result r=__real_nx_window_poll_with_fd(window,timeout,fd,ready);
    printf("OUTPUT_WAIT_RETURN ready=%u status=%d\n",ready?*ready:0,(int)r);return r;
}
int main(int argc,char **argv){int status=software_ordinary_main(argc,argv);return status?status:output_probe_okay?0:1;}
