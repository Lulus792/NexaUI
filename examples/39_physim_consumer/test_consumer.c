#include "view.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"Physim consumer line %d: %s\n",__LINE__,#x);goto done; } } while(0)
#define WIDTH 800u
#define HEIGHT 560u
extern const ps_experiment_api *ps_get_experiment(void);
static int model_test(void) {
    int ok=0;physim_snapshot *model=malloc(sizeof(*model)),*before=malloc(sizeof(*before));
    ps_context *reference=calloc(1,sizeof(*reference));const ps_experiment_api *api=ps_get_experiment();
    CHECK(model&&before&&reference&&api);
    CHECK(physim_model_run(1.5,.45,1,model)==NX_OK);
    CHECK(model->length.minimum==.1&&model->length.maximum==10&&model->angle.minimum==-1.5&&model->angle.maximum==1.5);
    CHECK(!strcmp(model->length_unit.symbol,"m")&&!strcmp(model->angle_unit.symbol,"rad")&&model->length_unit.dimension[0]==1);
    reference->struct_size=sizeof(*reference);reference->api_version=PS_API_VERSION;
    CHECK(api->create(reference)==PS_OK&&ps_parameter_finalize(reference)==PS_OK);
    for(unsigned step=0;step<PHYSIM_SAMPLE_COUNT;++step) {
        if(step) { CHECK(api->step(reference,1.0/120.0)==PS_OK);reference->time_s+=1.0/120.0; }
        CHECK(model->samples[step].time==reference->time_s);
        for(unsigned c=0;c<9;++c)CHECK(model->samples[step].values[c]==reference->values[c]);
        CHECK(fabs(hypot(model->samples[step].values[2],model->samples[step].values[3])-1.5)<1e-12);
    }
    ps_scene scene={0};api->build_scene(reference,&scene);
    CHECK(scene.count==model->scene.count&&ps_scene_valid(&model->scene));
    for(uint32_t i=0;i<scene.count;++i) {
        CHECK(scene.objects[i].id==model->scene.objects[i].id&&scene.objects[i].parent_id==model->scene.objects[i].parent_id);
        CHECK(scene.objects[i].shape==model->scene.objects[i].shape&&scene.objects[i].a.x==model->scene.objects[i].a.x&&scene.objects[i].a.y==model->scene.objects[i].a.y);
    }
    *before=*model;
    CHECK(physim_model_run(.01,.45,2,model)==NX_ERROR_ARGUMENT&&!memcmp(before,model,sizeof(*model)));
    CHECK(physim_model_run(1.5,2,2,model)==NX_ERROR_ARGUMENT&&!memcmp(before,model,sizeof(*model)));
    CHECK(physim_model_run(NAN,.45,2,model)==NX_ERROR_ARGUMENT&&!memcmp(before,model,sizeof(*model)));
    CHECK(physim_model_run(1.5,.45,0,model)==NX_ERROR_ARGUMENT&&!memcmp(before,model,sizeof(*model)));
    CHECK(physim_model_run(.75,.25,2,model)==NX_OK&&model->generation==2&&model->length.value==.75&&model->angle.value==.25);
    CHECK(model->samples[120].values[0]!=before->samples[120].values[0]);
    for(unsigned i=0;i<PHYSIM_SAMPLE_COUNT;++i)CHECK(fabs(hypot(model->samples[i].values[2],model->samples[i].values[3])-.75)<1e-12);
    ok=1;
done:
    if(reference&&reference->user)api->destroy(reference);
    free(reference);free(model);free(before);return ok;
}
typedef struct font_data { uint8_t *bytes;nx_font *font;nx_font_collection *fonts; } font_data;
static int open_font(const char *path,font_data *font) {
    FILE *f=fopen(path,"rb");if(!f)return 0;
    long size=-1;if(!fseek(f,0,SEEK_END))size=ftell(f);
    if(size>0&&size<=16*1024*1024&&!fseek(f,0,SEEK_SET)) {
        font->bytes=malloc((size_t)size);
        if(font->bytes&&fread(font->bytes,1,(size_t)size,f)==(size_t)size&&nx_font_open(font->bytes,(size_t)size,NULL,&font->font)==NX_OK&&
            nx_font_collection_create(NULL,&font->fonts)==NX_OK&&nx_font_collection_add(font->fonts,font->font)==NX_OK) { fclose(f);return 1; }
    }
    fclose(f);return 0;
}
static void close_font(font_data *font) { nx_font_collection_destroy(font->fonts);nx_font_close(font->font);free(font->bytes); }
static nx_result render(physim_view *view,nx_context *context,double scale,unsigned *action,uint8_t *pixels) {
    nx_result r=physim_view_frame(view,context,WIDTH,HEIGHT,scale,action);
    const nx_command *commands=NULL;size_t count=0;
    if(r==NX_OK)r=nx_frame_commands(context,&commands,&count);
    if(r==NX_OK)r=nx_headless_render(commands,count,pixels,WIDTH,HEIGHT,WIDTH*4u,(nx_color){1,1,1,1});
    nx_result completed=physim_view_complete(view);
    return r!=NX_OK?r:completed;
}
static int key(nx_context *context,nx_key value,uint32_t modifiers) {
    nx_event e={.type=NX_EVENT_KEY_DOWN,.key=value,.modifiers=modifiers};
    if(nx_context_queue_event(context,&e)!=NX_OK)return 0;e.type=NX_EVENT_KEY_UP;return nx_context_queue_event(context,&e)==NX_OK;
}
static int type(nx_context *context,const char *text) {
    for(;*text;++text) { nx_event e={.type=NX_EVENT_TEXT,.scalar=(unsigned char)*text};if(nx_context_queue_event(context,&e)!=NX_OK)return 0; }
    return 1;
}
static int save_size(const char *path,const uint8_t *pixels,uint32_t width,uint32_t height) {
    FILE *f=fopen(path,"wbx");if(!f)return 0;
    int ok=fprintf(f,"P6\n%u %u\n255\n",width,height)>0;
    for(size_t i=0;ok&&i<(size_t)width*height;++i)ok=fwrite(pixels+i*4u,1,3,f)==3;
    if(fclose(f))ok=0;return ok;
}
static int save(const char *path,const uint8_t *pixels) { return save_size(path,pixels,WIDTH,HEIGHT); }
static int view_test(const char *path,const char *image) {
    int ok=0,valid=0;unsigned action=0;nx_context *context=NULL;physim_view *view=NULL;font_data font={0};ps_plot_view plot={0},reference;
    physim_snapshot *model=malloc(sizeof(*model)),*before=malloc(sizeof(*before));
    size_t bytes=(size_t)WIDTH*HEIGHT*4u;uint8_t *pixels=malloc(bytes),*previous=malloc(bytes);
    CHECK(model&&before&&pixels&&previous&&open_font(path,&font));
    CHECK(physim_model_run(1.5,.45,1,model)==NX_OK&&nx_context_create(NULL,&context)==NX_OK&&physim_view_create(model,&plot,font.fonts,&view)==NX_OK);
    *before=*model;
    CHECK(render(view,context,1,&action,pixels)==NX_OK&&!action&&!memcmp(before,model,sizeof(*model)));
    memcpy(previous,pixels,bytes);
    /* Original force arrows/labels are painted after the bob and may cover
       its center. Check substantial bob-colored coverage in its actual disk. */
    const ps_object *bob=NULL;for(uint32_t i=0;i<model->scene.count;++i)if(model->scene.objects[i].id==3)bob=&model->scene.objects[i];
    CHECK(bob&&bob->shape==PS_SPHERE);
    double projection=384.0/(1.5*1.8);
    int x=(int)(408.0+188.0+bob->a.x*projection),y=(int)(160.0+384.0*.16-bob->a.y*projection);
    int radius=(int)(bob->radius*projection);unsigned colored=0;
    CHECK(radius>0&&x>=radius&&y>=radius&&x+radius<(int)WIDTH&&y+radius<(int)HEIGHT);
    for(int dy=-radius;dy<=radius;++dy)for(int dx=-radius;dx<=radius;++dx) {
        if(dx*dx+dy*dy>radius*radius)continue;
        size_t p=((size_t)(y+dy)*WIDTH+(size_t)(x+dx))*4u;
        colored+=pixels[p]==0x53&&pixels[p+1u]==0xde&&pixels[p+2u]==0xc2;
    }
    CHECK(colored>(unsigned)(radius*radius));
    CHECK(physim_view_destroy(&view)==NX_OK&&physim_view_create(model,&plot,font.fonts,&view)==NX_OK);
    CHECK(render(view,context,1,&action,pixels)==NX_OK&&!memcmp(previous,pixels,bytes));
    reference=plot;
    CHECK(nx_focus_set(context,13)==NX_OK&&key(context,NX_KEY_ENTER,0)&&render(view,context,1,&action,pixels)==NX_OK&&action==13);
    CHECK(physim_view_dispatch(view,action,&valid)==NX_OK&&valid&&ps_plot_view_zoom(&reference,2,.5,.5));
    CHECK(!memcmp(&plot,&reference,sizeof(plot))&&!memcmp(before,model,sizeof(*model)));
    CHECK(physim_view_dispatch(view,14,&valid)==NX_OK&&valid&&ps_plot_view_pan(&reference,.25,0)&&!memcmp(&plot,&reference,sizeof(plot)));
    CHECK(physim_view_dispatch(view,15,&valid)==NX_OK&&valid&&!ps_plot_view_changed(&plot));
    CHECK(nx_focus_set(context,10)==NX_OK&&key(context,NX_KEY_A,NX_MOD_CONTROL)&&type(context,"0")&&key(context,NX_KEY_ENTER,0));
    CHECK(render(view,context,1,&action,pixels)==NX_OK&&physim_view_dispatch(view,12,&valid)==NX_OK&&!valid&&!memcmp(before,model,sizeof(*model)));
    CHECK(key(context,NX_KEY_A,NX_MOD_CONTROL)&&type(context,"2.5")&&key(context,NX_KEY_ENTER,0));
    CHECK(render(view,context,1,&action,pixels)==NX_OK&&physim_view_dispatch(view,12,&valid)==NX_OK&&valid&&model->generation==2&&model->length.value==2.5);
    CHECK(model->samples[120].values[0]!=before->samples[120].values[0]);
    CHECK(render(view,context,2,&action,pixels)==NX_OK&&memcmp(previous,pixels,bytes)&&save(image,pixels));
    CHECK(physim_view_destroy(&view)==NX_OK&&physim_view_create(model,&plot,font.fonts,&view)==NX_OK);
    CHECK(render(view,context,1,&action,pixels)==NX_OK&&model->length.value==2.5&&model->generation==2);
    ok=1;
done:
    if(view) { (void)physim_view_complete(view);physim_view_destroy(&view); }
    nx_context_destroy(context);close_font(&font);free(model);free(before);free(pixels);free(previous);return ok;
}
typedef struct allocation { size_t calls,live;size_t fail;int bad; } allocation;
static void *allocate(void *user,size_t bytes) {
    allocation *a=user;++a->calls;if(a->fail==a->calls)return NULL;
    void *p=malloc(bytes);if(p)a->live+=bytes;return p;
}
static void deallocate(void *user,void *p,size_t bytes) {
    allocation *a=user;if(bytes>a->live)a->bad=1;else a->live-=bytes;free(p);
}
static int failure_test(const char *path) {
    int ok=0,valid=0;unsigned action=0;nx_context *context=NULL;physim_view *view=NULL;font_data font={0};ps_plot_view plot={0};
    allocation a={0};physim_snapshot *model=malloc(sizeof(*model)),*before=malloc(sizeof(*before));
    uint8_t *pixels=malloc((size_t)WIDTH*HEIGHT*4u);
    CHECK(model&&before&&pixels&&open_font(path,&font));
    CHECK(physim_model_run(1.5,.45,1,model)==NX_OK&&physim_view_create(model,&plot,font.fonts,&view)==NX_OK&&nx_context_create(NULL,&context)==NX_OK);
    *before=*model;
    CHECK(physim_view_frame(view,context,WIDTH,HEIGHT,1,&action)==NX_OK);
    CHECK(physim_view_destroy(&view)==NX_ERROR_STATE&&view&&physim_view_dispatch(view,12,&valid)==NX_ERROR_STATE&&!valid);
    CHECK(physim_view_set_ime_window(view,NULL)==NX_ERROR_STATE);
    CHECK(physim_view_frame(view,context,WIDTH,HEIGHT,1,&action)==NX_ERROR_STATE&&!memcmp(before,model,sizeof(*model)));
    const nx_command *commands=NULL;size_t count=0;
    CHECK(nx_frame_commands(context,&commands,&count)==NX_OK&&nx_headless_render(commands,count,pixels,WIDTH,HEIGHT,WIDTH*4u,(nx_color){1,1,1,1})==NX_OK&&physim_view_complete(view)==NX_OK);
    CHECK(physim_view_frame(view,context,639,HEIGHT,1,&action)==NX_ERROR_ARGUMENT);
    CHECK(physim_view_frame(view,context,WIDTH,HEIGHT,NAN,&action)==NX_ERROR_ARGUMENT);
    nx_context_destroy(context);context=NULL;
    unsigned failed=0;
    for(size_t offset=1;offset<=12;++offset) {
        nx_context_desc desc={.allocator={&a,allocate,deallocate}};
        CHECK(nx_context_create(&desc,&context)==NX_OK);a.fail=a.calls+offset;
        nx_result r=physim_view_frame(view,context,WIDTH,HEIGHT,1,&action);
        if(r!=NX_OK) { CHECK(r==NX_ERROR_MEMORY);++failed; }
        (void)physim_view_complete(view);a.fail=0;nx_context_destroy(context);context=NULL;
        CHECK(!a.live&&!a.bad&&!memcmp(before,model,sizeof(*model)));
    }
    CHECK(failed>0&&nx_context_create(NULL,&context)==NX_OK&&render(view,context,1,&action,pixels)==NX_OK);
    CHECK(physim_view_destroy(&view)==NX_OK&&!memcmp(before,model,sizeof(*model)));
    printf("Physim frame allocation failures rejected: %u\n",failed);ok=1;
done:
    if(view) { (void)physim_view_complete(view);physim_view_destroy(&view); }
    nx_context_destroy(context);close_font(&font);free(model);free(before);free(pixels);return ok;
}
static int export_test(const char *path,const char *output) {
    const uint32_t width=641,height=401,physical_width=802,physical_height=502;
    const double scale=1.25;
    int ok=0;
    unsigned action=0;
    font_data font={0};
    ps_plot_view plot={0},saved_plot;
    physim_snapshot *model=malloc(sizeof(*model)),*saved=malloc(sizeof(*saved));
    physim_export_image image={0}, rejected={0};
    physim_view *live=NULL,*reference=NULL;
    nx_context *live_context=NULL,*reference_context=NULL;
    const nx_command *commands=NULL;
    size_t count=0;
    size_t live_bytes=(size_t)WIDTH*HEIGHT*4u;
    size_t image_bytes=(size_t)physical_width*physical_height*4u;
    uint8_t *before=malloc(live_bytes),*after=malloc(live_bytes),*expected=malloc(image_bytes);
    CHECK(model&&saved&&before&&after&&expected&&open_font(path,&font));
    CHECK(physim_model_run(2.5,.45,7,model)==NX_OK);
    ps_plot_view_reset(&plot);
    CHECK(ps_plot_view_zoom(&plot,2,.5,.5));
    saved_plot=plot;
    *saved=*model;
    CHECK(nx_context_create(NULL,&live_context)==NX_OK);
    CHECK(physim_view_create(model,&plot,font.fonts,&live)==NX_OK);
    CHECK(physim_view_frame(live,live_context,WIDTH,HEIGHT,1,&action)==NX_OK);
    CHECK(nx_frame_commands(live_context,&commands,&count)==NX_OK);
    CHECK(nx_headless_render(commands,count,before,WIDTH,HEIGHT,WIDTH*4u,(nx_color){1,1,1,1})==NX_OK);

    CHECK(physim_export_render(model,&plot,font.fonts,width,height,scale,&image)==NX_OK);
    CHECK(image.width==physical_width&&image.height==physical_height&&image.stride==physical_width*4u);
    uint8_t *first=image.pixels;
    CHECK(physim_export_render(model,&plot,font.fonts,width,height,scale,&image)==NX_ERROR_ARGUMENT&&image.pixels==first);
    CHECK(physim_export_render(model,&plot,font.fonts,639,height,scale,&rejected)==NX_ERROR_ARGUMENT);
    CHECK(physim_export_render(model,&plot,font.fonts,UINT32_MAX,height,scale,&rejected)==NX_ERROR_ARGUMENT);
    CHECK(physim_export_render(model,&plot,font.fonts,width,height,NAN,&rejected)==NX_ERROR_ARGUMENT);
    CHECK(!rejected.pixels&&!rejected.width&&!rejected.height&&!rejected.stride);
    CHECK(!memcmp(saved,model,sizeof(*model))&&!memcmp(&saved_plot,&plot,sizeof(plot)));
    CHECK(physim_view_destroy(&live)==NX_ERROR_STATE&&live);
    CHECK(nx_headless_render(commands,count,after,WIDTH,HEIGHT,WIDTH*4u,(nx_color){1,1,1,1})==NX_OK);
    CHECK(!memcmp(before,after,live_bytes));
    CHECK(physim_view_complete(live)==NX_OK);

    CHECK(nx_context_create(NULL,&reference_context)==NX_OK);
    CHECK(nx_context_set_text_raster_scale(reference_context,(float)scale)==NX_OK);
    CHECK(physim_view_create(saved,&saved_plot,font.fonts,&reference)==NX_OK);
    CHECK(physim_view_frame(reference,reference_context,(float)width,(float)height,scale,&action)==NX_OK&&!action);
    CHECK(nx_frame_commands(reference_context,&commands,&count)==NX_OK);
    CHECK(nx_headless_render_scaled(commands,count,expected,physical_width,physical_height,
        (size_t)physical_width*4u,(nx_color){1,1,1,1},scale)==NX_OK);
    CHECK(!memcmp(expected,image.pixels,image_bytes));
    CHECK(physim_view_complete(reference)==NX_OK);
    CHECK(physim_export_render(model,&plot,font.fonts,width,height,.5,&rejected)==NX_OK);
    CHECK(rejected.width==321&&rejected.height==201&&rejected.stride==321u*4u);
    CHECK(save_size(output,image.pixels,image.width,image.height));
    physim_export_release(&image);
    CHECK(!image.pixels&&!image.width&&!image.height&&!image.stride);
    ok=1;
done:
    if(live) { (void)physim_view_complete(live);physim_view_destroy(&live); }
    if(reference) { (void)physim_view_complete(reference);physim_view_destroy(&reference); }
    nx_context_destroy(live_context);
    nx_context_destroy(reference_context);
    physim_export_release(&image);
    physim_export_release(&rejected);
    close_font(&font);
    free(model);free(saved);free(before);free(after);free(expected);
    return ok;
}
int main(int argc,char **argv) {
    if(argc!=4)return 2;int ok=0;
    if(!strcmp(argv[1],"model"))ok=model_test();
    else if(!strcmp(argv[1],"view"))ok=view_test(argv[2],argv[3]);
    else if(!strcmp(argv[1],"failure"))ok=failure_test(argv[2]);
    else if(!strcmp(argv[1],"export"))ok=export_test(argv[2],argv[3]);
    else return 2;
    if(ok)printf("Original Physim consumer %s passed\n",argv[1]);return ok?0:1;
}
