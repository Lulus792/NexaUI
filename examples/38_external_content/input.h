#ifndef EXAMPLE_CONTENT_INPUT_H
#define EXAMPLE_CONTENT_INPUT_H
#include <nexaui/nexaui.h>

typedef struct content_input content_input;
/* Example-local, owner-thread physical-input channel. The callback atomically
   copies/adopts the WHOLE batch or none, never retains events or reenters any
   mutator. NX_OK means local admission, NOT remote application/reset evidence.
   Normalized keys are not scancodes; wheel units remain abstract scroll units.
   No text/IME, touch, relative pointer, OS grab or engine protocol is provided. */
typedef nx_result (*content_input_sink)(void *user, uint64_t generation,
    const nx_event *events, size_t count);
typedef struct content_input_target {
    nx_rect bounds; /* Host logical coordinates, half-open. */
    uint32_t width, height; /* Source pixels, <= 2^24 for exact float integers. */
    int available, focused; /* Controlled by the host, never guest messages. */
} content_input_target;
typedef struct content_input_status {
    uint64_t generation;
    size_t held_keys, physical_keys, draining_keys;
    uint32_t held_buttons, physical_buttons, draining_buttons;
    int active, window_focused, fenced;
    nx_result last_error;
} content_input_status;

/* Borrow callback/user until close. Uses one bounded calloc allocation.
   Escape requires a nonmodifier key and exact known modifier mask. The caller
   must pick a platform-usable shortcut and present it before activation. */
nx_result content_input_create(uint64_t generation, nx_key escape_key,
    uint32_t escape_modifiers, content_input_sink sink, void *user,
    content_input **out);
/* A geometry, source-availability or host-focus change revokes active input.
   Supply the actual content focus after host frame/AX actions, before polling
   more native input. Frame arrival or native focus gain never arms the gate. */
nx_result content_input_set_target(content_input *input, const content_input_target *target);
/* Explicit host intent only; all observed keys/buttons must be released, the
   native window and content must be focused, and the source available. Sends
   FocusIn with pixel width/height, then enters active state only on admission. */
nx_result content_input_arm(content_input *input);
/* Feed each normalized native event ONCE, in queue order, even when inactive.
   On success queue *host_event only if *route_host is true. The copy removes
   modifiers still draining from revoked guest input. Output guest pointer x/y
   are floor-scaled, clamped integer source pixels. Repeat edges and unpaired
   ups never reach the guest. Active text/IME events are consumed, not forwarded.
   Leaving bounds revokes absolute-pointer input; this is not an OS capture.
   Unsupported physical keys/touch revoke and return UNSUPPORTED; the owner
   must report that capability gap. Invalid data returns ARGUMENT.
   Any error leaves route_host false: the owner must handle it, not retry/replay
   the native event. Normal native focus/resize/wake events still reach host. */
nx_result content_input_event(content_input *input, const nx_event *event,
    nx_event *host_event, int *route_host);
/* Releases every locally admitted hold plus FocusOut in one batch, once. On
   callback failure the gate fences, preserving the uncertain held ledger.
   Later native releases drain without new guest or host actions. No retries. */
nx_result content_input_release(content_input *input);
/* Explicit owner assertion: old backend retired/reset independently. Requires
   inactive state and strictly newer generation. Drops local uncertainty only;
   never proves remote reset. Physical/draining state is preserved until up. */
nx_result content_input_rebind(content_input *input, uint64_t generation);
nx_result content_input_get_status(const content_input *input, content_input_status *out);
/* Attempts one active release then frees, also on failure. Error means backend
   release remains uncertain and must be handled by the external owner. A
   reentrant close returns STATE and preserves the object. NULL is idempotent. */
nx_result content_input_close(content_input **input);
#endif
