# 03: Layout

Run `nexaui_03_layout FONT_FILE OUTPUT_PREFIX` to render the same UI at two
sizes. It writes `OUTPUT_PREFIX-wide.ppm` (720×400) and
`OUTPUT_PREFIX-narrow.ppm` (360×620). A local TrueType font is required.

The program measures and arranges a reusable layout tree. At the wide size,
navigation and content sit side by side. At the narrow size, they stack. The
resulting rectangles position painted panels and a real button, and the
example checks containment, orientation, and semantic parentage before
writing each image. The output is headless and runs on every supported build
platform; no native window or GPU is required.
