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

For the native task also enable `FILES_BUILD_NATIVE_CHILDREN` and use the original
private compositor/AT-SPI fixture described above. Run:

```sh
python3 examples/35_aetheri_files_consumer/run_ime.py \
  --binaries /path/to/files-build --original /path/to/exported-original-source \
  --atspi-fixture /path/to/private-atspi-fixture \
  --sdk-input /path/to/checked-sdk-input.json \
  --expected-sdk-revision c4f467d85b0147944d2456f5e7602a39ac18e579 \
  --linkage shared --server /path/to/test_compositor_server \
  --font /path/to/borrowed-sans.ttf --mono /path/to/borrowed-mono.ttf \
  --work-dir /short/fresh/private-output
```

Select `--linkage static` for a static SDK profile; the runner checks actual
process mappings. The work directory must be fresh and short enough for Unix
sockets. It performs real create operations only in its own document fixture.
See [IME evidence](../../docs/quality/files-ime-report.md) for 3×23 CTests,
three native IME tasks, the original document regression, lossless images and
negative controls. Native delete/replacement grouping, late/interrupted/empty
transactions, real installed IMEs, foreign clipboard and full acceptance remain
open. The preceding sections describe their historical checkpoint scopes.
