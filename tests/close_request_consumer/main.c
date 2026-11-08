#include <nexaui/nexaui.h>
#include <stddef.h>
#include <stdio.h>
_Static_assert(sizeof(nx_window_info)==16,"Existing window-info ABI changed");
_Static_assert(offsetof(nx_window_info,should_close)==12,"Existing close flag moved");
int main(void){
    nx_version version={0};
    nx_window_close_request request={(nx_window *)(uintptr_t)1,99};
    if(nx_get_version(&version)!=NX_OK||version.major!=0||version.minor!=1||version.patch<27)return 1;
    if(nx_window_take_close_request(NULL,&request)!=NX_ERROR_ARGUMENT||request.window||request.id)return 2;
    if(nx_window_resolve_close(NULL,&request,NX_WINDOW_CLOSE_CANCEL)!=NX_ERROR_ARGUMENT)return 3;
    puts("Installed C17 close request exports/version and existing window-info ABI verified");return 0;
}
