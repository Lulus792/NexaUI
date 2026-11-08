# Reservation and original lifetime receipt foundation — 2026-11-08

Consumer source: `45cdbba4832e0e720bd12d9a15e6ca4277df216e`.
Original Aetheri: `46e2a21ef979d9a76db2a8487d1de167a6a79ef7`.
Installed SDK: `c4f467d85b0147944d2456f5e7602a39ac18e579`, NexaUI 0.1.23.
Catalogue SHA-256: `9c932872caeba8007bf05b86f7c167b493260709b23c54d709222a5f19539e65`.
Scoped D07/L11/P04/P05/P07/Q02/Q04/Q08 data-foundation evidence.

The existing [selected-app history worker](../../examples/34_aetheri_software/README.md#reservation-and-original-lifetime-receipts)
now copies original reservation/dispatch metadata and original-runtime local
lifetime receipts. This step adds no receipt page. It reuses immutable batches,
the bounded worker and existing native UI; the following presentation step stays
open. Core/public API/ABI/version and Aetheri source/pin remain unchanged.

## Original read contracts

`ae_catalogue_get_receipt` already includes dispatch metadata; no duplicate
dispatch query is needed. Its context uses the admitted current Root runtime/boot,
selected account/UID and explicit profile broker epoch/runtime. These fields do
not authenticate a current broker/process. The returned `system_runtime` is the
current query Root, while `profile.system_runtime` remains the original execution
runtime. Replacement is never relabelled as a fresh execution/adopted owner.

Source epoch/runtime/boot and requested lease must match exactly. Flags, optional
invocation/cause values, canonical launch-derived unit, IDs, bounded known state
and positive revisions are checked. Lookup by lease alone is insufficient: request
fingerprint, stored broker runtime, operation and launch must also match the
explicit profile. A different request returns per-field `-EXDEV` and zero data;
missing/unsupported results retain their original negative errno. Intent, plan,
cause and withdrawal are copied metadata, not current authority or live state.
The original API does not prove every execution-profile field or actual provenance.

`ae_catalogue_get_run_receipt` binds the full original execution profile and
retained starter invocation/context evidence. The original Root SQL requires
the original runtime; replacement gets explicit `-ESTALE` without entering
recovery APIs. Missing retained starter evidence yields `-ENODATA`. No PID lookup,
invented invocation or foreign User database is used. Root-local closing,
counterpart quiet and release receipts retain every field plus their original
canonical digest. The original codec/matcher validates profile/invocation/kind,
revision and timestamp. Later stages must reference the preceding receipt digest;
the first previous digest remains opaque original start-result metadata.

Unavailable snapshots and hashes are zero, with independent error fields.
Unexpected I/O/allocation/conflict/context/corruption errors or malformed returns
reject the entire newly allocated batch and preserve caller output. A valid
prefix never becomes a complete result. The existing worker separately retains
valid selected-app observations when history fails. Before/after source context
and row/revision fences remain; this is no cross-query transaction or transport
authentication. Historical release/quiet/withdrawal records establish no current
holder drain, process/image ownership, permission, cleanup or replay right.

## Reproduction and checked coverage

The optional example's installed-SDK CMake build registers
`software_history_receipts` without requiring a test font. Its driver prepares
nine unchanged original metadata fixtures: completed history, Root replacement,
older schema, withdrawn reservation, unit, unit replacement, custody, custody
replacement and an explicitly corrupt process witness. The withdrawal fixture
is original `exec-user-prepare-fresh`; it uses the same `run` request and leaves
the User operation unsettled. No native execution/service harness is enabled.
Both private Root and User database bytes and all rows remain unchanged by reads.

All three profiles—static Release, shared Release, ASan/UBSan/LSan—pass **9/9
CTests**. The new test supplies **27 receipt scenarios** across the profiles;
existing suites retain **33 reader, 12 worker/view and 21 unit/custody scenarios**.
Additional cases inject source/lease/ID/flag/state/unit faults, request mismatches,
unavailable poisoned outputs, receipt kind/profile/invocation/revision/time/chain
faults and errors after a successful prefix. Worker wrappers assert non-GUI
queries. Both reservation and final-release read failures preserve independently
valid app replies; successful history detaches once and survives original owner,
message and worker disposal. Descriptor balance is checked.

Three actually compiled production-guard omissions—reservation validation,
previous-digest comparison and unexpected receipt-error propagation—are detected
by the new real tests. Their variant sources, commands, binaries and intended
failures remain in the artifact. They are separate counterexamples, not passing
normal implementations. All existing 64-profile/copy/row/revision/error/lifetime
tests remain in the full suite.

Seven native virtual-Wayland/independent OS AT-SPI regressions pass: completed
history on all three profiles, plus replacement, unsupported, corrupt and no
selected profile on Release. They retain keyboard/paging/focus/retired-reference,
explicit refresh, unchanged database and normal shutdown checks. The unchanged
unit/history peer is byte-source checked against its preserved source. Two
additional Release/ASan native IME/manifest regressions each pass ten checkpoints
and 84 content commands. These verify the existing pages after the new worker
reads; they do not claim a receipt UI, installed production IME, spoken reader,
physical display or performance acceptance. No visual output implementation is
changed in this step.

## Sources, failures and artifacts

The [local receipt](software-receipts-local.json) retains commands, actual exits,
database comparisons, binaries, shared-library resolution, native/legacy results,
counterexamples and storage proofs. The [source binding](software-receipts-source-binding.json)
checks all **66** final inputs against exact Git blobs after CRLF-to-LF
normalization, with no post-test code differences. Final input4 SHA-256 is
`52c2b4acbb771c9d1680f46e7dc37d34125ca9bb659af473158404bedfb83d92`.

Earlier attempt1 failed strict compilation on test-only misleading indentation.
Attempt2 built and passed the eight existing groups but the new test tried to
open a worker while its original handle still held the same-owner lock. It now
closes that handle before transferring ownership. Attempt3 passed the first new
history/replacement/unsupported cases but its withdrawal preparation used the
unrelated original `main` fixture. Original intent-digest validation rejected it.
Input4 selects the existing `run` preparation/withdrawal scenario. No original
source/assertion, sanitizer or production failure policy was weakened.

The initial attempts artifact remains at
`D:/NexaUI/out/nexaui-software-receipts-attempts-20261108.tar.gz`, SHA-256
`60c539721d6570a7046f5df4bd32f3b8e4697ea2073a9653c8d761b174568920`
(718 files, one link, 280 directories, one excluded FIFO).
Attempt3 remains at
`D:/NexaUI/out/nexaui-software-receipts-attempt3-20261108.tar.gz`, SHA-256
`2b1fe3ea1989b60878558e8bddcd85a4ec3ce82c0ff5dfb70dae32afb8ffaa7a`
(628 files, one link, 215 directories, one excluded FIFO).
Both complete manifests and member hashes/modes/links were verified on the host
before retiring only their exact duplicate VM build directories. Earlier unit
build duplicates were likewise compared against the fully verified authoritative
unit/custody host archive. No quota, mount, global cache or system limit changed.

The [final archive](software-receipts-archive.json) is
`D:/NexaUI/out/nexaui-software-receipts-20261108.tar.gz` (57,963,445 bytes), SHA-256
`dc54145db455addaf2217d2f81f446cf0917ba27dea8b401da8ae2c02c40ee93`. Its full manifest covers 2466 files,
7 links and 841 directories; explicitly
excluded FIFO/special nodes remain listed. It retains final source/builds,
earlier frozen attempts, all raw test/native data, helpers, SDK prefixes,
source binding and storage receipts. Original source/compositor/OS dependency
provenance remains in the [manifest baseline artifact](software-components-report.md).

Previous main `cfc03f8dc4296e7f1fbe9511e8d3d07949603908` has seven required CI
successes and two optional skips in
[run42427945078](https://github.com/Lulus792/NexaUI/actions/runs/42427945078).
The [API metadata](software-unit-links-ci.json) is not raw-log review, new-source
CI or coverage for the optional receipt consumer. This report's consumer claims
come from the retained local installed-SDK campaign.

## Remaining scope

Receipt presentation, broader operation/diagnostic/reason links, actual current
process/image/resource/ownership providers, full discovery/inventory, device/
permission/trusted-service composition and software-specific held-query/
clipboard/resize acceptance remain. Aetheri independently remains at
`1441b636eb14dbbed36e4e2825b2f44b16599a01`; no consumer source/pin was edited,
built, reset or pushed. All 103 catalogue IDs and the remaining packet6,
packets7–8 and physical/human/performance gates stay active. No 1.0 release or
whole-goal completion is declared.

## Subsequent presentation

Source `0769fab` opens the retained data in a native read-only Receipts
page. See [receipt presentation evidence](software-receipt-view-report.md);
further owner/service and broader catalogue acceptance remain open.
