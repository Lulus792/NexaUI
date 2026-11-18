#ifndef CONTENT_NATIVE_GUEST_INPUT_H
#define CONTENT_NATIVE_GUEST_INPUT_H

typedef struct native_guest_input {
    native_input_receiver ledger;
    content_guest_batch pending;
    int pointer_seen, merge_motion;
    int x, y, motion_from_x, motion_from_y;
} native_guest_input;

static uint8_t guest_key_code(nx_key key) {
    if (key>=NX_KEY_A && key<=NX_KEY_Z) return (uint8_t)('a'+key-NX_KEY_A);
    if (key>=NX_KEY_0 && key<=NX_KEY_9) return (uint8_t)('0'+key-NX_KEY_0);
    if (key>=NX_KEY_F1 && key<=NX_KEY_F12) return (uint8_t)(64u+key-NX_KEY_F1);
    static const uint8_t special[NX_KEY_COUNT]={
        [NX_KEY_CONTROL]=1,[NX_KEY_SHIFT]=3,[NX_KEY_ALT]=5,[NX_KEY_ENTER]=7,
        [NX_KEY_TAB]=8,[NX_KEY_SPACE]=9,[NX_KEY_ESCAPE]=10,[NX_KEY_BACKSPACE]=11,
        [NX_KEY_LEFT]=12,[NX_KEY_RIGHT]=13,[NX_KEY_UP]=14,[NX_KEY_DOWN]=15,
        [NX_KEY_DELETE]=16,[NX_KEY_INSERT]=17,[NX_KEY_HOME]=18,[NX_KEY_END]=19,
        [NX_KEY_PAGE_UP]=20,[NX_KEY_PAGE_DOWN]=21,[NX_KEY_SUPER]=22};
    return key>NX_KEY_UNKNOWN && key<NX_KEY_COUNT?special[key]:0;
}

static nx_result guest_append_edge(content_guest_batch *batch, uint8_t kind, uint8_t code, int down) {
    if (batch->count==CONTENT_GUEST_INPUT_LIMIT) return NX_ERROR_LIMIT;
    uint8_t *record=batch->records[batch->count++];
    record[0]=kind;
    record[1]=code;
    record[2]=(uint8_t)down;
    record[3]=record[4]=0;
    return NX_OK;
}

static nx_result guest_motion(native_guest_input *guest, int x, int y) {
    if (!guest->pointer_seen) {
        guest->pointer_seen=1;
        guest->x=x;
        guest->y=y;
        return NX_OK;
    }
    content_guest_batch *batch=&guest->pending;
    if (!guest->merge_motion||!batch->count) {
        if (guest->x==x&&guest->y==y) return NX_OK;
        if (batch->count==CONTENT_GUEST_INPUT_LIMIT) return NX_ERROR_LIMIT;
        ++batch->count;
        guest->motion_from_x=guest->x;
        guest->motion_from_y=guest->y;
        guest->merge_motion=1;
    }
    guest->x=x;
    guest->y=y;
    uint16_t dx=(uint16_t)(x-guest->motion_from_x), dy=(uint16_t)(y-guest->motion_from_y);
    if (!dx&&!dy) {
        --batch->count;
        guest->merge_motion=0;
        return NX_OK;
    }
    uint8_t *record=batch->records[batch->count-1u];
    record[0]=GUEST_MOTION;
    record[1]=(uint8_t)dx;
    record[2]=(uint8_t)(dx>>8);
    record[3]=(uint8_t)dy;
    record[4]=(uint8_t)(dy>>8);
    return NX_OK;
}

static nx_result native_guest_receive(void *user,uint64_t generation,const nx_event *events,size_t count) {
    native_guest_input *guest=user,next=*guest;
    if (!events || !count || count>NX_KEY_COUNT+6u || generation!=guest->ledger.generation) return NX_ERROR_ARGUMENT;
    for (size_t i=0;i<count;++i) {
        const nx_event *event=&events[i];
        nx_result result=NX_OK;
        if (event->type==NX_EVENT_FOCUS) {
            next.pointer_seen=next.merge_motion=0;
        } else if (event->type==NX_EVENT_POINTER_MOVE) {
            result=guest_motion(&next,(int)event->x,(int)event->y);
        } else if (event->type==NX_EVENT_POINTER_DOWN||event->type==NX_EVENT_POINTER_UP) {
            uint32_t button=event->button;
            uint8_t code=button==NX_BUTTON_LEFT?1:button==NX_BUTTON_RIGHT?2:button==NX_BUTTON_MIDDLE?3:0;
            if (code) {
                if (!next.pointer_seen&&event->type==NX_EVENT_POINTER_DOWN) {
                    next.pointer_seen=1;
                    next.x=(int)event->x;
                    next.y=(int)event->y;
                }
                next.merge_motion=0;
                result=guest_append_edge(&next.pending,GUEST_BUTTON,code,event->type==NX_EVENT_POINTER_DOWN);
            }
        } else if (event->type==NX_EVENT_KEY_DOWN||event->type==NX_EVENT_KEY_UP) {
            uint8_t code=guest_key_code(event->key);
            if (!code) return NX_ERROR_UNSUPPORTED;
            next.merge_motion=0;
            result=guest_append_edge(&next.pending,GUEST_KEY,code,event->type==NX_EVENT_KEY_DOWN);
        } else if (event->type!=NX_EVENT_WHEEL) return NX_ERROR_UNSUPPORTED;
        if (result!=NX_OK) return result;
    }
    nx_result result=native_input_receive(&next.ledger,generation,events,count);
    if (result==NX_OK) *guest=next;
    return result;
}

#endif
