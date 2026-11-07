#include "software_components.h"
#include <errno.h>
#include <stdlib.h>
#include <string.h>
typedef struct entry {sw_row observed;nx_result status;amh_document *document;} entry;
struct sw_components {sw_scope scope;size_t count;entry *entries;};
static int compare(const void *a,const void *b){return memcmp(((const entry *)a)->observed.install.bytes,((const entry *)b)->observed.install.bytes,16);}
static size_t find(const sw_result *r,sw_id id){size_t a=0,b=r->count;while(a<b){size_t m=a+(b-a)/2;if(memcmp(r->rows[m].install.bytes,id.bytes,16)<0)a=m+1;else b=m;}return a<r->count&&!memcmp(r->rows[a].install.bytes,id.bytes,16)?a:SIZE_MAX;}
int sw_component_identity_matches(const sw_row *row,const amh_document *document){
    const struct ae_fixture *d=amh_document_data(document);const unsigned char *artifact=amh_document_artifact(document);
    if(!row||row->error||ae_install_snapshot_valid(&row->value)||memcmp(row->install.bytes,row->value.install,16)||!d||!artifact)return 0;
    const char *component=row->value.component;size_t length=strlen(component);
    return !memcmp(row->value.artifact,artifact,32)&&!memcmp(row->value.publisher,d->publisher,16)&&
        length==d->component.size&&!memcmp(component,d->component.data,length);
}
void sw_components_free(sw_components **p){if(p&&*p){sw_components *b=*p;for(size_t i=0;i<b->count;++i)amh_document_free(&b->entries[i].document);free(b->entries);free(b);*p=NULL;}}
nx_result sw_components_read(const sw_result *observations,const sw_manifest_source *sources,size_t n,sw_components **out){
    if(!out||*out||!observations||(observations->count&&!observations->rows)||(n&&!sources))return NX_ERROR_ARGUMENT;
    if(n>SW_MAX_TARGETS||n>SIZE_MAX/sizeof(entry)||observations->count>SW_MAX_TARGETS)return NX_ERROR_LIMIT;
    sw_components *b=calloc(1,sizeof(*b));if(!b)return NX_ERROR_MEMORY;b->scope=observations->scope;b->count=n;
    b->entries=n?calloc(n,sizeof(*b->entries)):NULL;if(n&&!b->entries){free(b);return NX_ERROR_MEMORY;}
    /* Validate source keys and duplicates before opening any file. */
    for(size_t i=0;i<n;++i){if(!sources[i].path||!sources[i].path[0]){sw_components_free(&b);return NX_ERROR_ARGUMENT;}b->entries[i].observed.install=sources[i].install;}
    if(n>1)qsort(b->entries,n,sizeof(*b->entries),compare);
    for(size_t i=1;i<n;++i)if(!compare(&b->entries[i-1],&b->entries[i])){sw_components_free(&b);return NX_ERROR_ARGUMENT;}
    for(size_t i=0;i<n;++i){
        size_t at=find(observations,sources[i].install),a=0,z=n;while(a<z){size_t m=a+(z-a)/2;if(memcmp(b->entries[m].observed.install.bytes,sources[i].install.bytes,16)<0)a=m+1;else z=m;}
        entry *e=&b->entries[a];e->status=NX_ERROR_STATE;
        if(at==SIZE_MAX)continue;
        e->observed=observations->rows[at];
        if(e->observed.error||!sw_observation_same(&b->scope,&e->observed,&b->scope,&e->observed))continue;
        e->status=amh_document_read(sources[i].path,e->observed.value.artifact,&e->document);
        if(e->status==NX_OK&&!sw_component_identity_matches(&e->observed,e->document)){amh_document_free(&e->document);e->status=NX_ERROR_STATE;}
    }
    *out=b;return NX_OK;
}
nx_result sw_components_get(const sw_components *b,const sw_snapshot *snapshot,sw_id id,const amh_document **out){
    if(!out||*out||!snapshot)return NX_ERROR_ARGUMENT;
    const sw_result *r=sw_snapshot_data(snapshot);size_t at=sw_snapshot_find(snapshot,id);if(!r||at==SIZE_MAX)return NX_ERROR_STATE;
    if(!b)return NX_OK;
    size_t a=0,z=b->count;while(a<z){size_t m=a+(z-a)/2;if(memcmp(b->entries[m].observed.install.bytes,id.bytes,16)<0)a=m+1;else z=m;}
    if(a==b->count||memcmp(b->entries[a].observed.install.bytes,id.bytes,16))return NX_OK;
    const entry *e=&b->entries[a];if(!sw_observation_same(&b->scope,&e->observed,&r->scope,&r->rows[at]))return NX_ERROR_STATE;
    if(e->status!=NX_OK)return e->status;
    if(!sw_component_identity_matches(&r->rows[at],e->document))return NX_ERROR_STATE;
    *out=e->document;return NX_OK;
}
