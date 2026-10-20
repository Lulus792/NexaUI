#include "../renderers/vulkan/canvas_cache.h"
#include <stdio.h>
#include <stdlib.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr,"canvas-cache:%d: %s\n",__LINE__,#x); return 1; } } while (0)
typedef struct heap {void *pointer;size_t bytes,peak;int fail;} heap;
static void *allocate(void *user,size_t bytes){heap *h=user;if(h->fail){h->fail=0;return NULL;}if(h->pointer)abort();h->pointer=malloc(bytes);if(h->pointer){h->bytes=bytes;if(bytes>h->peak)h->peak=bytes;}return h->pointer;}
static void release(void *user,void *p,size_t n){heap *h=user;if(p!=h->pointer||n!=h->bytes)abort();free(p);h->pointer=NULL;h->bytes=0;}
int main(void){
 heap h={0};nx_allocator a={&h,allocate,release};nx_vk_canvas_cache cache={0};
 uint8_t rgba[]={1,2,3,255,4,5,6,128,77,78,79,80},other[12];
 nx_command commands[4]={0};
 commands[0].type=NX_COMMAND_PUSH_CLIP;commands[0].rect=(nx_rect){.25f,.5f,20,21};
 commands[1].type=NX_COMMAND_BITMAP;commands[1].rect=(nx_rect){1,2,10,11};commands[1].color=(nx_color){1,1,1,.7f};
 commands[1].bitmap=(nx_bitmap_view){rgba,2,1,12,NX_BITMAP_RGBA8};
 commands[2].type=NX_COMMAND_LINE;commands[2].points[0]=(nx_point){1,1};commands[2].points[1]=(nx_point){5,6};commands[2].thickness=1.5f;commands[2].color=(nx_color){0,1,1,1};
 commands[3].type=NX_COMMAND_POP_CLIP;nx_color clear={.1f,.2f,.3f,.4f};
 CHECK(!nx_vk_canvas_cache_match(&cache,commands,4,1,clear,1));
 cache.valid=nx_vk_canvas_cache_capture(&cache,a,commands,4,1,clear,1);CHECK(cache.valid);
 CHECK(nx_vk_canvas_cache_match(&cache,commands,4,1,clear,1));
 rgba[0]^=1;CHECK(!nx_vk_canvas_cache_match(&cache,commands,4,1,clear,1));rgba[0]^=1;
 rgba[8]^=1;CHECK(nx_vk_canvas_cache_match(&cache,commands,4,1,clear,1));
 memcpy(other,rgba,sizeof(other));commands[1].bitmap.pixels=other;
 CHECK(nx_vk_canvas_cache_match(&cache,commands,4,1,clear,1));
 memset(rgba,0,sizeof(rgba)); /* Old source is no longer needed. */
 CHECK(nx_vk_canvas_cache_match(&cache,commands,4,1,clear,1));
 commands[0].color=(nx_color){.7f,.8f,.9f,1};commands[0].thickness=123;
 commands[1].points[2]=(nx_point){99,100};commands[3].rect=(nx_rect){11,12,13,14};
 CHECK(nx_vk_canvas_cache_match(&cache,commands,4,1,clear,1));
 CHECK(!nx_vk_canvas_cache_match(&cache,commands,4,1.25,clear,1));
 CHECK(!nx_vk_canvas_cache_match(&cache,commands,4,1,clear,0));
 nx_color changed=clear;changed.a=.8f;CHECK(!nx_vk_canvas_cache_match(&cache,commands,4,1,changed,1));
 commands[0].rect.x+=1;CHECK(!nx_vk_canvas_cache_match(&cache,commands,4,1,clear,1));commands[0].rect.x-=1;
 commands[1].color.a=.5f;CHECK(!nx_vk_canvas_cache_match(&cache,commands,4,1,clear,1));commands[1].color.a=.7f;
 commands[2].thickness+=1;CHECK(!nx_vk_canvas_cache_match(&cache,commands,4,1,clear,1));commands[2].thickness-=1;
 commands[2].points[1].x+=1;CHECK(!nx_vk_canvas_cache_match(&cache,commands,4,1,clear,1));commands[2].points[1].x-=1;
 nx_vk_canvas_cache_drop(&cache,a);CHECK(!h.bytes&&!cache.valid);
 h.fail=1;CHECK(!nx_vk_canvas_cache_capture(&cache,a,commands,4,1,clear,1));CHECK(!h.bytes&&!cache.data&&!cache.valid);
 uint8_t mask[]={0,127,255,77,78,80};commands[1].bitmap=(nx_bitmap_view){mask,3,1,6,NX_BITMAP_A8};
 cache.valid=nx_vk_canvas_cache_capture(&cache,a,commands,4,1,clear,1);CHECK(cache.valid);
 mask[1]=126;CHECK(!nx_vk_canvas_cache_match(&cache,commands,4,1,clear,1));
 size_t big_bytes=1024u*1024u*4u;uint8_t *big=calloc(1,big_bytes);CHECK(big);
 nx_command many[8];for(unsigned i=0;i<8;++i){many[i]=commands[1];many[i].bitmap=(nx_bitmap_view){big,1024,1024,4096,NX_BITMAP_RGBA8};}
 cache.valid=nx_vk_canvas_cache_capture(&cache,a,many,7,1,clear,1);CHECK(cache.valid&&cache.capacity<=NX_VK_CANVAS_CACHE_BYTES);
 CHECK(nx_vk_canvas_cache_match(&cache,many,7,1,clear,1));
 CHECK(!nx_vk_canvas_cache_capture(&cache,a,many,8,1,clear,1));CHECK(!h.bytes&&!cache.data&&!cache.valid);
 free(big);CHECK(h.peak<=NX_VK_CANVAS_CACHE_BYTES);
 const nx_command_type shapes[]={NX_COMMAND_FILL_RECT,NX_COMMAND_STROKE_RECT,NX_COMMAND_FILL_ROUND_RECT,NX_COMMAND_FILL_TRIANGLE};
 for(unsigned i=0;i<4;++i){
  nx_command shape={0};shape.type=shapes[i];shape.rect=(nx_rect){1,2,3,4};shape.color=(nx_color){.1f,.2f,.3f,.4f};shape.thickness=1;shape.radius=2;
  shape.points[0]=(nx_point){1,1};shape.points[1]=(nx_point){8,2};shape.points[2]=(nx_point){3,9};
  cache.valid=nx_vk_canvas_cache_capture(&cache,a,&shape,1,1,clear,1);CHECK(cache.valid&&nx_vk_canvas_cache_match(&cache,&shape,1,1,clear,1));
  float *field=i==1?&shape.thickness:i==2?&shape.radius:i==3?&shape.points[2].x:&shape.rect.width;
  *field+=.25f;CHECK(!nx_vk_canvas_cache_match(&cache,&shape,1,1,clear,1));*field-=.25f;
  shape.color.g+=.25f;CHECK(!nx_vk_canvas_cache_match(&cache,&shape,1,1,clear,1));
 }
 nx_vk_canvas_cache_drop(&cache,a);CHECK(!h.bytes);
 puts("canvas-cache: exact metadata/pixel ownership, padding/address independence, invalidation, OOM and32MiB bound passed");return 0;
}
