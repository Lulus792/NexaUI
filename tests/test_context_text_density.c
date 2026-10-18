#include "nexaui/nexaui.h"
#include "search_font_fixture.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"%s:%d: %s\n",__FILE__,__LINE__,#x);return 1;}}while(0)

typedef struct heap {size_t calls,fail,live;int bad;} heap;
static void *allocate(void *u,size_t n){heap*h=u;if(++h->calls==h->fail)return NULL;void*p=malloc(n);if(p)h->live+=n;return p;}
static void release(void*u,void*p,size_t n){heap*h=u;if(n>h->live)h->bad=1;else h->live-=n;free(p);}
static int command(nx_context *ui,uint32_t cmd){nx_event e={0};e.type=NX_EVENT_TEXT_COMMAND;e.text_command=cmd;return nx_context_queue_event(ui,&e)!=NX_OK;}
static int pointer(nx_context *ui,nx_event_type type,float x,float y){nx_event e={0};e.type=type;e.x=x;e.y=y;e.button=NX_BUTTON_LEFT;return nx_context_queue_event(ui,&e)!=NX_OK;}

/* An independent mask at the required physical ppem must occur in every A8
   command; dimensions and bytes cannot pass by stretching the old 1x bitmap. */
static int masks(nx_context *ui,const nx_font *font,double ppem,float density) {
    nx_glyph_cache *cache=NULL;CHECK(nx_glyph_cache_create(font,1048576,NULL,&cache)==NX_OK);
    nx_glyph_mask_view expected[8];for(unsigned i=0;i<8;++i)
        CHECK(nx_glyph_cache_get_fractional(cache,(uint16_t)i,ppem*density,&expected[i])==NX_OK);
    const nx_command *c;size_t count;CHECK(nx_frame_commands(ui,&c,&count)==NX_OK);unsigned seen=0;
    for(size_t i=0;i<count;++i)if(c[i].type==NX_COMMAND_BITMAP&&c[i].bitmap.format==NX_BITMAP_A8) {
        ++seen;const nx_bitmap_view *b=&c[i].bitmap;int found=0;
        CHECK(fabs(c[i].rect.width*density-b->width)<.001&&fabs(c[i].rect.height*density-b->height)<.001);
        for(unsigned glyph=0;glyph<8&&!found;++glyph) {
            nx_glyph_mask_view *m=&expected[glyph];if(b->width!=m->width||b->height!=m->height)continue;
            found=1;for(uint32_t y=0;y<b->height;++y)
                if(memcmp(b->pixels+(size_t)y*b->stride_bytes,m->pixels+(size_t)y*m->stride_bytes,b->width)){found=0;break;}
        }CHECK(found);
    }
    nx_glyph_cache_destroy(cache);CHECK(seen);return 0;
}
static int geometry(nx_context *ui,nx_command out[256],size_t *n,int compare) {
    const nx_command *c;size_t count,j=0;unsigned depth=0;CHECK(nx_frame_commands(ui,&c,&count)==NX_OK);
    for(size_t i=0;i<count;++i) {
        unsigned clip_depth=depth;
        if(c[i].type==NX_COMMAND_PUSH_CLIP)clip_depth=++depth;
        if(c[i].type==NX_COMMAND_POP_CLIP)--depth;
        /* Ink-piece clips include raster padding and can vary with density.
           Field clip, selection fills, caret and control geometry cannot. */
        if(c[i].type==NX_COMMAND_BITMAP||((c[i].type==NX_COMMAND_PUSH_CLIP||c[i].type==NX_COMMAND_POP_CLIP)&&clip_depth>1))continue;
        CHECK(j<256);
        if(compare){if(j<*n&&memcmp(&c[i].rect,&out[j].rect,sizeof(nx_rect)))fprintf(stderr,"Geometry %zu type %d: (%g,%g,%g,%g) vs (%g,%g,%g,%g)\n",j,c[i].type,c[i].rect.x,c[i].rect.y,c[i].rect.width,c[i].rect.height,out[j].rect.x,out[j].rect.y,out[j].rect.width,out[j].rect.height);CHECK(j<*n&&c[i].type==out[j].type&&
            !memcmp(&c[i].rect,&out[j].rect,sizeof(nx_rect))&&
            !memcmp(&c[i].color,&out[j].color,sizeof(nx_color))&&
            !memcmp(c[i].points,out[j].points,sizeof(c[i].points))&&
            c[i].thickness==out[j].thickness&&c[i].radius==out[j].radius);}
        else out[j]=c[i];++j;
    }
    if(compare)CHECK(*n==j);else *n=j;return 0;
}
static nx_text_field_desc field_desc(const nx_font_collection *fonts,int password,int multiline) {
    nx_text_field_desc d={0};d.id=1;d.name=(nx_utf8_span){"Input",5};d.bounds=(nx_rect){0,0,86,72};
    d.fonts=fonts;d.pixels_per_em=19.25;d.glyph_cache_bytes=65536;d.password=password;d.multiline=multiline;return d;
}
static nx_result field_draw(nx_context *ui,const nx_text_field_desc *d,nx_text_field *field) {
    nx_interaction_state state;int valid,submitted;return nx_text_field_widget(ui,d,field,&state,&valid,&submitted);
}
static int field_frame(nx_context *ui,const nx_text_field_desc *d,nx_text_field *field) {
    CHECK(nx_begin_frame(ui,200,120,0)==NX_OK&&field_draw(ui,d,field)==NX_OK&&nx_end_frame(ui)==NX_OK);return 0;
}
static int fields(const nx_font *font,const nx_font_collection *fonts) {
    const float scales[]={1,2,1.25f,1.5f,1};
    for(unsigned kind=0;kind<3;++kind) {
        nx_context *ui=NULL;nx_text_field *field=NULL;const char *text=kind==2?"01010101\n10":"0101";
        CHECK(nx_context_create(NULL,&ui)==NX_OK);
        CHECK((kind==1?nx_text_field_create_password(text,strlen(text),&field):nx_text_field_create(text,strlen(text),4096,&field))==NX_OK);
        nx_text_field_desc d=field_desc(fonts,kind==1,kind==2);CHECK(!field_frame(ui,&d,field)&&nx_focus_set(ui,1)==NX_OK);
        for(unsigned selection=0;selection<2;++selection) {
            CHECK(!command(ui,selection?NX_TEXT_COMMAND_SELECT_ALL:NX_TEXT_COMMAND_DOCUMENT_END));
            nx_command baseline[256];size_t baseline_count=0,a=0,b=0;
            for(unsigned i=0;i<5;++i) {
                CHECK(nx_context_set_text_raster_scale(ui,scales[i])==NX_OK&&!field_frame(ui,&d,field));
                CHECK(!masks(ui,font,d.pixels_per_em,scales[i]));
                if(geometry(ui,baseline,&baseline_count,i!=0)){fprintf(stderr,"Field kind=%u selection=%u scale=%g\n",kind,selection,scales[i]);return 1;}
                size_t anchor,cursor;CHECK(nx_text_field_selection(field,&anchor,&cursor)==NX_OK);
                if(!i){a=anchor;b=cursor;}else CHECK(a==anchor&&b==cursor);
                uint32_t index;nx_semantic_view v;CHECK(nx_semantics_find(ui,1,&index)==NX_OK&&nx_semantics_get(ui,index,&v)==NX_OK);
                CHECK(v.bounds.width==86&&v.name.length==5);
                CHECK(kind==1?(v.state&NX_SEM_PROTECTED)&&!v.value.length:v.value.length==strlen(text)&&!memcmp(v.value.data,text,strlen(text)));
                if(selection) {
                    const nx_command *c;size_t n;nx_color selected;unsigned selected_ink=0;
                    CHECK(nx_frame_commands(ui,&c,&n)==NX_OK&&nx_theme_color(ui,NX_COLOR_SELECTION_TEXT,&selected)==NX_OK);
                    for(size_t j=0;j<n;++j)selected_ink+=c[j].type==NX_COMMAND_BITMAP&&!memcmp(&c[j].color,&selected,sizeof(selected));
                    CHECK(selected_ink);
                }
            }
        }
        /* Same logical hit target and caret at every density; actual typing
           replaces that selected range exactly once. */
        size_t anchor=0,cursor=0;
        for(unsigned i=0;i<5;++i) {
            CHECK(nx_context_set_text_raster_scale(ui,scales[i])==NX_OK);
            CHECK(!pointer(ui,NX_EVENT_POINTER_DOWN,18,12)&&!pointer(ui,NX_EVENT_POINTER_UP,18,12)&&!field_frame(ui,&d,field));
            size_t a,b;CHECK(nx_text_field_selection(field,&a,&b)==NX_OK);
            if(!i){anchor=a;cursor=b;}else CHECK(anchor==a&&cursor==b);
        }
        nx_event e={0};e.type=NX_EVENT_TEXT;e.scalar='5';CHECK(nx_context_queue_event(ui,&e)==NX_OK&&!field_frame(ui,&d,field));
        nx_utf8_span changed;CHECK(nx_text_field_text(field,&changed)==NX_OK&&changed.length==strlen(text)+1);
        nx_context_destroy(ui);nx_text_field_destroy(field);
    }return 0;
}
static int defaults_and_paths(const nx_font *font,const nx_font_collection *fonts) {
    nx_context *ui=NULL;CHECK(nx_context_create(NULL,&ui)==NX_OK);float value=0;
    CHECK(nx_context_get_text_raster_scale(ui,&value)==NX_OK&&value==1);
    CHECK(nx_context_get_text_raster_scale(NULL,&value)==NX_ERROR_ARGUMENT&&value==0);
    CHECK(nx_context_get_text_raster_scale(ui,NULL)==NX_ERROR_ARGUMENT&&nx_context_set_text_raster_scale(NULL,1)==NX_ERROR_ARGUMENT);
    const float invalid[]={0,-1,.124f,8.01f,NAN,INFINITY,-INFINITY};
    for(unsigned i=0;i<sizeof(invalid)/sizeof(*invalid);++i)CHECK(nx_context_set_text_raster_scale(ui,invalid[i])==NX_ERROR_ARGUMENT);
    CHECK(nx_context_get_text_raster_scale(ui,&value)==NX_OK&&value==1);
    nx_text_layout *layout=NULL;CHECK(nx_text_layout_create(fonts,"010\n1",5,NX_BIDI_LTR,19.25,40,&layout)==NX_OK);
    double w=nx_text_layout_width(layout),h=nx_text_layout_height(layout);size_t lines=nx_text_layout_line_count(layout);
    nx_text_caret before;CHECK(nx_text_layout_caret_at(layout,1,&before)==NX_OK);
    nx_text_layout *large=NULL;CHECK(nx_text_layout_create(fonts,"0",1,NX_BIDI_LTR,2500,3000,&large)==NX_OK);
    CHECK(nx_context_set_text_raster_scale(ui,8)==NX_OK&&nx_begin_frame(ui,100,100,0)==NX_OK);
    CHECK(nx_fill_rect(ui,(nx_rect){0,0,1,1},(nx_color){1,0,0,1})==NX_OK);
    CHECK(nx_draw_text_layout(ui,large,(nx_point){0},(nx_color){1,1,1,1},65536)==NX_ERROR_ARGUMENT&&nx_end_frame(ui)==NX_OK);
    const nx_command *sentinel;size_t sentinel_count;CHECK(nx_frame_commands(ui,&sentinel,&sentinel_count)==NX_OK&&sentinel_count==1);
    nx_text_layout_destroy(large);
    const float scales[]={1,2,1.25f,1.5f,1,.125f,8};
    for(unsigned path=0;path<5;++path)for(unsigned i=0;i<7;++i) {
        float density=scales[i];CHECK(nx_context_set_text_raster_scale(ui,density)==NX_OK&&nx_begin_frame(ui,200,120,0)==NX_OK);
        CHECK(nx_context_set_text_raster_scale(ui,2)==NX_ERROR_STATE);
        CHECK(nx_context_get_text_raster_scale(ui,&value)==NX_OK&&value==density);
        nx_text_style_span span={0,3,NX_TEXT_STYLE_FOREGROUND|NX_TEXT_STYLE_BACKGROUND|NX_TEXT_STYLE_UNDERLINE|NX_TEXT_STYLE_STRIKE,{1,0,1,1},{0,.3f,.5f,1}};
        nx_point origin={8,4};nx_color tint={1,1,1,1};nx_result r;
        if(path==0)r=nx_draw_text_layout(ui,layout,origin,tint,65536);
        else if(path==1)r=nx_label(ui,1,0,(nx_rect){0,0,150,90},layout,origin,tint,65536);
        else if(path==2)r=nx_draw_rich_text_layout(ui,layout,origin,tint,&span,1,65536);
        else if(path==3)r=nx_rich_label(ui,1,0,(nx_rect){0,0,150,90},layout,origin,tint,&span,1,65536);
        else {nx_control_desc c={0};c.id=1;c.name=(nx_utf8_span){"Label",5};c.bounds=(nx_rect){0,0,150,90};
            c.label_layout=layout;c.label_origin=origin;c.glyph_cache_bytes=65536;nx_interaction_state state;r=nx_button(ui,&c,&state);}
        CHECK(r==NX_OK&&nx_end_frame(ui)==NX_OK&&!masks(ui,font,19.25,density));
        nx_text_caret after;CHECK(nx_text_layout_caret_at(layout,1,&after)==NX_OK&&after.x==before.x&&after.top==before.top&&after.byte_offset==before.byte_offset);
        CHECK(nx_text_layout_width(layout)==w&&nx_text_layout_height(layout)==h&&nx_text_layout_line_count(layout)==lines);
        const nx_command *commands;size_t count;CHECK(nx_frame_commands(ui,&commands,&count)==NX_OK&&count);
        const uint8_t *held=NULL;uint8_t ink=0;for(size_t j=0;j<count;++j)if(commands[j].type==NX_COMMAND_BITMAP){held=commands[j].bitmap.pixels;ink=held[0];break;}
        CHECK(held&&nx_context_set_text_raster_scale(ui,1)==NX_OK&&held[0]==ink); /* setter never expires completed commands */
        CHECK(nx_context_set_text_raster_scale(ui,2)==NX_OK&&nx_begin_frame(ui,200,120,0)==NX_OK);
        CHECK(nx_draw_text_layout_scaled(ui,layout,origin,tint,density,65536)==NX_OK&&nx_end_frame(ui)==NX_OK&&!masks(ui,font,19.25,density));
    }
    nx_text_layout_destroy(layout);nx_context_destroy(ui);return 0;
}
static int failures(const nx_font_collection *fonts) {
    unsigned failed=0;size_t expected_count=0;
    for(size_t failure=1;failure<150;++failure) {
        heap h={0};nx_context_desc cd={0};cd.allocator=(nx_allocator){&h,allocate,release};
        nx_context *ui=NULL;nx_text_field *field=NULL;nx_text_field_desc d=field_desc(fonts,0,0);
        CHECK(nx_context_create(&cd,&ui)==NX_OK&&nx_text_field_create("01",2,4096,&field)==NX_OK);
        CHECK(!field_frame(ui,&d,field)&&!field_frame(ui,&d,field)&&nx_focus_set(ui,1)==NX_OK&&nx_context_set_text_raster_scale(ui,2)==NX_OK);
        nx_event e={0};e.type=NX_EVENT_TEXT;e.scalar='5';CHECK(nx_context_queue_event(ui,&e)==NX_OK);
        CHECK(nx_begin_frame(ui,200,120,0)==NX_OK&&nx_fill_rect(ui,(nx_rect){0,0,1,1},(nx_color){1,0,0,1})==NX_OK);
        size_t live=h.live;h.fail=h.calls+failure;nx_result r=field_draw(ui,&d,field);
        CHECK(r==NX_OK||r==NX_ERROR_MEMORY);h.fail=0;
        if(r!=NX_OK) {
            ++failed;CHECK(h.live==live);CHECK(field_draw(ui,&d,field)==NX_OK);
        }
        CHECK(nx_end_frame(ui)==NX_OK);nx_utf8_span text;const nx_command *commands;size_t count;
        CHECK(nx_frame_commands(ui,&commands,&count)==NX_OK);
        if(!expected_count)expected_count=count;else CHECK(count==expected_count);
        CHECK(nx_text_field_text(field,&text)==NX_OK&&text.length==3&&!memcmp(text.data,"015",3));
        nx_context_destroy(ui);nx_text_field_destroy(field);CHECK(!h.live&&!h.bad);
        if(r==NX_OK)break;
    }CHECK(failed>=5);printf("Context-density editor allocation failures/retries: %u\n",failed);return 0;
}
static int rich_failures(const nx_font_collection *fonts) {
    nx_text_layout *layout=NULL;CHECK(nx_text_layout_create(fonts,"010",3,NX_BIDI_LTR,19.25,80,&layout)==NX_OK);
    nx_text_style_span span={0,3,NX_TEXT_STYLE_BACKGROUND|NX_TEXT_STYLE_UNDERLINE,{0},{.2f,.4f,.8f,1}};
    unsigned failed=0;size_t expected_count=0;
    for(size_t failure=1;failure<150;++failure) {
        heap h={0};nx_context_desc cd={0};cd.allocator=(nx_allocator){&h,allocate,release};nx_context *ui=NULL;
        CHECK(nx_context_create(&cd,&ui)==NX_OK&&nx_context_set_text_raster_scale(ui,2)==NX_OK&&nx_begin_frame(ui,100,100,0)==NX_OK);
        CHECK(nx_fill_rect(ui,(nx_rect){0,0,1,1},(nx_color){1,0,0,1})==NX_OK);size_t live=h.live;h.fail=h.calls+failure;
        nx_result r=nx_draw_rich_text_layout(ui,layout,(nx_point){4,4},(nx_color){1,1,1,1},&span,1,65536);h.fail=0;
        CHECK(r==NX_OK||r==NX_ERROR_MEMORY);
        if(r!=NX_OK){++failed;CHECK(h.live==live);CHECK(nx_draw_rich_text_layout(ui,layout,(nx_point){4,4},(nx_color){1,1,1,1},&span,1,65536)==NX_OK);}
        CHECK(nx_end_frame(ui)==NX_OK);const nx_command *commands;size_t count;CHECK(nx_frame_commands(ui,&commands,&count)==NX_OK);
        CHECK(commands[0].type==NX_COMMAND_FILL_RECT&&commands[0].rect.width==1);
        if(!expected_count)expected_count=count;else CHECK(count==expected_count);
        nx_context_destroy(ui);CHECK(!h.live&&!h.bad);if(r==NX_OK)break;
    }
    nx_text_layout_destroy(layout);CHECK(failed>=5);printf("Context-density rich text allocation failures/retries: %u\n",failed);return 0;
}
static nx_result sample(void *user,size_t i,nx_series_point *p){*p=((const nx_series_point *)user)[i];return NX_OK;}
static int other_widgets(const nx_font *font,const nx_font_collection *fonts) {
    const float scales[]={1,2,1.25f,1.5f};nx_context *ui=NULL;nx_numeric_field *number=NULL;
    CHECK(nx_context_create(NULL,&ui)==NX_OK&&nx_numeric_field_create(1,4096,&number)==NX_OK);
    nx_numeric_field_desc n={0};n.id=1;n.bounds=(nx_rect){0,0,90,40};n.name=(nx_utf8_span){"Number",6};
    n.fonts=fonts;n.pixels_per_em=19.25;n.glyph_cache_bytes=65536;n.maximum=100;
    nx_series_point points[]={{0,0},{1,1}};nx_series_envelope scratch[200];nx_plot_state state={0};nx_plot_desc plot={0};
    plot.id=2;plot.bounds=(nx_rect){100,0,190,100};plot.name=(nx_utf8_span){"Plot",4};plot.series=(nx_series_view){2,points,sample};
    plot.x_max=plot.y_max=1;plot.scratch=scratch;plot.scratch_capacity=200;plot.fonts=fonts;plot.pixels_per_em=19.25;plot.glyph_cache_bytes=65536;
    for(unsigned i=0;i<4;++i) {
        nx_interaction_state interaction;int valid;CHECK(nx_context_set_text_raster_scale(ui,scales[i])==NX_OK&&nx_begin_frame(ui,320,150,0)==NX_OK);
        CHECK(nx_numeric_field_widget(ui,&n,number,&interaction,&valid)==NX_OK&&valid);
        CHECK(nx_plot(ui,&plot,&state)==NX_OK&&nx_end_frame(ui)==NX_OK&&!masks(ui,font,19.25,scales[i]));
        double value;CHECK(nx_numeric_field_value(number,&value)==NX_OK&&value==1);
    }
    nx_numeric_field_destroy(number);nx_context_destroy(ui);
    nx_text_field *query=NULL;nx_search_state search={0};nx_search_option options[]={ {.id=101,.name={"010",3}}, {.id=102,.name={"15",2}} };
    nx_search_popup_desc d={0};d.overlay.panel.id=10;d.overlay.panel.bounds=(nx_rect){10,5,190,130};
    d.overlay.panel.name=(nx_utf8_span){"Search",6};d.overlay.backdrop_id=11;
    d.query=field_desc(fonts,0,0);d.query.id=21;d.query.parent_id=10;d.query.bounds=(nx_rect){20,12,160,30};
    d.list_id=22;d.empty_id=23;d.list_name=(nx_utf8_span){"Results",7};d.empty_name=(nx_utf8_span){"Empty",5};
    d.list_bounds=(nx_rect){20,45,160,72};d.row_height=30;d.options=options;d.option_count=2;
    CHECK(nx_context_create(NULL,&ui)==NX_OK&&nx_text_field_create("",0,4096,&query)==NX_OK);search.query=query;
    for(unsigned i=0;i<4;++i) {
        int dismissed;uint64_t activated;CHECK(nx_context_set_text_raster_scale(ui,scales[i])==NX_OK&&nx_begin_frame(ui,220,150,0)==NX_OK);
        CHECK(nx_command_palette(ui,&d,&search,&dismissed,&activated)==NX_OK&&nx_end_frame(ui)==NX_OK&&!masks(ui,font,19.25,scales[i]));
        CHECK(!dismissed&&!activated);
    }
    nx_text_field_destroy(query);nx_context_destroy(ui);return 0;
}
int main(void) {
    uint8_t bytes[512];font_fixture(bytes); /* Give the password '*' actual ink. */
    put32(bytes+324,'*');put32(bytes+328,'*');nx_font *font=NULL;nx_font_collection *fonts=NULL;
    CHECK(nx_font_open(bytes,sizeof(bytes),NULL,&font)==NX_OK&&nx_font_collection_create(NULL,&fonts)==NX_OK&&nx_font_collection_add(fonts,font)==NX_OK);
    CHECK(!defaults_and_paths(font,fonts)&&!fields(font,fonts)&&!other_widgets(font,fonts)&&!failures(fonts)&&!rich_failures(fonts));
    nx_font_collection_destroy(fonts);nx_font_close(font);puts("Context text density: masks, logical geometry, selections, passwords, hit tests and retries passed");return 0;
}
