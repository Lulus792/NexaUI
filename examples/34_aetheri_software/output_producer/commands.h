/* Private inherited test-socket policy, never an installed compositor API. */
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/types/wlr_xdg_shell.h>
static bool sw_output_command(struct server *s,const char *text) {
    struct window *w,*only=NULL;
    wl_list_for_each(w,&s->windows,link) if(w->mapped) { if(only) return false;only=w; }
    if(!only) return false;
    unsigned index=0,value=0;int used=0;
    if(sscanf(text,"Z %u%n",&value,&used)==1 && text[used]==0 && value<=1) {
        ae_server_maximize(only,value!=0);
    } else if(sscanf(text,"V %u%n",&index,&used)==1 && text[used]==0 && index<s->output_count) {
        struct output *o=&s->outputs[index];
        struct wlr_output_layout_output *place=o->output?wlr_output_layout_get(s->layout,o->output):NULL;
        if(!place || only->maximized || only->is_fullscreen) return false;
        only->home=index;only->x=place->x+40;only->y=place->y+40;ae_server_configure(only);
    } else if(sscanf(text,"P %u %u%n",&index,&value,&used)==2 && text[used]==0 && index<s->output_count && value>=120 && value<=240) {
        struct output *o=&s->outputs[index];if(!o->output) return false;
        struct wlr_output_state requested;wlr_output_state_init(&requested);
        wlr_output_state_set_scale(&requested,(float)value/120.0f);
        struct wlr_output_event_request_state event={.output=o->output,.state=&requested};
        wl_signal_emit_mutable(&o->output->events.request_state,&event);wlr_output_state_finish(&requested);
        if(s->failed) return false;
    } else if(sscanf(text,"H %u %u%n",&index,&value,&used)==2 && text[used]==0 && index<s->output_count && value<=256) {
        struct output *o=&s->outputs[index];if(!o->output) return false;
        int width,height;wlr_output_effective_resolution(o->output,&width,&height);
        if(height-(int)value<96) return false;
        o->reserved_bottom=(int)value;if(ae_server_window_output(only)==o) ae_server_configure(only);
    } else return false;
    ae_input_recheck_pointer(s->input);
    struct output *o=ae_server_window_output(only);if(!o) return false;
    int width,height;wlr_output_effective_resolution(o->output,&width,&height);
    printf("OUTPUT_POLICY command=%s output=%u scale=%g logical=%dx%d reserved=%d max=%d x=%d y=%d\n",
        text,o->index,(double)o->output->scale,width,height,o->reserved_bottom,only->maximized,only->x,only->y);
    return true;
}
