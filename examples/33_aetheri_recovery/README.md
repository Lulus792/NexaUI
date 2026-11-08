# Original Aetheri recovery observations

This optional installed-SDK C17 consumer begins packet 6's component/recovery
presentation with an owned immutable observation model. It uses the original
`ae_catalogue_recovery_inventory` result and free function. There is no second
inventory query, SQL implementation, recovery actor or authority API here.
The presentation component now supplies a virtualized list, a local filter and
full read-only details, an in-process original-owner worker and an ordinary
Wayland software host. This is a private observation example, not a complete
Recovery app or a privileged owner service.

The trusted Aetheri owner remains responsible for opening its catalogue on its
single owner thread, retaining the original lock/context and obtaining a complete
bounded read transaction. `view_model.h` receives only its copied identity,
context, accepted query ticket and owned original result. It receives no
catalogue handle or path. `recovery_worker.h` adapts the existing public S02
channel with two reserved completion slots and copied session/request keys.
The UI correlates both that key and its ticket before adopting a reply.
A production IPC/authority service remains an external integration obligation.

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
before replacing a context. The independent OS peer checks retired row, detail
and filter references across refresh and page/context replacement.
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
cmake --build build-recovery
ctest --test-dir build-recovery --output-on-failure
```

Pass `-DRVG_TEST_FONT=/path/to/real/font.ttf` to enable the real-font
`recovery_view` test in addition to `recovery_view_model` and `recovery_worker`.
The shared consumer
explicitly links its own standard math operations with `libm`. No font is bundled.

## Presentation component

`recovery_view.h` owns an ordinary context, text-field draft, filtered index and
retained original snapshot. Create it with a borrowed owner/font collection and
explicit language, theme, text size and raster density. After owner state changes,
call `rvg_sync` between frames. Queue input through `rvg_queue_event`; `rvg_frame`
owns begin/end and returns Close, Read or native resize intents. A Read intent
is passed to
the app's existing correlated query path; the view never runs the original read
or a recovery action itself.

The host fetches the current borrowed context with `rvg_context` after every
successful frame, then publishes semantics and renders its commands. Mode,
filter and snapshot changes may replace that context. Bind one borrowed window
for native IME/clipboard and publication retirement; every bound window stays
alive until `rvg_close` succeeds. Closing retires publication before destroying
the context; a retirement error preserves the whole view. The host still owns
native events, presentation/wakeup, backend replies and shutdown/join order.
The runnable host below supplies this ordering for an explicit private owner.

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
inventory or an end-to-end performance/idle-memory benchmark. Native checks
below are separate from these component results. Clipboard, physical/human
acceptance and real app adoption remain unexecuted for this component.

## Private original-owner worker and native host

`recovery_native FONT.ttf OWNER_DIRECTORY system|user EPOCH32 OWNER32|-` opens
only the explicit private catalogue selected by its trusted launcher. Use a
disposable fixture, not the installed system/user catalogue. Identity arguments
are complete hexadecimal 16-byte values; a system owner uses `-` for OWNER32.
Use a matching installed SDK containing the Wayland backing fix `c4f467d` or
later. `RVG_GERMAN`, `RVG_LARGE`, `RVG_SMALL`, `RVG_CONTRAST` and `RVG_LIGHT`
select the example's locale, text size, initial size and theme.

Only the worker opens, queries and closes the original catalogue. Startup waits
for its open/get-info handshake; inventory reads run asynchronously. Stop rejects
new work and completes accepted queued reads as cancelled. An original read
already running can finish. The host retires semantics and destroys its GUI
before stop/join/drain; join has no filesystem deadline. This process is trusted
in its entirety: separating threads does not establish an authority boundary.

The worker test holds a real original SQLite read transaction while retiring
the GUI context. It checks worker-only original API calls, bounded admission,
ticket/key identity, queued cancellation, wake-FD readiness, drained completion
delivery, stale-result ownership, once-only transfer, error/OOM retry and final
descriptor balance on both original private owners. This core check does not
prove native window retirement during a held transaction.

For an independent OS peer, configure `-DRVG_BUILD_ATSPI_CLIENT=ON` with actual
`atspi-2`/`gobject-2.0` development packages. It includes the existing independent
OS helper from example 30 and links no NexaUI code. With a compatible immutable
headless Aetheri compositor and AT-SPI registry available:

```sh
python3 examples/33_aetheri_recovery/run_native.py \
  --server /path/to/test_compositor_server \
  --registry /path/to/at-spi2-registryd \
  --host build-recovery/recovery_native \
  --client build-recovery/recovery_atspi \
  --font /path/to/font.ttf \
  --source /path/to/immutable-original-aetheri \
  --work-dir /path/to/new-private-run --german --large --small --contrast --ime
```

The unprivileged driver creates its own private original owner, compositor and
accessibility bus. Physical key events traverse the native Wayland path; the
separate libatspi process reads full Text and invokes OS actions. It checks End
navigation, complete details with PageDown/PageUp, old-reference retirement,
Read refresh, editable local filtering, Enter refusal during native preedit,
commit/application, selection restoration and keyboard Close. It requires the
same original database bytes/rows and normal host/peer exits. `--kind user`
selects User v8 instead of Root v10. These virtual-input checks are not human
screenreader or physical-display acceptance. Output migration,
clipboard and production owner transport remain separate open gates. The
test-only lifecycle host below checks held-read close/wakeup and selected
whole-window retained references.

## Native held-transaction lifecycle checks

Configure `-DRVG_BUILD_LIFECYCLE_HOST=ON` to build `recovery_held_native`, and
`-DRVG_BUILD_ATSPI_CLIENT=ON` to build `recovery_lifecycle_atspi` with actual OS
development dependencies. The ordinary host source is included verbatim in the
test executable; GNU link wrappers hold only its second original inventory read
inside an active SQLite transaction and observe real teardown/disposal calls.
No test gate is compiled into `recovery_native` or the installed SDK.

Run the same driver/arguments above with `--host build-recovery/recovery_held_native`,
`--client build-recovery/recovery_lifecycle_atspi` and either `--held wake` or
`--held close`, in a new work directory. Omit `--ime`: ordinary IME checks use
the ordinary peer separately. All original fixture, byte/row equality and
normal-exit checks remain. Use `--kind user` for User v8, otherwise Root v10.

The wake case pauses the independent peer, observes the actual parent-owned
Linux x86-64 child in `poll` with its worker FD, then releases SQL. It requires
worker-FD readability, a fresh observation and unchanged key count before any
further OS request. This `/proc` witness requires parent access to its child's
syscall and bounded pollfd memory; other carriers need their own witness.

The close case opens a live filter draft during the held read, focuses Close
and closes with a native Enter key. The compositor must unmap the window while
the process/transaction remain alive. The independent peer then requires the
retained text and button to reject edits/actions. Only after those checks does
the driver release SQL. The original result is disposed exactly once after GUI
retirement, with no late adoption and balanced process FDs. The test adds no
timeout or cancellation promise to original filesystem work or worker join.

## Native keyboard navigation and filter cancellation

Add `--navigation` to the ordinary native driver/peer workflow to extend its
ten checkpoints to twenty-five. It drives native Tab/Shift-Tab, activates Read
with Enter, reaches the filter with Enter, types a draft, and cancels through
keyboard Back and Escape. An independent OS peer verifies focus, selected full
record identity, retained drafts on reopen, unchanged applied results and refused
late edits on retired fields. This mode expects exactly three original reads:
initial, the ordinary OS-action refresh and the additional keyboard refresh.
Filtering and cancellation dispatch no backend request.

The public scroll-area contract makes visible scrollbars focusable. The peer
inventories the filter-help scrollbar and checks the forward/reverse Tab path
through it when present; ordinary layouts also check the direct path without a
scrollbar. Each Tab stop and Cancel result is checked explicitly.
Combine `--navigation` with the ordinary `--ime`
case as needed; held-transaction checks use their own peer separately.

## Native resize and compact restoration

Add `--resize` to the ordinary workflow; it includes `--navigation` and extends
the sequence to thirty checkpoints. Native pointer presses on the bottom-right
grip call the public window resize API. The original test compositor must admit
and release each gesture, reaching 64×64, 240×96 and the original window size.
The driver requires a submitted frame after the release configure. Its fixed
origin and state-array observations belong to this pinned test compositor.

The independent peer verifies compact Close focus and in-window bounds, removal
of the filter field, retained draft and restored field focus after enlargement,
and selected full record identity after Escape. Retained old fields must refuse
edits both while absent and after a fresh field with the same ID appears. The
pointer-only grip preserves the previous key focus without adding a Tab stop.
Only the same three original reads occur; all resize/filter work stays local.

With `--resize --capture`, a compatible compositor saves four scenes on each of
its three virtual outputs. The driver records complete PPM hashes and view sizes.
Compact mode exposes only Close and an accessible enlargement explanation; it
does not display record/filter content. These captures and OS queries do not
establish mixed-scale output migration, physical display or human acceptance.

## Independent native clipboard checks

The existing window-bound text clipboard adapter also supports the filter.
Add `--clipboard normal --clipboard-tools /path/to/private-tools` to the ordinary
workflow, omitting `--navigation`/`--resize`. The private Linux x86-64 tools directory
contains `bin/wl-copy`, `bin/wl-paste`, and `lib/ld-musl-x86_64.so.1` with its
matching Wayland/FFI runtime. The recorded fixture uses unchanged wl-clipboard
2.2.1 tools, separate from NexaUI. No tool installation or account clipboard is
used; every process connects only to the private test compositor.

The sixteen checkpoints include the ordinary ten, then native Unicode paste,
Ctrl-A/C copy to an independent client, and a source held during a pending read.
Copy must actually cancel the previous foreign owner; equal text alone cannot
prove ownership. Foreign publication and its temporary popup retirement precede
probing. After Tab leaves the field, the source resumes. A parent-owned `/proc`
socket-wait witness confirms its send callback returned without opening another
focus-stealing probe window. A native frame then precedes the OS client's check
that the old draft remains unchanged. Escape preserves the selected record.

Use `--clipboard close` for fifteen checkpoints instead: Close remains usable
while the foreign source is stopped. The host must unmap and exit normally before
the source resumes; retained field/button references refuse edits/actions.
The harness resumes and terminates its own stopped source during cleanup.
The fixed public test payload is passed through wl-copy's text argument, avoiding
its stdin spool on a quota-exhausted filesystem. Neither clipboard workflow adds
an original-owner request, filesystem timeout or new SDK implementation.
## Reviewed original source profiles

The default `AETHERI_SOURCE_PROFILE=legacy-v10` retains the existing pinned
`46e2a21` Root2–10/User2–4,7–8 consumer. Explicit
`-DAETHERI_SOURCE_PROFILE=root-v11` selects the separately pinned
`fa5347264054b05e8af57e6d0a210008a6c849d1` snapshot and admits Root11, including
the original Unit issuer recovery record. Both profiles use the installed
NexaUI0.1.23 API and the same model/view/worker. Source hash checks reject a
profile/snapshot mismatch before building. No live Aetheri source/pin is changed.

The model checks each reviewed schema/kind combination; User11, future Root12,
unknown kinds and an issuer record in Root10 remain rejected. The view gives the
new full kind+ID a distinct English/German name, including when allocation,
custody and issuer records share the same opaque allocation ID. An issuer
recovery row is stored owner metadata; it proves no current actor/image/ProcessFD
admission, independent retirement, holder drain or permission.

With `RVG_TEST_FONT` configured, `recovery_root_v11` exercises original unchanged
metadata-only issuer fixtures through read-only inventory/model/view/worker
contracts. Existing tests continue covering older Root/User schemas. Native
OS-peer, actual current issuer/service and physical/human/performance acceptance
for this new profile remain separate gates.


## Native Root11 issuer inspection

For an explicitly built `root-v11` host, the ordinary native runner accepts
`--issuer-fixture /path/to/recovery_root_v11_fixture` and
`--issuer-mode fresh|lost|replacement` (default fresh). Use the matching frozen
source as `--source`. This requires the existing canonical preparation script
in example34 alongside example33 and the peer built from the same checkout.
The runner prepares private original metadata fixtures, not live service or
process admission. It currently combines this mode with ordinary IME/navigation;
held-query and clipboard workflows remain separate. Choose a short private
`--work-dir` so both Wayland and D-Bus socket pathnames fit Linux limits.

The independent peer checks the full issuer ID, related operation, schema,
runtime, reason and read-only Text. See the [native Root11 report](../../docs/quality/recovery-root-v11-native-report.md)
for nine current-profile and two legacy cases, compiled negative controls,
retained setup failure and exact source/artifact bindings. Native physical,
human assistive and actual current-authority acceptance remain open.
