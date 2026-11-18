#include "jpeg_frame.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <turbojpeg.h>

#define CHECK(expr) do { if (!(expr)) { fprintf(stderr,"line %d: %s\n",__LINE__,#expr); goto done; } } while (0)

static unsigned char *make_jpeg(int width,int height,int progressive,size_t *size) {
    static const unsigned char colors[4][3]={{230,30,40},{35,210,55},{25,60,220},{225,205,35}};
    unsigned char *pixels=malloc((size_t)width*height*3u),*encoded=NULL;
    tjhandle compressor=tj3Init(TJINIT_COMPRESS);
    if (pixels && compressor) {
        for (int y=0;y<height;++y) {
            for (int x=0;x<width;++x) {
                unsigned quadrant=2u*(y>=height/2)+(x>=width/2);
                memcpy(pixels+((size_t)y*width+x)*3u,colors[quadrant],3);
            }
        }
        if (tj3Set(compressor,TJPARAM_SUBSAMP,TJSAMP_444) || tj3Set(compressor,TJPARAM_QUALITY,95) ||
            tj3Set(compressor,TJPARAM_PROGRESSIVE,progressive) ||
            tj3Compress8(compressor,pixels,width,0,height,TJPF_RGB,&encoded,size)) {
            tj3Free(encoded);
            encoded=NULL;
        }
    }
    if (compressor) tj3Destroy(compressor);
    free(pixels);
    return encoded;
}

static unsigned char *with_orientation(const unsigned char *jpeg,size_t size,unsigned tag,int big,size_t *total) {
    unsigned char little[]={0xff,0xe1,0,34,'E','x','i','f',0,0,
        'I','I',42,0,8,0,0,0,1,0,0x12,1,3,0,1,0,0,0,0,0,0,0,0,0,0,0};
    unsigned char big_endian[]={0xff,0xe1,0,34,'E','x','i','f',0,0,
        'M','M',0,42,0,0,0,8,0,1,1,0x12,0,3,0,0,0,1,0,0,0,0,0,0,0,0};
    little[28]=(unsigned char)tag;
    big_endian[29]=(unsigned char)tag;
    *total=size+sizeof(little);
    unsigned char *data=malloc(*total);
    if (!data) return NULL;
    memcpy(data,jpeg,2);
    memcpy(data+2,big?big_endian:little,sizeof(little));
    memcpy(data+2+sizeof(little),jpeg+2,size-2);
    return data;
}

static int same_pixels(nx_bitmap_view base,nx_bitmap_view actual,unsigned tag) {
    uint32_t w=base.width,h=base.height;
    for (uint32_t y=0;y<h;++y) {
        for (uint32_t x=0;x<w;++x) {
            uint32_t dx=x,dy=y;
            switch (tag) {
                case 2: dx=w-1-x; break;
                case 3: dx=w-1-x; dy=h-1-y; break;
                case 4: dy=h-1-y; break;
                case 5: dx=y; dy=x; break;
                case 6: dx=h-1-y; dy=x; break;
                case 7: dx=h-1-y; dy=w-1-x; break;
                case 8: dx=y; dy=w-1-x; break;
            }
            const uint8_t *src=(const uint8_t *)base.pixels+y*base.stride_bytes+x*4u;
            const uint8_t *dst=(const uint8_t *)actual.pixels+dy*actual.stride_bytes+dx*4u;
            if (src[3]!=255 || memcmp(src,dst,4)!=0) return 0;
        }
    }
    return 1;
}

static int check_orientations(void) {
    int ok=0;
    size_t size=0,tagged_size=0;
    unsigned char *jpeg=make_jpeg(37,21,0,&size),*tagged=NULL;
    nx_bitmap_view base={0},actual={0};
    content_jpeg_size original={0};
    CHECK(jpeg && content_jpeg_bitmap(jpeg,size,&base,&original)==NX_OK);
    CHECK(base.width==37 && base.height==21 && original.width==37 && original.height==21);
    for (unsigned tag=1;tag<=9;++tag) {
        for (int big=0;big<=1;++big) {
            tagged=with_orientation(jpeg,size,tag,big,&tagged_size);
            CHECK(tagged && content_jpeg_bitmap(tagged,tagged_size,&actual,&original)==NX_OK);
            int swapped=tag>=5 && tag<=8;
            CHECK(actual.width==(swapped?21u:37u) && actual.height==(swapped?37u:21u) &&
                original.width==actual.width && original.height==actual.height);
            CHECK(same_pixels(base,actual,tag));
            free((void *)actual.pixels);
            actual=(nx_bitmap_view){0};
            free(tagged);
            tagged=NULL;
        }
    }
    CHECK(content_jpeg_bitmap(jpeg,size-2,&actual,&original)!=NX_OK &&
        !actual.pixels && original.width==37 && original.height==21);
    ok=1;
done:
    free((void *)base.pixels);
    free((void *)actual.pixels);
    free(tagged);
    tj3Free(jpeg);
    return ok;
}

static int check_large(const char *fixture) {
    int ok=0;
    size_t size=0,tagged_size=0;
    unsigned char *jpeg=make_jpeg(4096,2048,1,&size),*tagged=NULL;
    nx_bitmap_view actual={0};
    content_jpeg_size original={0};
    CHECK(jpeg && (tagged=with_orientation(jpeg,size,6,0,&tagged_size)));
    CHECK(content_jpeg_bitmap(tagged,tagged_size,&actual,&original)==NX_OK);
    CHECK(actual.width==512 && actual.height==1024 && actual.stride_bytes==2048 &&
        original.width==2048 && original.height==4096);
    if (fixture) {
        FILE *stream=fopen(fixture,"wb");
        CHECK(stream);
        size_t written=fwrite(tagged,1,tagged_size,stream);
        CHECK(fclose(stream)==0 && written==tagged_size);
    }
    ok=1;
done:
    free((void *)actual.pixels);
    free(tagged);
    tj3Free(jpeg);
    return ok;
}

int main(int argc,char **argv) {
    if (argc==2) return check_large(argv[1])?0:1;
    return argc==1 && check_orientations() && check_large(NULL)?0:1;
}
