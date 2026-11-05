# Aetheri terminal model: bounded visible view

This external C17 consumer adapts the **actual Aetheri terminal model** to the
existing NexaUI cell grid and read-only semantic snapshot. It is an incremental
presentation experiment, not a complete terminal application or adoption pass.
The model, escape parser, history, selection and copy remain Aetheri-owned.
This example neither modifies nor pins the Aetheri checkout.

## Reproduce

Build against an installed NexaUI 0.1.14 or newer and a separately obtained,
immutable Aetheri source snapshot. The checked model came from Aetheri commit
`87996e2f12128b172a80b325ad3b539df6aee527`; `model.c` Git blob is
`5865f239a0c9b2c83392f3b7a1f341d5d4cea28e`, `model.h` is
`cdcba69432c363c93848666d7b47fa73bac3f2bd`. NexaUI core and installed SDK
for the checks below were based on `129dea2bffa9f084ee1dd7949fa69134291264f7`.
Builds and this example do not need to write to Aetheri.

```sh
cmake -S examples/23_aetheri_terminal_view -B /tmp/terminal-view \
  -DCMAKE_PREFIX_PATH=/path/to/installed/nexaui \
  -DAETHERI_SOURCE_DIR=/path/to/aetheri-snapshot
cmake --build /tmp/terminal-view
/tmp/terminal-view/aetheri_terminal_view_test /path/to/monospace.ttf
```

On Linux with libatspi development files, D-Bus, a registry and Python, an
independent client can exercise the simulated accessibility host:

```sh
export NEXAUI_TERMINAL_FONT=/path/to/monospace.ttf
python3 tests/run_atspi_native.py /path/to/at-spi2-registryd \
  /tmp/terminal-view/aetheri_terminal_atspi_client \
  /tmp/terminal-view/aetheri_terminal_ax_host
NEXAUI_TERMINAL_AX_SCROLL=1 python3 tests/run_atspi_native.py \
  /path/to/at-spi2-registryd /tmp/terminal-view/aetheri_terminal_atspi_client \
  /tmp/terminal-view/aetheri_terminal_ax_host
```

The host is a headless fixture with simulated focus and geometry; it is not a
native Aetheri window. The first run checks real AT-SPI text, reverse selection,
caret, and rejection of mutation on a read-only snapshot. The second checks that
scrollback without a visible caret does not invent one. The fixture's zero-offset
combining-mark callback is synthetic and is not a shaping or visual-quality test.

## Contract and limits

- The app retains the model and fonts until the view is destroyed; access is on
  the same owner thread. It supplies real combining-mark offsets and a bold font
  when needed. Neither font fallback nor shaping is provided by this adapter.
- The owner supplies a revision increasing for every model mutation, including
  selection clearing. The adapter also checks model sequence and selection state;
  this catches selection clear in the tested Aetheri revision, which does not
  itself advance the model sequence. It cannot detect arbitrary unannounced
  mutations. Borrowed frame data expires at the next frame or destruction.
- Only visible cells are copied. The semantic value includes visible spaces and
  row separators, including soft-wrapped rows. It is not clipboard content:
  selection copying must use the original model's copy operation.
- A snapshot is limited to 65,536 UTF-8 bytes. Oversized frames fail explicitly;
  they are never silently truncated. If no actual caret or entire selection is
  visible, the plain read-only semantic node is published without AT-SPI Text;
  readable scrollback with an absent caret remains an SDK accessibility gap.
- Glyph lookup may return the font's missing-glyph index. Correct glyph coverage,
  width policy, mark positioning and actual font metrics remain host obligations.

## Evidence and remaining work

On 5 November 2026, Alpine Linux/WSL consumers used installed static and shared
SDKs, plus a Clang ASan/UBSan build. The model test exercises wide cells,
combining marks, attributes, visible selection and original-model copy, stale
revisions, resizing, scrollback, alternate screen and explicit size errors.
The independent libatspi fixture checks actual D-Bus interactions; its geometry
and focus are simulated. No PTY, live keyboard/IME, native selection gestures,
independent clipboard/paste, editor, session retention, measured performance,
physical display or human screenreader acceptance is proved here. These remain
open in delivery packet 5 and the quality gates.
