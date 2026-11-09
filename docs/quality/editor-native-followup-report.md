# Original Editor native follow-up on SDK0.1.33

Consumer/test source: **`02ba99ebd5c284b27af5b615b1f99751f31eb3b8`**.
Installed SDK: **0.1.33 / `1e00a6f4f3a0ab3a7da1aa35a36f02679421366b`**.
Original Editor: **`7bbe7042ecd48582b11ef7053a4dbeade6b02351`**.
The SDK's production headers/core/platform/renderers/package helpers are unchanged
by this consumer increment. Its previously verified libraries are reused; no new
SDK runtime, public API/ABI, dependency or version is introduced.

This follows the [partial attribute checkpoint](editor-attributes-report.md).
The old VM remains unavailable. A new private unprivileged musl/WSL environment
now executes the complete original native tasks, including the held-EOF cases,
through source-checked original compositor code. It does not establish the cause
of the inaccessible earlier VM failure or replace actual Aetheri desktop/hardware
acceptance.

## Changes and actual diagnosis

The native runner accepts the checked0.1.32/0.1.33 SDK receipt formats while
preserving feature floors, exact library hashes and unknown-version refusal.
The shared library filename must match the receipt version. Four actual negative
preflights reject altered hashes, mixed versions, unknown0.1.34 and an insufficient
attribute floor before starting an application.

Newly captured old0.1.31 native failures are retained. The unoptimized sanitizer
run times out on its first complete held-EOF paste after1,001,269us. Shared0.1.31
and the first current0.1.33 runs progress farther but exceed the unchanged
one-second client deadline during later invalidations. They are failed runs,
not evidence of premature successful application before EOF.

A diagnostic current-SDK run records actual provider replies on a separate
private-bus monitor: later false replies arrive after1,035/1,229/1,118ms. The
Python profile records1,555 complete log reads costing8.597 cumulative seconds
in a41.211-second instrumented driver; repeated full-history regex searches and
multiple `view()` calls per state predicate contribute additional controller
work. These nested timings must not be added as independent totals, and they
are not application performance measurements.

The generated driver now locates the last nonempty frame record directly and
checks all requested state fields in that one observation. Full logs remain
available. A regression compares12 complete histories and60 state predicates,
including Unicode, old frames, empty/trailing records and missing fields.
An actual old-helper counterexample combines `docs=1` from one frame with
`cursor=0` from another although neither frame has both; the new helper refuses
that false positive. Three changed-source boundaries are also rejected.
The regression runs in the standalone consumer and, when Python is available,
the SDK CTest suite.

No native event, original task, capture, peer deadline, EOF assertion, CPU/frame
guard or sanitizer is removed or relaxed. The original source stays immutable;
the generated substitutions remain in each `driver-adaptation.json`. On the same
current SDK and owner binaries, the revised driver completes Shared and sanitizer
tasks; the complete final matrix is separately rebuilt from frozen committed
inputs. This supports the controller correction for these captured runs. The
earlier VM's unsaved early reply remains unattributed.

## Restored private test environment

All new work runs as UID65534 in Alpine-Dev/WSL, Clang22.1.3. The exact original
test dependency versions are Wayland1.26.0, protocols1.49, wlroots0.20.2,
pixman0.46.2, libdrm2.4.134 and system xkbcommon1.13.1. They are installed only
under a private test prefix. Meson1.10.2 is privately unpacked; no system package,
driver, quota, mount or running Aetheri environment is changed.

The first strict producer build detects that upstream wlroots lacks the original
private clipboard ABI. A separate copy applies Aetheri's original clipboard-context
and pointer-enter patches from `d56d69523c33a48648af343e7d36526f84518c90`.
Every touched file matches the original graphics manifest's before/after hash.
The unchanged C17 producer then configures/builds successfully with its original
strict warnings. It supplies private headless software outputs and a controlled
seat, not a trusted session or physical display.

The exact previous `wl-copy`, `wl-paste`, musl loader, supporting libraries and
foreign-only stream gate are restored from the verified stream44 archive.
No foreign binary or system libatspi is patched. The canonical original Sans
font matches manifest SHA256
`76d04c18ea243f426b7de1f3ad208e927008f961dc5945e5aad352d0dfde8ee8`;
the original mono input and borrowed proportional/ligature test fonts retain
their recorded hashes. Received source/RPM/wheel hashes are recorded; no fresh
signature verification is claimed. Font bytes and the font RPM are excluded
from the new raw archive.

## Final executed evidence

| Evidence | Final executions and bindings |
| --- | --- |
| Consumer CTests | Release static/shared and Debug-O1 ASan/UBSan/LSan22 each; attributes-OFF shared20: **86** |
| Root observation CTest | **1** additional executed test on exact committed02ba99e; no SDK objects built in that check |
| Compiled inputs | **110** exact Git inputs, **133** C17 owner/independent-peer objects, **28** byte-identical generated C replays |
| Native workflows | **4** complete runs: three enabled profiles and attributes-OFF shared |
| Native captures | **336** complete compositor captures,84 per profile |
| Held EOF | All seven cases per profile; **104** forced pre-EOF frames preserve the original model |
| Omission control | The exact successful OFF binary fails when the independent reader demands font/style attributes |

Thus87 newly executed local CTests are distinct from reused SDK prerequisite
tests and from the earlier exploratory executions. See the
[local receipt](editor-native-followup-local.json),
[source/object binding](editor-native-followup-source-binding.json),
[image comparison](editor-native-followup-images.json) and
[raw archive ledger](editor-native-followup-archive.json).

Each positive native task retains the original large-document/history/save,
private IME, independent AT-SPI/cache, selection, geometry,
scale/style, Copy/Cut/Paste, source-stop, partial UTF-8, review/tab/preedit
invalidation and repeated native-close work. The foreign writer stays open
after3 and then12 consumed bytes; only actual EOF permits the complete paste.
Native Save and process exit finish while the close-task writer remains open.
The original1.2-second idle CPU guard passes. All14 native paste replies per
positive task arrive before the unchanged one-second client deadline without
a timeout error. The three enabled profiles additionally execute the native
font/color attribute tasks; the OFF profile deliberately omits those attributes.
The final sanitizer profile keeps leak detection enabled.

The attributes-demanded OFF negative reaches the real independent OS query and
fails at `default/run values`, with attribute count0. It uses the same OFF
application hash as the successful normal task. Its failure cleanup is not
counted as successful normal retirement or sanitizer acceptance.

Read-only Git comparison at Aetheri
`192479d39dde37989f1eb4de82bc316cfcca7b5d` finds all53 consumed original inputs
unchanged. No active source, SDK pin, default, build or process is modified.

## Pixel comparison and remaining visual evidence

All336 compressed captures decode to their complete declared RGB buffers and
match the original raw hashes. Of252 complete cross-profile pairs, **245 are
byte-identical**. No region is masked and no tolerance is used.

Six pairs contain the independently created temporary directory label in the
Save dialogs. In the sanitizer native-Save pair, additional cursor/border pixels
also differ. The seventh pair is the sanitizer unsaved-close review: the
reference shows the Cancel focus outline while that capture does not. Complete
lossless PNG exports and exact changed-pixel counts/bounds remain in the image
receipt and raw history. Visual inspection confirms these differences; it does
not approve the focus discrepancy or establish its cause.

The direct Shared-ON/OFF comparison is82 exact images and two temporary-path
label differences. That is scoped evidence that enabling copied attributes does
not change these view pixels. Full cross-profile focus/caret capture parity,
physical/human visual acceptance and the cause of the focus-ring difference
remain open. No full visual match or application speedup is claimed.

## CI and adoption boundaries

All seven required jobs pass on exact source02ba99e in
[run42708514559](https://github.com/Lulus792/NexaUI/actions/runs/42708514559),
attempt1: Windows static/shared148/147, macOS124/123, Linux272 plus one separate
repetition/270, and272 tests in both sanitizer driver orders. The new observer
regression executes on all three platforms. No CTest is skipped; optional
Windows-ASan and fuzz-campaign jobs are skipped and supply no new coverage.
Installed/package, close-request and source-tree consumer checks pass. Seven
complete decoded logs and final head-SHA/job metadata are bound in the
[CI receipt](editor-native-followup-ci.json) and
[CI/documentation supplement](editor-native-followup-ci-archive.json).
These generic SDK jobs do not execute the exported original Editor native matrix;
its source-bound local evidence is recorded above.

The optional original model/native composition is usable for this checked
development scope. Actual Aetheri compositor Q04, normal application adoption,
external installed IMEs/screenreaders, broader font/fallback and text interfaces,
UIA/AX, hardware/physical/human/design/performance and remaining packet6/packets7–8
acceptance remain open. All103 catalogue IDs/statuses persist. No1.0 release.
