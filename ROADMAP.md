# NexaUI development DAG

This is the implementation plan for [MASTER.md](MASTER.md). An edge `A -> B` means B starts after A's public contract and tests are accepted. Tasks are intentionally narrow; a task may be split further when implementation exposes new constraints. Every task follows the definition of done in MASTER section 26. Platform tasks additionally require a native build and a manual smoke test on that platform. A successful headless build never substitutes for native validation.

## Milestones and gates

| Gate | Outcome | Required tasks |
| --- | --- | --- |
| F0 | Reproducible C17 foundation with documented contracts | F01-F12 |
| F1 | Deterministic core frame and headless image | C01-C16, H01-H05 |
| F2 | First interactive native window on each OS | W01-W08, M01-M08, L01-L12 |
| F3 | Native GPU display for the core command stream | D01-D08, A01-A08, V01-V09 |
| F4 | Text, controls, accessibility, and simulation primitives | T01-T18, U01-U18, X01-X08, S01-S07 |
| F5 | Tool-grade UI and release readiness | P01-P14, Q01-Q12 |

F0/F1 may run on any development OS. F2/F3 cannot be signed off until Windows, macOS, Wayland and X11 have each been exercised on real systems. Keep missing validation visible in the status table below.

## Foundation — F

| ID | Work item | Depends on | Acceptance evidence |
| --- | --- | --- | --- |
| F01 | Repository layout, license, contribution rules and change log | — | Files exist and scope is documented |
| F02 | Target-scoped CMake static/shared builds, install and package config | F01 | add_subdirectory and installed find_package consumers build |
| F03 | Dependency policy and build-time dependency audit | F01 | forbidden libraries absent from core targets |
| F04 | Architecture: module boundaries, event and render data flow | F01 | ARCHITECTURE.md with ownership diagram |
| F05 | C ABI conventions, naming, versioning and error model | F04 | public header convention document |
| F06 | Allocator interface and overflow-safe allocation rules | F05 | allocator failure tests |
| F07 | Frame arena and reusable buffer primitives | F06 | reset/reuse and growth tests |
| F08 | Thread contract and simulation snapshot handoff design | F04 | documented race/lifetime invariants |
| F09 | Headless test architecture and deterministic output format | F04 | repeatability specification |
| F10 | Platform interface: windows, events, clipboard, IME, scaling | F04,F05 | internal interface compiles without native headers in core |
| F11 | Renderer interface: surfaces, commands, resources, failures | F04,F05 | internal interface plus mock tests |
| F12 | CI matrix bootstrap for MSVC, Apple Clang, GCC/Clang | F02 | configure/build/test jobs on all three OSes |

## Core and headless — C/H

| ID | Work item | Depends on | Acceptance evidence |
| --- | --- | --- | --- |
| C01 | Logical geometry types, bounds and overflow/NaN policy | F05 | geometry tests |
| C02 | UTF-8 validation and decoding with explicit error policy | F05 | malformed input tests and fuzz entry |
| C03 | Context create/destroy with caller allocator and independent instances | F06,F10,F11 | two-context tests and failure injection |
| C04 | Frame begin/end state machine and timing input | C03 | invalid-order tests |
| C05 | Hierarchical 64-bit IDs and scope stack | C04 | stability and collision diagnostics tests |
| C06 | Retained node registry and state lifetime reclamation | C05,F07 | frame reuse/removal tests |
| C07 | Normalized pointer, keyboard, wheel and text event queue | F10,C04 | ordering and transition tests |
| C08 | Focus traversal, scopes and programmatic focus | C06,C07 | keyboard navigation tests |
| C09 | Measure/arrange rules: constraints, padding and intrinsic size | C01,C06 | specification and golden layout tests |
| C10 | Row and column layout with grow/shrink and alignment | C09 | nested constraint tests |
| C11 | Overlay, clipping and scrolling primitives | C09,C10 | hit-testing and clipping tests |
| C12 | Grid, wrapping and split-pane layout | C10,C11 | finite/infinite constraint tests |
| C13 | Semantic design tokens and theme inheritance | C06 | dark/light/high-contrast token tests |
| C14 | Backend-neutral command stream with ordering and clips | C01,F11,F07 | command serialization tests |
| C15 | Hit testing, pointer capture and interaction states | C07,C11 | overlap/capture tests |
| C16 | Diagnostics counters: nodes, allocations, timing, commands | C04,C06,C14 | deterministic reporting tests |
| H01 | Headless RGBA surface, clear and filled rectangles | C14,F09 | pixel/golden tests |
| H02 | Stroke, rounded rectangle, line and triangle rasterization | H01 | pixel/golden tests |
| H03 | Clip stack and alpha compositing | H01,C11 | nested clip tests |
| H04 | Image sampling and glyph-mask compositing | H03 | golden image tests |
| H05 | Command batching measurements without visual reordering | H04,C16 | output equivalence and counts |

## Native platform layer — W/M/L

| ID | Work item | Depends on | Acceptance evidence |
| --- | --- | --- | --- |
| W01 | Win32 process/window class lifecycle | F10,C03 | create/close smoke test |
| W02 | Win32 resize, monitor and per-monitor DPI events | W01,C07 | monitor/scale smoke test |
| W03 | Win32 pointer, wheel, capture and cursors | W01,C07 | input smoke test |
| W04 | Win32 keyboard mapping and text/IME composition | W01,C07,C02 | international input smoke test |
| W05 | Win32 clipboard and file drag/drop | W01 | transfer tests |
| W06 | Win32 window focus and multi-window routing | W01,C08 | two-window smoke test |
| W07 | Win32 UI Automation bridge transport | W06,X01 | accessibility inspection |
| W08 | Win32 integration and teardown stress | W02-W07 | native regression run |
| M01 | Cocoa application/window lifecycle in internal Objective-C | F10,C03 | create/close smoke test |
| M02 | Cocoa backing scale, resize and displays | M01,C07 | display-change smoke test |
| M03 | Cocoa pointer, scrolling and cursors | M01,C07 | input smoke test |
| M04 | Cocoa keyboard and NSTextInputClient composition | M01,C07,C02 | international input smoke test |
| M05 | Cocoa pasteboard and file drop | M01 | transfer tests |
| M06 | Cocoa focus and multi-window routing | M01,C08 | two-window smoke test |
| M07 | macOS accessibility bridge transport | M06,X01 | accessibility inspection |
| M08 | Cocoa integration and teardown stress | M02-M07 | native regression run |
| L01 | Wayland connection and registry discovery | F10 | protocol smoke test |
| L02 | Wayland compositor surface and xdg-shell lifecycle | L01,C03 | create/close smoke test |
| L03 | Wayland seat and pointer events | L02,C07 | pointer smoke test |
| L04 | Wayland keyboard/keymap and repeat | L02,C07 | keyboard smoke test |
| L05 | Wayland scale, output and focus events | L02,C07,C08 | scale/focus smoke test |
| L06 | Wayland clipboard, drag/drop and text input | L03,L04,C02 | transfer/IME smoke tests |
| L07 | X11 connection, window lifecycle and close protocol | F10,C03 | create/close smoke test |
| L08 | X11 pointer, keyboard and text input | L07,C07,C02 | input smoke test |
| L09 | X11 resize, scale, focus and multiple windows | L07,C08 | two-window smoke test |
| L10 | X11 selections/clipboard and file drop | L07 | transfer smoke test |
| L11 | Linux AT-SPI transport strategy and bridge | X01,L05,L09 | accessibility inspection |
| L12 | Wayland/X11 selection, integration and teardown stress | L01-L11 | native regression runs |

## Native renderers — D/A/V

| ID | Work item | Depends on | Acceptance evidence |
| --- | --- | --- | --- |
| D01 | D3D12 device, queue and swap chain | W01,F11 | clear/present smoke test |
| D02 | D3D12 descriptor/resource lifetime and device loss | D01 | teardown/recovery tests |
| D03 | D3D12 HLSL rectangle and triangle pipeline | D01,C14 | visual comparison with headless |
| D04 | D3D12 clipping and blend state | D03,H03 | nested clip image comparison |
| D05 | D3D12 image/glyph texture upload | D02,D04,H04 | image/text image comparison |
| D06 | D3D12 dynamic upload ring and batching | D05 | stress and timing data |
| D07 | D3D12 GPU timestamps and diagnostics | D06,C16 | timing smoke test |
| D08 | D3D12 resize, DPI and device-loss integration | D02-D07,W02 | native regression run |
| A01 | Metal device, layer and command queue | M01,F11 | clear/present smoke test |
| A02 | Metal resource lifetime and surface recovery | A01 | teardown/recovery tests |
| A03 | MSL rectangle and triangle pipeline | A01,C14 | visual comparison with headless |
| A04 | Metal clipping and blend state | A03,H03 | nested clip image comparison |
| A05 | Metal image/glyph texture upload | A02,A04,H04 | image/text image comparison |
| A06 | Metal dynamic upload ring and batching | A05 | stress and timing data |
| A07 | Metal GPU timing hooks | A06,C16 | timing smoke test |
| A08 | Metal resize, scale and integration | A02-A07,M02 | native regression run |
| V01 | Vulkan instance, device and queue selection | L02,L07,F11 | device smoke test |
| V02 | Vulkan Wayland and X11 surfaces/swap chains | V01 | clear/present on both |
| V03 | Vulkan resource lifetime and surface recovery | V02 | teardown/recovery tests |
| V04 | SPIR-V rectangle and triangle pipeline | V02,C14 | visual comparison with headless |
| V05 | Vulkan clipping and straight-alpha composition | V04,H03 | nested clip and translucent image comparison |
| V06 | Vulkan image/glyph texture upload | V03,V05,H04 | image/text image comparison |
| V07 | Vulkan dynamic upload ring and batching | V06 | stress and timing data |
| V08 | Vulkan GPU timestamps and diagnostics | V07,C16 | timing smoke test |
| V09 | Vulkan resize, scale and integration | V03-V08,L12 | native regression run |

## Text engine — T

| ID | Work item | Depends on | Acceptance evidence |
| --- | --- | --- | --- |
| T01 | Bounded SFNT header and table directory parser | C02,F06 | malformed-font fuzz target |
| T02 | `head`, `maxp`, `hhea`, `hmtx` metric parsing | T01 | known-font metric tests |
| T03 | `cmap` formats 4 and 12 | T01 | Unicode mapping tests |
| T04 | `loca` and simple `glyf` outline parsing | T02,T03 | malformed-outline fuzz tests |
| T05 | Composite glyph resolution with depth limits | T04 | recursive/cycle tests |
| T06 | Outline flattening and grayscale scan conversion | T04 | glyph golden tests |
| T07 | Glyph mask cache and atlas eviction | T06,H04 | reuse and memory-cap tests |
| T08 | Fallback font chain and font collection model | T03,T07 | missing glyph tests |
| T09 | Unicode property data generation and version pinning | C02 | reproducible generated tables |
| T10 | Grapheme, word and line boundary algorithms | T09 | Unicode conformance subsets |
| T11 | Bidi levels and RTL run ordering | T09 | bidi conformance subsets |
| T12 | Shaping run model, basic kerning and combining marks | T03,T10,T11 | mixed-script tests |
| T13 | Bounded GSUB lookup subset | T01,T12 | substitution tests and fuzz target |
| T14 | Bounded GPOS lookup subset | T01,T12 | positioning tests and fuzz target |
| T15 | Text layout, wrapping, caret positions and selection geometry | T10-T14,C09 | multiline golden tests |
| T16 | Editing buffer, undo/redo and navigation | T10,T15 | editing tests |
| T17 | IME composition and password field integration | T16,W04,M04,L06,L08 | native input tests |
| T18 | Text stress, parser fuzzing and script coverage report | T01-T17 | corpus results and known gaps |

## Widgets, semantics and simulation — U/X/S

| ID | Work item | Depends on | Acceptance evidence |
| --- | --- | --- | --- |
| U01 | Custom widget API and semantic node registration | C06,C09,C14 | example custom widget |
| U02 | Label, separator, spacer and image | U01,T15 | golden tests |
| U03 | Button, checkbox, radio and toggle | U01,C15,C08 | pointer/keyboard tests |
| U04 | Slider, drag value and numeric input | U03,T16 | boundary tests |
| U05 | Single/multiline input and validation state | U03,T16 | editing/IME tests |
| U06 | Panels, groups, collapsible sections and tabs | U01,C12 | nested layout tests |
| U07 | Scroll area and virtual list range API | U06,C11 | million-row logical test |
| U08 | Table, sortable columns and column sizing | U07 | sort/resize tests |
| U09 | Tree and tree table | U07,U08 | expansion/selection tests |
| U10 | Popup, tooltip, modal and focus trapping | U03,C08 | navigation tests |
| U11 | Menus, menu bar, toolbar and status bar | U10 | keyboard and hierarchy tests |
| U12 | Split panes and docking model | U06,C12 | serialization and drag tests |
| U13 | Command palette and searchable select | U05,U10 | filtering/navigation tests |
| U14 | Color and theme controls | U04,C13 | conversion and state tests |
| U15 | Multi-window UI routing | W06,M06,L09,U01 | native integration tests |
| U16 | Animation and reduced-motion state | C13,U03,X03 | transition tests |
| U17 | Rich text foundation and inline formatting | T15,U02 | layout tests |
| U18 | Widget integration and visual regression suite | U02-U17 | headless and native runs |
| X01 | Backend-neutral semantics tree, roles and bounds | C06,C08 | tree snapshot tests |
| X02 | Accessible names, descriptions, values and actions | X01,U03 | semantic tests |
| X03 | Focus, high contrast, scale and reduced-motion contracts | X02,C13 | keyboard/contrast tests |
| X04 | Accessibility tree inspector and focus viewer | X01,C16 | inspectable output |
| X05 | Windows bridge acceptance | W07,X02 | UIA inspection |
| X06 | macOS bridge acceptance | M07,X02 | native inspection |
| X07 | Linux bridge acceptance | L11,X02 | AT-SPI inspection |
| X08 | End-to-end keyboard and screen-reader scenarios | X03,X05-X07 | platform reports |
| S01 | Snapshot/view lifetime API for simulation data | F08,C04 | race/lifetime tests |
| S02 | UI command queue and completion handoff | S01 | bounded queue tests |
| S03 | Time-series/plot data-view API and decimation | S01,U01 | large-series benchmark |
| S04 | Real-time plot widget and axes | S03,T15 | golden/interaction tests |
| S05 | Telemetry display and profiler panels | S01,S04,C16 | update stress test |
| S06 | Property/hierarchy inspector primitives | U08,U09,S01 | tool example |
| S07 | Custom 3D viewport surface contract | F11,U01,S01 | lifecycle and resize tests |

## Product and release — P/Q

| ID | Work item | Depends on | Acceptance evidence |
| --- | --- | --- | --- |
| P01 | Getting started, installation and first-window guide | F02,W01,M01,L02 | platform instructions |
| P02 | Architecture, memory, threading and frame model guide | F04,F08,C04 | reviewed examples |
| P03 | Layout, styling, input and focus guide | C08,C13 | examples compile |
| P04 | Rendering and custom widget guide | U01,H04 | examples compile |
| P05 | Text, accessibility and editing guide | T17,X08 | examples compile |
| P06 | Simulation integration, plotting and performance guide | S01-S07 | example and measured data |
| P07 | API reference generated or maintained for every public symbol | F05,U18 | ownership/failure/thread notes |
| P08 | Progressive examples 00-05 | U07 | each builds and runs |
| P09 | Progressive examples 06-09 | U18 | each builds and runs |
| P10 | Progressive examples 10-12 | U18,X08 | each builds and runs |
| P11 | Progressive examples 13-15 | S07 | each builds and runs |
| P12 | Showcase editor/dashboard | P08-P11 | native walkthrough |
| P13 | Platform notes, FAQ and troubleshooting | W08,M08,L12 | validated instructions |
| P14 | Contribution and ABI change policy | F05,F12 | release checklist |
| Q01 | Allocator and frame-allocation benchmark | F07,C16 | recorded baseline |
| Q02 | 1k/10k/100k logical-widget benchmark | U07 | recorded baseline |
| Q03 | Nested layout and text-heavy benchmark | C12,T15 | recorded baseline |
| Q04 | Large table and telemetry benchmark | U08,S05 | recorded baseline |
| Q05 | Render command, submission and latency benchmark | H05,D08,A08,V09 | hardware metadata |
| Q06 | Font, UTF-8, editing and layout fuzz campaigns | T18,C12 | reproducible corpora |
| Q07 | Optional ASan/UBSan/Windows sanitizer jobs | F12,Q06 | supported-toolchain report |
| Q08 | Installed-package consumer tests on three OSes | F02,W08,M08,L12 | CI evidence |
| Q09 | Debug inspector, clipping and bounds visualization | C16,X04 | screenshot/test |
| Q10 | Full native integration matrix and known-issues audit | W08,M08,L12,D08,A08,V09,X08 | dated results |
| Q11 | ABI review, API consistency pass and release candidate | P07,Q08,Q10 | reviewed C consumer |
| Q12 | v1.0 release, changelog and source package | P12,Q01-Q11 | reproducible release |

## Current status

| Gate | Status | Evidence |
| --- | --- | --- |
| F0 | In progress | F01-F09 have initial implementations/specifications. F03 now audits core link dependencies and forbidden framework headers at CMake configuration. F11 has a private renderer driver with shared command preflight and mock lifecycle tests. Static and shared MSVC and Alpine/GCC builds, tests, and installed-package consumers pass locally; Linux X11 passes under Xwayland and Xvfb, while Wayland protocol setup passes under WSLg with GCC sanitizers. F10 and F12 remain unverified; CI is authored but has not run on hosted runners. |
| F1 | In progress | C02-C15 and H01-H04 have initial slices, including frame-retained hit regions, basic focus traversal, one-child scroll layout, wrapping rows/columns, fixed/auto/flex grid tracks, two-pane split layout, semantic dark/light/high-contrast theme tokens, nested clip-aware hit testing, headless shapes, bitmap sampling and A8 mask compositing. A 1k/10k/100k layout baseline is recorded under `benchmarks/results.md`. An initial wheel-driven scroll-area widget and interactive split divider now exist. Focus scopes, complete widget styling, image decoding and native rendering remain pending. |
| F2 | In progress | W01 and basic W03/W04 events have Win32 tests, including a hidden-window smoke test. W03 now selects per-window native client-area cursors through a backend-neutral C enum; an integration test covers every mapped shape, two-window isolation and thread ownership, and the showcase uses a crosshair over its plot. The normalized-event and UTF-8 file-drop queues now live in private OS-neutral modules with static/shared tests. W04 now positions native IME candidate UI at a focused text field's drawn caret, copies IMM composition into the field, and has a native test for positioning, preedit state, focus withdrawal and committed Unicode from `WM_IME_CHAR`. W05 has UTF-8 plain-text clipboard get/set through `CF_UNICODETEXT`; its transfer test uses an isolated window station when available or skips while the interactive clipboard is occupied. W05 also accepts Win32 filesystem drops, converts full batches to UTF-8 and exposes a caller-buffer queue; a native message test covers Unicode paths, multiple windows and allocation rollback. W06 has an initial two-window routing test for independent focus/input queues and lifetime. L01-L04 now have an initial Wayland connection, xdg-shell toplevel, shared-memory mapping, pointer events and basic XKB keyboard/text translation, exercised under WSLg. Bounded key repeat and eleven self-drawn cursor shapes now have native callback and deterministic pixel/timer tests; integer output scaling, SHM background and cursor buffer transitions have a native 1×/2× callback test. Direct physical-input and mixed-DPI acceptance remain pending. L07-L09 now have an initial Linux/X11 window adapter with Unicode titles, close/resize/focus, pointer/wheel/basic keyboard and text events, cursors and per-window routing, exercised natively under Xwayland. X11 scale remains 1.0. UTF-8 clipboard selection ownership and direct/incremental transfers have native Xvfb tests, including a selection change mid-transfer. Xdnd local-file copy drops now have direct, incremental and rejection tests on a private Xvfb server; physical file-manager acceptance and installed-IME validation remain pending. macOS preedit, real installed-IME acceptance, virtual-file drops, actual Explorer drag acceptance and full DPI/input coverage remain pending. Cocoa, complete physical-input acceptance, fractional scaling and renderer integration remain pending. Wayland clipboard pipe and callback tests pass under WSLg; cross-client selection still needs interactive acceptance. Wayland local-file drops now parse URI-list offers into the shared queue; parser and native queue tests pass, while a physical drag from another client remains unverified. Optional Wayland text-input-v3 now accepts committed UTF-8 after `done`, with text-input-v1 as a fallback; a focused ordinary text-field widget activates the available protocol at frame end. A private v3 compositor test covers enable, cursor placement, buffered commit text and preedit, `done`, text-field preedit drawing, and disable. Native WSLg v1 protocol requests, synthetic commit events, two-field focus transitions, password composition and teardown pass. A real installed IME remains unverified; surrounding-text deletion has a private compositor test but still needs installed-IME acceptance. |
| F3 | In progress | D01 creates a Windows D3D12 device, queue and swap chain and clears/presents it. D02 detects device removal and recreates its resources. D03 draws all current core shape commands, including stroked and rounded rectangles and round-capped lines. D04 adds nested scissor clips and straight-alpha compositing. D05 uploads RGBA8 images and A8 glyph masks; GPU readback tests compare every byte with headless output at 1× and 2×, including translucent targets and rasterized masks. D06 has per-slot vertex upload rings, a bounded two-frame opaque path, reusable bitmap upload and texture storage, conservative opaque and non-overlapping translucent shape batching, and clipped-area compositing copies. D07 exposes fence-safe GPU upload/render timestamps and private diagnostics. V01 now creates a Vulkan instance, device and graphics queue in a separate private Linux target, with deterministic failure tests and real Lavapipe queue/fence execution under validation layers. Bitmap compositing optimization and broader physical device-loss acceptance remain pending. Metal A01–A06 has native static/shared shape/image/composition/ring checks; A07 now has submission-keyed GPU timing, with physical GPU/display acceptance still open. V02 has initial X11/Wayland surfaces and clear/present swapchains with byte-exact readback, resize/scale and owner-thread lease tests; V03 now adds strict present-fence retirement and retryable teardown; physical GPU-loss acceptance remains open. V04/V05 draw core shapes through embedded SPIR-V with nested clips, exact straight-alpha composition and byte-exact scaled readback. V06 stages RGBA8/A8 bitmaps into a bounded owned atlas; the native NexaCloud scene runs on X11/Wayland. V07 has instanced batching and a three-slot upload ring with native lifetime/pixel checks; V08 GPU timestamps and physical hardware acceptance remain open. |
| F4 | In progress | X01-X02 have initial backend-independent semantic tree slices. T01-T08 have SFNT directory, horizontal metrics, Unicode `cmap` 4/12, simple/composite TrueType outlines, an unhinted A8 rasterizer, a bounded per-glyph LRU mask cache and an ordered cmap-based font fallback collection; Arial A and é masks render locally. T09 has pinned Unicode 18.0.0 source data and reproducible grapheme, word, line, general-category, East-Asian-width, emoji, Indic, bidi, paired-bracket, script and default-ignorable property tables. T10 has default grapheme, word and line boundary APIs passing 853, 1944 and 19346 official samples respectively. T11 has single-line paragraph bidi levels and visual ordering passing 91,707 character cases and 770,241 property-mode cases. T12 has an initial logical-order shaping run API with cluster-aware font fallback, classic horizontal `kern` format-0 adjustments, and nonspacing mark centering, covered by synthetic mixed-script tests. T13 has a bounded GSUB subset for type-1 single and type-4 ligature substitutions with synthetic tests and an optional fuzz target. T14 has bounded GPOS type-1 single and type-2 pair positioning (glyph and class formats), synthetic tests and an optional fuzz target. T15 has an initial multiline layout object with Unicode wrap opportunities, hard breaks, glyph positions, grapheme carets and selection geometry, covered by synthetic golden tests. Line-local reshaping updates changed glyph sequences and following line offsets. A context draw call rasterizes positioned text into A8 commands with frame-owned mask copies; a synthetic font test and `nexaui_text_headless` example cover the path. Fractional-ppem drawing, atlas packing and native text rendering remain pending. Composite phantom-point alignment, CFF outlines, GPOS mark attachment and advanced shaping, native accessibility bridges, action dispatch and complete widgets remain pending. |
| F5 | Planned | Tool-grade UI and release acceptance remain pending. |

T16 now has an initial contiguous UTF-8 editing buffer with grapheme-boundary selection and deletion, default word and hard-line navigation, and a byte-budgeted undo/redo operation log. A focused-editor helper consumes backend-neutral key/text events, and a selected-byte view supports the Win32 plain-text clipboard path. Synthetic editing tests include a context event-stream round trip. The multiline text widget adds visual Up/Down navigation with a preferred horizontal position and Ctrl+C/X/V through optional clipboard callbacks. Its focused caret can position the Windows-managed IME UI. A single-line password field uses a no-history sensitive buffer, masks graphemes and omits semantic values; NexaUI-owned text copies are overwritten before release. macOS preedit, external secret-copy control, interactive Wayland clipboard acceptance and scalable storage remain pending.

T17 now includes Wayland text-input-v3 surrounding-text exchange for focused ordinary text fields. The adapter advertises a UTF-8 slice of at most 4000 bytes containing the selection, checks deletion bounds and scalar boundaries after `done`, and queues the deletion before committed text. The field checks its ID and text/selection revision before applying the edit; password fields are excluded. A private compositor test covers the wire exchange and text-buffer tests cover deletion, malformed byte positions and undo. A real installed IME and macOS preedit still need acceptance.

T18 has a reproducible text stress and fuzz report in `docs/quality/text-fuzz-report.md`: Clang ASan/UBSan ran 10,000 inputs each for SFNT, GSUB, GPOS and UTF-8/editing without a finding, and a follow-up 10,000-input layout campaign found and then verified a fix for U+001C–U+001E paragraph separators. A Release benchmark records 4–128 KiB beginning-of-buffer edit costs. The seed inventory contains characters from 21 of 175 assigned Unicode scripts. Q06 now has initial corpora and campaigns for font, UTF-8, editing and layout; complex shaping, large fonts, visual glyph quality and 154 absent scripts remain known gaps.

Q07 now has optional manual Windows/MSVC ASan jobs for static/shared builds and five bounded Clang fuzz jobs. An isolated runner records seed/executable hashes, logs and failures without mutating source corpora. Local MSVC RelWithDebInfo ASan suites each passed 81 tests and skipped the occupied clipboard case; all five Clang targets ran 10,000 inputs each. Deliberate heap-overflow probes were detected on both toolchains, runner failure/timeout tests pass, and Actionlint validates the workflow. `docs/quality/sanitizers.md` records commands and limits; hosted CI, macOS sanitizers, ThreadSanitizer and the skipped clipboard case remain unverified.

The Wayland text-input-v1 fallback now exposes bounded UTF-8 preedit and byte-cursor state through the same copy-out API. It rejects stale serials, clears composition on commit or focus leave, and uses the existing transient text-field paint path. It also advertises bounded surrounding text and applies valid cursor-relative deletion with the following commit. A private v1 compositor test covers the wire requests, preedit and commit event sequence, stale-serial rejection, valid and malformed deletions, and text-field editing; a WSLg test checks text-field drawing through the callback bridge. Installed-IME acceptance and macOS composition remain open.

The Windows IMM adapter now copies UTF-16 `GCS_COMPSTR` into the same UTF-8 preedit API, converts the IME cursor to a byte offset, and clears composition on end, result, focus loss or editor deactivation. Focused ordinary text fields draw it in place; other fields retain the default IME composition UI, and candidate UI stays native. A Windows native test covers supplementary characters, cursor boundaries, field drawing without draft mutation, focus withdrawal and the existing Unicode commit path in static and shared builds. An installed non-Latin IME walkthrough, clause styling and macOS preedit remain open.

The X11 adapter now opts focused ordinary editors into a separate callback-style XIM context when that style is advertised, while the earlier context remains available for other fields. It converts XIM character-indexed changes and caret positions to bounded UTF-8 preedit with byte offsets, publishes redraw events and clears composition on focus loss or deactivation. Synthetic callback-data tests run inside a native Xvfb window and cover wide and multibyte text, replacement, malformed data and copy-out. Installed-IME acceptance, feedback styling and macOS preedit remain open.

Wayland v1/v3 now advertise sensitive-data and password content purpose for a focused password field. The field sends an empty surrounding-text snapshot with editor identity, masks preedit in its visual composition, accepts IME commits into its sensitive buffer and wipes NexaUI-owned composition scratch. Private v1/v3 server tests check content hints, zero surrounding bytes and restoration of normal purpose; a WSLg callback test checks masked preedit and committed Unicode. Installed-IME behavior and external IME data handling remain unverified.

U01 has an initial custom widget registration API that binds a semantic node to an optional interactive region under one stable ID. A headless button and progress example exercises layout, theme drawing, pointer input and a semantic value. Built-in widgets and native accessibility action dispatch remain pending.

U02 has initial label, image and separator display calls with semantic nodes, plus fixed layout spacers. Synthetic text rendering and headless image/separator pixels are tested. Intrinsic text sizing in the layout tree, image decoding and richer display styling remain pending.

U03 has initial theme-drawn button, checkbox, radio and toggle calls. Pointer and keyboard activation, disabled behavior, radio-group state, rendering and allocation-failure rollback have synthetic tests. Text placement remains caller-controlled; icon buttons and richer styling remain pending.

U04 has initial horizontal slider and drag-value controls with caller-owned doubles, capture movement across bounds, keyboard range changes, clamping, snapping and semantic value strings. A locale-independent decimal parser and text-buffer event bridge keep incomplete drafts editable and commit valid values on Enter. A rendered numeric field draws clipped text, selection and caret, scrolls horizontally to keep the caret visible, supports pointer caret placement and reports invalid drafts semantically. Boundary, rendering and allocation-failure tests run headlessly. Vertical controls, IME and richer value styling remain pending.

U05 has an initial single-line and multiline UTF-8 text field with owned draft and undo history, wrapping, pointer caret placement, selection and caret drawing, visual Up/Down navigation, Enter submission or line insertion, same-frame validation state and semantic value. Focused carets remain visible through horizontal or vertical scroll offsets. Ctrl+C/X/V use an optional clipboard interface with a native-window adapter. An optional native-window pointer positions the Windows-managed IME UI at the drawn caret. A single-line password constructor creates a no-history sensitive buffer; its matching descriptor draws one mask glyph per grapheme, keeps pointer offsets aligned to UTF-8 boundaries, suppresses copy/cut and exposes protected empty semantics. Headless tests cover editing, validation, selection across more than 16 lines, keyboard navigation, caret scrolling, clipboard shortcuts and password presentation; a native test checks IME placement and Unicode commit. Wheel/touch scrolling, macOS preedit, external secret-copy control and native accessibility action dispatch remain pending.

U06 has initial painted panels and groups, collapsible sections with focusable headers, and selectable tabs with tab panels. The caller arranges child geometry and registers semantic children after their container. Headless tests cover nested layout geometry, parent-child semantics, pointer and keyboard activation, same-frame tab selection and allocation-failure retry. Tab-strip arrow navigation, automatic layout integration and animation remain pending.

U07 has a scroll area with clipped child regions, nested wheel routing, clamped offsets, optional draggable and keyboard-focusable scrollbar thumbs, and single-contact touch drag routed to the innermost area. Short touch taps activate and focus the original child target without scrolling. The Windows adapter maps `WM_POINTER` touch input to neutral events. A Fenwick-based virtual list height index answers offset and visible-range queries in logarithmic time and caps overscan at 256 rows per side. A transactional measured-row callback rebuilds changed sets with stable keys and preserves the visible key's scroll position across insertions and reordering. A budgeted visible-row visitor supplies stable IDs and positions to application callbacks; an integration test registers only those rows as widgets. Tests cover one million logical rows, variable-height updates, anchoring and missing-key fallback, nested scroll areas, layout integration, thumb and keyboard input, synthetic touch routing, taps, cancellation, disabled state and allocation-failure retry. Native hardware touch, inertia and control-specific touch drags remain pending.

P08 includes `examples/05_scroll`: a headless 10,000-row variable-height list that builds only visible row widgets, routes a wheel event and writes a PPM frame. The example builds and runs in local Windows static and shared Debug configurations.

P08 now also includes the canonical `examples/00_hello_window`: it reports client geometry and backing scale, logs normalized pointer/key/text input, and runs a bounded hidden-window smoke test. Win32, Wayland and X11 window backends are available; Cocoa and native rendering outside Win32 remain pending.

P08 now includes `examples/01_hello_ui`: a native Windows window displays a command stream from one UI context, with a semantic greeting and optional TrueType labels. Static and shared Debug smoke runs submit two D3D12 frames, and the font mode writes a visually reviewed headless preview. Other native backends remain pending.

P08 now also includes `examples/02_buttons_and_input`: a native Windows card with a button, checkbox and visible count. The hidden smoke mode sends Win32 pointer and Space messages for both controls, verifies the UI model and semantic states, submits six D3D12 frames and saves a font-rendered preview. Other native backends remain pending.

P08 now also includes `examples/03_layout`: one layout tree is measured and arranged for 720×400 and 360×620. The headless example places panels and a button from computed rectangles, checks containment, orientation and semantic parentage, and saves both reviewed frames. Non-Windows native backends remain pending.

P08 now also includes `examples/04_forms`: two validated fields, a confirmation checkbox, a disabled-until-ready submit button and an accessible status are exercised through invalid and submitted states. Two headless frames have been visually reviewed, and static and shared Debug example tests pass. The canonical progressive paths 00-05 are present; non-Windows native backends remain pending.

P09 now includes `examples/06_tables`: it clicks a sortable header twice, verifies stable descending source-index order and semantic row placement, and writes a headless table frame. Optional TrueType input adds visible labels. Local Windows static and shared Debug example tests pass. The other progressive examples and cross-platform runs remain pending.

P09 also includes `examples/07_text_editing`: a local-font light-theme form that verifies invalid and valid drafts, submission, multiline text insertion, undo/redo and semantic values before rendering a PPM frame. The example tests run in local Windows static and shared Debug builds with Arial; the remaining progressive examples and cross-platform runs are pending.

P09 now includes `examples/08_themes`: selector keyboard and pointer input changes a caller-owned preset, which the application applies between frames. Dark, Light and High Contrast PPM frames and scoped accent restoration are checked locally. Visual review found and corrected the built-in button's High Contrast label color; a portable control test now checks its accent-text tint in all three presets. Other required examples and cross-platform runs remain pending.

P09 now also includes the canonical `examples/09_custom_widget` path. It lays out a custom action button and progress display, checks semantic values after pointer and Space activation, and writes four headless frames. Its local Windows static and shared Debug example tests pass. Native accessibility dispatch and cross-platform runtime checks remain pending.

P10 now includes `examples/10_docking_editor`: it maps two caller-owned dock stacks to tab widgets and a layout split, moves Assets through a toolbar action, drags the divider, renders the final arrangement and verifies an encoded/decoded v1 snapshot. Its local Windows static and shared Debug example tests pass. Visual drop targeting, native walkthrough and cross-platform runtime checks remain pending.

P10 now also includes the Windows-only `examples/11_multi_window`: two separate contexts route input from their own native windows and submit independent command streams through separate D3D12 drivers. Its hidden smoke mode checks key isolation and rendering after one window closes. Static and shared Windows Debug suites pass; a visible walkthrough remains pending.

P10 includes `examples/12_accessibility`: it runs keyboard traversal and activation through headless controls, verifies the parent-first semantic tree and prints roles, names, focus, checked and disabled states. Local Windows static and shared Debug example tests pass. Native UIA/AX/AT-SPI bridges and screenreader acceptance remain pending.

P11 now includes `examples/13_realtime_plot`: a deterministic producer appends 60 points per frame to a 2,048-point ring and moves the visible time window. It writes early, late and cursor PPM frames and checks wraparound order, distinct rendered frames, cursor input and semantic focus. Local Windows static and shared Debug example tests pass; a live native dashboard and cross-platform runtime checks remain pending.

P11 now also includes `examples/14_simulation_dashboard`: a worker receives a bounded command, publishes 16 immutable oscillator snapshots and posts a completion while the UI holds a pinned earlier view. The example renders early and late plot/telemetry/profiler frames, checks ownership and semantics, and supports optional font labels. Local Windows static and shared Debug runs pass in font-free and Arial modes; the displayed timing values are illustrative. Native continuous updates and macOS/Linux runtime checks remain pending.

P11 also includes the existing `examples/15_3d_viewport_shell`: it renders an application-owned wireframe cube surface at 1× and 2× and writes a focused PPM frame. The example now has a direct CTest run and copies the runtime DLL beside the executable in Windows shared builds. Cross-platform runtime and native GPU surface import remain pending.

P12 has an initial Windows `examples/showcase` runtime: one native D3D12 window combines a fixed-step oscillator, live plot, scene tree, editable gain property, telemetry, toolbar and status. A hidden integration run checks pointer and keyboard control actions, semantic selection, D3D12 submissions and wide-to-compact resize; both preview frames were visually reviewed. Static and shared Debug example tests pass. A visible manual walkthrough, OS-level accessibility inspection, cross-platform native backends and deeper simulation-worker integration remain pending.

U08 has an initial table header API with sortable column semantics, pointer and keyboard width changes, and painted row/cell containers. A stable index sort keeps equal-key source order in either direction. Tests cover sort order, header activation, resizing, semantic nesting, headless rendering and allocation-failure retry. Column reordering, selection, pinned columns and a table-owned row data model remain pending.

U09 has an initial preorder tree flattener and selectable tree-item widget with a separate expander, pointer and keyboard activation, same-frame semantic selection updates, and composition inside a table cell. Tests cover visibility, invalid ancestry, expansion, selection, tree-table nesting, rendering and allocation-failure retry. Drag reordering, multi-selection, lazy child loading and a tree-table-owned data model remain pending.

U10 has initial popup, modal and tooltip APIs. Overlays own a backdrop and restrict pointer, wheel and keyboard focus to their scope across frames; outside click, Escape or programmatic close restores the previous focus when available. Tests cover forward/reverse Tab wrapping, underlay blocking, dismissal, immediate background input after close, headless drawing and allocation-failure retry. Nested overlays, platform-specific popup windows and native assistive action dispatch remain pending.

U11 has initial semantic menu-bar, menu, toolbar and status-bar containers. Menu items navigate among enabled siblings with directional keys and Home/End, report activation, and can toggle a caller-owned checked state after painting. A popup menu restores focus to its menu-bar trigger on Escape. Headless tests cover hierarchy, navigation, disabled siblings, painting and allocation-failure retry. Accelerators, nested popup submenus and native menu bridges remain pending.

U12 has an initial interactive split divider and a caller-buffer docking tree. Pointer drags and axis-aware keyboard commands update a caller-owned ratio, with semantic value exposure. Dock stacks support adding/selecting tabs, splitting, and moving tabs without leaving an empty stack. A little-endian v1 snapshot round-trips and rejects malformed data. Tests cover a drag through the layout engine, keyboard handling, model operations and serialization. Visual drop targeting, docking previews and native accessibility action dispatch remain pending.

U13 has initial command-palette and searchable-select popup widgets around the existing overlay and text field. Stable ASCII-folded substring filtering preserves source order, disabled results are skipped by keyboard navigation, and Enter or pointer click returns an activation ID. The searchable select commits its selected ID while the command palette leaves dispatch to the application. A scrollable result list, named empty state, automatic label shaping and composite retry path are covered by headless tests. Unicode case folding, result ranking, asynchronous providers and a built-in collapsed select trigger remain pending.

U14 has initial sRGB/HSV and hex conversion APIs, a saturation/value, hue and alpha color picker, and a three-preset theme selector. The color picker retains hue through achromatic colors and commits caller-owned state after successful painting; the theme selector exposes selected radio semantics and lets the application apply a preset between frames. Headless tests cover conversion boundaries, pointer/keyboard and disabled states, theme switching and allocation-failure retry. A hex text editor, gamut-aware color spaces and native assistive action dispatch remain pending.

U15 has an initial window-to-context event transfer API. Win32 pumps all windows on one thread; Wayland and X11 poll each window's display connection. Each window retains its own queue, and each context owns its own focus and pointer state. A Win32 two-window integration test exercises message routing, focus loss, allocation-failure retry and continued operation after one window closes. A Linux/X11 native test exercises two separate windows and their input queues. Cocoa routing, Wayland and X11 context-level routing acceptance, and native semantic-root associations remain pending.

U16 has an initial caller-owned scalar transition API with per-frame timing, linear and smoothstep easing, retargeting from the current value, explicit cancellation and reduced-motion snapping. An optional toggle transition moves only its thumb while checked semantics update immediately and painting failure rolls motion back. Headless tests cover interruption, repeated calls within a frame, independent contexts, visual progress, semantic values and toggle state. The Windows window adapter reads the native client-area animation preference. Other built-in widget transitions, macOS/Linux preference plumbing and native assistive acceptance remain pending.

U17 has an initial paint-only rich-text path over existing shaped layouts. Disjoint spans select foreground/background colors and underline/strike by UTF-8 byte range; boundaries are validated against grapheme and shaped-glyph boundaries. A rich label exposes the full source as one semantic text node, and failures roll back commands, masks and semantic insertion. Synthetic layout tests cover styling, wrapping, invalid grapheme/ligature cuts, rendering and retry. Style-specific font metrics, links, inline objects and mixed-style shaping remain pending.

U18 has a headless widget integration regression. One layout-arranged panel combines button, checkbox, toggle and slider across pointer activations. Six reviewed PPM baselines cover idle and active states in light, dark and high-contrast themes; the test compares every pixel, checks semantic state and verifies a one-pixel counterexample is rejected. A Windows D3D12 integration test compares actual widget-generated commands and rasterized button text with headless pixels in idle and active frames for all three themes. Broader widget scenes and macOS/Linux native runs remain pending.

S01 has a two-slot, single-producer/single-consumer snapshot exchange with read-only pinned views, no payload copies, off-lock disposal and explicit shutdown. Windows thread tests cover held-view lifetime across 20,000 publications, concurrent reads, exactly-once disposal, stale handles and allocation failure. A thread-sanitizer run and native macOS/Linux runs remain pending.

S02 has an initial fixed-capacity command/completion channel. Every accepted UI command reserves a completion slot; workers may finish tickets out of order, and full capacity returns explicit backpressure without taking payload ownership. Tests cover reversed completions, queued-payload cleanup, allocation failure and a two-worker 10,000-command exchange on Windows. Large-capacity ticket lookup optimization, native macOS/Linux runs and thread-sanitizer validation remain pending.

S03 has a callback-backed borrowed data-view API for finite, nondecreasing time series. Inclusive visible-range binary search and per-pixel envelopes preserve first/last and y-extreme points with source indices. Tests cover spikes, duplicate timestamps, sparse and empty windows, invalid input and narrow-range reads from ten million logical points. A one-million-point Release benchmark records full-range and narrow-window costs on Windows. NaN gaps, persistent indexing and native macOS/Linux runs remain pending.

S04 has an initial single-series real-time plot widget with caller-owned viewport and envelope buffer, theme-aware axes/grid, optional numeric tick labels, a pointer/keyboard cursor and a named semantic node. Three reviewed headless theme baselines plus interaction and allocation-failure rollback tests run on Windows. Native accessibility bridge behavior, multi-series/legend support, axis formatting, runtime GPU output and native macOS/Linux review remain pending.

S05 has initial telemetry and profiler panels over borrowed arrays. They read only geometry-visible rows, display theme-colored metric and timing bars, and expose names, numeric ranges, statuses, inclusive/self time and calls in the semantic tree. Tests exercise empty panels, malformed visible records with retry, three reviewed theme baselines and 10,000 frame updates over 100,000 logical records without new Context allocations after warmup on Windows. A Windows Arial preview checked normal and 200% text sizing. Native screen-reader and GPU paths, multi-panel dashboard composition and macOS/Linux review remain pending.

S06 has an initial typed property row for boolean, ranged number and text values, composed with the U09 tree for a hierarchy inspector. It reports toggle/step proposals or an external-editor request without changing snapshot values, updates same-frame semantics, and rolls back on allocation failure. A headless tool example selects a Light entity, proposes an intensity change and renders the accepted value. Windows MSVC tests and a reviewed Arial image cover this slice. Inline property editors, native assistive action dispatch, virtualized large hierarchies and macOS/Linux review remain pending.

S07 has an initial software viewport surface contract. An application producer supplies RGBA8 pixels through resize/acquire/release callbacks, and NexaUI keeps a single bitmap lease in flight until rendering completes. Tests cover physical scale changes, zero-size suspension, headless output, callback failures, allocation-failure retry and teardown. A headless 3D cube shell exercises 1× and 2× surfaces. Direct GPU texture import, native renderer synchronization and native platform review remain pending.

F11 has an initial internal renderer driver over the backend-neutral command stream. One shared preflight validates every command before headless painting or backend resize/submit. The driver caches physical size and scale, suspends zero-size targets, retries temporary surface loss and latches device loss until recovery. A mock backend forwards commands into the headless renderer and tests ordering, resize and failure transitions. D3D12 now exercises the full current core command stream, nested clips and forced device removal/recovery through the driver. Vulkan now exercises clear/present, opaque rectangles/triangles, surface rebuild and retryable retirement through F11; Metal lifecycle tests remain pending.

D01 has an initial Windows D3D12 device, direct command queue, flip-discard swap chain, RTV heap, command allocators/list and fence. It clears and presents from a hidden native test window, resizes after a Win32 size change, and exposes the renderer's pixel dimensions and submitted-frame count to that test. The F11 adapter accepts empty command frames. D06 later expanded the swap chain to three buffers and added a bounded two-frame path for opaque and empty submissions.

D02 has an initial device-loss and resource-recreation path. Failed D3D12/DXGI calls and the removed-fence sentinel latch loss; the F11 adapter returns `NX_RENDER_DEVICE_LOST` until explicit recovery rebuilds the factory, device, queue, swap chain, render targets, descriptor heap, allocator/list and fence. The local Windows smoke test forced real device removal twice, once through the direct API and once through F11, and successfully submitted after each rebuild in static and shared builds. A test-only partial-build failure confirms that F11 remains lost and a second recovery succeeds. Cross-adapter/hot-plug behavior has not been forced in a test; D08 still needs broader resize/DPI/device-loss integration.

D03 has an HLSL vertex/pixel pipeline with a triangle PSO. The F11 adapter uploads filled, stroked and rounded rectangle, triangle and line vertices in paint order and applies the target backing scale. A shape pixel shader enforces round caps and rounded corners. Its private test hook copies the GPU back buffer to CPU memory before presentation; Windows tests compare every RGBA8 byte against headless output at 1× and 2×, with fractional/offscreen geometry and layered half-alpha shapes. D06 later replaced per-frame vertex allocations with a reusable upload ring.

D04 supports nested `PUSH_CLIP`/`POP_CLIP` commands with pixel-center scissor conversion. Partially transparent primitives use a copied back buffer and integer RGBA8 composite shader to match headless straight-alpha results on transparent targets. The native readback test checks nested clips at 1×/2× and opaque, transparent and partially transparent target blending. D06 later limited copies to clipped shape bounds and batches adjacent non-overlapping translucent shapes. Bitmaps still have per-element transitions and draws.

D05 stages borrowed RGBA8 and A8 bitmap rows into D3D12 upload buffers, copies them into shader-readable textures and keeps resources alive until the frame fence completes. A bitmap pixel shader applies nearest-neighbor sampling, tint and integer straight-alpha compositing. GPU readback compares multiple images and a NexaUI-rasterized glyph mask with headless at 1×/2×, after source changes, resize and device recreation. The visible example includes an RGBA8 icon and A8 letter mask. D06 later replaced per-command texture and descriptor allocations with reusable storage.

D06 now deduplicates identical bitmap views per frame, stages distinct bitmaps in one reusable upload buffer, uses one growable shader-visible descriptor heap, and reuses textures from a pool capped at 256 entries and 64 MiB. Three back-buffer slots each own a command allocator and growable vertex upload ring. Opaque and empty submissions can have two frames in flight; a fence guards slot reuse and the oldest pending frame. Bitmap and translucent frames retain serial waits around their shared resources. Adjacent non-overlapping opaque or translucent rectangles, strokes and triangles batch into at most 64 primitives per draw. A translucent batch copies the union of its clipped shape regions once; overlapping shapes and clip changes flush it. Bitmaps and lines copy their clipped, expanded bounds individually. GPU pixel readback still matches headless for 100 disjoint translucent shapes, mixed shape and clip cases, 1,000 opaque rectangles and 128/260 distinct bitmaps; repeated frames check texture reuse and ring wraparound. A local 640×480 x64 Release benchmark records end-to-end and GPU samples in [benchmarks/results.md](benchmarks/results.md). Bitmap compositing and independent per-slot bitmap resources remain D06 work.

D07 now uses three GPU timestamp queries per frame slot and a fence-protected readback buffer for each slot. Private renderer diagnostics report the latest completed frame's texture-upload, render and total GPU time, plus queue frequency, draw/upload counts and copied texels. The Windows timing smoke test validates nonzero frequency, finite phase durations and their sum after a synchronized frame. This is an initial timing hook; cross-adapter timing characteristics and a broader performance profile remain unverified.

D08 has an initial Win32/F11 integration regression. A synthetic `WM_DPICHANGED` updates the window's logical size and 1.5× scale; the driver suspends a zero-size target without submission, resumes at the new scale, resizes the physical swap chain, and compares GPU pixels with headless output. Where `ID3D12Device5::RemoveDevice` is available, the same resized and scaled path is submitted again after forced device loss and recovery. The visible native example also calls driver recovery on device loss. A synthetic message does not validate an actual monitor move or OS DPI transition; those manual and cross-adapter checks remain open.

D03's native command set now also draws stroked and rounded rectangles and round-capped lines. A stroke is split into four non-overlapping strips so its translucent corners blend once; a stroke thicker than half either dimension fills the outer rectangle. Lines and rounded rectangles use a pixel shader that tests each pixel center against the segment or corner radius and composites through the existing previous-color texture. Pixel readback compares these commands against headless at 1× and 2×, including a zero-length line, 48 varied overlapping fractional lines and 32 overlapping rounded rectangles. This covers every current core drawing command on the Windows backend.

V01 now has a private Linux Vulkan device target with Vulkan 1.0 instance creation, bounded physical-device/extension enumeration, nonempty graphics-queue selection, discrete/integrated/virtual/other/CPU preference and per-object dispatch. Missing swapchain support is rejected when requested; failed adapter creation falls back except on allocation failure. Tests cover each NexaUI allocation failure, exact-size deallocation after shrinking enumerations, retry limits, owner-thread checks and independent objects. A native Mesa Lavapipe test submits real command buffers with bounded fence waits and repeats device creation/destruction under Khronos validation. V02 now adds the initial surface/presentation path described below; hardware GPU acceptance remains open, and this does not complete F3. See [the Vulkan contract](docs/concepts/vulkan.md).

V02 has initial Wayland and Xlib surfaces, actual surface-compatible graphics/present queue selection (including separate families), FIFO UNORM swapchains and a transfer clear/present path through F11. Per-window renderer leases suppress Wayland SHM background attachment while Vulkan owns the surface. Tests compare swapchain readback byte-for-byte with headless output for opaque/translucent clears and row padding, inspect displayed X11 pixels, exercise resize and synthetic Wayland 2× scale, explicit rebuild, allocation failures and injected driver state. The Linux native-clear example now runs on both backends. Mesa Lavapipe/WSLg validation passes; Broader Vulkan command drawing, physical GPU removal and hardware acceptance remain open. See [the Vulkan contract](docs/concepts/vulkan.md).

V03 now negotiates KHR/EXT swapchain maintenance and uses per-image presentation fences by default. Strict resize, recovery and teardown wait for acquisition, submission and pending presentations before releasing handles; a timeout retains the renderer and window lease for retry. Tests resume held acquired images, retain timed-out readback storage, clean partial rebuilds and distinguish unenqueued present OOM from enqueued out-of-date presentation. F11 close now returns a result and keeps the backend pointer on failure. Explicit compatibility/legacy modes retain the weaker unextended device-idle policy. Native fault coverage on Mesa Lavapipe is not physical GPU removal or cross-driver release acceptance.

V04 has a Vulkan 1.0 SPIR-V pipeline for core shapes. Authored shaders and validated embedded words require no runtime compiler. Pixel-center rectangle snapping and compensated analytic triangle edges match the tested headless output, including near-center fractional vertices, large offscreen cancellation, shared edges and 1×/1.5×/2× scale. Rounded fills, inside strokes and round-capped lines also use analytic coverage; tests include clamped/zero radii, disks, thin borders, exact circle boundaries, fractional coordinates and distant offscreen segments. Tests exercise growing/reused vertex storage, partial pipeline and upload OOM, submission timeouts, swapchain rebuild, recovery and independent windows under X11/Wayland. V05 extends this pipeline as described below. V06 textures are described below; hardware acceptance remains open.

V05 now intersects up to 64 nested clips and composites quantized straight-alpha paints in a shader over an owned canvas/history pair. Clipped-area copies preserve painter order; the final canvas copies into the swapchain. GPU readbacks match headless for transparent/translucent targets, alpha endpoints, shared edges and 96 overlapping mixed draws at 1×/1.5×/2×. X11 displayed pixels also match the reference. Tests cover host-record and GPU-upload OOM, exact allocator sizes, rebuild/recovery, retained readback after timeouts, retryable close and independent translucent windows. The native Linux example exercises clipping and alpha. A mock Wayland compositor regression also fixes queued new-id data-offer ownership during teardown; the corrected tests pass sanitizers. V06 and the native NexaCloud runner are described below; this does not complete F3 hardware acceptance.

V06 now stages per-frame deduplicated RGBA8 images and A8 glyph masks into an owned texture atlas. Packing checks both device axes, 4096 unique views and a 64 MiB texel budget; failed packs never present partial content. Host-coherent upload storage and atlas images grow/reuse only after submission retirement. Fault tests cover initial/growing view tables, texture and upload allocation, byte-exact tint/sampling/compositing, borrowed-source release with pending work, rebuild/recovery and retained close. The common NexaCloud scene now has an X11/Wayland native Vulkan runner with complete overview/file/compact GPU/reference comparisons. This is initial V06 support; V07 batching/rings, hardware performance, physical input/DPI and macOS remain open.

This plan does not imply that planned APIs or native backends already exist. Keep the table updated as tasks become verified.

## NexaCloud integration priority

The user-provided dashboard and file-browser designs now drive the next native
integration work. [The shared reference scene](examples/nexacloud/README.md)
and [reusable components](docs/concepts/cloud-components.md) include Midnight
styling, rounded cards, action variants, icons, progress and cached text stamps.
Windows has a runnable D3D12 version with native-input tests and GPU captures;
the shared scene also passes local Linux software-rendered workflows.
The full scene now runs through Vulkan on X11/Wayland. Continue with native
Cocoa/Metal and the remaining per-OS accessibility, DPI, large-text, performance
and release checks. [Delivery and acceptance scope](docs/nexacloud.md).

An initial AppKit window adapter now exists behind the same C ABI, with main-thread
ownership, per-window event/drop queues, point/backing-scale separation, native
close/focus/input and an initial NSTextInputClient composition path. Native
callbacks and ownership have authored macOS tests; their execution is pending.
Full-document IME/reconversion and Command editor actions remain M04 work, and
Metal/native NexaCloud rendering remains A01–A08 work. Windows/Linux build and
shared-input checks do not count as macOS validation. See [the AppKit report](docs/concepts/macos.md).

An initial A01/A02 Metal target now owns a main-thread device/queue, an AppKit
CAMetalLayer and a BGRA8 canvas. Clear/readback/presentation and retryable
retirement have authored native tests, with a plain C17 example. Pending GPU
work and timed-out readback retain their storage; drawable acquisition and
command retirement are bounded. Native SDK/runtime execution remains pending.
This is not command-stream or NexaCloud rendering: A03–A05 shapes, clips, alpha
and textures remain next, followed by batching/profiling and hardware acceptance.
See [the Metal contract](docs/concepts/metal.md).

Initial A03 opaque shapes and the scissor part of A04 now have an implementation.
MSL is compiled once by the OS; the plain C iterator has passing Windows and
Linux sanitizer/headless comparisons. Native GPU readback tests are authored
but have no execution evidence yet. Alpha composition and image/glyph uploads
remain A04/A05 work. Source embedding checks do not establish MSL compilation.

Native CI now supplies initial AppKit execution evidence: the dedicated window
test passes on static/shared AppleClang 21 builds at `addc4a6`, as do the common
text models and C hello-window example. An obsolete macOS-unsupported assertion
was the only failing core test (73/74 passed); its lifecycle branch now includes
Apple. The first Metal object also compiled at `aa42f01`; its test target's
missing core usage requirements are fixed. Follow-up full/native GPU results
remain required. Physical input, DPI, IME and assistive acceptance remain open.

At `083cf94`, Apple's static CI build compiled MSL and executed the opaque
readback comparisons on Apple Paravirtual device (36 submitted/retired frames).
The test then exposed saved-layer restoration, and the shared C example exposed
missing explicit Foundation linkage; both have corrections awaiting CI. Initial
A04 integer straight-alpha composition now has implementation and authored GPU
tests. Windows/Linux sanitizer CPU preparation comparisons pass with overlapping
alpha; GPU alpha and complete lifecycle validation still require the follow-up.

At `74aa8ec`, static/shared macOS CI passes 77/77 tests, including GPU alpha,
layer restoration, retirement/recovery and installed-package integration.
Initial A05 now implements a bounded RGBA8/A8 atlas, owned reusable staging,
nearest-cell sampling, integer tint and transactional growth. CPU preparation
and atlas tests pass on Windows and under Linux ASan/UBSan/leak detection. A
plain C17 AppKit/Metal NexaCloud runner and native overview/file/compact smoke
are authored; image/full-scene execution and visual review remain pending.

At `fb80bd0`, both macOS variants pass 79/79 tests, including the A05 image/glyph
path and native NexaCloud smoke. The actual 1024×642 overview/files and 820×645
compact GPU captures match headless and are byte-identical across static/shared
jobs. They are visually reviewed and published with artifact digests in the
native report. The virtual desktop limits wider/Retina acceptance. A native
compact-details modal capture with Escape/focus restoration is the next authored
case; A06 batching, A07 timing and physical platform acceptance remain open.

At `2f70d68`, all six static/shared OS core jobs and Linux sanitizers pass.
The macOS compact-details GPU capture, Escape dismissal and focus restoration
also pass, with four byte-identical static/shared captures. Ubuntu's active
validation counterexample and 20 untraced X11 lifecycle repetitions pass after
selecting the present-fence-aware tool; five Wayland display cases explicitly
skip in its Xvfb-only environment. This resolves the reproduced old-layer CI
failure without a production lifetime shortcut. Hardware, text/reflow,
assistive bridges, profiling and the remaining roadmap are still open.

The NexaCloud scene now has an initial 100/150/200% text preference and reflow:
measured wrapping and file columns, bounded previews, stacked narrow metadata,
scrollable navigation/dialogs, fixed modal close and focus reveal after text or
window changes. A separate text matrix covers six pages at three text sizes and
five window sizes, plus cache/resize, theme and keyboard workflows. Windows
D3D12 and Linux X11/Wayland Vulkan add native 200% captures; the new macOS case
passes at `e258f40` in both macOS variants (80/80 tests each), with three reviewed
200% GPU captures. This does not close physical display or assistive acceptance. See the
[reflow validation record](docs/nexacloud-validation.md#text-reflow--2026-10-15).

A06 measurement preparation now adds committed Metal submission counters and
separate CPU prepare/encode/drawable/commit intervals, plus cumulative retirement
polling time. The native NexaCloud smoke authors three workloads with four warmup
and 32 measured frames each, raw CSV artifacts, distribution summaries and a
post-run GPU/reference capture. Both macOS variants pass at `f870b29`; the
[preserved baseline](benchmarks/baselines/metal-f870b29/README.md) shows CPU
encoding and retirement costs on Apple's virtual device. The first bounded
change retains invariant state within a render pass and authors an in-process
A/B/B/A comparison. At `a364f2d`, both macOS suites pass 80/80; 768 measured
frames show 55.2–73.0% lower median CPU encode intervals on the virtual device,
with identical GPU output. The [comparison](benchmarks/baselines/metal-a364f2d/README.md)
also records wall-time/tail limitations. This does not complete A06
batching/upload rings or A07 GPU timing.

Initial A06 batching now has authored C17 record packing/group preparation and
a shared Metal draw-record buffer, with a 64 MiB capacity cap and transactional
growth. Each group preserves primitive order and exact per-record clips;
destination-reading images/alpha retain their history-copy boundaries. Windows
static/shared and Linux sanitizer CPU checks pass, including 10000 records.
At `c31c92e`, both macOS variants pass 81/81 including MSL, 10000-record GPU
comparison, staging lifetime and all A/B blocks. The [first measurement](benchmarks/baselines/metal-c31c92e/README.md)
shows 343/396/366 calls reduced to 28/30/9 with identical pixels, but identifies
CPU preparation overhead and inconsistent wall-time gains. A follow-up caches
validated preparation in a borrowed plan and fills storage in a second walk.
At `bbf3474`, all six desktop variants and Linux sanitizers pass; both macOS
suites pass 81/81. The [follow-up measurement](benchmarks/baselines/metal-bbf3474/README.md)
retains 768 samples and 24 byte-identical GPU captures. In-run median CPU
prepare+encode decreases by 7.1–59.8% against the unbatched path, with substantial
block drift and inconsistent wall-time tails. Concurrent upload rings and A07
remain open; this does not establish physical Release/FPS acceptance.

A06 now authors three fixed Metal frame slots with independent record buffers,
bitmap staging and atlases. The full ring retires the oldest submission before
reuse; resize/recovery/close drain all slots within one polling budget. New
native cases cover distinct all-frame GPU captures after source release,
wrap/reuse, busy-peer allocation failure, bounded full-ring/resize/recovery and
three-slot close. Profiling compares serial/ring/ring/serial with batching fixed
on, including per-slot/capacity diagnostics. Host C17 compilation and Windows /
Linux sanitizer preparation tests pass. At `a7c907b`, all six desktop variants
and Linux sanitizers pass; both macOS variants execute 81/81 tests. The
[ring measurement](benchmarks/baselines/metal-a7c907b/README.md) preserves 768
samples and 24 byte-identical mode/block GPU captures. Shorter retirement polls
mostly shift wait into drawable acquisition, with mixed CPU/wall-time outcomes.
A test-only shared-event GPU gate strengthens pending-source and real-timeout
coverage. At `29eb9c1`, both native macOS suites again pass 81/81: actual gated
GPU work preserves all three slots and caller output across a one-second timeout;
after CPU signaling, all captured frames match headless. The final native logs
record 100 submitted/100 retired frames; all 24 workload images remain byte-equal.
A06 physical performance/hardware acceptance and A07 remain open.

A07 now authors completed-command-buffer GPU timestamps and a separate CPU-driver
scheduling pair. A fixed 64-entry journal keys pending/ready/unavailable/failed
results by submission ID, slot and canvas size. Cached lookup does not wait;
bounded idle collection gathers the three-slot tail, and successful recovery
clears stale records. Portable finite-value/history tests pass on Windows and
under Linux sanitizers. Native cases and workload CSVs require actual valid GPU
times in macOS CI, with per-submission joining after each measured block.
At `e66d5af`, all six desktop variants and Linux sanitizers pass. Both macOS
variants pass 82/82, with 64/64 retained GPU timings in each native journal test
and 768/768 valid GPU/kernel workload pairs joined to their CPU submissions.
All 24 mode/block GPU images remain byte-identical. The
[preserved GPU timing baseline](benchmarks/baselines/metal-e66d5af/README.md)
records distributions and limits. A07 now has native virtual-device smoke
evidence; physical GPU timing, Release and broader A08 acceptance remain open.

V07 now authors one 84-byte instanced upload per visible quad in place of six
60-byte vertices. Ordered groups combine a destination-reading first instance
and subsequent opaque instances, preserving each history copy and exact shader
clip. Initial X11/Wayland draw suites pass with synchronization validation,
including 10000 opaque instances in one draw. CPU submission profiling and an
explicit native `--profile` mode retain a 192-frame
[baseline](benchmarks/baselines/vulkan-19c07ba/README.md); performance workloads
are separate from correctness/validation smokes. At `d4a2508`, all six desktop CI
variants and Linux sanitizers pass. Local shared and sanitizer runs each execute
six X11/Wayland native cases with synchronization validation. The
[controlled comparison](benchmarks/baselines/vulkan-d4a2508/README.md) preserves
1536 samples and 48 byte-identical mode/block captures, with held input and
geometry-drift rejection. Upload bytes fall by 76.7% and draw calls by 64.1–97.0%;
CPU/tail results remain mixed. Concurrent upload rings, V08 GPU timing and
physical hardware acceptance remain open.

V07 now adds three private Vulkan upload/atlas/descriptor/command/fence slots.
Only the selected slot retires before CPU reuse; GPU barriers order the shared
canvas/history. Geometry/staging/atlas payload capacity is bounded independently
at 64 MiB per slot, with failure-safe growth and aggregate resize/recovery/close
retirement. The new test-only timeline gate holds three actual GPU submissions,
checks freed source storage and real bounded timeouts, then compares captured
frames independently. X11/Wayland shared and Clang sanitizer runs each pass eight
native cases with synchronization validation. The
[ring measurement](benchmarks/baselines/vulkan-1cdf550/README.md) retains 768
serial/ring/ring/serial frames and 24 equal images with unchanged prior scene
pixels. Software-driver wall medians rise slightly; this is no speedup claim.
Both Linux and Windows CI variants, Linux sanitizers and static macOS pass at
`1cdf550`; shared macOS is queued. V08 GPU timestamps and physical acceptance
remain open.
