# Real-time plot

Run `nexaui_realtime_plot OUTPUT_PREFIX`. The headless example advances a
sample stream for 40 frames and writes `OUTPUT_PREFIX-early.ppm`,
`OUTPUT_PREFIX-late.ppm` and `OUTPUT_PREFIX-cursor.ppm`.

A caller-owned 2,048-point ring buffer keeps samples in time order and exposes
them through `nx_series_view`. Each frame appends 60 points, moves the plot's
300-sample time window, and reuses a caller-owned envelope buffer. The example
checks the oldest and newest sample after wraparound and verifies that the
early and late rendered frames differ. It then moves the pointer into the
plot, focuses it, presses Right, and checks both the advanced cursor and its
semantic value before saving the cursor frame.

The signal is deterministic, so the command also serves as a bounded CTest
smoke run. The plots are rendered with the headless backend; a live native
dashboard, multi-series legends and platform screenreader bridges remain
separate work.
