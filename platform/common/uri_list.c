#include "uri_list.h"

#include <stdint.h>
#include <string.h>

#define NX_URI_LIST_MAX_PATHS 65536u
#define NX_URI_LIST_MAX_BYTES (32u * 1024u * 1024u)

static int nx_uri_ascii_equal(char a, char b) {
    if (a >= 'A' && a <= 'Z') a = (char)(a + ('a' - 'A'));
    if (b >= 'A' && b <= 'Z') b = (char)(b + ('a' - 'A'));
    return a == b;
}

static int nx_uri_ascii_prefix(const char *text, size_t length,
                               const char *prefix, size_t prefix_length) {
    if (length < prefix_length) return 0;
    for (size_t i = 0; i < prefix_length; ++i)
        if (!nx_uri_ascii_equal(text[i], prefix[i])) return 0;
    return 1;
}

static int nx_uri_hex(char digit) {
    if (digit >= '0' && digit <= '9') return digit - '0';
    if (digit >= 'A' && digit <= 'F') return digit - 'A' + 10;
    if (digit >= 'a' && digit <= 'f') return digit - 'a' + 10;
    return -1;
}

static nx_result nx_uri_local_path(nx_allocator allocator,
                                   const char *uri, size_t length,
                                   const char *local_host,
                                   char **out_path, size_t *out_length) {
    *out_path = NULL;
    *out_length = 0;
    if (!nx_uri_ascii_prefix(uri, length, "file:", 5))
        return NX_ERROR_STATE;
    size_t path_start = 5;
    if (length >= 7 && uri[5] == '/' && uri[6] == '/') {
        path_start = 7;
        while (path_start < length && uri[path_start] != '/') ++path_start;
        size_t authority_length = path_start - 7;
        if (authority_length) {
            int localhost = authority_length == 9 &&
                nx_uri_ascii_prefix(uri + 7, authority_length,
                                    "localhost", 9);
            size_t host_length = local_host ? strlen(local_host) : 0;
            int own_host = host_length == authority_length && host_length &&
                nx_uri_ascii_prefix(uri + 7, authority_length,
                                    local_host, host_length);
            if (!localhost && !own_host) return NX_ERROR_STATE;
        }
    }
    if (path_start >= length || uri[path_start] != '/' ||
        length == SIZE_MAX) return NX_ERROR_STATE;
    size_t used = 0;
    for (size_t i = path_start; i < length; ++i) {
        unsigned char byte = (unsigned char)uri[i];
        if (byte == '%') {
            if (i + 2u >= length || nx_uri_hex(uri[i + 1u]) < 0 ||
                nx_uri_hex(uri[i + 2u]) < 0) return NX_ERROR_STATE;
            byte = (unsigned char)((nx_uri_hex(uri[i + 1u]) << 4) |
                                    nx_uri_hex(uri[i + 2u]));
            i += 2u;
        } else if (byte < 0x21u || byte == 0x7fu || byte == '?' ||
                   byte == '#' || byte == '\\') return NX_ERROR_STATE;
        if (!byte || byte == '\r' || byte == '\n') return NX_ERROR_STATE;
        ++used;
    }
    char *path = allocator.allocate(allocator.user, used + 1u);
    if (!path) return NX_ERROR_MEMORY;
    size_t written = 0;
    for (size_t i = path_start; i < length; ++i) {
        unsigned char byte = (unsigned char)uri[i];
        if (byte == '%') {
            byte = (unsigned char)((nx_uri_hex(uri[i + 1u]) << 4) |
                                    nx_uri_hex(uri[i + 2u]));
            i += 2u;
        }
        path[written++] = (char)byte;
    }
    path[used] = '\0';
    if ((used > 1u && path[1] == '/') ||
        nx_utf8_validate(path, used, NULL) != NX_OK) {
        allocator.deallocate(allocator.user, path, used + 1u);
        return NX_ERROR_STATE;
    }
    *out_path = path;
    *out_length = used;
    return NX_OK;
}

nx_result nx_platform_uri_list_append(nx_platform_drop_queue *queue,
                                      const char *text, size_t length,
                                      float x, float y,
                                      const char *local_host) {
    if (!queue || !queue->allocator.allocate || !queue->allocator.deallocate ||
        (!text && length)) return NX_ERROR_ARGUMENT;
    if (length > NX_URI_LIST_MAX_BYTES) return NX_ERROR_LIMIT;
    nx_allocator allocator = queue->allocator;
    nx_platform_drop_entry *batch = NULL;
    size_t count = 0, capacity = 0;
    nx_result result = NX_OK;
    for (size_t start = 0; start < length;) {
        size_t end = start;
        while (end < length && text[end] != '\n') ++end;
        size_t line_length = end - start;
        if (line_length && text[start + line_length - 1u] == '\r')
            --line_length;
        if (line_length && text[start] != '#') {
            char *path = NULL;
            size_t path_length = 0;
            result = nx_uri_local_path(allocator, text + start, line_length,
                                       local_host, &path, &path_length);
            if (result == NX_ERROR_STATE) result = NX_OK;
            else if (result != NX_OK) break;
            else {
                if (count >= NX_URI_LIST_MAX_PATHS) {
                    allocator.deallocate(allocator.user, path,
                                         path_length + 1u);
                    result = NX_ERROR_LIMIT;
                    break;
                }
                if (count == capacity) {
                    size_t grown = capacity ? capacity * 2u : 8u;
                    if (grown < capacity || grown > SIZE_MAX / sizeof(*batch)) {
                        allocator.deallocate(allocator.user, path,
                                             path_length + 1u);
                        result = NX_ERROR_LIMIT;
                        break;
                    }
                    nx_platform_drop_entry *replacement = allocator.allocate(
                        allocator.user, grown * sizeof(*batch));
                    if (!replacement) {
                        allocator.deallocate(allocator.user, path,
                                             path_length + 1u);
                        result = NX_ERROR_MEMORY;
                        break;
                    }
                    if (count) memcpy(replacement, batch,
                                      count * sizeof(*batch));
                    if (batch) allocator.deallocate(allocator.user, batch,
                                                     capacity * sizeof(*batch));
                    batch = replacement;
                    capacity = grown;
                }
                batch[count++] = (nx_platform_drop_entry){path, path_length,
                    {x, y, 0, 0}};
            }
        }
        start = end < length ? end + 1u : end;
    }
    if (result == NX_OK && count) {
        for (size_t i = 0; i < count; ++i) {
            batch[i].info.index = (uint32_t)i;
            batch[i].info.count = (uint32_t)count;
        }
        result = nx_platform_drop_queue_append_batch(queue, batch, count);
    }
    if (result != NX_OK)
        for (size_t i = 0; i < count; ++i)
            allocator.deallocate(allocator.user, batch[i].path,
                                 batch[i].length + 1u);
    if (batch) allocator.deallocate(allocator.user, batch,
                                     capacity * sizeof(*batch));
    return result;
}
