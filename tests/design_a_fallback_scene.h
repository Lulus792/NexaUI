#ifndef NEXAUI_TEST_DESIGN_A_FALLBACK_SCENE_H
#define NEXAUI_TEST_DESIGN_A_FALLBACK_SCENE_H
#include "nexaui/nexaui.h"
#include <string.h>

/* Private geometry/semantics fixture, not a complete or font-backed app UI.
   Resources are made before frames and retained through renderer consumption. */
enum { DESIGN_STATUS=1900, DESIGN_CONTRAST, DESIGN_EFFECTS, DESIGN_PLOT };
typedef struct design_a_scene {
    nx_paint_image *card, *shadow;
    nx_plot_state plot;
    int high_contrast, reduced_effects, applied_contrast;
    unsigned rgba_commands;
    int value_guard; /* Native retained-pattern protection/capability probe. */
} design_a_scene;
static void design_a_free(design_a_scene *s) {
    nx_paint_image_destroy(s->card); nx_paint_image_destroy(s->shadow);
    s->card=NULL; s->shadow=NULL;
}
static int design_a_prepare(design_a_scene *s,float density) {
    nx_color_stop stops[]={{0,{0,.18f,.28f,1}},{1,{0,.04f,.08f,1}}};
    nx_surface_image_desc card={320,52,8,density,
        {NX_PAINT_LINEAR,{0},{0,0},{320,52},stops,2},4u*1024u*1024u};
    nx_shadow_image_desc shadow={320,52,8,density,2,{0,0},
        {0,.7f,1,.5f},4u*1024u*1024u};
    if(nx_surface_image_create(&card,NULL,&s->card)!=NX_OK) return 1;
    if(nx_shadow_image_create(&shadow,NULL,&s->shadow)!=NX_OK) {
        design_a_free(s); return 1;
    }
    return 0;
}
static nx_result design_a_sample(void *u,size_t i,nx_series_point *p) {
    *p=((const nx_series_point *)u)[i]; return NX_OK;
}
static nx_control_desc design_a_control(uint64_t id,const char *name,nx_rect r) {
    nx_control_desc d={0}; d.id=id; d.parent_id=1;
    d.name=(nx_utf8_span){name,strlen(name)}; d.bounds=r; return d;
}
static int design_a_frame(nx_context *ui,design_a_scene *s,float density) {
#define DA_TRY(x) do { if((x)!=NX_OK) return 1; } while(0)
    nx_theme theme;
    s->applied_contrast=!!s->high_contrast;
    DA_TRY(nx_theme_preset(s->applied_contrast?NX_THEME_HIGH_CONTRAST:NX_THEME_MIDNIGHT,&theme));
    DA_TRY(nx_context_set_theme(ui,&theme));
    DA_TRY(nx_begin_frame(ui,360,240,0));
    DA_TRY(nx_fill_rect(ui,(nx_rect){0,0,360,240},theme.colors[NX_COLOR_BACKGROUND]));
    nx_semantic_desc root={0}; root.id=1; root.role=NX_ROLE_WINDOW;
    root.name=(nx_utf8_span){"Design A fallback probe",23}; root.bounds=(nx_rect){0,0,360,240};
    DA_TRY(nx_semantics_add(ui,&root,NULL));
    nx_interaction_state interaction;
    nx_control_desc c=design_a_control(DESIGN_CONTRAST,"High contrast",(nx_rect){16,12,150,30});
    DA_TRY(nx_checkbox(ui,&c,&s->high_contrast,&interaction));
    c=design_a_control(DESIGN_EFFECTS,"Reduced effects",(nx_rect){188,12,156,30});
    DA_TRY(nx_checkbox(ui,&c,&s->reduced_effects,&interaction));
    int omit=s->applied_contrast||s->reduced_effects;
    if(!omit) {
        DA_TRY(nx_draw_paint_image(ui,s->shadow,(nx_point){20,52},(nx_color){1,1,1,1}));
        DA_TRY(nx_draw_paint_image(ui,s->card,(nx_point){20,52},(nx_color){1,1,1,1}));
    } else {
        DA_TRY(nx_fill_rect(ui,(nx_rect){20,52,320,52},theme.colors[NX_COLOR_SURFACE]));
        DA_TRY(nx_stroke_rect(ui,(nx_rect){20,52,320,52},2,theme.colors[NX_COLOR_BORDER]));
    }
    nx_semantic_desc status={0}; status.id=DESIGN_STATUS; status.parent_id=1;
    status.role=NX_ROLE_STATUS; status.bounds=(nx_rect){20,52,320,52};
    status.name=(nx_utf8_span){"Local WLAN connected; sample data",33};
    DA_TRY(nx_semantics_add(ui,&status,NULL));
    DA_TRY(nx_draw_icon(ui,NX_ICON_WIFI_SHIELD,(nx_rect){30,64,28,28},2,theme.colors[NX_COLOR_TEXT]));
    nx_series_point download[]={{0,1},{1,3},{2,2},{3,6},{4,2},{5,4}};
    nx_series_point upload[]={{0,.5},{1,1},{2,.5},{3,2},{4,1},{5,.5}};
    nx_series_view views[]={{6,download,design_a_sample},{6,upload,design_a_sample}};
    nx_color a=s->applied_contrast?theme.colors[NX_COLOR_TEXT]:(nx_color){0,.75f,1,1};
    nx_color b=s->applied_contrast?theme.colors[NX_COLOR_ACCENT]:(nx_color){.95f,.4f,1,1};
    nx_plot_layer layers[]={{{"Download",8},&views[0],1,a,{0,.4f,1,.8f},{0,.2f,1,0},0,1},
        {{"Upload",6},&views[1],1,b,{.7f,.1f,1,.8f},{.4f,0,.7f,0},0,1}};
    nx_series_envelope scratch[720]; nx_plot_multi_desc plot={0};
    plot.plot.id=DESIGN_PLOT; plot.plot.parent_id=1;
    plot.plot.name=(nx_utf8_span){"Transfer rates",14}; plot.plot.bounds=(nx_rect){20,116,320,108};
    plot.plot.x_max=5; plot.plot.y_max=8; plot.plot.scratch=scratch; plot.plot.scratch_capacity=720;
    plot.layers=layers; plot.layer_count=2; plot.x_unit=(nx_utf8_span){"s",1};
    plot.y_unit=(nx_utf8_span){"MB/s",4}; plot.raster_scale=density; plot.hide_fills=omit;
    DA_TRY(nx_plot_multi(ui,&plot,&s->plot));
    nx_semantic_desc guard={0};guard.id=1904;guard.parent_id=1;guard.role=NX_ROLE_CUSTOM;
    guard.name=(nx_utf8_span){"Value guard",11};guard.value=(nx_utf8_span){"fixture-value",13};
    guard.bounds=(nx_rect){60,64,100,24};guard.state=s->value_guard==1?NX_SEM_PROTECTED:0;
    guard.actions=s->value_guard==2?NX_ACTION_SET_VALUE:0;
    DA_TRY(nx_semantics_add(ui,&guard,NULL));
    DA_TRY(nx_end_frame(ui));
    const nx_command *commands; size_t count; s->rgba_commands=0;
    DA_TRY(nx_frame_commands(ui,&commands,&count));
    for(size_t i=0;i<count;++i)
        s->rgba_commands+=commands[i].type==NX_COMMAND_BITMAP&&commands[i].bitmap.format==NX_BITMAP_RGBA8;
    return 0;
#undef DA_TRY
}
#endif
