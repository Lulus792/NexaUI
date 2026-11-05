#include "view.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do {if(!(x)){fprintf(stderr,"%s:%d: %s\n",__FILE__,__LINE__,#x);return 1;}} while(0)
static int dock_label_widths(nx_context *ctx,const void *bytes,size_t length) {
    nx_font *font=NULL;nx_font_collection *fonts=NULL;
    CHECK(nx_font_open(bytes,length,NULL,&font)==NX_OK);
    CHECK(nx_font_collection_create(NULL,&fonts)==NX_OK && nx_font_collection_add(fonts,font)==NX_OK);
    float previous_right=0;
    for(uint64_t id=10;id<=13;++id) {
        uint32_t index;nx_semantic_view button;
        CHECK(nx_semantics_find(ctx,id,&index)==NX_OK && nx_semantics_get(ctx,index,&button)==NX_OK);
        CHECK(button.bounds.width>=44 && button.bounds.x>=previous_right && button.bounds.x+button.bounds.width<=320.01f);
        previous_right=button.bounds.x+button.bounds.width;
        nx_text_layout *layout=NULL;
        CHECK(nx_text_layout_create(fonts,button.name.data,button.name.length,NX_BIDI_LTR,14,1000,&layout)==NX_OK);
        double width=nx_text_layout_width(layout);nx_text_layout_destroy(layout);
        if(width>button.bounds.width-8+0.01) {
            fprintf(stderr,"Dock label %.*s requires %.2f but has %.2f logical pixels\n",
                    (int)button.name.length,button.name.data,width,button.bounds.width-8);return 1;
        }
    }
    nx_font_collection_destroy(fonts);nx_font_close(font);return 0;
}
static int capture_dock(nx_context *ctx,const char *path) {
    if(!path)return 0;
    const nx_command *commands=NULL;size_t count=0;unsigned char pixels[320*84*4];
    CHECK(nx_frame_commands(ctx,&commands,&count)==NX_OK);
    CHECK(nx_headless_render(commands,count,pixels,320,84,320*4,(nx_color){0,0,0,1})==NX_OK);
    FILE *out=fopen(path,"wb");CHECK(out && fprintf(out,"P6\n320 84\n255\n")>0);
    for(size_t i=0;i<320u*84u;++i)CHECK(fwrite(pixels+i*4,1,3,out)==3);
    CHECK(!fclose(out));return 0;
}
int main(int argc,char **argv) {
    CHECK(argc==2);FILE *f=fopen(argv[1],"rb");CHECK(f && !fseek(f,0,SEEK_END));long n=ftell(f);
    CHECK(n>0 && !fseek(f,0,SEEK_SET));void *bytes=malloc((size_t)n);CHECK(bytes && fread(bytes,1,(size_t)n,f)==(size_t)n);CHECK(!fclose(f));
    struct ae_desktop_model model;ae_desktop_model_init(&model);aetheri_desktop_view *view=NULL;
    CHECK(aetheri_desktop_view_create(&model,bytes,(size_t)n,&view));aetheri_desktop_intent intent;uint64_t revision=1;
    CHECK(aetheri_desktop_view_frame(view,640,84,revision,&intent) && intent.kind==AETHERI_DESKTOP_NONE);
    nx_context *ctx=aetheri_desktop_view_context(view);
    CHECK(nx_semantics_request_action(ctx,13,NX_ACTION_PRESS)==NX_OK);
    CHECK(aetheri_desktop_view_frame(view,640,84,revision,&intent) && intent.kind==AETHERI_DESKTOP_LAUNCH && intent.app==AE_DESKTOP_FILES);
    CHECK(!model.pending[AE_DESKTOP_FILES] && !model.process[AE_DESKTOP_FILES]);
    CHECK(ae_desktop_model_request(&model,AE_DESKTOP_FILES));++revision;
    CHECK(aetheri_desktop_view_frame(view,640,84,revision,&intent) && intent.kind==AETHERI_DESKTOP_NONE);
    CHECK(nx_semantics_request_action(ctx,13,NX_ACTION_PRESS)==NX_ERROR_STATE);
    CHECK(ae_desktop_model_reply(&model,AE_DESKTOP_FILES,'E'));++revision;
    CHECK(aetheri_desktop_view_frame(view,320,84,revision,&intent) && intent.kind==AETHERI_DESKTOP_NONE);
    CHECK(dock_label_widths(ctx,bytes,(size_t)n)==0);
    CHECK(capture_dock(ctx,getenv("NEXAUI_DOCK_EN_CAPTURE"))==0);
    uint32_t files_index;nx_semantic_view files_sem;
    CHECK(nx_semantics_find(ctx,13,&files_index)==NX_OK && nx_semantics_get(ctx,files_index,&files_sem)==NX_OK &&
          files_sem.bounds.x>=0 && files_sem.bounds.x+files_sem.bounds.width<=320 && !(files_sem.state&NX_SEM_DISABLED));
    CHECK(nx_focus_set(ctx,13)==NX_OK);
    nx_event files_key={0};files_key.type=NX_EVENT_KEY_DOWN;files_key.key=NX_KEY_ENTER;
    CHECK(nx_context_queue_event(ctx,&files_key)==NX_OK);files_key.type=NX_EVENT_KEY_UP;
    CHECK(nx_context_queue_event(ctx,&files_key)==NX_OK && aetheri_desktop_view_frame(view,320,84,revision,&intent) &&
          intent.kind==AETHERI_DESKTOP_LAUNCH && intent.app==AE_DESKTOP_FILES);
    model.german=true;++revision;
    CHECK(aetheri_desktop_view_frame(view,320,84,revision,&intent) && intent.kind==AETHERI_DESKTOP_NONE);
    CHECK(dock_label_widths(ctx,bytes,(size_t)n)==0);
    CHECK(capture_dock(ctx,getenv("NEXAUI_DOCK_DE_CAPTURE"))==0);
    CHECK(nx_semantics_find(ctx,13,&files_index)==NX_OK && nx_semantics_get(ctx,files_index,&files_sem)==NX_OK &&
          files_sem.name.length==7 && !memcmp(files_sem.name.data,"Dateien",7));
    model.german=false;++revision;
    CHECK(aetheri_desktop_view_frame(view,640,84,revision,&intent) && intent.kind==AETHERI_DESKTOP_NONE);
    CHECK(nx_semantics_request_action(ctx,11,NX_ACTION_PRESS)==NX_OK);
    CHECK(aetheri_desktop_view_frame(view,640,84,revision,&intent) && intent.kind==AETHERI_DESKTOP_LAUNCH && intent.app==AE_DESKTOP_SETTINGS);
    CHECK(!model.pending[AE_DESKTOP_SETTINGS] && !model.process[AE_DESKTOP_SETTINGS]);
    CHECK(ae_desktop_model_request(&model,AE_DESKTOP_SETTINGS));++revision;
    CHECK(aetheri_desktop_view_frame(view,640,84,revision,&intent) && intent.kind==AETHERI_DESKTOP_NONE);
    CHECK(nx_semantics_request_action(ctx,11,NX_ACTION_PRESS)==NX_ERROR_STATE);
    CHECK(nx_focus_set(ctx,12)==NX_OK);
    nx_event key={0};key.type=NX_EVENT_KEY_DOWN;key.key=NX_KEY_ENTER;
    CHECK(nx_context_queue_event(ctx,&key)==NX_OK);key.type=NX_EVENT_KEY_UP;
    CHECK(nx_context_queue_event(ctx,&key)==NX_OK);
    CHECK(aetheri_desktop_view_frame(view,640,84,revision,&intent) && intent.kind==AETHERI_DESKTOP_LAUNCH && intent.app==AE_DESKTOP_TERMINAL);
    CHECK(!model.pending[AE_DESKTOP_TERMINAL]);
    CHECK(nx_semantics_request_action(ctx,10,NX_ACTION_PRESS)==NX_OK);
    CHECK(aetheri_desktop_view_frame(view,640,84,revision,&intent) && intent.kind==AETHERI_DESKTOP_SEARCH && !model.search);
    ae_desktop_model_search(&model,0,true);
    CHECK(ae_desktop_model_window(&model,42,"Editor","org.example.Editor",false));++revision;
    CHECK(aetheri_desktop_view_frame(view,640,480,revision,&intent) && intent.kind==AETHERI_DESKTOP_NONE);
    CHECK(nx_focus_set(ctx,20)==NX_OK);
    key=(nx_event){0};key.type=NX_EVENT_KEY_DOWN;key.key=NX_KEY_TAB;key.modifiers=NX_MOD_CONTROL|NX_MOD_SHIFT;
    CHECK(aetheri_desktop_view_queue_event(view,&key)==NX_OK);key.type=NX_EVENT_KEY_UP;
    CHECK(aetheri_desktop_view_queue_event(view,&key)==NX_OK);
    CHECK(aetheri_desktop_view_frame(view,640,480,revision,&intent) && intent.kind==AETHERI_DESKTOP_NONE && nx_focus_id(ctx)==13);
    key=(nx_event){0};key.type=NX_EVENT_KEY_DOWN;key.key=NX_KEY_ENTER;
    CHECK(aetheri_desktop_view_queue_event(view,&key)==NX_OK);key.type=NX_EVENT_KEY_UP;
    CHECK(aetheri_desktop_view_queue_event(view,&key)==NX_OK);
    CHECK(aetheri_desktop_view_frame(view,640,480,revision,&intent) && intent.kind==AETHERI_DESKTOP_LAUNCH && intent.app==AE_DESKTOP_FILES);
    key=(nx_event){0};key.type=NX_EVENT_KEY_DOWN;key.key=NX_KEY_TAB;key.modifiers=NX_MOD_CONTROL;
    CHECK(aetheri_desktop_view_queue_event(view,&key)==NX_OK);key.type=NX_EVENT_KEY_UP;
    CHECK(aetheri_desktop_view_queue_event(view,&key)==NX_OK);
    CHECK(aetheri_desktop_view_frame(view,640,480,revision,&intent) && intent.kind==AETHERI_DESKTOP_NONE && nx_focus_id(ctx)==20);
    key=(nx_event){0};key.type=NX_EVENT_KEY_DOWN_CONSUMED;key.key=NX_KEY_TAB;
    CHECK(aetheri_desktop_view_queue_event(view,&key)==NX_OK);
    CHECK(aetheri_desktop_view_frame(view,640,480,revision,&intent) && intent.kind==AETHERI_DESKTOP_NONE && nx_focus_id(ctx)==20);
    key=(nx_event){0};key.type=NX_EVENT_KEY_DOWN;key.key=NX_KEY_TAB;
    CHECK(aetheri_desktop_view_queue_event(view,&key)==NX_OK);key.type=NX_EVENT_KEY_UP;
    CHECK(aetheri_desktop_view_queue_event(view,&key)==NX_OK);
    CHECK(aetheri_desktop_view_frame(view,640,480,revision,&intent) && intent.kind==AETHERI_DESKTOP_SELECT && intent.app==AE_DESKTOP_TERMINAL && nx_focus_id(ctx)==20);
    ae_desktop_model_select(&model,1);++revision;
    CHECK(aetheri_desktop_view_frame(view,640,480,revision,&intent) && intent.kind==AETHERI_DESKTOP_NONE);
    key=(nx_event){0};key.type=NX_EVENT_KEY_DOWN;key.key=NX_KEY_TAB;key.modifiers=NX_MOD_SHIFT;
    CHECK(aetheri_desktop_view_queue_event(view,&key)==NX_OK);key.type=NX_EVENT_KEY_UP;
    CHECK(aetheri_desktop_view_queue_event(view,&key)==NX_OK);
    CHECK(aetheri_desktop_view_frame(view,640,480,revision,&intent) && intent.kind==AETHERI_DESKTOP_SELECT && intent.app==AE_DESKTOP_SETTINGS && nx_focus_id(ctx)==20);
    ae_desktop_model_select(&model,-1);++revision;
    CHECK(aetheri_desktop_view_frame(view,640,480,revision,&intent) && intent.kind==AETHERI_DESKTOP_NONE);
    key=(nx_event){0};key.type=NX_EVENT_KEY_DOWN;key.key=NX_KEY_DOWN;
    CHECK(nx_context_queue_event(ctx,&key)==NX_OK);key.type=NX_EVENT_KEY_UP;CHECK(nx_context_queue_event(ctx,&key)==NX_OK);
    CHECK(aetheri_desktop_view_frame(view,640,480,revision,&intent) && intent.kind==AETHERI_DESKTOP_SELECT && intent.app==AE_DESKTOP_TERMINAL && !intent.window);
    uint32_t selected_semantic;nx_semantic_view selected_state;
    CHECK(nx_semantics_find(ctx,UINT64_C(0x200000000)+AE_DESKTOP_SETTINGS,&selected_semantic)==NX_OK &&
          nx_semantics_get(ctx,selected_semantic,&selected_state)==NX_OK && selected_state.role==NX_ROLE_LIST_ITEM && (selected_state.state&NX_SEM_SELECTED));
    CHECK(nx_semantics_request_action(ctx,UINT64_C(0x200000000)+AE_DESKTOP_SETTINGS,NX_ACTION_SELECT)==NX_ERROR_STATE);
    CHECK(nx_semantics_request_action(ctx,UINT64_C(0x200000000)+AE_DESKTOP_TERMINAL,NX_ACTION_SELECT)==NX_OK);
    model.contrast=true;++revision;
    CHECK(aetheri_desktop_view_frame(view,640,480,revision,&intent) && intent.kind==AETHERI_DESKTOP_NONE && model.selected_app==AE_DESKTOP_SETTINGS);
    CHECK(nx_semantics_request_action(ctx,UINT64_C(0x200000000)+AE_DESKTOP_TERMINAL,NX_ACTION_SELECT)==NX_OK);
    CHECK(aetheri_desktop_view_frame(view,640,480,revision,&intent) && intent.kind==AETHERI_DESKTOP_SELECT && intent.app==AE_DESKTOP_TERMINAL);
    CHECK(model.selected_app==AE_DESKTOP_SETTINGS && nx_semantics_get(ctx,selected_semantic,&selected_state)==NX_OK && (selected_state.state&NX_SEM_SELECTED));
    CHECK(model.selected_app==AE_DESKTOP_SETTINGS);ae_desktop_model_select(&model,1);++revision;
    CHECK(aetheri_desktop_view_frame(view,640,480,revision,&intent) && intent.kind==AETHERI_DESKTOP_NONE);
    CHECK(nx_semantics_find(ctx,UINT64_C(0x200000000)+AE_DESKTOP_TERMINAL,&selected_semantic)==NX_OK &&
          nx_semantics_get(ctx,selected_semantic,&selected_state)==NX_OK && (selected_state.state&NX_SEM_SELECTED));
    model.contrast=true;++revision;
    CHECK(aetheri_desktop_view_frame(view,640,480,revision,&intent) && intent.kind==AETHERI_DESKTOP_NONE);
    CHECK(nx_semantics_find(ctx,UINT64_C(0x200000000)+AE_DESKTOP_TERMINAL,&selected_semantic)==NX_OK &&
          nx_semantics_get(ctx,selected_semantic,&selected_state)==NX_OK && (selected_state.state&NX_SEM_SELECTED));
    key=(nx_event){0};key.type=NX_EVENT_KEY_DOWN;key.key=NX_KEY_ENTER;
    CHECK(nx_context_queue_event(ctx,&key)==NX_OK);key.type=NX_EVENT_KEY_UP;CHECK(nx_context_queue_event(ctx,&key)==NX_OK);
    CHECK(aetheri_desktop_view_frame(view,640,480,revision,&intent) && intent.kind==AETHERI_DESKTOP_LAUNCH && intent.app==AE_DESKTOP_TERMINAL);
    nx_event text={0};text.type=NX_EVENT_TEXT;text.scalar='A';CHECK(nx_context_queue_event(ctx,&text)==NX_OK);
    CHECK(aetheri_desktop_view_frame(view,640,480,revision,&intent) && intent.kind==AETHERI_DESKTOP_QUERY && intent.query[0]=='A' && !intent.query[1]);
    CHECK(!model.query[0] && ae_desktop_model_text(&model,intent.query));++revision;
    CHECK(aetheri_desktop_view_frame(view,640,480,revision,&intent) && intent.kind==AETHERI_DESKTOP_NONE);
    ae_desktop_model_backspace(&model);++revision;
    CHECK(aetheri_desktop_view_frame(view,640,480,revision,&intent) && intent.kind==AETHERI_DESKTOP_NONE);
    text.scalar='B';CHECK(nx_context_queue_event(ctx,&text)==NX_OK);
    CHECK(ae_desktop_model_window(&model,42,"Editor updated","org.example.Editor",false));++revision;
    CHECK(aetheri_desktop_view_frame(view,640,480,revision,&intent) && intent.kind==AETHERI_DESKTOP_NONE);
    uint32_t query_index;nx_semantic_view query_view;
    CHECK(nx_semantics_find(ctx,20,&query_index)==NX_OK && nx_semantics_get(ctx,query_index,&query_view)==NX_OK && query_view.value.length==0);
    CHECK(aetheri_desktop_view_frame(view,640,480,revision,&intent) && intent.kind==AETHERI_DESKTOP_NONE && !model.query[0]);
    CHECK(nx_focus_id(ctx)==20);
    text.scalar='C';CHECK(nx_context_queue_event(ctx,&text)==NX_OK);
    CHECK(aetheri_desktop_view_frame(view,640,480,revision,&intent) && intent.kind==AETHERI_DESKTOP_QUERY && intent.query[0]=='C' && !intent.query[1]);
    CHECK(ae_desktop_model_text(&model,intent.query));++revision;
    CHECK(aetheri_desktop_view_frame(view,640,480,revision,&intent) && intent.kind==AETHERI_DESKTOP_NONE);
    ae_desktop_model_backspace(&model);++revision;
    CHECK(aetheri_desktop_view_frame(view,640,480,revision,&intent) && intent.kind==AETHERI_DESKTOP_NONE);
    /* A press captured for an old window must not activate an ID reused by a
       newer model, even when release arrives after reconciliation. */
    uint32_t old_index;nx_semantic_view old_row;
    CHECK(nx_semantics_find(ctx,UINT64_C(0x100000000)+42,&old_index)==NX_OK &&
          nx_semantics_get(ctx,old_index,&old_row)==NX_OK);
    nx_event pointer={0};pointer.type=NX_EVENT_POINTER_MOVE;
    pointer.x=old_row.bounds.x+8;pointer.y=old_row.bounds.y+8;
    CHECK(aetheri_desktop_view_queue_event(view,&pointer)==NX_OK);
    pointer.type=NX_EVENT_POINTER_DOWN;pointer.button=NX_BUTTON_LEFT;
    CHECK(aetheri_desktop_view_queue_event(view,&pointer)==NX_OK);
    CHECK(aetheri_desktop_view_frame(view,640,480,revision,&intent) && intent.kind==AETHERI_DESKTOP_NONE);
    ae_desktop_model_closed(&model,42);
    CHECK(ae_desktop_model_window(&model,42,"Replacement","org.example.Other",false));++revision;
    CHECK(aetheri_desktop_view_frame(view,640,480,revision,&intent) && intent.kind==AETHERI_DESKTOP_NONE);
    pointer.type=NX_EVENT_POINTER_UP;CHECK(aetheri_desktop_view_queue_event(view,&pointer)==NX_OK);
    CHECK(aetheri_desktop_view_frame(view,640,480,revision,&intent) && intent.kind==AETHERI_DESKTOP_NONE);
    pointer.type=NX_EVENT_POINTER_DOWN;CHECK(aetheri_desktop_view_queue_event(view,&pointer)==NX_OK);
    pointer.type=NX_EVENT_POINTER_UP;CHECK(aetheri_desktop_view_queue_event(view,&pointer)==NX_OK);
    CHECK(aetheri_desktop_view_frame(view,640,480,revision,&intent) && intent.kind==AETHERI_DESKTOP_ACTIVATE && intent.window==42);
    CHECK(nx_focus_set(ctx,UINT64_C(0x100000000)+42)==NX_OK);
    key=(nx_event){0};key.type=NX_EVENT_KEY_DOWN;key.key=NX_KEY_ENTER;
    CHECK(aetheri_desktop_view_queue_event(view,&key)==NX_OK);
    CHECK(aetheri_desktop_view_frame(view,640,480,revision,&intent) && intent.kind==AETHERI_DESKTOP_NONE);
    ae_desktop_model_closed(&model,42);
    CHECK(ae_desktop_model_window(&model,42,"Another replacement","org.example.Other",false));++revision;
    CHECK(aetheri_desktop_view_frame(view,640,480,revision,&intent) && intent.kind==AETHERI_DESKTOP_NONE);
    key.type=NX_EVENT_KEY_UP;CHECK(aetheri_desktop_view_queue_event(view,&key)==NX_OK);
    CHECK(aetheri_desktop_view_frame(view,640,480,revision,&intent) && intent.kind==AETHERI_DESKTOP_NONE);
    key.type=NX_EVENT_KEY_DOWN;CHECK(aetheri_desktop_view_queue_event(view,&key)==NX_OK);
    key.type=NX_EVENT_KEY_UP;CHECK(aetheri_desktop_view_queue_event(view,&key)==NX_OK);
    CHECK(aetheri_desktop_view_frame(view,640,480,revision,&intent) && intent.kind==AETHERI_DESKTOP_ACTIVATE && intent.window==42);
    CHECK(nx_semantics_request_action(ctx,UINT64_C(0x100000000)+42,NX_ACTION_PRESS)==NX_OK);
    CHECK(aetheri_desktop_view_frame(view,640,480,revision,&intent) && intent.kind==AETHERI_DESKTOP_ACTIVATE && intent.window==42 && intent.app==AE_DESKTOP_APP_NONE);
    CHECK(nx_semantics_request_action(ctx,UINT64_C(0x100000000)+42,NX_ACTION_PRESS)==NX_OK);
    ae_desktop_model_closed(&model,42);
    CHECK(ae_desktop_model_window(&model,42,"Replacement","org.example.Other",false));++revision;
    CHECK(aetheri_desktop_view_frame(view,640,480,revision,&intent) && intent.kind==AETHERI_DESKTOP_NONE);
    CHECK(model.count==1 && model.search);
    CHECK(nx_semantics_request_action(ctx,UINT64_C(0x100000000)+42,NX_ACTION_PRESS)==NX_OK);
    CHECK(aetheri_desktop_view_frame(view,640,480,revision,&intent) && intent.kind==AETHERI_DESKTOP_ACTIVATE && intent.window==42);
    for(uint32_t i=1;i<=20;++i)CHECK(ae_desktop_model_window(&model,i,"Window","org.example.App",false));
    model.selected=20;model.selected_app=AE_DESKTOP_APP_NONE;++revision;
    CHECK(aetheri_desktop_view_frame(view,640,240,revision,&intent) && intent.kind==AETHERI_DESKTOP_NONE);
    uint32_t selected_index;nx_semantic_view selected_view;
    CHECK(nx_semantics_find(ctx,UINT64_C(0x100000000)+20,&selected_index)==NX_OK && nx_semantics_get(ctx,selected_index,&selected_view)==NX_OK);
    CHECK(selected_view.bounds.y>=68 && selected_view.bounds.y+selected_view.bounds.height<=152);
    CHECK(nx_focus_set(ctx,20)==NX_OK);
    text=(nx_event){0};text.type=NX_EVENT_TEXT;text.scalar='A';CHECK(aetheri_desktop_view_queue_event(view,&text)==NX_OK);
    text.scalar='B';CHECK(aetheri_desktop_view_queue_event(view,&text)==NX_OK);
    CHECK(aetheri_desktop_view_frame(view,640,480,revision,&intent) && intent.kind==AETHERI_DESKTOP_QUERY && !strcmp(intent.query,"AB"));
    CHECK(ae_desktop_model_text(&model,intent.query));++revision;
    CHECK(aetheri_desktop_view_frame(view,640,480,revision,&intent) && intent.kind==AETHERI_DESKTOP_NONE);
    key=(nx_event){0};key.type=NX_EVENT_KEY_DOWN;key.key=NX_KEY_LEFT;
    CHECK(aetheri_desktop_view_queue_event(view,&key)==NX_OK);key.type=NX_EVENT_KEY_UP;CHECK(aetheri_desktop_view_queue_event(view,&key)==NX_OK);
    CHECK(aetheri_desktop_view_frame(view,640,480,revision,&intent) && intent.kind==AETHERI_DESKTOP_NONE);
    key=(nx_event){0};key.type=NX_EVENT_KEY_DOWN;key.key=NX_KEY_TAB;
    CHECK(aetheri_desktop_view_queue_event(view,&key)==NX_OK);key.type=NX_EVENT_KEY_UP;CHECK(aetheri_desktop_view_queue_event(view,&key)==NX_OK);
    CHECK(aetheri_desktop_view_frame(view,640,480,revision,&intent));
    text.scalar='C';CHECK(aetheri_desktop_view_queue_event(view,&text)==NX_OK);
    CHECK(aetheri_desktop_view_frame(view,640,480,revision,&intent) && intent.kind==AETHERI_DESKTOP_QUERY && !strcmp(intent.query,"ACB"));
    ae_desktop_model_search(&model,0,true);CHECK(ae_desktop_model_text(&model,intent.query));++revision;
    CHECK(aetheri_desktop_view_set_host_ime(view,1)==NX_OK);
    CHECK(aetheri_desktop_view_surface_focus(view,1)==NX_OK);
    CHECK(aetheri_desktop_view_frame(view,640,480,revision,&intent) && intent.kind==AETHERI_DESKTOP_NONE);
    CHECK(aetheri_desktop_view_frame(view,640,480,revision,&intent) && intent.kind==AETHERI_DESKTOP_NONE);
    uint64_t ime_revision=0;CHECK(aetheri_desktop_view_host_ime_revision(view,&ime_revision)==NX_OK && ime_revision);
    char surrounding[AE_DESKTOP_QUERY];size_t sl=0,sc=0,sa=0;
    CHECK(aetheri_desktop_view_host_ime_surrounding(view,ime_revision,surrounding,sizeof(surrounding),&sl,&sc,&sa)==NX_OK &&
          sl==3 && !memcmp(surrounding,"ACB",3) && sc==2 && sa==2);
    uint64_t before_focus_loss=ime_revision;
    CHECK(aetheri_desktop_view_surface_focus(view,0)==NX_OK);
    CHECK(aetheri_desktop_view_frame(view,640,480,revision,&intent) && intent.kind==AETHERI_DESKTOP_NONE);
    ime_revision=9;CHECK(aetheri_desktop_view_host_ime_revision(view,&ime_revision)==NX_ERROR_STATE && !ime_revision);
    CHECK(aetheri_desktop_view_surface_focus(view,1)==NX_OK);
    CHECK(aetheri_desktop_view_frame(view,640,480,revision,&intent) && intent.kind==AETHERI_DESKTOP_NONE);
    CHECK(aetheri_desktop_view_host_ime_revision(view,&ime_revision)==NX_OK && ime_revision && ime_revision!=before_focus_loss);
    nx_rect ime_caret={0};
    CHECK(aetheri_desktop_view_host_ime_preedit(view,ime_revision,"d",1,1,1)==NX_OK);
    CHECK(aetheri_desktop_view_frame(view,640,480,revision,&intent) && intent.kind==AETHERI_DESKTOP_NONE);
    CHECK(aetheri_desktop_view_host_ime_caret(view,&ime_caret)==NX_OK && ime_caret.height>0);
    text=(nx_event){0};text.type=NX_EVENT_TEXT;text.scalar='D';text.ime_target=20;text.ime_revision=ime_revision;
    CHECK(aetheri_desktop_view_queue_event(view,&text)==NX_OK);
    CHECK(aetheri_desktop_view_frame(view,640,480,revision,&intent) && intent.kind==AETHERI_DESKTOP_QUERY && !strcmp(intent.query,"ACDB"));
    aetheri_desktop_view_destroy(view);free(bytes);return 0;
}
