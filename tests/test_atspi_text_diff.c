#include "atspi_text_diff.h"
#include <stdio.h>
#include <stdlib.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr,"%s:%d: %s\n",__FILE__,__LINE__,#x); return 1; } } while (0)

typedef struct model {
    char *bytes;
    size_t length, capacity;
    unsigned inserts, deletes, calls;
    int fail;
} model;
static int byte_offset(const char *s,size_t length,int32_t character,size_t *out) {
    size_t at=0; uint32_t scalar;
    if (character<0) return 0;
    for (int32_t i=0;i<character;++i)
        if (nx_utf8_decode(s,length,&at,&scalar)!=NX_OK) return 0;
    *out=at; return 1;
}
static int emit(void *user,int deleting,int32_t position,int32_t characters,
                const char *bytes,size_t length) {
    model *m=user; ++m->calls;
    if (m->fail && m->calls==(unsigned)m->fail) return 0;
    if (!length || length>NX_AX_TEXT_CHUNK_BYTES || characters<=0 ||
        nx_utf8_validate(bytes,length,NULL)!=NX_OK) return 0;
    size_t count=0,at=0; uint32_t scalar;
    while (at<length) {
        if (nx_utf8_decode(bytes,length,&at,&scalar)!=NX_OK) return 0;
        ++count;
    }
    if (count!=(size_t)characters) return 0;
    size_t start,end;
    if (!byte_offset(m->bytes,m->length,position,&start)) return 0;
    if (deleting) {
        if (!byte_offset(m->bytes,m->length,position+characters,&end) ||
            end-start!=length || memcmp(m->bytes+start,bytes,length)) return 0;
        memmove(m->bytes+start,m->bytes+end,m->length-end);
        m->length-=length; ++m->deletes;
    } else {
        if (length>m->capacity || m->length>m->capacity-length) return 0;
        memmove(m->bytes+start+length,m->bytes+start,m->length-start);
        memcpy(m->bytes+start,bytes,length);
        m->length+=length; ++m->inserts;
    }
    return 1;
}
static int boundary(size_t left,size_t right) {
    const char globe[]="\xf0\x9f\x8c\x8d";
    size_t length=left+right+6u;
    char *large=malloc(length+1u),*storage=malloc(length+16u);
    CHECK(large && storage);
    large[0]='X'; memset(large+1,'a',left);
    memcpy(large+1+left,globe,4); memset(large+1+left+4,'b',right);
    large[length-1]='Y'; large[length]=0;
    model m={storage,2,length+16u,0,0,0,0}; memcpy(m.bytes,"XY",2);
    int32_t count=(int32_t)(left+right+3u);
    CHECK(nx_ax_text_diff("XY",2,2,large,length,count,emit,&m));
    CHECK(m.length==length && !memcmp(m.bytes,large,length) && m.inserts>=2 && !m.deletes);
    unsigned inserts=m.inserts;
    CHECK(nx_ax_text_diff(large,length,count,large,length,count,emit,&m));
    CHECK(m.inserts==inserts && !m.deletes);
    CHECK(nx_ax_text_diff(large,length,count,"XY",2,2,emit,&m));
    CHECK(m.length==2 && !memcmp(m.bytes,"XY",2) && m.deletes>=2);
    m.fail=(int)m.calls+1; unsigned calls=m.calls;
    CHECK(!nx_ax_text_diff("XY",2,2,large,length,count,emit,&m) && m.calls==calls+1 && m.length==2);
    m.fail=(int)m.calls+2; calls=m.calls; inserts=m.inserts;
    CHECK(!nx_ax_text_diff("XY",2,2,large,length,count,emit,&m) && m.calls==calls+2 &&
          m.inserts==inserts+1 && m.length>2 && m.length<length);
    free(storage); free(large); return 0;
}
int main(void) {
    CHECK(boundary(65534u,65540u)==0);
    CHECK(boundary(65535u,65534u)==0);
    CHECK(boundary(65536u,65535u)==0);
    CHECK(boundary(65537u,65536u)==0);
    puts("AT-SPI Unicode chunk event replay passed."); return 0;
}
