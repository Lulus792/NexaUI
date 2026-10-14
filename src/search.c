#include "nexaui/nexaui.h"

#include <stddef.h>

static unsigned char nx_search_fold(unsigned char byte) {
    return byte >= 'A' && byte <= 'Z' ?
        (unsigned char)(byte + ('a' - 'A')) : byte;
}

static int nx_search_contains(nx_utf8_span name, nx_utf8_span query) {
    if (!query.length) return 1;
    if (query.length > name.length) return 0;
    for (size_t start = 0; start <= name.length - query.length; ++start) {
        size_t i = 0;
        for (; i < query.length; ++i)
            if (nx_search_fold((unsigned char)name.data[start + i]) !=
                nx_search_fold((unsigned char)query.data[i])) break;
        if (i == query.length) return 1;
    }
    return 0;
}

nx_result nx_search_filter(const nx_search_option *options,
                           size_t option_count, nx_utf8_span query,
                           size_t *indices, size_t capacity,
                           size_t *out_count) {
    if (!out_count) return NX_ERROR_ARGUMENT;
    *out_count = 0;
    if ((!options && option_count) || (!indices && capacity) ||
        (!query.data && query.length) ||
        nx_utf8_validate(query.data, query.length, NULL) != NX_OK)
        return NX_ERROR_ARGUMENT;
    size_t matches = 0;
    for (size_t i = 0; i < option_count; ++i) {
        nx_utf8_span name = options[i].name;
        if (!options[i].id || !name.data || !name.length ||
            nx_utf8_validate(name.data, name.length, NULL) != NX_OK)
            return NX_ERROR_ARGUMENT;
        if (nx_search_contains(name, query)) ++matches;
    }
    *out_count = matches;
    if (!indices) return NX_OK;
    if (capacity < matches) return NX_ERROR_LIMIT;
    size_t next = 0;
    for (size_t i = 0; i < option_count; ++i)
        if (nx_search_contains(options[i].name, query)) indices[next++] = i;
    return NX_OK;
}
