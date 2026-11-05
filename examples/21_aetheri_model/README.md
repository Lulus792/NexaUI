# Aetheri Settings model integration experiment

This standalone C17 consumer compiles the original Aetheri Settings model from
an explicitly supplied source snapshot and links an installed NexaUI package.
It edits the in-memory appearance, language and text-size fields through
keyboard or accessibility actions. The model and font bytes belong to the
caller and outlive the view. Destroying and recreating the view retains the
model. Escape and the Close button request an orderly exit.

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
prints only the three model values for the automated integration fixture.

This experiment is not the complete Aetheri Settings application: it does not
enumerate displays, integrate Aetheri's existing process owner or service
requests, persist settings, implement an IME text field, or perform OS changes.
Its measured control wrapping and focus-following scroll area retain the chosen
text size, but need broader font, viewport and human accessibility review. The
layout remains provisional. The broader Settings example in `examples/18_settings`
provides separate text/IME and asynchronous service examples. Actual Aetheri
consumer adoption, physical hardware and human accessibility review remain
open. See `docs/quality/aetheri-adoption-assessment.md` for scoped evidence.
