#include <nexaui/nexaui.h>
#include <stdio.h>

#if NX_VERSION_MAJOR != 0 || NX_VERSION_MINOR != 1 || NX_VERSION_PATCH < 22
#error "This consumer needs NexaUI 0.1.22 or newer headers"
#endif

int main(void) {
    nx_version version={0};
    nx_interaction_state state={0};
    nx_text_field_style style={6,1,8};
    int valid=1,submitted=1;
    if(nx_get_version(&version)!=NX_OK || version.major!=0 || version.minor!=1 || version.patch<22)
        return 1;
    if(nx_text_field_widget_described(NULL,NULL,NULL,(nx_utf8_span){0},&state,&valid,&submitted)!=NX_ERROR_ARGUMENT || valid || submitted)
        return 2;
    valid=submitted=1;
    if(nx_text_field_widget_styled(NULL,NULL,NULL,(nx_utf8_span){0},&style,&state,&valid,&submitted)!=NX_ERROR_ARGUMENT || valid || submitted)
        return 3;
    puts("Installed 0.1.22 text field entry points resolved");
    return 0;
}
