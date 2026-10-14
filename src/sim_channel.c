#include "nexaui/nexaui.h"
#include "snapshot_lock.h"

#include <stdint.h>
#include <stdlib.h>

typedef struct nx_sim_entry {
    nx_sim_message message;
    size_t pending_index;
} nx_sim_entry;

typedef struct nx_sim_pending {
    uint64_t ticket;
    unsigned state; /* 0 free, 1 queued, 2 taken, 3 completed. */
} nx_sim_pending;

struct nx_sim_channel {
    nx_allocator allocator;
    nx_snapshot_lock *lock;
    size_t capacity;
    nx_sim_entry *commands, *completions;
    nx_sim_pending *pending;
    size_t command_head, command_count;
    size_t completion_head, completion_count;
    size_t outstanding;
    uint64_t next_ticket;
};

static void *nx_sim_allocate(void *user, size_t size) {
    (void)user;
    return malloc(size);
}

static void nx_sim_deallocate(void *user, void *data, size_t size) {
    (void)user;
    (void)size;
    free(data);
}

void nx_sim_message_dispose(nx_sim_message *message) {
    if (!message) return;
    nx_sim_message old = *message;
    *message = (nx_sim_message){0};
    if (old.data && old.dispose)
        old.dispose(old.dispose_user, old.data, old.bytes);
}

nx_result nx_sim_channel_create(size_t capacity,
                                const nx_allocator *allocator,
                                nx_sim_channel **out_channel) {
    if (!out_channel) return NX_ERROR_ARGUMENT;
    *out_channel = NULL;
    if (!capacity || capacity > SIZE_MAX / sizeof(nx_sim_entry) ||
        capacity > SIZE_MAX / sizeof(nx_sim_pending)) return NX_ERROR_LIMIT;
    nx_allocator a = allocator ? *allocator : (nx_allocator){0};
    if ((a.allocate == NULL) != (a.deallocate == NULL))
        return NX_ERROR_ARGUMENT;
    if (!a.allocate)
        a = (nx_allocator){NULL, nx_sim_allocate, nx_sim_deallocate};
    nx_sim_channel *channel = a.allocate(a.user, sizeof(*channel));
    if (!channel) return NX_ERROR_MEMORY;
    *channel = (nx_sim_channel){0};
    channel->allocator = a;
    channel->capacity = capacity;
    nx_result result = NX_ERROR_MEMORY;
    channel->commands = a.allocate(a.user, capacity * sizeof(nx_sim_entry));
    if (!channel->commands) goto fail;
    channel->completions = a.allocate(a.user, capacity * sizeof(nx_sim_entry));
    if (!channel->completions) goto fail;
    channel->pending = a.allocate(a.user, capacity * sizeof(nx_sim_pending));
    if (!channel->pending) goto fail;
    for (size_t i = 0; i < capacity; ++i)
        channel->pending[i] = (nx_sim_pending){0};
    result = nx_snapshot_lock_create(&a, &channel->lock);
    if (result != NX_OK) goto fail;
    *out_channel = channel;
    return NX_OK;

fail:
    if (channel->pending)
        a.deallocate(a.user, channel->pending,
                     capacity * sizeof(nx_sim_pending));
    if (channel->completions)
        a.deallocate(a.user, channel->completions,
                     capacity * sizeof(nx_sim_entry));
    if (channel->commands)
        a.deallocate(a.user, channel->commands,
                     capacity * sizeof(nx_sim_entry));
    a.deallocate(a.user, channel, sizeof(*channel));
    return result;
}

nx_result nx_sim_channel_destroy(nx_sim_channel *channel) {
    if (!channel) return NX_ERROR_ARGUMENT;
    /* All users must have stopped; only queued/posted messages remain. */
    nx_snapshot_lock_enter(channel->lock);
    for (size_t i = 0; i < channel->capacity; ++i) {
        if (channel->pending[i].state == 2u) {
            nx_snapshot_lock_leave(channel->lock);
            return NX_ERROR_STATE;
        }
    }
    nx_snapshot_lock_leave(channel->lock);
    nx_snapshot_lock_destroy(channel->lock);
    for (size_t i = 0; i < channel->command_count; ++i) {
        size_t index = (channel->command_head + i) % channel->capacity;
        nx_sim_message_dispose(&channel->commands[index].message);
    }
    for (size_t i = 0; i < channel->completion_count; ++i) {
        size_t index = (channel->completion_head + i) % channel->capacity;
        nx_sim_message_dispose(&channel->completions[index].message);
    }
    nx_allocator a = channel->allocator;
    size_t capacity = channel->capacity;
    a.deallocate(a.user, channel->pending,
                 capacity * sizeof(nx_sim_pending));
    a.deallocate(a.user, channel->completions,
                 capacity * sizeof(nx_sim_entry));
    a.deallocate(a.user, channel->commands,
                 capacity * sizeof(nx_sim_entry));
    a.deallocate(a.user, channel, sizeof(*channel));
    return NX_OK;
}

nx_result nx_sim_channel_post_command(nx_sim_channel *channel,
                                      void *data, size_t bytes,
                                      nx_snapshot_dispose_fn dispose,
                                      void *dispose_user,
                                      uint64_t *out_ticket) {
    if (!out_ticket) return NX_ERROR_ARGUMENT;
    *out_ticket = 0;
    if (!channel || !data || !dispose) return NX_ERROR_ARGUMENT;
    nx_snapshot_lock_enter(channel->lock);
    if (channel->outstanding == channel->capacity ||
        channel->next_ticket == UINT64_MAX) {
        nx_snapshot_lock_leave(channel->lock);
        return NX_ERROR_LIMIT;
    }
    size_t slot = 0;
    while (slot < channel->capacity && channel->pending[slot].state) ++slot;
    if (slot == channel->capacity) {
        nx_snapshot_lock_leave(channel->lock);
        return NX_ERROR_STATE;
    }
    uint64_t ticket = ++channel->next_ticket;
    channel->pending[slot] = (nx_sim_pending){ticket, 1u};
    size_t index = (channel->command_head + channel->command_count) %
                   channel->capacity;
    channel->commands[index] = (nx_sim_entry){
        {ticket, NX_OK, data, bytes, dispose, dispose_user}, slot};
    ++channel->command_count;
    ++channel->outstanding;
    *out_ticket = ticket;
    nx_snapshot_lock_leave(channel->lock);
    return NX_OK;
}

nx_result nx_sim_channel_take_command(nx_sim_channel *channel,
                                      nx_sim_message *out_message) {
    if (!out_message) return NX_ERROR_ARGUMENT;
    *out_message = (nx_sim_message){0};
    if (!channel) return NX_ERROR_ARGUMENT;
    nx_snapshot_lock_enter(channel->lock);
    if (channel->command_count) {
        nx_sim_entry entry = channel->commands[channel->command_head];
        channel->command_head = (channel->command_head + 1u) % channel->capacity;
        --channel->command_count;
        channel->pending[entry.pending_index].state = 2u;
        *out_message = entry.message;
    }
    nx_snapshot_lock_leave(channel->lock);
    return NX_OK;
}

nx_result nx_sim_channel_post_completion(nx_sim_channel *channel,
                                         uint64_t ticket, nx_result status,
                                         void *data, size_t bytes,
                                         nx_snapshot_dispose_fn dispose,
                                         void *dispose_user) {
    if (!channel || !ticket || status < NX_OK || status > NX_ERROR_PLATFORM ||
        (data == NULL && (bytes || dispose)) ||
        (data != NULL && !dispose)) return NX_ERROR_ARGUMENT;
    nx_snapshot_lock_enter(channel->lock);
    size_t slot = 0;
    while (slot < channel->capacity &&
           channel->pending[slot].ticket != ticket) ++slot;
    if (slot == channel->capacity || channel->pending[slot].state != 2u) {
        nx_snapshot_lock_leave(channel->lock);
        return NX_ERROR_STATE;
    }
    if (channel->completion_count == channel->capacity) {
        nx_snapshot_lock_leave(channel->lock);
        return NX_ERROR_LIMIT;
    }
    size_t index = (channel->completion_head + channel->completion_count) %
                   channel->capacity;
    channel->completions[index] = (nx_sim_entry){
        {ticket, status, data, bytes, dispose, dispose_user}, slot};
    channel->pending[slot].state = 3u;
    ++channel->completion_count;
    nx_snapshot_lock_leave(channel->lock);
    return NX_OK;
}

nx_result nx_sim_channel_take_completion(nx_sim_channel *channel,
                                         nx_sim_message *out_message) {
    if (!out_message) return NX_ERROR_ARGUMENT;
    *out_message = (nx_sim_message){0};
    if (!channel) return NX_ERROR_ARGUMENT;
    nx_snapshot_lock_enter(channel->lock);
    if (channel->completion_count) {
        nx_sim_entry entry = channel->completions[channel->completion_head];
        channel->completion_head = (channel->completion_head + 1u) %
                                   channel->capacity;
        --channel->completion_count;
        channel->pending[entry.pending_index] = (nx_sim_pending){0};
        --channel->outstanding;
        *out_message = entry.message;
    }
    nx_snapshot_lock_leave(channel->lock);
    return NX_OK;
}
