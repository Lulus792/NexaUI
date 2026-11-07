#include "recovery_view.h"
#include "../common/text_wrap.h"
#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
enum { ROW_BUDGET=192 };
typedef struct row_cache {size_t index;char text[160];nx_text_layout *layout;float height;} row_cache;
struct rvg_view {
    rv_owner *owner;rv_snapshot *snapshot;nx_font_collection *fonts;nx_window *window;
    nx_context *ctx;nx_text_field *query;nx_virtual_list *index;size_t *matches,count;
    rvg_style style;rvg_mode mode;rv_key detail;
    uint64_t epoch,restore,last_focus,resume_focus;rv_observation state;int error;
    char filter[129],status[192],body[2048];nx_text_layout *status_layout,*body_layout,*buttons[3];
    row_cache rows[ROW_BUDGET];size_t cached,measured;double cache_width;
    float scroll,body_scroll,width,height,viewport_height;int compact,again,escape;
};
static nx_utf8_span sp(const char *s){return (nx_utf8_span){s,strlen(s)};}
static const char *word(const rvg_view *v,const char *en,const char *de){return v->style.german?de:en;}
static const char *kind_name(enum ae_recovery_record_kind kind,int german){
    static const char *const names[2][AE_RECOVERY_KIND_COUNT]={
        {"","Operation","Admission gate","Installation","Generation","Code lease","Dispatch","Launch","Grant","Access lease","Resource","Data epoch","Closure","Handoff","Interruption","Unit allocation","Unit custody"},
        {"","Vorgang","Zulassungssperre","Installation","Generation","Code-Lease","Dispatch","Start","Freigabe","Zugriffs-Lease","Ressource","Datenepoche","Abschluss","Übergabe","Unterbrechung","Unit-Zuweisung","Unit-Verwahrung"}};
    return kind>0&&kind<AE_RECOVERY_KIND_COUNT?names[!!german][kind]:"?";
}
static void id_text(const uint8_t id[16],char out[33]){static const char digits[]="0123456789abcdef";for(size_t i=0;i<16;++i){out[i*2]=digits[id[i]>>4];out[i*2+1]=digits[id[i]&15];}out[32]=0;}
static rv_key record_key(const struct ae_recovery_record *r){rv_key k={.kind=r->kind};memcpy(k.id,r->id,16);return k;}
static void row_text(const rvg_view *v,const struct ae_recovery_record *r,char out[160]){
    char id[33];id_text(r->id,id);(void)snprintf(out,160,"%s · %s\n%s",kind_name(r->kind,v->style.german),r->state,id);
}
static void clear_rows(rvg_view *v){for(size_t i=0;i<v->cached;++i)nx_text_layout_destroy(v->rows[i].layout);v->cached=0;}
static void clear_layouts(rvg_view *v){
    clear_rows(v);nx_text_layout_destroy(v->status_layout);v->status_layout=NULL;nx_text_layout_destroy(v->body_layout);v->body_layout=NULL;
    for(unsigned i=0;i<3;++i){nx_text_layout_destroy(v->buttons[i]);v->buttons[i]=NULL;}v->cache_width=0;
}
static nx_result context(rvg_view *v,nx_context **out){
    nx_result r=nx_context_create(NULL,out);nx_theme theme;
    if(r==NX_OK)r=nx_theme_preset(v->style.theme,&theme);
    if(r==NX_OK)r=nx_context_set_theme(*out,&theme);
    if(r==NX_OK)r=nx_context_set_reduced_motion(*out,1);
    if(r==NX_OK)r=nx_context_set_text_raster_scale(*out,v->style.raster_scale);
    if(r!=NX_OK){nx_context_destroy(*out);*out=NULL;}return r;
}
static nx_result retire(rvg_view *v,nx_context *next){
    nx_result r=v->window?nx_window_publish_semantics(v->window,NULL):NX_OK;
    if(r!=NX_OK)return r;
    nx_context_destroy(v->ctx);v->ctx=next;v->last_focus=0;clear_layouts(v);return NX_OK;
}
static int valid_query(nx_utf8_span q,void *unused){
    (void)unused;if(q.length>128||(!q.data&&q.length)||nx_utf8_validate(q.data,q.length,NULL)!=NX_OK)return 0;
    for(size_t i=0;i<q.length;++i)if(!q.data[i]||q.data[i]=='\n'||q.data[i]=='\r')return 0;
    return 1;
}
typedef struct build_index {const rvg_view *view;const size_t *matches;const struct ae_recovery_inventory *data;} build_index;
static nx_result indexed(void *p,size_t i,uint64_t *key,double *height){
    const build_index *b=p;*key=RVG_ROWS+b->matches[i];*height=b->view->style.text_size*2+20;return NX_OK;
}
static nx_result filtered(rvg_view *v,rv_snapshot *s,nx_utf8_span query,size_t **matches,size_t *count,nx_virtual_list **index){
    const struct ae_recovery_inventory *data=rv_snapshot_data(s);size_t n=data?data->count:0;
    *matches=n?malloc(n*sizeof(**matches)):NULL;*count=0;*index=NULL;
    if(n&&!*matches)return NX_ERROR_MEMORY;
    nx_result r=NX_OK;
    for(size_t i=0;i<n&&r==NX_OK;++i){char name[160];row_text(v,&data->records[i],name);
        nx_search_option option={0};option.id=RVG_ROWS+i;option.name=sp(name);size_t found=0;
        r=nx_search_filter(&option,1,query,NULL,0,&found);if(found)(*matches)[(*count)++]=i;
    }
    if(r==NX_OK)r=nx_virtual_list_create(0,1,NULL,index);
    build_index b={v,*matches,data};double offset=0;
    if(r==NX_OK)r=nx_virtual_list_rebuild(*index,*count,indexed,&b,100,&offset);
    if(r!=NX_OK){free(*matches);*matches=NULL;nx_virtual_list_destroy(*index);*index=NULL;}return r;
}
nx_result rvg_create(rv_owner *owner,nx_font_collection *fonts,rvg_style style,rvg_view **out){
    if(!out||*out||!owner||!fonts||!isfinite(style.text_size)||style.text_size<12||style.text_size>48||
       !isfinite(style.raster_scale)||style.raster_scale<.125f||style.raster_scale>8)return NX_ERROR_ARGUMENT;
    rvg_view *v=calloc(1,sizeof(*v));if(!v)return NX_ERROR_MEMORY;
    v->owner=owner;v->fonts=fonts;v->style=style;v->restore=RVG_FILTER_BUTTON;
    nx_result r=context(v,&v->ctx);
    if(r==NX_OK)r=nx_text_field_create("",0,4096,&v->query);
    if(r==NX_OK)r=rvg_sync(v);
    if(r!=NX_OK){(void)rvg_close(&v);return r;}*out=v;return NX_OK;
}
nx_result rvg_close(rvg_view **value){
    if(!value||!*value)return NX_OK;
    rvg_view *v=*value;
    nx_result r=v->window?nx_window_publish_semantics(v->window,NULL):NX_OK;if(r!=NX_OK)return r;
    clear_layouts(v);nx_context_destroy(v->ctx);nx_text_field_destroy(v->query);nx_virtual_list_destroy(v->index);
    free(v->matches);rv_snapshot_release(&v->snapshot);free(v);*value=NULL;return NX_OK;
}
void rvg_bind_window(rvg_view *v,nx_window *w){if(v)v->window=w;}
nx_context *rvg_context(rvg_view *v){return v?v->ctx:NULL;}
nx_result rvg_sync(rvg_view *v){
    if(!v)return NX_ERROR_ARGUMENT;
    uint64_t epoch=rv_owner_epoch(v->owner);rv_observation state=rv_owner_state(v->owner);int error=rv_owner_error(v->owner);
    if(v->epoch!=epoch||!v->index){
        rv_snapshot *s=NULL;nx_result r=rv_owner_snapshot(v->owner,&s);if(r==NX_ERROR_STATE&&!epoch)r=NX_OK;
        size_t *matches=NULL,count=0;nx_virtual_list *index=NULL;nx_context *next=NULL;
        if(r==NX_OK)r=filtered(v,s,sp(v->filter),&matches,&count,&index);
        if(r==NX_OK)r=context(v,&next);
        if(r==NX_OK)r=retire(v,next);
        if(r!=NX_OK){nx_context_destroy(next);nx_virtual_list_destroy(index);free(matches);rv_snapshot_release(&s);return r;}
        rv_snapshot_release(&v->snapshot);nx_virtual_list_destroy(v->index);free(v->matches);
        v->snapshot=s;v->matches=matches;v->count=count;v->index=index;v->epoch=epoch;v->scroll=0;
        if(v->mode==RVG_DETAIL&&rv_snapshot_find(s,v->detail)==SIZE_MAX)v->mode=RVG_LIST;
        v->restore=v->mode==RVG_FILTER?RVG_QUERY:v->mode==RVG_DETAIL?RVG_BACK:RVG_FILTER_BUTTON;
    }
    if(v->state!=state||v->error!=error){clear_layouts(v);v->state=state;v->error=error;}return NX_OK;
}
nx_result rvg_filter(rvg_view *v,nx_utf8_span q){
    if(!v||!valid_query(q,NULL))return q.length>128?NX_ERROR_LIMIT:NX_ERROR_ARGUMENT;
    char copied[129];if(q.length)memcpy(copied,q.data,q.length);copied[q.length]=0;q.data=copied;
    size_t *matches=NULL,count=0;nx_virtual_list *index=NULL;nx_context *next=NULL;
    nx_result r=filtered(v,v->snapshot,q,&matches,&count,&index);
    if(r==NX_OK)r=context(v,&next);
    if(r==NX_OK)r=retire(v,next);
    if(r!=NX_OK){nx_context_destroy(next);nx_virtual_list_destroy(index);free(matches);return r;}
    free(v->matches);nx_virtual_list_destroy(v->index);v->matches=matches;v->count=count;v->index=index;
    if(q.length)memcpy(v->filter,q.data,q.length);
    v->filter[q.length]=0;v->scroll=0;v->mode=RVG_LIST;v->restore=RVG_FILTER_BUTTON;return NX_OK;
}
static nx_result mode(rvg_view *v,rvg_mode next_mode,uint64_t restore){
    nx_context *next=NULL;nx_result r=context(v,&next);if(r==NX_OK)r=retire(v,next);
    if(r!=NX_OK){nx_context_destroy(next);return r;}v->mode=next_mode;v->restore=restore;v->body_scroll=0;return NX_OK;
}
static size_t focused_index(const rvg_view *v,uint64_t id){for(size_t i=0;i<v->count;++i)if(RVG_ROWS+v->matches[i]==id)return i;return SIZE_MAX;}
nx_result rvg_queue_event(rvg_view *v,const nx_event *e){
    if(!v||!e)return NX_ERROR_ARGUMENT;
    if(e->type==NX_EVENT_KEY_DOWN&&!e->modifiers){
        if(e->key==NX_KEY_ESCAPE){v->escape=1;return NX_OK;}
        if(v->mode==RVG_DETAIL&&(e->key==NX_KEY_PAGE_DOWN||e->key==NX_KEY_PAGE_UP)){
            v->body_scroll+=v->viewport_height*.5f*(e->key==NX_KEY_PAGE_DOWN?1.f:-1.f);if(v->body_scroll<0)v->body_scroll=0;return NX_OK;}
        if(v->mode==RVG_LIST&&v->count){
            size_t i=focused_index(v,nx_focus_id(v->ctx)),target=i;
            if(e->key==NX_KEY_HOME)target=0;else if(e->key==NX_KEY_END)target=v->count-1;
            else if(e->key==NX_KEY_DOWN)target=i==SIZE_MAX?0:i+1<v->count?i+1:i;
            else if(e->key==NX_KEY_UP)target=i==SIZE_MAX?0:i?i-1:0;
            else if(e->key==NX_KEY_PAGE_DOWN||e->key==NX_KEY_PAGE_UP){
                float step=v->viewport_height*.5f;v->scroll+=e->key==NX_KEY_PAGE_DOWN?step:-step;if(v->scroll<0)v->scroll=0;return NX_OK;}
            if(target!=i){double offset=0;nx_result r=nx_virtual_list_offset_at(v->index,target,&offset);if(r!=NX_OK)return r;
                v->scroll=(float)offset;v->restore=RVG_ROWS+v->matches[target];return NX_OK;}
        }
    }
    return nx_context_queue_event(v->ctx,e);
}
static nx_result layout(rvg_view *v,const char *text,double width,nx_text_layout **out){
    return nx_example_small_text_layout(v->fonts,text,strlen(text),v->style.text_size,width,out)?NX_OK:NX_ERROR_STATE;
}
static nx_result button(rvg_view *v,uint64_t id,nx_rect bounds,const char *name,nx_text_layout *label,int disabled,int *clicked){
    nx_control_desc d={0};d.id=id;d.parent_id=RVG_ROOT;d.name=sp(name);d.bounds=bounds;d.label_layout=label;
    d.label_origin=(nx_point){bounds.x+8,bounds.y+8};d.glyph_cache_bytes=1024*1024;d.disabled=disabled;
    nx_interaction_state state;nx_result r=nx_button(v->ctx,&d,&state);*clicked=r==NX_OK&&state.clicked;return r;
}
static nx_result text(rvg_view *v,uint64_t id,uint64_t parent,nx_rect bounds,const char *name,const char *value,nx_text_layout *label,uint32_t state){
    nx_semantic_desc d={0};d.id=id;d.parent_id=parent;d.role=id==RVG_STATUS?NX_ROLE_STATUS:NX_ROLE_TEXT;
    d.bounds=bounds;d.name=sp(name);d.value=sp(value);d.state=NX_SEM_READONLY|state;
    nx_result r=d.role==NX_ROLE_TEXT?nx_semantics_add_readonly_text_snapshot_without_caret(v->ctx,&d,id,NULL):nx_semantics_add(v->ctx,&d,NULL);
    nx_color color;if(r==NX_OK)r=nx_theme_color(v->ctx,NX_COLOR_TEXT,&color);
    if(r==NX_OK&&label)r=nx_draw_text_layout(v->ctx,label,(nx_point){bounds.x,bounds.y},color,1024*1024);
    return r;
}
static void status(rvg_view *v){
    const struct ae_recovery_inventory *d=rv_snapshot_data(v->snapshot);
    if(v->state==RV_UNOBSERVED)snprintf(v->status,sizeof v->status,"%s",word(v,"Owner metadata: not read","Owner-Metadaten: ungelesen"));
    else if(v->state==RV_READING)snprintf(v->status,sizeof v->status,"%s",word(v,"Reading; previous observation retained","Wird gelesen; vorherige Beobachtung bleibt"));
    else if(v->state==RV_UNAVAILABLE)snprintf(v->status,sizeof v->status,word(v,"Read failed (%d); previous observation retained","Lesen fehlgeschlagen (%d); vorherige Beobachtung bleibt"),v->error);
    else snprintf(v->status,sizeof v->status,word(v,"Owner metadata: %zu of %zu records","Owner-Metadaten: %zu von %zu Einträgen"),v->count,d?d->count:0);
}
static nx_result row_measure(rvg_view *v,float width,float height,nx_virtual_range *range){
    nx_result r=NX_OK;
    for(unsigned pass=0;pass<8;++pass){
        r=nx_virtual_list_range(v->index,v->scroll,height,1,range);if(r!=NX_OK)return r;
        if(range->end-range->first>ROW_BUDGET)return NX_ERROR_LIMIT;
        if(v->cached&&(v->rows[0].index!=range->first||v->rows[v->cached-1].index+1!=range->end))clear_rows(v);
        int changed=0;double anchor=0;size_t first=range->first;
        r=nx_virtual_list_offset_at(v->index,first,&anchor);if(r!=NX_OK)return r;
        double intra=range->effective_offset-anchor;
        for(size_t i=range->first;i<range->end;++i){
            row_cache *c=NULL;for(size_t j=0;j<v->cached;++j)if(v->rows[j].index==i){c=&v->rows[j];break;}
            if(!c){c=&v->rows[v->cached++];*c=(row_cache){.index=i};const struct ae_recovery_inventory *d=rv_snapshot_data(v->snapshot);
                row_text(v,&d->records[v->matches[i]],c->text);r=layout(v,c->text,width-20,&c->layout);if(r!=NX_OK)return r;
                c->height=(float)nx_text_layout_height(c->layout)+20;++v->measured;}
            double a,b;r=nx_virtual_list_offset_at(v->index,i,&a);if(r==NX_OK)r=nx_virtual_list_offset_at(v->index,i+1,&b);if(r!=NX_OK)return r;
            if(b-a!=c->height){r=nx_virtual_list_set_height(v->index,i,c->height);if(r!=NX_OK)return r;changed=1;}
        }
        if(!changed)return nx_virtual_list_range(v->index,v->scroll,height,1,range);
        r=nx_virtual_list_offset_at(v->index,first,&anchor);if(r!=NX_OK)return r;v->scroll=(float)(anchor+intra);
    }
    return NX_ERROR_LIMIT;
}
static void detail_body(rvg_view *v){
    const struct ae_recovery_inventory *d=rv_snapshot_data(v->snapshot);size_t i=rv_snapshot_find(v->snapshot,v->detail);
    if(!d||i==SIZE_MAX){v->body[0]=0;return;}const struct ae_recovery_record *r=&d->records[i];
    char id[33],related[33],epoch[33],boot[33],runtime[33],owner[33];id_text(r->id,id);id_text(r->related,related);
    id_text(d->context.authority_epoch,epoch);id_text(d->context.boot_id,boot);id_text(d->context.runtime_incarnation,runtime);id_text(d->identity.owner,owner);
    (void)snprintf(v->body,sizeof v->body,word(v,
        "%s\n\n%s\nID: %s\nState: %s\nRevision: %" PRId64 "\nUnfinished steps: %" PRId64 "\nRelated: %s\nDetail: %s\nPhase: %s\nReason: %s\n\nOwner: %s\nOwner ID: %s\nEpoch: %s\nBoot: %s\nRuntime: %s\nSchema: %u\nCatalogue revision: %" PRId64 "\n\nOwner metadata only. Check external holders and recovery readiness separately.",
        "%s\n\n%s\nID: %s\nZustand: %s\nRevision: %" PRId64 "\nOffene Schritte: %" PRId64 "\nVerknüpft: %s\nDetail: %s\nPhase: %s\nGrund: %s\n\nOwner: %s\nOwner-ID: %s\nEpoche: %s\nBoot: %s\nRuntime: %s\nSchema: %u\nKatalogrevision: %" PRId64 "\n\nNur Owner-Metadaten. Externe Halter und Recovery-Bereitschaft separat prüfen."),
        v->status,kind_name(r->kind,v->style.german),id,r->state,r->revision,r->unfinished_steps,r->has_related?related:word(v,"None","Keine"),r->detail,r->phase,r->reason,
        d->identity.kind==AE_CATALOGUE_SYSTEM?"Root":"User",owner,epoch,boot,runtime,d->schema_version,d->context.revision);
}
static nx_result frame_once(rvg_view *v,float width,float height,rvg_intent *intent,int *transition){
    const char *labels[3];uint64_t ids[3];size_t buttons=0;int clicked[3]={0};float top=12,button_height=0;nx_result r=NX_OK;
    status(v);v->compact=width<240||height<160;
    if(v->compact){labels[0]="X";ids[0]=RVG_CLOSE;buttons=1;}
    else if(v->mode==RVG_LIST){labels[0]=word(v,"Filter","Filter");ids[0]=RVG_FILTER_BUTTON;labels[1]=word(v,"Read","Lesen");ids[1]=RVG_REFRESH;labels[2]=word(v,"Close","Schließen");ids[2]=RVG_CLOSE;buttons=3;}
    else if(v->mode==RVG_FILTER){labels[0]=word(v,"Apply","Anwenden");ids[0]=RVG_APPLY;labels[1]=word(v,"Back","Zurück");ids[1]=RVG_BACK;labels[2]=word(v,"Close","Schließen");ids[2]=RVG_CLOSE;buttons=3;}
    else {labels[0]=word(v,"Back","Zurück");ids[0]=RVG_BACK;labels[1]=word(v,"Close","Schließen");ids[1]=RVG_CLOSE;buttons=2;}
    float margin=v->compact?4:12,gap=8,cell=(width-margin*2-gap*(float)(buttons-1))/(float)buttons;
    if(v->cache_width!=width){clear_layouts(v);v->cache_width=width;}
    for(size_t i=0;i<buttons&&r==NX_OK;++i){if(!v->buttons[i])r=layout(v,labels[i],cell-16,&v->buttons[i]);if(v->buttons[i])button_height=fmaxf(button_height,(float)nx_text_layout_height(v->buttons[i])+16);}
    if(v->compact)button_height=fminf(button_height,height-8);
    nx_virtual_range range={0};float list_top=0,viewport=0;
    if(!v->compact&&v->mode==RVG_LIST){
        if(!v->status_layout&&r==NX_OK)r=layout(v,v->status,width-24,&v->status_layout);
        list_top=top+button_height+12+(v->status_layout?(float)nx_text_layout_height(v->status_layout):0)+12;viewport=height-list_top-12;
        if(viewport<16)v->compact=1;
        else if(r==NX_OK)r=row_measure(v,width-40,viewport,&range);
    }
    if(r!=NX_OK)return r;
    r=nx_begin_frame(v->ctx,width,height,0);if(r!=NX_OK)return r;
    nx_semantic_desc root={0};root.id=RVG_ROOT;root.role=NX_ROLE_WINDOW;root.bounds=(nx_rect){0,0,width,height};root.name=sp(word(v,"Recovery observations","Recovery-Beobachtungen"));root.description=sp(word(v,"Read-only owner metadata. Recovery actions belong to the owner.","Schreibgeschützte Owner-Metadaten. Recovery-Aktionen gehören dem Owner."));
    r=nx_semantics_add(v->ctx,&root,NULL);
    for(size_t i=0;i<buttons&&r==NX_OK;++i){int disabled=ids[i]==RVG_REFRESH&&v->state==RV_READING;
        r=button(v,ids[i],(nx_rect){margin+(cell+gap)*(float)i,margin,cell,button_height},ids[i]==RVG_CLOSE?word(v,"Close","Schließen"):labels[i],v->buttons[i],disabled,&clicked[i]);}
    if(v->compact){
        if(r==NX_OK)r=text(v,RVG_STATUS,RVG_ROOT,(nx_rect){4,button_height+8,width-8,fmaxf(0,height-button_height-12)},word(v,"Enlarge the window to inspect records","Fenster zum Prüfen der Einträge vergrößern"),v->status,NULL,0);
    }else if(v->mode==RVG_LIST){
        uint32_t flags=v->state==RV_READING?NX_SEM_BUSY:v->state==RV_UNAVAILABLE?NX_SEM_INVALID:0;
        if(r==NX_OK)r=text(v,RVG_STATUS,RVG_ROOT,(nx_rect){12,top+button_height+12,width-24,list_top-top-button_height-24},v->status,v->status,v->status_layout,flags);
        nx_scroll_area_desc area={0};area.id=RVG_SCROLL;area.parent_id=RVG_ROOT;area.thumb_y_id=RVG_THUMB;area.axes=NX_SCROLL_Y;area.name=sp(word(v,"Observed records","Beobachtete Einträge"));
        area.viewport=(nx_rect){12,list_top,width-24,viewport};area.content_size=(nx_size){width-24,(float)range.total_extent};area.wheel_step=v->style.text_size*3;
        float x=0;nx_scroll_info info;v->viewport_height=viewport;
        if(r==NX_OK)r=nx_scroll_area_begin(v->ctx,&area,&x,&v->scroll,&info);
        rv_key selected;int have=rv_owner_selection(v->owner,&selected);
        for(size_t j=0;j<v->cached&&r==NX_OK;++j){row_cache *c=&v->rows[j];double offset; r=nx_virtual_list_offset_at(v->index,c->index,&offset);if(r!=NX_OK)break;
            const struct ae_recovery_inventory *d=rv_snapshot_data(v->snapshot);const struct ae_recovery_record *record=&d->records[v->matches[c->index]];
            nx_action_desc action={0};action.variant=NX_ACTION_TILE;action.control.id=RVG_ROWS+v->matches[c->index];action.control.parent_id=RVG_SCROLL;
            action.control.name=sp(c->text);action.control.bounds=(nx_rect){info.viewport.x,info.viewport.y+(float)offset-v->scroll,info.viewport.width-16,c->height};
            action.control.label_layout=c->layout;action.control.label_origin=(nx_point){action.control.bounds.x+8,action.control.bounds.y+8};action.control.glyph_cache_bytes=1024*1024;
            action.selected=have&&selected.kind==record->kind&&!memcmp(selected.id,record->id,16);nx_interaction_state state;
            r=nx_action(v->ctx,&action,&state);
            if(r==NX_OK&&state.clicked){v->detail=record_key(record);*transition=3;}
        }
        if(r==NX_OK&&!v->count)r=text(v,RVG_EMPTY,RVG_SCROLL,(nx_rect){info.viewport.x,info.viewport.y,info.viewport.width,info.viewport.height},
            word(v,v->snapshot?"No matching observed records":"No observation available",v->snapshot?"Keine passenden beobachteten Einträge":"Keine Beobachtung vorhanden"),"",NULL,0);
        if(r==NX_OK)r=nx_scroll_area_end(v->ctx);
    }else{
        float y=top+button_height+12,available=height-y-12;v->viewport_height=available;
        if(v->mode==RVG_FILTER){
            nx_text_field_desc field={0};field.id=RVG_QUERY;field.parent_id=RVG_ROOT;field.bounds=(nx_rect){12,y,width-24,v->style.text_size*2+16};field.name=sp(word(v,"Record filter","Eintragsfilter"));field.fonts=v->fonts;field.pixels_per_em=v->style.text_size;field.glyph_cache_bytes=1024*1024;field.validate=valid_query;field.ime_window=v->window;
            if(v->window&&r==NX_OK)r=nx_text_clipboard_for_window(v->window,&field.clipboard);
            nx_interaction_state interaction;int valid=0,submitted=0;
            if(r==NX_OK)r=nx_text_field_widget_described(v->ctx,&field,v->query,sp(word(v,"At most 128 UTF-8 bytes. Apply searches kind, state and full ID. Empty shows all.","Höchstens 128 UTF-8-Bytes. Anwenden sucht Typ, Zustand und volle ID. Leer zeigt alle.")),&interaction,&valid,&submitted);
            size_t preedit=0;int32_t a=0,b=0;if(v->window&&r==NX_OK){nx_result ime=nx_window_get_ime_preedit(v->window,NULL,0,&preedit,&a,&b);if(ime!=NX_OK&&ime!=NX_ERROR_UNSUPPORTED)r=ime;}
            if(valid&&!preedit&&(submitted||clicked[0]))*transition=2;
            const char *help=valid?word(v,"Type, state or full ID.\n128 UTF-8 bytes; empty shows all.","Typ, Zustand oder volle ID.\n128 UTF-8-Bytes; leer zeigt alle."):word(v,"Invalid filter: at most 128 UTF-8 bytes, no line breaks","Ungültiger Filter: höchstens 128 UTF-8-Bytes, keine Zeilenumbrüche");
            if(strcmp(v->body,help)){snprintf(v->body,sizeof v->body,"%s",help);nx_text_layout_destroy(v->body_layout);v->body_layout=NULL;}
            if(!v->body_layout&&r==NX_OK)r=layout(v,help,width-44,&v->body_layout);
            nx_scroll_area_desc hint={0};hint.id=RVG_SCROLL;hint.parent_id=RVG_ROOT;hint.thumb_y_id=RVG_THUMB;hint.axes=NX_SCROLL_Y;
            hint.name=sp(word(v,"Filter help","Filterhilfe"));hint.viewport=(nx_rect){12,y+field.bounds.height+8,width-24,fmaxf(1,available-field.bounds.height-8)};
            hint.content_size=(nx_size){width-24,v->body_layout?(float)nx_text_layout_height(v->body_layout)+8:0};float x=0;nx_scroll_info info;
            if(r==NX_OK)r=nx_scroll_area_begin(v->ctx,&hint,&x,&v->body_scroll,&info);
            if(r==NX_OK)r=text(v,RVG_STATUS,RVG_SCROLL,(nx_rect){info.viewport.x+4,info.viewport.y-v->body_scroll,info.viewport.width-20,hint.content_size.height},help,help,v->body_layout,valid?0:NX_SEM_INVALID);
            if(r==NX_OK)r=nx_scroll_area_end(v->ctx);
        }else{
            if(!v->body_layout){detail_body(v);if(r==NX_OK)r=layout(v,v->body,width-44,&v->body_layout);}
            nx_scroll_area_desc area={0};area.id=RVG_SCROLL;area.parent_id=RVG_ROOT;area.thumb_y_id=RVG_THUMB;area.axes=NX_SCROLL_Y;area.name=sp(word(v,"Record details","Eintragsdetails"));area.viewport=(nx_rect){12,y,width-24,available};area.content_size=(nx_size){width-24,v->body_layout?(float)nx_text_layout_height(v->body_layout)+16:0};
            float x=0;nx_scroll_info info;if(r==NX_OK)r=nx_scroll_area_begin(v->ctx,&area,&x,&v->body_scroll,&info);
            if(r==NX_OK)r=text(v,RVG_DETAIL_TEXT,RVG_SCROLL,(nx_rect){info.viewport.x+4,info.viewport.y-v->body_scroll,info.viewport.width-20,area.content_size.height},word(v,"Observed record details","Beobachtete Eintragsdetails"),v->body,v->body_layout,0);
            if(r==NX_OK)r=nx_scroll_area_end(v->ctx);
        }
    }
    if(r==NX_OK)r=nx_end_frame(v->ctx);
    if(r!=NX_OK)return r;
    if(v->restore){uint32_t node;if(nx_semantics_find(v->ctx,v->restore,&node)==NX_OK){r=nx_focus_set(v->ctx,v->restore);if(r!=NX_OK)return r;v->restore=0;v->again=1;}}
    for(size_t i=0;i<buttons;++i)if(clicked[i]){if(ids[i]==RVG_CLOSE)intent->close=1;else if(ids[i]==RVG_REFRESH)intent->refresh=1;else if(ids[i]==RVG_FILTER_BUTTON)*transition=1;else if(ids[i]==RVG_BACK)*transition=4;}
    if(v->escape){v->escape=0;if(v->mode!=RVG_LIST)*transition=4;else intent->close=1;}return NX_OK;
}
nx_result rvg_frame(rvg_view *v,float width,float height,rvg_intent *out){
    if(!v||!out||!isfinite(width)||!isfinite(height)||width<48||height<48||width>4096||height>4096)return NX_ERROR_ARGUMENT;
    *out=(rvg_intent){0};v->measured=0;
    if(v->width!=width||v->height!=height){
        int compact=width<240||height<160;
        if(compact&&!v->compact){v->resume_focus=nx_focus_id(v->ctx);v->restore=RVG_CLOSE;}
        else if(!compact&&v->compact){v->restore=v->resume_focus?v->resume_focus:v->mode==RVG_DETAIL?RVG_BACK:v->mode==RVG_FILTER?RVG_QUERY:RVG_FILTER_BUTTON;v->resume_focus=0;}
        clear_layouts(v);v->width=width;v->height=height;
    }
    for(unsigned pass=0;pass<12;++pass){v->again=0;int transition=0;rvg_intent intent={0};nx_result r=frame_once(v,width,height,&intent,&transition);if(r!=NX_OK)return r;
        if(intent.close||intent.refresh){*out=intent;return NX_OK;}
        if(transition){
            if(transition==1)r=mode(v,RVG_FILTER,RVG_QUERY);
            else if(transition==2){nx_utf8_span query;r=nx_text_field_text(v->query,&query);if(r==NX_OK)r=rvg_filter(v,query);}
            else if(transition==3){if(v->epoch!=rv_owner_epoch(v->owner))return NX_ERROR_STATE;r=rv_owner_select(v->owner,v->detail);if(r==NX_OK)r=mode(v,RVG_DETAIL,RVG_BACK);}
            else {uint64_t restore=RVG_FILTER_BUTTON;if(v->mode==RVG_DETAIL){size_t i=rv_snapshot_find(v->snapshot,v->detail);if(i!=SIZE_MAX&&focused_index(v,RVG_ROWS+i)!=SIZE_MAX)restore=RVG_ROWS+i;}r=mode(v,RVG_LIST,restore);}
            if(r!=NX_OK)return r;
            continue;
        }
        if(!v->again)return NX_OK;
    }
    return NX_ERROR_LIMIT;
}
size_t rvg_matches(const rvg_view *v){return v?v->count:0;}
rvg_mode rvg_current_mode(const rvg_view *v){return v?v->mode:RVG_LIST;}
size_t rvg_last_row_layouts(const rvg_view *v){return v?v->measured:0;}
