#include "nexaui/nexaui.h"
#if !defined(_WIN32) || !defined(NEXAUI_SHARED)
#include "document_presentation.h"
#define PRIVATE_QUERIES 1
#endif
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(x) do {if(!(x)){fprintf(stderr,"document presentation %d: %s\n",__LINE__,#x);return 1;}} while(0)
typedef struct heap {size_t live,calls;int fail;} heap;
static void *allocate(void *user,size_t n){heap *h=user;++h->calls;if(h->fail)return NULL;void *p=malloc(n);if(p)h->live+=n;return p;}
static void release(void *user,void *p,size_t n){heap *h=user;if(p)h->live-=n;free(p);}
static int begin(nx_context *c,nx_utf8_span value) {
    CHECK(nx_begin_frame(c,600,400,0)==NX_OK);
    nx_widget_desc w={0};w.semantic.id=1;w.semantic.role=NX_ROLE_TEXT_FIELD;w.semantic.bounds=(nx_rect){0,0,600,400};
    w.semantic.name=(nx_utf8_span){"Owner",5};w.semantic.state=NX_SEM_READONLY;
    nx_text_document_state s={value,77,9,0,0,0,1};nx_text_document_result result;
    CHECK(nx_text_document_register(c,&w,&s,NULL,NULL,&result)==NX_OK);return 0;
}
static nx_text_document_presentation sample(nx_text_document_box *b,nx_text_document_attribute *a,nx_text_document_attribute_run *r) {
    /* A + combining accent is one owner cluster, followed by a wide character.
       Logical order intentionally differs from visual x order. */
    b[0]=(nx_text_document_box){0,3,{40.25f,10.5f,8,15}};
    b[1]=(nx_text_document_box){3,7,{20.5f,10.5f,16,15}};
    b[2]=(nx_text_document_box){7,7,{36.5f,10.5f,0,15}};
    a[0]=(nx_text_document_attribute){{"family-name",11},{"Test mono",9}};
    a[1]=(nx_text_document_attribute){{"weight",6},{"400",3}};
    a[2]=(nx_text_document_attribute){{"weight",6},{"700",3}};
    r[0]=(nx_text_document_attribute_run){3,7,&a[2],1};
    return (nx_text_document_presentation){b,3,a,2,r,1};
}
static int ordinary(void) {
    heap h={0};nx_context_desc desc={0};desc.allocator=(nx_allocator){&h,allocate,release};nx_context *c=NULL;
    CHECK(nx_context_create(&desc,&c)==NX_OK);
    const char value[]="A\xcc\x81\xf0\x9f\x8c\x8d";nx_text_document_box b[3];nx_text_document_attribute a[3];nx_text_document_attribute_run r[1];nx_text_document_presentation d=sample(b,a,r);
    CHECK(nx_text_document_set_presentation(NULL,1,77,9,&d)==NX_ERROR_ARGUMENT);
    CHECK(begin(c,(nx_utf8_span){value,7})==0);
    CHECK(nx_text_document_set_presentation(c,2,77,9,&d)==NX_ERROR_STATE);
    CHECK(nx_text_document_set_presentation(c,1,78,9,&d)==NX_ERROR_STATE);
    CHECK(nx_text_document_set_presentation(c,1,77,10,&d)==NX_ERROR_STATE);
    CHECK(nx_text_document_set_presentation(c,1,77,9,&d)==NX_OK);
    CHECK(nx_text_document_set_presentation(c,1,77,9,&d)==NX_ERROR_STATE);
    memset(b,0,sizeof(b));memset(a,0,sizeof(a));memset(r,0,sizeof(r));
    CHECK(nx_end_frame(c)==NX_OK && nx_text_document_set_presentation(c,1,77,9,&d)==NX_ERROR_STATE);
#ifdef PRIVATE_QUERIES
    const nx_document_presentation *p=nx_context_document_presentation(c,1);CHECK(p && p->text_bytes==7 && p->box_count==3);
    CHECK((uintptr_t)p%NX_DOCUMENT_PRESENTATION_ALIGNMENT==0 &&
        p->box_offset%_Alignof(nx_text_document_box)==0 && p->run_offset%_Alignof(nx_document_attribute_run)==0 &&
        p->defaults.offset%_Alignof(nx_document_attribute)==0);
    nx_rect box;size_t byte,start,end;
    CHECK(nx_document_presentation_character(p,1,&box)==NX_OK && box.x==40.25f);
    CHECK(nx_document_presentation_character(p,3,&box)==NX_OK && box.x==20.5f && box.width==16);
    CHECK(nx_document_presentation_character(p,7,&box)==NX_ERROR_ARGUMENT);
    CHECK(nx_document_presentation_range(p,0,7,&box)==NX_OK && box.x==20.5f && box.width==27.75f && box.height==15);
    CHECK(nx_document_presentation_range(p,7,7,&box)==NX_OK && box.width==0 && box.x==36.5f);
    CHECK(nx_document_presentation_range(p,0,0,&box)==NX_ERROR_UNSUPPORTED);
    CHECK(nx_document_presentation_point(p,(nx_point){24,12},&byte)==NX_OK && byte==3);
    CHECK(nx_document_presentation_point(p,(nx_point){100,12},&byte)==NX_OK && byte==SIZE_MAX);
    nx_document_attributes attrs=nx_document_presentation_attributes(p,3,&start,&end);CHECK(start==3 && end==7 && attrs.count==1);
    CHECK(!strcmp(nx_document_attribute_name(p,attrs,0),"weight") && !strcmp(nx_document_attribute_value(p,attrs,0),"700"));
    attrs=nx_document_presentation_attributes(p,0,&start,&end);CHECK(start==0 && end==3 && !attrs.count);
    CHECK(p->defaults.count==2 && !strcmp(nx_document_attribute_value(p,p->defaults,0),"Test mono"));
#endif
    CHECK(begin(c,(nx_utf8_span){value,7})==0 && nx_end_frame(c)==NX_OK);
#ifdef PRIVATE_QUERIES
    CHECK(!nx_context_document_presentation(c,1));
#endif
    nx_context_destroy(c);CHECK(h.live==0);return 0;
}
static int invalid(void) {
    heap h={0};nx_context_desc desc={0};desc.allocator=(nx_allocator){&h,allocate,release};nx_context *c=NULL;
    CHECK(nx_context_create(&desc,&c)==NX_OK && begin(c,(nx_utf8_span){"A\xcc\x81\xf0\x9f\x8c\x8d",7})==0);
    for(unsigned mode=0;mode<16;++mode) {
        nx_text_document_box b[3];nx_text_document_attribute a[3];nx_text_document_attribute_run r[1];nx_text_document_presentation d=sample(b,a,r);
        nx_result expected=NX_ERROR_ARGUMENT;
        if(mode==0)b[0].end_byte=2;
        if(mode==1)b[1].start_byte=0;
        if(mode==2)b[1].bounds.x=NAN;
        if(mode==3)b[1].bounds.width=-1;
        if(mode==4)b[1].bounds.height=0;
        if(mode==5)r[0].end_byte=8;
        if(mode==6)a[1].name=a[0].name;
        if(mode==7)a[0].name=(nx_utf8_span){"",0};
        if(mode==8)a[0].value=(nx_utf8_span){"a\0b",3};
        if(mode==9)a[0].value=(nx_utf8_span){"\xc3",1};
        if(mode==10){d.box_count=NX_TEXT_DOCUMENT_PRESENTATION_MAX_BOXES+1u;expected=NX_ERROR_LIMIT;}
        if(mode==11){d.run_count=NX_TEXT_DOCUMENT_PRESENTATION_MAX_RUNS+1u;expected=NX_ERROR_LIMIT;}
        if(mode==12){d.default_count=NX_TEXT_DOCUMENT_PRESENTATION_MAX_ATTRIBUTES+1u;expected=NX_ERROR_LIMIT;}
        if(mode==13){d.boxes=NULL;}
        if(mode==14){d.runs=NULL;}
        if(mode==15){r[0].end_byte=r[0].start_byte;}
        size_t calls=h.calls;CHECK(nx_text_document_set_presentation(c,1,77,9,&d)==expected && h.calls==calls);
    }
    nx_text_document_presentation empty={0};CHECK(nx_text_document_set_presentation(c,1,77,9,&empty)==NX_OK && nx_end_frame(c)==NX_OK);
    nx_context_destroy(c);CHECK(!h.live);return 0;
}
static int allocation_and_sparse(void) {
    heap h={0};nx_context_desc desc={0};desc.allocator=(nx_allocator){&h,allocate,release};nx_context *c=NULL;
    desc.initial_semantic_text_capacity=NX_TEXT_DOCUMENT_MAX_BYTES+16u;
    char *value=malloc(NX_TEXT_DOCUMENT_MAX_BYTES);nx_text_document_box *b=calloc(5000,sizeof(*b));CHECK(value && b);memset(value,'x',NX_TEXT_DOCUMENT_MAX_BYTES);
    CHECK(nx_context_create(&desc,&c)==NX_OK && begin(c,(nx_utf8_span){value,NX_TEXT_DOCUMENT_MAX_BYTES})==0);
    /* Force a distinct context-buffer allocation even after the 8-MiB value. */
    for(size_t i=0;i<5000;++i)b[i]=(nx_text_document_box){i,i+1,{(float)i,0,1,10}};
    nx_text_document_presentation d={b,5000,NULL,0,NULL,0};h.fail=1;
    CHECK(nx_text_document_set_presentation(c,1,77,9,&d)==NX_ERROR_MEMORY);h.fail=0;
    CHECK(nx_text_document_set_presentation(c,1,77,9,&d)==NX_OK && nx_end_frame(c)==NX_OK);
#ifdef PRIVATE_QUERIES
    const nx_document_presentation *p=nx_context_document_presentation(c,1);nx_rect box;
    CHECK(p && nx_document_presentation_character(p,4999,&box)==NX_OK && box.x==4999);
    CHECK(nx_document_presentation_character(p,5000,&box)==NX_ERROR_UNSUPPORTED);
    CHECK(nx_document_presentation_range(p,0,NX_TEXT_DOCUMENT_MAX_BYTES,&box)==NX_ERROR_UNSUPPORTED);
#endif
    nx_context_destroy(c);CHECK(!h.live);free(value);free(b);return 0;
}
int main(void){if(ordinary() || invalid() || allocation_and_sparse())return 1;puts("Document presentation: copied cluster geometry, caret, sparse ranges, attributes, identity, omission and allocation retry passed");return 0;}
