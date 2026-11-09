#include "presentation.h"
#include <math.h>
#include <stdlib.h>

static nx_result failed(struct editor_presentation *p,nx_result error) {
    p->count=0;p->error=error;return error;
}
static size_t budget(const struct editor_presentation *p) {
    return p->limit && p->limit<EDITOR_PRESENTATION_MAX_BOXES?p->limit:EDITOR_PRESENTATION_MAX_BOXES;
}
static nx_result reserve(nx_text_document_box **items,size_t *capacity,size_t n,size_t limit) {
    if(n>limit || n>SIZE_MAX/sizeof(**items))return NX_ERROR_LIMIT;
    if(n<=*capacity)return NX_OK;
    size_t next=*capacity?*capacity:64;
    while(next<n) {if(next>limit/2) {next=limit;break;}next*=2;}
    if(next>limit)next=limit;
    nx_text_document_box *grown=realloc(*items,next*sizeof(*grown));
    if(!grown)return NX_ERROR_MEMORY;
    *items=grown;*capacity=next;return NX_OK;
}
static nx_result append(struct editor_presentation *p,nx_text_document_box box) {
    nx_result result=reserve(&p->boxes,&p->capacity,p->count+1,budget(p));
    if(result!=NX_OK)return failed(p,result);
    p->boxes[p->count++]=box;return NX_OK;
}
static bool valid(nx_rect r) {
    return isfinite(r.x) && isfinite(r.y) && isfinite(r.width) && isfinite(r.height) && r.width>=0 && r.height>=0;
}
static bool clipped(double left,double top,double right,double bottom,nx_rect clip,nx_rect *out) {
    if(!isfinite(left) || !isfinite(top) || !isfinite(right) || !isfinite(bottom) || right<left || bottom<=top)return false;
    bool zero=right==left;
    if(left>=(double)clip.x+clip.width || right<clip.x || bottom<=clip.y || top>=(double)clip.y+clip.height)return false;
    left=fmax(left,clip.x);right=fmin(right,(double)clip.x+clip.width);
    top=fmax(top,clip.y);bottom=fmin(bottom,(double)clip.y+clip.height);
    if(bottom<=top || (!zero && right<=left))return false;
    *out=(nx_rect){(float)left,(float)top,(float)(right-left),(float)(bottom-top)};return true;
}
static int compare(const void *a,const void *b) {
    const nx_text_document_box *x=a,*y=b;
    if(x->start_byte!=y->start_byte)return x->start_byte<y->start_byte?-1:1;
    return x->end_byte<y->end_byte?-1:x->end_byte>y->end_byte?1:0;
}
static nx_rect united(nx_rect a,nx_rect b) {
    /* Zero-advance marks do not enlarge a cluster's logical selection cell. */
    if(!a.width)return b.width?b:a;
    if(!b.width)return a;
    double left=fmin(a.x,b.x),top=fmin(a.y,b.y);
    double right=fmax((double)a.x+a.width,(double)b.x+b.width);
    double bottom=fmax((double)a.y+a.height,(double)b.y+b.height);
    return (nx_rect){(float)left,(float)top,(float)(right-left),(float)(bottom-top)};
}
void editor_presentation_reset(struct editor_presentation *p) {
    p->count=0;p->last_end=0;p->error=NX_OK;p->have_row=false;
}
void editor_presentation_close(struct editor_presentation *p) {
    if(p) {free(p->boxes);free(p->work);*p=(struct editor_presentation){0};}
}
nx_result editor_presentation_row(struct editor_presentation *p,
    const nx_text_layout *layout,size_t base,size_t break_bytes,nx_point origin,nx_rect clip) {
    if(!p)return NX_ERROR_ARGUMENT;
    if(!layout)return failed(p,NX_ERROR_ARGUMENT);
    if(p->error!=NX_OK)return p->error;
    nx_utf8_span text;
    if(!valid(clip) || !isfinite(origin.x) || !isfinite(origin.y) ||
       nx_text_layout_text(layout,&text)!=NX_OK || base>SIZE_MAX-text.length ||
       break_bytes>2 || break_bytes>SIZE_MAX-base-text.length ||
       (p->have_row && base<=p->last_end))return failed(p,NX_ERROR_ARGUMENT);
    p->last_end=base+text.length;p->have_row=true;
    size_t count=0;
    for(size_t i=0;i<nx_text_layout_glyph_count(layout);++i) {
        nx_text_glyph g;nx_text_line line;
        if(nx_text_layout_glyph_at(layout,i,&g)!=NX_OK || nx_text_layout_line_at(layout,g.line_index,&line)!=NX_OK ||
           g.shape.cluster_byte_start>=g.shape.byte_end || g.shape.byte_end>text.length ||
           !nx_text_layout_is_grapheme_boundary(layout,g.shape.cluster_byte_start))return failed(p,NX_ERROR_ARGUMENT);
        double x=(double)origin.x+g.x,end=x+g.shape.advance_x;nx_rect bounds;
        if(!clipped(fmin(x,end),(double)origin.y+line.top,fmax(x,end),(double)origin.y+line.top+line.height,clip,&bounds))continue;
        nx_result result=reserve(&p->work,&p->work_capacity,count+1,budget(p));
        if(result!=NX_OK)return failed(p,result);
        p->work[count++]=(nx_text_document_box){g.shape.cluster_byte_start,g.shape.byte_end,bounds};
    }
    if(count)qsort(p->work,count,sizeof(*p->work),compare);
    for(size_t i=0;i<count;) {
        nx_text_document_box group=p->work[i++];
        while(i<count && p->work[i].start_byte<group.end_byte) {
            if(p->work[i].end_byte>group.end_byte)group.end_byte=p->work[i].end_byte;
            group.bounds=united(group.bounds,p->work[i++].bounds);
        }
        /* Disjoint cluster starts are grapheme boundaries. Scan a combining
           tail once per merged group, rather than once per glyph or scalar. */
        while(group.end_byte<text.length && !nx_text_layout_is_grapheme_boundary(layout,group.end_byte))++group.end_byte;
        group.start_byte+=base;group.end_byte+=base;
        nx_result result=append(p,group);if(result!=NX_OK)return result;
    }
    size_t previous=0;bool have_previous=false;
    for(size_t i=0;i<nx_text_layout_caret_count(layout);++i) {
        nx_text_caret caret;
        if(nx_text_layout_caret_at(layout,i,&caret)!=NX_OK || caret.byte_offset>text.length ||
           !nx_text_layout_is_grapheme_boundary(layout,caret.byte_offset))return failed(p,NX_ERROR_ARGUMENT);
        /* The original renderer selects the first occurrence, even if clipped.
           Verify monotonicity; a future different layout order fails closed. */
        if(have_previous && caret.byte_offset<previous)return failed(p,NX_ERROR_UNSUPPORTED);
        if(have_previous && caret.byte_offset==previous)continue;
        previous=caret.byte_offset;have_previous=true;
        nx_rect bounds;double x=(double)origin.x+caret.x,y=(double)origin.y+caret.top;
        if(clipped(x,y,x+1.5,y+caret.height,clip,&bounds)) {
            nx_result result=append(p,(nx_text_document_box){base+previous,base+previous,bounds});if(result!=NX_OK)return result;
        }
        if(break_bytes && previous==text.length && clipped(x,y,x,y+caret.height,clip,&bounds)) {
            nx_result result=append(p,(nx_text_document_box){base+previous,base+previous+break_bytes,bounds});if(result!=NX_OK)return result;
        }
    }
    return NX_OK;
}
nx_result editor_presentation_caret(struct editor_presentation *p,size_t byte,nx_rect bounds,nx_rect clip) {
    if(!p)return NX_ERROR_ARGUMENT;
    if(p->error!=NX_OK)return p->error;
    if(!valid(bounds) || !valid(clip))return failed(p,NX_ERROR_ARGUMENT);
    nx_rect visible;
    bool present=clipped(bounds.x,bounds.y,(double)bounds.x+bounds.width,(double)bounds.y+bounds.height,clip,&visible);
    for(size_t i=0;i<p->count;++i)if(p->boxes[i].start_byte==byte && p->boxes[i].end_byte==byte) {
        if(present)p->boxes[i].bounds=visible;
        else {--p->count;p->boxes[i]=p->boxes[p->count];}
        return NX_OK;
    }
    return present?append(p,(nx_text_document_box){byte,byte,visible}):NX_OK;
}
nx_result editor_presentation_finish(struct editor_presentation *p,nx_text_document_presentation *out) {
    if(!p || !out)return NX_ERROR_ARGUMENT;
    *out=(nx_text_document_presentation){0};if(p->error!=NX_OK)return p->error;
    if(p->count)qsort(p->boxes,p->count,sizeof(*p->boxes),compare);
    size_t end=0;
    for(size_t i=0;i<p->count;++i) {
        const nx_text_document_box *b=&p->boxes[i];
        if(i && compare(&p->boxes[i-1],b)>=0)return failed(p,NX_ERROR_ARGUMENT);
        if(b->start_byte!=b->end_byte) {if(b->start_byte<end)return failed(p,NX_ERROR_ARGUMENT);end=b->end_byte;}
    }
    *out=(nx_text_document_presentation){.boxes=p->boxes,.box_count=p->count};return NX_OK;
}
