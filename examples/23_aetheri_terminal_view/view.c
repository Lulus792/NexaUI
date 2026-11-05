#include "view.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

enum { TEXT_LIMIT=NX_TEXT_SNAPSHOT_MAX_BYTES };
struct aetheri_terminal_view {
    aetheri_terminal_view_desc desc;
    nx_context *context;
    nx_cell_view *cells;
    char *text;
    size_t rows,columns,text_length,anchor,cursor;
    uint64_t revision,last_revision,last_sequence;
    int has_caret,selecting;
    struct ae_term_selection last_selection;
};
static int same_selection(const struct ae_term_selection *a,const struct ae_term_selection *b) {
    if(a->count>AE_TERM_LINES || b->count>AE_TERM_LINES || a->count!=b->count ||
       a->anchor!=b->anchor || a->anchor_col!=b->anchor_col || a->first_col!=b->first_col ||
       a->last_col!=b->last_col || a->reverse!=b->reverse)return 0;
    return !a->count || !memcmp(a->rows,b->rows,(size_t)a->count*sizeof(a->rows[0]));
}
static nx_color color(uint32_t value) {
    return (nx_color){(float)((value>>16)&255u)/255.f,(float)((value>>8)&255u)/255.f,
                      (float)(value&255u)/255.f,1.f};
}
nx_result aetheri_terminal_resolve_glyph(const aetheri_terminal_view_desc *d,int bold,
    uint32_t scalar,const nx_font **font,uint16_t *glyph) {
    if(!font || !glyph)return NX_ERROR_ARGUMENT;
    *font=NULL;*glyph=0;
    if(!d || (bold!=0 && bold!=1))return NX_ERROR_ARGUMENT;
    const nx_font *primary=bold?d->bold:d->regular;
    const nx_font_collection *fallback=bold?d->bold_fallback:d->regular_fallback;
    if(primary) {
        nx_result r=nx_font_glyph_index(primary,scalar,glyph);if(r!=NX_OK)return r;
        if(*glyph || !fallback){*font=primary;return NX_OK;}
    }
    if(!fallback)return NX_ERROR_UNSUPPORTED;
    nx_font_match match={0};nx_result r=nx_font_collection_resolve(fallback,scalar,&match);
    if(r!=NX_OK)return r;
    if(!match.font || !match.glyph_index)return NX_ERROR_UNSUPPORTED;
    *font=match.font;*glyph=match.glyph_index;return NX_OK;
}
static uint64_t visible_token(const struct ae_terminal *m,unsigned row) {
    unsigned offset=m->alt?0:m->view;
    if(row<offset)return m->history_rows[(m->history_head+m->history_count-offset+row)%AE_TERM_HISTORY].token;
    return m->screen_rows[row-offset].token;
}
static nx_result append(char *out,size_t *used,uint32_t scalar) {
    if(!scalar || scalar>0x10ffffu || (scalar>=0xd800u && scalar<=0xdfffu))return NX_ERROR_ARGUMENT;
    size_t count=scalar<0x80u?1u:scalar<0x800u?2u:scalar<0x10000u?3u:4u;
    if(count>TEXT_LIMIT-*used)return NX_ERROR_LIMIT;
    if(count==1)out[(*used)++]=(char)scalar;
    else {
        for(size_t i=count-1;i>0;--i){out[*used+i]=(char)(0x80u|(scalar&63u));scalar>>=6;}
        out[*used]=(char)((count==2?0xc0u:count==3?0xe0u:0xf0u)|scalar);*used+=count;
    }
    return NX_OK;
}
nx_result aetheri_terminal_view_create(const aetheri_terminal_view_desc *d,aetheri_terminal_view **out) {
    if(!out)return NX_ERROR_ARGUMENT;
    *out=NULL;
    if(!d || !d->model || !d->regular || !d->text_identity ||
       !isfinite(d->cell_width) || d->cell_width<=0 || !isfinite(d->cell_height) || d->cell_height<=0 ||
       !isfinite(d->baseline) || d->baseline<0 || d->baseline>d->cell_height ||
       !isfinite(d->pixels_per_em) || d->pixels_per_em<.125 || d->pixels_per_em>16384)return NX_ERROR_ARGUMENT;
    aetheri_terminal_view *v=calloc(1,sizeof(*v));if(!v)return NX_ERROR_MEMORY;
    v->desc=*d;v->cells=calloc((size_t)AE_TERM_ROWS*AE_TERM_COLS,sizeof(*v->cells));v->text=malloc(TEXT_LIMIT);
    if(!v->cells || !v->text){aetheri_terminal_view_destroy(v);return NX_ERROR_MEMORY;}
    nx_result r=nx_context_create(NULL,&v->context);
    if(r!=NX_OK){aetheri_terminal_view_destroy(v);return r;}
    *out=v;return NX_OK;
}
void aetheri_terminal_view_destroy(aetheri_terminal_view *v) {
    if(!v)return;
    nx_context_destroy(v->context);free(v->cells);free(v->text);free(v);
}
nx_context *aetheri_terminal_view_context(aetheri_terminal_view *v){return v?v->context:NULL;}
static nx_cell_grid_desc grid(const aetheri_terminal_view *v) {
    return (nx_cell_grid_desc){.bounds={0,0,(float)v->columns*v->desc.cell_width,(float)v->rows*v->desc.cell_height},
        .cell_width=v->desc.cell_width,.cell_height=v->desc.cell_height,.baseline=v->desc.baseline,
        .pixels_per_em=v->desc.pixels_per_em,.cells=v->cells,.rows=v->rows,.columns=v->columns,
        .row_stride=v->columns,.cell_capacity=v->rows*v->columns,.glyph_cache_bytes=65536,
        .mask_budget_bytes=NX_CELL_GRID_MAX_MASK_BYTES,.cursor_color={.64f,.78f,1,1}};
}
nx_result aetheri_terminal_view_frame_with_overlay(aetheri_terminal_view *v,uint64_t revision,float scale,
    aetheri_terminal_overlay overlay,void *user) {
    if(!v || !revision || !isfinite(scale) || scale<=0)return NX_ERROR_ARGUMENT;
    if(v->last_revision && revision<v->last_revision)return NX_ERROR_STATE;
    struct ae_terminal *m=v->desc.model;
    if(revision==v->last_revision && (m->sequence!=v->last_sequence ||
       !same_selection(&m->selection,&v->last_selection)))return NX_ERROR_STATE;
    if(m->cols<2 || m->cols>AE_TERM_COLS || m->rows<2 || m->rows>AE_TERM_ROWS ||
       m->history_count>AE_TERM_HISTORY || m->history_head>=AE_TERM_HISTORY ||
       (!m->alt && m->view>m->history_count) || m->selection.count>AE_TERM_LINES)return NX_ERROR_ARGUMENT;
    v->last_revision=revision;v->last_sequence=m->sequence;v->last_selection=m->selection;
    v->revision=0;v->rows=m->rows;v->columns=m->cols;v->text_length=0;
    size_t caret=0,selection_first=0,selection_last=0;int selection_visible=0,first_visible=0,last_visible=0;
    for(unsigned y=0;y<m->rows;++y) {
        const struct ae_term_cell *source=ae_terminal_row(m,y);if(!source)return NX_ERROR_STATE;
        if(m->selection.count && m->selection.count<=AE_TERM_LINES) {
            uint64_t token=visible_token(m,y);
            if(token && token==m->selection.rows[0])first_visible=1;
            if(token && token==m->selection.rows[m->selection.count-1])last_visible=1;
        }
        unsigned first=0,last=0;int selected=ae_terminal_selected_range(m,y,&first,&last);
        if(selected && (first>=last || last>m->cols))return NX_ERROR_STATE;
        size_t offsets[AE_TERM_COLS+1];
        for(unsigned x=0;x<m->cols;++x) {
            const struct ae_term_cell *s=&source[x];nx_cell_view *c=&v->cells[(size_t)y*m->cols+x];
            offsets[x]=v->text_length;*c=(nx_cell_view){0};
            if(s->width>2 || s->count>3 || s->fg>0xffffffu || s->bg>0xffffffu ||
               (s->style&~(AE_TERM_BOLD|AE_TERM_UNDERLINE|AE_TERM_REVERSE)))return NX_ERROR_STATE;
            c->span=s->width;uint32_t fg=s->fg,bg=s->bg;
            if(s->style&AE_TERM_REVERSE){uint32_t t=fg;fg=bg;bg=t;}
            if(selected && x>=first && x<last){fg=0xffffff;bg=0x304e78;}
            c->foreground=color(fg);c->background=color(bg);
            if(s->style&AE_TERM_UNDERLINE)c->flags|=NX_CELL_UNDERLINE;
            if(m->cursor && !m->view && m->y==y && m->x==x)c->flags|=NX_CELL_CURSOR_OUTLINE;
            if(!s->width) {if(s->count)return NX_ERROR_STATE;continue;}
            nx_result r=append(v->text,&v->text_length,s->code);if(r!=NX_OK)return r;
            int bold=(s->style&AE_TERM_BOLD)!=0;
            if(bold && !v->desc.bold && !v->desc.bold_fallback)return NX_ERROR_UNSUPPORTED;
            if(s->code!=' ') {
                const nx_font *font=NULL;uint16_t glyph=0;
                r=aetheri_terminal_resolve_glyph(&v->desc,bold,s->code,&font,&glyph);if(r!=NX_OK)return r;
                c->glyphs[c->glyph_count++]=(nx_cell_glyph){font,glyph,0,0};
            }
            for(unsigned j=0;j<s->count;++j) {
                r=append(v->text,&v->text_length,s->marks[j]);if(r!=NX_OK)return r;
                if(!v->desc.mark_position)return NX_ERROR_UNSUPPORTED;
                float ox=0,oy=0;r=v->desc.mark_position(v->desc.mark_user,s->code,s->marks[j],j,&ox,&oy);
                if(r!=NX_OK)return r;
                if(!isfinite(ox) || !isfinite(oy))return NX_ERROR_ARGUMENT;
                const nx_font *font=NULL;uint16_t glyph=0;
                r=aetheri_terminal_resolve_glyph(&v->desc,bold,s->marks[j],&font,&glyph);if(r!=NX_OK)return r;
                c->glyphs[c->glyph_count++]=(nx_cell_glyph){font,glyph,ox,oy};
            }
        }
        offsets[m->cols]=v->text_length;
        if(!m->view && m->y==y && m->x<m->cols)caret=offsets[m->x];
        if(selected){if(!selection_visible)selection_first=offsets[first];selection_last=offsets[last];selection_visible=1;}
        if(y+1<m->rows){nx_result r=append(v->text,&v->text_length,'\n');if(r!=NX_OK)return r;}
    }
    v->anchor=selection_visible?(m->selection.reverse?selection_last:selection_first):caret;
    v->cursor=selection_visible?(m->selection.reverse?selection_first:selection_last):caret;
    v->has_caret=selection_visible?(first_visible && last_visible):(!m->view && m->cursor && m->y<m->rows && m->x<m->cols);
    nx_cell_grid_desc d=grid(v);
    nx_result r=nx_context_set_text_raster_scale(v->context,scale);if(r!=NX_OK)return r;
    r=nx_begin_frame(v->context,d.bounds.width,d.bounds.height,0);if(r!=NX_OK)return r;
    nx_semantic_desc root={.id=1,.role=NX_ROLE_WINDOW,.bounds=d.bounds,.name={"Aetheri Terminal",16}};
    nx_semantic_desc text={.id=2,.parent_id=1,.role=NX_ROLE_TEXT,.bounds=d.bounds,
        .name={"Visible terminal text",21},.value={v->text,v->text_length},.state=NX_SEM_READONLY};
    r=nx_draw_cell_grid(v->context,&d);
    if(r==NX_OK)r=nx_semantics_add(v->context,&root,NULL);
    if(r==NX_OK)r=v->has_caret?
        nx_semantics_add_readonly_text_snapshot(v->context,&text,v->desc.text_identity,v->anchor,v->cursor,NULL):
        nx_semantics_add_readonly_text_snapshot_without_caret(v->context,&text,v->desc.text_identity,NULL);
    if(r==NX_OK && overlay)r=overlay(user,v->context,&v->desc);
    nx_result end=nx_end_frame(v->context);if(r!=NX_OK)return r;if(end!=NX_OK)return end;
    v->revision=revision;return NX_OK;
}
nx_result aetheri_terminal_view_frame(aetheri_terminal_view *v,uint64_t revision,float scale) {
    return aetheri_terminal_view_frame_with_overlay(v,revision,scale,NULL,NULL);
}
nx_result aetheri_terminal_view_cells(const aetheri_terminal_view *v,uint64_t revision,
    const nx_cell_view **cells,size_t *rows,size_t *columns) {
    if(!cells || !rows || !columns)return NX_ERROR_ARGUMENT;
    *cells=NULL;*rows=*columns=0;
    if(!v || !revision)return NX_ERROR_ARGUMENT;
    if(v->revision!=revision || v->last_sequence!=v->desc.model->sequence ||
       !same_selection(&v->last_selection,&v->desc.model->selection))return NX_ERROR_STATE;
    *cells=v->cells;*rows=v->rows;*columns=v->columns;return NX_OK;
}
nx_result aetheri_terminal_view_text(const aetheri_terminal_view *v,uint64_t revision,
    nx_utf8_span *text,size_t *anchor,size_t *cursor,int *has_caret) {
    if(!text || !anchor || !cursor || !has_caret)return NX_ERROR_ARGUMENT;
    *text=(nx_utf8_span){0};*anchor=*cursor=0;*has_caret=0;
    if(!v || !revision)return NX_ERROR_ARGUMENT;
    if(v->revision!=revision || v->last_sequence!=v->desc.model->sequence ||
       !same_selection(&v->last_selection,&v->desc.model->selection))return NX_ERROR_STATE;
    *text=(nx_utf8_span){v->text,v->text_length};*anchor=v->anchor;*cursor=v->cursor;*has_caret=v->has_caret;return NX_OK;
}
nx_result aetheri_terminal_view_hit(const aetheri_terminal_view *v,uint64_t revision,
    float x,float y,size_t *row,size_t *column) {
    if(row)*row=0;
    if(column)*column=0;
    if(!v || !revision || !row || !column)return NX_ERROR_ARGUMENT;
    if(v->revision!=revision || v->last_sequence!=v->desc.model->sequence ||
       !same_selection(&v->last_selection,&v->desc.model->selection))return NX_ERROR_STATE;
    nx_cell_grid_desc d=grid(v);return nx_cell_grid_hit_test(&d,x,y,row,column);
}

/* Match Aetheri's selection boundary policy, not cell_grid_hit_test's cell
   identity policy. Aetheri resolves a continuation boundary in its model. */
static int selection_point(const aetheri_terminal_view *v,float x,float y,int clamp,
                           unsigned *row,unsigned *column) {
    if(!isfinite(x) || !isfinite(y))return 0;
    float col=x/v->desc.cell_width,line=y/v->desc.cell_height;
    if(!clamp && (col<0 || col>=(float)v->columns || line<0 || line>=(float)v->rows))return 0;
    col=fmaxf(0,fminf((float)v->columns,floorf(col+.5f)));
    line=fmaxf(0,fminf((float)v->rows-1,floorf(line)));
    *row=(unsigned)line;*column=(unsigned)col;return 1;
}
nx_result aetheri_terminal_view_selection_event(aetheri_terminal_view *v,uint64_t revision,
    const nx_event *e,int *handled,int *changed) {
    if(!handled || !changed || handled==changed)return NX_ERROR_ARGUMENT;
    *handled=*changed=0;
    if(!v || !revision || !e)return NX_ERROR_ARGUMENT;
    if(e->type==NX_EVENT_POINTER_CANCEL || (e->type==NX_EVENT_FOCUS && !e->focused)) {
        v->selecting=0;return NX_OK;
    }
    if(v->revision!=revision || v->last_sequence!=v->desc.model->sequence ||
       !same_selection(&v->last_selection,&v->desc.model->selection)) {
        v->selecting=0;return NX_ERROR_STATE;
    }
    struct ae_terminal *m=v->desc.model;unsigned row=0,col=0;
    if(e->type==NX_EVENT_POINTER_DOWN && e->button==NX_BUTTON_LEFT) {
        v->selecting=0;
        if(!selection_point(v,e->x,e->y,0,&row,&col))return NX_OK;
        if(revision==UINT64_MAX)return NX_ERROR_LIMIT;
        v->selecting=ae_terminal_select_begin(m,row,col);*handled=*changed=1;
    } else if((e->type==NX_EVENT_POINTER_MOVE ||
              (e->type==NX_EVENT_POINTER_UP && e->button==NX_BUTTON_LEFT)) && v->selecting) {
        if(!selection_point(v,e->x,e->y,1,&row,&col)){v->selecting=0;return NX_ERROR_ARGUMENT;}
        if(revision==UINT64_MAX){v->selecting=0;return NX_ERROR_LIMIT;}
        if(!ae_terminal_select_extend(m,row,col))v->selecting=0;
        if(e->type==NX_EVENT_POINTER_UP)v->selecting=0;
        *handled=*changed=1;
    } else if(e->type==NX_EVENT_KEY_DOWN && e->key==NX_KEY_ESCAPE && m->selection.count) {
        if(revision==UINT64_MAX)return NX_ERROR_LIMIT;
        ae_terminal_select_clear(m);v->selecting=0;*handled=*changed=1;
    } else if(e->type==NX_EVENT_KEY_DOWN &&
              (e->modifiers&(NX_MOD_CONTROL|NX_MOD_SHIFT|NX_MOD_ALT|NX_MOD_SUPER))==
              (NX_MOD_CONTROL|NX_MOD_SHIFT)) {
        int dx=e->key==NX_KEY_LEFT?-1:e->key==NX_KEY_RIGHT?1:0;
        int dy=e->key==NX_KEY_UP?-1:e->key==NX_KEY_DOWN?1:0;
        if(e->key!=NX_KEY_A && !dx && !dy)return NX_OK;
        if(revision==UINT64_MAX)return NX_ERROR_LIMIT;
        if(e->key==NX_KEY_A)(void)ae_terminal_select_all(m);
        else (void)ae_terminal_select_step(m,dx,dy);
        v->selecting=0;*handled=*changed=1;
    }
    if(*changed)v->revision=0;
    return NX_OK;
}
