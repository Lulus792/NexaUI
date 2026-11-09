#define _POSIX_C_SOURCE 200809L
/* Test-only observations/effects. No service admission, device backend or
 * production entry point. Used only inside the private Weston fixture. */
#include "settings/nexaui_main.h"
#include "settings/audio_host.h"
#include "audio/session_control.h"
#include <glib-unix.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

struct ae_audio_session_control {
    GMainContext *context;
    GThread *thread;
    struct ae_audio_session_snapshot snapshot;
    unsigned effects;
    double volume;
    bool mute;
};
static bool owned(struct ae_audio_session_control *s) {
    return s && s->thread==g_thread_self() && g_main_context_get_thread_default()==s->context;
}
bool ae_audio_session_control_snapshot(struct ae_audio_session_control *s,struct ae_audio_session_snapshot *out) {
    if(!owned(s) || !out)return false;
    *out=s->snapshot;return true;
}
bool ae_audio_session_control_poll(struct ae_audio_session_control *s,struct pollfd out[4],size_t *count,uint64_t *deadline) {
    (void)out;
    if(!owned(s) || !count || !deadline)return false;
    *count=0;*deadline=UINT64_MAX;return true;
}
static enum ae_audio_submit submit(struct ae_audio_session_control *s,uint64_t revision,
        struct ae_audio_output_ref ref,enum ae_audio_command_kind kind,double volume,bool mute) {
    if(!owned(s) || s->snapshot.loss)return AE_AUDIO_UNAVAILABLE;
    struct ae_audio_snapshot *audio=&s->snapshot.audio;
    if(revision!=audio->revision)return AE_AUDIO_STALE;
    if(audio->command_state==AE_AUDIO_COMMAND_PENDING)return AE_AUDIO_BUSY;
    for(size_t i=0;i<audio->count;++i) {
        struct ae_audio_output_ref current=audio->outputs[i].ref;
        if(ref.connection!=current.connection || ref.serial!=current.serial || ref.id!=current.id)continue;
        ++s->effects;audio->command=s->effects;audio->command_kind=kind;audio->command_target=ref;
        audio->command_state=AE_AUDIO_COMMAND_PENDING;++audio->revision;s->volume=volume;s->mute=mute;
        printf("CONTROLLED_SUBMIT %u %d %u %.9f %d\n",s->effects,(int)kind,ref.id,volume,mute);fflush(stdout);
        return AE_AUDIO_SUBMITTED;
    }
    return AE_AUDIO_STALE;
}
enum ae_audio_submit ae_audio_session_control_volume(struct ae_audio_session_control *s,uint64_t revision,struct ae_audio_output_ref ref,double value) {
    return submit(s,revision,ref,AE_AUDIO_VOLUME,value,false);
}
enum ae_audio_submit ae_audio_session_control_mute(struct ae_audio_session_control *s,uint64_t revision,struct ae_audio_output_ref ref,bool value) {
    return submit(s,revision,ref,AE_AUDIO_MUTE,0,value);
}
enum ae_audio_submit ae_audio_session_control_default(struct ae_audio_session_control *s,uint64_t revision,struct ae_audio_output_ref ref) {
    return submit(s,revision,ref,AE_AUDIO_DEFAULT,0,false);
}
static gboolean control(gint fd,GIOCondition condition,gpointer data) {
    struct ae_audio_session_control *s=data;
    if(!owned(s) || !(condition&G_IO_IN))return G_SOURCE_REMOVE;
    char command[32],reply[256];ssize_t n=recv(fd,command,sizeof(command)-1,MSG_DONTWAIT);
    if(n<=0)return G_SOURCE_REMOVE;
    command[n]=0;struct ae_audio_snapshot *audio=&s->snapshot.audio;
    if(!strcmp(command,"STATE")) {
        (void)snprintf(reply,sizeof(reply),"STATE %u %d %u %.9f %d %u",s->effects,
            (int)audio->command_state,s->snapshot.loss,audio->outputs[0].volume,
            audio->outputs[0].muted,audio->command_target.id);
    } else if(!strcmp(command,"ACK") && !s->snapshot.loss && audio->command_state==AE_AUDIO_COMMAND_PENDING) {
        for(size_t i=0;i<audio->count;++i) {
            struct ae_audio_output *output=&audio->outputs[i];
            bool target=output->ref.id==audio->command_target.id;
            if(audio->command_kind==AE_AUDIO_DEFAULT)output->is_default=target;
            if(target && audio->command_kind==AE_AUDIO_VOLUME)output->volume=s->volume;
            if(target && audio->command_kind==AE_AUDIO_MUTE)output->muted=s->mute;
        }
        audio->command_state=AE_AUDIO_COMMAND_OBSERVED;++audio->revision;strcpy(reply,"OK");
    } else if(!strcmp(command,"LOSS")) {
        s->snapshot.loss=AE_AUDIO_CONTROL_SESSION_LOST;
        if(audio->command_state==AE_AUDIO_COMMAND_PENDING)audio->command_state=AE_AUDIO_COMMAND_UNKNOWN;
        ++audio->revision;strcpy(reply,"OK");
    } else if(!strcmp(command,"RECOVER")) {
        s->snapshot.loss=0;++audio->revision;strcpy(reply,"OK");
    } else strcpy(reply,"ERROR");
    if(strcmp(command,"STATE")) {printf("CONTROLLED_OBSERVE %s\n",command);fflush(stdout);}
    if(send(fd,reply,strlen(reply),MSG_NOSIGNAL)!=(ssize_t)strlen(reply))return G_SOURCE_REMOVE;
    return G_SOURCE_CONTINUE;
}
int main(int argc,char **argv) {
    const char *private=getenv("NEXAUI_TEST_PRIVATE_WAYLAND"),*display=getenv("WAYLAND_DISPLAY");
    const char *input=getenv("NEXAUI_SETTINGS_AUDIO_CONTROL_FD");char *end=NULL;
    if(!private || strcmp(private,"1") || !display || strcmp(display,"nexaui-test") || !input)return 2;
    errno=0;long number=strtol(input,&end,10);
    if(errno || !end || *end || number<0 || number>INT_MAX)return 2;
    int fd=(int)number,type=0;socklen_t size=sizeof(type);
    if(getsockopt(fd,SOL_SOCKET,SO_TYPE,&type,&size) || type!=SOCK_SEQPACKET)return 2;
    struct ae_audio_session_control owner={0};owner.context=g_main_context_new();
    g_main_context_push_thread_default(owner.context);owner.thread=g_thread_self();
    owner.snapshot.audio=(struct ae_audio_snapshot){.revision=7,.state=AE_AUDIO_READY,.count=2};
    owner.snapshot.audio.outputs[0]=(struct ae_audio_output){.ref={33,101,4},.name="speakers",
        .description="Speakers",.is_default=true,.volume_available=true,.volume=0.6};
    owner.snapshot.audio.outputs[1]=(struct ae_audio_output){.ref={33,102,5},.name="headphones",
        .description="Headphones",.volume_available=true,.volume=0.8};
    GSource *source=g_unix_fd_source_new(fd,G_IO_IN|G_IO_HUP|G_IO_ERR);
    g_source_set_callback(source,G_SOURCE_FUNC(control),&owner,NULL);
    struct ae_settings_audio_host *host=ae_settings_audio_host_open(owner.context,&owner);
    int result=1;
    if(host && g_source_attach(source,owner.context))result=ae_settings_nexaui_run(argc,argv,host);
    if(host && !ae_settings_audio_host_close(&host))result=1;
    bool pending=getenv("NEXAUI_AUDIO_CLOSE_PENDING")!=NULL;
    if(pending && (owner.effects!=1 || owner.snapshot.audio.command_state!=AE_AUDIO_COMMAND_PENDING ||
                  owner.snapshot.audio.outputs[0].volume!=0.6))result=1;
    if(!pending && (owner.effects!=3 || owner.snapshot.audio.command_state!=AE_AUDIO_COMMAND_UNKNOWN ||
                   !owner.snapshot.audio.outputs[0].is_default))result=1;
    printf("CONTROLLED_OWNER_AFTER_WINDOW %u %d\n",owner.effects,(int)owner.snapshot.audio.command_state);
    g_source_destroy(source);g_source_unref(source);close(fd);
    g_main_context_pop_thread_default(owner.context);g_main_context_unref(owner.context);
    printf("CONTROLLED_RETIRED %u %d\n",owner.effects,result);fflush(stdout);return result;
}
