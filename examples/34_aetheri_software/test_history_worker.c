#define _GNU_SOURCE
/* Original historical getters plus application-private worker/view composition.
   The optional long strings are synthetic stress inputs, not OS observations. */
#include "software_worker.h"
#include "software_view.h"
#include <dirent.h>
#include <errno.h>
#include <inttypes.h>
#include <math.h>
#include <poll.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"history worker %d: %s\n",__LINE__,#x);exit(1);}}while(0)
static pthread_t gui;
static atomic_int calls,inject,long_strings;
static pthread_mutex_t lock=PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t changed=PTHREAD_COND_INITIALIZER;
static int hold,entered,released;
int __real_swh_read(struct ae_catalogue *,const sw_result *,const struct ae_execution_profile *,size_t,swh_batch **);
int __wrap_swh_read(struct ae_catalogue *c,const sw_result *r,const struct ae_execution_profile *p,size_t n,swh_batch **out){
    CHECK(!pthread_equal(gui,pthread_self()));atomic_fetch_add(&calls,1);
    pthread_mutex_lock(&lock);if(hold){entered=1;pthread_cond_signal(&changed);while(!released)pthread_cond_wait(&changed,&lock);hold=0;}pthread_mutex_unlock(&lock);
    int error=atomic_exchange(&inject,0);return error?error:__real_swh_read(c,r,p,n,out);
}
int __real_ae_catalogue_get_execution_process(struct ae_catalogue *,const struct ae_execution_profile *,enum ae_execution_process_role,struct ae_execution_process_snapshot *);
int __wrap_ae_catalogue_get_execution_process(struct ae_catalogue *c,const struct ae_execution_profile *p,enum ae_execution_process_role role,struct ae_execution_process_snapshot *out){
    CHECK(!pthread_equal(gui,pthread_self()));int r=__real_ae_catalogue_get_execution_process(c,p,role,out);
    if(!r&&atomic_load(&long_strings)){
        memset(out->process.label,'L',sizeof out->process.label-1);out->process.label[sizeof out->process.label-1]=0;
        memset(out->process.cgroup,'g',sizeof out->process.cgroup-1);out->process.cgroup[0]='/';out->process.cgroup[sizeof out->process.cgroup-1]=0;
    }return r;
}
static void gate_begin(void){pthread_mutex_lock(&lock);hold=1;entered=released=0;pthread_mutex_unlock(&lock);}
static void gate_wait(void){pthread_mutex_lock(&lock);while(!entered)pthread_cond_wait(&changed,&lock);pthread_mutex_unlock(&lock);}
static void gate_release(void){pthread_mutex_lock(&lock);released=1;pthread_cond_signal(&changed);pthread_mutex_unlock(&lock);}
static sw_id id(unsigned n){sw_id v={{0}};v.bytes[6]=0x40;v.bytes[8]=0x80;v.bytes[15]=(uint8_t)n;return v;}
static unsigned fds(void){DIR *d=opendir("/proc/self/fd");CHECK(d);unsigned n=0;struct dirent *e;while((e=readdir(d)))if(e->d_name[0]!='.')++n;CHECK(!closedir(d));return n;}
static unsigned char *read_file(const char *path,size_t *size){FILE *f=fopen(path,"rb");CHECK(f&&!fseek(f,0,SEEK_END));long n=ftell(f);CHECK(n>0&&!fseek(f,0,SEEK_SET));unsigned char *p=malloc((size_t)n);CHECK(p&&fread(p,1,(size_t)n,f)==(size_t)n&&!fclose(f));*size=(size_t)n;return p;}
static nx_sim_message receive(sww_worker *w){nx_sim_message m={0};for(unsigned i=0;i<100;++i){CHECK(sww_receive(w,&m)==NX_OK);if(m.ticket)return m;struct pollfd p={sww_wake_fd(w),POLLIN,0};CHECK(poll(&p,1,100)>=0);}CHECK(0);return m;}
static int semantic(swg_view *v,uint64_t key,nx_semantic_view *out){uint32_t at;return nx_semantics_find(swg_context(v),key,&at)==NX_OK&&nx_semantics_get(swg_context(v),at,out)==NX_OK;}
static int contains(nx_utf8_span text,const char *s){size_t n=strlen(s);for(size_t i=0;i+n<=text.length;++i)if(!memcmp(text.data+i,s,n))return 1;return 0;}
static void frame(swg_view *v,float w,float h){swg_intent intent;CHECK(swg_frame(v,w,h,&intent)==NX_OK&&!intent.close&&!intent.refresh);}
static void key(swg_view *v,nx_key k,float w,float h){nx_event e={0};e.type=NX_EVENT_KEY_DOWN;e.key=k;CHECK(swg_queue_event(v,&e)==NX_OK);e.type=NX_EVENT_KEY_UP;CHECK(swg_queue_event(v,&e)==NX_OK);frame(v,w,h);}
static void press(swg_view *v,uint64_t target,uint32_t action,float w,float h){CHECK(nx_semantics_request_action(swg_context(v),target,action)==NX_OK);frame(v,w,h);}
static void hex(const uint8_t *p,size_t n,char *out){for(size_t i=0;i<n;++i)snprintf(out+2*i,3,"%02x",(unsigned)p[i]);}
static void capture(swg_view *v,float w,float h,swg_style style,const char *name){
    const char *dir=getenv("SWG_CAPTURE_DIR");if(!dir)return;
    uint32_t width=(uint32_t)ceilf(w*style.raster_scale),height=(uint32_t)ceilf(h*style.raster_scale);size_t stride=(size_t)width*4;unsigned char *pixels=malloc(stride*height);CHECK(pixels);
    const nx_command *commands;size_t count;nx_color background;CHECK(nx_frame_commands(swg_context(v),&commands,&count)==NX_OK&&nx_theme_color(swg_context(v),NX_COLOR_BACKGROUND,&background)==NX_OK&&nx_headless_render_scaled(commands,count,pixels,width,height,stride,background,style.raster_scale)==NX_OK);
    char path[4096];int n=snprintf(path,sizeof path,"%s/%g-%s.ppm",dir,(double)w,name);CHECK(n>0&&(size_t)n<sizeof path);FILE *f=fopen(path,"wb");CHECK(f&&fprintf(f,"P6\n%u %u\n255\n",width,height)>0);
    for(uint32_t y=0;y<height;++y)for(uint32_t x=0;x<width;++x)CHECK(fwrite(pixels+(size_t)y*stride+(size_t)x*4,1,3,f)==3);
    CHECK(!fclose(f));free(pixels);
}
static void details(swg_view *v,const swh_record *r,int unsupported,swg_style style,float w,float h){
    nx_semantic_view node,area;CHECK(semantic(v,SWG_DETAIL_TEXT,&node)&&(node.state&NX_SEM_READONLY)&&!(node.actions&NX_ACTION_SET_VALUE));
    CHECK(contains(node.value,style.german?"Nur historische Registrierungsmetadaten":"Historical enrollment metadata only")&&contains(node.value,style.german?"unbekannt":"unknown"));
    const struct ae_execution_profile *p=&r->profile;
    const uint8_t *ids[]={p->system_epoch,p->system_runtime,p->broker_epoch,p->broker_runtime,p->boot,p->account,p->install,p->generation,p->data,p->storage,p->launch,p->lease,p->root_operation,p->user_operation};
    for(size_t i=0;i<14;++i){char text[33];hex(ids[i],16,text);CHECK(contains(node.value,text));}
    const uint8_t *hashes[]={r->digest,p->artifact,p->starter_digest,p->request_digest,p->allocation_digest};
    for(size_t i=0;i<5;++i){char text[65];hex(hashes[i],32,text);CHECK(contains(node.value,text));}
    if(unsupported){CHECK(r->starter_error==-EPROTONOSUPPORT&&r->app_error==-EPROTONOSUPPORT&&contains(node.value,"-93")&&!contains(node.value,"4242"));}
    else{CHECK(!r->starter_error&&!r->app_error&&contains(node.value,"4242")&&contains(node.value,"18446744073709551615")&&contains(node.value,"9223372036854775839")&&contains(node.value,"124000")&&contains(node.value,"124002")&&contains(node.value,r->starter.process.unit)&&contains(node.value,r->app.process.cgroup));}
    capture(v,w,h,style,"history-top");size_t pages=0;CHECK(semantic(v,SWG_SCROLL,&area));
    while(node.bounds.y+node.bounds.height>area.bounds.y+area.bounds.height+.1f){CHECK(++pages<512);key(v,NX_KEY_PAGE_DOWN,w,h);CHECK(semantic(v,SWG_DETAIL_TEXT,&node)&&semantic(v,SWG_SCROLL,&area)&&nx_focus_id(swg_context(v))==SWG_BACK);}
    capture(v,w,h,style,"history-bottom");
}
static void view_check(sw_owner *owner,const swh_batch *batch,int error,int unsupported,nx_font_collection *fonts,swg_style style,float w,float h){
    swg_view *v=NULL;CHECK(swg_create(owner,fonts,style,&v)==NX_OK&&swg_set_metadata(v,NULL,NX_OK,batch,error)==NX_OK);frame(v,w,h);
    press(v,SWG_ROWS,NX_ACTION_SELECT,w,h);CHECK(swg_current_mode(v)==SWG_DETAIL);nx_semantic_view node;
    CHECK(semantic(v,SWG_HISTORY_BUTTON,&node)&&((node.state&NX_SEM_DISABLED)!=0)==(error!=0));
    if(error){CHECK(semantic(v,SWG_DETAIL_TEXT,&node)&&contains(node.value,style.german?"nicht verfügbar":"unavailable")&&contains(node.value,style.german?"unbekannt":"unknown"));CHECK(swg_close(&v)==NX_OK);return;}
    sw_snapshot *s=NULL;CHECK(sw_owner_snapshot(owner,&s)==NX_OK);const swh_record *records=NULL;size_t count=0;CHECK(swh_get(batch,s,id(10),&records,&count)==NX_OK&&count);
    press(v,SWG_HISTORY_BUTTON,NX_ACTION_PRESS,w,h);CHECK(swg_current_mode(v)==SWG_HISTORY&&nx_focus_id(swg_context(v))==SWG_HISTORY_ROWS&&semantic(v,SWG_HISTORY_TREE,&node));
    CHECK(semantic(v,SWG_HISTORY_ROWS,&node)&&node.role==NX_ROLE_TREE_ITEM);char digest[65];hex(records[0].digest,32,digest);CHECK(contains(node.name,digest));
    capture(v,w,h,style,"history-tree");nx_semantic_view tree_area;CHECK(semantic(v,SWG_SCROLL,&tree_area));size_t tree_pages=0;
    while(node.bounds.y+node.bounds.height>tree_area.bounds.y+tree_area.bounds.height+.1f){CHECK(++tree_pages<512);key(v,NX_KEY_PAGE_DOWN,w,h);CHECK(semantic(v,SWG_HISTORY_ROWS,&node)&&nx_focus_id(swg_context(v))==SWG_HISTORY_ROWS);}
    capture(v,w,h,style,"history-tree-end");key(v,NX_KEY_END,w,h);CHECK(nx_focus_id(swg_context(v))==SWG_HISTORY_ROWS+count-1);key(v,NX_KEY_HOME,w,h);
    key(v,NX_KEY_ENTER,w,h);CHECK(swg_current_mode(v)==SWG_HISTORY_DETAIL);details(v,records,unsupported,style,w,h);
    key(v,NX_KEY_ESCAPE,w,h);CHECK(swg_current_mode(v)==SWG_HISTORY&&nx_focus_id(swg_context(v))==SWG_HISTORY_ROWS);
    frame(v,64,64);CHECK(nx_focus_id(swg_context(v))==SWG_CLOSE);frame(v,w,h);CHECK(nx_focus_id(swg_context(v))==SWG_HISTORY_ROWS);
    key(v,NX_KEY_ESCAPE,w,h);CHECK(swg_current_mode(v)==SWG_DETAIL&&nx_focus_id(swg_context(v))==SWG_HISTORY_BUTTON);
    /* Replacement retires queued tree activation, rather than using an old ordinal. */
    press(v,SWG_HISTORY_BUTTON,NX_ACTION_PRESS,w,h);CHECK(nx_semantics_request_action(swg_context(v),SWG_HISTORY_ROWS,NX_ACTION_SELECT)==NX_OK);
    CHECK(swg_set_metadata(v,NULL,NX_OK,NULL,-EIO)==NX_OK);frame(v,w,h);CHECK(swg_current_mode(v)==SWG_DETAIL&&nx_focus_id(swg_context(v))==SWG_BACK&&semantic(v,SWG_HISTORY_BUTTON,&node)&&(node.state&NX_SEM_DISABLED));
    CHECK(swg_set_metadata(v,NULL,NX_OK,NULL,0)==NX_OK);frame(v,w,h);CHECK(semantic(v,SWG_DETAIL_TEXT,&node)&&contains(node.value,style.german?"kein leeres Prozessinventar":"not an empty process inventory"));
    CHECK(swg_set_metadata(v,NULL,NX_OK,batch,0)==NX_OK);frame(v,w,h);press(v,SWG_HISTORY_BUTTON,NX_ACTION_PRESS,w,h);key(v,NX_KEY_ENTER,w,h);
    /* A new snapshot also retires profile selections, even with identical rows. */
    const sw_result *old=sw_snapshot_data(s);sw_result *copy=calloc(1,sizeof(*copy));CHECK(copy);*copy=*old;copy->rows=calloc(old->count,sizeof(*copy->rows));CHECK(copy->rows);memcpy(copy->rows,old->rows,old->count*sizeof(*copy->rows));
    CHECK(sw_owner_expect(owner,sw_owner_epoch(owner)+100)==NX_OK);uint64_t ticket=sw_owner_pending(owner);CHECK(sw_owner_complete(owner,ticket,0,&copy)==NX_OK&&!copy&&swg_sync(v)==NX_OK);frame(v,w,h);CHECK(swg_current_mode(v)==SWG_DETAIL&&nx_focus_id(swg_context(v))==SWG_BACK);
    sw_snapshot_release(&s);CHECK(swg_close(&v)==NX_OK&&!v);
}
static void request_hash(struct ae_execution_profile *p){
    struct ae_launch_request q={.install_revision=p->install_revision};const uint8_t *src[]={p->system_epoch,p->lease,p->launch,p->broker_epoch,p->broker_runtime,p->account,p->install,p->generation,p->data,p->boot};uint8_t *dst[]={q.system_epoch,q.lease,q.launch,q.broker_epoch,q.broker_runtime,q.account,q.install,q.generation,q.data,q.boot};
    for(size_t i=0;i<10;++i)memcpy(dst[i],src[i],16);
    strcpy(q.action,"run");strcpy(q.session,"p0");strcpy(q.seat,"none");CHECK(!ae_launch_fingerprint(&q,p->request_digest));
}
int main(int argc,char **argv){
    CHECK(argc==5);gui=pthread_self();unsigned before=fds();int corrupt=!strcmp(argv[4],"corrupt"),unsupported=!strcmp(argv[4],"unsupported");
    size_t n;unsigned char *raw=read_file(argv[2],&n);struct ae_execution_profile original;CHECK(!ae_execution_decode(raw,n,&original));free(raw);
    struct ae_catalogue_identity identity={.kind=AE_CATALOGUE_SYSTEM};memcpy(identity.authority_epoch,id(1).bytes,16);sw_scope scope={.identity=identity,.os_uid=1000};memcpy(scope.account,id(2).bytes,16);
    sww_worker *w=NULL;struct ae_execution_profile profiles[2]={original,original};sww_metadata metadata={NULL,0,profiles,1};int error=0;
    metadata.profile_count=SWH_MAX_PROFILES+1;CHECK(sww_start_with_metadata(argv[1],&identity,&metadata,&w,&scope.context,&error)==NX_ERROR_LIMIT&&!w);
    metadata.profile_count=2;CHECK(sww_start_with_metadata(argv[1],&identity,&metadata,&w,&scope.context,&error)==NX_ERROR_ARGUMENT&&!w);
    metadata.profile_count=1;CHECK(sww_start_with_metadata(argv[1],&identity,&metadata,&w,&scope.context,&error)==NX_OK&&!error&&!atomic_load(&calls));memset(profiles,0,sizeof profiles);
    sw_id targets[]={id(10),id(99)};uint64_t ticket=0;CHECK(sww_submit(w,(sww_key){7,1},&scope,targets,2,&ticket)==NX_OK);nx_sim_message m=receive(w);sww_reply *reply=sww_reply_get(&m);
    CHECK(reply&&reply->key.view==7&&reply->key.request==1&&!reply->original_error&&reply->result&&atomic_load(&calls)==1);
    CHECK(reply->history_error==(corrupt?-EUCLEAN:0)&&(reply->history!=NULL)==!corrupt);
    sw_owner *owner=NULL;CHECK(sw_owner_create(&scope,targets,2,&owner)==NX_OK&&sw_owner_expect(owner,ticket)==NX_OK&&sw_owner_complete(owner,ticket,0,&reply->result)==NX_OK);
    swh_batch *batch=NULL;if(!corrupt){CHECK(sww_reply_take_history(reply,&batch)==NX_OK&&batch);swh_batch *none=NULL;CHECK(sww_reply_take_history(reply,&none)==NX_ERROR_STATE&&!none);}
    nx_sim_message_dispose(&m);
    unsigned char *font_bytes=read_file(argv[3],&n);nx_font *font=NULL;nx_font_collection *fonts=NULL;CHECK(nx_font_open(font_bytes,n,NULL,&font)==NX_OK&&nx_font_collection_create(NULL,&fonts)==NX_OK&&nx_font_collection_add(fonts,font)==NX_OK);
    view_check(owner,batch,corrupt?-EUCLEAN:0,unsupported,fonts,(swg_style){0,16,1,NX_THEME_DARK},800,600);
    view_check(owner,batch,corrupt?-EUCLEAN:0,unsupported,fonts,(swg_style){1,32,2,NX_THEME_HIGH_CONTRAST},360,300);
    view_check(owner,batch,corrupt?-EUCLEAN:0,unsupported,fonts,(swg_style){0,16,1.25f,NX_THEME_LIGHT},480,400);
    /* History failure is independent of the separately valid selected-app result. */
    const int faults[]={-ENOMEM,-EIO};for(size_t i=0;i<2;++i){atomic_store(&inject,faults[i]);CHECK(sww_submit(w,(sww_key){7,2+i},&scope,targets,2,&ticket)==NX_OK);m=receive(w);reply=sww_reply_get(&m);CHECK(reply&&!reply->original_error&&reply->result&&reply->history_error==faults[i]&&!reply->history);nx_sim_message_dispose(&m);}
    /* Stop with an in-flight history and a second reserved job, after GUI retirement. */
    gate_begin();CHECK(sww_submit(w,(sww_key){7,4},&scope,targets,2,&ticket)==NX_OK);gate_wait();uint64_t queued=0,excess=0;CHECK(sww_submit(w,(sww_key){7,5},&scope,targets,2,&queued)==NX_OK&&sww_submit(w,(sww_key){7,6},&scope,targets,2,&excess)==NX_ERROR_LIMIT&&!excess);
    sw_owner_destroy(&owner);swh_free(&batch);CHECK(sww_stop(w)==NX_OK);gate_release();CHECK(sww_join(w)==NX_OK);m=receive(w);reply=sww_reply_get(&m);CHECK(m.ticket==ticket&&reply&&reply->key.request==4);nx_sim_message_dispose(&m);m=receive(w);reply=sww_reply_get(&m);CHECK(m.ticket==queued&&reply&&reply->original_error==-ECANCELED&&!reply->history);nx_sim_message_dispose(&m);CHECK(sww_destroy(&w)==NX_OK&&!w);
    if(!strcmp(argv[4],"history")){
        /* Exercise the full declared profile bound. Original launch is kept as
           the sole available pair; other canonical profiles remain unavailable. */
        struct ae_execution_profile many[SWH_MAX_PROFILES];for(size_t i=0;i<SWH_MAX_PROFILES;++i){many[i]=original;if(i){many[i].launch[14]=1;many[i].launch[15]=(uint8_t)i;request_hash(&many[i]);}}
        metadata.profiles=many;metadata.profile_count=SWH_MAX_PROFILES;CHECK(sww_start_with_metadata(argv[1],&identity,&metadata,&w,&scope.context,&error)==NX_OK);atomic_store(&long_strings,1);CHECK(sww_submit(w,(sww_key){8,1},&scope,targets,2,&ticket)==NX_OK);m=receive(w);reply=sww_reply_get(&m);CHECK(reply&&!reply->history_error&&reply->history);CHECK(sw_owner_create(&scope,targets,2,&owner)==NX_OK&&sw_owner_expect(owner,ticket)==NX_OK&&sw_owner_complete(owner,ticket,0,&reply->result)==NX_OK&&sww_reply_take_history(reply,&batch)==NX_OK);nx_sim_message_dispose(&m);CHECK(sww_destroy(&w)==NX_OK);
        sw_snapshot *s=NULL;const swh_record *records=NULL;size_t count=0;CHECK(sw_owner_snapshot(owner,&s)==NX_OK&&swh_get(batch,s,id(10),&records,&count)==NX_OK&&count==SWH_MAX_PROFILES);
        swg_view *v=NULL;CHECK(swg_create(owner,fonts,(swg_style){1,32,2,NX_THEME_HIGH_CONTRAST},&v)==NX_OK&&swg_set_metadata(v,NULL,NX_OK,batch,0)==NX_OK);frame(v,360,300);press(v,SWG_ROWS,NX_ACTION_SELECT,360,300);press(v,SWG_HISTORY_BUTTON,NX_ACTION_PRESS,360,300);key(v,NX_KEY_END,360,300);CHECK(nx_focus_id(swg_context(v))==SWG_HISTORY_ROWS+63);
        size_t at=0;while(at<count&&memcmp(records[at].profile.launch,original.launch,16))++at;CHECK(at<count);press(v,SWG_HISTORY_ROWS+at,NX_ACTION_SELECT,360,300);details(v,&records[at],0,(swg_style){1,32,2,NX_THEME_HIGH_CONTRAST},360,300);CHECK(swg_close(&v)==NX_OK);sw_snapshot_release(&s);sw_owner_destroy(&owner);swh_free(&batch);
    }
    nx_font_collection_destroy(fonts);nx_font_close(font);free(font_bytes);CHECK(fds()==before);
    puts("HISTORY_WORKER_COPY_OWNED_TRANSFER_2SLOTS_ERRORS_RETIRE_STOP_VIEW_FULL_IDS_SCROLL_UNKNOWN_CURRENT_OK");return 0;
}
