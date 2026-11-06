#include "review.h"
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct removal_snapshot {
    removal_plan plan;size_t counts[6],starts[6],*order,references;
    const removal_entry **by_semantic,**by_object;
} removal_snapshot;
struct removal_owner {removal_snapshot *current;removal_request request;unsigned issued;int invalidated;};
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
    removal_snapshot *snapshot;
    uint64_t restore_key,restore_focus;size_t restore_index;double restore_intra,restore_height;
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
static void snapshot_release(removal_snapshot *data){
    if(data && !--data->references){free(data->by_semantic);free(data->by_object);free(data->order);free(data->plan.entries);free(data);}
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
    removal_snapshot *data=calloc(1,sizeof(*data));if(!data){free(owner);return NX_ERROR_MEMORY;}
    data->references=1;owner->current=data;nx_result result=NX_ERROR_MEMORY;
    data->plan=*plan;data->plan.entries=NULL;
    if(plan->count){
        data->plan.entries=malloc(plan->count*sizeof(*plan->entries));
        data->order=malloc(plan->count*sizeof(*data->order));
        data->by_semantic=malloc(plan->count*sizeof(*data->by_semantic));
        data->by_object=malloc(plan->count*sizeof(*data->by_object));
        if(!data->plan.entries || !data->order || !data->by_semantic || !data->by_object)goto failed;
        memcpy(data->plan.entries,plan->entries,plan->count*sizeof(*plan->entries));
    }
    for(size_t i=0;i<plan->count;++i){
        const removal_entry *entry=&data->plan.entries[i];
        if(entry->semantic_id<1000 || !valid_id(entry->object) ||
           entry->disposition<REMOVAL_REMOVE || entry->disposition>REMOVAL_UNKNOWN ||
           !valid_string(entry->label,sizeof(entry->label)) || !valid_string(entry->reason,sizeof(entry->reason))){result=NX_ERROR_ARGUMENT;goto failed;}
        data->by_semantic[i]=data->by_object[i]=entry;++data->counts[entry->disposition];
    }
    if(plan->count>1){
        qsort(data->by_semantic,plan->count,sizeof(*data->by_semantic),compare_semantic);
        for(size_t i=1;i<plan->count;++i)if(data->by_semantic[i-1]->semantic_id==data->by_semantic[i]->semantic_id){result=NX_ERROR_ARGUMENT;goto failed;}
        qsort(data->by_object,plan->count,sizeof(*data->by_object),compare_object);
        for(size_t i=1;i<plan->count;++i)if(!memcmp(data->by_object[i-1]->object,data->by_object[i]->object,16)){result=NX_ERROR_ARGUMENT;goto failed;}
    }
    for(unsigned group=1;group<6;++group)data->starts[group]=data->starts[group-1]+data->counts[group-1];
    size_t next[6];memcpy(next,data->starts,sizeof(next));
    for(size_t i=0;i<plan->count;++i)data->order[next[data->plan.entries[i].disposition]++]=i;
    *out=owner;return NX_OK;
failed:
    snapshot_release(data);free(owner);return result;
}
nx_result removal_owner_refresh(removal_owner *owner,const removal_plan *plan){
    if(!owner || !plan)return NX_ERROR_ARGUMENT;
    const removal_snapshot *old=owner->current;
    if(memcmp(old->plan.plan,plan->plan,16) || memcmp(old->plan.installation,plan->installation,16) || memcmp(old->plan.scope,plan->scope,16))return NX_ERROR_ARGUMENT;
    if(plan->revision<=old->plan.revision)return NX_ERROR_STATE;
    removal_owner *candidate=NULL;nx_result result=removal_owner_create(plan,&candidate);if(result!=NX_OK)return result;
    removal_snapshot *next=candidate->current;
    for(unsigned kind=0;kind<2;++kind){
        size_t i=0,j=0;const removal_entry *const *a=kind?old->by_object:old->by_semantic;
        const removal_entry *const *b=kind?next->by_object:next->by_semantic;
        while(i<old->plan.count && j<next->plan.count){
            int order=kind?memcmp(a[i]->object,b[j]->object,16):(a[i]->semantic_id>b[j]->semantic_id)-(a[i]->semantic_id<b[j]->semantic_id);
            if(!order){
                if(a[i]->semantic_id!=b[j]->semantic_id || memcmp(a[i]->object,b[j]->object,16)){removal_owner_destroy(candidate);return NX_ERROR_ARGUMENT;}
                ++i;++j;
            } else if(order<0)++i;else ++j;
        }
    }
    removal_snapshot *retired=owner->current;owner->current=next;owner->invalidated=0;
    candidate->current=NULL;free(candidate);snapshot_release(retired);return NX_OK;
}
uint64_t removal_owner_revision(const removal_owner *owner){return owner?owner->current->plan.revision:0;}
void removal_owner_destroy(removal_owner *owner){if(owner){snapshot_release(owner->current);free(owner);}}
void removal_owner_invalidate(removal_owner *owner){if(owner)owner->invalidated=1;}
unsigned removal_owner_requests(const removal_owner *owner){return owner?owner->issued:0;}
nx_result removal_review_create(removal_owner *owner,nx_font_collection *fonts,removal_review **out){
    if(!out)return NX_ERROR_ARGUMENT;
    *out=NULL;if(!owner || !fonts)return NX_ERROR_ARGUMENT;
    removal_snapshot *data=owner->current;if(data->references==SIZE_MAX)return NX_ERROR_LIMIT;
    removal_review *view=calloc(1,sizeof(*view));if(!view)return NX_ERROR_MEMORY;
    nx_result result=nx_context_create(NULL,&view->ctx);
    if(result!=NX_OK){free(view);return result;}
    view->capacity=data->plan.count+12;if(view->capacity>VISIBLE_BUDGET)view->capacity=VISIBLE_BUDGET;
    view->scratch=calloc(view->capacity,sizeof(*view->scratch));
    if(!view->scratch){nx_context_destroy(view->ctx);free(view);return NX_ERROR_MEMORY;}
    if(data->plan.count>EAGER_ENTRIES){
        result=nx_virtual_list_create(0,1,NULL,&view->index);
        if(result!=NX_OK){free(view->scratch);nx_context_destroy(view->ctx);free(view);return result;}
    }
    view->owner=owner;view->fonts=fonts;view->match_count=data->plan.count;
    memcpy(view->match_counts,data->counts,sizeof(view->match_counts));memcpy(view->starts,data->starts,sizeof(view->starts));
    result=nx_text_field_create("",0,4096,&view->query);
    if(result!=NX_OK){removal_review_destroy(view);return result;}
    ++data->references;view->snapshot=data;*out=view;return NX_OK;
}
void removal_review_destroy(removal_review *view){if(view){clear_rows(view);nx_text_field_destroy(view->query);free(view->matches);nx_virtual_list_destroy(view->index);free(view->scratch);nx_context_destroy(view->ctx);snapshot_release(view->snapshot);free(view);}}
nx_context *removal_review_context(removal_review *view){return view?view->ctx:NULL;}
uint64_t removal_review_revision(const removal_review *view){return view?view->snapshot->plan.revision:0;}
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
            size_t position=view->snapshot->order[view->matches?view->matches[ordinal]:ordinal];
            if(group)*group=g;
            if(item)*item=position;
            return view->snapshot->plan.entries[position].semantic_id;
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
    nx_search_option *options=NULL;size_t *matches=NULL,count=view->snapshot->plan.count,found=count;
    nx_result result=NX_ERROR_MEMORY;removal_review next=*view;nx_virtual_list *index=NULL;
    memset(next.match_counts,0,sizeof(next.match_counts));memset(next.starts,0,sizeof(next.starts));
    if(query.length && count){
        options=calloc(count,sizeof(*options));matches=malloc(count*sizeof(*matches));
        if(!options || !matches)goto failed;
        for(size_t i=0;i<count;++i){const removal_entry *entry=&view->snapshot->plan.entries[view->snapshot->order[i]];
            options[i].id=entry->semantic_id;options[i].name=span(entry->label);}
        result=nx_search_filter(options,count,query,matches,count,&found);if(result!=NX_OK)goto failed;
    }
    for(size_t i=0;i<found;++i){size_t at=view->snapshot->order[matches?matches[i]:i];
        ++next.match_counts[view->snapshot->plan.entries[at].disposition];}
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
    view->reviewed=0;view->x=view->y=0;view->index_width=0;view->restore_key=0;view->again=1;return NX_OK;
failed:
    free(options);free(matches);nx_virtual_list_destroy(index);return result;
}
static const removal_entry *semantic_entry(const removal_snapshot *data,uint64_t id){
    size_t first=0,last=data->plan.count;
    while(first<last){size_t middle=first+(last-first)/2;uint64_t key=data->by_semantic[middle]->semantic_id;
        if(key<id)first=middle+1;else last=middle;}
    return first<data->plan.count && data->by_semantic[first]->semantic_id==id?data->by_semantic[first]:NULL;
}
nx_result removal_review_refresh(removal_review *view){
    if(!view)return NX_ERROR_ARGUMENT;
    if(view->snapshot==view->owner->current)return NX_OK;
    removal_review *next=NULL;nx_result result=removal_review_create(view->owner,view->fonts,&next);if(result!=NX_OK)return result;
    result=removal_review_filter(next,span(view->filter));if(result!=NX_OK)goto failed;
    nx_theme theme;result=nx_theme_preset(NX_THEME_DARK,&theme);if(result!=NX_OK)goto failed;
    for(unsigned i=0;i<NX_COLOR_TOKEN_COUNT && result==NX_OK;++i)result=nx_theme_color(view->ctx,(nx_color_token)i,&theme.colors[i]);
    for(unsigned i=0;i<NX_METRIC_TOKEN_COUNT && result==NX_OK;++i)result=nx_theme_metric(view->ctx,(nx_metric_token)i,&theme.metrics[i]);
    if(result==NX_OK)result=nx_context_set_theme(next->ctx,&theme);
    float density=1;int reduced=0;
    if(result==NX_OK)result=nx_context_get_text_raster_scale(view->ctx,&density);
    if(result==NX_OK)result=nx_context_get_reduced_motion(view->ctx,&reduced);
    if(result==NX_OK)result=nx_context_set_text_raster_scale(next->ctx,density);
    if(result==NX_OK)result=nx_context_set_reduced_motion(next->ctx,reduced);
    if(result!=NX_OK)goto failed;
    uint64_t anchor=0;double intra=0,anchor_height=0;uint32_t area_index;nx_semantic_view area;size_t count=0;float first_y=0;
    if(nx_semantics_find(view->ctx,20,&area_index)==NX_OK && nx_semantics_get(view->ctx,area_index,&area)==NX_OK &&
       nx_semantics_count(view->ctx,&count)==NX_OK){
        for(size_t i=0;i<count;++i){nx_semantic_view node;
            if(nx_semantics_get(view->ctx,(uint32_t)i,&node)!=NX_OK)continue;
            if(node.parent_id!=20 || (node.role!=NX_ROLE_TEXT && node.role!=NX_ROLE_STATUS) || node.bounds.y+node.bounds.height<=area.bounds.y ||
               node.bounds.y>=area.bounds.y+area.bounds.height || (anchor && node.bounds.y>=first_y))continue;
            anchor=node.id;first_y=node.bounds.y;intra=area.bounds.y-node.bounds.y;if(intra<0)intra=0;anchor_height=node.bounds.height;
        }
    }
    const removal_entry *old_object=anchor>=1000?semantic_entry(view->snapshot,anchor):NULL;
    for(size_t i=0;anchor && i<next->match_count+12;++i){size_t item=0;uint64_t key=row_key(next,i,NULL,&item);
        if(old_object?(key>=1000 && !memcmp(old_object->object,next->snapshot->plan.entries[item].object,16)):key==anchor){
            next->restore_key=key;next->restore_index=i;next->restore_intra=intra;next->restore_height=anchor_height;break;
        }
    }
    next->x=view->x;next->filter_open=view->filter_open;next->filter_error=view->filter_error;next->window=view->window;
    uint64_t focused=nx_focus_id(view->ctx);
    next->restore_focus=view->filter_open && focused>=301 && focused<=303?focused:view->filter_open?301:250;
    /* All allocation and validation has finished before native revocation. */
    if(view->window){result=nx_window_publish_semantics(view->window,NULL);if(result==NX_ERROR_UNSUPPORTED)result=NX_OK;if(result!=NX_OK)goto failed;}
    nx_text_field_destroy(next->query);next->query=view->query;view->query=NULL;
    removal_review previous=*view;*view=*next;*next=previous;removal_review_destroy(next);return NX_OK;
failed:
    removal_review_destroy(next);return result;
}
static nx_result restore_position(removal_review *view,float viewport,const review_row *rows,size_t count){
    if(!view->restore_key)return NX_OK;
    size_t index=view->restore_index;double offset=0,height=0,total=0;nx_result result=NX_OK;
    if(view->index){
        if(view->restore_height>0){
            double begin,end;result=nx_virtual_list_offset_at(view->index,index,&begin);
            if(result==NX_OK)result=nx_virtual_list_offset_at(view->index,index+1,&end);
            if(result==NX_OK && end-begin!=view->restore_height){result=nx_virtual_list_set_height(view->index,index,view->restore_height);view->again=1;}
            view->restore_height=0;
        }
        if(result==NX_OK)result=nx_virtual_list_offset_at(view->index,index,&offset);
        double end=0;if(result==NX_OK)result=nx_virtual_list_offset_at(view->index,index+1,&end);height=end-offset;
        if(result==NX_OK)result=nx_virtual_list_offset_at(view->index,view->match_count+12,&total);
    } else {
        if(index>=count)return NX_ERROR_STATE;
        for(size_t i=0;i<count;++i){if(i<index)offset+=rows[i].height;total+=rows[i].height;}
        height=rows[index].height;
    }
    if(result!=NX_OK)return result;
    double inner=view->restore_intra;if(inner>=height)inner=height>.5?height-.5:0;
    double y=offset+inner,maximum=total>viewport?total-viewport:0;if(y>maximum)y=maximum;
    if(view->y!=(float)y){view->y=(float)y;view->again=1;}
    return NX_OK;
}
static void format_row(removal_review *view,size_t index,const char *status,review_row *row){
    const removal_plan *plan=&view->snapshot->plan;char identity[33],value[256];unsigned group=0;size_t item=0;
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
        size_t count=view->snapshot->counts[group];
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
    if(node->role==NX_ROLE_STATUS && (view->owner->issued || view->pending)){
        char description[160];uint64_t revision=view->owner->issued?view->owner->request.revision:view->snapshot->plan.revision;
        snprintf(description,sizeof(description),"Original request revision %" PRIu64 "; outcome unknown. A newer preview does not rearm this request.",revision);
        nx_semantic_desc status=*node;status.description=span(description);return nx_semantics_add(view->ctx,&status,NULL);
    }
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
    const removal_plan *plan=&view->snapshot->plan;
    const size_t *counts=view->snapshot->counts;int stale=view->snapshot!=view->owner->current;
    int eligible=!stale && !view->owner->issued && !view->pending && !view->owner->invalidated && plan->inventory_complete &&
        !counts[REMOVAL_UNKNOWN] && counts[REMOVAL_REMOVE]>0;
    char filtered_status[160];
    if(view->match_count)snprintf(filtered_status,sizeof(filtered_status),"Showing %zu of %zu objects; request covers the entire plan",view->match_count,plan->count);
    else strcpy(filtered_status,"No matching objects; request covers the entire plan");
    char request_status[160];
    snprintf(request_status,sizeof(request_status),"Request revision %" PRIu64 " handed to owner; outcome unknown",view->owner->request.revision);
    const char *status=stale?(view->owner->issued?"Preview outdated; original request outcome unknown":"Preview outdated; request unavailable"):
        view->owner->issued && view->owner->request.revision!=plan->revision?request_status:
        view->owner->issued || view->pending?"Request handed to owner; outcome unknown":
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
    const char *names[]={"Entire plan reviewed","Close view",stale?"Refresh":"Request removal"};
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
    if(result==NX_OK)result=restore_position(view,info.viewport.height,rows,count);
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
        control.id=i==2 && stale?203:200+i;control.name=span(i==2 && stale?"Refresh preview":names[i]);control.label_layout=labels[i];
        control.bounds=stacked?(nx_rect){16,height-footer+24+button_height+(button_height+12)*(float)(i-1),width-32,button_height}:
            (nx_rect){16+(half+12)*(float)(i-1),height-footer+24+button_height,half,button_height};
        control.label_origin=(nx_point){control.bounds.x+8,control.bounds.y+10};control.disabled=view->filter_open || (i==2 && !stale && (!eligible || !view->reviewed));
        result=nx_button(view->ctx,&control,&state);
        if(result==NX_OK && state.clicked){
            if(i==1)action->kind=REMOVAL_CLOSE;
            else if(action->kind==REMOVAL_NONE && stale)action->kind=REMOVAL_REFRESH;
            else if(action->kind==REMOVAL_NONE && eligible && view->reviewed){
                action->kind=REMOVAL_REQUEST;action->request.revision=plan->revision;
                memcpy(action->request.plan,plan->plan,16);memcpy(action->request.installation,plan->installation,16);
                memcpy(action->request.scope,plan->scope,16);
            }
        }
    }
    if(result==NX_OK && view->filter_open)result=filter_panel(view,width,height,em,text);
    if(result==NX_OK && view->restore_focus){
        result=nx_focus_set(view->ctx,view->restore_focus);
        if(result==NX_ERROR_ARGUMENT)result=nx_focus_set(view->ctx,view->filter_open?301:250);
        if(result==NX_OK){view->restore_focus=0;view->again=1;}
    }
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
            if(saved.kind==REMOVAL_REQUEST){view->owner->request=saved.request;view->owner->issued=1;}
            view->pending=0;view->restore_key=0;*action=saved;clear_rows(view);return NX_OK;
        }
    }
    view->pending=0;clear_rows(view);return result==NX_OK?NX_ERROR_LIMIT:result;
}
