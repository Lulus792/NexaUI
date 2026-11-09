# Shared-cluster character geometry with interior carets

SDK0.1.30 development, source `aa899fbd7b95a975391a29dfd01b05b39e6b4c05`,
9 November2026. This corrects the copied document presentation prerequisite
introduced in `1ee9d99540587494a0667e2c41caf63e6fd27e14`. Original rendered
Editor geometry/font adoption remains the next owner integration.

## Actual failures and bounded correction

A shared ligature rectangle may cover bytes `[0,2)` while a real insertion
caret lies at byte1. The old validator rejected the sorted records
`(0,2),(1,1)`. Merely accepting that caret exposed a second error: character
lookup searched end offsets, which are no longer monotone when a caret lies
inside the preceding cluster. The second character's geometry disappeared.

Validation now checks strict `(start,end)` ordering and tracks the previous
nonempty end separately. Nonempty ranges still cannot overlap. Character
lookup searches sorted starts, skips caret-only predecessors and checks the
first preceding nonempty range. Range and point queries continue to use their
existing rules. No public declaration, version macro, allocation scheme or
resource budget changes. Existing539 Linux `nx_` definitions are retained
without additions or removals. The public comment clarifies interior carets.

Three actual compiled negative states are retained: the original rejected
presentation, the validation-only repair with missing character geometry,
and the final new regression linked against the previously installed1ee9d99
Release static library. The latter compiles/links successfully and exits1 at
the valid presentation assertion. The two WIP builders had no embedded source
revision; their source/library/executable/command bytes identify those controls.
The rejected source matches the old commit; the partial repair's validator
matches the final code while its lookup still matches the old code.

## Executed matrix and native behavior

1082 exact committed Git blobs feed three new installed builds: GCC15.2 Release
static/shared and Clang22.1.3 Debug ASan/UBSan on Alpine/WSL2. Each runs23 selected
CTests, including four actual private Wayland/AT-SPI tasks. All69 tests and three
external installed-package consumers pass. Every one of the276 built SDK,
fixture and package-consumer C objects explicitly uses C17 with extensions
disabled and warnings as errors. The entire sanitized graph retains ASan/UBSan
and leak detection. These selected tests do not replace the separate full CI.

The direct regression checks minimal and richer shared-cluster/caret lists,
multibyte UTF-8, explicit missing geometry, point/range behavior, duplicate
carets, backward ordering, overlapping nonempty ranges and continuation-byte
offset rejection. The independent OS libatspi peer obtains the same rectangle
for both characters of the synthetic `fi` cluster and a distinct interior
caret, checks a sparse character and hit test, then performs a real native
EditableText change from `fizz` to `fiZZ`. Text and geometry are checked again
after the admitted edit's new document revision. Original byte/scalar and
selection/clipboard/cache tests remain in the same selected matrix.

The native environment reuses the owned private Weston14.0.2/Xvfb fixture,
private D-Bus/registry and libatspi2.60.5. Its rectangles remain synthetic owner
data; no original Editor rendered-layout coverage is inferred from them.

## CI, raw evidence and independence

Exact-source [run42591253299](https://github.com/Lulus792/NexaUI/actions/runs/42591253299)
initially passes all six regular platform jobs. The sanitizer job exits before
its suites because the pinned Mesa prerequisite download raises
`ConnectionResetError: [Errno104]`. The unchanged failed job is retried
separately; its first log remains preserved. Attempt2 passes all seven required
jobs: Windows145/144, macOS121/120, Linux267/265 and sanitizer267 in each of two
driver orders. The static Linux job also passes its separate one-test repeat.
No CTests skip; two optional Windows-ASan/fuzz jobs skip. The six regular-job
logs are byte-identical reused attempt1 evidence, not six new executions.
All package/close-request/add_subdirectory consumer steps remain successful.

[Local commands and compiled artifacts](document-interior-caret-local.json),
[source/header/export binding](document-interior-caret-source-binding.json),
[checks](document-interior-caret-checks.json),
[CI receipt](document-interior-caret-ci.json) and
[archive ledger](document-interior-caret-archive.json) retain the exact inputs,
outputs, negative controls and limits. The raw archive verifies1748 files and
six links, including the installed old libraries used for negative/export checks.

Aetheri independently advances to `54afa682edc368ec54db19c7dc88a948617cbbe1`;
all53 consumed original files remain byte-identical to frozen7bbe704. No active
Aetheri source, pin or build is changed. Consumers need this exact fix or a later
containing revision; the0.1.30 version alone does not identify it.

Separate preparation for that owner integration builds the same source in the
Fedora/glibc VM. Release and Shared each pass19 core/protocol CTests and their
installed C17 consumer. The additional VM ASan build stops during linking with
`No space left on device`, before tests/install. Its failed commands/logs and
the two completed preparations are preserved in the host evidence archive.
These preparations are not counted in the accepted69-test matrix above; the
VM ASan environment must be resolved before its future owner task is claimed.

Actual original Editor layouts/carets and truthful font/style metadata,
scrolling/IME/review transitions, native AX/UIA presentation, broader text
interfaces, physical/human/design/performance and ordinary Aetheri adoption
remain open. All103 catalogue IDs and remaining packets6–8 persist. No1.0 is
released or accepted by this step.
