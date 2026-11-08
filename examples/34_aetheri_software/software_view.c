#include "software_view.h"
#include "../common/text_wrap.h"
#include <inttypes.h>
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
/* All five bounded recovery records may contain legal embedded newlines. */
enum { ROW_BUDGET=192,BODY_PARAGRAPHS=2048,CONTENT_ROWS=SWH_MAX_PROFILES };
_Static_assert(CONTENT_ROWS>=2+AE_FIXTURE_FILES+AE_FIXTURE_ACTIONS,"manifest cache capacity");
typedef struct row_cache {size_t index;char text[256];nx_text_layout *layout;float height;} row_cache;
typedef struct paragraph_cache {nx_text_layout *layout;float y,height;} paragraph_cache;
typedef struct content_cache {uint64_t id,parent;char name[257];nx_text_layout *layout;float height;} content_cache;
struct swg_view {
    sw_owner *owner;sw_snapshot *snapshot;nx_font_collection *fonts;nx_window *window;
    const sw_components *components;nx_result components_status;
    const swh_batch *history;int history_error;
    nx_context *ctx;nx_text_field *query;nx_virtual_list *index;size_t *matches,count;
    swg_style style;swg_mode mode;sw_id detail;
    uint64_t epoch,restore,resume_focus;sw_state state;int error;
    char filter[129],status[192],body[8192];nx_text_layout *status_layout,*body_layout,*buttons[4];
    row_cache rows[ROW_BUDGET];size_t cached,measured;double cache_width;
    paragraph_cache paragraphs[BODY_PARAGRAPHS];size_t paragraph_count;float body_height;
    content_cache contents[CONTENT_ROWS];size_t content_count;uint64_t item,tree_selection,tree_focus;
    int files_expanded,actions_expanded;
    float scroll,body_scroll,width,height,viewport_height;int compact,again,escape;
};
static nx_utf8_span sp(const char *s){return (nx_utf8_span){s,strlen(s)};}
static const char *word(const swg_view *v,const char *en,const char *de){return v->style.german?de:en;}
static void hex_text(const uint8_t *id,size_t n,char *out){static const char digits[]="0123456789abcdef";for(size_t i=0;i<n;++i){out[i*2]=digits[id[i]>>4];out[i*2+1]=digits[id[i]&15];}out[n*2]=0;}
static void row_text(const swg_view *v,const sw_row *r,char out[256]){
    char id[33];hex_text(r->install.bytes,16,id);(void)snprintf(out,256,"%s · %s\n%s",
        r->error?word(v,"Selected target","Ausgewähltes Ziel"):r->value.component,
        r->error?word(v,"Eligibility not observed","Eignung nicht beobachtet"):word(v,"Active app observed","Aktive App beobachtet"),id);
}
static void clear_rows(swg_view *v){for(size_t i=0;i<v->cached;++i)nx_text_layout_destroy(v->rows[i].layout);v->cached=0;}
static void clear_paragraphs(swg_view *v){for(size_t i=0;i<v->paragraph_count;++i)nx_text_layout_destroy(v->paragraphs[i].layout);v->paragraph_count=0;v->body_height=0;}
static void clear_layouts(swg_view *v){
    clear_rows(v);clear_paragraphs(v);nx_text_layout_destroy(v->status_layout);v->status_layout=NULL;nx_text_layout_destroy(v->body_layout);v->body_layout=NULL;
    for(unsigned i=0;i<4;++i){nx_text_layout_destroy(v->buttons[i]);v->buttons[i]=NULL;}
    for(size_t i=0;i<v->content_count;++i)nx_text_layout_destroy(v->contents[i].layout);
    v->content_count=0;v->tree_focus=0;v->cache_width=0;
}
static const amh_document *manifest(const swg_view *v){
    const amh_document *d=NULL;
    return v->components_status==NX_OK&&sw_components_get(v->components,v->snapshot,v->detail,&d)==NX_OK?d:NULL;
}
static nx_result histories(const swg_view *v,const swh_record **records,size_t *count){
    if(v->history_error)return NX_ERROR_STATE;
    return swh_get(v->history,v->snapshot,v->detail,records,count);
}
static int have_history(const swg_view *v){const swh_record *records=NULL;size_t count=0;return histories(v,&records,&count)==NX_OK&&count;}
static nx_result context(swg_view *v,nx_context **out){
    nx_result r=nx_context_create(NULL,out);nx_theme theme;
    if(r==NX_OK)r=nx_theme_preset(v->style.theme,&theme);
    if(r==NX_OK)r=nx_context_set_theme(*out,&theme);
    if(r==NX_OK)r=nx_context_set_reduced_motion(*out,1);
    if(r==NX_OK)r=nx_context_set_text_raster_scale(*out,v->style.raster_scale);
    if(r!=NX_OK){nx_context_destroy(*out);*out=NULL;}return r;
}
static nx_result retire(swg_view *v,nx_context *next){
    nx_result r=v->window?nx_window_publish_semantics(v->window,NULL):NX_OK;
    if(r!=NX_OK)return r;
    nx_context_destroy(v->ctx);v->ctx=next;clear_layouts(v);return NX_OK;
}
static int valid_query(nx_utf8_span q,void *unused){
    (void)unused;if(q.length>128||(!q.data&&q.length)||nx_utf8_validate(q.data,q.length,NULL)!=NX_OK)return 0;
    for(size_t i=0;i<q.length;++i)if(!q.data[i]||q.data[i]=='\n'||q.data[i]=='\r')return 0;
    return 1;
}
typedef struct build_index {const swg_view *view;const size_t *matches;} build_index;
static nx_result indexed(void *p,size_t i,uint64_t *key,double *height){
    const build_index *b=p;*key=SWG_ROWS+b->matches[i];*height=b->view->style.text_size*2+20;return NX_OK;
}
static nx_result filtered(swg_view *v,sw_snapshot *s,nx_utf8_span query,size_t **matches,size_t *count,nx_virtual_list **index){
    const sw_result *data=sw_snapshot_data(s);size_t n=data?data->count:0;
    *matches=n?malloc(n*sizeof(**matches)):NULL;*count=0;*index=NULL;
    if(n&&!*matches)return NX_ERROR_MEMORY;
    nx_result r=NX_OK;
    for(size_t i=0;i<n&&r==NX_OK;++i){char name[256];row_text(v,&data->rows[i],name);
        nx_search_option option={0};option.id=SWG_ROWS+i;option.name=sp(name);size_t found=0;
        r=nx_search_filter(&option,1,query,NULL,0,&found);if(found)(*matches)[(*count)++]=i;
    }
    if(r==NX_OK)r=nx_virtual_list_create(0,1,NULL,index);
    build_index b={v,*matches};double offset=0;
    if(r==NX_OK)r=nx_virtual_list_rebuild(*index,*count,indexed,&b,100,&offset);
    if(r!=NX_OK){free(*matches);*matches=NULL;nx_virtual_list_destroy(*index);*index=NULL;}return r;
}
nx_result swg_create(sw_owner *owner,nx_font_collection *fonts,swg_style style,swg_view **out){
    if(!out||*out||!owner||!fonts||!isfinite(style.text_size)||style.text_size<12||style.text_size>48||
       !isfinite(style.raster_scale)||style.raster_scale<.125f||style.raster_scale>8)return NX_ERROR_ARGUMENT;
    swg_view *v=calloc(1,sizeof(*v));if(!v)return NX_ERROR_MEMORY;
    v->owner=owner;v->fonts=fonts;v->style=style;v->restore=SWG_FILTER_BUTTON;
    nx_result r=context(v,&v->ctx);
    if(r==NX_OK)r=nx_text_field_create("",0,4096,&v->query);
    if(r==NX_OK)r=swg_sync(v);
    if(r!=NX_OK){(void)swg_close(&v);return r;}*out=v;return NX_OK;
}
nx_result swg_close(swg_view **value){
    if(!value||!*value)return NX_OK;
    swg_view *v=*value;
    nx_result r=v->window?nx_window_publish_semantics(v->window,NULL):NX_OK;if(r!=NX_OK)return r;
    clear_layouts(v);nx_context_destroy(v->ctx);nx_text_field_destroy(v->query);nx_virtual_list_destroy(v->index);
    free(v->matches);sw_snapshot_release(&v->snapshot);free(v);*value=NULL;return NX_OK;
}
void swg_bind_window(swg_view *v,nx_window *w){if(v)v->window=w;}
nx_context *swg_context(swg_view *v){return v?v->ctx:NULL;}
nx_result swg_set_raster_scale(swg_view *v,float scale){
    if(!v)return NX_ERROR_ARGUMENT;
    nx_result r=nx_context_set_text_raster_scale(v->ctx,scale);if(r==NX_OK)v->style.raster_scale=scale;return r;
}
nx_result swg_set_components(swg_view *v,const sw_components *components,nx_result status_value){
    return v?swg_set_metadata(v,components,status_value,v->history,v->history_error):NX_ERROR_ARGUMENT;
}
nx_result swg_set_metadata(swg_view *v,const sw_components *components,nx_result status_value,const swh_batch *history,int history_error){
    if(!v||(components&&status_value!=NX_OK)||history_error>0||(history&&history_error))return NX_ERROR_ARGUMENT;
    if(v->components==components&&v->components_status==status_value&&v->history==history&&v->history_error==history_error)return NX_OK;
    uint64_t focus=nx_focus_id(v->ctx);nx_context *next=NULL;nx_result r=context(v,&next);
    if(r==NX_OK)r=retire(v,next);
    if(r!=NX_OK){nx_context_destroy(next);return r;}
    v->components=components;v->components_status=status_value;
    v->history=history;v->history_error=history_error;
    if(v->mode==SWG_DETAIL&&(focus==SWG_CONTENTS_BUTTON||focus==SWG_HISTORY_BUTTON))focus=SWG_BACK;
    if(v->mode==SWG_CONTENTS||v->mode==SWG_DECLARATION||v->mode==SWG_HISTORY||v->mode==SWG_HISTORY_DETAIL||v->mode==SWG_UNIT_LINKS||v->mode==SWG_RECEIPTS||v->mode==SWG_RECOVERY_LINKS){v->mode=SWG_DETAIL;focus=SWG_BACK;v->body_scroll=0;v->item=0;v->tree_selection=0;}
    v->restore=focus?focus:v->mode==SWG_FILTER?SWG_QUERY:v->mode==SWG_DETAIL?SWG_BACK:SWG_FILTER_BUTTON;return NX_OK;
}
nx_result swg_sync(swg_view *v){
    if(!v)return NX_ERROR_ARGUMENT;
    uint64_t epoch=sw_owner_epoch(v->owner);sw_state state=sw_owner_state(v->owner);int error=sw_owner_error(v->owner);
    if(v->epoch!=epoch||!v->index){
        uint64_t focus=v->compact?v->resume_focus:nx_focus_id(v->ctx);sw_id focused={{0}};int focused_row=0;
        const sw_result *previous=sw_snapshot_data(v->snapshot);
        if(previous&&focus>=SWG_ROWS&&focus-SWG_ROWS<previous->count){focused=previous->rows[focus-SWG_ROWS].install;focused_row=1;}
        sw_snapshot *s=NULL;nx_result r=sw_owner_snapshot(v->owner,&s);if(r==NX_ERROR_STATE&&!epoch)r=NX_OK;
        size_t *matches=NULL,count=0;nx_virtual_list *index=NULL;nx_context *next=NULL;
        if(r==NX_OK)r=filtered(v,s,sp(v->filter),&matches,&count,&index);
        if(r==NX_OK)r=context(v,&next);
        if(r==NX_OK)r=retire(v,next);
        if(r!=NX_OK){nx_context_destroy(next);nx_virtual_list_destroy(index);free(matches);sw_snapshot_release(&s);return r;}
        sw_snapshot_release(&v->snapshot);nx_virtual_list_destroy(v->index);free(v->matches);
        v->snapshot=s;v->matches=matches;v->count=count;v->index=index;v->epoch=epoch;v->scroll=0;
        if(v->mode==SWG_CONTENTS||v->mode==SWG_DECLARATION||v->mode==SWG_HISTORY||v->mode==SWG_HISTORY_DETAIL||v->mode==SWG_UNIT_LINKS||v->mode==SWG_RECEIPTS||v->mode==SWG_RECOVERY_LINKS){v->mode=SWG_DETAIL;v->body_scroll=0;v->item=0;v->tree_selection=0;}
        if(v->mode==SWG_DETAIL&&sw_snapshot_find(s,v->detail)==SIZE_MAX)v->mode=SWG_LIST;
        v->restore=v->mode==SWG_FILTER?SWG_QUERY:v->mode==SWG_DETAIL?SWG_BACK:SWG_FILTER_BUTTON;
        if(v->mode==SWG_LIST){
            if(focus==SWG_CLOSE||focus==SWG_REFRESH||focus==SWG_FILTER_BUTTON)v->restore=focus;
            if(focused_row){size_t at=sw_snapshot_find(s,focused);for(size_t j=0;j<count;++j)if(matches[j]==at){
                double offset=0;if(nx_virtual_list_offset_at(index,j,&offset)==NX_OK){v->scroll=(float)offset;v->restore=SWG_ROWS+at;}break;}}
        }
        if(v->compact){v->resume_focus=v->restore;v->restore=SWG_CLOSE;}
    }
    if(v->state!=state||v->error!=error){if(!v->restore)v->restore=nx_focus_id(v->ctx);clear_layouts(v);v->state=state;v->error=error;}return NX_OK;
}
nx_result swg_filter(swg_view *v,nx_utf8_span q){
    if(!v||!valid_query(q,NULL))return q.length>128?NX_ERROR_LIMIT:NX_ERROR_ARGUMENT;
    char copied[129];if(q.length)memcpy(copied,q.data,q.length);copied[q.length]=0;q.data=copied;
    size_t *matches=NULL,count=0;nx_virtual_list *index=NULL;nx_context *next=NULL;
    nx_result r=filtered(v,v->snapshot,q,&matches,&count,&index);
    if(r==NX_OK)r=context(v,&next);
    if(r==NX_OK)r=retire(v,next);
    if(r!=NX_OK){nx_context_destroy(next);nx_virtual_list_destroy(index);free(matches);return r;}
    free(v->matches);nx_virtual_list_destroy(v->index);v->matches=matches;v->count=count;v->index=index;
    if(q.length)memcpy(v->filter,q.data,q.length);
    v->filter[q.length]=0;v->scroll=0;v->mode=SWG_LIST;v->restore=SWG_FILTER_BUTTON;return NX_OK;
}
static nx_result mode(swg_view *v,swg_mode next_mode,uint64_t restore){
    nx_context *next=NULL;nx_result r=context(v,&next);if(r==NX_OK)r=retire(v,next);
    if(r!=NX_OK){nx_context_destroy(next);return r;}v->mode=next_mode;v->restore=restore;v->body_scroll=0;return NX_OK;
}
static size_t focused_index(const swg_view *v,uint64_t id){for(size_t i=0;i<v->count;++i)if(SWG_ROWS+v->matches[i]==id)return i;return SIZE_MAX;}
nx_result swg_queue_event(swg_view *v,const nx_event *e){
    if(!v||!e)return NX_ERROR_ARGUMENT;
    if(e->type==NX_EVENT_KEY_DOWN&&!e->modifiers){
        if(e->key==NX_KEY_ESCAPE){v->escape=1;return NX_OK;}
        if((v->mode==SWG_DETAIL||v->mode==SWG_CONTENTS||v->mode==SWG_DECLARATION||v->mode==SWG_HISTORY||v->mode==SWG_HISTORY_DETAIL||v->mode==SWG_UNIT_LINKS||v->mode==SWG_RECEIPTS||v->mode==SWG_RECOVERY_LINKS)&&(e->key==NX_KEY_PAGE_DOWN||e->key==NX_KEY_PAGE_UP)){
            v->body_scroll+=v->viewport_height*.5f*(e->key==NX_KEY_PAGE_DOWN?1.f:-1.f);if(v->body_scroll<0)v->body_scroll=0;return NX_OK;}
        if((v->mode==SWG_CONTENTS||v->mode==SWG_HISTORY)&&v->content_count){
            uint64_t focused=nx_focus_id(v->ctx),wanted=0;size_t at=SIZE_MAX;
            for(size_t i=0;i<v->content_count;++i)if(v->contents[i].id==focused)at=i;
            if(e->key==NX_KEY_HOME)wanted=v->contents[0].id;
            else if(e->key==NX_KEY_END)wanted=v->contents[v->content_count-1].id;
            else if(e->key==NX_KEY_DOWN)wanted=v->contents[at==SIZE_MAX?0:at+1<v->content_count?at+1:at].id;
            else if(e->key==NX_KEY_UP)wanted=v->contents[at==SIZE_MAX?0:at?at-1:0].id;
            else if(e->key==NX_KEY_LEFT&&at!=SIZE_MAX&&v->contents[at].parent!=SWG_SCROLL)wanted=v->contents[at].parent;
            if(wanted){v->restore=wanted;return NX_OK;}
        }
        if(v->mode==SWG_LIST&&v->count){
            size_t i=focused_index(v,nx_focus_id(v->ctx)),target=i;
            if(e->key==NX_KEY_HOME)target=0;else if(e->key==NX_KEY_END)target=v->count-1;
            else if(e->key==NX_KEY_DOWN)target=i==SIZE_MAX?0:i+1<v->count?i+1:i;
            else if(e->key==NX_KEY_UP)target=i==SIZE_MAX?0:i?i-1:0;
            else if(e->key==NX_KEY_PAGE_DOWN||e->key==NX_KEY_PAGE_UP){
                float step=v->viewport_height*.5f;v->scroll+=e->key==NX_KEY_PAGE_DOWN?step:-step;if(v->scroll<0)v->scroll=0;return NX_OK;}
            if(target!=i){double offset=0;nx_result r=nx_virtual_list_offset_at(v->index,target,&offset);if(r!=NX_OK)return r;
                v->scroll=(float)offset;v->restore=SWG_ROWS+v->matches[target];return NX_OK;}
        }
    }
    return nx_context_queue_event(v->ctx,e);
}
static nx_result layout(swg_view *v,const char *text,double width,nx_text_layout **out){
    return nx_example_small_text_layout(v->fonts,text,strlen(text),v->style.text_size,width,out)?NX_OK:NX_ERROR_STATE;
}
static nx_result button(swg_view *v,uint64_t id,nx_rect bounds,const char *name,nx_text_layout *label,int disabled,int *clicked){
    nx_control_desc d={0};d.id=id;d.parent_id=SWG_ROOT;d.name=sp(name);d.bounds=bounds;d.label_layout=label;
    d.label_origin=(nx_point){bounds.x+8,bounds.y+8};d.glyph_cache_bytes=1024*1024;d.disabled=disabled;
    nx_interaction_state state;nx_result r=nx_button(v->ctx,&d,&state);*clicked=r==NX_OK&&state.clicked;return r;
}
static nx_result text(swg_view *v,uint64_t id,uint64_t parent,nx_rect bounds,const char *name,const char *value,nx_text_layout *label,uint32_t state){
    nx_semantic_desc d={0};d.id=id;d.parent_id=parent;d.role=id==SWG_STATUS?NX_ROLE_STATUS:NX_ROLE_TEXT;
    d.bounds=bounds;d.name=sp(name);d.value=sp(value);d.state=NX_SEM_READONLY|state;
    nx_result r=d.role==NX_ROLE_TEXT?nx_semantics_add_readonly_text_snapshot_without_caret(v->ctx,&d,id,NULL):nx_semantics_add(v->ctx,&d,NULL);
    nx_color color;if(r==NX_OK)r=nx_theme_color(v->ctx,NX_COLOR_TEXT,&color);
    if(r==NX_OK&&label)r=nx_draw_text_layout(v->ctx,label,(nx_point){bounds.x,bounds.y},color,1024*1024);
    return r;
}
static void status(swg_view *v){
    const sw_result *d=sw_snapshot_data(v->snapshot);
    if(v->state==SW_UNOBSERVED)snprintf(v->status,sizeof v->status,"%s",word(v,"Selected apps: not read","Ausgewählte Apps: ungelesen"));
    else if(v->state==SW_READING)snprintf(v->status,sizeof v->status,"%s",word(v,"Reading selected apps; old view retained","Ausgewählte Apps lesen; alte Ansicht bleibt"));
    else if(v->state==SW_UNAVAILABLE)snprintf(v->status,sizeof v->status,word(v,"Read failed (%d); old view retained","Lesefehler (%d); alte Ansicht bleibt"),v->error);
    else snprintf(v->status,sizeof v->status,word(v,"Selected apps: %zu of %zu targets","Ausgewählte Apps: %zu von %zu Zielen"),v->count,d?d->count:0);
}
static nx_result row_measure(swg_view *v,float width,float height,nx_virtual_range *range){
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
            if(!c){c=&v->rows[v->cached++];*c=(row_cache){.index=i};const sw_result *d=sw_snapshot_data(v->snapshot);
                row_text(v,&d->rows[v->matches[i]],c->text);r=layout(v,c->text,width-20,&c->layout);if(r!=NX_OK)return r;
                c->height=(float)nx_text_layout_height(c->layout)+20;++v->measured;}
            double a,b;r=nx_virtual_list_offset_at(v->index,i,&a);if(r==NX_OK)r=nx_virtual_list_offset_at(v->index,i+1,&b);if(r!=NX_OK)return r;
            if(b-a!=c->height){r=nx_virtual_list_set_height(v->index,i,c->height);if(r!=NX_OK)return r;changed=1;}
        }
        if(!changed)return nx_virtual_list_range(v->index,v->scroll,height,1,range);
        r=nx_virtual_list_offset_at(v->index,first,&anchor);if(r!=NX_OK)return r;v->scroll=(float)(anchor+intra);
    }
    return NX_ERROR_LIMIT;
}
static void detail_body(swg_view *v){
    const sw_result *d=sw_snapshot_data(v->snapshot);size_t i=sw_snapshot_find(v->snapshot,v->detail);
    if(!d||i==SIZE_MAX){v->body[0]=0;return;}const sw_row *r=&d->rows[i];
    char id[33],epoch[33],boot[33],runtime[33],account[33];hex_text(r->install.bytes,16,id);
    hex_text(d->scope.context.authority_epoch,16,epoch);hex_text(d->scope.context.boot_id,16,boot);
    hex_text(d->scope.context.runtime_incarnation,16,runtime);hex_text(d->scope.account,16,account);
    int used=snprintf(v->body,sizeof v->body,word(v,
        "%s\n\nSelected installation: %s\nRecipient account: %s\nRecipient UID: %" PRIu32 "\nSource: Root\nEpoch: %s\nBoot: %s\nRuntime: %s\nCatalogue revision: %" PRId64 "\n\n",
        "%s\n\nAusgewählte Installation: %s\nEmpfängerkonto: %s\nEmpfänger-UID: %" PRIu32 "\nQuelle: Root\nEpoche: %s\nBoot: %s\nRuntime: %s\nKatalogrevision: %" PRId64 "\n\n"),
        v->status,id,account,d->scope.os_uid,epoch,boot,runtime,d->scope.context.revision);
    if(used<0||(size_t)used>=sizeof v->body){v->body[0]=0;return;}
    if(r->error){
        (void)snprintf(v->body+(size_t)used,sizeof v->body-(size_t)used,"%s",word(v,
            "No eligible active app observed for this recipient. This does not distinguish absent, inactive or ineligible targets. It proves neither removal nor denial.\n\nSelected targets only; not a complete software inventory.",
            "Keine geeignete aktive App für diesen Empfänger beobachtet. Abwesende, inaktive und ungeeignete Ziele sind nicht unterscheidbar. Dies belegt weder Entfernung noch Ablehnung.\n\nNur ausgewählte Ziele; kein vollständiges Softwareinventar."));
    }else{
        const struct ae_install_snapshot *s=&r->value;char publisher[33],scope[33],generation[33],artifact[65];
        hex_text(s->publisher,16,publisher);hex_text(s->scope,16,scope);hex_text(s->generation,16,generation);hex_text(s->artifact,32,artifact);
        const char *gate=s->gate==AE_INSTALL_GATE_OPEN?word(v,"Open","Offen"):s->gate==AE_INSTALL_GATE_CLOSING?word(v,"Closing","Wird geschlossen"):word(v,"Closed","Geschlossen");
        (void)snprintf(v->body+(size_t)used,sizeof v->body-(size_t)used,word(v,
            "Component: %s\nPublisher: %s\nInstall scope: %s\nGeneration: %s\nArtifact: %s\nInstall revision: %" PRId64 "\nGeneration revision: %" PRId64 "\nGate: %s\nGate revision: %" PRId64 "\n\nActive app metadata only. An open gate is not a launch ticket, permission or running-process observation. Selected targets only; not a complete software inventory.",
            "Komponente: %s\nHerausgeber: %s\nInstallationsbereich: %s\nGeneration: %s\nArtefakt: %s\nInstallationsrevision: %" PRId64 "\nGenerationsrevision: %" PRId64 "\nSperre: %s\nSperrrevision: %" PRId64 "\n\nNur Metadaten einer aktiven App. Eine offene Sperre ist kein Startticket, keine Berechtigung und keine Beobachtung eines laufenden Prozesses. Nur ausgewählte Ziele; kein vollständiges Softwareinventar."),
            s->component,publisher,scope,generation,artifact,s->install_revision,s->generation_revision,gate,s->gate_revision);
    }
}
static nx_result component_details(swg_view *v){
    size_t used=strlen(v->body);const amh_document *document=NULL;
    nx_result status_value=v->components_status;
    if(status_value==NX_OK)status_value=sw_components_get(v->components,v->snapshot,v->detail,&document);
    int n;
    if(status_value!=NX_OK)n=snprintf(v->body+used,sizeof v->body-used,word(v,
        "\n\nManifest unavailable (%d). Declared dependencies and installed dependency state are unknown.",
        "\n\nManifest nicht verfügbar (%d). Deklarierte Abhängigkeiten und installierter Abhängigkeitszustand sind unbekannt."),(int)status_value);
    else if(!document)n=snprintf(v->body+used,sizeof v->body-used,"%s",word(v,
        "\n\nNo matching manifest selected. Declared dependencies and installed dependency state are unknown.",
        "\n\nKein passendes Manifest ausgewählt. Deklarierte Abhängigkeiten und installierter Abhängigkeitszustand sind unbekannt."));
    else{
        const struct ae_fixture *d=amh_document_data(document);
        n=snprintf(v->body+used,sizeof v->body-used,word(v,
            "\n\nVerified manifest matches this observation's artifact, component and publisher. Publisher trust remains external.\nVersion: %.*s\nTarget: %.*s\nDeclared files: %" PRIu32 "\nDeclared actions: %" PRIu32 "\nDeclared dependencies: %" PRIu32 "\nInstalled/active dependency and ownership state: unknown.\n",
            "\n\nVerifiziertes Manifest passt zu Artefakt, Komponente und Herausgeber dieser Beobachtung. Herausgebervertrauen bleibt extern.\nVersion: %.*s\nZiel: %.*s\nDeklarierte Dateien: %" PRIu32 "\nDeklarierte Aktionen: %" PRIu32 "\nDeklarierte Abhängigkeiten: %" PRIu32 "\nInstallierter/aktiver Abhängigkeits- und Besitzstatus: unbekannt.\n"),
            (int)d->version.size,d->version.data,(int)d->target.size,d->target.data,d->file_count,d->action_count,d->dependency_count);
        if(n<0||(size_t)n>=sizeof v->body-used)return NX_ERROR_LIMIT;
        used+=(size_t)n;
        for(uint32_t i=0;i<d->dependency_count;++i){char digest[65];hex_text(d->dependencies[i],32,digest);
            n=snprintf(v->body+used,sizeof v->body-used,word(v,"Declared dependency %" PRIu32 ": %s\n","Deklarierte Abhängigkeit %" PRIu32 ": %s\n"),i+1,digest);
            if(n<0||(size_t)n>=sizeof v->body-used)return NX_ERROR_LIMIT;
            used+=(size_t)n;
        }
        return NX_OK;
    }
    return n<0||(size_t)n>=sizeof v->body-used?NX_ERROR_LIMIT:NX_OK;
}
static nx_result append(swg_view *v,const char *format,...){
    size_t used=strlen(v->body);va_list args;va_start(args,format);
    int n=vsnprintf(v->body+used,sizeof v->body-used,format,args);va_end(args);
    return n<0||(size_t)n>=sizeof v->body-used?NX_ERROR_LIMIT:NX_OK;
}
static nx_result history_summary(swg_view *v){
    const swh_record *records=NULL;size_t count=0;nx_result r=histories(v,&records,&count);
    if(r!=NX_OK)return append(v,word(v,"\n\nHistorical process observations unavailable (%d). Current process/resource/ownership state: unknown.","\n\nHistorische Prozessbeobachtungen nicht verfügbar (%d). Aktueller Prozess-/Ressourcen-/Besitzstatus: unbekannt."),v->history_error?v->history_error:(int)r);
    if(!count)return append(v,"%s",word(v,"\n\nNo historical profiles selected for this installation. This is not an empty process inventory. Current process/resource/ownership state: unknown.","\n\nKeine historischen Profile für diese Installation ausgewählt. Dies ist kein leeres Prozessinventar. Aktueller Prozess-/Ressourcen-/Besitzstatus: unbekannt."));
    return append(v,word(v,"\n\nSelected historical profiles: %zu. Enrollment metadata only; not current processes. Current process/resource/ownership state: unknown.","\n\nAusgewählte historische Profile: %zu. Nur registrierte Metadaten; keine aktuellen Prozesse. Aktueller Prozess-/Ressourcen-/Besitzstatus: unbekannt."),count);
}
static nx_result history_process(swg_view *v,const char *name,int error,const struct ae_execution_process_snapshot *snapshot){
    if(error)return append(v,word(v,"\nHistorical %s: unavailable (%d). Current process state remains unknown.\n","\nHistorisch %s: nicht verfügbar (%d). Aktueller Prozesszustand bleibt unbekannt.\n"),name,error);
    const struct ae_execution_process *p=&snapshot->process;char boot[33],invocation[33],evidence[65],profile[65];
    hex_text(p->boot,16,boot);hex_text(p->invocation,16,invocation);hex_text(p->evidence_digest,32,evidence);hex_text(p->profile_digest,32,profile);
    return append(v,word(v,
        "\nHistorical %s enrollment\nRecorded Unix ms: %" PRId64 "\nRecorded PID: %" PRIu32 "\nRecorded UID: %" PRIu32 "\nRecorded pidfs ID: %" PRIu64 "\nRecorded cgroup ID: %" PRIu64 "\nRecorded unit: %s\nRecorded label: %s\nRecorded cgroup: %s\nRecorded boot: %s\nRecorded invocation: %s\nRecorded evidence SHA-256: %s\nRecorded profile SHA-256: %s\n",
        "\nHistorische Registrierung %s\nAufgezeichnete Unix-ms: %" PRId64 "\nAufgezeichnete PID: %" PRIu32 "\nAufgezeichnete UID: %" PRIu32 "\nAufgezeichnete pidfs-ID: %" PRIu64 "\nAufgezeichnete cgroup-ID: %" PRIu64 "\nAufgezeichnete Unit: %s\nAufgezeichnetes Label: %s\nAufgezeichnete cgroup: %s\nAufgezeichneter Boot: %s\nAufgezeichnete Invocation: %s\nAufgezeichneter Nachweis-SHA-256: %s\nAufgezeichneter Profil-SHA-256: %s\n"),
        name,snapshot->recorded_unix_ms,p->pid,p->uid,p->pidfd_id,p->cgroup_id,p->unit,p->label,p->cgroup,boot,invocation,evidence,profile);
}
static nx_result history_body(swg_view *v){
    const swh_record *records=NULL;size_t count=0;nx_result r=histories(v,&records,&count);
    if(r!=NX_OK||v->item<SWG_HISTORY_ROWS||v->item-SWG_HISTORY_ROWS>=count)return NX_ERROR_STATE;
    const swh_record *record=&records[v->item-SWG_HISTORY_ROWS];const struct ae_execution_profile *p=&record->profile;
    char install[33],generation[33],artifact[65],digest[65];hex_text(v->detail.bytes,16,install);hex_text(record->observed.value.generation,16,generation);hex_text(record->observed.value.artifact,32,artifact);hex_text(record->digest,32,digest);v->body[0]=0;
    r=append(v,word(v,
        "Historical enrollment metadata only. Current process, image, resources, ownership and permission state: unknown. No process is opened, started or stopped.\n\nSelected installation: %s\nCurrent observed generation: %s\nCurrent observed artifact: %s\nCurrent catalogue revision: %" PRId64 "\nHistorical profile SHA-256: %s\n\n",
        "Nur historische Registrierungsmetadaten. Aktueller Prozess-, Image-, Ressourcen-, Besitz- und Berechtigungsstatus: unbekannt. Kein Prozess wird geöffnet, gestartet oder gestoppt.\n\nAusgewählte Installation: %s\nAktuell beobachtete Generation: %s\nAktuell beobachtetes Artefakt: %s\nAktuelle Katalogrevision: %" PRId64 "\nHistorischer Profil-SHA-256: %s\n\n"),install,generation,artifact,record->observed.value.catalogue_revision,digest);
    const uint8_t *ids[]={p->system_epoch,p->system_runtime,p->broker_epoch,p->broker_runtime,p->boot,p->account,p->install,p->generation,p->data,p->storage,p->launch,p->lease,p->root_operation,p->user_operation};
    const char *en[]={"Root epoch","Root runtime","Broker epoch","Broker runtime","Boot","Account","Install","Generation","Data","Storage","Launch","Lease","Root operation","User operation"};
    const char *de[]={"Root-Epoche","Root-Runtime","Broker-Epoche","Broker-Runtime","Boot","Konto","Installation","Generation","Daten","Speicher","Launch","Lease","Root-Operation","User-Operation"};
    for(size_t i=0;i<14&&r==NX_OK;++i){char text[33];hex_text(ids[i],16,text);r=append(v,"%s %s: %s\n",word(v,"Historical","Historisch"),word(v,en[i],de[i]),text);}
    if(r==NX_OK)r=append(v,word(v,"Historical install revision: %" PRId64 "\nHistorical install gate revision: %" PRId64 "\nHistorical data gate revision: %" PRId64 "\n","Historische Installationsrevision: %" PRId64 "\nHistorische Installationssperrrevision: %" PRId64 "\nHistorische Datensperrrevision: %" PRId64 "\n"),p->install_revision,p->install_gate_revision,p->data_gate_revision);
    const uint8_t *hashes[]={p->artifact,p->starter_digest,p->request_digest,p->allocation_digest};
    const char *hash_en[]={"Artifact","Starter","Request","Allocation"},*hash_de[]={"Artefakt","Starter","Anfrage","Allokation"};
    for(size_t i=0;i<4&&r==NX_OK;++i){char text[65];hex_text(hashes[i],32,text);r=append(v,"%s %s SHA-256: %s\n",word(v,"Historical","Historisch"),word(v,hash_en[i],hash_de[i]),text);}
    if(r==NX_OK)r=history_process(v,"Starter",record->starter_error,&record->starter);
    if(r==NX_OK)r=history_process(v,"App",record->app_error,&record->app);
    return r;
}
static nx_result unit_links_body(swg_view *v){
    const swh_record *records=NULL;size_t count=0;nx_result r=histories(v,&records,&count);
    if(r!=NX_OK||v->item<SWG_HISTORY_ROWS||v->item-SWG_HISTORY_ROWS>=count)return NX_ERROR_STATE;
    const swh_record *record=&records[v->item-SWG_HISTORY_ROWS];char install[33],operation[33],digest[65];
    hex_text(v->detail.bytes,16,install);hex_text(record->profile.root_operation,16,operation);hex_text(record->digest,32,digest);v->body[0]=0;
    r=append(v,word(v,
        "Recorded unit associations only. A selection witness or RPC acknowledgement is not independent retirement, holder drain or permission to repeat an effect. Current process, resources, ownership and permission state: unknown.\n\nSelected installation: %s\nHistorical Root operation: %s\nHistorical profile SHA-256: %s\n\n",
        "Nur aufgezeichnete Unit-Bezüge. Ein Auswahlbeleg oder eine RPC-Bestätigung ist kein unabhängiger Freigabe-, Holder-Ende- oder Wiederholungsnachweis. Aktueller Prozess-, Ressourcen-, Besitz- und Berechtigungsstatus: unbekannt.\n\nAusgewählte Installation: %s\nHistorische Root-Operation: %s\nHistorischer Profil-SHA-256: %s\n\n"),install,operation,digest);
    if(record->unit_error){if(r==NX_OK)r=append(v,word(v,"Original unit allocation unavailable (%d). No absence or retirement inferred.\n\n","Originale Unit-Zuordnung nicht verfügbar (%d). Keine Abwesenheit oder Freigabe abgeleitet.\n\n"),record->unit_error);}
    else{
        const struct ae_unit_allocation_snapshot *u=&record->unit;char id[33],recipe[65],placement[65];hex_text(u->allocation.id,16,id);hex_text(u->allocation.recipe,32,recipe);hex_text(u->allocation.placement,32,placement);
        if(r==NX_OK)r=append(v,word(v,"Recorded allocation: %s\nRecipe SHA-256: %s\nPlacement SHA-256: %s\nPrepared Unix ms: %" PRId64 "\nSelection witnessed: %s\n",
            "Aufgezeichnete Zuordnung: %s\nRezept-SHA-256: %s\nPlatzierungs-SHA-256: %s\nVorbereitete Unix-ms: %" PRId64 "\nAuswahl bezeugt: %s\n"),id,recipe,placement,u->allocation.prepared_unix_ms,u->witnessed?word(v,"yes","ja"):word(v,"no","nein"));
        if(r==NX_OK&&u->witnessed){char selection[65];hex_text(u->selection,32,selection);r=append(v,word(v,"Selection SHA-256: %s\nWitnessed Unix ms: %" PRId64 "\n","Auswahl-SHA-256: %s\nBezeugte Unix-ms: %" PRId64 "\n"),selection,u->witnessed_unix_ms);}
        if(r==NX_OK)r=append(v,"\n");
    }
    if(record->custody_error){if(r==NX_OK)r=append(v,word(v,"Original connection custody unavailable (%d). End/retirement remains unproven.\n","Originaler Verbindungsbesitz nicht verfügbar (%d). Ende/Freigabe bleibt unbelegt.\n"),record->custody_error);}
    else{
        const struct ae_unit_custody_snapshot *c=&record->custody;char boot[33],bus[33],raw_label[1023];hex_text(c->origin.boot,16,boot);hex_text(c->origin.bus,16,bus);
        const char *label=c->origin.manager_label;size_t label_bytes=strlen(label);int raw=nx_utf8_validate(label,label_bytes,NULL)!=NX_OK;
        if(raw){hex_text((const uint8_t *)label,label_bytes,raw_label);label=raw_label;}
        if(r==NX_OK)r=append(v,word(v,"Recorded custody boot: %s\nRecorded bus: %s\nRecorded issuer: %s\nRecorded manager: %s\nRecorded manager label (%s): %s\nCustody prepared Unix ms: %" PRId64 "\nNode count: %u\nRecorded effects: %u\n\n",
            "Aufgezeichneter Besitz-Boot: %s\nAufgezeichneter Bus: %s\nAufgezeichneter Aussteller: %s\nAufgezeichneter Manager: %s\nAufgezeichnetes Manager-Label (%s): %s\nBesitz vorbereitet, Unix-ms: %" PRId64 "\nKnotenanzahl: %u\nAufgezeichnete Effekte: %u\n\n"),boot,bus,c->origin.issuer,c->origin.manager,raw?word(v,"raw bytes, hex","Rohbytes, hex"):"UTF-8",label,c->prepared_unix_ms,c->node_count,c->count);
        const char *en[]={"Subscribe","Ref","Start","Unref","Unsubscribe"},*de[]={"Abonnieren","Referenz","Start","Referenz lösen","Abonnement beenden"};
        for(unsigned i=0;i<c->count&&r==NX_OK;++i){const struct ae_unit_custody_step *s=&c->steps[i];
            r=append(v,word(v,"Recorded effect %u: %s\nNode index: %u\nIntended Unix ms: %" PRId64 "\nAcknowledgement recorded: %s\n","Aufgezeichneter Effekt %u: %s\nKnotenindex: %u\nBeabsichtigte Unix-ms: %" PRId64 "\nBestätigung aufgezeichnet: %s\n"),i+1,word(v,en[s->effect-1],de[s->effect-1]),s->node,s->intended_unix_ms,s->confirmed?word(v,"yes","ja"):word(v,"no","nein"));
            if(r==NX_OK&&s->confirmed)r=append(v,word(v,"Acknowledged Unix ms: %" PRId64 "\n","Bestätigte Unix-ms: %" PRId64 "\n"),s->confirmed_unix_ms);
            if(r==NX_OK)r=append(v,"\n");
        }
    }
    return r;
}
static const char *receipt_error(const swg_view *v,int error){
    switch(error){
    case -ENOENT:return word(v,"not recorded","nicht aufgezeichnet");
    case -EPROTONOSUPPORT:return word(v,"original schema unsupported","Originalschema nicht unterstützt");
    case -ESTALE:return word(v,"original Root runtime differs","ursprüngliche Root-Runtime weicht ab");
    case -ENODATA:return word(v,"retained starter evidence unavailable","gespeicherter Starter-Nachweis fehlt");
    case -EXDEV:return word(v,"reservation belongs to another request","Reservierung gehört zu anderer Anfrage");
    default:return word(v,"observation unavailable","Beobachtung nicht verfügbar");
    }
}
static nx_result receipt_id(swg_view *v,const char *en,const char *de,const uint8_t *bytes,size_t size){
    char value[65];hex_text(bytes,size,value);return append(v,"%s: %s\n",word(v,en,de),value);
}
static nx_result receipts_body(swg_view *v){
    const swh_record *records=NULL;size_t count=0;nx_result r=histories(v,&records,&count);
    if(r!=NX_OK||v->item<SWG_HISTORY_ROWS||v->item-SWG_HISTORY_ROWS>=count)return NX_ERROR_STATE;
    const swh_record *record=&records[v->item-SWG_HISTORY_ROWS];const struct ae_execution_profile *p=&record->profile;
    const sw_scope *scope=&sw_snapshot_data(v->snapshot)->scope;v->body[0]=0;
    r=append(v,word(v,
        "Recorded receipts only. Current process, ownership, holder drain and permission state: unknown. Historical quiet, release or withdrawal is no current authority.\n\n",
        "Nur aufgezeichnete Belege. Aktueller Prozess-, Besitz-, Holder-Ende- und Berechtigungsstatus: unbekannt. Historische Ruhe, Freigabe oder Rücknahme erteilt keine aktuelle Befugnis.\n\n"));
    const uint8_t *ids[]={record->observed.install.bytes,record->digest,p->system_runtime,scope->context.runtime_incarnation,scope->identity.authority_epoch,scope->context.boot_id,scope->account,p->broker_epoch};
    const char *en[]={"Selected installation","Historical profile SHA-256","Original Root runtime","Query Root runtime","Query Root epoch","Query boot","Query account","Historical broker epoch"};
    const char *de[]={"Ausgewählte Installation","Historischer Profil-SHA-256","Ursprüngliche Root-Runtime","Abfrage-Root-Runtime","Abfrage-Root-Epoche","Abfrage-Boot","Abfragekonto","Historische Broker-Epoche"};
    for(size_t i=0;i<8&&r==NX_OK;++i)r=receipt_id(v,en[i],de[i],ids[i],i==1?32:16);
    if(r==NX_OK)r=append(v,word(v,"Query UID: %" PRIu32 "\nQuery catalogue revision: %" PRId64 "\n\n","Abfrage-UID: %" PRIu32 "\nAbfrage-Katalogrevision: %" PRId64 "\n\n"),scope->os_uid,scope->context.revision);
    if(record->reservation_error){if(r==NX_OK)r=append(v,word(v,"Reservation unavailable (%d): %s. No absence or completion inferred.\n\n","Reservierung nicht verfügbar (%d): %s. Keine Abwesenheit oder Fertigstellung abgeleitet.\n\n"),record->reservation_error,receipt_error(v,record->reservation_error));}
    else{
        const struct ae_reservation_receipt *a=&record->reservation;const struct ae_dispatch_snapshot *d=&a->dispatch;
        const uint8_t *values[]={a->plan,a->broker_runtime,a->lease,d->operation,d->launch,a->intent,a->fingerprint};
        const char *names[]={"Recorded plan","Recorded broker runtime","Recorded lease","Recorded Root operation","Recorded launch","Intent SHA-256","Request fingerprint SHA-256"};
        const char *german[]={"Aufgezeichneter Plan","Aufgezeichnete Broker-Runtime","Aufgezeichnete Lease","Aufgezeichnete Root-Operation","Aufgezeichneter Launch","Intent-SHA-256","Anfragefingerabdruck-SHA-256"};
        for(size_t i=0;i<7&&r==NX_OK;++i)r=receipt_id(v,names[i],german[i],values[i],i<5?16:32);
        if(r==NX_OK)r=append(v,word(v,"Recorded dispatch state: %s\nRecorded unit: %s\nDispatch revision: %" PRId64 "\nInstall gate revision: %" PRId64 "\nRecorded withdrawal: %s\nInvocation present: %s\n",
            "Aufgezeichneter Dispatch-Status: %s\nAufgezeichnete Unit: %s\nDispatch-Revision: %" PRId64 "\nInstallationssperrrevision: %" PRId64 "\nRücknahme aufgezeichnet: %s\nInvocation vorhanden: %s\n"),d->state,d->unit,d->revision,d->install_gate_revision,a->withdrawn?word(v,"yes","ja"):word(v,"no","nein"),d->invocation_present?word(v,"yes","ja"):word(v,"no","nein"));
        if(r==NX_OK&&d->invocation_present)r=receipt_id(v,"Recorded invocation","Aufgezeichnete Invocation",d->invocation,16);
        if(r==NX_OK)r=append(v,word(v,"Close cause present: %s\n","Beendigungsgrund vorhanden: %s\n"),a->cause_present?word(v,"yes","ja"):word(v,"no","nein"));
        if(r==NX_OK&&a->cause_present)r=receipt_id(v,"Recorded cause epoch","Aufgezeichnete Grund-Epoche",a->cause_epoch,16);
        if(r==NX_OK&&a->cause_present)r=receipt_id(v,"Recorded cause operation","Aufgezeichnete Grund-Operation",a->cause,16);
        if(r==NX_OK)r=append(v,"\n");
    }
    const char *kinds[]={"ROOT_CLOSING","USER_QUIET","ROOT_RELEASED"};
    for(unsigned i=0;i<SWH_RUN_RECEIPTS&&r==NX_OK;++i){const swh_run_receipt *a=&record->run[i];
        r=append(v,word(v,"Root-local receipt %u: %s\n","Root-lokaler Beleg %u: %s\n"),i+1,kinds[i]);
        if(a->error){if(r==NX_OK)r=append(v,word(v,"Unavailable (%d): %s. State remains unknown.\n\n","Nicht verfügbar (%d): %s. Status bleibt unbekannt.\n\n"),a->error,receipt_error(v,a->error));continue;}
        const struct ae_run_receipt *c=&a->value;
        if(r==NX_OK)r=append(v,word(v,"Recorded kind: %d\nRecorded revision: %" PRId64 "\nRecorded Unix ms: %" PRId64 "\n","Aufgezeichnete Art: %d\nAufgezeichnete Revision: %" PRId64 "\nAufgezeichnete Unix-ms: %" PRId64 "\n"),(int)c->kind,c->revision,c->unix_ms);
        const uint8_t *values[]={c->profile_digest,c->invocation,c->previous_digest,c->evidence_digest,a->digest};
        const char *names[]={"Receipt profile SHA-256","Receipt invocation",i==0?"Start result SHA-256":"Previous receipt SHA-256","Receipt evidence SHA-256","Receipt SHA-256"};
        const char *german[]={"Beleg-Profil-SHA-256","Beleg-Invocation",i==0?"Startergebnis-SHA-256":"Vorheriger Beleg-SHA-256","Beleg-Nachweis-SHA-256","Beleg-SHA-256"};
        for(size_t j=0;j<5&&r==NX_OK;++j)r=receipt_id(v,names[j],german[j],values[j],j==1?16:32);
        if(r==NX_OK)r=append(v,"\n");
    }
    return r;
}
static nx_result recovery_body(swg_view *v){
    const swh_record *records=NULL;size_t count=0;nx_result r=histories(v,&records,&count);
    if(r!=NX_OK||v->item<SWG_HISTORY_ROWS||v->item-SWG_HISTORY_ROWS>=count)return NX_ERROR_STATE;
    const swh_record *record=&records[v->item-SWG_HISTORY_ROWS];
    const sw_scope *scope=&sw_snapshot_data(v->snapshot)->scope;v->body[0]=0;
    r=append(v,"%s",word(v,
        "Selective recovery metadata only. Missing records are unknown, not proof of absence, completion or holder drain. Current process, ownership and permission state: unknown.\n\n",
        "Nur selektive Recovery-Metadaten. Fehlende Einträge sind unbekannt und belegen weder Abwesenheit, Fertigstellung noch Holder-Ende. Aktueller Prozess-, Besitz- und Berechtigungsstatus: unbekannt.\n\n"));
    const uint8_t *ids[]={record->observed.install.bytes,record->digest,record->profile.system_runtime,scope->context.runtime_incarnation,scope->context.authority_epoch,scope->context.boot_id};
    const char *en[]={"Selected installation","Historical profile SHA-256","Original Root runtime","Query Root runtime","Query Root epoch","Query boot"};
    const char *de[]={"Ausgewählte Installation","Historischer Profil-SHA-256","Ursprüngliche Root-Runtime","Abfrage-Root-Runtime","Abfrage-Root-Epoche","Abfrage-Boot"};
    for(size_t i=0;i<6&&r==NX_OK;++i)r=receipt_id(v,en[i],de[i],ids[i],i==1?32:16);
    if(r==NX_OK)r=append(v,word(v,"Query catalogue revision: %" PRId64 "\n","Abfrage-Katalogrevision: %" PRId64 "\n"),scope->context.revision);
    if(record->recovery_error){if(r==NX_OK)r=append(v,word(v,"Recovery query unavailable (%d): %s. State remains unknown.\n\n","Recovery-Abfrage nicht verfügbar (%d): %s. Status bleibt unbekannt.\n\n"),record->recovery_error,receipt_error(v,record->recovery_error));}
    else if(r==NX_OK)r=append(v,word(v,"Query recovery schema: %u\n\n","Abfrage-Recovery-Schema: %u\n\n"),record->recovery_schema);
    const char *roles[]={"Root operation","Code lease","Dispatch","Run closure","Execution handoff"};
    const char *german[]={"Root-Operation","Code-Lease","Dispatch","Run-Abschluss","Ausführungsübergabe"};
    for(unsigned i=0;i<SWH_RECOVERY_LINKS&&r==NX_OK;++i){const swh_recovery_link *link=&record->recovery[i];
        r=append(v,"%s\n",word(v,roles[i],german[i]));
        if(link->error){if(r==NX_OK)r=append(v,word(v,"Unavailable (%d): %s. State remains unknown.\n\n","Nicht verfügbar (%d): %s. Status bleibt unbekannt.\n\n"),link->error,receipt_error(v,link->error));continue;}
        const struct ae_recovery_record *value=&link->value;
        if(r==NX_OK)r=append(v,word(v,"Recorded kind: %d\n","Aufgezeichnete Art: %d\n"),(int)value->kind);
        if(r==NX_OK)r=receipt_id(v,"Recorded ID","Aufgezeichnete ID",value->id,16);
        if(r==NX_OK&&value->has_related)r=receipt_id(v,"Recorded related ID","Aufgezeichnete Bezugs-ID",value->related,16);
        else if(r==NX_OK)r=append(v,"%s",word(v,"Recorded related ID: none\n","Aufgezeichnete Bezugs-ID: keine\n"));
        if(r==NX_OK)r=append(v,word(v,
            "Recorded revision: %" PRId64 "\nRecorded unfinished steps: %" PRId64 "\nRecorded state: %s\nRecorded detail: %s\nRecorded phase: %s\nRecorded reason: %s\n\n",
            "Aufgezeichnete Revision: %" PRId64 "\nAufgezeichnete offene Schritte: %" PRId64 "\nAufgezeichneter Status: %s\nAufgezeichnetes Detail: %s\nAufgezeichnete Phase: %s\nAufgezeichneter Grund: %s\n\n"),
            value->revision,value->unfinished_steps,value->state,value->detail,value->phase,value->reason);
    }
    return r;
}
static nx_result declaration_body(swg_view *v){
    const amh_document *document=manifest(v);if(!document)return NX_ERROR_STATE;
    const struct ae_fixture *d=amh_document_data(document);uint32_t file=0;int is_action=0;
    if(v->item>=SWG_FILE_ROWS&&v->item-SWG_FILE_ROWS<d->file_count)file=(uint32_t)(v->item-SWG_FILE_ROWS);
    else if(v->item>=SWG_ACTION_ROWS&&v->item-SWG_ACTION_ROWS<d->action_count){is_action=1;file=d->actions[v->item-SWG_ACTION_ROWS].file_index;}
    else return NX_ERROR_STATE;
    char artifact[65],install[33],digest[65];hex_text(amh_document_artifact(document),32,artifact);hex_text(v->detail.bytes,16,install);
    const struct ae_fixture_file *f=&d->files[file];hex_text(f->sha256,32,digest);v->body[0]=0;
    nx_result r=append(v,word(v,
        "Selected installation: %s\nArtifact: %s\n\nDeclared metadata only. Installed file contents, ownership, permission and running-process state are unknown. No file is opened and no action is executed.\n\n",
        "Ausgewählte Installation: %s\nArtefakt: %s\n\nNur deklarierte Metadaten. Installierter Dateiinhalt, Besitz, Berechtigung und Prozesszustand sind unbekannt. Keine Datei wird geöffnet und keine Aktion ausgeführt.\n\n"),install,artifact);
    if(is_action&&r==NX_OK){const struct ae_fixture_action *a=&d->actions[v->item-SWG_ACTION_ROWS];
        r=append(v,word(v,"Declared action: %.*s\nExecutable (argv[0]): %.*s\nLiteral arguments: %" PRIu32 "\n",
            "Deklarierte Aktion: %.*s\nProgramm (argv[0]): %.*s\nLiterale Argumente: %" PRIu32 "\n"),(int)a->id.size,a->id.data,(int)f->path.size,f->path.data,a->argument_count);
        for(uint32_t i=0;i<a->argument_count&&r==NX_OK;++i)r=append(v,word(v,"Argument %" PRIu32 " (%zu UTF-8 bytes):\n%.*s\n","Argument %" PRIu32 " (%zu UTF-8-Bytes):\n%.*s\n"),i+1,a->arguments[i].size,(int)a->arguments[i].size,a->arguments[i].data);
        if(r==NX_OK)r=append(v,"\n");
    }
    if(r==NX_OK)r=append(v,word(v,"Declared file: %.*s\nDeclared mode: %04" PRIo32 "\nDeclared size: %" PRIu64 " bytes\nDeclared SHA-256: %s\n",
        "Deklarierte Datei: %.*s\nDeklarierter Modus: %04" PRIo32 "\nDeklarierte Größe: %" PRIu64 " Bytes\nDeklarierter SHA-256: %s\n"),(int)f->path.size,f->path.data,f->mode,f->size,digest);
    return r;
}
static nx_result content_row(swg_view *v,uint64_t id,uint64_t parent,const char *s,size_t n,double width){
    if(v->content_count==CONTENT_ROWS||n>256)return NX_ERROR_LIMIT;
    content_cache *c=&v->contents[v->content_count];*c=(content_cache){.id=id,.parent=parent};
    /* Reserve the expander/indent, scrollbar and both label insets. */
    memcpy(c->name,s,n);c->name[n]=0;nx_result r=layout(v,c->name,width-((id==SWG_FILES||id==SWG_ACTIONS)?52:68),&c->layout);
    if(r==NX_OK){c->height=fmaxf(v->style.text_size+16,(float)nx_text_layout_height(c->layout)+16);++v->content_count;}return r;
}
static nx_result content_layout(swg_view *v,double width){
    if(v->mode==SWG_HISTORY){
        const swh_record *records=NULL;size_t count=0;nx_result r=histories(v,&records,&count);if(r!=NX_OK||!count)return NX_ERROR_STATE;
        for(size_t i=0;i<count&&r==NX_OK;++i){char launch[33],digest[65],name[257];hex_text(records[i].profile.launch,16,launch);hex_text(records[i].digest,32,digest);
            int n=snprintf(name,sizeof name,"%s\n%s\n%s",word(v,"Historical launch","Historischer Launch"),launch,digest);
            r=n<0||(size_t)n>=sizeof name?NX_ERROR_LIMIT:content_row(v,SWG_HISTORY_ROWS+i,SWG_SCROLL,name,(size_t)n,width);}
        if(r!=NX_OK){for(size_t i=0;i<v->content_count;++i)nx_text_layout_destroy(v->contents[i].layout);v->content_count=0;}return r;
    }
    const amh_document *document=manifest(v);if(!document)return NX_ERROR_STATE;
    const struct ae_fixture *d=amh_document_data(document);nx_result r=NX_OK;
    char name[80];snprintf(name,sizeof name,word(v,"Declared files (%" PRIu32 ")","Deklarierte Dateien (%" PRIu32 ")"),d->file_count);r=content_row(v,SWG_FILES,SWG_SCROLL,name,strlen(name),width);
    for(uint32_t i=0;i<d->file_count&&r==NX_OK&&v->files_expanded;++i)r=content_row(v,SWG_FILE_ROWS+i,SWG_FILES,d->files[i].path.data,d->files[i].path.size,width);
    snprintf(name,sizeof name,word(v,"Declared actions (%" PRIu32 ")","Deklarierte Aktionen (%" PRIu32 ")"),d->action_count);if(r==NX_OK)r=content_row(v,SWG_ACTIONS,SWG_SCROLL,name,strlen(name),width);
    for(uint32_t i=0;i<d->action_count&&r==NX_OK&&v->actions_expanded;++i)r=content_row(v,SWG_ACTION_ROWS+i,SWG_ACTIONS,d->actions[i].id.data,d->actions[i].id.size,width);
    if(r!=NX_OK){for(size_t i=0;i<v->content_count;++i)nx_text_layout_destroy(v->contents[i].layout);v->content_count=0;}
    return r;
}
static nx_result content_tree(swg_view *v,nx_rect bounds,int *transition){
    nx_result r=NX_OK;if(!v->content_count)r=content_layout(v,bounds.width);
    float total=0,target=-1,target_height=0;uint64_t focused=v->restore?v->restore:nx_focus_id(v->ctx);
    for(size_t i=0;i<v->content_count;++i){if(v->contents[i].id==focused){target=total;target_height=v->contents[i].height;}total+=v->contents[i].height;}
    if(target>=0&&(target_height<=bounds.height||focused!=v->tree_focus)){
        if(target<v->body_scroll||target_height>bounds.height)v->body_scroll=target;
        else if(target+target_height>v->body_scroll+bounds.height)v->body_scroll=target+target_height-bounds.height;
    }
    v->tree_focus=focused;nx_container_desc tree={0};tree.id=v->mode==SWG_HISTORY?SWG_HISTORY_TREE:SWG_CONTENT_TREE;tree.parent_id=SWG_ROOT;tree.bounds=bounds;tree.name=sp(v->mode==SWG_HISTORY?word(v,"Historical enrollment profiles","Historische Registrierungsprofile"):word(v,"Declared manifest contents","Deklarierter Manifestinhalt"));
    if(r==NX_OK)r=nx_tree(v->ctx,&tree);
    nx_scroll_area_desc area={0};area.id=SWG_SCROLL;area.parent_id=tree.id;area.thumb_y_id=SWG_THUMB;area.axes=NX_SCROLL_Y;
    area.name=sp(v->mode==SWG_HISTORY?word(v,"Historical profile entries","Historische Profileinträge"):word(v,"Manifest entries","Manifesteinträge"));area.viewport=bounds;area.content_size=(nx_size){bounds.width,total};area.wheel_step=v->style.text_size*3;
    float x=0;nx_scroll_info info;if(r==NX_OK)r=nx_scroll_area_begin(v->ctx,&area,&x,&v->body_scroll,&info);
    float y=bounds.y-v->body_scroll;int changed=0;
    for(size_t i=0;i<v->content_count&&r==NX_OK;++i){content_cache *c=&v->contents[i];int *expanded=c->id==SWG_FILES?&v->files_expanded:c->id==SWG_ACTIONS?&v->actions_expanded:NULL;int before=expanded?*expanded:0;
        nx_tree_item_desc row={0};float indent=expanded?0:16;row.control.id=c->id;row.control.parent_id=c->parent;row.control.name=sp(c->name);
        row.control.bounds=(nx_rect){bounds.x+indent,y,bounds.width-16-indent,c->height};row.control.label_layout=c->layout;
        row.control.label_origin=(nx_point){bounds.x+indent+28,y+8};row.control.glyph_cache_bytes=1024*1024;
        if(expanded){row.has_children=1;row.expander_id=c->id==SWG_FILES?SWG_FILES_EXPAND:SWG_ACTIONS_EXPAND;row.expander_bounds=(nx_rect){bounds.x,y,24,c->height};}
        nx_interaction_state state;r=nx_tree_item(v->ctx,&row,expanded,&v->tree_selection,&state);
        if(r==NX_OK&&state.clicked&&!expanded){v->item=c->id;*transition=6;}
        if(expanded&&*expanded!=before)changed=1;
        y+=c->height;
    }
    if(r==NX_OK)r=nx_scroll_area_end(v->ctx);
    if(changed&&r==NX_OK){clear_layouts(v);v->again=1;}
    return r;
}
static nx_result detail_layout(swg_view *v,double width){
    /* Use the existing measured grapheme wrapper per original paragraph. Its
       fallback must not repeatedly reshape the entire preceding multi-line
       document for every scalar of a later long identity/version. Full original
       text remains one read-only semantic snapshot, with no editable caret. */
    clear_paragraphs(v);size_t length=strlen(v->body),start=0;nx_result r=NX_OK;
    while(start<length&&r==NX_OK){
        if(v->paragraph_count==BODY_PARAGRAPHS){r=NX_ERROR_LIMIT;break;}
        size_t end=start;while(end<length&&v->body[end]!='\n')++end;
        paragraph_cache *p=&v->paragraphs[v->paragraph_count];*p=(paragraph_cache){.y=v->body_height};
        const char *text=end==start?" ":v->body+start;size_t bytes=end==start?1:end-start;
        if(!nx_example_small_text_layout(v->fonts,text,bytes,v->style.text_size,width,&p->layout)){r=NX_ERROR_STATE;break;}
        p->height=(float)nx_text_layout_height(p->layout);v->body_height+=p->height;++v->paragraph_count;start=end+1;
    }
    if(r!=NX_OK)clear_paragraphs(v);
    return r;
}
static nx_result frame_once(swg_view *v,float width,float height,swg_intent *intent,int *transition){
    uint64_t focus_before=nx_focus_id(v->ctx);
    const char *labels[4];uint64_t ids[4];size_t buttons=0;int clicked[4]={0};float top=12,button_height=0;nx_result r=NX_OK;
    int apply_valid=1;size_t active_preedit=0;
    if(v->mode==SWG_FILTER){nx_utf8_span query;r=nx_text_field_text(v->query,&query);if(r==NX_OK)apply_valid=valid_query(query,NULL);
        if(v->window&&r==NX_OK){int32_t start=0,end=0;nx_result ime=nx_window_get_ime_preedit(v->window,NULL,0,&active_preedit,&start,&end);if(ime!=NX_OK&&ime!=NX_ERROR_UNSUPPORTED)r=ime;}}
    if(r!=NX_OK)return r;
    status(v);v->compact=width<240||height<160;
    if(v->compact){labels[0]="X";ids[0]=SWG_CLOSE;buttons=1;}
    else if(v->mode==SWG_LIST){labels[0]=word(v,"Filter","Filter");ids[0]=SWG_FILTER_BUTTON;labels[1]=word(v,"Read","Lesen");ids[1]=SWG_REFRESH;labels[2]=word(v,"Close","Schließen");ids[2]=SWG_CLOSE;buttons=3;}
    else if(v->mode==SWG_FILTER){labels[0]=word(v,"Apply","Anwenden");ids[0]=SWG_APPLY;labels[1]=word(v,"Back","Zurück");ids[1]=SWG_BACK;labels[2]=word(v,"Close","Schließen");ids[2]=SWG_CLOSE;buttons=3;}
    else if(v->mode==SWG_DETAIL){labels[0]=word(v,"Back","Zurück");ids[0]=SWG_BACK;labels[1]=word(v,"Contents","Inhalt");ids[1]=SWG_CONTENTS_BUTTON;labels[2]=word(v,"History","Historie");ids[2]=SWG_HISTORY_BUTTON;labels[3]=word(v,"Close","Schließen");ids[3]=SWG_CLOSE;buttons=4;}
    else if(v->mode==SWG_HISTORY_DETAIL){labels[0]=word(v,"Back","Zurück");ids[0]=SWG_BACK;labels[1]=word(v,"Unit links","Unit-Bezüge");ids[1]=SWG_UNIT_LINKS_BUTTON;labels[2]=word(v,"Receipts","Belege");ids[2]=SWG_RECEIPTS_BUTTON;labels[3]=word(v,"Close","Schließen");ids[3]=SWG_CLOSE;buttons=4;}
    else if(v->mode==SWG_RECEIPTS){labels[0]=word(v,"Back","Zurück");ids[0]=SWG_BACK;labels[1]=word(v,"Recovery links","Recovery-Bezüge");ids[1]=SWG_RECOVERY_BUTTON;labels[2]=word(v,"Close","Schließen");ids[2]=SWG_CLOSE;buttons=3;}
    else {labels[0]=word(v,"Back","Zurück");ids[0]=SWG_BACK;labels[1]=word(v,"Close","Schließen");ids[1]=SWG_CLOSE;buttons=2;}
    float margin=v->compact?4:12,gap=8,cell=(width-margin*2-gap*(float)(buttons-1))/(float)buttons;
    if(v->cache_width!=width){clear_layouts(v);v->cache_width=width;}
    for(size_t i=0;i<buttons&&r==NX_OK;++i){if(!v->buttons[i])r=layout(v,labels[i],cell-16,&v->buttons[i]);if(v->buttons[i])button_height=fmaxf(button_height,(float)nx_text_layout_height(v->buttons[i])+16);}
    if(v->compact)button_height=fminf(button_height,height-8);
    nx_virtual_range range={0};float list_top=0,viewport=0;
    if(!v->compact&&v->mode==SWG_LIST){
        if(!v->status_layout&&r==NX_OK)r=layout(v,v->status,width-24,&v->status_layout);
        list_top=top+button_height+12+(v->status_layout?(float)nx_text_layout_height(v->status_layout):0)+12;viewport=height-list_top-12;
        if(viewport<16)v->compact=1;
        else if(r==NX_OK){
            /* A new snapshot starts with estimated heights. Keep the intended
               full-ID focus anchor visible after measuring preceding rows. */
            size_t anchor=focused_index(v,v->restore);
            for(unsigned pass=0;pass<8&&r==NX_OK;++pass){
                r=row_measure(v,width-40,viewport,&range);if(r!=NX_OK||anchor==SIZE_MAX)break;
                double offset=0;r=nx_virtual_list_offset_at(v->index,anchor,&offset);if(r!=NX_OK||v->scroll==(float)offset)break;
                v->scroll=(float)offset;if(pass==7)r=NX_ERROR_LIMIT;
            }
        }
    }
    if(r!=NX_OK)return r;
    r=nx_begin_frame(v->ctx,width,height,0);if(r!=NX_OK)return r;
    nx_semantic_desc root={0};root.id=SWG_ROOT;root.role=NX_ROLE_WINDOW;root.bounds=(nx_rect){0,0,width,height};root.name=sp(word(v,"Selected app observations","Ausgewählte App-Beobachtungen"));root.description=sp(word(v,"Read-only selected targets, not a complete inventory. Administrative actions belong to the owner.","Schreibgeschützte ausgewählte Ziele, kein vollständiges Inventar. Verwaltungsaktionen gehören dem Owner."));
    r=nx_semantics_add(v->ctx,&root,NULL);
    for(size_t i=0;i<buttons&&r==NX_OK;++i){int disabled=(ids[i]==SWG_REFRESH&&v->state==SW_READING)||(ids[i]==SWG_APPLY&&(!apply_valid||active_preedit));
        disabled=disabled||(ids[i]==SWG_CONTENTS_BUTTON&&!manifest(v));
        disabled=disabled||(ids[i]==SWG_HISTORY_BUTTON&&!have_history(v));
        r=button(v,ids[i],(nx_rect){margin+(cell+gap)*(float)i,margin,cell,button_height},ids[i]==SWG_CLOSE?word(v,"Close","Schließen"):labels[i],v->buttons[i],disabled,&clicked[i]);}
    if(v->compact){
        if(r==NX_OK)r=text(v,SWG_STATUS,SWG_ROOT,(nx_rect){4,button_height+8,width-8,fmaxf(0,height-button_height-12)},word(v,"Enlarge the window to inspect targets","Fenster zum Prüfen der Ziele vergrößern"),v->status,NULL,0);
    }else if(v->mode==SWG_LIST){
        uint32_t flags=v->state==SW_READING?NX_SEM_BUSY:v->state==SW_UNAVAILABLE?NX_SEM_INVALID:0;
        if(r==NX_OK)r=text(v,SWG_STATUS,SWG_ROOT,(nx_rect){12,top+button_height+12,width-24,list_top-top-button_height-24},v->status,v->status,v->status_layout,flags);
        nx_scroll_area_desc area={0};area.id=SWG_SCROLL;area.parent_id=SWG_ROOT;area.thumb_y_id=SWG_THUMB;area.axes=NX_SCROLL_Y;area.name=sp(word(v,"Selected app targets","Ausgewählte App-Ziele"));
        area.viewport=(nx_rect){12,list_top,width-24,viewport};area.content_size=(nx_size){width-24,(float)range.total_extent};area.wheel_step=v->style.text_size*3;
        float x=0;nx_scroll_info info;v->viewport_height=viewport;
        if(r==NX_OK)r=nx_scroll_area_begin(v->ctx,&area,&x,&v->scroll,&info);
        sw_id selected;int have=sw_owner_selection(v->owner,&selected);
        for(size_t j=0;j<v->cached&&r==NX_OK;++j){row_cache *c=&v->rows[j];double offset; r=nx_virtual_list_offset_at(v->index,c->index,&offset);if(r!=NX_OK)break;
            const sw_result *d=sw_snapshot_data(v->snapshot);const sw_row *record=&d->rows[v->matches[c->index]];
            nx_action_desc action={0};action.variant=NX_ACTION_TILE;action.control.id=SWG_ROWS+v->matches[c->index];action.control.parent_id=SWG_SCROLL;
            action.control.name=sp(c->text);action.control.bounds=(nx_rect){info.viewport.x,info.viewport.y+(float)offset-v->scroll,info.viewport.width-16,c->height};
            action.control.label_layout=c->layout;action.control.label_origin=(nx_point){action.control.bounds.x+8,action.control.bounds.y+8};action.control.glyph_cache_bytes=1024*1024;
            action.selected=have&&!memcmp(selected.bytes,record->install.bytes,16);nx_interaction_state state;
            r=nx_action(v->ctx,&action,&state);
            if(r==NX_OK&&state.clicked){v->detail=record->install;*transition=3;}
        }
        if(r==NX_OK&&!v->count)r=text(v,SWG_EMPTY,SWG_SCROLL,(nx_rect){info.viewport.x,info.viewport.y,info.viewport.width,info.viewport.height},
            word(v,v->snapshot?"No matching selected targets":"No observation available",v->snapshot?"Keine passenden ausgewählten Ziele":"Keine Beobachtung vorhanden"),"",NULL,0);
        if(r==NX_OK)r=nx_scroll_area_end(v->ctx);
    }else{
        float y=top+button_height+12,available=height-y-12;v->viewport_height=available;
        if(v->mode==SWG_FILTER){
            nx_text_field_desc field={0};field.id=SWG_QUERY;field.parent_id=SWG_ROOT;field.bounds=(nx_rect){12,y,width-24,v->style.text_size*2+16};field.name=sp(word(v,"App filter","App-Filter"));field.fonts=v->fonts;field.pixels_per_em=v->style.text_size;field.glyph_cache_bytes=1024*1024;field.validate=valid_query;field.ime_window=v->window;
            if(v->window&&r==NX_OK)r=nx_text_clipboard_for_window(v->window,&field.clipboard);
            nx_interaction_state interaction;int valid=0,submitted=0;
            if(r==NX_OK)r=nx_text_field_widget_described(v->ctx,&field,v->query,sp(word(v,"At most 128 UTF-8 bytes. Apply searches component, observation and full install ID. Empty shows all selected targets.","Höchstens 128 UTF-8-Bytes. Anwenden sucht Komponente, Beobachtung und volle Installations-ID. Leer zeigt alle ausgewählten Ziele.")),&interaction,&valid,&submitted);
            if(valid!=apply_valid)v->again=1;
            size_t preedit=0;int32_t a=0,b=0;if(v->window&&r==NX_OK){nx_result ime=nx_window_get_ime_preedit(v->window,NULL,0,&preedit,&a,&b);if(ime!=NX_OK&&ime!=NX_ERROR_UNSUPPORTED)r=ime;}
            if(valid&&!preedit&&(submitted||clicked[0]))*transition=2;
            const char *help=valid?word(v,"Component, observation or full install ID.\n128 UTF-8 bytes; empty shows all.","Komponente, Beobachtung oder volle Installations-ID.\n128 UTF-8-Bytes; leer zeigt alle."):word(v,"Invalid filter: at most 128 UTF-8 bytes, no line breaks","Ungültiger Filter: höchstens 128 UTF-8-Bytes, keine Zeilenumbrüche");
            if(strcmp(v->body,help)){snprintf(v->body,sizeof v->body,"%s",help);nx_text_layout_destroy(v->body_layout);v->body_layout=NULL;}
            if(!v->body_layout&&r==NX_OK)r=layout(v,help,width-44,&v->body_layout);
            nx_scroll_area_desc hint={0};hint.id=SWG_SCROLL;hint.parent_id=SWG_ROOT;hint.thumb_y_id=SWG_THUMB;hint.axes=NX_SCROLL_Y;
            hint.name=sp(word(v,"Filter help","Filterhilfe"));hint.viewport=(nx_rect){12,y+field.bounds.height+8,width-24,fmaxf(1,available-field.bounds.height-8)};
            hint.content_size=(nx_size){width-24,v->body_layout?(float)nx_text_layout_height(v->body_layout)+8:0};float x=0;nx_scroll_info info;
            if(r==NX_OK)r=nx_scroll_area_begin(v->ctx,&hint,&x,&v->body_scroll,&info);
            if(r==NX_OK)r=text(v,SWG_STATUS,SWG_SCROLL,(nx_rect){info.viewport.x+4,info.viewport.y-v->body_scroll,info.viewport.width-20,hint.content_size.height},help,help,v->body_layout,valid?0:NX_SEM_INVALID);
            if(r==NX_OK)r=nx_scroll_area_end(v->ctx);
        }else if(v->mode==SWG_CONTENTS||v->mode==SWG_HISTORY){
            if(r==NX_OK)r=content_tree(v,(nx_rect){12,y,width-24,available},transition);
        }else{
            if(!v->paragraph_count){if(v->mode==SWG_DECLARATION)r=declaration_body(v);else if(v->mode==SWG_HISTORY_DETAIL)r=history_body(v);else if(v->mode==SWG_UNIT_LINKS)r=unit_links_body(v);else if(v->mode==SWG_RECEIPTS)r=receipts_body(v);else if(v->mode==SWG_RECOVERY_LINKS)r=recovery_body(v);else{detail_body(v);if(r==NX_OK)r=component_details(v);if(r==NX_OK)r=history_summary(v);}if(r==NX_OK)r=detail_layout(v,width-44);}
            nx_scroll_area_desc area={0};area.id=SWG_SCROLL;area.parent_id=SWG_ROOT;area.thumb_y_id=SWG_THUMB;area.axes=NX_SCROLL_Y;area.name=sp(word(v,"App observation details","App-Beobachtungsdetails"));area.viewport=(nx_rect){12,y,width-24,available};area.content_size=(nx_size){width-24,v->body_height+16};
            float x=0;nx_scroll_info info;if(r==NX_OK)r=nx_scroll_area_begin(v->ctx,&area,&x,&v->body_scroll,&info);
            if(r==NX_OK)r=text(v,SWG_DETAIL_TEXT,SWG_SCROLL,(nx_rect){info.viewport.x+4,info.viewport.y-v->body_scroll,info.viewport.width-20,area.content_size.height},v->mode==SWG_DECLARATION?word(v,"Declared entry details","Details des deklarierten Eintrags"):v->mode==SWG_HISTORY_DETAIL?word(v,"Historical enrollment details","Historische Registrierungsdetails"):v->mode==SWG_UNIT_LINKS?word(v,"Recorded unit association details","Details aufgezeichneter Unit-Bezüge"):v->mode==SWG_RECEIPTS?word(v,"Recorded receipt details","Details aufgezeichneter Belege"):v->mode==SWG_RECOVERY_LINKS?word(v,"Recorded recovery relationship details","Details aufgezeichneter Recovery-Bezüge"):word(v,"Selected installation details","Details der ausgewählten Installation"),v->body,NULL,0);
            nx_color color;if(r==NX_OK)r=nx_theme_color(v->ctx,NX_COLOR_TEXT,&color);
            for(size_t i=0;i<v->paragraph_count&&r==NX_OK;++i){const paragraph_cache *p=&v->paragraphs[i];float py=info.viewport.y-v->body_scroll+p->y;
                if(py+p->height>info.viewport.y&&py<info.viewport.y+info.viewport.height)r=nx_draw_text_layout(v->ctx,p->layout,(nx_point){info.viewport.x+4,py},color,1024*1024);}
            if(r==NX_OK)r=nx_scroll_area_end(v->ctx);
        }
    }
    if(r==NX_OK&&v->window){float side=fminf(12,fminf(width,height));nx_widget_desc grip={0};grip.semantic.id=SWG_RESIZE;grip.semantic.parent_id=SWG_ROOT;
        grip.semantic.role=NX_ROLE_CUSTOM;grip.semantic.name=sp(word(v,"Drag to resize window","Ziehen zum Ändern der Fenstergröße"));grip.semantic.bounds=(nx_rect){width-side,height-side,side,side};grip.flags=NX_WIDGET_INTERACTIVE;
        nx_interaction_state interaction;r=nx_widget_register(v->ctx,&grip,&interaction);if(r==NX_OK&&interaction.pointer_pressed)intent->resize=1;
        nx_color color;if(r==NX_OK)r=nx_theme_color(v->ctx,NX_COLOR_BORDER,&color);
        if(r==NX_OK)r=nx_draw_line(v->ctx,(nx_point){width-side+2,height-2},(nx_point){width-2,height-side+2},1,color);}
    if(r==NX_OK)r=nx_end_frame(v->ctx);
    if(r!=NX_OK)return r;
    /* The pointer-only window grip must not discard the content's key focus. */
    if(intent->resize&&!v->restore)v->restore=focus_before;
    if(v->restore){uint32_t node;if(nx_semantics_find(v->ctx,v->restore,&node)==NX_OK){r=nx_focus_set(v->ctx,v->restore);if(r!=NX_OK)return r;v->restore=0;v->again=1;}}
    for(size_t i=0;i<buttons;++i)if(clicked[i]){if(ids[i]==SWG_CLOSE)intent->close=1;else if(ids[i]==SWG_REFRESH)intent->refresh=1;else if(ids[i]==SWG_FILTER_BUTTON)*transition=1;else if(ids[i]==SWG_BACK)*transition=4;else if(ids[i]==SWG_CONTENTS_BUTTON)*transition=5;else if(ids[i]==SWG_HISTORY_BUTTON)*transition=7;else if(ids[i]==SWG_UNIT_LINKS_BUTTON)*transition=8;else if(ids[i]==SWG_RECEIPTS_BUTTON)*transition=9;else if(ids[i]==SWG_RECOVERY_BUTTON)*transition=10;}
    if(v->escape){v->escape=0;if(v->mode!=SWG_LIST)*transition=4;else intent->close=1;}return NX_OK;
}
nx_result swg_frame(swg_view *v,float width,float height,swg_intent *out){
    if(!v||!out||!isfinite(width)||!isfinite(height)||width<48||height<48||width>4096||height>4096)return NX_ERROR_ARGUMENT;
    *out=(swg_intent){0};v->measured=0;
    if(v->width!=width||v->height!=height){
        int compact=width<240||height<160;
        if(compact&&!v->compact){v->resume_focus=nx_focus_id(v->ctx);v->restore=SWG_CLOSE;}
        else if(!compact&&v->compact){v->restore=v->resume_focus?v->resume_focus:v->mode==SWG_DETAIL?SWG_BACK:v->mode==SWG_FILTER?SWG_QUERY:SWG_FILTER_BUTTON;v->resume_focus=0;}
        clear_layouts(v);v->width=width;v->height=height;
    }
    for(unsigned pass=0;pass<12;++pass){v->again=0;int transition=0;swg_intent intent={0};nx_result r=frame_once(v,width,height,&intent,&transition);if(r!=NX_OK)return r;
        if(intent.close||intent.refresh||intent.resize){*out=intent;return NX_OK;}
        if(transition){
            if(transition==1)r=mode(v,SWG_FILTER,SWG_QUERY);
            else if(transition==2){nx_utf8_span query;r=nx_text_field_text(v->query,&query);if(r==NX_OK)r=swg_filter(v,query);}
            else if(transition==3){if(v->epoch!=sw_owner_epoch(v->owner))return NX_ERROR_STATE;r=sw_owner_select(v->owner,v->detail);if(r==NX_OK)r=mode(v,SWG_DETAIL,SWG_BACK);}
            else if(transition==5){if(!manifest(v))return NX_ERROR_STATE;r=mode(v,SWG_CONTENTS,SWG_FILES);if(r==NX_OK){v->files_expanded=1;v->actions_expanded=1;v->tree_selection=0;v->item=0;}}
            else if(transition==6)r=mode(v,v->mode==SWG_HISTORY?SWG_HISTORY_DETAIL:SWG_DECLARATION,SWG_BACK);
            else if(transition==7){if(!have_history(v))return NX_ERROR_STATE;r=mode(v,SWG_HISTORY,SWG_HISTORY_ROWS);if(r==NX_OK){v->tree_selection=0;v->item=0;}}
            else if(transition==8)r=mode(v,SWG_UNIT_LINKS,SWG_BACK);
            else if(transition==9)r=mode(v,SWG_RECEIPTS,SWG_BACK);
            else if(transition==10)r=mode(v,SWG_RECOVERY_LINKS,SWG_BACK);
            else if(v->mode==SWG_RECOVERY_LINKS)r=mode(v,SWG_RECEIPTS,SWG_RECOVERY_BUTTON);
            else if(v->mode==SWG_RECEIPTS)r=mode(v,SWG_HISTORY_DETAIL,SWG_RECEIPTS_BUTTON);
            else if(v->mode==SWG_UNIT_LINKS)r=mode(v,SWG_HISTORY_DETAIL,SWG_UNIT_LINKS_BUTTON);
            else if(v->mode==SWG_HISTORY_DETAIL)r=mode(v,SWG_HISTORY,v->item);
            else if(v->mode==SWG_HISTORY)r=mode(v,SWG_DETAIL,SWG_HISTORY_BUTTON);
            else if(v->mode==SWG_DECLARATION)r=mode(v,SWG_CONTENTS,v->item);
            else if(v->mode==SWG_CONTENTS)r=mode(v,SWG_DETAIL,SWG_CONTENTS_BUTTON);
            else {uint64_t restore=SWG_FILTER_BUTTON;if(v->mode==SWG_DETAIL){size_t i=sw_snapshot_find(v->snapshot,v->detail);if(i!=SIZE_MAX&&focused_index(v,SWG_ROWS+i)!=SIZE_MAX)restore=SWG_ROWS+i;}r=mode(v,SWG_LIST,restore);}
            if(r!=NX_OK)return r;
            continue;
        }
        if(!v->again)return NX_OK;
    }
    return NX_ERROR_LIMIT;
}
size_t swg_matches(const swg_view *v){return v?v->count:0;}
swg_mode swg_current_mode(const swg_view *v){return v?v->mode:SWG_LIST;}
size_t swg_last_row_layouts(const swg_view *v){return v?v->measured:0;}
