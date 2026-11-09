#include "settings/nexaui_view.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(x) do {if(!(x)){fprintf(stderr,"original sound/view:%d: %s\n",__LINE__,#x);goto done;}} while(0)

static bool frame(struct ae_settings_view *view,float width,float height) {
    const nx_window_status status={0};
    bool changed=false;
    return ae_settings_view_frame(view,width,height,&status,&changed);
}

static bool semantic(nx_context *context,uint64_t id,nx_semantic_view *out) {
    uint32_t index=0;
    return nx_semantics_find(context,id,&index)==NX_OK && nx_semantics_get(context,index,out)==NX_OK;
}

static bool key(nx_context *context,nx_key code) {
    nx_event event={.type=NX_EVENT_KEY_DOWN,.key=code};
    if(nx_context_queue_event(context,&event)!=NX_OK)return false;
    event.type=NX_EVENT_KEY_UP;
    return nx_context_queue_event(context,&event)==NX_OK;
}

int main(int argc,char **argv) {
    int result=1;
    FILE *file=NULL;
    unsigned char *font=NULL;
    struct ae_settings_view *view=NULL;
    struct ae_settings model;
    ae_settings_init(&model);
    model.page=AE_SETTINGS_SOUND;
    struct ae_audio_snapshot observed={.revision=7,.state=AE_AUDIO_READY,.count=AE_AUDIO_OUTPUTS_MAX};
    for(size_t i=0;i<observed.count;++i) {
        struct ae_audio_output *output=&observed.outputs[i];
        output->ref=(struct ae_audio_output_ref){33,101+i,(uint32_t)i+1};
        (void)snprintf(output->name,sizeof(output->name),"output-%zu",i+1);
        (void)snprintf(output->description,sizeof(output->description),"Audio output %zu",i+1);
        output->is_default=i==0;output->volume_available=true;output->volume=0.6;
    }
    struct ae_audio_output *last=&observed.outputs[observed.count-1];
    /* The original model accepts 511 description bytes. Keep the full name in
     * semantics even when the original view wraps it over many painted lines. */
    for(size_t i=0;i<sizeof(last->description)-1;++i)last->description[i]="Long output name "[i%17];
    last->description[sizeof(last->description)-1]=0;
    CHECK(ae_settings_sound_snapshot(&model,&observed,0));
    uint64_t first_id=model.sound.ids[0],last_id=model.sound.ids[observed.count-1];
    CHECK(argc==2 && (file=fopen(argv[1],"rb")) && !fseek(file,0,SEEK_END));
    long length=ftell(file);
    CHECK(length>0 && length<64*1024*1024 && !fseek(file,0,SEEK_SET));
    font=malloc((size_t)length);
    CHECK(font && fread(font,1,(size_t)length,file)==(size_t)length);
    int closed=fclose(file);file=NULL;CHECK(!closed);
    CHECK(ae_settings_view_create(&model,font,(size_t)length,&view) && frame(view,1000,720));
    nx_context *context=ae_settings_view_context(view);
    nx_semantic_view item;
    for(size_t i=0;i<observed.count;++i) {
        CHECK(semantic(context,model.sound.ids[i],&item) && item.role==NX_ROLE_RADIO);
        CHECK(item.name.length==strlen(observed.outputs[i].description) &&
            !memcmp(item.name.data,observed.outputs[i].description,item.name.length));
    }
    CHECK(nx_focus_set(context,last_id)==NX_OK && frame(view,1000,720));
    CHECK(semantic(context,last_id,&item) && item.bounds.y<720 && item.bounds.y+item.bounds.height>0);
    CHECK(key(context,NX_KEY_SPACE) && frame(view,1000,720));
    CHECK(model.sound.queued && model.sound.request.kind==AE_AUDIO_DEFAULT &&
        model.sound.request.target.id==AE_AUDIO_OUTPUTS_MAX && model.sound.request.revision==7);
    CHECK(model.sound.observation.outputs[0].is_default && !model.sound.observation.outputs[observed.count-1].is_default);
    model.german=true;model.text_percent=200;model.theme=AE_SETTINGS_CONTRAST;
    ae_settings_view_destroy(view);view=NULL;
    CHECK(ae_settings_view_create(&model,font,(size_t)length,&view) && frame(view,320,320));
    CHECK(model.sound.queued && model.sound.ids[0]==first_id && model.sound.ids[observed.count-1]==last_id);
    struct ae_settings_sound_request request;
    CHECK(ae_settings_sound_take(&model,&request) && request.kind==AE_AUDIO_DEFAULT &&
        request.target.connection==33 && request.target.serial==101+AE_AUDIO_OUTPUTS_MAX-1 &&
        request.target.id==AE_AUDIO_OUTPUTS_MAX && request.revision==7);
    CHECK(!ae_settings_sound_take(&model,&request));
    ae_settings_sound_result(&model,AE_AUDIO_SUBMITTED);
    CHECK(frame(view,320,320));context=ae_settings_view_context(view);
    CHECK(semantic(context,last_id,&item) && (item.state&NX_SEM_DISABLED));
    /* Reorder complete observations while the original owner supplies the exact
     * command outcome. No UI action may acknowledge or replay its own intent. */
    struct ae_audio_output swap=observed.outputs[0];observed.outputs[0]=*last;*last=swap;
    observed.outputs[0].is_default=true;last->is_default=false;
    ++observed.revision;observed.command=1;observed.command_kind=AE_AUDIO_DEFAULT;
    observed.command_target=request.target;observed.command_state=AE_AUDIO_COMMAND_OBSERVED;
    CHECK(ae_settings_sound_snapshot(&model,&observed,0));
    CHECK(model.sound.ids[0]==last_id && model.sound.ids[observed.count-1]==first_id);
    CHECK(frame(view,640,600) && model.sound.feedback==AE_SETTINGS_SOUND_OBSERVED && !model.sound.queued);
    CHECK(ae_settings_sound_snapshot(&model,NULL,8) && frame(view,640,600));
    CHECK(model.sound.fenced && model.sound.loss==8 && !model.sound.queued);
    CHECK(!ae_settings_sound_request(&model,last_id,AE_AUDIO_MUTE,0,true));
    CHECK(!ae_settings_sound_snapshot(&model,&observed,0));
    puts("Original Settings/Sound: maximum outputs/names, keyboard intent, view replacement, observed result, reorder and loss fence");
    result=0;
done:
    if(file)fclose(file);
    ae_settings_view_destroy(view);free(font);return result;
}
