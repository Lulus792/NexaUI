# Unchanged original Files consumer: candidate build and controller/view checks

Consumer adapter: `23c0699960d67812d59fd60e5eec690d19a37d21`, 2026-11-08.
Original Aetheri: `7483286bcaafbeaced5831f72cb4d0e0cad9a158`.
Installed SDK: `c4f467d85b0147944d2456f5e7602a39ac18e579`, NexaUI 0.1.23.
Catalogue SHA-256: `9c932872caeba8007bf05b86f7c167b493260709b23c54d709222a5f19539e65`.
Scoped B01/B03/B04/D07/L05/L06/L11/I01/A06/P04/P05/P07/Q02/Q04/Q08 evidence.

The [new independent consumer build](../../examples/35_aetheri_files_consumer/README.md)
compiles the original Files model/controller, existing NexaUI view, native host
and original session/view tests. There is no second Files implementation or
copied application code in the SDK. All source is borrowed from a separately
exported, checked immutable Git snapshot; no active Aetheri checkout is built.

## Candidate scope and original ownership

Aetheri's own `spec/nexaui-inputs.json` selects its reviewed SDK source
`97a42647480bb05167cee5e4ae08cedd9147d80d`. That selection is preserved. This
separate experiment explicitly selects SDK `c4f467d` and checks its installed
CMake source declaration rather than invoking or editing Aetheri's reviewed-pin
build. A source declaration is not a complete provenance attestation; original
Git/archive bytes, actual binaries, compiler commands and paired SDK artifacts
remain bound separately. This is candidate compatibility, not approved migration.

The standalone CMake graph uses only the original grapheme/document/file,
Files model/session/view sources and public installed NexaUI API. It does not
configure unrelated authority/boot services or replace their dependency policy.
All 15 compiled translation-unit entries are checked against their exact original
paths, with C17, extensions off and strict warnings as errors. The ordinary host
links no SDL, X11, Vulkan or EGL library; that statement applies to this host,
not to separately launched original Terminal/editor children.

The original controller retains real local creation/rename/move and unknown
outcomes. Preparation has no effects; its existing operation/worker ownership,
one-attempt behavior, source/held-parent identity and direct-document-child
close fences are unchanged. The SDK neither acquires a file grant nor decides
authorization. A held development directory is not a persistent volume identity,
hostile-writer exclusion or normal installed app/session admission.

The ordinary native host and optional lost-reply host are separate executables.
Symbol checks show no `__wrap_*` definitions in the ordinary host; only the
explicit fault target contains `__wrap_mkdirat`/`__wrap_renameat2`. Building that
target does not execute its hooks or install it as an application.

## Executed checks

| Installed SDK profile | Original tests | Result |
| --- | --- | --- |
| Static Release | Files session, view 1×, view 2× | 3/3 |
| Shared Release | Files session, view 1×, view 2× | 3/3 |
| ASan/UBSan/LeakSanitizer | Files session, view 1×, view 2× | 3/3 |

Each original view test executes 128 layout checks: 768 across both densities
and the three profiles. The unchanged source covers stale snapshot/selection/
dialog/sidebar scopes, modal keyboard and 200% bounds, minimum layout and
whole-commit guards. The original session test exercises deferred work, failed
worker start, real filesystem create/rename/move, controlled lost-mkdir reply,
unknown outcome, rejected intent and document/close fencing. These are original
test assertions, not a newly written mirror of the application implementation.
The view deadlines remain 30 seconds; sanitized 1×/2× tests take 4.49/5.40 seconds.
Leak detection is enabled; no timeout, assertion or sanitizer is relaxed.

Four independent admission checks reject a changed original view source,
a simulated live-checkout marker, an indirect source symlink and a wrong
40-character SDK declaration. Each requires its specific diagnostic and a
nonzero actual exit. Three profiles additionally check normal/fault symbol
separation, exact compilation entries/flags and library resolution. The Shared
host resolves the selected `/sdk-shared-prefix/` library. After testing, all 46
original source inputs are rechecked successfully.

The first extraction preflight stopped before CMake: Python's safe tar data
filter removed archive group-write mode bits, while the helper initially
expected the raw Git-tar mode. The corrected helper checks all file bytes first,
then restores only recorded regular-file modes inside its owned private export.
It rechecks modes and sets extracted file mtimes to 0 to avoid future timestamp
rebuilds. The original tar retains its metadata. No VM clock or limit changes;
the initial failure log is retained and the complete build/test matrix then
passes without retries of a failed CTest.

## Exact inputs and artifact

The [consumer binding](files-consumer-source-binding.json) checks all four adapter
files against exact tested input/Git/archive bytes, with CRLF-to-LF normalization
only and no post-test code differences. `source-inputs.json` separately binds 46
unchanged original files to Aetheri `7483286`; every file matches its Git blob,
raw archive bytes and extracted mode. Archive `aetheri-files-7483286-20261108.tar`
is 747,520 bytes, SHA-256
`db0effa4f71b2b7341d058f260a270d8c0e0aaf5527049596a1789c7f9205be0`.
The active checkout's relevant Files/editor/text inputs had no local differences
when surveyed, but this experiment still uses the committed export only.
At publication, the independently advancing Aetheri checkout is at
`c4199abacdd1048574d2d3f92c053eefa5b87a09`; a read-only Git comparison finds
no differences in these 46 inputs between that commit and the tested snapshot.
The executed source binding remains `7483286`, not a build of the newer checkout.

The [local receipt](files-consumer-local.json) retains actual commands/exits,
all three CTest/LastTest logs, compile commands, binaries, source recheck,
negative diagnostics, linkage, font identity and extraction history. The
borrowed font is 410,712 bytes, SHA-256
`76d04c18ea243f426b7de1f3ad208e927008f961dc5945e5aad352d0dfde8ee8`;
it is not redistributed. All four host/test executable hashes per profile are
bound to their archive entries. No SDK API/ABI/version is changed here.

Host artifact: `D:/NexaUI/out/nexaui-files-consumer-20261108.tar.gz`,
9,160,134 bytes, SHA-256 `d9723934970bc6419cc975b45d51b0d191f95a79e10efe9dcfa25c4d37447dc4`.
[Archive receipt](files-consumer-archive.json): 377 files,
1 link, 317 directories. Every regular-file
hash/size/mode, symlink target and directory entry is verified on the host.
Original source/raw tar, all consumer builds, original logs, negative fixtures,
bindings and helper scripts remain in the artifact. The paired
[Root11 component artifact](recovery-root-v11-archive.json) supplies the unchanged
SDK prefixes, avoiding another SDK copy. No previously owned data was retired
for this small build.

Previous main `7217d70503d851f8980fd2bb688aff7b721de6f2` has
[seven required CI successes/two optional skips](software-recovery-view-ci.json).
That job metadata covers its own source, not this new optional external consumer.

## Required continuation

This step builds the native host but does not run a native window, original
Terminal/editor document workflow, IME/clipboard or independent OS assistive task.
Reuse those original private-compositor workflows next with explicitly bound
child binaries and runtime inputs; adapt only the candidate-specific test
selection/provenance check, retaining Aetheri's own pin and all behavior/assertions.
The original Files view explicitly rejects tagged IME edits pending a host-owned
target/revision binding; this build does not add that integration. Original
untagged whole-commit guards are narrower than native IME acceptance.

Physical/human/performance and actual consumer adoption remain open. Remaining
device, permission/login/lock and installer services require their real owner
observations/admission; the similarly named original setup-only execution API
is not an OS installer or trusted login provider. Broader packet 6, packets 7–8
and all 103 catalogue requirements remain intact and incomplete.
