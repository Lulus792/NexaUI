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
build-aetheri-model/aetheri_model_view /path/to/font.ttf
```

The process requires a working Wayland session and a TrueType font. The
software renderer uses a minimum logical window size of 480 by 380. Native
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
layout remains provisional. The broader Settings example in `examples/18_settings`
provides separate text/IME and asynchronous service examples. Actual Aetheri
consumer adoption, physical hardware and human accessibility review remain
open. See `docs/quality/aetheri-adoption-assessment.md` for scoped evidence.
