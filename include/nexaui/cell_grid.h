#ifndef NEXAUI_CELL_GRID_H
#define NEXAUI_CELL_GRID_H
#include "nexaui/nexaui.h"

#ifdef __cplusplus
extern "C" {
#endif

#define NX_CELL_GRID_MAX_CELLS 32768u
#define NX_CELL_GRID_MAX_GLYPHS 4u
#define NX_CELL_GRID_MAX_FONTS 16u
#define NX_CELL_GRID_MAX_CACHE_BYTES (4u * 1024u * 1024u)
#define NX_CELL_GRID_MAX_MASK_BYTES (32u * 1024u * 1024u)

typedef struct nx_cell_glyph {
    const nx_font *font;
    uint16_t glyph_index;
    float offset_x, offset_y; /* Logical offset from the cell's baseline. */
} nx_cell_glyph;

enum { NX_CELL_UNDERLINE = 1u, NX_CELL_CURSOR_OUTLINE = 2u };
typedef struct nx_cell_view {
    nx_color foreground, background;
    uint32_t flags;
    uint8_t span; /* 1 or 2 at a leading cell; 0 is a wide-cell continuation. */
    uint8_t glyph_count;
    nx_cell_glyph glyphs[NX_CELL_GRID_MAX_GLYPHS];
} nx_cell_view;

typedef struct nx_cell_grid_desc {
    nx_rect bounds; /* Visible logical viewport; cell (0,0) begins at its origin. */
    float cell_width, cell_height, baseline;
    double pixels_per_em;
    const nx_cell_view *cells; /* Caller-owned visible rows only. */
    size_t rows, columns, row_stride; /* row_stride is in cells, not bytes. */
    size_t cell_capacity; /* Readable elements in cells, including row padding. */
    size_t glyph_cache_bytes; /* Per-font, bounded by NX_CELL_GRID_MAX_CACHE_BYTES. */
    size_t mask_budget_bytes; /* Per-call copied A8 bytes; see MAX_MASK_BYTES. */
    nx_color cursor_color;
} nx_cell_grid_desc;

/* Paint backgrounds, pre-positioned glyphs, underline and cursor outlines in
   a clipped fixed-cell grid. The app supplies glyph IDs/fonts, cell widths,
   combining-mark placement and selection colors; no parsing, bidi, shaping,
   grapheme segmentation, focus or semantics are inferred. A continuation at
   column zero or a leading wide cell at the final column may be clipped by the
   visible range. All cells and fonts are borrowed only for this call; copies
   of glyph masks stay valid until the next begin_frame or context destruction.
   Raster density follows nx_context_set_text_raster_scale. All limits include
   visible cells only. On failure, commands and owned masks are rolled back. */
NX_API nx_result nx_draw_cell_grid(nx_context *context, const nx_cell_grid_desc *desc);

/* Return the visible zero-based row/column at a logical point. A hit on a
   visible wide-cell continuation resolves to its leading cell; a continuation
   in the first column remains column zero. Outside/clipped points return
   NX_ERROR_ARGUMENT and clear both outputs. Output pointers must differ. No
   model index is inferred. The visible-cell descriptor is validated in full,
   so this call is O(rows * columns), independent of offscreen history. */
NX_API nx_result nx_cell_grid_hit_test(const nx_cell_grid_desc *desc, float x, float y,
                                       size_t *out_row, size_t *out_column);

#ifdef __cplusplus
}
#endif
#endif
