#ifndef NEXAUI_CLOSE_REQUEST_H
#define NEXAUI_CLOSE_REQUEST_H
#include "nexaui/nexaui.h"

/* Owner-thread state. Notifications supersede older pending requests without
   an allocation or unbounded queue. Terminal conditions can never be revived. */
typedef struct nx_close_state {
    uint64_t sequence, pending;
    int taken, terminal;
} nx_close_state;
static inline void nx_close_force(nx_close_state *state,int *legacy) {
    state->pending=0;state->taken=0;state->terminal=1;*legacy=1;
}
static inline void nx_close_notify(nx_close_state *state,int *legacy) {
    if(state->terminal)return;
    if(state->sequence==UINT64_MAX){nx_close_force(state,legacy);return;}
    state->pending=++state->sequence;state->taken=0;*legacy=1;
}
static inline uint64_t nx_close_take(nx_close_state *state,int *legacy) {
    if(state->terminal||!state->pending||state->taken)return 0;
    state->taken=1;*legacy=0;return state->pending;
}
static inline nx_result nx_close_resolve(nx_close_state *state,int *legacy,
    uint64_t id,nx_window_close_action action) {
    if(action!=NX_WINDOW_CLOSE_CANCEL&&action!=NX_WINDOW_CLOSE_ACCEPT)return NX_ERROR_ARGUMENT;
    if(state->terminal||!id||id!=state->pending||!state->taken)return NX_ERROR_STATE;
    state->pending=0;state->taken=0;
    if(action==NX_WINDOW_CLOSE_ACCEPT)nx_close_force(state,legacy);else *legacy=0;
    return NX_OK;
}
#endif
