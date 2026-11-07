#include "software_history.h"
#include <errno.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"software history %d: %s\n",__LINE__,#x);exit(1);}}while(0)
static unsigned infos,reads;static int fault,mutate;static long fail_after=-1;
static sw_result *caller_apps;static struct ae_execution_profile *caller_profiles;
void *__real_calloc(size_t,size_t);
void *__wrap_calloc(size_t n,size_t size){if(fail_after==0){fail_after=-1;return NULL;}if(fail_after>0)--fail_after;return __real_calloc(n,size);}
int __real_ae_catalogue_get_info(struct ae_catalogue *,struct ae_catalogue_info *);
int __wrap_ae_catalogue_get_info(struct ae_catalogue *c,struct ae_catalogue_info *v){
    ++infos;int r=__real_ae_catalogue_get_info(c,v);
    if(!r&&((fault==1&&infos==1)||(fault==2&&infos==2)))++v->revision;
    if(!r&&mutate&&infos==1){caller_apps->scope.os_uid=4000;caller_apps->rows[0].value.gate_revision+=100;caller_profiles[0].install[15]^=1;}
    return r;
}
int __real_ae_catalogue_get_execution_process(struct ae_catalogue *,const struct ae_execution_profile *,enum ae_execution_process_role,struct ae_execution_process_snapshot *);
int __wrap_ae_catalogue_get_execution_process(struct ae_catalogue *c,const struct ae_execution_profile *p,enum ae_execution_process_role role,struct ae_execution_process_snapshot *out){
    ++reads;if(reads==2){if(fault==3)return -EIO;if(fault==9)return -ENOMEM;if(fault==10)return -EUCLEAN;if(fault==11)return -ESTALE;}
    int r=__real_ae_catalogue_get_execution_process(c,p,role,out);
    if(!r){if(fault==4)out->process.profile_digest[0]^=1;else if(fault==5)out->process.role=role==AE_EXECUTION_STARTER?AE_EXECUTION_APP:AE_EXECUTION_STARTER;
        else if(fault==6)out->process.uid=4000;else if(fault==7)out->recorded_unix_ms=-1;else if(fault==8)out->process.pid=0;}
    return r;
}
static void reset(void){infos=reads=0;fault=mutate=0;fail_after=-1;}
static unsigned descriptors(void){DIR *d=opendir("/proc/self/fd");CHECK(d);unsigned n=0;struct dirent *e;while((e=readdir(d)))if(e->d_name[0]!='.')++n;CHECK(!closedir(d));return n;}
static sw_id id(unsigned n){sw_id v={{0}};v.bytes[6]=0x40;v.bytes[8]=0x80;v.bytes[15]=(uint8_t)n;return v;}
static void request_hash(struct ae_execution_profile *p){
    struct ae_launch_request q={.install_revision=p->install_revision};
    const uint8_t *src[]={p->system_epoch,p->lease,p->launch,p->broker_epoch,p->broker_runtime,p->account,p->install,p->generation,p->data,p->boot};
    uint8_t *dst[]={q.system_epoch,q.lease,q.launch,q.broker_epoch,q.broker_runtime,q.account,q.install,q.generation,q.data,q.boot};
    for(size_t i=0;i<10;++i)memcpy(dst[i],src[i],16);
    strcpy(q.action,"run");strcpy(q.session,"p0");strcpy(q.seat,"none");CHECK(!ae_launch_fingerprint(&q,p->request_digest));
}
static sw_result *copy(const sw_result *s){sw_result *r=calloc(1,sizeof(*r));CHECK(r);*r=*s;r->rows=calloc(s->count,sizeof(*r->rows));CHECK(r->rows);memcpy(r->rows,s->rows,s->count*sizeof(*r->rows));return r;}
static sw_owner *owner_for(const sw_result *r){
    sw_owner *o=NULL;sw_id targets[2];CHECK(r->count==2);for(size_t i=0;i<2;++i)targets[i]=r->rows[i].install;
    sw_result *c=copy(r);CHECK(sw_owner_create(&r->scope,targets,2,&o)==NX_OK&&sw_owner_expect(o,1)==NX_OK&&sw_owner_complete(o,1,0,&c)==NX_OK&&!c);return o;
}
static const swh_record *original(const swh_record *r,size_t n,const struct ae_execution_profile *p){for(size_t i=0;i<n;++i)if(!memcmp(r[i].profile.launch,p->launch,16))return &r[i];return NULL;}
int main(int argc,char **argv){
    CHECK(argc==4);unsigned before=descriptors();int unsupported=!strcmp(argv[3],"unsupported"),replacement=!strcmp(argv[3],"replacement");
    uint8_t raw[AE_EXECUTION_BYTES];FILE *f=fopen(argv[2],"rb");CHECK(f&&fread(raw,1,sizeof raw,f)==sizeof raw&&fgetc(f)==EOF&&!fclose(f));
    struct ae_execution_profile p;CHECK(!ae_execution_decode(raw,sizeof raw,&p));
    struct ae_catalogue_identity identity={.kind=AE_CATALOGUE_SYSTEM};memcpy(identity.authority_epoch,p.system_epoch,16);struct ae_catalogue *c=NULL;CHECK(!ae_catalogue_open(argv[1],&identity,&c));
    sw_scope scope={.identity=identity,.os_uid=1000};memcpy(scope.account,p.account,16);CHECK(!ae_catalogue_get_info(c,&scope.context));
    sw_id targets[]={id(10),id(99)};sw_result *apps=NULL;CHECK(!sw_read(c,&scope,targets,2,&apps)&&sw_result_valid(apps));
    CHECK(replacement==(memcmp(p.system_runtime,apps->scope.context.runtime_incarnation,16)!=0));
    sw_owner *owner=owner_for(apps);sw_snapshot *snapshot=NULL;CHECK(sw_owner_snapshot(owner,&snapshot)==NX_OK);
    struct ae_execution_profile profiles[4]={p,p,p,p};memcpy(profiles[1].launch,id(98).bytes,16);request_hash(&profiles[1]);
    memcpy(profiles[2].launch,id(97).bytes,16);memcpy(profiles[2].install,id(99).bytes,16);request_hash(&profiles[2]);
    memcpy(profiles[3].launch,id(96).bytes,16);memcpy(profiles[3].install,id(77).bytes,16);request_hash(&profiles[3]);
    swh_batch *batch=NULL;reset();
    if(!strcmp(argv[3],"corrupt")){
        CHECK(swh_read(c,apps,profiles,4,&batch)==-EUCLEAN&&!batch&&reads==2&&infos==1);
        CHECK(!ae_catalogue_close(&c));sw_owner_destroy(&owner);sw_snapshot_release(&snapshot);sw_result_free(&apps);CHECK(descriptors()==before);
        puts("HISTORICAL_PROCESS_ORIGINAL_READS_COPY_SCOPE_NO_CURRENT_STATE_OK mode=corrupt");return 0;
    }
    CHECK(!swh_read(c,apps,profiles,4,&batch)&&reads==4&&infos==2);
    const swh_record *range=NULL;size_t count=0;CHECK(swh_get(batch,snapshot,id(10),&range,&count)==NX_OK&&count==2);const swh_record *got=original(range,count,&p);CHECK(got);
    if(unsupported)CHECK(got->starter_error==-EPROTONOSUPPORT&&got->app_error==-EPROTONOSUPPORT);
    else CHECK(!got->starter_error&&!got->app_error&&got->starter.process.uid==0&&got->app.process.uid==1000&&got->app.process.pid==4242&&got->app.process.pidfd_id==UINT64_MAX&&got->app.process.cgroup_id==(UINT64_C(1)<<63)+31&&got->starter.recorded_unix_ms==124000&&got->app.recorded_unix_ms==124002);
    for(size_t i=0;i<count;++i)if(range[i].app_error){struct ae_execution_process_snapshot empty={0};CHECK(!memcmp(&range[i].app,&empty,sizeof empty)&&!memcmp(&range[i].starter,&empty,sizeof empty));}
    range=NULL;count=0;CHECK(swh_get(batch,snapshot,id(99),&range,&count)==NX_ERROR_STATE&&!range&&!count&&swh_get(NULL,snapshot,id(10),&range,&count)==NX_OK&&!range&&!count);
    swh_batch *none=NULL;reset();CHECK(!swh_read(c,apps,NULL,0,&none)&&!reads&&infos==2);swh_free(&none);
    for(int which=0;which<3;++which){sw_result *changed=copy(apps);if(which==0)++changed->scope.os_uid;else if(which==1){++changed->scope.context.revision;++changed->rows[0].value.catalogue_revision;}else ++changed->rows[0].value.gate_revision;
        sw_owner *other=owner_for(changed);sw_snapshot *fresh=NULL;CHECK(sw_owner_snapshot(other,&fresh)==NX_OK&&swh_get(batch,fresh,id(10),&range,&count)==NX_ERROR_STATE&&!range&&!count);sw_snapshot_release(&fresh);sw_owner_destroy(&other);sw_result_free(&changed);}
    for(int which=0;which<5;++which){struct ae_execution_profile bad[2]={p,p};if(which==0)bad[1].system_epoch[15]^=1;else if(which==1){bad[1].account[15]^=1;request_hash(&bad[1]);}else if(which==2){bad[1].boot[15]^=1;request_hash(&bad[1]);}else if(which==3)bad[1].request_digest[0]^=1;
        reset();CHECK(swh_read(c,apps,bad,2,&none)<0&&!none&&!reads&&!infos);}
    reset();CHECK(swh_read(c,apps,&p,SWH_MAX_PROFILES+1,&none)==-E2BIG&&!none&&!reads&&!infos);
    fail_after=0;CHECK(swh_read(c,apps,&p,1,&none)==-ENOMEM&&!none&&!reads&&!infos);
    for(int which=1;which<=(unsupported?2:11);++which){reset();fault=which;CHECK(swh_read(c,apps,&p,1,&none)<0&&!none);}
    /* Mutation after admission cannot alter the copied query or row anchor. */
    sw_scope saved_scope=apps->scope;sw_row saved_row=apps->rows[0];struct ae_execution_profile selected_profile=p;reset();mutate=1;caller_apps=apps;caller_profiles=&selected_profile;
    CHECK(!swh_read(c,apps,&selected_profile,1,&none));apps->scope=saved_scope;apps->rows[0]=saved_row;range=NULL;count=0;CHECK(swh_get(none,snapshot,id(10),&range,&count)==NX_OK&&count==1&&!memcmp(range[0].profile.install,p.install,16));swh_free(&none);reset();
    if(!unsupported){
        /* Synthetic newer generation selection: retained history stays old. */
        sw_result *newer=copy(apps);memcpy(newer->rows[0].value.generation,id(88).bytes,16);sw_owner *other=owner_for(newer);sw_snapshot *fresh=NULL;CHECK(sw_owner_snapshot(other,&fresh)==NX_OK&&!swh_read(c,newer,&p,1,&none));range=NULL;count=0;
        CHECK(swh_get(none,fresh,id(10),&range,&count)==NX_OK&&count==1&&!memcmp(range[0].profile.generation,p.generation,16)&&memcmp(range[0].profile.generation,range[0].observed.value.generation,16));swh_free(&none);sw_snapshot_release(&fresh);sw_owner_destroy(&other);sw_result_free(&newer);
    }
    struct ae_execution_profile maximum[SWH_MAX_PROFILES];
    for(unsigned i=0;i<SWH_MAX_PROFILES;++i){maximum[i]=p;if(i){memcpy(maximum[i].launch,id(100+i).bytes,16);request_hash(&maximum[i]);}}
    reset();CHECK(!swh_read(c,apps,maximum,SWH_MAX_PROFILES,&none)&&reads==2*SWH_MAX_PROFILES);range=NULL;count=0;
    CHECK(swh_get(none,snapshot,id(10),&range,&count)==NX_OK&&count==SWH_MAX_PROFILES);
    for(size_t i=1;i<count;++i)CHECK(memcmp(range[i-1].digest,range[i].digest,32)<0);
    swh_free(&none);maximum[SWH_MAX_PROFILES-1].request_digest[0]^=1;reset();CHECK(swh_read(c,apps,maximum,SWH_MAX_PROFILES,&none)<0&&!none&&!reads&&!infos);
    /* The batch owns all metadata after original handle/model disposal. */
    CHECK(!ae_catalogue_close(&c));sw_owner_destroy(&owner);sw_result_free(&apps);range=NULL;count=0;CHECK(swh_get(batch,snapshot,id(10),&range,&count)==NX_OK&&count==2);sw_snapshot_release(&snapshot);swh_free(&batch);CHECK(!batch);
    CHECK(descriptors()==before);printf("HISTORICAL_PROCESS_ORIGINAL_READS_COPY_SCOPE_NO_CURRENT_STATE_OK mode=%s\n",argv[3]);return 0;
}
