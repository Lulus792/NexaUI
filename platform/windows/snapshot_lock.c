#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "../../src/snapshot_lock.h"

struct nx_snapshot_lock {
    nx_allocator allocator;
    SRWLOCK native;
};

nx_result nx_snapshot_lock_create(const nx_allocator *allocator,
                                  nx_snapshot_lock **out_lock) {
    *out_lock = NULL;
    nx_snapshot_lock *lock = allocator->allocate(allocator->user, sizeof(*lock));
    if (!lock) return NX_ERROR_MEMORY;
    lock->allocator = *allocator;
    InitializeSRWLock(&lock->native);
    *out_lock = lock;
    return NX_OK;
}

void nx_snapshot_lock_destroy(nx_snapshot_lock *lock) {
    nx_allocator a = lock->allocator;
    a.deallocate(a.user, lock, sizeof(*lock));
}

void nx_snapshot_lock_enter(nx_snapshot_lock *lock) {
    AcquireSRWLockExclusive(&lock->native);
}

void nx_snapshot_lock_leave(nx_snapshot_lock *lock) {
    ReleaseSRWLockExclusive(&lock->native);
}
