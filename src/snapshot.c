#include "nexaui/nexaui.h"
#include "snapshot_lock.h"

#include <stdint.h>
#include <stdlib.h>

typedef struct nx_snapshot_slot {
    void *data;
    size_t bytes;
    nx_snapshot_dispose_fn dispose;
    void *dispose_user;
    uint64_t generation;
} nx_snapshot_slot;

struct nx_snapshot_exchange {
    nx_allocator allocator;
    nx_snapshot_lock *lock;
    nx_snapshot_slot slots[2];
    uint64_t next_ticket;
    unsigned current, viewed;
    int has_current, view_open;
};

static void *nx_snapshot_allocate(void *user, size_t size) {
    (void)user;
    return malloc(size);
}

static void nx_snapshot_deallocate(void *user, void *pointer, size_t size) {
    (void)user;
    (void)size;
    free(pointer);
}

static void nx_snapshot_dispose_slot(nx_snapshot_slot slot) {
    if (slot.data) slot.dispose(slot.dispose_user, slot.data, slot.bytes);
}

nx_result nx_snapshot_exchange_create(const nx_allocator *allocator,
                                      nx_snapshot_exchange **out_exchange) {
    if (!out_exchange) return NX_ERROR_ARGUMENT;
    *out_exchange = NULL;
    nx_allocator a = allocator ? *allocator : (nx_allocator){0};
    if ((a.allocate == NULL) != (a.deallocate == NULL))
        return NX_ERROR_ARGUMENT;
    if (!a.allocate) {
        a = (nx_allocator){NULL, nx_snapshot_allocate, nx_snapshot_deallocate};
    }
    nx_snapshot_exchange *exchange = a.allocate(a.user, sizeof(*exchange));
    if (!exchange) return NX_ERROR_MEMORY;
    *exchange = (nx_snapshot_exchange){0};
    exchange->allocator = a;
    nx_result result = nx_snapshot_lock_create(&a, &exchange->lock);
    if (result != NX_OK) {
        a.deallocate(a.user, exchange, sizeof(*exchange));
        return result;
    }
    *out_exchange = exchange;
    return NX_OK;
}

nx_result nx_snapshot_exchange_destroy(nx_snapshot_exchange *exchange) {
    if (!exchange) return NX_ERROR_ARGUMENT;
    /* Shutdown is exclusive: no other operation may start or be in flight. */
    nx_snapshot_lock_enter(exchange->lock);
    if (exchange->view_open) {
        nx_snapshot_lock_leave(exchange->lock);
        return NX_ERROR_STATE;
    }
    nx_snapshot_slot first = exchange->slots[0];
    nx_snapshot_slot second = exchange->slots[1];
    nx_snapshot_lock_leave(exchange->lock);
    nx_snapshot_lock_destroy(exchange->lock);
    nx_allocator a = exchange->allocator;
    a.deallocate(a.user, exchange, sizeof(*exchange));
    nx_snapshot_dispose_slot(first);
    nx_snapshot_dispose_slot(second);
    return NX_OK;
}

nx_result nx_snapshot_exchange_publish(nx_snapshot_exchange *exchange,
                                       void *data, size_t bytes,
                                       nx_snapshot_dispose_fn dispose,
                                       void *dispose_user) {
    if (!exchange || !data || !dispose) return NX_ERROR_ARGUMENT;
    nx_snapshot_lock_enter(exchange->lock);
    if (exchange->slots[0].data == data || exchange->slots[1].data == data) {
        nx_snapshot_lock_leave(exchange->lock);
        return NX_ERROR_ARGUMENT;
    }
    unsigned target = !exchange->has_current ? 0u :
                      exchange->view_open && exchange->viewed == exchange->current ?
                      1u - exchange->current : exchange->current;
    nx_snapshot_slot *slot = &exchange->slots[target];
    if ((exchange->view_open && exchange->viewed == target) ||
        slot->generation == UINT64_MAX) {
        nx_snapshot_lock_leave(exchange->lock);
        return NX_ERROR_LIMIT;
    }
    nx_snapshot_slot retired = *slot;
    *slot = (nx_snapshot_slot){data, bytes, dispose, dispose_user,
                               retired.generation + 1u};
    exchange->current = target;
    exchange->has_current = 1;
    nx_snapshot_lock_leave(exchange->lock);
    nx_snapshot_dispose_slot(retired);
    return NX_OK;
}

nx_result nx_snapshot_exchange_acquire(nx_snapshot_exchange *exchange,
                                       nx_snapshot_view *out_view) {
    if (!out_view) return NX_ERROR_ARGUMENT;
    *out_view = (nx_snapshot_view){0};
    if (!exchange) return NX_ERROR_ARGUMENT;
    nx_snapshot_lock_enter(exchange->lock);
    if (exchange->view_open || exchange->next_ticket == UINT64_MAX) {
        nx_result result = exchange->view_open ? NX_ERROR_STATE : NX_ERROR_LIMIT;
        nx_snapshot_lock_leave(exchange->lock);
        return result;
    }
    if (exchange->has_current) {
        unsigned slot = exchange->current;
        exchange->view_open = 1;
        exchange->viewed = slot;
        ++exchange->next_ticket;
        *out_view = (nx_snapshot_view){exchange->slots[slot].data,
                     exchange->slots[slot].bytes, exchange,
                     exchange->slots[slot].generation,
                     exchange->next_ticket, slot};
    }
    nx_snapshot_lock_leave(exchange->lock);
    return NX_OK;
}

nx_result nx_snapshot_exchange_release(nx_snapshot_exchange *exchange,
                                       nx_snapshot_view *view) {
    if (!exchange || !view) return NX_ERROR_ARGUMENT;
    nx_snapshot_lock_enter(exchange->lock);
    if (!exchange->view_open || view->owner != exchange ||
        view->slot != exchange->viewed || view->slot > 1u ||
        view->ticket != exchange->next_ticket ||
        view->generation != exchange->slots[view->slot].generation ||
        view->data != exchange->slots[view->slot].data ||
        view->bytes != exchange->slots[view->slot].bytes) {
        nx_snapshot_lock_leave(exchange->lock);
        return NX_ERROR_STATE;
    }
    unsigned slot = exchange->viewed;
    exchange->view_open = 0;
    nx_snapshot_slot retired = {0};
    if (slot != exchange->current) {
        retired = exchange->slots[slot];
        exchange->slots[slot] = (nx_snapshot_slot){0};
        exchange->slots[slot].generation = retired.generation;
    }
    *view = (nx_snapshot_view){0};
    nx_snapshot_lock_leave(exchange->lock);
    nx_snapshot_dispose_slot(retired);
    return NX_OK;
}
