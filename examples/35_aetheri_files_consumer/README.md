# Original Aetheri Files consumer compatibility

This separate Linux C17 build reuses the original Files controller, model,
NexaUI view, native host and session/view tests from the exported Aetheri source
named in `source-inputs.json`. It adds no second Files implementation. All file
creation, rename/move, original document/child lifetime and unknown-outcome rules
stay in that original source. The host uses the installed SDK's ordinary
Wayland/software window and semantics; the original SDL application is unchanged.

This experiment selects an explicit NexaUI SDK candidate independently of the
reviewed pin in Aetheri's own build. It neither changes that pin nor claims its
normal adoption requirements passed. `NexaUI_SOURCE_REVISION` is a builder
declaration; compare the actual installed artifacts and source evidence too.
Only an exported source snapshot is accepted; no live Aetheri checkout is built.

```sh
cmake -S examples/35_aetheri_files_consumer -B out/files-consumer \
  -DNexaUI_DIR=/path/to/installed-sdk/lib64/cmake/NexaUI \
  -DNEXAUI_EXPECTED_REVISION=FULL_40_CHARACTER_SDK_SOURCE_REVISION \
  -DAETHERI_SOURCE_DIR=/path/to/exported-original-source \
  -DFILES_TEST_FONT=/path/to/borrowed-font.ttf \
  -DCMAKE_BUILD_TYPE=Release
cmake --build out/files-consumer --parallel 2
ctest --test-dir out/files-consumer --output-on-failure --no-tests=error
```

Use an ordinary non-root user and fresh private test directories. The unchanged
tests perform real local filesystem operations in their own fixtures, including
a test-only lost-mkdir acknowledgement. Their results do not prove production
file grants, durable restart recovery, installed app admission or hostile-writer
exclusion. The original view tests exercise 1×/2× raster density, selection and
scope retirement, modal keyboard/large-text bounds and complete-commit guards.
The three CTests alone are no native compositor, IME, clipboard, independent
assistive-client, physical-display, performance or complete Files acceptance.

`aetheri_files_candidate` is the unchanged original native host. It requires the
same explicitly selected root, font, private 0700 cache and actual Terminal/editor
paths documented in original `spec/files.md`. Do not supply fake child programs
and infer a completed document workflow. `FILES_BUILD_FAULT_HOST=ON` additionally
builds the original controlled lost-reply host for private native tests only;
it is never an installed application or production error injector.

Reconnecting/reopening, actual native file/document/operation flows and the
original independent AT-SPI task remain follow-up evidence for this candidate.
Device/installer/login/lock service observations and authorization retain their
external owners; this Files build supplies none of those capabilities.

## Checked source and evidence

Export committed Aetheri `7483286bcaafbeaced5831f72cb4d0e0cad9a158` with
`git archive` into a separate directory; a full export is accepted, while the
46 inputs in `source-inputs.json` are checked before configuration. The executed
SDK declaration is `c4f467d85b0147944d2456f5e7602a39ac18e579`, distinct from
Aetheri's own reviewed 97a4264 pin. See the [candidate report](../../docs/quality/files-consumer-report.md)
for actual 3×3 results, four refusals, binary/source/archive bindings and the
native, IME, child/document, adoption and physical/human/performance work still
required. A successful build is not that remaining acceptance.

## Native original workflows

Add `-DFILES_BUILD_NATIVE_CHILDREN=ON -DFILES_BUILD_FAULT_HOST=ON` to build the
unchanged original SDL Terminal and modal editor plus affected original tests.
The expanded 87 inputs are in `native-inputs.json`; use their separate immutable
export. SDL3/SDL_ttf/FreeType/HarfBuzz/Wayland versions follow original CMake.
`run_native.py --help` describes the private normal/fault driver arguments.
Its SDK input JSON binds revision/version, exact shared library/hash, the
`atspi_compiled` declaration, `dbus_sdk` prefix and `dbus_headers` path/hash rows.
Use a short owned runtime output path for Unix sockets, real child programs,
and the original private compositor/AT-SPI fixture, never a normal desktop seat.

See [native evidence](../../docs/quality/files-native-report.md) for the actual
3×21 CTests, five native runs, nine lost-result operations, exact artifacts and
observed missing emoji fallback. Host-bound IME, foreign clipboard, localization,
font fallback, normal adoption and physical/human/performance remain open.

## Native IME integration variant

Add `-DFILES_BUILD_IME_HOST=ON` for the separately reviewed generated
`aetheri_files_ime_candidate`. The default remains OFF. Its three generated
application files are recorded in `files-ime-generated/source-delta.json`; the
original export and `aetheri_files_candidate` are retained unchanged. The binding
reuses the installed public text-field/window APIs and original bounded commit
validation. It is an integration experiment, not an approved Aetheri migration.

The IME variant now requires installed SDK **0.1.26**, with the explicitly
selected source declaration `794be7cec49f427fcc56e2604cddb44245347021` in the
executed experiment. The default-OFF original consumer still selects exactly
0.1.23. Keep the two SDK installations and build directories separate.

For the native task also enable `FILES_BUILD_NATIVE_CHILDREN` and use the original
private compositor/AT-SPI fixture described above. Run:

```sh
python3 examples/35_aetheri_files_consumer/run_ime.py \
  --binaries /path/to/files-build --original /path/to/exported-original-source \
  --atspi-fixture /path/to/private-atspi-fixture \
  --sdk-input /path/to/checked-sdk-input.json \
  --expected-sdk-revision 794be7cec49f427fcc56e2604cddb44245347021 \
  --linkage shared --server /path/to/test_compositor_server \
  --font /path/to/borrowed-sans.ttf --mono /path/to/borrowed-mono.ttf \
  --work-dir /short/fresh/private-output
```

Select `--linkage static` for a static SDK profile; the runner checks actual
process mappings. The work directory must be fresh and short enough for Unix
sockets. It performs real create operations only in its own document fixture.
See [earlier IME evidence](../../docs/quality/files-ime-report.md) for its
0.1.23 checkpoint. The preceding sections describe their historical scopes.

The current generated view buffers a complete marked native transaction before
passing it to the real field. It validates the whole proposed name/query using
the original app policy, including deletion, replacement and empty commits.
Rejected proposals retain the draft and block confirmation until an explicit
clear. A temporary public text buffer tracks the byte proposal and grapheme
caret for coqueued transactions; it never mutates the original model. Each real
field has a bounded 32 KiB undo history. A complete native packet occupies one
undo entry, including its original selection direction. The controller's event
buffer has 263 slots and its proposal is bounded by the original 255-byte limit.
Scope, revision and focus changes retire stale/incomplete input. Native payloads
are drained before drawing; ordinary app commands observe completed edits first.

`files_ime_transactions` adds ten public-host-token controller/model cases. Its
substituted native guard is explicitly not a native-window test. The independent
Wayland/AT-SPI task supplies that separate evidence, including actual original
file creation and rename, selection, undo/redo, rejection and close.

For the extended native task, separately export committed Aetheri
`d56d69523c33a48648af343e7d36526f84518c90`. The 17 producer inputs in
`ime-producer-inputs.json` must match. Build its private producer independently:

```sh
cmake -S examples/35_aetheri_files_consumer/ime_producer \
  -B /path/to/fresh-private-producer \
  -DAETHERI_PRODUCER_SOURCE_DIR=/path/to/frozen-d56-export \
  -DCMAKE_BUILD_TYPE=Release
cmake --build /path/to/fresh-private-producer --parallel 2
```

Its exact wlroots/Wayland/protocol dependency versions are checked by CMake.
Only two test-fixture C files are generated; their reversible substitutions are
recorded in `generated/source-delta.json`. No SDL or NexaUI is linked into this
producer. Pass its `files_ime_producer` as `--server` and add `--transactions`
to the runner above. The runner checks generated-source hashes and uses
processed native surrounding-text snapshots before sending each transaction.
All producer diagnostics contain only the private fixture's synthetic text;
do not use this producer for ordinary application input.

Real installed input methods, broader clipboard pressure/lifetime acceptance, broader localization/font coverage,
normal admission/migration and physical/human/performance acceptance remain open.

## Native text clipboard in the generated variant

The generated variant also supplies `nx_text_clipboard_for_window` to its fields.
Keyboard and logical Copy/Cut/Paste reuse the installed SDK's clipboard path;
file Cut/Paste outside an editor remains the original app-owned file operation.
Wayland reads are asynchronous and retain the existing SDK focus, field revision,
selection and close checks. Copy owns an immutable snapshot; Cut removes the
selection only after a successful copy. Ordinary single-line paste converts
CR/LF to spaces under the existing widget contract, then the original Files
whole-proposal validator accepts or rejects the complete draft. The current
variant installs `nx_text_field_set_paste_validator` with persistent per-field
application data, so an invalid/oversized paste is refused before changing text,
selection or undo/redo. The original error message and confirmation barrier remain:
explicit clear is still required before further editing. That deliberate clear
uses `nx_text_field_set_text` and resets history; Undo is not a bypass of the
rejection barrier. The defensive post-edit rollback for other edits retains its
original behavior.

The application combines the current prefix, insertion and suffix into a bounded
256-byte stack buffer and invokes the original whole-draft rule. It checks the
complete byte budget before copying and refuses further coqueued pastes after a
rejection. Function/user identities stay stable across frames, preserving pending
native transfers. Keyboard, logical and independent assistive Paste share that
admission point. The latter reports a rejected edit as rejected.

`files_paste_admission` adds 28 original-view component cases for name/query and
keyboard/logical paths. It checks retained reversed selection, size boundaries,
dot drafts, coqueued rejection and actual retained history/redo. The history probe
temporarily submits the same field through a separate public widget; this is a
component observation, not an extra Files user action or a way around its barrier.

The normal SDK keyboard-paste transport budget remains 32 MiB and five seconds
per read; the accepted app draft remains at most 255 bytes. This is not a new 255-byte
transport budget or full worst-case memory/performance acceptance.

Add `--clipboard-tools /path/to/checked-private-wl-clipboard` to `run_ime.py`
to exercise independent source/receiver processes and AT-SPI clipboard edits.
The fixture must contain the checked `bin/wl-copy`, `bin/wl-paste` and private
musl loader/runtime under `lib`; normal desktop clipboard access is not used.
`--clipboard-budget` additionally tests a 65,537-byte foreign proposal. The task
checks immutable copies, range replacement and undo, complete rejection, late
replies after Tab and app-driven close while the foreign producer is stopped.
EOF alone remains no proof that an external source sent its complete value.
Wayland clipboard writes also require this window's current native input serial.
The private task supplies a fresh Shift key before AT-SPI Cut after a foreign
receiver's focus popup; it does not synthesize a serial or grant app admission.
The extended task also checks rejected reversed Unicode selection, explicit
independent AT-SPI rejection and the original query control-character rule. Its
`clipboard-paste-rejected` peer mode requires an attempted Paste to return false
without a transport error;
the driver separately observes the app's new rejection state and retained text.

## Explicit font fallback and complete naming captions

The generated IME host accepts repeated `--fallback-font /path/to/font.ttf`
arguments, at most four fonts of 4 MiB each. It uses the existing public font
collection API; the primary font keeps precedence. Fonts are explicit host inputs:
there is no system discovery, download or installation. The primary font retains
the original less-than-64-MiB limit. Missing, oversized or unsupported fallback
files fail startup with a diagnostic and release resources.

`ae_files_view_add_fallback` belongs only to this generated application view.
Call it before its first frame. It owns the opened font faces and borrows the
host's byte buffers until view destruction. Layouts/fields/context and collection
are destroyed before those faces; host buffers are freed afterward. Runtime
replacement of the collection is deliberately refused.

List naming controls measure the complete localized Cancel/Abbrechen and OK
captions, retain the original minimum button width and give the remaining width
to the name field. Their original action IDs, keyboard and semantic labels stay
in use. `files_appearance_layout` checks 96 actual drawn-label/layout cases across
English/German, list/grid, four text scales, three window sizes and two raster
densities. Add `-DFILES_TEST_FALLBACK_FONT=/path/to/checked-font.ttf` for the
separate fallback admission/selection/lifetime CTest. That particular test requires
a primary without U+1F30D and a fallback containing it, as specified by the test.

For the native task add `--fallback-font` and optionally `--font-errors` to the
runner above. The latter checks five startup refusals in the private fixture;
its briefly mapped failed host can change which virtual output receives the
following valid window. Every capture retains all three outputs and requires
nonuniform pixels on at least one; select the actual mapped output for review.
This is not proof of rendering on all three outputs or physical migration.

The optional [font input manifest](appearance-font-inputs.json) pins Noto Sans
Symbols2 Regular 2.003 and its OFL-1.1 license for the executed selected-symbol
test. Font bytes are not shipped by this example. U+1F30D is rendered as a
monochrome outline; this does not establish color emoji or complete script
coverage. Broader localization, font distribution/admission, normal Aetheri
migration and physical/human/performance acceptance remain separate gates.

## Current SDK pressure and view-lifetime follow-up

The IME consumer can explicitly select `-DFILES_IME_SDK_VERSION=0.1.33` with
the matching `NEXAUI_EXPECTED_REVISION`. Its default remains exactly0.1.26;
`FILES_BUILD_IME_HOST=OFF` still selects exactly0.1.23. No Aetheri pin changes.
For the explicit0.1.33 Clang profile, `prepare_toolchain.py` records a separate
layout source with one equivalent float zero/nonzero predicate. The original
export and strict conversion diagnostics remain intact.

The component IME test has14 cases. Added cases retain a rejected draft when a
valid packet follows before the frame, reject event-buffer overflow, and destroy
and recreate the real view with an incomplete or accepted-but-unpainted packet.
Retired tokens must not edit the new draft; current edits and Undo still work.
The private producer adds commands28/29; use `--transactions --pressure` to
exercise native oversized and rejected-then-valid packets. These are text
transactions, not filesystem delete operations or a new file-operation owner.

`run_ime.py --paste-timeout-guard --clipboard-tools TOOLS
--clipboard-stream-gate LIBRARY` additionally reuses the Editor's
`../36_aetheri_editor_consumer/foreign_stream_task.py`. Keep that helper in
isolated source exports. Only the independent `wl-copy` child loads the gate.
The guard holds a real incomplete UTF-8 writer beyond the unchanged one-second
AT-SPI deadline, requires peer exit1 with an actual NoReply, closes the writer,
and checks the unchanged original name/files. This deliberate failure is
separate from ordinary successful native replies and semantic refusals.

The current scoped0.1.33 follow-up passes nine component CTests and the native
IME/clipboard/create/rename runner on each of Release-static, Release-shared and
Debug-O1 ASan/UBSan/LSan. It does not supersede the historical0.1.26 full original
document-child regression. The local musl libc lacks the original launch module's
`posix_spawn_file_actions_addclosefrom_np`; the private scoped tests use an
aborting tripwire for that unavailable function and never accept document launch.
The optional `NEXAUI_AETHERI_MUSL_COMPAT` reuses Example23's Terminal syscall
bridge only; it does not supply that missing Files spawn action or change FD
closing policy. Full0.1.33 document-child regression on the required libc,
actual Aetheri compositor/IME, broader pressure/lifetime and normal adoption
remain open.
