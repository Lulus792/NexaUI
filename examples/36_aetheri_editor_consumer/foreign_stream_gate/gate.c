/* Private transfer scheduler for the independent wl-copy process only.
   It matches one exact synthetic payload and a FIFO, then performs real writes
   while holding the original FILE/pipe open. It links no NexaUI/model code and
   must never be preloaded into the app, compositor, OS reader or other programs.
   The foreign provider is single-threaded; this is not a general stdio shim. */
#define _GNU_SOURCE
#include <dlfcn.h>
#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <unistd.h>

struct message {uint32_t magic,stage;uint64_t total,written;int32_t error,pipe_fd;uint64_t queued;};
_Static_assert(sizeof(struct message)==40,"private gate message layout");
static size_t (*original_write)(const void *,size_t,size_t,FILE *);
static int (*original_close)(FILE *);
static int control=-1;
static const char *payload;
static size_t payload_length,prefix;
static FILE *active;
static struct message state;

static void initialize(void) {
    if(original_write)return;
    void *write_symbol=dlsym(RTLD_NEXT,"fwrite"),*close_symbol=dlsym(RTLD_NEXT,"fclose");
    _Static_assert(sizeof(original_write)==sizeof(write_symbol),"POSIX function pointer ABI");
    _Static_assert(sizeof(original_close)==sizeof(close_symbol),"POSIX function pointer ABI");
    if(!write_symbol || !close_symbol)_exit(90);
    memcpy(&original_write,&write_symbol,sizeof(original_write));
    memcpy(&original_close,&close_symbol,sizeof(original_close));
    const char *fd=getenv("NEXAUI_FOREIGN_GATE_FD"),*part=getenv("NEXAUI_FOREIGN_GATE_PREFIX");
    payload=getenv("NEXAUI_FOREIGN_GATE_PAYLOAD");
    if(!fd && !part && !payload)return;
    if(!fd || !part || !payload)_exit(91);
    char *end=NULL;errno=0;long parsed=strtol(fd,&end,10);
    if(errno || !end || *end || parsed<=STDERR_FILENO || parsed>INT_MAX)_exit(92);
    control=(int)parsed;int kind=0;socklen_t length=sizeof(kind);
    if(getsockopt(control,SOL_SOCKET,SO_TYPE,&kind,&length) || kind!=SOCK_SEQPACKET)_exit(93);
    payload_length=strlen(payload);errno=0;unsigned long requested=strtoul(part,&end,10);
    if(errno || !end || *end || !requested || !payload_length || payload_length>131072 || requested>=payload_length)_exit(94);
    prefix=(size_t)requested;
}
static int command(int flags) {
    char value=0;ssize_t result;
    do {result=recv(control,&value,1,flags);} while(result<0 && errno==EINTR);
    return result==1?(unsigned char)value:-1;
}
static void report(uint32_t stage) {
    int saved=errno;state.stage=stage;
    ssize_t sent;
    do {sent=send(control,&state,sizeof(state),MSG_NOSIGNAL);} while(sent<0 && errno==EINTR);
    if(sent!=(ssize_t)sizeof(state))_exit(95);
    errno=saved;
}
static int next_command(void) {
    for(;;) {
        int value=command(0);if(value!='P')return value;
        int queued=0;
        if(ioctl(state.pipe_fd,FIONREAD,&queued) || queued<0) {state.error=errno?errno:EIO;report(4);return -1;}
        state.queued=(uint64_t)queued;report(5);
    }
}
static int transfer(int fd,const char *bytes,size_t length) {
    size_t done=0;
    while(done<length) {
        ssize_t count=write(fd,bytes+done,length-done);
        if(count<0 && errno==EINTR)continue;
        if(count<=0) {state.error=count<0?errno:EIO;return 0;}
        done+=(size_t)count;state.written+=(uint64_t)count;
    }
    return 1;
}
size_t fwrite(const void *bytes,size_t size,size_t count,FILE *stream) {
    initialize();
    if(control<0 || active || size!=1 || count!=payload_length ||
       memcmp(bytes,payload,payload_length))return original_write(bytes,size,count,stream);
    int fd=fileno(stream);struct stat status;
    if(fd<=STDERR_FILENO || fd==control || fstat(fd,&status) || !S_ISFIFO(status.st_mode))
        return original_write(bytes,size,count,stream);
    int next=command(MSG_DONTWAIT);
    if(next<0)return original_write(bytes,size,count,stream);
    if(next!='A')_exit(96);
    active=stream;state=(struct message){UINT32_C(0x4e584753),0,(uint64_t)payload_length,0,0,fd,0};
    if(!transfer(fd,bytes,prefix)) {report(4);return (size_t)state.written/size;}
    report(1);next=next_command();
    if(next=='N') {
        if(!transfer(fd,(const char *)bytes+prefix,payload_length-prefix)) {report(4);return (size_t)state.written/size;}
        report(2);next=next_command();
    }
    if(next!='E') {state.error=ECANCELED;report(4);}
    if(state.error)errno=state.error;
    return (size_t)state.written/size;
}
int fclose(FILE *stream) {
    initialize();int gated=stream==active;int result=original_close(stream);int saved=errno;
    if(gated) {active=NULL;if(result && !state.error)state.error=saved;report(3);}
    errno=saved;return result;
}
