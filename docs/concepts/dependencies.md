# Core dependency boundary

NexaUI's core runtime uses its own C17 code and the C standard library.
Platform adapters may link OS or GPU-driver libraries behind the public C API.
The current direct system links are `user32` on Windows, `m` on non-Apple Unix,
and CMake's `Threads::Threads` system-thread target on POSIX platforms.
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
