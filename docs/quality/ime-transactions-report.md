# Opt-in complete native IME boundaries — 2026-11-08

Follow-up: [0.1.25 atomic field application](ime-atomic-report.md) extends this
transport checkpoint. The results and remaining work below describe 0.1.24.

SDK source: `f413682a1aed3e2952b1fd335f260e338fccc448`, development version
0.1.24. Affected requirements: B03/B04/I02/T04/P04/P05/Q02/Q03/Q06/Q08.
This closes a transport gap found while extending the separate Files consumer:
an all-filtered native commit previously emitted no TEXT event, leaving an
application unable to distinguish its preceding deletion from an independent
delete. It does not complete Files replacement, undo or catalogue acceptance.

## Public contract and compatibility

`nx_window_set_ime_transaction_events(window, 1)` opts in before the first
successful IME enable. Wayland v1/v3 emit BEGIN/END pairs with identical
target/revision and COMMIT/DELETE/REJECTED flags. Explicit empty commits, v3
null commits, delete-only operations and all-filtered commits are distinguishable.
Existing TEXT flags and DELETE/PREEDIT meanings and order are retained. Separate
native batches are not merged when their target/revision happen to match.

The mode defaults OFF. Prior enum values and `nx_event` layout are preserved;
ordinary callers retain their existing stream. Changing mode after enabling
IME returns STATE; owner-thread, argument and unsupported-backend rules are
explicit. Win32/AppKit/unsupported stubs and X11 return UNSUPPORTED. The native
window implementation and public core accept the new marker types, without
changing ordinary widget editing or dispatching an application action.
See the [contract](../concepts/ime-transactions.md) and
[installed C17 consumer](../../tests/ime_transaction_consumer/CMakeLists.txt).

Buffering, freshness, rejection and atomic application remain the consumer's
responsibility. Never infer END from an empty event queue. Discard a partial
buffer on polling errors, focus/context changes or intervening commands. These
events supply no authorization or automatic undo grouping.

## Final executed evidence

Alpine-Dev/WSL, Clang 22.1.3, Wayland client/server 1.25.0, xkbcommon 1.13.1 and
wayland-protocols 1.48. Actual environment, UID, commands and hashes are in
[the local receipt](ime-transactions-local.json). These are controlled native
Wayland messages and headless core/package checks, not a current Aetheri
compositor/application run, real installed IME or physical/human acceptance.

| Profile | Affected CTests | Wire cases | Installed public consumer |
| --- | ---: | ---: | --- |
| Static Debug ASan/UBSan, leak detection | 9/9 | 33 | builds/runs, version 0.1.24 |
| Static Release | 9/9 | 33 | builds/runs, version 0.1.24 |
| Shared Release | 9/9 | 33 | builds/runs, version 0.1.24, correct shared mapping |

Each profile compiles 66 objects with explicit C17 and Werror. Sanitizer flags
and actual ASan symbols are checked. The three package consumers require exactly
0.1.24 and call the exported new symbol; runtime versions match the headers.
Their dependencies contain no SDL/X11/Vulkan/EGL. Builder revision declarations
are empty on these diagnostic builds; the [source binding](ime-transactions-source-binding.json)
instead checks all 826 tested/package inputs against exact Git blobs.

The new v3 tests run thirteen cases in both opt-in and default modes: replacement,
all-filtered input, empty commit, delete only, empty commit without deletion,
mixed allowed/filtered scalars, two consecutive batches, preedit only, preedit
clearing, stale serial, invalid UTF-8 delete boundary, malformed commit and null
commit. The v1 fallback adds seven cases, including whole rejection of invalid
range/serial/UTF-8 input. Five existing v1/v3 CTests and the portable input test
also pass; the latter checks marker validation, neutral input state and ABI.
No test deadlines, expected edits or sanitizer options were relaxed.

## Retained failures and corrected preparation

The first prototype placed the received-commit bit in the wrong callback.
Existing tests passed, but the new v3 wire case observed flags2 where3 were
required. That actual compiled failure is retained. The new v1 fixture initially
mistook its initial native focus event for a transaction; it now explicitly
requires and consumes that focus event before checking the transaction stream.
Both original failed logs/sources and diagnostic repeats remain in the archive.

The first limited source export omitted an example used by an existing test;
configuration failed. The corrected source export later omitted the Unicode
license required for installation; all three failed install attempts remain.
Adding that unchanged original packaging input allows installation and consumer
build/run. This was an export omission, not a production install-rule change.

Artifact checks also found that the core and installed consumer were strict
C17 while some test programs used the compiler default. The final tests were
rebuilt with explicit C17; their former binaries/commands/logs are retained.
All SDK library hashes remain identical across that test-only flag correction.

Full local archive: `D:/NexaUI/out/nexaui-sdk-ime-transactions-20261108.tar.gz`.
118,642,460 bytes; SHA-256
`8ff143cb085da234dba58a6790754f2c9c4b40b9655515eb8ac2fcadf94fd413`.
The [archive receipt](ime-transactions-archive.json) records complete file,
size/mode/link/directory verification. It includes immutable source exports,
builds, installed prefixes/consumers, actual earlier failures and reproduction
helpers. The [checkpoint checks](ime-transactions-checks.json) bind those records.

The preceding Files main `9a45895d3bded6b24d5a9cfca9756a20a7263a23` has seven
required CI successes and two optional skips; [receipt](files-ime-ci.json).
That metadata does not cover the new SDK feature or complete Files integration.

## Remaining work and independent adoption

Next connect complete native transactions to atomic text-field application,
selection replacement and undo grouping, then to the original Files controller.
Continue late/interrupted/focus-retired events, foreign clipboard, font fallback
and localization. The Files example's prior 0.1.23 experiment remains unchanged;
it does not yet use the new API. Aetheri's own active source, backend and reviewed
SDK 97a4264 pin are untouched. No current Aetheri checkout is built or repinned.
New-symbol consumers require SDK 0.1.24; existing applications may continue using
their accepted SDK. Packet 6, packets 7–8, all 103 IDs and final physical/human/
performance/adoption gates remain active. No 1.0 release is declared.

Read-only Aetheri survey: committed HEAD
`fcb09094102547d19b790487d4ff7f19f65f911f` still declares the reviewed
`97a42647480bb05167cee5e4ae08cedd9147d80d` / 0.1.23 SDK in
`spec/nexaui-inputs.json`. This records independent development, not a build or
runtime acceptance of that current checkout.
