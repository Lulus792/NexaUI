#include "view.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do { if(!(x)) {fprintf(stderr,"%s:%d: %s\n",__FILE__,__LINE__,#x); goto fail;} } while(0)
static int press(nx_context *ui, nx_key key) {
    nx_event event={0}; event.type=NX_EVENT_KEY_DOWN; event.key=key;
    if(nx_context_queue_event(ui,&event)!=NX_OK) return 0;
    event.type=NX_EVENT_KEY_UP; return nx_context_queue_event(ui,&event)==NX_OK;
}
static int checked(nx_context *ui,uint64_t id) {
    uint32_t index; nx_semantic_view view;
    return nx_semantics_find(ui,id,&index)==NX_OK && nx_semantics_get(ui,index,&view)==NX_OK &&
           view.role==NX_ROLE_RADIO && (view.state & NX_SEM_CHECKED);
}
int main(int argc,char **argv) {
    FILE *file=NULL; unsigned char *font=NULL; aetheri_view *view=NULL; long length; int changed=0;
    struct ae_settings model; ae_settings_init(&model);
    CHECK(argc==2 && (file=fopen(argv[1],"rb")));
    CHECK(!fseek(file,0,SEEK_END) && (length=ftell(file))>0 && length<64*1024*1024 && !fseek(file,0,SEEK_SET));
    font=malloc((size_t)length); CHECK(font && fread(font,1,(size_t)length,file)==(size_t)length);
    CHECK(!fclose(file)); file=NULL;
    CHECK(aetheri_view_create(&model,font,(size_t)length,&view));
    nx_context *ui=aetheri_view_context(view);
    CHECK(aetheri_view_frame(view,800,480,&changed) && !changed);
    CHECK(checked(ui,10) && checked(ui,13) && checked(ui,16));
    CHECK(nx_focus_set(ui,12)==NX_OK && press(ui,NX_KEY_SPACE) && aetheri_view_frame(view,800,480,&changed));
    CHECK(changed && model.theme==AE_SETTINGS_CONTRAST);
    CHECK(aetheri_view_frame(view,800,480,&changed) && checked(ui,12) && checked(ui,13) && checked(ui,16));
    CHECK(nx_focus_set(ui,14)==NX_OK && press(ui,NX_KEY_SPACE) && aetheri_view_frame(view,800,480,&changed));
    CHECK(changed && model.german);
    CHECK(aetheri_view_frame(view,800,480,&changed) && checked(ui,12) && checked(ui,14) && checked(ui,16));
    CHECK(nx_focus_set(ui,18)==NX_OK && press(ui,NX_KEY_SPACE) && aetheri_view_frame(view,800,480,&changed));
    CHECK(changed && model.text_percent==200);
    CHECK(aetheri_view_frame(view,800,480,&changed) && checked(ui,12) && checked(ui,14) && checked(ui,18));
    aetheri_view_destroy(view); view=NULL;
    CHECK(aetheri_view_create(&model,font,(size_t)length,&view));
    CHECK(aetheri_view_frame(view,800,480,&changed) && checked(aetheri_view_context(view),12) &&
          checked(aetheri_view_context(view),14) && checked(aetheri_view_context(view),18));
    aetheri_view_destroy(view); free(font);
    puts("Original Aetheri model retains theme, language and scale across NexaUI view replacement"); return 0;
fail:
    if(file) fclose(file); aetheri_view_destroy(view); free(font); return 1;
}
