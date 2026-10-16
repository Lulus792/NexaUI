#include "ot_layout.h"
#include <math.h>

static nx_result device(nx_ot_view v,size_t at,double ppem,double *delta) {
    *delta=0;
    if (!nx_ot_has(v,at,6)) return NX_ERROR_ARGUMENT;
    unsigned first=nx_ot_u16(v.data+at),last=nx_ot_u16(v.data+at+2),format=nx_ot_u16(v.data+at+4);
    /* This engine currently renders default variation instances. The standard
       explicitly permits ignoring VariationIndex without variation support. */
    if (format==0x8000u) return NX_OK;
    if (format<1 || format>3) return NX_ERROR_UNSUPPORTED;
    if (first>last) return NX_ERROR_ARGUMENT;
    unsigned bits=1u<<format,per_word=16u/bits;
    size_t count=(size_t)last-first+1u,words=count/per_word+(count%per_word!=0);
    if (!nx_ot_array(v,at,6,words,2)) return NX_ERROR_ARGUMENT;
    /* Fractional ppem has no integer device grid; keep design coordinates. */
    if (ppem<first || ppem>last || floor(ppem)!=ppem) return NX_OK;
    size_t index=(size_t)ppem-first;
    unsigned word=nx_ot_u16(v.data+at+6u+2u*(index/per_word));
    unsigned shift=16u-bits-(unsigned)(index%per_word)*bits,mask=(1u<<bits)-1u;
    unsigned raw=(word>>shift)&mask;
    *delta=(raw&(1u<<(bits-1u)))?(double)((int)raw-(int)(1u<<bits)):(double)raw;
    return NX_OK;
}
static nx_result anchor(nx_ot_view v,size_t at,double scale,double ppem,double *x,double *y) {
    if (!nx_ot_has(v,at,6)) return NX_ERROR_ARGUMENT;
    unsigned format=nx_ot_u16(v.data+at);
    *x=(int16_t)nx_ot_u16(v.data+at+2)*scale;
    *y=(int16_t)nx_ot_u16(v.data+at+4)*scale;
    if (format==1) return NX_OK;
    /* No TrueType hinting is performed: format 2 uses its specified design-unit
       fallback instead of claiming hinted contour-point placement. */
    if (format==2) return nx_ot_has(v,at,8)?NX_OK:NX_ERROR_ARGUMENT;
    if (format!=3) return NX_ERROR_UNSUPPORTED;
    if (!nx_ot_has(v,at,10)) return NX_ERROR_ARGUMENT;
    double *coords[]={x,y};
    for (unsigned axis=0;axis<2;++axis) {
        size_t relative=nx_ot_u16(v.data+at+6u+2u*axis),child;
        if (!relative) continue;
        if (!nx_ot_child(v,at,relative,6,&child)) return NX_ERROR_ARGUMENT;
        double adjustment=0; nx_result r=device(v,child,ppem,&adjustment);
        if (r!=NX_OK) return r;
        *coords[axis]+=adjustment;
    }
    return NX_OK;
}
static nx_result anchors_validate(nx_ot_view v,size_t base,size_t start,size_t cells,size_t *work) {
    if (!nx_ot_array(v,start,0,cells,2)) return NX_ERROR_ARGUMENT;
    if (nx_ot_charge(work,cells)!=NX_OK) return NX_ERROR_LIMIT;
    for (size_t i=0;i<cells;++i) {
        size_t relative=nx_ot_u16(v.data+start+2*i),child;
        if (!relative) continue;
        if (!nx_ot_child(v,base,relative,6,&child)) return NX_ERROR_ARGUMENT;
        double x,y; nx_result r=anchor(v,child,1,0,&x,&y); if (r!=NX_OK) return r;
    }
    return NX_OK;
}
nx_result nx_gpos_mark_validate(nx_ot_view v,size_t sub,uint16_t type,uint16_t limit,size_t *work) {
    if (!nx_ot_has(v,sub,12)) return NX_ERROR_ARGUMENT;
    if (nx_ot_u16(v.data+sub)!=1) return NX_ERROR_UNSUPPORTED;
    size_t mc,bc,marks,bases,mark_count,base_count;
    if (!nx_ot_child(v,sub,nx_ot_u16(v.data+sub+2),4,&mc) ||
        !nx_ot_child(v,sub,nx_ot_u16(v.data+sub+4),4,&bc) ||
        !nx_ot_child(v,sub,nx_ot_u16(v.data+sub+8),2,&marks) ||
        !nx_ot_child(v,sub,nx_ot_u16(v.data+sub+10),2,&bases)) return NX_ERROR_ARGUMENT;
    nx_result r=nx_ot_coverage_validate(v,mc,limit,&mark_count,work); if (r!=NX_OK) return r;
    r=nx_ot_coverage_validate(v,bc,limit,&base_count,work); if (r!=NX_OK) return r;
    size_t classes=nx_ot_u16(v.data+sub+6);
    if ((!classes && mark_count) || nx_ot_u16(v.data+marks)!=mark_count ||
        nx_ot_u16(v.data+bases)!=base_count || !nx_ot_array(v,marks,2,mark_count,4)) return NX_ERROR_ARGUMENT;
    if (nx_ot_charge(work,mark_count)!=NX_OK) return NX_ERROR_LIMIT;
    for (size_t i=0;i<mark_count;++i) {
        size_t child;
        if (nx_ot_u16(v.data+marks+2+4*i)>=classes ||
            !nx_ot_child(v,marks,nx_ot_u16(v.data+marks+4+4*i),6,&child)) return NX_ERROR_ARGUMENT;
        double x,y; r=anchor(v,child,1,0,&x,&y); if (r!=NX_OK) return r;
    }
    if (type!=5) {
        if (!nx_ot_array(v,bases,2,base_count,classes*2u)) return NX_ERROR_ARGUMENT;
        return anchors_validate(v,bases,bases+2,base_count*classes,work);
    }
    if (!nx_ot_array(v,bases,2,base_count,2)) return NX_ERROR_ARGUMENT;
    if (nx_ot_charge(work,base_count)!=NX_OK) return NX_ERROR_LIMIT;
    for (size_t i=0;i<base_count;++i) {
        size_t lig;
        if (!nx_ot_child(v,bases,nx_ot_u16(v.data+bases+2+2*i),2,&lig)) return NX_ERROR_ARGUMENT;
        size_t components=nx_ot_u16(v.data+lig);
        if (!components || !nx_ot_array(v,lig,2,components,classes*2u)) return NX_ERROR_ARGUMENT;
        r=anchors_validate(v,lig,lig+2,components*classes,work); if (r!=NX_OK) return r;
    }
    return NX_OK;
}
nx_result nx_gpos_mark_apply(nx_ot_view v,size_t sub,uint16_t type,size_t i,size_t p,
                             nx_shaped_glyph *glyphs,nx_ot_glyph_state *state,double scale,double ppem,int *matched) {
    *matched=0; if (p==SIZE_MAX) return NX_OK;
    size_t cov=sub+nx_ot_u16(v.data+sub+4);
    size_t base=nx_ot_coverage_index(v,cov,glyphs[p].glyph_index); if (base==SIZE_MAX) return NX_OK;
    cov=sub+nx_ot_u16(v.data+sub+2);
    size_t mark=nx_ot_coverage_index(v,cov,glyphs[i].glyph_index); if (mark==SIZE_MAX) return NX_OK;
    if (type==6 && state[i].ligature_id && state[p].ligature_id &&
        (state[i].ligature_id!=state[p].ligature_id || state[i].component!=state[p].component)) return NX_OK;
    size_t classes=nx_ot_u16(v.data+sub+6),marks=sub+nx_ot_u16(v.data+sub+8),bases=sub+nx_ot_u16(v.data+sub+10);
    size_t c=nx_ot_u16(v.data+marks+2+4*mark),anchor_base=bases;
    size_t record=bases+2u+2u*(base*classes+c);
    if (type==5) {
        anchor_base=bases+nx_ot_u16(v.data+bases+2u+2u*base);
        size_t components=nx_ot_u16(v.data+anchor_base),component=components-1u;
        if (state[i].ligature_id && state[i].ligature_id==state[p].ligature_id && state[i].component) {
            component=state[i].component-1u;
            if (component>=components) component=components-1u;
        }
        record=anchor_base+2u+2u*(component*classes+c);
    }
    size_t relative=nx_ot_u16(v.data+record); if (!relative) return NX_OK;
    double bx,by,mx,my;
    nx_result r=anchor(v,anchor_base+relative,scale,ppem,&bx,&by); if (r!=NX_OK) return r;
    r=anchor(v,marks+nx_ot_u16(v.data+marks+4u+4u*mark),scale,ppem,&mx,&my); if (r!=NX_OK) return r;
    state[i].parent=p; state[i].anchor_x=bx-mx; state[i].anchor_y=by-my;
    glyphs[i].offset_x=glyphs[i].offset_y=0; *matched=1; return NX_OK;
}
