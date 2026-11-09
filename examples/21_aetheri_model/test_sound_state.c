#include "view.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(x) do {if(!(x)){fprintf(stderr,"sound/view:%d: %s\n",__LINE__,#x);goto done;}} while(0)

static int activate(aetheri_view *view,uint64_t id) {
    nx_context *context=aetheri_view_context(view);
    nx_event event={.type=NX_EVENT_KEY_DOWN,.key=NX_KEY_SPACE};
    if(nx_focus_set(context,id)!=NX_OK || nx_context_queue_event(context,&event)!=NX_OK)return 0;
    event.type=NX_EVENT_KEY_UP;
    int changed=0;
    return nx_context_queue_event(context,&event)==NX_OK &&
        aetheri_view_frame(view,800,480,&changed) && changed;
}

int main(int argc,char **argv) {
    int result=1,changed=0;
    FILE *file=NULL;
    unsigned char *font=NULL;
    aetheri_view *view=NULL;
    struct ae_settings model;
    ae_settings_init(&model);
    struct ae_audio_snapshot observed={.state=AE_AUDIO_READY,.revision=7,.count=1};
    observed.outputs[0]=(struct ae_audio_output){.ref={33,101,4},.name="speakers",
        .description="Speakers",.volume=0.6,.volume_available=true,.is_default=true};
    CHECK(ae_settings_sound_snapshot(&model,&observed,0));
    uint64_t original_id=model.sound.ids[0];
    CHECK(original_id && ae_settings_sound_request(&model,original_id,AE_AUDIO_VOLUME,0.4,false));
    CHECK(argc==2 && (file=fopen(argv[1],"rb")) && !fseek(file,0,SEEK_END));
    long length=ftell(file);
    CHECK(length>0 && length<64*1024*1024 && !fseek(file,0,SEEK_SET));
    font=malloc((size_t)length);
    CHECK(font && fread(font,1,(size_t)length,file)==(size_t)length);
    int closed=fclose(file);file=NULL;CHECK(!closed);
    CHECK(aetheri_view_create(&model,font,(size_t)length,&view));
    CHECK(aetheri_view_frame(view,800,480,&changed));
    CHECK(model.sound.queued && model.sound.observation.revision==7 && model.sound.ids[0]==original_id);
    CHECK(activate(view,12) && activate(view,14));
    CHECK(model.theme==AE_SETTINGS_CONTRAST && model.german);
    aetheri_view_destroy(view);view=NULL;
    CHECK(aetheri_view_create(&model,font,(size_t)length,&view));
    CHECK(aetheri_view_frame(view,640,320,&changed));
    CHECK(model.sound.queued && model.sound.observation.outputs[0].volume==0.6 &&
        model.sound.ids[0]==original_id && model.theme==AE_SETTINGS_CONTRAST && model.german);
    struct ae_settings_sound_request request;
    CHECK(ae_settings_sound_take(&model,&request) && request.kind==AE_AUDIO_VOLUME &&
        request.revision==7 && request.target.connection==33 && request.target.serial==101 &&
        request.target.id==4 && request.volume==0.4 && !request.mute);
    CHECK(!ae_settings_sound_take(&model,&request));
    ae_settings_sound_result(&model,AE_AUDIO_STALE);
    CHECK(model.sound.feedback==AE_SETTINGS_SOUND_REJECTED);
    CHECK(ae_settings_sound_snapshot(&model,NULL,8) && model.sound.fenced);
    aetheri_view_destroy(view);view=NULL;
    CHECK(aetheri_view_create(&model,font,(size_t)length,&view));
    CHECK(aetheri_view_frame(view,800,480,&changed));
    CHECK(model.sound.fenced && model.sound.loss==8 && !model.sound.queued &&
        !ae_settings_sound_request(&model,original_id,AE_AUDIO_MUTE,0,true) &&
        !ae_settings_sound_snapshot(&model,&observed,0));
    puts("Original sound observation/identity/once-only intent and permanent loss survive Settings view replacement without device effects");
    result=0;
done:
    if(file)fclose(file);
    aetheri_view_destroy(view);free(font);return result;
}
