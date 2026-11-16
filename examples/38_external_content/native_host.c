#define _GNU_SOURCE
#include "native_worker.h"
#include <nexaui/software.h>
#include <stdio.h>

enum phase { PREPARING, WAITING, REPLACED, RELEASED, OLD_REJECTED, FRESH_PENDING, READY,
             BAD_PENDING, BAD_REJECTED, RECOVERY_PENDING, RECOVERED, SCALED_WAIT, SCALED_PENDING,
             SCALED_READY, FAILED };
static const char *phase_name(enum phase phase) {
    static const char *names[] = {"Preparing old frame.","Waiting for frame.","Replacement waiting.",
        "Old frame completing.","Old frame rejected.","Fresh frame pending.","Frame ready.",
        "Bad frame pending.","Frame format unavailable.","Recovery frame pending.","Frame recovered.",
        "Waiting for scaled frame.","Scaled frame pending.","Scaled frame ready.","Producer unavailable."};
    return names[(unsigned)phase];
}
static uint8_t *read_font(const char *path, size_t *bytes) {
    FILE *file = fopen(path, "rb"); if (!file) return NULL;
    uint8_t *data = NULL;
    if (!fseek(file, 0, SEEK_END)) {
        long length = ftell(file);
        if (length > 0 && length <= 16 * 1024 * 1024 && !fseek(file, 0, SEEK_SET)) {
            data = malloc((size_t)length);
            if (data && fread(data, 1, (size_t)length, file) != (size_t)length) { free(data); data = NULL; }
            if (data) *bytes = (size_t)length;
        }
    }
    fclose(file); return data;
}
static nx_result label(nx_context *context, const nx_font_collection *fonts, const char *text, float width) {
    nx_text_layout *layout = NULL;
    nx_result result = nx_text_layout_create(fonts, text, strlen(text), NX_BIDI_AUTO, 15, width - 32, &layout);
    if (result == NX_OK) result = nx_label(context, 2, 1, (nx_rect){16,12,width-32,24}, layout,
        (nx_point){16,12}, (nx_color){0.05f,0.07f,0.1f,1}, 65536);
    nx_text_layout_destroy(layout); return result;
}
static nx_result button(nx_context *context, const nx_font_collection *fonts, uint64_t id,
    const char *text, float x, float y, float width, int enabled, unsigned *action) {
    nx_text_layout *layout = NULL;
    nx_result result = nx_text_layout_create(fonts, text, strlen(text), NX_BIDI_AUTO, 14, width - 12, &layout);
    if (result == NX_OK) {
        nx_control_desc desc = {.id=id,.parent_id=1,.bounds={x,y,width,32},.name={text,strlen(text)},
            .label_layout=layout,.label_origin={x+6,y+8},.glyph_cache_bytes=65536,.disabled=!enabled};
        nx_interaction_state state;
        result = nx_button(context, &desc, &state);
        if (result == NX_OK && state.clicked && enabled) *action = (unsigned)id;
    }
    nx_text_layout_destroy(layout); return result;
}
static int frame(nx_context *context, nx_window_info info, const nx_font_collection *fonts,
    content_surface *surface, double scale, enum phase phase, int focus, unsigned *action,
    nx_result *content_result, int *leased) {
    *action = 0; *leased = 0;
    if (nx_begin_frame(context, info.width, info.height, 0) != NX_OK) return 0;
    nx_semantic_desc root = {.id=1,.role=NX_ROLE_WINDOW,.bounds={0,0,info.width,info.height},
        .name={"External CPU content fixture",28}};
    if (nx_semantics_add(context, &root, NULL) != NX_OK || label(context, fonts, phase_name(phase), info.width) != NX_OK ||
        button(context,fonts,10,"Replace tab",16,48,112,phase==WAITING,action)!=NX_OK ||
        button(context,fonts,11,"Release old",136,48,112,phase==REPLACED,action)!=NX_OK ||
        button(context,fonts,12,"Publish frame",256,48,128,phase==OLD_REJECTED||phase==BAD_REJECTED||phase==SCALED_WAIT,action)!=NX_OK ||
        button(context,fonts,13,"Bad frame",16,88,112,phase==READY,action)!=NX_OK ||
        button(context,fonts,14,"Scale 2x",136,88,112,phase==RECOVERED,action)!=NX_OK ||
        button(context,fonts,15,"Close",256,88,128,1,action)!=NX_OK) return 0;
    nx_viewport_desc desc = {.id=100,.parent_id=1,.bounds={16,136,info.width>32?info.width-32:0,info.height>152?info.height-152:0},
        .name={"Fixture frame",13},.scale=scale};
    nx_interaction_state interaction;
    *content_result = content_surface_draw(surface, context, &desc, &interaction);
    *leased = *content_result == NX_OK && desc.bounds.width>0 && desc.bounds.height>0;
    if (*content_result != NX_OK && nx_fill_rect(context, desc.bounds,
        (nx_color){227.0f/255.0f,231.0f/255.0f,239.0f/255.0f,1}) != NX_OK) return 0;
    if (focus) {
        uint64_t id = phase==WAITING?10u:phase==REPLACED?11u:
            phase==OLD_REJECTED||phase==BAD_REJECTED||phase==SCALED_WAIT?12u:
            phase==READY?13u:phase==RECOVERED?14u:15u;
        if (nx_focus_set(context,id)!=NX_OK) return 0;
    }
    return nx_end_frame(context)==NX_OK;
}

int main(void) {
    const char *isolated=getenv("NEXAUI_TEST_PRIVATE_WAYLAND"),*display=getenv("WAYLAND_DISPLAY");
    const char *path=getenv("NEXAUI_CONTENT_TEST_FONT"),*test_case=getenv("NEXAUI_CONTENT_TEST_CASE");
    if (!isolated||strcmp(isolated,"1")||!display||strcmp(display,"nexaui-test")||
        !getenv("NEXAUI_TEST_ATSPI_ADDRESS")||!path||!test_case||
        (strcmp(test_case,"full")&&strcmp(test_case,"pending-close")&&strcmp(test_case,"rejected-close"))) return 2;
    int result=1,leased=0,worker_ready=0;
    nx_snapshot_exchange *exchange=NULL; content_surface *surface=NULL;
    nx_window *window=NULL; nx_software_view *software=NULL; nx_context *context=NULL;
    nx_font *font=NULL; nx_font_collection *fonts=NULL;
    native_worker worker={0}; worker.wake[0]=worker.wake[1]=-1;
    size_t bytes=0; uint8_t *data=read_font(path,&bytes);
    enum phase phase=PREPARING,last_phase=FAILED;
    unsigned frames=0,action=0; int need_frame=1,initial_job=0; double scale=1;
    uint64_t logged_generation=0,logged_resize=0,logged_sequence=0;
    const nx_window_desc desc={.title_utf8="External CPU content fixture",.width=480,.height=360};
    if (!data||nx_font_open(data,bytes,NULL,&font)!=NX_OK||nx_font_collection_create(NULL,&fonts)!=NX_OK||
        nx_font_collection_add(fonts,font)!=NX_OK||nx_snapshot_exchange_create(NULL,&exchange)!=NX_OK||
        content_surface_create(exchange,1,4u*1024u*1024u,NULL,&surface)!=NX_OK||
        nx_context_create(NULL,&context)!=NX_OK||nx_window_create(&desc,&window)!=NX_OK||
        nx_software_view_open(window,&software)!=NX_OK||native_worker_open(&worker,exchange)) goto done;
    worker_ready=1;
    for (;;) {
        uint32_t ready=0; size_t routed=0; nx_window_info info;
        if (nx_window_poll_with_fd_events(window,need_frame?0:200,worker.wake[0],NX_WINDOW_FD_READABLE,&ready)!=NX_OK||
            nx_window_get_info(window,&info)!=NX_OK||nx_window_route_events(window,context,&routed)!=NX_OK) goto done;
        if (ready) {
            char wake[64]; ssize_t count;
            do { count=read(worker.wake[0],wake,sizeof(wake)); } while (count>0||(count<0&&errno==EINTR));
            if (count<0&&errno!=EAGAIN&&errno!=EWOULDBLOCK) goto done;
        }
        native_worker_state state=native_state(&worker);
        if (phase==PREPARING&&state.prepared) { phase=WAITING; need_frame=1; }
        if (state.result!=NX_OK) { phase=FAILED; need_frame=1; }
        if (info.should_close) { result=0; break; }
        if (!need_frame&&!routed&&!ready) continue;
        need_frame=0;
        enum phase drawn=phase; nx_result content_result=NX_ERROR_STATE;
        if (++frames>150||!frame(context,info,fonts,surface,scale,phase,phase!=last_phase,&action,&content_result,&leased)) goto done;
        if (phase!=last_phase) need_frame=1;
        last_phase=drawn;
        if (nx_key_pressed(context,NX_KEY_ESCAPE)) { result=0; break; }
        if (phase==RELEASED&&state.publications==1) {
            phase=content_result==NX_ERROR_STATE?OLD_REJECTED:FAILED; need_frame=1;
        }
        if ((phase==FRESH_PENDING&&state.publications==2)||(phase==RECOVERY_PENDING&&state.publications==4)||
            (phase==SCALED_PENDING&&state.publications==5)) {
            if (content_result!=NX_OK) phase=FAILED;
            else phase=phase==FRESH_PENDING?READY:phase==RECOVERY_PENDING?RECOVERED:SCALED_READY;
            need_frame=1;
        }
        if (phase==BAD_PENDING&&state.publications==3) {
            phase=content_result==NX_ERROR_UNSUPPORTED?BAD_REJECTED:FAILED; need_frame=1;
        }
        if (nx_window_publish_semantics(window,context)!=NX_OK) goto done;
        const nx_command *commands=NULL; size_t count=0; nx_software_view_outcome outcome=NX_SOFTWARE_VIEW_READY;
        if (nx_frame_commands(context,&commands,&count)!=NX_OK||
            nx_software_view_render(software,commands,count,(nx_color){1,1,1,1},&outcome)!=NX_OK) goto done;
        /* The presenter borrows commands only until return. Discard this
           stream after completing its lease; BUSY/SUSPENDED rebuild later. */
        if (leased) { if (content_surface_complete(surface)!=NX_OK) goto done; leased=0; }
        if (outcome!=NX_SOFTWARE_VIEW_READY) need_frame=1;
        content_surface_status status;
        if (content_surface_get_status(surface,&status)!=NX_OK) goto done;
        if (status.request.generation!=logged_generation||status.request.resize!=logged_resize||status.sequence!=logged_sequence) {
            printf("CONTENT_STATE generation=%llu resize=%llu width=%u height=%u sequence=%llu copied=%llu\n",
                (unsigned long long)status.request.generation,(unsigned long long)status.request.resize,
                status.request.width,status.request.height,(unsigned long long)status.sequence,
                (unsigned long long)status.copied_frames);
            fflush(stdout);
            logged_generation=status.request.generation;logged_resize=status.request.resize;logged_sequence=status.sequence;
        }
        if (!initial_job&&status.request.width&&status.request.height) {
            if (!native_schedule(&worker,(native_job){status.request,1,0,1})) goto done;
            initial_job=1;
        }
        if (action==15) { result=0; break; }
        if (action==10&&phase==WAITING) {
            if (content_surface_destroy(&surface)!=NX_OK||content_surface_create(exchange,2,4u*1024u*1024u,NULL,&surface)!=NX_OK) goto done;
            phase=REPLACED; need_frame=1;
        } else if (action==11&&phase==REPLACED) { native_release(&worker); phase=RELEASED; need_frame=1; }
        else if (action==12&&(phase==OLD_REJECTED||phase==BAD_REJECTED||phase==SCALED_WAIT)) {
            native_job job={status.request,phase==BAD_REJECTED?3u:1u,phase==BAD_REJECTED?3u:phase==SCALED_WAIT?4u:1u,0};
            if (!native_schedule(&worker,job)) goto done;
            phase=phase==OLD_REJECTED?FRESH_PENDING:phase==BAD_REJECTED?RECOVERY_PENDING:SCALED_PENDING; need_frame=1;
        } else if (action==13&&phase==READY) {
            if (!native_schedule(&worker,(native_job){status.request,2,2,0})) goto done;
            phase=BAD_PENDING; need_frame=1;
        } else if (action==14&&phase==RECOVERED) { scale=2; phase=SCALED_WAIT; need_frame=1; }
    }
done:
    if (leased&&content_surface_complete(surface)!=NX_OK) result=1;
    if (window&&nx_window_publish_semantics(window,NULL)!=NX_OK) result=1;
    if (software&&nx_software_view_close(&software)!=NX_OK) result=1;
    if (content_surface_destroy(&surface)!=NX_OK) result=1;
    nx_context_destroy(context);
    if (native_worker_close(&worker)) result=1;
    if (exchange&&nx_snapshot_exchange_destroy(exchange)!=NX_OK) result=1;
    if (window) nx_window_destroy(window);
    nx_font_collection_destroy(fonts); nx_font_close(font); free(data);
    unsigned expected=!strcmp(test_case,"full")?5u:!strcmp(test_case,"rejected-close")?1u:0u;
    unsigned discarded=!strcmp(test_case,"pending-close")?1u:0u;
    if (!worker_ready||worker.publications!=expected||worker.discarded!=discarded||
        atomic_load(&worker.disposals)!=expected||(!strcmp(test_case,"full")&&phase!=SCALED_READY)||
        (!strcmp(test_case,"rejected-close")&&phase!=OLD_REJECTED)||(!strcmp(test_case,"pending-close")&&phase!=WAITING)) result=1;
    printf("CONTENT_EXIT result=%d publications=%u discarded=%u disposals=%u frames=%u\n",
        result,worker.publications,worker.discarded,atomic_load(&worker.disposals),frames);
    return result;
}
