#include "software_history.h"
#include <errno.h>
#include <stdlib.h>
#include <string.h>
struct swh_batch {sw_scope scope;size_t count;swh_record records[];};
static int context_same(const struct ae_catalogue_info *a,const struct ae_catalogue_info *b){
    return a->revision==b->revision&&!memcmp(a->authority_epoch,b->authority_epoch,16)&&
        !memcmp(a->boot_id,b->boot_id,16)&&!memcmp(a->runtime_incarnation,b->runtime_incarnation,16);
}
static size_t selected(const sw_result *v,const uint8_t id[16]){
    size_t a=0,z=v->count;while(a<z){size_t m=a+(z-a)/2;if(memcmp(v->rows[m].install.bytes,id,16)<0)a=m+1;else z=m;}
    return a<v->count&&!memcmp(v->rows[a].install.bytes,id,16)?a:SIZE_MAX;
}
static int compare(const void *a,const void *b){
    const swh_record *x=a,*y=b;int r=memcmp(x->observed.install.bytes,y->observed.install.bytes,16);return r?r:memcmp(x->digest,y->digest,32);
}
static int process_valid(const swh_record *r,enum ae_execution_process_role role,const struct ae_execution_process_snapshot *v){
    struct ae_execution_evidence evidence={0};memcpy(evidence.invocation,v->process.invocation,16);memcpy(evidence.context_digest,v->process.evidence_digest,32);
    return v->recorded_unix_ms>=0&&v->process.role==role&&!memcmp(v->process.profile_digest,r->digest,32)&&
        !memcmp(v->process.boot,r->profile.boot,16)&&ae_execution_process_match(&r->profile,&evidence,&v->process)==0;
}
static int unit_valid(const struct ae_unit_allocation_snapshot *v){
    const uint8_t *id=v->allocation.id;uint8_t zero[32]={0};
    return (id[6]&0xf0)==0x40&&(id[8]&0xc0)==0x80&&v->allocation.prepared_unix_ms>=0&&
        (v->witnessed==0||v->witnessed==1)&&(v->witnessed?v->witnessed_unix_ms>=v->allocation.prepared_unix_ms:
            v->witnessed_unix_ms==0&&!memcmp(v->selection,zero,32));
}
static int custody_valid(const struct ae_execution_profile *p,const struct ae_unit_custody_snapshot *v){
    uint8_t raw[AE_UNIT_ORIGIN_BYTES];
    if(v->node_count<1||v->node_count>8||v->count>AE_UNIT_EFFECTS_MAX||v->prepared_unix_ms<0||
        memcmp(v->origin.boot,p->boot,16)||ae_unit_origin_encode(&v->origin,raw))return 0;
    for(unsigned i=0;i<v->count;++i){const struct ae_unit_custody_step *s=&v->steps[i];
        if(s->effect<AE_UNIT_SUBSCRIBE||s->effect>AE_UNIT_UNSUBSCRIBE||
            ((s->effect==AE_UNIT_REF||s->effect==AE_UNIT_UNREF)?s->node>=v->node_count:s->node!=0)||
            (s->confirmed!=0&&s->confirmed!=1)||s->intended_unix_ms<v->prepared_unix_ms||
            (s->confirmed?s->confirmed_unix_ms<s->intended_unix_ms:s->confirmed_unix_ms!=0))return 0;
    }
    return 1;
}
static int unit_links(struct ae_catalogue *c,const sw_scope *scope,swh_record *r){
    struct ae_effect_owner current;memcpy(current.epoch,scope->identity.authority_epoch,16);
    memcpy(current.boot,scope->context.boot_id,16);memcpy(current.runtime,scope->context.runtime_incarnation,16);
    struct ae_unit_allocation_snapshot unit={0};int status=ae_catalogue_get_unit(c,&r->profile,&current,&unit);
    if(status&&status!=-ENOENT&&status!=-EPROTONOSUPPORT)return status;
    if(!status&&!unit_valid(&unit))return -EBADMSG;
    r->unit_error=status;if(!status)r->unit=unit;
    struct ae_unit_custody_snapshot custody={0};status=ae_catalogue_get_unit_custody(c,&r->profile,&current,&custody);
    if(status&&status!=-ENOENT&&status!=-EPROTONOSUPPORT)return status;
    if(!status&&!custody_valid(&r->profile,&custody))return -EBADMSG;
    /* Custody is tied to an original allocation by the original getter. */
    if(!status&&r->unit_error)return -EBADMSG;
    r->custody_error=status;if(!status)r->custody=custody;return 0;
}
void swh_free(swh_batch **p){if(p&&*p){free(*p);*p=NULL;}}
int swh_read(struct ae_catalogue *catalogue,const sw_result *apps,const struct ae_execution_profile *profiles,size_t n,swh_batch **out){
    if(!catalogue||!out||*out||(n&&!profiles)||!sw_result_valid(apps))return -EINVAL;
    if(n>SWH_MAX_PROFILES)return -E2BIG;
    swh_batch *b=calloc(1,sizeof(*b)+n*sizeof(*b->records));if(!b)return -ENOMEM;
    b->scope=apps->scope;b->count=n;int error=0;
    /* Copy every input/anchor before calling the original getters. */
    for(size_t i=0;i<n;++i)b->records[i].profile=profiles[i];
    for(size_t i=0;i<n&&!error;++i){
        swh_record *r=&b->records[i];const struct ae_execution_profile *p=&r->profile;
        error=ae_execution_digest(p,r->digest);
        if(!error&&(memcmp(p->system_epoch,b->scope.identity.authority_epoch,16)||memcmp(p->account,b->scope.account,16)||memcmp(p->boot,b->scope.context.boot_id,16)))error=-ESTALE;
        for(size_t j=0;j<i&&!error;++j)if(!memcmp(p->launch,b->records[j].profile.launch,16))error=-EINVAL;
        size_t at=selected(apps,p->install);memcpy(r->observed.install.bytes,p->install,16);
        if(at!=SIZE_MAX)r->observed=apps->rows[at];else r->observed.error=-ENOENT;
        r->starter_error=r->app_error=r->unit_error=r->custody_error=-ENOENT;
    }
    struct ae_catalogue_info before,after;
    if(!error)error=ae_catalogue_get_info(catalogue,&before);
    if(!error&&!context_same(&b->scope.context,&before))error=-ESTALE;
    for(size_t i=0;i<n&&!error;++i){
        swh_record *r=&b->records[i];if(r->observed.error)continue;
        for(unsigned role=AE_EXECUTION_STARTER;role<=AE_EXECUTION_APP&&!error;++role){
            struct ae_execution_process_snapshot value={0};int status=ae_catalogue_get_execution_process(catalogue,&r->profile,(enum ae_execution_process_role)role,&value);
            if(status&&status!=-ENOENT&&status!=-EPROTONOSUPPORT){error=status;break;}
            if(!status&&!process_valid(r,(enum ae_execution_process_role)role,&value)){error=-EBADMSG;break;}
            if(role==AE_EXECUTION_STARTER){r->starter_error=status;if(!status)r->starter=value;}
            else{r->app_error=status;if(!status)r->app=value;}
        }
        if(!error)error=unit_links(catalogue,&b->scope,r);
    }
    if(!error)error=ae_catalogue_get_info(catalogue,&after);
    if(!error&&!context_same(&before,&after))error=-ESTALE;
    if(error){swh_free(&b);return error;}
    if(n>1)qsort(b->records,n,sizeof(*b->records),compare);
    *out=b;return 0;
}
nx_result swh_get(const swh_batch *b,const sw_snapshot *snapshot,sw_id id,const swh_record **out,size_t *count){
    if(!out||*out||!count||*count||!snapshot)return NX_ERROR_ARGUMENT;
    const sw_result *apps=sw_snapshot_data(snapshot);size_t at=sw_snapshot_find(snapshot,id);
    if(!apps||at==SIZE_MAX||apps->rows[at].error)return NX_ERROR_STATE;
    if(!b)return NX_OK;
    const sw_row *row=&apps->rows[at];
    if(!sw_observation_same(&b->scope,row,&apps->scope,row))return NX_ERROR_STATE;
    size_t a=0,z=b->count;while(a<z){size_t m=a+(z-a)/2;if(memcmp(b->records[m].observed.install.bytes,id.bytes,16)<0)a=m+1;else z=m;}
    size_t first=a;
    while(a<b->count&&!memcmp(b->records[a].observed.install.bytes,id.bytes,16)){
        if(!sw_observation_same(&b->scope,&b->records[a].observed,&apps->scope,row))return NX_ERROR_STATE;
        ++a;
    }
    if(a!=first){*out=&b->records[first];*count=a-first;}return NX_OK;
}
