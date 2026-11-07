# Original Aetheri-owned Files consumer

This standalone Linux C17 build checks Aetheri's original Files controller and
NexaUI view against an independently selected installed SDK, minimum 0.1.23.
It imports original sources from immutable Aetheri
`27de5cb228da0f37264fad64d964ad7a660d210d`; it adds no competing Files model,
edit history, naming operation or UI. `source-inputs.json` hashes all 38 selected
source/header/test/metadata inputs and rejects a different snapshot at configure.
The full original Git archive SHA-256 is recorded separately in that file.
The optional document test additionally verifies 20 original Terminal inputs.

Aetheri's own CMake project intentionally requires exactly SDK 0.1.18. This
external experiment builds the original application sources without executing
that project's package-selection policy. It does not edit or satisfy that
original exact-pin check, change the normal consumer selection, or approve an
upgrade. Production adoption remains an Aetheri owner decision.

The selected original view already composes list/grid, held-folder sidebar,
filter, inline naming, rename/move review, action menu, information, window chrome,
English/German, high contrast and text sizing. The controller owns workers,
unknown results and original document-child close obligations. None of those
models or operations move into the toolkit. Original source notices remain in
the independently supplied archive. No SDL view, text/rendering dependency or
privileged authority is added to NexaUI.

```sh
cmake -S examples/29_aetheri_owned_files -B out/original-files \
  -DCMAKE_PREFIX_PATH=/absolute/installed/sdk \
  -DAETHERI_SOURCE_DIR=/absolute/immutable/aetheri \
  -DNEXAUI_OWNED_FILES_TEST_FONT=/absolute/LiberationSans-Regular.ttf \
  -DCMAKE_BUILD_TYPE=Release
cmake --build out/original-files
ctest --test-dir out/original-files --output-on-failure
```

The unmodified original controller test and two original view tests run without
a display. The latter use raster densities 1 and 2, with 128 combinations each
of list/grid, locale, contrast, text size and window geometry. They also exercise
stale row/dialog/sidebar actions, whole-commit rejection, Unicode edit commands,
safe minimum-size behavior and modal bounds. These are application-view checks,
not physical-input, total-memory or performance-budget acceptance.

`owned_files_native` is the original ordinary application host.
`owned_files_fault_native` is a separate test-only host with the original
`mkdirat`/`renameat2` lost-acknowledgement probes; never install it as an app.
The original native fault runner can exercise create/rename/move directly:

```sh
python3 /absolute/aetheri/tools/test_files_view_fault.py \
  /absolute/private/test_compositor_server out/original-files/owned_files_fault_native \
  /absolute/LiberationSans-Regular.ttf /absolute/aetheri-terminal \
  /absolute/aetheri-editor /absolute/LiberationMono-Regular.ttf /absolute/new-build-parent
```

That unchanged original script owns its private seat, temporary folders and
processes. It performs real ordinary-user filesystem effects, holds the worker
acknowledgement, observes one attempt and preserves the unknown result without
retry. These effects are only in its new test folders. It asserts that no
document child starts, so the supplied Terminal/editor are not exercised there.
This is not power-loss recovery, managed deletion or a grant protocol.

When libatspi development files are available, CMake builds the original
independent `owned_files_atspi_client`, linked neither to NexaUI nor Aetheri.
Run the separate candidate driver with explicit private assets:

```sh
python3 examples/29_aetheri_owned_files/run_native.py \
  --aetheri-source /absolute/immutable/aetheri \
  --server /absolute/private/test_compositor_server \
  --host out/original-files/owned_files_native \
  --client out/original-files/owned_files_atspi_client \
  --registry /absolute/at-spi2-registryd --font /absolute/LiberationSans-Regular.ttf \
  --terminal /absolute/aetheri-terminal --editor /absolute/aetheri-editor \
  --mono /absolute/LiberationMono-Regular.ttf --work-dir /absolute/fresh-short-path
```

The Python 3.11+ driver hashes original inputs, starts a private compositor/bus/registry,
and runs the unmodified original peer's initial/dialog/text/cancel/query/query-clear
modes. They check actual selection, native Unicode text/caret/selection/editing,
rejected stale references, modal cancellation/focus and real filtered entries.
View-only actions must neither launch a document nor alter the two-entry fixture.
Virtual Ctrl+Q closes the original app normally. Logs, mapped libraries and raw
three-output PPMs are retained. `--expect-unavailable` requires a separately built
provider-off host: the already rendered app must fail native registration at the
expected discovery boundary and still close normally by keyboard. Private AT-SPI
runtime libraries must already be available on PATH/LD_LIBRARY_PATH.

The original full `test_files_view.py` is deliberately not reported as passed
by this experiment: it verifies Aetheri's exact 0.1.18 pin and a particular shared
deployment record. Its assertions remain intact. This consumer instead selects
and records its own experimental SDK and reuses the original client/fault tests.
Full normal-owner/document/Terminal workflows, actual pickers/authorities,
installed IMEs, broader native chrome/scale/loss cases, other platforms and human/
physical acceptance remain open. See the
[quality report](../../docs/quality/aetheri-adoption-assessment.md#original-aetheri-owned-files-on-the-candidate-sdk-2026-11-07)
for exact tested revisions, binaries and acceptance boundaries.

## Original document and close lifetime

`NEXAUI_OWNED_FILES_DOCUMENTS=ON` additionally builds `owned_files_terminal` from
the same original Aetheri SDL Terminal and `owned_files_editor` from its original
C17 editor/document/file sources. The editor has no toolkit dependency. This is
an external application integration profile; SDL does not enter NexaUI or the
Files host. The default OFF configuration does not search for SDL. ON requires
the original reviewed SDL3 3.4.20, SDL3_ttf 3.2.2 and Wayland-client 1.26.0 packages
in independently supplied local prefixes. No download or installation is implied.

```sh
cmake -S examples/29_aetheri_owned_files -B out/original-documents \
  -DCMAKE_PREFIX_PATH='/absolute/installed/sdk;/absolute/reviewed/sdl-prefix' \
  -DAETHERI_SOURCE_DIR=/absolute/immutable/aetheri \
  -DNEXAUI_OWNED_FILES_DOCUMENTS=ON \
  -DNEXAUI_OWNED_FILES_TEST_FONT=/absolute/LiberationSans-Regular.ttf \
  -DCMAKE_BUILD_TYPE=Release
cmake --build out/original-documents
```

Use the native command above with these newly built Terminal/editor paths, plus
`--document --runtime-library-dir /absolute/reviewed/sdl-prefix/lib`. This mode
requires the accessible positive host and cannot combine with
`--expect-unavailable`. It first runs all six original independent Files peer
modes, then opens its selected literal `seed.txt` through the original held-FD
launch. It checks the original actor executables/parents using pidfds and records
their process maps. Native input edits the original document. A Terminal-window
close must retain its dirty-content choice; Escape cancels it. A localized native
Files Close action must wait for that same document rather than ending/replacing
it. Explicit save must produce exactly `helloseed`; only explicit document close
ends the original editor/Terminal pair and lets Files exit normally. Captures
retain both the dirty choice and Files' waiting message. Failure cleanup targets
only the retained private actors and records escalation separately.

This proves a selected ordinary-user document/save/close path through the
original controller. It is not a normal installed owner launch, document grant,
full Files/Terminal suite, PTY/lease drain, durable recovery or a NexaUI Terminal
presentation claim. The child Terminal deliberately uses Aetheri's existing SDL
view. Its external shared dependencies are not sanitizer-instrumented just
because the original C17 app/editor/SDK are. See the
[document-lifetime evidence](../../docs/quality/aetheri-adoption-assessment.md#original-files-document-save-and-close-lifetime-2026-11-07)
for tested static/shared/sanitizer artifacts and the detected early-close mutant.
