# NexaUI

NexaUI is a planned native, self-contained C17 desktop UI framework. The repository currently contains its **foundation**, not a complete UI toolkit. The implemented API creates independent contexts, supplies a reusable scratch arena and UTF-8 validation, measures and arranges nested row/column/wrapping/overlay/grid/split/scroll layouts, processes frame input and clip-aware interactive hit regions, records accessibility semantics, builds render command streams, renders basic shapes, RGBA8 images and A8 masks to a deterministic RGBA8 buffer, and parses SFNT font directories, horizontal metrics, Unicode `cmap` formats 4/12 and simple/composite TrueType glyph outlines. It rasterizes outlines into A8 masks, reuses them through a bounded LRU cache, and resolves glyphs through an ordered font fallback collection. Unicode 18.0.0 property tables are reproducibly generated from pinned sources. Default grapheme, word and line boundaries and single-line paragraph bidi levels and ordering pass the official Unicode test corpora. An initial shaping path groups glyphs by font, script and bidi level and supports selected GSUB single and ligature substitutions, GPOS single and pair positioning, legacy kerning and combining marks. Initial multiline text layout exposes wrapping, hard-break lines, positioned glyphs, grapheme carets, hit testing and selection geometry; positioned glyphs can be added to a frame as A8 bitmap commands. Native window and basic input backends are available on Windows and Linux/Wayland or X11; Windows also has a D3D12 clear/present path that draws filled rectangles, triangles, RGBA8 images and A8 glyph masks with nested clips and alpha compositing. macOS, complete Wayland input, other GPU drawing and native accessibility bridges remain pending; text layout and script coverage remain incomplete. See [ROADMAP.md](ROADMAP.md) for the dependency-aware path to v1.0.

The initial mutable text buffer supports grapheme-safe selection and deletion, word/line navigation, bounded undo/redo and normalized key/text events. Single-line and multiline text fields render and edit that buffer with validation semantics and optional clipboard shortcuts. Win32, X11 and Wayland plain-text clipboard and local-file drop reception are available. Win32 displays preedit in focused ordinary text fields and positions native IME candidate UI; X11 accepts composed UTF-8 through an optional XIM context and positions preedit UI when its input method supports that style. Wayland accepts committed UTF-8 through optional text-input-v3, with v1 as a fallback; a text-field widget with `ime_window` activates it according to frame focus. Preedit from Windows, X11 callback-style XIM, and Wayland v3/v1 is drawn by focused text fields without changing their committed draft; Wayland password fields display masked preedit. Focused ordinary text fields also advertise a bounded UTF-8 surrounding-text slice and apply valid v3 selection-relative or v1 cursor-relative deletion requests before committed text; X11 callback-style XIM still needs installed-IME acceptance, and macOS preedit remains planned.

Semantic color and metric tokens have dark, light and high-contrast presets. Contexts can switch themes between frames and inherit local overrides through balanced frame scopes. See [the theme contract](docs/concepts/themes.md); complete widget styling is still planned.

Applications can compose custom widgets from layout, frame commands, interaction and accessibility semantics through `nx_widget_register`. See [the custom widget contract](docs/concepts/widgets.md) and the headless [button and progress example](examples/09_custom_widget/README.md).

Initial display widgets provide labels, images, separators and fixed layout spacers. Labels use an existing shaped text layout; images borrow their bitmap pixels through rendering. See [the display widget contract](docs/concepts/widgets.md#display-widgets).

An initial rich-text drawing path styles ranges of an existing text layout with foreground/background colors, underline and strike while preserving its grapheme, bidi and caret geometry. A rich label keeps the complete text as one accessible name. See [the text contract](docs/concepts/text.md).

Basic theme-drawn buttons, checkboxes, radios and toggles handle pointer and keyboard activation with matching accessibility state. See [the control contract](docs/concepts/widgets.md#basic-controls).

Panels, groups, collapsible sections and tabs provide painted containers and nested accessibility semantics. See [the container contract](docs/concepts/widgets.md#containers-and-tabs).

Scroll areas route wheel input, clip their children, expose clamped offsets and optionally support draggable, keyboard-focusable scrollbar thumbs. A virtual-list height index computes visible ranges without creating offscreen rows; its measured-row rebuild keeps a stable keyed row anchored after data changes, and a budgeted visitor instantiates only visible rows. See [the scroll and virtualization contract](docs/concepts/layout.md#scroll-and-virtualization).

Tables now have sortable, keyboard-accessible headers, resizable columns and stable row-index sorting. Applications register only visible rows and cells. See [the table contract](docs/concepts/widgets.md#tables).

Trees now flatten expanded preorder data to visible indices and provide selectable, expandable items that also fit inside table cells. See [the tree contract](docs/concepts/widgets.md#trees-and-tree-tables).

Popups and modals now provide a backdrop, Escape/outside dismissal, bounded keyboard focus and focus restoration. Named noninteractive tooltips are available. See [the overlay contract](docs/concepts/widgets.md#popups-modals-and-tooltips).

Menu bars, menus, toolbars and status bars now expose semantic containers. Menu items support keyboard sibling navigation and optional checked state. See [the menu contract](docs/concepts/widgets.md#menus-and-bars).

Split panes now have a draggable, keyboard-accessible divider. A caller-buffer docking model supports split stacks, tab moves and a versioned binary layout snapshot. See [the split and docking contract](docs/concepts/layout.md#split-panes).
The [docking editor example](examples/10_docking_editor/README.md) connects that model to tab widgets and a draggable two-pane layout.

A command palette and searchable select now share a filtered popup with keyboard, pointer and wheel navigation. The application owns command dispatch and the select trigger. See [the search widget contract](docs/concepts/widgets.md#command-palette-and-searchable-select).

Color controls now provide HSV and hex conversions, an interactive color picker and a three-preset theme selector. See [the color and theme contract](docs/concepts/themes.md#color-and-theme-controls).

On Windows, each native window can route its queued events to an independent UI context. A native two-window test covers focus, input isolation, transfer retry and closing one window while the other continues. See [the input contract](docs/concepts/input.md).

Caller-owned scalar transitions now use frame time, linear or smoothstep easing, interruption from the current value and a per-context reduced-motion setting. Toggles can animate their thumb without delaying checked semantics. Windows can read the current system animation preference. See [the animation contract](docs/concepts/animation.md).

Horizontal sliders and drag-value controls now clamp and snap caller-owned numbers, track pointer capture across bounds and expose their current value through accessibility semantics. See [the value-control contract](docs/concepts/widgets.md#value-controls).

A [headless visual regression scene](tests/baselines/README.md) checks layout, multiple controls, interaction, semantics and all three theme presets against reviewed PPM images.

A [two-slot snapshot exchange](docs/concepts/simulation.md) lets one simulation thread publish immutable data while the UI holds a read-only view through a frame, with deterministic reclamation and no data copy by the exchange.

A [bounded command channel](docs/concepts/sim-channel.md) sends UI requests to simulation workers and returns ticketed completions with explicit ownership and backpressure.

A [time-series data view](docs/concepts/series.md) borrows immutable points and decimates only the visible range into per-pixel envelopes retaining extrema and source order. A one-million-point Release benchmark records full and narrow-view costs.

An initial [real-time plot widget](docs/concepts/plotting.md) renders one series with theme-aware axes, grid and optional numeric labels. It supports pointer and keyboard cursors and caller-owned state. Three headless theme baselines cover the visual output.

Initial [telemetry and profiler panels](docs/concepts/telemetry.md) consume borrowed metric and timing arrays, render only visible rows, and expose names, values, ranges and status through semantics. A 10,000-frame update test covers 100,000 logical records without steady-state Context allocations.

A [hierarchy and property inspector](docs/concepts/inspector.md) combines the existing tree primitives with typed property rows. Rows show immutable snapshot values and return boolean or numeric change proposals or an external-editor request. The [headless inspector example](examples/09_inspector_headless/README.md) exercises selection and a proposed edit.

The initial [custom viewport surface](docs/concepts/viewport.md) embeds an application-rendered RGBA8 image with explicit physical resize and render-completion lifetime. A [software 3D shell](examples/15_3d_viewport_shell/README.md) demonstrates a cube preview at 1× and 2× backing scales. Direct GPU texture import remains planned with the native renderers.

A [private renderer driver](docs/concepts/renderer-backend.md) validates complete command streams and defines resize, temporary suspension and device-loss recovery for backend implementations. The headless renderer uses the same command preflight. Windows now has an initial [D3D12 geometry, bitmap and clip path](docs/concepts/d3d12.md) with reusable upload storage, opaque batching, two bounded in-flight opaque frames, clipped-area translucent copies and GPU timestamp diagnostics, plus a [native sample](examples/10_native_clear/README.md). Remaining GPU shapes and more efficient translucent batching are planned.

The numeric editor parses decimal input independently of locale and commits valid drafts on Enter while preserving incomplete text for correction. A rendered numeric field now draws its draft, selection and caret and exposes validation state to accessibility.

## Build

Optional Windows ASan and Clang fuzz jobs, local results and reproduction
commands are in [the sanitizer report](docs/quality/sanitizers.md).

```sh
cmake -S . -B build
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
```

`BUILD_SHARED_LIBS=ON` builds a shared library. `NEXAUI_BUILD_TESTS` and `NEXAUI_BUILD_EXAMPLES` control optional targets. The public target is `NexaUI::NexaUI` with `add_subdirectory` or after installation through `find_package(NexaUI CONFIG REQUIRED)`.

Linux builds require X11, Wayland and xkbcommon development headers and the Wayland protocol scanner. At runtime NexaUI chooses Wayland when available, then X11; `NEXAUI_PLATFORM` can force either backend. Headless core tests run without a display, while native integration tests skip if their display is absent.

The [core dependency boundary](docs/concepts/dependencies.md) is checked during CMake configuration and with negative CTest fixtures.

Unicode tables are generated from the checked-in [Unicode 18.0.0 data](data/unicode/18.0.0/README.md). Run `python tools/generate_unicode.py --check` to verify source hashes and generated output.

The headless samples in `examples/00_headless`, `examples/02_layout_headless`, `examples/03_interactive_region`, `examples/05_scroll`, `examples/06_tables`, `examples/07_text_editing` and `examples/08_themes` write PPM images. They demonstrate virtual scrolling, stable table sorting, text editing and runtime theme switching. On Windows, `examples/00_hello_window` introduces a native window, `examples/10_native_clear` exercises the private D3D12 clear/present path, and `examples/11_multi_window` runs two independent native UI contexts and renderers.
`examples/04_font_inspect` reads a local TrueType/OpenType file and prints its checked table directory and horizontal metrics.
`examples/07_text_headless` renders positioned text from a TrueType font into a PPM image, for example `nexaui_text_headless arial.ttf text.ppm`.
`examples/09_custom_widget` renders a custom button and progress display across four frames, including pointer and keyboard activation.
`examples/12_accessibility` verifies keyboard traversal and prints a semantic tree with focus, checked and disabled states.
`examples/01_hello_ui` renders the first UI command stream in a native Windows window, with optional TrueType labels.
`examples/02_buttons_and_input` adds a native button and checkbox with mouse, keyboard and semantic-state checks; it requires a local TrueType font.
`examples/03_layout` shows wide and narrow layouts of the same headless UI, with geometry and semantic checks.
`examples/04_forms` validates two text fields, requires confirmation, and saves invalid and submitted form states.
`examples/13_realtime_plot` advances a ring-buffered signal, renders a moving time window and checks its pointer and keyboard cursor.
`examples/14_simulation_dashboard` composes a plot, telemetry and profiler panels from worker-published immutable snapshots; its optional font displays labels and values.
`examples/showcase` runs a native Windows D3D12 editor/dashboard with live oscillator data, scene selection, a gain inspector, telemetry and a responsive compact view.

## Current contract

- Public interface: [include/nexaui/nexaui.h](include/nexaui/nexaui.h).
- Architecture and threading: [ARCHITECTURE.md](ARCHITECTURE.md).
- Decisions and known gaps: [DECISIONS.md](DECISIONS.md).
- Full project requirements: [MASTER.md](MASTER.md).

NexaUI does not depend on third-party UI, windowing, rendering, layout, text or font libraries. Native OS and graphics APIs remain behind internal platform and renderer modules.
