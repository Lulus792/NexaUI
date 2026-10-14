/* Deliberate defect used only to prove the optional ASan runtime can report it. */
#include <stdlib.h>

int main(int argc, char **argv) {
    if (argc != 2) return 2;
    size_t offset = (size_t)strtoul(argv[1], NULL, 10);
    volatile unsigned char *bytes = malloc(8u);
    if (!bytes) return 3;
    bytes[offset] = 1u;
    free((void *)bytes);
    return 0;
}
