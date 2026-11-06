#include "review.h"
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct removal_owner {removal_plan plan;unsigned issued;int invalidated;};
struct removal_review {removal_owner *owner;nx_font_collection *fonts;nx_context *ctx;int reviewed;float x,y;};
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
nx_result removal_owner_create(const removal_plan *plan,removal_owner **out){
    if(!out)return NX_ERROR_ARGUMENT;
    *out=NULL;
    if(!plan || !plan->revision || !valid_id(plan->plan) || !valid_id(plan->installation) ||
       !valid_id(plan->scope) || !valid_string(plan->component,sizeof(plan->component)) ||
       !valid_string(plan->scope_label,sizeof(plan->scope_label)) ||
       (plan->inventory_complete!=0 && plan->inventory_complete!=1) || plan->count>REMOVAL_MAX_ENTRIES)return NX_ERROR_ARGUMENT;
    for(size_t i=0;i<plan->count;++i){
        const removal_entry *entry=&plan->entries[i];
        if(entry->semantic_id<1000 || !valid_id(entry->object) ||
           entry->disposition<REMOVAL_REMOVE || entry->disposition>REMOVAL_UNKNOWN ||
           !valid_string(entry->label,sizeof(entry->label)) || !valid_string(entry->reason,sizeof(entry->reason)))return NX_ERROR_ARGUMENT;
        for(size_t j=0;j<i;++j)if(plan->entries[j].semantic_id==entry->semantic_id ||
            !memcmp(plan->entries[j].object,entry->object,16))return NX_ERROR_ARGUMENT;
    }
    removal_owner *owner=calloc(1,sizeof(*owner));if(!owner)return NX_ERROR_MEMORY;
    owner->plan=*plan;*out=owner;return NX_OK;
}
void removal_owner_destroy(removal_owner *owner){free(owner);}
void removal_owner_invalidate(removal_owner *owner){if(owner)owner->invalidated=1;}
unsigned removal_owner_requests(const removal_owner *owner){return owner?owner->issued:0;}
nx_result removal_review_create(removal_owner *owner,nx_font_collection *fonts,removal_review **out){
    if(!out)return NX_ERROR_ARGUMENT;
    *out=NULL;if(!owner || !fonts)return NX_ERROR_ARGUMENT;
    removal_review *view=calloc(1,sizeof(*view));if(!view)return NX_ERROR_MEMORY;
    nx_result result=nx_context_create(NULL,&view->ctx);
    if(result!=NX_OK){free(view);return result;}
    view->owner=owner;view->fonts=fonts;*out=view;return NX_OK;
}
void removal_review_destroy(removal_review *view){if(view){nx_context_destroy(view->ctx);free(view);}}
nx_context *removal_review_context(removal_review *view){return view?view->ctx:NULL;}
typedef struct review_row {
    uint64_t id;nx_semantic_role role;const char *name;
    char value[768],visual[1024];nx_text_layout *layout;float height;
} review_row;
static void metadata(review_row *row,uint64_t id,const char *name,const char *value){
    row->id=id;row->role=NX_ROLE_TEXT;row->name=name;
    snprintf(row->value,sizeof(row->value),"%s",value);
    snprintf(row->visual,sizeof(row->visual),"%s: %s",name,value);
}
nx_result removal_review_frame(removal_review *view,float width,float height,removal_action *action){
    if(!view || !action)return NX_ERROR_ARGUMENT;
    *action=(removal_action){0};if(width<420 || height<320)return NX_ERROR_LIMIT;
    const removal_plan *plan=&view->owner->plan;
    size_t counts[6]={0};for(size_t i=0;i<plan->count;++i)++counts[plan->entries[i].disposition];
    int eligible=!view->owner->issued && !view->owner->invalidated && plan->inventory_complete &&
        !counts[REMOVAL_UNKNOWN] && counts[REMOVAL_REMOVE]>0;
    const char *status=view->owner->issued?"Request handed to owner; outcome unknown":
        view->owner->invalidated?"Plan no longer current; request unavailable":
        !plan->inventory_complete || counts[REMOVAL_UNKNOWN]?"Inventory incomplete; request unavailable":
        !counts[REMOVAL_REMOVE]?"No removals listed; request unavailable":"Preview only; no removal has started";
    const char *groups[]={"Remove in this scope","Retained: shared dependency","Retained: explicit choice",
        "Independent documents","Backups or remote copies","Unknown or unmanaged"};
    review_row rows[REMOVAL_MAX_ENTRIES+13]={0};size_t count=0;
    rows[count].id=10;rows[count].role=NX_ROLE_STATUS;rows[count].name=status;
    snprintf(rows[count++].visual,sizeof(rows[0].visual),"%s",status);
    metadata(&rows[count++],30,"Component",plan->component);
    char value[256],identity[33];
    hex(plan->plan,identity);snprintf(value,sizeof(value),"%s / revision %" PRIu64,identity,plan->revision);
    metadata(&rows[count++],31,"Plan",value);
    snprintf(rows[count-1].visual,sizeof(rows[0].visual),"Plan: %.16s %.16s / revision %" PRIu64,identity,identity+16,plan->revision);
    hex(plan->installation,identity);metadata(&rows[count++],32,"Installation",identity);
    snprintf(rows[count-1].visual,sizeof(rows[0].visual),"Installation: %.16s %.16s",identity,identity+16);
    hex(plan->scope,identity);snprintf(value,sizeof(value),"%s (%s)",plan->scope_label,identity);
    metadata(&rows[count++],33,"Scope",value);
    snprintf(rows[count-1].visual,sizeof(rows[0].visual),"Scope: %s (%.16s %.16s)",plan->scope_label,identity,identity+16);
    metadata(&rows[count++],34,"Storage limits","Logical sizes do not promise reclaimed space, secure erasure, or deletion of backups or remote copies.");
    for(unsigned group=0;group<6;++group){
        snprintf(value,sizeof(value),"%zu %s",counts[group],counts[group]==1?"object":"objects");
        metadata(&rows[count++],100+group,groups[group],value);
        for(size_t i=0;i<plan->count;++i){
            const removal_entry *entry=&plan->entries[i];if((unsigned)entry->disposition!=group)continue;
            review_row *row=&rows[count++];row->id=entry->semantic_id;row->role=NX_ROLE_TEXT;row->name=entry->label;
            hex(entry->object,identity);
            snprintf(row->value,sizeof(row->value),"%s. %s\nObject: %s\nLogical bytes: %" PRIu64,
                groups[group],entry->reason,identity,entry->logical_bytes);
            snprintf(row->visual,sizeof(row->visual),"%s\n%s. %s\nObject: %.16s %.16s\nLogical bytes: %" PRIu64,
                entry->label,groups[group],entry->reason,identity,identity+16,entry->logical_bytes);
        }
    }
    nx_text_layout *title=NULL,*labels[3]={0};float em=0,total=0,content_width=width-56;
    nx_result result=nx_theme_metric(view->ctx,NX_METRIC_FONT_SIZE,&em);
    if(result==NX_OK)result=nx_text_layout_create(view->fonts,"Removal review",14,NX_BIDI_LTR,em,width-32,&title);
    for(size_t i=0;i<count && result==NX_OK;++i){
        result=nx_text_layout_create(view->fonts,rows[i].visual,strlen(rows[i].visual),NX_BIDI_LTR,em,width-56,&rows[i].layout);
        if(result==NX_OK){
            rows[i].height=(float)nx_text_layout_height(rows[i].layout)+12;total+=rows[i].height;
            if(nx_text_layout_width(rows[i].layout)>content_width)content_width=(float)nx_text_layout_width(rows[i].layout);
        }
    }
    const char *names[]={"Scope reviewed","Close view","Request removal"};
    for(unsigned i=0;i<3 && result==NX_OK;++i)
        result=nx_text_layout_create(view->fonts,names[i],strlen(names[i]),NX_BIDI_LTR,em,width-64,&labels[i]);
    if(result!=NX_OK)goto cleanup;
    float button_height=em+20,half=(width-44)/2;
    int stacked=nx_text_layout_width(labels[1])+16>half || nx_text_layout_width(labels[2])+16>half;
    float footer=button_height+(stacked?2*button_height+12:button_height)+36;
    float top=24+(float)nx_text_layout_height(title);
    if(height-top-footer<24){result=NX_ERROR_LIMIT;goto cleanup;}
    result=nx_begin_frame(view->ctx,width,height,0);if(result!=NX_OK)goto cleanup;
    nx_color text;result=nx_theme_color(view->ctx,NX_COLOR_TEXT,&text);
    nx_semantic_desc root={1,0,NX_ROLE_WINDOW,{0,0,width,height},span("Removal review"),
        span("Owner-supplied preview. Requesting does not authorize, accept or complete removal."),{0},0,0};
    if(result==NX_OK)result=nx_semantics_add(view->ctx,&root,NULL);
    if(result==NX_OK)result=nx_draw_text_layout(view->ctx,title,(nx_point){16,8},text,1024*1024);
    nx_scroll_area_desc area={0};area.id=20;area.parent_id=1;area.name=span("Complete removal preview");
    area.viewport=(nx_rect){16,top,width-32,height-top-footer};area.content_size=(nx_size){content_width,total};
    area.thumb_x_id=22;area.thumb_y_id=21;area.axes=NX_SCROLL_X|NX_SCROLL_Y;nx_scroll_info info;int opened=0;
    if(result==NX_OK){result=nx_scroll_area_begin(view->ctx,&area,&view->x,&view->y,&info);opened=result==NX_OK;}
    float y=top-view->y;
    for(size_t i=0;i<count && result==NX_OK;++i){
        review_row *row=&rows[i];nx_semantic_desc node={row->id,20,row->role,{16-view->x,y,content_width,row->height},
            span(row->name),{0},span(row->value),NX_SEM_READONLY,0};
        result=nx_semantics_add(view->ctx,&node,NULL);
        if(result==NX_OK)result=nx_draw_text_layout(view->ctx,row->layout,(nx_point){16-view->x,y},text,1024*1024);
        y+=row->height;
    }
    if(opened){nx_result end=nx_scroll_area_end(view->ctx);if(result==NX_OK)result=end;}
    nx_control_desc control={0};control.id=200;control.parent_id=1;
    control.bounds=(nx_rect){16,height-footer+12,width-32,button_height};control.name=span(names[0]);
    control.label_layout=labels[0];control.label_origin=(nx_point){control.bounds.x+button_height+8,control.bounds.y+10};
    control.glyph_cache_bytes=1024*1024;control.disabled=!eligible;
    nx_interaction_state state;
    if(result==NX_OK)result=nx_checkbox(view->ctx,&control,&view->reviewed,&state);
    for(unsigned i=1;i<3 && result==NX_OK;++i){
        control.id=200+i;control.name=span(names[i]);control.label_layout=labels[i];
        control.bounds=stacked?(nx_rect){16,height-footer+24+button_height+(button_height+12)*(float)(i-1),width-32,button_height}:
            (nx_rect){16+(half+12)*(float)(i-1),height-footer+24+button_height,half,button_height};
        control.label_origin=(nx_point){control.bounds.x+8,control.bounds.y+10};control.disabled=i==2 && (!eligible || !view->reviewed);
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
    {nx_result end=nx_end_frame(view->ctx);if(result==NX_OK)result=end;}
    if(result==NX_OK && action->kind==REMOVAL_REQUEST)view->owner->issued=1;
cleanup:
    if(result!=NX_OK)*action=(removal_action){0};
    if(title)nx_text_layout_destroy(title);
    for(unsigned i=0;i<3;++i)if(labels[i])nx_text_layout_destroy(labels[i]);
    for(size_t i=0;i<count;++i)if(rows[i].layout)nx_text_layout_destroy(rows[i].layout);
    return result;
}
