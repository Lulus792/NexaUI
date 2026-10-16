# Text editing

Build the example and pass a local TrueType font plus an output image path:

```sh
cmake --build build --config Debug --target nexaui_text_editing_example
build/examples/07_text_editing/Debug/nexaui_text_editing_example C:/Windows/Fonts/arial.ttf editing.ppm
```

On Windows, add `.exe` to the executable path. Single-config generators
usually place it directly in `build/examples/07_text_editing/`. A font is
required because the text widgets shape and rasterize the edited text.

The sample builds a light-theme form with a validated single-line project
name and a multiline notes field. It focuses the project name, selects all,
types `NexaUI`, and submits with Enter. It then appends a third notes line,
types `!`, undoes it and redoes it. The program verifies the text and semantic
values after those events. It then selects the notes through a queued
press/move/release sequence, checks the resulting source range and writes the
final 680 × 420 PPM frame with that selection visible.

Optional arguments choose `light`, `dark` or `contrast` and a text multiplier
of `1`, `1.5` or `2`:

```sh
nexaui_text_editing_example C:/Windows/Fonts/arial.ttf selection.ppm contrast 2
```

These are logical text-size variants; they do not simulate a physical display
scale or a native pointer device. The common selection/capture tests are in
`tests/test_text_pointer.c`.

When an available font is found during CMake configuration, the example is
also registered as a CTest. Set `NEXAUI_EXAMPLE_FONT` to a local TrueType file
to select one explicitly. The filename is passed to the C runtime, so use a
path representable in its current filename encoding.
