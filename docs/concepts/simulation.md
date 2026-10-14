# Simulation snapshot handoff

`nx_snapshot_exchange` transfers immutable, application-allocated data from one
simulation producer to one UI consumer. It stores pointers and byte lengths;
it never copies the snapshot contents. The producer completes all writes before
`nx_snapshot_exchange_publish`. A successful publish transfers ownership to
the exchange. A failed publish leaves ownership with the caller. Each snapshot
has a disposer, called once after it is no longer current and no UI view holds
it. The disposer can run on the producer or UI thread and must not call back
into the same exchange.

The UI calls `nx_snapshot_exchange_acquire` before reading a frame and
`nx_snapshot_exchange_release` after it has finished using the data. A zero
view means nothing has been published yet. Only one view may be open at once;
do not copy or modify its fields. Holding a view pins its payload even while
the producer publishes newer data. An unviewed current payload may be replaced
and disposed immediately, so intermediate simulation steps can be dropped
instead of delaying the producer. The exchange has two metadata slots and
does not allocate per publish or acquire.
Payloads being disposed outside the lock can briefly coexist with both slot
payloads.

```c
static void dispose_snapshot(void *user, void *data, size_t bytes) {
    (void)user;
    (void)bytes;
    free(data);
}

/* Simulation thread: fill data completely before publishing it. */
sample *data = malloc(sizeof(*data));
if (data) {
    fill_sample(data);
    if (nx_snapshot_exchange_publish(exchange, data, sizeof(*data),
                                     dispose_snapshot, NULL) != NX_OK)
        free(data);
}

/* UI thread: keep the view through command generation. */
nx_snapshot_view view = {0};
if (nx_snapshot_exchange_acquire(exchange, &view) == NX_OK) {
    if (view.data) draw_sample((const sample *)view.data);
    if (view.data) nx_snapshot_exchange_release(exchange, &view);
}
```

Stop and join the producer before calling `nx_snapshot_exchange_destroy`.
Destroy returns `NX_ERROR_STATE` while a view is open and leaves the exchange
usable. Other calls must not overlap a successful destroy. The library locks
only the two slot descriptors and view token. It never holds that lock while
the UI builds a frame or while a disposer runs. Windows uses an SRW lock;
macOS and Linux use a POSIX mutex behind private platform files. The public
header has no native lock type.

The current test holds a view across 20,000 producer publications, checks
exactly-once disposal and stale-view rejection, and exercises concurrent
acquire/release loops. It ran on Windows with MSVC. A thread sanitizer run and
native macOS/Linux runs remain part of cross-platform acceptance.

For UI-to-simulation requests and their results, use the
[bounded command channel](sim-channel.md).
