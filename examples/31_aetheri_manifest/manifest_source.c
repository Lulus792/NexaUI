#define _POSIX_C_SOURCE 200809L
#include "manifest_source.h"
#include "core/fixture.h"
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

_Static_assert(AH_CHILD_LIMIT >= AE_FIXTURE_FILES, "Do not truncate the original manifest");
_Static_assert(AH_NAME_BYTES >= 257, "Keep complete original 256-byte paths");

nx_result amh_project(const unsigned char *bytes, size_t size,
    const unsigned char expected[32], ah_key key, ah_response *output) {
    if (!bytes || !expected || !output || !key.view || !key.generation || key.branch != AH_BRANCH)
        return NX_ERROR_ARGUMENT;
    struct ae_fixture fixture;
    int r = ae_fixture_verify_manifest(bytes, size, expected, &fixture);
    if (r) return r == -E2BIG ? NX_ERROR_LIMIT : r == -ENOMEM ? NX_ERROR_MEMORY :
        r == -EBADMSG || r == -EINVAL ? NX_ERROR_ARGUMENT : NX_ERROR_PLATFORM;
    ah_response next = {0}; next.key = key; next.count = fixture.file_count;
    for (size_t i = 0; i < next.count; ++i) {
        /* Parser admission proves unique strictly sorted paths and size <=256.
           Ordinals are stable because this view is bound to ONE exact artifact. */
        next.children[i].id = 100 + i;
        memcpy(next.children[i].name, fixture.files[i].path.data, fixture.files[i].path.size);
    }
    *output = next;
    return NX_OK;
}
nx_result amh_read(const char *source, const unsigned char expected[32], ah_key key, ah_response *output) {
    if (!source || !expected || !output) return NX_ERROR_ARGUMENT;
    int fd = open(source, O_RDONLY | O_CLOEXEC | O_NOFOLLOW | O_NONBLOCK);
    if (fd < 0) return NX_ERROR_PLATFORM;
    struct stat st;
    nx_result r = NX_ERROR_PLATFORM;
    ah_response next;
    unsigned char *bytes = NULL;
    if (fstat(fd, &st) || !S_ISREG(st.st_mode) || st.st_size < 0) goto done;
    if (st.st_size > AE_FIXTURE_MAX_BYTES) { r = NX_ERROR_LIMIT; goto done; }
    bytes = malloc(AE_FIXTURE_MAX_BYTES + 1u);
    if (!bytes) { r = NX_ERROR_MEMORY; goto done; }
    size_t used = 0;
    while (used < AE_FIXTURE_MAX_BYTES + 1u) {
        ssize_t n = read(fd, bytes + used, AE_FIXTURE_MAX_BYTES + 1u - used);
        if (n < 0) { if (errno == EINTR) continue; goto done; }
        if (!n) break;
        used += (size_t)n;
    }
    r = used > AE_FIXTURE_MAX_BYTES ? NX_ERROR_LIMIT : amh_project(bytes, used, expected, key, &next);
done:
    free(bytes);
    if (close(fd) && r == NX_OK) r = NX_ERROR_PLATFORM;
    if (r == NX_OK) *output = next;
    return r;
}
int amh_digest(const char *hex, unsigned char output[32]) {
    if (!hex || !output || strlen(hex) != 64) return 0;
    unsigned char next[32] = {0};
    for (size_t i = 0; i < 64; ++i) {
        unsigned char c = (unsigned char)hex[i];
        unsigned value;
        if (c >= '0' && c <= '9') value = c - '0';
        else if (c >= 'a' && c <= 'f') value = c - 'a' + 10u;
        else return 0;
        next[i / 2] = (unsigned char)((next[i / 2] << 4) | value);
    }
    memcpy(output, next, 32); return 1;
}
