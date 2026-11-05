# Aetheri terminal model: bounded visible view

This external C17 consumer adapts the **actual Aetheri terminal model** to the
existing NexaUI cell grid and read-only semantic snapshot. It is an incremental
presentation experiment, not a complete terminal application or adoption pass.
The model, escape parser, history, selection and copy remain Aetheri-owned.
This example neither modifies nor pins the Aetheri checkout.

## Reproduce

Build against an installed NexaUI 0.1.15 or newer and a separately obtained,
immutable Aetheri source snapshot. The checked model came from Aetheri commit
`87996e2f12128b172a80b325ad3b539df6aee527`; `model.c` Git blob is
`5865f239a0c9b2c83392f3b7a1f341d5d4cea28e`, `model.h` is
`cdcba69432c363c93848666d7b47fa73bac3f2bd`; `paste.c` is
`a5a79d13b6bb1a8a49d7442121592435495bfb0d` and `paste.h` is
`aa719aa40bbeb0b78110ba7dbf183a5d73edfd1a`. The original 0.1.14 example
used core based on `129dea2bffa9f084ee1dd7949fa69134291264f7`. The 0.1.15
no-caret snapshot extension was developed on NexaUI base
`4763bdd73143708207e678e5df204bc375d98ad6`.
Builds and this example do not need to write to Aetheri.

```sh
cmake -S examples/23_aetheri_terminal_view -B /tmp/terminal-view \
  -DCMAKE_PREFIX_PATH=/path/to/installed/nexaui \
  -DAETHERI_SOURCE_DIR=/path/to/aetheri-snapshot
cmake --build /tmp/terminal-view
/tmp/terminal-view/aetheri_terminal_view_test /path/to/monospace.ttf
/tmp/terminal-view/aetheri_terminal_selection_test /path/to/monospace.ttf
/tmp/terminal-view/aetheri_terminal_paste_test
/tmp/terminal-view/aetheri_terminal_review_test /path/to/monospace.ttf
# Optional: a proportional font tests an overwide glyph on a later page.
/tmp/terminal-view/aetheri_terminal_review_test /path/to/monospace.ttf /path/to/proportional.ttf
```

On Linux, the optional controlled Wayland fixture uses a NexaUI checkout and
the generated protocols from an already configured NexaUI test build:

```sh
cmake -S examples/23_aetheri_terminal_view -B /tmp/terminal-view \
  -DCMAKE_PREFIX_PATH=/path/to/installed/nexaui \
  -DAETHERI_SOURCE_DIR=/path/to/aetheri-snapshot \
  -DNEXAUI_FIXTURE_SOURCE_DIR=/path/to/nexaui-checkout \
  -DNEXAUI_FIXTURE_PROTOCOL_DIR=/path/to/nexaui-build/generated/wayland
cmake --build /tmp/terminal-view
/tmp/terminal-view/aetheri_terminal_paste_wayland_test /path/to/monospace.ttf
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
NEXAUI_TERMINAL_AX_TRANSITION=1 python3 tests/run_atspi_native.py \
  /path/to/at-spi2-registryd /tmp/terminal-view/aetheri_terminal_atspi_client \
  /tmp/terminal-view/aetheri_terminal_ax_host
NEXAUI_TERMINAL_AX_EVENT=1 NEXAUI_TERMINAL_AX_TRANSITION=1 \
  python3 tests/run_atspi_native.py /path/to/at-spi2-registryd \
  /tmp/terminal-view/aetheri_terminal_atspi_client \
  /tmp/terminal-view/aetheri_terminal_ax_host
python3 tests/run_atspi_native.py /path/to/at-spi2-registryd \
  /tmp/terminal-view/aetheri_terminal_review_atspi_client \
  /tmp/terminal-view/aetheri_terminal_review_ax_host
NEXAUI_REVIEW_ACCEPT=1 python3 tests/run_atspi_native.py /path/to/at-spi2-registryd \
  /tmp/terminal-view/aetheri_terminal_review_atspi_client \
  /tmp/terminal-view/aetheri_terminal_review_ax_host
NEXAUI_REVIEW_DE_ZOOM=1 NEXAUI_REVIEW_ACCEPT=1 \
  python3 tests/run_atspi_native.py /path/to/at-spi2-registryd \
  /tmp/terminal-view/aetheri_terminal_review_atspi_client \
  /tmp/terminal-view/aetheri_terminal_review_ax_host
```

The host is a headless fixture with simulated focus and geometry; it is not a
native Aetheri window. The first run checks real AT-SPI text, reverse selection,
caret, and rejection of mutation on a read-only snapshot. The second checks that
scrollback remains readable without an invented caret; the third queries the same
accessible across the transition from an active selection to scrollback. The fixture's zero-offset
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
- The example can route host-authorized `nx_event` pointer selection and
  Ctrl+Shift+A/arrow selection to the original model, including cancel and
  focus loss. The host must enforce focus/modal/input policy, copy any queued
  events before rebuilding, and build a newer frame after every mutation. The
  mapping uses Aetheri's boundary policy; it does not use cell hit identity as
  a substitute for the model's selection boundaries. IME-composition and paste
  review precedence remain the host's responsibility.
- Only visible cells are copied. The semantic value includes visible spaces and
  row separators, including soft-wrapped rows. It is not clipboard content:
  selection copying must use the original model's copy operation.
- A snapshot is limited to 65,536 UTF-8 bytes. Oversized frames fail explicitly;
  they are never silently truncated. If no actual caret or entire selection is
  visible, the read-only AT-SPI Text snapshot reports caret offset -1 and no
  selection. Partial offscreen selection is not represented as a complete one.
- Glyph lookup may return the font's missing-glyph index. Correct glyph coverage,
  width policy, mark positioning and actual font metrics remain host obligations.
- `paste_flow` uses the real nonblocking Wayland clipboard API and original
  `ae_terminal_paste_set`/preview. The owner must advance identity, revision and
  focus epoch and supply the actual focus state; it must cancel on close and
  render/navigate the complete review before a fresh non-repeat confirmation.
  The owner copies confirmed bytes into its own PTY write queue. The example
  does not write to a PTY. Returned text is borrowed and expires at reset;
  reset is not a secure-erasure promise.
- The example `paste_review` scene uses NexaUI's modal and buttons, and shows
  every escaped preview row in navigable pages. Cancel has initial focus;
  Escape cancels and repeated Enter does not confirm. Page navigation and
  actions are exposed to AT-SPI. Destroying an open review cancels it. The scene
  supports English and German, configurable logical size and 16–32 px text,
  with 16 preview columns. It checks every preview row before accepting a new
  size or font scale; rejection retains the previous configuration and paste.
  The caller must not present the old layout in a window that no longer fits.
  The visible source position is
  preserved where possible on reflow. After geometry/text/language changes,
  confirmation waits for a frame in which activation keys and buttons are
  released. Integration in the actual Aetheri window remains open. A fresh
  confirmation returns bytes to the app, not to a PTY.

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

The 0.1.15 extension was checked on Alpine Linux/WSL with installed static and
shared SDKs and Clang ASan/UBSan. The independent client read the expected
scrollback, observed -1 with no selection, and exercised a transition on the
same accessible object; the existing selected-text case also passed. The core
semantic snapshot tests passed for static, shared and sanitizer configurations.
The existing private-Weston large-text test was skipped outside its required
fixture; a follow-up could not start because Weston was unavailable in this
environment. These runs are not native Aetheri or human screenreader acceptance.

The event-selection example was developed on NexaUI base
`3f93c535fd3bfa9387a232a732594210f3cd6dfd` and checked against the same
Aetheri model snapshot using installed static and shared SDKs and Clang
ASan/UBSan. Queued pointer events (including several in one frame), reverse
selection, original-model copy, Ctrl+Shift keyboard selection, consumed keys,
focus loss, pointer cancellation and stale model changes were exercised. An
isolated negative control that removed selection extension failed the copy
assertion. A separate libatspi client also read a reverse selection created by
these events, then the same accessible after scrolling. The events are synthetic
and no native pointer capture is proved.

The paste flow was developed on NexaUI base
`aecd753658edf0b0db379eee16a1e527d45f59f2`. Static, shared and Clang
ASan/UBSan tests exercised the original Aetheri paste model, full 4096-byte
preview traversal, CRLF normalization, rejected controls, explicit review,
cancellation, duplicate delivery and stale focus/revision. The real NexaUI
Wayland async transport was exercised against an in-process controlled protocol
server, including a stalled transfer revoked on focus-epoch change. A deliberately
broken stale guard failed the test. An independent application clipboard peer,
native review UI, PTY writes and real Aetheri session remain open.

The standalone review scene was developed on NexaUI base
`3b4daee263a485639ea313a59853d97d252060bc`. Static, shared and Clang
ASan/UBSan runs covered keyboard paging, modal focus, repeat suppression,
guard changes, cancel and confirmation. Headless software output at 640x360
was visually inspected with DejaVu Sans Mono. An independent libatspi client
read the escaped rows, used Next, and exercised both Cancel and Paste against a
simulated host with simulated native focus/geometry. A stale-guard mutant was
detected. The controlled Wayland test also connected actual asynchronous receipt
to the scene and exercised both confirmation and cancellation. This does not prove native Aetheri integration, a physical display,
multiple scales, localization, or human assistive-technology acceptance.

The reflow update was developed on NexaUI base
`14462a33126acbde336151bfe921be7cf4024dd5`. At 480x380 logical pixels and
32 px German text, the test verifies all buttons, complete last-row navigation,
preservation through resize, focus and suppression of held keyboard/pointer
activation. Bounds were additionally checked at 400x320/16 px and 960x640/32 px.
Dark and light software-rendered images were visually inspected;
an independent libatspi client navigated and confirmed that German scene with
simulated native focus/geometry. Static, shared and ASan/UBSan configurations
passed; disabling the resize guard produced a failing counterexample. This is
not acceptance at every size, font, native pixel scale, compositor or locale.

The transactional layout check was developed on NexaUI base
`b009c9f25205845c43e250707061fb0c6090c4d5`. A 400x320/32 px request is rejected
without losing the model, old layout or focus. A proportional-font case verifies
that an overwide row on a later page rejects the candidate, while a matching
narrow-row case succeeds. A mutant that checked only the first row was detected.
The guard also covers held activation after a rejected resize. A single local
Alpine/WSL measurement of creation and preflight for 4096 bytes with roughly
1365 distinct short preview rows took about 0.07 seconds (shell `time`, static
SDK, DejaVu Sans Mono); this is not a target-hardware latency guarantee.
