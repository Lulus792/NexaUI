#include "scene.h"
#include <fcntl.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

enum { KEY_LIMIT=8192, HASH_CAPACITY=16384, ROOT_ID=1, FILTER_ID=2, FILTER_HEIGHT=36, ROW_HEIGHT=28, MAX_ROWS=64 };
typedef struct row_key {dev_t dev;ino_t ino;enum ae_files_kind kind;char name[AE_FILES_NAME+1];uint64_t id;} row_key;
struct files_scene {
    struct ae_files *model;nx_font_collection *fonts;nx_context *ctx;nx_virtual_list *list;
    nx_text_field *query;nx_window *window;
    row_key keys[KEY_LIMIT];uint32_t slots[HASH_CAPACITY];size_t key_count,count;
    uint64_t ids[AE_FILES_ENTRIES],next_id,selected;int directory,query_valid,query_rejected,need_focus,ready;
    double offset;float viewport_height;
};
static uint64_t hash_bytes(uint64_t h,const void *p,size_t n) {
    const unsigned char *bytes=p;while(n--)h=(h^*bytes++)*UINT64_C(1099511628211);return h;
}
static nx_result row_id(files_scene *s,const struct ae_files_entry *entry,uint64_t *out) {
    uint64_t hash=UINT64_C(14695981039346656037);
    hash=hash_bytes(hash,&entry->device,sizeof(entry->device));
    hash=hash_bytes(hash,&entry->inode,sizeof(entry->inode));
    hash=hash_bytes(hash,&entry->kind,sizeof(entry->kind));
    hash=hash_bytes(hash,entry->name,strlen(entry->name));
    size_t slot=(size_t)hash&(HASH_CAPACITY-1u);
    while(s->slots[slot]) {
        row_key *key=&s->keys[s->slots[slot]-1u];
        if(key->dev==entry->device && key->ino==entry->inode && key->kind==entry->kind && !strcmp(key->name,entry->name)) {
            *out=key->id;return NX_OK;
        }
        slot=(slot+1u)&(HASH_CAPACITY-1u);
    }
    if(s->key_count==KEY_LIMIT || s->next_id==UINT64_MAX)return NX_ERROR_LIMIT;
    row_key *key=&s->keys[s->key_count];key->dev=entry->device;key->ino=entry->inode;key->kind=entry->kind;
    memcpy(key->name,entry->name,strlen(entry->name)+1);key->id=s->next_id++;
    s->slots[slot]=(uint32_t)++s->key_count;*out=key->id;return NX_OK;
}
static int same_directory(int a,int b) {
    struct stat x,y;return a>=0 && b>=0 && !fstat(a,&x) && !fstat(b,&y) &&
        S_ISDIR(x.st_mode) && S_ISDIR(y.st_mode) && x.st_dev==y.st_dev && x.st_ino==y.st_ino;
}
nx_result files_scene_sync(files_scene *s) {
    if(!s)return NX_ERROR_ARGUMENT;
    s->ready=0;
    int dir=ae_files_directory(s->model);if(dir<0)return NX_ERROR_PLATFORM;
    if(!same_directory(dir,s->directory)) {
        if(s->directory>=0)close(s->directory);
        s->directory=dir;dir=-1;memset(s->slots,0,sizeof(s->slots));s->key_count=0;s->selected=0;s->offset=0;
    }
    if(dir>=0)close(dir);
    size_t count=ae_files_count(s->model);if(count>AE_FILES_ENTRIES)return NX_ERROR_LIMIT;
    uint64_t ids[AE_FILES_ENTRIES];
    for(size_t i=0;i<count;++i){const struct ae_files_entry *entry=ae_files_entry(s->model,i);
        if(!entry)return NX_ERROR_STATE;
        nx_result r=row_id(s,entry,&ids[i]);if(r!=NX_OK)return r;}
    nx_virtual_list *next=NULL;nx_result r=nx_virtual_list_create(count,ROW_HEIGHT,NULL,&next);if(r!=NX_OK)return r;
    nx_virtual_list_destroy(s->list);s->list=next;s->count=count;memcpy(s->ids,ids,count*sizeof(*ids));
    if(!s->selected && count) {
        size_t selected_index=ae_files_selected(s->model);
        if(selected_index>=count)return NX_ERROR_STATE;
        s->selected=s->ids[selected_index];
    }
    for(size_t i=0;i<count;++i)if(s->ids[i]==s->selected){
        (void)ae_files_select(s->model,i);double y=(double)i*ROW_HEIGHT;
        if(y<s->offset)s->offset=y;
        if(y+ROW_HEIGHT>s->offset+s->viewport_height)s->offset=y+ROW_HEIGHT-s->viewport_height;
        if(s->offset<0)s->offset=0;
        break;
    }
    s->ready=1;return NX_OK;
}
nx_result files_scene_create(struct ae_files *model,nx_font_collection *fonts,files_scene **out) {
    if(!out)return NX_ERROR_ARGUMENT;
    *out=NULL;if(!model || !fonts)return NX_ERROR_ARGUMENT;
    files_scene *s=calloc(1,sizeof(*s));if(!s)return NX_ERROR_MEMORY;
    s->model=model;s->fonts=fonts;s->directory=-1;s->query_valid=1;s->next_id=100;s->viewport_height=ROW_HEIGHT;
    nx_result r=nx_context_create(NULL,&s->ctx);if(r==NX_OK)r=files_scene_sync(s);
    if(r!=NX_OK){files_scene_destroy(s);return r;}*out=s;return NX_OK;
}
void files_scene_destroy(files_scene *s){if(!s)return;if(s->directory>=0)close(s->directory);
    nx_text_field_destroy(s->query);nx_virtual_list_destroy(s->list);if(s->ctx)nx_context_destroy(s->ctx);free(s);}
nx_result files_scene_enable_filter(files_scene *s,nx_window *window) {
    if(!s || s->query)return NX_ERROR_ARGUMENT;
    const char *text=ae_files_query(s->model);nx_result r=nx_text_field_create(text,strlen(text),32768,&s->query);
    if(r==NX_OK)s->window=window;
    return r;
}
nx_result files_scene_focus_filter(files_scene *s,int clear) {
    if(!s || !s->query || (clear!=0 && clear!=1))return NX_ERROR_ARGUMENT;
    nx_result r=nx_focus_set(s->ctx,FILTER_ID);if(r!=NX_OK)return r;
    if(!clear)return NX_OK;
    r=nx_text_field_set_text(s->query,"",0);if(r==NX_OK)s->query_rejected=0;return r;
}
int files_scene_filter_has_focus(const files_scene *s){return s && s->query && nx_focus_id(s->ctx)==FILTER_ID;}
nx_result files_scene_filter(files_scene *s,const char *query,size_t length) {
    if(!s)return NX_ERROR_ARGUMENT;
    if(!ae_files_filter(s->model,query,length)){s->query_valid=0;s->query_rejected=1;return NX_ERROR_ARGUMENT;}
    if(s->query){nx_result r=nx_text_field_set_text(s->query,query,length);if(r!=NX_OK){s->ready=0;return r;}}
    s->query_valid=1;s->query_rejected=0;s->offset=0;return files_scene_sync(s);
}
nx_result files_scene_key(files_scene *s,nx_key key,int owns_focus) {
    if(!s || (owns_focus!=0 && owns_focus!=1))return NX_ERROR_ARGUMENT;
    if(!s->ready)return NX_ERROR_STATE;
    if(!owns_focus || !s->count)return NX_OK;
    size_t index=0;int found=0;
    for(size_t i=0;i<s->count;++i)if(s->ids[i]==s->selected){index=i;found=1;break;}
    size_t page=s->viewport_height>=ROW_HEIGHT?(size_t)(s->viewport_height/ROW_HEIGHT):1;
    int direction=1;
    switch(key){case NX_KEY_UP:direction=-1;if(found && index)--index;break;case NX_KEY_DOWN:if(found && index+1<s->count)++index;break;
        case NX_KEY_HOME:index=0;break;case NX_KEY_END:direction=-1;index=s->count-1;break;
        case NX_KEY_PAGE_UP:direction=-1;index=index>page?index-page:0;break;
        case NX_KEY_PAGE_DOWN:index=page<s->count-1-index?index+page:s->count-1;break;
        default:return NX_OK;}
    const struct ae_files_entry *entry=ae_files_entry(s->model,index);
    if(!entry)return NX_ERROR_STATE;
    while(entry->kind==AE_FILES_UNAVAILABLE) {
        if((direction<0 && !index) || (direction>0 && index+1==s->count))return NX_OK;
        index=direction<0?index-1:index+1;
        entry=ae_files_entry(s->model,index);if(!entry)return NX_ERROR_STATE;
    }
    if(!ae_files_select(s->model,index))return NX_ERROR_STATE;
    s->selected=s->ids[index];
    double y=(double)index*ROW_HEIGHT;
    if(y<s->offset)s->offset=y;
    if(y+ROW_HEIGHT>s->offset+s->viewport_height)s->offset=y+ROW_HEIGHT-s->viewport_height;
    if(s->offset<0)s->offset=0;
    s->need_focus=1;return NX_OK;
}
nx_result files_scene_scroll(files_scene *s,double pixels) {
    if(!s || !isfinite(pixels))return NX_ERROR_ARGUMENT;
    if(!s->ready)return NX_ERROR_STATE;
    double desired=s->offset+pixels;
    if(!isfinite(desired))return NX_ERROR_ARGUMENT;
    if(desired<0)desired=0;
    nx_virtual_range range;
    nx_result r=nx_virtual_list_range(s->list,desired,s->viewport_height,0,&range);
    if(r==NX_OK)s->offset=range.effective_offset;
    return r;
}
static int query_bytes_valid(nx_utf8_span text) {
    if(text.length>AE_FILES_QUERY)return 0;
    size_t at=0;while(at<text.length){uint32_t scalar;if(nx_utf8_decode(text.data,text.length,&at,&scalar)!=NX_OK ||
        scalar<32 || (scalar>=0x7f && scalar<=0x9f))return 0;}
    return 1;
}
static int query_valid(nx_utf8_span text,void *user) {
    files_scene *s=user;return !(s->query_rejected && text.length) && query_bytes_valid(text);
}
/* Replay only an unambiguous subset on the same public text buffer. Complex
   event routing, IME and clipboard operations retain the conservative bound. */
static nx_result guard_text_burst(files_scene *s,uint64_t previous_focus,
    nx_text_buffer **simulation,int *fallback_high) {
    *simulation=NULL;*fallback_high=0;
    if(nx_focus_id(s->ctx)!=FILTER_ID)return NX_OK;
    nx_utf8_span before={0};nx_result r=nx_text_field_text(s->query,&before);if(r!=NX_OK)return r;
    const nx_event *events=NULL;size_t count=0;r=nx_frame_events(s->ctx,&events,&count);if(r!=NX_OK)return r;
    size_t upper=before.length;int simple=previous_focus==FILTER_ID,has_text=0;
    for(size_t i=0;i<count;++i) {
        const nx_event *e=&events[i];
        if(e->type==NX_EVENT_TEXT && (!e->ime_target || e->ime_target==FILTER_ID)) {
            uint32_t scalar=e->scalar;size_t bytes=scalar<=0x7f?1:scalar<=0x7ff?2:scalar<=0xffff?3:4;
            has_text=1;if(e->ime_target)simple=0;
            if(upper>AE_FILES_QUERY || bytes>AE_FILES_QUERY-upper)*fallback_high=1;
            else upper+=bytes;
        } else if(e->type==NX_EVENT_KEY_DOWN) {
            if((e->key==NX_KEY_BACKSPACE || e->key==NX_KEY_DELETE) && !e->modifiers)continue;
            if(e->key>=NX_KEY_A && e->key<=NX_KEY_Z && !e->modifiers)continue;
            if(e->key>=NX_KEY_0 && e->key<=NX_KEY_9 && !e->modifiers)continue;
            simple=0;
        } else if(e->type!=NX_EVENT_KEY_UP && e->type!=NX_EVENT_POINTER_MOVE) simple=0;
    }
    if(!has_text)return NX_OK;
    if(!simple){if(*fallback_high){s->query_rejected=1;s->query_valid=0;}return NX_OK;}
    size_t anchor=0,cursor=0;r=nx_text_field_selection(s->query,&anchor,&cursor);if(r!=NX_OK)return r;
    r=nx_text_buffer_create(before.data,before.length,0,simulation);if(r!=NX_OK)return r;
    r=nx_text_buffer_set_selection(*simulation,anchor,cursor);if(r!=NX_OK)return r;
    for(size_t i=0;i<count;++i) {
        const nx_event *e=&events[i];int handled=0;
        if(e->type==NX_EVENT_TEXT || (e->type==NX_EVENT_KEY_DOWN &&
           (e->key==NX_KEY_BACKSPACE || e->key==NX_KEY_DELETE))) {
            r=nx_text_buffer_apply_event(*simulation,e,&handled);if(r!=NX_OK)return r;
            const char *bytes=NULL;size_t length=0;r=nx_text_buffer_text(*simulation,&bytes,&length);if(r!=NX_OK)return r;
            if(handled && !query_bytes_valid((nx_utf8_span){bytes,length})){s->query_rejected=1;s->query_valid=0;}
        }
    }
    return NX_OK;
}
nx_result files_scene_frame(files_scene *s,float width,float height) {
    float header=s && s->query?FILTER_HEIGHT:0;
    if(!s || !isfinite(width) || !isfinite(height) || width<40 || height<header+ROW_HEIGHT || height>ROW_HEIGHT*MAX_ROWS+header)
        return NX_ERROR_ARGUMENT;
    if(!s->ready)return NX_ERROR_STATE;
    s->viewport_height=height-header;nx_virtual_range range;
    nx_text_layout *layouts[MAX_ROWS]={0},*hint=NULL;nx_text_buffer *simulation=NULL;size_t made=0;int fallback_high=0;
    uint64_t previous_focus=nx_focus_id(s->ctx);
    nx_result r=nx_begin_frame(s->ctx,width,height,0);if(r!=NX_OK)return r;
    if(s->query) {
        r=guard_text_burst(s,previous_focus,&simulation,&fallback_high);
        nx_text_field_desc field={0};field.id=FILTER_ID;field.bounds=(nx_rect){0,0,width,FILTER_HEIGHT};
        field.name=(nx_utf8_span){"Filter files",12};field.fonts=s->fonts;field.pixels_per_em=15;
        field.glyph_cache_bytes=1024*1024;field.validate=query_valid;field.validate_user=s;field.ime_window=s->window;
        if(r==NX_OK && s->window)r=nx_text_clipboard_for_window(s->window,&field.clipboard);
        nx_interaction_state state;int valid=0,submitted=0;
        if(r==NX_OK)r=nx_text_field_widget(s->ctx,&field,s->query,&state,&valid,&submitted);
        nx_utf8_span draft={0};if(r==NX_OK)r=nx_text_field_text(s->query,&draft);
        if(r==NX_OK && simulation) {
            const char *bytes=NULL;size_t length=0;r=nx_text_buffer_text(simulation,&bytes,&length);
            if(r==NX_OK && (length!=draft.length || (length && memcmp(bytes,draft.data,length))) && fallback_high) {
                s->query_rejected=1;s->query_valid=0;valid=0;
            }
        }
        if(r==NX_OK && !draft.length && !state.focused) {
            nx_color muted;
            r=nx_theme_color(s->ctx,NX_COLOR_TEXT_MUTED,&muted);
            if(r==NX_OK)r=nx_text_layout_create(s->fonts,"Filter files",12,NX_BIDI_AUTO,15,width-16,&hint);
            if(r==NX_OK)r=nx_push_clip(s->ctx,field.bounds);
            if(r==NX_OK){r=nx_draw_text_layout(s->ctx,hint,(nx_point){8,9},muted,1024*1024);
                nx_result pop=nx_pop_clip(s->ctx);if(r==NX_OK)r=pop;}
        }
        if(r==NX_OK && !valid){s->query_valid=0;s->query_rejected=1;}
        if(r==NX_OK && valid && s->query_rejected && !draft.length)s->query_rejected=0;
        if(r==NX_OK && valid && !s->query_rejected) {
            const char *current=ae_files_query(s->model);size_t size=strlen(current);
            if(size!=draft.length || (size && memcmp(current,draft.data,size))) {
                if(!ae_files_filter(s->model,draft.data,draft.length))r=NX_ERROR_STATE;
                else {s->offset=0;r=files_scene_sync(s);}
            }
            if(r==NX_OK)s->query_valid=1;
        }
    }
    if(r==NX_OK)r=nx_virtual_list_range(s->list,s->offset,s->viewport_height,0,&range);
    if(r==NX_OK && range.end-range.first>MAX_ROWS)r=NX_ERROR_LIMIT;
    if(r==NX_OK)s->offset=range.effective_offset;
    nx_container_desc tree={0};tree.id=ROOT_ID;tree.bounds=(nx_rect){0,header,width,s->viewport_height};tree.name=(nx_utf8_span){"Files",5};
    if(r==NX_OK)r=nx_tree(s->ctx,&tree);
    for(size_t i=range.first;r==NX_OK && i<range.end;++i) {
        const struct ae_files_entry *entry=ae_files_entry(s->model,i);if(!entry){r=NX_ERROR_STATE;break;}
        float y=header+(float)((double)i*ROW_HEIGHT-s->offset),top=fmaxf(header,y),bottom=fminf(height,y+ROW_HEIGHT);
        if(bottom<=top)continue;
        size_t len=strlen(entry->label);
        r=nx_text_layout_create(s->fonts,entry->label,len,NX_BIDI_AUTO,15,width-16,&layouts[made]);
        if(r!=NX_OK)break;
        nx_tree_item_desc item={0};item.control.id=s->ids[i];item.control.parent_id=ROOT_ID;
        item.control.bounds=(nx_rect){0,top,width,bottom-top};item.control.name=(nx_utf8_span){entry->label,len};
        item.control.label_layout=layouts[made++];item.control.label_origin=(nx_point){8,y+5};
        item.control.glyph_cache_bytes=1024*1024;item.control.disabled=entry->kind==AE_FILES_UNAVAILABLE;
        nx_interaction_state state;r=nx_push_clip(s->ctx,item.control.bounds);
        if(r==NX_OK){r=nx_tree_item(s->ctx,&item,NULL,&s->selected,&state);nx_result pop=nx_pop_clip(s->ctx);if(r==NX_OK)r=pop;}
        if(r==NX_OK && state.clicked){if(!ae_files_select(s->model,i))r=NX_ERROR_STATE;}
        if(r==NX_OK && s->need_focus && s->selected==s->ids[i] && !item.control.disabled) {
            r=nx_focus_set(s->ctx,s->selected);if(r==NX_OK)s->need_focus=0;
        }
    }
    {nx_result end=nx_end_frame(s->ctx);if(r==NX_OK)r=end;}
    nx_text_buffer_destroy(simulation);
    if(hint)nx_text_layout_destroy(hint);
    for(size_t i=0;i<made;++i)nx_text_layout_destroy(layouts[i]);
    return r;
}
nx_context *files_scene_context(files_scene *s){return s?s->ctx:NULL;}
uint64_t files_scene_selected(const files_scene *s){return s?s->selected:0;}
int files_scene_query_accepted(const files_scene *s){return s?s->query_valid:0;}
enum ae_files_result files_scene_stage(files_scene *s,files_intent *intent) {
    if(!s || !intent || !s->query_valid)return AE_FILES_INVALID;
    if(!s->ready)return AE_FILES_CHANGED;
    size_t index=0;while(index<s->count && s->ids[index]!=s->selected)++index;
    if(index==s->count)return AE_FILES_CHANGED;
    int dir=ae_files_directory(s->model);if(dir<0)return AE_FILES_IO;
    int same=same_directory(dir,s->directory);close(dir);if(!same)return AE_FILES_CHANGED;
    const struct ae_files_entry *entry=ae_files_entry(s->model,index);uint64_t id=0;
    if(!entry || row_id(s,entry,&id)!=NX_OK || id!=s->selected || !ae_files_select(s->model,index))return AE_FILES_CHANGED;
    return files_intent_stage(intent,s->model,1);
}
