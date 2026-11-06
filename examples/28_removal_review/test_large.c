#define _POSIX_C_SOURCE 200809L
#include "fixture.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"large-removal:%d: %s\n",__LINE__,#x);exit(1);}}while(0)
static double now_ms(void){
    struct timespec t;
#if defined(CLOCK_MONOTONIC)
    CHECK(!clock_gettime(CLOCK_MONOTONIC,&t));
#else
    CHECK(timespec_get(&t,TIME_UTC)==TIME_UTC);
#endif
    return t.tv_sec*1000.+t.tv_nsec/1e6;
}
static size_t peak_layouts,peak_passes,peak_nodes;
static int compare_time(const void *a,const void *b){double x=*(const double *)a,y=*(const double *)b;return (x>y)-(x<y);}
static nx_semantic_view node(removal_review *view,uint64_t id){
    uint32_t index;nx_semantic_view result;nx_context *ctx=removal_review_context(view);
    CHECK(nx_semantics_find(ctx,id,&index)==NX_OK && nx_semantics_get(ctx,index,&result)==NX_OK);return result;
}
static int equals(nx_utf8_span value,const char *text){return value.length==strlen(text) && !memcmp(value.data,text,value.length);}
static void key(removal_review *view,nx_key code){
    nx_event event={0};event.type=NX_EVENT_KEY_DOWN;event.key=code;
    CHECK(nx_context_queue_event(removal_review_context(view),&event)==NX_OK);
    event.type=NX_EVENT_KEY_UP;CHECK(nx_context_queue_event(removal_review_context(view),&event)==NX_OK);
}
static void frame(removal_review *view,float width,float height){
    removal_action action;nx_result result=removal_review_frame(view,width,height,&action);
    if(result!=NX_OK)fprintf(stderr,"frame result=%d at %.0fx%.0f\n",result,width,height);
    CHECK(result==NX_OK && action.kind==REMOVAL_NONE);
    removal_work work;removal_review_last_work(view,&work);
    CHECK(work.row_layouts<=4096 && work.settling_frames<=16 && work.indexed_extent>0);
    size_t count=0;CHECK(nx_semantics_count(removal_review_context(view),&count)==NX_OK && count<=275);
    if(work.row_layouts>peak_layouts)peak_layouts=work.row_layouts;
    if(work.settling_frames>peak_passes)peak_passes=work.settling_frames;
    if(count>peak_nodes)peak_nodes=count;
}
int main(int argc,char **argv){
    CHECK(argc==2);FILE *file=fopen(argv[1],"rb");CHECK(file && !fseek(file,0,SEEK_END));long length=ftell(file);
    CHECK(length>0 && length<64000000 && !fseek(file,0,SEEK_SET));unsigned char *font_bytes=malloc((size_t)length);CHECK(font_bytes);
    CHECK(fread(font_bytes,1,(size_t)length,file)==(size_t)length && !fclose(file));
    nx_font *font=NULL;nx_font_collection *fonts=NULL;
    CHECK(nx_font_open(font_bytes,(size_t)length,NULL,&font)==NX_OK && nx_font_collection_create(NULL,&fonts)==NX_OK && nx_font_collection_add(fonts,font)==NX_OK);
    removal_entry seed[8];removal_plan plan=removal_fixture(seed);
    removal_entry *entries=removal_large_fixture(REMOVAL_MAX_ENTRIES);CHECK(entries);
    plan.entries=entries;plan.count=REMOVAL_MAX_ENTRIES;removal_owner *owner=NULL;removal_review *view=NULL;
    double start=now_ms();CHECK(removal_owner_create(&plan,&owner)==NX_OK);double copied=now_ms()-start;
    CHECK(removal_review_create(owner,fonts,&view)==NX_OK);start=now_ms();frame(view,640,480);double first=now_ms()-start;
    CHECK(equals(node(view,100).value,"20000 objects") && equals(node(view,104).value,"20000 objects"));
    CHECK(nx_focus_set(removal_review_context(view),21)==NX_OK);key(view,NX_KEY_END);frame(view,640,480);
    CHECK(equals(node(view,100999).name,"managed-object-099999"));
    for(unsigned i=0;i<2;++i){
        nx_theme theme;CHECK(nx_theme_preset(i?NX_THEME_LIGHT:NX_THEME_DARK,&theme)==NX_OK);
        if(i)CHECK(nx_theme_set_metric(&theme,NX_METRIC_FONT_SIZE,2*theme.metrics[NX_METRIC_FONT_SIZE])==NX_OK);
        CHECK(nx_context_set_theme(removal_review_context(view),&theme)==NX_OK);frame(view,420,320);
        key(view,NX_KEY_END);frame(view,420,320);CHECK(equals(node(view,100999).name,"managed-object-099999"));
        key(view,NX_KEY_HOME);frame(view,420,320);CHECK(equals(node(view,10).name,"Preview only; no removal has started"));
    }
    for(unsigned density=0;density<5;++density)for(unsigned size=0;size<4;++size){
        nx_theme theme;CHECK(nx_theme_preset(NX_THEME_DARK,&theme)==NX_OK &&
            nx_theme_set_metric(&theme,NX_METRIC_FONT_SIZE,(1.+.25*density)*theme.metrics[NX_METRIC_FONT_SIZE])==NX_OK &&
            nx_context_set_theme(removal_review_context(view),&theme)==NX_OK);
        float width=420+37*(float)size,height=size&1?480:320;
        frame(view,width,height);key(view,NX_KEY_END);frame(view,width,height);
        CHECK(equals(node(view,100999).name,"managed-object-099999"));
    }
    nx_theme ordinary;CHECK(nx_theme_preset(NX_THEME_DARK,&ordinary)==NX_OK && nx_context_set_theme(removal_review_context(view),&ordinary)==NX_OK);
    frame(view,640,480);key(view,NX_KEY_HOME);frame(view,640,480);
    double timings[64];
    for(unsigned i=0;i<64;++i){
        nx_rect area=node(view,20).bounds;nx_event wheel={0};wheel.type=NX_EVENT_WHEEL;
        wheel.x=area.x+5;wheel.y=area.y+5;wheel.delta_y=(i&1?1.f:-1.f)*(float)((i*7919u)%100000u)/4;
        CHECK(nx_context_queue_event(removal_review_context(view),&wheel)==NX_OK);
        start=now_ms();frame(view,640,480);timings[i]=now_ms()-start;
    }
    qsort(timings,64,sizeof(*timings),compare_time);
    printf("rows=%u copied_entry_bytes=%zu copy_ms=%.3f first_frame_ms=%.3f scroll64_median_ms=%.3f scroll64_p95_ms=%.3f scroll64_max_ms=%.3f peak_row_layouts=%zu peak_settling=%zu peak_semantics=%zu\n",
        (unsigned)REMOVAL_MAX_ENTRIES,(size_t)REMOVAL_MAX_ENTRIES*sizeof(*entries),copied,first,timings[32],timings[60],timings[63],peak_layouts,peak_passes,peak_nodes);
    CHECK(nx_semantics_request_action(removal_review_context(view),200,NX_ACTION_PRESS)==NX_OK);frame(view,640,480);
    CHECK(nx_semantics_request_action(removal_review_context(view),202,NX_ACTION_PRESS)==NX_OK);
    CHECK(nx_theme_set_metric(&ordinary,NX_METRIC_FONT_SIZE,2*ordinary.metrics[NX_METRIC_FONT_SIZE])==NX_OK &&
        nx_context_set_theme(removal_review_context(view),&ordinary)==NX_OK);
    removal_action action;CHECK(removal_review_frame(view,420,320,&action)==NX_OK && action.kind==REMOVAL_REQUEST && action.request.revision==42 && removal_owner_requests(owner)==1);
    frame(view,420,320);CHECK(equals(node(view,10).name,"Request handed to owner; outcome unknown"));
    removal_review_destroy(view);CHECK(removal_review_create(owner,fonts,&view)==NX_OK);frame(view,640,480);
    CHECK(removal_owner_requests(owner)==1 && node(view,202).state&NX_SEM_DISABLED);
    removal_review_destroy(view);removal_owner_destroy(owner);
    entries[REMOVAL_MAX_ENTRIES-1].disposition=REMOVAL_UNKNOWN;
    strcpy(entries[REMOVAL_MAX_ENTRIES-1].reason,"Owner cannot establish this object's managed state");
    CHECK(removal_owner_create(&plan,&owner)==NX_OK && removal_review_create(owner,fonts,&view)==NX_OK);frame(view,640,480);
    CHECK(equals(node(view,10).name,"Inventory incomplete; request unavailable") && node(view,200).state&NX_SEM_DISABLED);
    CHECK(nx_semantics_request_action(removal_review_context(view),200,NX_ACTION_PRESS)!=NX_OK);
    CHECK(nx_focus_set(removal_review_context(view),21)==NX_OK);key(view,NX_KEY_END);frame(view,640,480);
    CHECK(equals(node(view,100999).name,"managed-object-099999") && node(view,202).state&NX_SEM_DISABLED);
    removal_review_destroy(view);removal_owner_destroy(owner);
    entries[REMOVAL_MAX_ENTRIES-1].semantic_id=entries[0].semantic_id;
    CHECK(removal_owner_create(&plan,&owner)==NX_ERROR_ARGUMENT && !owner);
    entries[REMOVAL_MAX_ENTRIES-1].semantic_id=100999;memcpy(entries[REMOVAL_MAX_ENTRIES-1].object,entries[0].object,16);
    CHECK(removal_owner_create(&plan,&owner)==NX_ERROR_ARGUMENT && !owner);
    free(entries);nx_font_collection_destroy(fonts);nx_font_close(font);free(font_bytes);
    puts("Full 100000-object copied plan, bounded visible work, both ends, resize/text and offscreen unknown checks passed");return 0;
}
