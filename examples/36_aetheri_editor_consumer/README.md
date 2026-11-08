# Original Aetheri Editor: native close consumer

This separate Linux C17 consumer uses the original Editor from Aetheri
`7bbe7042ecd48582b11ef7053a4dbeade6b02351` and installed NexaUI **0.1.27**.
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
Those native canvas connections, preedit presentation and editable accessibility
remain separate work. This seam adds no SDK API, dependency, document authority,
file operation or active Aetheri change.
