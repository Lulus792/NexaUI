#include "nexaui/nexaui.h"
#include "../tests/fixture_font.h"

#include <stdio.h>

int main(void) {
    uint8_t bytes[512];
    font_fixture(bytes);
    return fwrite(bytes, 1u, sizeof(bytes), stdout) == sizeof(bytes) ? 0 : 1;
}
