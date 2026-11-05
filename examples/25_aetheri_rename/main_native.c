#define _GNU_SOURCE
#include "review.h"
#include <nexaui/software.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
static unsigned rename_calls,flush_calls;static int inject_flush;
int __real_renameat2(int,const char *,int,const char *,unsigned);
int __real_fsync(int);
int __wrap_renameat2(int a,const char *b,int c,const char *d,unsigned f){++rename_calls;return __real_renameat2(a,b,c,d,f);}
int __wrap_fsync(int fd){++flush_calls;int r=__real_fsync(fd);if(!r && inject_flush){errno=EIO;return -1;}return r;}
static unsigned char *read_font(const char *path,size_t *size){FILE *f=fopen(path,"rb");if(!f)return NULL;
    if(fseek(f,0,SEEK_END)){fclose(f);return NULL;}long n=ftell(f);if(n<=0 || n>64000000 || fseek(f,0,SEEK_SET)){fclose(f);return NULL;}
    unsigned char *p=malloc((size_t)n);if(!p){fclose(f);return NULL;}if(fread(p,1,(size_t)n,f)!=(size_t)n){free(p);fclose(f);return NULL;}
    fclose(f);*size=(size_t)n;return p;}
int main(int argc,char **argv){
    int ok=0,done=0,dir=-1,pin=-1;size_t bytes_size=0;unsigned char *bytes=NULL;
    nx_font *font=NULL;nx_font_collection *fonts=NULL;nx_window *window=NULL;nx_software_view *renderer=NULL;
    rename_review *view=NULL;rename_review_owner owner={0};struct ae_files_rename *op=NULL;
    if(argc!=3 || !getenv("NEXAUI_TEST_PRIVATE_WAYLAND") || strcmp(getenv("NEXAUI_TEST_PRIVATE_WAYLAND"),"1"))goto cleanup;
    dir=open(argv[1],O_RDONLY|O_DIRECTORY|O_CLOEXEC);if(dir<0)goto cleanup;
    pin=openat(dir,"old.txt",O_PATH|O_NOFOLLOW|O_CLOEXEC);if(pin<0)goto cleanup;
    enum ae_files_rename_result result;op=ae_files_rename_prepare(dir,pin,"old.txt",7,"new.txt",7,&result);
    if(!op || result!=AE_FILES_RENAME_PREPARED || rename_review_owner_init(&owner,op)!=NX_OK)goto cleanup;
    bytes=read_font(argv[2],&bytes_size);
    if(!bytes || nx_font_open(bytes,bytes_size,NULL,&font)!=NX_OK || nx_font_collection_create(NULL,&fonts)!=NX_OK ||
       nx_font_collection_add(fonts,font)!=NX_OK || rename_review_create(&owner,fonts,&view)!=NX_OK)goto cleanup;
    nx_window_desc desc={"NexaUI Rename experiment",400,300,0,{0}};
    if(nx_window_create(&desc,&window)!=NX_OK || nx_software_view_open(window,&renderer)!=NX_OK)goto cleanup;
    ok=1;inject_flush=1;
    while(ok && !done){
        if(nx_window_poll(window,50)!=NX_OK){ok=0;break;}
        nx_window_info info;if(nx_window_get_info(window,&info)!=NX_OK){ok=0;break;}
        if(info.should_close){done=1;break;}
        nx_context *ctx=rename_review_context(view);nx_event event;int available=0;
        do{if(nx_window_next_event(window,&event,&available)!=NX_OK){ok=0;break;}
            if(available && nx_context_queue_event(ctx,&event)!=NX_OK){ok=0;break;}}while(available);
        size_t routed=0;if(!ok || nx_window_route_events(window,ctx,&routed)!=NX_OK){ok=0;break;}
        rename_review_action action=REVIEW_NONE;
        if(rename_review_frame(view,&action)!=NX_OK){ok=0;break;}
        if(action==REVIEW_CONFIRM){
            if(ae_files_rename_apply(op)!=AE_FILES_RENAME_UNKNOWN || rename_calls!=1 || flush_calls!=1){ok=0;break;}
            if(nx_window_publish_semantics(window,NULL)!=NX_OK){ok=0;break;}
            rename_review_destroy(view);view=NULL;
            if(rename_review_create(&owner,fonts,&view)!=NX_OK){ok=0;break;}
            puts("RENAME_NATIVE_REOPENED_UNKNOWN");fflush(stdout);continue;
        }
        if(action==REVIEW_CANCEL){done=1;break;}
        if(action==REVIEW_BACKGROUND){ok=0;break;}
        nx_result ax=nx_window_publish_semantics(window,ctx);if(ax!=NX_OK && ax!=NX_ERROR_UNSUPPORTED){ok=0;break;}
        const nx_command *commands=NULL;size_t count=0;nx_software_view_outcome outcome;
        if(nx_frame_commands(ctx,&commands,&count)!=NX_OK ||
           nx_software_view_render(renderer,commands,count,(nx_color){0,0,0,1},&outcome)!=NX_OK){ok=0;break;}
    }
cleanup:
    if(window && nx_window_publish_semantics(window,NULL)!=NX_OK)ok=0;
    if(renderer && nx_software_view_close(&renderer)!=NX_OK)ok=0;
    if(view)rename_review_destroy(view);
    if(window)nx_window_destroy(window);
    if(op)ae_files_rename_close(op);
    if(fonts)nx_font_collection_destroy(fonts);
    if(font)nx_font_close(font);
    free(bytes);if(pin>=0)close(pin);if(dir>=0)close(dir);
    if(ok && done){printf("RENAME_NATIVE_CLOSED %u %u\n",rename_calls,flush_calls);fflush(stdout);return 0;}
    fputs("RENAME_NATIVE_FAILED\n",stderr);return 1;
}
