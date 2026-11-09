# Original Editor: native system clipboard composition

Consumer source `76a08fc8c08173e31f41ef9bb8e5c199d3876657`, 9 November2026.
Installed SDK0.1.29 remains source `8862cb5a1b78c63845a2d2a50441cda8e19243e7`.
Original Aetheri inputs remain `7bbe7042ecd48582b11ef7053a4dbeade6b02351`.
The complete103-ID catalogue and remaining packet6/packets7–8 stay active.

## Existing owner and optional connection

`EDITOR_BUILD_NATIVE_CLIPBOARD=ON` requires native document accessibility and
SDK0.1.29. Its default is OFF. A real original Editor window supplies the existing
`nx_text_clipboard_for_window` callbacks to `nx_text_document_register_with_clipboard`.
The headless owner still registers replace/select only. No controller, history,
clipboard service, public structure layout or SDK dependency is replaced.

Native Copy exports a copied scalar-indexed range without a model callback,
document revision change or composition cancellation. Cut and Paste use the
existing atomic original replacement callback, full six-field input token,
preflight UI capacity and original Undo/Redo. Multiline CRLF bytes remain intact.
Review/picker policy disables document operations. Policy/tab roundtrips retain
the existing never-reused assistive input serial.

An actual pre-release owner accepted an old pending Paste after a fresh native
preedit: uncommitted composition had not changed the original document token.
Nonempty preedit now also advances the published assistive input serial. This
retires the older native request while preserving the original composition.
Copy still leaves that composition intact. The original public keyboard-paste
token remains independently owned by the app.

Build instructions and opt-in runner arguments are in the
[consumer example](../../examples/36_aetheri_editor_consumer/README.md#optional-native-document-clipboard).

## Executed original-owner matrix

Fedora44/Linux6.19.10, GCC16.2.1, CMake4.3.0, Python3.14.3 and the borrowed
AT-SPI2.60.7 runtime. All targets use C17 and the existing warning-as-error flags.
The ASan/UBSan profile instruments the whole installed candidate and compiled
consumers, retaining leak detection. The unchanged private producer uses pixman
outputs1920×1080,2560×1440 and3840×2160; app maps retain the Wayland software
dependency checks.

| Installed profile | Owner CTests | Complete captures | Raw cache probes |
| --- | ---: | ---: | ---: |
| Release static | 15 | 78 | 53 |
| Release shared | 15 | 78 | 53 |
| Debug ASan/UBSan | 15 | 78 | 53 |

The234 captures retain all216 previous original task captures and add18 clipboard
captures. The159 probes include126 phases recorded before the style tasks,
6 subsequent style probes and27 held/finite-reader probes. Existing full8-MiB
document reads/edits, grapheme/CRLF rejection,
history/save/review, native IME, retained references, density/style and real
native close/move/resize/zoom tasks remain in the run. Every final1.2-second
retained-close idle sample records zero CPU ticks and zero new frames; this is
not an energy or general performance benchmark.

Separate compatibility builds pass11 default SDK0.1.27 tests,15 native-document
SDK0.1.28 tests with clipboard OFF, and15 SDK0.1.29 clipboard tests with native
IME OFF. Enabling clipboard without native document accessibility, or against
SDK0.1.28, is correctly refused at configure time. These41 tests are build/model
compatibility evidence, not additional native clipboard tasks. The existing SDK
tests/package runs retain their earlier source attribution; none is recounted
as a new SDK test here.

## Independent native requests and transfers

The OS reader links system libatspi without NexaUI or original model code.
The pinned foreign `wl-copy`/`wl-paste` executables use their separately checked
musl/Wayland runtime. Their complete input hashes and process/protocol witnesses
are recorded in the [local matrix](editor-clipboard-local.json).

- Copy of `é🌍` preserves exact bytes, original caret/revision and active preedit.
  Its void native wire reply is checked through the actual GError, then through
  independent foreign clipboard bytes.
- Cut exports the same Unicode range; one original Undo/Redo restores/reapplies
  deletion. Foreign Unicode/CRLF Paste likewise uses one original undo entry.
- A genuinely stopped foreign provider leaves the request pending across owner
  frames. Completion is accepted once with unchanged input, and refused after
  caret movement, preedit, preedit completion, review/Cancel and tab roundtrips.
  Retired native references stay unusable; fresh references verify actual text.
- A refused native read does not displace the original outstanding keyboard
  paste token; that original paste still completes after the provider resumes.
- Actual compositor close rejects the pending native request. Original Save
  writes the expected bytes and exit0 completes while the foreign provider is
  still stopped. Context/window destruction order remains the original order.

The unchanged producer assigns temporary clipboard toplevels round-robin too.
The runner retires at most two checked extra offer/read pairs to restore the
original next-output ordinal before the original contrast/light tasks. Their
density assertions and captures remain unchanged. Finite native request peers
are explicitly joined separately from the original must-stay-alive processes.
IME barriers join actual client key/context and producer commit serials; stale
surrounding bytes alone do not establish readiness. Deadlines and CPU guards
are unchanged.

The final driver/OS reader also runs against the actual archived old owner36.
It detects the accepted-after-preedit defect at `!accepted`; the corrected
owners pass the identical task. This is an actual compiled counterexample,
not a source-string mutation. Initial compiler/driver refusals, finite-peer and
retired-reference expectation errors, misplaced style output and stale IME
barriers remain in the archives with their actual failures.

## Provenance, storage and visual limits

The [source binding](editor-clipboard-source-binding.json) matches all96 final
inputs to Git. Compiled bundle40 and final driver42 differ only in one uncompiled
Python flow; all owner/model/reader/compiler inputs are identical. There are183
compiled objects,35 exact generated-C replays and15 complete bytewise matches
against previously reviewed full captures. At committed Aetheri HEAD
`320231f11740e65fc5a0ed3e702ef321862f043d`, all53 consumed original inputs are
unchanged. No active Aetheri source, build, service or pin was modified.

Two new Shared captures were visually inspected: layout, caret and multiline
placement remain visible. The borrowed monospace font shows the globe as a
replacement box. Exact UTF-8 transfer is proved separately; emoji glyph/fallback
acceptance remains open. See the [check and visual ledger](editor-clipboard-checks.json).

A bounded private scratch write confirms EDQUOT for test UID1000. The
[archive ledger](editor-clipboard-archive.json) preserves all complete raw
artifacts and the partial capture failures. Before either retirement, every
member type/mode/size/hash and complete live tree were checked, with process
cwd/fd exclusion and permanent test-UID/GID drop before deletion. Frozen sources
and installed prefixes remain. No quota, mount, driver, deadline, sanitizer,
capture dimension or rendering rule was changed. Unchanged binaries pass after
the verified retirement.

All seven required exact-source jobs pass in
[run42567850763](https://github.com/Lulus792/NexaUI/actions/runs/42567850763).
The [CI ledger](editor-clipboard-ci.json) binds seven complete decoded logs:
Windows144/143, macOS120/119, Ubuntu265/263, the separate Linux lifecycle repeat,
and both265-test sanitizer orders. No CTest skips occur; two optional jobs skip.
Installed/package/close/add_subdirectory consumers pass in all six platform
builds. This covers the SDK/platform/package matrix; the external original-owner
tasks above are their own local native evidence.

## Remaining gates

The original test stops the foreign source before sending bytes. Partial-byte,
held-EOF behavior is separately exercised by the existing SDK Wayland fixture;
this report does not claim that additional joined original-owner stream case.
Document geometry/attributes, broader native text interfaces, new AX/UIA document
and clipboard tasks, installed real IMEs/screenreaders, font coverage, physical/
human/design/performance and ordinary Aetheri adoption remain open. The new
connection is a controlled opt-in consumer experiment, not a normal app migration.
All103 requirements remain mapped; version1.0 still requires the user's approval.
