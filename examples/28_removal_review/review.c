#include "review.h"
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct removal_owner {removal_plan plan;size_t counts[6],starts[6],*order;unsigned issued;int invalidated;};
enum { EAGER_ENTRIES=32, VISIBLE_BUDGET=256, SETTLING_BUDGET=16 };
typedef struct review_row {
    uint64_t id;nx_semantic_role role;const char *name;
    char value[768],visual[1024];nx_text_layout *layout;float height;size_t index;
} review_row;
struct removal_review {
    removal_owner *owner;nx_font_collection *fonts;nx_context *ctx;int reviewed,again,pending;
    float x,y,index_width,index_em,wide;nx_virtual_list *index;review_row *scratch;size_t capacity,cached;
    removal_work work;
    size_t *matches,match_count,match_counts[6],starts[6];char filter[129];
    nx_text_field *query;nx_window *window;int filter_open,filter_error,escape;
};
static const char *const group_names[]={"Remove in this scope","Retained: shared dependency","Retained: explicit choice",
    "Independent documents","Backups or remote copies","Unknown or unmanaged"};
static void clear_rows(removal_review *view){
    for(size_t i=0;i<view->cached;++i)if(view->scratch[i].layout)nx_text_layout_destroy(view->scratch[i].layout);
    view->cached=0;memset(view->scratch,0,view->capacity*sizeof(*view->scratch));
}
static nx_utf8_span span(const char *text){return (nx_utf8_span){text,strlen(text)};}
static int valid_string(const char *text,size_t capacity){
    const char *end=memchr(text,0,capacity);size_t invalid=0;
    return end && end!=text && nx_utf8_validate(text,(size_t)(end-text),&invalid)==NX_OK;
}
static int valid_id(const unsigned char *id){unsigned any=0;for(unsigned i=0;i<16;++i)any|=id[i];return any!=0;}
static void hex(const unsigned char *id,char out[33]){
    const char *digits="0123456789abcdef";
    for(unsigned i=0;i<16;++i){out[2*i]=digits[id[i]>>4];out[2*i+1]=digits[id[i]&15];}out[32]=0;
}
static int compare_semantic(const void *left,const void *right){
    const removal_entry *a=*(const removal_entry *const *)left,*b=*(const removal_entry *const *)right;
    return (a->semantic_id>b->semantic_id)-(a->semantic_id<b->semantic_id);
}
static int compare_object(const void *left,const void *right){
    const removal_entry *a=*(const removal_entry *const *)left,*b=*(const removal_entry *const *)right;
    return memcmp(a->object,b->object,16);
}
nx_result removal_owner_create(const removal_plan *plan,removal_owner **out){
    if(!out)return NX_ERROR_ARGUMENT;
    *out=NULL;
    if(!plan || !plan->revision || !valid_id(plan->plan) || !valid_id(plan->installation) ||
       !valid_id(plan->scope) || !valid_string(plan->component,sizeof(plan->component)) ||
       !valid_string(plan->scope_label,sizeof(plan->scope_label)) ||
       (plan->inventory_complete!=0 && plan->inventory_complete!=1) || plan->count>REMOVAL_MAX_ENTRIES ||
       (plan->count && !plan->entries))return NX_ERROR_ARGUMENT;
    removal_owner *owner=calloc(1,sizeof(*owner));if(!owner)return NX_ERROR_MEMORY;
    const removal_entry **sorted=NULL;nx_result result=NX_ERROR_MEMORY;
    owner->plan=*plan;owner->plan.entries=NULL;
    if(plan->count){
        owner->plan.entries=malloc(plan->count*sizeof(*plan->entries));
        owner->order=malloc(plan->count*sizeof(*owner->order));
        sorted=malloc(plan->count*sizeof(*sorted));
        if(!owner->plan.entries || !owner->order || !sorted)goto failed;
        memcpy(owner->plan.entries,plan->entries,plan->count*sizeof(*plan->entries));
    }
    for(size_t i=0;i<plan->count;++i){
        const removal_entry *entry=&owner->plan.entries[i];
        if(entry->semantic_id<1000 || !valid_id(entry->object) ||
           entry->disposition<REMOVAL_REMOVE || entry->disposition>REMOVAL_UNKNOWN ||
           !valid_string(entry->label,sizeof(entry->label)) || !valid_string(entry->reason,sizeof(entry->reason))){result=NX_ERROR_ARGUMENT;goto failed;}
        sorted[i]=entry;++owner->counts[entry->disposition];
    }
    if(plan->count>1){
        qsort(sorted,plan->count,sizeof(*sorted),compare_semantic);
        for(size_t i=1;i<plan->count;++i)if(sorted[i-1]->semantic_id==sorted[i]->semantic_id){result=NX_ERROR_ARGUMENT;goto failed;}
        qsort(sorted,plan->count,sizeof(*sorted),compare_object);
        for(size_t i=1;i<plan->count;++i)if(!memcmp(sorted[i-1]->object,sorted[i]->object,16)){result=NX_ERROR_ARGUMENT;goto failed;}
    }
    free(sorted);sorted=NULL;
    for(unsigned group=1;group<6;++group)owner->starts[group]=owner->starts[group-1]+owner->counts[group-1];
    size_t next[6];memcpy(next,owner->starts,sizeof(next));
    for(size_t i=0;i<plan->count;++i)owner->order[next[owner->plan.entries[i].disposition]++]=i;
    *out=owner;return NX_OK;
failed:
    free(sorted);free(owner->order);free(owner->plan.entries);free(owner);return result;
}
void removal_owner_destroy(removal_owner *owner){if(owner){free(owner->order);free(owner->plan.entries);free(owner);}}
void removal_owner_invalidate(removal_owner *owner){if(owner)owner->invalidated=1;}
unsigned removal_owner_requests(const removal_owner *owner){return owner?owner->issued:0;}
nx_result removal_review_create(removal_owner *owner,nx_font_collection *fonts,removal_review **out){
    if(!out)return NX_ERROR_ARGUMENT;
    *out=NULL;if(!owner || !fonts)return NX_ERROR_ARGUMENT;
    removal_review *view=calloc(1,sizeof(*view));if(!view)return NX_ERROR_MEMORY;
    nx_result result=nx_context_create(NULL,&view->ctx);
    if(result!=NX_OK){free(view);return result;}
    view->capacity=owner->plan.count+12;if(view->capacity>VISIBLE_BUDGET)view->capacity=VISIBLE_BUDGET;
    view->scratch=calloc(view->capacity,sizeof(*view->scratch));
    if(!view->scratch){nx_context_destroy(view->ctx);free(view);return NX_ERROR_MEMORY;}
    if(owner->plan.count>EAGER_ENTRIES){
        result=nx_virtual_list_create(0,1,NULL,&view->index);
        if(result!=NX_OK){free(view->scratch);nx_context_destroy(view->ctx);free(view);return result;}
    }
    view->owner=owner;view->fonts=fonts;view->match_count=owner->plan.count;
    memcpy(view->match_counts,owner->counts,sizeof(view->match_counts));memcpy(view->starts,owner->starts,sizeof(view->starts));
    result=nx_text_field_create("",0,4096,&view->query);
    if(result!=NX_OK){removal_review_destroy(view);return result;}
    *out=view;return NX_OK;
}
void removal_review_destroy(removal_review *view){if(view){clear_rows(view);nx_text_field_destroy(view->query);free(view->matches);nx_virtual_list_destroy(view->index);free(view->scratch);nx_context_destroy(view->ctx);free(view);}}
nx_context *removal_review_context(removal_review *view){return view?view->ctx:NULL;}
void removal_review_bind_window(removal_review *view,nx_window *window){if(view)view->window=window;}
int removal_review_filter_open(const removal_review *view){return view && view->filter_open;}
nx_result removal_review_queue_event(removal_review *view,const nx_event *event){
    if(!view || !event)return NX_ERROR_ARGUMENT;
    nx_result result=nx_context_queue_event(view->ctx,event);
    if(result==NX_OK && event->type==NX_EVENT_KEY_DOWN && event->key==NX_KEY_ESCAPE && !event->repeat)view->escape=1;
    return result;
}
void removal_review_last_work(const removal_review *view,removal_work *out){if(out)*out=view?view->work:(removal_work){0};}
static void metadata(review_row *row,uint64_t id,const char *name,const char *value){
    row->id=id;row->role=NX_ROLE_TEXT;row->name=name;
    snprintf(row->value,sizeof(row->value),"%s",value);
    snprintf(row->visual,sizeof(row->visual),"%s: %s",name,value);
}
/* The immutable grouped index keeps caller order within each disposition. */
static uint64_t row_key(const removal_review *view,size_t index,unsigned *group,size_t *item){
    static const uint64_t fixed[]={10,30,31,32,33,34};
    if(index<6)return fixed[index];
    for(unsigned g=0;g<6;++g){
        size_t header=6+view->starts[g]+g;
        if(index==header){if(group)*group=g;return 100+g;}
        if(index>header && index<=header+view->match_counts[g]){
            size_t ordinal=view->starts[g]+index-header-1;
            size_t position=view->owner->order[view->matches?view->matches[ordinal]:ordinal];
            if(group)*group=g;
            if(item)*item=position;
            return view->owner->plan.entries[position].semantic_id;
        }
    }
    return 0;
}
static nx_result indexed_row(void *user,size_t index,uint64_t *key,double *height){
    removal_review *view=user;*key=row_key(view,index,NULL,NULL);*height=view->index_em*6+12;
    return *key?NX_OK:NX_ERROR_ARGUMENT;
}
static int valid_query(nx_utf8_span query,void *unused){
    (void)unused;
    if(query.length>128 || (!query.data && query.length) || nx_utf8_validate(query.data,query.length,NULL)!=NX_OK)return 0;
    for(size_t i=0;i<query.length;++i)if(!query.data[i] || query.data[i]=='\n' || query.data[i]=='\r')return 0;
    return 1;
}
nx_result removal_review_filter(removal_review *view,nx_utf8_span query){
    if(!view)return NX_ERROR_ARGUMENT;
    if(!valid_query(query,NULL))return query.length>128?NX_ERROR_LIMIT:NX_ERROR_ARGUMENT;
    if(strlen(view->filter)==query.length && (!query.length || !memcmp(view->filter,query.data,query.length)))return NX_OK;
    nx_search_option *options=NULL;size_t *matches=NULL,count=view->owner->plan.count,found=count;
    nx_result result=NX_ERROR_MEMORY;removal_review next=*view;nx_virtual_list *index=NULL;
    memset(next.match_counts,0,sizeof(next.match_counts));memset(next.starts,0,sizeof(next.starts));
    if(query.length && count){
        options=calloc(count,sizeof(*options));matches=malloc(count*sizeof(*matches));
        if(!options || !matches)goto failed;
        for(size_t i=0;i<count;++i){const removal_entry *entry=&view->owner->plan.entries[view->owner->order[i]];
            options[i].id=entry->semantic_id;options[i].name=span(entry->label);}
        result=nx_search_filter(options,count,query,matches,count,&found);if(result!=NX_OK)goto failed;
    }
    for(size_t i=0;i<found;++i){size_t at=view->owner->order[matches?matches[i]:i];
        ++next.match_counts[view->owner->plan.entries[at].disposition];}
    for(unsigned group=1;group<6;++group)next.starts[group]=next.starts[group-1]+next.match_counts[group-1];
    next.matches=matches;next.match_count=found;
    if(view->index){
        result=nx_virtual_list_create(0,1,NULL,&index);if(result!=NX_OK)goto failed;
        double offset=0;if(next.index_em<=0)next.index_em=16;
        result=nx_virtual_list_rebuild(index,found+12,indexed_row,&next,128,&offset);if(result!=NX_OK)goto failed;
    }
    free(options);free(view->matches);nx_virtual_list_destroy(view->index);clear_rows(view);
    view->matches=matches;view->index=index;view->match_count=found;
    memcpy(view->match_counts,next.match_counts,sizeof(view->match_counts));memcpy(view->starts,next.starts,sizeof(view->starts));
    if(query.length)memcpy(view->filter,query.data,query.length);
    view->filter[query.length]=0;
    view->reviewed=0;view->x=view->y=0;view->index_width=0;view->again=1;return NX_OK;
failed:
    free(options);free(matches);nx_virtual_list_destroy(index);return result;
}
static void format_row(removal_review *view,size_t index,const char *status,review_row *row){
    const removal_plan *plan=&view->owner->plan;char identity[33],value[256];unsigned group=0;size_t item=0;
    row->index=index;row->id=row_key(view,index,&group,&item);row->role=NX_ROLE_TEXT;
    if(index==0){row->role=NX_ROLE_STATUS;snprintf(row->visual,sizeof(row->visual),"%s",status);row->name=row->visual;}
    else if(index==1)metadata(row,30,"Component",plan->component);
    else if(index==2){
        hex(plan->plan,identity);snprintf(value,sizeof(value),"%s / revision %" PRIu64,identity,plan->revision);
        metadata(row,31,"Plan",value);snprintf(row->visual,sizeof(row->visual),"Plan: %.16s %.16s / revision %" PRIu64,identity,identity+16,plan->revision);
    } else if(index==3){
        hex(plan->installation,identity);metadata(row,32,"Installation",identity);
        snprintf(row->visual,sizeof(row->visual),"Installation: %.16s %.16s",identity,identity+16);
    } else if(index==4){
        hex(plan->scope,identity);snprintf(value,sizeof(value),"%s (%s)",plan->scope_label,identity);metadata(row,33,"Scope",value);
        snprintf(row->visual,sizeof(row->visual),"Scope: %s (%.16s %.16s)",plan->scope_label,identity,identity+16);
    } else if(index==5)metadata(row,34,"Storage limits","Logical sizes do not promise reclaimed space, secure erasure, or deletion of backups or remote copies.");
    else if(row->id<1000){
        size_t count=view->owner->counts[group];
        if(view->filter[0])snprintf(value,sizeof(value),"%zu of %zu objects shown",view->match_counts[group],count);
        else snprintf(value,sizeof(value),"%zu %s",count,count==1?"object":"objects");
        metadata(row,100+group,group_names[group],value);
    } else {
        const removal_entry *entry=&plan->entries[item];row->name=entry->label;hex(entry->object,identity);
        snprintf(row->value,sizeof(row->value),"%s. %s\nObject: %s\nLogical bytes: %" PRIu64,group_names[group],entry->reason,identity,entry->logical_bytes);
        snprintf(row->visual,sizeof(row->visual),"%s\n%s. %s\nObject: %.16s %.16s\nLogical bytes: %" PRIu64,
            entry->label,group_names[group],entry->reason,identity,identity+16,entry->logical_bytes);
    }
}
static nx_result add_row(removal_review *view,const nx_semantic_desc *node){
    /* Values are readable through native Text, without inventing a caret or
       exposing edits to the owner's immutable plan. Status remains a status. */
    return node->role==NX_ROLE_TEXT?
        nx_semantics_add_readonly_text_snapshot_without_caret(view->ctx,node,node->id,NULL):
        nx_semantics_add(view->ctx,node,NULL);
}
static nx_result row_semantics(removal_review *view,const review_row *row,float top,float width){
    double offset=0,next=0;nx_result result=nx_virtual_list_offset_at(view->index,row->index,&offset);
    if(result==NX_OK)result=nx_virtual_list_offset_at(view->index,row->index+1,&next);
    nx_semantic_desc node={row->id,20,row->role,{16-view->x,top+(float)(offset-view->y),width,(float)(next-offset)},
        span(row->name),{0},span(row->value),NX_SEM_READONLY,0};
    return result==NX_OK?add_row(view,&node):result;
}
static nx_result virtual_body(removal_review *view,float top,float viewport_height,const char *status,nx_color text){
    nx_virtual_range range,anchor;nx_result result=nx_virtual_list_range(view->index,view->y,viewport_height,1,&range);
    if(result==NX_OK)result=nx_virtual_list_range(view->index,view->y,viewport_height,0,&anchor);
    if(result!=NX_OK)return result;
    if(range.end-range.first>view->capacity)return NX_ERROR_LIMIT;
    double old_y=range.effective_offset;int bottom=old_y>0 && old_y+viewport_height>=range.total_extent-.5;
    review_row *active[VISIBLE_BUDGET];size_t active_count=0;
    for(size_t index=range.first;index<range.end;++index){
        review_row *row=NULL;
        for(size_t i=0;i<view->cached;++i)if(view->scratch[i].index==index){row=&view->scratch[i];break;}
        if(!row){
            if(view->cached==view->capacity)return NX_ERROR_LIMIT;
            row=&view->scratch[view->cached++];format_row(view,index,status,row);
        } else if(index==0 && strcmp(row->name,status)){
            nx_text_layout_destroy(row->layout);row->layout=NULL;format_row(view,index,status,row);
        }
        if(!row->layout){
            result=nx_text_layout_create(view->fonts,row->visual,strlen(row->visual),NX_BIDI_LTR,view->index_em,view->index_width-56,&row->layout);
            if(result!=NX_OK)return result;
            ++view->work.row_layouts;row->height=(float)nx_text_layout_height(row->layout)+12;
        }
        active[active_count++]=row;
        if(nx_text_layout_width(row->layout)>view->wide){view->wide=(float)nx_text_layout_width(row->layout);view->again=1;}
        double offset,next;result=nx_virtual_list_offset_at(view->index,index,&offset);
        if(result==NX_OK)result=nx_virtual_list_offset_at(view->index,index+1,&next);
        if(result!=NX_OK)return result;
        if(next-offset!=row->height){
            result=nx_virtual_list_set_height(view->index,index,row->height);if(result!=NX_OK)return result;
            view->again=1;
        }
    }
    double total,new_anchor;result=nx_virtual_list_offset_at(view->index,view->match_count+12,&total);
    if(result==NX_OK)result=nx_virtual_list_offset_at(view->index,anchor.first,&new_anchor);
    if(result!=NX_OK)return result;
    double maximum=total>viewport_height?total-viewport_height:0;
    double y=bottom?maximum:new_anchor+old_y-anchor.first_offset;if(y<0)y=0;if(y>maximum)y=maximum;
    if(view->y!=(float)y){view->y=(float)y;view->again=1;}
    view->work.indexed_extent=total;
    /* Global identities and all six counts stay queryable without publishing an
     * object-sized semantic tree. Object nodes belong to the visible range. */
    for(unsigned i=0;i<12;++i){
        review_row summary={0};size_t index=i<6?i:6+view->starts[i-6]+i-6;
        format_row(view,index,status,&summary);result=row_semantics(view,&summary,top,view->wide);
        if(result!=NX_OK)return result;
    }
    for(size_t i=0;i<active_count;++i){
        review_row *row=active[i];
        if(row->id>=1000){result=row_semantics(view,row,top,view->wide);if(result!=NX_OK)return result;}
        double offset;result=nx_virtual_list_offset_at(view->index,row->index,&offset);
        if(result==NX_OK)result=nx_draw_text_layout(view->ctx,row->layout,(nx_point){16-view->x,top+(float)(offset-view->y)},text,1024*1024);
        if(result!=NX_OK)return result;
    }
    return NX_OK;
}
static nx_result filter_panel(removal_review *view,float width,float height,float em,nx_color text){
    float wide=width-32;if(wide>560)wide=560;
    const char *strings[]={"Filter preview",view->filter_error?"Filter unavailable; try again.":
        "Names only. Empty shows all. Full plan stays in scope.","Apply filter","Cancel"};
    nx_text_layout *layouts[4]={0};nx_result result=NX_OK;
    for(unsigned i=0;i<4 && result==NX_OK;++i)
        result=nx_text_layout_create(view->fonts,strings[i],strlen(strings[i]),NX_BIDI_LTR,em,i<2?wide-24:(wide-36)/2-16,&layouts[i]);
    if(result!=NX_OK)goto done;
    float title_height=(float)nx_text_layout_height(layouts[0]),help_height=(float)nx_text_layout_height(layouts[1]);
    float button=em+20,panel_height=title_height+help_height+2*button+60;
    if(panel_height>height-16){result=NX_ERROR_LIMIT;goto done;}
    float x=(width-wide)/2,y=(height-panel_height)/2;
    nx_overlay_desc modal={0};modal.panel.id=300;modal.panel.parent_id=1;modal.backdrop_id=304;
    modal.panel.bounds=(nx_rect){x,y,wide,panel_height};modal.panel.name=span(strings[0]);modal.dismiss_on_escape=1;
    int dismissed=0,opened=0,apply=0,cancel=0,submitted=0,valid=0;
    result=nx_modal_begin(view->ctx,&modal,&dismissed);opened=result==NX_OK;
    if(result==NX_OK)result=nx_draw_text_layout(view->ctx,layouts[0],(nx_point){x+12,y+12},text,1024*1024);
    nx_text_field_desc field={0};field.id=301;field.parent_id=300;field.name=span("Object name filter");
    field.bounds=(nx_rect){x+12,y+20+title_height,wide-24,button};field.fonts=view->fonts;
    field.pixels_per_em=em;field.glyph_cache_bytes=1024*1024;field.validate=valid_query;field.ime_window=view->window;
    if(view->window && result==NX_OK)result=nx_text_clipboard_for_window(view->window,&field.clipboard);
    nx_interaction_state state;
    const char *description="At most 128 UTF-8 bytes. Apply to filter names; empty shows all. Requests always cover the entire plan.";
    if(result==NX_OK)result=nx_text_field_widget_described(view->ctx,&field,view->query,span(description),&state,&valid,&submitted);
    float help_y=field.bounds.y+button+8;
    nx_semantic_desc help={305,300,NX_ROLE_TEXT,{x+12,help_y,wide-24,help_height},span(strings[1]),{0},{0},NX_SEM_READONLY,0};
    if(result==NX_OK)result=nx_semantics_add(view->ctx,&help,NULL);
    if(result==NX_OK)result=nx_draw_text_layout(view->ctx,layouts[1],(nx_point){x+12,help_y},text,1024*1024);
    for(unsigned i=0;i<2 && result==NX_OK;++i){
        nx_control_desc control={0};control.id=302+i;control.parent_id=300;control.name=span(strings[2+i]);
        control.bounds=(nx_rect){x+12+(wide-12)/2*(float)i,y+panel_height-button-12,(wide-36)/2,button};
        control.label_layout=layouts[2+i];control.label_origin=(nx_point){control.bounds.x+8,control.bounds.y+10};
        control.glyph_cache_bytes=1024*1024;control.disabled=i==0 && !valid;
        result=nx_button(view->ctx,&control,&state);if(result==NX_OK && state.clicked){if(i)cancel=1;else apply=1;}
    }
    if(opened){nx_result end=nx_overlay_end(view->ctx);if(result==NX_OK)result=end;}
    if(result==NX_OK && !dismissed && !cancel && valid && (apply || submitted)){
        nx_utf8_span query;result=nx_text_field_text(view->query,&query);
        if(result==NX_OK){
            nx_result filtered=removal_review_filter(view,query);
            if(filtered==NX_OK)cancel=1;
            else {view->filter_error=1;view->again=1;}
        }
    }
    if(result==NX_OK && (dismissed || cancel)){
        /* A dismissed scope is already absent from the SDK's active path;
           it may have opened and dismissed within this same input batch. */
        if(!dismissed)result=nx_overlay_close(view->ctx);
        if(result==NX_OK){view->filter_open=0;view->again=1;}
    }
done:
    for(unsigned i=0;i<4;++i)if(layouts[i])nx_text_layout_destroy(layouts[i]);
    return result;
}
static nx_result frame_once(removal_review *view,float width,float height,removal_action *action){
    if(!view || !action)return NX_ERROR_ARGUMENT;
    *action=(removal_action){0};if(width<420 || height<320)return NX_ERROR_LIMIT;
    const removal_plan *plan=&view->owner->plan;
    const size_t *counts=view->owner->counts;
    int eligible=!view->owner->issued && !view->pending && !view->owner->invalidated && plan->inventory_complete &&
        !counts[REMOVAL_UNKNOWN] && counts[REMOVAL_REMOVE]>0;
    char filtered_status[160];
    if(view->match_count)snprintf(filtered_status,sizeof(filtered_status),"Showing %zu of %zu objects; request covers the entire plan",view->match_count,plan->count);
    else strcpy(filtered_status,"No matching objects; request covers the entire plan");
    const char *status=view->owner->issued || view->pending?"Request handed to owner; outcome unknown":
        view->owner->invalidated?"Plan no longer current; request unavailable":
        !plan->inventory_complete || counts[REMOVAL_UNKNOWN]?"Inventory incomplete; request unavailable":
        !counts[REMOVAL_REMOVE]?"No removals listed; request unavailable":view->filter[0]?filtered_status:"Preview only; no removal has started";
    int large=view->index!=NULL;review_row *rows=view->scratch;size_t count=0;
    view->again=0;
    if(!large){
        /* Modal focus/close may need another internal frame. Eager layouts
           are rebuilt per pass, unlike the measured virtual-row cache. */
        clear_rows(view);
        for(size_t index=0;index<view->match_count+12;++index)format_row(view,index,status,&rows[count++]);
        view->cached=count;
    }
    nx_text_layout *title=NULL,*filter_label=NULL,*labels[3]={0};float em=0,total=0,content_width=width-56;
    nx_result result=nx_theme_metric(view->ctx,NX_METRIC_FONT_SIZE,&em);
    const char *filter_text=view->filter[0]?"Filter active":"Filter";
    if(result==NX_OK)result=nx_text_layout_create(view->fonts,filter_text,strlen(filter_text),NX_BIDI_LTR,em,width-32,&filter_label);
    if(result==NX_OK)result=nx_text_layout_create(view->fonts,"Removal review",14,NX_BIDI_LTR,em,width-64-nx_text_layout_width(filter_label),&title);
    for(size_t i=0;i<count && result==NX_OK;++i){
        result=nx_text_layout_create(view->fonts,rows[i].visual,strlen(rows[i].visual),NX_BIDI_LTR,em,width-56,&rows[i].layout);
        if(result==NX_OK){
            ++view->work.row_layouts;
            rows[i].height=(float)nx_text_layout_height(rows[i].layout)+12;total+=rows[i].height;
            if(nx_text_layout_width(rows[i].layout)>content_width)content_width=(float)nx_text_layout_width(rows[i].layout);
        }
    }
    const char *names[]={"Entire plan reviewed","Close view","Request removal"};
    for(unsigned i=0;i<3 && result==NX_OK;++i)
        result=nx_text_layout_create(view->fonts,names[i],strlen(names[i]),NX_BIDI_LTR,em,width-64,&labels[i]);
    if(result!=NX_OK)goto cleanup;
    float button_height=em+20,half=(width-44)/2;
    int stacked=nx_text_layout_width(labels[1])+16>half || nx_text_layout_width(labels[2])+16>half;
    float footer=button_height+(stacked?2*button_height+12:button_height)+36;
    float header=(float)nx_text_layout_height(title);if(header<em+12)header=em+12;
    float top=24+header;
    if(height-top-footer<24){result=NX_ERROR_LIMIT;goto cleanup;}
    if(large){
        if(view->index_width!=width || view->index_em!=em){
            double offset=view->y;view->index_width=width;view->index_em=em;view->wide=width-56;
            result=nx_virtual_list_rebuild(view->index,view->match_count+12,indexed_row,view,height-top-footer,&offset);
            if(result!=NX_OK){view->index_width=0;goto cleanup;}
            view->y=(float)offset;
        }
        double extent;result=nx_virtual_list_offset_at(view->index,view->match_count+12,&extent);
        if(result!=NX_OK)goto cleanup;
        total=(float)extent;content_width=view->wide;
    }
    result=nx_begin_frame(view->ctx,width,height,0);if(result!=NX_OK)goto cleanup;
    nx_color text;result=nx_theme_color(view->ctx,NX_COLOR_TEXT,&text);
    nx_semantic_desc root={1,0,NX_ROLE_WINDOW,{0,0,width,height},span("Removal review"),
        span("Owner-supplied preview. Requesting does not authorize, accept or complete removal."),{0},0,0};
    if(result==NX_OK)result=nx_semantics_add(view->ctx,&root,NULL);
    if(result==NX_OK)result=nx_draw_text_layout(view->ctx,title,(nx_point){16,8},text,1024*1024);
    nx_control_desc filter={0};filter.id=250;filter.parent_id=1;filter.name=span("Filter preview");
    float filter_width=(float)nx_text_layout_width(filter_label)+16;
    filter.bounds=(nx_rect){width-16-filter_width,8,filter_width,em+12};filter.label_layout=filter_label;
    filter.label_origin=(nx_point){filter.bounds.x+8,filter.bounds.y+6};filter.glyph_cache_bytes=1024*1024;
    nx_interaction_state filter_state;
    if(result==NX_OK)result=nx_button(view->ctx,&filter,&filter_state);
    if(result==NX_OK && filter_state.clicked){
        result=nx_text_field_set_text(view->query,view->filter,strlen(view->filter));
        if(result==NX_OK){view->filter_open=1;view->filter_error=0;view->again=1;}
    }
    if(result==NX_OK && view->escape){
        view->escape=0;if(!view->filter_open)action->kind=REMOVAL_CLOSE;
    }
    nx_scroll_area_desc area={0};area.id=20;area.parent_id=1;area.name=span("Complete removal preview");
    area.viewport=(nx_rect){16,top,width-32,height-top-footer};area.content_size=(nx_size){content_width,total};
    area.thumb_x_id=22;area.thumb_y_id=21;area.axes=NX_SCROLL_X|NX_SCROLL_Y;nx_scroll_info info;int opened=0;
    if(result==NX_OK){result=nx_scroll_area_begin(view->ctx,&area,&view->x,&view->y,&info);opened=result==NX_OK;}
    if(result==NX_OK && large)result=virtual_body(view,info.viewport.y,info.viewport.height,status,text);
    float y=top-view->y;
    for(size_t i=0;!large && i<count && result==NX_OK;++i){
        review_row *row=&rows[i];nx_semantic_desc node={row->id,20,row->role,{16-view->x,y,content_width,row->height},
            span(row->name),{0},span(row->value),NX_SEM_READONLY,0};
        result=add_row(view,&node);
        if(result==NX_OK)result=nx_draw_text_layout(view->ctx,row->layout,(nx_point){16-view->x,y},text,1024*1024);
        y+=row->height;
    }
    if(opened){nx_result end=nx_scroll_area_end(view->ctx);if(result==NX_OK)result=end;}
    nx_control_desc control={0};control.id=200;control.parent_id=1;
    control.bounds=(nx_rect){16,height-footer+12,width-32,button_height};control.name=span(names[0]);
    control.label_layout=labels[0];control.label_origin=(nx_point){control.bounds.x+button_height+8,control.bounds.y+10};
    control.glyph_cache_bytes=1024*1024;control.disabled=!eligible || view->filter_open;
    nx_interaction_state state;
    if(result==NX_OK)result=nx_checkbox(view->ctx,&control,&view->reviewed,&state);
    for(unsigned i=1;i<3 && result==NX_OK;++i){
        control.id=200+i;control.name=span(names[i]);control.label_layout=labels[i];
        control.bounds=stacked?(nx_rect){16,height-footer+24+button_height+(button_height+12)*(float)(i-1),width-32,button_height}:
            (nx_rect){16+(half+12)*(float)(i-1),height-footer+24+button_height,half,button_height};
        control.label_origin=(nx_point){control.bounds.x+8,control.bounds.y+10};control.disabled=view->filter_open || (i==2 && (!eligible || !view->reviewed));
        result=nx_button(view->ctx,&control,&state);
        if(result==NX_OK && state.clicked){
            if(i==1)action->kind=REMOVAL_CLOSE;
            else if(action->kind==REMOVAL_NONE && eligible && view->reviewed){
                action->kind=REMOVAL_REQUEST;action->request.revision=plan->revision;
                memcpy(action->request.plan,plan->plan,16);memcpy(action->request.installation,plan->installation,16);
                memcpy(action->request.scope,plan->scope,16);
            }
        }
    }
    if(result==NX_OK && view->filter_open)result=filter_panel(view,width,height,em,text);
    {nx_result end=nx_end_frame(view->ctx);if(result==NX_OK)result=end;}
    if(result==NX_OK && action->kind==REMOVAL_REQUEST)view->pending=1;
cleanup:
    if(result!=NX_OK)*action=(removal_action){0};
    if(title)nx_text_layout_destroy(title);
    if(filter_label)nx_text_layout_destroy(filter_label);
    for(unsigned i=0;i<3;++i)if(labels[i])nx_text_layout_destroy(labels[i]);
    return result;
}
nx_result removal_review_frame(removal_review *view,float width,float height,removal_action *action){
    if(!view || !action)return NX_ERROR_ARGUMENT;
    *action=(removal_action){0};view->work=(removal_work){0};view->pending=0;clear_rows(view);
    removal_action saved={0};nx_result result=NX_OK;
    for(unsigned pass=0;pass<SETTLING_BUDGET;++pass){
        removal_action next={0};++view->work.settling_frames;
        result=frame_once(view,width,height,&next);if(result!=NX_OK)break;
        if(next.kind!=REMOVAL_NONE && saved.kind==REMOVAL_NONE)saved=next;
        if(!view->again){
            if(saved.kind==REMOVAL_REQUEST)view->owner->issued=1;
            view->pending=0;*action=saved;clear_rows(view);return NX_OK;
        }
    }
    view->pending=0;clear_rows(view);return result==NX_OK?NX_ERROR_LIMIT:result;
}
