#include "batch_plan.h"
#include <stddef.h>
#include <string.h>

_Static_assert(sizeof(nx_mtl_record) == 116 && offsetof(nx_mtl_record,shape) == 48 &&
    offsetof(nx_mtl_record,scissor) == 100, "C/MSL draw record layout");

nx_result nx_mtl_record_bytes(size_t count, size_t *out_bytes) {
    if (!out_bytes) return NX_ERROR_ARGUMENT;
    *out_bytes = 0;
    if (count > NX_MTL_RECORD_BUDGET / sizeof(nx_mtl_record) || count > UINT32_MAX / 6u)
        return NX_ERROR_LIMIT;
    *out_bytes = count * sizeof(nx_mtl_record); return NX_OK;
}

nx_result nx_mtl_records_begin(nx_mtl_record_plan *out, const nx_mtl_plan *draws,
    const nx_mtl_bitmap_plan *bitmaps) {
    if (!out) return NX_ERROR_ARGUMENT;
    *out = (nx_mtl_record_plan){0};
    if (!draws) return NX_ERROR_ARGUMENT;
    nx_mtl_plan copy = *draws; nx_mtl_draw draw; size_t n = 0, bytes;
    while (nx_mtl_plan_next(&copy,&draw)) {
        if (nx_mtl_record_bytes(++n,&bytes) != NX_OK) return NX_ERROR_LIMIT;
        if (draw.shape.kind == 5 && (!bitmaps || nx_mtl_bitmap_find(bitmaps,draw.bitmap) == bitmaps->count))
            return NX_ERROR_STATE;
    }
    out->draws = *draws; out->bitmaps = bitmaps; out->count = n; return NX_OK;
}

nx_result nx_mtl_records_write(const nx_mtl_record_plan *plan,
    nx_mtl_record *records, size_t capacity, size_t *out_count) {
    if (!out_count) return NX_ERROR_ARGUMENT;
    *out_count = 0;
    if (!plan || (!records && capacity)) return NX_ERROR_ARGUMENT;
    if (plan->count > capacity) { *out_count = plan->count; return NX_ERROR_LIMIT; }
    nx_mtl_plan copy = plan->draws; nx_mtl_draw draw; size_t index = 0;
    while (nx_mtl_plan_next(&copy,&draw)) {
        if (index == plan->count) return NX_ERROR_STATE; /* Borrowed source was invalidated. */
        if (draw.shape.kind == 5) {
            if (!plan->bitmaps) return NX_ERROR_STATE;
            uint32_t entry = nx_mtl_bitmap_find(plan->bitmaps,draw.bitmap);
            if (entry == plan->bitmaps->count) return NX_ERROR_STATE;
            draw.shape.c = (nx_point){(float)plan->bitmaps->entries[entry].x,(float)plan->bitmaps->entries[entry].y};
        }
        nx_mtl_record record = {0};
        memcpy(record.vertices,draw.vertices,sizeof(record.vertices)); record.shape = draw.shape;
        record.scissor[0] = draw.scissor.x; record.scissor[1] = draw.scissor.y;
        record.scissor[2] = draw.scissor.width; record.scissor[3] = draw.scissor.height;
        records[index++] = record;
    }
    if (index != plan->count) return NX_ERROR_STATE;
    *out_count = index; return NX_OK;
}

nx_result nx_mtl_batches_begin(nx_mtl_batches *batches, const nx_mtl_record *records,
    size_t count, uint32_t width, uint32_t height) {
    if (!batches) return NX_ERROR_ARGUMENT;
    *batches = (nx_mtl_batches){0};
    if ((!records && count) || width > 8192 || height > 8192) return NX_ERROR_ARGUMENT;
    size_t bytes; nx_result result = nx_mtl_record_bytes(count,&bytes);
    if (result != NX_OK) return result;
    for (size_t i = 0; i < count; ++i) {
        const nx_mtl_record *r = &records[i];
        if (!r->scissor[2] || !r->scissor[3] || r->scissor[0] >= width || r->scissor[1] >= height ||
            r->scissor[2] > width-r->scissor[0] || r->scissor[3] > height-r->scissor[1] || r->shape.kind > 5)
            return NX_ERROR_ARGUMENT;
        for (unsigned c = 0; c < 4; ++c) if (r->shape.color[c] > 255) return NX_ERROR_ARGUMENT;
    }
    batches->records = records; batches->count = (uint32_t)count; return NX_OK;
}

static int reads_destination(const nx_mtl_record *r) { return r->shape.kind == 5 || r->shape.color[3] < 255; }
static nx_mtl_box record_box(const nx_mtl_record *r) {
    return (nx_mtl_box){r->scissor[0],r->scissor[1],r->scissor[2],r->scissor[3]};
}
int nx_mtl_batches_next(nx_mtl_batches *batches, nx_mtl_batch *out) {
    if (!batches || !out || batches->index == batches->count) return 0;
    uint32_t first = batches->index++;
    nx_mtl_batch batch = {0}; batch.first = first;
    batch.scissor = record_box(&batches->records[first]);
    batch.needs_history = reads_destination(&batches->records[first]);
    if (batch.needs_history) batch.history_box = batch.scissor;
    while (batches->index < batches->count && !reads_destination(&batches->records[batches->index])) {
        nx_mtl_box next = record_box(&batches->records[batches->index++]);
        uint32_t right = batch.scissor.x+batch.scissor.width, bottom = batch.scissor.y+batch.scissor.height;
        if (next.x+next.width > right) right = next.x+next.width;
        if (next.y+next.height > bottom) bottom = next.y+next.height;
        if (next.x < batch.scissor.x) batch.scissor.x = next.x;
        if (next.y < batch.scissor.y) batch.scissor.y = next.y;
        batch.scissor.width = right-batch.scissor.x; batch.scissor.height = bottom-batch.scissor.y;
    }
    batch.count = batches->index-first; *out = batch; return 1;
}
