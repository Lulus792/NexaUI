# Original historical process observation adapter — 2026-11-07

Continuation: [worker and native historical inspection](software-history-worker-report.md)
is implemented at `755a788`. The reader-only scope and remaining-work notes below
record the earlier `ddf4981` checkpoint.

Consumer source: `ddf4981530afe25cdc6f9a768cfcc8dc63da49af`.
Original Aetheri: `46e2a21ef979d9a76db2a8487d1de167a6a79ef7`.
Installed SDK: `c4f467d85b0147944d2456f5e7602a39ac18e579`, NexaUI 0.1.23.
Catalogue: `NEXT_AETHERI_UI.md`, SHA-256
`9c932872caeba8007bf05b86f7c167b493260709b23c54d709222a5f19539e65`.
Scoped requirements: D07/L05/L11/P04/P05/P07/Q02/Q04/Q08.

The [example34 history adapter](../../examples/34_aetheri_software/README.md#historical-process-observations)
adds original historical process metadata to the selected-app data foundation.
It reuses `ae_catalogue_get_execution_process`, original profile/process codecs
and the existing selected-app model. It adds no native process page, worker
transport or current process/resource/ownership provider. Those remain next work.
SDK implementation/public API/ABI/version and Aetheri's active pin are unchanged.

## Observation contract

The trusted owner supplies at most 64 explicit original profiles and a complete
validated selected-app result on its serialized Root worker. The adapter has no
SQL, inventory discovery, numeric PID lookup, service/system-bus call or mutating
catalogue/launcher method. The existing model's shape validation is exposed as
an application-private helper; validation does not authenticate a DTO or source.

Profiles, scope and selected row anchors are copied before original getters.
Original canonical validation/hash computation rejects malformed profiles;
foreign epoch/account/boot and duplicate launch IDs reject the entire selection
before any original read. Unknown or ineligible selected targets cause no process
query. Before/after source identity/runtime/boot/revision must exactly equal the
selected-app observation. These guards create no cross-query transaction,
authentication or mutation fence.

Each owned record retains both current selected-app metadata and the original
historical execution profile, full digest and recorded starter/app observations.
Full 64-bit pidfs/cgroup identifiers and original timestamps are preserved. Missing
records and unsupported schemas remain explicit per-role negative errno with
zero snapshots. Unexpected I/O, allocation, corruption or context errors reject
the whole batch, preserving caller output and earlier observations. A successful
prefix cannot become complete success. No selected profiles means no selected
profiles, not no processes.

Borrowed ranges require the exact current source, recipient UID, full install
identity and row/revisions. Storage survives original catalogue/owner destruction
while a retained snapshot and batch remain alive. Profile runtime/generation/
artifact can differ from the current selected app; a historical record must never
be silently relabeled as its current execution. Existing original getters allow
same-boot owner replacement without adopting a run. Cross-boot process reads are
rejected by the original contract and this adapter.

A retained PID/unit/cgroup or past observation is no current process, image,
readiness, CPU/RAM metric, ownership, permission, drain or effect-replay proof.
Aetheri already has current process APIs requiring separately admitted held
ProcessFD/system-bus/supervisor evidence. This increment neither supplies nor
substitutes for that evidence. Complete inventory and inactive/uninstalled target
history remain separate owner/service integration obligations.

## Executed checks

Static Release, shared Release and GCC ASan/UBSan each pass **6/6 CTests**. All
five existing selected-app/model/worker/real-font view/manifest regressions remain;
the new test calls real original history getters on private metadata fixtures.
The shared view executable resolves the actual installed shared SDK. No new SDK
core suite or native process-UI/IME/AT-SPI campaign is claimed.

Each profile performs eleven read-only cases: Root v4–v10, one actual same-boot
Root runtime replacement, unsupported Root v2/v3, and an explicitly corrupted
original witness digest. Thus **33 read-only cases** preserve Root/User database
bytes and every table row after preparation. Original migrations and fixture
mutations are separately performed before those equality checks. Original
`test_launch.c` prepares completed/released run history, replacement history and
corruption through its unchanged fixture workflow; no synthetic witness is
claimed to be an actual admitted kernel process.

Tests check all 64 selected profiles, sorted complete digests, malformed final
profiles before queries, skipped unselected/ineligible targets, absent/unsupported
roles, full 64-bit IDs, caller-input mutation after admission, allocation failure,
I/O/ENOMEM/EUCLEAN/ESTALE after a successful prefix, malformed returned records,
UID/revision/gate substitution and retained snapshot/batch lifetime with balanced
FDs. A separately labeled synthetic newer-generation observation verifies that
the historical profile retains its original generation. It is not an actual
backend generation migration.

Four privately compiled counterexamples are detected by intended assertions:
omitted source-context equality, late caller-profile reads, treating an unexpected
getter failure as successful partial metadata, and omitted exact lookup binding.
All variant builds exit 0; each defective test exits 1 as intended. Their private
database bytes remain unchanged. These Release counterexamples are not passing
sanitizer runs. The [local receipt](software-history-local.json) retains commands,
all per-profile cases, binaries, assertions and actual shared linkage.

## Source and artifact binding

All **58** archived example/transitive source inputs match the exact consumer
commit and the tested input3, normalizing CRLF to LF only. There are no post-test
code differences; see [source binding](software-history-source-binding.json).
The existing 67 original inputs and manifest encoder pins remain. Three extra
original fixture/vector inputs are pinned at the same Aetheri revision, including
unchanged `test_launch.c` and its reference driver. The frozen process vector
supplies the profile bytes; the original C harness independently verifies them.

`D:/NexaUI/out/nexaui-software-history-20261107.tar.gz` contains source exports,
current/initial builds, original fixture inputs, test databases, all four compiled
counterexamples, command/results logs, SDK installation prefixes and storage/CI
receipts. Size: 30,994,654 bytes. SHA-256:
`2ce0131145645bf22f4fd9a93c9501e5f03ffb126cf4ba4efff300983b628ecb`.
The full verifier checks all 1,284 regular-file hashes/sizes/modes, six symlink
targets and 484 directory modes. Four explicitly named FIFO fixtures, the borrowed
font and the live archive log/holder journal are excluded. The retained runners
recreate test FIFOs. See [archive receipt](software-history-archive.json).

To stay within the VM's existing user quota, three preceding build directories
were compared file-for-file with the already fully verified
`nexaui-software-contents-20261107.tar.gz` before retiring only their VM copies.
That archive remains unchanged with SHA-256
`274d79b7662db62ec74b556c25ffdfe47396361f5bf7ede174ac57ac2eb73a0d`.
The comparison/retirement receipt is retained in the new artifact. No quota, mount
or system-service configuration changes. Original source/SDK provenance remains
in the earlier verified [manifest/dependency artifact](software-components-report.md).

The previous published main `da8a008856423af6d4d884590ebae0b7d2fe65be` has all
seven required CI jobs successful and two optional skips. This is exact-head
[GitHub API metadata](software-contents-ci.json), not raw-log verification or CI
acceptance for this new source. The optional original-Aetheri example campaign
above is local evidence separate from the root CI.

## Remaining work

Integrate copied explicit profiles and historical batches into the existing
bounded worker, then present complete historical relationships/reasons with exact
IDs, truthful unknown/error state, keyboard/focus and native semantics. Historical
selection must remain distinct from current kernel/service/image observations.
Actual process/resource/ownership/dependency/inventory/device/permission/trusted
service adapters and their lifetimes still require owner-provided evidence.
Further packet-6 compositions, packets 7–8 and physical/human/performance gates
remain active across all 103 requirements. No final acceptance or 1.0 is declared.

The independent Aetheri checkout advanced to
`35a4badb7fcfed4eec0a07381eb93a9e625250c4` with its ongoing edits. Its relevant
surveyed API headers are unchanged from the earlier read at `3026d4d`; this work
does not compile, edit, reset, publish or repin that checkout.
