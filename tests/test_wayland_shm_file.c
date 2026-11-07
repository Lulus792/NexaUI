#define _GNU_SOURCE
#include "../platform/linux/wayland/shm_file.h"
#include <dirent.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr,"%d: %s\n",__LINE__,#x); return 1; } } while (0)
static int create_error,reserve_error,reserve_interrupt,temp_error,unlink_error;
static unsigned creates,reserves,temps,unlinks;
int __real_memfd_create(const char *,unsigned);
int __real_posix_fallocate(int,off_t,off_t);
int __real_mkostemp(char *,int);
int __real_unlink(const char *);
int __wrap_memfd_create(const char *name,unsigned flags) {
    ++creates;
    if (create_error) { errno=create_error; return -1; }
    return __real_memfd_create(name,flags);
}
int __wrap_posix_fallocate(int fd,off_t offset,off_t bytes) {
    ++reserves;
    if (reserve_interrupt) { reserve_interrupt=0; return EINTR; }
    return reserve_error?reserve_error:__real_posix_fallocate(fd,offset,bytes);
}
int __wrap_mkostemp(char *path,int flags) {
    ++temps;
    if (strcmp(path,"/tmp/nexaui-wayland-XXXXXX") || flags!=O_CLOEXEC) abort();
    if (temp_error) { errno=temp_error; return -1; }
    /* Isolate only this test's fallback files from a full system /tmp. */
    const char *scratch=getenv("TMPDIR"); if (!scratch) scratch="/tmp";
    char actual[PATH_MAX];
    int n=snprintf(actual,sizeof actual,"%s/nexaui-shm-test-XXXXXX",scratch);
    if (n<0 || (size_t)n>=sizeof actual) abort();
    int fd=__real_mkostemp(actual,flags);
    if (fd>=0 && __real_unlink(actual)) abort();
    return fd;
}
int __wrap_unlink(const char *path) {
    ++unlinks;
    if (strncmp(path,"/tmp/nexaui-wayland-",20)) abort();
    if (unlink_error) { errno=unlink_error; return -1; }
    return 0; /* The real private file was unlinked by the wrapper above. */
}
static int descriptors(void) {
    DIR *d=opendir("/proc/self/fd");if (!d) return -1;
    int count=0;struct dirent *entry;
    while ((entry=readdir(d))) if (entry->d_name[0]!='.') ++count;
    if (closedir(d)) return -1;
    return count;
}
static int writable(int fallback) {
    unsigned old_temps=temps,old_reserves=reserves;
    int before=descriptors(),fd=nx_wayland_shm_file(1920000);CHECK(before>=0 && fd>=0);
    struct stat info;CHECK(!fstat(fd,&info) && info.st_size==1920000);
    CHECK((fcntl(fd,F_GETFD)&FD_CLOEXEC)!=0);
    CHECK(temps==old_temps+(unsigned)fallback && reserves>old_reserves);
    unsigned char *pixels=mmap(NULL,1920000,PROT_READ|PROT_WRITE,MAP_SHARED,fd,0);
    CHECK(pixels!=MAP_FAILED);memset(pixels,0xa5,1920000);
    for (size_t i=0;i<1920000;i+=4096) CHECK(pixels[i]==0xa5);
    CHECK(pixels[1919999]==0xa5 && !munmap(pixels,1920000) && !close(fd));
    CHECK(descriptors()==before);return 0;
}
static int refuses(int expected) {
    int before=descriptors();errno=0;
    CHECK(before>=0 && nx_wayland_shm_file(4096)==-1 && errno==expected);
    CHECK(descriptors()==before);return 0;
}
int main(void) {
    CHECK(nx_wayland_shm_file(0)==-1 && errno==EINVAL);
    CHECK(nx_wayland_shm_file((size_t)INT_MAX+1)==-1 && errno==EINVAL && !creates);
    CHECK(!writable(0));reserve_interrupt=1;CHECK(!writable(0) && !reserve_interrupt);
    const int failures[]={EDQUOT,ENOSPC,ENOMEM,EOPNOTSUPP};
    for (size_t i=0;i<sizeof failures/sizeof *failures;++i) { reserve_error=failures[i];CHECK(!refuses(failures[i])); }
    reserve_error=0;
    create_error=ENOMEM;unsigned old_temps=temps;CHECK(!refuses(ENOMEM) && temps==old_temps);
    create_error=ENOSYS;CHECK(!writable(1) && unlinks);
    reserve_error=ENOSPC;CHECK(!refuses(ENOSPC));reserve_error=0;
    temp_error=EMFILE;CHECK(!refuses(EMFILE));temp_error=0;
    unlink_error=EACCES;CHECK(!refuses(EACCES));
    puts("Wayland backing: eager reservation, full writable mappings, CLOEXEC, old-kernel fallback, quota/space errors and descriptor balance");
    return 0;
}
