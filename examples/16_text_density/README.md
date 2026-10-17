# Logical text size and raster density

Build `nexaui_text_density` and run it with a local TrueType font and output path:

```powershell
build/examples/16_text_density/Debug/nexaui_text_density.exe C:/Windows/Fonts/segoeui.ttf out/text-density.ppm
```

The two columns share the same shaped 18.5-pixel layout and line breaks. The
left uses a 1× stamp sampled at 2×; the right rasterizes at 2× first. Both are
current API paths. This comparison does not represent an old implementation
supporting fractional text. Captions use direct density-aware text drawing.

Printed ink dimensions include physical raster padding and can differ slightly
in logical units; layout metrics remain unchanged. Font/layout/source bytes are
released before rendering. This is a software raster-density demonstration,
not an OS monitor/DPI-switch or physical-display acceptance test. No font is
redistributed. See [the text contract](../../docs/concepts/text.md).
