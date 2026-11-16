#include "view.h"
#include <math.h>
#include <stdio.h>
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
    int leased,apply_rejected;
    uint32_t inspected;
    uint64_t pick_generation;
    float pick_width,pick_height;
    double pick_scale;
    uint32_t pick_index;
    int pick_armed,pick_pending;
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
static double projection_scale(const physim_view *v) {
    return (double)v->height/(v->model->length.value*1.8);
}
static nx_point project(const physim_view *v,ps_vec3 point,double scale) {
    return (nx_point){(float)((double)v->width*.5+point.x*scale),(float)((double)v->height*.16-point.y*scale)};
}
static int arrow_head(nx_point a,nx_point b,double backing,nx_point *left,nx_point *right) {
    float dx=b.x-a.x,dy=b.y-a.y,length=hypotf(dx,dy);
    if(!(length>0))return 0;
    float size=fminf((float)(10*backing),length*.3f),ux=dx/length,uy=dy/length;
    *left=(nx_point){b.x-size*ux-size*.5f*uy,b.y-size*uy+size*.5f*ux};
    *right=(nx_point){b.x-size*ux+size*.5f*uy,b.y-size*uy-size*.5f*ux};
    return 1;
}
static float edge(nx_point a,nx_point b,nx_point p) {
    return (b.x-a.x)*(p.y-a.y)-(b.y-a.y)*(p.x-a.x);
}
static int triangle_hit(nx_point a,nx_point b,nx_point c,nx_point point) {
    float x=edge(a,b,point),y=edge(b,c,point),z=edge(c,a,point);
    return (x>=0&&y>=0&&z>=0)||(x<=0&&y<=0&&z<=0);
}
static int stroke_hit(nx_point a,nx_point b,nx_point point,float radius) {
    float dx=b.x-a.x,dy=b.y-a.y,squared=dx*dx+dy*dy;
    float along=squared>0?((point.x-a.x)*dx+(point.y-a.y)*dy)/squared:0;
    along=fminf(1,fmaxf(0,along));
    return hypotf(point.x-(a.x+along*dx),point.y-(a.y+along*dy))<=radius;
}
static uint32_t pick_scene(const physim_view *v,nx_rect bounds,float x,float y) {
    nx_point point={(x-bounds.x)*(float)v->width/bounds.width,(y-bounds.y)*(float)v->height/bounds.height};
    double scale=projection_scale(v);
    for(uint32_t i=v->model->scene.count;i>0;--i) {
        const ps_object *object=&v->model->scene.objects[i-1u];
        if(!(object->color&255u))continue;
        nx_point a=project(v,object->a,scale);
        if(object->shape==PS_SPHERE&&hypotf(point.x-a.x,point.y-a.y)<=(float)(object->radius*scale))return i;
        if(object->shape==PS_LINE||object->shape==PS_ARROW) {
            nx_point b=project(v,object->b,scale),left,right;
            if(stroke_hit(a,b,point,(float)v->backing))return i;
            if(object->shape==PS_ARROW&&arrow_head(a,b,v->backing,&left,&right)&&triangle_hit(b,left,right,point))return i;
        }
    }
    return 0;
}
static void stage_pick(physim_view *v,nx_rect bounds,const nx_interaction_state *state,unsigned *action) {
    /* NexaUI routes capture against the preceding layout. Discard presses
       spanning a change to the projected image or its mapping. */
    if(v->pick_generation!=v->model->generation||v->pick_width!=bounds.width||
        v->pick_height!=bounds.height||v->pick_scale!=v->backing) {
        v->pick_generation=v->model->generation;
        v->pick_width=bounds.width;
        v->pick_height=bounds.height;
        v->pick_scale=v->backing;
        v->pick_armed=0;
        return;
    }
    if(state->pointer_pressed)v->pick_armed=1;
    if(!*action&&v->pick_armed&&state->pointer_released&&!state->held&&state->clicked&&
        state->pointer_x>=bounds.x&&state->pointer_y>=bounds.y&&
        state->pointer_x<bounds.x+bounds.width&&state->pointer_y<bounds.y+bounds.height) {
        v->pick_index=pick_scene(v,bounds,state->pointer_x,state->pointer_y);
        v->pick_pending=1;
        *action=PHYSIM_INSPECT_PICK;
    }
    if(!state->held)v->pick_armed=0;
}
static nx_result render_scene(void *user,nx_bitmap_view *out) {
    physim_view *v=user;
    if(v->leased||!v->pixels) return NX_ERROR_STATE;
    if(nx_begin_frame(v->canvas,(float)v->width,(float)v->height,0)!=NX_OK) return NX_ERROR_STATE;
    double scale=projection_scale(v);
    for(uint32_t i=0;i<v->model->scene.count;++i) {
        const ps_object *object=&v->model->scene.objects[i];
        nx_point a=project(v,object->a,scale),b=project(v,object->b,scale);nx_result r=NX_OK;
        if(object->shape==PS_LINE||object->shape==PS_ARROW) {
            r=nx_draw_line(v->canvas,a,b,(float)(2*v->backing),color(object->color));
            nx_point left,right;
            if(r==NX_OK&&object->shape==PS_ARROW&&arrow_head(a,b,v->backing,&left,&right))
                r=nx_fill_triangle(v->canvas,b,left,right,color(object->color));
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
static nx_result inspector(physim_view *v,nx_context *context,float width) {
    const ps_scene *scene=&v->model->scene;
    char summary[192];
    if (!v->inspected) {
        snprintf(summary,sizeof(summary),"Scene objects (%u)",(unsigned)scene->count);
    } else {
        const ps_object *object=&scene->objects[v->inspected-1u];
        static const char *const shape_names[]={
            [PS_SPHERE]="Sphere", [PS_LINE]="Line", [PS_ARROW]="Arrow",
            [PS_LABEL]="Label", [PS_GROUP]="Group"
        };
        const char *shape=object->shape<sizeof(shape_names)/sizeof(shape_names[0])
            ?shape_names[object->shape]:NULL;
        const char *name=object->text[0]?object->text:shape?shape:"Object";
        char parent[32]="root";
        if (object->parent_id) snprintf(parent,sizeof(parent),"#%u",(unsigned)object->parent_id);
        snprintf(summary,sizeof(summary),"%u/%u %s #%u, parent %s",(unsigned)v->inspected,
            (unsigned)scene->count,name,(unsigned)object->id,parent);
    }
    return text(v,context,23,summary,(nx_rect){width*.5f+8,142,width*.5f-24,16},12);
}
nx_result physim_view_set_ime_window(physim_view *v,nx_window *window) {
    if(!v)return NX_ERROR_ARGUMENT;if(v->leased)return NX_ERROR_STATE;
    nx_result r=nx_numeric_field_set_ime_window(v->length,window);
    if(r==NX_OK)r=nx_numeric_field_set_ime_window(v->angle,window);
    return r;
}
static nx_result button(physim_view *v,nx_context *context,unsigned id,const char *s,nx_rect bounds,unsigned *action) {
    nx_text_layout *layout=NULL;
    nx_result r=nx_text_layout_create(v->fonts,s,strlen(s),NX_BIDI_AUTO,14,bounds.width-12,&layout);
    if(r==NX_OK) {
        nx_control_desc desc={.id=id,.parent_id=1,.bounds=bounds,.name=span(s),
            .label_layout=layout,.label_origin={bounds.x+6,bounds.y+8},.glyph_cache_bytes=65536};
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
static int valid_view_size(double width,double height,double scale) {
    return isfinite(width) && isfinite(height) && width>=640 && width<=2048 &&
        height>=400 && height<=1200 && isfinite(scale) && scale>=.5 && scale<=2;
}
nx_result physim_view_frame(physim_view *v,nx_context *context,float width,float height,double scale,unsigned *action) {
    if(!v||!context||!action||!valid_view_size(width,height,scale))return NX_ERROR_ARGUMENT;
    *action=0;if(v->leased)return NX_ERROR_STATE;
    v->pick_pending=0;
    nx_result r=nx_begin_frame(context,width,height,0);
    nx_semantic_desc root={.id=1,.role=NX_ROLE_WINDOW,.bounds={0,0,width,height},.name=span("Original Physim pendulum reference")};
    if(r==NX_OK)r=nx_semantics_add(context,&root,NULL);
    if(r==NX_OK)r=text(v,context,2,"Original Physim pendulum",(nx_rect){16,12,width-164,26},18);
    if(r==NX_OK)r=text(v,context,3,"Length (m)",(nx_rect){16,42,200,18},14);
    if(r==NX_OK)r=text(v,context,4,"Initial angle (rad)",(nx_rect){240,42,200,18},14);
    nx_numeric_field_desc field={.id=10,.parent_id=1,.bounds={16,64,200,32},.name=span("Length (m)"),
        .fonts=v->fonts,.pixels_per_em=16,.glyph_cache_bytes=65536,.minimum=v->model->length.minimum,.maximum=v->model->length.maximum};
    nx_interaction_state state;int valid=0;
    if(r==NX_OK)r=nx_numeric_field_widget(context,&field,v->length,&state,&valid);
    field.id=11;field.bounds.x=240;field.name=span("Initial angle (rad)");field.minimum=v->model->angle.minimum;field.maximum=v->model->angle.maximum;
    if(r==NX_OK)r=nx_numeric_field_widget(context,&field,v->angle,&state,&valid);
    if(r==NX_OK)r=button(v,context,17,"Replace view",(nx_rect){464,64,160,32},action);
    static const char *names[]={"Apply model","Zoom +","Pan right","Fit data","Close"};
    for(unsigned i=0;r==NX_OK&&i<5;++i)r=button(v,context,12u+i,names[i],(nx_rect){16+112*(float)i,108,104,32},action);
    char status[128];
    if(v->apply_rejected)snprintf(status,sizeof(status),"Invalid parameters; model generation %llu retained.",(unsigned long long)v->model->generation);
    else snprintf(status,sizeof(status),"Model generation %llu; length %.6g m.",(unsigned long long)v->model->generation,v->model->length.value);
    if(r==NX_OK)r=text(v,context,5,status,(nx_rect){16,142,width*.5f-24,16},12);
    if(r==NX_OK)r=inspector(v,context,width);
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
    if(r==NX_OK&&v->model->scene.count)
        r=button(v,context,PHYSIM_INSPECT_NEXT,"Inspect next",(nx_rect){width-140,8,124,32},action);
    if(r==NX_OK)r=nx_end_frame(context);
    if(r==NX_OK)stage_pick(v,viewport.bounds,&state,action);
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
        if(r!=NX_OK)return r;
        if(!length_valid||!angle_valid) { v->apply_rejected=1;return NX_OK; }
        double length=0,angle=0;
        if((r=nx_numeric_field_value(v->length,&length))!=NX_OK||(r=nx_numeric_field_value(v->angle,&angle))!=NX_OK)return r;
        r=physim_model_run(length,angle,v->model->generation+1u,v->model);
        if(r==NX_OK) { *valid=1;v->apply_rejected=0;v->inspected=0;v->pick_pending=0; }return r;
    }
    if(action==13)*valid=ps_plot_view_zoom(v->plot,2,.5,.5);
    else if(action==14)*valid=ps_plot_view_pan(v->plot,.25,0);
    else if(action==15) { ps_plot_view_reset(v->plot);*valid=1; }
    else if(action==16)*valid=1;
    else if(action==17)return NX_ERROR_UNSUPPORTED; /* Owner rebuilds the view. */
    else if(action==PHYSIM_INSPECT_NEXT&&v->model->scene.count) {
        v->inspected=v->inspected%v->model->scene.count+1u;
        *valid=1;
    }
    else if(action==PHYSIM_INSPECT_PICK) {
        if(!v->pick_pending)return NX_ERROR_STATE;
        v->inspected=v->pick_index;
        v->pick_pending=0;
        *valid=1;
    }
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

nx_result physim_export_render(const physim_snapshot *model, const ps_plot_view *plot,
    const nx_font_collection *fonts, uint32_t width, uint32_t height, double scale,
    physim_export_image *out) {
    if (!model || !plot || !fonts || !out || out->pixels ||
        !valid_view_size(width,height,scale) || !ps_scene_valid(&model->scene))
        return NX_ERROR_ARGUMENT;
    physim_export_image image = {
        .width = (uint32_t)ceil((double)width * scale),
        .height = (uint32_t)ceil((double)height * scale),
    };
    image.stride = (size_t)image.width * 4u;
    if (image.stride > (40u * 1024u * 1024u) / image.height) return NX_ERROR_LIMIT;

    /* The interactive view can edit its model and plot. Give the export its
       own copies and context so it cannot consume live input or alter a lease. */
    physim_snapshot *snapshot = malloc(sizeof(*snapshot));
    if (!snapshot) return NX_ERROR_MEMORY;
    *snapshot = *model;
    ps_plot_view range = *plot;
    nx_context *context = NULL;
    physim_view *view = NULL;
    nx_result result = physim_view_create(snapshot,&range,fonts,&view);
    if (result == NX_OK) result = nx_context_create(NULL,&context);
    if (result == NX_OK) result = nx_context_set_text_raster_scale(context,(float)scale);
    unsigned action = 0;
    if (result == NX_OK)
        result = physim_view_frame(view,context,(float)width,(float)height,scale,&action);
    const nx_command *commands = NULL;
    size_t count = 0;
    if (result == NX_OK) result = nx_frame_commands(context,&commands,&count);
    if (result == NX_OK) {
        image.pixels = malloc(image.stride * image.height);
        if (!image.pixels) result = NX_ERROR_MEMORY;
    }
    if (result == NX_OK)
        result = nx_headless_render_scaled(commands,count,image.pixels,image.width,
            image.height,image.stride,(nx_color){1,1,1,1},scale);
    if (view && view->leased) {
        nx_result completed = physim_view_complete(view);
        if (result == NX_OK) result = completed;
    }
    nx_result destroyed = physim_view_destroy(&view);
    if (result == NX_OK) result = destroyed;
    nx_context_destroy(context);
    free(snapshot);
    if (result == NX_OK) *out = image;
    else free(image.pixels);
    return result;
}

void physim_export_release(physim_export_image *image) {
    if (!image) return;
    free(image->pixels);
    *image = (physim_export_image){0};
}
