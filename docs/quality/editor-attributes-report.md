# Original Editor font and selection attributes: partial integration

Consumer source `6d1f2739276bd4c88a3157187f86df5b5bcd960e`, 9 November 2026.
Initial implementation `644c3065a2abf1e07a6280880982704a801c3d94`.
Original Editor `7bbe7042ecd48582b11ef7053a4dbeade6b02351`;
installed SDK0.1.31 `9f0b2294d04d7f5fdd63d3fe5c118b2bd222eade`.
Catalogue SHA-256 `9c932872caeba8007bf05b86f7c167b493260709b23c54d709222a5f19539e65`.

## Implemented boundary

The [optional original consumer](../../examples/36_aetheri_editor_consumer/README.md#optional-original-fontstyle-attributes)
uses the existing copied document presentation API to publish the actual base
document font, point size, original foreground/background/selection colors and
editable policy after the original owner paints. `EDITOR_BUILD_NATIVE_ATTRIBUTES`
defaults to OFF and requires native presentation plus installed SDK0.1.31.
Existing geometry-only consumers retain the0.1.30 minimum. No SDK API/ABI/version,
active Aetheri source/pin/default or application model changes are introduced.

`attributes.c` uses fixed owner buffers, without another allocation or retained
font/layout/model pointer. Known size/colors/editability survive unsupported or
malformed optional font names/style. Invalid owner descriptors omit metadata
atomically while preserving valid geometry. Font metadata is read from the
current borrowed font each paint, avoiding a stale same-address font cache.
The SDK copies these temporary spans before the next frame overwrites them.

The original owner explicitly defines96 logical DPI for point conversion:
14 logical pixels become10.5 points. Backing scale is not physical DPI.
RGB values use16-bit channels. Selected high-contrast text reports the same
black foreground and yellow background as the original paint. UI language is
not inferred to be document language; a base-face family is not a per-glyph
fallback inventory. See the [SDK metadata prerequisite](font-metadata-report.md)
and [copied presentation contract](document-presentation-report.md).

The independent OS reader adds checks for eight defaults, selected-run overrides,
read-only state, original A+/A− size/geometry changes and attribute events:
selection, clearing and each real font-size change should each emit one event,
while an unchanged caret should emit none. These are native test requirements;
the current complete native matrix is still open.

## Executed local checks

Alpine-Dev/WSL, Clang22.1.3, C17 with extensions OFF, conversion/shadow/pedantic
warnings and `-Werror`. All four profiles run as UID65534 on a private native
Linux filesystem, retaining the original file-permission/xattr/save/history
assertions. Borrowed Arial and Candara exercise proportional layout, the original
paint, metadata and required real `ffi` shaping. Font bytes are not distributed.

| Profile | Owner CTests | Owner objects | Independent OS-peer objects |
| --- | ---: | ---: | ---: |
| Release static, attributes ON | 21/21 | 33 | 1 |
| Release shared, attributes ON | 21/21 | 33 | 1 |
| ASan/UBSan/LSan, attributes ON | 21/21 | 33 | 1 |
| Release shared, attributes OFF | 19/19 | 31 | — |

Total: **82 executed CTests,133 owner/peer objects**, no local skips.
An additional [SDK0.1.30 compatibility run](editor-attributes-compatibility.json)
passes19 CTests with attributes OFF and31 separately bound objects. It uses the
existing corrected `aa899fb` package and imports neither new metadata function.
That gives101 local owner CTests across both SDK versions; the older-package
objects are separate from the133 objects in the primary0.1.31 matrix.
The sanitized library and consumers retain leak detection and halt-on-error.
Two additional compiled mutations use the unchanged final attribute test:
wrong point conversion and wrong selected high-contrast foreground both exit1;
the unmodified control exits0. All six control objects use ASan/UBSan, with
LSan enabled and no sanitizer diagnostics. Actual terminal success/error peers
and a live peer separately verify that the EOF assertion remains active and
its failure diagnostic preserves stdout and the genuine open writer pipe.

The [local ledger](editor-attributes-local.json) and
[source binding](editor-attributes-source-binding.json) verify all108 exported
inputs against exact Git blobs,133 actual compiled owner/peer objects and28
byte-identical generated C replays. Installed SDK header/library hashes match
the earlier complete SDK receipt; its111 tests are reused prerequisite evidence,
not additional executions here. The separate Fedora SDK15 prerequisite matrix
and archive likewise remain attributed to their original execution.

## Retained failures and corrections

The [attempt ledger](editor-attributes-attempts.json) preserves successful and
failed preflights. The system pkg-config path initially omitted GObject despite
direct use in the independent reader; explicit `gobject-2.0` fixes the real
link failure. Clang required explicit equivalent floating zero/nonzero comparisons
and parentheses around an intentional adjacent-string test initializer. The
Editor library now compiles only its used Files model/create/rename modules;
the unused launcher and Files layout are not part of this Editor's link graph.
All original Editor targets and tests link successfully after this change.

A Windows-worktree Git indirection rejected the first Linux-Git export; the
corrected export uses Windows Git and verifies every blob. Nonroot CMake could
not set file modes on the Windows-mounted output directory; native Linux build
directories preserve the actual permission/xattr contract. The proportional
layout test correctly rejected an initial monospace font. Arial replaces that
incorrect fixture input; its width oracle is unchanged. Failed logs and partial
exports remain separate from the final matrix.

## Native gap and next evidence

Earlier live VM observations at644c306 completed Release and Shared native tasks.
Their raw owner/native directories are now inaccessible: SSH port19222 refuses
connections and the old terminal handle is missing. A cached Release result
does not substitute for complete captures, logs or current source binding.
Those observations are **not added to the accepted local counts above**.

The earlier ASan native task failed before the new attribute phases, in the
held-EOF clipboard preedit case. Three stream modes had completed. The gate
record showed all12 bytes written and drained while its writer remained open;
the peer had already exited. Its exact exit/reply was not saved by that driver.
No sanitizer diagnostic was found in the inspected app log, which does not
establish the cause. Later cleanup closes the writer and changes the model;
those later frames do not prove premature application before the assertion.

Source517a1b7 adds durable native paste begin/reply timings and saves exitcode,
stdout, original model/frame state and the actual pipe link before failure
cleanup. It preserves the EOF assertion and adds no successful-path file I/O.
No request deadline, original CPU/frame guard, sanitizer, quota, mount or
compositor setting has been relaxed. No replacement VM was started.

Required next work: recover an available native test environment; diagnose the
original early reply; run the complete enabled matrix, the attributes-OFF native
baseline and an OFF binary with attributes demanded; compare complete output
pixels and retain the compiled omitted-data refusal. Existing clipboard/stream,
IME/history/cache/save/refusal/close tasks and every capture remain required.
No native acceptance is inferred from the82 headless tests or peer compilation.

## CI, archives and scope

The [CI ledger](editor-attributes-ci.json) binds all seven required SDK jobs at
6d1f273, with two optional skips and complete decoded logs. Final-source
[run42645409613](https://github.com/Lulus792/NexaUI/actions/runs/42645409613)
passes Windows static/shared146/145, macOS122/121, Linux268 plus a separate
lifecycle test/266, and268 tests in each sanitizer driver order, with no CTest
skips. The [earlier644c306 CI](editor-attributes-initial-ci.json) remains separate.
These SDK jobs do not execute the standalone original consumer matrix.

The [verified local archive receipt](editor-attributes-archive.json) records
`nexaui-editor-attributes53-local-20261109.tar.gz`:46,501,566 bytes,
SHA-256 `772b5e0b9ec20431561a9425259e197efcb70d847d0c29e4d026cebd11da6dd1`.
All1,404 files,6 links and1,144 directories were verified; original test FIFOs
are recorded without serialization. All719 local-ledger artifacts are covered.
Borrowed font bytes are excluded. The native raw-data gap above remains open.
The [supplemental archive receipt](editor-attributes-supplement-archive.json)
records final CI logs and the additional0.1.30 compatibility artifacts:
`nexaui-editor-attributes53-supplement-20261109.tar.gz`,1,651,409 bytes,
SHA-256 `33731fc7ded1694dd4ae195bc588f8dd95b406765de80d51cc910c4aaba2fbbe`,
156 verified files. Inactive original test FIFO/symlink entries are recorded.

Read-only Git verification at Aetheri159c4be831e1112dfc30a401d4e52d43ac36a1a2
finds all53 consumed original files unchanged. Aetheri continues independently.
Broader font/fallback and native text interfaces, AX/UIA, real IMEs/screenreaders,
physical/human/design/performance and normal owner adoption remain open.
All103 IDs and remaining packets6–8 persist. This is a development checkpoint,
not a1.0 release or complete catalogue acceptance.
