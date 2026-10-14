#ifndef NEXAUI_PRIVATE_SECURE_ZERO_H
#define NEXAUI_PRIVATE_SECURE_ZERO_H

#include <stddef.h>

/* Volatile byte stores keep this best-effort scrub observable to the compiler.
   This does not control copies owned by callers, the OS, or other subsystems. */
static void nx_private_secure_zero(void *memory, size_t length) {
    volatile unsigned char *bytes = (volatile unsigned char *)memory;
    for (size_t i = 0; i < length; ++i) bytes[i] = 0;
}

#endif
