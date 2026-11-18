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

static nx_result read_frame(int fd,uint8_t *encoded,size_t capacity,size_t *size,int64_t deadline) {
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

static nx_result transfer(const char *socket_path,content_request request,uint64_t sequence,unsigned kind,
    const content_browser_edit *edit,void **out,size_t *bytes,nx_rect *caret) {
    struct sockaddr_un address={.sun_family=AF_UNIX};
    if (!socket_path || socket_path[0]!='/' || strlen(socket_path)>=sizeof(address.sun_path) ||
        !request.generation || !request.resize || !request.width || !request.height || !sequence ||
        !out || !bytes) return NX_ERROR_ARGUMENT;
    if (request.width>CONTENT_PNG_SOURCE_DIMENSION || request.height>CONTENT_PNG_SOURCE_DIMENSION ||
        request.width*4u>CONTENT_PNG_PIXEL_LIMIT/request.height) return NX_ERROR_LIMIT;
    strcpy(address.sun_path,socket_path);
    char line[128+CONTENT_BROWSER_TEXT_LIMIT];
    int length=snprintf(line,sizeof(line),"%llu %llu %u %u %u",
        (unsigned long long)request.generation,(unsigned long long)request.resize,request.width,request.height,kind);
    if (length<=0 || (size_t)length>=sizeof(line)) return NX_ERROR_LIMIT;
    size_t send_size=(size_t)length;
    if (edit) {
        const content_browser_pointer *pointer=&edit->pointer;
        size_t body=edit->length;
        char pointer_payload[80];
        const char *payload=edit->text;
        if (kind>=CONTENT_BROWSER_MOUSE_MOVE && kind<=CONTENT_BROWSER_MOUSE_WHEEL) {
            int n=snprintf(pointer_payload,sizeof(pointer_payload),"%d %d %d %d %u %u",
                pointer->x,pointer->y,pointer->delta_x,pointer->delta_y,pointer->button,pointer->modifiers);
            if (n<=0 || (size_t)n>=sizeof(pointer_payload)) return NX_ERROR_LIMIT;
            body=(size_t)n;
            payload=pointer_payload;
        }
        length=snprintf(line+send_size,sizeof(line)-send_size," %d %d %zu\n",edit->begin,edit->end,body);
        if (length<=0 || (size_t)length>=sizeof(line)-send_size || body>sizeof(line)-send_size-(size_t)length)
            return NX_ERROR_LIMIT;
        send_size+=(size_t)length;
        memcpy(line+send_size,payload,body);
        send_size+=body;
    } else line[send_size++]='\n';
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
    if (!send_request(fd,line,send_size,deadline)) goto done;
    size_t prefix=edit?8u:0,capacity=prefix+CONTENT_PNG_ENCODED_LIMIT+1u;
    encoded=malloc(capacity);
    if (!encoded) { result=NX_ERROR_MEMORY;goto done; }
    size_t size=0;
    result=read_frame(fd,encoded,capacity,&size,deadline);
    nx_rect candidate={0};
    if (result==NX_OK && size<=prefix) result=NX_ERROR_ARGUMENT;
    if (result==NX_OK && edit) {
        uint32_t x=((uint32_t)encoded[0]<<8)|encoded[1],y=((uint32_t)encoded[2]<<8)|encoded[3];
        uint32_t w=((uint32_t)encoded[4]<<8)|encoded[5],h=((uint32_t)encoded[6]<<8)|encoded[7];
        if (kind==CONTENT_BROWSER_BLUR?x||y||w||h:
            !w||!h||x+w>request.width||y+h>request.height) result=NX_ERROR_ARGUMENT;
        candidate=(nx_rect){(float)x,(float)y,(float)w,(float)h};
    }
    if (result==NX_OK) result=content_png_bitmap(encoded+prefix,size-prefix,&bitmap);
    if (result==NX_OK) {
        if (bitmap.width!=request.width || bitmap.height!=request.height) result=NX_ERROR_ARGUMENT;
        else result=content_png_fit_bitmap(bitmap,request,sequence,out,bytes,NULL);
    }
    if (result==NX_OK && edit) *caret=candidate;
done:
    free((void *)bitmap.pixels);
    free(encoded);
    close(fd);
    return result;
}

nx_result content_browser_frame(const char *socket_path,content_request request,
    uint64_t sequence,unsigned kind,void **out,size_t *bytes) {
    if (kind>4) return NX_ERROR_ARGUMENT;
    return transfer(socket_path,request,sequence,kind,NULL,out,bytes,NULL);
}

nx_result content_browser_edit_frame(const char *socket_path,content_request request,
    uint64_t sequence,const content_browser_edit *edit,void **out,size_t *bytes,nx_rect *caret) {
    if (!edit || !caret || edit->kind<CONTENT_BROWSER_FOCUS || edit->kind>CONTENT_BROWSER_MOUSE_CANCEL ||
        edit->length>CONTENT_BROWSER_TEXT_LIMIT ||
        (edit->length && (edit->kind==CONTENT_BROWSER_FOCUS || edit->kind==CONTENT_BROWSER_BLUR ||
                          edit->kind==CONTENT_BROWSER_MOUSE_CANCEL)) ||
        (!edit->length && edit->kind==CONTENT_BROWSER_COMMIT) ||
        (edit->kind==CONTENT_BROWSER_PREEDIT &&
            ((edit->begin==-1)!=(edit->end==-1) || edit->begin < -1 || edit->end < -1 ||
             edit->begin>(int32_t)edit->length || edit->end>(int32_t)edit->length)) ||
        memchr(edit->text,0,edit->length) ||
        nx_utf8_validate(edit->text,edit->length,NULL)!=NX_OK) return NX_ERROR_ARGUMENT;
    if (edit->kind>=CONTENT_BROWSER_MOUSE_MOVE && edit->kind<=CONTENT_BROWSER_MOUSE_WHEEL) {
        const content_browser_pointer *pointer=&edit->pointer;
        uint32_t button=pointer->button;
        int edge=edit->kind==CONTENT_BROWSER_MOUSE_DOWN || edit->kind==CONTENT_BROWSER_MOUSE_UP;
        if (edit->length || pointer->x<0 || pointer->y<0 ||
            (uint32_t)pointer->x/128u>=request.width || (uint32_t)pointer->y/128u>=request.height ||
            (edge?(!button || button>NX_BUTTON_X2 || (button&(button-1u))):button!=0) ||
            (pointer->modifiers & ~(NX_MOD_SHIFT|NX_MOD_CONTROL|NX_MOD_ALT|NX_MOD_SUPER)))
            return NX_ERROR_ARGUMENT;
    }
    return transfer(socket_path,request,sequence,edit->kind,edit,out,bytes,caret);
}
