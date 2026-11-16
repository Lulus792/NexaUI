#include "view.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
struct physim_view {
    physim_snapshot *model;
    ps_plot_view *plot;
    const nx_font_collection *fonts;
    nx_numeric_field *length,*angle;
    nx_viewport_surface *surface;
    nx_context *canvas;
    uint8_t *pixels;
    uint32_t width,height;
    double backing;
    int leased;
    nx_plot_state plot_state;
    nx_series_envelope scratch[1024];
};
static nx_utf8_span span(const char *text) { return (nx_utf8_span){text,strlen(text)}; }
static nx_result resize(void *user,uint32_t width,uint32_t height) {
    physim_view *v=user;
    if(v->leased) return NX_ERROR_STATE;
    if(!width||!height) { free(v->pixels);v->pixels=NULL;v->width=v->height=0;return NX_OK; }
    if((size_t)width>16u*1024u*1024u/4u/height) return NX_ERROR_LIMIT;
    uint8_t *pixels=malloc((size_t)width*height*4u);if(!pixels)return NX_ERROR_MEMORY;
    free(v->pixels);v->pixels=pixels;v->width=width;v->height=height;return NX_OK;
}
static nx_color color(uint32_t rgba) {
    return (nx_color){(float)((rgba>>24)&255u)/255.0f,(float)((rgba>>16)&255u)/255.0f,
        (float)((rgba>>8)&255u)/255.0f,(float)(rgba&255u)/255.0f};
}
static nx_point project(const physim_view *v,ps_vec3 point) {
    double scale=(double)v->height/(v->model->length.value*1.8);
    return (nx_point){(float)((double)v->width*.5+point.x*scale),(float)((double)v->height*.16-point.y*scale)};
}
static nx_result render_scene(void *user,nx_bitmap_view *out) {
    physim_view *v=user;
    if(v->leased||!v->pixels) return NX_ERROR_STATE;
    if(nx_begin_frame(v->canvas,(float)v->width,(float)v->height,0)!=NX_OK) return NX_ERROR_STATE;
    double scale=(double)v->height/(v->model->length.value*1.8);
    for(uint32_t i=0;i<v->model->scene.count;++i) {
        const ps_object *object=&v->model->scene.objects[i];
        nx_point a=project(v,object->a),b=project(v,object->b);nx_result r=NX_OK;
        if(object->shape==PS_LINE||object->shape==PS_ARROW) {
            r=nx_draw_line(v->canvas,a,b,(float)(2*v->backing),color(object->color));
            if(r==NX_OK&&object->shape==PS_ARROW) {
                float dx=b.x-a.x,dy=b.y-a.y,length=hypotf(dx,dy);
                if(length>0) {
                    float size=fminf((float)(10*v->backing),length*.3f),ux=dx/length,uy=dy/length;
                    r=nx_fill_triangle(v->canvas,b,(nx_point){b.x-size*ux-size*.5f*uy,b.y-size*uy+size*.5f*ux},
                        (nx_point){b.x-size*ux+size*.5f*uy,b.y-size*uy-size*.5f*ux},color(object->color));
                }
            }
        } else if(object->shape==PS_SPHERE) {
            float radius=(float)(object->radius*scale);
            r=nx_fill_round_rect(v->canvas,(nx_rect){a.x-radius,a.y-radius,2*radius,2*radius},radius,color(object->color));
        } else if(object->shape==PS_LABEL) {
            nx_text_layout *layout=NULL;
            r=nx_text_layout_create(v->fonts,object->text,strlen(object->text),NX_BIDI_AUTO,12*v->backing,
                (double)v->width,&layout);
            if(r==NX_OK) r=nx_draw_text_layout(v->canvas,layout,(nx_point){a.x+(float)(6*v->backing),a.y+(float)(5*v->backing)},color(object->color),65536);
            nx_text_layout_destroy(layout);
        } else if(object->shape!=PS_GROUP) return NX_ERROR_UNSUPPORTED;
        if(r!=NX_OK) return r;
    }
    const nx_command *commands=NULL;size_t count=0;
    nx_result r=nx_end_frame(v->canvas);
    if(r==NX_OK)r=nx_frame_commands(v->canvas,&commands,&count);
    if(r==NX_OK)r=nx_headless_render(commands,count,v->pixels,v->width,v->height,(size_t)v->width*4u,(nx_color){.035f,.055f,.09f,1});
    if(r!=NX_OK)return r;
    v->leased=1;*out=(nx_bitmap_view){v->pixels,v->width,v->height,(size_t)v->width*4u,NX_BITMAP_RGBA8};return NX_OK;
}
static nx_result acquire(void *user,nx_bitmap_view *out) {
    physim_view *v=user;
    if(!v->canvas) {
        nx_result r=nx_context_create(NULL,&v->canvas);if(r!=NX_OK)return r;
    }
    nx_result r=render_scene(user,out);
    if(r!=NX_OK) { nx_context_destroy(v->canvas);v->canvas=NULL; }
    return r;
}
static void release(void *user) { ((physim_view *)user)->leased=0; }
nx_result physim_view_create(physim_snapshot *model,ps_plot_view *plot,const nx_font_collection *fonts,physim_view **out) {
    if(!out)return NX_ERROR_ARGUMENT;*out=NULL;
    if(!model||!plot||!fonts||!model->generation||!model->length.defined||!model->angle.defined)return NX_ERROR_ARGUMENT;
    physim_view *v=calloc(1,sizeof(*v));if(!v)return NX_ERROR_MEMORY;
    v->model=model;v->plot=plot;v->fonts=fonts;
    nx_result r=nx_numeric_field_create(model->length.value,8192,&v->length);
    if(r==NX_OK)r=nx_numeric_field_create(model->angle.value,8192,&v->angle);
    if(r==NX_OK)r=nx_context_create(NULL,&v->canvas);
    nx_viewport_source source={v,resize,acquire,release};
    if(r==NX_OK)r=nx_viewport_surface_create(&source,NULL,&v->surface);
    if(r!=NX_OK) { physim_view_destroy(&v);return r; }
    if(!plot->initialized)ps_plot_view_reset(plot);
    *out=v;return NX_OK;
}
static nx_result text(physim_view *v,nx_context *context,uint64_t id,const char *s,nx_rect bounds,double size) {
    nx_text_layout *layout=NULL;
    nx_result r=nx_text_layout_create(v->fonts,s,strlen(s),NX_BIDI_AUTO,size,bounds.width,&layout);
    if(r==NX_OK)r=nx_label(context,id,1,bounds,layout,(nx_point){bounds.x,bounds.y},(nx_color){.08f,.1f,.15f,1},65536);
    nx_text_layout_destroy(layout);return r;
}
static nx_result button(physim_view *v,nx_context *context,unsigned id,const char *s,float x,unsigned *action) {
    nx_text_layout *layout=NULL;
    nx_result r=nx_text_layout_create(v->fonts,s,strlen(s),NX_BIDI_AUTO,14,92,&layout);
    if(r==NX_OK) {
        nx_control_desc desc={.id=id,.parent_id=1,.bounds={x,108,104,32},.name=span(s),
            .label_layout=layout,.label_origin={x+6,116},.glyph_cache_bytes=65536};
        nx_interaction_state state;r=nx_button(context,&desc,&state);
        if(r==NX_OK&&state.clicked)*action=id;
    }
    nx_text_layout_destroy(layout);return r;
}
static nx_result sample(void *user,size_t index,nx_series_point *point) {
    const physim_snapshot *model=user;
    if(index>=PHYSIM_SAMPLE_COUNT||!point)return NX_ERROR_ARGUMENT;
    *point=(nx_series_point){model->samples[index].time,model->samples[index].values[0]};return NX_OK;
}
nx_result physim_view_frame(physim_view *v,nx_context *context,float width,float height,double scale,unsigned *action) {
    if(!v||!context||!action||!isfinite(width)||!isfinite(height)||width<640||width>2048||height<400||height>1200||
        !isfinite(scale)||scale<.5||scale>2)return NX_ERROR_ARGUMENT;
    *action=0;if(v->leased)return NX_ERROR_STATE;
    nx_result r=nx_begin_frame(context,width,height,0);
    nx_semantic_desc root={.id=1,.role=NX_ROLE_WINDOW,.bounds={0,0,width,height},.name=span("Original Physim pendulum reference")};
    if(r==NX_OK)r=nx_semantics_add(context,&root,NULL);
    if(r==NX_OK)r=text(v,context,2,"Original Physim pendulum - installed NexaUI reference",(nx_rect){16,12,width-32,26},18);
    if(r==NX_OK)r=text(v,context,3,"Length (m)",(nx_rect){16,42,200,18},14);
    if(r==NX_OK)r=text(v,context,4,"Initial angle (rad)",(nx_rect){240,42,200,18},14);
    nx_numeric_field_desc field={.id=10,.parent_id=1,.bounds={16,64,200,32},.name=span("Length (m)"),
        .fonts=v->fonts,.pixels_per_em=16,.glyph_cache_bytes=65536,.minimum=v->model->length.minimum,.maximum=v->model->length.maximum};
    nx_interaction_state state;int valid=0;
    if(r==NX_OK)r=nx_numeric_field_widget(context,&field,v->length,&state,&valid);
    field.id=11;field.bounds.x=240;field.name=span("Initial angle (rad)");field.minimum=v->model->angle.minimum;field.maximum=v->model->angle.maximum;
    if(r==NX_OK)r=nx_numeric_field_widget(context,&field,v->angle,&state,&valid);
    static const char *names[]={"Apply model","Zoom +","Pan right","Fit data","Close"};
    for(unsigned i=0;r==NX_OK&&i<5;++i)r=button(v,context,12u+i,names[i],16+112*(float)i,action);
    nx_plot_desc plot={.id=20,.parent_id=1,.bounds={16,160,width*.5f-24,height-176},.name=span("Original angle channel (rad)"),
        .series={PHYSIM_SAMPLE_COUNT,v->model,sample},.scratch=v->scratch,.scratch_capacity=1024,.fonts=v->fonts,
        .pixels_per_em=13,.glyph_cache_bytes=65536};
    plot.x_min=ps_plot_axis_value(0,v->model->samples[PHYSIM_SAMPLE_COUNT-1u].time,v->plot->low[0]);
    plot.x_max=ps_plot_axis_value(0,v->model->samples[PHYSIM_SAMPLE_COUNT-1u].time,v->plot->high[0]);
    plot.y_min=ps_plot_axis_value(-1.6,1.6,v->plot->low[1]);plot.y_max=ps_plot_axis_value(-1.6,1.6,v->plot->high[1]);
    if(r==NX_OK)r=nx_plot(context,&plot,&v->plot_state);
    nx_viewport_desc viewport={.id=21,.parent_id=1,.bounds={width*.5f+8,160,width*.5f-24,height-176},
        .name=span("Original Physim pendulum scene"),.scale=scale};
    v->backing=scale;
    if(r==NX_OK)r=nx_viewport(context,v->surface,&viewport,&state);
    if(r==NX_OK)r=nx_end_frame(context);
    return r;
}
nx_result physim_view_complete(physim_view *v) { return v?nx_viewport_surface_complete(v->surface):NX_ERROR_ARGUMENT; }
nx_result physim_view_dispatch(physim_view *v,unsigned action,int *valid) {
    if(valid)*valid=0;if(!v||!valid)return NX_ERROR_ARGUMENT;
    if(v->leased)return NX_ERROR_STATE;
    if(action==12) {
        if(v->model->generation==UINT64_MAX)return NX_ERROR_LIMIT;
        int length_valid=0,angle_valid=0;
        nx_result r=nx_numeric_field_commit(v->length,v->model->length.minimum,v->model->length.maximum,&length_valid);
        if(r==NX_OK)r=nx_numeric_field_commit(v->angle,v->model->angle.minimum,v->model->angle.maximum,&angle_valid);
        if(r!=NX_OK||!length_valid||!angle_valid)return r;
        double length=0,angle=0;
        if((r=nx_numeric_field_value(v->length,&length))!=NX_OK||(r=nx_numeric_field_value(v->angle,&angle))!=NX_OK)return r;
        r=physim_model_run(length,angle,v->model->generation+1u,v->model);
        if(r==NX_OK)*valid=1;return r;
    }
    if(action==13)*valid=ps_plot_view_zoom(v->plot,2,.5,.5);
    else if(action==14)*valid=ps_plot_view_pan(v->plot,.25,0);
    else if(action==15) { ps_plot_view_reset(v->plot);*valid=1; }
    else if(action==16)*valid=1;
    else return NX_ERROR_ARGUMENT;
    return *valid?NX_OK:NX_ERROR_STATE;
}
nx_result physim_view_destroy(physim_view **view) {
    if(!view)return NX_ERROR_ARGUMENT;if(!*view)return NX_OK;physim_view *v=*view;
    if(v->leased)return NX_ERROR_STATE;
    if(v->surface) { nx_result r=nx_viewport_surface_destroy(v->surface);if(r!=NX_OK)return r; }
    nx_numeric_field_destroy(v->length);nx_numeric_field_destroy(v->angle);nx_context_destroy(v->canvas);
    free(v->pixels);free(v);*view=NULL;return NX_OK;
}
