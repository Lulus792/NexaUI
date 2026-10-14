# Theme switching

Build the example and pass a TrueType font plus an output prefix:

```sh
cmake --build build --config Debug --target nexaui_themes_example
build/examples/08_themes/Debug/nexaui_themes_example C:/Windows/Fonts/arial.ttf themes
```

On Windows, add `.exe` to the executable path. Single-config generators
usually place the program directly in `build/examples/08_themes/`. The run
writes `themes_dark.ppm`, `themes_light.ppm` and
`themes_high_contrast.ppm`, each 640 × 430 pixels.

The sample starts in Dark, uses Right and End on the theme selector to choose
Light and High Contrast, then clicks Dark. It applies each selected preset
with `nx_context_set_theme` **between** frames. The same button, checkbox,
disabled button, semantic color swatches and scoped accent override are
repainted in each saved frame. The program checks the selected radio's
semantic state and that the scoped override restores the base accent.

`NEXAUI_EXAMPLE_FONT` can select a TrueType file for CTest. When a local font
is found during CMake configuration, this example is registered as a test.
