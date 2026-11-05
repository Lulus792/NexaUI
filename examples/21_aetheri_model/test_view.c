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
static int visible(nx_context *ui,uint64_t id,float width,float height) {
    uint32_t index; nx_semantic_view item;
    return nx_semantics_find(ui,id,&index)==NX_OK && nx_semantics_get(ui,index,&item)==NX_OK &&
           item.bounds.x>=0 && item.bounds.y>=0 && item.bounds.width>0 && item.bounds.height>0 &&
           item.bounds.x+item.bounds.width<=width && item.bounds.y+item.bounds.height<=height;
}
static int inside_scroll(nx_context *ui,uint64_t id) {
    uint32_t index; nx_semantic_view item, scroll;
    if(nx_semantics_find(ui,id,&index)!=NX_OK || nx_semantics_get(ui,index,&item)!=NX_OK ||
       nx_semantics_find(ui,5,&index)!=NX_OK || nx_semantics_get(ui,index,&scroll)!=NX_OK) return 0;
    if(item.bounds.y>=scroll.bounds.y &&
       item.bounds.y+item.bounds.height<=scroll.bounds.y+scroll.bounds.height) return 1;
    fprintf(stderr,"item %llu: %.1f+%.1f outside %.1f+%.1f\n",(unsigned long long)id,
            item.bounds.y,item.bounds.height,scroll.bounds.y,scroll.bounds.height);
    return 0;
}
static int named(nx_context *ui,uint64_t id,const char *name,uint32_t required_state) {
    uint32_t index; nx_semantic_view item; size_t length=strlen(name);
    return nx_semantics_find(ui,id,&index)==NX_OK && nx_semantics_get(ui,index,&item)==NX_OK &&
        item.name.length==length && !memcmp(item.name.data,name,length) &&
        (item.state & required_state)==required_state;
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
    CHECK(named(ui,7,"Display data not yet observed",0));
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
    ui=aetheri_view_context(view);
    CHECK(aetheri_view_frame(view,480,380,&changed));
    CHECK(nx_focus_set(ui,10)==NX_OK && aetheri_view_frame(view,480,380,&changed));
    const uint64_t order[]={10,11,12,13,14,16,17,18,19};
    for(size_t i=0;i<sizeof(order)/sizeof(order[0]);++i) {
        CHECK(nx_focus_id(ui)==order[i]);
        CHECK(visible(ui,order[i],480,380));
        if(i+1<sizeof(order)/sizeof(order[0]))
            CHECK(press(ui,NX_KEY_TAB) && aetheri_view_frame(view,480,380,&changed));
    }
    /* The installed host must keep rendering and following keyboard focus
       after a compositor resize below its former 480x380 cutoff. */
    const float widths[]={320,360,399,400,440,480};
    for(size_t w=0;w<sizeof(widths)/sizeof(widths[0]);++w) {
        float width=widths[w];
        CHECK(aetheri_view_frame(view,width,240,&changed));
        CHECK(nx_focus_set(ui,10)==NX_OK && aetheri_view_frame(view,width,240,&changed));
        for(size_t i=0;i<sizeof(order)/sizeof(order[0]);++i) {
            CHECK(nx_focus_id(ui)==order[i]);
            CHECK(visible(ui,order[i],width,240));
            if(order[i]!=19) CHECK(inside_scroll(ui,order[i]));
            if(i+1<sizeof(order)/sizeof(order[0]))
                CHECK(press(ui,NX_KEY_TAB) && aetheri_view_frame(view,width,240,&changed));
        }
    }
    CHECK(aetheri_view_frame(view,480,380,&changed));
    /* At 200% the longest option retains its 28px layout, rather than being
       squeezed into a third of the window. */
    nx_font *measure_font=NULL; nx_font_collection *measure_fonts=NULL; nx_text_layout *measure=NULL;
    CHECK(nx_font_open(font,(size_t)length,NULL,&measure_font)==NX_OK);
    CHECK(nx_font_collection_create(NULL,&measure_fonts)==NX_OK && nx_font_collection_add(measure_fonts,measure_font)==NX_OK);
    CHECK(nx_text_layout_create(measure_fonts,"Hoher Kontrast",14,NX_BIDI_LTR,28,416,&measure)==NX_OK);
    uint32_t index; nx_semantic_view option;
    CHECK(nx_semantics_find(ui,12,&index)==NX_OK && nx_semantics_get(ui,index,&option)==NX_OK);
    CHECK(option.bounds.width>=nx_text_layout_width(measure)+63);
    nx_text_layout_destroy(measure); nx_font_collection_destroy(measure_fonts); nx_font_close(measure_font);
    /* Synthetic observations exercise the actual Aetheri model: identifiers,
       not row indices, retain the selected output when a snapshot is reordered. */
    struct ae_settings_display observations[2]={
        {.id=42,.name="Monitor A",.width=1920,.height=1080,.scale=1,.refresh=60,.mode_known=1},
        {.id=91,.name="Monitor B",.scale=1,.refresh=0,.mode_known=0}};
    CHECK(ae_settings_snapshot(&model,observations,2));
    CHECK(aetheri_view_frame(view,800,800,&changed));
    CHECK(named(ui,7,"Beobachtete Anzeigen",0));
    CHECK(named(ui,UINT64_C(0x100000000)+42,"Monitor A (1920 x 1080) (ausgew\xc3\xa4hlt)",0));
    CHECK(named(ui,UINT64_C(0x100000000)+91,"Monitor B",0));
    CHECK(nx_semantics_request_action(ui,UINT64_C(0x100000000)+91,NX_ACTION_PRESS)==NX_OK);
    CHECK(aetheri_view_frame(view,800,800,&changed) && changed && model.displays[model.selected].id==91);
    struct ae_settings_display reverse[2]={observations[1],observations[0]};
    CHECK(ae_settings_snapshot(&model,reverse,2) && model.displays[model.selected].id==91);
    CHECK(aetheri_view_frame(view,800,800,&changed));
    CHECK(named(ui,UINT64_C(0x100000000)+91,"Monitor B (ausgew\xc3\xa4hlt)",0));
    CHECK(nx_semantics_request_action(ui,UINT64_C(0x100000000)+42,NX_ACTION_PRESS)==NX_OK);
    CHECK(!ae_settings_snapshot(&model,NULL,1) && model.stale && model.count==2);
    CHECK(aetheri_view_frame(view,800,800,&changed) && !changed);
    CHECK(named(ui,7,"Anzeigedaten veraltet",0));
    CHECK(named(ui,UINT64_C(0x100000000)+42,"Monitor A (1920 x 1080)",NX_SEM_DISABLED));
    CHECK(nx_semantics_request_action(ui,UINT64_C(0x100000000)+42,NX_ACTION_PRESS)==NX_ERROR_STATE);
    CHECK(model.displays[model.selected].id==91);
    CHECK(ae_settings_snapshot(&model,NULL,0) && aetheri_view_frame(view,800,800,&changed));
    CHECK(named(ui,7,"Keine Anzeigen beobachtet",0));
    aetheri_view_destroy(view); free(font);
    puts("Original Aetheri model retains theme, language and scale across NexaUI view replacement"); return 0;
fail:
    if(file) fclose(file); aetheri_view_destroy(view); free(font); return 1;
}
