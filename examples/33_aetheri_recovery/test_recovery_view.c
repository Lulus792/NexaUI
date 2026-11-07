#include "recovery_view.h"
#include <errno.h>
#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"recovery UI %d: %s\n",__LINE__,#x);exit(1);}}while(0)
static int fail_malloc;
void *__real_malloc(size_t);
void *__wrap_malloc(size_t n){if(fail_malloc){fail_malloc=0;return NULL;}return __real_malloc(n);}
static void parse_id(const char *s,uint8_t out[16]){CHECK(strlen(s)==32);for(size_t i=0;i<16;++i){unsigned n;CHECK(sscanf(s+2*i,"%2x",&n)==1);out[i]=(uint8_t)n;}}
static int semantic(rvg_view *v,uint64_t id,nx_semantic_view *out){uint32_t index;return nx_semantics_find(rvg_context(v),id,&index)==NX_OK&&nx_semantics_get(rvg_context(v),index,out)==NX_OK;}
static int contains(nx_utf8_span s,const char *text){size_t n=strlen(text);for(size_t i=0;i+n<=s.length;++i)if(!memcmp(s.data+i,text,n))return 1;return 0;}
static void frame(rvg_view *v,float w,float h){rvg_intent action;CHECK(rvg_frame(v,w,h,&action)==NX_OK&&!action.close&&!action.refresh);}
static void press(rvg_view *v,uint64_t id,uint32_t action,float w,float h){CHECK(nx_semantics_request_action(rvg_context(v),id,action)==NX_OK);frame(v,w,h);}
static void key(rvg_view *v,nx_key k,float w,float h){nx_event e={0};e.type=NX_EVENT_KEY_DOWN;e.key=k;CHECK(rvg_queue_event(v,&e)==NX_OK);frame(v,w,h);}
static void reverse_tab(rvg_view *v,float w,float h){nx_event e={0};e.type=NX_EVENT_KEY_DOWN;e.key=NX_KEY_TAB;e.modifiers=NX_MOD_SHIFT;CHECK(rvg_queue_event(v,&e)==NX_OK);frame(v,w,h);}
static void text_input(rvg_view *v,const char *text,float w,float h){for(size_t i=0;text[i];++i){nx_event e={0};e.type=NX_EVENT_TEXT;e.scalar=(unsigned char)text[i];CHECK(rvg_queue_event(v,&e)==NX_OK);}frame(v,w,h);}
static void capture(rvg_view *v,float w,float h,rvg_style style,const char *name){
    const char *directory=getenv("RVG_CAPTURE_DIR");if(!directory)return;
    uint32_t width=(uint32_t)ceilf(w*style.raster_scale),height=(uint32_t)ceilf(h*style.raster_scale);size_t stride=(size_t)width*4;
    unsigned char *pixels=malloc(stride*height);CHECK(pixels);const nx_command *commands;size_t count;nx_color background;
    CHECK(nx_frame_commands(rvg_context(v),&commands,&count)==NX_OK&&nx_theme_color(rvg_context(v),NX_COLOR_BACKGROUND,&background)==NX_OK);
    CHECK(nx_headless_render_scaled(commands,count,pixels,width,height,stride,background,style.raster_scale)==NX_OK);
    char path[4096];int n=snprintf(path,sizeof path,"%s/%g-%s.ppm",directory,(double)w,name);CHECK(n>0&&(size_t)n<sizeof path);
    FILE *file=fopen(path,"wb");CHECK(file&&fprintf(file,"P6\n%u %u\n255\n",width,height)>0);
    for(uint32_t y=0;y<height;++y)for(uint32_t x=0;x<width;++x)CHECK(fwrite(pixels+(size_t)y*stride+(size_t)x*4,1,3,file)==3);
    CHECK(!fclose(file));free(pixels);
}
static void exercise(rv_owner *owner,nx_font_collection *fonts,rvg_style style,float w,float h,size_t total){
    rvg_view *v=NULL;CHECK(rvg_create(owner,fonts,style,&v)==NX_OK);frame(v,w,h);
    nx_semantic_view node;CHECK(semantic(v,RVG_STATUS,&node)&&!(node.state&NX_SEM_BUSY));CHECK(rvg_matches(v)==total);
    fprintf(stderr,"CASE german=%d size=%g width=%g height=%g focus=%" PRIu64 " mode=%d\n",style.german,(double)style.text_size,(double)w,(double)h,nx_focus_id(rvg_context(v)),(int)rvg_current_mode(v));
    CHECK(nx_focus_id(rvg_context(v))==RVG_FILTER_BUTTON);CHECK(rvg_last_row_layouts(v)<192);
    capture(v,w,h,style,"list");
    key(v,NX_KEY_TAB,w,h);CHECK(nx_focus_id(rvg_context(v))==RVG_REFRESH);
    key(v,NX_KEY_TAB,w,h);CHECK(nx_focus_id(rvg_context(v))==RVG_CLOSE);
    reverse_tab(v,w,h);CHECK(nx_focus_id(rvg_context(v))==RVG_REFRESH);
    reverse_tab(v,w,h);CHECK(nx_focus_id(rvg_context(v))==RVG_FILTER_BUTTON);
    /* Offscreen navigation uses the whole logical result, not mounted rows. */
    key(v,NX_KEY_END,w,h);CHECK(nx_focus_id(rvg_context(v))==RVG_ROWS+total-1);
    CHECK(semantic(v,RVG_ROWS+total-1,&node)&&contains(node.name,"\n"));
    press(v,RVG_ROWS+total-1,NX_ACTION_SELECT,w,h);CHECK(rvg_current_mode(v)==RVG_DETAIL&&nx_focus_id(rvg_context(v))==RVG_BACK);
    CHECK(semantic(v,RVG_DETAIL_TEXT,&node)&&(node.state&NX_SEM_READONLY)&&!(node.actions&NX_ACTION_SET_VALUE)&&contains(node.value,"Runtime:")&&contains(node.value,style.german?"Katalogrevision:":"Catalogue revision:"));
    capture(v,w,h,style,"detail-top");
    rv_key selected;CHECK(rv_owner_selection(owner,&selected));
    nx_semantic_view area;CHECK(semantic(v,RVG_SCROLL,&area));size_t pages=0;int overheight=node.bounds.height>area.bounds.height;
    while(node.bounds.y+node.bounds.height>area.bounds.y+area.bounds.height+.1f){
        CHECK(++pages<256);key(v,NX_KEY_PAGE_DOWN,w,h);CHECK(nx_focus_id(rvg_context(v))==RVG_BACK);CHECK(semantic(v,RVG_DETAIL_TEXT,&node)&&semantic(v,RVG_SCROLL,&area));
    }
    CHECK((pages>0)==overheight&&node.bounds.y>=area.bounds.y-node.bounds.height&&node.bounds.y+node.bounds.height<=area.bounds.y+area.bounds.height+.1f);
    capture(v,w,h,style,"detail-bottom");
    key(v,NX_KEY_ESCAPE,w,h);CHECK(rvg_current_mode(v)==RVG_LIST&&nx_focus_id(rvg_context(v))==RVG_ROWS+total-1);
    press(v,RVG_FILTER_BUTTON,NX_ACTION_PRESS,w,h);CHECK(rvg_current_mode(v)==RVG_FILTER&&nx_focus_id(rvg_context(v))==RVG_QUERY);
    capture(v,w,h,style,"filter");
    text_input(v,"no-such-record",w,h);press(v,RVG_BACK,NX_ACTION_PRESS,w,h);CHECK(rvg_current_mode(v)==RVG_LIST&&rvg_matches(v)==total);
    press(v,RVG_FILTER_BUTTON,NX_ACTION_PRESS,w,h);CHECK(semantic(v,RVG_QUERY,&node)&&contains(node.value,"no-such-record"));
    key(v,NX_KEY_ENTER,w,h);CHECK(rvg_current_mode(v)==RVG_LIST&&rvg_matches(v)==0);
    CHECK(semantic(v,RVG_EMPTY,&node));rv_key still;CHECK(rv_owner_selection(owner,&still)&&still.kind==selected.kind&&!memcmp(still.id,selected.id,16));
    CHECK(rvg_filter(v,(nx_utf8_span){"",0})==NX_OK);frame(v,w,h);CHECK(rvg_matches(v)==total);
    CHECK(rvg_filter(v,(nx_utf8_span){"\xff",1})==NX_ERROR_ARGUMENT&&rvg_matches(v)==total);
    char too_long[129];memset(too_long,'x',sizeof too_long);CHECK(rvg_filter(v,(nx_utf8_span){too_long,sizeof too_long})==NX_ERROR_LIMIT&&rvg_matches(v)==total);
    key(v,NX_KEY_HOME,w,h);CHECK(nx_focus_id(rvg_context(v))==RVG_ROWS);
    press(v,RVG_ROWS,NX_ACTION_SELECT,w,h);CHECK(rvg_current_mode(v)==RVG_DETAIL);
    /* Compact mode hides data inspection, preserves identity and offers Close. */
    frame(v,64,64);CHECK(!semantic(v,RVG_DETAIL_TEXT,&node)&&semantic(v,RVG_CLOSE,&node));
    frame(v,w,h);CHECK(rvg_current_mode(v)==RVG_DETAIL&&semantic(v,RVG_DETAIL_TEXT,&node)&&nx_focus_id(rvg_context(v))==RVG_BACK);
    key(v,NX_KEY_ESCAPE,w,h);CHECK(rvg_current_mode(v)==RVG_LIST);
    rvg_intent intent;CHECK(nx_semantics_request_action(rvg_context(v),RVG_REFRESH,NX_ACTION_PRESS)==NX_OK);
    CHECK(rvg_frame(v,w,h,&intent)==NX_OK&&intent.refresh&&!intent.close);
    CHECK(nx_semantics_request_action(rvg_context(v),RVG_CLOSE,NX_ACTION_PRESS)==NX_OK);
    CHECK(rvg_frame(v,w,h,&intent)==NX_OK&&!intent.refresh&&intent.close);
    CHECK(rvg_close(&v)==NX_OK&&!v);
}
/* Declared maximum-size presentation inputs, not a populated authority service.
   Original identity/context/free ownership is reused; SQL is never changed. */
static struct ae_recovery_inventory *synthetic(struct ae_catalogue *catalogue,size_t start,int64_t revision){
    struct ae_recovery_inventory *d=NULL;CHECK(ae_catalogue_recovery_inventory(catalogue,65536,&d)==0);
    free(d->records);d->count=65536-start;d->records=calloc(d->count,sizeof(*d->records));CHECK(d->records);
    memset(d->by_kind,0,sizeof d->by_kind);d->by_kind[AE_RECOVERY_OPERATION]=d->count;d->context.revision=revision;
    for(size_t i=0;i<d->count;++i){struct ae_recovery_record *r=&d->records[i];r->kind=AE_RECOVERY_OPERATION;r->id[14]=(uint8_t)((i+start)>>8);r->id[15]=(uint8_t)(i+start);r->revision=1;strcpy(r->state,"pending");}
    return d;
}
static void maximum(struct ae_catalogue *catalogue,const struct ae_catalogue_identity *identity,const struct ae_catalogue_info *context,nx_font_collection *fonts){
    rv_owner *owner=NULL;CHECK(rv_owner_create(identity,context,&owner)==NX_OK&&rv_owner_expect(owner,1)==NX_OK);
    struct ae_recovery_inventory *d=synthetic(catalogue,0,context->revision+100);
    CHECK(rv_owner_complete(owner,1,0,&d)==NX_OK&&!d);
    rvg_view *v=NULL;rvg_style style={0,16,1,NX_THEME_DARK};CHECK(rvg_create(owner,fonts,style,&v)==NX_OK);frame(v,800,600);
    CHECK(rvg_matches(v)==65536&&rvg_last_row_layouts(v)<192);size_t semantics;CHECK(nx_semantics_count(rvg_context(v),&semantics)==NX_OK&&semantics<200);
    key(v,NX_KEY_END,800,600);CHECK(nx_focus_id(rvg_context(v))==RVG_ROWS+65535);
    press(v,RVG_ROWS+65535,NX_ACTION_SELECT,800,600);nx_semantic_view node;CHECK(semantic(v,RVG_DETAIL_TEXT,&node)&&contains(node.value,"0000000000000000000000000000ffff"));
    key(v,NX_KEY_ESCAPE,800,600);
    CHECK(rvg_filter(v,(nx_utf8_span){"0000000000000000000000000000FFFF",32})==NX_OK);frame(v,800,600);CHECK(rvg_matches(v)==1);
    nx_context *previous=rvg_context(v);fail_malloc=1;
    CHECK(rvg_filter(v,(nx_utf8_span){"missing",7})==NX_ERROR_MEMORY&&rvg_context(v)==previous&&rvg_matches(v)==1);
    frame(v,800,600);
    CHECK(rv_owner_expect(owner,2)==NX_OK);d=synthetic(catalogue,1,context->revision+101);CHECK(rv_owner_complete(owner,2,0,&d)==NX_OK&&!d);
    fail_malloc=1;CHECK(rvg_sync(v)==NX_ERROR_MEMORY&&rvg_context(v)==previous&&rvg_matches(v)==1);frame(v,800,600);
    CHECK(rvg_sync(v)==NX_OK&&rvg_matches(v)==1);frame(v,800,600);
    key(v,NX_KEY_HOME,800,600);CHECK(nx_focus_id(rvg_context(v))==RVG_ROWS+65534);
    press(v,RVG_ROWS+65534,NX_ACTION_SELECT,800,600);CHECK(semantic(v,RVG_DETAIL_TEXT,&node)&&contains(node.value,"0000000000000000000000000000ffff"));
    CHECK(rvg_close(&v)==NX_OK);rv_owner_destroy(&owner);
    puts("RECOVERY_VIEW_65536_BOUND_FULL_ID_FILTER_SHIFTED_SNAPSHOT_AND_ALLOCATION_ROLLBACK_OK");
}
int main(int argc,char **argv){
    CHECK(argc==6);struct ae_catalogue_identity identity={.kind=!strcmp(argv[2],"system")?AE_CATALOGUE_SYSTEM:AE_CATALOGUE_USER};parse_id(argv[3],identity.authority_epoch);if(identity.kind==AE_CATALOGUE_USER)parse_id(argv[4],identity.owner);
    FILE *file=fopen(argv[5],"rb");CHECK(file&&!fseek(file,0,SEEK_END));long length=ftell(file);CHECK(length>0&&!fseek(file,0,SEEK_SET));unsigned char *bytes=malloc((size_t)length);CHECK(bytes&&fread(bytes,1,(size_t)length,file)==(size_t)length&&!fclose(file));
    nx_font *font=NULL;nx_font_collection *fonts=NULL;CHECK(nx_font_open(bytes,(size_t)length,NULL,&font)==NX_OK&&nx_font_collection_create(NULL,&fonts)==NX_OK&&nx_font_collection_add(fonts,font)==NX_OK);
    struct ae_catalogue *catalogue=NULL;CHECK(ae_catalogue_open(argv[1],&identity,&catalogue)==0);struct ae_catalogue_info before,after;CHECK(ae_catalogue_get_info(catalogue,&before)==0);
    rv_owner *owner=NULL;CHECK(rv_owner_create(&identity,&before,&owner)==NX_OK);
    rvg_style normal={0,16,1,NX_THEME_DARK};rvg_view *v=NULL;CHECK(rvg_create(owner,fonts,normal,&v)==NX_OK);frame(v,800,600);nx_semantic_view node;CHECK(semantic(v,RVG_EMPTY,&node)&&contains(node.name,"No observation"));
    CHECK(rv_owner_expect(owner,1)==NX_OK&&rvg_sync(v)==NX_OK);frame(v,800,600);CHECK(semantic(v,RVG_STATUS,&node)&&(node.state&NX_SEM_BUSY));
    struct ae_recovery_inventory *inventory=NULL;CHECK(ae_catalogue_recovery_inventory(catalogue,65536,&inventory)==0);size_t total=inventory->count;CHECK(total>2);
    CHECK(rv_owner_complete(owner,1,0,&inventory)==NX_OK&&!inventory&&rvg_sync(v)==NX_OK);frame(v,800,600);CHECK(rvg_matches(v)==total);
    CHECK(rv_owner_expect(owner,2)==NX_OK&&rv_owner_complete(owner,2,-EIO,NULL)==NX_OK&&rvg_sync(v)==NX_OK);frame(v,800,600);CHECK(semantic(v,RVG_STATUS,&node)&&(node.state&NX_SEM_INVALID)&&rvg_matches(v)==total);
    CHECK(rvg_close(&v)==NX_OK);
    exercise(owner,fonts,normal,800,600,total);
    rvg_style large={1,32,2,NX_THEME_HIGH_CONTRAST};exercise(owner,fonts,large,360,300,total);
    rvg_style light={0,16,1.25f,NX_THEME_LIGHT};exercise(owner,fonts,light,480,360,total);
    maximum(catalogue,&identity,&before,fonts);
    CHECK(ae_catalogue_get_info(catalogue,&after)==0&&!memcmp(&before,&after,sizeof before));
    rv_owner_destroy(&owner);CHECK(ae_catalogue_close(&catalogue)==0);nx_font_collection_destroy(fonts);nx_font_close(font);free(bytes);
    puts("RECOVERY_VIEW_LOGICAL_KEYS_DETAILS_FILTER_READING_COMPACT_STATES_AND_INTENTS_OK");return 0;
}
