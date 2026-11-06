#include "fixture.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"removal-refresh:%d: %s\n",__LINE__,#x);exit(1);}}while(0)
static nx_semantic_view node(removal_review *view,uint64_t id){
    uint32_t index;nx_semantic_view value;nx_context *ctx=removal_review_context(view);
    CHECK(nx_semantics_find(ctx,id,&index)==NX_OK && nx_semantics_get(ctx,index,&value)==NX_OK);return value;
}
static int equals(nx_utf8_span value,const char *text){return value.length==strlen(text) && !memcmp(value.data,text,value.length);}
static int contains(nx_utf8_span value,const char *text){
    size_t length=strlen(text);for(size_t i=0;i+length<=value.length;++i)if(!memcmp(value.data+i,text,length))return 1;return 0;
}
static removal_action frame(removal_review *view){
    removal_action action;nx_result result=removal_review_frame(view,640,480,&action);
    if(result!=NX_OK)fprintf(stderr,"frame returned %d\n",result);
    CHECK(result==NX_OK);return action;
}
static void press(removal_review *view,uint64_t id){CHECK(nx_semantics_request_action(removal_review_context(view),id,NX_ACTION_PRESS)==NX_OK);}
static void key(removal_review *view,nx_key code,uint32_t modifiers){
    nx_event event={0};event.type=NX_EVENT_KEY_DOWN;event.key=code;event.modifiers=modifiers;
    CHECK(removal_review_queue_event(view,&event)==NX_OK);event.type=NX_EVENT_KEY_UP;CHECK(removal_review_queue_event(view,&event)==NX_OK);
}
static void edit(removal_review *view,const char *text){
    CHECK(nx_semantics_request_text_value(removal_review_context(view),301,(nx_utf8_span){text,strlen(text)})==NX_OK);CHECK(frame(view).kind==REMOVAL_NONE);
}
static uint64_t first_object(removal_review *view,double *intra){
    nx_semantic_view area=node(view,20),best={0};size_t count=0;CHECK(nx_semantics_count(removal_review_context(view),&count)==NX_OK);
    for(size_t i=0;i<count;++i){nx_semantic_view value;CHECK(nx_semantics_get(removal_review_context(view),(uint32_t)i,&value)==NX_OK);
        if(value.parent_id==20 && value.id>=1000 && value.bounds.y+value.bounds.height>area.bounds.y && value.bounds.y<area.bounds.y+area.bounds.height &&
           (!best.id || value.bounds.y<best.bounds.y))best=value;
    }
    *intra=best.id?area.bounds.y-best.bounds.y:0;return best.id;
}
int main(int argc,char **argv){
    CHECK(argc==2);FILE *file=fopen(argv[1],"rb");CHECK(file && !fseek(file,0,SEEK_END));long length=ftell(file);
    CHECK(length>0 && length<64000000 && !fseek(file,0,SEEK_SET));unsigned char *bytes=malloc((size_t)length);CHECK(bytes);
    CHECK(fread(bytes,1,(size_t)length,file)==(size_t)length && !fclose(file));
    nx_font *font=NULL;nx_font_collection *fonts=NULL;CHECK(nx_font_open(bytes,(size_t)length,NULL,&font)==NX_OK &&
        nx_font_collection_create(NULL,&fonts)==NX_OK && nx_font_collection_add(fonts,font)==NX_OK);
    removal_entry entries[8],next_entries[8];removal_plan plan=removal_fixture(entries),next=plan;next.entries=next_entries;
    memcpy(next_entries,entries,sizeof(entries));removal_owner *owner=NULL;removal_review *a=NULL,*b=NULL,*frozen=NULL;
    CHECK(removal_owner_create(&plan,&owner)==NX_OK && removal_review_create(owner,fonts,&a)==NX_OK &&
        removal_review_create(owner,fonts,&b)==NX_OK && removal_review_create(owner,fonts,&frozen)==NX_OK);
    frame(a);frame(b);frame(frozen);press(a,200);frame(a);press(a,202); /* Admitted against revision 42, not executed. */
    press(b,200);frame(b);press(b,200);press(b,202); /* Old toggle must not confirm the new plan. */
    next.revision=43;next_entries[0].logical_bytes=9999;strcpy(next_entries[0].label,"Application code revised");
    nx_theme theme;CHECK(nx_theme_preset(NX_THEME_LIGHT,&theme)==NX_OK && nx_theme_set_metric(&theme,NX_METRIC_FONT_SIZE,28)==NX_OK &&
        nx_context_set_theme(removal_review_context(a),&theme)==NX_OK && nx_context_set_text_raster_scale(removal_review_context(a),1.5f)==NX_OK &&
        nx_context_set_reduced_motion(removal_review_context(a),1)==NX_OK);
    CHECK(removal_owner_refresh(owner,&next)==NX_OK && removal_owner_revision(owner)==43);
    CHECK(removal_review_refresh(b)==NX_OK && frame(b).kind==REMOVAL_NONE && !(node(b,200).state&NX_SEM_CHECKED));
    CHECK(frame(a).kind==REMOVAL_NONE && frame(frozen).kind==REMOVAL_NONE && !removal_owner_requests(owner));
    CHECK(removal_review_revision(a)==42 && removal_review_revision(frozen)==42 && equals(node(a,31).value,"00000000000000000000000000000001 / revision 42"));
    CHECK(equals(node(a,1000).name,"Application code") && node(a,200).state&NX_SEM_DISABLED);
    uint32_t missing;CHECK(nx_semantics_find(removal_review_context(a),202,&missing)!=NX_OK);
    nx_context *old=removal_review_context(a);press(a,203);CHECK(frame(a).kind==REMOVAL_REFRESH);
    CHECK(removal_review_refresh(a)==NX_OK && removal_review_context(a)!=old);frame(a);
    CHECK(removal_review_revision(a)==43 && removal_review_revision(frozen)==42 && equals(node(a,1000).name,"Application code revised") && contains(node(a,1000).value,"9999"));
    float density=0;int reduced=0;nx_color color;CHECK(nx_context_get_text_raster_scale(removal_review_context(a),&density)==NX_OK && density==1.5f &&
        nx_context_get_reduced_motion(removal_review_context(a),&reduced)==NX_OK && reduced==1 && nx_theme_color(removal_review_context(a),NX_COLOR_BACKGROUND,&color)==NX_OK &&
        !memcmp(&color,&theme.colors[NX_COLOR_BACKGROUND],sizeof(color)) && nx_focus_id(removal_review_context(a))==250 && node(a,202).state&NX_SEM_DISABLED);
    CHECK(removal_owner_refresh(owner,&next)==NX_ERROR_STATE);next.revision=41;CHECK(removal_owner_refresh(owner,&next)==NX_ERROR_STATE);next.revision=44;
    next.scope[15]=4;CHECK(removal_owner_refresh(owner,&next)==NX_ERROR_ARGUMENT);next.scope[15]=3;
    next_entries[0].object[15]=99;CHECK(removal_owner_refresh(owner,&next)==NX_ERROR_ARGUMENT);next_entries[0].object[15]=10;
    next_entries[0].semantic_id=5000;CHECK(removal_owner_refresh(owner,&next)==NX_ERROR_ARGUMENT);next_entries[0].semantic_id=1000;
    CHECK(removal_owner_revision(owner)==43);
    /* Preserve a live editor model, its undo history and selection; discard queued old-context writes. */
    press(a,250);frame(a);edit(a,"before");edit(a,"draft");
    key(a,NX_KEY_HOME,0);frame(a);key(a,NX_KEY_RIGHT,NX_MOD_SHIFT);frame(a);
    CHECK(nx_semantics_request_text_value(removal_review_context(a),301,(nx_utf8_span){"stale queued edit",17})==NX_OK);
    next.revision=44;CHECK(removal_owner_refresh(owner,&next)==NX_OK && removal_review_refresh(a)==NX_OK);frame(a);
    CHECK(removal_review_filter_open(a) && equals(node(a,301).value,"draft") && nx_focus_id(removal_review_context(a))==301);
    key(a,NX_KEY_DELETE,0);frame(a);CHECK(equals(node(a,301).value,"raft"));
    key(a,NX_KEY_Z,NX_MOD_CONTROL);frame(a);CHECK(equals(node(a,301).value,"draft"));
    key(a,NX_KEY_Z,NX_MOD_CONTROL);frame(a);CHECK(equals(node(a,301).value,"before"));
    key(a,NX_KEY_ESCAPE,0);frame(a);CHECK(!removal_review_filter_open(a));
    next.revision=45;next.count=0;next.entries=NULL;CHECK(removal_owner_refresh(owner,&next)==NX_OK && removal_review_refresh(a)==NX_OK);frame(a);
    CHECK(node(a,200).state&NX_SEM_DISABLED && equals(node(a,100).value,"0 objects"));
    next.revision=46;next.count=8;next.entries=next_entries;CHECK(removal_owner_refresh(owner,&next)==NX_OK && removal_review_refresh(a)==NX_OK);frame(a);
    press(a,200);frame(a);press(a,202);removal_action request=frame(a);CHECK(request.kind==REMOVAL_REQUEST && request.request.revision==46);
    next.revision=47;CHECK(removal_owner_refresh(owner,&next)==NX_OK && removal_review_refresh(a)==NX_OK);frame(a);
    CHECK(removal_owner_requests(owner)==1 && node(a,202).state&NX_SEM_DISABLED && contains(node(a,10).name,"revision 46") && contains(node(a,10).description,"revision 46"));
    CHECK(removal_review_revision(frozen)==42 && frame(frozen).kind==REMOVAL_NONE && equals(node(frozen,1000).name,"Application code"));
    CHECK(removal_review_refresh(frozen)==NX_OK);frame(frozen);CHECK(removal_review_revision(frozen)==47 && node(frozen,202).state&NX_SEM_DISABLED);
    next.revision=UINT64_MAX;CHECK(removal_owner_refresh(owner,&next)==NX_OK && removal_review_refresh(a)==NX_OK);frame(a);
    next.revision=1;CHECK(removal_owner_refresh(owner,&next)==NX_ERROR_STATE && removal_owner_revision(owner)==UINT64_MAX && removal_owner_requests(owner)==1);
    removal_review_destroy(a);removal_review_destroy(b);removal_review_destroy(frozen);removal_owner_destroy(owner);
    removal_entry seed[8];plan=removal_fixture(seed);removal_entry *large=removal_large_fixture(REMOVAL_MAX_ENTRIES);CHECK(large);
    plan.entries=large;plan.count=REMOVAL_MAX_ENTRIES;CHECK(removal_owner_create(&plan,&owner)==NX_OK && removal_review_create(owner,fonts,&a)==NX_OK);frame(a);
    CHECK(nx_focus_set(removal_review_context(a),21)==NX_OK);key(a,NX_KEY_END,0);frame(a);double before=0,after=0;uint64_t anchor=first_object(a,&before);CHECK(anchor);
    for(size_t i=0;i<plan.count/2;++i){removal_entry swap=large[i];large[i]=large[plan.count-i-1];large[plan.count-i-1]=swap;}
    plan.revision=43;CHECK(removal_owner_refresh(owner,&plan)==NX_OK && removal_review_refresh(a)==NX_OK);frame(a);
    CHECK(first_object(a,&after)==anchor && fabs(after-before)<=1);
    size_t removed=0;while(removed<plan.count && large[removed].semantic_id!=anchor)++removed;CHECK(removed<plan.count);
    large[removed]=large[--plan.count];plan.revision=44;CHECK(removal_owner_refresh(owner,&plan)==NX_OK && removal_review_refresh(a)==NX_OK);frame(a);
    CHECK(fabs(node(a,10).bounds.y-node(a,20).bounds.y)<=1); /* Removed anchor goes Home. */
    size_t full_count=plan.count;plan.count=8;plan.revision=45;CHECK(removal_owner_refresh(owner,&plan)==NX_OK && removal_review_refresh(a)==NX_OK);frame(a);
    CHECK(removal_review_filter(a,(nx_utf8_span){"managed-object-099999",21})==NX_OK);frame(a);
    plan.count=full_count;plan.revision=46;CHECK(removal_owner_refresh(owner,&plan)==NX_OK && removal_review_refresh(a)==NX_OK);frame(a);
    CHECK(equals(node(a,104).value,"1 of 19999 objects shown"));
    large[plan.count-1].disposition=REMOVAL_UNKNOWN;strcpy(large[plan.count-1].reason,"Owner cannot establish this object's managed state");plan.revision=47;
    CHECK(removal_owner_refresh(owner,&plan)==NX_OK && removal_review_refresh(a)==NX_OK);frame(a);
    CHECK(node(a,200).state&NX_SEM_DISABLED && equals(node(a,105).value,"0 of 1 objects shown") && !removal_owner_requests(owner));
    removal_review_destroy(a);removal_owner_destroy(owner);free(large);nx_font_collection_destroy(fonts);nx_font_close(font);free(bytes);
    puts("Monotonic owned snapshots, stale actions, retained editor/history, exact issued revision, 100k reorder/anchor and eager/virtual replacement passed");return 0;
}
