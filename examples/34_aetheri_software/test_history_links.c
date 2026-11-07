#define _GNU_SOURCE
/* Original metadata fixture getters; no service, filesystem or process effect. */
#include "software_history.h"
#include "software_view.h"
#include "software_worker.h"
#include <dirent.h>
#include <errno.h>
#include <inttypes.h>
#include <math.h>
#include <poll.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"history links %d: %s\n",__LINE__,#x);exit(1);}}while(0)
static int fault,stress,worker_phase;static pthread_t gui;
int __real_ae_catalogue_get_unit(struct ae_catalogue *,const struct ae_execution_profile *,const struct ae_effect_owner *,struct ae_unit_allocation_snapshot *);
int __wrap_ae_catalogue_get_unit(struct ae_catalogue *c,const struct ae_execution_profile *p,const struct ae_effect_owner *owner,struct ae_unit_allocation_snapshot *out){
    if(worker_phase)CHECK(!pthread_equal(gui,pthread_self()));
    int r=__real_ae_catalogue_get_unit(c,p,owner,out);if(!r){if(fault==1)out->allocation.id[6]=0;else if(fault==2)out->witnessed=2;else if(fault==3)out->witnessed_unix_ms=-1;}return r;
}
int __real_ae_catalogue_get_unit_custody(struct ae_catalogue *,const struct ae_execution_profile *,const struct ae_effect_owner *,struct ae_unit_custody_snapshot *);
int __wrap_ae_catalogue_get_unit_custody(struct ae_catalogue *c,const struct ae_execution_profile *p,const struct ae_effect_owner *owner,struct ae_unit_custody_snapshot *out){
    if(worker_phase)CHECK(!pthread_equal(gui,pthread_self()));
    if(fault==4)return -EIO;
    if(fault==5)return -ENOMEM;
    if(fault==6)return -ESTALE;
    int r=__real_ae_catalogue_get_unit_custody(c,p,owner,out);
    if(!r){if(fault==7)out->origin.boot[0]^=1;else if(fault==8)out->node_count=9;else if(fault==9)out->count=AE_UNIT_EFFECTS_MAX+1;else if(fault==10)out->steps[0].confirmed=2;else if(fault==11)out->steps[0].intended_unix_ms=-1;else if(fault==12)out->steps[0].node=8;
        if(stress){memset(out->origin.issuer,'1',sizeof out->origin.issuer-1);memcpy(out->origin.issuer,":1.",3);out->origin.issuer[sizeof out->origin.issuer-1]=0;memset(out->origin.manager,'2',sizeof out->origin.manager-1);memcpy(out->origin.manager,":1.",3);out->origin.manager[sizeof out->origin.manager-1]=0;memset(out->origin.manager_label,0xff,sizeof out->origin.manager_label-1);out->origin.manager_label[sizeof out->origin.manager_label-1]=0;}}
    return r;
}
static sw_id id(unsigned n){sw_id v={{0}};v.bytes[6]=0x40;v.bytes[8]=0x80;v.bytes[15]=(uint8_t)n;return v;}
static unsigned fds(void){DIR *d=opendir("/proc/self/fd");CHECK(d);unsigned n=0;struct dirent *e;while((e=readdir(d)))if(e->d_name[0]!='.')++n;CHECK(!closedir(d));return n;}
static unsigned char *read_file(const char *path,size_t *size){FILE *f=fopen(path,"rb");CHECK(f&&!fseek(f,0,SEEK_END));long n=ftell(f);CHECK(n>0&&!fseek(f,0,SEEK_SET));unsigned char *p=malloc((size_t)n);CHECK(p&&fread(p,1,(size_t)n,f)==(size_t)n&&!fclose(f));*size=(size_t)n;return p;}
static int semantic(swg_view *v,uint64_t key,nx_semantic_view *out){uint32_t at;return nx_semantics_find(swg_context(v),key,&at)==NX_OK&&nx_semantics_get(swg_context(v),at,out)==NX_OK;}
static int contains(nx_utf8_span text,const char *s){size_t n=strlen(s);for(size_t i=0;i+n<=text.length;++i)if(!memcmp(text.data+i,s,n))return 1;return 0;}
static void frame(swg_view *v,float w,float h){swg_intent intent;CHECK(swg_frame(v,w,h,&intent)==NX_OK&&!intent.close&&!intent.refresh);}
static void key(swg_view *v,nx_key k,float w,float h){nx_event e={0};e.type=NX_EVENT_KEY_DOWN;e.key=k;CHECK(swg_queue_event(v,&e)==NX_OK);e.type=NX_EVENT_KEY_UP;CHECK(swg_queue_event(v,&e)==NX_OK);frame(v,w,h);}
static void press(swg_view *v,uint64_t target,uint32_t action,float w,float h){CHECK(nx_semantics_request_action(swg_context(v),target,action)==NX_OK);frame(v,w,h);}
static void hex(const uint8_t *p,size_t n,char *out){for(size_t i=0;i<n;++i)snprintf(out+2*i,3,"%02x",(unsigned)p[i]);}
static void capture(swg_view *v,float w,float h,swg_style style,const char *name){
    const char *dir=getenv("SWG_CAPTURE_DIR");if(!dir)return;uint32_t width=(uint32_t)ceilf(w*style.raster_scale),height=(uint32_t)ceilf(h*style.raster_scale);size_t stride=(size_t)width*4;unsigned char *pixels=malloc(stride*height);CHECK(pixels);
    const nx_command *commands;size_t count;nx_color background;CHECK(nx_frame_commands(swg_context(v),&commands,&count)==NX_OK&&nx_theme_color(swg_context(v),NX_COLOR_BACKGROUND,&background)==NX_OK&&nx_headless_render_scaled(commands,count,pixels,width,height,stride,background,style.raster_scale)==NX_OK);
    char path[4096];int n=snprintf(path,sizeof path,"%s/%g-%s.ppm",dir,(double)w,name);CHECK(n>0&&(size_t)n<sizeof path);FILE *f=fopen(path,"wb");CHECK(f&&fprintf(f,"P6\n%u %u\n255\n",width,height)>0);for(uint32_t y=0;y<height;++y)for(uint32_t x=0;x<width;++x)CHECK(fwrite(pixels+(size_t)y*stride+(size_t)x*4,1,3,f)==3);CHECK(!fclose(f));free(pixels);
}
static sw_owner *owner_for(sw_result **apps){sw_owner *owner=NULL;sw_id targets[]={id(10),id(99)};CHECK(sw_owner_create(&(*apps)->scope,targets,2,&owner)==NX_OK&&sw_owner_expect(owner,1)==NX_OK&&sw_owner_complete(owner,1,0,apps)==NX_OK&&!*apps);return owner;}
static void inspect(sw_owner *owner,const swh_batch *batch,const swh_record *record,nx_font_collection *fonts,swg_style style,float w,float h){
    swg_view *v=NULL;CHECK(swg_create(owner,fonts,style,&v)==NX_OK&&swg_set_metadata(v,NULL,NX_OK,batch,0)==NX_OK);frame(v,w,h);press(v,SWG_ROWS,NX_ACTION_SELECT,w,h);press(v,SWG_HISTORY_BUTTON,NX_ACTION_PRESS,w,h);key(v,NX_KEY_ENTER,w,h);CHECK(swg_current_mode(v)==SWG_HISTORY_DETAIL);
    press(v,SWG_UNIT_LINKS_BUTTON,NX_ACTION_PRESS,w,h);CHECK(swg_current_mode(v)==SWG_UNIT_LINKS&&nx_focus_id(swg_context(v))==SWG_BACK);nx_semantic_view node,area;CHECK(semantic(v,SWG_DETAIL_TEXT,&node)&&(node.state&NX_SEM_READONLY)&&!(node.actions&NX_ACTION_SET_VALUE)&&contains(node.value,style.german?"Nur aufgezeichnete Unit-Bezüge":"Recorded unit associations only")&&contains(node.value,style.german?"unbekannt":"unknown"));
    char text[128];hex(record->digest,32,text);CHECK(contains(node.value,text));hex(record->profile.root_operation,16,text);CHECK(contains(node.value,text));
    if(record->unit_error)CHECK(contains(node.value,style.german?"Unit-Zuordnung nicht verfügbar":"unit allocation unavailable")&&!contains(node.value,"123500"));
    else{hex(record->unit.allocation.id,16,text);CHECK(contains(node.value,text)&&contains(node.value,"123500")&&contains(node.value,"123501"));const uint8_t *hashes[]={record->unit.allocation.recipe,record->unit.allocation.placement,record->unit.selection};for(size_t i=0;i<3;++i){hex(hashes[i],32,text);CHECK(contains(node.value,text));}}
    if(record->custody_error)CHECK(contains(node.value,style.german?"Verbindungsbesitz nicht verfügbar":"connection custody unavailable")&&!contains(node.value,style.german?"Aufgezeichnete Effekte:":"Recorded effects:"));
    else{CHECK(contains(node.value,record->custody.origin.issuer)&&contains(node.value,record->custody.origin.manager));const char *label=record->custody.origin.manager_label;
        if(nx_utf8_validate(label,strlen(label),NULL)==NX_OK)CHECK(contains(node.value,label));else{char bytes[1023];hex((const uint8_t *)label,strlen(label),bytes);CHECK(contains(node.value,bytes)&&contains(node.value,style.german?"Rohbytes, hex":"raw bytes, hex"));}
        snprintf(text,sizeof text,style.german?"Aufgezeichnete Effekte: %u":"Recorded effects: %u",record->custody.count);CHECK(contains(node.value,text));
        for(unsigned i=0;i<record->custody.count;++i){snprintf(text,sizeof text,style.german?"Aufgezeichneter Effekt %u:":"Recorded effect %u:",i+1);CHECK(contains(node.value,text));snprintf(text,sizeof text,"%" PRId64,record->custody.steps[i].intended_unix_ms);CHECK(contains(node.value,text));if(record->custody.steps[i].confirmed){snprintf(text,sizeof text,"%" PRId64,record->custody.steps[i].confirmed_unix_ms);CHECK(contains(node.value,text));}}}
    capture(v,w,h,style,"unit-links-top");CHECK(semantic(v,SWG_SCROLL,&area));size_t pages=0;while(node.bounds.y+node.bounds.height>area.bounds.y+area.bounds.height+.1f){CHECK(++pages<512);key(v,NX_KEY_PAGE_DOWN,w,h);CHECK(semantic(v,SWG_DETAIL_TEXT,&node)&&nx_focus_id(swg_context(v))==SWG_BACK);}capture(v,w,h,style,"unit-links-bottom");
    frame(v,64,64);CHECK(nx_focus_id(swg_context(v))==SWG_CLOSE);frame(v,w,h);CHECK(nx_focus_id(swg_context(v))==SWG_BACK);key(v,NX_KEY_ESCAPE,w,h);CHECK(swg_current_mode(v)==SWG_HISTORY_DETAIL&&nx_focus_id(swg_context(v))==SWG_UNIT_LINKS_BUTTON);
    CHECK(nx_semantics_request_action(swg_context(v),SWG_UNIT_LINKS_BUTTON,NX_ACTION_PRESS)==NX_OK&&swg_set_metadata(v,NULL,NX_OK,NULL,-EIO)==NX_OK);frame(v,w,h);CHECK(swg_current_mode(v)==SWG_DETAIL&&nx_focus_id(swg_context(v))==SWG_BACK);CHECK(swg_close(&v)==NX_OK);
}
int main(int argc,char **argv){
    CHECK(argc==5);gui=pthread_self();unsigned before=fds();size_t n;unsigned char *raw=read_file(argv[2],&n);struct ae_execution_profile p;CHECK(!ae_execution_decode(raw,n,&p));free(raw);
    int unit=!strncmp(argv[4],"unit",4)||!strncmp(argv[4],"custody",7),custody=!strncmp(argv[4],"custody",7),maximum=!strcmp(argv[4],"custody-max"),replacement=strstr(argv[4],"replacement")!=NULL;
    struct ae_catalogue_identity identity={.kind=AE_CATALOGUE_SYSTEM};memcpy(identity.authority_epoch,p.system_epoch,16);struct ae_catalogue *c=NULL;CHECK(!ae_catalogue_open(argv[1],&identity,&c));sw_scope scope={.identity=identity,.os_uid=1000};memcpy(scope.account,p.account,16);CHECK(!ae_catalogue_get_info(c,&scope.context));
    CHECK(replacement==(memcmp(p.system_runtime,scope.context.runtime_incarnation,16)!=0));sw_id targets[]={id(10),id(99)};sw_result *apps=NULL;CHECK(!sw_read(c,&scope,targets,2,&apps));swh_batch *batch=NULL;CHECK(!swh_read(c,apps,&p,1,&batch));
    /* Shape and whole-batch error propagation after a successful unit prefix. */
    if(unit)for(fault=1;fault<=(custody&&!replacement?12:6);++fault){swh_batch *none=NULL;CHECK(swh_read(c,apps,&p,1,&none)<0&&!none);}
    fault=0;
    if(maximum){swh_free(&batch);stress=1;CHECK(!swh_read(c,apps,&p,1,&batch));stress=0;}
    sw_owner *owner=owner_for(&apps);sw_snapshot *snapshot=NULL;CHECK(sw_owner_snapshot(owner,&snapshot)==NX_OK);const swh_record *record=NULL;size_t count=0;CHECK(swh_get(batch,snapshot,id(10),&record,&count)==NX_OK&&count==1);
    CHECK(record->unit_error==(unit?0:-EPROTONOSUPPORT)&&record->custody_error==(custody?0:-EPROTONOSUPPORT));
    if(unit)CHECK(record->unit.witnessed&&record->unit.allocation.id[15]==90&&record->unit.allocation.prepared_unix_ms==123500&&record->unit.witnessed_unix_ms==123501);
    if(custody)CHECK(record->custody.node_count==(maximum?8U:3U)&&record->custody.count==(replacement?0U:maximum?19U:9U)&&record->custody.prepared_unix_ms==123600);
    CHECK(!ae_catalogue_close(&c));unsigned char *font_bytes=read_file(argv[3],&n);nx_font *font=NULL;nx_font_collection *fonts=NULL;CHECK(nx_font_open(font_bytes,n,NULL,&font)==NX_OK&&nx_font_collection_create(NULL,&fonts)==NX_OK&&nx_font_collection_add(fonts,font)==NX_OK);
    inspect(owner,batch,record,fonts,(swg_style){0,16,1,NX_THEME_DARK},800,600);inspect(owner,batch,record,fonts,(swg_style){1,32,2,NX_THEME_HIGH_CONTRAST},360,300);inspect(owner,batch,record,fonts,(swg_style){0,16,1.25f,NX_THEME_LIGHT},480,400);
    swh_free(&batch);sw_snapshot_release(&snapshot);sw_owner_destroy(&owner);
    /* Existing worker performs the new original getters on its owner thread. */
    sww_worker *worker=NULL;sww_metadata config={NULL,0,&p,1};int error=0;worker_phase=1;CHECK(sww_start_with_metadata(argv[1],&identity,&config,&worker,&scope.context,&error)==NX_OK&&!error);uint64_t ticket=0;CHECK(sww_submit(worker,(sww_key){1,1},&scope,targets,2,&ticket)==NX_OK);nx_sim_message m={0};for(unsigned i=0;i<100&&!m.ticket;++i){CHECK(sww_receive(worker,&m)==NX_OK);if(!m.ticket){struct pollfd fd={sww_wake_fd(worker),POLLIN,0};CHECK(poll(&fd,1,100)>=0);}}
    sww_reply *reply=sww_reply_get(&m);CHECK(reply&&!reply->original_error&&!reply->history_error&&reply->history);nx_sim_message_dispose(&m);CHECK(sww_destroy(&worker)==NX_OK);worker_phase=0;
    nx_font_collection_destroy(fonts);nx_font_close(font);free(font_bytes);CHECK(fds()==before);puts("ORIGINAL_UNIT_CUSTODY_PROFILE_LINKS_UNKNOWN_RETIREMENT_WORKER_READONLY_VIEW_FULL_FIELDS_OK");return 0;
}
