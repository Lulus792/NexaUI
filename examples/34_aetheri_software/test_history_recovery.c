#include "software_worker.h"
#include <dirent.h>
#include <errno.h>
#include <poll.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"software recovery %d: %s (fault=%d)\n",__LINE__,#x,fault);exit(1);}}while(0)
static int fault,worker_phase;static unsigned reads;static pthread_t gui;static struct ae_execution_profile *caller_profile;
int __real_ae_catalogue_recovery_inventory(struct ae_catalogue *,size_t,struct ae_recovery_inventory **);
int __wrap_ae_catalogue_recovery_inventory(struct ae_catalogue *c,size_t limit,struct ae_recovery_inventory **out){
    if(worker_phase)CHECK(!pthread_equal(gui,pthread_self()));
    ++reads;if(fault==1)return -EIO;if(fault==2)return -EPROTONOSUPPORT;if(fault==9)return -ENOMEM;
    int error=__real_ae_catalogue_recovery_inventory(c,limit,out);if(error)return error;
    struct ae_recovery_inventory *v=*out;
    if(fault==3)v->context.runtime_incarnation[15]^=1;
    else if(fault==4)++v->by_kind[AE_RECOVERY_OPERATION];
    else if(fault==5&&v->count){v->records[v->count-1].reason[0]=(char)0xff;v->records[v->count-1].reason[1]=0;}
    else if(fault==6){
        for(size_t i=0;i<v->count;++i)if(v->records[i].kind==AE_RECOVERY_OPERATION){v->records[i].related[15]^=1;break;}
    }else if(fault==7&&v->count)v->records[v->count-1].kind=AE_RECOVERY_KIND_COUNT;
    else if(fault==8&&v->count>1){struct ae_recovery_record a=v->records[0];v->records[0]=v->records[v->count-1];v->records[v->count-1]=a;}
    else if(fault==10){CHECK(caller_profile);memset(caller_profile,0,sizeof *caller_profile);}
    else if(fault==11)++v->context.revision;
    return 0;
}
static unsigned descriptors(void){DIR *d=opendir("/proc/self/fd");CHECK(d);unsigned n=0;struct dirent *e;while((e=readdir(d)))if(e->d_name[0]!='.')++n;CHECK(!closedir(d));return n;}
static sw_id id(unsigned n){sw_id v={{0}};v.bytes[6]=0x40;v.bytes[8]=0x80;v.bytes[15]=(uint8_t)n;return v;}
static void request_hash(struct ae_execution_profile *p){
    struct ae_launch_request q={.install_revision=p->install_revision};
    const uint8_t *src[]={p->system_epoch,p->lease,p->launch,p->broker_epoch,p->broker_runtime,p->account,p->install,p->generation,p->data,p->boot};
    uint8_t *dst[]={q.system_epoch,q.lease,q.launch,q.broker_epoch,q.broker_runtime,q.account,q.install,q.generation,q.data,q.boot};
    for(size_t i=0;i<10;++i)memcpy(dst[i],src[i],16);
    strcpy(q.action,"run");strcpy(q.session,"p0");strcpy(q.seat,"none");CHECK(!ae_launch_fingerprint(&q,p->request_digest));
}
static const swh_record *one(const swh_batch *b,const sw_snapshot *snapshot){const swh_record *r=NULL;size_t n=0;CHECK(swh_get(b,snapshot,id(10),&r,&n)==NX_OK&&n==1);return r;}
static void unavailable(const swh_record *r,int error){struct ae_recovery_record zero={0};CHECK(r->recovery_error==error&&!r->recovery_schema);for(unsigned j=0;j<SWH_RECOVERY_LINKS;++j)CHECK(r->recovery[j].error==error&&!memcmp(&r->recovery[j].value,&zero,sizeof zero));}
static unsigned matches(const swh_record *r,const struct ae_recovery_inventory *v){
    const enum ae_recovery_record_kind kinds[SWH_RECOVERY_LINKS]={AE_RECOVERY_OPERATION,AE_RECOVERY_CODE_LEASE,AE_RECOVERY_DISPATCH,AE_RECOVERY_CLOSURE,AE_RECOVERY_HANDOFF};
    const uint8_t *ids[SWH_RECOVERY_LINKS]={r->profile.root_operation,r->profile.lease,r->profile.launch,r->profile.launch,r->profile.launch};unsigned found=0;struct ae_recovery_record zero={0};
    CHECK(!r->recovery_error&&r->recovery_schema==v->schema_version);
    for(unsigned j=0;j<SWH_RECOVERY_LINKS;++j){
        const struct ae_recovery_record *expected=NULL;
        for(size_t i=0;i<v->count;++i)if(v->records[i].kind==kinds[j]&&!memcmp(v->records[i].id,ids[j],16)){CHECK(!expected);expected=&v->records[i];}
        if(expected){CHECK(!r->recovery[j].error&&!memcmp(&r->recovery[j].value,expected,sizeof *expected));++found;}
        else CHECK(r->recovery[j].error==-ENOENT&&!memcmp(&r->recovery[j].value,&zero,sizeof zero));
    }
    return found;
}
static nx_sim_message receive(sww_worker *w){nx_sim_message m={0};for(unsigned i=0;i<100&&!m.ticket;++i){CHECK(sww_receive(w,&m)==NX_OK);if(!m.ticket){struct pollfd fd={sww_wake_fd(w),POLLIN,0};CHECK(poll(&fd,1,100)>=0);}}CHECK(m.ticket);return m;}
int main(int argc,char **argv){
    CHECK(argc==4);gui=pthread_self();unsigned before=descriptors();uint8_t bytes[AE_EXECUTION_BYTES];struct ae_execution_profile p;
    FILE *f=fopen(argv[2],"rb");CHECK(f&&fread(bytes,1,sizeof bytes,f)==sizeof bytes&&fgetc(f)==EOF&&!fclose(f)&&!ae_execution_decode(bytes,sizeof bytes,&p));
    struct ae_catalogue_identity identity={.kind=AE_CATALOGUE_SYSTEM};memcpy(identity.authority_epoch,p.system_epoch,16);struct ae_catalogue *c=NULL;CHECK(!ae_catalogue_open(argv[1],&identity,&c));
    sw_scope scope={.identity=identity,.os_uid=1000};memcpy(scope.account,p.account,16);CHECK(!ae_catalogue_get_info(c,&scope.context));sw_id targets[]={id(10),id(99)};sw_result *apps=NULL;CHECK(!sw_read(c,&scope,targets,2,&apps));
    sw_owner *owner=NULL;CHECK(sw_owner_create(&scope,targets,2,&owner)==NX_OK&&sw_owner_expect(owner,1)==NX_OK&&sw_owner_complete(owner,1,0,&apps)==NX_OK&&!apps);sw_snapshot *snapshot=NULL;CHECK(sw_owner_snapshot(owner,&snapshot)==NX_OK);const sw_result *observed=sw_snapshot_data(snapshot);swh_batch *batch=NULL;
    if(!strcmp(argv[3],"corrupt")){CHECK(swh_read(c,observed,&p,1,&batch)==-EUCLEAN&&!batch&&!reads);goto done;}
    CHECK(!swh_read(c,observed,&p,1,&batch));const swh_record *r=one(batch,snapshot);unsigned expected_reads=r->reservation_error?0U:1U;CHECK(reads==expected_reads);
    struct ae_recovery_inventory *reference=NULL;unsigned present=0;
    if(!r->reservation_error){
        CHECK(!__real_ae_catalogue_recovery_inventory(c,AE_RECOVERY_MAX_RECORDS,&reference));present=matches(r,reference);
        for(fault=1;fault<=9;++fault){
            if((fault==5||fault==7)&&!reference->count)continue;
            if(fault==8&&reference->count<2)continue;
            if(fault==6&&r->recovery[SWH_RECOVERY_OPERATION].error)continue;
            swh_batch *none=NULL;int error=swh_read(c,observed,&p,1,&none);
            if(fault==2){CHECK(!error&&none);unavailable(one(none,snapshot),-EPROTONOSUPPORT);swh_free(&none);}
            else{int wanted=fault==1?-EIO:fault==3?-ESTALE:fault==9?-ENOMEM:-EBADMSG;CHECK(error==wanted&&!none);}
        }
        fault=11;swh_batch *stale=NULL;CHECK(swh_read(c,observed,&p,1,&stale)==-ESTALE&&!stale);
        fault=10;struct ae_execution_profile changed=p;caller_profile=&changed;swh_batch *copied=NULL;CHECK(!swh_read(c,observed,&changed,1,&copied)&&memcmp(&changed,&p,sizeof p)&&!memcmp(&one(copied,snapshot)->profile,&p,sizeof p));matches(one(copied,snapshot),reference);swh_free(&copied);caller_profile=NULL;fault=0;
    }else unavailable(r,r->reservation_error);
    fault=0;unsigned prior=reads;struct ae_execution_profile missing=p;memcpy(missing.install,id(99).bytes,16);request_hash(&missing);swh_batch *none=NULL;CHECK(!swh_read(c,observed,&missing,1,&none)&&reads==prior);swh_free(&none);
    CHECK(!swh_read(c,observed,NULL,0,&none)&&reads==prior);swh_free(&none);
    if(!r->reservation_error){
        if(!r->unit_error){struct ae_execution_profile conflict=p;conflict.launch[0]^=0x71;request_hash(&conflict);CHECK(swh_read(c,observed,&conflict,1,&none)==-EEXIST&&!none&&reads==prior);}
        struct ae_execution_profile many[SWH_MAX_PROFILES];many[0]=p;
        for(unsigned i=1;i<SWH_MAX_PROFILES;++i){many[i]=p;memcpy(many[i].launch,id(i).bytes,16);many[i].launch[0]=0x71;memcpy(many[i].lease,id(i).bytes,16);many[i].lease[0]=0x72;memcpy(many[i].root_operation,id(i).bytes,16);many[i].root_operation[0]=0x73;request_hash(&many[i]);}
        int maximum_error=swh_read(c,observed,many,SWH_MAX_PROFILES,&none);if(maximum_error)fprintf(stderr,"maximum original errno=%d\n",maximum_error);
        CHECK(!maximum_error&&reads==prior+1);const swh_record *range=NULL;size_t count=0;CHECK(swh_get(none,snapshot,id(10),&range,&count)==NX_OK&&count==SWH_MAX_PROFILES);unsigned bound=0;
        for(size_t i=0;i<count;++i){if(!range[i].reservation_error){++bound;matches(&range[i],reference);}else unavailable(&range[i],range[i].reservation_error);}CHECK(bound==1);swh_free(&none);
    }
    CHECK(!ae_catalogue_close(&c));sww_worker *worker=NULL;sww_metadata config={NULL,0,&p,1};int original_error=0;struct ae_catalogue_info initial;worker_phase=1;prior=reads;
    CHECK(sww_start_with_metadata(argv[1],&identity,&config,&worker,&initial,&original_error)==NX_OK&&!original_error&&reads==prior);uint64_t ticket=0;CHECK(sww_submit(worker,(sww_key){1,1},&scope,targets,2,&ticket)==NX_OK);nx_sim_message m=receive(worker);sww_reply *reply=sww_reply_get(&m);CHECK(reply&&!reply->original_error&&!reply->history_error&&reply->history&&reads==prior+expected_reads);swh_batch *owned=NULL,*twice=NULL;CHECK(sww_reply_take_history(reply,&owned)==NX_OK&&owned&&sww_reply_take_history(reply,&twice)==NX_ERROR_STATE&&!twice);nx_sim_message_dispose(&m);
    if(expected_reads){fault=1;CHECK(sww_submit(worker,(sww_key){1,2},&scope,targets,2,&ticket)==NX_OK);m=receive(worker);reply=sww_reply_get(&m);CHECK(reply&&!reply->original_error&&reply->result&&reply->history_error==-EIO&&!reply->history);nx_sim_message_dispose(&m);fault=0;}
    CHECK(sww_destroy(&worker)==NX_OK);worker_phase=0;if(reference)CHECK(matches(one(owned,snapshot),reference)==present);else unavailable(one(owned,snapshot),r->reservation_error);swh_free(&owned);ae_recovery_inventory_free(&reference);
done:
    if(c)CHECK(!ae_catalogue_close(&c));
    sw_owner_destroy(&owner);sw_snapshot_release(&snapshot);swh_free(&batch);CHECK(descriptors()==before);
    puts("ORIGINAL_RECOVERY_RELATIONS_FULL_KEYS_SELECTIVE_MISSING_SCOPE_WORKER_OWNED_READONLY_OK");return 0;
}
