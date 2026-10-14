#include <pthread.h>

#include "../../src/snapshot_lock.h"

struct nx_snapshot_lock {
    nx_allocator allocator;
    pthread_mutex_t native;
};

nx_result nx_snapshot_lock_create(const nx_allocator *allocator,
                                  nx_snapshot_lock **out_lock) {
    *out_lock = NULL;
    nx_snapshot_lock *lock = allocator->allocate(allocator->user, sizeof(*lock));
    if (!lock) return NX_ERROR_MEMORY;
    lock->allocator = *allocator;
    if (pthread_mutex_init(&lock->native, NULL) != 0) {
        allocator->deallocate(allocator->user, lock, sizeof(*lock));
        return NX_ERROR_PLATFORM;
    }
    *out_lock = lock;
    return NX_OK;
}

void nx_snapshot_lock_destroy(nx_snapshot_lock *lock) {
    nx_allocator a = lock->allocator;
    pthread_mutex_destroy(&lock->native);
    a.deallocate(a.user, lock, sizeof(*lock));
}

void nx_snapshot_lock_enter(nx_snapshot_lock *lock) {
    pthread_mutex_lock(&lock->native);
}

void nx_snapshot_lock_leave(nx_snapshot_lock *lock) {
    pthread_mutex_unlock(&lock->native);
}
