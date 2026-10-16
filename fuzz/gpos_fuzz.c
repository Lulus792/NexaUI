#include "nexaui/nexaui.h"
#include "font_internal.h"
#include "ot_layout.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static void put16(uint8_t *p,uint16_t x) { p[0]=(uint8_t)(x>>8); p[1]=(uint8_t)x; }
static void put32(uint8_t *p,uint32_t x) { p[0]=(uint8_t)(x>>24); p[1]=(uint8_t)(x>>16); p[2]=(uint8_t)(x>>8); p[3]=(uint8_t)x; }
static void record(uint8_t *p,uint32_t tag,size_t at,size_t size) {
    put32(p,tag); put32(p+8,(uint32_t)at); put32(p+12,(uint32_t)size);
}
/* Legacy inputs are raw GPOS tables. NXP2 inputs combine GPOS, GDEF and GSUB
   behind three big-endian lengths, so mutations can reach ligature provenance,
   lookup filtering and attachment resolution in the same invocation. */
int LLVMFuzzerTestOneInput(const uint8_t *data,size_t size) {
    if (size>(1u<<20)) return 0;
    const uint8_t *gpos=data,*gdef=NULL,*gsub=NULL;
    size_t plen=size,dlen=0,slen=0;
    if (size>=16 && memcmp(data,"NXP2",4)==0) {
        plen=nx_ot_u32(data+4); dlen=nx_ot_u32(data+8); slen=nx_ot_u32(data+12);
        if (plen>size-16 || dlen>size-16-plen || slen!=size-16-plen-dlen) return 0;
        gpos=data+16; gdef=gpos+plen; gsub=gdef+dlen;
    }
    size_t length=352+plen+dlen+slen;
    uint8_t *bytes=calloc(length,1); if (!bytes) return 0;
    put32(bytes,0x10000); put16(bytes+4,(uint16_t)(5+(dlen!=0)+(slen!=0)));
    record(bytes+12,NX_SFNT_TAG('h','e','a','d'),128,54);
    record(bytes+28,NX_SFNT_TAG('m','a','x','p'),192,32);
    record(bytes+44,NX_SFNT_TAG('h','h','e','a'),240,36);
    record(bytes+60,NX_SFNT_TAG('h','m','t','x'),288,64);
    record(bytes+76,NX_SFNT_TAG('G','P','O','S'),352,plen);
    size_t entry=92;
    if (dlen) { record(bytes+entry,NX_SFNT_TAG('G','D','E','F'),352+plen,dlen); entry+=16; }
    if (slen) record(bytes+entry,NX_SFNT_TAG('G','S','U','B'),352+plen+dlen,slen);
    put32(bytes+128,0x10000); put32(bytes+140,0x5f0f3cf5); put16(bytes+146,1000);
    put32(bytes+192,0x10000); put16(bytes+196,16);
    put32(bytes+240,0x10000); put16(bytes+244,900); put16(bytes+246,(uint16_t)-200); put16(bytes+274,16);
    for (unsigned i=0;i<16;++i) put16(bytes+288+4*i,600);
    put16(bytes+288+4*6,900); put16(bytes+288+4*13,1200);
    if (plen) memcpy(bytes+352,gpos,plen);
    if (dlen) memcpy(bytes+352+plen,gdef,dlen);
    if (slen) memcpy(bytes+352+plen+dlen,gsub,slen);
    nx_font *font=NULL;
    if (nx_font_open(bytes,length,NULL,&font)==NX_OK) {
        const uint16_t sequences[3][6]={{2,7,8,3,0,0},{4,7,5,8,0,0},{4,4,7,5,8,0}};
        const size_t counts[3]={4,4,5};
        for (unsigned mode=0;mode<4;++mode) {
            nx_shaped_glyph glyphs[32]={0}; nx_ot_glyph_state state[32]; uint8_t pairs[32]={0};
            size_t count=mode<3?counts[mode]:(size<32?size:32);
            for (size_t i=0;i<count;++i) {
                uint16_t id=mode<3?sequences[mode][i]:(uint16_t)(data[i]&15u);
                glyphs[i].glyph_index=id; glyphs[i].scalar_index=i;
                glyphs[i].scalar=id==7?0x301u:id==8?0x302u:id==10?0x307u:'A';
                glyphs[i].bidi_level=(uint8_t)(mode&1u);
                nx_ot_state_init(&glyphs[i],&state[i]);
            }
            if (nx_gsub_apply_state(font,NX_SFNT_TAG('L','a','t','n'),glyphs,state,&count)!=NX_OK) continue;
            for (size_t i=0;i<count;++i) {
                uint16_t advance=0; int16_t lsb=0;
                if (nx_font_glyph_hmetrics(font,glyphs[i].glyph_index,&advance,&lsb)==NX_OK)
                    glyphs[i].advance_x=state[i].nonspacing && !state[i].ligated?0:advance*.02;
            }
            if (nx_gpos_apply_state(font,NX_SFNT_TAG('L','a','t','n'),glyphs,state,count,
                                   mode==3?20.5:20,pairs)==NX_OK)
                (void)nx_ot_resolve(glyphs,state,count);
        }
        nx_font_close(font);
    }
    free(bytes); return 0;
}
