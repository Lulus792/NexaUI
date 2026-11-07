# Native file picker over the original Files model

This optional ordinary Wayland/software dialog composes an editable name filter,
held-ancestor path, paged single selection and Open/Cancel over the original Files
model on one worker. It does not replace the existing Files app, create another
Files model or turn a GUI selection into a grant, StorageRef, installation or
privileged authority receipt. Normal Aetheri adoption remains separate.

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
  -DPICKER_TEST_FONT=/path/to/local-font.ttf \
  -DCMAKE_BUILD_TYPE=Release
cmake --build build-picker
ctest --test-dir build-picker --output-on-failure
build-picker/file_picker_native /path/to/local-font.ttf /caller/chosen/root
```

The caller chooses an ordinary root directory. Successful file selection prints
the literal leaf as hexadecimal and the held witness's device/inode, then closes
the owned descriptors after GUI teardown. This example does not read file
contents or return durable authorization. Cancel closes without a selection.
An integrating caller can take the original directory/O_PATH witness once via
`fp_reply_take` instead of this demonstration's printing/closing step.

Set `FP_DIRECTORY=1` for single-directory selection. Enter on a directory row
still navigates; regular-file rows cannot complete this mode. The fixed action
“Choose this folder / Diesen Ordner auswählen” selects the current held folder,
including the caller's root, a genuinely empty folder or a filtered view with no
matches. An unread root remains unavailable. The worker reuses the original
`ae_files_directory` contract and returns two distinct owned CLOEXEC descriptors
for that same held directory with literal leaf `.`. It never reopens a displayed
path, so renaming the held folder and replacing its old name cannot redirect the
selection. Disposal/once-only transfer and Stop/Join ownership remain the same.
These ordinary directory descriptors are not grants or durable directory IDs.
An embedding caller sets `picker.directories` once after init and before the
first request/frame. The mode has folder-specific normal and compact help text.

All admitted rows remain available through explicit 16-entry pages with complete
counts. Up/Down and Home/End on a row move across pages; Enter opens a directory
or selects an ordinary file. Tab/Shift-Tab traverse controls. PageUp/PageDown
scroll long content while retaining focus. The expandable Folder path offers
every held ancestor. Applying the filter is explicit, including Enter in its
field; invalid oversized drafts disable Apply. Hidden entries use a checkbox.
Open/Cancel stay outside the scrolling body and stack in a narrow window.
Esc cancels the picker. Unsupported IME transport preserves ordinary editing.

`FP_GERMAN`, `FP_LARGE`, `FP_SMALL`, `FP_CONTRAST` and `FP_LIGHT` select the example's
German, 200%-text, 360x300, high-contrast and light configurations. Default is
English/dark at 720x580. Colors, focus, reduced motion, fields, clipboard,
IME placement and semantic actions reuse installed public APIs. Rows have full
original escaped labels; directory icons use the existing portable vector API.

`FP_OWNER=1` instead opens a Selection workspace with its own editable note,
file/folder chooser buttons, last selection and Close. It reuses `nx_modal_begin`
for a full-viewport modal inside this same native window; it does not implement
native child-window parenting. Background controls reject input while the
chooser is open. Cancel/Escape returns to the corresponding trigger, preserving
the owner note and last successful selection. Reopening creates a fresh view
epoch/editor while READ rediscovers the persistent original worker's held
folder and committed filter. Unapplied chooser drafts belong to their view.

The owner takes an admitted FD pair once, copies its original escaped display
label, and closes the preceding pair only when replacing it with a successful
selection. Cancel and retired-view completions cannot replace it. The pair stays
owned until replacement or owner teardown; it is not a grant or durable ID.
Outside the note editor, PageUp/PageDown read the scrollable owner body in
overlapping half-viewport steps without moving focus. The fixed Close action
remains available while reading a selected name taller than the viewport.
The note editor retains its own keys; owner paging does not intercept an active
modal's commands. The native test's `--owner --owner-reading --small` workflow
selects a real 255-byte non-UTF-8 filename and reads its complete 1,020-byte
escaped result down and back up, preserving focus, note and later cancellation.
Closing a chooser does not Join its worker. Owner window teardown retires all
UI, closes its selection descriptors, then stops/joins/disposes original work.
The optional `owner.c` composition uses only public APIs. The view's configurable
`parent_id` defaults to 1; the composition sets its modal parent before frames.
Measured labels determine whether the normal controls and fixed footer fit.
Smaller sizes use a compact, scrollable size hint and Cancel (a close icon when
text would crowd the hint). Folder, page, selection and query draft survive.
The native bottom-right grip uses the existing public resize API with a real
pointer gesture. These are not approved final Aetheri visual variants.

Errors distinguish an unread root from a retained previous folder. An explicit
Retry repeats the failed READ/Enter/Refresh against the worker's current state;
it does not silently substitute a parent refresh. Unknown/malformed completion
recovery observes with READ instead of replaying an uncertain operation.
Changed entries require an explicit Refresh folder. At the 64-level path limit,
the adjacent recovery action goes Up rather than repeating an unchanged Enter.
Error actions receive safe focus beside their message. PageUp/PageDown can read
long messages while retaining that focus; ordinary scroll does not snap back.

The host retires native publication/context before a request or admitted result
can rebind old controls, and on page changes (including return to a prior page).
It preserves safe focus through replacement. Page buttons clear hidden selection;
row navigation explicitly selects its new target. One active request per view
is correlated by both ticket and copied view/request key. Malformed current
replies show an error without replacing a good observation. Stale replies are
disposed without adoption. Work is posted only after successful frame completion.
The native loop waits on the existing worker descriptor instead of a redraw timer.

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
authority. The picker checks both the channel ticket and its active view/request
key before adopting a reply.

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
The view says “Entries not available” until an observation has been admitted.
Successful empty views show numeric zeros, with the original total preserved
when a filter has no matches. Failed operations retain prior observed counts.

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
cancelled. These worker checks alone are headless core/model evidence.

With `PICKER_TEST_FONT`, a separate real-font CTest verifies current ticket/key
correlation, malformed-current-result retention/Retry, actual filtering, ordinal
selection, paging, invalid query refusal and full 1,020-byte escaped-label reflow
at German 200%/360x300, including reading an over-height focused row. Without an
explicit local font only the worker CTest is registered.

`atspi_client.c` is an independent OS peer, using only the existing example 30
transport helpers. `run_native.py --help` describes the isolated compositor/bus/
registry/host/client fixture. Nine native stages cover actual folder entry,
preedit/cancel/commit, filter/clear, ancestor expansion/navigation, last-page
keyboard reveal, page-button round trips, stale references and real selected
inode or Cancel. `--maximum` uses all 4,096 entries. `--no-ime` instead checks
ordinary AT-SPI editing without a text-input protocol. `--pending-cancel` uses
the separate `file_picker_held_native` test target: its link-time wrapper holds
the original Refresh until after actual keyboard Cancel and GUI retirement.
The production target contains no gate. Release-active assertions, strict C17
profiles and sanitizers do not replace installed input methods or human review.

The same native runner accepts `--recovery` with `recovery_atspi.c` as its
independent peer. It starts with 4,097 root entries, retries after the real
filesystem reaches 4,096, repeats a failed Enter into another overfull folder,
and distinguishes unavailable counts from a successful filter with no matches,
reads the retained error using PageUp, and navigates to the last child. Real
pointer drags resize through 64x64, 240x96 and back, checking preserved draft,
selection/focus and retired controls. It then replaces the selected inode,
verifies refused selection, explicitly refreshes and selects the new witness.
Scoped Wayland client traces wait for compositor resize admission/release;
merely flushing a client request is not treated as an accepted grab. The pinned
fixture's first ordinary window origin is (40,40). This is controlled native
input, not physical output or human acceptance.

`--mixed` uses the independent `mixed_atspi.c` peer. The fixture briefly stops
only its own test host, submits each complete native input burst through the
compositor, then resumes it. It checks Tab/Home away from a row, Shift-Tab/End
back into the list, pointer focus loss before Home, text-field Home/End and
cross-page Home/Down/End. A final Home/Down/Enter burst must open the first regular
file's exact witness. `--maximum` also applies to this workflow. The ordinary
host completes queued interaction frames before interpreting the next custom
key and before interpreting another handled row key. It retains at most one
deferred copied event and postpones rasterization until that native queue is
drained. General routing is called only after the native queue is empty, so it
cannot bypass those command boundaries. This is ordering evidence, not a
throughput or latency budget.

`--directory` runs `directory_atspi.c` against actual navigation, a no-match
filter, held-folder rename/path replacement and exact directory selection or
Cancel. The native host checks both descriptor types and their shared inode.

`--owner` uses `owner_atspi.c` to exercise file and directory completion,
background blocking, retired references, restored focus, preserved note/result
and cancellation of a subsequent view. A held original Refresh variant retires
the chooser while the owner remains usable, then closes the owner before the
worker is released. A controlled native IME case commits text in the owner note
and cancels a subsequent preedit through real focus traversal. The fixture waits
for completed field frames and matching compositor-observed v3 commit serials
before further injection. This is a private test producer, not installed-IME or
human acceptance.

Multiple/save selection, separate native owner/child windows, richer sorting/file-type policy, additional
I/O/permission/allocation failures and resize/scale configurations, other platforms, real services/grants,
performance, physical/human acceptance and normal Aetheri integration stay open.
