#define _POSIX_C_SOURCE 200809L
#include "scene.h"
#include "renderer.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static double seconds(clockid_t clock) {
    struct timespec t;
    return clock_gettime(clock,&t)==0 ? (double)t.tv_sec+(double)t.tv_nsec/1.e9 : -1;
}

static unsigned char *read_font(const char *path, size_t *size) {
    FILE *file=fopen(path,"rb"); if (!file) return NULL;
    long length=-1; unsigned char *bytes=NULL;
    if (fseek(file,0,SEEK_END)==0 && (length=ftell(file))>0 && length<64*1024*1024 &&
        fseek(file,0,SEEK_SET)==0 && (bytes=malloc((size_t)length)) &&
        fread(bytes,1,(size_t)length,file)==(size_t)length) *size=(size_t)length;
    else { free(bytes); bytes=NULL; }
    if (fclose(file)!=0) { free(bytes); bytes=NULL; }
    return bytes;
}
int main(int argc, char **argv) {
    int smoke=argc==3 && !strcmp(argv[2],"--smoke");
    int verify_ax=argc==3 && !strcmp(argv[2],"--verify-accessibility");
    int verify_keyboard_text=argc==3 && !strcmp(argv[2],"--verify-keyboard");
    int verify_keyboard_controls=argc==3 && !strcmp(argv[2],"--verify-keyboard-controls");
    int verify_keyboard_small=argc==3 && !strcmp(argv[2],"--verify-keyboard-small");
    int verify_ime_compose=argc==3 && !strcmp(argv[2],"--verify-ime-compose");
    int verify_ime_cancel=argc==3 && !strcmp(argv[2],"--verify-ime-focus-cancel");
    int verify_ime_control=argc==3 && !strcmp(argv[2],"--verify-ime-control-cancel");
    int verify_ime_replace=argc==3 && !strcmp(argv[2],"--verify-ime-replace");
    int verify_ime=verify_ime_compose || verify_ime_cancel || verify_ime_control || verify_ime_replace;
    int verify_keyboard=verify_keyboard_text || verify_keyboard_controls || verify_keyboard_small;
    int test_continuous=verify_keyboard && getenv("NEXAUI_TEST_CONTINUOUS")!=NULL;
    int idle_probe=argc==3 && !strcmp(argv[2],"--idle-probe");
    int simulate_error=argc==3 && !strcmp(argv[2],"--simulate-error");
    int fail_first=simulate_error || (verify_ax && getenv("NEXAUI_SETTINGS_TEST_APPLY_ERROR"));
    if (argc!=2 && !smoke && !verify_ax && !verify_keyboard && !verify_ime && !idle_probe && !simulate_error) { fputs("usage: nexaui_settings FONT_FILE [--smoke|--idle-probe|--simulate-error]\n",stderr); return 2; }
    size_t font_size=0; unsigned char *font=read_font(argv[1],&font_size);
    nx_window *window=NULL; settings_scene *scene=NULL; nx_renderer_driver renderer={0};
    int opened=0, okay=0, keyboard_ready=0, ax_test_ready=0, ime_stage=0, ime_focus_reported=0; unsigned presented=0, loops=0, polls=0, settled_focuses=0, idle_long_waits=0;
    double start=-1, cpu_start=-1, elapsed=0, cpu_elapsed=0;
    double next_ax_retry=0; int dirty=1, drawable=1, ax_unavailable=0;
    double apply_deadline=0; uint64_t scheduled_apply=0;
    nx_window_info previous={0};
    nx_window_desc desc={"Settings example",640,360,0,{0}};
    if (verify_keyboard_small) { desc.width=240; desc.height=150; }
    if (!font || nx_window_create(&desc,&window)!=NX_OK ||
        !settings_create(font,font_size,window,&scene) ||
        nx_vulkan_driver_open(window,&renderer)!=NX_OK) goto cleanup;
    opened=1; okay=1;
    if (verify_ax && (!settings_set_draft_name(scene,"A\xc3\xa9\xf0\x9f\x8c\x8d",7) ||
                      !settings_enable_accessibility_probe(scene))) okay=0;
    if (idle_probe) {start=seconds(CLOCK_MONOTONIC);cpu_start=seconds(CLOCK_PROCESS_CPUTIME_ID);if(start<0 || cpu_start<0)okay=0;}
    while (okay) {
        nx_window_info info; size_t routed;
        if (test_continuous) dirty=1;
        uint32_t timeout=(dirty && drawable) || smoke ? 16u : ax_unavailable ? 1000u : UINT32_MAX;
        if (scheduled_apply) {
            double now=seconds(CLOCK_MONOTONIC);
            if (now<0) {okay=0;break;}
            double remaining=apply_deadline-now;
            uint32_t wait=remaining<=0 ? 0u : (uint32_t)ceil(remaining*1000.0);
            if (wait<timeout) timeout=wait;
        }
        if (idle_probe) {
            double current=seconds(CLOCK_MONOTONIC);
            if(current<0) {okay=0;break;}
            double remaining=3.0-(current-start);
            if(remaining<=0) break;
            if(remaining*1000.0 < timeout) timeout=(uint32_t)ceil(remaining*1000.0);
        }
        int long_wait=idle_probe && !dirty && !ax_unavailable && timeout>1500u;
        double wait_started=long_wait?seconds(CLOCK_MONOTONIC):0;
        if(long_wait && wait_started<0) {okay=0;break;}
        ++polls;
        if (nx_window_poll(window,timeout)!=NX_OK || nx_window_get_info(window,&info)!=NX_OK ||
            nx_window_route_events(window,settings_context(scene),&routed)!=NX_OK) { okay=0; break; }
        if(long_wait) {
            double waited=seconds(CLOCK_MONOTONIC);
            if(waited<0) {okay=0;break;}
            if(waited-wait_started>=1.0) ++idle_long_waits;
        }
        if (info.should_close || (smoke && loops++>=20)) break;
        if (scheduled_apply) {
            double now=seconds(CLOCK_MONOTONIC);
            if (now<0) {okay=0;break;}
            if (now>=apply_deadline) {
                if (!settings_complete_apply(scene,scheduled_apply,!fail_first)) {okay=0;break;}
                fail_first=0; scheduled_apply=0; dirty=1;
            }
        }
        if (idle_probe) {
            double now=seconds(CLOCK_MONOTONIC),cpu=seconds(CLOCK_PROCESS_CPUTIME_ID);
            if(now<0 || cpu<0) {okay=0;break;}
            elapsed=now-start;cpu_elapsed=cpu-cpu_start;
            if(elapsed>=3.0) break;
        }
        if (routed || info.width!=previous.width || info.height!=previous.height || info.scale!=previous.scale)
            dirty=1;
        previous=info;
        drawable=info.width>=128 && info.height>=120;
        if (!drawable) continue;
        if (!dirty) {
            if (ax_unavailable) {
                double now=seconds(CLOCK_MONOTONIC);
                if (now<0) {okay=0;break;}
                if (now>=next_ax_retry) {
                    nx_result ax=nx_window_publish_semantics(window,settings_context(scene));
                    if (ax!=NX_OK && ax!=NX_ERROR_UNSUPPORTED) {okay=0;break;}
                    ax_unavailable=ax==NX_ERROR_UNSUPPORTED; next_ax_retry=now+1.0;
                }
            }
            continue;
        }
        uint64_t previous_focus=nx_focus_id(settings_context(scene));
        if (nx_context_set_text_raster_scale(settings_context(scene),info.scale)!=NX_OK ||
            !settings_frame(scene,info.width,info.height)) { okay=0; break; }
        int scheduled_now=0;
        if (!scheduled_apply && settings_pending_apply(scene)) {
            double now=seconds(CLOCK_MONOTONIC);
            if (now<0) {okay=0;break;}
            scheduled_apply=settings_pending_apply(scene);
            apply_deadline=now+0.25; scheduled_now=1;
        }
        if (verify_ax && !ax_test_ready) {
            nx_context *ui=settings_context(scene);
            nx_event event={0}; event.type=NX_EVENT_TEXT_COMMAND;
            event.text_command=NX_TEXT_COMMAND_DOCUMENT_END;
            if (nx_focus_set(ui,3)!=NX_OK || nx_context_queue_event(ui,&event)!=NX_OK) { okay=0; break; }
            event.text_command=NX_TEXT_COMMAND_MOVE_BACKWARD|NX_TEXT_COMMAND_EXTEND_SELECTION;
            if (nx_context_queue_event(ui,&event)!=NX_OK || nx_context_queue_event(ui,&event)!=NX_OK ||
                !settings_frame(scene,info.width,info.height)) { okay=0; break; }
            ax_test_ready=1;
        }
        nx_result ax=nx_window_publish_semantics(window,settings_context(scene));
        if (ax!=NX_OK && ax!=NX_ERROR_UNSUPPORTED) { okay=0; break; }
        ax_unavailable=ax==NX_ERROR_UNSUPPORTED;
        if (ax_unavailable) {
            double now=seconds(CLOCK_MONOTONIC); if(now<0) {okay=0;break;}
            next_ax_retry=now+1.0;
        }
        const nx_command *commands; size_t count;
        nx_pixel_size pixels;
        if (nx_window_get_pixel_size(window,&pixels)!=NX_OK || nx_frame_commands(settings_context(scene),&commands,&count)!=NX_OK) { okay=0; break; }
        nx_color bg; nx_render_outcome outcome;
        if (nx_theme_color(settings_context(scene),NX_COLOR_BACKGROUND,&bg)!=NX_OK ||
            nx_renderer_run(&renderer,(nx_render_target){pixels.width,pixels.height,info.scale},commands,count,bg,&outcome)!=NX_OK) { okay=0; break; }
        if (outcome==NX_RENDER_DEVICE_LOST && nx_renderer_recover(&renderer)!=NX_OK) { okay=0; break; }
        if (verify_keyboard_small && outcome==NX_RENDER_READY && (info.width!=240 || info.height!=150)) {
            fprintf(stderr,"Unexpected small-window size %.0fx%.0f\n",info.width,info.height);
            okay=0; break;
        }
        if (outcome==NX_RENDER_READY) ++presented;
        if (verify_keyboard && outcome==NX_RENDER_READY && !keyboard_ready) {
            puts("SETTINGS_KEYBOARD_READY"); fflush(stdout); keyboard_ready=1;
        }
        if (verify_ime && outcome==NX_RENDER_READY && !keyboard_ready) {
            puts("SETTINGS_IME_READY"); fflush(stdout); keyboard_ready=1;
        }
        if (verify_ime && outcome==NX_RENDER_READY) {
            uint64_t ime_focus=nx_focus_id(settings_context(scene));
            if (ime_focus==3 && !ime_focus_reported) { puts("SETTINGS_IME_FOCUSED"); fflush(stdout); ime_focus_reported=1; }
            uint32_t index; nx_semantic_view view; nx_utf8_span applied; int motion=0;
            char preedit[16]; size_t preedit_length=0; int32_t begin=0,end=0;
            if (nx_semantics_find(settings_context(scene),3,&index)!=NX_OK ||
                nx_semantics_get(settings_context(scene),index,&view)!=NX_OK ||
                !settings_applied(scene,&applied,&motion) ||
                nx_window_get_ime_preedit(window,preedit,sizeof(preedit),&preedit_length,&begin,&end)!=NX_OK ||
                applied.length!=7 || memcmp(applied.data,"Aetheri",7) || motion) { okay=0; break; }
            if (!ime_stage && ime_focus==3 && preedit_length==1 && preedit[0]=='\'') {
                if (view.value.length!=7 || memcmp(view.value.data,"Aetheri",7)) { okay=0; break; }
                puts("SETTINGS_IME_PREEDIT"); fflush(stdout); ime_stage=1;
            } else if ((verify_ime_cancel || (verify_ime_control && ime_focus==4)) && ime_stage==1 && !preedit_length &&
                       view.value.length==7 && !memcmp(view.value.data,"Aetheri",7)) {
                puts("SETTINGS_IME_CANCELLED"); fflush(stdout); ime_stage=2;
            } else if (verify_ime_control && ime_stage==2 && ime_focus==3 && !preedit_length) {
                puts("SETTINGS_IME_REFOCUSED"); fflush(stdout); ime_stage=3;
            } else if ((verify_ime_cancel && ime_stage==2) || (verify_ime_control && ime_stage==3)) {
                if (!preedit_length && ime_focus==3 &&
                       view.value.length==8 && !memcmp(view.value.data,"Aetheria",8)) {
                    puts("SETTINGS_IME_AFTER_RETURN"); fflush(stdout); ime_stage=verify_ime_control?4:3; break;
                }
            } else if ((verify_ime_compose || verify_ime_replace) && ime_stage==1 && !preedit_length &&
                       ((verify_ime_compose && view.value.length==9 && !memcmp(view.value.data,"Aetheri\xc3\xa1",9)) ||
                        (verify_ime_replace && view.value.length==6 && !memcmp(view.value.data,"A\xc3\xa1" "eri",6)))) {
                puts("SETTINGS_IME_COMMITTED"); fflush(stdout); ime_stage=2; break;
            }
        }
        /* The scene learns the new focus while building this frame. Give it
           one further frame to reveal the focused control before going idle. */
        int probe_redraw=settings_take_accessibility_probe_redraw(scene);
        dirty=outcome!=NX_RENDER_READY || previous_focus!=nx_focus_id(settings_context(scene)) || probe_redraw || scheduled_now;
        if (verify_ax || verify_keyboard) {
            nx_utf8_span applied; int motion=0;
            if (!settings_applied(scene,&applied,&motion)) { okay=0; break; }
            if (verify_keyboard) {
                uint32_t index; nx_semantic_view view;
                if (nx_semantics_find(settings_context(scene),3,&index)!=NX_OK ||
                    nx_semantics_get(settings_context(scene),index,&view)!=NX_OK) {okay=0;break;}
                printf("KEYBOARD_TRACE routed=%zu focus=%llu draft_bytes=%zu applied_bytes=%zu motion=%d\n",
                    routed,(unsigned long long)nx_focus_id(settings_context(scene)),view.value.length,applied.length,motion);
                fflush(stdout);
            }
            if (verify_keyboard_small && previous_focus==nx_focus_id(settings_context(scene))) {
                uint64_t focus=nx_focus_id(settings_context(scene));
                uint32_t index; nx_semantic_view focused;
                if (focus>=3 && focus<=5 && nx_semantics_find(settings_context(scene),focus,&index)==NX_OK &&
                    nx_semantics_get(settings_context(scene),index,&focused)==NX_OK &&
                    focused.bounds.y>=70 && focused.bounds.y+focused.bounds.height<=info.height-16) {
                    unsigned bit=1u<<(focus-3);
                    if (!(settled_focuses&bit)) {
                        settled_focuses|=bit;
                        printf("SETTINGS_FOCUS_VISIBLE %llu\n",(unsigned long long)focus);
                        fflush(stdout);
                    }
                }
            }
            const char *expected=verify_ax?"AT-SPI Tokyo":
                (verify_keyboard_text || verify_keyboard_small)?"Ada":"Aetheri";
            size_t expected_length=strlen(expected);
            if (motion && applied.length==expected_length && !memcmp(applied.data,expected,expected_length)) {
                if (verify_keyboard_small && settled_focuses!=7) {
                    fprintf(stderr,"Focus did not settle visibly before activation: %u\n",settled_focuses);
                    okay=0;
                }
                break;
            }
        }
    }
    if (smoke && !presented) okay=0;
cleanup:
    if (opened) {
        nx_result r=nx_renderer_close(&renderer);
        for (unsigned i=0; r==NX_ERROR_LIMIT && i<4; ++i) {
            if (nx_window_poll(window,16)!=NX_OK) break;
            r=nx_renderer_close(&renderer);
        }
        if (r!=NX_OK) { fputs("Renderer retirement incomplete\n",stderr); return 1; }
    }
    settings_destroy(scene); nx_window_destroy(window); free(font);
    if (verify_ime && okay && (((verify_ime_compose || verify_ime_replace) && ime_stage==2) ||
                               (verify_ime_cancel && ime_stage==3) ||
                               (verify_ime_control && ime_stage==4))) { puts("SETTINGS_IME_CLOSED"); fflush(stdout); }
    else if (verify_ime) okay=0;
    if (idle_probe && okay) printf("idle elapsed=%.6f cpu=%.6f polls=%u presents=%u long_waits=%u\n",elapsed,cpu_elapsed,polls,presented,idle_long_waits);
    if (!okay) fputs("Settings example failed\n",stderr);
    return okay ? 0 : 1;
}
