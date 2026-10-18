# Context text density in editable widgets

Build `nexaui_widget_text_density`, then pass a locally installed TrueType font:

```sh
nexaui_widget_text_density FONT.ttf OUTPUT.ppm
```

The 1024×360 software image shows selected editor text, a masked password and a
numeric field at the same 18.5 logical ppem. Both panels are displayed at 2×;
the left context uses default 1× masks, the right opts into 2× with
`nx_context_set_text_raster_scale`. Titles explicitly use 2× in both contexts,
demonstrating that an explicit call ignores the default. Layout and input sizes
are identical. Fonts, layouts and original bytes are released before rendering;
the contexts retain their copied masks through consumption. No font is bundled.

This is an output-quality comparison, not an interactive window, physical DPI
transition or screenreader acceptance test. See the [text contract](../../docs/concepts/text.md).
