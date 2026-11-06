# Operation status without a fabricated percentage

This external C17 consumer uses the installed NexaUI 0.1.23 package. It renders
three deliberately distinct observations to a PPM image:

- A measured completion fraction uses `nx_progress` and exposes a percentage.
- An observed active operation without a completion estimate uses `nx_activity`.
  Its copied accessible name/description and read-only busy status have no
  numeric value or actions. The band position is visual phase, never progress.
- An unknown result uses an ordinary read-only status without a busy flag.
  Unknown does not establish that work is still running or that it completed.

The example supplies fixed observations; it does not install software, change
devices, authorize a request or contact an operation service. Aetheri owns the
operation identity, exact target, admission, correlation, reconciliation and
commands. A reopened view must query that owner rather than submit the command
again. The example is a presentation seam, not a completed installer or recovery
workflow.

```sh
cmake -S examples/26_operation_status -B out/operation-status \
  -DCMAKE_PREFIX_PATH=/absolute/path/to/installed/nexaui
cmake --build out/operation-status
out/operation-status/operation_status /absolute/path/to/font.ttf status.ppm
```

Supply a local licensed font. The example uses the software rasterizer, needs no
display, GPU, network or service, and keeps font/layout memory alive until render
completion. It verifies the busy and unknown semantic states before writing the
image; a failed check returns a nonzero exit code.

The activity control owns no timer and requests no frames. A host may advance
phase only while active and visible, using its existing deadline/wakeup loop.
For reduced motion keep phase fixed, as this example does. On leaving the busy
state, stop its animation deadline and replace its semantic node with the
observed result. Reuse a stable identity when it still describes the same status;
phase changes alone should not generate assistive value announcements.

The scoped checks cover software presentation and core semantics. The separate
Linux AT-SPI fixture also checks busy/read-only state, description, absence of a
numeric value/action and replacement with an unknown status at the same identity
through an independent libatspi client on Aetheri's test compositor. It also
requires no object events on a visual-only phase update and one busy-clear event
on the transition to unknown. Human
screenreader announcements, other platforms, localization, resizing/scaling and
real offline installation/recovery integration remain separate acceptance gates.
Aetheri can adopt this control independently without changing its models.
