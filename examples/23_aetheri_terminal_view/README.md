# Aetheri terminal model: bounded visible view

This external C17 consumer adapts the **actual Aetheri terminal model** to the
existing NexaUI cell grid and read-only semantic snapshot. It is an incremental
presentation experiment, not a complete terminal application or adoption pass.
The model, escape parser, history, selection and copy remain Aetheri-owned.
This example neither modifies nor pins the Aetheri checkout.

## Reproduce

Build against an installed NexaUI 0.1.19 or newer and a separately obtained,
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

- The app retains the model, font collections, faces and font bytes until the
  view is destroyed; access is on the same owner thread. It supplies real
  combining-mark offsets and the desired regular/bold face policy. Optional
  ordered collections supply missing glyphs; this adapter does not shape text.
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
- A read-only snapshot is limited to 512 KiB of UTF-8. Oversized frames fail explicitly;
  they are never silently truncated. If no actual caret or entire selection is
  visible, the read-only AT-SPI Text snapshot reports caret offset -1 and no
  selection. Partial offscreen selection is not represented as a complete one.
- Without an explicit fallback collection, glyph lookup preserves the primary
  font's missing-glyph index. With one, unavailable coverage returns
  `NX_ERROR_UNSUPPORTED` before presenting the frame. Width policy,
  mark positioning across selected fonts and actual font metrics remain host
  obligations; the grid limits the number of distinct faces in a frame.
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

On candidate base `c3fdd31ae945626b40225e2464421bff24b98417`, the 0.1.19
development SDK separates the read-only snapshot bound (512 KiB) from the
unchanged writable-request bound (64 KiB). The full 240-by-100 Aetheri model
filled with four-byte Unicode characters previously failed the frame at the
64-KiB boundary; it now renders and publishes all 96,099 bytes including row
separators. An independent libatspi client read and checked all 24,000 scalars
and 99 separators from the copied snapshot. Static, shared and Clang
ASan/UBSan consumers passed on Alpine/WSL, along with core bounds and grid
tests. The accessible host uses simulated geometry/focus, the sample uses one
font's missing-glyph behavior, and this does not establish Unicode shaping,
physical rendering, human screenreader use, rapid large updates or production
Aetheri integration. This is a development package, not a 1.0.0 release.

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

The independent clipboard integration was developed on NexaUI base
`782ba7d210fe960af2bd391c2143aaa629fbca4a`, with Aetheri model/paste source
`87996e2f12128b172a80b325ad3b539df6aee527` and an installed 0.1.15 SDK based
on `3f93c535fd3bfa9387a232a732594210f3cd6dfd`. On 5 November 2026, separate
`wl-copy`/`wl-paste` 2.2.1 processes and a private Weston 14.0.2/Xvfb session
exercised actual Wayland receipt, Review Cancel and Paste, and Escape during an
intentionally stalled independent owner. A separate payload containing ESC was
rejected before review or delivery. Static, shared and Clang ASan/UBSan builds
passed; the consumer reports only a test length/checksum and never writes a PTY.
The Alpine package `wl-clipboard-2.2.1-r1.apk` SHA-256 was
`ba96cc7dc6daf8998816a556ff2a915c54087428655632f6fd5abedb42fed853`;
it was extracted into a temporary directory, not installed globally. Weston
binary SHA-256 was `9803344d5ed2c07c4976524b948d895eba120fb822a73f4db7c6fa018538934f`.
The test driver is `run_paste_native.py` and requires the existing private
`tests/run_weston_native.py` fixture, xdotool, the two clipboard binaries and
the installed example target `aetheri_terminal_native_paste`. It is a test
consumer with virtual input, not Aetheri's production terminal. Independent
copy in the opposite direction, real Aetheri integration, actual PTY writes,
other compositors and physical/human acceptance remain open.

```sh
NEXAUI_WESTON=/path/to/weston NEXAUI_WESTON_SHELL=/path/to/desktop-shell.so \
  NEXAUI_WESTON_LIBEXEC=/path/to/weston-libexec xvfb-run -a \
  python3 tests/run_weston_native.py --work-dir /tmp/new-terminal-peer-run -- \
  python3 examples/23_aetheri_terminal_view/run_paste_native.py \
  /tmp/terminal-view/aetheri_terminal_native_paste /path/to/font.ttf \
  /path/to/xdotool /path/to/wl-copy /path/to/wl-paste
```

The previously skipped `nexaui_atspi_large_text_wayland_private` test was later
executed successfully on the same kind of private Weston/Xvfb fixture against
the 0.1.15 static build. This does not change the boundaries of the earlier
headless AT-SPI evidence.

On NexaUI base `ad4a54fcaa01083ec26c228e64d78b0bc2ea5c25`, the separate
`aetheri_terminal_native_copy` consumer connects NexaUI selection events to the
original Aetheri model and publishes the model's own copied bytes. An independent
`wl-paste` 2.2.1 process received a two-line selection with a two-cell character
and a combining mark under private Weston 14/Xvfb. After clearing the selection,
an independent `wl-copy` owner published a marker; Ctrl+C and empty Ctrl+Shift+C
left that marker intact. Static, shared and Clang ASan/UBSan runs passed with
virtual keyboard input. A deliberately truncated publication failed the same
receiver check. This demonstrates transfer while the owner is running; clipboard
persistence after owner exit, a real Aetheri session, PTY behavior, physical
input, other compositors and human accessibility acceptance remain open.

The private fixture command above can be reused by replacing the driver and
consumer arguments with `run_copy_native.py` and
`/tmp/terminal-view/aetheri_terminal_native_copy`; the same font, xdotool,
`wl-copy` and `wl-paste` arguments apply.

### Original shell and PTY lifetime experiment

On NexaUI base `6c44a9abc9a3fd25901f71ec2dad76579b68affe`, this optional
headless test builds the original Aetheri terminal model, PTY and shell from
the immutable Aetheri snapshot `87996e2f12128b172a80b325ad3b539df6aee527`.
The PTY source blob is `e1edae65915a9749466550bee0265bdf7a4166ff` and the
shell source tree is `d0fb1d637b00bb330f0c38bc00e5e8111d087eae`. The test
starts the shell before initializing the font or UI, sends a command, feeds
the actual PTY output to the app-owned model and checks the rendered view and
its text snapshot. It destroys the view, confirms the PTY cannot be closed
while its child is alive, sends another command, recreates the view on the
same model and checks that output. Finally it requests shell exit, observes
both child exit and PTY EOF, and closes the PTY. Each output marker is split
in the command so the echoed command cannot satisfy the output assertion.

The following enables only the optional Linux test. The shell refuses root,
so run the executable as an ordinary user. The font must be a readable TTF.
Use a disposable immutable source snapshot, not Aetheri's live worktree:

```sh
cmake -S examples/23_aetheri_terminal_view -B /tmp/terminal-pty \
  -DCMAKE_PREFIX_PATH=/path/to/installed-nexaui \
  -DAETHERI_SOURCE_DIR=/path/to/aetheri-snapshot \
  -DNEXAUI_AETHERI_PTY_EXPERIMENT=ON
cmake --build /tmp/terminal-pty
/tmp/terminal-pty/aetheri_terminal_pty_view_test \
  /tmp/terminal-pty/aetheri_original_shell /path/to/font.ttf
```

On Alpine/musl, add `-DNEXAUI_AETHERI_MUSL_COMPAT=ON` to supply test-local
`close_range` and `execveat` system-call wrappers. In this environment the
unmodified shell then exited 125 with `editor terminal modes drifted`.
Adding `-DNEXAUI_AETHERI_MUSL_NORMALIZE_TERMIOS=ON` separately wraps
`tcgetattr`, zeroing its output before the libc call; this test-only change
made the experiment pass. The suspected difference in unused control-character
slots is not established as the sole cause. Neither option changes Aetheri's
sources or demonstrates its unmodified glibc target. Both default to OFF.

On 5 November 2026, the normalized Alpine/WSL experiment passed as an
unprivileged user against installed 0.1.15 SDKs in static, shared and Clang
ASan/UBSan configurations. Replacing the shell with `/bin/false` caused the
test to fail as expected. These are headless, synthetic command checks: real
Aetheri process-owner integration, native input and IME, real clipboard-to-PTY
delivery, resize, terminal session policy, physical and human accessibility
acceptance remain open. The toolkit does not own the PTY or the shell.

### Independent Wayland clipboard to the original PTY

On NexaUI base `105b9f67dc73649cfe908e37e8d63765ae90c172`, the optional
`aetheri_terminal_native_paste_pty` test combines the existing native paste
review with the same immutable Aetheri shell, PTY and model. A separate
`wl-copy` process offers two controlled shell commands. The split output
marker cannot be produced by echoing the offered commands. Only after a fresh
review confirmation does this private test consumer write the confirmed bytes
to its own disposable shell; actual PTY output is parsed by the original model
and must contain the marker. For pending-receive cancellation, review
cancellation and rejected control bytes, a separate fixed probe command must
complete and the offered marker must not appear in the model. The shell must
then exit and reach EOF before PTY close.

On Alpine/WSL the musl syscall and termios test options described above were
enabled. Static and shared installed 0.1.15 builds and Clang ASan/UBSan with
leak detection passed as an unprivileged user under private Weston 14.0.2,
Xvfb and virtual keyboard input. The original no-PTY consumer also passed its
independent peer regression. In an isolated mutant build, suppressing the
confirmed PTY write made the driver fail on the missing shell-output marker.
Reproduce using the private Weston command shown
earlier in this document, but select `aetheri_terminal_native_paste_pty` and
append `/tmp/terminal-pty/aetheri_original_shell` as the final driver argument.
Both executables must come from a build with the PTY experiment enabled.

On NexaUI base `5c844d726f268b465846b5f8b291ab2c75e7e227`, the native PTY
paste consumer requires SDK 0.1.18 and copies the complete confirmed snapshot
into the bounded queue. It requests PTY write readiness while bytes remain,
reads output in the window event loop and checks the actual model for the
result. This fixture retains its 50 ms poll timeout. Its separate test cleanup and cancellation probe still use bounded
synchronous helpers. With `NEXAUI_TEST_PASTE_BACKPRESSURE=1`, the independent
clipboard driver additionally stops its owned shell and fills the PTY before
the accept case; it resumes the shell after confirmation. Static/shared and
ASan/UBSan cases passed on private Weston for accept, both cancellations and
rejected controls. An isolated mutant which did not drain the queue failed on
the missing shell output. The original no-PTY consumer also passed; the
0.1.16 SDK can still build that target and skips the new PTY consumer.

The shell resumes immediately after confirmation, so this does not prove a
particular number of blocked post-confirmation write attempts. This is not an
Aetheri target or glibc acceptance result; a real app still owns its session
policy, cancellation and handling of already-written prefixes. It is not a
native IME, real-session, physical-input or human accessibility test and does
not establish clipboard persistence after an owner exits.

### Terminal preedit adapter experiment

On NexaUI base `47d7436d8e91edcafd8b46c0484c1426f81a9812`, the optional
`NEXAUI_AETHERI_IME_EXPERIMENT` builds the original Aetheri composition model
from `87996e2f12128b172a80b325ad3b539df6aee527` (`composition.c` blob
`5a2c3586803c95b2fa761927a90f9dd9f74806c0`, header blob
`9906676a02cecfb6e150b99e1d47514f31f5878b`) and an example adapter.
It converts NexaUI's UTF-8 byte offsets into the model's Unicode scalar
positions, preserves hidden caret and representable selections, rejects invalid
boundaries and invalid drafts without mutating the prior model, and clears the
draft if the caller's focus/session epoch no longer matches. The original
composition model reserves selection length -1 as a sentinel, so the adapter
explicitly rejects an adjacent one-scalar reverse selection rather than
silently changing it.

Enable the option in the normal example build, then run
`aetheri_terminal_ime_test`. To include the controlled Wayland v3 test, also
set the two existing `NEXAUI_FIXTURE_*` paths described above and run
`aetheri_terminal_ime_wayland_test`. Static/shared and Clang ASan/UBSan
builds passed on Alpine/WSL. The controlled protocol test queried native
preedit state after a v3 notification and passed it into the original Aetheri
model. A separate mutant that treated byte offsets as scalar positions failed
the multibyte test.

This is an adapter experiment, not an IME-complete terminal. On the original
`47d7436` base, Wayland preedit notifications were not tagged with an
application epoch; the later guarded snapshot below addresses that case.
At that base the example did not draw the composition or forward commits to a
PTY; later experiments below address those pieces under controlled conditions.
Actual Aetheri, installed IME, scale, focus, physical input and human
accessibility acceptance remain open.

On NexaUI base `006cc663cf38111e8fcdebc18718ac79a64e89cb`, version 0.1.16
adds Wayland v1/v3 commit-boundary and filtered-input flags to existing scalar
events. The example buffers flagged, correctly targeted events until the END
marker and uses the original Aetheri whole-snapshot paste validation before
returning a borrowed complete commit. Incomplete, filtered, interrupted,
stale, over-limit and unsafe content produces no output. The owner must call
`terminal_ime_interrupt` on interleaved events or queue/transport failure,
and copy successful bytes into its own output queue. Unmarked input is not a
complete native commit. Static/shared and Clang ASan/UBSan installed SDKs
passed both the adapter and controlled Wayland v3 valid/filtered cases; the
existing v1/v3 tests also passed. This does not yet demonstrate actual IME
input being written to a PTY or solve native preedit focus fencing.

On NexaUI base `21e90dd44606ef4d332b42f2f08a0d31f373983a`, version 0.1.17
adds tagged Wayland preedit notifications and
`nx_window_get_ime_preedit_for`. The example checks the event and current
snapshot against the app-owned identity and epoch before changing the model.
In controlled v3 tests a revision change invalidated the old snapshot; an old
event could not adopt or clear a newer draft. A v1 controlled test also queried
a tagged snapshot and rejected a wrong revision. Static/shared and Clang
ASan/UBSan checks passed; a mutant that ignored the event identity failed the
stale-event check. The owner must still advance epochs and clear composition on
real focus loss; no installed IME, real Aetheri session or visual acceptance
is claimed.

### Controlled Wayland IME to an Aetheri PTY

On NexaUI base `032bfeccdd546b34d500bbd6f15b59eeb3e19f9c`, enable both
`NEXAUI_AETHERI_IME_EXPERIMENT` and `NEXAUI_AETHERI_PTY_EXPERIMENT`, as
well as the controlled fixture paths above. The additional
`aetheri_terminal_ime_pty_test` starts the original Aetheri shell and PTY
from revision `87996e2f12128b172a80b325ad3b539df6aee527` before starting
the Wayland server thread. A controlled v3 server emits an IME commit. The
adapter validates the entire event sequence and the test owner writes only a
complete accepted result to that PTY. It sends Enter separately and verifies
the command's split output marker through Aetheri's original terminal model.

Run the three scenarios as an unprivileged user:

```sh
/tmp/terminal-pty/aetheri_terminal_ime_pty_test /tmp/terminal-pty/aetheri_original_shell valid
/tmp/terminal-pty/aetheri_terminal_ime_pty_test /tmp/terminal-pty/aetheri_original_shell filtered
/tmp/terminal-pty/aetheri_terminal_ime_pty_test /tmp/terminal-pty/aetheri_original_shell stale
```

For filtered and stale commits, no bytes are sent from the commit. A separate
fixed shell probe must complete without the rejected marker; then the test
requests shell exit and observes both exit and PTY EOF. Static/shared and
Clang ASan/UBSan runs passed on Alpine/WSL with both explicitly enabled musl
test options described above. This is a deterministic controlled protocol
server, not an installed IME or real Aetheri compositor. PTY writes and waits
are bounded synchronous fixture helpers: an app must integrate a nonblocking
write queue, focus/foreground ownership and process shutdown itself. Preedit
painting, real focus changes, real-session and human acceptance remain open.
An isolated mutant that discarded the approved commit failed the marker check.

### Composition overlay on the terminal grid

On NexaUI base `99c2787b2f1d6855900265220d6f3b57bc4365f6`, the optional
IME example paints Aetheri's original composition viewport as a temporary
overlay using the existing cell-grid API. It follows the draft cursor, uses
the full row near the right edge, shows the composition selection and clipping
indicators, and exposes logical candidate-caret geometry to the owner. The
owner supplies the original width and combining-mark policies; missing mark
positioning, excessive marks, invalid state and scrolled-back views are
rejected rather than silently changing the model. The confirmed terminal text
and its read-only semantic snapshot remain unchanged. A view's optional
overlay callback is owner-thread, must not reenter or mutate the view/model,
and leaves unmodified existing callers on the original frame path.

`aetheri_terminal_ime_overlay_test /path/to/monospace.ttf` exercises wide and
combining cells, selection, visible/hidden caret, long draft scrolling, edge
fallback, stale owner guard and unchanged semantic text. It passed against
static/shared and Clang ASan/UBSan installed SDKs on Alpine/WSL at raster
scales 1, 1.5 and 2. An optional `NEXAUI_IME_OVERLAY_PPM=/tmp/frame.ppm`
writes the synthetic first frame; a mutant that omitted the overlay failed the
pixel and mark checks. The DejaVu Mono test font did not contain the selected
CJK glyph, so its glyph appeared as a missing-glyph box in the inspected
image. The later fallback experiment below addresses that specific missing
glyph; complete Unicode shaping remains open.

This is not an installed-IME or real-Aetheri visual pass. At this base candidate
placement still needed a native-window connection; the later controlled test
below exercises it. The original overlay did not
publish preedit text separately; the later accessibility experiment below adds
a scoped read-only snapshot, not full screenreader acceptance.

### Ordered fallback for terminal and preedit glyphs

On NexaUI base `6cf6baf7fc7ad2734ed6661f092e10b4d0b438e0`, the app can
provide optional borrowed `regular_fallback` and `bold_fallback` font
collections in the view descriptor. The primary face is tried first; for a
missing glyph, the existing ordered `nx_font_collection_resolve` selects the
first face with coverage. The terminal cells and IME overlay share that
selection. A supplied collection with no covering glyph returns
`NX_ERROR_UNSUPPORTED`; without a collection the legacy primary `.notdef`
behavior remains. Collections, fonts and their source bytes must outlive the
view. The app remains responsible for cell widths, mark positioning when the
base and mark use different fonts, font metrics, licensing and shaping.

On Alpine/WSL, a locally available DejaVu Sans Mono primary
(`b4a6c3e4faab8773f4ff761d56451646409f29abedd68f05d38c2df667d3c582`)
and the local Windows Noto Sans JP variable TTF
(`5113756f8a3b5d01b2211025e267c50121e3b36f465b7bbaf3cdaf4c3430bfd0`)
were passed to the test without copying or bundling either font. The primary
had no U+754C glyph; the fallback did. Static/shared and Clang ASan/UBSan runs
verified regular and bold cell selection, primary preference, explicit
no-coverage failure, the overlay's changed pixels, and legacy behavior. The
synthetic image was inspected and showed the glyph instead of the previous
box. A mutant that ignored fallback failed the face-selection check.

The result covers this glyph and test font pair only. It does not establish
general CJK shaping, cross-font mark placement, variable-font interpolation,
font discovery or redistribution rights. Real Aetheri visual and human
accessibility acceptance remain open. Supply both font paths to
`aetheri_terminal_ime_overlay_test` to run the fallback-specific case.

### Read-only accessible composition snapshot

On NexaUI base `1c915a65e3d10027500efbd2dc2d59c879eac3c3`, a host may opt in
to a separate read-only semantic object by supplying a nonzero
`semantic_identity` on `terminal_ime_overlay`. It must differ from the
terminal text identity and change when the terminal/composition session is
replaced. The object defaults to the name “Uncommitted composition”; the host
can provide a localized `semantic_name`. The entire bounded composition is
exposed, even when the visual viewport scrolls. Its selection and scalar
caret derive from the original model; a hidden caret remains absent. The
confirmed terminal snapshot is unchanged, and clearing composition removes
the separate object. Zero identity suppresses publication. The owner must
use that opt-out for sensitive or password input and enforce its own terminal
privacy policy; this example cannot infer whether terminal input is secret.

An independent libatspi client using a private D-Bus registry and the
headless accessibility host read the text, selected range and caret, saw
updates on the same object, observed its removal, and confirmed that native
text mutation is rejected. It also checked a hidden caret and unchanged
terminal content. Static/shared and Clang ASan/UBSan runs passed, as did the
existing terminal AT-SPI regression. A mutant publishing incorrect draft text
failed the independent client. Reproduce with the earlier AT-SPI command and
`NEXAUI_TERMINAL_AX_COMPOSITION=1`; add `NEXAUI_TERMINAL_AX_HIDDEN=1` for the
hidden-caret case. The example must be built with
`NEXAUI_AETHERI_IME_EXPERIMENT=ON`.

This is a queryable read-only snapshot in a simulated accessibility host. It
does not establish automatic announcement, real native focus, an installed IME,
production sensitive-input handling or human screenreader acceptance.

### Candidate position through the native window

On NexaUI base `d09c971ce6cc3b6fa19d434c4a8d4ff5c5e32fff`, the terminal
example can pass its computed composition caret to the existing
`nx_window_set_ime_rect` using `terminal_ime_overlay_publish_caret`. The
application supplies the grid's logical origin within the window and must
republish after cursor, draft, layout or scale changes. The helper translates
coordinates without multiplying by device scale and rejects a stale owner
guard or non-finite origin before making a native call.

With the controlled Wayland v3 fixture enabled, run
`aetheri_terminal_ime_wayland_test --candidate /path/to/monospace.ttf`. The
server observed the expected rounded rectangle after preedit, clearing the
draft, terminal cursor movement and a resize that switches the composition
to the whole-row viewport. It also checked a negative origin and no update
for a stale guard or invalid origin. The view was rendered with raster scales
1, 1.5 and 2, while the native rectangle remained in logical coordinates.
Static/shared and Clang ASan/UBSan runs passed; the existing v3 tests passed.
A mutant that omitted the grid origin failed the server's coordinate check.

This uses a deterministic in-process test compositor. It does not verify an
installed IME's candidate popup, native fractional-output scale transitions,
real Aetheri window/chrome geometry or human visual acceptance. The owner
still controls focus and native-window lifetime.

### Private Weston simple-IME to the original PTY

On NexaUI base `2f3b3af20af80918738b5dbeef13fb8b9d0fbb19`, the optional
`aetheri_terminal_native_ime_pty` test consumer combines an ordinary Wayland
window, terminal grid and preedit overlay, guarded IME adapter and the
original Aetheri shell/PTY from `87996e2f12128b172a80b325ad3b539df6aee527`.
The owner opens the shell before UI initialization, validates each complete
native commit before sending it to its own PTY, observes the resulting shell
output, and waits for shell exit and PTY EOF before closing. It processes all
queued focus changes before synchronizing the final IME state and avoids
republishing an unchanged candidate rectangle. At that base the test consumer
used bounded synchronous PTY writes, not a production backpressure solution.

With both IME and PTY experiment options enabled and the separately described
Alpine/musl test wrappers selected, the following owns a private Xvfb/Weston
seat. Supply the existing Weston executable, desktop-shell module and libexec
directory via `NEXAUI_WESTON`, `NEXAUI_WESTON_SHELL` and
`NEXAUI_WESTON_LIBEXEC`; `NEXAUI_SETXKBMAP` can name a private setxkbmap.
The work directory must be new. Run as an unprivileged user:

```sh
NEXAUI_TEST_FORCE_FOCUS=1 python3 examples/23_aetheri_terminal_view/run_native_ime_pty_session.py \
  /tmp/terminal-pty/aetheri_terminal_native_ime_pty /path/to/font.ttf \
  /tmp/terminal-pty/aetheri_original_shell /path/to/xdotool /tmp/new-ime-commit-run
NEXAUI_TEST_FOCUS_CANCEL=1 python3 examples/23_aetheri_terminal_view/run_native_ime_pty_session.py \
  /tmp/terminal-pty/aetheri_terminal_native_ime_pty /path/to/font.ttf \
  /tmp/terminal-pty/aetheri_original_shell /path/to/xdotool /tmp/new-ime-cancel-run
```

With Weston 14.0.2 `weston-simple-im`, virtual compose-key input produced an
apostrophe preedit, one `á` commit and one matching original PTY/model cell;
the native protocol trace included a candidate rectangle. The second run
moved focus to an independent X11 peer during preedit, restored it, and
observed only the new `z` commit and PTY cell, not the cancelled `á`.
Static/shared and Clang ASan/UBSan runs passed on Alpine/WSL. An early
focus-forcing run of the initial test consumer failed to adopt native preedit
after multiple queued focus changes; the consumer was changed to synchronize
the final queued focus state, then the explicit refocus case passed. This is
specific evidence for the test consumer, not a general claim that all
Wayland-v1 focus races are solved.
An isolated mutant suppressing the validated PTY write failed because the
expected shell output did not appear.

On NexaUI base `1b8020e694236d3a2151345ea07064040b9e87fa`, the test consumer
requires SDK 0.1.18 and copies its one validated commit into bounded storage.
It requests descriptor write readiness only while bytes remain, handles partial
writes and retries after interruption or `EAGAIN`. The static, shared and
ASan/UBSan private-Weston commit and focus-cancellation cases passed again.
Those short commits do not establish behavior under actual PTY backpressure;
the fixture's separate shutdown helper still uses bounded synchronous writes.

On NexaUI base `00649d379c10bc4565daaf63e6faad8db275e2f2`, this example uses
the bounded `pty_queue.h` helper. A controlled Wayland group-wait test fills a
real Linux PTY until it blocks, then a delayed raw-mode slave reader verifies
two queued items byte-for-byte after write readiness. The same test injects
short writes, `EINTR`, `EAGAIN` and an I/O failure, checks whole-item overflow
rejection and queue compaction, and clears pending bytes. Static, shared and
ASan/UBSan runs passed; an isolated queue-offset mutant failed the byte-order
check. The native private-Weston IME commit and focus-cancellation tests passed
again in all three configurations. The controlled PTY reader is not Aetheri's
shell, and the native IME test still commits only one short input.

The setup uses a real separate Weston input-method process and virtual input,
not a selected production IME, real Aetheri session, physical input/output or
human assistive/visual acceptance. The app still owns session focus, security,
output queue limits and cancellation, font selection and process policy.

### Original shell while its PTY is full

On NexaUI base `8c77c8bab7676ff7213d1b23f6ef3edf7cc9fb56`, build the
`aetheri_terminal_pty_backpressure_test` target with the PTY experiment and
an installed NexaUI 0.1.18 SDK. Run it as an unprivileged user under the
existing private-Weston runner, passing the separately built original shell:

```sh
xvfb-run -a python3 tests/run_weston_native.py --work-dir /tmp/new-pty-run -- \
  /tmp/terminal-pty/aetheri_terminal_pty_backpressure_test \
  /tmp/terminal-pty/aetheri_original_shell
```

Select the same private Weston environment as the IME test above. With the
Alpine/musl wrappers described earlier, static/shared and ASan/UBSan builds
passed: the fixture stopped its own shell, filled its PTY, queued two commands
and an exit, resumed the shell and observed both results exactly once in order,
plus process exit and EOF. A reversed-command negative control failed. The
fixture does not test IME or paste review under this same load; it is not a
production session-switch, physical-device or Aetheri-compositor acceptance.
