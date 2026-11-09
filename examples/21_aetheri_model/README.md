# Aetheri Settings model integration experiment

This standalone C17 consumer compiles the original Aetheri Settings model from
an explicitly supplied source snapshot and links an installed NexaUI package.
It edits the in-memory appearance, language and text-size fields through
keyboard or accessibility actions. The model and font bytes belong to the
caller and outlive the view. Destroying and recreating the view retains the
model. Display observations supplied by the model owner are selectable by stable
ID; stale observations are visible but disabled. An unpopulated model is shown
as not yet observed, not as a confirmed empty output list. Escape and the Close
button request an orderly exit.

Supply a complete immutable export for the chosen model version. Current
Settings models also need `src/audio/types.h` and the original UTF-8 helper
`src/text/grapheme.c`, `grapheme.h` and `grapheme_data.h`. The build exposes the
original `src` include root and compiles that helper when present. Older
model-only exports remain supported. This example still presents display,
appearance and language; compiling the current sound model does not add a Sound
page, connect an audio service or submit device changes.
With a current Sound-capable model, `-DNEXAUI_MODEL_BUILD_SOUND_TEST=ON` adds
`aetheri_model_sound_state_test FONT`. It checks that view replacement and
appearance/language changes retain the original observation, output identity,
once-only queued intent and permanent owner-loss fence, without device effects.
The [compatibility report](../../docs/quality/settings-model-compatibility-report.md)
records exact current/legacy sources, sanitizer checks, compiled counterexamples
and the separate remaining Sound/native service integration.

## Optional original Settings/Sound presentation

`-DNEXAUI_MODEL_BUILD_ORIGINAL_VIEW=ON` compiles the supplied Aetheri
`src/settings/nexaui_view.c` directly, using the same original model and an
installed NexaUI 0.1.31 or newer. It adds `aetheri_original_view`,
`aetheri_original_view_test` (the unchanged original `tests/test_settings_view.c`)
and `aetheri_original_sound_test`. On Linux it also builds
`aetheri_original_settings` directly from the original `nexaui_main.c`; that file
already supplies the executable entry point. The existing example targets and
the default OFF behavior remain available.

In addition to the complete current model export listed above, supply
`src/settings/nexaui_view.c`, `nexaui_view.h`, `nexaui_main.c`, `nexaui_main.h`
and `tests/test_settings_view.c` from the same immutable Aetheri revision.
Retain its `LICENSE`, `src/settings/NEXAUI-NOTICE.txt` and the text module's
`UNICODE-LICENSE.txt` with the export. No original application files or fonts
are bundled here. For example:

```sh
cmake -S examples/21_aetheri_model -B build-original-settings \
  -DCMAKE_PREFIX_PATH=/path/to/nexaui-install \
  -DAETHERI_SOURCE_DIR=/path/to/aetheri-snapshot \
  -DNEXAUI_MODEL_BUILD_ORIGINAL_VIEW=ON
cmake --build build-original-settings
timeout 30 build-original-settings/aetheri_original_view_test /path/to/font.ttf 1
timeout 30 build-original-settings/aetheri_original_view_test /path/to/font.ttf 2
timeout 30 build-original-settings/aetheri_original_sound_test /path/to/font.ttf
build-original-settings/aetheri_original_settings --font /path/to/font.ttf
```

The original view supplies Displays, Appearance, Language and Sound, its own
chrome, keyboard focus, text scaling and semantics. Its original host supplies
Wayland observations, software presentation, native action routing and normal
close/signal teardown. The new Sound test covers all 64 admitted output rows,
a 511-byte accessible description, keyboard default-output intent, replacement,
reordering, once-only dispatch, independently supplied observation and permanent
loss fencing. These observations are deterministic test inputs, not audio-device
effects. The ordinary executable does not enable `AETHERI_SETTINGS_AUDIO_HOST`;
Sound reports unavailable until a separately admitted original audio owner is
integrated. No GLib/PipeWire dependency or new service admission is added to
the SDK. The view has no editable text field and therefore supplies no IME task.
Physical input, screenreader speech, canonical-font/design acceptance and normal
Aetheri adoption remain separate gates.
The [original-view report](../../docs/quality/settings-original-view-report.md)
binds exact sources, installed profiles, native tasks, captures and retained
failures without closing those wider gates.

With NexaUI 0.1.32 or newer, `NEXAUI_MODEL_BUILD_ORIGINAL_AUDIO_ENTRY=ON` adds
`aetheri_original_audio_entry`, compiling the same original `nexaui_main.c` with
`AETHERI_SETTINGS_AUDIO_HOST` and `AETHERI_SETTINGS_NO_MAIN`. Enable the original
view too, on Linux, and set `AETHERI_SETTINGS_AUDIO_HOST_TARGET` to an existing
caller-defined target providing Aetheri's original `settings/audio_host.c`
implementation and its transitive application dependencies, including GLib.
Supply `src/settings/audio_host.h` from the same immutable Aetheri snapshot.
The example does not construct or admit an audio session. The normal executable
still uses its original ordinary, audio-unavailable entry point.

The embedding application calls the original `ae_settings_nexaui_run(argc, argv,
audio_host)` with an already independently admitted original host on its owning
thread/process and pushed thread-default `GMainContext`. The original entry
borrows that host for the run, binds its temporary model and unbinds before
returning. The caller retains the host/session/context, closes them through their
original contracts and keeps iterating until deferred owner retirement finishes.
The existing original prepare/finish/abort path surrounds the SDK window wait;
no second poll loop, worker thread, service admission or reconnect is introduced.
Use the same original model/source generation for the supplied host target.

For example, in a parent project that already defines `aetheri_settings_audio_host`:

```cmake
set(NEXAUI_MODEL_BUILD_ORIGINAL_VIEW ON CACHE BOOL "Original view")
set(NEXAUI_MODEL_BUILD_ORIGINAL_AUDIO_ENTRY ON CACHE BOOL "Original audio entry")
set(AETHERI_SETTINGS_AUDIO_HOST_TARGET aetheri_settings_audio_host CACHE STRING "Original host")
add_subdirectory(/path/to/NexaUI/examples/21_aetheri_model settings-consumer)
target_link_libraries(my_original_owner PRIVATE aetheri_original_audio_entry)
```

The [isolated audio fixture](../../tests/aetheri_settings_audio/README.md) combines
the unchanged original entry/pump/GLib adapter with explicitly controlled test
observations and an independent AT-SPI peer. It supplies no real service or
device effect. The 0.1.32 minimum includes the slider/drag-value semantic-step
path required by native assistive volume actions; numeric Value interfaces and
other accessibility acceptance remain separate work.

On Linux, with a Wayland-only NexaUI SDK installed and a compatible Aetheri
source snapshot available:

```sh
cmake -S examples/21_aetheri_model -B build-aetheri-model \
  -DCMAKE_PREFIX_PATH=/path/to/nexaui-install \
  -DAETHERI_SOURCE_DIR=/path/to/aetheri-snapshot
cmake --build build-aetheri-model
build-aetheri-model/aetheri_model_view_test /path/to/font.ttf
build-aetheri-model/aetheri_model_text_wrap_test /path/to/font.ttf
build-aetheri-model/aetheri_model_view /path/to/font.ttf
```

The process requires a working Wayland session and a TrueType font. The
software renderer supports logical window sizes down to 128 by 150. Below that
size this example does not render a new frame; it needs a smaller-layout design.
Native
accessibility requires a NexaUI build with AT-SPI and an available service;
the view remains usable when that optional service is unavailable. `--verify`
prints the three appearance model values for the automated integration fixture.
The explicit `--fixture-displays` test option supplies two synthetic displays;
it is not an operating-system observation.
The Linux host uses NexaUI's Wayland output snapshot on its existing connection
and supplies it to the original Aetheri model; updates wake the owner thread.
Output IDs are connection-local and selecting one does not change system
configuration. Another embedding owner must request a new frame when it
updates the model.
For an isolated owner integration probe, the executable also accepts
`--font FONT --cache DIR --diagnostics`, matching the development owner's
argument shape. The cache argument is accepted but unused; this example creates
no cache or persistence. Escape, Ctrl+Q and SIGINT/SIGTERM request normal
owner-thread teardown. This compatibility does not change Aetheri's launcher
or establish its app identity, commands, service behavior or adoption.

This experiment is not the complete Aetheri Settings application: it does not
integrate real Aetheri service requests, persist settings, implement an IME text
field, or perform OS changes. Its compositor output observations are not proof
of physical monitors or display control.
Its measured control wrapping and focus-following scroll area retain the chosen
text size, but need broader font, viewport and human accessibility review. The
layout remains provisional. At narrow widths its title scrolls with the content;
below 320x240 radio marks and display buttons have a separate text line, and
Page Up/Down scrolls through labels longer than the viewport. The visible
labels may break at grapheme boundaries while accessible names remain intact.
Controlled keyboard tests cover German at 200% down to 128x150. A
private headless Sway test drives a native 320x240 resize and restores 800x480.
With `--atspi-client` and `--atspi-registry`, an independent client changes
the model before and during that resize and checks the semantic window sizes.
The separate `--wtype` mode drives a private virtual keyboard through the
resize, including focus retention and Escape. It does not test physical input,
all fonts or languages. Build
`atspi_client.c` separately using the `atspi-2`, `gobject-2.0` and `dbus-1`
pkg-config modules; run `run_sway_resize.py --help` for fixture inputs. The broader Settings example in `examples/18_settings`
provides separate text/IME and asynchronous service examples. Actual Aetheri
consumer adoption, physical hardware and human accessibility review remain
open. See `docs/quality/aetheri-adoption-assessment.md` for scoped evidence.

The view keeps bounded text-layout caches keyed by the original text, wrap
width, text size and small-layout mode. Changing those values rebuilds the
affected layout; removing displays releases their cached layouts. To run the
optional headless 16-display layout benchmark, configure the standalone build
with `-DNEXAUI_MODEL_BUILD_BENCHMARK=ON` and run
`aetheri_model_view_benchmark /path/to/font.ttf`. It does not measure native
presentation or physical input.
For a single unbroken printable ASCII name, narrow wrapping can reuse caret
positions from the initial shape. It validates the reshaped result and falls
back to the general grapheme path when that check or its preconditions fail.

For the optional virtual-pointer check, generate the helper from the installed
`wlr-protocols` XML in a temporary directory, then pass its binary as
`--pointer-driver` together with `--wtype` and a size below 320x240:

```sh
wayland-scanner client-header /usr/share/wlr-protocols/unstable/wlr-virtual-pointer-unstable-v1.xml /tmp/virtual-pointer-client.h
wayland-scanner private-code /usr/share/wlr-protocols/unstable/wlr-virtual-pointer-unstable-v1.xml /tmp/virtual-pointer-protocol.c
cc -std=c17 -I/tmp examples/21_aetheri_model/virtual_pointer.c /tmp/virtual-pointer-protocol.c -lwayland-client -o /tmp/nexaui-virtual-pointer
```

The helper requires the fixture's private-Sway guard and is not linked into
the SDK or the application.
