#include "ime_overlay.h"
#include <math.h>
#include <string.h>

typedef struct geometry {
    struct ae_terminal_composition_view draft;
    float x,y;unsigned columns;
} geometry;
static nx_result locate(const terminal_ime_overlay *overlay,const aetheri_terminal_view_desc *d,geometry *g) {
    if(!overlay || !overlay->ime || !d || !d->model || !g)return NX_ERROR_ARGUMENT;
    if(overlay->semantic_identity && overlay->semantic_identity==d->text_identity)return NX_ERROR_ARGUMENT;
    const terminal_ime_guard a=overlay->ime->guard,b=overlay->current;
    if(!a.identity || !a.epoch || a.focused!=1 || b.focused!=1 || a.identity!=b.identity || a.epoch!=b.epoch)
        return NX_ERROR_STATE;
    const struct ae_terminal *m=d->model;
    if(!d->regular || !isfinite(d->cell_width) || d->cell_width<=0 ||
       !isfinite(d->cell_height) || d->cell_height<=0 || m->cols<2 || m->cols>AE_TERM_COLS ||
       m->rows<2 || m->rows>AE_TERM_ROWS || m->x>=m->cols || m->y>=m->rows || m->view)
        return NX_ERROR_STATE;
    if(!isfinite((float)m->cols*d->cell_width) || !isfinite((float)m->rows*d->cell_height))return NX_ERROR_LIMIT;
    *g=(geometry){0};g->columns=m->cols-m->x;g->x=(float)m->x*d->cell_width;g->y=(float)m->y*d->cell_height;
    if(overlay->ime->composition.size && g->columns<16){g->columns=m->cols;g->x=0;}
    if(overlay->ime->composition.size &&
       !ae_terminal_composition_view(&overlay->ime->composition,g->columns,m->width,&g->draft))return NX_ERROR_STATE;
    return NX_OK;
}
nx_result terminal_ime_overlay_caret(const terminal_ime_overlay *overlay,
    const aetheri_terminal_view_desc *d,nx_rect *caret) {
    if(!caret)return NX_ERROR_ARGUMENT;
    *caret=(nx_rect){0};geometry g;nx_result r=locate(overlay,d,&g);if(r!=NX_OK)return r;
    unsigned x=overlay->ime->composition.size?g.draft.cursor:0;
    if(x>=g.columns)x=g.columns-1;
    *caret=(nx_rect){g.x+(float)x*d->cell_width,g.y,d->cell_width,d->cell_height};return NX_OK;
}
nx_result terminal_ime_overlay_draw(void *user,nx_context *context,const aetheri_terminal_view_desc *d) {
    terminal_ime_overlay *overlay=user;geometry g;
    if(!context)return NX_ERROR_ARGUMENT;
    nx_result r=locate(overlay,d,&g);if(r!=NX_OK)return r;
    const struct ae_terminal_composition *c=&overlay->ime->composition;
    if(!c->size)return NX_OK;
    nx_cell_view cells[AE_TERM_COLS];memset(cells,0,sizeof(cells));
    const nx_color fg={1,1,1,1},background={38.f/255.f,48.f/255.f,67.f/255.f,1};
    const nx_color selected={48.f/255.f,78.f/255.f,120.f/255.f,1},accent={162.f/255.f,199.f/255.f,1,1};
    unsigned occupied=g.draft.cells<g.columns?g.draft.cells+1:g.columns;
    for(unsigned i=0;i<occupied;++i) {
        cells[i].span=1;cells[i].foreground=fg;
        cells[i].background=(i>=g.draft.selection_begin && i<g.draft.selection_end)?selected:background;
        cells[i].flags=NX_CELL_UNDERLINE;
    }
    size_t at=g.draft.begin;unsigned column=0,last=0,marks=0;uint32_t base=0;int have_base=0;
    while(at<g.draft.end) {
        uint32_t scalar=0;r=nx_utf8_decode(c->text,g.draft.end,&at,&scalar);if(r!=NX_OK)return r;
        int advance=d->model->width?d->model->width(scalar):1;
        if(advance<0 || advance>2)advance=1;
        const nx_font *font=NULL;uint16_t glyph=0;
        r=aetheri_terminal_resolve_glyph(d,0,scalar,&font,&glyph);if(r!=NX_OK)return r;
        if(!advance) {
            if(!have_base || marks>=3 || !d->mark_position)return NX_ERROR_UNSUPPORTED;
            float x=0,y=0;r=d->mark_position(d->mark_user,base,scalar,marks,&x,&y);
            if(r!=NX_OK)return r;
            if(!isfinite(x) || !isfinite(y))return NX_ERROR_ARGUMENT;
            cells[last].glyphs[cells[last].glyph_count++]=(nx_cell_glyph){font,glyph,x,y};++marks;
            continue;
        }
        if(column>=occupied)return NX_ERROR_STATE;
        cells[column].span=(uint8_t)advance;cells[column].glyphs[0]=(nx_cell_glyph){font,glyph,0,0};
        cells[column].glyph_count=1;last=column;base=scalar;marks=0;have_base=1;
        if(advance==2 && column+1<occupied)cells[column+1].span=0;
        column+=(unsigned)advance;
    }
    nx_cell_grid_desc grid={.bounds={g.x,g.y,(float)occupied*d->cell_width,d->cell_height},
        .cell_width=d->cell_width,.cell_height=d->cell_height,.baseline=d->baseline,.pixels_per_em=d->pixels_per_em,
        .cells=cells,.rows=1,.columns=occupied,.row_stride=occupied,.cell_capacity=occupied,
        .glyph_cache_bytes=65536,.mask_budget_bytes=NX_CELL_GRID_MAX_MASK_BYTES,.cursor_color=fg};
    r=nx_draw_cell_grid(context,&grid);if(r!=NX_OK)return r;
    /* Do not invent a visible caret when the native IME hides it. */
    float bar=fminf(2,d->cell_width);
    if(g.draft.caret) {
        nx_rect caret;if((r=terminal_ime_overlay_caret(overlay,d,&caret))!=NX_OK)return r;
        if((r=nx_fill_rect(context,(nx_rect){caret.x,caret.y,bar,caret.height},fg))!=NX_OK)return r;
    }
    if(g.draft.hidden_before && (r=nx_fill_rect(context,(nx_rect){g.x,g.y,bar,d->cell_height},accent))!=NX_OK)return r;
    if(g.draft.hidden_after && (r=nx_fill_rect(context,(nx_rect){g.x+(float)g.columns*d->cell_width-bar,g.y,bar,d->cell_height},accent))!=NX_OK)return r;
    if(overlay->semantic_identity) {
        nx_utf8_span name=overlay->semantic_name.length?overlay->semantic_name:
            (nx_utf8_span){"Uncommitted composition",23};
        nx_semantic_desc semantic={.id=3,.parent_id=1,.role=NX_ROLE_TEXT,
            .bounds={g.x,g.y,(float)occupied*d->cell_width,d->cell_height},.name=name,
            .value={c->text,c->size},.state=NX_SEM_READONLY};
        r=c->caret?nx_semantics_add_readonly_text_snapshot(context,&semantic,overlay->semantic_identity,
             c->offsets[c->anchor],c->offsets[c->cursor],NULL):
             nx_semantics_add_readonly_text_snapshot_without_caret(context,&semantic,overlay->semantic_identity,NULL);
        if(r!=NX_OK)return r;
    }
    return NX_OK;
}
