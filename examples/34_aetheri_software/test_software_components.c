#define _GNU_SOURCE
#include "software_worker.h"
#include "software_view.h"
#include <dirent.h>
#include <errno.h>
#include <math.h>
#include <poll.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"software components %d: %s\n",__LINE__,#x);exit(1);}}while(0)
static long fail_after=-1;
static pthread_t main_thread;
static atomic_int worker_phase,manifest_reads;
void *__real_malloc(size_t);
void *__real_calloc(size_t,size_t);
static int fail(void){if(fail_after==0){fail_after=-1;return 1;}if(fail_after>0)--fail_after;return 0;}
void *__wrap_malloc(size_t n){return fail()?NULL:__real_malloc(n);}
void *__wrap_calloc(size_t n,size_t s){return fail()?NULL:__real_calloc(n,s);}
nx_result __real_amh_document_read(const char *,const unsigned char[32],amh_document **);
nx_result __wrap_amh_document_read(const char *p,const unsigned char expected[32],amh_document **out){if(atomic_load(&worker_phase)){CHECK(!pthread_equal(main_thread,pthread_self()));atomic_fetch_add(&manifest_reads,1);}return __real_amh_document_read(p,expected,out);}
static sw_id id(uint32_t n){sw_id v={{0}};v.bytes[6]=0x40;v.bytes[8]=0x80;for(unsigned i=0;i<4;++i)v.bytes[15-i]=(uint8_t)(n>>(8*i));return v;}
static void filename(char out[4096],const char *dir,const char *name){int n=snprintf(out,4096,"%s/%s",dir,name);CHECK(n>0&&n<4096);}
static unsigned fds(void){DIR *d=opendir("/proc/self/fd");CHECK(d);unsigned n=0;struct dirent *e;while((e=readdir(d)))if(e->d_name[0]!='.')++n;CHECK(!closedir(d));return n;}
static unsigned char *read_file(const char *p,size_t *size){FILE *f=fopen(p,"rb");CHECK(f&&!fseek(f,0,SEEK_END));long n=ftell(f);CHECK(n>0&&!fseek(f,0,SEEK_SET));unsigned char *bytes=malloc((size_t)n+1);CHECK(bytes&&fread(bytes,1,(size_t)n,f)==(size_t)n&&!fclose(f));bytes[n]=0;*size=(size_t)n;return bytes;}
static void digest(const char *dir,const char *name,unsigned char out[32]){char path[4096];filename(path,dir,name);size_t n;unsigned char *p=read_file(path,&n);CHECK(n==64);for(size_t i=0;i<32;++i){unsigned v;CHECK(sscanf((char *)p+i*2,"%2x",&v)==1);out[i]=(unsigned char)v;}free(p);}
static sw_result *copy_result(const sw_result *s){sw_result *r=calloc(1,sizeof(*r));CHECK(r);*r=*s;r->rows=calloc(s->count,sizeof(*r->rows));CHECK(r->rows);memcpy(r->rows,s->rows,s->count*sizeof(*r->rows));return r;}
static sw_owner *owner_for(const sw_result *r){sw_id targets[5];CHECK(r->count==5);for(size_t i=0;i<5;++i)targets[i]=r->rows[i].install;sw_owner *o=NULL;sw_result *copy=copy_result(r);CHECK(sw_owner_create(&r->scope,targets,5,&o)==NX_OK&&sw_owner_expect(o,1)==NX_OK&&sw_owner_complete(o,1,0,&copy)==NX_OK&&!copy);return o;}
static nx_sim_message receive(sww_worker *w){nx_sim_message m={0};for(unsigned i=0;i<100;++i){CHECK(sww_receive(w,&m)==NX_OK);if(m.ticket)return m;struct pollfd p={sww_wake_fd(w),POLLIN,0};CHECK(poll(&p,1,100)>=0);}CHECK(0);return m;}
static void document_checks(const char *dir,const sw_result *base){
    char path[4096];filename(path,dir,"maximum.bin");size_t size;unsigned char *bytes=read_file(path,&size),expected[32];digest(dir,"maximum.sha256",expected);
    amh_document *d=NULL;CHECK(amh_document_project(bytes,size,expected,&d)==NX_OK&&d);memset(bytes,0,size);free(bytes);
    const struct ae_fixture *data=amh_document_data(d);CHECK(data&&data->file_count==32&&data->action_count==8&&data->dependency_count==8&&data->files[31].path.size==256&&data->actions[7].argument_count==8&&data->actions[7].arguments[7].size==256);
    CHECK(!memcmp(data->component.data,"aetheri.probe",13)&&sw_component_identity_matches(&base->rows[0],d));
    amh_document *same=d;CHECK(amh_document_read(path,expected,&d)==NX_ERROR_ARGUMENT&&d==same);amh_document_free(&d);CHECK(!d);
    bytes=read_file(path,&size);
    for(long n=0;n<2;++n){fail_after=n;CHECK(amh_document_project(bytes,size,expected,&d)==NX_ERROR_MEMORY&&!d);}free(bytes);
    unsigned before=fds();const char *bad[]={"link","fifo","absent","."};
    for(unsigned repeat=0;repeat<8;++repeat){for(size_t i=0;i<4;++i){filename(path,dir,bad[i]);CHECK(amh_document_read(path,expected,&d)==NX_ERROR_PLATFORM&&!d);}}
    CHECK(fds()==before);
    filename(path,dir,"maximum.bin");expected[0]^=1;CHECK(amh_document_read(path,expected,&d)==NX_ERROR_ARGUMENT&&!d);expected[0]^=1;
    const char *variants[]={"component","publisher","empty"};
    for(size_t i=0;i<3;++i){char name[64];snprintf(name,sizeof name,"%s.bin",variants[i]);filename(path,dir,name);snprintf(name,sizeof name,"%s.sha256",variants[i]);digest(dir,name,expected);
        CHECK(amh_document_read(path,expected,&d)==NX_OK);sw_row row=base->rows[0];memcpy(row.value.artifact,expected,32);
        CHECK(sw_component_identity_matches(&row,d)==(i==2));if(i==2)CHECK(amh_document_data(d)->dependency_count==0);amh_document_free(&d);
    }
    puts("OWNED_ORIGINAL_MANIFEST_MAXIMUM_EMPTY_HASH_COMPONENT_PUBLISHER_LIFETIME_OOM_FILE_GUARDS_OK");
}
static int semantic(swg_view *v,uint64_t key,nx_semantic_view *out){uint32_t at;return nx_semantics_find(swg_context(v),key,&at)==NX_OK&&nx_semantics_get(swg_context(v),at,out)==NX_OK;}
static int contains(nx_utf8_span text,const char *s){size_t n=strlen(s);for(size_t i=0;i+n<=text.length;++i)if(!memcmp(text.data+i,s,n))return 1;return 0;}
static void frame(swg_view *v,float w,float h){swg_intent intent;CHECK(swg_frame(v,w,h,&intent)==NX_OK&&!intent.close&&!intent.refresh);}
static void key(swg_view *v,nx_key key_value,float w,float h){nx_event e={0};e.type=NX_EVENT_KEY_DOWN;e.key=key_value;CHECK(swg_queue_event(v,&e)==NX_OK);frame(v,w,h);}
static void capture(swg_view *v,float w,float h,swg_style style,const char *name){
    const char *dir=getenv("SWG_CAPTURE_DIR");if(!dir)return;uint32_t width=(uint32_t)ceilf(w*style.raster_scale),height=(uint32_t)ceilf(h*style.raster_scale);size_t stride=(size_t)width*4;unsigned char *pixels=malloc(stride*height);CHECK(pixels);
    const nx_command *commands;size_t count;nx_color background;CHECK(nx_frame_commands(swg_context(v),&commands,&count)==NX_OK&&nx_theme_color(swg_context(v),NX_COLOR_BACKGROUND,&background)==NX_OK&&nx_headless_render_scaled(commands,count,pixels,width,height,stride,background,style.raster_scale)==NX_OK);
    char namebuf[64],path[4096];snprintf(namebuf,sizeof namebuf,"%g-%s.ppm",(double)w,name);filename(path,dir,namebuf);FILE *f=fopen(path,"wb");CHECK(f&&fprintf(f,"P6\n%u %u\n255\n",width,height)>0);for(uint32_t y=0;y<height;++y)for(uint32_t x=0;x<width;++x)CHECK(fwrite(pixels+(size_t)y*stride+(size_t)x*4,1,3,f)==3);CHECK(!fclose(f));free(pixels);
}
static void view_checks(sw_owner *owner,const sw_components *batch,nx_font_collection *fonts,const char *last,swg_style style,float w,float h){
    swg_view *v=NULL;CHECK(swg_create(owner,fonts,style,&v)==NX_OK&&swg_set_components(v,batch,NX_OK)==NX_OK);frame(v,w,h);
    key(v,NX_KEY_HOME,w,h);CHECK(nx_semantics_request_action(swg_context(v),SWG_ROWS,NX_ACTION_SELECT)==NX_OK);frame(v,w,h);nx_semantic_view node,area;
    CHECK(semantic(v,SWG_DETAIL_TEXT,&node)&&contains(node.value,last)&&contains(node.value,style.german?"Deklarierte Abhängigkeiten: 8":"Declared dependencies: 8")&&contains(node.value,style.german?"Besitzstatus: unbekannt":"ownership state: unknown"));
    capture(v,w,h,style,"dependencies-top");CHECK(semantic(v,SWG_SCROLL,&area));size_t pages=0;
    while(node.bounds.y+node.bounds.height>area.bounds.y+area.bounds.height+.1f){CHECK(++pages<256);key(v,NX_KEY_PAGE_DOWN,w,h);CHECK(semantic(v,SWG_DETAIL_TEXT,&node)&&semantic(v,SWG_SCROLL,&area)&&nx_focus_id(swg_context(v))==SWG_BACK);}
    capture(v,w,h,style,"dependencies-bottom");
    CHECK(swg_set_components(v,NULL,NX_ERROR_MEMORY)==NX_OK);frame(v,w,h);CHECK(semantic(v,SWG_DETAIL_TEXT,&node)&&contains(node.value,style.german?"Manifest nicht verfügbar":"Manifest unavailable")&&!contains(node.value,last));
    CHECK(swg_set_components(v,NULL,NX_OK)==NX_OK);frame(v,w,h);CHECK(semantic(v,SWG_DETAIL_TEXT,&node)&&contains(node.value,style.german?"Kein passendes Manifest":"No matching manifest"));
    CHECK(swg_close(&v)==NX_OK&&!v);
}
int main(int argc,char **argv){
    CHECK(argc==4);main_thread=pthread_self();unsigned before=fds();struct ae_catalogue_identity identity={.kind=AE_CATALOGUE_SYSTEM};memcpy(identity.authority_epoch,id(1).bytes,16);
    struct ae_catalogue *catalogue=NULL;CHECK(ae_catalogue_open(argv[1],&identity,&catalogue)==0);sw_scope scope={.identity=identity,.os_uid=1000};memcpy(scope.account,id(2).bytes,16);CHECK(ae_catalogue_get_info(catalogue,&scope.context)==0);
    sw_id targets[]={id(12),id(99),id(10),id(13),id(11)};sw_result *base=NULL;CHECK(sw_read(catalogue,&scope,targets,5,&base)==0&&ae_catalogue_close(&catalogue)==0);document_checks(argv[2],base);
    char correct[4096],wrong[4096],fifo[4096];filename(correct,argv[2],"maximum.bin");filename(wrong,argv[2],"malformed.bin");filename(fifo,argv[2],"fifo");
    sw_manifest_source sources[]={{id(10),correct},{id(12),wrong},{id(99),fifo},{id(77),fifo}};sww_worker *worker=NULL;struct ae_catalogue_info initial;int error=0;
    CHECK(sww_start_with_manifests(argv[1],&identity,sources,4,&worker,&initial,&error)==NX_OK&&!error);strcpy(correct,"/no-longer-selected");sources[0].install=id(99);
    atomic_store(&worker_phase,1);uint64_t ticket=0;CHECK(sww_submit(worker,(sww_key){1,1},&scope,targets,5,&ticket)==NX_OK);nx_sim_message message=receive(worker);sww_reply *reply=sww_reply_get(&message);CHECK(reply&&reply->components_status==NX_OK&&reply->components&&atomic_load(&manifest_reads)==2);
    sw_owner *owner=NULL;CHECK(sw_owner_create(&scope,targets,5,&owner)==NX_OK&&sw_owner_expect(owner,ticket)==NX_OK&&sw_owner_complete(owner,ticket,0,&reply->result)==NX_OK&&!reply->result);
    sw_components *batch=reply->components;reply->components=NULL;nx_sim_message_dispose(&message);CHECK(sww_destroy(&worker)==NX_OK&&!worker);atomic_store(&worker_phase,0);
    sw_snapshot *snapshot=NULL;CHECK(sw_owner_snapshot(owner,&snapshot)==NX_OK);const amh_document *document=NULL;CHECK(sw_components_get(batch,snapshot,id(10),&document)==NX_OK&&document&&amh_document_data(document)->dependency_count==8);document=NULL;
    CHECK(sw_components_get(batch,snapshot,id(12),&document)==NX_ERROR_ARGUMENT&&!document&&sw_components_get(batch,snapshot,id(99),&document)==NX_ERROR_STATE&&!document);
    CHECK(sw_components_get(batch,snapshot,id(11),&document)==NX_OK&&!document);
    sw_result *changed=copy_result(base);++changed->scope.os_uid;sw_owner *other=owner_for(changed);sw_result_free(&changed);sw_snapshot *different=NULL;CHECK(sw_owner_snapshot(other,&different)==NX_OK&&sw_components_get(batch,different,id(10),&document)==NX_ERROR_STATE&&!document);sw_snapshot_release(&different);sw_owner_destroy(&other);
    sw_scope same_scope=scope;same_scope.context.revision+=100;sw_row a=base->rows[0],b=a;a.value.catalogue_revision=b.value.catalogue_revision=same_scope.context.revision;++b.value.gate_revision;CHECK(ae_install_snapshot_valid(&b.value)==0&&!sw_observation_same(&same_scope,&a,&same_scope,&b));
    changed=copy_result(base);++changed->scope.context.revision;for(size_t i=0;i<changed->count;++i)if(!changed->rows[i].error)++changed->rows[i].value.catalogue_revision;other=owner_for(changed);sw_result_free(&changed);CHECK(sw_owner_snapshot(other,&different)==NX_OK&&sw_components_get(batch,different,id(10),&document)==NX_ERROR_STATE&&!document);sw_snapshot_release(&different);sw_owner_destroy(&other);
    filename(correct,argv[2],"maximum.bin");sw_manifest_source duplicates[]={{id(10),correct},{id(10),correct}};sw_components *none=NULL;CHECK(sw_components_read(base,duplicates,2,&none)==NX_ERROR_ARGUMENT&&!none);fail_after=0;CHECK(sw_components_read(base,duplicates,1,&none)==NX_ERROR_MEMORY&&!none);
    size_t bytes_count;unsigned char *font_bytes=read_file(argv[3],&bytes_count);nx_font *font=NULL;nx_font_collection *fonts=NULL;CHECK(nx_font_open(font_bytes,bytes_count,NULL,&font)==NX_OK&&nx_font_collection_create(NULL,&fonts)==NX_OK&&nx_font_collection_add(fonts,font)==NX_OK);
    char dep_path[4096];filename(dep_path,argv[2],"dependencies.txt");size_t dep_size;unsigned char *deps=read_file(dep_path,&dep_size);CHECK(dep_size==8*65);deps[dep_size-1]=0;const char *last=(char *)deps+7*65;
    view_checks(owner,batch,fonts,last,(swg_style){0,16,1,NX_THEME_DARK},800,600);view_checks(owner,batch,fonts,last,(swg_style){1,32,2,NX_THEME_HIGH_CONTRAST},360,300);
    free(deps);nx_font_collection_destroy(fonts);nx_font_close(font);free(font_bytes);sw_snapshot_release(&snapshot);sw_owner_destroy(&owner);sw_components_free(&batch);sw_result_free(&base);CHECK(fds()==before);
    puts("SOFTWARE_COMPONENTS_ORIGINAL_IDENTITIES_OWNED_DOCUMENTS_FULL_DECLARED_DEPENDENCIES_WORKER_VIEW_OK");return 0;
}
