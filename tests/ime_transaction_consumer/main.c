#include <nexaui/nexaui.h>
#include <stdio.h>

int main(void) {
    nx_version version={0};
    if (NX_VERSION_MAJOR!=0 || NX_VERSION_MINOR!=1 || NX_VERSION_PATCH!=25 ||
        nx_get_version(&version)!=NX_OK || version.major!=0 || version.minor!=1 || version.patch!=25 ||
        nx_window_set_ime_transaction_events(NULL,1)!=NX_ERROR_ARGUMENT) return 1;
    printf("Installed C17 IME transaction API %u.%u.%u\n",version.major,version.minor,version.patch);
    return 0;
}
