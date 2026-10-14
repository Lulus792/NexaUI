#include "uri_list.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(expr) do { if (!(expr)) { \
    fprintf(stderr, "%s:%d: %s failed\n", __FILE__, __LINE__, #expr); \
    return 1; \
} } while (0)

typedef struct allocation_probe {
    unsigned calls, live, fail_on;
} allocation_probe;

static void *probe_allocate(void *user, size_t size) {
    allocation_probe *probe = user;
    if (++probe->calls == probe->fail_on) return NULL;
    void *pointer = malloc(size);
    if (pointer) ++probe->live;
    return pointer;
}

static void probe_deallocate(void *user, void *pointer, size_t size) {
    allocation_probe *probe = user;
    (void)size;
    if (pointer) --probe->live;
    free(pointer);
}

int main(void) {
    allocation_probe probe = {0};
    nx_platform_drop_queue queue;
    nx_platform_drop_queue_init(&queue, (nx_allocator){&probe,
        probe_allocate, probe_deallocate});
    CHECK(nx_platform_uri_list_append(&queue, "x", 32u * 1024u * 1024u + 1u,
                                        0, 0, "test-host") == NX_ERROR_LIMIT);
    const char input[] =
        "# source\r\n"
        "file:///tmp/first%20file.txt\r\n"
        "https://example.org/ignored\r\n"
        "file://remote/tmp/ignored\r\n"
        "file:////remote/ambiguous\n"
        "file://LOCALHOST/tmp/caf%C3%A9.txt\n"
        "file://TEST-HOST/tmp/hosted\n"
        "file:///tmp/bad%00path\n"
        "file:///tmp/bad%2\n"
        "file:///tmp/bad%C0\n"
        "file:///tmp/raw space\n"
        "file:/tmp/third\r\n";
    CHECK(nx_platform_uri_list_append(&queue, input, sizeof(input) - 1u,
                                        12.5f, 6.25f,
                                        "test-host") == NX_OK);
    char path[128] = {0};
    size_t length = 0;
    nx_file_drop_info info;
    int available = 0;
    CHECK(nx_platform_drop_queue_next(&queue, NULL, 0, &length, &info,
                                       &available) == NX_OK && available &&
          length == strlen("/tmp/first file.txt") && info.index == 0 &&
          info.count == 4 && info.x == 12.5f && info.y == 6.25f);
    CHECK(nx_platform_drop_queue_next(&queue, path, length, &length, &info,
                                       &available) == NX_ERROR_LIMIT &&
          available && info.index == 0);
    CHECK(nx_platform_drop_queue_next(&queue, path, sizeof(path), &length,
                                       &info, &available) == NX_OK &&
          !strcmp(path, "/tmp/first file.txt"));
    CHECK(nx_platform_drop_queue_next(&queue, path, sizeof(path), &length,
                                       &info, &available) == NX_OK &&
          !strcmp(path, "/tmp/caf\xc3\xa9.txt") && info.index == 1);
    CHECK(nx_platform_drop_queue_next(&queue, path, sizeof(path), &length,
                                       &info, &available) == NX_OK &&
          !strcmp(path, "/tmp/hosted") && info.index == 2);
    CHECK(nx_platform_drop_queue_next(&queue, path, sizeof(path), &length,
                                       &info, &available) == NX_OK &&
          !strcmp(path, "/tmp/third") && info.index == 3);
    CHECK(nx_platform_drop_queue_next(&queue, path, sizeof(path), &length,
                                       &info, &available) == NX_OK && !available);

    const char prior[] = "file:///tmp/prior\n";
    CHECK(nx_platform_uri_list_append(&queue, prior, sizeof(prior) - 1u,
                                        1, 2, "test-host") == NX_OK);
    unsigned before = probe.live;
    probe.fail_on = probe.calls + 3u;
    CHECK(nx_platform_uri_list_append(&queue,
        "file:///tmp/a\nfile:///tmp/b\n", 28u, 3, 4,
        "test-host") == NX_ERROR_MEMORY);
    CHECK(probe.live == before);
    CHECK(nx_platform_drop_queue_next(&queue, path, sizeof(path), &length,
                                       &info, &available) == NX_OK &&
          available && !strcmp(path, "/tmp/prior") && info.count == 1);
    CHECK(nx_platform_drop_queue_next(&queue, path, sizeof(path), &length,
                                       &info, &available) == NX_OK && !available);
    nx_platform_drop_queue_destroy(&queue);
    CHECK(probe.live == 0);
    return 0;
}
