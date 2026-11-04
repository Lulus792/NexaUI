#ifndef NEXAUI_LINUX_ATSPI_TEXT_DIFF_H
#define NEXAUI_LINUX_ATSPI_TEXT_DIFF_H
#include "nexaui/nexaui.h"
#include <stdint.h>
#include <string.h>

/* Internal: both spans are validated, NUL-free UTF-8 from owned snapshots;
   counts are Unicode scalar counts. The callback borrows each bounded span. */
#define NX_AX_TEXT_CHUNK_BYTES (64u * 1024u)
typedef int (*nx_ax_text_diff_emit)(void *user, int deleting, int32_t position,
                                    int32_t characters, const char *bytes, size_t length);

static int nx_ax_text_diff_chunks(const char *value,size_t first,size_t last,
                                 int32_t position,int32_t characters,int deleting,
                                 nx_ax_text_diff_emit emit,void *user) {
    while (first<last) {
        size_t begin=first,end=last;
        if (deleting && end-begin>NX_AX_TEXT_CHUNK_BYTES) {
            begin=end-NX_AX_TEXT_CHUNK_BYTES;
            while (begin<end && (((unsigned char)value[begin]&0xc0u)==0x80u)) ++begin;
        } else if (!deleting && end-begin>NX_AX_TEXT_CHUNK_BYTES) {
            end=begin+NX_AX_TEXT_CHUNK_BYTES;
            while (end>begin && (((unsigned char)value[end]&0xc0u)==0x80u)) --end;
        }
        if (begin>=end) return 0;
        size_t at=begin; int32_t count=0; uint32_t scalar;
        while (at<end) {
            if (nx_utf8_decode(value,end,&at,&scalar)!=NX_OK) return 0;
            ++count;
        }
        int32_t where=deleting?position+characters-count:position;
        if (!emit(user,deleting,where,count,value+begin,end-begin)) return 0;
        if (deleting) { last=begin; characters-=count; }
        else { first=end; position+=count; }
    }
    return 1;
}

static int nx_ax_text_diff(const char *old,size_t old_bytes,int32_t old_count,
                           const char *next,size_t next_bytes,int32_t next_count,
                           nx_ax_text_diff_emit emit,void *user) {
    if (old_bytes==next_bytes && !memcmp(old,next,old_bytes)) return 1;
    size_t a=0,b=0; int32_t prefix=0,suffix=0;
    while (a<old_bytes && b<next_bytes) {
        size_t na=a,nb=b; uint32_t ca,cb;
        if (nx_utf8_decode(old,old_bytes,&na,&ca)!=NX_OK ||
            nx_utf8_decode(next,next_bytes,&nb,&cb)!=NX_OK) return 0;
        if (ca!=cb) break;
        a=na; b=nb; ++prefix;
    }
    size_t ae=old_bytes,be=next_bytes;
    while (ae>a && be>b) {
        size_t ap=ae-1u,bp=be-1u;
        while (ap>a && (((unsigned char)old[ap]&0xc0u)==0x80u)) --ap;
        while (bp>b && (((unsigned char)next[bp]&0xc0u)==0x80u)) --bp;
        if (ae-ap!=be-bp || memcmp(old+ap,next+bp,ae-ap)) break;
        ae=ap; be=bp; ++suffix;
    }
    return nx_ax_text_diff_chunks(old,a,ae,prefix,old_count-prefix-suffix,1,emit,user) &&
           nx_ax_text_diff_chunks(next,b,be,prefix,next_count-prefix-suffix,0,emit,user);
}
#endif
