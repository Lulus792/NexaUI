#include "view_model.h"
#include <limits.h>
#include <stdlib.h>
#include <string.h>
struct rv_snapshot { unsigned references; struct ae_recovery_inventory *inventory; };
struct rv_owner {
    struct ae_catalogue_identity identity;
    struct ae_catalogue_info context;
    rv_snapshot *current;
    uint64_t last_ticket,pending,epoch;
    rv_observation state;
    int error,selected;
    rv_key key;
};
static int identity_equal(const struct ae_catalogue_identity *a,const struct ae_catalogue_identity *b){
    return a->kind==b->kind&&!memcmp(a->authority_epoch,b->authority_epoch,16)&&!memcmp(a->owner,b->owner,16);
}
static int context_equal(const struct ae_catalogue_info *a,const struct ae_catalogue_info *b){
    return !memcmp(a->authority_epoch,b->authority_epoch,16)&&!memcmp(a->boot_id,b->boot_id,16)&&!memcmp(a->runtime_incarnation,b->runtime_incarnation,16);
}
static int key_compare(rv_key a,rv_key b){
    if(a.kind!=b.kind)return a.kind<b.kind?-1:1;
    return memcmp(a.id,b.id,16);
}
static rv_key record_key(const struct ae_recovery_record *r){rv_key k={.kind=r->kind};memcpy(k.id,r->id,16);return k;}
static int string_valid(const char *s,size_t capacity){
    const char *end=memchr(s,0,capacity);return end&&nx_utf8_validate(s,(size_t)(end-s),NULL)==NX_OK;
}
static int shape(const struct ae_recovery_inventory *v){
    if(!v||v->count>AE_RECOVERY_MAX_RECORDS||(v->count&&!v->records)||v->context.revision<1||
       memcmp(v->identity.authority_epoch,v->context.authority_epoch,16)||
       (v->identity.kind!=AE_CATALOGUE_SYSTEM&&v->identity.kind!=AE_CATALOGUE_USER)||
       v->schema_version<2||v->schema_version>RV_ROOT_SCHEMA_MAX||
       (v->identity.kind==AE_CATALOGUE_USER&&(v->schema_version==5||v->schema_version==6||v->schema_version>=9)))return 0;
    size_t counts[AE_RECOVERY_KIND_COUNT]={0};
    for(size_t i=0;i<v->count;++i){
        const struct ae_recovery_record *p=&v->records[i];
#if RV_UNIT_ISSUER
        if(p->kind==AE_RECOVERY_UNIT_ISSUER&&v->schema_version!=11)return 0;
#endif
        if(p->kind<AE_RECOVERY_OPERATION||p->kind>=AE_RECOVERY_KIND_COUNT||p->revision<1||p->unfinished_steps<0||
           (p->has_related!=0&&p->has_related!=1)||!p->state[0]||
           !string_valid(p->state,sizeof p->state)||!string_valid(p->detail,sizeof p->detail)||
           !string_valid(p->phase,sizeof p->phase)||!string_valid(p->reason,sizeof p->reason))return 0;
        if(!p->has_related){uint8_t zero[16]={0};if(memcmp(p->related,zero,16))return 0;}
        if(i&&key_compare(record_key(&v->records[i-1]),record_key(p))>=0)return 0;
        ++counts[p->kind];
    }
    for(size_t i=0;i<AE_RECOVERY_KIND_COUNT;++i)if(counts[i]!=v->by_kind[i])return 0;
    return 1;
}
static int equal(const struct ae_recovery_inventory *a,const struct ae_recovery_inventory *b){
    if(a->schema_version!=b->schema_version||a->count!=b->count)return 0;
    for(size_t i=0;i<a->count;++i){const struct ae_recovery_record *p=&a->records[i],*q=&b->records[i];
        if(key_compare(record_key(p),record_key(q))||p->has_related!=q->has_related||memcmp(p->related,q->related,16)||
           p->revision!=q->revision||p->unfinished_steps!=q->unfinished_steps||strcmp(p->state,q->state)||
           strcmp(p->detail,q->detail)||strcmp(p->phase,q->phase)||strcmp(p->reason,q->reason))return 0;
    }
    return 1;
}
nx_result rv_owner_create(const struct ae_catalogue_identity *identity,const struct ae_catalogue_info *context,rv_owner **out){
    if(!out||*out)return NX_ERROR_ARGUMENT;
    if(!identity||!context||context->revision<1||memcmp(identity->authority_epoch,context->authority_epoch,16)||
       (identity->kind!=AE_CATALOGUE_SYSTEM&&identity->kind!=AE_CATALOGUE_USER))return NX_ERROR_ARGUMENT;
    rv_owner *o=calloc(1,sizeof(*o));if(!o)return NX_ERROR_MEMORY;
    o->identity=*identity;o->context=*context;*out=o;return NX_OK;
}
void rv_snapshot_release(rv_snapshot **value){
    if(!value||!*value)return;
    rv_snapshot *s=*value;*value=NULL;
    if(!--s->references){ae_recovery_inventory_free(&s->inventory);free(s);}
}
void rv_owner_destroy(rv_owner **value){
    if(!value||!*value)return;
    rv_owner *o=*value;*value=NULL;rv_snapshot_release(&o->current);free(o);
}
nx_result rv_owner_expect(rv_owner *o,uint64_t ticket){
    if(!o||!ticket)return NX_ERROR_ARGUMENT;
    if(ticket<=o->last_ticket)return NX_ERROR_STATE;
    o->pending=o->last_ticket=ticket;o->state=RV_READING;o->error=0;return NX_OK;
}
size_t rv_snapshot_find(const rv_snapshot *s,rv_key key){
    if(!s||key.kind<AE_RECOVERY_OPERATION||key.kind>=AE_RECOVERY_KIND_COUNT)return SIZE_MAX;
    size_t a=0,b=s->inventory->count;
    while(a<b){size_t m=a+(b-a)/2;int c=key_compare(record_key(&s->inventory->records[m]),key);if(c<0)a=m+1;else b=m;}
    return a<s->inventory->count&&!key_compare(record_key(&s->inventory->records[a]),key)?a:SIZE_MAX;
}
nx_result rv_owner_complete(rv_owner *o,uint64_t ticket,int error,struct ae_recovery_inventory **result){
    if(!o||!ticket||error>0)return NX_ERROR_ARGUMENT;
    if(ticket!=o->pending)return NX_ERROR_STATE;
    if(error){if(result&&*result)return NX_ERROR_ARGUMENT;o->pending=0;o->state=RV_UNAVAILABLE;o->error=error;return NX_OK;}
    if(!result||!*result||!shape(*result))return NX_ERROR_ARGUMENT;
    struct ae_recovery_inventory *v=*result;
    if(!identity_equal(&o->identity,&v->identity)||!context_equal(&o->context,&v->context)||v->context.revision<o->context.revision)return NX_ERROR_STATE;
    if(o->current){const struct ae_recovery_inventory *old=o->current->inventory;
        if(v==old)return NX_ERROR_ARGUMENT;
        if(v->context.revision<old->context.revision||(v->context.revision==old->context.revision&&!equal(old,v)))return NX_ERROR_STATE;
    }
    if(o->epoch==UINT64_MAX)return NX_ERROR_LIMIT;
    rv_snapshot *s=calloc(1,sizeof(*s));if(!s)return NX_ERROR_MEMORY;
    s->references=1;s->inventory=v;
    if(o->selected&&rv_snapshot_find(s,o->key)==SIZE_MAX)o->selected=0;
    rv_snapshot_release(&o->current);o->current=s;*result=NULL;
    o->pending=0;o->state=RV_OBSERVED;o->error=0;++o->epoch;return NX_OK;
}
rv_observation rv_owner_state(const rv_owner *o){return o?o->state:RV_UNOBSERVED;}
int rv_owner_error(const rv_owner *o){return o?o->error:0;}
uint64_t rv_owner_epoch(const rv_owner *o){return o?o->epoch:0;}
uint64_t rv_owner_pending(const rv_owner *o){return o?o->pending:0;}
nx_result rv_owner_select(rv_owner *o,rv_key key){
    if(!o)return NX_ERROR_ARGUMENT;
    if(rv_snapshot_find(o->current,key)==SIZE_MAX)return NX_ERROR_ARGUMENT;
    o->key=key;o->selected=1;return NX_OK;
}
int rv_owner_selection(const rv_owner *o,rv_key *out){
    if(!out)return 0;
    *out=(rv_key){0};if(!o||!o->selected)return 0;*out=o->key;return 1;
}
nx_result rv_owner_snapshot(rv_owner *o,rv_snapshot **out){
    if(!out||*out)return NX_ERROR_ARGUMENT;
    if(!o||!o->current)return NX_ERROR_STATE;
    if(o->current->references==UINT_MAX)return NX_ERROR_LIMIT;
    ++o->current->references;*out=o->current;return NX_OK;
}
const struct ae_recovery_inventory *rv_snapshot_data(const rv_snapshot *s){return s?s->inventory:NULL;}
