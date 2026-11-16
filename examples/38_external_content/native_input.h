#ifndef CONTENT_NATIVE_INPUT_H
#define CONTENT_NATIVE_INPUT_H
#include "input.h"
/* Local atomic fixture receiver only. Never a VM/engine/reset acknowledgement. */
typedef struct native_input_receiver {
    uint64_t generation;
    unsigned char keys[NX_KEY_COUNT];
    uint32_t buttons;
    unsigned downs, ups, pointer_downs, pointer_ups, enters, exits, texts;
    float width, height;
    int focused;
} native_input_receiver;
static unsigned native_input_keys(const native_input_receiver *receiver) {
    unsigned keys=0;
    for(int i=1;i<NX_KEY_COUNT;++i) if(receiver->keys[i]) ++keys;
    return keys;
}
static nx_result native_input_receive(void *user,uint64_t generation,const nx_event *events,size_t count) {
    native_input_receiver *receiver=user,next=*receiver;
    if(generation!=receiver->generation||!count||count>(size_t)NX_KEY_COUNT+6u) return NX_ERROR_STATE;
    for(size_t i=0;i<count;++i) {
        nx_event e=events[i];
        if(e.type==NX_EVENT_FOCUS) {
            if(e.focused) {
                if(next.focused||native_input_keys(&next)||next.buttons||e.width<1||e.height<1) return NX_ERROR_STATE;
                next.focused=1;next.width=e.width;next.height=e.height;++next.enters;
            } else {
                if(!next.focused||native_input_keys(&next)||next.buttons) return NX_ERROR_STATE;
                next.focused=0;++next.exits;
            }
        } else if(!next.focused) return NX_ERROR_STATE;
        else if(e.type==NX_EVENT_KEY_DOWN||e.type==NX_EVENT_KEY_UP) {
            if(e.key<=NX_KEY_UNKNOWN||e.key>=NX_KEY_COUNT||e.repeat) return NX_ERROR_STATE;
            if(e.type==NX_EVENT_KEY_DOWN) {
                if(next.keys[e.key]) return NX_ERROR_STATE;
                next.keys[e.key]=1;++next.downs;
            } else {
                if(!next.keys[e.key]) return NX_ERROR_STATE;
                next.keys[e.key]=0;++next.ups;
            }
        } else if(e.type==NX_EVENT_POINTER_MOVE||e.type==NX_EVENT_POINTER_DOWN||e.type==NX_EVENT_POINTER_UP||e.type==NX_EVENT_WHEEL) {
            if(e.x<0||e.y<0||e.x>=next.width||e.y>=next.height) return NX_ERROR_STATE;
            if(e.type==NX_EVENT_POINTER_DOWN) {
                if(next.buttons&e.button) return NX_ERROR_STATE;
                next.buttons|=e.button;++next.pointer_downs;
            } else if(e.type==NX_EVENT_POINTER_UP) {
                if(!(next.buttons&e.button)) return NX_ERROR_STATE;
                next.buttons&=~e.button;++next.pointer_ups;
            }
        } else { ++next.texts;return NX_ERROR_UNSUPPORTED; }
    }
    *receiver=next;
    printf("INPUT_ADMITTED generation=%llu keys=%u buttons=%u down=%u up=%u pdown=%u pup=%u enter=%u exit=%u text=%u x=%.0f y=%.0f\n",
        (unsigned long long)generation,native_input_keys(receiver),receiver->buttons,receiver->downs,receiver->ups,
        receiver->pointer_downs,receiver->pointer_ups,receiver->enters,receiver->exits,receiver->texts,
        (double)events[count-1u].x,(double)events[count-1u].y);
    fflush(stdout);return NX_OK;
}
#endif
