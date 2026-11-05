#include "view.h"
#include <stdio.h>
#include <stdlib.h>
#define CHECK(x) do {if(!(x)){fprintf(stderr,"%s:%d: %s\n",__FILE__,__LINE__,#x);return 1;}} while(0)
int main(int argc,char **argv) {
    CHECK(argc==2);FILE *f=fopen(argv[1],"rb");CHECK(f && !fseek(f,0,SEEK_END));long n=ftell(f);
    CHECK(n>0 && !fseek(f,0,SEEK_SET));void *bytes=malloc((size_t)n);CHECK(bytes && fread(bytes,1,(size_t)n,f)==(size_t)n);CHECK(!fclose(f));
    struct ae_desktop_model model;ae_desktop_model_init(&model);aetheri_desktop_view *view=NULL;
    CHECK(aetheri_desktop_view_create(&model,bytes,(size_t)n,&view));aetheri_desktop_intent intent;
    CHECK(aetheri_desktop_view_frame(view,640,84,&intent) && intent.kind==AETHERI_DESKTOP_NONE);
    nx_context *ctx=aetheri_desktop_view_context(view);CHECK(nx_semantics_request_action(ctx,11,NX_ACTION_PRESS)==NX_OK);
    CHECK(aetheri_desktop_view_frame(view,640,84,&intent) && intent.kind==AETHERI_DESKTOP_LAUNCH && intent.app==AE_DESKTOP_SETTINGS);
    CHECK(!model.pending[AE_DESKTOP_SETTINGS] && !model.process[AE_DESKTOP_SETTINGS]);
    CHECK(ae_desktop_model_request(&model,AE_DESKTOP_SETTINGS));
    CHECK(aetheri_desktop_view_frame(view,640,84,&intent) && intent.kind==AETHERI_DESKTOP_NONE);
    CHECK(nx_semantics_request_action(ctx,11,NX_ACTION_PRESS)==NX_ERROR_STATE);
    CHECK(nx_focus_set(ctx,12)==NX_OK);
    nx_event key={0};key.type=NX_EVENT_KEY_DOWN;key.key=NX_KEY_ENTER;
    CHECK(nx_context_queue_event(ctx,&key)==NX_OK);key.type=NX_EVENT_KEY_UP;
    CHECK(nx_context_queue_event(ctx,&key)==NX_OK);
    CHECK(aetheri_desktop_view_frame(view,640,84,&intent) && intent.kind==AETHERI_DESKTOP_LAUNCH && intent.app==AE_DESKTOP_TERMINAL);
    CHECK(!model.pending[AE_DESKTOP_TERMINAL]);
    CHECK(nx_semantics_request_action(ctx,10,NX_ACTION_PRESS)==NX_OK);
    CHECK(aetheri_desktop_view_frame(view,640,84,&intent) && intent.kind==AETHERI_DESKTOP_SEARCH && !model.search);
    aetheri_desktop_view_destroy(view);free(bytes);return 0;
}
