#include "view.h"
#include <stdlib.h>
#include <string.h>

struct aetheri_desktop_view {
    struct ae_desktop_model *model;
    nx_context *context;
    nx_font *font;
    nx_font_collection *fonts;
    nx_text_field *query;
    nx_window *window;
    float scroll_x,scroll_y;
    float revealed_width,revealed_height;
    uint64_t model_revision;
    int host_ime;
};
void aetheri_desktop_view_destroy(aetheri_desktop_view *v) {
    if(!v)return;
    nx_context_destroy(v->context);nx_text_field_destroy(v->query);nx_font_collection_destroy(v->fonts);nx_font_close(v->font);free(v);
}
int aetheri_desktop_view_create(struct ae_desktop_model *model,const void *font,size_t length,aetheri_desktop_view **out) {
    if(!out)return 0;
    *out=NULL;
    if(!model || !font || !length)return 0;
    aetheri_desktop_view *v=calloc(1,sizeof(*v));if(!v)return 0;v->model=model;
    if(nx_context_create(NULL,&v->context)!=NX_OK || nx_font_open(font,length,NULL,&v->font)!=NX_OK ||
       nx_font_collection_create(NULL,&v->fonts)!=NX_OK || nx_font_collection_add(v->fonts,v->font)!=NX_OK ||
       nx_text_field_create("",0,4096,&v->query)!=NX_OK) {
        aetheri_desktop_view_destroy(v);return 0;
    }
    *out=v;return 1;
}
nx_context *aetheri_desktop_view_context(aetheri_desktop_view *v) {return v?v->context:NULL;}
nx_result aetheri_desktop_view_set_window(aetheri_desktop_view *v,nx_window *window) {
    if(!v)return NX_ERROR_ARGUMENT;
    if(window && v->host_ime)return NX_ERROR_STATE;
    v->window=window;return NX_OK;
}
nx_result aetheri_desktop_view_set_host_ime(aetheri_desktop_view *v,int enabled) {
    if(!v || (enabled!=0 && enabled!=1))return NX_ERROR_ARGUMENT;
    if(enabled && v->window)return NX_ERROR_STATE;
    nx_result result=nx_text_field_set_host_ime(v->query,enabled);
    if(result==NX_OK)v->host_ime=enabled;
    return result;
}
nx_result aetheri_desktop_view_host_ime_revision(aetheri_desktop_view *v,uint64_t *out_revision) {
    if(!v){if(out_revision)*out_revision=0;return NX_ERROR_ARGUMENT;}
    return nx_text_field_host_ime_revision(v->context,20,v->query,out_revision);
}
nx_result aetheri_desktop_view_host_ime_preedit(aetheri_desktop_view *v,uint64_t revision,
    const char *text,size_t length,int32_t begin,int32_t end) {
    if(!v)return NX_ERROR_ARGUMENT;
    return nx_text_field_host_ime_preedit(v->context,20,v->query,revision,text,length,begin,end);
}
nx_result aetheri_desktop_view_host_ime_caret(aetheri_desktop_view *v,nx_rect *out_rect) {
    if(!v){if(out_rect)*out_rect=(nx_rect){0};return NX_ERROR_ARGUMENT;}
    return nx_text_field_host_ime_caret(v->context,20,v->query,out_rect);
}
nx_result aetheri_desktop_view_host_ime_surrounding(aetheri_desktop_view *v,uint64_t revision,
    char *bytes,size_t capacity,size_t *length,size_t *cursor,size_t *anchor) {
    if(!length || !cursor || !anchor)return NX_ERROR_ARGUMENT;
    *length=*cursor=*anchor=0;
    if(!v || !bytes || !capacity)return NX_ERROR_ARGUMENT;
    uint64_t current=0;nx_result r=aetheri_desktop_view_host_ime_revision(v,&current);
    if(r!=NX_OK)return r;
    if(!revision || revision!=current)return NX_ERROR_STATE;
    nx_utf8_span text={0};size_t a=0,c=0;
    r=nx_text_field_text(v->query,&text);if(r!=NX_OK)return r;
    r=nx_text_field_selection(v->query,&a,&c);if(r!=NX_OK)return r;
    if(text.length>=capacity)return NX_ERROR_LIMIT;
    if(text.length)memcpy(bytes,text.data,text.length);
    bytes[text.length]=0;*length=text.length;*cursor=c;*anchor=a;return NX_OK;
}
nx_result aetheri_desktop_view_surface_focus(aetheri_desktop_view *v,int focused) {
    if(!v || (focused!=0 && focused!=1))return NX_ERROR_ARGUMENT;
    nx_event event={0};event.type=NX_EVENT_FOCUS;event.focused=focused;
    return nx_context_queue_event(v->context,&event);
}
nx_result aetheri_desktop_view_queue_event(aetheri_desktop_view *v,const nx_event *event) {
    if(!v || !event)return NX_ERROR_ARGUMENT;
    nx_event mapped=*event;
    if(v->model->search && nx_focus_id(v->context)==20 &&
       (event->type==NX_EVENT_KEY_DOWN || event->type==NX_EVENT_KEY_UP) &&
       event->key==NX_KEY_TAB && !(event->modifiers&~NX_MOD_SHIFT)) {
        mapped.key=(event->modifiers&NX_MOD_SHIFT)?NX_KEY_UP:NX_KEY_DOWN;
        mapped.modifiers&=~NX_MOD_SHIFT;
    }
    return nx_context_queue_event(v->context,&mapped);
}
static int valid_query(nx_utf8_span text,void *user) {(void)user;return text.length<AE_DESKTOP_QUERY;}
int aetheri_desktop_view_frame(aetheri_desktop_view *v,float width,float height,uint64_t revision,aetheri_desktop_intent *intent) {
    if(!intent)return 0;
    *intent=(aetheri_desktop_intent){0};
    if(!v || !revision || revision<v->model_revision || width<320 || height<84 || (v->model->search && height<240) ||
       v->model->count>AE_DESKTOP_WINDOWS || !memchr(v->model->query,0,sizeof(v->model->query)))return 0;
    aetheri_desktop_intent pending={0};
    const char *labels[]={v->model->german?"Suchen":"Search",v->model->german?"System":"Settings","Terminal"};
    nx_text_layout *layouts[3]={0},*result_layouts[AE_DESKTOP_RESULTS]={0};int okay=0,guarded=0;
    struct ae_desktop_result results[AE_DESKTOP_RESULTS];size_t result_count=0;
    float result_y[AE_DESKTOP_RESULTS]={0},result_height[AE_DESKTOP_RESULTS]={0},content_height=0;
    for(int i=0;i<3;++i)
        if(nx_text_layout_create(v->fonts,labels[i],strlen(labels[i]),NX_BIDI_LTR,16,width,&layouts[i])!=NX_OK)goto done;
    if(v->model->search) {
        nx_utf8_span draft;
        if(nx_text_field_text(v->query,&draft)!=NX_OK)goto done;
        size_t query_length=strlen(v->model->query);
        if((!v->model_revision || revision!=v->model_revision) &&
           (draft.length!=query_length || (query_length && memcmp(draft.data,v->model->query,query_length))))
            if(nx_text_field_set_text(v->query,v->model->query,query_length)!=NX_OK)goto done;
        result_count=ae_desktop_model_results(v->model,results);
        if(result_count>AE_DESKTOP_RESULTS)goto done;
        for(size_t i=0;i<result_count;++i) {
            if(nx_text_layout_create(v->fonts,results[i].label,strlen(results[i].label),NX_BIDI_LTR,16,width-64,&result_layouts[i])!=NX_OK)goto done;
            float h=(float)nx_text_layout_height(result_layouts[i])+16;
            if(h<44)h=44;
            result_y[i]=content_height;result_height[i]=h;content_height+=h+8;
        }
    }
    if(v->model_revision && revision!=v->model_revision) {
        /* A captured press belongs to the previous model, even if its
           semantic ID is immediately reused. Cancel after any queued input. */
        nx_event cancel={0};cancel.type=NX_EVENT_POINTER_CANCEL;
        if(nx_context_queue_event(v->context,&cancel)!=NX_OK)goto done;
    }
    if(v->model->search && v->model_revision && revision!=v->model_revision) {
        if(nx_text_field_set_read_only(v->query,1)!=NX_OK)goto done;
        guarded=1;
    }
    nx_theme theme;
    if(nx_theme_preset(v->model->contrast?NX_THEME_HIGH_CONTRAST:NX_THEME_DARK,&theme)!=NX_OK ||
       nx_context_set_theme(v->context,&theme)!=NX_OK || nx_begin_frame(v->context,width,height,0)!=NX_OK)goto done;
    nx_semantic_desc root={0};root.id=1;root.role=NX_ROLE_WINDOW;root.bounds=(nx_rect){0,0,width,height};
    root.name=(nx_utf8_span){"Aetheri Desktop",15};
    if(nx_semantics_add(v->context,&root,NULL)!=NX_OK)goto done;
    float button_width=(width-48)/3;
    for(int i=0;i<3;++i) {
        nx_control_desc d={0};d.id=(uint64_t)(10+i);d.parent_id=1;
        d.bounds=(nx_rect){8+(float)i*(button_width+8),height-70,button_width,44};
        d.name=(nx_utf8_span){labels[i],strlen(labels[i])};d.label_layout=layouts[i];
        d.label_origin=(nx_point){d.bounds.x+8,d.bounds.y+8};d.glyph_cache_bytes=1024u*1024u;
        d.disabled=i>0 && v->model->pending[i==1?AE_DESKTOP_SETTINGS:AE_DESKTOP_TERMINAL];
        nx_interaction_state state;if(nx_button(v->context,&d,&state)!=NX_OK)goto done;
        if(state.clicked) {
            pending.kind=i?AETHERI_DESKTOP_LAUNCH:AETHERI_DESKTOP_SEARCH;
            pending.app=i==1?AE_DESKTOP_SETTINGS:i==2?AE_DESKTOP_TERMINAL:AE_DESKTOP_APP_NONE;
        }
    }
    if(v->model->search) {
        nx_text_field_desc query={0};query.id=20;query.parent_id=1;
        query.bounds=(nx_rect){16,16,width-32,44};
        const char *query_name=v->model->german?"Suche":"Search";
        query.name=(nx_utf8_span){query_name,strlen(query_name)};query.fonts=v->fonts;
        query.pixels_per_em=16;query.glyph_cache_bytes=1024u*1024u;query.validate=valid_query;
        query.ime_window=v->window;
        if(v->window && nx_text_clipboard_for_window(v->window,&query.clipboard)!=NX_OK)goto done;
        nx_interaction_state query_state;int valid=0,submitted=0;
        if(nx_text_field_widget(v->context,&query,v->query,&query_state,&valid,&submitted)!=NX_OK)goto done;
        size_t selected=result_count;int moved=0;
        for(size_t i=0;i<result_count;++i)
            if(results[i].window==v->model->selected && results[i].app==v->model->selected_app) {selected=i;break;}
        if(query_state.focused && result_count) {
            const nx_event *events=NULL;size_t event_count=0;
            if(nx_frame_events(v->context,&events,&event_count)!=NX_OK)goto done;
            for(size_t i=0;i<event_count;++i) {
                if(events[i].type!=NX_EVENT_KEY_DOWN || events[i].modifiers)continue;
                if(events[i].key!=NX_KEY_DOWN && events[i].key!=NX_KEY_UP)continue;
                if(selected==result_count)selected=0;
                else if(events[i].key==NX_KEY_DOWN)selected=(selected+1)%result_count;
                else selected=selected?selected-1:result_count-1;
                moved=1;
            }
            if(selected<result_count && (moved || submitted)) {
                struct ae_desktop_result choice=results[selected];
                if(moved)pending.kind=AETHERI_DESKTOP_SELECT;
                if(submitted && (!choice.app || !v->model->pending[choice.app]))
                    pending.kind=choice.window?AETHERI_DESKTOP_ACTIVATE:AETHERI_DESKTOP_LAUNCH;
                if(pending.kind!=AETHERI_DESKTOP_NONE) {pending.window=choice.window;pending.app=choice.app;}
            }
        }
        nx_scroll_area_desc scroll={0};scroll.id=30;scroll.parent_id=1;
        const char *heading=v->model->german?"Suchergebnisse":"Search results";
        scroll.name=(nx_utf8_span){heading,strlen(heading)};
        scroll.viewport=(nx_rect){16,68,width-32,height-156};
        scroll.content_size=(nx_size){width-32,content_height};scroll.axes=NX_SCROLL_Y;
        if(selected<result_count && (revision!=v->model_revision || width!=v->revealed_width || height!=v->revealed_height)) {
            float top=result_y[selected],bottom=top+result_height[selected];
            if(top<v->scroll_y)v->scroll_y=top;
            else if(bottom>v->scroll_y+scroll.viewport.height)v->scroll_y=bottom-scroll.viewport.height;
            if(v->scroll_y<0)v->scroll_y=0;
        }
        v->revealed_width=width;v->revealed_height=height;
        nx_scroll_info info;
        if(nx_scroll_area_begin(v->context,&scroll,&v->scroll_x,&v->scroll_y,&info)!=NX_OK)goto done;
        nx_semantic_desc list={0};list.id=31;list.parent_id=30;list.role=NX_ROLE_LIST;
        list.name=scroll.name;list.bounds=scroll.viewport;
        if(nx_semantics_add(v->context,&list,NULL)!=NX_OK)goto done;
        nx_color surface,hover,selected_color,foreground,selected_text,disabled_text;
        if(nx_theme_color(v->context,NX_COLOR_SURFACE_RAISED,&surface)!=NX_OK ||
           nx_theme_color(v->context,NX_COLOR_HOVER,&hover)!=NX_OK ||
           nx_theme_color(v->context,NX_COLOR_SELECTION,&selected_color)!=NX_OK ||
           nx_theme_color(v->context,NX_COLOR_TEXT,&foreground)!=NX_OK ||
           nx_theme_color(v->context,NX_COLOR_SELECTION_TEXT,&selected_text)!=NX_OK ||
           nx_theme_color(v->context,NX_COLOR_DISABLED_TEXT,&disabled_text)!=NX_OK)goto done;
        for(size_t i=0;i<result_count;++i) {
            nx_widget_desc d={0};
            d.semantic.id=results[i].window ? UINT64_C(0x100000000)+results[i].window : UINT64_C(0x200000000)+(uint64_t)results[i].app;
            d.semantic.parent_id=31;d.semantic.role=NX_ROLE_LIST_ITEM;
            d.semantic.bounds=(nx_rect){24,scroll.viewport.y-v->scroll_y+result_y[i],width-48,result_height[i]};
            d.semantic.name=(nx_utf8_span){results[i].label,strlen(results[i].label)};
            int is_selected=results[i].window==v->model->selected && results[i].app==v->model->selected_app;
            int disabled=results[i].app && v->model->pending[results[i].app];
            d.semantic.state=(is_selected?NX_SEM_SELECTED:0u)|(disabled?NX_SEM_DISABLED:0u);
            d.semantic.actions=NX_ACTION_PRESS|NX_ACTION_FOCUS;
            d.flags=NX_WIDGET_INTERACTIVE|NX_WIDGET_FOCUSABLE;
            nx_interaction_state state;if(nx_widget_register(v->context,&d,&state)!=NX_OK)goto done;
            nx_color fill=is_selected?selected_color:state.hovered?hover:surface;
            nx_color text=disabled?disabled_text:is_selected?selected_text:foreground;
            if(nx_fill_rect(v->context,d.semantic.bounds,fill)!=NX_OK ||
               nx_draw_text_layout(v->context,result_layouts[i],(nx_point){d.semantic.bounds.x+8,d.semantic.bounds.y+8},text,1024u*1024u)!=NX_OK)goto done;
            if(state.clicked) {
                pending.kind=results[i].window?AETHERI_DESKTOP_ACTIVATE:AETHERI_DESKTOP_LAUNCH;
                pending.window=results[i].window;pending.app=results[i].app;
            }
        }
        if(nx_scroll_area_end(v->context)!=NX_OK)goto done;
        nx_utf8_span draft;if(nx_text_field_text(v->query,&draft)!=NX_OK)goto done;
        size_t query_length=strlen(v->model->query);
        if(valid && (draft.length!=query_length || (query_length && memcmp(draft.data,v->model->query,query_length)))) {
            pending=(aetheri_desktop_intent){0};pending.kind=AETHERI_DESKTOP_QUERY;
            if(draft.length)memcpy(pending.query,draft.data,draft.length);
            pending.query[draft.length]=0;
        }
    }
    if(nx_end_frame(v->context)!=NX_OK)goto done;
    if(v->model_revision && revision!=v->model_revision) {
        /* Reassert the retained focus to retire an Enter/Space press captured
           for the old model without moving focus to another control. */
        if(nx_focus_set(v->context,nx_focus_id(v->context))!=NX_OK)goto done;
    }
    if(v->model_revision && revision!=v->model_revision && v->model->search) {
        nx_utf8_span draft;if(nx_text_field_text(v->query,&draft)!=NX_OK)goto done;
        size_t n=strlen(v->model->query);
        if(draft.length!=n || (n && memcmp(draft.data,v->model->query,n)))
            if(nx_text_field_set_text(v->query,v->model->query,n)!=NX_OK)goto done;
    }
    if(!v->model_revision || revision==v->model_revision)*intent=pending;
    v->model_revision=revision;okay=1;
done:
    if(guarded && nx_text_field_set_read_only(v->query,0)!=NX_OK)okay=0;
    for(int i=0;i<3;++i)nx_text_layout_destroy(layouts[i]);
    for(size_t i=0;i<AE_DESKTOP_RESULTS;++i)nx_text_layout_destroy(result_layouts[i]);
    return okay;
}
