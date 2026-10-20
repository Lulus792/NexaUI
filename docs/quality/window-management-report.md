# Portable native window management — D01

Implementation and automated verification in progress, 2026-10-21. This is the
NexaCloud handoff's window-control D01, separate from the roadmap's historical
D3D12-device D01 identifier. Full native/physical/application acceptance remains
open. No NexaCloud source or tested dependency pin is changed by this work.

## Implemented boundary

The public C17 API adds opt-in `NX_WINDOW_CLIENT_CHROME` and
`NX_WINDOW_STATE_EVENTS`, a separate status struct (observed bits, known bits,
capabilities, requested client mode), minimize/maximize/restore, move and eight
resize directions. Existing descriptor/info/event layouts and prior event enum
values are preserved. The new event payload aliases existing union storage;
core input validation rejects unknown state bits and routing leaves input state
unchanged. Owner-thread and closing-window guards apply. Failed status queries
zero their output. Requests do not fabricate observed state.

| Backend | Implementation | Local evidence so far | Remaining acceptance |
| --- | --- | --- | --- |
| Windows | Overlapped styles retained; full client NCCALCSIZE/work-area clipping; DPI-based edge hit testing; native nonclient move/resize; ShowWindow state actions; client-mode system menu route | Debug static and DLL suites each 116/116; final DLL checks 2/2 and MSVC-ASan 3/3 | Physical drag/Snap, real mixed DPI/input and composed accessible controls |
| Wayland | xdg-toplevel configure state/caps; own seat/press serial; xdg-decoration negotiation/refusal; no invented minimized state/unminimize | Real isolated server-wire fixture passes ASan/UBSan/LSan including serial zero, eight edges, release/leave/seat-removal, refusal cleanup and default event regression | Real private Weston state suite and regular CI; physical compositor/application acceptance |
| X11 | ICCCM selection/WM_STATE, EWMH supported/state/active properties and messages; Motif decoration hint | Instrumented wire fixture and real Openbox transitions; startup-barrier fixture passes ten fresh runs | Regular CI and full suite after fixture correction; physical WM/application acceptance; hints may be declined |
| AppKit | Native titled full-size content, hidden standard buttons; retained own NSEvent drag; native frame resize with limits/Escape; zoom/miniaturize/restore/state | Source and native test implemented; no macOS execution claimed yet | Native macOS CI and physical drag, monitors, input, VoiceOver |

The Windows gesture fixture intercepts `SC_MOVE`/`SC_SIZE` only at native loop
entry and verifies all nine requests/cancellation. It does not simulate physical
dragging or establish Snap. Actual native maximize/minimize/restore and work-area
geometry are checked separately. Client Alt+Space reaches `SC_KEYMENU` without
text insertion; legacy flags preserve the previous SYSCHAR route. A custom
maximize control receives no Windows 11 hover Snap-layout flyout automatically.

## Executed evidence

- Windows Debug static baseline: `out/d01-windows-debug.xml`, 116/116, no skips,
  308.81 seconds. Subsequent small closing/status/SYSCHAR regression checks are
  rerun separately; do not interpret the baseline as those later checks.
- Linux focused instrumented run: `out/d01-linux-focused.xml`, 3/3, no skips
  (`nexaui_wayland_management`, `nexaui_x11_management`, `nexaui_input`). Actual
  protocol resources, messages, native properties and allocator cleanup are
  checked, not a success-only adapter stub.
- Windows Debug DLL baseline: `out/d01-windows-shared.xml`, 116/116, no skips,
  318.67 seconds. Final guard/SYSCHAR/DPI-aware assertions also pass DLL 2/2
  (`out/d01-windows-shared-focused-final.xml`) and MSVC-ASan 3/3
  (`out/d01-windows-asan-final.xml`). DPI awareness here changes only the test
  thread and is restored; no display/scale/system setting is changed.
- Expanded Linux focused run: `out/d01-linux-focused-final.xml`, 4/4, including
  actual Openbox maximize 1024x768, restore 320x200 and observable iconify/restore.
- First full instrumented private Weston run: `out/d01-linux-full.xml`,
  120/121; only the new Openbox fixture fails. Wayland's native WM transition
  case passes. The original result/logs are retained under `out/d01-weston-native`.
  The failure branch's missing test cleanup produced a 64-byte wrapper leak;
  failure cleanup is now explicit. This is not a Vulkan/Mesa leak correction.
- Openbox startup diagnosis (`out/d01-linux-openbox-native-diagnostic.xml`):
  native window map_state=0, parent=root, override_redirect=0, status known=0,
  root capabilities=127; Openbox debug log shows no adoption of that window.
  Waiting after mapping for a state property did not repair this startup case.
  The fixture now starts its child through Openbox's `--startup`, after
  `OB_STATE_RUNNING` in the [upstream startup sequence](https://github.com/danakj/openbox/blob/master/openbox/openbox.c).
  This changes test process ordering, not a production backend. The explicit
  X11 wire test separately proves that root capabilities can exist without an
  observed per-window state. Ten fresh corrected native runs pass, no sanitizer
  report (`out/d01-linux-openbox-startup-barrier.xml`).
- Full native platform runs and CI results will be appended after execution.
  The Wayland no-skip gate now additionally requires both management fixtures;
  historical 117-test/7-Wayland reports remain historical evidence.

## Limits and integration handoff

The API supplies native window control; the application composes its title
controls, accessible names, shortcuts and orderly worker/renderer shutdown.
See the [contract and integration guide](../concepts/window-management.md).
No SDK test substitutes for NexaCloud's tested pin adoption or full Design-A
visual/physical acceptance. X11 decoration remains a hint. Wayland restore does
not unminimize, and Windows fullscreen observation is unknown. macOS execution
must be read independently from Windows/Linux results.

R01 remains open at the documented profiling-rights boundary. The WSLg live
frame-callback/host-visibility findings remain open; these private compositor
tests do not repair or accept that session. Existing Vulkan/Mesa evidence is
preserved; the integrated private dependency patch is not reimplemented here.
