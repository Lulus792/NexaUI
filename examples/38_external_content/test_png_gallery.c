#define _POSIX_C_SOURCE 200809L
#include "png_gallery.h"

#include <png.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define CHECK(value) do { if (!(value)) { fprintf(stderr,"line %d: %s\n",__LINE__,#value);goto done; } } while (0)

static int write_text(const char *path,const char *text) {
    FILE *file=fopen(path,"wb");
    if (!file) return 0;
    int ok=fputs(text,file)!=EOF;
    return fclose(file)==0&&ok;
}

int main(void) {
    char directory[]="/tmp/nexaui-gallery-XXXXXX";
    if (!mkdtemp(directory)) return 1;
    char manifest[96],blue[96],red[96],alias[96];
    snprintf(manifest,sizeof(manifest),"%s/gallery.txt",directory);
    snprintf(blue,sizeof(blue),"%s/Küste.png",directory);
    snprintf(red,sizeof(red),"%s/Abend.png",directory);
    snprintf(alias,sizeof(alias),"%s/alias.png",directory);
    png_gallery *gallery=NULL,*rejected=NULL;
    void *frame=NULL;
    size_t bytes=0;
    nx_bitmap_view original={0};
    int passed=0;
    png_image png={.version=PNG_IMAGE_VERSION,.width=1,.height=1,.format=PNG_FORMAT_RGBA};
    uint8_t a[]={12,25,245,255},b[]={220,43,18,255};
    CHECK(png_image_write_to_file(&png,blue,0,a,0,NULL)&&png_image_write_to_file(&png,red,0,b,0,NULL));
    CHECK(write_text(manifest,"Küste.png\r\nAbend.png")&&png_gallery_open(directory,&gallery)==NX_OK);
    CHECK(png_gallery_count(gallery)==2&&!strcmp(png_gallery_name(gallery,1),"Küste.png")&&
        !strcmp(png_gallery_name(gallery,2),"Abend.png")&&!png_gallery_name(gallery,3));
    content_request request={1,1,2,2};
    CHECK(png_gallery_frame(gallery,1,request,1,&frame,&bytes,&original)==NX_OK&&
        bytes==sizeof(content_frame_header)+16&&original.width==1&&!memcmp(original.pixels,a,sizeof(a)));
    free(frame);frame=NULL;
    free((void *)original.pixels);original=(nx_bitmap_view){0};
    bytes=73;
    CHECK(write_text(blue,"Changed after admission")&&
        png_gallery_frame(gallery,1,request,2,&frame,&bytes,&original)==NX_ERROR_STATE&&!frame&&bytes==73);
    CHECK(write_text(manifest,"../outside.png\n")&&png_gallery_open(directory,&rejected)==NX_ERROR_ARGUMENT&&!rejected);
    CHECK(write_text(manifest,"Abend.png\nAbend.png\n")&&png_gallery_open(directory,&rejected)==NX_ERROR_ARGUMENT&&!rejected);
    CHECK(symlink("Abend.png",alias)==0&&write_text(manifest,"alias.png")&&
        png_gallery_open(directory,&rejected)==NX_ERROR_PLATFORM&&!rejected);
    CHECK(png_gallery_frame(gallery,2,request,3,&frame,&bytes,&original)==NX_OK&&
        !memcmp(original.pixels,b,sizeof(b)));
    passed=1;
done:
    png_image_free(&png);
    free(frame);
    free((void *)original.pixels);
    png_gallery_close(rejected);
    png_gallery_close(gallery);
    unlink(alias);unlink(blue);unlink(red);unlink(manifest);
    if (rmdir(directory)) passed=0;
    if (passed) puts("PNG gallery verifies name order, pinned decode, changed file, traversal, duplicates and symlinks");
    return passed?0:1;
}
