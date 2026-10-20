# Portable native window management — D01

SDK implementation and native automated verification delivered, 2026-10-21. This is the
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
| Windows | Overlapped styles retained; full client NCCALCSIZE/work-area clipping; DPI-based edge hit testing; native nonclient move/resize; ShowWindow state actions; client-mode system menu route | Corrective CI static/shared116/116 each; local final DLL checks2/2 and MSVC-ASan3/3 | Physical drag/Snap, real mixed DPI/input and composed accessible controls |
| Wayland | xdg-toplevel configure state/caps; own seat/press serial; decoration negotiation/refusal; normal-size restoration and phased fullscreen exit | Corrective CI Linux static/shared121/121 plus both full sanitizer suites121/121; no skips | Physical compositor/application acceptance |
| X11 | ICCCM selection/WM_STATE, EWMH supported/state/active properties and messages; Motif decoration hint | Instrumented wire fixture and real Openbox transitions; startup-barrier fixture passes ten fresh runs; corrective CI Linux static/shared121/121 | Physical WM/application acceptance; hints may be declined |
| AppKit | Native titled full-size content, hidden standard buttons; retained own NSEvent drag; native frame resize with limits/Escape; zoom/miniaturize/restore/state | Corrective CI39507858713: static and shared each96/96, including native management assertions | Physical drag, monitors, input, VoiceOver |

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

## Corrective native geometry checks after 63f31ee

The complete local instrumented rerun passes 121/121 without skips, 178.91s
(`out/d01-linux-full-final.xml`; native no-skip gate includes 11 cases). That
baseline originally checked restored state and positive dimensions separately;
it did not prove returning to the previous normal dimensions. It must not be
used as the stronger geometry acceptance below.

The additional check found two independent issues:

1. X11 state properties can change before the ConfigureNotify has updated the
   cached geometry. The native test now waits independently for both observed
   state and dimensions, against monotonic deadlines. The SDK does not promise
   an atomic snapshot across those two APIs. The shared-link diagnostic test
   also needs its own direct X11 link dependency; static linkage had supplied it
   transitively. Initial CI's Ubuntu shared build caught that declaration error.
2. A normal Wayland configure with zero width/height needs the previously normal
   dimensions, rather than the maximized dimensions. A new real server-wire
   counterexample fails before the cache correction
   (`out/d01-wayland-restore-before.xml`). Maximize, fullscreen, combined
   fullscreen/maximized and a later user normal-size change are now covered.
   Actual Weston tracing also identifies the original unconditional
   unset_fullscreen after unset_maximized as a separate issue: it can replace
   the normal-size configure with the still-committed maximized dimensions.
   The [Weston 14.0.2 shell implementation](https://github.com/wayland-mirror/weston/blob/14.0.2/desktop-shell/shell.c#L2290)
   explicitly uses current or pending maximized state for that size. A single-
   maximize control still fails, ruling out repeated maximize as the cause.
   Fullscreen exit is now sent only for observed fullscreen, and unmaximizing
   an underlying state waits for the exit configure. Explicit compositor size
   proposals continue to be honored.

The failed traces and controls are retained:
`out/d01-wayland-restore-trace.log`, `out/d01-wayland-single-max-control.log`
and their private compositor directories. No compositor/OS setting was changed.
Final focused instrumented and shared suites each pass 3/3
(`out/d01-management-final-asan.xml`, `out/d01-management-final-shared.xml`).
Private Weston plus legacy window regression, native dimensions and protocol
fixture each pass 3/3 in static ASan/UBSan/LSan and shared builds
(`out/d01-wayland-phased-restore.xml`, `out/d01-wayland-shared-restore.xml`).
The native result now proves maximize 1600x1200 and restore 320x200.

[Initial CI 39506513206](https://github.com/Lulus792/NexaUI/actions/runs/39506513206)
contains separately successful macOS static/shared jobs (96/96 each) and Ubuntu
static. It has an Ubuntu shared build failure from the diagnostic link edge;
do not call the initial workflow wholly green. Logs are preserved under
`out/d01-window-management-v1/ci-39506513206`. The corrective revision requires
its own complete CI result, recorded below.

Corrective [CI39507858713](https://github.com/Lulus792/NexaUI/actions/runs/39507858713)
on66d596d has successful Linux static/shared121/121 and macOS static/shared96/96
jobs. Four downloaded artifact ZIP digests match GitHub's metadata. Both Linux
JUnit files contain121 unique cases, no failed/skipped results and all11 required
native gate cases. Their native Wayland output confirms1600x1200→320x200.
Both macOS raw LastTest logs confirm the actual AppKit client-chrome/resize/
Escape/zoom/miniaturize/restore assertions ran successfully. Both Windows jobs
also pass116/116, including the new native management case. The required
sanitizer job is successful: paired private Mesa cleanup, both complete121/121
suites, both11-case no-skip gates, module attribution and driver-order controls.
All seven required jobs succeed. Optional Windows-ASan/fuzz CI jobs are skipped
by workflow configuration; local Windows-ASan evidence above is separate.
Sanitizer artifact12305012613 is downloaded and its SHA256 matches GitHub:
`5ffcafda96ffbf84c24d747ba4e0d9ada23d9ea4e1a6a7e6cc0bd8eae90c4660`.
Both raw JUnit files independently contain121 unique executed cases and all11
mandatory native cases, without failures/skips. The eight corrected dependency
controls report no leaks with verified layer state; all six module-attribution
runs and both discovery-order groups exit0. Sources, original negative controls
and full archive remain retained. These are SDK/software results, not a WSLg
live-session, physical input, monitor or full Design-A acceptance.
Machine-readable hashes and result scopes are in
[window-management-local.json](window-management-local.json).

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
