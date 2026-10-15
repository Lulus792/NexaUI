#ifndef NEXAUI_METAL_BATCH_PLAN_H
#define NEXAUI_METAL_BATCH_PLAN_H

#include "draw_plan.h"
#include "bitmap_plan.h"

#define NX_MTL_RECORD_BUDGET (64u * 1024u * 1024u)
/* Plain C/MSL device-buffer ABI. No source pointers survive preparation. */
typedef struct nx_mtl_record {
    nx_point vertices[6];
    nx_mtl_shape shape;
    uint32_t scissor[4];
} nx_mtl_record;

typedef struct nx_mtl_batch {
    uint32_t first, count;
    nx_mtl_box scissor, history_box;
    int needs_history;
} nx_mtl_batch;

typedef struct nx_mtl_batches {
    const nx_mtl_record *records;
    uint32_t count, index;
} nx_mtl_batches;
typedef struct nx_mtl_record_plan {
    nx_mtl_plan draws;
    const nx_mtl_bitmap_plan *bitmaps;
    size_t count;
} nx_mtl_record_plan;

nx_result nx_mtl_record_bytes(size_t count, size_t *out_bytes);
/* One preflight counts visible records and checks atlas mappings. The input
   draw plan must come from successful plan_begin; its commands and the bitmap
   plan stay borrowed and unchanged through records_write. No pixels are read. */
nx_result nx_mtl_records_begin(nx_mtl_record_plan *out, const nx_mtl_plan *draws,
    const nx_mtl_bitmap_plan *bitmaps);
/* Capacity is checked before writing, then the preflighted source is traversed
   once. LIMIT returns the required count and leaves storage untouched. Other
   valid-plan failures leave count zero. Record stride is exactly 116 bytes. */
nx_result nx_mtl_records_write(const nx_mtl_record_plan *plan,
    nx_mtl_record *records, size_t capacity, size_t *out_count);
/* records come from records_write; begin additionally checks bounded clips and
   colors before grouping, rather than accepting arbitrary GPU byte buffers. */
nx_result nx_mtl_batches_begin(nx_mtl_batches *batches, const nx_mtl_record *records,
    size_t count, uint32_t width, uint32_t height);
/* Each group begins with at most one destination-reading record, followed by
   opaque records. The next destination reader starts a new group. Order is
   unchanged; every record also carries its own shader-enforced scissor. */
int nx_mtl_batches_next(nx_mtl_batches *batches, nx_mtl_batch *out_batch);

#endif
