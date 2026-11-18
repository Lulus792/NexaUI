#ifndef CONTENT_NATIVE_GUEST_INPUT_H
#define CONTENT_NATIVE_GUEST_INPUT_H

typedef struct native_guest_input {
    native_input_receiver ledger;
    content_guest_keys pending;
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

static nx_result native_guest_receive(void *user,uint64_t generation,const nx_event *events,size_t count) {
    native_guest_input *guest=user;
    if (!events || !count || count>NX_KEY_COUNT+1u || generation!=guest->ledger.generation) return NX_ERROR_ARGUMENT;
    content_guest_keys next=guest->pending;
    for (size_t i=0;i<count;++i) {
        const nx_event *event=&events[i];
        if (event->type==NX_EVENT_FOCUS) continue;
        if (event->type!=NX_EVENT_KEY_DOWN && event->type!=NX_EVENT_KEY_UP) return NX_ERROR_UNSUPPORTED;
        uint8_t code=guest_key_code(event->key);
        if (!code) return NX_ERROR_UNSUPPORTED;
        if (next.count==CONTENT_GUEST_KEY_LIMIT) return NX_ERROR_LIMIT;
        next.bytes[next.count*2u]=code;
        next.bytes[next.count*2u+1u]=(uint8_t)(event->type==NX_EVENT_KEY_DOWN);
        ++next.count;
    }
    nx_result result=native_input_receive(&guest->ledger,generation,events,count);
    if (result==NX_OK) guest->pending=next;
    return result;
}

#endif
