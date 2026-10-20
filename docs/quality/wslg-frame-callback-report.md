# WSLg frame callback boundary

2026-10-20, source `1cea7b9`. This follows the frozen
[Wayland FIFO waits](vulkan-canvas-response-report.md) and the separate
[private Weston acceptance](wayland-private-compositor-report.md).
The new evidence narrows the symptom; it does **not** close WSLg liveness.

## One bounded native trace

The original clear/present test was built with the existing Clang22.1.3
ASan/UBSan configuration (`ninja: nothing to do`) and launched once against
WSLg1.0.73.2, Alpine3.24.2, the installed Mesa26.1.6 Lavapipe and native
Khronos/synchronization validation. `WAYLAND_DEBUG=client` adds a new protocol
observation; no present mode, rendering behavior, assertion or OS setting changes.

The diagnostic observer has a15-second bound, distinct from CTest's90-second
test deadline. At that bound GDB captures the owned process's main stack, detaches,
and the controller terminates/waits only that process group. Its SIGTERM exit is
not a normal teardown/LSan success or a complete executed test result.

The captured first clear/present has `draw_count=0`. Its stack waits in
`wl_display_dispatch_queue` inside Lavapipe, called by `vkQueuePresentKHR`
at `renderer.c:972`, from `test_renderer`'s first `show_frame` at line197.
The trace records these consecutive requests on the Mesa queue:

```text
wl_surface#11.attach(wl_buffer#36, 0, 0)
wl_surface#11.damage_buffer(0, 0, 2147483647, 2147483647)
wl_surface#11.frame(new id wl_callback#45)
wl_surface#11.commit()
```

No `wl_callback#45.done` is received before the diagnostic ends. Further events
are received and dispatched: keyboard enter, a second toplevel configure,
`xdg_surface#13.configure(348)` and its ack, ID deletion and release/destruction
of the earlier framework background buffer18. That release is **not** release
of Mesa's buffer36 or completion of callback45. The trace does not prove
physical visibility or identify a Windows/RDP scheduling cause.

## Independent shared-memory control

[`tools/wayland_frame_control.c`](../../tools/wayland_frame_control.c) links only
libwayland and generated xdg-shell protocol code, without NexaUI or Vulkan.
It creates a normal128×96 toplevel, acknowledges configure, attaches actual
XRGB8888 shared memory, requests one frame notification and services the socket
with finite polls for five seconds. It never modifies its submitted buffer.
Callback and object cleanup remain enabled on the failure path.

The same instrumented executable is used in a fixed WSLg→private-Weston order:

| Environment | Actual result |
| --- | --- |
| WSLg1.0.73.2 | Configured twice, zero frame notifications in5.022s, exit2 after normal cleanup |
| Private Weston14.0.2, X11 backend/Pixman/private Xvfb | Configured twice, one frame notification in0.046730s, exit0; owned compositor stopped |

Both have zero observed buffer releases in their short observation. A compositor
may keep the current buffer, so that count is not a failing release oracle.
No sanitizer error is observed in either completed SHM control. There is no
pixel-readback oracle in this protocol-only client and no physical/display
or human-input acceptance claim.

Two private-compositor setup failures are retained separately: the first launcher
omitted the existing private `libexec_weston` search directory; the second omitted
the existing absolute X11-backend module map. Neither launched the SHM client.
Restoring those known private-tool settings permits its single actual private
run. The WSLg control is not repeated to obtain a pass; source and binary remain
unchanged throughout.

## Conclusion and next discriminating check

The observed missing frame notification also occurs without Vulkan, the
renderer, its canvas cache, shaders or NexaUI event handling. Therefore none of
those components is necessary for this particular symptom. This is narrower
than proving a WSLg defect or its lower-level cause.

The official [Wayland frame contract](https://wayland.freedesktop.org/docs/html/apa.html#protocol-spec-wl_surface-request-frame)
allows withholding callbacks for invisible/fully obscured surfaces. The installed
Wayland1.25 protocol XML is retained by hash as a versioned local reference.
An ordinary toplevel/configure and keyboard-enter event do not independently
establish physical visibility. The next useful measurement is to correlate an
owned SHM surface with its Windows host window's actual mapping, minimize/cloak
state, bounds and output/occlusion or RDP presentation records. That observation
was **not** performed here. Do not substitute a different present mode, restart
the shared compositor or change display/OS settings to label this fixed.

The [receipt](wslg-frame-callback-local.json) records binary/source/library and
raw-log hashes, the exact controls, failed setup boundaries and stack excerpts.
Frozen raw material lives under `out/wslg-fifo-protocol-20261020`.
R01's lower-layer profiling permission gap, mixed-DPI transitions, real keyboard/
IME use, human assistive tasks and overall Design-A acceptance remain open.

## Reproduce the independent control

With an existing Linux Wayland development toolchain and desktop (or the existing
private Weston fixture), build in a fresh directory:

```sh
work=$(mktemp -d)
protocol=/usr/share/wayland-protocols/stable/xdg-shell/xdg-shell.xml
wayland-scanner client-header "$protocol" "$work/xdg-shell-client-protocol.h"
wayland-scanner private-code "$protocol" "$work/xdg-shell-protocol.c"
clang -std=c17 -O1 -g -Wall -Wextra -Wpedantic \
  -fsanitize=address,undefined -fno-omit-frame-pointer -I"$work" \
  tools/wayland_frame_control.c "$work/xdg-shell-protocol.c" \
  -lwayland-client -o "$work/frame-control"
ASAN_OPTIONS=halt_on_error=1:detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 \
  WAYLAND_DEBUG=client timeout 15 "$work/frame-control"
```

The actual local build used the same existing CMake-generated protocol source/
header, identified in the receipt. Exit0 means a notification was observed;
exit2 means the five-second observation ended without it or a socket operation
failed. Inspect the protocol/error logs to distinguish those cases. An outer
timeout or setup failure cannot count as a passing callback result.
