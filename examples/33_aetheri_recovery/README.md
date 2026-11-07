# Original Aetheri recovery observations

This optional installed-SDK C17 consumer begins packet 6's component/recovery
presentation with an owned immutable observation model. It uses the original
`ae_catalogue_recovery_inventory` result and free function. There is no second
inventory query, SQL implementation, recovery actor or authority API here.
The presentation component now supplies a virtualized list, a local filter and
full read-only details. Native host/worker composition and keyboard/IME/assistive
acceptance remain the next integration step; this is not a complete Recovery app.

The trusted Aetheri owner remains responsible for opening its catalogue on its
single owner thread, retaining the original lock/context and obtaining a complete
bounded read transaction. `view_model.h` receives only its copied identity,
context, accepted query ticket and owned original result. It receives no
catalogue handle or path. A production IPC/worker host still has to correlate
and deliver replies using the existing application channel; this example does
not introduce another channel or an owner service.

Successful completion transfers the complete original allocation once. A view
retains a snapshot reference and can inspect it after newer observations or
owner destruction. The original data, including nested pointers, is borrowed
read-only. All calls and reference counts belong to one application thread.
Selection binds the complete record kind and opaque 16-byte ID; a removed
selection clears instead of following a row ordinal. Record IDs are not assumed
to be UUIDs, labels or authorization tokens.

Unobserved, reading, observed and unavailable are distinct. An unavailable read
retains the previous observation and original errno. A successful empty inventory
is observed empty metadata. It proves neither external holder drain nor global
readiness. No operation can be replayed, admitted, cancelled or settled through
this model. A new query ticket supersedes an old reply, without cancelling its
work or releasing a channel reservation.

The local acceptance policy requires the same original owner/epoch/boot/runtime,
nondecreasing catalogue revision and identical records/schema at equal revision.
Revisions remain observations, not mutation fences or future readiness proofs.
Changed context or explicit schema rebinding needs a fresh owner/session and
native view/provider retirement. The component retires its bound publication
before replacing a context; native retained-reference acceptance remains open.
Malformed, stale, inconsistent or allocation-failed candidates stay caller-owned;
the pending ticket and prior snapshot survive. The caller frees rejected original
results with `ae_recovery_inventory_free`. It may then explicitly report a read
failure for the current ticket; rejected replies do not silently retry work.

## Reproduce

Use a fresh immutable archive of original Aetheri
`46e2a21ef979d9a76db2a8487d1de167a6a79ef7`, never its live working checkout.
Configuration checks 67 original build/core/catalogue/schema/fixture inputs,
normalizing CRLF to LF only. Original OpenSSL 3.5.9–3.5.x, SQLite and Python
requirements remain in force for this optional consumer. They are not NexaUI
core dependencies, and no consumer pin or live catalogue is changed.

```sh
cmake -S examples/33_aetheri_recovery -B build-recovery -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_PREFIX_PATH=/path/to/installed-nexaui \
  -DAETHERI_SOURCE_DIR=/path/to/immutable-original-aetheri
cmake --build build-recovery --target recovery_view_model_test
ctest --test-dir build-recovery --output-on-failure
```

Pass `-DRVG_TEST_FONT=/path/to/real/font.ttf` to enable the real-font
`recovery_view` test in addition to `recovery_view_model`. The shared consumer
explicitly links its own standard math operations with `libm`. No font is bundled.

## Presentation component

`recovery_view.h` owns an ordinary context, text-field draft, filtered index and
retained original snapshot. Create it with a borrowed owner/font collection and
explicit language, theme, text size and raster density. After owner state changes,
call `rvg_sync` between frames. Queue input through `rvg_queue_event`; `rvg_frame`
owns begin/end and returns only Close or Read intents. A Read intent is passed to
the app's existing correlated query path; the view never runs the original read
or a recovery action itself.

The host fetches the current borrowed context with `rvg_context` after every
successful frame, then publishes semantics and renders its commands. Mode,
filter and snapshot changes may replace that context. Bind one borrowed window
for native IME/clipboard and publication retirement; every bound window stays
alive until `rvg_close` succeeds. Closing retires publication before destroying
the context; a retirement error preserves the whole view. The host still owns
native events, presentation/wakeup, backend replies and shutdown/join order.
There is no runnable native Recovery host yet.

The list mounts only measured visible/overscan rows using the existing virtual
height index. Arrow/Home/End navigation reaches the full filtered result. Page
keys scroll; activation opens the complete record. Back restores the inspected
row, and Escape goes Back before closing the list. The filter page has Apply,
Back and Close; cancelling preserves the applied filter and draft. Invalid
UTF-8, line breaks and more than 128 bytes cannot apply an older prefix. Matching
uses the public ASCII-case-insensitive filter, with exact non-ASCII bytes, over
localized kind, original state and complete hexadecimal ID.

Details expose original ID/relation, state, revision, unfinished steps, detail,
phase and reason plus owner/epoch/boot/runtime/schema/catalogue revision. They
are one read-only Text snapshot with no editable value action. Page keys read
long details while Back retains focus. Empty, unread, reading and unavailable
observations stay distinct; failure retains the previous data and selection.
At very small sizes only Close and a resize explanation remain; the prior mode,
draft and inspected identity survive restoration. The component accepts
48–4096 logical pixels per dimension and text sizes 12–48. Broader adaptive layout,
offscreen assistive traversal and final visual acceptance remain required.

Semantic row numbers are ordinals within one immutable observation, not hashes
of opaque IDs. Snapshot changes retire the whole context before ordinals can
refer to different records. Selection and detail lookup always use the full
kind/16-byte key. A local filter never changes the complete retained inventory.
Only the component's owner thread may use its methods or borrowed context.

The test driver creates private 0700 directories/0600 databases with original
Python fixtures and explicit migrations. Fourteen view-only owner/schema cases
compare every result record with the original independent Python reference and
require identical database bytes and table rows afterward. Root v2–10 and User
v2–4/v7–8 are exercised; base kinds 1–11 are populated as applicable. Later
schemas are exercised with empty added tables, not actual closure/handoff/unit
custody workflows. The source's original 65,536-record bound is retained; a full
maximum-size populated owner is not benchmarked here.

Two separately marked cases deliberately invoke the original backend's runtime
replacement on those private owners. A genuinely newer query must be refused
by the old presentation session and accepted by a newly bound one. These are
explicit backend mutations, excluded from the view-only byte-equality claim.
The tests also inject malformed observations, ticket races, allocation failure,
equal-revision inconsistency and full-key/selection/lifetime adversaries. The
adversarial record edits are test inputs, never asserted as actual backend work.

The additional view test uses private original Root v10/User v8 observations,
real fonts and ordinary public frame/input/action APIs. It checks logical
navigation, full details/reading, filter/cancel behavior, compact focus restoration
and owner-only Read/Close intents at ordinary, narrow German/high-contrast 2×,
and light/fractional density. A separately named synthetic 65,536-record workload
checks bounded mounted semantics, full-ID filtering, shifted snapshot identity
and allocation-failure rollback. It is not an actual maximum-size authority
inventory or an end-to-end performance/idle-memory benchmark. Native IME,
clipboard, AT-SPI clients, physical/human acceptance and real app adoption remain
unexecuted for this component.
