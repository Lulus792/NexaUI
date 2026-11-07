#include "software_model.h"
#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

struct sw_snapshot { unsigned references; sw_result *result; };
struct sw_owner {
    sw_scope scope;
    sw_id *targets;
    size_t count;
    sw_snapshot *current;
    uint64_t last_ticket,pending,epoch;
    sw_state state;
    int error,selected;
    sw_id selection;
};
static int zero(const uint8_t *p,size_t n){for(size_t i=0;i<n;++i)if(p[i])return 0;return 1;}
static int id_valid(const uint8_t p[16]){return (p[6]&0xf0)==0x40&&(p[8]&0xc0)==0x80;}
static int scope_valid(const sw_scope *s,int active){
    return s&&s->identity.kind==AE_CATALOGUE_SYSTEM&&zero(s->identity.owner,16)&&
        id_valid(s->identity.authority_epoch)&&!memcmp(s->identity.authority_epoch,s->context.authority_epoch,16)&&
        id_valid(s->context.boot_id)&&s->context.revision>0&&id_valid(s->account)&&s->os_uid!=UINT32_MAX&&
        (id_valid(s->context.runtime_incarnation)||(!active&&zero(s->context.runtime_incarnation,16)));
}
static int context_same(const struct ae_catalogue_info *a,const struct ae_catalogue_info *b){
    return !memcmp(a->authority_epoch,b->authority_epoch,16)&&!memcmp(a->boot_id,b->boot_id,16)&&!memcmp(a->runtime_incarnation,b->runtime_incarnation,16);
}
static int scope_same(const sw_scope *a,const sw_scope *b){
    return a->identity.kind==b->identity.kind&&!memcmp(a->identity.authority_epoch,b->identity.authority_epoch,16)&&
        !memcmp(a->identity.owner,b->identity.owner,16)&&context_same(&a->context,&b->context)&&
        !memcmp(a->account,b->account,16)&&a->os_uid==b->os_uid;
}
static int id_compare(const void *a,const void *b){return memcmp(((const sw_id *)a)->bytes,((const sw_id *)b)->bytes,16);}
static int row_compare(const void *a,const void *b){return id_compare(&((const sw_row *)a)->install,&((const sw_row *)b)->install);}
static int targets_valid(const sw_id *ids,size_t n){
    if(n>SW_MAX_TARGETS||(n&&!ids))return 0;
    for(size_t i=0;i<n;++i)if(!id_valid(ids[i].bytes))return 0;
    return 1;
}
static int row_valid(const sw_row *r,const sw_scope *s){
    if(r->error==-ENOENT){
        const struct ae_install_snapshot empty={0};
        /* Error rows originate in zero-initialized owned storage. Check fields,
           not struct padding received from another owner-side representation. */
        return zero(r->value.system_epoch,16)&&zero(r->value.system_runtime,16)&&zero(r->value.boot,16)&&
            zero(r->value.install,16)&&zero((const uint8_t *)r->value.component,sizeof r->value.component)&&
            zero(r->value.publisher,16)&&zero(r->value.account,16)&&zero(r->value.scope,16)&&
            zero(r->value.generation,16)&&zero(r->value.artifact,32)&&
            r->value.install_revision==empty.install_revision&&r->value.generation_revision==0&&
            r->value.gate_revision==0&&r->value.catalogue_revision==0&&r->value.gate==empty.gate;
    }
    return !r->error&&ae_install_snapshot_valid(&r->value)==0&&
        !memcmp(r->install.bytes,r->value.install,16)&&!memcmp(s->account,r->value.account,16)&&
        !memcmp(s->context.authority_epoch,r->value.system_epoch,16)&&!memcmp(s->context.boot_id,r->value.boot,16)&&
        !memcmp(s->context.runtime_incarnation,r->value.system_runtime,16)&&s->context.revision==r->value.catalogue_revision;
}
static int result_valid(const sw_result *v){
    if(!v||!scope_valid(&v->scope,1)||v->count>SW_MAX_TARGETS||(v->count&&!v->rows))return 0;
    for(size_t i=0;i<v->count;++i)if(!id_valid(v->rows[i].install.bytes)||!row_valid(&v->rows[i],&v->scope)||
        (i&&id_compare(&v->rows[i-1].install,&v->rows[i].install)>=0))return 0;
    return 1;
}
void sw_result_free(sw_result **p){if(p&&*p){free((*p)->rows);free(*p);*p=NULL;}}
int sw_read(struct ae_catalogue *catalogue,const sw_scope *scope,const sw_id *targets,size_t n,sw_result **out){
    if(!out||*out||!catalogue||!scope_valid(scope,0))return -EINVAL;
    if(n>SW_MAX_TARGETS)return -E2BIG;
    if(!targets_valid(targets,n))return -EINVAL;
    sw_scope copied=*scope;
    if(!scope_valid(&copied,1))return -EAGAIN;
    if(n>SIZE_MAX/sizeof(sw_row))return -EOVERFLOW;
    sw_result *r=calloc(1,sizeof(*r));if(!r)return -ENOMEM;
    r->scope=copied;r->count=n;
    if(n){r->rows=calloc(n,sizeof(*r->rows));if(!r->rows){sw_result_free(&r);return -ENOMEM;}}
    for(size_t i=0;i<n;++i)r->rows[i].install=targets[i];
    if(n>1)qsort(r->rows,n,sizeof(*r->rows),row_compare);
    for(size_t i=1;i<n;++i)if(!id_compare(&r->rows[i-1].install,&r->rows[i].install)){sw_result_free(&r);return -EINVAL;}
    struct ae_catalogue_info before,after;int error=ae_catalogue_get_info(catalogue,&before);
    if(!error&&(!context_same(&copied.context,&before)||before.revision<copied.context.revision))error=-ESTALE;
    if(!error)r->scope.context=before;
    for(size_t i=0;i<n&&!error;++i){
        struct ae_install_snapshot value={0};
        int status=ae_catalogue_resolve_app(catalogue,copied.account,copied.os_uid,r->rows[i].install.bytes,copied.context.boot_id,copied.context.runtime_incarnation,&value);
        if(status==-ENOENT)r->rows[i].error=status;
        else if(status)error=status;
        else r->rows[i].value=value;
    }
    if(!error)error=ae_catalogue_get_info(catalogue,&after);
    if(!error&&(!context_same(&before,&after)||before.revision!=after.revision))error=-ESTALE;
    if(!error&&!result_valid(r))error=-EBADMSG;
    if(error){sw_result_free(&r);return error;}
    *out=r;return 0;
}
static int row_equal(const sw_row *a,const sw_row *b){
    if(id_compare(&a->install,&b->install)||a->error!=b->error)return 0;
    if(a->error)return 1;
    const struct ae_install_snapshot *p=&a->value,*q=&b->value;
    return !memcmp(p->system_epoch,q->system_epoch,16)&&!memcmp(p->system_runtime,q->system_runtime,16)&&
        !memcmp(p->boot,q->boot,16)&&!memcmp(p->install,q->install,16)&&!strcmp(p->component,q->component)&&
        !memcmp(p->publisher,q->publisher,16)&&!memcmp(p->account,q->account,16)&&!memcmp(p->scope,q->scope,16)&&
        !memcmp(p->generation,q->generation,16)&&!memcmp(p->artifact,q->artifact,32)&&
        p->install_revision==q->install_revision&&p->generation_revision==q->generation_revision&&
        p->gate_revision==q->gate_revision&&p->catalogue_revision==q->catalogue_revision&&p->gate==q->gate;
}
nx_result sw_owner_create(const sw_scope *scope,const sw_id *targets,size_t n,sw_owner **out){
    if(!out||*out||!scope_valid(scope,0))return NX_ERROR_ARGUMENT;
    if(n>SW_MAX_TARGETS||n>SIZE_MAX/sizeof(sw_id))return NX_ERROR_LIMIT;
    if(!targets_valid(targets,n))return NX_ERROR_ARGUMENT;
    sw_owner *o=calloc(1,sizeof(*o));if(!o)return NX_ERROR_MEMORY;o->scope=*scope;o->count=n;
    if(n){o->targets=calloc(n,sizeof(*o->targets));if(!o->targets){free(o);return NX_ERROR_MEMORY;}memcpy(o->targets,targets,n*sizeof(*targets));}
    if(n>1)qsort(o->targets,n,sizeof(*o->targets),id_compare);
    for(size_t i=1;i<n;++i)if(!id_compare(&o->targets[i-1],&o->targets[i])){free(o->targets);free(o);return NX_ERROR_ARGUMENT;}
    *out=o;return NX_OK;
}
void sw_snapshot_release(sw_snapshot **p){if(p&&*p){sw_snapshot *s=*p;*p=NULL;if(!--s->references){sw_result_free(&s->result);free(s);}}}
void sw_owner_destroy(sw_owner **p){if(p&&*p){sw_owner *o=*p;*p=NULL;sw_snapshot_release(&o->current);free(o->targets);free(o);}}
nx_result sw_owner_expect(sw_owner *o,uint64_t ticket){
    if(!o||!ticket)return NX_ERROR_ARGUMENT;
    if(ticket<=o->last_ticket)return NX_ERROR_STATE;
    o->pending=o->last_ticket=ticket;o->state=SW_READING;o->error=0;return NX_OK;
}
nx_result sw_owner_complete(sw_owner *o,uint64_t ticket,int error,sw_result **p){
    if(!o||!ticket||error>0)return NX_ERROR_ARGUMENT;
    if(ticket!=o->pending)return NX_ERROR_STATE;
    if(error){if(p&&*p)return NX_ERROR_ARGUMENT;o->pending=0;o->state=SW_UNAVAILABLE;o->error=error;return NX_OK;}
    if(!p||!*p||!result_valid(*p))return NX_ERROR_ARGUMENT;
    sw_result *v=*p;
    if(!scope_same(&o->scope,&v->scope)||v->scope.context.revision<o->scope.context.revision||v->count!=o->count)return NX_ERROR_STATE;
    for(size_t i=0;i<v->count;++i)if(id_compare(&o->targets[i],&v->rows[i].install))return NX_ERROR_STATE;
    if(o->current){const sw_result *old=o->current->result;
        if(v==old)return NX_ERROR_ARGUMENT;
        if(v->scope.context.revision<old->scope.context.revision)return NX_ERROR_STATE;
        if(v->scope.context.revision==old->scope.context.revision)for(size_t i=0;i<v->count;++i)if(!row_equal(&old->rows[i],&v->rows[i]))return NX_ERROR_STATE;
    }
    if(o->epoch==UINT64_MAX)return NX_ERROR_LIMIT;
    sw_snapshot *s=calloc(1,sizeof(*s));if(!s)return NX_ERROR_MEMORY;s->references=1;s->result=v;
    sw_snapshot_release(&o->current);o->current=s;*p=NULL;o->pending=0;o->state=SW_OBSERVED;o->error=0;++o->epoch;return NX_OK;
}
sw_state sw_owner_state(const sw_owner *o){return o?o->state:SW_UNOBSERVED;}
int sw_owner_error(const sw_owner *o){return o?o->error:0;}
uint64_t sw_owner_epoch(const sw_owner *o){return o?o->epoch:0;}
uint64_t sw_owner_pending(const sw_owner *o){return o?o->pending:0;}
size_t sw_snapshot_find(const sw_snapshot *s,sw_id id){
    if(!s)return SIZE_MAX;
    size_t a=0,b=s->result->count;
    while(a<b){size_t m=a+(b-a)/2;if(id_compare(&s->result->rows[m].install,&id)<0)a=m+1;else b=m;}
    return a<s->result->count&&!id_compare(&s->result->rows[a].install,&id)?a:SIZE_MAX;
}
nx_result sw_owner_select(sw_owner *o,sw_id id){if(!o||sw_snapshot_find(o->current,id)==SIZE_MAX)return NX_ERROR_ARGUMENT;o->selection=id;o->selected=1;return NX_OK;}
int sw_owner_selection(const sw_owner *o,sw_id *out){if(!out)return 0;*out=(sw_id){0};if(!o||!o->selected)return 0;*out=o->selection;return 1;}
nx_result sw_owner_snapshot(sw_owner *o,sw_snapshot **out){
    if(!out||*out)return NX_ERROR_ARGUMENT;
    if(!o||!o->current)return NX_ERROR_STATE;
    if(o->current->references==UINT_MAX)return NX_ERROR_LIMIT;
    ++o->current->references;*out=o->current;return NX_OK;
}
const sw_result *sw_snapshot_data(const sw_snapshot *s){return s?s->result:NULL;}
