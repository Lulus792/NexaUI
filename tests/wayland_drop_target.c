#include "nexaui/nexaui.h"
#include <stdio.h>
#include <string.h>

int main(void) {
    nx_window_desc desc={"NexaUI independent drag target",320,220,0,{0}};
    nx_window *window=NULL;
    if(nx_window_create(&desc,&window)!=NX_OK)return 1;
    puts("TARGET_READY");fflush(stdout);
    int success=0;
    while(!success) {
        if(nx_window_poll(window,250)!=NX_OK)break;
        nx_event event;int available=0;
        do {
            if(nx_window_next_event(window,&event,&available)!=NX_OK)goto done;
            if(available && event.type==NX_EVENT_POINTER_MOVE) {
                printf("TARGET_POINTER %.0f %.0f\n",event.x,event.y);fflush(stdout);
            }
        } while(available);
        char path[128];size_t length=0;nx_file_drop_info info={0};
        if(nx_window_next_file_drop(window,path,sizeof(path),&length,&info,&available)!=NX_OK)break;
        if(available) {
            printf("TARGET_DROP %s %.0f %.0f\n",path,info.x,info.y);fflush(stdout);
            success=!strcmp(path,"/tmp/nexaui drop.txt") && info.count==1 && info.index==0 &&
                    info.x>=0 && info.y>=0 && info.x<320 && info.y<220;
            break;
        }
    }
done:
    nx_window_destroy(window);return success?0:2;
}
