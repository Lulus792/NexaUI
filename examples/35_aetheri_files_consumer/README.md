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
