#ifndef NX_ASCII_RUN_H
#define NX_ASCII_RUN_H

#include <stddef.h>
#include <limits.h>
#include <stdint.h>
#include <string.h>

/* Complete bytes only; memcpy avoids alignment and aliasing requirements.
   The high-bit mask is independent of byte order. No padding is read. */
static inline size_t nx_ascii_prefix(const char *bytes, size_t length) {
    size_t offset = 0;
#if CHAR_BIT == 8
    while (length - offset >= sizeof(uint64_t)) {
        uint64_t word;
        memcpy(&word, bytes + offset, sizeof(word));
        if (word & UINT64_C(0x8080808080808080)) break;
        offset += sizeof(word);
    }
#endif
    while (offset < length && (unsigned char)bytes[offset] <= 0x7fu) ++offset;
    return offset;
}

#endif
