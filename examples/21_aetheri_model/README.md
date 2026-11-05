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
