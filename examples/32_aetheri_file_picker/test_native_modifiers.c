/* Test-only adapter variant: expose released modifier state on key-up. Some
   producer sequences report the preceding state until their modifiers event.
   Keep the same actual native key/pointer input and independent OS peer. */
#include <nexaui/nexaui.h>
#include <stdio.h>
nx_result __real_nx_window_next_event(nx_window *,nx_event *,int *);
nx_result __wrap_nx_window_next_event(nx_window *window,nx_event *event,int *available){
    nx_result result=__real_nx_window_next_event(window,event,available);
    if(result==NX_OK&&*available&&event->type==NX_EVENT_KEY_UP&&event->key==NX_KEY_CONTROL){
        event->modifiers&=~NX_MOD_CONTROL;puts("TEST_POST_RELEASE_MODIFIER_EVENT");
    }
    return result;
}
