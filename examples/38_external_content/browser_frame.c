#define _POSIX_C_SOURCE 200809L
#include "browser_frame.h"
#include "guest_frame.h"
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

#define BROWSER_AX_WIRE_LIMIT (4u+CONTENT_AX_NODE_LIMIT*(22u+CONTENT_AX_NAME_LIMIT))

static uint32_t wire_u16(const uint8_t *bytes) { return ((uint32_t)bytes[0]<<8)|bytes[1]; }
static uint32_t wire_u32(const uint8_t *bytes) { return (wire_u16(bytes)<<16)|wire_u16(bytes+2); }

static nx_result read_ax_packet(const uint8_t *data,size_t bytes,content_request request,
    content_ax_action action,content_ax_node nodes[CONTENT_AX_NODE_LIMIT],uint32_t *count,uint32_t *epoch) {
    if (bytes<4) return NX_ERROR_ARGUMENT;
    uint32_t id=wire_u16(data),total=wire_u16(data+2);
    if (!id || !total || total>CONTENT_AX_NODE_LIMIT || (action.node && action.epoch!=id))
        return NX_ERROR_ARGUMENT;
    size_t offset=4;
    for (uint32_t i=0;i<total;++i) {
        if (bytes-offset<22u) return NX_ERROR_ARGUMENT;
        const uint8_t *row=data+offset;
        uint32_t x=wire_u16(row+12),y=wire_u16(row+14),w=wire_u16(row+16),h=wire_u16(row+18),n=wire_u16(row+20);
        if (!w || !h || x+w>request.width || y+h>request.height ||
            n>CONTENT_AX_NAME_LIMIT || n>bytes-offset-22u) return NX_ERROR_ARGUMENT;
        nodes[i]=(content_ax_node){.id=wire_u32(row),.parent=wire_u32(row+4),
            .role=(uint16_t)wire_u16(row+8),.state=(uint16_t)wire_u16(row+10),.name_bytes=(uint16_t)n,
            .x=(uint16_t)x,.y=(uint16_t)y,.width=(uint16_t)w,.height=(uint16_t)h};
        memcpy(nodes[i].name,row+22,n);
        offset+=22u+n;
    }
    if (offset!=bytes) return NX_ERROR_ARGUMENT;
    *count=total;
    *epoch=id;
    return NX_OK;
}

static nx_result transfer(const char *socket_path,content_request request,uint64_t sequence,unsigned kind,
    const content_browser_edit *edit,int accessible,content_ax_action action,const content_guest_batch *input,
    void **out,size_t *bytes,nx_rect *caret) {
    struct sockaddr_un address={.sun_family=AF_UNIX};
    if (!socket_path || socket_path[0]!='/' || strlen(socket_path)>=sizeof(address.sun_path) ||
        !request.generation || !request.resize || !request.width || !request.height || !sequence ||
        !out || !bytes) return NX_ERROR_ARGUMENT;
    if (request.width>CONTENT_PNG_SOURCE_DIMENSION || request.height>CONTENT_PNG_SOURCE_DIMENSION ||
        request.width*4u>CONTENT_PNG_PIXEL_LIMIT/request.height) return NX_ERROR_LIMIT;
    strcpy(address.sun_path,socket_path);
    char line[128u+CONTENT_GUEST_INPUT_LIMIT*CONTENT_GUEST_RECORD_SIZE];
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
    } else if (input && input->count) {
        size_t body=input->count*CONTENT_GUEST_RECORD_SIZE;
        length=snprintf(line+send_size,sizeof(line)-send_size," %zu\n",body);
        if (length<=0 || (size_t)length>=sizeof(line)-send_size || body>sizeof(line)-send_size-(size_t)length)
            return NX_ERROR_LIMIT;
        send_size+=(size_t)length;
        memcpy(line+send_size,input->records,body);
        send_size+=body;
    } else if (action.node) {
        length=snprintf(line+send_size,sizeof(line)-send_size," %u %u\n",action.epoch,action.node);
        if (length<=0 || (size_t)length>=sizeof(line)-send_size) return NX_ERROR_LIMIT;
        send_size+=(size_t)length;
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
    size_t prefix=edit?8u:0;
    size_t capacity=prefix+(accessible?4u+BROWSER_AX_WIRE_LIMIT:0)+CONTENT_PNG_ENCODED_LIMIT+1u;
    encoded=malloc(capacity);
    if (!encoded) { result=NX_ERROR_MEMORY;goto done; }
    size_t size=0;
    result=read_frame(fd,encoded,capacity,&size,deadline);
    if (result==NX_OK && input && kind==2 && size==1u && encoded[0]=='P')
        result=NX_ERROR_STATE;
    nx_rect candidate={0};
    content_ax_node ax[CONTENT_AX_NODE_LIMIT];
    uint32_t ax_count=0,epoch=0;
    if (result==NX_OK && accessible) {
        uint32_t metadata=size>=4?wire_u32(encoded):0;
        if (!metadata || metadata>BROWSER_AX_WIRE_LIMIT || metadata>size-4u) result=NX_ERROR_ARGUMENT;
        else {
            prefix=4u+metadata;
            result=read_ax_packet(encoded+4,metadata,request,action,ax,&ax_count,&epoch);
        }
    }
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
        if (!input && (bitmap.width!=request.width || bitmap.height!=request.height)) result=NX_ERROR_ARGUMENT;
        else {
            void *frame=NULL;
            size_t frame_bytes=0;
            result=content_png_fit_bitmap(bitmap,request,sequence,&frame,&frame_bytes,NULL);
            if (result==NX_OK && ax_count) {
                size_t tail=(size_t)ax_count*sizeof(*ax);
                void *extended=realloc(frame,frame_bytes+tail);
                if (!extended) { free(frame);result=NX_ERROR_MEMORY; }
                else {
                    frame=extended;
                    content_frame_header *header=frame;
                    header->ax_count=ax_count;
                    header->ax_epoch=epoch;
                    memcpy((uint8_t *)frame+frame_bytes,ax,tail);
                    frame_bytes+=tail;
                }
            }
            if (result==NX_OK) { *out=frame;*bytes=frame_bytes; }
        }
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
    return transfer(socket_path,request,sequence,kind,NULL,0,(content_ax_action){0},NULL,out,bytes,NULL);
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
    return transfer(socket_path,request,sequence,edit->kind,edit,0,(content_ax_action){0},NULL,out,bytes,caret);
}

nx_result content_browser_ax_frame(const char *socket_path,content_request request,
    uint64_t sequence,unsigned kind,content_ax_action action,void **out,size_t *bytes) {
    if (kind>4 || (!!action.epoch!=!!action.node) || action.epoch>UINT16_MAX) return NX_ERROR_ARGUMENT;
    return transfer(socket_path,request,sequence,action.node?14u:kind,NULL,1,action,NULL,out,bytes,NULL);
}

nx_result content_guest_frame(const char *path,content_request request,uint64_t sequence,
    unsigned kind,const content_guest_batch *input,void **out,size_t *bytes) {
    if (!input || input->count>CONTENT_GUEST_INPUT_LIMIT || kind>5 || (!!input->count!=(kind==5)))
        return NX_ERROR_ARGUMENT;
    return transfer(path,request,sequence,kind,NULL,0,(content_ax_action){0},input,out,bytes,NULL);
}
