#include "nexaui/nexaui.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr,"utf8 validation %d: %s\n",__LINE__,#x); return 1; } } while (0)

/* The public scalar decoder is unchanged and remains the reference contract. */
static nx_result reference(const char *bytes,size_t length,size_t *error) {
    if (!bytes && length) return NX_ERROR_ARGUMENT;
    size_t offset=0;
    while (offset<length) {
        size_t start=offset;uint32_t scalar;
        if (nx_utf8_decode(bytes,length,&offset,&scalar)!=NX_OK) {
            *error=start;return NX_ERROR_ARGUMENT;
        }
    }
    *error=length;return NX_OK;
}
static int compare(const char *bytes,size_t length) {
    size_t expected=SIZE_MAX,actual=SIZE_MAX;
    nx_result result=reference(bytes,length,&expected);
    CHECK(nx_utf8_validate(bytes,length,&actual)==result && actual==expected);
    CHECK(nx_utf8_validate(bytes,length,NULL)==result);
    return 0;
}
static size_t encode(uint32_t scalar,char *out) {
    if (scalar<=0x7f) {out[0]=(char)scalar;return 1;}
    if (scalar<=0x7ff) {out[0]=(char)(0xc0u|(scalar>>6));out[1]=(char)(0x80u|(scalar&63u));return 2;}
    if (scalar<=0xffff) {
        out[0]=(char)(0xe0u|(scalar>>12));out[1]=(char)(0x80u|((scalar>>6)&63u));out[2]=(char)(0x80u|(scalar&63u));return 3;
    }
    out[0]=(char)(0xf0u|(scalar>>18));out[1]=(char)(0x80u|((scalar>>12)&63u));
    out[2]=(char)(0x80u|((scalar>>6)&63u));out[3]=(char)(0x80u|(scalar&63u));return 4;
}
int main(void) {
    CHECK(!compare(NULL,0) && !compare(NULL,1));
    /* Exact allocation ends plus deliberately unaligned spans expose word
       overreads under ASan; embedded NUL remains a valid ASCII scalar. */
    for (size_t length=0;length<=80;++length) {
        char *allocation=malloc(length+1u);CHECK(allocation);
        char *bytes=allocation+1;memset(bytes,'a',length);
        CHECK(!compare(bytes,length));
        for (unsigned value=0;length && value<256;++value) {
            bytes[length-1]=(char)(unsigned char)value;CHECK(!compare(bytes,length));
        }
        free(allocation);
    }
    char bytes[96];
    for (size_t at=0;at<64;++at) {
        for (unsigned value=0;value<256;++value) {
            memset(bytes,'x',sizeof(bytes));bytes[at]=(char)(unsigned char)value;
            bytes[at+1]=0;CHECK(!compare(bytes,sizeof(bytes)) && !compare(bytes,at+1u));
        }
    }
    size_t scalars=0;
    for (uint32_t scalar=0;scalar<=0x10ffffu;++scalar) {
        if (scalar>=0xd800u && scalar<=0xdfffu) continue;
        size_t prefix=scalar&15u;memset(bytes,'a',sizeof(bytes));
        size_t length=prefix+encode(scalar,bytes+prefix)+8u;bytes[length-1]=0;
        CHECK(!compare(bytes,length));
        if (!(scalar&63u)) {bytes[length]=(char)0xff;CHECK(!compare(bytes,length+1u));}
        ++scalars;
    }
    uint32_t random=UINT32_C(0x9e3779b9);
    for (unsigned trial=0;trial<100000;++trial) {
        size_t length=trial%sizeof(bytes);
        for (size_t i=0;i<length;++i) {random^=random<<13;random^=random>>17;random^=random<<5;bytes[i]=(char)(unsigned char)random;}
        CHECK(!compare(bytes,length));
    }
    CHECK(scalars==1112064u);
    puts("UTF-8 validation matches the scalar decoder for every Unicode scalar, malformed spans, NUL, unaligned ends and random byte strings.");
    return 0;
}
