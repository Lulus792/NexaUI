#define _GNU_SOURCE
#include "review.h"
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"rename-review:%d: %s\n",__LINE__,#x);goto cleanup;}}while(0)
static unsigned rename_calls,flush_calls;static int lose_flush;
int __real_renameat2(int,const char *,int,const char *,unsigned);
int __real_fsync(int);
int __wrap_renameat2(int a,const char *b,int c,const char *d,unsigned f){++rename_calls;return __real_renameat2(a,b,c,d,f);}
int __wrap_fsync(int fd){++flush_calls;int r=__real_fsync(fd);if(!r && lose_flush){errno=EIO;return -1;}return r;}
static unsigned char *read_font(const char *path,size_t *size){FILE *f=fopen(path,"rb");if(!f)return NULL;
    if(fseek(f,0,SEEK_END)){fclose(f);return NULL;}long n=ftell(f);if(n<=0 || n>64000000 || fseek(f,0,SEEK_SET)){fclose(f);return NULL;}
    unsigned char *p=malloc((size_t)n);if(!p){fclose(f);return NULL;}if(fread(p,1,(size_t)n,f)!=(size_t)n){free(p);fclose(f);return NULL;}
    fclose(f);*size=(size_t)n;return p;}
static int value(nx_context *ctx,uint64_t id,const char *text,int disabled){uint32_t i;nx_semantic_view v;
    return nx_semantics_find(ctx,id,&i)==NX_OK && nx_semantics_get(ctx,i,&v)==NX_OK &&
        (!text || (v.value.length==strlen(text) && !memcmp(v.value.data,text,v.value.length))) &&
        (disabled<0 || !!(v.state&NX_SEM_DISABLED)==disabled);}
static int capture(nx_context *ctx,const char *path){
    if(!path)return 1;
    const nx_command *commands=NULL;size_t count=0;unsigned char pixels[400*300*4];
    if(nx_frame_commands(ctx,&commands,&count)!=NX_OK || nx_headless_render(commands,count,pixels,400,300,400*4,(nx_color){0,0,0,1})!=NX_OK)return 0;
    FILE *f=fopen(path,"wb");if(!f)return 0;int ok=fprintf(f,"P6\n400 300\n255\n")>0;
    for(size_t i=0;ok && i<400u*300u;++i)ok=fwrite(pixels+i*4u,1,3,f)==3;
    if(fclose(f))ok=0;
    return ok;
}
int main(int argc,char **argv){int ok=0,dir=-1,pin=-1;char path[]="/tmp/nexaui-rename-review-XXXXXX";
    unsigned char *font_data=NULL;size_t font_size=0;nx_font *font=NULL;nx_font_collection *fonts=NULL;
    struct ae_files_rename *op=NULL;rename_review *view=NULL;rename_review_action action=REVIEW_NONE;
    CHECK(argc==2 && getuid()!=0 && mkdtemp(path));dir=open(path,O_RDONLY|O_DIRECTORY|O_CLOEXEC);CHECK(dir>=0);
    int fd=openat(dir,"old.txt",O_CREAT|O_EXCL|O_WRONLY|O_CLOEXEC,0600);CHECK(fd>=0 && write(fd,"data",4)==4 && !close(fd));
    pin=openat(dir,"old.txt",O_PATH|O_NOFOLLOW|O_CLOEXEC);CHECK(pin>=0);
    enum ae_files_rename_result result;op=ae_files_rename_prepare(dir,pin,"old.txt",7,"new.txt",7,&result);
    CHECK(op && result==AE_FILES_RENAME_PREPARED && !rename_calls && !flush_calls);
    CHECK((font_data=read_font(argv[1],&font_size)) && nx_font_open(font_data,font_size,NULL,&font)==NX_OK);
    CHECK(nx_font_collection_create(NULL,&fonts)==NX_OK && nx_font_collection_add(fonts,font)==NX_OK);
    CHECK(rename_review_create(op,fonts,&view)==NX_OK && rename_review_frame(view,&action)==NX_OK && action==REVIEW_NONE);
    nx_context *ctx=rename_review_context(view);
    CHECK(value(ctx,3,"old.txt",-1) && value(ctx,4,"new.txt",-1) && value(ctx,5,"Ready for confirmation",-1) && value(ctx,11,NULL,0));
    CHECK(capture(ctx,getenv("NEXAUI_RENAME_CAPTURE")));
    CHECK(nx_focus_set(ctx,10)==NX_OK && rename_review_frame(view,&action)==NX_OK);
    nx_event event={0};event.type=NX_EVENT_KEY_DOWN;event.key=NX_KEY_ENTER;CHECK(nx_context_queue_event(ctx,&event)==NX_OK);
    event.type=NX_EVENT_KEY_UP;CHECK(nx_context_queue_event(ctx,&event)==NX_OK);
    CHECK(rename_review_frame(view,&action)==NX_OK && action==REVIEW_CANCEL && !rename_calls && !flush_calls);
    rename_review_destroy(view);view=NULL;CHECK(rename_review_create(op,fonts,&view)==NX_OK);
    CHECK(rename_review_frame(view,&action)==NX_OK && action==REVIEW_NONE);ctx=rename_review_context(view);
    CHECK(nx_focus_set(ctx,11)==NX_OK && rename_review_frame(view,&action)==NX_OK && action==REVIEW_NONE);
    event=(nx_event){0};event.type=NX_EVENT_KEY_DOWN;event.key=NX_KEY_ENTER;CHECK(nx_context_queue_event(ctx,&event)==NX_OK);
    event.type=NX_EVENT_KEY_UP;CHECK(nx_context_queue_event(ctx,&event)==NX_OK);
    CHECK(rename_review_frame(view,&action)==NX_OK && action==REVIEW_CONFIRM && !rename_calls && !flush_calls);
    lose_flush=1;CHECK(ae_files_rename_apply(op)==AE_FILES_RENAME_UNKNOWN && rename_calls==1 && flush_calls==1);lose_flush=0;
    struct stat st;CHECK(fstatat(dir,"old.txt",&st,AT_SYMLINK_NOFOLLOW)<0 && errno==ENOENT);
    CHECK(!fstatat(dir,"new.txt",&st,AT_SYMLINK_NOFOLLOW));
    rename_review_destroy(view);view=NULL;CHECK(rename_review_create(op,fonts,&view)==NX_OK);
    CHECK(rename_review_frame(view,&action)==NX_OK && action==REVIEW_NONE);
    ctx=rename_review_context(view);CHECK(value(ctx,5,"Result unknown; check the original operation",-1) && value(ctx,11,NULL,1));
    CHECK(ae_files_rename_apply(op)==AE_FILES_RENAME_UNKNOWN && rename_calls==1 && flush_calls==1);
    CHECK(nx_focus_set(ctx,11)==NX_ERROR_STATE || !nx_focus_id(ctx));
    rename_review_destroy(view);view=NULL;ae_files_rename_close(op);op=NULL;close(pin);pin=-1;
    fd=openat(dir,"old.txt",O_CREAT|O_EXCL|O_WRONLY|O_CLOEXEC,0600);CHECK(fd>=0 && !close(fd));
    pin=openat(dir,"new.txt",O_PATH|O_NOFOLLOW|O_CLOEXEC);CHECK(pin>=0);
    op=ae_files_rename_prepare(dir,pin,"new.txt",7,"old.txt",7,&result);CHECK(op && result==AE_FILES_RENAME_PREPARED);
    CHECK(ae_files_rename_apply(op)==AE_FILES_RENAME_CONFLICT && rename_calls==1 && flush_calls==1);
    CHECK(rename_review_create(op,fonts,&view)==NX_OK && rename_review_frame(view,&action)==NX_OK && action==REVIEW_NONE);
    ctx=rename_review_context(view);CHECK(value(ctx,5,"Destination already exists",-1) && value(ctx,11,NULL,1));
    rename_review_destroy(view);view=NULL;ae_files_rename_close(op);op=NULL;close(pin);pin=-1;
    fd=openat(dir,"bad\nname",O_CREAT|O_EXCL|O_WRONLY|O_CLOEXEC,0600);CHECK(fd>=0 && !close(fd));
    pin=openat(dir,"bad\nname",O_PATH|O_NOFOLLOW|O_CLOEXEC);CHECK(pin>=0);
    op=ae_files_rename_prepare(dir,pin,"bad\nname",8,"clean",5,&result);CHECK(op && result==AE_FILES_RENAME_PREPARED);
    CHECK(rename_review_create(op,fonts,&view)==NX_OK && rename_review_frame(view,&action)==NX_OK && action==REVIEW_NONE);
    CHECK(value(rename_review_context(view),3,"bad\\x0Aname",-1));
    rename_review_destroy(view);view=NULL;ae_files_rename_close(op);op=NULL;
    char long_name[256];memset(long_name,'x',255);long_name[255]=0;
    op=ae_files_rename_prepare(dir,pin,"bad\nname",8,long_name,255,&result);CHECK(op && result==AE_FILES_RENAME_PREPARED);
    CHECK(rename_review_create(op,fonts,&view)==NX_OK && rename_review_frame(view,&action)==NX_ERROR_LIMIT && action==REVIEW_NONE);
    CHECK(rename_calls==1 && flush_calls==1);
    ok=1;
cleanup:
    if(view)rename_review_destroy(view);
    if(op)ae_files_rename_close(op);
    if(fonts)nx_font_collection_destroy(fonts);
    if(font)nx_font_close(font);
    free(font_data);if(pin>=0)close(pin);
    if(dir>=0){(void)unlinkat(dir,"old.txt",0);(void)unlinkat(dir,"new.txt",0);(void)unlinkat(dir,"bad\nname",0);close(dir);(void)rmdir(path);}
    if(ok)puts("Original Aetheri rename: explicit confirmation, unknown outcome and no replay passed");
    return ok?0:1;
}
