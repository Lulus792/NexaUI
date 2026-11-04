#include "scene.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"%s:%d: %s\n",__FILE__,__LINE__,#x); goto fail; } } while (0)
static int key(nx_context *ui, nx_key k, uint32_t mod) {
    nx_event e={0}; e.type=NX_EVENT_KEY_DOWN; e.key=k; e.modifiers=mod;
    if (nx_context_queue_event(ui,&e)!=NX_OK) return 0;
    e.type=NX_EVENT_KEY_UP; return nx_context_queue_event(ui,&e)==NX_OK;
}
static int type(nx_context *ui, const char *str) {
    for (; *str; ++str) { nx_event e={0}; e.type=NX_EVENT_TEXT; e.scalar=(unsigned char)*str;
        if (nx_context_queue_event(ui,&e)!=NX_OK) return 0; }
    return 1;
}
int main(int argc,char **argv) {
    FILE *f=NULL; unsigned char *bytes=NULL; settings_scene *s=NULL; long length;
    CHECK(argc==2); f=fopen(argv[1],"rb"); CHECK(f);
    CHECK(fseek(f,0,SEEK_END)==0 && (length=ftell(f))>0 && length<64*1024*1024);
    CHECK(fseek(f,0,SEEK_SET)==0); bytes=malloc((size_t)length); CHECK(bytes);
    CHECK(fread(bytes,1,(size_t)length,f)==(size_t)length); CHECK(fclose(f)==0); f=NULL;
    CHECK(settings_create(bytes,(size_t)length,NULL,&s));
    nx_context *ui=settings_context(s); nx_utf8_span value; int motion;
    CHECK(settings_frame(s,640,360));
    CHECK(key(ui,NX_KEY_TAB,0) && settings_frame(s,640,360) && nx_focus_id(ui)==3);
    CHECK(key(ui,NX_KEY_A,NX_MOD_CONTROL) && type(ui,"Ada") && settings_frame(s,640,360));
    CHECK(key(ui,NX_KEY_TAB,0) && settings_frame(s,640,360) && nx_focus_id(ui)==4);
    CHECK(key(ui,NX_KEY_SPACE,0) && settings_frame(s,640,360));
    CHECK(key(ui,NX_KEY_TAB,0) && settings_frame(s,640,360) && nx_focus_id(ui)==5);
    /* A resize must reveal the still-focused Apply control without more input. */
    CHECK(settings_frame(s,640,360) && settings_frame(s,240,150));
    uint32_t resize_index; nx_semantic_view resize_view;
    CHECK(nx_focus_id(ui)==5 && nx_semantics_find(ui,5,&resize_index)==NX_OK &&
          nx_semantics_get(ui,resize_index,&resize_view)==NX_OK &&
          resize_view.bounds.y>=70 && resize_view.bounds.y+resize_view.bounds.height<=134);
    CHECK(settings_frame(s,640,360));
    CHECK(key(ui,NX_KEY_SPACE,0) && settings_frame(s,640,360));
    CHECK(settings_applied(s,&value,&motion) && value.length==3 && !memcmp(value.data,"Ada",3) && motion==1);
    uint32_t index; nx_semantic_view view;
    CHECK(nx_semantics_find(ui,6,&index)==NX_OK && nx_semantics_get(ui,index,&view)==NX_OK &&
          view.role==NX_ROLE_STATUS && view.value.length==23 && !memcmp(view.value.data,"Applied to this example",23));
    CHECK(key(ui,NX_KEY_TAB,NX_MOD_SHIFT) && settings_frame(s,640,360) && nx_focus_id(ui)==4);
    CHECK(key(ui,NX_KEY_TAB,NX_MOD_SHIFT) && settings_frame(s,640,360) && nx_focus_id(ui)==3);
    CHECK(key(ui,NX_KEY_A,NX_MOD_CONTROL) && key(ui,NX_KEY_BACKSPACE,0) && settings_frame(s,640,360));
    CHECK(nx_semantics_find(ui,5,&index)==NX_OK && nx_semantics_get(ui,index,&view)==NX_OK &&
          (view.state & NX_SEM_DISABLED));
    CHECK(settings_applied(s,&value,&motion) && value.length==3 && !memcmp(value.data,"Ada",3) && motion==1);
    /* Small viewport: keyboard navigation must reveal each focused control. */
    CHECK(settings_frame(s,240,150) && type(ui,"B") && settings_frame(s,240,150));
    CHECK(key(ui,NX_KEY_TAB,0) && settings_frame(s,240,150) && nx_focus_id(ui)==4);
    CHECK(settings_frame(s,240,150));
    CHECK(nx_semantics_find(ui,4,&index)==NX_OK && nx_semantics_get(ui,index,&view)==NX_OK &&
          view.bounds.y>=70 && view.bounds.y+view.bounds.height<=134);
    CHECK(key(ui,NX_KEY_TAB,0) && settings_frame(s,240,150) && nx_focus_id(ui)==5);
    CHECK(settings_frame(s,240,150));
    CHECK(nx_semantics_find(ui,5,&index)==NX_OK && nx_semantics_get(ui,index,&view)==NX_OK &&
          view.bounds.y>=70 && view.bounds.y+view.bounds.height<=134);
    CHECK(key(ui,NX_KEY_TAB,NX_MOD_SHIFT) && settings_frame(s,240,150) && nx_focus_id(ui)==4);
    CHECK(settings_frame(s,240,150));
    CHECK(nx_semantics_find(ui,4,&index)==NX_OK && nx_semantics_get(ui,index,&view)==NX_OK &&
          view.bounds.y>=70 && view.bounds.y+view.bounds.height<=134);
    settings_destroy(s); free(bytes); return 0;
fail:
    if (f) fclose(f); settings_destroy(s); free(bytes); return 1;
}
