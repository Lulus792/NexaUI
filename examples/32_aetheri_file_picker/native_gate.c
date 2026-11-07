#define _POSIX_C_SOURCE 200809L
/* Test-only original-I/O gate, linked solely into file_picker_held_native.
   The production host/worker has no environment-selected gate or callback. */
#include "files/model.h"
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
enum ae_files_result __real_ae_files_refresh(struct ae_files *);
enum ae_files_result __wrap_ae_files_refresh(struct ae_files *f) {
    const char *input=getenv("FP_TEST_REFRESH_GATE");char *end;
    if(!input)return __real_ae_files_refresh(f);
    errno=0;long value=strtol(input,&end,10);
    if(errno||!*input||*end||value<0||value>INT_MAX)return AE_FILES_INVALID;
    puts("ORIGINAL_REFRESH_HELD");char byte;ssize_t n;
    do{n=read((int)value,&byte,1);}while(n<0&&errno==EINTR);
    close((int)value);
    if(n!=1)return AE_FILES_IO;
    puts("ORIGINAL_REFRESH_RELEASED");return __real_ae_files_refresh(f);
}
