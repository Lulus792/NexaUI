#define _POSIX_C_SOURCE 200809L
#include "manifest_document.h"
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
struct amh_document {unsigned char artifact[32];unsigned char *bytes;struct ae_fixture data;};
static nx_result verified(unsigned char *bytes,size_t used,const unsigned char expected[32],amh_document **out){
    amh_document *d=calloc(1,sizeof(*d));if(!d){free(bytes);return NX_ERROR_MEMORY;}
    int error=ae_fixture_verify_manifest(bytes,used,expected,&d->data);
    if(error){free(bytes);free(d);return error==-E2BIG?NX_ERROR_LIMIT:error==-ENOMEM?NX_ERROR_MEMORY:error==-EBADMSG||error==-EINVAL?NX_ERROR_ARGUMENT:NX_ERROR_PLATFORM;}
    memcpy(d->artifact,expected,32);d->bytes=bytes;*out=d;return NX_OK;
}
nx_result amh_document_project(const unsigned char *bytes,size_t size,const unsigned char expected[32],amh_document **out){
    if(!bytes||!expected||!out||*out)return NX_ERROR_ARGUMENT;
    if(size>AE_FIXTURE_MAX_BYTES)return NX_ERROR_LIMIT;
    unsigned char *copy=malloc(size?size:1);if(!copy)return NX_ERROR_MEMORY;
    if(size)memcpy(copy,bytes,size);
    return verified(copy,size,expected,out);
}
nx_result amh_document_read(const char *source,const unsigned char expected[32],amh_document **out){
    if(!source||!expected||!out||*out)return NX_ERROR_ARGUMENT;
    int fd=open(source,O_RDONLY|O_CLOEXEC|O_NOFOLLOW|O_NONBLOCK);if(fd<0)return NX_ERROR_PLATFORM;
    struct stat st;nx_result r=NX_ERROR_PLATFORM;unsigned char *bytes=NULL;size_t used=0;
    if(fstat(fd,&st)||!S_ISREG(st.st_mode)||st.st_size<0)goto done;
    if(st.st_size>AE_FIXTURE_MAX_BYTES){r=NX_ERROR_LIMIT;goto done;}
    bytes=malloc(AE_FIXTURE_MAX_BYTES+1u);if(!bytes){r=NX_ERROR_MEMORY;goto done;}
    while(used<AE_FIXTURE_MAX_BYTES+1u){ssize_t n=read(fd,bytes+used,AE_FIXTURE_MAX_BYTES+1u-used);if(n<0){if(errno==EINTR)continue;goto done;}if(!n)break;used+=(size_t)n;}
    r=used>AE_FIXTURE_MAX_BYTES?NX_ERROR_LIMIT:NX_OK;
done:
    if(close(fd)&&r==NX_OK)r=NX_ERROR_PLATFORM;
    if(r!=NX_OK){free(bytes);return r;}
    return verified(bytes,used,expected,out);
}
const struct ae_fixture *amh_document_data(const amh_document *d){return d?&d->data:NULL;}
const unsigned char *amh_document_artifact(const amh_document *d){return d?d->artifact:NULL;}
void amh_document_free(amh_document **p){if(p&&*p){free((*p)->bytes);free(*p);*p=NULL;}}
