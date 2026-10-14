# MASTER PROJECT PROMPT — NexaUI Native

## 0. Mission

Design and implement **NexaUI**, a production-quality, reusable, high-performance cross-platform UI framework and public UI API for future applications.

Primary platforms:
- Windows
- macOS
- Linux

Primary public implementation/API language:
- **C17**
- Plain C ABI
- No C++ required by consumers

NexaUI is intended to become the common UI foundation for:
- physics and scientific simulations,
- visualization software,
- game/engine tooling,
- editors,
- CAD-like tools,
- desktop utilities,
- launchers,
- developer tools,
- data-heavy applications,
- real-time applications.

This is a long-term foundational library, not a demo.

---

# 1. Hard dependency rule — self-made UI stack

NexaUI must NOT use third-party UI, windowing, rendering, layout, widget, text-rendering, font-rasterization, input, or immediate-mode frameworks/libraries.

Forbidden examples include, but are not limited to:
- SDL / SDL2 / SDL3
- GLFW
- Qt
- GTK
- Dear ImGui
- Nuklear
- Skia
- FreeType
- HarfBuzz
- Pango
- Cairo
- bgfx
- sokol
- raylib
- stb_truetype
- Yoga
- NanoVG
- any third-party widget/UI framework

Allowed foundations:
- C standard library
- platform/OS APIs
- GPU-driver APIs
- public file-format/specification standards
- Unicode/OpenType/TrueType specifications and public standards data
- compiler/platform SDK headers supplied by the target operating system/toolchain

Interpret "100% self-made" as:
**all reusable UI/window/input/layout/text/rendering logic belongs to NexaUI itself.**
We do not attempt to rewrite operating systems, GPU drivers, kernel services, or public standards.

If the platform normally exposes functionality through an OS/system framework, NexaUI may call that native platform API, but it must wrap it behind its own internal backend and must not expose that API in NexaUI's public interface.

No optional third-party runtime dependency may become required for core functionality.

---

# 2. Chosen architecture — do not re-decide casually

The following architecture is the default project direction.

## 2.1 Public API

- C17
- opaque handles where appropriate
- stable C ABI target
- namespaced API
- no third-party types in public headers
- no backend-specific types in ordinary widget/layout APIs
- explicit ownership/lifetimes
- no hidden mutable global state
- multiple independent UI contexts

## 2.2 Programming model

Use a **hybrid immediate/declarative public API with retained internal state/tree**.

Reason:
- immediate/declarative call sites are concise and synchronize naturally with application state,
- retained internal structure is superior for focus, accessibility, layout caching, animation, virtualization, stable identity, diagnostics and incremental work.

Principle:
`UI = f(application state)`

Application domain state belongs to the application.
NexaUI owns only UI-specific state.

## 2.3 Native platform backends

### Windows
Windowing/input/platform:
- Win32 native APIs
- native clipboard
- native cursor handling
- native IME/text-input integration
- native DPI/display handling
- native accessibility bridge

Renderer:
- Direct3D 12

### macOS
Windowing/input/platform:
- native Cocoa/AppKit implementation
- Objective-C `.m` implementation files are allowed internally where required by Apple's APIs
- public API remains pure C
- native clipboard
- native text/IME integration
- native DPI/backing-scale handling
- native accessibility bridge

Renderer:
- Metal

### Linux
Primary window/display backend:
- Wayland

Fallback/compatibility backend:
- X11

Renderer:
- Vulkan

Linux platform functionality must remain behind the NexaUI platform interface.
Avoid making application code depend directly on Wayland/X11/Vulkan.

## 2.4 Renderer abstraction

The core UI produces a backend-independent render command stream.

Backend implementations:
- `renderer_d3d12`
- `renderer_metal`
- `renderer_vulkan`
- `renderer_headless`

Widget/layout code must not contain D3D12/Metal/Vulkan branches.

## 2.5 Shader strategy

Maintain small backend-specific shader sets:
- HLSL/DXIL path for D3D12
- MSL/metallib path for Metal
- GLSL or direct SPIR-V build path for Vulkan

Do not introduce a third-party shader translation dependency.

Keep the UI shader surface deliberately small and shared conceptually across all backends.

---

# 3. Performance philosophy

NexaUI must be appropriate for real-time scientific simulations and editor/tool workloads.

The UI must not become the synchronization bottleneck of the simulation.

## 3.1 Simulation separation

Simulation, UI and rendering are separate subsystems.

Recommended architecture:

```text
                 OS event/UI thread
                         |
                         v
                   NexaUI frame
                         |
                  render commands
                         |
                         v
                    GPU renderer

Simulation thread(s)
       |
 fixed timestep
       |
       v
 immutable/read-only frame snapshot
       |
 double/triple buffer or bounded queue
       |
       v
 UI/rendering reads snapshot
```

Rules:
- simulation must not wait for ordinary UI layout/rendering,
- UI must not mutate simulation memory unsafely,
- avoid a global mutex around the complete simulation state,
- use snapshot/double-buffer/ring-buffer techniques,
- allow fixed-step simulation independent from display refresh rate,
- no UI callbacks inside hot simulation loops,
- no per-frame full copy of huge simulation data when views/ranges/handles are sufficient.

## 3.2 Memory/per-frame behavior

Target:
- near-zero avoidable heap allocations in steady-state UI frames,
- frame arenas/scratch allocators,
- reusable command buffers,
- glyph/image caches,
- persistent layout caches where beneficial,
- pooled transient objects,
- compact widget state,
- explicit memory accounting.

## 3.3 Rendering

Optimize for:
- batching,
- persistent GPU buffers where appropriate,
- ring buffers for dynamic geometry,
- texture/glyph atlases,
- scissor clipping,
- minimal pipeline changes,
- minimal descriptor/resource churn,
- backend-specific upload optimization,
- GPU timestamp/profiling hooks.

## 3.4 Large data

Support:
- virtualized lists/tables,
- viewport-based rendering,
- data decimation/downsampling interfaces for plots,
- custom render surfaces,
- zero/low-copy data views where safe,
- millions of logical rows without instantiating millions of widgets.

## 3.5 Benchmarks

Create repeatable benchmarks for:
- 1k / 10k / 100k logical widgets,
- nested layouts,
- large virtualized tables,
- text-heavy screens,
- rapidly changing simulation telemetry,
- render command generation,
- GPU submission,
- allocations/frame,
- memory use,
- frame latency.

Never claim performance superiority without measurements.

---

# 4. Native platform layer

Create a platform-neutral internal interface for:

- application lifecycle,
- windows,
- monitors,
- DPI/scaling,
- mouse/pointer,
- keyboard,
- text input,
- IME,
- cursors,
- clipboard,
- drag/drop,
- timing,
- file-drop,
- window focus,
- accessibility transport,
- optional native dialogs only if later justified.

Backends:
- Win32
- Cocoa/AppKit
- Wayland
- X11

The platform abstraction must make it possible to replace/add backends without changing core widgets.

---

# 5. Own text and font engine

NexaUI must implement its own text stack instead of FreeType/HarfBuzz/Pango/etc.

This is a major subsystem and must be planned incrementally.

Required final capabilities:

## Font file support
- TrueType/OpenType container parsing
- required SFNT tables
- cmap
- metrics
- glyph outlines
- kerning/positioning tables as required
- variable-font architecture where practical
- font fallback
- font collections where practical

## Rasterization
- own vector outline rasterizer
- grayscale antialiasing
- glyph cache/atlas
- scalable DPI-aware rendering
- subpixel strategy only if appropriate and platform-safe

## Shaping
- script-aware shaping architecture
- ligatures
- combining marks
- contextual substitution/positioning
- OpenType GSUB/GPOS support developed incrementally
- fallback shaping path

## Unicode
- UTF-8 public interface
- UTF decoding/validation
- grapheme boundaries
- word boundaries
- line breaking
- bidi/RTL architecture
- Unicode property tables generated from public Unicode data/specifications
- emoji/fallback architecture

## Editing
- cursor movement
- selection
- word navigation
- multiline editing
- clipboard
- IME/composition
- password fields
- undo/redo architecture

Do not fake final Unicode support with byte-oriented or ASCII-only logic.

During early milestones, limited script coverage is acceptable only if clearly marked incomplete and the architecture supports the final system.

---

# 6. Layout engine

Implement NexaUI's own layout system.

Philosophy:
- predictable,
- deterministic,
- fast,
- easier than CSS,
- expressive enough for professional desktop tools.

Required concepts:
- row
- column
- overlay/stack
- grid
- split panes
- wrapping
- min/preferred/max size
- fixed/flexible size
- grow/shrink
- padding
- margin
- gaps
- alignment
- scroll containers
- nested layouts
- DPI-aware logical units
- intrinsic sizing
- clipping
- virtualization hooks

Use measure/arrange or an equivalent deterministic two-phase model if evaluation confirms it is the best design.

Document layout rules precisely.

---

# 7. Styling and themes

Implement:
- semantic design tokens,
- typography,
- spacing,
- colors,
- borders,
- radii,
- shadows,
- opacity,
- focus state,
- hover state,
- pressed state,
- disabled state,
- validation/error state,
- theme inheritance/overrides,
- runtime theme switching.

Ship:
- polished dark theme
- polished light theme
- high-contrast theme

Default look:
- modern,
- restrained,
- professional,
- high information density without clutter,
- visually consistent,
- good typography,
- obvious keyboard focus,
- accessible contrast.

---

# 8. Accessibility

Accessibility is foundational, not optional.

Create a backend-independent semantic/accessibility tree containing:
- role
- accessible name
- description
- value
- state
- actions
- bounds
- hierarchy
- focus

Native bridges:
- Windows UI Automation
- macOS native accessibility API
- Linux AT-SPI-compatible integration strategy

Support:
- keyboard-only navigation
- logical focus traversal
- high contrast
- reduced motion
- scalable text/UI
- no essential color-only state

Accessibility semantics must be independent of visual rendering.

---

# 9. Core UI systems

Implement:

## Lifecycle/context
- create/destroy context
- begin/end frame
- multiple contexts
- allocator interface
- timing/frame state

## Identity
- stable hierarchical widget IDs
- ID stack/scopes
- collision diagnostics
- state lifetime/garbage collection

## Input
- pointer
- mouse buttons
- wheel
- keyboard
- modifiers
- shortcuts
- text input
- IME
- drag/drop
- multi-click
- focus

## Geometry
- Vec2/Vec4 or equivalent
- rectangles
- sizes
- insets
- transforms where justified
- clipping

## Focus/navigation
- tab/reverse-tab
- focus scopes
- modal focus
- programmatic focus
- restoration
- optional directional/spatial navigation

## Animation
- time-based transitions
- easing
- style/state transitions
- reduced-motion support

---

# 10. Widget set

Implement incrementally after foundations.

## Display
- text
- rich-text foundation
- image
- separator
- spacer
- progress
- tooltip

## Controls
- button
- icon button
- checkbox
- radio
- toggle
- slider
- drag value
- text input
- multiline editor
- numeric input
- combo/select
- searchable combo
- color controls

## Containers
- panel
- group
- collapsible section
- tabs
- split panes
- scroll area
- modal
- popup
- context menu

## Data/editor widgets
- virtualized list
- table
- sortable table
- tree
- tree table
- property inspector primitives

## Professional tool UI
- menu bar
- menus
- toolbar
- status bar
- docking
- resizable panes
- command-palette foundation
- multi-window/viewports

Provide a clean custom-widget API.

---

# 11. Renderer command model

Create a backend-neutral command representation for:
- filled rect
- stroked rect
- rounded rect
- line/polyline
- triangle/mesh
- glyph run
- image
- clipping/scissor
- transform
- layer/z-order
- custom render callback/surface where safe

The core renderer front-end should:
- sort/batch where safe,
- preserve visual ordering guarantees,
- minimize allocation,
- be testable headlessly.

---

# 12. Simulation-focused features

Because NexaUI will be used in simulation software, provide first-class foundations for:

- real-time plots,
- time-series views,
- high-frequency numeric displays,
- scene/viewport embedding,
- property inspectors,
- hierarchy/tree inspectors,
- large data tables,
- pause/step/reset controls,
- timeline controls,
- profiler panels,
- telemetry dashboards.

Plotting should eventually be part of NexaUI itself, not a mandatory third-party plotting library.

Design plot data APIs to avoid copying huge datasets every frame.

---

# 13. Threading model

Default:
- platform event processing on the OS-required/main thread,
- UI state mutation/frame construction on one defined UI thread,
- simulation on independent thread(s),
- renderer submission strategy may vary per backend,
- background jobs allowed through explicit synchronization.

Document all thread rules.

Never claim arbitrary thread safety.

Provide helper primitives/patterns for:
- simulation snapshots,
- UI command queues,
- background task completion,
- frame-safe handoff.

---

# 14. Build system

Use modern CMake.

Requirements:
- clean target-scoped settings
- Windows/macOS/Linux
- MSVC/Clang/GCC as appropriate
- no global CMake flag pollution
- static/shared build options
- install rules
- package config
- `find_package(NexaUI CONFIG REQUIRED)`
- `NexaUI::NexaUI`
- add_subdirectory support
- examples optional
- tests optional
- benchmarks optional
- documentation optional

Platform files may include:
- `.c`
- `.m` for macOS native integration
- `.metal`
- `.hlsl`
- Vulkan shader sources/build outputs

Public consumer API remains C.

---

# 15. Repository structure target

Plan toward something like:

```text
NexaUI/
  CMakeLists.txt
  README.md
  LICENSE
  CHANGELOG.md
  CONTRIBUTING.md
  ARCHITECTURE.md
  DECISIONS.md
  ROADMAP.md

  include/
    nexaui/

  src/
    core/
    memory/
    geometry/
    input/
    layout/
    style/
    animation/
    widgets/
    text/
    accessibility/
    render/
    diagnostics/

  platform/
    windows/
    macos/
    linux/
      wayland/
      x11/

  renderers/
    d3d12/
    metal/
    vulkan/
    headless/

  shaders/
    d3d12/
    metal/
    vulkan/

  tests/
  benchmarks/
  examples/
  docs/
  tools/
```

Planner may improve this structure, but changes require an explicit architectural reason.

---

# 16. Testing

Mandatory:

## Unit
- memory/allocators
- IDs/hashing
- geometry
- layout
- state
- input transitions
- focus
- styles
- text parsing
- Unicode logic
- clipping
- render command generation

## Integration
- complete frame behavior
- text editing
- keyboard navigation
- nested layout
- widgets
- DPI changes
- multi-window

## Headless/golden
Implement NexaUI's own deterministic headless renderer/test path.

## Fuzz/property
Especially:
- font parsers
- UTF-8
- OpenType tables
- text editing
- layout sizes
- hostile inputs

## Performance
Maintain benchmarks and regression history.

## Native
Test each real platform backend and renderer.

---

# 17. Documentation

Documentation is a required product deliverable.

Create a full User Guide:

```text
docs/
  index.md
  getting_started.md
  installation.md

  concepts/
    architecture.md
    frame_model.md
    state_and_identity.md
    memory.md
    threading.md
    layout.md
    styling.md
    input.md
    focus.md
    rendering.md
    text.md
    accessibility.md
    simulation_integration.md

  guide/
    first_window.md
    layouts.md
    widgets.md
    forms.md
    themes.md
    custom_widgets.md
    text_editing.md
    tables.md
    plotting.md
    docking.md
    simulation_ui.md
    performance.md
    debugging.md
    platform_notes.md

  api/
  contributing.md
  faq.md
```

Every public API should document:
- purpose
- parameters
- ownership
- lifetime
- thread rules
- failure behavior
- examples

---

# 18. Examples

Required progressive examples:

```text
examples/
  00_hello_window/
  01_hello_ui/
  02_buttons_and_input/
  03_layout/
  04_forms/
  05_scroll/
  06_tables/
  07_text_editing/
  08_themes/
  09_custom_widget/
  10_docking_editor/
  11_multi_window/
  12_accessibility/
  13_realtime_plot/
  14_simulation_dashboard/
  15_3d_viewport_shell/
  showcase/
```

Each example:
- compiles,
- runs,
- has README,
- is referenced by User Guide,
- avoids unnecessary unrelated complexity.

---

# 19. Diagnostics/developer experience

Build first-class diagnostics:
- UI inspector
- widget tree
- accessibility tree viewer
- focus viewer
- layout bounds
- clipping visualization
- ID collision warnings
- allocation stats
- frame timing
- layout timing
- render-command stats
- draw-call/batch stats
- GPU timing hooks
- simulation/UI synchronization diagnostics

Debug tooling must be removable/cheap in release builds.

---

# 20. API quality

Before v1.0 define:
- naming conventions
- handle conventions
- result/error model
- allocator conventions
- ownership
- callbacks/events
- string representation
- versioning
- deprecation
- ABI policy

Do not freeze an awkward API too early.

Before v1.0, breaking changes are allowed if documented and justified.

---

# 21. Security/robustness

C implementation must be defensive:
- bounds checks
- integer overflow awareness
- robust parser validation
- no use-after-free
- no buffer overflow
- no hidden mutable global state
- allocation failure paths where relevant
- malformed font/file data must not corrupt memory

Font/text parsers are untrusted-input surfaces and require fuzzing.

---

# 22. CI and platform validation

Create CI for:
- Windows
- macOS
- Linux

Compilers:
- MSVC
- Clang
- GCC where appropriate

CI should run:
- configure
- build
- unit tests
- headless tests
- warnings
- format/lint checks
- appropriate sanitizer configurations where reliable
- packaging tests

GPU/native visual tests may require separate/manual or hardware runners.

---

# 23. Performance acceptance philosophy

Do not bake arbitrary marketing numbers into correctness.

Instead:
- define benchmark scenarios,
- record hardware/software,
- establish baselines,
- detect regressions,
- profile before optimizing.

A UI frame should consume a small fraction of the frame budget in normal simulation applications.

When a performance issue exists:
1. measure,
2. locate bottleneck,
3. fix architecture/algorithm,
4. measure again.

---

# 24. Planning rules

This is a very large project.

The planner MUST create small dependency-aware tasks.

Do NOT create tasks like:
- "Implement Wayland"
- "Implement text engine"
- "Implement renderer"
- "Implement all widgets"

Instead subdivide.

Examples:

Wayland:
- connect to compositor
- registry discovery
- bind compositor
- create surface
- xdg-shell integration
- seat/pointer
- keyboard
- scale changes
- clipboard
- text input
- tests

Text:
- parse SFNT header
- parse table directory
- parse cmap format X
- glyph metrics
- simple glyf outlines
- raster edge representation
- scan conversion
- glyph cache
- shaping primitives
- GSUB subset
- GPOS subset
- bidi
- line breaking
- etc.

Renderer:
- backend interface
- resource handles
- command stream
- upload ring
- rectangle pipeline
- image pipeline
- text pipeline
- clipping
- batching
- profiling
- teardown/device loss

A task should generally fit comfortably inside one 65k-token agent context.

If it does not, subdivide it before implementation.

---

# 25. Required early task order

The initial plan should prioritize:

1. repository/bootstrap
2. architectural specification
3. dependency boundary/self-made rule
4. public API conventions
5. memory/allocator architecture
6. threading/simulation-integration model
7. testing/headless architecture
8. platform interface
9. renderer interface
10. core geometry
11. context/lifecycle
12. IDs/state
13. input event model
14. layout specification
15. style/theme specification
16. render command stream
17. headless backend
18. first native platform window backend
19. first native GPU backend
20. minimal text bootstrap
21. minimal interactive widgets
22. expand platform coverage
23. expand text engine
24. advanced widgets
25. accessibility
26. simulation-focused tooling
27. documentation/examples
28. performance hardening
29. cross-platform validation
30. v1.0 showcase/integration

Development may initially use macOS as the first live platform because the current developer machine is macOS, but architecture and tests must remain cross-platform.

Do not let the macOS-first implementation contaminate the public API with Apple-specific concepts.

---

# 26. Per-task definition of done

A task is complete only if applicable criteria are satisfied:
- compiles
- warnings checked
- tests exist
- tests pass
- finite timeouts used
- existing tests still pass
- architecture respected
- public API documented
- relevant User Guide/example updated
- no fabricated verification claim

If a generated executable hangs, terminate it and diagnose the cause.

Sanitizers must be optional because platform/toolchain sanitizer reliability differs.

---

# 27. v1.0 definition of done

NexaUI v1.0 requires:

## Platforms
- native Windows backend
- native macOS backend
- native Linux Wayland backend
- Linux X11 fallback

## Rendering
- D3D12
- Metal
- Vulkan
- headless test renderer

## Core
- stable C API
- layout
- styling
- input
- focus
- text
- accessibility
- diagnostics
- custom widgets

## Tool-grade widgets
- tables
- trees
- virtualized lists
- docking
- menus/toolbars
- multi-window
- text editor foundation
- plot foundation
- custom viewport surface

## Text
- own font parser/rasterizer
- Unicode-capable shaping architecture
- editing
- fallback
- IME integration
- bidi/RTL support

## Simulation readiness
- snapshot-friendly APIs
- high-frequency telemetry
- plotting
- large-table virtualization
- viewport integration
- benchmark evidence that UI does not impose an avoidable simulation bottleneck

## Quality
- tests
- CI
- benchmarks
- no known severe memory/correctness issues

## Documentation
- complete User Guide
- API reference
- architecture docs
- platform notes
- examples
- showcase

No third-party UI/windowing/rendering/text library may be required at runtime.

---

# 28. First planner objective

Create a detailed project DAG from this MASTER.

First produce architecture/foundation tasks before substantial feature implementation.

The plan must explicitly include:
- native Win32/D3D12 path,
- native Cocoa/Metal path,
- native Wayland+X11/Vulkan path,
- own text/font engine,
- own layout engine,
- own widget system,
- own accessibility layer,
- own headless testing renderer,
- simulation/UI threading and snapshot design,
- full documentation/examples,
- benchmarks and profiling.

Do not implement code during the planning phase.
