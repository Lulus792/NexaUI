#include "design_a_fallback_scene.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#define CHECK(x) do{if(!(x)){fprintf(stderr,"%s:%d: %s\n",__FILE__,__LINE__,#x);return 1;}}while(0)
static double linear(double x) {return x<=.04045?x/12.92:pow((x+.055)/1.055,2.4);}
static double luminance(nx_color c) {return .2126*linear(c.r)+.7152*linear(c.g)+.0722*linear(c.b);}
static double contrast(nx_color a,nx_color b) {
    double x=luminance(a),y=luminance(b);return (fmax(x,y)+.05)/(fmin(x,y)+.05);
}
static int same(nx_color a,nx_color b) {return !memcmp(&a,&b,sizeof(a));}
static int key(nx_context *ui,nx_key k) {
    nx_event e={0};e.type=NX_EVENT_KEY_DOWN;e.key=k;CHECK(nx_context_queue_event(ui,&e)==NX_OK);
    e.type=NX_EVENT_KEY_UP;CHECK(nx_context_queue_event(ui,&e)==NX_OK);return 0;
}
static int verify(nx_context *ui,design_a_scene *s,float scale) {
    const nx_command *commands;size_t count;CHECK(nx_frame_commands(ui,&commands,&count)==NX_OK);
    int omit=s->applied_contrast||s->reduced_effects;
    CHECK(s->rgba_commands==(omit?0u:4u));
    nx_theme theme;CHECK(nx_theme_preset(s->applied_contrast?NX_THEME_HIGH_CONTRAST:NX_THEME_MIDNIGHT,&theme)==NX_OK);
    nx_color text=theme.colors[NX_COLOR_TEXT],surface=theme.colors[NX_COLOR_SURFACE];
    CHECK(text.a==1&&surface.a==1&&contrast(text,surface)>=4.5);
    CHECK(contrast(theme.colors[NX_COLOR_FOCUS],surface)>=3);
    if(s->applied_contrast) CHECK(contrast(theme.colors[NX_COLOR_ACCENT],surface)>=3);
    unsigned focus=0,lines[2]={0};
    nx_color a=s->applied_contrast?text:(nx_color){0,.75f,1,1};
    nx_color b=s->applied_contrast?theme.colors[NX_COLOR_ACCENT]:(nx_color){.95f,.4f,1,1};
    for(size_t i=0;i<count;++i) {
        const nx_command *c=&commands[i];
        if(c->type==NX_COMMAND_STROKE_RECT&&c->rect.y==116&&same(c->color,theme.colors[NX_COLOR_FOCUS])) ++focus;
        if(c->type==NX_COMMAND_LINE&&c->thickness==1.5f) {
            lines[0]+=same(c->color,a);lines[1]+=same(c->color,b);
        }
    }
    CHECK(focus==1&&lines[0]>=5&&lines[1]>=5);
    uint32_t index;nx_semantic_view v;
    CHECK(nx_semantics_find(ui,DESIGN_STATUS,&index)==NX_OK&&nx_semantics_get(ui,index,&v)==NX_OK);
    CHECK(v.role==NX_ROLE_STATUS&&v.name.length==33&&!memcmp(v.name.data,"Local WLAN connected; sample data",33));
    CHECK(nx_semantics_find(ui,DESIGN_PLOT,&index)==NX_OK&&nx_semantics_get(ui,index,&v)==NX_OK);
    CHECK(v.role==NX_ROLE_CUSTOM&&(v.actions&NX_ACTION_FOCUS)&&v.value.length>2);
    CHECK(strstr(v.description.data,"Download; Upload; x [s]; y [MB/s]"));
    /* Actual pixels contain both opaque line colors, including the HC palette.
       This fixture intentionally has no font labels and proves no readability. */
    uint32_t w=(uint32_t)(360*scale),h=(uint32_t)(240*scale);
    uint8_t *pixels=malloc((size_t)w*h*4);nx_command *scaled=malloc(count*sizeof(*scaled));
    CHECK(pixels&&scaled);
    for(size_t i=0;i<count;++i) {
        scaled[i]=commands[i];scaled[i].rect.x*=scale;scaled[i].rect.y*=scale;
        scaled[i].rect.width*=scale;scaled[i].rect.height*=scale;
        for(unsigned p=0;p<3;++p){scaled[i].points[p].x*=scale;scaled[i].points[p].y*=scale;}
        scaled[i].thickness*=scale;scaled[i].radius*=scale;
    }
    CHECK(nx_headless_render(scaled,count,pixels,w,h,(size_t)w*4,(nx_color){0,0,0,1})==NX_OK);
    unsigned found[2]={0};
    for(uint32_t y=(uint32_t)(120*scale);y<h;++y)for(uint32_t x=(uint32_t)(25*scale);x<w;++x) {
        const uint8_t *p=pixels+((size_t)y*w+x)*4;
        nx_color colors[]={a,b};
        for(unsigned k=0;k<2;++k)if(p[0]==(uint8_t)lroundf(colors[k].r*255)&&p[1]==(uint8_t)lroundf(colors[k].g*255)&&p[2]==(uint8_t)lroundf(colors[k].b*255)&&p[3]==255)++found[k];
    }
    CHECK(found[0]>5&&found[1]>5);free(pixels);free(scaled);return 0;
}
int main(void) {
    const float scales[]={1,1.25f,1.5f,2};
    for(unsigned scale=0;scale<4;++scale) {
        nx_context *ui=NULL;design_a_scene s={0};CHECK(nx_context_create(NULL,&ui)==NX_OK);
        CHECK(!design_a_prepare(&s,scales[scale])&&!design_a_frame(ui,&s,scales[scale]));
        CHECK(nx_focus_set(ui,DESIGN_PLOT)==NX_OK);
        for(unsigned repeat=0;repeat<3;++repeat)for(unsigned mode=0;mode<4;++mode) {
            s.high_contrast=(int)(mode&1);s.reduced_effects=(int)(mode>>1);
            double previous=s.plot.cursor_x;CHECK(!key(ui,NX_KEY_RIGHT));
            CHECK(!design_a_frame(ui,&s,scales[scale])&&s.plot.cursor_valid&&s.plot.cursor_x>previous);
            int invalid=nx_focus_id(ui)!=DESIGN_PLOT||verify(ui,&s,scales[scale]);
            if(invalid){nx_context_destroy(ui);design_a_free(&s);CHECK(!invalid);}
        }
        /* The same semantic actions used by native UIA toggle both switches. */
        CHECK(nx_semantics_request_action(ui,DESIGN_EFFECTS,NX_ACTION_PRESS)==NX_OK);
        CHECK(!design_a_frame(ui,&s,scales[scale])&&!s.reduced_effects&&s.rgba_commands==0);
        CHECK(nx_semantics_request_action(ui,DESIGN_CONTRAST,NX_ACTION_PRESS)==NX_OK);
        CHECK(!design_a_frame(ui,&s,scales[scale])&&!s.high_contrast&&s.applied_contrast);
        CHECK(!design_a_frame(ui,&s,scales[scale])&&!s.applied_contrast&&s.rgba_commands==4);
        nx_context_destroy(ui);design_a_free(&s);
    }
    puts("Design A fallback: 48 mode/density frames, semantic toggles and line/cursor pixels passed");return 0;
}
