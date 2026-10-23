#define main included_native_fixture_main
#include "test_macos_multiselection.m"
#undef main
#include <spawn.h>
#include <sys/wait.h>
#include <signal.h>
#include <unistd.h>
extern char **environ;
int main(int argc,char **argv) {
    if(argc!=3)return 2;
    @autoreleasepool {
        scene s={0};CHECK(setup(&s)==0);
        NSWindow *native=nx_window_native_nswindow(s.window);
        int visible=!strcmp(argv[2],"visible");
        if(visible)[native orderFront:nil];
        CHECK(!![native isVisible]==visible);
        char pid[32];snprintf(pid,sizeof(pid),"%ld",(long)getpid());
        char *args[]={argv[1],pid,NULL};pid_t child=0;
        CHECK(posix_spawn(&child,argv[1],NULL,NULL,args,environ)==0);
        double start=[[NSProcessInfo processInfo] systemUptime];int status=0,failed=0;
        for(;;) {
            pid_t result=waitpid(child,&status,WNOHANG);if(result==child)break;
            if(result<0){failed=1;break;}
            size_t routed=0;
            if([[NSProcessInfo processInfo] systemUptime]-start>60 || nx_window_poll(s.window,0)!=NX_OK ||
                nx_window_route_events(s.window,s.ctx,&routed)!=NX_OK || publish(&s)) {
                kill(child,SIGKILL);waitpid(child,&status,0);failed=1;break;
            }
            CFRunLoopRunInMode(kCFRunLoopDefaultMode,.002,true);
        }
        CHECK(!failed && WIFEXITED(status));int code=WEXITSTATUS(status);
        if(!code) {
            uint64_t custom=2002;
            CHECK(selected(s.model,NULL,0,1066)==0 && selected(s.table,&custom,1,0)==0);
            puts("Independent AX client changes acknowledged in real application models");
        }
        [native orderOut:nil];CHECK(cleanup(&s)==0);
        return code;
    }
}
