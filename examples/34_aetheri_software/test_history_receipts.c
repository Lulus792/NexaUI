#include "software_worker.h"
#include <dirent.h>
#include <errno.h>
#include <poll.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"software receipts %d: %s (fault=%d)\n",__LINE__,#x,fault);exit(1);}}while(0)
static int fault,worker_phase;static unsigned reservations,runs;static pthread_t gui;
int __real_ae_catalogue_get_receipt(struct ae_catalogue *,const uint8_t[16],const struct ae_launch_context *,struct ae_reservation_receipt *);
int __wrap_ae_catalogue_get_receipt(struct ae_catalogue *c,const uint8_t lease[16],const struct ae_launch_context *q,struct ae_reservation_receipt *v){
    if(worker_phase)CHECK(!pthread_equal(gui,pthread_self()));
    ++reservations;
    if(fault==19){memset(v,0xa5,sizeof *v);return -ENOENT;}
    if(fault==20)return -EIO;
    int r=__real_ae_catalogue_get_receipt(c,lease,q,v);if(r)return r;
    switch(fault){
    case 1:v->system_epoch[15]^=1;break;case 2:v->system_runtime[15]^=1;break;case 3:v->boot[15]^=1;break;
    case 4:v->lease[15]^=1;break;case 5:v->plan[6]=0;break;case 6:v->broker_runtime[6]=0;break;
    case 7:v->dispatch.operation[6]=0;break;case 8:v->dispatch.launch[6]=0;break;case 9:v->dispatch.revision=0;break;
    case 10:v->dispatch.install_gate_revision=0;break;case 11:v->dispatch.invocation_present=2;break;case 12:v->cause_present=2;break;
    case 13:v->withdrawn=2;break;case 14:memset(v->dispatch.state,'x',sizeof v->dispatch.state);break;
    case 15:memset(v->dispatch.unit,'x',sizeof v->dispatch.unit);break;case 16:v->dispatch.invocation_present=0;v->dispatch.invocation[0]=1;break;
    case 17:v->cause_present=0;v->cause[0]=1;break;case 18:v->withdrawn=1;v->cause_present=0;memset(v->cause,0,16);memset(v->cause_epoch,0,16);break;
    case 21:v->fingerprint[0]^=1;break;case 22:v->broker_runtime[15]^=1;break;case 23:v->dispatch.operation[15]^=1;break;
    case 24:strcpy(v->dispatch.state,"invented");break;case 25:v->dispatch.unit[12]^=1;break;
    }
    return 0;
}
int __real_ae_catalogue_get_run_receipt(struct ae_catalogue *,const struct ae_execution_profile *,const struct ae_execution_evidence *,enum ae_run_receipt_kind,struct ae_run_receipt *);
int __wrap_ae_catalogue_get_run_receipt(struct ae_catalogue *c,const struct ae_execution_profile *p,const struct ae_execution_evidence *e,enum ae_run_receipt_kind k,struct ae_run_receipt *v){
    if(worker_phase)CHECK(!pthread_equal(gui,pthread_self()));
    ++runs;
    if(fault==40){memset(v,0xa5,sizeof *v);return -ENOENT;}
    if(fault==41&&k==AE_RUN_ROOT_CLOSING)return -ENOENT;
    if(k==AE_RUN_ROOT_RELEASED){if(fault==37)return -EIO;if(fault==38)return -EUCLEAN;if(fault==39)return -ESTALE;}
    int r=__real_ae_catalogue_get_run_receipt(c,p,e,k,v);if(r)return r;
    if(k==AE_RUN_ROOT_RELEASED)switch(fault){
    case 31:v->kind=AE_RUN_USER_QUIET;break;case 32:v->profile_digest[0]^=1;break;case 33:v->invocation[0]^=1;break;
    case 34:v->revision=0;break;case 35:v->unix_ms=-1;break;case 36:v->previous_digest[0]^=1;break;
    }
    return 0;
}
static unsigned descriptors(void){DIR *d=opendir("/proc/self/fd");CHECK(d);unsigned count=0;struct dirent *e;while((e=readdir(d)))if(e->d_name[0]!='.')++count;CHECK(!closedir(d));return count;}
static sw_id id(unsigned n){sw_id v={{0}};v.bytes[6]=0x40;v.bytes[8]=0x80;v.bytes[15]=(uint8_t)n;return v;}
static const swh_record *record(const swh_batch *b,const sw_snapshot *snapshot){const swh_record *r=NULL;size_t n=0;CHECK(swh_get(b,snapshot,id(10),&r,&n)==NX_OK&&n==1);return r;}
static void empty_run(const swh_record *r,int error){struct ae_run_receipt zero={0};uint8_t digest[32]={0};for(unsigned i=0;i<SWH_RUN_RECEIPTS;++i)CHECK(r->run[i].error==error&&!memcmp(&r->run[i].value,&zero,sizeof zero)&&!memcmp(r->run[i].digest,digest,32));}
static nx_sim_message receive(sww_worker *worker){
    nx_sim_message m={0};for(unsigned i=0;i<100&&!m.ticket;++i){CHECK(sww_receive(worker,&m)==NX_OK);if(!m.ticket){struct pollfd fd={sww_wake_fd(worker),POLLIN,0};CHECK(poll(&fd,1,100)>=0);}}
    CHECK(m.ticket);return m;
}
int main(int argc,char **argv){
    CHECK(argc==4);gui=pthread_self();unsigned before=descriptors();struct ae_execution_profile p;uint8_t bytes[AE_EXECUTION_BYTES];FILE *f=fopen(argv[2],"rb");CHECK(f&&fread(bytes,1,sizeof bytes,f)==sizeof bytes&&fgetc(f)==EOF&&!fclose(f)&&!ae_execution_decode(bytes,sizeof bytes,&p));
    struct ae_catalogue_identity identity={.kind=AE_CATALOGUE_SYSTEM};memcpy(identity.authority_epoch,p.system_epoch,16);struct ae_catalogue *c=NULL;CHECK(!ae_catalogue_open(argv[1],&identity,&c));
    sw_scope scope={.identity=identity,.os_uid=1000};memcpy(scope.account,p.account,16);CHECK(!ae_catalogue_get_info(c,&scope.context));sw_id targets[]={id(10),id(99)};sw_result *apps=NULL;CHECK(!sw_read(c,&scope,targets,2,&apps));
    sw_owner *owner=NULL;CHECK(sw_owner_create(&scope,targets,2,&owner)==NX_OK&&sw_owner_expect(owner,1)==NX_OK);sw_result *working=apps;apps=NULL;CHECK(sw_owner_complete(owner,1,0,&working)==NX_OK&&!working);sw_snapshot *snapshot=NULL;CHECK(sw_owner_snapshot(owner,&snapshot)==NX_OK);const sw_result *observed=sw_snapshot_data(snapshot);
    swh_batch *batch=NULL;
    if(!strcmp(argv[3],"corrupt")){CHECK(swh_read(c,observed,&p,1,&batch)==-EUCLEAN&&!batch&&!reservations&&!runs);goto finish;}
    CHECK(!swh_read(c,observed,&p,1,&batch)&&reservations==1);const swh_record *r=record(batch,snapshot);
    if(!r->reservation_error){
        CHECK(!memcmp(r->reservation.system_runtime,scope.context.runtime_incarnation,16)&&!memcmp(r->reservation.fingerprint,p.request_digest,32)&&!memcmp(r->reservation.dispatch.operation,p.root_operation,16));
        for(fault=1;fault<=25;++fault){
            swh_batch *none=NULL;unsigned prior=runs;int error=swh_read(c,observed,&p,1,&none);
            if(fault==19||fault==21||fault==22||fault==23){CHECK(!error&&none&&runs==prior);const swh_record *missing=record(none,snapshot);int expected=fault==19?-ENOENT:-EXDEV;struct ae_reservation_receipt zero={0};CHECK(missing->reservation_error==expected&&!memcmp(&missing->reservation,&zero,sizeof zero));empty_run(missing,expected);swh_free(&none);}
            else CHECK(error<0&&!none&&runs==prior);
        }
        fault=0;
    }else{struct ae_reservation_receipt zero={0};CHECK(r->reservation_error==-ENOENT&&!memcmp(&r->reservation,&zero,sizeof zero));}
    if(!strcmp(argv[3],"history")){
        CHECK(!r->reservation_error&&r->reservation.dispatch.invocation_present&&!r->reservation.withdrawn);
        for(unsigned i=0;i<SWH_RUN_RECEIPTS;++i){uint8_t digest[32];CHECK(!r->run[i].error&&r->run[i].value.kind==(enum ae_run_receipt_kind)(i+1)&&!ae_run_receipt_digest(&r->run[i].value,digest)&&!memcmp(digest,r->run[i].digest,32)&&!memcmp(r->run[i].value.profile_digest,r->digest,32));if(i)CHECK(!memcmp(r->run[i].value.previous_digest,r->run[i-1].digest,32));}
        for(fault=31;fault<=41;++fault){swh_batch *none=NULL;int error=swh_read(c,observed,&p,1,&none);if(fault==40){CHECK(!error&&none);empty_run(record(none,snapshot),-ENOENT);swh_free(&none);}else CHECK(error<0&&!none);}
        fault=0;
    }else if(strstr(argv[3],"replacement")){CHECK(!r->reservation_error&&!memcmp(r->reservation.system_runtime,scope.context.runtime_incarnation,16)&&memcmp(p.system_runtime,r->reservation.system_runtime,16));empty_run(r,-ESTALE);}
    else if(!strcmp(argv[3],"withdrawn")){CHECK(!r->reservation_error&&r->reservation.withdrawn&&r->reservation.cause_present&&r->reservation.cause[15]==61&&!r->reservation.dispatch.invocation_present&&!strcmp(r->reservation.dispatch.state,"terminal"));empty_run(r,-ENODATA);}
    else{CHECK(r->starter_error);empty_run(r,r->reservation_error?r->reservation_error:-ENODATA);}
    /* Existing worker owns each original query. Copies stay valid after worker,
       message, catalogue and model disposal; history detaches exactly once. */
    CHECK(!ae_catalogue_close(&c));
    sww_worker *worker=NULL;sww_metadata config={NULL,0,&p,1};int error=0;worker_phase=1;CHECK(sww_start_with_metadata(argv[1],&identity,&config,&worker,&scope.context,&error)==NX_OK&&!error);unsigned queried=reservations;uint64_t ticket=0;CHECK(sww_submit(worker,(sww_key){1,1},&scope,targets,2,&ticket)==NX_OK);nx_sim_message m=receive(worker);
    sww_reply *reply=sww_reply_get(&m);CHECK(reply&&!reply->original_error&&!reply->history_error&&reply->history&&reservations==queried+1);swh_batch *owned=NULL,*twice=NULL;CHECK(sww_reply_take_history(reply,&owned)==NX_OK&&owned&&sww_reply_take_history(reply,&twice)==NX_ERROR_STATE&&!twice);nx_sim_message_dispose(&m);
    for(unsigned i=0;i<(!strcmp(argv[3],"history")?2U:1U);++i){
        fault=i?37:20;CHECK(sww_submit(worker,(sww_key){1,i+2},&scope,targets,2,&ticket)==NX_OK);m=receive(worker);reply=sww_reply_get(&m);
        CHECK(reply&&!reply->original_error&&reply->result&&reply->history_error==-EIO&&!reply->history);nx_sim_message_dispose(&m);
    }
    CHECK(sww_destroy(&worker)==NX_OK);fault=0;worker_phase=0;
    const swh_record *detached=record(owned,snapshot);CHECK(detached->reservation_error==r->reservation_error&&!memcmp(&detached->reservation,&r->reservation,sizeof r->reservation)&&!memcmp(detached->run,r->run,sizeof r->run));swh_free(&owned);
finish:
    if(c)CHECK(!ae_catalogue_close(&c));
    sw_owner_destroy(&owner);if(batch){r=record(batch,snapshot);CHECK(!memcmp(r->profile.launch,p.launch,16));}sw_snapshot_release(&snapshot);swh_free(&batch);CHECK(descriptors()==before);
    puts("ORIGINAL_RESERVATION_RUN_RECEIPTS_PROFILE_CHAIN_SCOPE_ERRORS_WORKER_OWNED_READONLY_OK");return 0;
}
