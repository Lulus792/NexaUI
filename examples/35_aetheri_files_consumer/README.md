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

The IME variant now requires installed SDK **0.1.25**, with the explicitly
selected source declaration `ca1cb3db26badde2c6fbe3cc1bae1de1b149ad18` in the
executed experiment. The default-OFF original consumer still selects exactly
0.1.23. Keep the two SDK installations and build directories separate.

For the native task also enable `FILES_BUILD_NATIVE_CHILDREN` and use the original
private compositor/AT-SPI fixture described above. Run:

```sh
python3 examples/35_aetheri_files_consumer/run_ime.py \
  --binaries /path/to/files-build --original /path/to/exported-original-source \
  --atspi-fixture /path/to/private-atspi-fixture \
  --sdk-input /path/to/checked-sdk-input.json \
  --expected-sdk-revision ca1cb3db26badde2c6fbe3cc1bae1de1b149ad18 \
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

Real installed input methods, broader clipboard pressure/lifetime acceptance, localization/font fallback,
normal admission/migration and physical/human/performance acceptance remain open.

## Native text clipboard in the generated variant

The generated variant also supplies `nx_text_clipboard_for_window` to its fields.
Keyboard and logical Copy/Cut/Paste reuse the installed SDK's clipboard path;
file Cut/Paste outside an editor remains the original app-owned file operation.
Wayland reads are asynchronous and retain the existing SDK focus, field revision,
selection and close checks. Copy owns an immutable snapshot; Cut removes the
selection only after a successful copy. Ordinary single-line paste converts
CR/LF to spaces under the existing widget contract, then the original Files
whole-proposal validator accepts or rejects the complete draft. Invalid/oversized
names retain the app's previous text and block confirmation until explicit clear.
That original rollback uses `nx_text_field_set_text`, including its history-reset
semantics; it does not preserve the field's preceding history on rejection.

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
