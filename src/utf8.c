#include "nexaui/nexaui.h"

nx_result nx_utf8_decode(const char *bytes, size_t length,
                         size_t *offset, uint32_t *out_scalar) {
    if ((!bytes && length) || !offset || !out_scalar || *offset >= length)
        return NX_ERROR_ARGUMENT;
    size_t i = *offset;
    unsigned char first = (unsigned char)bytes[i];
    unsigned needed;
    uint32_t scalar;
    uint32_t minimum;
    if (first <= 0x7fu) {
        needed = 1;
        scalar = first;
        minimum = 0;
    } else if (first >= 0xc2u && first <= 0xdfu) {
        needed = 2;
        scalar = first & 0x1fu;
        minimum = 0x80u;
    } else if (first >= 0xe0u && first <= 0xefu) {
        needed = 3;
        scalar = first & 0x0fu;
        minimum = 0x800u;
    } else if (first >= 0xf0u && first <= 0xf4u) {
        needed = 4;
        scalar = first & 0x07u;
        minimum = 0x10000u;
    } else {
        return NX_ERROR_ARGUMENT;
    }
    if ((size_t)needed > length - i) return NX_ERROR_ARGUMENT;
    for (unsigned j = 1; j < needed; ++j) {
        unsigned char next = (unsigned char)bytes[i + j];
        if ((next & 0xc0u) != 0x80u) return NX_ERROR_ARGUMENT;
        scalar = (scalar << 6u) | (next & 0x3fu);
    }
    if (scalar < minimum || (scalar >= 0xd800u && scalar <= 0xdfffu) ||
        scalar > 0x10ffffu) return NX_ERROR_ARGUMENT;
    *offset = i + needed;
    *out_scalar = scalar;
    return NX_OK;
}

nx_result nx_utf8_validate(const char *bytes, size_t length, size_t *error_offset) {
    if (!bytes && length) return NX_ERROR_ARGUMENT;
    size_t offset = 0;
    uint32_t scalar;
    while (offset < length) {
        size_t start = offset;
        if (nx_utf8_decode(bytes, length, &offset, &scalar) != NX_OK) {
            if (error_offset) *error_offset = start;
            return NX_ERROR_ARGUMENT;
        }
    }
    if (error_offset) *error_offset = length;
    return NX_OK;
}
