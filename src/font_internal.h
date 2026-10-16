#ifndef NEXAUI_FONT_INTERNAL_H
#define NEXAUI_FONT_INTERNAL_H

#include "nexaui/nexaui.h"

/* Temporary work buffers use the allocator selected when the face was opened. */
void *nx_font_scratch_allocate(const nx_font *font, size_t size);
void nx_font_scratch_deallocate(const nx_font *font, void *pointer, size_t size);

/* In-place GSUB type 1/4 (including extension 7) substitution for a single homogeneous logical run.
   The glyph array may shrink; it never grows. Internal callers discard it on
   error, so malformed font data cannot leak partial public output. */
nx_result nx_gsub_apply(const nx_font *font, uint32_t script_tag,
                        nx_shaped_glyph *glyphs, size_t *inout_count);

/* In-place GPOS type 1/2 and mark 4/5/6 positioning (including extension 9)
   on a homogeneous logical run.
   pair_mask[i] marks pairs positioned by GPOS so legacy kern is not repeated. */
nx_result nx_gpos_apply(const nx_font *font, uint32_t script_tag,
                        nx_shaped_glyph *glyphs, size_t count,
                        double pixels_per_em, uint8_t *pair_mask);

#endif
