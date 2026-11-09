# Original Editor: native document accessibility

Consumer `b2490d58d5813d855c003d0f6f34f74da505ab1e`, 9 November 2026.
Installed SDK0.1.28 source `9ad027abb9d10b8c1c715ce9953601313603a7da`.
Original Editor `7bbe7042ecd48582b11ef7053a4dbeade6b02351`.
Catalogue SHA256 `9c932872caeba8007bf05b86f7c167b493260709b23c54d709222a5f19539e65`;
all103 requirements and the complete eight-packet goal remain in scope.

## Implemented owner and native behavior

The [optional original consumer](../../examples/36_aetheri_editor_consumer/README.md#native-document-accessibility)
connects `nx_text_document_register` to the existing original document,
controller, history, renderer, save worker and close review. It adds no second
editing model. The callback retains no model pointer outside its registration.
It rechecks all six original token fields, document identity, policy and a
monotone SDK serial; policy-only modal roundtrips also advance that serial.
UI byte/line capacities are reserved before the original atomic mutation.
Allocation failures preserve original text, selection, history and token.

Native Text/EditableText supports scalar-addressed reads, whole/ranged
replacement, original grapheme-bounded selection/caret and original Undo/Redo.
The complete8-MiB original model is readable in bounded ranges; an oversized
whole reply and insertion past the original model bound are refused. The
original close review withholds its controller: a fresh read-only native view
still exposes the document, while old retained references retire. Cancel and
tab removal/return never revive those references. Successful assistive edits
cancel the original pending paste/composition outside the atomic callback.

The owner publishes a completed semantic projection even when its software
presenter is busy. It drains already-received native requests before a presenter
retry can retire their frame, only after the original event queue is observed
empty. Each original raw event still receives its original frame. A followup
frame refreshes headers constructed before a late assistive edit.

## Executed checks and source boundaries

The [local ledger](editor-native-document-local.json) records Release, shared
and whole-library ASan/UBSan/LeakSanitizer profiles against SDK8:

- 15 original-owner CTests per profile:45 executed tests. Each profile compiles31
  owner/model/test objects and one independent OS-reader object:96 in total.
- All three complete native tasks pass, with72 captures each:216 full raw-image
  hashes checked. Original keyboard, IME, save/conflict, retained close,
  move/resize/zoom, styling and cleanup tasks remain in the same driver.
- Each task includes native Unicode operations, grapheme/CRLF refusal, original
  Undo/Redo, first-save bytes, review/Cancel, retained references, tab reuse and
  the full8-MiB document. Both200% German contrast and150% light/2x variants
  perform an independent native edit followed by original Undo.
- Each final retained-close idle guard observes zero CPU ticks and zero new
  frames over the unchanged1.2-second window. This is no energy benchmark.
- Two compiled defects are detected against SDK8: removing UI-capacity preflight
  and removing policy-only serial advancement.

The SDK8 build separately passes15 relevant CTests and an installed package
consumer per profile:45 SDK CTests and three package executions. Earlier bundle29
checks retain their own attribution: the default original SDK0.1.27 build passes
11 tests; the native-IME-off SDK7 variant passes15 tests and its full native task.
Three invalid option/old-SDK configurations are rejected. See the
[check ledger](editor-native-document-checks.json).

The [source binding](editor-native-document-source-binding.json) matches all93
frozen consumer inputs to Git:53 original and40 SDK-side inputs. All21 generated
model/host C inputs replay exactly against their compiled copies. The separately
bound SDK8 tree contains1034 Git-matched inputs; its31 uncompiled example36
inputs were recorded before the later consumer commit. They are now covered by
the separate93-input consumer binding. The SDK declares no fabricated build
revision. The original53 consumed paths remain unchanged at independently moving
Aetheri HEAD `b71d816aaf692476d17a662df39073d8b74722bf`.

## Observed failure and bounded optimization

Old compiled native trials expose two real owner-loop failures: a completed edit
could answer true while GetText still read the previous snapshot, and a busy
presenter retry could invalidate a waiting caret request. Both are retained in
the [verified archives](editor-native-document-archive.json).

The first full SDK7 ASan task additionally times out at the8-MiB edit. A diagnostic
peer measures a delete reply at905,317us and an insert timing out at1,001,463us,
using the unchanged1,000ms client deadline. A separate compiled phase probe
places the original mutation near0.9ms and synchronization near7.2ms; full text
registration and native publication dominate that observation. A failed probe
build and quota/capture/link failures remain recorded separately.

SDK9ad027a skips bounded ASCII runs during UTF-8 validation and counts an entirely
ASCII native snapshot directly. Unaligned loads use `memcpy`, never overread the
span, and require no allocation. Multi-byte input retains the unchanged scalar
decoder. A new differential test covers every1,112,064 valid Unicode scalar,
embedded NUL, exact allocation ends, alignment, malformed and random spans.
Release and whole-library sanitizer checks pass. The final native ASan task
observes317,083us for delete and375,059us for restore, with complete successful
cleanup. These are individual task observations, not paired statistical speedups,
general UI latency, an approved performance budget or an SDL comparison.

Five reviewed raw captures (Unicode, review, maximum document and both styles)
are byte-identical between SDK7 Release and SDK8 Release. Borrowed mono-font
coverage still includes a missing globe glyph; scalar/byte correctness does not
establish complete glyph coverage or human visual acceptance.

## Remaining acceptance

All seven required exact-SDK9ad027a CI jobs pass; the
[CI ledger](editor-native-document-sdk-ci.json) retains all seven decoded logs,
the new validation test on all six platform builds and both sanitizer driver
orders, native document tests and installed consumers. This SDK step is on main.
Consumer b2490d5 also passes all seven required exact-source jobs; its
[CI ledger](editor-native-document-ci.json) binds all seven complete logs,
executed tests and installed consumers. Both code steps are on main. The
consumer CI checks the SDK/platform suite; the original-owner matrix above is
separately executed locally against the immutable original export.
The local native task is an independent OS peer, not a real screenreader task.
Large native Cache.GetItems emits its existing bounded-reply-budget warning;
the peer discovers the object through ordinary Accessible calls and completes
all range reads. Broader cache/screenreader behavior needs separate acceptance.

The later [metadata-budget correction](atspi-cache-report.md) reproduces and
repairs this bulk-query refusal, with explicit independent wire queries against
the installed SDK and original Editor. The warning above remains the historical
result for SDK9ad027a; broader screenreader acceptance remains separate.

Native document clipboard/geometry/attributes, broader native text contracts,
new app-document AX/UIA tasks, installed real IMEs, delayed clipboard/IME
composition, physical/human/design/performance gates and normal Aetheri owner
adoption remain open. No active Aetheri checkout, pin, default, service or build
was changed. No1.0 release is declared. Remaining packet6, packets7–8 and all103
catalogue requirements stay active.
