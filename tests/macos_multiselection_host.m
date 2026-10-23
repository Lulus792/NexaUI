#include "macos_multiselection_fixture.h"
#include <spawn.h>
#include <sys/wait.h>
#include <signal.h>
#include <unistd.h>
extern char **environ;

static int control(scene *s,const char *command) {
    if(!strcmp(command,"ancestry"))s->kind=2;
    else if(!strcmp(command,"detach"))CHECK(nx_window_publish_semantics(s->window,NULL)==NX_OK);
    else if(!strcmp(command,"context")) {
        nx_context *previous=s->ctx;nx_context_desc d={0};
        d.allocator=(nx_allocator){&s->core,allocate,deallocate};
        CHECK(nx_context_create(&d,&s->ctx)==NX_OK);
        CHECK(publish(s)==0);nx_context_destroy(previous);
    } else if(!strcmp(command,"demote")){s->demoted=1;s->demoted_choice=1000;}
    else if(!strcmp(command,"reset")){s->kind=0;s->demoted=0;}
    else if(!strcmp(command,"verify")) {
        CHECK(selected(s->model,NULL,0,1066)==0);
        CHECK(!s->demoted || s->demoted_choice==1000);
    } else return 1;
    CHECK(publish(s)==0);
    return 0;
}

int main(int argc,char **argv) {
    if(argc!=3)return 2;
    @autoreleasepool {
        scene s={0};CHECK(setup(&s)==0);
        NSWindow *native=nx_window_native_nswindow(s.window);
        int visible=!strcmp(argv[2],"visible");if(visible)[native orderFront:nil];
        CHECK(!![native isVisible]==visible);
        char directory[]="/tmp/nexaui-ax-multi-XXXXXX";CHECK(mkdtemp(directory));
        char request[512],reply[512];snprintf(request,sizeof(request),"%s/request",directory);snprintf(reply,sizeof(reply),"%s/reply",directory);
        char pid[32];snprintf(pid,sizeof(pid),"%ld",(long)getpid());
        char *args[]={argv[1],pid,directory,NULL};pid_t child=0;
        CHECK(posix_spawn(&child,argv[1],NULL,NULL,args,environ)==0);
        double start=[[NSProcessInfo processInfo] systemUptime];int status=0,failed=0;unsigned last=0;
        for(;;) {
            pid_t result=waitpid(child,&status,WNOHANG);if(result==child)break;
            if(result<0){failed=1;break;}
            size_t routed=0;
            if([[NSProcessInfo processInfo] systemUptime]-start>100 || nx_window_poll(s.window,0)!=NX_OK ||
                nx_window_route_events(s.window,s.ctx,&routed)!=NX_OK || publish(&s)) {failed=1;break;}
            FILE *input=fopen(request,"r");unsigned serial=0;char command[32]={0};
            if(input){int fields=fscanf(input,"%u %31s",&serial,command);fclose(input);
                if(fields==2 && serial>last) {
                    if(control(&s,command)){failed=1;break;}
                    FILE *output=fopen(reply,"w");if(!output){failed=1;break;}
                    fprintf(output,"%u\n",serial);fclose(output);last=serial;
                }
            }
            CFRunLoopRunInMode(kCFRunLoopDefaultMode,.002,true);
        }
        if(failed){kill(child,SIGKILL);waitpid(child,&status,0);}
        int code=!failed && WIFEXITED(status)?WEXITSTATUS(status):1;
        if(!code) {
            uint64_t custom=2002;
            if(selected(s.model,NULL,0,1066) || selected(s.table,&custom,1,0))code=1;
            else puts("Independent AX complete65, stale ancestor/detach/context/demotion acknowledged in real models");
        }
        [native orderOut:nil];if(cleanup(&s))code=1;
        unlink(request);unlink(reply);rmdir(directory);
        if(code==78){puts("External AX skipped: hidden hierarchy unavailable; enable NEXAUI_TEST_MACOS_AX_VISIBLE only on an isolated test desktop.");return 77;}
        return code;
    }
}
