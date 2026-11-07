#include "view_model.h"
#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"recovery view %d: %s\n",__LINE__,#x);exit(1);}}while(0)
static int fail_allocation;
static unsigned frees;
void *__real_calloc(size_t,size_t);
void *__wrap_calloc(size_t n,size_t size){if(fail_allocation){fail_allocation=0;return NULL;}return __real_calloc(n,size);}
void __real_ae_recovery_inventory_free(struct ae_recovery_inventory **);
void __wrap_ae_recovery_inventory_free(struct ae_recovery_inventory **v){if(v&&*v)++frees;__real_ae_recovery_inventory_free(v);}
static void parse_id(const char *s,uint8_t out[16]){
    CHECK(strlen(s)==32);for(size_t i=0;i<16;++i){unsigned x;CHECK(sscanf(s+2*i,"%2x",&x)==1);out[i]=(uint8_t)x;}
}
static void hex(const void *p,size_t n){const uint8_t *b=p;if(!n){putchar('-');return;}for(size_t i=0;i<n;++i)printf("%02x",b[i]);}
static struct ae_recovery_inventory *read_inventory(struct ae_catalogue *c){
    struct ae_recovery_inventory *v=NULL;CHECK(ae_catalogue_recovery_inventory(c,AE_RECOVERY_MAX_RECORDS,&v)==0&&v);return v;
}
static rv_key key(const struct ae_recovery_record *r){rv_key k={.kind=r->kind};memcpy(k.id,r->id,16);return k;}
static void publish_reference(const struct ae_recovery_inventory *v){
    printf("CONTEXT %u %" PRId64 " %zu\n",v->schema_version,v->context.revision,v->count);
    for(size_t i=0;i<v->count;++i){const struct ae_recovery_record *r=&v->records[i];
        printf("ITEM %u ",(unsigned)r->kind);hex(r->id,16);putchar(' ');if(r->has_related)hex(r->related,16);else putchar('-');
        printf(" %" PRId64 " %" PRId64 " ",r->revision,r->unfinished_steps);
        hex(r->state,strlen(r->state));putchar(' ');hex(r->detail,strlen(r->detail));putchar(' ');hex(r->phase,strlen(r->phase));putchar(' ');hex(r->reason,strlen(r->reason));putchar('\n');
    }
}
/* Adversarial presentation inputs, explicitly NOT new owner observations.
   Share 15/16 ID bytes and the whole ID across different record kinds. */
static struct ae_recovery_inventory *keys_fixture(struct ae_catalogue *c,int64_t revision){
    struct ae_recovery_inventory *v=read_inventory(c);free(v->records);
    v->records=calloc(4,sizeof(*v->records));CHECK(v->records);v->count=4;v->context.revision=revision;
    memset(v->by_kind,0,sizeof v->by_kind);v->by_kind[AE_RECOVERY_OPERATION]=v->by_kind[AE_RECOVERY_GATE]=2;
    for(size_t i=0;i<4;++i){struct ae_recovery_record *r=&v->records[i];
        r->kind=i<2?AE_RECOVERY_OPERATION:AE_RECOVERY_GATE;r->id[15]=(uint8_t)(1+i%2);r->revision=1;strcpy(r->state,"observed");}
    return v;
}
static void erase(struct ae_recovery_inventory *v,size_t index){
    CHECK(index<v->count);--v->by_kind[v->records[index].kind];
    memmove(v->records+index,v->records+index+1,(v->count-index-1)*sizeof(*v->records));--v->count;
}
/* The isolated original backend deliberately changes its runtime. Presentation
   remains read-only; its old session must reject a genuine fresh query result. */
static void runtime_replacement(struct ae_catalogue *catalogue,const struct ae_catalogue_identity *identity,const struct ae_catalogue_info *before){
    rv_owner *old=NULL,*fresh=NULL;rv_snapshot *view=NULL;
    CHECK(rv_owner_create(identity,before,&old)==NX_OK&&rv_owner_expect(old,1)==NX_OK);
    struct ae_recovery_inventory *v=read_inventory(catalogue);
    CHECK(rv_owner_complete(old,1,0,&v)==NX_OK&&!v&&rv_owner_snapshot(old,&view)==NX_OK);
    uint8_t runtime[16]={0x81,0,0,0,0,0,0x40,0,0x80,0,0,0,0,0,0,0x33};
    CHECK(ae_catalogue_begin_runtime(catalogue,before->boot_id,runtime)==0);
    struct ae_catalogue_info after,still;CHECK(ae_catalogue_get_info(catalogue,&after)==0);
    CHECK(after.revision>before->revision&&memcmp(after.runtime_incarnation,before->runtime_incarnation,16));
    v=read_inventory(catalogue);struct ae_recovery_inventory *held=v;
    CHECK(rv_owner_expect(old,2)==NX_OK&&rv_owner_complete(old,2,0,&v)==NX_ERROR_STATE&&v==held);
    CHECK(rv_owner_state(old)==RV_READING&&rv_owner_pending(old)==2&&rv_owner_epoch(old)==1);
    CHECK(!memcmp(rv_snapshot_data(view)->context.runtime_incarnation,before->runtime_incarnation,16));
    CHECK(rv_owner_create(identity,&after,&fresh)==NX_OK&&rv_owner_expect(fresh,1)==NX_OK);
    publish_reference(v);CHECK(rv_owner_complete(fresh,1,0,&v)==NX_OK&&!v);
    rv_owner_destroy(&old);rv_owner_destroy(&fresh);
    CHECK(rv_snapshot_data(view)->context.revision==before->revision);rv_snapshot_release(&view);
    CHECK(ae_catalogue_get_info(catalogue,&still)==0&&!memcmp(&after,&still,sizeof after));
    puts("RUNTIME_OK");
}
int main(int argc,char **argv){
    CHECK(argc==5||argc==6);struct ae_catalogue_identity identity={.kind=!strcmp(argv[2],"system")?AE_CATALOGUE_SYSTEM:AE_CATALOGUE_USER};
    parse_id(argv[3],identity.authority_epoch);if(identity.kind==AE_CATALOGUE_USER)parse_id(argv[4],identity.owner);
    struct ae_catalogue *catalogue=NULL;CHECK(ae_catalogue_open(argv[1],&identity,&catalogue)==0);
    struct ae_catalogue_info context,after;CHECK(ae_catalogue_get_info(catalogue,&context)==0);
    if(argc==6){CHECK(!strcmp(argv[5],"runtime"));runtime_replacement(catalogue,&identity,&context);CHECK(ae_catalogue_close(&catalogue)==0);return 0;}
    rv_owner *o=NULL;CHECK(rv_owner_create(&identity,&context,&o)==NX_OK);
    rv_owner *same=o;CHECK(rv_owner_create(&identity,&context,&o)==NX_ERROR_ARGUMENT&&o==same);
    CHECK(rv_owner_state(o)==RV_UNOBSERVED&&rv_owner_epoch(o)==0&&rv_owner_pending(o)==0);
    rv_snapshot *first=NULL,*second=NULL;CHECK(rv_owner_snapshot(o,&first)==NX_ERROR_STATE&&!first);
    CHECK(rv_owner_expect(o,0)==NX_ERROR_ARGUMENT);CHECK(rv_owner_expect(o,1)==NX_OK);
    CHECK(rv_owner_expect(o,1)==NX_ERROR_STATE&&rv_owner_state(o)==RV_READING);
    struct ae_recovery_inventory *v=read_inventory(catalogue),*original=v;
    publish_reference(v);CHECK(v->count>2);
    CHECK(rv_owner_complete(o,1,0,&v)==NX_OK&&!v&&rv_owner_epoch(o)==1);
    CHECK(rv_owner_complete(o,1,0,&v)==NX_ERROR_STATE);
    CHECK(rv_owner_snapshot(o,&first)==NX_OK&&rv_snapshot_data(first)==original);
    CHECK(rv_owner_snapshot(o,&first)==NX_ERROR_ARGUMENT&&rv_snapshot_data(first)==original);
    for(size_t i=0;i<original->count;++i)CHECK(rv_snapshot_find(first,key(&original->records[i]))==i);
    rv_key chosen=key(&original->records[1]),selected;
    CHECK(rv_owner_select(o,chosen)==NX_OK&&rv_owner_selection(o,&selected)&&!memcmp(selected.id,chosen.id,16)&&selected.kind==chosen.kind);
    CHECK(rv_owner_expect(o,2)==NX_OK&&rv_owner_expect(o,3)==NX_OK);
    v=read_inventory(catalogue);struct ae_recovery_inventory *held=v;
    CHECK(rv_owner_complete(o,2,0,&v)==NX_ERROR_STATE&&v==held&&rv_owner_pending(o)==3);
    ae_recovery_inventory_free(&v);
    CHECK(rv_owner_complete(o,2,-EIO,NULL)==NX_ERROR_STATE&&rv_owner_state(o)==RV_READING);
    CHECK(rv_owner_complete(o,3,-EIO,NULL)==NX_OK&&rv_owner_error(o)==-EIO&&rv_owner_state(o)==RV_UNAVAILABLE);
    CHECK(rv_owner_selection(o,&selected)&&selected.kind==chosen.kind&&!memcmp(selected.id,chosen.id,16));
    CHECK(rv_owner_expect(o,4)==NX_OK);v=read_inventory(catalogue);held=v;
    CHECK(rv_owner_complete(o,4,-EIO,&v)==NX_ERROR_ARGUMENT&&v==held&&rv_owner_pending(o)==4);
    /* Fail the private wrapper allocation after the original read completed. */
    fail_allocation=1;CHECK(rv_owner_complete(o,4,0,&v)==NX_ERROR_MEMORY&&v==held&&rv_owner_pending(o)==4&&rv_owner_epoch(o)==1);
    unsigned frees_before_refresh=frees;
    CHECK(rv_owner_complete(o,4,0,&v)==NX_OK&&!v&&rv_owner_epoch(o)==2&&frees==frees_before_refresh);
    CHECK(rv_owner_snapshot(o,&second)==NX_OK&&rv_snapshot_data(second)!=original);
    CHECK(rv_snapshot_data(first)==original&&original->count>2);
    rv_snapshot_release(&second);
    /* Every malformed/context/stale candidate is caller-owned on rejection. */
    CHECK(rv_owner_expect(o,5)==NX_OK);
    for(unsigned fault=0;fault<15;++fault){
        v=read_inventory(catalogue);held=v;
        switch(fault){
        case 0:v->identity.owner[15]^=1;break;
        case 1:v->identity.authority_epoch[15]^=1;v->context.authority_epoch[15]^=1;break;
        case 2:v->context.boot_id[15]^=1;break;
        case 3:v->context.runtime_incarnation[15]^=1;break;
        case 4:--v->context.revision;break;
        case 5:v->records[0].revision++;break; /* unchanged catalogue revision */
        case 6:++v->by_kind[AE_RECOVERY_OPERATION];break;
        case 7:v->records[1]=v->records[0];break;
        case 8:memset(v->records[0].reason,'x',sizeof v->records[0].reason);break;
        case 9:v->records[0].state[0]=(char)0xff;break;
        case 10:v->records[0].kind=AE_RECOVERY_KIND_COUNT;break;
        case 11:v->count=AE_RECOVERY_MAX_RECORDS+1;break;
        case 12:v->schema_version=99;break;
        case 13:v->records[0].has_related=2;break;
        case 14:v->records[0].unfinished_steps=-1;break;
        }
        CHECK(rv_owner_complete(o,5,0,&v)!=NX_OK&&v==held&&rv_owner_pending(o)==5&&rv_owner_epoch(o)==2);
        ae_recovery_inventory_free(&v);
    }
    CHECK(rv_owner_complete(o,5,-E2BIG,NULL)==NX_OK&&rv_owner_state(o)==RV_UNAVAILABLE&&rv_owner_error(o)==-E2BIG);
    /* Full opaque key, shifting rows, disappearing selection, empty snapshot. */
    int64_t revision=context.revision+100;
    CHECK(rv_owner_expect(o,6)==NX_OK);v=keys_fixture(catalogue,revision);
    chosen=key(&v->records[1]);rv_key same_id_other_kind=key(&v->records[3]);
    CHECK(rv_owner_complete(o,6,0,&v)==NX_OK&&!v&&rv_owner_select(o,chosen)==NX_OK);
    CHECK(rv_owner_snapshot(o,&second)==NX_OK);
    CHECK(rv_snapshot_find(second,chosen)==1&&rv_snapshot_find(second,same_id_other_kind)==3);
    rv_snapshot_release(&second);
    CHECK(rv_owner_expect(o,7)==NX_OK);v=keys_fixture(catalogue,++revision);erase(v,0);
    CHECK(rv_owner_complete(o,7,0,&v)==NX_OK&&!v&&rv_owner_selection(o,&selected)&&selected.kind==chosen.kind&&!memcmp(selected.id,chosen.id,16));
    CHECK(rv_owner_snapshot(o,&second)==NX_OK&&rv_snapshot_find(second,selected)==0);rv_snapshot_release(&second);
    CHECK(rv_owner_expect(o,8)==NX_OK);v=keys_fixture(catalogue,++revision);erase(v,1);
    CHECK(rv_owner_complete(o,8,0,&v)==NX_OK&&!v&&!rv_owner_selection(o,&selected));
    CHECK(rv_owner_select(o,chosen)==NX_ERROR_ARGUMENT&&!rv_owner_selection(o,&selected));
    CHECK(rv_owner_expect(o,9)==NX_OK);v=keys_fixture(catalogue,++revision);v->count=0;memset(v->by_kind,0,sizeof v->by_kind);
    CHECK(rv_owner_complete(o,9,0,&v)==NX_OK&&!v&&rv_owner_state(o)==RV_OBSERVED);
    CHECK(rv_owner_snapshot(o,&second)==NX_OK&&rv_snapshot_data(second)->count==0);
    unsigned before=frees;rv_owner_destroy(&o);CHECK(!o&&frees==before);
    CHECK(rv_snapshot_data(first)==original&&original->records[1].kind==chosen.kind);
    rv_snapshot_release(&first);CHECK(!first&&frees==before+1);rv_snapshot_release(&first);
    rv_snapshot_release(&second);CHECK(!second&&frees==before+2);rv_owner_destroy(&o);
    CHECK(ae_catalogue_get_info(catalogue,&after)==0&&!memcmp(&after,&context,sizeof context));
    CHECK(ae_catalogue_close(&catalogue)==0&&!catalogue);puts("MODEL_OK");return 0;
}
