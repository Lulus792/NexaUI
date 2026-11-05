#include <nexaui/nexaui.h>
#include <stdint.h>
int main(void) {
    uint32_t events=42;
    nx_version version;
    if(nx_get_version(&version)!=NX_OK || version.major!=0 || version.minor!=1 || version.patch<3) return 1;
    if(nx_window_poll_many(0,0,0,-1,&events)!=NX_ERROR_ARGUMENT || events!=0) return 2;
    return 0;
}
