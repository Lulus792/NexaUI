#include "scene.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"%s:%d: %s\n",__FILE__,__LINE__,#x); goto fail; } } while (0)
static int key(nx_context *ui, nx_key k, uint32_t mod) {
    nx_event e={0}; e.type=NX_EVENT_KEY_DOWN; e.key=k; e.modifiers=mod;
    if (nx_context_queue_event(ui,&e)!=NX_OK) return 0;
    e.type=NX_EVENT_KEY_UP; return nx_context_queue_event(ui,&e)==NX_OK;
}
static int type(nx_context *ui, const char *str) {
    for (; *str; ++str) { nx_event e={0}; e.type=NX_EVENT_TEXT; e.scalar=(unsigned char)*str;
        if (nx_context_queue_event(ui,&e)!=NX_OK) return 0; }
    return 1;
}
typedef struct example_service { settings_apply_request request; int starts,queries,result; } example_service;
static int service_submit(example_service *service,const settings_apply_request *request) {
    if(!service || !request || !request->token || service->starts)return 0;
    service->request=*request;++service->starts;service->result=-1;return 1;
}
static int service_query(example_service *service,uint64_t token,int *result) {
    if(!service || !result || token!=service->request.token)return 0;
    ++service->queries;*result=service->result;return 1;
}
int main(int argc,char **argv) {
    FILE *f=NULL; unsigned char *bytes=NULL; settings_scene *s=NULL; settings_session *session=NULL; long length;
    CHECK(argc==2); f=fopen(argv[1],"rb"); CHECK(f);
    CHECK(fseek(f,0,SEEK_END)==0 && (length=ftell(f))>0 && length<64*1024*1024);
    CHECK(fseek(f,0,SEEK_SET)==0); bytes=malloc((size_t)length); CHECK(bytes);
    CHECK(fread(bytes,1,(size_t)length,f)==(size_t)length); CHECK(fclose(f)==0); f=NULL;
    CHECK(settings_create(bytes,(size_t)length,NULL,&s));
    nx_context *ui=settings_context(s); nx_utf8_span value; int motion;
    CHECK(settings_frame(s,640,360));
    CHECK(key(ui,NX_KEY_TAB,0) && settings_frame(s,640,360) && nx_focus_id(ui)==3);
    CHECK(key(ui,NX_KEY_A,NX_MOD_CONTROL) && type(ui,"Ada") && settings_frame(s,640,360));
    CHECK(key(ui,NX_KEY_TAB,0) && settings_frame(s,640,360) && nx_focus_id(ui)==4);
    CHECK(key(ui,NX_KEY_SPACE,0) && settings_frame(s,640,360));
    CHECK(key(ui,NX_KEY_TAB,0) && settings_frame(s,640,360) && nx_focus_id(ui)==5);
    /* A resize must reveal the still-focused Apply control without more input. */
    CHECK(settings_frame(s,640,360) && settings_frame(s,240,150));
    uint32_t resize_index; nx_semantic_view resize_view;
    CHECK(nx_focus_id(ui)==5 && nx_semantics_find(ui,5,&resize_index)==NX_OK &&
          nx_semantics_get(ui,resize_index,&resize_view)==NX_OK &&
          resize_view.bounds.y>=70 && resize_view.bounds.y+resize_view.bounds.height<=134);
    CHECK(settings_frame(s,640,360));
    CHECK(key(ui,NX_KEY_SPACE,0) && settings_frame(s,640,360));
    uint64_t token=settings_pending_apply(s); CHECK(token);
    CHECK(settings_applied(s,&value,&motion) && value.length==7 && !memcmp(value.data,"Aetheri",7) && !motion);
    CHECK(settings_frame(s,640,360));
    uint32_t pending_index; nx_semantic_view pending_view;
    CHECK(nx_semantics_find(ui,5,&pending_index)==NX_OK && nx_semantics_get(ui,pending_index,&pending_view)==NX_OK &&
          (pending_view.state & NX_SEM_DISABLED));
    CHECK(nx_semantics_find(ui,6,&pending_index)==NX_OK && nx_semantics_get(ui,pending_index,&pending_view)==NX_OK &&
          pending_view.value.length==strlen("Applying to example...") &&
          !memcmp(pending_view.value.data,"Applying to example...",pending_view.value.length));
    CHECK(!settings_complete_apply(s,token+1,1) && settings_pending_apply(s)==token);
    CHECK(settings_complete_apply(s,token,1) && !settings_complete_apply(s,token,1));
    CHECK(settings_frame(s,640,360));
    CHECK(nx_focus_id(ui)==5);
    CHECK(settings_applied(s,&value,&motion) && value.length==3 && !memcmp(value.data,"Ada",3) && motion==1);
    uint32_t index; nx_semantic_view view;
    CHECK(nx_semantics_find(ui,6,&index)==NX_OK && nx_semantics_get(ui,index,&view)==NX_OK &&
          view.role==NX_ROLE_STATUS && view.value.length==23 && !memcmp(view.value.data,"Applied to this example",23));
    CHECK(key(ui,NX_KEY_TAB,NX_MOD_SHIFT) && settings_frame(s,640,360) && nx_focus_id(ui)==4);
    CHECK(key(ui,NX_KEY_TAB,NX_MOD_SHIFT) && settings_frame(s,640,360) && nx_focus_id(ui)==3);
    CHECK(key(ui,NX_KEY_A,NX_MOD_CONTROL) && key(ui,NX_KEY_BACKSPACE,0) && settings_frame(s,640,360));
    CHECK(nx_semantics_find(ui,5,&index)==NX_OK && nx_semantics_get(ui,index,&view)==NX_OK &&
          (view.state & NX_SEM_DISABLED));
    CHECK(settings_applied(s,&value,&motion) && value.length==3 && !memcmp(value.data,"Ada",3) && motion==1);
    /* Small viewport: keyboard navigation must reveal each focused control. */
    CHECK(settings_frame(s,240,150) && type(ui,"B") && settings_frame(s,240,150));
    CHECK(key(ui,NX_KEY_TAB,0) && settings_frame(s,240,150) && nx_focus_id(ui)==4);
    CHECK(settings_frame(s,240,150));
    CHECK(nx_semantics_find(ui,4,&index)==NX_OK && nx_semantics_get(ui,index,&view)==NX_OK &&
          view.bounds.y>=70 && view.bounds.y+view.bounds.height<=134);
    CHECK(key(ui,NX_KEY_TAB,0) && settings_frame(s,240,150) && nx_focus_id(ui)==5);
    CHECK(settings_frame(s,240,150));
    CHECK(nx_semantics_find(ui,5,&index)==NX_OK && nx_semantics_get(ui,index,&view)==NX_OK &&
          view.bounds.y>=70 && view.bounds.y+view.bounds.height<=134);
    CHECK(key(ui,NX_KEY_TAB,NX_MOD_SHIFT) && settings_frame(s,240,150) && nx_focus_id(ui)==4);
    CHECK(settings_frame(s,240,150));
    CHECK(nx_semantics_find(ui,4,&index)==NX_OK && nx_semantics_get(ui,index,&view)==NX_OK &&
          view.bounds.y>=70 && view.bounds.y+view.bounds.height<=134);
    /* Failure preserves the old model; retry snapshots the submitted draft,
       even if the user edits again before the reply arrives. */
    CHECK(settings_set_draft_name(s,"First",5) && settings_frame(s,640,360));
    CHECK(nx_focus_set(ui,5)==NX_OK && key(ui,NX_KEY_SPACE,0) && settings_frame(s,640,360));
    token=settings_pending_apply(s); CHECK(token && settings_complete_apply(s,token,0));
    CHECK(settings_frame(s,640,360));
    CHECK(nx_semantics_find(ui,6,&index)==NX_OK && nx_semantics_get(ui,index,&view)==NX_OK &&
          view.value.length==28 && !memcmp(view.value.data,"Example apply failed. Retry.",28));
    CHECK(settings_applied(s,&value,&motion) && value.length==3 && !memcmp(value.data,"Ada",3) && motion==1);
    CHECK(settings_set_draft_name(s,"",0) && settings_frame(s,640,360));
    CHECK(nx_semantics_find(ui,6,&index)==NX_OK && nx_semantics_get(ui,index,&view)==NX_OK &&
          view.value.length==strlen("Enter a name under 128 bytes") &&
          !memcmp(view.value.data,"Enter a name under 128 bytes",view.value.length));
    CHECK(settings_set_draft_name(s,"First",5) && settings_frame(s,640,360) && nx_focus_set(ui,5)==NX_OK);
    CHECK(key(ui,NX_KEY_SPACE,0) && settings_frame(s,640,360));
    uint64_t retry=settings_pending_apply(s); CHECK(retry && retry!=token);
    CHECK(!settings_complete_apply(s,token,1));
    CHECK(settings_set_draft_name(s,"Later",5) && settings_frame(s,640,360));
    CHECK(nx_focus_set(ui,3)==NX_OK);
    CHECK(settings_complete_apply(s,retry,1) && settings_frame(s,640,360));
    CHECK(nx_focus_id(ui)==3);
    CHECK(settings_applied(s,&value,&motion) && value.length==5 && !memcmp(value.data,"First",5));
    CHECK(nx_semantics_find(ui,6,&index)==NX_OK && nx_semantics_get(ui,index,&view)==NX_OK &&
          view.value.length==15 && !memcmp(view.value.data,"Unsaved changes",15));
    /* At the minimum width, status geometry must accommodate wrapped text and
       Apply must not extend outside the content area. */
    CHECK(settings_set_draft_name(s,"",0) && settings_frame(s,128,150));
    CHECK(nx_semantics_find(ui,5,&index)==NX_OK && nx_semantics_get(ui,index,&view)==NX_OK &&
          view.bounds.x+view.bounds.width<=112);
    CHECK(nx_semantics_find(ui,6,&index)==NX_OK && nx_semantics_get(ui,index,&view)==NX_OK &&
          view.bounds.height>20);
    nx_font *measure_font=NULL; nx_font_collection *measure_fonts=NULL; nx_text_layout *measured=NULL;
    const char *invalid="Enter a name under 128 bytes";
    CHECK(nx_font_open(bytes,(size_t)length,NULL,&measure_font)==NX_OK);
    CHECK(nx_font_collection_create(NULL,&measure_fonts)==NX_OK &&
          nx_font_collection_add(measure_fonts,measure_font)==NX_OK);
    CHECK(nx_text_layout_create(measure_fonts,invalid,strlen(invalid),NX_BIDI_LTR,15,64,&measured)==NX_OK);
    double measured_width=nx_text_layout_width(measured), measured_height=nx_text_layout_height(measured);
    if (measured_width>64) {
        double size=15*0.98*64/measured_width;
        nx_text_layout_destroy(measured); measured=NULL;
        CHECK(nx_text_layout_create(measure_fonts,invalid,strlen(invalid),NX_BIDI_LTR,size,64,&measured)==NX_OK);
        measured_width=nx_text_layout_width(measured); measured_height=nx_text_layout_height(measured);
    }
    nx_text_layout_destroy(measured); measured=NULL;
    nx_font_collection_destroy(measure_fonts); nx_font_close(measure_font);
    CHECK(measured_width<=64 && view.bounds.height==measured_height);
    CHECK(nx_focus_set(ui,4)==NX_OK && settings_frame(s,128,150));
    CHECK(nx_semantics_find(ui,6,&index)==NX_OK && nx_semantics_get(ui,index,&view)==NX_OK);
    float initial_status_y=view.bounds.y;
    for (unsigned page=0;page<8;++page)
        CHECK(key(ui,NX_KEY_PAGE_DOWN,0) && settings_frame(s,128,150));
    CHECK(nx_focus_id(ui)==4 && nx_semantics_find(ui,6,&index)==NX_OK &&
          nx_semantics_get(ui,index,&view)==NX_OK && view.bounds.y<initial_status_y &&
          view.bounds.y+view.bounds.height<=134);
    float bottom_status_y=view.bounds.y;
    CHECK(key(ui,NX_KEY_PAGE_UP,0) && settings_frame(s,128,150));
    CHECK(nx_semantics_find(ui,6,&index)==NX_OK && nx_semantics_get(ui,index,&view)==NX_OK &&
          view.bounds.y>bottom_status_y);
    CHECK(settings_frame(s,640,150) && settings_frame(s,128,150) && nx_focus_id(ui)==4);
    CHECK(nx_semantics_find(ui,4,&index)==NX_OK && nx_semantics_get(ui,index,&view)==NX_OK &&
          view.bounds.y>=70 && view.bounds.y+view.bounds.height<=134 &&
          view.bounds.x+view.bounds.width<=112);
    /* Language changes keep IDs, focus, draft and in-flight work intact. */
    const char *german_name="M\xc3\xbcnchen";
    CHECK(settings_set_draft_name(s,german_name,strlen(german_name)) && settings_frame(s,240,150));
    CHECK(nx_focus_set(ui,3)==NX_OK && settings_set_language(s,"de") && settings_frame(s,240,150));
    CHECK(nx_focus_id(ui)==3 && nx_semantics_find(ui,3,&index)==NX_OK && nx_semantics_get(ui,index,&view)==NX_OK &&
          view.name.length==strlen("Anzeigename") && !memcmp(view.name.data,"Anzeigename",view.name.length) &&
          view.value.length==strlen(german_name) && !memcmp(view.value.data,german_name,view.value.length));
    CHECK(nx_semantics_find(ui,5,&index)==NX_OK && nx_semantics_get(ui,index,&view)==NX_OK &&
          view.name.length==strlen("Einstellungen anwenden") && !memcmp(view.name.data,"Einstellungen anwenden",view.name.length));
    const char *german_status="Ungespeicherte \xc3\x84nderungen";
    CHECK(nx_semantics_find(ui,6,&index)==NX_OK && nx_semantics_get(ui,index,&view)==NX_OK &&
          view.value.length==strlen(german_status) && !memcmp(view.value.data,german_status,view.value.length));
    CHECK(!settings_set_language(s,"fr") && settings_frame(s,240,150) && nx_focus_id(ui)==3);
    CHECK(key(ui,NX_KEY_TAB,0) && settings_frame(s,240,150) && nx_focus_id(ui)==4);
    CHECK(key(ui,NX_KEY_TAB,0) && settings_frame(s,240,150) && nx_focus_id(ui)==5);
    CHECK(key(ui,NX_KEY_SPACE,0) && settings_frame(s,240,150));
    uint64_t language_token=settings_pending_apply(s); CHECK(language_token);
    CHECK(settings_set_language(s,"en") && settings_frame(s,240,150));
    CHECK(settings_pending_apply(s)==language_token && settings_set_language(s,"de") && settings_frame(s,240,150));
    CHECK(settings_pending_apply(s)==language_token && settings_complete_apply(s,language_token,1));
    CHECK(settings_frame(s,240,150) && settings_applied(s,&value,&motion) &&
          value.length==strlen(german_name) && !memcmp(value.data,german_name,value.length));
    /* An uncertain reply is not failure or success and must never redispatch. */
    CHECK(settings_set_language(s,"en") && settings_set_draft_name(s,"Submitted",9) && settings_frame(s,640,360));
    CHECK(nx_focus_set(ui,5)==NX_OK && key(ui,NX_KEY_SPACE,0) && settings_frame(s,640,360));
    uint64_t uncertain=settings_pending_apply(s); CHECK(uncertain);
    CHECK(settings_take_apply_request(s)==uncertain && !settings_take_apply_request(s));
    CHECK(!settings_observe_apply_unknown(s,uncertain+1));
    CHECK(settings_observe_apply_unknown(s,uncertain) && settings_frame(s,640,360));
    CHECK(settings_pending_apply(s)==uncertain && !settings_take_apply_request(s));
    CHECK(nx_semantics_find(ui,5,&index)==NX_OK && nx_semantics_get(ui,index,&view)==NX_OK && (view.state&NX_SEM_DISABLED));
    const char *unknown="Outcome unknown. Check the service.";
    CHECK(nx_semantics_find(ui,6,&index)==NX_OK && nx_semantics_get(ui,index,&view)==NX_OK &&
          view.value.length==strlen(unknown) && !memcmp(view.value.data,unknown,view.value.length));
    CHECK(settings_applied(s,&value,&motion) && value.length==strlen(german_name) && !memcmp(value.data,german_name,value.length));
    CHECK(settings_set_draft_name(s,"Later",5) && settings_set_language(s,"de") && settings_frame(s,240,150));
    const char *unknown_de="Ergebnis unbekannt. Dienst pr\xc3\xbc" "fen.";
    CHECK(nx_semantics_find(ui,6,&index)==NX_OK && nx_semantics_get(ui,index,&view)==NX_OK &&
          view.value.length==strlen(unknown_de) && !memcmp(view.value.data,unknown_de,view.value.length));
    CHECK(!settings_take_apply_request(s) && !settings_complete_apply(s,uncertain+1,1));
    CHECK(settings_complete_apply(s,uncertain,1) && settings_frame(s,640,360));
    CHECK(!settings_pending_apply(s) && !settings_take_apply_request(s) &&
          settings_applied(s,&value,&motion) && value.length==9 && !memcmp(value.data,"Submitted",9));
    CHECK(nx_semantics_find(ui,6,&index)==NX_OK && nx_semantics_get(ui,index,&view)==NX_OK &&
          view.value.length==strlen(german_status) && !memcmp(view.value.data,german_status,view.value.length));
    /* Only a definitive failure releases the old token and allows a retry. */
    CHECK(nx_focus_set(ui,5)==NX_OK && key(ui,NX_KEY_SPACE,0) && settings_frame(s,640,360));
    uint64_t unresolved=settings_pending_apply(s);CHECK(unresolved && unresolved!=uncertain);
    CHECK(settings_take_apply_request(s)==unresolved && settings_observe_apply_unknown(s,unresolved));
    CHECK(settings_set_draft_name(s,"",0) && settings_frame(s,640,360));
    CHECK(nx_semantics_find(ui,6,&index)==NX_OK && nx_semantics_get(ui,index,&view)==NX_OK &&
          view.value.length==strlen(unknown_de) && !memcmp(view.value.data,unknown_de,view.value.length));
    CHECK(settings_complete_apply(s,unresolved,0) && settings_set_draft_name(s,"Retry",5) && settings_frame(s,640,360));
    CHECK(settings_applied(s,&value,&motion) && value.length==9 && !memcmp(value.data,"Submitted",9));
    CHECK(nx_focus_set(ui,5)==NX_OK && key(ui,NX_KEY_SPACE,0) && settings_frame(s,640,360));
    uint64_t resolved_retry=settings_pending_apply(s);CHECK(resolved_retry && resolved_retry!=unresolved);
    CHECK(!settings_complete_apply(s,unresolved,1) && settings_take_apply_request(s)==resolved_retry);
    CHECK(settings_complete_apply(s,resolved_retry,0));
    /* The application keeps operation state while the view is absent. */
    CHECK(settings_set_language(s,"en") && settings_set_draft_name(s,"Original",8) && settings_frame(s,640,360));
    CHECK(nx_focus_set(ui,5)==NX_OK && key(ui,NX_KEY_SPACE,0) && settings_frame(s,640,360));
    uint64_t detached_token=settings_pending_apply(s);settings_apply_request original={0};example_service service={0};
    CHECK(detached_token && settings_claim_apply_request(s,&original) && original.token==detached_token &&
          original.name_length==8 && !memcmp(original.name,"Original",8) && service_submit(&service,&original));
    CHECK(!settings_take_apply_request(s));
    CHECK(settings_set_draft_name(s,"Newer draft",11) && settings_frame(s,640,360));
    CHECK(nx_focus_set(ui,4)==NX_OK && settings_frame(s,128,150));
    CHECK(!settings_detach(&s,NULL) && s);
    CHECK(settings_detach(&s,&session) && !s && session && settings_session_pending_apply(session)==detached_token);
    CHECK(!settings_session_complete_apply(session,detached_token+1,1));
    int service_result=0;
    CHECK(service_query(&service,settings_session_pending_apply(session),&service_result) && service_result==-1);
    CHECK(settings_session_observe_unknown(session,detached_token));
    CHECK(!settings_resume(NULL,0,NULL,&session,&s) && session && !s);
    CHECK(settings_resume(bytes,(size_t)length,NULL,&session,&s) && !session && s);
    ui=settings_context(s);CHECK(settings_frame(s,128,150) && nx_focus_id(ui)==4 && settings_frame(s,128,150));
    CHECK(nx_semantics_find(ui,4,&index)==NX_OK && nx_semantics_get(ui,index,&view)==NX_OK &&
          view.bounds.y>=70 && view.bounds.y+view.bounds.height<=134);
    CHECK(settings_pending_apply(s)==detached_token && !settings_take_apply_request(s));
    CHECK(nx_semantics_find(ui,6,&index)==NX_OK && nx_semantics_get(ui,index,&view)==NX_OK &&
          view.value.length==strlen(unknown) && !memcmp(view.value.data,unknown,view.value.length));
    CHECK(nx_semantics_find(ui,3,&index)==NX_OK && nx_semantics_get(ui,index,&view)==NX_OK &&
          view.value.length==11 && !memcmp(view.value.data,"Newer draft",11));
    CHECK(settings_detach(&s,&session) && !s && session);
    service.result=1;
    CHECK(service_query(&service,settings_session_pending_apply(session),&service_result) && service_result==1);
    CHECK(settings_session_complete_apply(session,detached_token,service_result) && service.starts==1 && service.queries==2);
    CHECK(!settings_session_complete_apply(session,detached_token,1) && !settings_session_pending_apply(session));
    CHECK(settings_resume(bytes,(size_t)length,NULL,&session,&s) && !session && s);
    ui=settings_context(s);CHECK(settings_frame(s,640,360) && nx_focus_id(ui)==4);
    CHECK(settings_applied(s,&value,&motion) && value.length==8 && !memcmp(value.data,"Original",8));
    CHECK(nx_semantics_find(ui,3,&index)==NX_OK && nx_semantics_get(ui,index,&view)==NX_OK &&
          view.value.length==11 && !memcmp(view.value.data,"Newer draft",11));
    CHECK(!settings_take_apply_request(s));
    /* An unclaimed request stays claimable exactly once after transfer. */
    CHECK(nx_focus_set(ui,5)==NX_OK && key(ui,NX_KEY_SPACE,0) && settings_frame(s,640,360));
    uint64_t unclaimed=settings_pending_apply(s);CHECK(unclaimed && unclaimed>detached_token);
    CHECK(settings_detach(&s,&session) && settings_resume(bytes,(size_t)length,NULL,&session,&s));
    ui=settings_context(s);CHECK(settings_frame(s,640,360) && nx_focus_id(ui)==4);
    CHECK(settings_take_apply_request(s)==unclaimed && !settings_take_apply_request(s));
    CHECK(settings_complete_apply(s,unclaimed,0));
    CHECK(settings_frame(s,640,360) && nx_focus_set(ui,5)==NX_OK && key(ui,NX_KEY_SPACE,0) && settings_frame(s,640,360));
    uint64_t finished_away=settings_take_apply_request(s);CHECK(finished_away);
    CHECK(settings_detach(&s,&session) && settings_session_complete_apply(session,finished_away,1));
    CHECK(settings_resume(bytes,(size_t)length,NULL,&session,&s));
    ui=settings_context(s);CHECK(settings_frame(s,640,360) && nx_focus_id(ui)==5);
    /* Input already queued for the replacement wins over remembered focus. */
    CHECK(settings_frame(s,640,360) && nx_focus_set(ui,3)==NX_OK);
    CHECK(settings_detach(&s,&session) && settings_resume(bytes,(size_t)length,NULL,&session,&s));
    ui=settings_context(s);CHECK(key(ui,NX_KEY_TAB,0) && settings_frame(s,640,360) && !nx_focus_id(ui));
    CHECK(nx_focus_set(ui,4)==NX_OK && settings_frame(s,128,150));
    CHECK(nx_semantics_find(ui,6,&index)==NX_OK && nx_semantics_get(ui,index,&view)==NX_OK);
    float before_transfer_scroll_y=view.bounds.y;
    CHECK(key(ui,NX_KEY_PAGE_DOWN,0) && settings_frame(s,128,150));
    CHECK(nx_focus_set(ui,0)==NX_OK && settings_frame(s,128,150));
    CHECK(nx_semantics_find(ui,6,&index)==NX_OK && nx_semantics_get(ui,index,&view)==NX_OK);
    float saved_status_y=view.bounds.y;CHECK(saved_status_y<before_transfer_scroll_y);
    CHECK(settings_detach(&s,&session) && settings_resume(bytes,(size_t)length,NULL,&session,&s));
    ui=settings_context(s);CHECK(settings_frame(s,128,150) && !nx_focus_id(ui));
    CHECK(nx_semantics_find(ui,6,&index)==NX_OK && nx_semantics_get(ui,index,&view)==NX_OK &&
          view.bounds.y==saved_status_y);
    char *large=malloc(65537u);CHECK(large);memset(large,'x',65537u);
    CHECK(settings_set_draft_name(s,large,65537u));free(large);large=NULL;
    CHECK(!settings_detach(&s,&session) && s && !session);
    settings_destroy(s); free(bytes); return 0;
fail:
    if (f) fclose(f); settings_session_destroy(session);settings_destroy(s); free(bytes); return 1;
}
