# Simulation dashboard

Run `nexaui_simulation_dashboard OUTPUT_PREFIX [FONT_FILE]`. It writes
`OUTPUT_PREFIX-early.ppm` and `OUTPUT_PREFIX-late.ppm`. Supply a local
TrueType font to show axis numbers, metric names and values; without
one, the same controls render bars and curves. The command runs headlessly.

The example integrates a damped unit-mass oscillator with a fixed 0.04-second
step using semi-implicit Euler. It is a deterministic UI sample, not a
physical-accuracy benchmark. A worker receives one bounded command, publishes
16 immutable snapshots and posts a completion. The UI keeps an older snapshot
pinned while the worker runs, renders it, releases it, then acquires and
renders the latest snapshot. It checks each snapshot's sequence, ownership
cleanup, the semantic tree and that the two rendered frames differ.

The plot reads the snapshot's 128 position samples through `nx_series_view`.
Telemetry shows position, speed, energy and integration-step count; the
profiler panel shows **illustrative**, fixed timing values to demonstrate the
layout, not measured performance. The font variant is exercised by CTest
when a suitable local font is found.

All snapshot data is caller-owned and read-only during UI composition. The
application releases each view after rendering. This sample uses a headless
renderer; a native, continuously updated dashboard and cross-platform runtime
checks remain separate work.
