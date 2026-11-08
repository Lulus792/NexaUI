#include "software_view.h"
#include <errno.h>
#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"software UI %d: %s\n",__LINE__,#x);exit(1);}}while(0)
static int fail_malloc;
void *__real_malloc(size_t);
void *__wrap_malloc(size_t n){if(fail_malloc){fail_malloc=0;return NULL;}return __real_malloc(n);}
static size_t checked_captions;
nx_result __real_nx_button(nx_context *,const nx_control_desc *,nx_interaction_state *);
nx_result __wrap_nx_button(nx_context *ctx,const nx_control_desc *desc,nx_interaction_state *out){
    if(desc&&desc->label_layout&&desc->parent_id==SWG_ROOT&&desc->id>=SWG_FILTER_BUTTON&&desc->id<SWG_CONTENT_TREE){
        CHECK(nx_text_layout_line_count(desc->label_layout)==1);
        CHECK(nx_text_layout_width(desc->label_layout)+16<=desc->bounds.width+.02);
        CHECK(nx_text_layout_height(desc->label_layout)+16<=desc->bounds.height+.02);++checked_captions;
    }
    return __real_nx_button(ctx,desc,out);
}
static sw_id id(uint32_t n){sw_id v={{0}};v.bytes[6]=0x40;v.bytes[8]=0x80;for(unsigned i=0;i<4;++i)v.bytes[15-i]=(uint8_t)(n>>(8*i));return v;}
static void hex_id(sw_id v,char out[33]){for(size_t i=0;i<16;++i)(void)snprintf(out+i*2,3,"%02x",v.bytes[i]);}
static int semantic(swg_view *v,uint64_t id_value,nx_semantic_view *out){uint32_t index;return nx_semantics_find(swg_context(v),id_value,&index)==NX_OK&&nx_semantics_get(swg_context(v),index,out)==NX_OK;}
static int contains(nx_utf8_span s,const char *text){size_t n=strlen(text);for(size_t i=0;i+n<=s.length;++i)if(!memcmp(s.data+i,text,n))return 1;return 0;}
static void frame(swg_view *v,float w,float h){swg_intent action;nx_result r=swg_frame(v,w,h,&action);if(r!=NX_OK)fprintf(stderr,"FRAME status=%d count=%zu mode=%d focus=%" PRIu64 "\n",(int)r,swg_matches(v),(int)swg_current_mode(v),nx_focus_id(swg_context(v)));CHECK(r==NX_OK&&!action.close&&!action.refresh);}
static void press(swg_view *v,uint64_t id_value,uint32_t action,float w,float h){CHECK(nx_semantics_request_action(swg_context(v),id_value,action)==NX_OK);frame(v,w,h);}
static void key(swg_view *v,nx_key k,uint32_t mods,float w,float h){nx_event e={0};e.type=NX_EVENT_KEY_DOWN;e.key=k;e.modifiers=mods;CHECK(swg_queue_event(v,&e)==NX_OK);frame(v,w,h);}
static void input(swg_view *v,const char *s,float w,float h){for(size_t i=0;s[i];++i){nx_event e={0};e.type=NX_EVENT_TEXT;e.scalar=(unsigned char)s[i];CHECK(swg_queue_event(v,&e)==NX_OK);}frame(v,w,h);}
static sw_result *clone(const sw_result *s){sw_result *v=calloc(1,sizeof(*v));CHECK(v);*v=*s;v->rows=s->count?calloc(s->count,sizeof(*v->rows)):NULL;CHECK(!s->count||v->rows);if(s->count)memcpy(v->rows,s->rows,s->count*sizeof(*v->rows));return v;}
static void capture(swg_view *v,float w,float h,swg_style style,const char *name){
    const char *directory=getenv("SWG_CAPTURE_DIR");if(!directory)return;
    uint32_t width=(uint32_t)ceilf(w*style.raster_scale),height=(uint32_t)ceilf(h*style.raster_scale);size_t stride=(size_t)width*4;
    unsigned char *pixels=malloc(stride*height);CHECK(pixels);const nx_command *commands;size_t count;nx_color background;
    CHECK(nx_frame_commands(swg_context(v),&commands,&count)==NX_OK&&nx_theme_color(swg_context(v),NX_COLOR_BACKGROUND,&background)==NX_OK);
    CHECK(nx_headless_render_scaled(commands,count,pixels,width,height,stride,background,style.raster_scale)==NX_OK);
    char path[4096];int n=snprintf(path,sizeof path,"%s/%g-%s.ppm",directory,(double)w,name);CHECK(n>0&&(size_t)n<sizeof path);
    FILE *file=fopen(path,"wb");CHECK(file&&fprintf(file,"P6\n%u %u\n255\n",width,height)>0);
    for(uint32_t y=0;y<height;++y)for(uint32_t x=0;x<width;++x)CHECK(fwrite(pixels+(size_t)y*stride+(size_t)x*4,1,3,file)==3);
    CHECK(!fclose(file));free(pixels);
}
static void exercise(const sw_result *base,nx_font_collection *fonts,swg_style style,float w,float h){
    sw_id targets[]={id(12),id(99),id(10),id(13),id(11)};sw_owner *owner=NULL;swg_view *v=NULL;nx_semantic_view node;
    CHECK(sw_owner_create(&base->scope,targets,5,&owner)==NX_OK&&swg_create(owner,fonts,style,&v)==NX_OK);frame(v,w,h);
    CHECK(semantic(v,SWG_EMPTY,&node)&&contains(node.name,style.german?"Keine Beobachtung":"No observation"));capture(v,w,h,style,"unobserved");
    CHECK(sw_owner_expect(owner,1)==NX_OK&&swg_sync(v)==NX_OK);frame(v,w,h);CHECK(semantic(v,SWG_STATUS,&node)&&(node.state&NX_SEM_BUSY));
    sw_result *value=clone(base);CHECK(sw_owner_complete(owner,1,0,&value)==NX_OK&&!value&&swg_sync(v)==NX_OK);frame(v,w,h);
    CHECK(swg_matches(v)==5&&nx_focus_id(swg_context(v))==SWG_FILTER_BUTTON&&swg_last_row_layouts(v)<192);
    CHECK(semantic(v,SWG_ROOT,&node)&&contains(node.description,style.german?"kein vollständiges Inventar":"not a complete inventory"));capture(v,w,h,style,"list");
    key(v,NX_KEY_TAB,0,w,h);CHECK(nx_focus_id(swg_context(v))==SWG_REFRESH);
    key(v,NX_KEY_TAB,0,w,h);CHECK(nx_focus_id(swg_context(v))==SWG_CLOSE);
    key(v,NX_KEY_TAB,NX_MOD_SHIFT,w,h);CHECK(nx_focus_id(swg_context(v))==SWG_REFRESH);
    key(v,NX_KEY_TAB,NX_MOD_SHIFT,w,h);CHECK(nx_focus_id(swg_context(v))==SWG_FILTER_BUTTON);
    key(v,NX_KEY_END,0,w,h);CHECK(nx_focus_id(swg_context(v))==SWG_ROWS+4);
    press(v,SWG_ROWS+4,NX_ACTION_SELECT,w,h);CHECK(swg_current_mode(v)==SWG_DETAIL&&nx_focus_id(swg_context(v))==SWG_BACK);
    CHECK(semantic(v,SWG_DETAIL_TEXT,&node)&&(node.state&NX_SEM_READONLY)&&!(node.actions&NX_ACTION_SET_VALUE));
    CHECK(contains(node.value,"00000000000040008000000000000063")&&contains(node.value,style.german?"weder Entfernung noch Ablehnung":"neither removal nor denial"));capture(v,w,h,style,"unavailable-detail");
    key(v,NX_KEY_ESCAPE,0,w,h);CHECK(swg_current_mode(v)==SWG_LIST&&nx_focus_id(swg_context(v))==SWG_ROWS+4);
    key(v,NX_KEY_HOME,0,w,h);CHECK(nx_focus_id(swg_context(v))==SWG_ROWS);
    press(v,SWG_ROWS,NX_ACTION_SELECT,w,h);CHECK(semantic(v,SWG_DETAIL_TEXT,&node)&&contains(node.value,"aetheri.probe")&&contains(node.value,"0000000000004000800000000000000e"));
    CHECK(contains(node.value,style.german?"Artefakt:":"Artifact:")&&contains(node.value,style.german?"kein Startticket":"not a launch ticket"));capture(v,w,h,style,"detail-top");
    nx_semantic_view area;CHECK(semantic(v,SWG_SCROLL,&area));size_t pages=0;int overheight=node.bounds.height>area.bounds.height;
    while(node.bounds.y+node.bounds.height>area.bounds.y+area.bounds.height+.1f){CHECK(++pages<256);key(v,NX_KEY_PAGE_DOWN,0,w,h);CHECK(nx_focus_id(swg_context(v))==SWG_BACK&&semantic(v,SWG_DETAIL_TEXT,&node)&&semantic(v,SWG_SCROLL,&area));}
    CHECK((pages>0)==overheight);capture(v,w,h,style,"detail-bottom");
    frame(v,64,64);CHECK(!semantic(v,SWG_DETAIL_TEXT,&node)&&semantic(v,SWG_CLOSE,&node)&&nx_focus_id(swg_context(v))==SWG_CLOSE);
    frame(v,240,96);CHECK(nx_focus_id(swg_context(v))==SWG_CLOSE);frame(v,w,h);CHECK(swg_current_mode(v)==SWG_DETAIL&&nx_focus_id(swg_context(v))==SWG_BACK);
    key(v,NX_KEY_ESCAPE,0,w,h);press(v,SWG_FILTER_BUTTON,NX_ACTION_PRESS,w,h);CHECK(swg_current_mode(v)==SWG_FILTER&&nx_focus_id(swg_context(v))==SWG_QUERY);
    input(v,"draft",w,h);capture(v,w,h,style,"filter");
    frame(v,240,160);CHECK(semantic(v,SWG_CLOSE,&node)&&node.bounds.x>=0&&node.bounds.y>=0&&node.bounds.x+node.bounds.width<=240&&node.bounds.y+node.bounds.height<=160);
    if(style.german&&style.text_size>=32)CHECK(!semantic(v,SWG_QUERY,&node)&&nx_focus_id(swg_context(v))==SWG_CLOSE);
    frame(v,w,h);CHECK(semantic(v,SWG_QUERY,&node)&&node.value.length==5&&!memcmp(node.value.data,"draft",5)&&nx_focus_id(swg_context(v))==SWG_QUERY);
    frame(v,240,160);key(v,NX_KEY_ESCAPE,0,240,160);frame(v,w,h);
    CHECK(swg_current_mode(v)==SWG_LIST&&nx_focus_id(swg_context(v))==SWG_FILTER_BUTTON);
    press(v,SWG_FILTER_BUTTON,NX_ACTION_PRESS,w,h);CHECK(semantic(v,SWG_QUERY,&node)&&node.value.length==5&&!memcmp(node.value.data,"draft",5)&&nx_focus_id(swg_context(v))==SWG_QUERY);
    CHECK(nx_semantics_request_text_value(swg_context(v),SWG_QUERY,(nx_utf8_span){"stale",5})==NX_OK);
    CHECK(sw_owner_expect(owner,2)==NX_OK);value=clone(base);CHECK(sw_owner_complete(owner,2,0,&value)==NX_OK&&!value&&swg_sync(v)==NX_OK);frame(v,w,h);
    CHECK(swg_current_mode(v)==SWG_FILTER&&nx_focus_id(swg_context(v))==SWG_QUERY&&semantic(v,SWG_QUERY,&node)&&node.value.length==5&&!memcmp(node.value.data,"draft",5));
    press(v,SWG_BACK,NX_ACTION_PRESS,w,h);CHECK(swg_matches(v)==5);
    press(v,SWG_FILTER_BUTTON,NX_ACTION_PRESS,w,h);key(v,NX_KEY_ENTER,0,w,h);CHECK(swg_current_mode(v)==SWG_LIST&&swg_matches(v)==0&&semantic(v,SWG_EMPTY,&node));capture(v,w,h,style,"no-matches");
    sw_id selected;CHECK(sw_owner_selection(owner,&selected)&&!memcmp(selected.bytes,id(10).bytes,16));
    CHECK(swg_filter(v,(nx_utf8_span){"AETHERI.PROBE",13})==NX_OK);frame(v,w,h);CHECK(swg_matches(v)==2);
    CHECK(swg_filter(v,(nx_utf8_span){"",0})==NX_OK);frame(v,w,h);
    CHECK(swg_filter(v,(nx_utf8_span){"\xff",1})==NX_ERROR_ARGUMENT&&swg_matches(v)==5);
    char too_long[129];memset(too_long,'x',sizeof too_long);CHECK(swg_filter(v,(nx_utf8_span){too_long,sizeof too_long})==NX_ERROR_LIMIT&&swg_matches(v)==5);
    key(v,NX_KEY_END,0,w,h);CHECK(nx_focus_id(swg_context(v))==SWG_ROWS+4);
    /* An action queued on the old context must not select on the next snapshot. */
    CHECK(nx_semantics_request_action(swg_context(v),SWG_ROWS+4,NX_ACTION_SELECT)==NX_OK&&sw_owner_expect(owner,3)==NX_OK);
    value=clone(base);CHECK(sw_owner_complete(owner,3,0,&value)==NX_OK&&!value&&swg_sync(v)==NX_OK);frame(v,w,h);
    fprintf(stderr,"REBIND german=%d mode=%d focus=%" PRIu64 " selected=%d last=%u\n",style.german,(int)swg_current_mode(v),nx_focus_id(swg_context(v)),sw_owner_selection(owner,&selected),(unsigned)selected.bytes[15]);
    CHECK(swg_current_mode(v)==SWG_LIST&&nx_focus_id(swg_context(v))==SWG_ROWS+4&&sw_owner_selection(owner,&selected)&&!memcmp(selected.bytes,id(10).bytes,16));
    CHECK(sw_owner_expect(owner,4)==NX_OK&&sw_owner_complete(owner,4,-EIO,NULL)==NX_OK&&swg_sync(v)==NX_OK);frame(v,w,h);
    CHECK(semantic(v,SWG_STATUS,&node)&&(node.state&NX_SEM_INVALID)&&swg_matches(v)==5);capture(v,w,h,style,"read-error");
    swg_intent intent;CHECK(nx_semantics_request_action(swg_context(v),SWG_REFRESH,NX_ACTION_PRESS)==NX_OK&&swg_frame(v,w,h,&intent)==NX_OK&&intent.refresh&&!intent.close);
    CHECK(nx_semantics_request_action(swg_context(v),SWG_CLOSE,NX_ACTION_PRESS)==NX_OK&&swg_frame(v,w,h,&intent)==NX_OK&&intent.close&&!intent.refresh);
    CHECK(swg_close(&v)==NX_OK&&!v);sw_owner_destroy(&owner);
    printf("STYLE_OK german=%d size=%g raster=%g width=%g height=%g\n",style.german,(double)style.text_size,(double)style.raster_scale,(double)w,(double)h);
}
static void maximum(const sw_scope *scope,nx_font_collection *fonts){
    const size_t n=SW_MAX_TARGETS;sw_id *targets=calloc(n,sizeof(*targets));sw_result *value=calloc(1,sizeof(*value));CHECK(targets&&value);
    value->scope=*scope;value->count=n;value->rows=calloc(n,sizeof(*value->rows));CHECK(value->rows);
    for(size_t i=0;i<n;++i){targets[i]=id((uint32_t)i+1000);value->rows[i].install=targets[i];value->rows[i].error=-ENOENT;}
    sw_owner *owner=NULL;CHECK(sw_owner_create(scope,targets,n,&owner)==NX_OK&&sw_owner_expect(owner,1)==NX_OK&&sw_owner_complete(owner,1,0,&value)==NX_OK&&!value);free(targets);
    swg_style style={0,16,1,NX_THEME_DARK};swg_view *v=NULL;CHECK(swg_create(owner,fonts,style,&v)==NX_OK);frame(v,800,600);
    size_t semantics=0;CHECK(swg_matches(v)==n&&swg_last_row_layouts(v)<192&&nx_semantics_count(swg_context(v),&semantics)==NX_OK&&semantics<200);
    key(v,NX_KEY_END,0,800,600);CHECK(nx_focus_id(swg_context(v))==SWG_ROWS+n-1);
    press(v,SWG_ROWS+n-1,NX_ACTION_SELECT,800,600);char last[33];hex_id(id((uint32_t)n+999),last);nx_semantic_view node;CHECK(semantic(v,SWG_DETAIL_TEXT,&node)&&contains(node.value,last));
    key(v,NX_KEY_ESCAPE,0,800,600);CHECK(swg_filter(v,(nx_utf8_span){last,32})==NX_OK);frame(v,800,600);CHECK(swg_matches(v)==1);
    nx_context *previous=swg_context(v);fail_malloc=1;CHECK(swg_filter(v,(nx_utf8_span){"missing",7})==NX_ERROR_MEMORY&&swg_context(v)==previous&&swg_matches(v)==1);frame(v,800,600);
    sw_snapshot *snapshot=NULL;CHECK(sw_owner_snapshot(owner,&snapshot)==NX_OK);value=clone(sw_snapshot_data(snapshot));sw_snapshot_release(&snapshot);++value->scope.context.revision;
    CHECK(sw_owner_expect(owner,2)==NX_OK&&sw_owner_complete(owner,2,0,&value)==NX_OK&&!value);
    fail_malloc=1;CHECK(swg_sync(v)==NX_ERROR_MEMORY&&swg_context(v)==previous&&swg_matches(v)==1);frame(v,800,600);
    CHECK(swg_sync(v)==NX_OK);frame(v,800,600);key(v,NX_KEY_HOME,0,800,600);CHECK(nx_focus_id(swg_context(v))==SWG_ROWS+n-1);
    CHECK(swg_close(&v)==NX_OK);sw_owner_destroy(&owner);puts("SOFTWARE_VIEW_DECLARED_100000_TARGET_FULL_ID_FILTER_ALLOCATION_ROLLBACK_OK");
}
int main(int argc,char **argv){
    CHECK(argc==3);FILE *file=fopen(argv[2],"rb");CHECK(file&&!fseek(file,0,SEEK_END));long length=ftell(file);CHECK(length>0&&!fseek(file,0,SEEK_SET));
    unsigned char *bytes=malloc((size_t)length);CHECK(bytes&&fread(bytes,1,(size_t)length,file)==(size_t)length&&!fclose(file));
    nx_font *font=NULL;nx_font_collection *fonts=NULL;CHECK(nx_font_open(bytes,(size_t)length,NULL,&font)==NX_OK&&nx_font_collection_create(NULL,&fonts)==NX_OK&&nx_font_collection_add(fonts,font)==NX_OK);
    struct ae_catalogue_identity identity={.kind=AE_CATALOGUE_SYSTEM};memcpy(identity.authority_epoch,id(1).bytes,16);struct ae_catalogue *catalogue=NULL;CHECK(ae_catalogue_open(argv[1],&identity,&catalogue)==0);
    sw_scope scope={.identity=identity,.os_uid=1000};memcpy(scope.account,id(2).bytes,16);CHECK(ae_catalogue_get_info(catalogue,&scope.context)==0);
    sw_id targets[]={id(12),id(99),id(10),id(13),id(11)};sw_result *base=NULL;CHECK(sw_read(catalogue,&scope,targets,5,&base)==0&&base&&base->count==5);
    exercise(base,fonts,(swg_style){0,16,1,NX_THEME_DARK},800,600);
    exercise(base,fonts,(swg_style){1,32,2,NX_THEME_HIGH_CONTRAST},360,300);
    exercise(base,fonts,(swg_style){0,16,1.25f,NX_THEME_LIGHT},480,360);
    exercise(base,fonts,(swg_style){1,48,1,NX_THEME_HIGH_CONTRAST},520,400);maximum(&scope,fonts);
    sw_result_free(&base);CHECK(ae_catalogue_close(&catalogue)==0);nx_font_collection_destroy(fonts);nx_font_close(font);free(bytes);
    CHECK(checked_captions>0);printf("SOFTWARE_HEADER_SINGLE_LINE_COMPLETE_CAPTION_BOUNDS_OK count=%zu\n",checked_captions);
    puts("SOFTWARE_VIEW_KEYS_FULL_METADATA_FILTER_DRAFT_STALE_ACTION_FOCUS_COMPACT_ERROR_INTENTS_OK");return 0;
}
