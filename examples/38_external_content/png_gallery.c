#define _POSIX_C_SOURCE 200809L
#include "png_gallery.h"
#ifdef CONTENT_NATIVE_JPEG
#include "jpeg_frame.h"
#endif
#include "png_frame.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

enum { gallery_limit=256, name_limit=127 };
typedef struct gallery_image {
    char name[name_limit+1];
    struct stat stamp;
    int file;
} gallery_image;
struct png_gallery {
    unsigned count;
    gallery_image images[gallery_limit];
};

void png_gallery_close(png_gallery *gallery) {
    if (!gallery) return;
    for (unsigned i=0;i<gallery->count;++i) close(gallery->images[i].file);
    free(gallery);
}

static nx_result read_manifest(png_gallery *gallery,int directory) {
    int file=openat(directory,"gallery.txt",O_RDONLY|O_CLOEXEC|O_NONBLOCK|O_NOFOLLOW);
    if (file<0) return NX_ERROR_PLATFORM;
    struct stat stamp;
    if (fstat(file,&stamp)||!S_ISREG(stamp.st_mode)||stamp.st_size<=0||
        stamp.st_size>(name_limit+2)*gallery_limit) { close(file);return NX_ERROR_ARGUMENT; }
    FILE *stream=fdopen(file,"rb");
    if (!stream) { close(file);return NX_ERROR_PLATFORM; }
    nx_result result=NX_OK;
    char name[name_limit+2];
    size_t length=0;
    for (;;) {
        int c=fgetc(stream);
        if (c==EOF&&!length) break;
        if (c!=EOF&&c!='\n') {
            if (length==sizeof(name)-1u || !c || c=='/' || c=='\\' || c==127 || (c<32&&c!='\r')) {
                result=NX_ERROR_ARGUMENT;break;
            }
            if (length&&name[length-1]=='\r') { result=NX_ERROR_ARGUMENT;break; }
            name[length++]=(char)c;
            continue;
        }
        if (length&&name[length-1]=='\r') --length;
        name[length]=0;
        if (!length||length>name_limit||!strcmp(name,".")||!strcmp(name,"..")||
            nx_utf8_validate(name,length,NULL)!=NX_OK) { result=NX_ERROR_ARGUMENT;break; }
        if (gallery->count==gallery_limit) { result=NX_ERROR_LIMIT;break; }
        for (unsigned i=0;i<gallery->count;++i)
            if (!strcmp(name,gallery->images[i].name)) result=NX_ERROR_ARGUMENT;
        if (result!=NX_OK) break;
        int fd=openat(directory,name,O_RDONLY|O_CLOEXEC|O_NONBLOCK|O_NOFOLLOW);
        if (fd<0) { result=NX_ERROR_PLATFORM;break; }
        gallery_image *image=&gallery->images[gallery->count];
        if (fstat(fd,&image->stamp)||!S_ISREG(image->stamp.st_mode)||image->stamp.st_size<=0||
            image->stamp.st_size>CONTENT_PNG_ENCODED_LIMIT) {
            close(fd);result=NX_ERROR_ARGUMENT;break;
        }
        image->file=fd;
        memcpy(image->name,name,length+1u);
        ++gallery->count;
        length=0;
        if (c==EOF) break;
    }
    if (ferror(stream)) result=NX_ERROR_PLATFORM;
    fclose(stream);
    return result;
}

nx_result png_gallery_open(const char *directory,png_gallery **out) {
    if (!directory||*directory!='/'||!out) return NX_ERROR_ARGUMENT;
    int fd=open(directory,O_RDONLY|O_CLOEXEC|O_DIRECTORY);
    if (fd<0) return NX_ERROR_PLATFORM;
    png_gallery *gallery=calloc(1,sizeof(*gallery));
    nx_result result=gallery?read_manifest(gallery,fd):NX_ERROR_MEMORY;
    close(fd);
    if (result==NX_OK&&!gallery->count) result=NX_ERROR_ARGUMENT;
    if (result==NX_OK) *out=gallery;
    else png_gallery_close(gallery);
    return result;
}

unsigned png_gallery_count(const png_gallery *gallery) { return gallery?gallery->count:0; }

const char *png_gallery_name(const png_gallery *gallery,unsigned image) {
    return image&&image<=png_gallery_count(gallery)?gallery->images[image-1u].name:NULL;
}

static int unchanged(const gallery_image *image) {
    struct stat now;
    const struct stat *was=&image->stamp;
    return !fstat(image->file,&now)&&now.st_size==was->st_size&&
        now.st_mtim.tv_sec==was->st_mtim.tv_sec&&now.st_mtim.tv_nsec==was->st_mtim.tv_nsec&&
        now.st_ctim.tv_sec==was->st_ctim.tv_sec&&now.st_ctim.tv_nsec==was->st_ctim.tv_nsec;
}

nx_result png_gallery_frame(const png_gallery *gallery,unsigned image,content_request request,
    uint64_t sequence,void **out,size_t *bytes,nx_bitmap_view *original) {
    if (!image||image>png_gallery_count(gallery)||!out||!bytes) return NX_ERROR_ARGUMENT;
    const gallery_image *entry=&gallery->images[image-1u];
    if (!unchanged(entry)) return NX_ERROR_STATE;
    size_t size=(size_t)entry->stamp.st_size,offset=0;
    uint8_t *encoded=malloc(size);
    if (!encoded) return NX_ERROR_MEMORY;
    while (offset<size) {
        ssize_t n=pread(entry->file,encoded+offset,size-offset,(off_t)offset);
        if (n<0&&errno==EINTR) continue;
        if (n<=0) break;
        offset+=(size_t)n;
    }
    nx_bitmap_view bitmap={0};
    content_jpeg_size jpeg={0};
    nx_result result=NX_ERROR_STATE;
    if (offset==size&&unchanged(entry)) {
#ifdef CONTENT_NATIVE_JPEG
        if (size>=2 && encoded[0]==0xff && encoded[1]==0xd8)
            result=content_jpeg_bitmap(encoded,size,&bitmap,&jpeg);
        else
#endif
            result=content_png_bitmap(encoded,size,&bitmap);
    }
    free(encoded);
    if (result==NX_OK) result=content_png_fit_bitmap(bitmap,request,sequence,out,bytes,NULL);
    if (result==NX_OK && jpeg.width) ((content_frame_header *)*out)->jpeg=jpeg;
    if (result==NX_OK&&original) *original=bitmap;
    else free((void *)bitmap.pixels);
    return result;
}
