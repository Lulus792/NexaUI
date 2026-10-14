# NexaUI Studio showcase

On Windows, run `nexaui_showcase FONT_FILE` with a local TrueType font. The
native D3D12 window combines a scene tree, a live oscillator plot, a spring
gain property, telemetry, a toolbar and a status bar. Pause or resume with the
first toolbar button, advance one fixed 0.04-second model step with **Step**,
or return to the initial state with **Reset**. Selecting the Oscillator exposes
the editable gain; selecting Signal shows its current sample count. Use the
arrow keys on the focused gain property to change the spring. The visible
runtime accumulates elapsed time and caps catch-up to six steps per frame.
The plot uses a native crosshair cursor while the pointer is over its data area.
This is an
illustrative single-threaded simulation, not a physical-accuracy benchmark.

`nexaui_showcase --smoke FONT_FILE WIDE.ppm DETAIL.ppm COMPACT.ppm` opens a hidden native window,
submits D3D12 frames, sends Win32 mouse and keyboard messages for pause,
step, reset, gain and tree selection, checks the model and semantic state,
checks the plot and default cursor shapes, then resizes the window and checks
the compact layout. It writes headless
previews of the initial dashboard, the selected Signal inspector, and the
compact layout.

At widths below 1000 logical pixels, the tree and inspector are hidden so the
plot remains usable. Native macOS/Linux backends and assistive-technology
transport remain separate project work. The D3D12 driver is private to the
build tree.
