# NexaCloud reference UI

A native NexaUI example based on the supplied NexaCloud dashboard and file-browser
designs. The shared C17 scene contains the interface and local demonstration
state. It has no cloud server connection and never uploads, deletes or shares
real user files. The generated landscape thumbnails are original sample artwork.

The overview contains navigation, storage/CPU/network/system cards, a two-series
rate chart, transfers, activity and quick actions. The file browser contains
search, grid/list views, file selection, a scrollable inspector, description
editing, favorites, local folder creation and reversible example-file deletion.
Upload, share and preview actions open explicit demonstration dialogs.

The reusable [component APIs](../../docs/concepts/cloud-components.md) are part
of NexaUI itself: Midnight theme, rounded surfaces/cards, vector icons, action
variants, progress displays and immutable cached text stamps. App data stays in
`scene.c`; platform window/renderer code stays in the native runner.

## Run

Build with the default `NEXAUI_BUILD_EXAMPLES=ON`. Supply a local TrueType font;
font files are not redistributed. For example, on Windows:

```powershell
cmake -S . -B build
cmake --build build --config Debug --target nexaui_nexacloud_native
.\build\examples\nexacloud\Debug\nexaui_nexacloud_native.exe C:\Windows\Fonts\segoeui.ttf
```

The native Windows runner uses a real Win32 window and the existing D3D12 driver.
The OS supplies window decorations and backing scale. The shared scene does not
draw platform-specific window buttons.

![NexaCloud file browser, actual D3D12 back-buffer capture](../../docs/images/nexacloud-files.png)

The portable workflow writes PPM previews and exercises the same normalized
pointer/key input as the native adapters:

```sh
./build/examples/nexacloud/nexaui_nexacloud --smoke /path/to/font.ttf /tmp/nexacloud
```

On a Windows multi-configuration build the executable is inside `Debug`.
`ctest -R nexaui_nexacloud --output-on-failure` runs available checks when an
example font is found. Set `NEXAUI_EXAMPLE_FONT` explicitly on systems where the
default font paths are unavailable. With tests enabled, the Windows native runner
also accepts `--smoke FONT.ttf OUTPUT_PREFIX`; its `native-*` captures are actual
D3D12 back-buffer readbacks, rather than software-rendered approximations.

## Interaction and layout

- Tab/Shift+Tab move focus; Enter/Space activate named actions and file tiles.
- Arrow keys move among visible file tiles and scroll the destination into view.
- Ctrl+F focuses search on Windows/Linux; the shared macOS mapping uses Command+F.
- Delete moves the selected example file to the example trash while a file tile
  has focus. The trash toolbar restores a selected file.
- Escape closes a dialog and restores its trigger's focus. The detail button opens
  a scrollable dialog when an inline inspector does not fit.
- The sidebar becomes an icon rail on narrow windows; grid column count adapts,
  long names remain available in semantic labels/details, and wide content is
  capped to avoid stretching cards across large desktops. Target minimum size
  is 640×480 logical pixels. Very small windows show a resize hint.
- Settings switch between Midnight and Light during runtime. Reduced motion is
  an explicit application preference; these scenes currently use direct changes.

The sample search performs ASCII case folding and byte matching for non-ASCII
text. It is not a complete locale-aware cloud search implementation.

## Verification and remaining platform work

The portable smoke workflow checks navigation, keyboard file selection, deletion,
folder creation, empty search, compact layout, dialog Escape/focus return and
theme changes. The Windows native workflow sends real Win32 mouse/key messages,
checks selection semantics after routing, resizes the actual window and reads
back D3D12 frames. Component tests cover keyboard/disabled state, semantic
progress, rounded geometry and allocation rollback. Text-stamp tests cover
pixel output, font-independent ownership and exact-size cleanup at every
injected creation failure.

| Platform | Current evidence | Remaining native work |
| --- | --- | --- |
| Windows | Local MSVC Debug native D3D12 smoke and visual back-buffer review | Physical HiDPI monitor changes, native screenreader and release acceptance |
| Linux | Local Alpine/WSL portable scene, component and Vulkan clip/alpha tests | Remaining Vulkan shapes and bitmap/glyph textures before this full scene runs natively |
| macOS | CI static/shared core, portable scene and installed-package jobs passed in `cffbcf8` | Cocoa window/input, Metal rendering, actual native runtime acceptance |

Large text at 200%, physical 4K/HiDPI display behavior and native UIA/AX/AT-SPI
screenreader tasks are still open. A successful portable render does not establish
native support for an operating system. See the [NexaCloud delivery plan](../../docs/nexacloud.md).
