# Original-owner recovery observation foundation — 2026-11-07

Consumer source: `7bf9b92f1ba4d448295cc76db5975ce535c26770`.
Installed NexaUI: `ac0fb3b3b3aa53b2a4de927ec52a06deb33d5c36`, version 0.1.23.
Original Aetheri: `46e2a21ef979d9a76db2a8487d1de167a6a79ef7`.

## Requirement and original-API reconciliation

Packet 6 requires device/software/component views and installer/recovery
presentation, including observed progress, failure/unknown and reconnect-safe
state. Existing Aetheri Settings models supply transient display/theme settings;
`ae_catalogue_resolve_app` observes one known installation. Neither API is a
complete device or installed-software inventory service. The previously pinned
Files/manifest/operation examples keep their original sources and evidence.

The newer original `catalogue/recovery.h` supplies a real bounded metadata
inventory from one SQLite read transaction, sorted by full record kind/ID.
Its original lock, thread, owner, mount and source contracts remain in force.
The result includes selected pending/transitional owner metadata. It is not a
hardware/process/FD census, a global readiness decision, a recovery actor or a
complete application/dependency inventory. Empty or historical SQL cannot prove
external holder retirement or permit future effects.

[Example 33](../../examples/33_aetheri_recovery/README.md) starts that presentation
with immutable original allocations and read-only full-key selection. It changes
no public SDK API/ABI, implementation, version or dependency. Original Aetheri
OpenSSL/SQLite/Python dependencies belong solely to this optional consumer.
Configuration verifies 66 pinned build/core/catalogue/schema/fixture inputs.

## Delivered behavior and limits

The trusted original owner obtains an inventory on its owner thread. The
presentation receives an owned result, copied identity/context and an already
accepted application query ticket. It never receives a catalogue/path or an
admission/replay operation. Successful publication moves the whole result once;
snapshot references retain it across refresh and owner destruction. All
reference operations stay on one application thread.

Unobserved, reading, observed and unavailable are distinct. An original read
error retains the last snapshot/selection and exact errno. A successful empty
result means observed empty metadata. Full kind plus all sixteen opaque ID bytes
bind selection; shifting row positions preserve the key and a removed object
clears selection. Stale replies retain caller ownership. An allocation failure
leaves the pending ticket and current observation intact.

The local policy requires unchanged source/epoch/boot/runtime, nondecreasing
catalogue revision and identical records/schema at equal revision. A changed
context requires a new session. This is a conservative presentation acceptance
policy, not a new producer guarantee, mutation fence or readiness capability.
Discarded results use the original free function. Native context/provider
retirement and reply transport remain to be composed with the existing host.

## Executed checks

Fedora 44 guest, Linux 6.19.10, GCC 16.2.1, CMake 4.3.0, Python 3.14.3,
OpenSSL 3.5.9 and SQLite 3.51.2. Final inputs build as strict C17 with
`-Wall -Wextra -Wpedantic -Wconversion -Wshadow -Werror`.

| Installed SDK / consumer profile | Final CTest | View-only owner/schema fixtures | Actual owner runtime replacements |
| --- | ---: | ---: | ---: |
| Static Release | 1/1 | 14/14 | 2/2 |
| Shared Release | 1/1 | 14/14 | 2/2 |
| Static Debug ASan/UBSan/LSan | 1/1 | 14/14 | 2/2 |

The sanitizer SDK, consumer and original libraries are instrumented; leak
detection and halt-on-error remain enabled. Final test logs contain no sanitizer
diagnostics. This is a Linux installed-consumer result, not native UI acceptance.

The driver uses the original Python owner fixtures/migrations in private 0700
directories and 0600 databases. Root v2–10 and User v2–4/v7–8 are exercised.
Every observed record matches the original independent Python reference; all
view-only database bytes and table rows remain identical after execution. Base
kinds 1–11 are populated as applicable, including two-/four-byte UTF-8 reasons
and unfinished steps. Later schema tables are empty; actual closure/handoff/
interruption/unit-custody workflows are not covered by these fixtures.

Each case checks ticket supersession, stale success/error disposal, unchanged
snapshot/error retention, allocation-failure ownership, malformed strings/counts/
ordering/relations, equal-revision inconsistency, old-view lifetime, full-ID/kind
distinction and removed/shifting selection. Artificial record edits are named
adversarial inputs, not claimed as real owner effects. Empty success is explicit;
the original 65,536-record bound is kept, without a populated-maximum benchmark.

Two separate cases per profile deliberately call the original backend's runtime
replacement on the private Root/User owners. Old sessions refuse genuine newer
inventory; a newly bound session accepts it. Original context observations in
held snapshots survive. These deliberate backend mutations are excluded from
the fourteen view-only byte-equality cases.

Three independently compiled defects fail at the intended model assertions:
ignoring the current ticket, comparing only fifteen ID bytes, and failing to
retain snapshot ownership. A fourth configuration check changes a private copy
of the original recovery header and fails the source-hash gate. Successful
controls retain the original strict flags, bounds and deadlines.

## Artifacts and retained attempts

The [machine receipt](recovery-observation-local.json) records SDK/binary/source
hashes, all final commands, counterexample exits and independent archive checks.
Local archive: `D:/NexaUI/out/nexaui-recovery-model-20261107.tar.gz`.

- SHA-256: `5b0dab534a2bed73f6945f0b6f222b3e79a0229ae3605ade470ea16a1d05508b`
- 24,138,604 bytes; 1,776 regular files, 2 links and 582 directories, plus manifest.
- Seven tested input files match the exact code archive, allowing only CRLF/LF.
- Original full archive SHA-256: `7c57c717f009f8bb55c9889ca3ae016d5f47a73efe4d2c0b41b129e8749f92e6`.
- Full original/code archives, extracted inputs, three installed SDK prefixes,
  both test iterations, builds/private corpora/logs, counterexamples and helpers
  are retained. After transfer every regular file hash/size/mode, directory mode
  and link target was independently verified locally.

The first build was stopped only in its owned process tree when future archive
timestamps repeatedly regenerated CMake against the guest's clock. Only private
extraction mtimes were normalized with contents checked; neither clock changed.
Two partial quota-failed transfers remain. The second started before a pending
retirement verification had completed; the later successful transfer follows its
terminal result. Only nine duplicate final Save UI PPMs, 126,489,753 bytes, were
retired after checking every file against the frozen, locally verified Save UI
archive. Its archive/local copy remain; the precise retirement receipt is in
this bundle. The initial collector also found no OpenSSL CLI; the retained
helper was corrected to query available pkg-config metadata, without installing
dependencies or changing executed tests.

## Adoption and remaining work

Aetheri may evaluate this optional observation model against the pinned original
contract while its own app/backend and SDK pin continue independently. No live
Aetheri source, catalogue, quarantined fixture or consumer pin was changed.

L11/I07/P05/P07/Q02 gain scoped ownership/correlation/failure evidence; D11 gains
a real recovery-data foundation. The next step remains the native virtualized
list/filter/detail composition, keyboard/IME/accessibility and view/provider
retirement. No native recovery window, production worker/IPC service, complete
device/software inventory, recovery/admission action, installer/trusted surface
or consumer adoption is closed here. Packet 6 and packets 7–8, physical/human
acceptance and all other open rows of the 103-ID catalogue remain active.

Subsequent component composition: [source 88d12b4](recovery-view-report.md) adds
list/filter/full read-only details through public frame APIs. Its native
host/worker/IME/assistive acceptance remains open. The earlier published
foundation documentation `97a42647480bb05167cee5e4ae08cedd9147d80d` completes
[CI42378557292](https://github.com/Lulus792/NexaUI/actions/runs/42378557292)
with all seven required jobs successful; optional Windows-ASan/fuzz are skipped.
That root SDK matrix does not execute these separately pinned optional examples.
