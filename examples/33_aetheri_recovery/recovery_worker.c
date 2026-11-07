#define _GNU_SOURCE
#include "recovery_worker.h"
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
struct rw_worker {
    char path[PATH_MAX];struct ae_catalogue_identity identity;struct ae_catalogue_info initial;
    pthread_t thread;pthread_mutex_t lock;pthread_cond_t changed;nx_sim_channel *channel;
    int wake[2],ready,initial_error,stop,joined,failed;size_t hints;
};
static void free_key(void *unused,void *data,size_t bytes){(void)unused;(void)bytes;free(data);}
static void free_reply(void *unused,void *data,size_t bytes){
    (void)unused;(void)bytes;rw_reply *r=data;if(r){ae_recovery_inventory_free(&r->inventory);free(r);}
}
static void failed(rw_worker *w){pthread_mutex_lock(&w->lock);w->failed=1;pthread_mutex_unlock(&w->lock);}
static void *run(void *p){
    rw_worker *w=p;struct ae_catalogue *catalogue=NULL;
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
        rw_reply *reply=calloc(1,sizeof(*reply));
        if(!reply)status=NX_ERROR_MEMORY;
        else if(message.bytes!=sizeof(rw_key)||!message.data){free(reply);reply=NULL;status=NX_ERROR_ARGUMENT;}
        else {
            reply->key=*(const rw_key *)message.data;
            reply->original_error=stopping?-ECANCELED:ae_catalogue_recovery_inventory(catalogue,AE_RECOVERY_MAX_RECORDS,&reply->inventory);
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
nx_result rw_start(const char *path,const struct ae_catalogue_identity *identity,rw_worker **out,struct ae_catalogue_info *context,int *original_error){
    if(!out||*out||!context||!original_error)return NX_ERROR_ARGUMENT;
    *original_error=0;
    if(!path||!identity||!path[0]||strlen(path)>=PATH_MAX)return NX_ERROR_ARGUMENT;
    rw_worker *w=calloc(1,sizeof(*w));if(!w)return NX_ERROR_MEMORY;
    strcpy(w->path,path);w->identity=*identity;w->wake[0]=w->wake[1]=-1;
    int mutex=0,cond=0,started=0;nx_result r=NX_ERROR_PLATFORM;
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
    free(w);return r;
}
nx_result rw_submit(rw_worker *w,rw_key key,uint64_t *ticket){
    if(!ticket)return NX_ERROR_ARGUMENT;
    *ticket=0;if(!w||!key.view||!key.request)return NX_ERROR_ARGUMENT;
    rw_key *copy=malloc(sizeof(*copy));if(!copy)return NX_ERROR_MEMORY;*copy=key;
    pthread_mutex_lock(&w->lock);
    nx_result r=w->stop||w->failed?NX_ERROR_STATE:nx_sim_channel_post_command(w->channel,copy,sizeof(*copy),free_key,NULL,ticket);
    if(r==NX_OK){++w->hints;pthread_cond_signal(&w->changed);}
    pthread_mutex_unlock(&w->lock);if(r!=NX_OK)free(copy);return r;
}
int rw_wake_fd(const rw_worker *w){return w?w->wake[0]:-1;}
nx_result rw_receive(rw_worker *w,nx_sim_message *message){
    if(!message)return NX_ERROR_ARGUMENT;
    *message=(nx_sim_message){0};if(!w)return NX_ERROR_ARGUMENT;
    char bytes[64];ssize_t n;do{n=read(w->wake[0],bytes,sizeof bytes);}while(n>0||(n<0&&errno==EINTR));
    if(n>=0||(errno!=EAGAIN&&errno!=EWOULDBLOCK))return NX_ERROR_PLATFORM;
    nx_result r=nx_sim_channel_take_completion(w->channel,message);
    if(r==NX_OK&&!message->ticket){pthread_mutex_lock(&w->lock);int bad=w->failed;pthread_mutex_unlock(&w->lock);if(bad)r=NX_ERROR_STATE;}return r;
}
rw_reply *rw_reply_get(const nx_sim_message *message){
    if(!message||!message->ticket||message->status!=NX_OK||message->bytes!=sizeof(rw_reply)||!message->data)return NULL;
    rw_reply *r=message->data;
    return r->key.view&&r->key.request&&r->original_error<=0&&((r->original_error==0)==(r->inventory!=NULL))?r:NULL;
}
nx_result rw_reply_take(rw_reply *reply,struct ae_recovery_inventory **out){
    if(!out||*out)return NX_ERROR_ARGUMENT;
    if(!reply||reply->original_error||!reply->inventory)return NX_ERROR_STATE;
    *out=reply->inventory;reply->inventory=NULL;return NX_OK;
}
nx_result rw_stop(rw_worker *w){
    if(!w)return NX_ERROR_ARGUMENT;
    pthread_mutex_lock(&w->lock);w->stop=1;pthread_cond_signal(&w->changed);pthread_mutex_unlock(&w->lock);return NX_OK;
}
nx_result rw_join(rw_worker *w){
    if(!w)return NX_ERROR_ARGUMENT;
    pthread_mutex_lock(&w->lock);int stop=w->stop;pthread_mutex_unlock(&w->lock);
    if(!stop)return NX_ERROR_STATE;
    if(!w->joined){if(pthread_join(w->thread,NULL))return NX_ERROR_PLATFORM;w->joined=1;}
    return w->failed?NX_ERROR_STATE:NX_OK;
}
nx_result rw_destroy(rw_worker **value){
    if(!value||!*value)return NX_ERROR_ARGUMENT;
    rw_worker *w=*value;(void)rw_stop(w);nx_result r=rw_join(w);if(!w->joined)return r;
    nx_result destroyed=nx_sim_channel_destroy(w->channel);if(destroyed!=NX_OK)return destroyed;
    close(w->wake[0]);close(w->wake[1]);pthread_cond_destroy(&w->changed);pthread_mutex_destroy(&w->lock);free(w);*value=NULL;return r;
}
