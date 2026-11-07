# Original Files model worker for a file picker

This is the data-owner foundation for an ordinary file-picker composition.
The native dialog, text/IME controls, accessible list/breadcrumb UI and actual
Aetheri adoption remain to be implemented and accepted. It does not replace the
existing Files app, create another Files model or turn a GUI selection into a
grant, StorageRef, installation or privileged authority receipt.

The external Linux C17 consumer builds the original Files model and its original
UTF-8/grapheme support from immutable Aetheri
`27de5cb228da0f37264fad64d964ad7a660d210d`. Five hashed source/header/data inputs
must match `source-inputs.json`. The original app's exact SDK selection, UI,
launchers and live sources are unchanged. Neither this optional application
model nor its text validation becomes a dependency of NexaUI's own core stack.

```sh
cmake -S examples/32_aetheri_file_picker -B build-picker \
  -DCMAKE_PREFIX_PATH=/path/to/installed-nexaui \
  -DAETHERI_SOURCE_DIR=/path/to/immutable-aetheri \
  -DCMAKE_BUILD_TYPE=Release
cmake --build build-picker
ctest --test-dir build-picker --output-on-failure
```

`fp_worker_start` duplicates the caller-selected root FD, creates a two-slot
`nx_sim_channel` and atomically creates a nonblocking/CLOEXEC wake pipe. One
worker then exclusively creates, reads, mutates and destroys the original opaque
model. The constructor performs no directory scan; the caller can close its
original root after success. No worker receives a context, window, font, UI
callback or mutable application view.

The caller submits copied `fp_command` values **after a successful UI frame**.
READ observes current state, creating the original model on its first successful
read. Other commands require the exact observed adapter state. Each admitted
attempt supersedes older commands, even if the original operation fails; stale
commands return Changed with the retained current observation and do not execute.
READ can rediscover current state after an abandoned reply without replaying a
navigation or selection. View/request keys are copied, not interpreted as
authority. A future UI must correlate both the channel ticket and its active
view/request key before adopting a reply.

The worker calls the original Refresh/Enter/Up/held-Ancestor/filter/hidden/selected
witness functions. It reserves a maximum-size reply before those calls, then
copies every visible entry, full raw name and escaped label, stable complete-
snapshot ordinal, original revision, current filter/hidden state and all held
ancestor labels. The original 4,096-entry/64-level limits remain; 4,097 entries
fail without replacing the prior snapshot. Filtering reuses original membership
and complete-snapshot order rather than inventing a second sort/filter model.
Query byte lengths preserve original validation, including interior NULs and
invalid UTF-8; no prefix truncation is treated as success.

Replies are immutable in-process observations, owned until message disposal.
Only `fp_reply_take` mutates their FD ownership: it transfers the original
directory/O_PATH regular-file witness plus literal leaf exactly once. Otherwise
message disposal closes the witnesses, including abandoned queued selections.
The caller independently decides what to do with those ordinary-user FDs; the
worker neither reads contents nor authorizes filesystem operations. Display
labels are never passed back as filenames or navigation commands.

Use the borrowed `fp_worker_wake_fd` with the existing `nx_window_poll_with_fd`.
Do not read or close it directly. Drain `fp_worker_receive` until ticket zero;
it drains notifications **before** taking a completion to avoid losing a wakeup
between an empty-queue observation and the next native poll. Dispose every taken
message, including retired-view messages. A failed root read has an explicit
original error and zero revision/state; it is not confirmed empty success.

Stop rejects new requests and cancels accepted queued work. Running original I/O
may finish; GUI/context retirement can precede it because worker jobs contain no
UI pointers. Call Stop before Join. Join has no arbitrary-filesystem timeout;
Destroy performs Stop/Join, disposes queued responses/witnesses, then releases
the channel/pipe/root. Caller-retained responses remain caller-owned. Reply
reservation has an explicit schema-sized maximum and is shrunk when possible;
received snapshots must be disposed promptly rather than retained without bound.

The strict test executes actual filesystem navigation, held ancestors after
rename, replaced inode rejection, all 4,096 rows and filtered ordinal 4,095,
65-level rejection, a 255-byte non-UTF-8 filename with its complete 1,020-byte
escaped label, input-byte rejection, hidden-state rediscovery, owned/abandoned
witnesses, wakeup/backpressure and repeated FD balance. Link-time wrappers prove
the model calls run outside the caller thread. A test-only gate holds original
Refresh while ordinary core Busy/Cancel controls still dispatch keyboard input;
the context retires before the original refresh continues and queued work is
cancelled. These are headless core/model checks, not a native dialog, screenreader,
installed IME, physical input, performance budget or full Files adoption result.
