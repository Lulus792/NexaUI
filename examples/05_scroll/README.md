# Virtual scrolling

Build `nexaui_scroll_example` and run it with an optional output path:

```sh
cmake --build build --config Debug --target nexaui_scroll_example
build/examples/05_scroll/Debug/nexaui_scroll_example scroll.ppm
```

The example measures 10,000 rows with stable keys and variable heights. It
begins a scroll area, visits only rows inside the viewport plus two rows of
overscan, and builds a button for each visited row. It then routes a wheel
event through a second frame and writes that frame as a 480 × 320 PPM image.
The console reports the visited count and final offset. The visible bars stand
in for application row content, so this example does not require a font file.
On Windows, use the `.exe` suffix; single-config CMake generators usually put
the executable directly in `build/examples/05_scroll/`.

`nx_virtual_list_visit` checks the 32-row budget before it calls the builder.
The row callback receives viewport-relative Y coordinates and a stable key;
the scroll area supplies clipping and a draggable scrollbar thumb. For an
application with data changes, call `nx_virtual_list_rebuild` to preserve the
visible keyed row before the next frame.
