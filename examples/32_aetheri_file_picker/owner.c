#include "owner.h"
#include "../common/text_wrap.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
static nx_utf8_span span(const char *s){return (nx_utf8_span){s,strlen(s)};}
static const char *word(const fp_owner *o,const char *en,const char *de){return o->german?de:en;}
static void clear(fp_owner *o){for(size_t i=0;i<10;++i){nx_text_layout_destroy(o->labels[i]);o->labels[i]=NULL;}for(size_t i=0;i<FP_PAGE_ROWS;++i){nx_text_layout_destroy(o->rows[i]);o->rows[i]=NULL;}o->layout_width=0;o->revealed_focus=0;}
static void release_batch(fp_owner *o){
    for(size_t i=0;i<o->batch_count;++i){close(o->batch[i].directory);close(o->batch[i].witness);}
    free(o->batch);o->batch=NULL;o->batch_count=o->batch_page=0;nx_sim_message_dispose(&o->batch_labels);
}
const fp_row *fp_owner_batch_row(const fp_owner *o,size_t index){
    const fp_reply *r=o?fp_reply_get(&o->batch_labels):NULL;if(!r||index>=o->batch_count)return NULL;
    size_t ordinal=o->batch[index].ordinal,first=0,last=r->count;
    while(first<last){size_t middle=first+(last-first)/2;if(r->rows[middle].ordinal<ordinal)first=middle+1;else last=middle;}
    return first<r->count&&r->rows[first].ordinal==ordinal?&r->rows[first]:NULL;
}
nx_result fp_owner_init(fp_owner *o,nx_font_collection *fonts){
    if(!o||!fonts)return NX_ERROR_ARGUMENT;
    *o=(fp_owner){0};o->fonts=fonts;o->directory=o->witness=-1;o->text_size=16;o->restore=FP_OWNER_FILE;
    return nx_text_field_create("",0,4096,&o->note);
}
void fp_owner_destroy(fp_owner *o){
    if(!o||!o->fonts)return;
    clear(o);nx_text_field_destroy(o->note);release_batch(o);
    if(o->directory>=0)close(o->directory);
    if(o->witness>=0&&o->witness!=o->directory)close(o->witness);
    *o=(fp_owner){0};o->directory=o->witness=-1;
}
nx_result fp_owner_open(fp_owner *o,fp_picker *p,fp_worker *worker,int mode){
    if(!o||!p||!worker||mode<0||mode>3)return NX_ERROR_ARGUMENT;
    if(o->active)return NX_ERROR_STATE;
    if(o->epoch==UINT64_MAX)return NX_ERROR_LIMIT;
    nx_result result=fp_picker_init(p,worker,o->fonts,++o->epoch);
    if(result!=NX_OK)return result;
    p->german=o->german;p->text_size=o->text_size;p->directories=mode==1;p->multiple=mode==2;p->save=mode==3;p->parent_id=FP_OWNER_MODAL;
    o->active=1;o->modal_built=0;o->trigger=mode==1?FP_OWNER_DIRECTORY:mode==2?FP_OWNER_MULTIPLE:mode==3?FP_OWNER_SAVE:FP_OWNER_FILE;return NX_OK;
}
nx_result fp_owner_finish(fp_owner *o,fp_picker *p,nx_context *ctx,int selected){
    if(!o||!p||!ctx||(selected!=0&&selected!=1))return NX_ERROR_ARGUMENT;
    if(!o->active)return NX_ERROR_STATE;
    const fp_reply *r=fp_picker_snapshot(p);const char *label=NULL;struct stat d,w;
    if(selected&&p->save){
        if(o->selections==UINT64_MAX)return NX_ERROR_LIMIT;
        if(!fp_picker_save_ready(p)||r->directory<4||fstat(r->directory,&d)||!S_ISDIR(d.st_mode)||
            (r->save_kind==FP_SAVE_EXISTING&&(r->witness<4||fstat(r->witness,&w)||!S_ISREG(w.st_mode))))return NX_ERROR_STATE;
        if(o->modal_built){nx_result result=nx_overlay_close(ctx);if(result!=NX_OK)return result;}
        fp_save_target target;nx_result result=fp_reply_take_save(fp_reply_get(&p->observation),&target);if(result!=NX_OK)return result;
        release_batch(o);if(o->directory>=0)close(o->directory);if(o->witness>=0&&o->witness!=o->directory)close(o->witness);
        o->directory=target.directory;o->witness=target.witness;o->save_kind=target.kind;o->directories=0;
        memcpy(o->leaf,target.leaf,strlen(target.leaf)+1);memcpy(o->label,target.label,strlen(target.label)+1);++o->selections;clear(o);
        fp_picker_destroy(p);o->active=o->modal_built=0;o->restore=o->trigger;return NX_OK;
    }
    if(selected&&p->multiple){
        if(o->selections==UINT64_MAX)return NX_ERROR_LIMIT;
        if(!r||r->result!=AE_FILES_OK||p->operation!=FP_PICK_MANY||!r->pick_count||r->pick_count>r->count)return NX_ERROR_STATE;
        size_t row=0;
        for(size_t i=0;i<r->pick_count;++i){const fp_pick *pick=&r->picks[i];while(row<r->count&&r->rows[row].ordinal<pick->ordinal)++row;
            if(row==r->count||r->rows[row].ordinal!=pick->ordinal||r->rows[row].entry.kind!=AE_FILES_REGULAR||
               fstat(pick->directory,&d)||!S_ISDIR(d.st_mode)||fstat(pick->witness,&w)||!S_ISREG(w.st_mode)||
               w.st_dev!=r->rows[row].entry.device||w.st_ino!=r->rows[row].entry.inode)return NX_ERROR_STATE;
        }
        fp_pick *batch=malloc(r->pick_count*sizeof(*batch));if(!batch)return NX_ERROR_MEMORY;
        if(o->modal_built){nx_result result=nx_overlay_close(ctx);if(result!=NX_OK){free(batch);return result;}}
        size_t count;nx_result result=fp_reply_take_many(fp_reply_get(&p->observation),batch,r->pick_count,&count);if(result!=NX_OK){free(batch);return result;}
        release_batch(o);if(o->directory>=0)close(o->directory);if(o->witness>=0&&o->witness!=o->directory)close(o->witness);
        o->directory=o->witness=-1;o->batch=batch;o->batch_count=count;o->batch_labels=p->observation;p->observation=(nx_sim_message){0};
        (void)snprintf(o->label,sizeof o->label,o->german?"%zu Dateien ausgewählt":"%zu files selected",count);o->directories=0;o->save_kind=FP_SAVE_NONE;o->leaf[0]=0;++o->selections;clear(o);
        fp_picker_destroy(p);o->active=o->modal_built=0;o->restore=o->trigger;return NX_OK;
    }
    if(selected){
        if(o->selections==UINT64_MAX)return NX_ERROR_LIMIT;
        if(!r||r->result!=AE_FILES_OK||(p->operation!=FP_PICK&&p->operation!=FP_PICK_DIRECTORY)||
           r->directory<4||r->witness<4||fstat(r->directory,&d)||!S_ISDIR(d.st_mode)||fstat(r->witness,&w))return NX_ERROR_STATE;
        if(p->directories){
            if(p->operation!=FP_PICK_DIRECTORY||!S_ISDIR(w.st_mode)||d.st_dev!=w.st_dev||d.st_ino!=w.st_ino||strcmp(r->leaf,"."))return NX_ERROR_STATE;
            label=r->ancestors[r->depth];
        }else{
            if(p->operation!=FP_PICK||!S_ISREG(w.st_mode))return NX_ERROR_STATE;
            for(size_t i=0;i<r->count;++i)if(!strcmp(r->leaf,r->rows[i].entry.name)){label=r->rows[i].entry.label;break;}
        }
        if(!label||strlen(label)>AE_FILES_LABEL)return NX_ERROR_STATE;
    }
    if(o->modal_built){nx_result result=nx_overlay_close(ctx);if(result!=NX_OK)return result;}
    if(selected){
        int directory=-1,witness=-1;char leaf[AE_FILES_NAME+1];
        nx_result result=fp_reply_take(fp_reply_get(&p->observation),&directory,&witness,leaf);if(result!=NX_OK)return result;
        release_batch(o);
        if(o->directory>=0)close(o->directory);
        if(o->witness>=0&&o->witness!=o->directory)close(o->witness);
        o->directory=directory;o->witness=witness;memcpy(o->label,label,strlen(label)+1);o->directories=p->directories;o->save_kind=FP_SAVE_NONE;o->leaf[0]=0;++o->selections;clear(o);
    }
    fp_picker_destroy(p);o->active=0;o->modal_built=0;o->restore=o->trigger;return NX_OK;
}
static float height(const fp_owner *o,size_t i){float h=(float)nx_text_layout_height(o->labels[i])+16;return fmaxf(h,o->text_size+16);}
static nx_result layouts(fp_owner *o,float width){
    if(o->layout_width==width&&o->layout_size==o->text_size)return NX_OK;
    clear(o);const char *texts[]={word(o,"Choose file…","Datei auswählen…"),word(o,"Choose folder…","Ordner auswählen…"),
        word(o,"Close","Schließen"),word(o,"Owner note","Notiz"),o->save_kind?word(o,"Save destination selected","Speicherziel ausgewählt"):o->selections?word(o,"Selected item","Ausgewählter Eintrag"):word(o,"Nothing selected","Noch keine Auswahl"),o->label,
        word(o,"Choose files…","Dateien auswählen…"),word(o,"Previous results","Vorherige Ergebnisse"),word(o,"Next results","Nächste Ergebnisse"),word(o,"Save as…","Speichern unter…")};
    for(size_t i=0;i<10;++i)if(!nx_example_small_text_layout(o->fonts,texts[i],strlen(texts[i]),o->text_size,width-24,&o->labels[i])){clear(o);return NX_ERROR_STATE;}
    for(size_t i=0;i<FP_PAGE_ROWS&&o->batch_page*FP_PAGE_ROWS+i<o->batch_count;++i){const fp_row *row=fp_owner_batch_row(o,o->batch_page*FP_PAGE_ROWS+i);
        if(!row||!nx_example_small_text_layout(o->fonts,row->entry.label,strlen(row->entry.label),o->text_size,width-24,&o->rows[i])){clear(o);return NX_ERROR_STATE;}}
    o->layout_width=width;o->layout_size=o->text_size;return NX_OK;
}
static nx_result button_state(fp_owner *o,nx_context *ctx,uint64_t id,uint64_t parent,nx_rect bounds,size_t i,const char *name,int disabled,int *clicked){
    nx_control_desc d={0};d.id=id;d.parent_id=parent;d.bounds=bounds;d.name=span(name);d.label_layout=o->labels[i];
    d.label_origin=(nx_point){bounds.x+12,bounds.y+8};d.glyph_cache_bytes=1024u*1024u;d.disabled=disabled;nx_interaction_state s;
    nx_result result=nx_button(ctx,&d,&s);*clicked=result==NX_OK&&s.clicked;return result;
}
static nx_result button(fp_owner *o,nx_context *ctx,uint64_t id,uint64_t parent,nx_rect bounds,size_t i,const char *name,int *clicked){return button_state(o,ctx,id,parent,bounds,i,name,0,clicked);}
static nx_result label(fp_owner *o,nx_context *ctx,uint64_t id,nx_rect bounds,size_t i,const char *text){
    nx_semantic_desc d={0};d.id=id;d.parent_id=FP_OWNER_SCROLL;d.bounds=bounds;d.role=NX_ROLE_TEXT;d.name=span(text);
    nx_result result=nx_semantics_add(ctx,&d,NULL);if(result!=NX_OK)return result;
    nx_color color;result=nx_theme_color(ctx,NX_COLOR_TEXT,&color);if(result!=NX_OK)return result;
    return nx_draw_text_layout(ctx,o->labels[i],(nx_point){bounds.x+12,bounds.y+8},color,1024u*1024u);
}
#define TRY(x) do{nx_result result=(x);if(result!=NX_OK)return result;}while(0)
nx_result fp_owner_build(fp_owner *o,fp_picker *p,nx_context *ctx,nx_window *window,float width,float h,fp_intent *intent,int *redraw,int *open_mode,int *quit){
    if(!o||!p||!ctx||!intent||!redraw||!open_mode||!quit||width<1||h<1)return NX_ERROR_ARGUMENT;
    *redraw=0;*open_mode=0;*quit=0;
    if(width>=160&&h>=160){
        float w=width-32;TRY(layouts(o,w));float footer=height(o,2),body=h-footer-32;
        if(body<24)body=24;
        float total=height(o,0)+height(o,1)+height(o,6)+height(o,9)+8+height(o,3)+o->text_size+20+height(o,4)+(o->selections?height(o,5)+8:0)+48;
        if(o->batch_count){total+=height(o,7)+height(o,8)+16;for(size_t i=0;i<FP_PAGE_ROWS&&o->rows[i];++i)total+=(float)nx_text_layout_height(o->rows[i])+20;}
        nx_scroll_area_desc scroll={0};scroll.id=FP_OWNER_SCROLL;scroll.parent_id=1;scroll.thumb_y_id=FP_OWNER_THUMB;
        scroll.name=span(word(o,"Selection workspace","Auswahlbereich"));scroll.viewport=(nx_rect){16,8,w,body};scroll.content_size=(nx_size){w,total};scroll.axes=NX_SCROLL_Y;scroll.wheel_step=o->text_size*3;
        float offset=0;nx_scroll_info area;TRY(nx_scroll_area_begin(ctx,&scroll,&offset,&o->scroll_y,&area));float y=area.viewport.y-o->scroll_y;int clicked;
        for(size_t i=0;i<2;++i){
            float row_h=height(o,i);TRY(button(o,ctx,FP_OWNER_FILE+i,FP_OWNER_SCROLL,(nx_rect){16,y,w,row_h},i,i?word(o,"Choose folder…","Ordner auswählen…"):word(o,"Choose file…","Datei auswählen…"),&clicked));
            if(clicked&&!o->active)*open_mode=i?2:1;
            y+=row_h+8;
        }
        float multiple_h=height(o,6);TRY(button(o,ctx,FP_OWNER_MULTIPLE,FP_OWNER_SCROLL,(nx_rect){16,y,w,multiple_h},6,word(o,"Choose files…","Dateien auswählen…"),&clicked));
        if(clicked&&!o->active)*open_mode=3;
        y+=multiple_h+8;
        float save_h=height(o,9);TRY(button(o,ctx,FP_OWNER_SAVE,FP_OWNER_SCROLL,(nx_rect){16,y,w,save_h},9,word(o,"Save as…","Speichern unter…"),&clicked));
        if(clicked&&!o->active)*open_mode=4;
        y+=save_h+8;
        float note_h=height(o,3);TRY(label(o,ctx,FP_OWNER_NOTE_LABEL,(nx_rect){16,y,w,note_h},3,word(o,"Owner note","Notiz")));y+=note_h+4;
        nx_text_field_desc note={0};note.id=FP_OWNER_NOTE;note.parent_id=FP_OWNER_SCROLL;note.bounds=(nx_rect){16,y,w,o->text_size+20};
        note.name=span(word(o,"Owner note","Notiz"));note.fonts=o->fonts;note.pixels_per_em=o->text_size;note.glyph_cache_bytes=1024u*1024u;note.ime_window=window;
        if(window)TRY(nx_text_clipboard_for_window(window,&note.clipboard));
        nx_interaction_state edited;int valid,submitted;TRY(nx_text_field_widget(ctx,&note,o->note,&edited,&valid,&submitted));y+=note.bounds.height+8;
        float result_h=height(o,4);TRY(label(o,ctx,FP_OWNER_RESULT,(nx_rect){16,y,w,result_h},4,o->selections?word(o,"Selected item","Ausgewählter Eintrag"):word(o,"Nothing selected","Noch keine Auswahl")));y+=result_h+4;
        if(o->selections){TRY(label(o,ctx,FP_OWNER_RESULT+10,(nx_rect){16,y,w,height(o,5)},5,o->label));y+=height(o,5)+8;}
        if(o->batch_count){
            for(size_t i=0;i<2;++i){float row_h=height(o,7+i);uint64_t id=i?FP_OWNER_NEXT:FP_OWNER_PREVIOUS;int enabled=i?(o->batch_page+1)*FP_PAGE_ROWS<o->batch_count:o->batch_page!=0;
                TRY(button_state(o,ctx,id,FP_OWNER_SCROLL,(nx_rect){16,y,w,row_h},7+i,i?word(o,"Next results","Nächste Ergebnisse"):word(o,"Previous results","Vorherige Ergebnisse"),!enabled,&clicked));
                if(clicked&&!o->active){o->batch_page=i?o->batch_page+1:o->batch_page-1;o->restore=!o->batch_page?FP_OWNER_NEXT:(o->batch_page+1)*FP_PAGE_ROWS>=o->batch_count?FP_OWNER_PREVIOUS:id;o->rebind=1;clear(o);*redraw=1;}
                y+=row_h+8;
                if(*redraw)break;
            }
            if(!*redraw)for(size_t i=0;i<FP_PAGE_ROWS&&o->rows[i];++i){const fp_row *row=fp_owner_batch_row(o,o->batch_page*FP_PAGE_ROWS+i);float row_h=(float)nx_text_layout_height(o->rows[i])+16;
                nx_semantic_desc d={0};d.id=FP_OWNER_BATCH_ROWS+o->batch_page*FP_PAGE_ROWS+i;d.parent_id=FP_OWNER_SCROLL;d.bounds=(nx_rect){16,y,w,row_h};d.role=NX_ROLE_TEXT;d.name=span(row->entry.label);TRY(nx_semantics_add(ctx,&d,NULL));
                nx_color color;TRY(nx_theme_color(ctx,NX_COLOR_TEXT,&color));TRY(nx_draw_text_layout(ctx,o->rows[i],(nx_point){28,y+8},color,1024u*1024u));y+=row_h+4;}
        }
        TRY(nx_scroll_area_end(ctx));TRY(button(o,ctx,FP_OWNER_CLOSE,1,(nx_rect){16,h-footer-12,w,footer},2,word(o,"Close","Schließen"),&clicked));if(clicked&&!o->active)*quit=1;
    }else{
        nx_action_desc close={0};close.variant=NX_ACTION_SECONDARY;close.control.id=FP_OWNER_CLOSE;close.control.parent_id=1;
        close.control.name=span(word(o,"Close","Schließen"));close.control.bounds=(nx_rect){0,0,width,h};close.icon=NX_ICON_CLOSE;
        float side=fminf(20,fminf(width,h)*0.6f);close.icon_bounds=(nx_rect){(width-side)/2,(h-side)/2,side,side};nx_interaction_state s;
        TRY(nx_action(ctx,&close,&s));if(s.clicked&&!o->active)*quit=1;
    }
    if(o->active){
        if(!o->modal_built)(void)nx_focus_set(ctx,o->trigger);
        nx_overlay_desc modal={0};modal.panel.id=FP_OWNER_MODAL;modal.panel.parent_id=1;modal.panel.bounds=(nx_rect){0,0,width,h};
        modal.panel.name=span(p->save?word(o,"Save as","Speichern unter"):p->directories?word(o,"Choose a folder","Ordner auswählen"):p->multiple?word(o,"Choose files","Dateien auswählen"):word(o,"Choose a file","Datei auswählen"));modal.backdrop_id=FP_OWNER_BACKDROP;
        int dismissed;TRY(nx_modal_begin(ctx,&modal,&dismissed));o->modal_built=1;
        TRY(fp_picker_build(p,ctx,window,width,h,intent,redraw));TRY(nx_overlay_end(ctx));
    }else if(!o->rebind&&o->restore&&nx_focus_set(ctx,o->restore)==NX_OK){o->restore=0;*redraw=1;}
    return NX_OK;
}
int fp_owner_reveal(fp_owner *o,const nx_context *ctx,float h){
    if(!o||!ctx||o->active)return 0;
    uint64_t focus=nx_focus_id(ctx);if(o->revealed_focus==focus&&o->revealed_height==h)return 0;
    o->revealed_focus=focus;o->revealed_height=h;uint32_t a,b;nx_semantic_view area,node;
    if(nx_semantics_find(ctx,FP_OWNER_SCROLL,&a)!=NX_OK||nx_semantics_get(ctx,a,&area)!=NX_OK||nx_semantics_find(ctx,focus,&b)!=NX_OK||nx_semantics_get(ctx,b,&node)!=NX_OK||node.parent_id!=FP_OWNER_SCROLL)return 0;
    float before=o->scroll_y;
    if(node.bounds.y<area.bounds.y)o->scroll_y-=area.bounds.y-node.bounds.y;
    else if(node.bounds.y+node.bounds.height>area.bounds.y+area.bounds.height)o->scroll_y+=node.bounds.y+node.bounds.height-area.bounds.y-area.bounds.height;
    if(o->scroll_y<0)o->scroll_y=0;
    return before!=o->scroll_y;
}
int fp_owner_key(fp_owner *o,const nx_context *ctx,const nx_event *event){
    if(!o||!ctx||!event||o->active||event->type!=NX_EVENT_KEY_DOWN||event->modifiers||
       (event->key!=NX_KEY_PAGE_UP&&event->key!=NX_KEY_PAGE_DOWN)||nx_focus_id(ctx)==FP_OWNER_NOTE)return 0;
    uint32_t index;nx_semantic_view area;
    if(nx_semantics_find(ctx,FP_OWNER_SCROLL,&index)!=NX_OK||nx_semantics_get(ctx,index,&area)!=NX_OK)return 0;
    float step=area.bounds.height*0.5f;
    o->scroll_y=fmaxf(0,o->scroll_y+(event->key==NX_KEY_PAGE_DOWN?step:-step));return 1;
}
