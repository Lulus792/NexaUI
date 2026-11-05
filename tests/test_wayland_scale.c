#include "scale.h"
#include <limits.h>
#include <stdio.h>

#define CHECK(x) do { if (!(x)) { \
    fprintf(stderr,"%s:%d: %s\n",__FILE__,__LINE__,#x);return 1; \
} } while (0)

int main(void) {
    int width=-1,height=-1;
    CHECK(nx_wayland_scale_extent(101,51,120,&width,&height)==NX_OK && width==101 && height==51);
    CHECK(nx_wayland_scale_extent(101,51,150,&width,&height)==NX_OK && width==126 && height==64);
    CHECK(nx_wayland_scale_extent(101,51,180,&width,&height)==NX_OK && width==152 && height==77);
    CHECK(nx_wayland_scale_extent(101,51,240,&width,&height)==NX_OK && width==202 && height==102);
    CHECK(nx_wayland_scale_extent(20,60,123,&width,&height)==NX_OK && width==21 && height==62);
    CHECK(nx_wayland_scale_extent(1,1,59,&width,&height)==NX_ERROR_LIMIT && !width && !height);
    CHECK(nx_wayland_scale_extent(0,1,120,&width,&height)==NX_ERROR_ARGUMENT);
    CHECK(nx_wayland_scale_extent(1,1,0,&width,&height)==NX_ERROR_ARGUMENT);
    CHECK(nx_wayland_scale_extent(1,1,120,NULL,&height)==NX_ERROR_ARGUMENT);
    CHECK(nx_wayland_scale_extent(INT_MAX,INT_MAX,UINT32_MAX,&width,&height)==NX_ERROR_LIMIT && !width && !height);
    CHECK(nx_wayland_scale_extent(30000,30000,120,&width,&height)==NX_ERROR_LIMIT);
    puts("Wayland scale extents use exact 120ths and checked buffer limits");
    return 0;
}
