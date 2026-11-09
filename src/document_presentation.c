#include "document_presentation.h"
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static int boundary(nx_utf8_span value,size_t at) {
    return at<=value.length && (at==value.length || ((unsigned char)value.data[at]&0xc0u)!=0x80u);
}
static int rect_valid(nx_rect r) {
    return isfinite(r.x) && isfinite(r.y) && isfinite(r.width) && isfinite(r.height) &&
        r.width>=0 && r.height>0 && isfinite(r.x+r.width) && isfinite(r.y+r.height);
}
static int add(size_t *bytes,size_t extra) {
    if(extra>NX_TEXT_DOCUMENT_PRESENTATION_MAX_BYTES-*bytes)return 0;
    *bytes+=extra;return 1;
}
static int align_bytes(size_t *bytes) {
    size_t alignment=NX_DOCUMENT_PRESENTATION_ALIGNMENT,extra=(alignment-*bytes%alignment)%alignment;
    return add(bytes,extra);
}
static nx_result attributes_size(const nx_text_document_attribute *a,size_t count,size_t *bytes) {
    if(count>NX_TEXT_DOCUMENT_PRESENTATION_MAX_ATTRIBUTES)return NX_ERROR_LIMIT;
    if(count && !a)return NX_ERROR_ARGUMENT;
    if(!align_bytes(bytes) || !add(bytes,count*sizeof(nx_document_attribute)))return NX_ERROR_LIMIT;
    for(size_t i=0;i<count;++i) {
        nx_utf8_span name=a[i].name,value=a[i].value;
        if(name.length>128 || value.length>4096)return NX_ERROR_LIMIT;
        if(!name.data || !name.length || (!value.data && value.length) ||
           memchr(name.data,0,name.length) || (value.length && memchr(value.data,0,value.length)) ||
           nx_utf8_validate(name.data,name.length,NULL)!=NX_OK ||
           (value.length && nx_utf8_validate(value.data,value.length,NULL)!=NX_OK))return NX_ERROR_ARGUMENT;
        for(size_t j=0;j<i;++j)if(a[j].name.length==name.length && !memcmp(a[j].name.data,name.data,name.length))return NX_ERROR_ARGUMENT;
        if(!add(bytes,name.length+1) || !add(bytes,value.length+1))return NX_ERROR_LIMIT;
    }
    return NX_OK;
}
static nx_document_attributes copy_attributes(char *blob,size_t *at,
    const nx_text_document_attribute *a,size_t count) {
    (void)align_bytes(at);nx_document_attributes result={count,*at};
    nx_document_attribute *items=(nx_document_attribute *)(void *)(blob+*at);*at+=count*sizeof(*items);
    for(size_t i=0;i<count;++i) {
        items[i].name=*at;memcpy(blob+*at,a[i].name.data,a[i].name.length);*at+=a[i].name.length;blob[(*at)++]=0;
        items[i].value=*at;if(a[i].value.length)memcpy(blob+*at,a[i].value.data,a[i].value.length);*at+=a[i].value.length;blob[(*at)++]=0;
    }
    return result;
}
nx_result nx_document_presentation_build(nx_utf8_span value,
    const nx_text_document_presentation *d,nx_document_presentation **out) {
    if(!out)return NX_ERROR_ARGUMENT;
    *out=NULL;
    if(!d || (!value.data && value.length))return NX_ERROR_ARGUMENT;
    if(value.length>NX_TEXT_DOCUMENT_MAX_BYTES || d->box_count>NX_TEXT_DOCUMENT_PRESENTATION_MAX_BOXES ||
       d->run_count>NX_TEXT_DOCUMENT_PRESENTATION_MAX_RUNS)return NX_ERROR_LIMIT;
    if((d->box_count && !d->boxes) || (d->run_count && !d->runs))return NX_ERROR_ARGUMENT;
    size_t bytes=sizeof(nx_document_presentation);(void)align_bytes(&bytes);
    size_t box_offset=bytes;if(!add(&bytes,d->box_count*sizeof(nx_text_document_box)) || !align_bytes(&bytes))return NX_ERROR_LIMIT;
    size_t run_offset=bytes;if(!add(&bytes,d->run_count*sizeof(nx_document_attribute_run)))return NX_ERROR_LIMIT;
    size_t end=0;
    for(size_t i=0;i<d->box_count;++i) {
        const nx_text_document_box *b=&d->boxes[i];
        if(b->start_byte>b->end_byte || b->start_byte<end || !boundary(value,b->start_byte) ||
           !boundary(value,b->end_byte) || !rect_valid(b->bounds))return NX_ERROR_ARGUMENT;
        if(i && b->start_byte==d->boxes[i-1].start_byte && b->end_byte<=d->boxes[i-1].end_byte)return NX_ERROR_ARGUMENT;
        end=b->end_byte;
    }
    nx_result result=attributes_size(d->defaults,d->default_count,&bytes);if(result!=NX_OK)return result;
    end=0;
    for(size_t i=0;i<d->run_count;++i) {
        const nx_text_document_attribute_run *r=&d->runs[i];
        if(r->start_byte>=r->end_byte || r->start_byte<end || !boundary(value,r->start_byte) || !boundary(value,r->end_byte))return NX_ERROR_ARGUMENT;
        end=r->end_byte;result=attributes_size(r->attributes,r->attribute_count,&bytes);if(result!=NX_OK)return result;
    }
    nx_document_presentation *p=calloc(1,bytes);if(!p)return NX_ERROR_MEMORY;
    *p=(nx_document_presentation){bytes,value.length,d->box_count,box_offset,d->run_count,run_offset,{0}};
    char *blob=(char *)p;if(d->box_count)memcpy(blob+box_offset,d->boxes,d->box_count*sizeof(*d->boxes));
    nx_document_attribute_run *runs=(nx_document_attribute_run *)(void *)(blob+run_offset);
    size_t at=run_offset+d->run_count*sizeof(*runs);p->defaults=copy_attributes(blob,&at,d->defaults,d->default_count);
    for(size_t i=0;i<d->run_count;++i) {
        runs[i].start=d->runs[i].start_byte;runs[i].end=d->runs[i].end_byte;
        runs[i].attributes=copy_attributes(blob,&at,d->runs[i].attributes,d->runs[i].attribute_count);
    }
    if(at!=bytes){free(p);return NX_ERROR_STATE;}
    *out=p;return NX_OK;
}
static const nx_text_document_box *boxes(const nx_document_presentation *p) {
    return (const nx_text_document_box *)(const void *)((const char *)p+p->box_offset);
}
nx_result nx_document_presentation_character(const nx_document_presentation *p,size_t byte,nx_rect *out) {
    if(!out)return NX_ERROR_ARGUMENT;
    *out=(nx_rect){0};if(!p)return NX_ERROR_UNSUPPORTED;
    if(byte>=p->text_bytes)return NX_ERROR_ARGUMENT;
    const nx_text_document_box *b=boxes(p);size_t lo=0,hi=p->box_count;
    while(lo<hi){size_t mid=lo+(hi-lo)/2;if(b[mid].end_byte<=byte)lo=mid+1;else hi=mid;}
    if(lo==p->box_count || b[lo].start_byte>byte)return NX_ERROR_UNSUPPORTED;
    *out=b[lo].bounds;return NX_OK;
}
nx_result nx_document_presentation_range(const nx_document_presentation *p,size_t start,size_t end,nx_rect *out) {
    if(!out)return NX_ERROR_ARGUMENT;
    *out=(nx_rect){0};if(!p)return NX_ERROR_UNSUPPORTED;
    if(start>end || end>p->text_bytes)return NX_ERROR_ARGUMENT;
    const nx_text_document_box *b=boxes(p);int found=0;size_t covered=start;double x=0,y=0,right=0,bottom=0;
    for(size_t i=0;i<p->box_count;++i) {
        if(start==end) {
            if(b[i].start_byte==start && b[i].end_byte==start){*out=b[i].bounds;return NX_OK;}
            continue;
        }
        if(b[i].start_byte==b[i].end_byte || b[i].end_byte<=start)continue;
        if(b[i].start_byte>=end)break;
        if(b[i].start_byte>covered)return NX_ERROR_UNSUPPORTED;
        nx_rect r=b[i].bounds;double rright=(double)r.x+r.width,rbottom=(double)r.y+r.height;
        if(!found){x=r.x;y=r.y;right=rright;bottom=rbottom;found=1;}
        else {x=fmin(x,r.x);y=fmin(y,r.y);right=fmax(right,rright);bottom=fmax(bottom,rbottom);}
        covered=b[i].end_byte;
    }
    if(!found || covered<end)return NX_ERROR_UNSUPPORTED;
    nx_rect r={(float)x,(float)y,(float)(right-x),(float)(bottom-y)};if(!rect_valid(r))return NX_ERROR_LIMIT;
    *out=r;return NX_OK;
}
nx_result nx_document_presentation_point(const nx_document_presentation *p,nx_point point,size_t *out) {
    if(!out || !isfinite(point.x) || !isfinite(point.y))return NX_ERROR_ARGUMENT;
    *out=SIZE_MAX;if(!p)return NX_ERROR_UNSUPPORTED;
    const nx_text_document_box *b=boxes(p);
    for(size_t i=0;i<p->box_count;++i) {
        nx_rect r=b[i].bounds;
        if(b[i].start_byte!=b[i].end_byte && point.x>=r.x && (double)point.x<(double)r.x+r.width &&
           point.y>=r.y && (double)point.y<(double)r.y+r.height){*out=b[i].start_byte;return NX_OK;}
    }
    return NX_OK;
}
nx_document_attributes nx_document_presentation_attributes(const nx_document_presentation *p,
    size_t byte,size_t *start,size_t *end) {
    *start=0;*end=p->text_bytes;
    const nx_document_attribute_run *runs=(const nx_document_attribute_run *)(const void *)((const char *)p+p->run_offset);
    for(size_t i=0;i<p->run_count;++i) {
        if(byte<runs[i].start){*end=runs[i].start;break;}
        if(byte<runs[i].end){*start=runs[i].start;*end=runs[i].end;return runs[i].attributes;}
        *start=runs[i].end;
    }
    return (nx_document_attributes){0};
}
const char *nx_document_attribute_name(const nx_document_presentation *p,nx_document_attributes a,size_t index) {
    const nx_document_attribute *items=(const nx_document_attribute *)(const void *)((const char *)p+a.offset);
    return (const char *)p+items[index].name;
}
const char *nx_document_attribute_value(const nx_document_presentation *p,nx_document_attributes a,size_t index) {
    const nx_document_attribute *items=(const nx_document_attribute *)(const void *)((const char *)p+a.offset);
    return (const char *)p+items[index].value;
}
