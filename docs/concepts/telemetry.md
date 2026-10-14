# Telemetry and profiler panels

`nx_telemetry_panel` displays live scalar metrics; `nx_profiler_panel` displays
hierarchical timing rows. Both borrow arrays owned by the application and read
only rows that fit in the panel after `first_row`. Keep those arrays immutable
during each call. If they belong to a simulation snapshot, acquire the S01
view before building the frame and release it afterward. A producer can publish
new arrays without changing the UI's current view.

```c
nx_telemetry_panel_desc telemetry = {0};
telemetry.id = telemetry_panel_id;
telemetry.bounds = telemetry_bounds;
telemetry.name = (nx_utf8_span){"Telemetry", 9};
telemetry.metrics = snapshot->metrics;
telemetry.metric_count = snapshot->metric_count;
telemetry.first_row = scroll_row;
telemetry.row_height = 40;
telemetry.fonts = fonts;
telemetry.pixels_per_em = 12;
telemetry.glyph_cache_bytes = 2u * 1024u * 1024u;
nx_result result = nx_telemetry_panel(context, &telemetry);
```

Each metric has a stable nonzero ID, UTF-8 name and unit, a finite value and
finite increasing display range. Values outside the range remain exact in
text; their fill bar clamps to the range. A warning or critical metric must
also supply a localized status string. With fonts, that status appears as text
as well as a theme color. Its semantic node includes the status and a value
string with the numeric range. Units are limited to 64 UTF-8 bytes in this
initial formatted-value path. An empty panel reports “No telemetry.”

Profiler entries have stable IDs, names, nonnegative finite inclusive and
self times, a call count and a nesting depth. Self time cannot exceed inclusive
time. Each row paints an inclusive-time bar with a stronger self-time segment,
scaled against positive `frame_ms`. A compact total time is visible; the
semantic value retains total time, self time and call count; the semantic
description reports the visual nesting level. The profiler is a semantic list
with list-item children. An empty panel reports “No profiling
samples.”

The panel computes a visible row count from its bounds, header and effective
row height, capped at 4096 rows per call. It does not iterate through offscreen
records. `first_row` may equal the record count, producing no rows. The caller
owns scrolling, filtering and sort order; a virtual-list or scroll-area
controller can update `first_row`. `row_height` zero selects a theme-based
minimum; nonzero values below 16 logical pixels are rejected. With supplied
fonts, row and header heights grow for legible text. At larger text sizes,
labels and values stack, so fewer rows fit and scrolling remains necessary.
Fonts are optional; without them, semantic names and values remain available
but only the bars are painted. Supplied fonts use integer `pixels_per_em` in
[1,16384] and a positive glyph-cache budget.

Panels use semantic theme colors for dark, light and high-contrast appearance.
On any error they restore the frame's command count, clip depth and semantic
tree, allowing the caller to correct the data and retry with the same ID.
They do not mutate records or allocate panel-owned persistent state. Text
layout and glyph drawing may allocate temporary/frame-owned memory when fonts
are supplied. The current visual baseline uses no system font; a separate
Windows font preview checks 12 and 24 pixel text. Native screen-reader and GPU
output still require their platform-specific validation.
