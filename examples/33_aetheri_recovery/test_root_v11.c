#include "recovery_view.h"
#include "recovery_worker.h"
#include <dirent.h>
#include <errno.h>
#include <math.h>
#include <poll.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"Root11 recovery %d: %s\n",__LINE__,#x);exit(1);}}while(0)
static pthread_t gui;static int worker_phase,fail_read;static unsigned reads;
int __real_ae_catalogue_recovery_inventory(struct ae_catalogue *,size_t,struct ae_recovery_inventory **);
int __wrap_ae_catalogue_recovery_inventory(struct ae_catalogue *c,size_t limit,struct ae_recovery_inventory **v){
    if(worker_phase)CHECK(!pthread_equal(gui,pthread_self()));
    ++reads;if(fail_read)return -EIO;return __real_ae_catalogue_recovery_inventory(c,limit,v);
}
static unsigned fds(void){DIR *d=opendir("/proc/self/fd");CHECK(d);unsigned n=0;struct dirent *e;while((e=readdir(d)))if(e->d_name[0]!='.')++n;CHECK(!closedir(d));return n;}
static void hex(const uint8_t *p,size_t n,char *s){for(size_t i=0;i<n;++i)snprintf(s+i*2,3,"%02x",(unsigned)p[i]);}
static int contains(nx_utf8_span text,const char *s){size_t n=strlen(s);for(size_t i=0;i+n<=text.length;++i)if(!memcmp(text.data+i,s,n))return 1;return 0;}
static int node(rvg_view *v,uint64_t id,nx_semantic_view *out){uint32_t at;return nx_semantics_find(rvg_context(v),id,&at)==NX_OK&&nx_semantics_get(rvg_context(v),at,out)==NX_OK;}
static void frame(rvg_view *v,float w,float h){rvg_intent intent;CHECK(rvg_frame(v,w,h,&intent)==NX_OK&&!intent.close&&!intent.refresh);}
static void key(rvg_view *v,nx_key k,float w,float h){nx_event e={.type=NX_EVENT_KEY_DOWN,.key=k};CHECK(rvg_queue_event(v,&e)==NX_OK);e.type=NX_EVENT_KEY_UP;CHECK(rvg_queue_event(v,&e)==NX_OK);frame(v,w,h);}
static void capture(rvg_view *v,rvg_style style,float w,float h,const char *name){
    const char *dir=getenv("RVG_CAPTURE_DIR");if(!dir)return;uint32_t width=(uint32_t)ceilf(w*style.raster_scale),height=(uint32_t)ceilf(h*style.raster_scale);size_t stride=(size_t)width*4;unsigned char *pixels=malloc(stride*height);CHECK(pixels);
    const nx_command *commands;size_t count;nx_color background;CHECK(nx_frame_commands(rvg_context(v),&commands,&count)==NX_OK&&nx_theme_color(rvg_context(v),NX_COLOR_BACKGROUND,&background)==NX_OK&&nx_headless_render_scaled(commands,count,pixels,width,height,stride,background,style.raster_scale)==NX_OK);
    char path[4096];int n=snprintf(path,sizeof path,"%s/%g-%s.ppm",dir,(double)w,name);CHECK(n>0&&(size_t)n<sizeof path);FILE *f=fopen(path,"wb");CHECK(f&&fprintf(f,"P6\n%u %u\n255\n",width,height)>0);for(uint32_t y=0;y<height;++y)for(uint32_t x=0;x<width;++x)CHECK(fwrite(pixels+(size_t)y*stride+(size_t)x*4,1,3,f)==3);CHECK(!fclose(f));free(pixels);
}
static nx_sim_message receive(rw_worker *w){nx_sim_message m={0};for(unsigned i=0;i<100&&!m.ticket;++i){CHECK(rw_receive(w,&m)==NX_OK);if(!m.ticket){struct pollfd fd={rw_wake_fd(w),POLLIN,0};CHECK(poll(&fd,1,100)>=0);}}CHECK(m.ticket);return m;}
static void inspect(rv_owner *owner,rv_snapshot *snapshot,rv_key issuer,nx_font_collection *fonts,rvg_style style,float w,float h){
    size_t index=rv_snapshot_find(snapshot,issuer);CHECK(index!=SIZE_MAX);const struct ae_recovery_record *record=&rv_snapshot_data(snapshot)->records[index];rvg_view *v=NULL;CHECK(rvg_create(owner,fonts,style,&v)==NX_OK);frame(v,w,h);
    const char *name=style.german?"Unit-Aussteller":"Unit issuer";CHECK(rvg_filter(v,(nx_utf8_span){name,strlen(name)})==NX_OK);frame(v,w,h);CHECK(rvg_matches(v)==1);nx_semantic_view row,detail,area;CHECK(node(v,RVG_ROWS+index,&row)&&contains(row.name,name));
    capture(v,style,w,h,"issuer-list");CHECK(nx_semantics_request_action(rvg_context(v),RVG_ROWS+index,NX_ACTION_SELECT)==NX_OK);frame(v,w,h);CHECK(rvg_current_mode(v)==RVG_DETAIL&&nx_focus_id(rvg_context(v))==RVG_BACK);
    CHECK(node(v,RVG_DETAIL_TEXT,&detail)&&(detail.state&NX_SEM_READONLY)&&!(detail.actions&NX_ACTION_SET_VALUE)&&contains(detail.value,name)&&contains(detail.value,"issuer_retirement_unsettled")&&contains(detail.value,"Schema: 11")&&contains(detail.value,"observed"));char id[33];hex(record->id,16,id);CHECK(contains(detail.value,id));hex(record->related,16,id);CHECK(contains(detail.value,id));hex(rv_snapshot_data(snapshot)->context.runtime_incarnation,16,id);CHECK(contains(detail.value,id));
    capture(v,style,w,h,"issuer-detail-top");CHECK(node(v,RVG_SCROLL,&area));size_t pages=0;while(detail.bounds.y+detail.bounds.height>area.bounds.y+area.bounds.height+.1f){CHECK(++pages<512);key(v,NX_KEY_PAGE_DOWN,w,h);CHECK(node(v,RVG_DETAIL_TEXT,&detail)&&nx_focus_id(rvg_context(v))==RVG_BACK);}capture(v,style,w,h,"issuer-detail-bottom");
    frame(v,64,64);CHECK(nx_focus_id(rvg_context(v))==RVG_CLOSE);frame(v,w,h);CHECK(nx_focus_id(rvg_context(v))==RVG_BACK);key(v,NX_KEY_ESCAPE,w,h);CHECK(rvg_current_mode(v)==RVG_LIST&&nx_focus_id(rvg_context(v))==RVG_ROWS+index);CHECK(rvg_close(&v)==NX_OK);
}
int main(int argc,char **argv){
    CHECK(argc==3);gui=pthread_self();unsigned before=fds();struct ae_catalogue_identity identity={.kind=AE_CATALOGUE_SYSTEM};identity.authority_epoch[6]=0x40;identity.authority_epoch[8]=0x80;identity.authority_epoch[15]=1;
    struct ae_catalogue *c=NULL;struct ae_catalogue_info context;CHECK(!ae_catalogue_open(argv[1],&identity,&c)&&!ae_catalogue_get_info(c,&context));struct ae_recovery_inventory *inventory=NULL;CHECK(!ae_catalogue_recovery_inventory(c,AE_RECOVERY_MAX_RECORDS,&inventory)&&inventory->schema_version==11&&inventory->by_kind[AE_RECOVERY_UNIT_ISSUER]==1);
    rv_owner *owner=NULL;CHECK(rv_owner_create(&identity,&context,&owner)==NX_OK&&rv_owner_expect(owner,1)==NX_OK&&rv_owner_complete(owner,1,0,&inventory)==NX_OK&&!inventory);rv_snapshot *snapshot=NULL;CHECK(rv_owner_snapshot(owner,&snapshot)==NX_OK);
    rv_key issuer={.kind=AE_RECOVERY_UNIT_ISSUER};size_t at=0;const struct ae_recovery_inventory *data=rv_snapshot_data(snapshot);while(at<data->count&&data->records[at].kind!=issuer.kind)++at;CHECK(at<data->count);memcpy(issuer.id,data->records[at].id,16);CHECK(issuer.id[15]==90&&data->records[at].has_related&&data->records[at].related[15]==41);
    rv_key allocation=issuer;allocation.kind=AE_RECOVERY_UNIT_ALLOCATION;CHECK(rv_snapshot_find(snapshot,allocation)!=SIZE_MAX&&rv_snapshot_find(snapshot,allocation)!=at&&rv_owner_select(owner,issuer)==NX_OK);
    for(unsigned bad=0;bad<4;++bad){inventory=NULL;CHECK(!ae_catalogue_recovery_inventory(c,AE_RECOVERY_MAX_RECORDS,&inventory));struct ae_recovery_inventory *held=inventory;CHECK(rv_owner_expect(owner,bad+2)==NX_OK);if(bad==0)inventory->schema_version=10;else if(bad==1)inventory->schema_version=12;else if(bad==2){inventory->identity.kind=AE_CATALOGUE_USER;}else ++inventory->by_kind[AE_RECOVERY_UNIT_ISSUER];CHECK(rv_owner_complete(owner,bad+2,0,&inventory)!=NX_OK&&inventory==held);ae_recovery_inventory_free(&inventory);}
    CHECK(rv_owner_expect(owner,6)==NX_OK&&!ae_catalogue_recovery_inventory(c,AE_RECOVERY_MAX_RECORDS,&inventory)&&rv_owner_complete(owner,6,0,&inventory)==NX_OK&&!inventory&&rv_owner_state(owner)==RV_OBSERVED);
    CHECK(!ae_catalogue_close(&c));FILE *file=fopen(argv[2],"rb");CHECK(file&&!fseek(file,0,SEEK_END));long size=ftell(file);CHECK(size>0&&!fseek(file,0,SEEK_SET));unsigned char *bytes=malloc((size_t)size);CHECK(bytes&&fread(bytes,1,(size_t)size,file)==(size_t)size&&!fclose(file));nx_font *font=NULL;nx_font_collection *fonts=NULL;CHECK(nx_font_open(bytes,(size_t)size,NULL,&font)==NX_OK&&nx_font_collection_create(NULL,&fonts)==NX_OK&&nx_font_collection_add(fonts,font)==NX_OK);
    inspect(owner,snapshot,issuer,fonts,(rvg_style){0,16,1,NX_THEME_DARK},800,600);inspect(owner,snapshot,issuer,fonts,(rvg_style){1,32,2,NX_THEME_HIGH_CONTRAST},360,300);inspect(owner,snapshot,issuer,fonts,(rvg_style){0,16,1.25f,NX_THEME_LIGHT},480,400);
    rw_worker *worker=NULL;int error=0;worker_phase=1;unsigned prior=reads;CHECK(rw_start(argv[1],&identity,&worker,&context,&error)==NX_OK&&!error&&reads==prior);uint64_t ticket=0;CHECK(rw_submit(worker,(rw_key){1,1},&ticket)==NX_OK);nx_sim_message m=receive(worker);rw_reply *reply=rw_reply_get(&m);CHECK(reply&&!reply->original_error&&reply->inventory&&reply->inventory->schema_version==11&&reply->inventory->by_kind[AE_RECOVERY_UNIT_ISSUER]==1);struct ae_recovery_inventory *owned=NULL,*twice=NULL;CHECK(rw_reply_take(reply,&owned)==NX_OK&&owned&&rw_reply_take(reply,&twice)==NX_ERROR_STATE&&!twice);nx_sim_message_dispose(&m);
    fail_read=1;CHECK(rw_submit(worker,(rw_key){1,2},&ticket)==NX_OK);m=receive(worker);reply=rw_reply_get(&m);CHECK(reply&&reply->original_error==-EIO&&!reply->inventory);nx_sim_message_dispose(&m);CHECK(rw_destroy(&worker)==NX_OK);worker_phase=0;fail_read=0;ae_recovery_inventory_free(&owned);
    nx_font_collection_destroy(fonts);nx_font_close(font);free(bytes);rv_owner_destroy(&owner);CHECK(rv_snapshot_data(snapshot)->schema_version==11&&rv_snapshot_find(snapshot,issuer)==at);rv_snapshot_release(&snapshot);CHECK(fds()==before);puts("ROOT11_ORIGINAL_ISSUER_KIND_KEYS_READONLY_VIEW_WORKER_OWNERSHIP_OK");return 0;
}
