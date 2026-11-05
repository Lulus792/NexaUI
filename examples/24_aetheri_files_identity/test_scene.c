#define _POSIX_C_SOURCE 200809L
#include "scene.h"
#include <fcntl.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define CHECK(x) do {if(!(x)){fprintf(stderr,"files-scene:%d: %s\n",__LINE__,#x);goto cleanup;}} while(0)
static unsigned char *read_file(const char *path,size_t *size) {
    FILE *f=fopen(path,"rb");if(!f)return NULL;
    if(fseek(f,0,SEEK_END)){fclose(f);return NULL;}long n=ftell(f);
    if(n<=0 || n>64*1024*1024 || fseek(f,0,SEEK_SET)){fclose(f);return NULL;}
    unsigned char *bytes=malloc((size_t)n);if(!bytes){fclose(f);return NULL;}
    if(fread(bytes,1,(size_t)n,f)!=(size_t)n){free(bytes);fclose(f);return NULL;}
    fclose(f);*size=(size_t)n;return bytes;
}
static int semantic(nx_context *ctx,uint64_t id,const char *name,int selected,int focused) {
    uint32_t index;nx_semantic_view v;
    return nx_semantics_find(ctx,id,&index)==NX_OK && nx_semantics_get(ctx,index,&v)==NX_OK &&
        v.role==NX_ROLE_TREE_ITEM && v.parent_id==1 && v.name.length==strlen(name) &&
        !memcmp(v.name.data,name,v.name.length) && !!(v.state&NX_SEM_SELECTED)==selected &&
        !!(v.state&NX_SEM_FOCUSED)==focused;
}
static int capture(const char *path,const unsigned char *pixels) {
    if(!path)return 1;
    FILE *f=fopen(path,"wb");if(!f)return 0;
    int okay=fprintf(f,"P6\n240 84\n255\n")>0;
    for(size_t i=0;okay && i<240u*84u;++i)okay=fwrite(pixels+i*4u,1,3,f)==3;
    if(fclose(f))okay=0;
    return okay;
}
int main(int argc,char **argv) {
    int okay=0,root=-1,created=0;char path[]="/tmp/nexaui-files-scene-XXXXXX",name[32];
    struct ae_files *model=NULL;files_scene *scene=NULL;files_intent intent;files_intent_init(&intent);
    unsigned char *bytes=NULL;size_t length=0;nx_font *font=NULL;nx_font_collection *fonts=NULL;
    CHECK(argc==2 && mkdtemp(path));created=1;root=open(path,O_RDONLY|O_DIRECTORY|O_CLOEXEC);CHECK(root>=0);
    for(unsigned i=0;i<12;++i){snprintf(name,sizeof(name),"item%02u",i);
        int fd=openat(root,name,O_CREAT|O_EXCL|O_WRONLY|O_CLOEXEC,0600);CHECK(fd>=0 && !close(fd));}
    enum ae_files_result result;model=ae_files_create(root,&result);CHECK(model && result==AE_FILES_OK);
    CHECK((bytes=read_file(argv[1],&length)) && nx_font_open(bytes,length,NULL,&font)==NX_OK);
    CHECK(nx_font_collection_create(NULL,&fonts)==NX_OK && nx_font_collection_add(fonts,font)==NX_OK);
    CHECK(files_scene_create(model,fonts,&scene)==NX_OK);
    CHECK(files_scene_frame(scene,240,84)==NX_OK);
    nx_context *ctx=files_scene_context(scene);uint64_t first=files_scene_selected(scene);CHECK(first);
    CHECK(semantic(ctx,first,"item00",1,0));
    size_t semantics=0;CHECK(nx_semantics_count(ctx,&semantics)==NX_OK && semantics<=5);
    const nx_command *commands=NULL;size_t command_count=0,bitmaps=0;
    CHECK(nx_frame_commands(ctx,&commands,&command_count)==NX_OK);
    for(size_t i=0;i<command_count;++i)if(commands[i].type==NX_COMMAND_BITMAP)++bitmaps;
    CHECK(bitmaps>0);
    unsigned char pixels[240*84*4];CHECK(nx_headless_render(commands,command_count,pixels,240,84,240*4,(nx_color){0,0,0,1})==NX_OK);
    CHECK(capture(getenv("NEXAUI_FILES_SCENE_CAPTURE"),pixels));
    CHECK(files_scene_key(scene,NX_KEY_DOWN,0)==NX_OK && files_scene_selected(scene)==first);
    CHECK(files_scene_key(scene,NX_KEY_DOWN,1)==NX_OK);uint64_t second=files_scene_selected(scene);CHECK(second && second!=first);
    CHECK(files_scene_frame(scene,240,84)==NX_OK && files_scene_frame(scene,240,84)==NX_OK);
    CHECK(semantic(ctx,second,"item01",1,1));
    CHECK(files_scene_filter(scene,"item00",6)==NX_OK && files_scene_selected(scene)==second);
    CHECK(files_scene_frame(scene,240,84)==NX_OK);
    CHECK(files_scene_stage(scene,&intent)==AE_FILES_CHANGED);
    CHECK(files_scene_filter(scene,"",0)==NX_OK && files_scene_selected(scene)==second);
    CHECK(files_scene_frame(scene,240,84)==NX_OK && semantic(ctx,second,"item01",1,0));
    char oversized[AE_FILES_QUERY+1];memset(oversized,'x',sizeof(oversized));
    CHECK(files_scene_filter(scene,oversized,sizeof(oversized))==NX_ERROR_ARGUMENT && !files_scene_query_accepted(scene));
    CHECK(files_scene_stage(scene,&intent)==AE_FILES_INVALID);
    CHECK(files_scene_filter(scene,"",0)==NX_OK && files_scene_query_accepted(scene));
    CHECK(files_scene_stage(scene,&intent)==AE_FILES_OK && !strcmp(intent.leaf,"item01"));
    int added=openat(root,"item00a",O_CREAT|O_EXCL|O_WRONLY|O_CLOEXEC,0600);CHECK(added>=0 && !close(added));
    CHECK(ae_files_refresh(model)==AE_FILES_OK);
    CHECK(files_scene_stage(scene,&intent)==AE_FILES_CHANGED && !strcmp(intent.leaf,"item01"));
    CHECK(files_scene_sync(scene)==NX_OK);
    CHECK(files_scene_selected(scene)==second && files_scene_frame(scene,240,84)==NX_OK);
    CHECK(semantic(ctx,second,"item01",1,0));
    CHECK(files_scene_key(scene,NX_KEY_END,1)==NX_OK && files_scene_frame(scene,240,84)==NX_OK && files_scene_frame(scene,240,84)==NX_OK);
    CHECK(semantic(ctx,files_scene_selected(scene),"item11",1,1));
    CHECK(nx_semantics_count(ctx,&semantics)==NX_OK && semantics<=5);
    uint64_t end_selection=files_scene_selected(scene);
    CHECK(files_scene_scroll(scene,NAN)==NX_ERROR_ARGUMENT);
    CHECK(files_scene_scroll(scene,-56)==NX_OK && files_scene_frame(scene,240,84)==NX_OK);
    CHECK(files_scene_selected(scene)==end_selection);
    CHECK(files_scene_scroll(scene,56)==NX_OK && files_scene_frame(scene,240,84)==NX_OK);
    nx_semantic_view visible;CHECK(nx_semantics_get(ctx,1,&visible)==NX_OK && visible.name.length<sizeof(name));
    uint64_t clicked=visible.id;memcpy(name,visible.name.data,visible.name.length);name[visible.name.length]=0;
    nx_event event={0};event.button=NX_BUTTON_LEFT;event.x=10;event.y=10;event.type=NX_EVENT_POINTER_DOWN;
    CHECK(nx_context_queue_event(ctx,&event)==NX_OK);event.type=NX_EVENT_POINTER_UP;
    CHECK(nx_context_queue_event(ctx,&event)==NX_OK && files_scene_frame(scene,240,84)==NX_OK);
    CHECK(files_scene_selected(scene)==clicked && !strcmp(ae_files_entry(model,ae_files_selected(model))->name,name));
    okay=1;
cleanup:
    files_intent_clear(&intent);if(scene)files_scene_destroy(scene);
    if(fonts)nx_font_collection_destroy(fonts);
    if(font)nx_font_close(font);
    free(bytes);
    if(model)ae_files_destroy(model);
    if(root>=0){for(unsigned i=0;i<12;++i){snprintf(name,sizeof(name),"item%02u",i);(void)unlinkat(root,name,0);}
        (void)unlinkat(root,"item00a",0);close(root);}
    if(created)(void)rmdir(path);
    if(okay)puts("Original Files model: virtual scene preserves selection, exposes rows and accepts keyboard navigation");
    return okay?0:1;
}
