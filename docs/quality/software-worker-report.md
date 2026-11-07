# Original selected-app worker — 2026-11-07

Consumer source: `a7a920ebf298c03851b1c78a4af974e8c91337a7`.
Original Aetheri: `46e2a21ef979d9a76db2a8487d1de167a6a79ef7`.
Installed NexaUI SDK: `c4f467d85b0147944d2456f5e7602a39ac18e579`, 0.1.23.
Scoped requirements: D07/L05/L11/P04/P05/P07/Q02/Q08.

[Example34](../../examples/34_aetheri_software/README.md) now schedules its
selected-app read producer on an application-owned worker. It reuses the existing
`nx_sim_channel`, two reserved command/completion slots and a nonblocking
close-on-exec wake pipe, following the existing Recovery consumer pattern.
Only the worker opens, queries and closes the original Root catalogue. The
SDK API/ABI/version, Recovery implementation and independent Aetheri checkout/pin
are unchanged. This is the worker foundation for the visible software composition;
it does not complete the native GUI, installed inventory or packet 6.

## Ownership and shutdown

Startup copies the explicit protected absolute Root source path and identity.
It waits for original open/get-info and returns the source context without an
app scan. This handshake can block; no filesystem deadline is claimed. Each job
copies the complete source context, recipient account/UID and selected install
IDs before wakeup. No UI context, callback or semantic object enters the worker.
The adapter is trusted owner-side code, not a new broker or authority boundary.

Completions carry a channel ticket and view/request key. The future host must
check both before adoption; the existing presentation owner independently refuses
a superseded ticket and validates the full source/recipient/target set. Taking a
successful result detaches it once. Every received message needs disposal, also
after rejection or view retirement. Transport/allocation status remains distinct
from original negative errno; neither establishes an empty installed inventory.

Stop refuses new commands. A dequeued original batch may finish, while jobs still
queued complete as cancelled without another original scan. The UI retires first;
join waits for original I/O and then destroy drains undispatched results before
freeing channel/pipe resources. Detached results stay caller-owned. The pipe is
only a hint: receive drains it before taking completions, so the host must drain
the channel until no ticket remains before sleeping. No new SDK wake API or
periodic timer is introduced. Native `nx_window_poll_with_fd` integration remains
the next host increment.

## Executed evidence

Static Release, shared Release and ASan/UBSan consumers each pass **3/3
CTests**: the prior nine-schema model/read regression, declared 100,000-target
model and new nine-schema worker lifecycle runner. Each worker runner uses private
original Root v2–v10 catalogues and a User catalogue; exact database bytes and rows
remain unchanged. Model regression separately retains its explicit original
runtime-replacement test. These are real original API/SQLite calls over synthetic
eligibility metadata, not actual installed payloads or production-service admission.

For each of the 27 worker schema/profile cases, the test holds two actual original
SQLite read transactions at the app-resolution SELECT and confirms transaction
state. The first hold queues another job, then overwrites the caller's scope and
target array before that queued job can run. Its completed five-target result
still matches the original copied account, UID and complete install IDs. The test
also checks both reserved slots, rejected third admission, stale-ticket refusal,
once-only transfer, original read error with retained snapshot, reply
allocation failure and subsequent retry.

The second hold destroys an actual SDK `nx_context` and the application owner
while the original query is still blocked. Stop rejects admission, the running
batch finishes after release, the waiting job is cancelled without another scan,
and both late messages are disposed after UI-model retirement. Original open,
get-info, resolve and close wrappers assert one worker thread distinct from the
UI thread. Poll observes the wake FD; after draining the pipe, a second already
queued completion is receivable without a new poll. Descriptor counts balance
after normal/error/startup-failure paths. Wrong source identity leaves the output
context unchanged. This proves context/model lifetime, not native-window or
assistive-provider retirement.

The worker binaries exercise actual installed SDK channel and context functions.
`readelf`/`ldd` receipts record linkage, including resolution of the shared NexaUI
library to the selected installed prefix. This extends beyond the prior pure
model's package/header check, but still exercises no renderer, native window,
IME, clipboard, AT-SPI or physical display. No new SDK core-test run is claimed.
ASan/UBSan run with leak detection and halt-on-error, without final diagnostics.

Three private copied defects compile and fail their intended assertions:
borrowing the caller's target array instead of reading its owned copy; reading a
queued batch after Stop; and omitting the wake byte. Mutated code and failing
outputs remain separate from passing source. Initial read-only locator attempts
assumed a projection implementation filename; the real original SELECT lives in
`src/catalogue/store.c`. Those attempts are retained, with no source/build fix
required. The local Windows worktree was compared through a Git archive because
its absolute Windows gitdir is not usable by WSL's Git.

## Revisions and retained artifacts

[Local receipt](software-worker-local.json),
[archive receipt](software-worker-archive.json).
All 12 source inputs match `a7a920e` after CRLF-to-LF normalization only. All 11
executed inputs are unchanged from the tested export; only README was updated
after execution. The immutable original check still verifies its 67 inputs.
Prior main `b5b7a1d` has seven required CI jobs successful and two optional skips;
[exact-run API receipt](software-model-ci.json). This is API status evidence, not
downloaded raw CI logs. Optional example34 tests remain local and separate from
root CI.

`D:/NexaUI/out/nexaui-software-worker-20261107.tar.gz` contains 13,725,579 bytes;
SHA-256 `7e1f2a0b1df89d49c71960fb96656b2c8ec74b9e72654ee8ea7de2469205a56e`.
Independent local verification checks every file hash/size/mode, directory mode
and symlink target: 1,083 files, two links and 383 directories. The archive retains
source exports, exact SDK inputs, immutable original source, all build/test logs,
private fixtures, negative builds and the command journal. The collector's own
live log and terminal journal entry are outside its frozen boundary. Earlier
artifacts are retained.

## Remaining scope

The next increment composes the native software list/filter/details using the
existing list, text, input, software-renderer and host APIs, with keyboard/IME/
clipboard/AT-SPI, selection/focus and late-publication checks. Exact observed
artifact/component/publisher joins precede dependency details. Full inventory,
device control, current process/resources, permissions, install/update/removal
admission and trusted login/installer/recovery hosts require their actual owner
interfaces; selected active-app observations must not stand in for them. The
rest of packet 6, packets 7–8 and physical/human/performance acceptance remain
open. All 103 requirements and all planned Aetheri surfaces remain in scope.
