#include "batch_plan.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr,"batch %d: %s\n",__LINE__,#x); return 1; } } while (0)

static int compare(double scale) {
    enum { W=97,H=83,STRIDE=W*4+7 };
    uint8_t rgba[] = {17,201,83,255, 231,19,71,93, 0,0,0,0, 255,255,255,213};
    nx_command stream[128] = {0}; size_t n = 0;
    stream[n++] = (nx_command){.type=NX_COMMAND_FILL_RECT,.rect={0,0,97,83},.color={.2f,.3f,.4f,1}};
    stream[n++] = (nx_command){.type=NX_COMMAND_PUSH_CLIP,.rect={3.5f,4.5f,75,67}};
    for (unsigned i = 0; i < 90; ++i) {
        nx_command c = {.type=NX_COMMAND_FILL_RECT,.rect={(float)((i*7)%101)-8.5f,(float)((i*11)%89)-9.5f,31.25f,23.5f},
            .color={(i%7)/6.f,(i%11)/10.f,(i%13)/12.f,i%9 ? 1 : .5f}};
        if (i%5 == 1) { c.type=NX_COMMAND_FILL_ROUND_RECT; c.radius=5.5f; }
        if (i%5 == 2) { c.type=NX_COMMAND_STROKE_RECT; c.thickness=3.5f; }
        if (i%5 == 3) { c.type=NX_COMMAND_LINE; c.points[0]=(nx_point){c.rect.x,c.rect.y}; c.points[1]=(nx_point){c.rect.x+25,c.rect.y+17}; c.thickness=2.25f; }
        if (i%5 == 4) { c.type=NX_COMMAND_FILL_TRIANGLE; c.points[0]=(nx_point){c.rect.x,c.rect.y}; c.points[1]=(nx_point){c.rect.x+27,c.rect.y+4}; c.points[2]=(nx_point){c.rect.x+3,c.rect.y+21}; }
        if (i%13 == 0) { c.type=NX_COMMAND_BITMAP; c.bitmap=(nx_bitmap_view){rgba,2,2,8,NX_BITMAP_RGBA8}; }
        stream[n++] = c;
    }
    stream[n++] = (nx_command){.type=NX_COMMAND_POP_CLIP};
    nx_mtl_bitmap_plan *images = calloc(1,sizeof(*images)); CHECK(images);
    CHECK(nx_mtl_bitmaps_build(images,stream,n,0,0) == NX_OK);
    nx_mtl_plan plan; CHECK(nx_mtl_plan_begin(&plan,stream,n,scale,W,H) == NX_OK);
    size_t count; CHECK(nx_mtl_records_measure(&plan,&count) == NX_OK && count > 1 && count <= n);
    size_t bytes; CHECK(nx_mtl_record_bytes(count,&bytes) == NX_OK);
    nx_mtl_record *records = malloc(bytes+31); CHECK(records); memset(records,0xa7,bytes+31);
    size_t written;
    CHECK(nx_mtl_records_write(&plan,images,records,count-1,&written) == NX_ERROR_LIMIT && written == count);
    for (size_t i=0;i<bytes+31;++i) CHECK(((uint8_t *)records)[i] == 0xa7);
    CHECK(nx_mtl_records_write(&plan,NULL,records,count,&written) == NX_ERROR_STATE && !written);
    CHECK(nx_mtl_records_write(&plan,images,records,count,&written) == NX_OK && written == count && !plan.index);
    for (size_t i=bytes;i<bytes+31;++i) CHECK(((uint8_t *)records)[i] == 0xa7);
    nx_command rebuilt[384] = {0}, scaled[128]; size_t used=0, seen=0, groups=0;
    nx_mtl_batches batches; CHECK(nx_mtl_batches_begin(&batches,records,count,W,H) == NX_OK);
    nx_mtl_batch batch;
    while (nx_mtl_batches_next(&batches,&batch)) {
        ++groups; CHECK(batch.first == seen && batch.count);
        for (uint32_t k=0;k<batch.count;++k) {
            const nx_mtl_record *r=&records[seen++];
            nx_rect clip={(float)r->scissor[0],(float)r->scissor[1],(float)r->scissor[2],(float)r->scissor[3]};
            CHECK(r->scissor[0] >= batch.scissor.x && r->scissor[1] >= batch.scissor.y);
            CHECK(r->scissor[0]+r->scissor[2] <= batch.scissor.x+batch.scissor.width);
            CHECK(r->scissor[1]+r->scissor[3] <= batch.scissor.y+batch.scissor.height);
            CHECK(!k || (r->shape.kind != 5 && r->shape.color[3] == 255));
            if (!k && batch.needs_history) CHECK(batch.history_box.x == r->scissor[0] && batch.history_box.width == r->scissor[2]);
            rebuilt[used++] = (nx_command){.type=NX_COMMAND_PUSH_CLIP,.rect=clip};
            nx_mtl_shape s=r->shape; nx_command c={0};
            c.color=(nx_color){s.color[0]/255.f,s.color[1]/255.f,s.color[2]/255.f,s.color[3]/255.f};
            if (s.kind == 0) { c.type=NX_COMMAND_FILL_RECT; c.rect=clip; }
            else if (s.kind == 1) { c.type=NX_COMMAND_FILL_TRIANGLE; c.points[0]=s.a; c.points[1]=s.b; c.points[2]=s.c; }
            else if (s.kind == 4) { c.type=NX_COMMAND_LINE; c.points[0]=s.a; c.points[1]=s.b; c.thickness=s.c.x*2; }
            else {
                c.type=s.kind == 2 ? NX_COMMAND_STROKE_RECT : s.kind == 3 ? NX_COMMAND_FILL_ROUND_RECT : NX_COMMAND_BITMAP;
                c.rect=(nx_rect){s.a.x,s.a.y,s.b.x,s.b.y}; c.thickness=c.radius=s.c.x;
                if (s.kind == 5) {
                    int found=0;
                    for (uint32_t j=0;j<images->count;++j) if (images->entries[j].x == (uint32_t)s.c.x && images->entries[j].y == (uint32_t)s.c.y) { c.bitmap=images->entries[j].view; found=1; break; }
                    CHECK(found);
                }
            }
            rebuilt[used++]=c; rebuilt[used++]=(nx_command){.type=NX_COMMAND_POP_CLIP};
        }
    }
    CHECK(seen == count && groups < count);
    for (size_t i=0;i<n;++i) {
        scaled[i]=stream[i]; nx_command *c=&scaled[i];
        if (c->type == NX_COMMAND_FILL_TRIANGLE || c->type == NX_COMMAND_LINE) for (unsigned j=0;j<3;++j) { c->points[j].x=(float)(c->points[j].x*scale); c->points[j].y=(float)(c->points[j].y*scale); }
        else if (c->type != NX_COMMAND_POP_CLIP) { c->rect.x=(float)(c->rect.x*scale); c->rect.y=(float)(c->rect.y*scale); c->rect.width=(float)(c->rect.width*scale); c->rect.height=(float)(c->rect.height*scale); }
        c->thickness=(float)(c->thickness*scale); c->radius=(float)(c->radius*scale);
    }
    uint8_t expected[STRIDE*H], actual[STRIDE*H]; memset(expected,0x7b,sizeof(expected)); memset(actual,0x7b,sizeof(actual));
    CHECK(nx_headless_render(scaled,n,expected,W,H,STRIDE,(nx_color){0}) == NX_OK);
    CHECK(nx_headless_render(rebuilt,used,actual,W,H,STRIDE,(nx_color){0}) == NX_OK);
    CHECK(!memcmp(actual,expected,sizeof(actual)));
    records[0].scissor[0]=UINT32_MAX;
    CHECK(nx_mtl_batches_begin(&batches,records,count,W,H) == NX_ERROR_ARGUMENT && !batches.count);
    free(records); free(images); return 0;
}

static int stress(void) {
    const size_t count=10000; nx_command *stream=calloc(count,sizeof(*stream)); CHECK(stream);
    nx_mtl_record *records=malloc(count*sizeof(*records)); CHECK(records);
    for (size_t i=0;i<count;++i) stream[i]=(nx_command){.type=NX_COMMAND_FILL_RECT,.rect={(float)(i%91),(float)(i%79),4,4},.color={1,1,1,1}};
    nx_mtl_plan plan; CHECK(nx_mtl_plan_begin(&plan,stream,count,1,97,83) == NX_OK);
    size_t written; CHECK(nx_mtl_records_write(&plan,NULL,records,count,&written) == NX_OK && written == count);
    nx_mtl_batches batches; nx_mtl_batch batch; CHECK(nx_mtl_batches_begin(&batches,records,count,97,83) == NX_OK);
    CHECK(nx_mtl_batches_next(&batches,&batch) && batch.first == 0 && batch.count == count && !batch.needs_history);
    CHECK(!nx_mtl_batches_next(&batches,&batch));
    size_t bytes; CHECK(nx_mtl_record_bytes(SIZE_MAX,&bytes) == NX_ERROR_LIMIT && !bytes);
    CHECK(nx_mtl_record_bytes(NX_MTL_RECORD_BUDGET/sizeof(*records)+1,&bytes) == NX_ERROR_LIMIT);
    CHECK(nx_mtl_record_bytes(NX_MTL_RECORD_BUDGET/sizeof(*records),&bytes) == NX_OK && bytes <= NX_MTL_RECORD_BUDGET);
    CHECK(nx_mtl_records_write(NULL,NULL,NULL,0,&written) == NX_ERROR_ARGUMENT && !written);
    CHECK(nx_mtl_batches_begin(&batches,NULL,0,0,0) == NX_OK && !nx_mtl_batches_next(&batches,&batch));
    free(records); free(stream); return 0;
}
int main(void) {
    const double scales[]={1,1.25,1.5,2};
    for (unsigned i=0;i<4;++i) CHECK(compare(scales[i]) == 0);
    CHECK(stress() == 0);
    puts("Metal batch preparation: packed records, exact clips/order/pixels, rollback and bounded 10000-record stress passed; GPU validation is separate.");
    return 0;
}
