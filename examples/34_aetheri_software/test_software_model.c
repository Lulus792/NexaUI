#include "software_model.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(x) do{if(!(x)){fprintf(stderr,"software model %d: %s\n",__LINE__,#x);exit(1);}}while(0)
static long fail_after=-1;
static unsigned resolves,infos;
static int fault,mutate_inputs;
static sw_scope *borrowed_scope;
static sw_id *borrowed_targets;
void *__real_calloc(size_t,size_t);
void *__wrap_calloc(size_t n,size_t size){if(fail_after==0){fail_after=-1;return NULL;}if(fail_after>0)--fail_after;return __real_calloc(n,size);}
int __real_ae_catalogue_get_info(struct ae_catalogue *,struct ae_catalogue_info *);
int __wrap_ae_catalogue_get_info(struct ae_catalogue *c,struct ae_catalogue_info *out){
    ++infos;int r=__real_ae_catalogue_get_info(c,out);
    if(!r&&infos==2){if(fault==1)++out->revision;if(fault==2)out->runtime_incarnation[15]^=1;}
    return r;
}
int __real_ae_catalogue_resolve_app(struct ae_catalogue *,const uint8_t[16],uint32_t,const uint8_t[16],const uint8_t[16],const uint8_t[16],struct ae_install_snapshot *);
int __wrap_ae_catalogue_resolve_app(struct ae_catalogue *c,const uint8_t account[16],uint32_t uid,const uint8_t install[16],const uint8_t boot[16],const uint8_t runtime[16],struct ae_install_snapshot *out){
    ++resolves;
    if(mutate_inputs&&resolves==1){borrowed_scope->account[15]^=1;borrowed_targets[0].bytes[15]^=1;}
    if(fault==3&&resolves==2)return -EIO;
    if(fault==4&&resolves==2)return -ENOMEM;
    return __real_ae_catalogue_resolve_app(c,account,uid,install,boot,runtime,out);
}
static sw_id id(uint32_t n){sw_id v={{0}};v.bytes[6]=0x40;v.bytes[8]=0x80;for(unsigned i=0;i<4;++i)v.bytes[15-i]=(uint8_t)(n>>(8*i));return v;}
static void ids(uint8_t out[16],uint32_t n){sw_id v=id(n);memcpy(out,v.bytes,16);}
static sw_scope scope_for(struct ae_catalogue *c){
    sw_scope s={0};s.identity.kind=AE_CATALOGUE_SYSTEM;ids(s.identity.authority_epoch,1);ids(s.account,2);s.os_uid=1000;CHECK(ae_catalogue_get_info(c,&s.context)==0);return s;
}
static sw_result *clone(const sw_result *src){
    sw_result *r=calloc(1,sizeof(*r));CHECK(r);*r=*src;r->rows=src->count?calloc(src->count,sizeof(*r->rows)):NULL;CHECK(!src->count||r->rows);if(src->count)memcpy(r->rows,src->rows,src->count*sizeof(*r->rows));return r;
}
static void revision(sw_result *r,int64_t v){r->scope.context.revision=v;for(size_t i=0;i<r->count;++i)if(!r->rows[i].error)r->rows[i].value.catalogue_revision=v;}
static void changed_context(sw_result *r,unsigned which){
    uint8_t *p=which==0?r->scope.context.runtime_incarnation:r->scope.context.boot_id;p[15]^=1;
    for(size_t i=0;i<r->count;++i)if(!r->rows[i].error)memcpy(which==0?r->rows[i].value.system_runtime:r->rows[i].value.boot,p,16);
}
static void reset(void){fault=mutate_inputs=0;resolves=infos=0;}
static void bootstrap(const char *root_path,const char *user_path){
    struct ae_catalogue_identity root={.kind=AE_CATALOGUE_SYSTEM},user={.kind=AE_CATALOGUE_USER};ids(root.authority_epoch,1);ids(user.authority_epoch,3);ids(user.owner,2);
    uint8_t boot[16],runtime[16],other[16];ids(boot,4);ids(runtime,5);ids(other,7);struct ae_catalogue *c=NULL;
    CHECK(ae_catalogue_create(root_path,&root,boot,&c)==0&&ae_catalogue_enroll_account(c,user.owner,1000)==0&&ae_catalogue_enroll_account(c,other,1001)==0&&ae_catalogue_begin_runtime(c,boot,runtime)==0&&ae_catalogue_close(&c)==0);
    ids(runtime,6);CHECK(ae_catalogue_create(user_path,&user,boot,&c)==0&&ae_catalogue_begin_runtime(c,boot,runtime)==0&&ae_catalogue_close(&c)==0);puts("BOOTSTRAP_OK");
}
static void owner_checks(const sw_scope *scope,const sw_id targets[5],sw_result **original){
    sw_owner *o=NULL;sw_snapshot *old=NULL,*current=NULL;sw_id selected;
    CHECK(sw_owner_create(scope,targets,5,&o)==NX_OK&&sw_owner_state(o)==SW_UNOBSERVED&&sw_owner_epoch(o)==0&&!sw_owner_selection(o,&selected));
    CHECK(sw_owner_snapshot(o,&old)==NX_ERROR_STATE&&!old&&sw_owner_select(o,id(10))==NX_ERROR_ARGUMENT);
    CHECK(sw_owner_expect(o,0)==NX_ERROR_ARGUMENT&&sw_owner_expect(o,1)==NX_OK&&sw_owner_state(o)==SW_READING);
    sw_result *r=clone(*original);CHECK(sw_owner_complete(o,2,0,&r)==NX_ERROR_STATE&&r&&sw_owner_pending(o)==1);sw_result_free(&r);
    CHECK(sw_owner_complete(o,1,0,original)==NX_OK&&!*original&&sw_owner_epoch(o)==1&&sw_owner_state(o)==SW_OBSERVED);
    CHECK(sw_owner_snapshot(o,&old)==NX_OK&&sw_owner_select(o,id(12))==NX_OK&&sw_owner_selection(o,&selected)&&!memcmp(selected.bytes,id(12).bytes,16));
    const sw_result *base=sw_snapshot_data(old);CHECK(base&&base->count==5&&sw_snapshot_find(old,id(99))==4&&sw_snapshot_find(old,id(200))==SIZE_MAX);
    CHECK(sw_owner_expect(o,1)==NX_ERROR_STATE&&sw_owner_expect(o,2)==NX_OK);
    r=clone(base);CHECK(sw_owner_complete(o,2,-EIO,&r)==NX_ERROR_ARGUMENT&&r&&sw_owner_pending(o)==2);sw_result_free(&r);
    CHECK(sw_owner_complete(o,2,-EIO,NULL)==NX_OK&&sw_owner_state(o)==SW_UNAVAILABLE&&sw_owner_error(o)==-EIO&&sw_owner_epoch(o)==1&&sw_owner_selection(o,&selected));
    CHECK(sw_owner_expect(o,3)==NX_OK&&sw_owner_expect(o,4)==NX_OK);r=clone(base);CHECK(sw_owner_complete(o,3,0,&r)==NX_ERROR_STATE&&r&&sw_owner_pending(o)==4);sw_result_free(&r);
    for(unsigned test=0;test<10;++test){
        r=clone(base);nx_result expected=NX_ERROR_STATE;
        if(test==0)r->scope.os_uid=1001;
        else if(test==1){r->scope.account[15]=7;for(size_t i=0;i<r->count;++i)if(!r->rows[i].error)r->rows[i].value.account[15]=7;}
        else if(test==2||test==3)changed_context(r,test-2);
        else if(test==4)revision(r,base->scope.context.revision-1);
        else if(test==5)++r->rows[0].value.install_revision;
        else if(test==6){r->rows[4].install=id(100);revision(r,base->scope.context.revision+1);}
        else if(test==7)--r->count;
        else if(test==8){r->rows[1].value.component[0]='x';expected=NX_ERROR_ARGUMENT;}
        else {r->rows[0].value.install[15]^=1;expected=NX_ERROR_ARGUMENT;}
        CHECK(sw_owner_complete(o,4,0,&r)==expected&&r&&sw_owner_pending(o)==4&&sw_owner_epoch(o)==1&&sw_owner_selection(o,&selected));sw_result_free(&r);
    }
    r=clone(base);fail_after=0;CHECK(sw_owner_complete(o,4,0,&r)==NX_ERROR_MEMORY&&r&&sw_owner_pending(o)==4&&sw_owner_epoch(o)==1);
    CHECK(sw_owner_complete(o,4,0,&r)==NX_OK&&!r&&sw_owner_epoch(o)==2&&sw_owner_selection(o,&selected));
    r=clone(base);revision(r,base->scope.context.revision+1);CHECK(sw_owner_expect(o,5)==NX_OK&&sw_owner_complete(o,5,0,&r)==NX_OK&&!r&&sw_owner_snapshot(o,&current)==NX_OK);
    CHECK(sw_snapshot_data(current)->scope.context.revision==base->scope.context.revision+1&&sw_snapshot_data(old)==base&&sw_owner_selection(o,&selected));
    sw_owner_destroy(&o);CHECK(!o&&base->rows[0].value.install[15]==10&&sw_snapshot_data(current)->rows[2].install.bytes[15]==12);sw_snapshot_release(&old);sw_snapshot_release(&current);CHECK(!old&&!current);
    for(long budget=0;budget<2;++budget){fail_after=budget;CHECK(sw_owner_create(scope,targets,5,&o)==NX_ERROR_MEMORY&&!o);}fail_after=-1;
    sw_id duplicate[2]={id(10),id(10)};CHECK(sw_owner_create(scope,duplicate,2,&o)==NX_ERROR_ARGUMENT&&!o);
    CHECK(sw_owner_create(scope,targets,(size_t)SW_MAX_TARGETS+1,&o)==NX_ERROR_LIMIT&&!o);
    sw_scope unstarted=*scope;memset(unstarted.context.runtime_incarnation,0,16);CHECK(sw_owner_create(&unstarted,targets,5,&o)==NX_OK&&sw_owner_expect(o,1)==NX_OK&&sw_owner_complete(o,1,-EAGAIN,NULL)==NX_OK&&sw_owner_state(o)==SW_UNAVAILABLE);sw_owner_destroy(&o);
    puts("OWNER_TICKETS_CONTEXT_RECIPIENT_KEYS_REVISION_OOM_AND_RETAINED_SNAPSHOTS_OK");
}
static void read_checks(const char *root_path,const char *user_path,int change_runtime){
    struct ae_catalogue_identity root={.kind=AE_CATALOGUE_SYSTEM},user={.kind=AE_CATALOGUE_USER};ids(root.authority_epoch,1);ids(user.authority_epoch,3);ids(user.owner,2);
    struct ae_catalogue *c=NULL,*u=NULL;CHECK(ae_catalogue_open(root_path,&root,&c)==0&&ae_catalogue_open(user_path,&user,&u)==0);
    sw_scope scope=scope_for(c);sw_id targets[5]={id(12),id(99),id(10),id(13),id(11)};sw_result *r=NULL;
    reset();CHECK(sw_read(c,&scope,targets,5,&r)==0&&r&&r->count==5&&resolves==5&&infos==2);
    CHECK(r->rows[0].install.bytes[15]==10&&!r->rows[0].error&&r->rows[0].value.scope[15]==2&&r->rows[0].value.account[15]==2);
    CHECK(r->rows[2].install.bytes[15]==12&&!r->rows[2].error&&r->rows[2].value.scope[15]==8);
    CHECK(r->rows[1].error==-ENOENT&&r->rows[3].error==-ENOENT&&r->rows[4].error==-ENOENT);
    struct ae_install_snapshot exact;CHECK(ae_catalogue_resolve_app(c,scope.account,scope.os_uid,id(10).bytes,scope.context.boot_id,scope.context.runtime_incarnation,&exact)==0&&!strcmp(exact.component,r->rows[0].value.component)&&!memcmp(exact.artifact,r->rows[0].value.artifact,32)&&exact.gate==r->rows[0].value.gate);
    owner_checks(&scope,targets,&r);CHECK(!r);
    for(int mode=1;mode<=4;++mode){reset();fault=mode;CHECK(sw_read(c,&scope,targets,5,&r)==(mode<=2?-ESTALE:mode==3?-EIO:-ENOMEM)&&!r);}reset();
    for(long budget=0;budget<2;++budget){fail_after=budget;reset();CHECK(sw_read(c,&scope,targets,5,&r)==-ENOMEM&&!r&&resolves==0);}fail_after=-1;
    reset();sw_id dup[2]={id(10),id(10)};CHECK(sw_read(c,&scope,dup,2,&r)==-EINVAL&&!r&&resolves==0&&infos==0);
    CHECK(sw_read(c,&scope,targets,(size_t)SW_MAX_TARGETS+1,&r)==-E2BIG&&!r);sw_scope wrong=scope;wrong.os_uid=1001;CHECK(sw_read(c,&wrong,targets,5,&r)<0&&!r);
    wrong=scope;wrong.context.runtime_incarnation[15]^=1;CHECK(sw_read(c,&wrong,targets,5,&r)==-ESTALE&&!r);
    CHECK(sw_read(u,&scope,targets,5,&r)==-ESTALE&&!r);
    reset();CHECK(sw_read(c,&scope,NULL,0,&r)==0&&r&&!r->count&&!resolves);sw_result_free(&r);
    sw_scope copied=scope;sw_id copy[5];memcpy(copy,targets,sizeof copy);borrowed_scope=&copied;borrowed_targets=copy;reset();mutate_inputs=1;
    CHECK(sw_read(c,&copied,copy,5,&r)==0&&r&&r->scope.account[15]==2&&r->rows[2].install.bytes[15]==12&&copied.account[15]!=2);sw_result_free(&r);reset();
    if(change_runtime){
        uint8_t next[16];ids(next,17);CHECK(ae_catalogue_begin_runtime(c,scope.context.boot_id,next)==0&&sw_read(c,&scope,targets,5,&r)==-ESTALE&&!r);
        scope=scope_for(c);CHECK(sw_read(c,&scope,targets,5,&r)==0&&r&&r->scope.context.runtime_incarnation[15]==17);sw_result_free(&r);puts("EXPLICIT_ORIGINAL_RUNTIME_REPLACEMENT_OLD_SCOPE_REFUSED_OK");
    }
    CHECK(ae_catalogue_close(&u)==0&&ae_catalogue_close(&c)==0);puts("ORIGINAL_SELECTED_APP_OBSERVATIONS_READONLY_OK");
}
static void large(void){
    sw_scope scope={0};scope.identity.kind=AE_CATALOGUE_SYSTEM;ids(scope.identity.authority_epoch,1);ids(scope.context.authority_epoch,1);ids(scope.context.boot_id,4);ids(scope.context.runtime_incarnation,5);scope.context.revision=1;ids(scope.account,2);scope.os_uid=1000;
    size_t n=SW_MAX_TARGETS;sw_id *targets=calloc(n,sizeof(*targets));sw_result *r=calloc(1,sizeof(*r));CHECK(targets&&r);r->scope=scope;r->count=n;r->rows=calloc(n,sizeof(*r->rows));CHECK(r->rows);
    for(size_t i=0;i<n;++i){targets[n-i-1]=id((uint32_t)i+1000);r->rows[i].install=id((uint32_t)i+1000);r->rows[i].error=-ENOENT;}
    sw_owner *o=NULL;CHECK(sw_owner_create(&scope,targets,n,&o)==NX_OK&&sw_owner_expect(o,1)==NX_OK);
    r->rows[n-1].value.component[0]='x';CHECK(sw_owner_complete(o,1,0,&r)==NX_ERROR_ARGUMENT&&r&&sw_owner_pending(o)==1);r->rows[n-1].value.component[0]=0;
    CHECK(sw_owner_complete(o,1,0,&r)==NX_OK&&!r&&sw_owner_select(o,id((uint32_t)n+999))==NX_OK);sw_snapshot *s=NULL;CHECK(sw_owner_snapshot(o,&s)==NX_OK&&sw_snapshot_find(s,id((uint32_t)n+999))==n-1);
    sw_owner_destroy(&o);CHECK(sw_snapshot_data(s)->count==n);sw_snapshot_release(&s);free(targets);
    printf("DECLARED_MODEL_STORAGE rows=%zu targets=%zu row_size=%zu\n",n*sizeof(sw_row),n*sizeof(sw_id),sizeof(sw_row));
    puts("DECLARED_100000_TARGET_MODEL_AND_HIDDEN_INVALID_ROW_REJECTION_OK");
}
int main(int argc,char **argv){
    if(argc==2&&!strcmp(argv[1],"large")){large();return 0;}
    CHECK(argc==4);if(!strcmp(argv[1],"create"))bootstrap(argv[2],argv[3]);else {CHECK(!strcmp(argv[1],"test")||!strcmp(argv[1],"runtime"));read_checks(argv[2],argv[3],!strcmp(argv[1],"runtime"));}return 0;
}
