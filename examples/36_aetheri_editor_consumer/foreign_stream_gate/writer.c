/* Standalone real-pipe subject; no Wayland, NexaUI or editing implementation. */
#define _POSIX_C_SOURCE 200809L
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int main(int argc,char **argv) {
    if(argc!=3)return 1;
    char *end=NULL;long fd=strtol(argv[1],&end,10);
    if(!end || *end || fd<3 || fd>INT_MAX)return 2;
    const char *text=getenv("NEXAUI_FOREIGN_GATE_PAYLOAD");if(!text)return 3;
    signal(SIGPIPE,SIG_IGN);FILE *stream=fdopen((int)fd,"w");if(!stream)return 4;
    size_t length=strlen(text),written=fwrite(text,1,length,stream);
    int closed=fclose(stream);
    return closed || (strcmp(argv[2],"short")?written!=length:written>=length);
}
