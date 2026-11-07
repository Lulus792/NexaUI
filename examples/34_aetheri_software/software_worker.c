#define _GNU_SOURCE
#include "software_worker.h"
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
typedef struct sww_job { sww_key key; sw_scope scope; size_t count; sw_id targets[]; } sww_job;
struct sww_worker {
    char path[PATH_MAX];struct ae_catalogue_identity identity;struct ae_catalogue_info initial;
    pthread_t thread;pthread_mutex_t lock;pthread_cond_t changed;nx_sim_channel *channel;
    int wake[2],ready,initial_error,stop,joined,failed;size_t hints;
    sw_manifest_source *sources;size_t source_count;
};
static void free_job(void *unused,void *p,size_t n){(void)unused;(void)n;free(p);}
static void free_reply(void *unused,void *p,size_t n){(void)unused;(void)n;sww_reply *r=p;if(r){sw_components_free(&r->components);sw_result_free(&r->result);free(r);}}
static void free_sources(sww_worker *w){for(size_t i=0;i<w->source_count;++i)free((void *)w->sources[i].path);free(w->sources);}
static void failed(sww_worker *w){pthread_mutex_lock(&w->lock);w->failed=1;pthread_mutex_unlock(&w->lock);}
static void *run(void *p){
    sww_worker *w=p;struct ae_catalogue *catalogue=NULL;
    int error=ae_catalogue_open(w->path,&w->identity,&catalogue);
    if(!error)error=ae_catalogue_get_info(catalogue,&w->initial);
    pthread_mutex_lock(&w->lock);w->initial_error=error;w->ready=1;pthread_cond_signal(&w->changed);pthread_mutex_unlock(&w->lock);
    if(error)goto done;
    for(;;){
        pthread_mutex_lock(&w->lock);
        while(!w->hints&&!w->stop)pthread_cond_wait(&w->changed,&w->lock);
        int stopping=w->stop;
        if(!w->hints&&stopping){pthread_mutex_unlock(&w->lock);break;}
        --w->hints;pthread_mutex_unlock(&w->lock);
        nx_sim_message message={0};nx_result status=nx_sim_channel_take_command(w->channel,&message);
        if(status!=NX_OK||!message.ticket){failed(w);break;}
        sww_reply *reply=calloc(1,sizeof(*reply));
        if(!reply)status=NX_ERROR_MEMORY;
        else {
            const sww_job *job=message.data;
            if(!job||message.bytes<sizeof(*job)||job->count>SW_MAX_TARGETS||
                job->count>(SIZE_MAX-sizeof(*job))/sizeof(sw_id)||message.bytes!=sizeof(*job)+job->count*sizeof(sw_id)){
                free(reply);reply=NULL;status=NX_ERROR_ARGUMENT;
            }else {
                reply->key=job->key;
                reply->original_error=stopping?-ECANCELED:sw_read(catalogue,&job->scope,job->targets,job->count,&reply->result);
                if(!reply->original_error&&w->source_count)reply->components_status=sw_components_read(reply->result,w->sources,w->source_count,&reply->components);
            }
        }
        uint64_t ticket=message.ticket;nx_sim_message_dispose(&message);
        nx_result posted=nx_sim_channel_post_completion(w->channel,ticket,status,reply,reply?sizeof(*reply):0,reply?free_reply:NULL,NULL);
        if(posted!=NX_OK)free_reply(NULL,reply,0);
        ssize_t n;do{n=write(w->wake[1],"W",1);}while(n<0&&errno==EINTR);
        if(posted!=NX_OK||(n<0&&errno!=EAGAIN&&errno!=EWOULDBLOCK))failed(w);
    }
done:
    if(catalogue&&ae_catalogue_close(&catalogue))failed(w);
    return NULL;
}
nx_result sww_start(const char *path,const struct ae_catalogue_identity *identity,sww_worker **out,struct ae_catalogue_info *context,int *original_error){
    return sww_start_with_manifests(path,identity,NULL,0,out,context,original_error);
}
nx_result sww_start_with_manifests(const char *path,const struct ae_catalogue_identity *identity,const sw_manifest_source *sources,size_t count,sww_worker **out,struct ae_catalogue_info *context,int *original_error){
    if(!out||*out||!context||!original_error)return NX_ERROR_ARGUMENT;
    *original_error=0;
    if(!path||!identity||identity->kind!=AE_CATALOGUE_SYSTEM||path[0]!='/'||strlen(path)>=PATH_MAX||(count&&!sources))return NX_ERROR_ARGUMENT;
    if(count>SW_MAX_TARGETS||count>SIZE_MAX/sizeof(*sources))return NX_ERROR_LIMIT;
    for(size_t i=0;i<count;++i)if(!sources[i].path||sources[i].path[0]!='/'||strlen(sources[i].path)>=PATH_MAX||
        (sources[i].install.bytes[6]&0xf0)!=0x40||(sources[i].install.bytes[8]&0xc0)!=0x80)return NX_ERROR_ARGUMENT;
    sww_worker *w=calloc(1,sizeof(*w));if(!w)return NX_ERROR_MEMORY;
    strcpy(w->path,path);w->identity=*identity;w->wake[0]=w->wake[1]=-1;
    int mutex=0,cond=0,started=0;nx_result r=NX_ERROR_MEMORY;
    if(count){w->sources=calloc(count,sizeof(*w->sources));if(!w->sources)goto fail;w->source_count=count;
        for(size_t i=0;i<count;++i){w->sources[i].install=sources[i].install;w->sources[i].path=strdup(sources[i].path);if(!w->sources[i].path)goto fail;}}
    r=NX_ERROR_PLATFORM;
    if(pipe2(w->wake,O_CLOEXEC|O_NONBLOCK))goto fail;
    if(pthread_mutex_init(&w->lock,NULL))goto fail;
    mutex=1;
    if(pthread_cond_init(&w->changed,NULL))goto fail;
    cond=1;r=nx_sim_channel_create(2,NULL,&w->channel);if(r!=NX_OK)goto fail;
    r=NX_ERROR_PLATFORM;if(pthread_create(&w->thread,NULL,run,w))goto fail;
    started=1;pthread_mutex_lock(&w->lock);
    while(!w->ready)pthread_cond_wait(&w->changed,&w->lock);
    *original_error=w->initial_error;pthread_mutex_unlock(&w->lock);
    if(*original_error)goto fail;
    *context=w->initial;*out=w;return NX_OK;
fail:
    if(started){if(pthread_join(w->thread,NULL))return NX_ERROR_PLATFORM;}
    if(w->channel)(void)nx_sim_channel_destroy(w->channel);
    if(cond)pthread_cond_destroy(&w->changed);
    if(mutex)pthread_mutex_destroy(&w->lock);
    if(w->wake[0]>=0)close(w->wake[0]);
    if(w->wake[1]>=0)close(w->wake[1]);
    free_sources(w);free(w);return r;
}
nx_result sww_submit(sww_worker *w,sww_key key,const sw_scope *scope,const sw_id *targets,size_t count,uint64_t *ticket){
    if(!ticket)return NX_ERROR_ARGUMENT;
    *ticket=0;if(!w||!key.view||!key.request||!scope||(count&&!targets))return NX_ERROR_ARGUMENT;
    if(count>SW_MAX_TARGETS||count>(SIZE_MAX-sizeof(sww_job))/sizeof(sw_id))return NX_ERROR_LIMIT;
    size_t bytes=sizeof(sww_job)+count*sizeof(sw_id);sww_job *copy=malloc(bytes);if(!copy)return NX_ERROR_MEMORY;
    copy->key=key;copy->scope=*scope;copy->count=count;if(count)memcpy(copy->targets,targets,count*sizeof(*targets));
    pthread_mutex_lock(&w->lock);
    nx_result r=w->stop||w->failed?NX_ERROR_STATE:nx_sim_channel_post_command(w->channel,copy,bytes,free_job,NULL,ticket);
    if(r==NX_OK){++w->hints;pthread_cond_signal(&w->changed);}
    pthread_mutex_unlock(&w->lock);if(r!=NX_OK)free(copy);return r;
}
int sww_wake_fd(const sww_worker *w){return w?w->wake[0]:-1;}
nx_result sww_receive(sww_worker *w,nx_sim_message *message){
    if(!message)return NX_ERROR_ARGUMENT;
    *message=(nx_sim_message){0};if(!w)return NX_ERROR_ARGUMENT;
    char bytes[64];ssize_t n;do{n=read(w->wake[0],bytes,sizeof bytes);}while(n>0||(n<0&&errno==EINTR));
    if(n>=0||(errno!=EAGAIN&&errno!=EWOULDBLOCK))return NX_ERROR_PLATFORM;
    nx_result r=nx_sim_channel_take_completion(w->channel,message);
    if(r==NX_OK&&!message->ticket){pthread_mutex_lock(&w->lock);int bad=w->failed;pthread_mutex_unlock(&w->lock);if(bad)r=NX_ERROR_STATE;}return r;
}
sww_reply *sww_reply_get(const nx_sim_message *message){
    if(!message||!message->ticket||message->status!=NX_OK||message->bytes!=sizeof(sww_reply)||!message->data)return NULL;
    sww_reply *r=message->data;
    return r->key.view&&r->key.request&&r->original_error<=0&&((r->original_error==0)==(r->result!=NULL))&&
        (!r->original_error||(!r->components&&r->components_status==NX_OK))&&
        (r->components_status==NX_OK||!r->components)?r:NULL;
}
nx_result sww_reply_take(sww_reply *reply,sw_result **out){
    if(!out||*out)return NX_ERROR_ARGUMENT;
    if(!reply||reply->original_error||!reply->result)return NX_ERROR_STATE;
    *out=reply->result;reply->result=NULL;return NX_OK;
}
nx_result sww_stop(sww_worker *w){if(!w)return NX_ERROR_ARGUMENT;pthread_mutex_lock(&w->lock);w->stop=1;pthread_cond_signal(&w->changed);pthread_mutex_unlock(&w->lock);return NX_OK;}
nx_result sww_join(sww_worker *w){
    if(!w)return NX_ERROR_ARGUMENT;
    pthread_mutex_lock(&w->lock);int stop=w->stop;pthread_mutex_unlock(&w->lock);
    if(!stop)return NX_ERROR_STATE;
    if(!w->joined){if(pthread_join(w->thread,NULL))return NX_ERROR_PLATFORM;w->joined=1;}
    return w->failed?NX_ERROR_STATE:NX_OK;
}
nx_result sww_destroy(sww_worker **value){
    if(!value||!*value)return NX_ERROR_ARGUMENT;
    sww_worker *w=*value;(void)sww_stop(w);nx_result r=sww_join(w);if(!w->joined)return r;
    nx_result destroyed=nx_sim_channel_destroy(w->channel);if(destroyed!=NX_OK)return destroyed;
    close(w->wake[0]);close(w->wake[1]);pthread_cond_destroy(&w->changed);pthread_mutex_destroy(&w->lock);free_sources(w);free(w);*value=NULL;return r;
}
