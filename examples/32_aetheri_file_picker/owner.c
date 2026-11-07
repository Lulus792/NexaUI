#include "owner.h"
#include "../common/text_wrap.h"
#include <math.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
static nx_utf8_span span(const char *s){return (nx_utf8_span){s,strlen(s)};}
static const char *word(const fp_owner *o,const char *en,const char *de){return o->german?de:en;}
static void clear(fp_owner *o){for(size_t i=0;i<6;++i){nx_text_layout_destroy(o->labels[i]);o->labels[i]=NULL;}o->layout_width=0;o->revealed_focus=0;}
nx_result fp_owner_init(fp_owner *o,nx_font_collection *fonts){
    if(!o||!fonts)return NX_ERROR_ARGUMENT;
    *o=(fp_owner){0};o->fonts=fonts;o->directory=o->witness=-1;o->text_size=16;o->restore=FP_OWNER_FILE;
    return nx_text_field_create("",0,4096,&o->note);
}
void fp_owner_destroy(fp_owner *o){
    if(!o||!o->fonts)return;
    clear(o);nx_text_field_destroy(o->note);
    if(o->directory>=0)close(o->directory);
    if(o->witness>=0&&o->witness!=o->directory)close(o->witness);
    *o=(fp_owner){0};o->directory=o->witness=-1;
}
nx_result fp_owner_open(fp_owner *o,fp_picker *p,fp_worker *worker,int directories){
    if(!o||!p||!worker||(directories!=0&&directories!=1))return NX_ERROR_ARGUMENT;
    if(o->active)return NX_ERROR_STATE;
    if(o->epoch==UINT64_MAX)return NX_ERROR_LIMIT;
    nx_result result=fp_picker_init(p,worker,o->fonts,++o->epoch);
    if(result!=NX_OK)return result;
    p->german=o->german;p->text_size=o->text_size;p->directories=directories;p->parent_id=FP_OWNER_MODAL;
    o->active=1;o->modal_built=0;o->trigger=directories?FP_OWNER_DIRECTORY:FP_OWNER_FILE;return NX_OK;
}
nx_result fp_owner_finish(fp_owner *o,fp_picker *p,nx_context *ctx,int selected){
    if(!o||!p||!ctx||(selected!=0&&selected!=1))return NX_ERROR_ARGUMENT;
    if(!o->active)return NX_ERROR_STATE;
    const fp_reply *r=fp_picker_snapshot(p);const char *label=NULL;struct stat d,w;
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
        if(o->directory>=0)close(o->directory);
        if(o->witness>=0&&o->witness!=o->directory)close(o->witness);
        o->directory=directory;o->witness=witness;memcpy(o->label,label,strlen(label)+1);o->directories=p->directories;++o->selections;clear(o);
    }
    fp_picker_destroy(p);o->active=0;o->modal_built=0;o->restore=o->trigger;return NX_OK;
}
static float height(const fp_owner *o,size_t i){float h=(float)nx_text_layout_height(o->labels[i])+16;return fmaxf(h,o->text_size+16);}
static nx_result layouts(fp_owner *o,float width){
    if(o->layout_width==width&&o->layout_size==o->text_size)return NX_OK;
    clear(o);const char *texts[]={word(o,"Choose file…","Datei auswählen…"),word(o,"Choose folder…","Ordner auswählen…"),
        word(o,"Close","Schließen"),word(o,"Owner note","Notiz"),o->selections?word(o,"Selected item","Ausgewählter Eintrag"):word(o,"Nothing selected","Noch keine Auswahl"),o->label};
    for(size_t i=0;i<6;++i)if(!nx_example_small_text_layout(o->fonts,texts[i],strlen(texts[i]),o->text_size,width-24,&o->labels[i])){clear(o);return NX_ERROR_STATE;}
    o->layout_width=width;o->layout_size=o->text_size;return NX_OK;
}
static nx_result button(fp_owner *o,nx_context *ctx,uint64_t id,uint64_t parent,nx_rect bounds,size_t i,const char *name,int *clicked){
    nx_control_desc d={0};d.id=id;d.parent_id=parent;d.bounds=bounds;d.name=span(name);d.label_layout=o->labels[i];
    d.label_origin=(nx_point){bounds.x+12,bounds.y+8};d.glyph_cache_bytes=1024u*1024u;nx_interaction_state s;
    nx_result result=nx_button(ctx,&d,&s);*clicked=result==NX_OK&&s.clicked;return result;
}
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
        float total=height(o,0)+height(o,1)+height(o,3)+o->text_size+20+height(o,4)+(o->selections?height(o,5)+8:0)+40;
        nx_scroll_area_desc scroll={0};scroll.id=FP_OWNER_SCROLL;scroll.parent_id=1;scroll.thumb_y_id=FP_OWNER_THUMB;
        scroll.name=span(word(o,"Selection workspace","Auswahlbereich"));scroll.viewport=(nx_rect){16,8,w,body};scroll.content_size=(nx_size){w,total};scroll.axes=NX_SCROLL_Y;scroll.wheel_step=o->text_size*3;
        float offset=0;nx_scroll_info area;TRY(nx_scroll_area_begin(ctx,&scroll,&offset,&o->scroll_y,&area));float y=area.viewport.y-o->scroll_y;int clicked;
        for(size_t i=0;i<2;++i){
            float row_h=height(o,i);TRY(button(o,ctx,FP_OWNER_FILE+i,FP_OWNER_SCROLL,(nx_rect){16,y,w,row_h},i,i?word(o,"Choose folder…","Ordner auswählen…"):word(o,"Choose file…","Datei auswählen…"),&clicked));
            if(clicked&&!o->active)*open_mode=i?2:1;
            y+=row_h+8;
        }
        float note_h=height(o,3);TRY(label(o,ctx,FP_OWNER_NOTE_LABEL,(nx_rect){16,y,w,note_h},3,word(o,"Owner note","Notiz")));y+=note_h+4;
        nx_text_field_desc note={0};note.id=FP_OWNER_NOTE;note.parent_id=FP_OWNER_SCROLL;note.bounds=(nx_rect){16,y,w,o->text_size+20};
        note.name=span(word(o,"Owner note","Notiz"));note.fonts=o->fonts;note.pixels_per_em=o->text_size;note.glyph_cache_bytes=1024u*1024u;note.ime_window=window;
        if(window)TRY(nx_text_clipboard_for_window(window,&note.clipboard));
        nx_interaction_state edited;int valid,submitted;TRY(nx_text_field_widget(ctx,&note,o->note,&edited,&valid,&submitted));y+=note.bounds.height+8;
        float result_h=height(o,4);TRY(label(o,ctx,FP_OWNER_RESULT,(nx_rect){16,y,w,result_h},4,o->selections?word(o,"Selected item","Ausgewählter Eintrag"):word(o,"Nothing selected","Noch keine Auswahl")));y+=result_h+4;
        if(o->selections)TRY(label(o,ctx,FP_OWNER_RESULT+10,(nx_rect){16,y,w,height(o,5)},5,o->label));
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
        modal.panel.name=span(p->directories?word(o,"Choose a folder","Ordner auswählen"):word(o,"Choose a file","Datei auswählen"));modal.backdrop_id=FP_OWNER_BACKDROP;
        int dismissed;TRY(nx_modal_begin(ctx,&modal,&dismissed));o->modal_built=1;
        TRY(fp_picker_build(p,ctx,window,width,h,intent,redraw));TRY(nx_overlay_end(ctx));
    }else if(o->restore&&nx_focus_set(ctx,o->restore)==NX_OK){o->restore=0;*redraw=1;}
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
