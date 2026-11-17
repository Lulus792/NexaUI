#define _GNU_SOURCE
#include "native_worker.h"
#include <nexaui/software.h>
#include <limits.h>
#include <stdio.h>
#include "native_input.h"

enum input_phase { INPUT_IDLE, INPUT_PENDING, INPUT_ACTIVE, INPUT_RELEASED, INPUT_FOCUS_LOST, INPUT_DISCONNECTED };
static const char *input_name(enum input_phase phase) {
    static const char *names[]={"Scaled frame ready.","Release keys to enter content.",
        "Input active: Ctrl+Shift+F12 exits.","Input released.","Input focus lost.","Input disconnected; pixels retained."};
    return names[(unsigned)phase];
}

enum phase { PREPARING, WAITING, REPLACED, RELEASED, OLD_REJECTED, FRESH_PENDING, READY,
             BAD_PENDING, BAD_REJECTED, RECOVERY_PENDING, RECOVERED, SCALED_WAIT, SCALED_PENDING,
             SCALED_READY, FAILED, PDF_PAGE1_SCALED, PDF_PAGE_PENDING };
static const char *phase_name(enum phase phase) {
    static const char *names[] = {"Preparing old frame.","Waiting for frame.","Replacement waiting.",
        "Old frame completing.","Old frame rejected.","Fresh frame pending.","Frame ready.",
        "Bad frame pending.","Frame format unavailable.","Recovery frame pending.","Frame recovered.",
        "Waiting for scaled frame.","Scaled frame pending.","Scaled frame ready.","Producer unavailable.",
        "PDF page 1 of 2 at 2x.","Rendering PDF page."};
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
static nx_result input_hint(nx_context *context,const nx_font_collection *fonts) {
    const char *text="Physical input fixture; exit: Ctrl+Shift+F12";
    nx_text_layout *layout=NULL;
    nx_result result=nx_text_layout_create(fonts,text,strlen(text),NX_BIDI_AUTO,11,448,&layout);
    if(result==NX_OK) result=nx_label(context,3,1,(nx_rect){16,122,448,14},layout,
        (nx_point){16,122},(nx_color){0.05f,0.07f,0.1f,1},65536);
    nx_text_layout_destroy(layout);return result;
}
typedef struct pdf_text_view {
    uint64_t generation, resize, sequence, identity;
} pdf_text_view;

static nx_result pdf_text_semantics(nx_context *context, content_surface *surface,
    const nx_viewport_desc *viewport, pdf_text_view *text_view) {
    nx_utf8_span text;
    nx_result result = content_surface_get_text(surface, &text);
    if (result != NX_OK || !text.length) return result;
    content_surface_status status;
    result = content_surface_get_status(surface, &status);
    if (result != NX_OK) return result;
    if (text_view->generation != status.request.generation || text_view->resize != status.request.resize ||
        text_view->sequence != status.sequence) {
        if (text_view->identity == UINT64_MAX) return NX_ERROR_LIMIT;
        *text_view = (pdf_text_view){status.request.generation, status.request.resize, status.sequence, text_view->identity+1};
    }
    nx_semantic_desc node = {.id=101, .parent_id=viewport->id, .role=NX_ROLE_TEXT, .bounds=viewport->bounds,
        .name={"PDF page text",13}, .value=text, .state=NX_SEM_READONLY};
    result = nx_semantics_add_readonly_text_snapshot_without_caret(context, &node, text_view->identity, NULL);
    if (result != NX_OK) return result;
    const content_frame_word *words;
    size_t word_count;
    result = content_surface_get_words(surface, &words, &word_count);
    if (result != NX_OK || !word_count) return result;
    nx_text_document_box boxes[CONTENT_FRAME_WORD_LIMIT];
    nx_rect view = viewport->bounds;
    for (size_t i = 0; i < word_count; ++i) {
        nx_rect word = words[i].bounds;
        boxes[i] = (nx_text_document_box){words[i].start_byte, words[i].end_byte, {
            view.x + word.x * view.width, view.y + word.y * view.height,
            word.width * view.width, word.height * view.height}};
    }
    nx_text_document_presentation presentation = {.boxes=boxes, .box_count=word_count};
    return nx_text_document_set_presentation(context, node.id, text_view->identity, 0, &presentation);
}
static int frame(nx_context *context, nx_window_info info, const nx_font_collection *fonts,
    content_surface *surface, double scale, enum phase phase, int focus, unsigned *action,
    nx_result *content_result, int *leased, int input_case, enum input_phase input_phase, int png_case, int pdf_case,
    pdf_text_view *text_view) {
    *action = 0; *leased = 0;
    if (nx_begin_frame(context, info.width, info.height, 0) != NX_OK) return 0;
    nx_semantic_desc root = {.id=1,.role=NX_ROLE_WINDOW,.bounds={0,0,info.width,info.height},
        .name={"External CPU content fixture",28}};
    int input_controls=input_case&&phase==SCALED_READY;
    const char *status=png_case&&phase==BAD_REJECTED?"PNG decoding failed; image retained.":phase_name(phase);
    if (pdf_case) {
        if (phase==READY) status="PDF page 1 of 2.";
        if (phase==BAD_REJECTED) status="PDF rendering failed; page retained.";
        if (phase==RECOVERED) status="PDF page 2 of 2.";
        if (phase==SCALED_READY) status="PDF page 2 of 2 at 2x.";
    }
    if (nx_semantics_add(context, &root, NULL) != NX_OK || label(context, fonts,
        input_controls?input_name(input_phase):status, info.width) != NX_OK ||
        button(context,fonts,input_controls?16u:10u,input_controls?"Enter content":"Replace tab",16,48,112,
            input_controls?input_phase!=INPUT_DISCONNECTED:phase==WAITING,action)!=NX_OK ||
        button(context,fonts,input_controls?17u:11u,input_controls?"Release input":"Release old",136,48,112,
            input_controls?input_phase==INPUT_ACTIVE||input_phase==INPUT_PENDING:phase==REPLACED,action)!=NX_OK ||
        button(context,fonts,input_controls?18u:12u,input_controls?"Disconnect":
            pdf_case&&(phase==BAD_REJECTED||phase==PDF_PAGE1_SCALED)?"Next page":"Publish frame",256,48,128,
            input_controls?input_phase!=INPUT_DISCONNECTED:phase==OLD_REJECTED||phase==BAD_REJECTED||
            phase==SCALED_WAIT||phase==PDF_PAGE1_SCALED,action)!=NX_OK ||
        button(context,fonts,13,pdf_case&&phase==SCALED_READY?"Previous page":"Bad frame",16,88,112,
            phase==READY||(pdf_case&&phase==SCALED_READY),action)!=NX_OK ||
        button(context,fonts,14,"Scale 2x",136,88,112,phase==RECOVERED,action)!=NX_OK ||
        button(context,fonts,15,"Close",256,88,128,1,action)!=NX_OK) return 0;
    if(input_controls&&input_hint(context,fonts)!=NX_OK) return 0;
    const char *name=pdf_case?"PDF page image":"Fixture frame";
    nx_viewport_desc desc = {.id=100,.parent_id=1,.bounds={16,136,info.width>32?info.width-32:0,info.height>152?info.height-152:0},
        .name={name,strlen(name)},.scale=scale};
    nx_interaction_state interaction;
    *content_result = content_surface_draw(surface, context, &desc, &interaction);
    *leased = *content_result == NX_OK && desc.bounds.width>0 && desc.bounds.height>0;
    if (pdf_case && *leased && pdf_text_semantics(context,surface,&desc,text_view)!=NX_OK) return 0;
    if (*content_result != NX_OK && nx_fill_rect(context, desc.bounds,
        (nx_color){227.0f/255.0f,231.0f/255.0f,239.0f/255.0f,1}) != NX_OK) return 0;
    if (focus) {
        uint64_t id = phase==WAITING?10u:phase==REPLACED?11u:
            phase==OLD_REJECTED||phase==BAD_REJECTED||phase==SCALED_WAIT?12u:
            phase==READY?13u:phase==RECOVERED?14u:15u;
        if (pdf_case&&phase==SCALED_READY) id=13u;
        if (phase==PDF_PAGE1_SCALED) id=12u;
        if (nx_focus_set(context,id)!=NX_OK) return 0;
    }
    return nx_end_frame(context)==NX_OK;
}

int main(void) {
    const char *isolated=getenv("NEXAUI_TEST_PRIVATE_WAYLAND"),*display=getenv("WAYLAND_DISPLAY");
    const char *path=getenv("NEXAUI_CONTENT_TEST_FONT"),*test_case=getenv("NEXAUI_CONTENT_TEST_CASE");
    if (!isolated||strcmp(isolated,"1")||!display||strcmp(display,"nexaui-test")||
        !getenv("NEXAUI_TEST_ATSPI_ADDRESS")||!path||!test_case||
        (strcmp(test_case,"full")&&strcmp(test_case,"png")&&strcmp(test_case,"pdf")&&strcmp(test_case,"input")&&
        strcmp(test_case,"pending-close")&&strcmp(test_case,"rejected-close"))) return 2;
    int png_case=!strcmp(test_case,"png");
    int pdf_case=!strcmp(test_case,"pdf");
    const char *encoded_dir=(png_case||pdf_case)?getenv("NEXAUI_CONTENT_TEST_ENCODED"):NULL;
    const char *pdf_renderer=pdf_case?getenv("NEXAUI_CONTENT_TEST_PDF_RENDERER"):NULL;
    const char *pdf_reader=pdf_case?getenv("NEXAUI_CONTENT_TEST_PDF_READER"):NULL;
#ifndef CONTENT_NATIVE_PNG
    if (png_case) return 2;
#endif
#ifndef CONTENT_NATIVE_PDF
    if (pdf_case) return 2;
#endif
    if (((png_case||pdf_case)&&!encoded_dir)||(pdf_case&&(!pdf_renderer||!pdf_reader))) return 2;
    int result=1,leased=0,worker_ready=0;
    nx_snapshot_exchange *exchange=NULL; content_surface *surface=NULL;
    nx_window *window=NULL; nx_software_view *software=NULL; nx_context *context=NULL;
    nx_font *font=NULL; nx_font_collection *fonts=NULL;
    content_input *input=NULL; native_input_receiver receiver={.generation=1};
    int input_case=!strcmp(test_case,"input"); enum input_phase input_phase=INPUT_IDLE;
    native_worker worker={0}; worker.wake[0]=worker.wake[1]=-1;
    size_t bytes=0; uint8_t *data=read_font(path,&bytes);
    enum phase phase=PREPARING,last_phase=FAILED;
    unsigned frames=0,action=0; int need_frame=1,initial_job=0; double scale=1;
    unsigned nav_completion=0; enum phase nav_target=FAILED;
    pdf_text_view text_view={0};
    uint64_t logged_generation=0,logged_resize=0,logged_sequence=0;
    const nx_window_desc desc={.title_utf8="External CPU content fixture",.width=480,.height=360};
    if (!data||nx_font_open(data,bytes,NULL,&font)!=NX_OK||nx_font_collection_create(NULL,&fonts)!=NX_OK||
        nx_font_collection_add(fonts,font)!=NX_OK||nx_snapshot_exchange_create(NULL,&exchange)!=NX_OK||
        content_surface_create(exchange,1,4u*1024u*1024u,NULL,&surface)!=NX_OK||
        nx_context_create(NULL,&context)!=NX_OK||nx_window_create(&desc,&window)!=NX_OK||
        nx_software_view_open(window,&software)!=NX_OK||
        native_worker_open(&worker,exchange,encoded_dir,pdf_renderer,pdf_reader)) goto done;
    worker_ready=1;
    if(input_case&&content_input_create(1,NX_KEY_F12,NX_MOD_CONTROL|NX_MOD_SHIFT,native_input_receive,&receiver,&input)!=NX_OK) goto done;
    for (;;) {
        uint32_t ready=0; size_t routed=0; nx_window_info info;
        if (nx_window_poll_with_fd_events(window,need_frame?0:200,worker.wake[0],NX_WINDOW_FD_READABLE,&ready)!=NX_OK||
            nx_window_get_info(window,&info)!=NX_OK) goto done;
        if(input) {
            /* Drain native events first; pending native AX actions still use
               route_events below, in their ordinary native-then-AX order. */
            for(;;) {
                nx_event event,host; int available=0,route=0;
                if(nx_window_next_event(window,&event,&available)!=NX_OK) goto done;
                if(!available) break;
                ++routed;
                if(event.type==NX_EVENT_FOCUS||event.type==NX_EVENT_KEY_DOWN||event.type==NX_EVENT_KEY_UP||event.type==NX_EVENT_POINTER_CANCEL) {
                    printf("INPUT_NATIVE type=%d key=%d focus=%d repeat=%d\n",(int)event.type,(int)event.key,event.focused,event.repeat);fflush(stdout);
                }
                content_input_status before,after;
                if(content_input_get_status(input,&before)!=NX_OK||
                    content_input_event(input,&event,&host,&route)!=NX_OK||
                    content_input_get_status(input,&after)!=NX_OK) goto done;
                if(route&&nx_context_queue_event(context,&host)!=NX_OK) goto done;
                if(before.active&&!after.active) {
                    input_phase=event.type==NX_EVENT_FOCUS&&!event.focused?INPUT_FOCUS_LOST:INPUT_RELEASED;
                    if(nx_focus_set(context,15)!=NX_OK) goto done;
                    need_frame=1;
                }
                /* Some backends deliver pointer cancellation before FocusOut.
                   Preserve the actual focus-loss reason after that release. */
                if(event.type==NX_EVENT_FOCUS&&!event.focused&&phase==SCALED_READY&&input_phase!=INPUT_DISCONNECTED) {
                    input_phase=INPUT_FOCUS_LOST;need_frame=1;
                }
            }
        }
        size_t ax_routed=0;
        if(nx_window_route_events(window,context,&ax_routed)!=NX_OK) goto done;
        routed+=ax_routed;
        if (ready) {
            char wake[64]; ssize_t count;
            do { count=read(worker.wake[0],wake,sizeof(wake)); } while (count>0||(count<0&&errno==EINTR));
            if (count<0&&errno!=EAGAIN&&errno!=EWOULDBLOCK) goto done;
        }
        native_worker_state state=native_state(&worker);
        if (phase==PREPARING&&state.prepared) { phase=WAITING; need_frame=1; }
        int decode_rejected=(png_case||pdf_case)&&state.completed==3&&state.result==NX_ERROR_ARGUMENT;
        if (state.result!=NX_OK&&!decode_rejected&&phase!=FAILED) { phase=FAILED; need_frame=1; }
        if (info.should_close) { result=0; break; }
        if (!need_frame&&!routed&&!ready) continue;
        need_frame=0;
        enum phase drawn=phase; nx_result content_result=NX_ERROR_STATE;
        if (++frames>150||!frame(context,info,fonts,surface,scale,phase,phase!=last_phase,&action,
            &content_result,&leased,input_case,input_phase,png_case,pdf_case,&text_view)) goto done;
        if (phase!=last_phase) need_frame=1;
        last_phase=drawn;
        if (nx_key_pressed(context,NX_KEY_ESCAPE)) { result=0; break; }
        if (phase==RELEASED&&state.completed==1) {
            phase=content_result==NX_ERROR_STATE?OLD_REJECTED:FAILED; need_frame=1;
        }
        if ((phase==FRESH_PENDING&&state.completed==2)||(phase==RECOVERY_PENDING&&state.completed==4)||
            (phase==SCALED_PENDING&&state.completed==5)) {
            if (content_result!=NX_OK) phase=FAILED;
            else phase=phase==FRESH_PENDING?READY:phase==RECOVERY_PENDING?RECOVERED:SCALED_READY;
            need_frame=1;
        }
        if (phase==BAD_PENDING&&state.completed==3) {
            int rejected=(png_case||pdf_case)?decode_rejected&&content_result==NX_OK:content_result==NX_ERROR_UNSUPPORTED;
            phase=rejected?BAD_REJECTED:FAILED; need_frame=1;
        }
        if (phase==PDF_PAGE_PENDING&&state.completed==nav_completion) {
            phase=content_result==NX_OK?nav_target:FAILED; need_frame=1;
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
            printf("CONTENT_STATE generation=%llu resize=%llu width=%u height=%u sequence=%llu copied=%llu copy_bytes=%zu\n",
                (unsigned long long)status.request.generation,(unsigned long long)status.request.resize,
                status.request.width,status.request.height,(unsigned long long)status.sequence,
                (unsigned long long)status.copied_frames,status.last_copy_bytes);
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
            if(input) { if(content_input_rebind(input,2)!=NX_OK) goto done; receiver.generation=2; }
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
        else if (pdf_case&&((action==13&&phase==SCALED_READY)||(action==12&&phase==PDF_PAGE1_SCALED))) {
            if (status.sequence==UINT64_MAX||state.completed==UINT_MAX) goto done;
            nav_target=action==13?PDF_PAGE1_SCALED:SCALED_READY;
            native_job job={status.request,status.sequence+1,action==13?5u:4u,0};
            if (!native_schedule(&worker,job)) goto done;
            nav_completion=state.completed+1; phase=PDF_PAGE_PENDING; need_frame=1;
        }
        if(input) {
            if(action==16&&phase==SCALED_READY&&input_phase!=INPUT_DISCONNECTED&&input_phase!=INPUT_ACTIVE) {
                input_phase=INPUT_PENDING;
                if(nx_focus_set(context,100)!=NX_OK) goto done;
                need_frame=1;
            } else if(action==17&&(input_phase==INPUT_ACTIVE||input_phase==INPUT_PENDING)) {
                if(content_input_release(input)!=NX_OK||nx_focus_set(context,15)!=NX_OK) goto done;
                input_phase=INPUT_RELEASED;need_frame=1;
            } else if(action==18&&input_phase!=INPUT_DISCONNECTED) {
                if(content_input_release(input)!=NX_OK||nx_focus_set(context,15)!=NX_OK) goto done;
                input_phase=INPUT_DISCONNECTED;need_frame=1;
            }
            content_input_target target={.bounds={16,136,info.width>32?info.width-32:0,info.height>152?info.height-152:0},
                .width=status.request.width,.height=status.request.height,
                .available=phase==SCALED_READY&&input_phase!=INPUT_DISCONNECTED,
                .focused=nx_focus_id(context)==100};
            content_input_status before,after;
            if(content_input_get_status(input,&before)!=NX_OK||content_input_set_target(input,&target)!=NX_OK||
                content_input_get_status(input,&after)!=NX_OK) goto done;
            if(before.active&&!after.active) { input_phase=INPUT_RELEASED;need_frame=1; }
            if(input_phase==INPUT_PENDING) {
                nx_result armed=content_input_arm(input);
                printf("INPUT_ARM result=%d focus=%llu window=%d keys=%zu buttons=%u draining=%zu/%u\n",(int)armed,
                    (unsigned long long)nx_focus_id(context),after.window_focused,after.physical_keys,after.physical_buttons,
                    after.draining_keys,after.draining_buttons);fflush(stdout);
                if(armed==NX_OK) { input_phase=INPUT_ACTIVE;need_frame=1; }
                else if(armed!=NX_ERROR_STATE) goto done;
            }
        }
    }
done:
    if(content_input_close(&input)!=NX_OK) result=1;
    if (leased&&content_surface_complete(surface)!=NX_OK) result=1;
    if (window&&nx_window_publish_semantics(window,NULL)!=NX_OK) result=1;
    if (software&&nx_software_view_close(&software)!=NX_OK) result=1;
    if (content_surface_destroy(&surface)!=NX_OK) result=1;
    nx_context_destroy(context);
    if (native_worker_close(&worker)) result=1;
    if (exchange&&nx_snapshot_exchange_destroy(exchange)!=NX_OK) result=1;
    if (window) nx_window_destroy(window);
    nx_font_collection_destroy(fonts); nx_font_close(font); free(data);
    unsigned expected=(!strcmp(test_case,"full")||input_case||png_case)?5u:!strcmp(test_case,"rejected-close")?1u:0u;
    if (pdf_case) expected=6;
    unsigned published=expected-(unsigned)(png_case||pdf_case);
    unsigned discarded=!strcmp(test_case,"pending-close")?1u:0u;
    if (!worker_ready||worker.completed!=expected||worker.publications!=published||worker.discarded!=discarded||
        atomic_load(&worker.disposals)!=published||((expected==5u)&&phase!=SCALED_READY)||
        (pdf_case&&phase!=PDF_PAGE1_SCALED)||
        (!strcmp(test_case,"rejected-close")&&phase!=OLD_REJECTED)||(!strcmp(test_case,"pending-close")&&phase!=WAITING)) result=1;
    if(input_case) {
        if(receiver.downs!=6||receiver.ups!=6||receiver.pointer_downs!=3||receiver.pointer_ups!=3||
            receiver.enters!=3||receiver.exits!=3||receiver.texts||receiver.buttons||native_input_keys(&receiver)||
            receiver.focused||input_phase!=INPUT_DISCONNECTED) result=1;
        printf("INPUT_EXIT result=%d down=%u up=%u pdown=%u pup=%u enter=%u exit=%u text=%u keys=%u buttons=%u\n",
            result,receiver.downs,receiver.ups,receiver.pointer_downs,receiver.pointer_ups,receiver.enters,receiver.exits,
            receiver.texts,native_input_keys(&receiver),receiver.buttons);
    }
    printf("CONTENT_EXIT result=%d publications=%u discarded=%u disposals=%u frames=%u completed=%u\n",
        result,worker.publications,worker.discarded,atomic_load(&worker.disposals),frames,worker.completed);
    return result;
}
