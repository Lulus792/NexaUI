#include "nexaui/nexaui.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"%s:%d: %s failed\n",__FILE__,__LINE__,#x); return 1; } } while (0)
#include "fixture_font.h"

typedef struct fixture {
    uint8_t bytes[512]; nx_font *font; nx_font_collection *fonts;
    nx_context *ctx; nx_text_field *field; nx_text_field_desc desc;
} fixture;
static int frame(fixture *f,double dt) {
    nx_interaction_state state; int valid,submitted;
    return nx_begin_frame(f->ctx,200,120,dt)==NX_OK &&
        nx_text_field_widget(f->ctx,&f->desc,f->field,&state,&valid,&submitted)==NX_OK &&
        nx_end_frame(f->ctx)==NX_OK;
}
static int setup(fixture *f,const char *text,int password,int multiline) {
    memset(f,0,sizeof(*f)); font_fixture(f->bytes);
    if (nx_font_open(f->bytes,sizeof(f->bytes),NULL,&f->font)!=NX_OK ||
        nx_font_collection_create(NULL,&f->fonts)!=NX_OK ||
        nx_font_collection_add(f->fonts,f->font)!=NX_OK || nx_context_create(NULL,&f->ctx)!=NX_OK) return 0;
    nx_result r=password?nx_text_field_create_password(text,strlen(text),&f->field):
        nx_text_field_create(text,strlen(text),4096,&f->field);
    if (r!=NX_OK) return 0;
    f->desc.id=1; f->desc.name=(nx_utf8_span){"Selection",9};
    f->desc.bounds=(nx_rect){0,0,80,48}; f->desc.fonts=f->fonts;
    f->desc.pixels_per_em=20; f->desc.glyph_cache_bytes=65536;
    f->desc.password=password; f->desc.multiline=multiline;
    return frame(f,0);
}
static void cleanup(fixture *f) {
    nx_text_field_destroy(f->field); nx_context_destroy(f->ctx);
    nx_font_collection_destroy(f->fonts); nx_font_close(f->font);
}
static int pointer(fixture *f,nx_event_type type,float x,float y,uint32_t modifiers) {
    nx_event e={0}; e.type=type; e.x=x; e.y=y; e.modifiers=modifiers;
    if (type==NX_EVENT_POINTER_DOWN || type==NX_EVENT_POINTER_UP) e.button=NX_BUTTON_LEFT;
    return nx_context_queue_event(f->ctx,&e)==NX_OK;
}
static int command(fixture *f,uint32_t value) {
    nx_event e={0}; e.type=NX_EVENT_TEXT_COMMAND; e.text_command=value;
    return nx_context_queue_event(f->ctx,&e)==NX_OK;
}
static int selection(fixture *f,size_t a,size_t c) {
    size_t actual_a=0,actual_c=0;
    return nx_text_field_selection(f->field,&actual_a,&actual_c)==NX_OK && actual_a==a && actual_c==c;
}
static int cursor(fixture *f,size_t *out) {
    size_t a; return nx_text_field_selection(f->field,&a,out)==NX_OK;
}
static int test_order_and_shift(void) {
    fixture f; CHECK(setup(&f,"11111",0,0));
    CHECK(pointer(&f,NX_EVENT_POINTER_DOWN,16,20,0));
    CHECK(pointer(&f,NX_EVENT_POINTER_MOVE,52,20,0));
    CHECK(pointer(&f,NX_EVENT_POINTER_UP,40,20,0));
    CHECK(frame(&f,0) && selection(&f,1,3));
    CHECK(pointer(&f,NX_EVENT_POINTER_DOWN,64,20,NX_MOD_SHIFT));
    CHECK(pointer(&f,NX_EVENT_POINTER_UP,64,20,NX_MOD_SHIFT));
    CHECK(frame(&f,0) && selection(&f,1,5));
    CHECK(pointer(&f,NX_EVENT_POINTER_DOWN,52,20,0) && frame(&f,0));
    CHECK(selection(&f,4,4));
    CHECK(pointer(&f,NX_EVENT_POINTER_MOVE,16,20,0) && frame(&f,0));
    CHECK(selection(&f,4,1));
    CHECK(pointer(&f,NX_EVENT_POINTER_UP,-100,20,0) && frame(&f,0));
    CHECK(selection(&f,4,0));
    CHECK(pointer(&f,NX_EVENT_POINTER_MOVE,76,20,0) && frame(&f,.1));
    CHECK(selection(&f,4,0));
    /* Ordinary typing replaces the dragged range as one model replacement. */
    nx_event e={0}; e.type=NX_EVENT_TEXT; e.scalar='5';
    CHECK(nx_context_queue_event(f.ctx,&e)==NX_OK && frame(&f,0));
    nx_utf8_span text; CHECK(nx_text_field_text(f.field,&text)==NX_OK && text.length==2 && !memcmp(text.data,"51",2));
    cleanup(&f); return 0;
}
static int test_capture_and_cancel(void) {
    fixture f; CHECK(setup(&f,"11111",0,0));
    CHECK(pointer(&f,NX_EVENT_POINTER_DOWN,16,20,0) && frame(&f,0));
    CHECK(pointer(&f,NX_EVENT_POINTER_MOVE,52,20,0) && frame(&f,0) && selection(&f,1,4));
    CHECK(pointer(&f,NX_EVENT_POINTER_CANCEL,0,0,0) && frame(&f,0));
    CHECK(pointer(&f,NX_EVENT_POINTER_MOVE,4,20,0) && frame(&f,.1) && selection(&f,1,4));
    CHECK(pointer(&f,NX_EVENT_POINTER_DOWN,16,20,0) && frame(&f,0));
    nx_event e={0}; e.type=NX_EVENT_FOCUS; e.focused=0;
    CHECK(nx_context_queue_event(f.ctx,&e)==NX_OK);
    CHECK(pointer(&f,NX_EVENT_POINTER_MOVE,64,20,0) && frame(&f,.1) && selection(&f,1,1));
    f.desc.disabled=1;
    CHECK(pointer(&f,NX_EVENT_POINTER_DOWN,52,20,0) && frame(&f,0) && selection(&f,1,1));
    cleanup(&f); return 0;
}
static int test_autoscroll(void) {
    fixture f; const char *long_text="1111111111111111111111111111111111111111";
    CHECK(setup(&f,long_text,0,0));
    CHECK(pointer(&f,NX_EVENT_POINTER_DOWN,4,20,0));
    CHECK(pointer(&f,NX_EVENT_POINTER_MOVE,120,20,0) && frame(&f,0));
    size_t before=0,after=0; CHECK(cursor(&f,&before) && before>0 && before<strlen(long_text));
    CHECK(frame(&f,0) && selection(&f,0,before));
    CHECK(frame(&f,.1) && cursor(&f,&after) && after>before);
    for (unsigned i=0;i<20;++i) CHECK(frame(&f,.1));
    CHECK(selection(&f,0,strlen(long_text)));
    CHECK(pointer(&f,NX_EVENT_POINTER_MOVE,-80,20,0) && frame(&f,.1));
    CHECK(cursor(&f,&after) && after<strlen(long_text));
    CHECK(pointer(&f,NX_EVENT_POINTER_UP,-80,20,0) && frame(&f,0));
    CHECK(cursor(&f,&after) && frame(&f,10) && selection(&f,0,after));
    cleanup(&f);
    CHECK(setup(&f,"1\n1\n1\n1\n1\n1\n1\n1\n1\n1\n1\n1",0,1));
    CHECK(pointer(&f,NX_EVENT_POINTER_DOWN,4,14,0));
    CHECK(pointer(&f,NX_EVENT_POINTER_MOVE,4,100,0) && frame(&f,0));
    CHECK(cursor(&f,&before) && frame(&f,.1) && cursor(&f,&after) && after>before);
    CHECK(pointer(&f,NX_EVENT_POINTER_MOVE,4,-100,0) && frame(&f,.1));
    CHECK(cursor(&f,&before) && before<after);
    cleanup(&f); return 0;
}
static int test_mask_and_touch(void) {
    fixture f; CHECK(setup(&f,"A\xf0\x9f\x8c\x8d" "e\xcc\x81",1,0));
    CHECK(pointer(&f,NX_EVENT_POINTER_DOWN,4,20,0));
    CHECK(pointer(&f,NX_EVENT_POINTER_MOVE,28,20,0));
    CHECK(pointer(&f,NX_EVENT_POINTER_UP,28,20,0) && frame(&f,0) && selection(&f,0,5));
    uint32_t index; nx_semantic_view view;
    CHECK(nx_semantics_find(f.ctx,1,&index)==NX_OK && nx_semantics_get(f.ctx,index,&view)==NX_OK);
    CHECK((view.state&NX_SEM_PROTECTED) && view.value.length==0);
    CHECK(command(&f,NX_TEXT_COMMAND_DELETE_BACKWARD) && frame(&f,0));
    nx_utf8_span text; CHECK(nx_text_field_text(f.field,&text)==NX_OK && text.length==3 && !memcmp(text.data,"e\xcc\x81",3));
    cleanup(&f);
    CHECK(setup(&f,"11111",0,0));
    nx_event e={0}; e.type=NX_EVENT_TOUCH_DOWN; e.contact_id=17; e.x=16; e.y=20;
    CHECK(nx_context_queue_event(f.ctx,&e)==NX_OK); e.type=NX_EVENT_TOUCH_UP;
    CHECK(nx_context_queue_event(f.ctx,&e)==NX_OK && frame(&f,0) && selection(&f,1,1));
    cleanup(&f); return 0;
}
static int test_two_fields(void) {
    fixture f; CHECK(setup(&f,"111",0,0));
    nx_text_field *second=NULL; CHECK(nx_text_field_create("111",3,4096,&second)==NX_OK);
    nx_text_field_desc other=f.desc; other.id=2; other.bounds.x=100;
    nx_interaction_state state; int valid,submitted;
    CHECK(nx_begin_frame(f.ctx,200,120,0)==NX_OK);
    CHECK(nx_text_field_widget(f.ctx,&f.desc,f.field,&state,&valid,&submitted)==NX_OK);
    CHECK(nx_text_field_widget(f.ctx,&other,second,&state,&valid,&submitted)==NX_OK && nx_end_frame(f.ctx)==NX_OK);
    CHECK(pointer(&f,NX_EVENT_POINTER_DOWN,16,20,0));
    nx_event e={0}; e.type=NX_EVENT_TEXT; e.scalar='5';
    CHECK(nx_context_queue_event(f.ctx,&e)==NX_OK);
    CHECK(pointer(&f,NX_EVENT_POINTER_UP,16,20,0));
    CHECK(pointer(&f,NX_EVENT_POINTER_DOWN,116,20,0));
    e.scalar='0'; CHECK(nx_context_queue_event(f.ctx,&e)==NX_OK);
    CHECK(pointer(&f,NX_EVENT_POINTER_UP,116,20,0));
    CHECK(nx_begin_frame(f.ctx,200,120,0)==NX_OK);
    CHECK(nx_text_field_widget(f.ctx,&f.desc,f.field,&state,&valid,&submitted)==NX_OK && !state.focused);
    CHECK(nx_text_field_widget(f.ctx,&other,second,&state,&valid,&submitted)==NX_OK && state.focused);
    CHECK(nx_end_frame(f.ctx)==NX_OK);
    nx_utf8_span text;
    CHECK(nx_text_field_text(f.field,&text)==NX_OK && text.length==4 && !memcmp(text.data,"1511",4));
    CHECK(nx_text_field_text(second,&text)==NX_OK && text.length==4 && !memcmp(text.data,"1011",4));
    nx_text_field_destroy(second); cleanup(&f); return 0;
}
static int test_numeric_pointer(void) {
    fixture f; CHECK(setup(&f,"",0,0));
    nx_numeric_field *number=NULL; CHECK(nx_numeric_field_create(11111,4096,&number)==NX_OK);
    nx_numeric_field_desc desc={0}; desc.id=2; desc.bounds=(nx_rect){0,0,80,48};
    desc.name=(nx_utf8_span){"Number",6}; desc.fonts=f.fonts; desc.pixels_per_em=20;
    desc.glyph_cache_bytes=65536; desc.minimum=0; desc.maximum=100000;
    nx_interaction_state state; int valid;
    CHECK(nx_begin_frame(f.ctx,200,120,0)==NX_OK);
    CHECK(nx_numeric_field_widget(f.ctx,&desc,number,&state,&valid)==NX_OK && nx_end_frame(f.ctx)==NX_OK);
    CHECK(pointer(&f,NX_EVENT_POINTER_DOWN,16,20,0));
    CHECK(pointer(&f,NX_EVENT_POINTER_UP,40,20,0));
    nx_event e={0}; e.type=NX_EVENT_TEXT; e.scalar='5';
    CHECK(nx_context_queue_event(f.ctx,&e)==NX_OK);
    CHECK(nx_begin_frame(f.ctx,200,120,0)==NX_OK);
    CHECK(nx_numeric_field_widget(f.ctx,&desc,number,&state,&valid)==NX_OK && nx_end_frame(f.ctx)==NX_OK);
    nx_utf8_span text; double committed;
    CHECK(nx_numeric_field_text(number,&text)==NX_OK && text.length==4 && !memcmp(text.data,"1511",4));
    CHECK(nx_numeric_field_value(number,&committed)==NX_OK && committed==11111);
    nx_numeric_field_destroy(number); cleanup(&f); return 0;
}
static int test_bidi_and_clip(void) {
    fixture f; CHECK(setup(&f,"\xd7\x90\xd7\x91",0,0));
    CHECK(pointer(&f,NX_EVENT_POINTER_DOWN,4,20,0));
    CHECK(pointer(&f,NX_EVENT_POINTER_UP,28,20,0) && frame(&f,0) && selection(&f,4,0));
    cleanup(&f);
    CHECK(setup(&f,"11111",0,0));
    nx_interaction_state state; int valid,submitted;
    CHECK(nx_begin_frame(f.ctx,200,120,0)==NX_OK && nx_push_clip(f.ctx,(nx_rect){0,0,30,48})==NX_OK);
    CHECK(nx_text_field_widget(f.ctx,&f.desc,f.field,&state,&valid,&submitted)==NX_OK);
    CHECK(nx_pop_clip(f.ctx)==NX_OK && nx_end_frame(f.ctx)==NX_OK);
    /* A press in clipped-away content must not move the caret. */
    CHECK(pointer(&f,NX_EVENT_POINTER_DOWN,52,20,0));
    CHECK(pointer(&f,NX_EVENT_POINTER_UP,52,20,0) && frame(&f,0) && selection(&f,5,5));
    cleanup(&f); return 0;
}
typedef union allocation_header { long double alignment; size_t bytes; void *pointer; } allocation_header;
typedef struct allocation_state { size_t calls,live,fail_on; int bad_size; } allocation_state;
static void *sized_allocate(void *user,size_t bytes) {
    allocation_state *s=user; ++s->calls;
    if (s->calls==s->fail_on || bytes>SIZE_MAX-sizeof(allocation_header)) return NULL;
    allocation_header *h=malloc(sizeof(*h)+bytes); if (!h) return NULL;
    h->bytes=bytes; ++s->live; return h+1;
}
static void sized_free(void *user,void *pointer,size_t bytes) {
    allocation_state *s=user; allocation_header *h=(allocation_header *)pointer-1;
    if (h->bytes!=bytes || !s->live) s->bad_size=1;
    --s->live; free(h);
}
static int test_event_growth(void) {
    allocation_state state={0}; nx_context_desc desc={0}; nx_context *ctx=NULL;
    desc.initial_event_capacity=1; desc.allocator=(nx_allocator){&state,sized_allocate,sized_free};
    CHECK(nx_context_create(&desc,&ctx)==NX_OK);
    nx_event e={0}; e.type=NX_EVENT_POINTER_DOWN; e.button=NX_BUTTON_LEFT; e.x=12; e.y=8;
    CHECK(nx_context_queue_event(ctx,&e)==NX_OK);
    state.fail_on=state.calls+1; e.type=NX_EVENT_POINTER_UP;
    CHECK(nx_context_queue_event(ctx,&e)==NX_ERROR_MEMORY);
    state.fail_on=0; CHECK(nx_context_queue_event(ctx,&e)==NX_OK);
    size_t warm_calls=0;
    for (unsigned frame_index=0;frame_index<20;++frame_index) {
        if (frame_index) {
            e.type=NX_EVENT_POINTER_DOWN; CHECK(nx_context_queue_event(ctx,&e)==NX_OK);
            e.type=NX_EVENT_POINTER_UP; CHECK(nx_context_queue_event(ctx,&e)==NX_OK);
        }
        CHECK(nx_begin_frame(ctx,200,120,0)==NX_OK);
        const nx_event *events=NULL; size_t count=0;
        CHECK(nx_frame_events(ctx,&events,&count)==NX_OK && count==2);
        CHECK(events[0].type==NX_EVENT_POINTER_DOWN && events[1].type==NX_EVENT_POINTER_UP && events[0].x==12);
        CHECK(nx_end_frame(ctx)==NX_OK && !state.bad_size);
        if (frame_index==1) warm_calls=state.calls;
        if (frame_index>1) CHECK(state.calls==warm_calls);
    }
    nx_context_destroy(ctx); CHECK(state.live==0 && !state.bad_size); return 0;
}
static int test_event_frequency(void) {
    fixture a,b; const char *text="1111111111111111111111111111111111111111";
    CHECK(setup(&a,text,0,0) && setup(&b,text,0,0));
    CHECK(pointer(&a,NX_EVENT_POINTER_DOWN,4,20,0) && pointer(&b,NX_EVENT_POINTER_DOWN,4,20,0));
    CHECK(pointer(&a,NX_EVENT_POINTER_MOVE,120,20,0));
    for (unsigned i=0;i<100;++i) CHECK(pointer(&b,NX_EVENT_POINTER_MOVE,120,20,0));
    CHECK(frame(&a,.1) && frame(&b,.1));
    size_t first,second; CHECK(cursor(&a,&first) && cursor(&b,&second) && first==second);
    CHECK(frame(&a,10) && frame(&b,.1)); /* A paused frame cannot cause an unbounded jump. */
    CHECK(cursor(&a,&first) && cursor(&b,&second) && first==second);
    cleanup(&a); cleanup(&b); return 0;
}
static int test_selected_text_contrast(void) {
    fixture f; CHECK(setup(&f,"111",0,0));
    nx_theme theme; CHECK(nx_theme_preset(NX_THEME_HIGH_CONTRAST,&theme)==NX_OK);
    CHECK(nx_context_set_theme(f.ctx,&theme)==NX_OK);
    CHECK(pointer(&f,NX_EVENT_POINTER_DOWN,4,20,0));
    CHECK(pointer(&f,NX_EVENT_POINTER_UP,16,20,0) && frame(&f,0) && selection(&f,0,1));
    CHECK(nx_focus_set(f.ctx,0)==NX_OK && frame(&f,0)); /* Keep the caret out of the pixel oracle. */
    const nx_command *commands=NULL; size_t count=0; uint8_t rgba[200*120*4];
    CHECK(nx_frame_commands(f.ctx,&commands,&count)==NX_OK);
    CHECK(nx_headless_render(commands,count,rgba,200,120,200*4,(nx_color){0,0,0,1})==NX_OK);
    unsigned selected_ink=0,unselected_ink=0;
    for (unsigned y=14;y<34;++y) for (unsigned x=4;x<40;++x) {
        const uint8_t *p=rgba+(y*200+x)*4;
        if (x<16 && p[0]<200 && p[1]<200 && p[2]<59) ++selected_ink;
        if (x>=16 && p[0]>60 && p[1]>60 && p[2]>60) ++unselected_ink;
    }
    CHECK(selected_ink>0 && unselected_ink>0);
    cleanup(&f); return 0;
}
static int save_selection_frame(const char *directory) {
    fixture f; CHECK(setup(&f,"11111\n11111\n11111",0,1));
    const nx_command *commands=NULL; size_t count=0;
    uint8_t rgba[200*120*4], before[200*120*4];
    CHECK(nx_frame_commands(f.ctx,&commands,&count)==NX_OK);
    CHECK(nx_headless_render(commands,count,before,200,120,200*4,(nx_color){0,0,0,1})==NX_OK);
    CHECK(pointer(&f,NX_EVENT_POINTER_DOWN,16,14,0));
    CHECK(pointer(&f,NX_EVENT_POINTER_UP,40,34,0) && frame(&f,0) && selection(&f,1,9));
    CHECK(nx_frame_commands(f.ctx,&commands,&count)==NX_OK);
    CHECK(nx_headless_render(commands,count,rgba,200,120,200*4,(nx_color){0,0,0,1})==NX_OK);
    /* Selection must be visible inside, with no paint leaking past the field. */
    CHECK(memcmp(rgba+(10*200+20)*4,before+(10*200+20)*4,4)!=0);
    CHECK(rgba[(20*200+90)*4]==0 && rgba[(20*200+90)*4+2]==0);
    if (directory) {
        char path[1024]; int written=snprintf(path,sizeof(path),"%s/text-pointer-selection.ppm",directory);
        CHECK(written>0 && (size_t)written<sizeof(path));
        FILE *out=fopen(path,"wb"); CHECK(out!=NULL);
        CHECK(fprintf(out,"P6\n200 120\n255\n")>0);
        for (size_t i=0;i<200*120;++i) CHECK(fwrite(rgba+4*i,1,3,out)==3);
        CHECK(fclose(out)==0);
    }
    cleanup(&f); return 0;
}
int main(int argc,char **argv) {
    if (test_order_and_shift() || test_capture_and_cancel() || test_autoscroll() || test_mask_and_touch() ||
        test_two_fields() || test_numeric_pointer() || test_bidi_and_clip() || test_event_growth() ||
        test_event_frequency() || test_selected_text_contrast() || save_selection_frame(argc>1?argv[1]:NULL)) return 1;
    puts("text pointer selection tests passed"); return 0;
}
