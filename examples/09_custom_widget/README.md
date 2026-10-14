# 08 Custom widget

Build `nexaui_custom_widget` and run it with an optional output prefix:

```sh
cmake --build build --config Debug --target nexaui_custom_widget
build/examples/09_custom_widget/Debug/nexaui_custom_widget custom_widget
```

On Windows, add `.exe` to the executable path. Single-config generators
usually place it directly in `build/examples/09_custom_widget/`.

The run writes four PPM frames named `<prefix>_0.ppm` through
`<prefix>_3.ppm`. It lays out a custom action button and progress bar,
registers their semantic nodes through `nx_widget_register`, and uses theme
tokens for drawing. A pointer click advances caller-owned progress to 25%;
Space activates the focused button and advances it to 50%. Every frame checks
that the semantic progress value matches the application state.
