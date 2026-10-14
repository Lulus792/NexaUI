# Getting started

Build with CMake and run the tests. Linux additionally needs Vulkan headers
and the system loader (`libvulkan-dev` on Debian/Ubuntu) for the default
private backend target. It can be disabled with `-DNEXAUI_BUILD_VULKAN=OFF`.
See the [Vulkan bootstrap guide](concepts/vulkan.md) for driver and validation
layer setup.

```sh
cmake -S . -B build
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure --timeout 20
```

The `nexaui_headless` and `nexaui_layout_headless` examples construct command streams and write PPM images. The latter measures and arranges a nested layout first. On Windows, `nexaui_00_hello_window` opens a native window, reports its logical size and scale, and prints normalized key, text and pointer events. `--smoke` runs the bounded hidden-window check. See [the first progressive example](../examples/00_hello_window/README.md). The older `nexaui_hello_window` target remains available.

`nexaui_01_hello_ui [FONT_FILE]` renders an accessible greeting from a UI context into a native D3D12 window. A TrueType font adds visible heading and subtitle text. `--smoke [FONT_FILE [OUTPUT.ppm]]` runs the hidden integration check and can save a preview. See [the hello UI example](../examples/01_hello_ui/README.md).

`nexaui_02_buttons_and_input FONT_FILE` adds a native button, checkbox, count display and keyboard focus feedback. Click either control, or focus the button with Tab and press Space. `--smoke FONT_FILE OUTPUT.ppm` exercises Win32 mouse and keyboard input in a hidden window, checks the semantic states, and saves a preview. See [the buttons and input example](../examples/02_buttons_and_input/README.md).

`nexaui_03_layout FONT_FILE OUTPUT_PREFIX` measures and arranges the same UI at wide and narrow sizes, then writes two PPM frames. It checks panel containment, responsive orientation, and button parentage. See [the layout example](../examples/03_layout/README.md).

`nexaui_04_forms FONT_FILE OUTPUT_PREFIX` composes validated text fields, a confirmation checkbox, a disabled-until-ready button, and a semantic status message. It checks invalid and submitted states and writes a PPM image of each. See [the forms example](../examples/04_forms/README.md).

On Windows, `nexaui_showcase FONT_FILE` opens the native NexaUI Studio dashboard with a live plot, scene tree, gain inspector, telemetry, and simulation controls. `--smoke FONT_FILE WIDE.ppm DETAIL.ppm COMPACT.ppm` checks Win32 mouse and keyboard actions, D3D12 submission, inspector selection, and responsive layout in a hidden window. See [the showcase](../examples/showcase/README.md).

`nexaui_interactive_region` demonstrates a custom interactive rectangle in three headless frames: idle, held and clicked. It uses `nx_id` for stable identity and `nx_interact_region` for input routing. The [focus guide](concepts/focus.md) explains the one-frame registration model.

`nexaui_scroll_example OUTPUT.ppm` renders a 10,000-row measured list after a wheel event. Only visible rows plus two rows of overscan become button widgets; the output also shows the scroll area's scrollbar. See [the example](../examples/05_scroll/README.md) and [the virtualization contract](concepts/layout.md#scroll-and-virtualization).

`nexaui_tables_example OUTPUT.ppm [FONT_FILE]` renders a semantic table after two clicks on its Score header. It keeps equal-score rows in source order and prints the final order. A TrueType font path enables visible text; the font-free mode draws data bars. See [the table example](../examples/06_tables/README.md).

`nexaui_text_editing_example FONT_FILE OUTPUT.ppm` renders a validated single-line field and a multiline notes field. It exercises selection, text input, Enter, undo and redo before writing the final frame. See [the text-editing example](../examples/07_text_editing/README.md).

`nexaui_themes_example FONT_FILE OUTPUT_PREFIX` switches a theme selector through Dark, Light and High Contrast, checks the selected semantic radio state, and writes one PPM image per preset. See [the theme example](../examples/08_themes/README.md).

`nexaui_custom_widget [OUTPUT_PREFIX]` composes a button and progress display from layout, interaction, semantic nodes and draw commands. It writes four PPM frames after pointer and keyboard activation. See [the custom-widget example](../examples/09_custom_widget/README.md).

`nexaui_docking_editor FONT_FILE OUTPUT.ppm SNAPSHOT.nxdock` composes a two-pane docking layout. It moves an Assets tab, drags the splitter, writes a rendered frame and verifies a binary docking snapshot roundtrip. See [the docking example](../examples/10_docking_editor/README.md).

On Windows, `nexaui_multi_window_example` opens two independently rendered native windows. Click or type in either one, resize them, and close them separately. `nexaui_multi_window_example --smoke` runs the hidden integration check. See [the multi-window example](../examples/11_multi_window/README.md).

`nexaui_accessibility_example` checks keyboard traversal and activation, then prints the resulting semantic tree with roles, names, focus, checked and disabled states. It runs headlessly on supported build platforms. See [the accessibility example](../examples/12_accessibility/README.md).

`nexaui_realtime_plot OUTPUT_PREFIX` feeds a bounded ring buffer through a moving plot viewport, checks pointer and keyboard cursor interaction, and writes early, late and cursor PPM frames. See [the real-time plot example](../examples/13_realtime_plot/README.md).

`nexaui_simulation_dashboard OUTPUT_PREFIX [FONT_FILE]` renders two snapshots from a worker-produced oscillator with plot, telemetry and illustrative profiler panels. A font enables visible labels and values. See [the simulation dashboard example](../examples/14_simulation_dashboard/README.md).

`nexaui_3d_viewport_shell OUTPUT.ppm` draws a software wireframe cube through an application-owned viewport surface at 1× and 2× backing scales, then writes the focused frame. See [the 3D viewport example](../examples/15_3d_viewport_shell/README.md).

`nexaui_font_inspect FONT_FILE` checks one SFNT font and prints its table directory, horizontal metrics, sample Unicode-to-glyph mappings and TrueType outlines for A and é when available. The input is a local file path in the C runtime's filename encoding. Font bytes remain owned by the example until the font is closed.

`nexaui_glyph_raster FONT_FILE OUTPUT.pgm [HEX_SCALAR]` writes a cached grayscale glyph mask at 64 pixels per em. The scalar defaults to U+0041; pass `00E9` to exercise a composite glyph in fonts that use one. The output is a PGM image that most image tools can open.

`nexaui_font_fallback PRIMARY_FONT [FALLBACK_FONT ...]` opens up to eight local faces and prints which face supplies A, é, 日 and 😀. Font order determines the priority. This demonstrates cmap coverage selection; rendering a selected CFF or color glyph requires later text-engine work.

The same example registers a semantic window/button tree. Inspect its console output for the accessible name and focus state. Native screenreader bridges are still under development; see [Accessibility semantics](concepts/accessibility.md).

To use NexaUI from a CMake project, either call `add_subdirectory(path/to/NexaUI)` or install it and use:

```cmake
find_package(NexaUI CONFIG REQUIRED)
target_link_libraries(your_app PRIVATE NexaUI::NexaUI)
```

Include `<nexaui/nexaui.h>`. Create a context, begin a frame, append commands, end the frame, retrieve the borrowed command array, then render it. A later `nx_begin_frame` invalidates that borrowed array. See [Core API](api/core.md) for exact failure and ownership rules.
