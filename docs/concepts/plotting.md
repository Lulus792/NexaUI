# Real-time line plot

`nx_plot` draws one time-series view inside a named, focusable widget. It uses
the S03 decimator to inspect only the inclusive visible `x` range, then draws
the first/last and vertical extrema from each occupied logical pixel column
in source order. The application supplies finite increasing x and y ranges;
the widget does not pan or autoscale them. Data stays borrowed and must remain
stable until the call returns. For simulation data, keep the S01 snapshot view
open through the call.

```c
nx_series_envelope scratch[800]; /* Reuse between frames. */
nx_plot_state plot_state = {0};   /* Keep between frames. */

nx_plot_desc plot = {0};
plot.id = plot_id;
plot.bounds = plot_bounds;
plot.name = (nx_utf8_span){"Velocity", 8};
plot.series = series_view;
plot.x_min = visible_start;
plot.x_max = visible_end;
plot.y_min = -10.0;
plot.y_max = 10.0;
plot.scratch = scratch;
plot.scratch_capacity = 800;
nx_result result = nx_plot(context, &plot, &plot_state);
```

The buffer must hold at least one envelope per logical pixel in the inner
plot area, up to a cap of 16,384. The inset leaves space for axes and optional
labels. Callers can size a reusable buffer to the full widget width, which is
an upper bound for the inner plot area. An insufficient buffer returns
`NX_ERROR_LIMIT` before registering or painting the widget. When the data view
is empty, the axes and grid still render.

The widget paints a surface, border, five horizontal and vertical grid lines,
two axes and a theme-colored trace. A supplied font collection plus integer
`pixels_per_em` in [1,16384] and positive `glyph_cache_bytes` adds numeric tick labels; without fonts,
the lines and ticks remain. Colors come from the active NexaUI theme,
including dark, light and high-contrast presets. The trace and cursor are
clipped to the inner area. Tick labels use `%.4g` formatting and currently
follow the process numeric locale; applications needing exact localized units
or label precision will need a later axis-label formatter.

The plot is a named custom semantic node with a short description. Pointer
hover or capture sets a vertical cursor in the plot area. When focused,
Left/Right moves it by one logical plot pixel; the semantic value reports its
x coordinate in the same frame. Disabled plots keep their cursor position and
use disabled theme colors. The caller retains `nx_plot_state` across frames.
Frame errors leave that state unchanged and roll back commands, hit region and
semantic node so a retry with the same ID can succeed.

The plot currently draws one series, linear x/y scales and no legend or
selection. Rendering a full million-point viewport still requires a linear
scan through S03. The widget does not own simulation data, cache samples or
schedule redraws; the application controls update cadence and visible ranges.
