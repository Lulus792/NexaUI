# Core dependency boundary

## Minimal Wayland software build for offline hosts

A graphical recovery host can choose a small local build without the X11 or
Vulkan adapters, and can explicitly omit the optional AT-SPI transport:

```sh
cmake -S /path/to/NexaUI -B build-recovery \
  -DNEXAUI_ENABLE_X11=OFF -DNEXAUI_BUILD_VULKAN=OFF \
  -DNEXAUI_ENABLE_ATSPI=OFF -DCMAKE_INSTALL_PREFIX=/path/to/local-sdk
cmake --build build-recovery
cmake --install build-recovery
```

Select `NEXAUI_SETTINGS_SOFTWARE=ON` when building the standalone Settings
example against this SDK. `BUILD_SHARED_LIBS` selects shared or static core.
The application must provide its own local font and a working Wayland
compositor/session. Build prerequisites include a C17 toolchain, CMake,
pkg-config, Wayland client and protocol development files, wayland-scanner and
xkbcommon. Runtime dependencies include the system C library, Wayland client
and xkbcommon and their platform-specific dependencies. Inspect the actual
artifacts and platform licenses for the target image; this is not a complete
distribution manifest. No network service is required by the Settings model.

With `NEXAUI_ENABLE_ATSPI=OFF` the native accessibility transport is unavailable;
this profile cannot provide screenreader access. Keep AT-SPI enabled where the
host can provide it, and provide a separate usable recovery path when required
accessibility or the graphical session is unavailable. The application owns
all recovery operations and must not infer completion from UI state. The Settings
example demonstrates local choices and an in-memory operation, not installer
target selection, persistent progress, recovery authorization or disk changes.

NexaUI's core runtime uses its own C17 code and the C standard library.
Platform adapters may link OS or GPU-driver libraries behind the public C API.
Windows window/input adapters use the OS `user32`, `shell32` and `imm32` libraries.
The initial native accessibility bridge additionally uses `ole32` for COM and
free-threaded marshalling, `oleaut32` for BSTR/SAFEARRAY ownership,
`uiautomationcore` for UIA transport, and `uuid` for Windows SDK interface IDs.
Those links are allowed only on Windows; layout, widgets and rendering remain
NexaUI code. Non-Apple Unix uses `m`; POSIX adapters use CMake's
`Threads::Threads` system-thread target.
There is no third-party UI, windowing, rendering, layout, shaping or font
runtime dependency in the core target.

The configure-time audit in `cmake/NexaUIDependencyAudit.cmake` examines the
actual `nexaui` target's direct and interface links and rejects unreviewed
link flags or directories. It rejects libraries outside the explicit
system-library allowlist. It also checks that core target sources and the public
header do not include headers of the frameworks named in [MASTER.md](../../MASTER.md).
CTest configures clean and intentionally forbidden fixtures to confirm the
link, flag and header checks work. Add a new native library to the allowlist
only with a documented platform need and a backend-local use site.

This is a build guard, not a complete provenance or license audit. It cannot
identify arbitrary copied third-party code or dynamically loaded libraries.
Review source additions and packaged artifacts as part of release acceptance.
Build tools, test scripts and pinned Unicode data are outside the core runtime
link boundary; their origins and licenses are tracked separately in the
repository.

The private Linux `nexaui_renderer_vulkan` target links NexaUI, the system
`Vulkan::Vulkan` loader, `Threads::Threads` and the native X11/Wayland client
targets used by its surface adapter. Vulkan types stay in that
backend, and neither the core target nor its installed package requires the
Vulkan loader. See the [bootstrap contract](vulkan.md) for build options and
current implementation limits.

The Vulkan backend includes authored GLSL and generated SPIR-V words.
`glslangValidator` and `spirv-val` are optional development tools for shader
regeneration/verification; normal builds and application runtime use the
embedded arrays and link neither tool. See [shader regeneration](vulkan.md#embedded-shaders).

The initial Apple adapter uses the OS `AppKit` framework and `objc` runtime.
These links are allowed only for an Apple target; the implementation remains in
`platform/macos/window.m`. The runtime link is explicit for plain C consumers of
the static archive. The audit checks both `#include` and Objective-C `#import`;
fixtures reject forbidden UI imports and AppKit links on non-Apple targets.

The private Linux accessibility adapter uses optional system `dbus-1` SDK headers and late-binds `libdbus-1.so.3`. Linux links `${CMAKE_DL_LIBS}` solely for that system loader; it does not link libdbus, libatspi or GLib into the core or require them in the installed package. Missing development headers, `NEXAUI_ENABLE_ATSPI=OFF`, a missing runtime ABI or bus preserve ordinary UI operation. The independent native test client links libatspi, GObject and D-Bus only in its separate process. See [AT-SPI](atspi.md) for the private protocol implementation and validation.
