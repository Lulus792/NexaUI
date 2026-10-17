#ifndef NEXAUI_OT_LAYOUT_H
#define NEXAUI_OT_LAYOUT_H
#include "nexaui/nexaui.h"

/* Private OpenType state: provenance/attachments never enlarge the public ABI. */
typedef struct nx_ot_glyph_state {
    size_t origin, ligature_id, parent;
    uint32_t component, components;
    double anchor_x, anchor_y, pen;
    uint8_t mark, nonspacing, ligated;
} nx_ot_glyph_state;
typedef struct nx_ot_view { const uint8_t *data; size_t length; } nx_ot_view;
typedef struct nx_ot_gdef {
    nx_ot_view view;
    size_t classes, mark_classes, mark_sets;
    uint16_t glyph_limit, set_count;
} nx_ot_gdef;
static inline uint16_t nx_ot_u16(const uint8_t *p) { return (uint16_t)((uint16_t)p[0]<<8u | p[1]); }
static inline uint32_t nx_ot_u32(const uint8_t *p) { return (uint32_t)p[0]<<24u | (uint32_t)p[1]<<16u | (uint32_t)p[2]<<8u | p[3]; }
static inline int nx_ot_has(nx_ot_view v,size_t offset,size_t bytes) {
    return offset<=v.length && bytes<=v.length-offset;
}
static inline int nx_ot_array(nx_ot_view v,size_t offset,size_t header,size_t count,size_t stride) {
    return nx_ot_has(v,offset,header) && (!stride || count<=(v.length-offset-header)/stride);
}
static inline int nx_ot_child(nx_ot_view v,size_t base,size_t offset,size_t bytes,size_t *out) {
    if (!offset || base>v.length || offset>v.length-base || !nx_ot_has(v,base+offset,bytes)) return 0;
    *out=base+offset; return 1;
}
static inline nx_result nx_ot_charge(size_t *left,size_t cost) {
    if (cost>*left) return NX_ERROR_LIMIT;
    *left-=cost; return NX_OK;
}
nx_result nx_ot_coverage_validate(nx_ot_view view,size_t offset,uint16_t limit,size_t *count,size_t *work);
size_t nx_ot_coverage_index(nx_ot_view view,size_t offset,uint16_t glyph);
nx_result nx_ot_class_validate(nx_ot_view view,size_t offset,uint16_t limit,size_t *work);
unsigned nx_ot_class_at(nx_ot_view view,size_t offset,uint16_t glyph);
nx_result nx_ot_gdef_load(const nx_font *font,nx_ot_gdef *out,uint16_t glyph_limit,size_t *work);
nx_result nx_ot_filter_validate(const nx_ot_gdef *g,uint16_t flags,uint16_t set);
int nx_ot_ignored(const nx_ot_gdef *g,uint16_t flags,uint16_t set,uint16_t glyph);
unsigned nx_ot_glyph_class(const nx_ot_gdef *g,const nx_shaped_glyph *glyph,const nx_ot_glyph_state *state);
void nx_ot_state_init(const nx_shaped_glyph *glyph,nx_ot_glyph_state *state);
nx_result nx_ot_resolve(nx_shaped_glyph *glyphs,nx_ot_glyph_state *state,size_t count);
nx_result nx_gsub_apply_state(const nx_font *font,uint32_t script,nx_shaped_glyph *glyphs,
                              nx_ot_glyph_state *state,size_t *count);
nx_result nx_gsub_context_apply(nx_ot_view view,size_t lookups,size_t lookup_count,
    size_t root,uint16_t glyph_limit,nx_shaped_glyph *glyphs,nx_ot_glyph_state *state,
    size_t *count,const nx_ot_gdef *gdef,size_t *work);
nx_result nx_gpos_apply_state(const nx_font *font,uint32_t script,nx_shaped_glyph *glyphs,
                              nx_ot_glyph_state *state,size_t count,double ppem,uint8_t *pair_mask);
nx_result nx_gpos_mark_validate(nx_ot_view view,size_t sub,uint16_t type,uint16_t limit,size_t *work);
nx_result nx_gpos_mark_apply(nx_ot_view view,size_t sub,uint16_t type,size_t current,size_t parent,
                             nx_shaped_glyph *glyphs,nx_ot_glyph_state *state,double scale,double ppem,int *matched);
#endif
