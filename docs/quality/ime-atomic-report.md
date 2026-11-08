# Atomic marked IME edits in fields — 2026-11-08

SDK source: `ca1cb3db26badde2c6fbe3cc1bae1de1b149ad18`, development version
0.1.25. Affected requirements: B03/B04/I02/T01/T03/T04/P04/P05/Q02/Q03/Q06/Q08.
This extends the preceding 0.1.24 transport with whole-operation field editing.
The original Files controller's adoption remains the next integration step.

## Behavior and ownership

Text and numeric widgets recognize complete BEGIN/END envelopes queued before
one frame. A private buffer helper validates the whole operation, encodes up
to 1 MiB of UTF-8 scratch, then calls the existing range-edit/history path once.
There is no second editor or new public structure/function. Existing history
budgets still govern retention. One undo restores the original text and exact
selection; identical replacement bytes still collapse the selection and retain
its original direction for undo. Empty commits and delete-only packets work.
Numeric edits change the draft, retaining explicit value confirmation.

Payload cannot fall back to legacy scalar editing when an envelope is rejected,
nested, malformed, mismatched or incomplete. Split packets are not accumulated
and later applied. Only quarantine metadata persists until matching END; the
host must buffer whole packets before a frame and discard interrupted work.
The field revalidates target, buffer/model/selection revision, provider and
routed focus generation. Consecutive same-revision native packets can apply in
one frame with separate undo while the preceding result remains current.
Old-token replay in a later frame is refused. Ordinary commands retain their
event routing; an intervening command rejects the atomic edit.

Read-only/disabled fields reject edits. Sensitive commits keep no history,
publish no secret semantic value and wipe temporary commit bytes before free;
surrounding deletion is refused. Allocation failure preserves buffer, selection
and history. A later paint failure can be retried in the same frame without
applying accepted edits again. Input/provider lifetime remains owner-thread
work. See the [contract](../concepts/ime-transactions.md).

Default unmarked event streams are unchanged. Existing enum values, public
event layout and signatures remain unchanged; all 525 Linux shared export
names match the actual 0.1.24 baseline. The new helper is hidden. Header, CMake
package and runtime library versions match 0.1.25. This is no 1.0 release.

## Executed evidence

Alpine-Dev/WSL, Clang 22.1.3, explicit C17 with Werror; Wayland client/server
1.25.0, xkbcommon 1.13.1, wayland-protocols 1.48. These are controlled native
protocol fixtures and portable public-widget tests, without a physical display,
production input method, current Aetheri application run or performance claim.

| Final profile | Affected CTests | Native widget tasks | Existing transport cases | Installed C17 probe |
| --- | ---: | ---: | ---: | --- |
| Static Debug ASan/UBSan, leak detection | 16/16 | 8 | 33 | builds/runs, 0.1.25 |
| Static Release | 16/16 | 8 | 33 | builds/runs, 0.1.25 |
| Shared Release | 16/16 | 8 | 33 | builds/runs, 0.1.25 |

Each profile compiles 79 objects with explicit C17/Werror. All 828 exported
source/package inputs match [exact Git blobs](ime-atomic-source-binding.json),
allowing recorded CRLF/LF normalization only. Diagnostic builds leave the builder
revision declaration empty; actual input/artifact hashes establish provenance.
Commands, sources, objects, binaries, package linkage, test output and dependency
versions are retained in [the local receipt](ime-atomic-local.json).

The [public regression](../../tests/test_ime_field_atomic.c) checks Unicode v3
replacement, v1 range deletion, empty commits, delete-only edits, identical bytes,
combining marks, multiline selection, whole undo/redo, retained prior history,
filtered/rejected/stale/wrong-target/model-replaced input, missing scalar flags,
duplicate deletion, invalid UTF-8 boundaries, split/nested/mismatched frames,
focus interruption, commands, read-only/disabled policy and paint retry.
The [native fixture](../../tests/test_wayland_ime_atomic.c) uses the actual window
adapter and its published tokens for five text tasks, two numeric drafts and
one password task. Native strings travel through real private Wayland v3 wire
messages; no invented application-side token substitutes for that adapter.

The extended allocation-failure test exercises the real buffer translation unit,
including commit scratch, history and boundary allocation failures in ordinary
and sensitive modes. At exactly 1 MiB a forced first allocation returns MEMORY;
one extra four-byte scalar returns LIMIT with no allocation. Both preserve all
saved text/selection/history/boundary state. This is admission/failure evidence,
not a successful largest-edit, process-RSS or latency budget measurement.

## Actual previous-SDK counterexample and retained corrections

An identical public-API test source is separately linked to the actual installed
0.1.24 and final 0.1.25 shared SDKs, with exact package/runtime version checks and
loader paths. Both produce the intended Unicode replacement. One undo on 0.1.24
leaves two bytes of replacement instead of the original five bytes (exit 1).
The final 0.1.25 executable restores all five original bytes and reversed
selection (exit 0). Library/executable/source hashes and complete logs are retained.

The first native widget run incorrectly expected numeric drafts to confirm the
committed value automatically. It failed one of 16 CTests. The fixture now
checks the unchanged committed value, calls the existing explicit confirmation
API and checks valid/invalid outcomes and draft-only undo. Production numeric
confirmation was not changed. Original source, binaries and failing logs remain.

The first export audit found the new private helper in the Linux dynamic symbol
table. A hidden declaration removes it without changing existing visibility
policy. Three fresh final builds and the paired counterexample were repeated;
all 525 previous names remain, with no additions/removals. The earlier export
and all intermediate profiles are retained. No deadline, sanitizer setting or
expected edit was weakened to obtain passing results.

The preceding transport checkpoint at main
`9a746654c5619312ab65f7a47c7991989d16f592` has seven successful required GitHub
jobs and two optional skips; see [exact-source metadata](ime-transactions-ci.json).
That is previous-checkpoint CI, not CI for this new atomic field implementation.

## Remaining integration

Connect full marked-packet buffering and controller validation to the separate
original Files consumer, then test create/rename, interrupted/late/focus-retired
input, rejection, undo, cancellation and teardown. Raw buffer/event consumers do
not gain automatic widget transactions. The prior Files 0.1.23 experiment is
unchanged and still needs deliberate adoption of the installed new SDK.

Read-only Aetheri survey: HEAD `fcb09094102547d19b790487d4ff7f19f65f911f`
still declares reviewed SDK `97a42647480bb05167cee5e4ae08cedd9147d80d` /
0.1.23 in `spec/nexaui-inputs.json`. Its independent checkout, pin, application
and compositor were not changed or built here. Foreign clipboard, fonts,
localization, remaining packet 6, packets 7–8 and final physical/human/performance
and adoption gates remain open across all 103 catalogue IDs.

The [archive receipt](ime-atomic-archive.json) and
[checkpoint checks](ime-atomic-checks.json) bind final and retained diagnostic
artifacts. The host archive includes all eight build stages, paired tests,
the actual previous installed SDK prefix, receipts and reproducer scripts.
`D:/NexaUI/out/nexaui-sdk-ime-atomic-20261108.tar.gz` is 205,414,993 bytes;
SHA-256 `164669b886940cb9e1e6bb24219a84eaf13c8379f32cd46229b470c29d7be5a3`.
All 8,064 files, 10 links and 3,811 directories passed host verification of
bytes, sizes, modes, link targets and directory membership. Nothing was
excluded as a special file. Previous accepted archives remain available.
