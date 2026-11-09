# Original Aetheri Editor: native close consumer

This separate Linux C17 consumer uses the original Editor from Aetheri
`7bbe7042ecd48582b11ef7053a4dbeade6b02351` and installed NexaUI **0.1.27 or newer**.
The optional native document accessibility variant requires **0.1.28**.
Its 53 checked inputs include the original document/workspace/direct editor,
file/save/dialog code, regression tests and native driver. It never configures,
builds or modifies the independently developed Aetheri checkout or its SDK pin.

`prepare_host.py` writes a generated host in the build directory and records
every exact substitution. The adapter takes the latest native close request
before inspecting the legacy flag, keeps its token during the original
unsaved-work review, resolves Cancel when that close sequence is cancelled,
and accepts only after the original workspace has completed its close/save.
A newer native request replaces the pending token without restarting an active
document review. Forced native failures use the existing failed-teardown path.
The existing renderer/model/save-thread retirement order is preserved.

`window_input.h` adds ordinary native caption and edge gestures to the original
Editor geometry using the existing public move/resize/capability contracts.
The original zoom button and all document rendering/input remain in the host.
The SDK retains press/serial provenance; no OS handle or synthetic SDK event is
introduced. This is an independently reviewable adoption candidate, not a
change to normal Aetheri admission or the installed default editor.

## Build and run

Use an immutable export with no `.git`, containing all paths in
`source-inputs.json`. A different or modified original input fails configuration.

```sh
cmake -S examples/36_aetheri_editor_consumer -B /path/to/editor-build \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=/path/to/installed/nexaui \
  -DAETHERI_SOURCE_DIR=/path/to/immutable-aetheri-export
cmake --build /path/to/editor-build --parallel 2
ctest --test-dir /path/to/editor-build --output-on-failure
/path/to/editor-build/aetheri_editor_candidate /path/to/ui-font.ttf \
  --mono-font /path/to/mono-font.ttf --directory /path/to/disposable-documents
```

Eleven original model/file/view/history/search/selection/workspace/save tests
are registered. Their allocation/I/O fault wrappers and the original 45-second
save-work deadline are retained. All targets require C17 without extensions
and warnings as errors. Sanitizer builds require a matching sanitized installed
SDK and compiler/linker flags for the whole consumer.

## Private native task

The optional `close_producer` reuses the checked private Files/output producer
and exact dependencies from the preceding examples. It adds only two commands
to the inherited test socket: C sends a real xdg close to the sole mapped window;
Q observes that window's ID, current scene geometry and maximize state. Those
commands refuse DRM/nested sessions and ambiguous window ownership. The original
Editor test producer's integer-density branch is reused unchanged for its later
200% style case. It is not installed policy.

`run_native.py` checks the exported inputs and recorded SDK library bytes before
running the original Editor native task. It preserves the original document,
save/conflict/selection and style tasks and replaces the final empty-window
idle/keyboard quit with retained-document native-close coverage. Exact driver
substitutions and their hashes are recorded. Supply the matching actual SDK
artifact receipt and source binding; an SDK declaration alone is not a build
attestation.

The added task exercises two native close notifications during one review,
Cancel, an unchanged sleeping editor, continued native text input, real caption
movement, resize, zoom/restore and another native close through the original
Save destination and worker. It verifies saved bytes and the retained native
window identity. The original captures and native deadlines remain in force.
Diagnostics expose native-wait entry/return and Escape release; the short idle
guard begins after that final input, records native wait transitions and checks
the sleeping owner, unchanged frame count and the existing CPU-tick limit.
Protocol housekeeping may wake native polling without an application event.
This short guard is not a performance or energy benchmark.
Physical/human assistive use, external IME, normal owner adoption, service and
broader catalogue gates require separate acceptance.

Fonts and original application sources are borrowed inputs, not redistributed.
The original scaffold's licence notice is checked with those inputs.

## Atomic original-model IME prerequisite

`-DEDITOR_BUILD_IME_MODEL=ON` selects an additional, separately generated model
variant and registers `editor.atomic_ime_model` alongside all eleven original
tests. This option alone does not enable native IME in the document canvas.
The default continues to compile the original model sources directly.

`prepare_ime_model.py` checks the same immutable export and records every
substitution and generated-file hash. `ime_model.h` exposes one application-owned
replacement through the original gap-buffer, preflight and undo engine. It
requires all six fields of the original direct-input token. Native byte ranges
may end inside an old grapheme, but must preserve complete UTF-8 scalars and
CRLF; the resulting caret uses the original final-text grapheme preflight.
Delete and insert become one replacement and one undo group. Invalid input,
limits or allocation failure preserve content, selection, history and token.
Other original entry points retain their existing grapheme-boundary policy.

The owning native adapter must still admit the focused window/document/target
and epoch, collect a complete non-rejected BEGIN/END transaction, discard stale
or interrupted batches, and publish surrounding text and composition geometry.
The optional native variant below supplies the canvas connections and preedit
presentation; editable accessibility is selected separately below. This seam adds no SDK API, dependency, document authority,
file operation or active Aetheri change.

## Native document IME variant

Configure with both `-DEDITOR_BUILD_IME_MODEL=ON` and
`-DEDITOR_BUILD_NATIVE_IME=ON`. The latter requires the former. Thirteen tests
cover the original models, atomic seam and complete batch collector. The native
variant is generated from the same checked native-close host; each additional
substitution is recorded in `generated/ime-host/ime-host-delta.json`.

The document host opts in to complete SDK transactions before enabling IME.
It checks the native target/revision, original controller token and focused
document before collecting a bounded commit. It changes the model only at a
matching END. Filtered, malformed, stale and interrupted batches are discarded.
Own accepted batches update the original token while preserving the advertised
native revision through already queued batches. A fresh context is published
after the native queue drains; exhaustion discards an incomplete batch without
inventing END. Focus/tab/review/ordinary-input changes retire the old binding.

The original document remains the only content and history authority. A separate
bounded preedit is drawn with existing text-layout/selection APIs and exposed as
a read-only composition snapshot. Native hidden-caret state is respected.
Long preedits scroll inside their own clip; their text cannot overwrite the
original prefix. Candidate geometry comes from the current logical layout,
including text scale and output density, without multiplying logical coordinates
by the backing scale. Paragraph preedit is bounded to4000 UTF-8 bytes; unsupported
composition is reported without changing the document. Nonempty preedit cancels
an older pending clipboard receive through the original owner's cancellation
function. A delayed independent paste/composition peer still needs separate
acceptance.

Surrounding context reuses the original host's render snapshot and sends at most
4000 bytes, preserving scalar/CRLF endpoints and the complete selection. A larger
selection advertises identity only; the original controller still owns that
selection and its atomic replacement. No full-document SDK buffer is created.
The original filename field retains its own native path. If complete native
boundaries are unavailable, ordinary original document editing remains available.

`run_native.py --ime` enables the private producer before launching the original
Editor. It adds processed native context/candidate and original-model revision
barriers, replacement/Undo/Redo/queue/focus/large-document cases and complete
captures to the original task. The producer's extra commands28/29 exercise a
commit with continuing preedit and a hidden caret. Other original producer,
window/save/assertion/deadline behavior is retained. A full before/after pixel
comparison checks that long preedit leaves the original prefix unchanged.
These are controlled native-v3 tests; installed IMEs, independent editable
accessibility, human/physical input, performance and normal Aetheri adoption
remain separate gates.

## Original accessible-model prerequisite

`-DEDITOR_BUILD_ACCESSIBLE_MODEL=ON` requires `EDITOR_BUILD_IME_MODEL=ON`.
It adds scalar-indexed reads/replacements and atomic two-endpoint selection
through the original controller/document/history. It does not yet connect a
native AT-SPI document object; enabling this option alone changes no visible
document semantics or accessibility transport. The native IME option remains
independently selectable and the default accessible-model option is OFF.

The adapter scans the original gap-buffer spans without a second document
buffer. Reads count Unicode scalars, can address combining marks and either
CR/LF separately, allocate nothing and preserve the input token/history grouping.
The read caller chooses its output capacity; insufficient capacity preserves
the entire destination and reports the required byte count. Whole-document reads
are not silently truncated at the SDK's separate read-only snapshot bound.

Every operation checks the original controller identity, interaction, revision,
cursor, anchor and selection state. Replacements use the existing atomic native
byte seam and one original undo entry, preserving strict UTF-8/CRLF/control and
allocation rollback rules. Selection supports either direction, validates both
original grapheme endpoints before mutation, and consumes one gesture even at
an unchanged caret. The generated selection entry never moves one endpoint
before validating the other. No native target/window/focus admission, file or
clipboard authority is granted by this model API.

`editor.accessible_original_model` covers scalar/byte distinctions, gap-spanning
reads, reverse/collapsed selections, rejected interior grapheme/CRLF endpoints,
all six stale token fields, whole replacement and Undo/Redo, three allocation
rollbacks, and a600,000-byte allocation-free read. These model tests alone do not
establish native provider lifetime, deferred completion, geometry/events or
independent assistive tasks. The optional native variant below adds its own
provider and task evidence.

## Native document accessibility

The additional `EDITOR_BUILD_NATIVE_ACCESSIBILITY=ON` option requires installed
SDK0.1.28, `EDITOR_BUILD_ACCESSIBLE_MODEL=ON` and `EDITOR_BUILD_IME_MODEL=ON`.
`EDITOR_BUILD_NATIVE_IME` remains independently selectable. The default build
still uses the original model and SDK0.1.27-compatible close adapter.

```sh
cmake -S examples/36_aetheri_editor_consumer -B /path/to/editor-native-build \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=/path/to/installed/nexaui \
  -DAETHERI_SOURCE_DIR=/path/to/immutable-aetheri-export \
  -DEDITOR_BUILD_IME_MODEL=ON -DEDITOR_BUILD_ACCESSIBLE_MODEL=ON \
  -DEDITOR_BUILD_NATIVE_ACCESSIBILITY=ON -DEDITOR_BUILD_NATIVE_IME=ON
cmake --build /path/to/editor-native-build --parallel 2
ctest --test-dir /path/to/editor-native-build --output-on-failure
```

The generated native-document host publishes the existing original controller
through `nx_text_document_register`. Its snapshots copy up to the original8-MiB
bound. Owner callbacks recheck complete original tokens and reserve UI copies
before mutation. Review/picker policy and tab changes retire stale requests;
original input/history/save/rendering remain authoritative. Completed semantics
are published while a software buffer is busy, and pending assistive requests
are routed before a renderer retry when the native raw-event queue is empty.
Every generated substitution is recorded in `generated/native-document/`.

Fifteen CTests include the actual generated host's allocation/token/policy
regression. `atspi_client` builds a separate OS peer without NexaUI/model
linkage. `run_accessible_native.py` creates an owned private bus/registry around
the original task; `run_native.py --accessible-client /path/to/editor_document_client`
adds native Text/EditableText tasks. It needs the same recorded SDK bytes,
immutable export, fonts and private producer as the existing task; `--ime`
additionally selects its native document IME work. No shared desktop bus or
global system configuration is changed.

Native document geometry, real screenreaders/installed IMEs, new
AX/UIA document tasks, physical/human/performance and normal Aetheri adoption
remain separate. See the [source-specific report](../../docs/quality/editor-native-document-report.md).

## Optional native document clipboard

`EDITOR_BUILD_NATIVE_CLIPBOARD=ON` additionally requires installed SDK0.1.29
and `EDITOR_BUILD_NATIVE_ACCESSIBILITY=ON`. It defaults to OFF, preserving
the existing0.1.28 native-document and0.1.27 default consumers. Native IME is
independently selectable. The real window supplies the existing native clipboard
callbacks to `nx_text_document_register_with_clipboard`; an owner without a
window still registers only replace/select capabilities. Review/picker policy
continues to disable document operations.

Copy exports the requested copied snapshot without a model edit or composition
cancellation. Cut and Paste use the same atomic original document callback,
complete controller token, preflight UI capacity and original Undo/Redo history
as other native edits. A nonempty native preedit also advances the published
input revision: uncommitted composition does not necessarily change the original
document token, but must retire an older pending assistive paste. Native reads
reuse SDK bounds/lifetimes; the original keyboard-paste token remains app-owned.

With this owner enabled, pass `--clipboard-tools /path/to/checked/clipboard-tools`
alongside `--accessible-client` to the existing native runner. The additional
task reuses the independent Files `wl-copy`/`wl-paste` helper and checks its exact
loader/tool hashes. It adds Unicode/CRLF transfers, Copy during composition,
original Cut/Paste Undo/Redo, stopped-provider completion and invalidation,
keyboard/native token overlap and original native-close/Save teardown. Original
workflows, assertions, deadlines and the retained-close CPU/frame guard remain
in the run. `clipboard-result.json` records the actual outcome; requesting the
option alone is not evidence of successful native or ordinary Aetheri adoption.

### Joined partial-byte and EOF task

`foreign_stream_gate` is a standalone Linux test utility, with no NexaUI/model
linkage or install target. Build it with a compiler matching the foreign
provider's libc ABI, then run its CTest. The recorded `wl-copy` runtime uses
musl; a system-glibc shared object is not an interchangeable preload.

```sh
cmake -S examples/36_aetheri_editor_consumer/foreign_stream_gate \
  -B /path/to/private-stream-build -DCMAKE_BUILD_TYPE=Release
cmake --build /path/to/private-stream-build
ctest --test-dir /path/to/private-stream-build --output-on-failure
```

Add `--clipboard-stream-gate /path/to/libeditor_foreign_stream_gate.so` to the
existing `--clipboard-tools` native task. The runner injects it only into
independent foreground `wl-copy` children. Its exact synthetic-payload/FIFO
match schedules real writes and retains the original stream until explicit
EOF; it changes neither the app nor the compositor. Pipe fill queries prove
that the original reader consumed the bytes without creating another reader.
Forced original pointer-event frames must preserve text/revision and keep the
native request pending for both an incomplete UTF-8 prefix and all valid bytes
without EOF. The task also checks truncated EOF, invalidation after consumed
bytes, one original Undo/Redo, and Save/exit while the writer remains open.
Earlier ordinary-transfer tasks, captures, style assertions, budgets and
deadlines remain in the run. This private transfer adapter is not an SDK or
application runtime dependency and must not be preloaded into other processes.

## Optional actual document geometry

`-DEDITOR_BUILD_NATIVE_PRESENTATION=ON` requires native accessibility and the
installed SDK0.1.30 containing `aa899fb`'s interior-caret fix or a later containing
revision. The option stays OFF for existing consumers. Pass a borrowed real
proportional font with `-DEDITOR_PRESENTATION_TEST_FONT=/path/to/font.ttf` to run
both the layout-oracle and actual original `paint_document` CTests.
An additional `-DEDITOR_PRESENTATION_LIGATURE_FONT=/path/to/font.ttf` runs both
tests with required real `ffi` shaping; absence of that ligature is a failure.

After the original owner handles any accepted edit and draws its rows, the
adapter copies logical selection-cell geometry from those same public glyphs,
line metrics and carets. It preserves shared ligature/combining intervals,
the first actual bidi caret, CRLF, row/canvas clipping, horizontal/vertical
scroll and font scaling. A current IME caret comes from the original paint.
These are logical character cells, not raster ink bounds. Offscreen geometry
is absent; a budget/allocation failure omits the whole optional projection
without disabling the original text/input. Buffers are reused and released
on original owner teardown; no layout/model/font pointer enters the SDK snapshot.

Add `--presentation` to the existing independent-reader native task to require
character/range/point/caret round trips on the original rendered owner, including
an accepted combining/CRLF edit and the existing style/scale cases. Existing
assertions, captures, CPU guard, deadlines and native close work remain enabled.
Font/style attributes, offscreen scrolling interfaces, AX/UIA and broader
physical/human/design/performance/adoption acceptance remain separate work.

The [original geometry report](../../docs/quality/editor-presentation-report.md)
records the exact compiled/driver/SDK revisions, actual font and native tasks,
default-OFF comparison, retained failures and remaining acceptance.

## Optional original font/style attributes

`-DEDITOR_BUILD_NATIVE_ATTRIBUTES=ON` additionally requires presentation and
installed SDK0.1.31. The option defaults to OFF, retaining the0.1.30 geometry
consumer. `--attributes` adds independent OS checks to the existing native task
and requires `--presentation`. It reads actual font metadata, default and
selection-run colors, editable policy and attributes after original model edits.
The peer selects/clears text and invokes the original A+/A− actions, requiring
matching size/geometry changes and one attribute event per actual change.

The owner uses the actual document font, foreground/background and original
selection. It reports point size using an explicit96-DPI logical reference:
14 logical pixels become10.5 points, with original text scaling already applied.
This does not infer physical monitor DPI. RGB text attributes use16-bit channels;
the high-contrast selected text is black on the original yellow highlight.
UI language is not asserted to be document language. Unsupported/malformed font
metadata is omitted individually while known owner attributes and geometry
remain available. Invalid owner metadata removes the whole metadata projection
while retaining valid geometry. Fixed owner buffers are overwritten each frame;
no font/layout/model pointer or extra allocation is retained by this helper.
Broader font/fallback, native interfaces, AX/UIA, physical/human/design/performance
and normal Aetheri owner adoption remain separate acceptance work.

The [partial attribute report](../../docs/quality/editor-attributes-report.md)
binds the local owner/model/paint matrix and explicitly retains the pending native
matrix. Native paste logs now record call/reply timing; a failed pre-EOF pending
check saves `clipboard-early-reply.json` with the exact exit/stdout, model/frame
and actual writer-pipe observation before cleanup. EOF assertions and deadlines
remain unchanged. The Editor target compiles its used Files model/create/rename
modules; the Files process launcher and Files layout remain outside this target.
