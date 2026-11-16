#include "input.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "input line %d: %s\n", __LINE__, #x); goto done; } } while (0)
/* Separate atomic receiver model: only observed output changes its state.
   It rejects duplicate downs/unpaired ups, invalid coordinates, text, stale
   identities and focus loss with unreleased holds. No adapter internals read. */
typedef struct receiver {
    uint64_t generation;
    unsigned char keys[NX_KEY_COUNT];
    uint32_t buttons;
    unsigned calls, focus_in, focus_out, down, up, pointer_down, pointer_up, move, wheel;
    float x, y, width, height;
    int focused, fail, bad, reenter;
    content_input **input;
    nx_result nested[6];
} receiver;
static int receiver_keys(const receiver *r) {
    int count = 0;
    for (int k = 1; k < NX_KEY_COUNT; ++k) count += r->keys[k] != 0;
    return count;
}
static content_input_target target(void) {
    return (content_input_target){{16,136,448,208},896,416,1,1};
}
static nx_result receive(void *user, uint64_t generation, const nx_event *events, size_t count) {
    receiver *r = user; ++r->calls;
    if (r->reenter) {
        nx_event e = {.type=NX_EVENT_KEY_DOWN,.key=NX_KEY_A}, host;
        int routed = 1; content_input_target t = target(); r->reenter = 0;
        r->nested[0] = content_input_release(*r->input);
        r->nested[1] = content_input_close(r->input);
        r->nested[2] = content_input_arm(*r->input);
        r->nested[3] = content_input_set_target(*r->input, &t);
        r->nested[4] = content_input_rebind(*r->input, generation+1u);
        r->nested[5] = content_input_event(*r->input, &e, &host, &routed);
        if (routed || !*r->input) r->bad = 1;
    }
    if (r->fail) { r->fail = 0; return NX_ERROR_LIMIT; }
    receiver next = *r;
    if (!count || count > (size_t)NX_KEY_COUNT + 6u || generation != r->generation) goto invalid;
    for (size_t i = 0; i < count; ++i) {
        nx_event e = events[i];
        if (e.type == NX_EVENT_FOCUS) {
            if (e.focused) {
                if (next.focused || receiver_keys(&next) || next.buttons || e.width < 1 || e.height < 1) goto invalid;
                next.focused = 1; ++next.focus_in; next.width=e.width; next.height=e.height;
            } else {
                if (!next.focused || receiver_keys(&next) || next.buttons) goto invalid;
                next.focused = 0; ++next.focus_out;
            }
        } else {
            if (!next.focused) goto invalid;
            switch (e.type) {
            case NX_EVENT_KEY_DOWN: case NX_EVENT_KEY_UP:
                if (e.key <= NX_KEY_UNKNOWN || e.key >= NX_KEY_COUNT || e.repeat) goto invalid;
                if (e.type == NX_EVENT_KEY_DOWN) {
                    if (next.keys[e.key]) goto invalid;
                    next.keys[e.key] = 1; ++next.down;
                } else {
                    if (!next.keys[e.key]) goto invalid;
                    next.keys[e.key] = 0; ++next.up;
                }
                break;
            case NX_EVENT_POINTER_DOWN: case NX_EVENT_POINTER_UP:
            case NX_EVENT_POINTER_MOVE: case NX_EVENT_WHEEL:
                if (!isfinite(e.x) || !isfinite(e.y) || e.x < 0 || e.y < 0 ||
                    e.x >= next.width || e.y >= next.height ||
                    e.x != (float)(uint32_t)e.x || e.y != (float)(uint32_t)e.y) goto invalid;
                next.x=e.x; next.y=e.y;
                if (e.type == NX_EVENT_POINTER_DOWN) {
                    if (next.buttons & e.button) goto invalid;
                    next.buttons |= e.button; ++next.pointer_down;
                } else if (e.type == NX_EVENT_POINTER_UP) {
                    if (!(next.buttons & e.button)) goto invalid;
                    next.buttons &= ~e.button; ++next.pointer_up;
                } else if (e.type == NX_EVENT_POINTER_MOVE) ++next.move;
                else { if (e.delta_x != -2 || e.delta_y != 3) goto invalid; ++next.wheel; }
                break;
            default: goto invalid;
            }
        }
    }
    *r = next; return NX_OK;
invalid:
    r->bad = 1; return NX_ERROR_STATE;
}
static nx_result send(content_input *input, nx_event e, int *route, nx_event *host) {
    return content_input_event(input, &e, host, route);
}
static int start(content_input **input, receiver *r) {
    nx_event host; int route = 0; content_input_target t = target();
    r->generation = 1; r->input=input;
    return content_input_create(1, NX_KEY_F12, NX_MOD_CONTROL|NX_MOD_SHIFT, receive, r, input) == NX_OK &&
        content_input_set_target(*input, &t) == NX_OK &&
        send(*input, (nx_event){.type=NX_EVENT_FOCUS,.focused=1}, &route, &host) == NX_OK && route;
}
static int edges(void) {
    int ok = 0, route = 0; nx_event host; content_input *input = NULL; receiver r = {0};
    CHECK(start(&input, &r));
    CHECK(send(input, (nx_event){.type=NX_EVENT_KEY_DOWN,.key=NX_KEY_ENTER}, &route,&host)==NX_OK && route);
    CHECK(content_input_arm(input)==NX_ERROR_STATE && r.calls==0);
    CHECK(send(input, (nx_event){.type=NX_EVENT_KEY_UP,.key=NX_KEY_ENTER}, &route,&host)==NX_OK && route);
    CHECK(content_input_arm(input)==NX_OK && r.focus_in==1 && r.width==896 && r.height==416);
    CHECK(send(input, (nx_event){.type=NX_EVENT_KEY_DOWN,.key=NX_KEY_A}, &route,&host)==NX_OK && !route);
    CHECK(send(input, (nx_event){.type=NX_EVENT_KEY_DOWN,.key=NX_KEY_A,.repeat=1}, &route,&host)==NX_OK && !route);
    CHECK(send(input, (nx_event){.type=NX_EVENT_KEY_DOWN,.key=NX_KEY_A}, &route,&host)==NX_OK && !route);
    CHECK(send(input, (nx_event){.type=NX_EVENT_KEY_DOWN_CONSUMED,.key=NX_KEY_B}, &route,&host)==NX_OK && !route);
    CHECK(send(input, (nx_event){.type=NX_EVENT_KEY_UP,.key=NX_KEY_B}, &route,&host)==NX_OK && !route);
    for (int i=NX_EVENT_IME_PREEDIT_CHANGED; i<=NX_EVENT_TEXT_COMMAND; ++i)
        CHECK(send(input, (nx_event){.type=(nx_event_type)i}, &route,&host)==NX_OK && !route);
    CHECK(send(input, (nx_event){.type=NX_EVENT_TEXT,.scalar='a'}, &route,&host)==NX_OK && !route);
    CHECK(send(input, (nx_event){.type=NX_EVENT_IME_TRANSACTION_BEGIN}, &route,&host)==NX_OK && !route);
    CHECK(send(input, (nx_event){.type=NX_EVENT_IME_TRANSACTION_END}, &route,&host)==NX_OK && !route);
    CHECK(send(input, (nx_event){.type=NX_EVENT_POINTER_MOVE,.x=76.25f,.y=196.25f}, &route,&host)==NX_OK && !route && r.x==120 && r.y==120);
    CHECK(send(input, (nx_event){.type=NX_EVENT_POINTER_MOVE,.x=463.99f,.y=343.99f}, &route,&host)==NX_OK && !route && r.x==895 && r.y==415);
    CHECK(send(input, (nx_event){.type=NX_EVENT_POINTER_DOWN,.x=16,.y=136,.button=NX_BUTTON_X2}, &route,&host)==NX_OK && !route);
    CHECK(send(input, (nx_event){.type=NX_EVENT_POINTER_DOWN,.x=16,.y=136,.button=NX_BUTTON_X2}, &route,&host)==NX_OK && !route);
    CHECK(send(input, (nx_event){.type=NX_EVENT_WHEEL,.x=76,.y=196,.delta_x=-2,.delta_y=3}, &route,&host)==NX_OK && !route);
    CHECK(send(input, (nx_event){.type=NX_EVENT_KEY_UP,.key=NX_KEY_A}, &route,&host)==NX_OK && !route);
    CHECK(send(input, (nx_event){.type=NX_EVENT_KEY_UP,.key=NX_KEY_A}, &route,&host)==NX_OK && !route);
    CHECK(send(input, (nx_event){.type=NX_EVENT_POINTER_UP,.x=16,.y=136,.button=NX_BUTTON_X2}, &route,&host)==NX_OK && !route);
    CHECK(send(input, (nx_event){.type=NX_EVENT_POINTER_UP,.x=16,.y=136,.button=NX_BUTTON_X2}, &route,&host)==NX_OK && !route);
    CHECK(r.down==1 && r.up==1 && r.pointer_down==1 && r.pointer_up==1 && r.move==2 && r.wheel==1 && !r.bad);
    CHECK(content_input_close(&input)==NX_OK && !r.focused && r.focus_out==1);
    ok=1;
done: content_input_close(&input); return ok;
}
static int escape(void) {
    int ok=0,route=0; nx_event host; content_input *input=NULL; receiver r={0};
    CHECK(start(&input,&r) && content_input_arm(input)==NX_OK);
    nx_key keys[]={NX_KEY_A,NX_KEY_TAB,NX_KEY_CONTROL,NX_KEY_SHIFT};
    for (size_t i=0;i<4;++i) CHECK(send(input,(nx_event){.type=NX_EVENT_KEY_DOWN,.key=keys[i]},&route,&host)==NX_OK && !route);
    CHECK(send(input,(nx_event){.type=NX_EVENT_POINTER_DOWN,.x=76,.y=196,.button=NX_BUTTON_LEFT},&route,&host)==NX_OK && !route);
    CHECK(send(input,(nx_event){.type=NX_EVENT_KEY_DOWN,.key=NX_KEY_F12,.modifiers=NX_MOD_CONTROL|NX_MOD_SHIFT},&route,&host)==NX_OK && !route);
    CHECK(!r.focused && !receiver_keys(&r) && !r.buttons && r.up==4 && r.pointer_up==1 && r.focus_out==1 && r.down==4);
    CHECK(content_input_arm(input)==NX_ERROR_STATE);
    CHECK(send(input,(nx_event){.type=NX_EVENT_POINTER_MOVE,.x=5,.y=5,.modifiers=NX_MOD_CONTROL|NX_MOD_SHIFT},&route,&host)==NX_OK && route && host.modifiers==0);
    for (size_t i=0;i<4;++i) {
        CHECK(send(input,(nx_event){.type=NX_EVENT_KEY_DOWN,.key=keys[i],.repeat=1},&route,&host)==NX_OK && !route);
        CHECK(send(input,(nx_event){.type=NX_EVENT_KEY_UP,.key=keys[i]},&route,&host)==NX_OK && !route);
    }
    CHECK(send(input,(nx_event){.type=NX_EVENT_KEY_UP,.key=NX_KEY_F12},&route,&host)==NX_OK && !route);
    CHECK(send(input,(nx_event){.type=NX_EVENT_POINTER_UP,.x=76,.y=196,.button=NX_BUTTON_LEFT},&route,&host)==NX_OK && !route);
    CHECK(content_input_release(input)==NX_OK && r.focus_out==1 && r.up==4 && r.pointer_up==1);
    CHECK(send(input,(nx_event){.type=NX_EVENT_KEY_DOWN,.key=NX_KEY_ENTER},&route,&host)==NX_OK && route);
    CHECK(send(input,(nx_event){.type=NX_EVENT_KEY_UP,.key=NX_KEY_ENTER},&route,&host)==NX_OK && route);
    CHECK(content_input_arm(input)==NX_OK && r.focus_in==2);
    /* Extra modifiers do not match the configured escape chord. */
    CHECK(send(input,(nx_event){.type=NX_EVENT_KEY_DOWN,.key=NX_KEY_F12,.modifiers=NX_MOD_CONTROL|NX_MOD_SHIFT|NX_MOD_ALT},&route,&host)==NX_OK && !route && r.keys[NX_KEY_F12]);
    CHECK(content_input_close(&input)==NX_OK && !receiver_keys(&r) && r.focus_out==2 && !r.bad);
    ok=1;
done: content_input_close(&input); return ok;
}
static int revoke(void) {
    int ok=0,route=0; nx_event host; content_input *input=NULL; receiver r={0};
    CHECK(start(&input,&r));
    for (unsigned cause=0;cause<8;++cause) {
        content_input_target t=target();
        CHECK(content_input_set_target(input,&t)==NX_OK && content_input_arm(input)==NX_OK);
        CHECK(send(input,(nx_event){.type=NX_EVENT_KEY_DOWN,.key=NX_KEY_A},&route,&host)==NX_OK && !route);
        CHECK(send(input,(nx_event){.type=NX_EVENT_POINTER_DOWN,.x=76,.y=196,.button=NX_BUTTON_LEFT},&route,&host)==NX_OK && !route);
        switch (cause) {
        case 0:
            CHECK(send(input,(nx_event){.type=NX_EVENT_FOCUS,.focused=0},&route,&host)==NX_OK && route);
            CHECK(send(input,(nx_event){.type=NX_EVENT_FOCUS,.focused=1},&route,&host)==NX_OK && route);
            CHECK(send(input,(nx_event){.type=NX_EVENT_KEY_DOWN,.key=NX_KEY_A,.repeat=1},&route,&host)==NX_OK && route);
            break;
        case 1: t.available=0; CHECK(content_input_set_target(input,&t)==NX_OK); break;
        case 2: t.focused=0; CHECK(content_input_set_target(input,&t)==NX_OK); break;
        case 3: ++t.width; CHECK(content_input_set_target(input,&t)==NX_OK); break;
        case 4: t.bounds.x+=1; CHECK(content_input_set_target(input,&t)==NX_OK); break;
        case 5: CHECK(send(input,(nx_event){.type=NX_EVENT_POINTER_CANCEL},&route,&host)==NX_OK && route); break;
        case 6: CHECK(send(input,(nx_event){.type=NX_EVENT_POINTER_MOVE,.x=464,.y=136},&route,&host)==NX_OK && route); break;
        default:
            /* A fresh outside right click belongs to host, including its up. */
            CHECK(send(input,(nx_event){.type=NX_EVENT_POINTER_DOWN,.x=5,.y=5,.button=NX_BUTTON_RIGHT},&route,&host)==NX_OK && route);
            CHECK(send(input,(nx_event){.type=NX_EVENT_POINTER_UP,.x=5,.y=5,.button=NX_BUTTON_RIGHT},&route,&host)==NX_OK && route);
            break;
        }
        CHECK(!r.focused && !receiver_keys(&r) && !r.buttons && r.focus_out==cause+1u);
        CHECK(send(input,(nx_event){.type=NX_EVENT_KEY_UP,.key=NX_KEY_A},&route,&host)==NX_OK);
        CHECK(route==(cause==0));
        CHECK(send(input,(nx_event){.type=NX_EVENT_POINTER_UP,.x=76,.y=196,.button=NX_BUTTON_LEFT},&route,&host)==NX_OK);
        CHECK(route==(cause==0 || cause==5));
    }
    CHECK(r.down==8 && r.up==8 && r.pointer_down==8 && r.pointer_up==8 && !r.bad);
    /* Focus gain is not an arm, and a post-focus repeat cannot invent a hold. */
    CHECK(send(input,(nx_event){.type=NX_EVENT_KEY_DOWN,.key=NX_KEY_B},&route,&host)==NX_OK && route);
    CHECK(content_input_close(&input)==NX_OK && r.focus_in==8 && r.focus_out==8);
    ok=1;
done: content_input_close(&input); return ok;
}
static int identity(void) {
    int ok=0,route=0; nx_event host; content_input *input=NULL; receiver r={0}; content_input_status s;
    CHECK(content_input_create(0,NX_KEY_F12,0,receive,&r,&input)==NX_ERROR_ARGUMENT && !input);
    CHECK(content_input_create(1,NX_KEY_CONTROL,0,receive,&r,&input)==NX_ERROR_ARGUMENT && !input);
    CHECK(content_input_create(1,NX_KEY_F12,16u,receive,&r,&input)==NX_ERROR_ARGUMENT && !input);
    CHECK(start(&input,&r));
    r.reenter=1; CHECK(content_input_arm(input)==NX_OK);
    for (size_t i=0;i<6;++i) CHECK(r.nested[i]==NX_ERROR_STATE);
    CHECK(content_input_rebind(input,2)==NX_ERROR_STATE && r.calls==1);
    CHECK(send(input,(nx_event){.type=NX_EVENT_KEY_DOWN,.key=NX_KEY_UNKNOWN},&route,&host)==NX_ERROR_UNSUPPORTED && !route && r.focus_out==1);
    CHECK(content_input_rebind(input,1)==NX_ERROR_STATE);
    CHECK(content_input_rebind(input,0)==NX_ERROR_ARGUMENT);
    CHECK(content_input_rebind(input,2)==NX_OK); r.generation=2;
    CHECK(content_input_arm(input)==NX_ERROR_STATE); /* Explicit target required. */
    content_input_target t=target(); CHECK(content_input_set_target(input,&t)==NX_OK);
    t.bounds.width=NAN; CHECK(content_input_set_target(input,&t)==NX_ERROR_ARGUMENT);
    t=target(); t.width=UINT32_MAX; CHECK(content_input_set_target(input,&t)==NX_ERROR_ARGUMENT);
    CHECK(content_input_arm(input)==NX_OK && r.focus_in==2);
    unsigned calls=r.calls;
    CHECK(send(input,(nx_event){.type=NX_EVENT_POINTER_MOVE,.x=NAN},&route,&host)==NX_ERROR_ARGUMENT && !route && r.calls==calls);
    CHECK(send(input,(nx_event){.type=NX_EVENT_WHEEL,.x=76,.y=196,.delta_y=INFINITY},&route,&host)==NX_ERROR_ARGUMENT && !route && r.calls==calls);
    CHECK(send(input,(nx_event){.type=NX_EVENT_POINTER_DOWN,.x=76,.y=196,.button=3},&route,&host)==NX_ERROR_ARGUMENT && !route && r.calls==calls);
    CHECK(send(input,(nx_event){.type=NX_EVENT_KEY_DOWN,.key=(nx_key)NX_KEY_COUNT},&route,&host)==NX_ERROR_ARGUMENT && !route && r.calls==calls);
    CHECK(send(input,(nx_event){.type=NX_EVENT_TOUCH_DOWN},&route,&host)==NX_ERROR_UNSUPPORTED && !route && r.focus_out==2);
    CHECK(content_input_get_status(input,&s)==NX_OK && s.generation==2 && !s.active && !s.fenced);
    /* The largest possible reset batch must release every distinct hold. */
    CHECK(content_input_arm(input)==NX_OK);
    for(int k=1;k<NX_KEY_COUNT;++k)
        CHECK(send(input,(nx_event){.type=NX_EVENT_KEY_DOWN,.key=(nx_key)k},&route,&host)==NX_OK && !route);
    for(uint32_t b=1;b<=NX_BUTTON_X2;b<<=1)
        CHECK(send(input,(nx_event){.type=NX_EVENT_POINTER_DOWN,.button=b,.x=76,.y=196},&route,&host)==NX_OK && !route);
    CHECK(receiver_keys(&r)==NX_KEY_COUNT-1 && r.buttons==31u);
    CHECK(content_input_release(input)==NX_OK && !receiver_keys(&r) && !r.buttons && r.focus_out==3);
    CHECK(content_input_close(&input)==NX_OK && !r.bad);
    CHECK(content_input_close(&input)==NX_OK);
    ok=1;
done: content_input_close(&input); return ok;
}
static int failure(void) {
    int ok=0,route=0; nx_event host; content_input *input=NULL; receiver r={0}; content_input_status s;
    CHECK(start(&input,&r) && content_input_arm(input)==NX_OK);
    CHECK(send(input,(nx_event){.type=NX_EVENT_KEY_DOWN,.key=NX_KEY_A},&route,&host)==NX_OK);
    CHECK(send(input,(nx_event){.type=NX_EVENT_POINTER_DOWN,.x=76,.y=196,.button=NX_BUTTON_LEFT},&route,&host)==NX_OK);
    r.fail=1; CHECK(content_input_release(input)==NX_ERROR_LIMIT);
    CHECK(content_input_get_status(input,&s)==NX_OK && !s.active && s.fenced && s.last_error==NX_ERROR_LIMIT && s.held_keys==1 && s.held_buttons==NX_BUTTON_LEFT);
    CHECK(r.keys[NX_KEY_A] && r.buttons==NX_BUTTON_LEFT && r.focused && !r.focus_out);
    unsigned calls=r.calls;
    CHECK(content_input_release(input)==NX_ERROR_LIMIT && content_input_arm(input)==NX_ERROR_STATE && r.calls==calls);
    CHECK(send(input,(nx_event){.type=NX_EVENT_KEY_UP,.key=NX_KEY_A},&route,&host)==NX_OK && !route);
    CHECK(send(input,(nx_event){.type=NX_EVENT_POINTER_UP,.x=76,.y=196,.button=NX_BUTTON_LEFT},&route,&host)==NX_OK && !route);
    CHECK(content_input_get_status(input,&s)==NX_OK && s.held_keys==1 && !s.physical_keys && !s.physical_buttons && r.calls==calls);
    /* Independently retire the fixture receiver; rebind is the owner's assertion. */
    memset(r.keys,0,sizeof(r.keys)); r.buttons=0; r.focused=0; r.generation=2;
    CHECK(content_input_rebind(input,2)==NX_OK);
    content_input_target t=target(); CHECK(content_input_set_target(input,&t)==NX_OK && content_input_arm(input)==NX_OK);
    CHECK(send(input,(nx_event){.type=NX_EVENT_KEY_DOWN,.key=NX_KEY_B},&route,&host)==NX_OK);
    r.fail=1;
    CHECK(send(input,(nx_event){.type=NX_EVENT_KEY_UP,.key=NX_KEY_B},&route,&host)==NX_ERROR_LIMIT && !route);
    CHECK(content_input_get_status(input,&s)==NX_OK && s.fenced && !s.active && s.held_keys==1 && r.keys[NX_KEY_B]);
    calls=r.calls; CHECK(content_input_close(&input)==NX_ERROR_LIMIT && !input && r.calls==calls);
    CHECK(start(&input,&r)); /* A fresh receiver below needs its own reset. */
    memset(r.keys,0,sizeof(r.keys)); r.buttons=0; r.focused=0;
    r.fail=1; CHECK(content_input_arm(input)==NX_ERROR_LIMIT);
    CHECK(content_input_get_status(input,&s)==NX_OK && s.fenced && !s.held_keys && !s.active);
    CHECK(content_input_close(&input)==NX_ERROR_LIMIT && !input && !r.bad);
    /* Close with active held input attempts exactly one failed reset then frees. */
    memset(&r,0,sizeof(r)); CHECK(start(&input,&r) && content_input_arm(input)==NX_OK);
    CHECK(send(input,(nx_event){.type=NX_EVENT_KEY_DOWN,.key=NX_KEY_C},&route,&host)==NX_OK);
    calls=r.calls; r.fail=1;
    CHECK(content_input_close(&input)==NX_ERROR_LIMIT && !input && r.calls==calls+1u && r.keys[NX_KEY_C] && !r.bad);
    ok=1;
done: content_input_close(&input); return ok;
}
int main(int argc, char **argv) {
    if (argc!=2) return 2;
    int ok=0;
    if (!strcmp(argv[1],"edges")) ok=edges();
    else if (!strcmp(argv[1],"escape")) ok=escape();
    else if (!strcmp(argv[1],"revoke")) ok=revoke();
    else if (!strcmp(argv[1],"identity")) ok=identity();
    else if (!strcmp(argv[1],"failure")) ok=failure();
    else return 2;
    if (ok) printf("content input %s passed\n",argv[1]);
    return ok ? 0 : 1;
}
