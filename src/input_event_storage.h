#ifndef NEXAUI_PRIVATE_INPUT_EVENT_STORAGE_H
#define NEXAUI_PRIVATE_INPUT_EVENT_STORAGE_H

#include "nexaui/nexaui.h"
#include "secure_zero.h"

/* Native queues cannot know which field will receive these records. Wipe the
   stored copies of keyboard and text-system events at the point of retirement. */
static void nx_private_scrub_input_event(nx_event *event) {
    switch (event->type) {
    case NX_EVENT_KEY_DOWN:
    case NX_EVENT_KEY_UP:
    case NX_EVENT_KEY_DOWN_CONSUMED:
    case NX_EVENT_TEXT:
    case NX_EVENT_TEXT_COMMAND:
    case NX_EVENT_IME_PREEDIT_CHANGED:
    case NX_EVENT_IME_DELETE_SURROUNDING:
    case NX_EVENT_IME_DELETE_RANGE:
    case NX_EVENT_IME_TRANSACTION_BEGIN:
    case NX_EVENT_IME_TRANSACTION_END:
        nx_private_secure_zero(event, sizeof(*event));
        break;
    default:
        break;
    }
}

#endif
