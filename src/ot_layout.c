#include "ot_layout.h"
#include "unicode_data.h"
#include <math.h>

nx_result nx_ot_coverage_validate(nx_ot_view v,size_t at,uint16_t limit,size_t *covered,size_t *work) {
    if (!nx_ot_has(v,at,4)) return NX_ERROR_ARGUMENT;
    unsigned format=nx_ot_u16(v.data+at); size_t count=nx_ot_u16(v.data+at+2),total=0;
    if (format!=1 && format!=2) return NX_ERROR_UNSUPPORTED;
    if (!nx_ot_array(v,at,4,count,format==1?2u:6u)) return NX_ERROR_ARGUMENT;
    if (nx_ot_charge(work,count+1u)!=NX_OK) return NX_ERROR_LIMIT;
    uint16_t previous=0;
    for (size_t i=0;i<count;++i) {
        const uint8_t *p=v.data+at+4+i*(format==1?2u:6u);
        uint16_t first=nx_ot_u16(p),last=format==1?first:nx_ot_u16(p+2);
        if (first>last || last>=limit || (i && first<=previous) ||
            (format==2 && nx_ot_u16(p+4)!=total)) return NX_ERROR_ARGUMENT;
        previous=last; total+=(size_t)last-first+1u;
    }
    *covered=total; return NX_OK;
}
size_t nx_ot_coverage_index(nx_ot_view v,size_t at,uint16_t glyph) {
    unsigned format=nx_ot_u16(v.data+at); size_t low=0,high=nx_ot_u16(v.data+at+2);
    while (low<high) {
        size_t mid=low+(high-low)/2u;
        const uint8_t *p=v.data+at+4u+mid*(format==1?2u:6u);
        uint16_t first=nx_ot_u16(p),last=format==1?first:nx_ot_u16(p+2);
        if (glyph<first) high=mid;
        else if (glyph>last) low=mid+1u;
        else return format==1?mid:(size_t)nx_ot_u16(p+4)+glyph-first;
    }
    return SIZE_MAX;
}
static nx_result classes_validate(nx_ot_view v,size_t at,unsigned maximum,uint16_t limit,size_t *work) {
    if (!nx_ot_has(v,at,4)) return NX_ERROR_ARGUMENT;
    unsigned format=nx_ot_u16(v.data+at); size_t count;
    if (format==1) {
        if (!nx_ot_has(v,at,6)) return NX_ERROR_ARGUMENT;
        count=nx_ot_u16(v.data+at+4);
        if ((size_t)nx_ot_u16(v.data+at+2)+count>limit || !nx_ot_array(v,at,6,count,2)) return NX_ERROR_ARGUMENT;
        if (nx_ot_charge(work,count+1u)!=NX_OK) return NX_ERROR_LIMIT;
        for (size_t i=0;i<count;++i) if (nx_ot_u16(v.data+at+6+2*i)>maximum) return NX_ERROR_ARGUMENT;
    } else if (format==2) {
        count=nx_ot_u16(v.data+at+2);
        if (!nx_ot_array(v,at,4,count,6)) return NX_ERROR_ARGUMENT;
        if (nx_ot_charge(work,count+1u)!=NX_OK) return NX_ERROR_LIMIT;
        uint16_t previous=0;
        for (size_t i=0;i<count;++i) {
            const uint8_t *p=v.data+at+4+6*i; uint16_t first=nx_ot_u16(p),last=nx_ot_u16(p+2);
            if (first>last || last>=limit || (i && first<=previous) || nx_ot_u16(p+4)>maximum) return NX_ERROR_ARGUMENT;
            previous=last;
        }
    } else return NX_ERROR_UNSUPPORTED;
    return NX_OK;
}
static unsigned class_at(nx_ot_view v,size_t at,uint16_t glyph) {
    if (!at) return 0;
    unsigned format=nx_ot_u16(v.data+at); size_t low=0;
    if (format==1) {
        uint16_t first=nx_ot_u16(v.data+at+2); size_t count=nx_ot_u16(v.data+at+4);
        return glyph>=first && (size_t)glyph-first<count?nx_ot_u16(v.data+at+6u+2u*((size_t)glyph-first)):0;
    }
    size_t high=nx_ot_u16(v.data+at+2);
    while (low<high) {
        size_t mid=low+(high-low)/2u; const uint8_t *p=v.data+at+4+mid*6;
        if (glyph<nx_ot_u16(p)) high=mid;
        else if (glyph>nx_ot_u16(p+2)) low=mid+1u;
        else return nx_ot_u16(p+4);
    }
    return 0;
}
nx_result nx_ot_class_validate(nx_ot_view v,size_t at,uint16_t limit,size_t *work) {
    return classes_validate(v,at,UINT16_MAX,limit,work);
}
unsigned nx_ot_class_at(nx_ot_view v,size_t at,uint16_t glyph) { return class_at(v,at,glyph); }
nx_result nx_ot_gdef_load(const nx_font *font,nx_ot_gdef *out,uint16_t limit,size_t *work) {
    *out=(nx_ot_gdef){0}; out->glyph_limit=limit;
    nx_bytes_view data;
    if (nx_font_find_table(font,NX_SFNT_TAG('G','D','E','F'),&data)!=NX_OK) return NX_OK;
    nx_ot_view v={data.data,data.length}; out->view=v;
    if (!nx_ot_has(v,0,12)) return NX_ERROR_ARGUMENT;
    uint32_t version=nx_ot_u32(v.data);
    size_t header=version==0x10000u?12u:version==0x10002u?14u:version==0x10003u?18u:0u;
    if (!header) return NX_ERROR_UNSUPPORTED;
    if (!nx_ot_has(v,0,header)) return NX_ERROR_ARGUMENT;
    uint16_t offsets[]={nx_ot_u16(v.data+4),nx_ot_u16(v.data+10)};
    for (unsigned i=0;i<2;++i) if (offsets[i]) {
        if (offsets[i]<header) return NX_ERROR_ARGUMENT;
        nx_result r=classes_validate(v,offsets[i],i?255u:4u,limit,work); if (r!=NX_OK) return r;
    }
    out->classes=offsets[0]; out->mark_classes=offsets[1];
    if (header>=14u) out->mark_sets=nx_ot_u16(v.data+12);
    if (out->mark_sets) {
        size_t at=out->mark_sets;
        if (at<header || !nx_ot_has(v,at,4)) return NX_ERROR_ARGUMENT;
        if (nx_ot_u16(v.data+at)!=1) return NX_ERROR_UNSUPPORTED;
        out->set_count=nx_ot_u16(v.data+at+2);
        if (!nx_ot_array(v,at,4,out->set_count,4)) return NX_ERROR_ARGUMENT;
        if (nx_ot_charge(work,(size_t)out->set_count+1)!=NX_OK) return NX_ERROR_LIMIT;
        for (size_t i=0;i<out->set_count;++i) {
            size_t cov,count;
            if (!nx_ot_child(v,at,nx_ot_u32(v.data+at+4+i*4),4,&cov)) return NX_ERROR_ARGUMENT;
            nx_result r=nx_ot_coverage_validate(v,cov,limit,&count,work); if (r!=NX_OK) return r;
        }
    }
    return NX_OK;
}
nx_result nx_ot_filter_validate(const nx_ot_gdef *g,uint16_t flags,uint16_t set) {
    if (flags&0xe0u) return NX_ERROR_UNSUPPORTED;
    if ((flags&0x1eu) && !g->classes) return NX_ERROR_ARGUMENT;
    if (flags&8u) return NX_OK; /* IgnoreMarks supersedes both mark filters. */
    if (flags&16u) return g->mark_sets && set<g->set_count?NX_OK:NX_ERROR_ARGUMENT;
    if ((flags&0xff00u) && (!g->classes || !g->mark_classes)) return NX_ERROR_ARGUMENT;
    return NX_OK;
}
int nx_ot_ignored(const nx_ot_gdef *g,uint16_t flags,uint16_t set,uint16_t glyph) {
    unsigned c=class_at(g->view,g->classes,glyph);
    if ((c==1 && (flags&2u)) || (c==2 && (flags&4u)) || (c==3 && (flags&8u))) return 1;
    if (c!=3) return 0;
    if (flags&16u) {
        size_t at=g->mark_sets+nx_ot_u32(g->view.data+g->mark_sets+4u+4u*set);
        return nx_ot_coverage_index(g->view,at,glyph)==SIZE_MAX;
    }
    return (flags&0xff00u) && class_at(g->view,g->mark_classes,glyph)!=((unsigned)flags>>8u);
}
unsigned nx_ot_glyph_class(const nx_ot_gdef *g,const nx_shaped_glyph *glyph,const nx_ot_glyph_state *state) {
    unsigned c=class_at(g->view,g->classes,glyph->glyph_index);
    if (c) return c;
    if (state && state->ligated) return 2;
    return state && state->mark?3u:1u;
}
void nx_ot_state_init(const nx_shaped_glyph *glyph,nx_ot_glyph_state *state) {
    *state=(nx_ot_glyph_state){0}; state->parent=SIZE_MAX; state->components=1;
    state->origin=glyph->scalar_index+1u;
    uint8_t gc=nx_ucd_gc(glyph->scalar);
    state->mark=(uint8_t)(gc==NX_UCD_GC_MN || gc==NX_UCD_GC_ME || gc==NX_UCD_GC_MC);
    state->nonspacing=(uint8_t)(gc==NX_UCD_GC_MN || gc==NX_UCD_GC_ME);
}
nx_result nx_ot_resolve(nx_shaped_glyph *glyphs,nx_ot_glyph_state *state,size_t count) {
    double pen=0;
    for (size_t i=0;i<count;++i) { state[i].pen=pen; pen+=glyphs[i].advance_x; }
    if (!isfinite(pen)) return NX_ERROR_LIMIT;
    for (size_t i=0;i<count;++i) if (state[i].parent!=SIZE_MAX) {
        size_t p=state[i].parent; if (p>=i) return NX_ERROR_ARGUMENT;
        double delta=(glyphs[i].bidi_level&1u)?state[i].pen+glyphs[i].advance_x-state[p].pen-glyphs[p].advance_x:
            state[p].pen-state[i].pen;
        glyphs[i].offset_x+=glyphs[p].offset_x+state[i].anchor_x+delta;
        glyphs[i].offset_y+=glyphs[p].offset_y+state[i].anchor_y;
        if (!isfinite(glyphs[i].offset_x) || !isfinite(glyphs[i].offset_y)) return NX_ERROR_LIMIT;
    }
    return NX_OK;
}
