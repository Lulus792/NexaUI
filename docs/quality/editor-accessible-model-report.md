# Original Editor: accessible document model prerequisite

Consumer source `e6cd1d0b61b451589b7708f402e95fef534267f8`, 8 November 2026.
Original Editor `7bbe7042ecd48582b11ef7053a4dbeade6b02351`;
installed SDK0.1.27 source `47ec4e640a8a0f54bc8a09d1fe825b57956df6d7`.
Catalogue SHA-256 `9c932872caeba8007bf05b86f7c167b493260709b23c54d709222a5f19539e65`.

## Implemented behavior and remaining native boundary

The [optional original accessible-model adapter](../../examples/36_aetheri_editor_consumer/README.md#original-accessible-model-prerequisite)
reads the existing document's gap-buffer spans, maps Unicode scalar positions
to byte offsets, and applies original-controller replacements and selections.
`EDITOR_BUILD_ACCESSIBLE_MODEL=ON` requires `EDITOR_BUILD_IME_MODEL=ON`; default
OFF still builds the untouched original model. Native IME remains independently
selectable. No SDK implementation, API, version, dependency or active Aetheri
source/pin/default is changed. This is a model prerequisite, not a native
accessible document object or an independent AT-SPI client result.

Reads allocate nothing, change no controller/history state, and never silently
truncate the original document at the SDK's separate512-KiB read-only snapshot
bound. The caller supplies capacity; insufficient capacity leaves the complete
destination unchanged and reports required bytes. Scalar positions distinguish
UTF-8 byte length, supplementary emoji, combining marks and the two CR/LF
characters. A read may address either CR/LF scalar or an interior combining mark.
The original document still decides valid edit/selection boundaries.

Every operation compares all six fields of the original controller token:
identity, interaction, document revision, cursor, anchor and selection state.
Replacement delegates to the prior atomic native-byte seam with the same
document preflight, gap buffer, final grapheme caret and one original undo group.
Selection supports either direction and preflights both original grapheme/CRLF
endpoints before moving anything. It consumes one original gesture even at an
unchanged caret. Invalid/stale requests and edit allocation failure preserve
document bytes, cursor, selection, history, dirty state and input correlation.
The adapter grants no window/focus, clipboard, file or service authority.

The offset distinction follows the primary [AT-SPI range-read contract](https://gnome.pages.gitlab.gnome.org/at-spi2-core/libatspi/method.Text.get_text.html)
and [insertion contract](https://gnome.pages.gitlab.gnome.org/at-spi2-core/libatspi/method.EditableText.insert_text.html):
character positions and inserted UTF-8 byte length are separate quantities.
Native message validation and completion still need the future provider below.

## Executed checks

| Configuration | CTests | Retained full native IME/edit/save/window task | Captures |
| --- | ---: | --- | ---: |
| Static Release, model/native/accessibility options ON | 14/14 | Passed | 57 |
| Shared Release, same options ON | 14/14 | Passed | 57 |
| ASan/UBSan/LSan, same options ON | 14/14 | Passed | 57 |
| Static Release, all options OFF | 11/11 | Not rerun in this configuration | — |

No local skips. Actual compile commands bind90 ON-profile C17/Werror objects
and25 default objects. Sanitizer flags cover the full consumer and matching
installed SDK, with leak detection and halt-on-error retained. Invalid accessibility
ON/model OFF configuration is rejected with its specific diagnostic.

The new model test checks exact scalar/byte reads, output-capacity rollback,
gap-spanning data after replacement, reverse/collapsed/no-op selection, both
invalid grapheme endpoints, partial CRLF edits, all six altered tokens, foreign
controllers, cancelled input, malformed/control/NUL text, whole replacement,
Undo/Redo and successful no-op token consumption. Three actual edit allocation
sites are failed separately with selection and a pre-existing redo branch retained.
A600,000-byte full read and normal reads/snapshots execute with the first malloc
set to fail and make zero malloc calls. This is a bounded allocation/work check,
not a performance or maximum-document benchmark.

Two separately compiled defects fail at their intended assertions: omitting
interaction checking admits stale reads; omitting anchor preflight accepts a
selection whose anchor lies inside a grapheme. Both compile and link successfully;
compiler errors are not counted as functional detections.

All171 complete native PPM captures pass decoded size/hash checks. These tasks
retain the prior complete [native IME workflow](editor-native-ime-report.md),
original editing/history/file workers, close/review/Cancel, idle guard, retained
window management and theme/density cases. They do not invoke the new model
operations through a native accessibility provider. No new visual design or
human screenreader acceptance is claimed.

## Source, artifacts and preserved failures

[Source binding](editor-accessible-model-source-binding.json) checks83 immutable
inputs:53 original and30 repository files. README gained only a post-stage
documentation section. All19 generated C outputs replay byte-for-byte from the
checked native-close, IME-model, accessibility-model and native-IME generators.
The original53 committed inputs were checked against the independently advancing
Aetheri HEAD recorded in that receipt and remain identical. Frozen producer16
dependencies match the preceding source binding; its existing17-input provenance
and the three installed SDK libraries remain separately bound.

[Local receipts](editor-accessible-model-local.json), [checked counts](editor-accessible-model-checks.json)
and [archive receipts](editor-accessible-model-archive.json) bind541 selected artifacts.
The final9,111,978-byte archive has SHA-256
`161d982a0f3518651d9ba09ff5d45edec3370c38625e78bf1486f4d0278820af`;
every844 files, four links and509 directories was verified.

All seven required SDK jobs pass on this exact consumer source in
[CI42531888398](https://github.com/Lulus792/NexaUI/actions/runs/42531888398):
Windows static/shared142/141, macOS118/117, Ubuntu259/257 plus the separate
static presentation repeat, and sanitizer259 in both discovery orders. Optional
Windows-ASan and fuzz jobs are skipped. The [complete job/log receipt](editor-accessible-model-ci.json)
binds successful native tests and six installed close-consumer runs. These are
SDK/ABI/package regressions, separate from the external original-model/native
matrix above. No physical Mac session or new document AT-SPI acceptance follows.

The initial relocated negative source lacked its original include directory;
its compilation failure is retained and not counted. The corrected negative
driver adds that checked source directory and both functional defects are caught.
One native attempt stopped with compositor SIGBUS before READY; another reached
the first capture and failed during output capture. A bounded owned probe reported
EDQUOT, with46,018,560 file bytes before its own cleanup. Exact kernel SIGBUS
causation is unproven. Only fully archived inactive native19, model11 and native17
Release build roots were retired, totaling189,875,626 bytes. Full hash/mode/tree,
namespace/holder and live-process checks precede permanent UID/GID1000 drop and
artifact deletion. An initial plan included two never-created17 roots; its guard
refused before deleting anything. Current22, immutable exports, producers and all
SDK prefixes remain. No global quota/cache/driver/mount changes occurred.
Unchanged candidate binaries and native driver then pass all three profiles;
the failed attempts, original assertions/deadlines/CPU budgets and sanitizer flags
remain explicit.

The first artifact collector included its still-open own log before printing
its final summary; archive/source verification caught that changed hash. The
final selection excludes mutable collector logs from individual artifact rows,
while the completed logs and initial archive remain preserved. Final archive
member verification and all selected artifact bindings pass.

## Next integration contract and scope

Current SDK [read-only app snapshots](../../include/nexaui/nexaui.h) intentionally
reject native editing; built-in field requests require a genuine SDK-owned text
buffer. Reusing either as a second editable document would bypass the original
controller and is not the intended integration. The next SDK/provider step must:

- Keep native snapshots free of borrowed model pointers and application callbacks;
  run original-model operations only on the owner thread.
- Bind queries/edits to native window/context lifetime, document incarnation and
  the full original input token; expire stale requests after focus/tab/review or
  unrelated edits. A successful native response must follow actual application.
- Expose truthful full-document counts/ranges within explicit budgets, without
  silently publishing only512 KiB of a larger original document.
- Connect selection/caret, atomic replacement, native events and visible geometry;
  preserve original Undo and asynchronous clipboard cancellation/revalidation.
- Exercise an independent OS AT-SPI client against the actual Editor, including
  stale objects, native/IME/clipboard interleaving and clean shutdown.

The immutable original host's `register_document` currently omits its read-only
content Text node when the document exceeds `NX_TEXT_SNAPSHOT_MAX_BYTES`.
The new600,000-byte model read is deliberately above that threshold; it does not
by itself repair the native publication gap. Full native range reading remains
part of the provider work, rather than silently accepting the existing omission.

All of those native-provider gates remain open. So do installed IMEs,
live output/manual offscreen-caret restoration, normal Aetheri owner/pin adoption,
physical/human/design/performance acceptance and the earlier Software ASan AT-SPI
Read-action instability. Remaining packet6, packets7–8 and the full103-ID goal
stay active. This checkpoint does not approve or publish1.0.
