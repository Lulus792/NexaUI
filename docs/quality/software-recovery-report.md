# Profile-bound selective recovery data foundation — 2026-11-08

Consumer source: `226a847b6655ec39ebf6427ab95d265ea82f9a0d`.
Original Aetheri: `46e2a21ef979d9a76db2a8487d1de167a6a79ef7`.
Installed SDK: `c4f467d85b0147944d2456f5e7602a39ac18e579`, NexaUI0.1.23.
Catalogue SHA-256: `9c932872caeba8007bf05b86f7c167b493260709b23c54d709222a5f19539e65`.
Scoped D07/L11/P04/P05/P07/Q02/Q04/Q08 evidence in packet6.

The [software history worker](../../examples/34_aetheri_software/README.md#selective-recovery-relationships)
now copies five selective original recovery relationships for each admitted
historical execution profile. This is a data foundation only: no new Recovery
page is delivered here. The existing Receipts page supplies regression coverage.
Public SDK API/ABI/version, Aetheri source, active runtime and consumer pin are
unchanged. Independently observed Aetheri HEAD at publication preparation was
`7483286bcaafbeaced5831f72cb4d0e0cad9a158`; it is not the exercised source.

## Reuse, identities and unknown state

The private example34 adapter calls actual `ae_catalogue_recovery_inventory`
once per batch with eligible profiles, inside the existing before/after source
context bracket. It reuses example33's `rv_owner`/`rv_snapshot` validator and
full-kind/full-ID lookup rather than adding another inventory validator. The
owner/snapshot are created, read and retired entirely on the serialized worker.
Only flat records are copied into the existing owned history reply; no snapshot
reference count or catalogue object crosses the GUI/worker boundary.

The inventory must match exact revision, authority epoch, boot and runtime of
the selected app's current query context. Each profile must already match its
original reservation by request fingerprint, broker runtime, operation and launch.
The adapter selects these original records and checks their related identity:

| Role | Kind and full key | Required related ID |
| --- | --- | --- |
| Root operation | operation, profile root operation | install |
| Code lease | code lease, profile lease | generation |
| Dispatch | dispatch, profile launch | lease |
| Run closure | closure, profile launch | Root operation |
| Execution handoff | handoff, profile launch | none |

The reply carries query schema, overall recovery error and independent role
errors. A row absent from the selective inventory stays `-ENOENT` with a zero
record: unknown, not completed, drained or proven absent. Unsupported schema
retains `-EPROTONOSUPPORT` with zero snapshots. Ineligible profiles are not queried.
Unexpected I/O/allocation, malformed inventory, wrong relation or context drift
rejects the entire history result; independently valid app observations remain
available through the existing worker reply. This is no multi-call transaction,
current-process observation, lifetime capability, ownership, permission or replay
authorization. Reasons are original bounded metadata, not invented diagnoses.

## Executed verification

| Profile | Actual CTest result |
| --- | --- |
| Static Release | 10/10 in the complete final run |
| Shared Release | 9/10 in the complete retry, then the one remaining component test passes 1/1 after quota recovery |
| ASan/UBSan/LSan | 10/10 in the complete final run, leak detection enabled |

The Shared result is explicitly a 9+1 scoped retry. All eleven binary hashes,
source and build flags are unchanged across the final targeted retry. Both
preceding Shared runs retain their quota failures; no full-green Shared run is
invented. The new sanitized recovery test takes464.04seconds within its existing
600second deadline. No timeout, assertion, sanitizer or system limit was relaxed;
this is not a performance acceptance result.

The new test covers nine original fixture modes on each profile (27 scenarios):
history, replacement, unsupported, withdrawn, unit, unit replacement, custody,
custody replacement and corrupt process witness. Original Root/User database
bytes and every row are unchanged after reads. A separate linear reference walks
the actual inventory and compares every copied struct byte and missing/error
output. Injected I/O/unsupported/OOM, context/runtime/revision drift, bad counts,
UTF-8, kind/order and related-ID faults exercise refusal paths. A64-profile
batch makes exactly one inventory query; no eligible profile makes none. Caller
profile mutation during the original callback cannot alter the copied request.
The original conflicting-unit case retains its `-EEXIST` refusal before inventory.
Worker-only reads, independently valid app replies on history failure, detached
batch lifetime, exactly-once disposal and descriptor balance are checked.

Three actually compiled adapter defects are detected by the unchanged test:
wrong lease kind (fault0), omitted related-ID validation (fault6), and omitted
revision equality with the other context fields retained (fault11). Variant
source, build commands, binaries and exact diagnostics remain in the artifact.

Five existing native receipt-page regressions pass on the virtual Wayland
compositor with an independent OS AT-SPI peer: history on all three profiles,
plus Release replacement and unit. The peer's two source inputs are unchanged
from the earlier receipt-view checkpoint. These checks cover the existing
presentation and normal retirement after the larger worker reply, not a new
Recovery page, installed production IME, spoken reader, physical display or
performance. No new visual output implementation or screenshots are claimed.

## Retained attempts, storage and exact artifact

Input1 failed to compile a test that used a nonexistent worker startup name;
input2 compiled and passed9/10, with its new test incorrectly reusing a real
unit operation across synthetic maximum profiles. The original getter correctly
refused that conflict. Input3 fixes the test identities, retains the explicit
conflict refusal and passes the scoped matrix above. Production adapter code
is unchanged by those test corrections. Initial native setup ran before upload
completed and could not open its helper (exit2, no app started); after the same
upload completed all five cases passed. The first result collector assumed the
wrong worker/view result filenames; the corrected collector reads the existing
outputs, without rerunning tests. All failed logs remain retained.

Quota recovery retired only verified archived VM duplicates:83 directories,
eight VM archive-file copies and five older Root11 build directories. Authoritative
host archives remain intact. Files, bytes, modes, links and directory sets were
checked before retirement. The five builds additionally held one generated
`link.d` each, added after their original archive; all five full byte sequences,
SHA-256 and modes were preserved before retirement. No SDK prefixes, new builds,
failure data, original source, VM limits, mounts or clock were changed.

The [local receipt](software-recovery-local.json) retains actual commands,
exits, all profile results, binaries/linkage, native results, compiled defects,
failed attempts and storage proofs. The [source binding](software-recovery-source-binding.json)
checks all71 final input files against exact Git blobs and archive bytes, with
CRLF-to-LF normalization only and no post-test code differences. The final
document verifier is retained in full with its hash in the local receipt. Its
initial strict live-file check correctly noticed the appended README section;
the final check compares all71 exact feature Git/archive inputs and all live
code, permitting only that additive documentation section. The artifact retains
the earlier verifier helper as an attempt, not as the final document assertion.

Host artifact: `D:/NexaUI/out/nexaui-software-recovery-20261108.tar.gz`,
59,750,992bytes, SHA-256 `a742a3419ec6ab3290933492eeb5cfa38e98084ed193d59e1f75f1ee469dd67f`.
[Archive receipt](software-recovery-archive.json): 4699files,
5links and 1495directories. Every archived
regular-file hash/size/mode, symlink target and directory entry is verified on
the host. It includes all three source/build attempts, test data, controls,
native results, helper scripts and original46 source. Borrowed fonts are not
redistributed. The paired [Root11 component artifact](recovery-root-v11-archive.json)
supplies unchanged installed SDK prefixes; the paired
[receipt-view artifact](software-receipt-view-archive.json) supplies the unchanged
OS peer/source used in the five native regressions.

The preceding main `64ae5497ca988f3879a5b7d71e2df487010b84e8` has
[seven required CI successes and two optional skips](recovery-root-v11-window-ci.json).
That CI receipt applies to its own source, not these new adapter changes.

Next implement a separate keyboard/read-only Recovery-links page using existing
controls, full Text, scroll/focus and retirement, rather than overflowing the
existing8192-byte Receipts body. Broader Files/device/software/trusted-system
compositions, packets7–8, actual current source/adoption, performance and final
physical/human gates remain open across the unchanged103 requirements.
