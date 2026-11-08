#include "software_recovery.h"
#include "../33_aetheri_recovery/view_model.h"
#include <errno.h>
#include <string.h>
static int context_same(const struct ae_catalogue_info *a,const struct ae_catalogue_info *b){
    return a->revision==b->revision&&!memcmp(a->authority_epoch,b->authority_epoch,16)&&
        !memcmp(a->boot_id,b->boot_id,16)&&!memcmp(a->runtime_incarnation,b->runtime_incarnation,16);
}
static void unavailable(swh_record *r,int error){
    r->recovery_error=error;r->recovery_schema=0;
    for(unsigned j=0;j<SWH_RECOVERY_LINKS;++j){r->recovery[j]=(swh_recovery_link){.error=error};}
}
int swh_recovery_read(struct ae_catalogue *catalogue,const sw_scope *scope,swh_record *records,size_t count){
    if(!catalogue||!scope||(count&&!records))return -EINVAL;
    if(count>SWH_MAX_PROFILES)return -E2BIG;
    int needed=0;
    for(size_t i=0;i<count;++i){swh_record *r=&records[i];int error=r->observed.error?r->observed.error:r->reservation_error;unavailable(r,error?error:-ENOENT);if(!error)needed=1;}
    if(!needed)return 0;
    struct ae_recovery_inventory *inventory=NULL;int error=ae_catalogue_recovery_inventory(catalogue,AE_RECOVERY_MAX_RECORDS,&inventory);
    if(error){
        ae_recovery_inventory_free(&inventory);
        if(error!=-EPROTONOSUPPORT)return error;
        for(size_t i=0;i<count;++i)if(!records[i].observed.error&&!records[i].reservation_error)unavailable(&records[i],error);
        return 0;
    }
    rv_owner *owner=NULL;rv_snapshot *snapshot=NULL;
    if(!inventory){error=-EBADMSG;goto done;}
    if(!context_same(&scope->context,&inventory->context)){error=-ESTALE;goto done;}
    nx_result result=rv_owner_create(&scope->identity,&scope->context,&owner);
    if(result==NX_OK)result=rv_owner_expect(owner,1);
    if(result==NX_OK)result=rv_owner_complete(owner,1,0,&inventory);
    if(result==NX_OK)result=rv_owner_snapshot(owner,&snapshot);
    if(result!=NX_OK){error=result==NX_ERROR_MEMORY?-ENOMEM:-EBADMSG;goto done;}
    const struct ae_recovery_inventory *data=rv_snapshot_data(snapshot);
    const enum ae_recovery_record_kind kinds[SWH_RECOVERY_LINKS]={AE_RECOVERY_OPERATION,AE_RECOVERY_CODE_LEASE,AE_RECOVERY_DISPATCH,AE_RECOVERY_CLOSURE,AE_RECOVERY_HANDOFF};
    for(size_t i=0;i<count&&!error;++i){
        swh_record *r=&records[i];if(r->observed.error||r->reservation_error)continue;
        const uint8_t *ids[SWH_RECOVERY_LINKS]={r->profile.root_operation,r->profile.lease,r->profile.launch,r->profile.launch,r->profile.launch};
        const uint8_t *related[SWH_RECOVERY_LINKS]={r->profile.install,r->profile.generation,r->profile.lease,r->profile.root_operation,NULL};
        r->recovery_error=0;r->recovery_schema=data->schema_version;
        for(unsigned j=0;j<SWH_RECOVERY_LINKS;++j){
            rv_key key={.kind=kinds[j]};memcpy(key.id,ids[j],16);size_t at=rv_snapshot_find(snapshot,key);if(at==SIZE_MAX)continue;
            const struct ae_recovery_record *value=&data->records[at];
            if(related[j]?(!value->has_related||memcmp(value->related,related[j],16)):value->has_related){error=-EBADMSG;break;}
            r->recovery[j].error=0;r->recovery[j].value=*value;
        }
    }
done:
    rv_snapshot_release(&snapshot);rv_owner_destroy(&owner);ae_recovery_inventory_free(&inventory);return error;
}
