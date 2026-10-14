#ifndef NEXAUI_SNAPSHOT_LOCK_H
#define NEXAUI_SNAPSHOT_LOCK_H

#include "nexaui/nexaui.h"

typedef struct nx_snapshot_lock nx_snapshot_lock;

nx_result nx_snapshot_lock_create(const nx_allocator *allocator,
                                  nx_snapshot_lock **out_lock);
void nx_snapshot_lock_destroy(nx_snapshot_lock *lock);
void nx_snapshot_lock_enter(nx_snapshot_lock *lock);
void nx_snapshot_lock_leave(nx_snapshot_lock *lock);

#endif
