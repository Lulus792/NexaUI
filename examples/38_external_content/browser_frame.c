#define _POSIX_C_SOURCE 200809L
#include "browser_frame.h"
#include "png_frame.h"
#include <errno.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <time.h>
#include <unistd.h>

static int64_t monotonic_ms(void) {
    struct timespec now;
    if (clock_gettime(CLOCK_MONOTONIC,&now)) return -1;
    return (int64_t)now.tv_sec*1000+now.tv_nsec/1000000;
}

static int ready(int fd,short events,int64_t deadline) {
    for (;;) {
        int64_t now=monotonic_ms();
        if (now<0 || now>=deadline) return 0;
        struct pollfd wait={.fd=fd,.events=events};
        int result=poll(&wait,1,(int)(deadline-now));
        if (result<0 && errno==EINTR) continue;
        return result>0 && (wait.revents&(events|POLLHUP));
    }
}

static int send_request(int fd,const char *line,size_t size,int64_t deadline) {
    size_t sent=0;
    while (sent<size && ready(fd,POLLOUT,deadline)) {
        ssize_t count=send(fd,line+sent,size-sent,MSG_NOSIGNAL);
        if (count<0 && (errno==EINTR||errno==EAGAIN)) continue;
        if (count<=0) break;
        sent+=(size_t)count;
    }
    return sent==size && shutdown(fd,SHUT_WR)==0;
}

static nx_result read_frame(int fd,uint8_t *encoded,size_t *size,int64_t deadline) {
    size_t capacity=CONTENT_PNG_ENCODED_LIMIT+1u;
    while (ready(fd,POLLIN,deadline)) {
        ssize_t count=recv(fd,encoded+*size,capacity-*size,0);
        if (count<0 && (errno==EINTR||errno==EAGAIN)) continue;
        if (count<0) break;
        if (!count) return *size?NX_OK:NX_ERROR_ARGUMENT;
        *size+=(size_t)count;
        if (*size==capacity) return NX_ERROR_LIMIT;
    }
    return NX_ERROR_PLATFORM;
}

nx_result content_browser_frame(const char *socket_path,content_request request,
    uint64_t sequence,unsigned kind,void **out,size_t *bytes) {
    struct sockaddr_un address={.sun_family=AF_UNIX};
    if (!socket_path || socket_path[0]!='/' || strlen(socket_path)>=sizeof(address.sun_path) ||
        !request.generation || !request.resize || !request.width || !request.height || !sequence ||
        !out || !bytes || kind>4) return NX_ERROR_ARGUMENT;
    if (request.width>CONTENT_PNG_SOURCE_DIMENSION || request.height>CONTENT_PNG_SOURCE_DIMENSION ||
        request.width*4u>CONTENT_PNG_PIXEL_LIMIT/request.height) return NX_ERROR_LIMIT;
    strcpy(address.sun_path,socket_path);
    char line[96];
    int length=snprintf(line,sizeof(line),"%llu %llu %u %u %u\n",
        (unsigned long long)request.generation,(unsigned long long)request.resize,request.width,request.height,kind);
    if (length<=0 || (size_t)length>=sizeof(line)) return NX_ERROR_LIMIT;
    int64_t start=monotonic_ms();
    int fd=socket(AF_UNIX,SOCK_STREAM|SOCK_CLOEXEC|SOCK_NONBLOCK,0);
    if (fd<0) return NX_ERROR_PLATFORM;
    uint8_t *encoded=NULL;
    nx_bitmap_view bitmap={0};
    nx_result result=NX_ERROR_PLATFORM;
    if (start<0) goto done;
    int64_t deadline=start+5000;
    if (connect(fd,(struct sockaddr *)&address,sizeof(address))) {
        if (errno!=EINPROGRESS || !ready(fd,POLLOUT,deadline)) goto done;
        int error=0;
        socklen_t size=sizeof(error);
        if (getsockopt(fd,SOL_SOCKET,SO_ERROR,&error,&size) || error) goto done;
    }
    if (!send_request(fd,line,(size_t)length,deadline)) goto done;
    encoded=malloc(CONTENT_PNG_ENCODED_LIMIT+1u);
    if (!encoded) { result=NX_ERROR_MEMORY;goto done; }
    size_t size=0;
    result=read_frame(fd,encoded,&size,deadline);
    if (result==NX_OK) result=content_png_bitmap(encoded,size,&bitmap);
    if (result==NX_OK) {
        if (bitmap.width!=request.width || bitmap.height!=request.height) result=NX_ERROR_ARGUMENT;
        else result=content_png_fit_bitmap(bitmap,request,sequence,out,bytes,NULL);
    }
done:
    free((void *)bitmap.pixels);
    free(encoded);
    close(fd);
    return result;
}
