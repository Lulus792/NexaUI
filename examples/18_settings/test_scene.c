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
    uint64_t token=settings_pending_apply(s); CHECK(token);
    CHECK(settings_applied(s,&value,&motion) && value.length==7 && !memcmp(value.data,"Aetheri",7) && !motion);
    CHECK(settings_frame(s,640,360));
    uint32_t pending_index; nx_semantic_view pending_view;
    CHECK(nx_semantics_find(ui,5,&pending_index)==NX_OK && nx_semantics_get(ui,pending_index,&pending_view)==NX_OK &&
          (pending_view.state & NX_SEM_DISABLED));
    CHECK(nx_semantics_find(ui,6,&pending_index)==NX_OK && nx_semantics_get(ui,pending_index,&pending_view)==NX_OK &&
          pending_view.value.length==strlen("Applying to example...") &&
          !memcmp(pending_view.value.data,"Applying to example...",pending_view.value.length));
    CHECK(!settings_complete_apply(s,token+1,1) && settings_pending_apply(s)==token);
    CHECK(settings_complete_apply(s,token,1) && !settings_complete_apply(s,token,1));
    CHECK(settings_frame(s,640,360));
    CHECK(nx_focus_id(ui)==5);
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
    /* Failure preserves the old model; retry snapshots the submitted draft,
       even if the user edits again before the reply arrives. */
    CHECK(settings_set_draft_name(s,"First",5) && settings_frame(s,640,360));
    CHECK(nx_focus_set(ui,5)==NX_OK && key(ui,NX_KEY_SPACE,0) && settings_frame(s,640,360));
    token=settings_pending_apply(s); CHECK(token && settings_complete_apply(s,token,0));
    CHECK(settings_frame(s,640,360));
    CHECK(nx_semantics_find(ui,6,&index)==NX_OK && nx_semantics_get(ui,index,&view)==NX_OK &&
          view.value.length==28 && !memcmp(view.value.data,"Example apply failed. Retry.",28));
    CHECK(settings_applied(s,&value,&motion) && value.length==3 && !memcmp(value.data,"Ada",3) && motion==1);
    CHECK(settings_set_draft_name(s,"",0) && settings_frame(s,640,360));
    CHECK(nx_semantics_find(ui,6,&index)==NX_OK && nx_semantics_get(ui,index,&view)==NX_OK &&
          view.value.length==strlen("Enter a name under 128 bytes") &&
          !memcmp(view.value.data,"Enter a name under 128 bytes",view.value.length));
    CHECK(settings_set_draft_name(s,"First",5) && settings_frame(s,640,360) && nx_focus_set(ui,5)==NX_OK);
    CHECK(key(ui,NX_KEY_SPACE,0) && settings_frame(s,640,360));
    uint64_t retry=settings_pending_apply(s); CHECK(retry && retry!=token);
    CHECK(!settings_complete_apply(s,token,1));
    CHECK(settings_set_draft_name(s,"Later",5) && settings_frame(s,640,360));
    CHECK(nx_focus_set(ui,3)==NX_OK);
    CHECK(settings_complete_apply(s,retry,1) && settings_frame(s,640,360));
    CHECK(nx_focus_id(ui)==3);
    CHECK(settings_applied(s,&value,&motion) && value.length==5 && !memcmp(value.data,"First",5));
    CHECK(nx_semantics_find(ui,6,&index)==NX_OK && nx_semantics_get(ui,index,&view)==NX_OK &&
          view.value.length==15 && !memcmp(view.value.data,"Unsaved changes",15));
    /* At the minimum width, status geometry must accommodate wrapped text and
       Apply must not extend outside the content area. */
    CHECK(settings_set_draft_name(s,"",0) && settings_frame(s,128,150));
    CHECK(nx_semantics_find(ui,5,&index)==NX_OK && nx_semantics_get(ui,index,&view)==NX_OK &&
          view.bounds.x+view.bounds.width<=112);
    CHECK(nx_semantics_find(ui,6,&index)==NX_OK && nx_semantics_get(ui,index,&view)==NX_OK &&
          view.bounds.height>20);
    nx_font *measure_font=NULL; nx_font_collection *measure_fonts=NULL; nx_text_layout *measured=NULL;
    const char *invalid="Enter a name under 128 bytes";
    CHECK(nx_font_open(bytes,(size_t)length,NULL,&measure_font)==NX_OK);
    CHECK(nx_font_collection_create(NULL,&measure_fonts)==NX_OK &&
          nx_font_collection_add(measure_fonts,measure_font)==NX_OK);
    CHECK(nx_text_layout_create(measure_fonts,invalid,strlen(invalid),NX_BIDI_LTR,15,64,&measured)==NX_OK);
    double measured_width=nx_text_layout_width(measured), measured_height=nx_text_layout_height(measured);
    if (measured_width>64) {
        double size=15*0.98*64/measured_width;
        nx_text_layout_destroy(measured); measured=NULL;
        CHECK(nx_text_layout_create(measure_fonts,invalid,strlen(invalid),NX_BIDI_LTR,size,64,&measured)==NX_OK);
        measured_width=nx_text_layout_width(measured); measured_height=nx_text_layout_height(measured);
    }
    nx_text_layout_destroy(measured); measured=NULL;
    nx_font_collection_destroy(measure_fonts); nx_font_close(measure_font);
    CHECK(measured_width<=64 && view.bounds.height==measured_height);
    CHECK(nx_focus_set(ui,4)==NX_OK && settings_frame(s,128,150));
    CHECK(nx_semantics_find(ui,6,&index)==NX_OK && nx_semantics_get(ui,index,&view)==NX_OK);
    float initial_status_y=view.bounds.y;
    for (unsigned page=0;page<8;++page)
        CHECK(key(ui,NX_KEY_PAGE_DOWN,0) && settings_frame(s,128,150));
    CHECK(nx_focus_id(ui)==4 && nx_semantics_find(ui,6,&index)==NX_OK &&
          nx_semantics_get(ui,index,&view)==NX_OK && view.bounds.y<initial_status_y &&
          view.bounds.y+view.bounds.height<=134);
    float bottom_status_y=view.bounds.y;
    CHECK(key(ui,NX_KEY_PAGE_UP,0) && settings_frame(s,128,150));
    CHECK(nx_semantics_find(ui,6,&index)==NX_OK && nx_semantics_get(ui,index,&view)==NX_OK &&
          view.bounds.y>bottom_status_y);
    CHECK(settings_frame(s,640,150) && settings_frame(s,128,150) && nx_focus_id(ui)==4);
    CHECK(nx_semantics_find(ui,4,&index)==NX_OK && nx_semantics_get(ui,index,&view)==NX_OK &&
          view.bounds.y>=70 && view.bounds.y+view.bounds.height<=134 &&
          view.bounds.x+view.bounds.width<=112);
    settings_destroy(s); free(bytes); return 0;
fail:
    if (f) fclose(f); settings_destroy(s); free(bytes); return 1;
}
