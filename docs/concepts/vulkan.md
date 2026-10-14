# Linux Vulkan backend

The private `nexaui_renderer_vulkan` target implements V01 device selection
and an initial V02 Wayland/X11 surface, swapchain and clear/present path. It
accepts empty command streams through the private renderer driver. Drawing
NexaUI shapes, images and text remains V04–V06 work; resource hardening,
batching, profiling and broader recovery remain V03/V07–V09 work.

## Build boundary

Linux builds enable `NEXAUI_BUILD_VULKAN` by default and require Vulkan SDK
headers and the system Vulkan loader. Debian/Ubuntu packages are
`libvulkan-dev`; a software driver for development is available through
`mesa-vulkan-drivers`. Pass `-DNEXAUI_BUILD_VULKAN=OFF` to build the core without
the private target. Windows and macOS do not configure this option or find
Vulkan.

The core library, public header and installed package do not link Vulkan or
expose its types. `renderers/vulkan/device.h` is a build-tree interface, and
`device_internal.h` contains the backend-only native handles and dispatch.
The backend uses the GPU-driver API directly and adds no UI or rendering
framework dependency.

## Device and ownership contract

`nx_vk_device_create` requests Vulkan 1.0 with no optional device features.
It enables `VK_KHR_surface`, which needs no display connection and is required
by the `VK_KHR_swapchain` device extension. Callers can supply up to 64 unique
additional instance extension names for later platform surface integration.

Adapters need a graphics queue family with at least one queue. Selection
prefers discrete, integrated, virtual, other and finally CPU devices, keeping
enumeration order for ties. A driver that cannot create a logical device is
skipped in favor of the next candidate; host/device allocation failures stop
immediately. `require_swapchain` rejects adapters lacking that extension.
The extension is enabled whenever advertised. Surface-backed creation filters
queues by their support for the actual native surface before creating the
logical device. A shared graphics/present family is preferred; otherwise both
families get a queue and the swapchain uses concurrent sharing.

Enumeration uses at most four retries and caps each list at 4096 entries.
Shrinking lists retain the original allocation size for correct release
through caller allocators. Every partial-create path releases NexaUI storage
and successfully created native objects, and clears the output pointer.

All calls belong to the creating thread. Wrong-thread queries fail with
`NX_ERROR_STATE`; wrong-thread destruction leaves the object intact for its
owner. Multiple devices are independent. Query outputs are cleared on error.
NexaUI bookkeeping uses the supplied `nx_allocator`, or malloc/free by default;
Vulkan driver allocations use the driver's own callbacks. Internal borrowed
handles remain valid only until destruction. Their resources and external
submissions must be retired by the caller.

`nx_vk_device_wait_idle` maps device loss to `NX_ERROR_STATE`. Destruction
attempts an idle wait and releases the logical device before its instance,
including after device loss. Recovery and in-flight resource tracking are
V03 work. Adapter diagnostics report the advertised API and driver version,
vendor/device IDs, name, kind, queue families and enabled swapchain extension.

## Window surfaces and presentation

`nx_vulkan_renderer_create` leases one native window, enables the matching
Wayland or Xlib instance extension, creates its surface and selects a device
for that surface. The device owns the surface and destroys it after its logical
device but before its instance. The caller must keep the native window alive
until renderer destruction. A second renderer cannot lease the same window.

While leased, the Wayland adapter continues to process events, scale and
configuration but does not attach its SHM background over Vulkan's buffers.
Releasing the lease restores the platform background. Scale changes are
committed with the next Vulkan image. This state is per window and owner-thread
only; it introduces no native types into the public C API.

The initial path selects RGBA8/BGRA8 UNORM with the standard nonlinear surface
color space and FIFO presentation. It requires transfer-destination support
for clear commands; optional transfer-source support enables readback. Format
enumeration retries at most four times, and the backend caps format lists at
256 and swapchain images at 64. Unsupported formats/usages fail explicitly.

One command buffer, acquisition semaphore and submission fence serialize the
initial frames. Each swapchain image owns a separate present-wait semaphore,
reused only after that image is reacquired. Image barriers preserve acquisition,
transfer and presentation order, including separate graphics/present families.
Acquisition and submission-fence waits use one-second timeouts. A timeout
suspends a frame without reusing in-flight storage; an out-of-date swapchain
is rebuilt on retry. Suboptimal surfaces also trigger a rebuild even when the
application's target dimensions have not changed.

Resize currently waits for device idle, destroys the old swapchain and then
creates the replacement. A failure after teardown leaves an empty target for retry.
Fatal device/surface failures latch an unusable renderer until explicit
recovery recreates the device and surface. Native tests exercise explicit
rebuild and injected driver state; actual hardware removal is not yet forced.
Empty zero-size driver targets suspend, and drawing commands are rejected
before image acquisition. The visible [native clear example](../../examples/10_native_clear/README.md)
runs this path on either Linux backend.

### Remaining presentation-lifetime work

The current Vulkan 1.0 cleanup follows the conventional device-idle wait before
destroying swapchain resources. As the [Khronos semaphore guide](https://docs.vulkan.org/guide/latest/swapchain_semaphore_reuse.html#vk_ext_swapchain_maintenance1_extension)
explains, unextended Vulkan does not give that wait a formal presentation-
completion guarantee. V03 must add presentation-fence support and an explicit
fallback/retirement policy before this backend can pass release lifetime
acceptance. Passing validation layers does not resolve that specification gap.

## Tests

The injected-dispatch test exercises device preference and fallback, missing
queues and extensions, changing enumeration counts, bounded retries, native
errors, independent objects and owner-thread enforcement. A counting
allocator fails each NexaUI allocation in turn and checks exact-size release.
Dispatch belongs to each object; no mutable production test global is used.

The native test creates real Vulkan devices, submits an empty command buffer
to the selected queue, waits on a fence with a five-second timeout, verifies
independent device lifetimes and repeats teardown. CTest bounds the entire
process to 30 seconds. Without a suitable driver it can skip with code 77;
`NEXAUI_REQUIRE_VULKAN=1` makes driver absence a failure instead.

The X11 and Wayland presentation tests compare every swapchain readback byte
with headless RGBA8 output for opaque and translucent clears, including row
padding. X11 additionally checks the actual displayed window pixels. They
exercise 32 consecutive presents, X11 resize, synthetic Wayland 2× output
scale, window lease rejection, wrong-thread access, every NexaUI bookkeeping
allocation failure and explicit device/surface rebuild. Driver-state injection
checks cached-target invalidation, zero-size suspension, unsupported commands
and recovery; it is not physical device-loss evidence.

The Wayland presentation tests skip if no compositor is available; set
`NEXAUI_REQUIRE_VULKAN_WAYLAND=1` together with `NEXAUI_REQUIRE_VULKAN=1` to
require them. Linux CI requires X11 presentation under Xvfb. The visible
example has a bounded smoke mode registered for both backends when examples
are built.

```sh
cmake -S . -B out/vulkan-debug -DCMAKE_BUILD_TYPE=Debug
cmake --build out/vulkan-debug --parallel 4
NEXAUI_REQUIRE_VULKAN=1 VK_INSTANCE_LAYERS=VK_LAYER_KHRONOS_validation \
  ctest --test-dir out/vulkan-debug -R nexaui_vulkan_ --output-on-failure -V
```

Install Khronos validation layers to use `VK_INSTANCE_LAYERS`; on
Debian/Ubuntu the package is `vulkan-validationlayers`. CTest treats validation
errors and VUID diagnostics from the native test as failures. The Linux CI
core jobs install a Mesa driver and require this test to run. Authored CI
configuration is not evidence of a hosted run.

### Recorded validation — 2026-10-14

Alpine 3.24 under WSL, GCC 15.2.0, CMake 4.2.3, Vulkan loader/headers
1.4.347, Mesa 26.1.6 Lavapipe (`llvmpipe`, LLVM 22.1.3):

- Fresh Ninja Debug build with four parallel jobs: complete build succeeded;
  all 73 registered tests passed in 24.96 seconds, including Wayland, X11 and
  both Vulkan tests. X11 clipboard tests ran on an isolated Xvfb display.
- Make Debug build: complete build succeeded after fixing the shared Wayland
  header generator. Its X11 test passed on an isolated display; an earlier
  run against the interactive WSLg clipboard failed the empty-owner check.
- Clang 22.1.3 ASan/UBSan build: both Vulkan tests passed with
  `ASAN_OPTIONS=detect_leaks=1:halt_on_error=1` and
  `UBSAN_OPTIONS=halt_on_error=1`. Khronos validation was enabled for real
  command submission; no sanitizer, leak or validation finding was reported.
- Windows CMake configuration succeeded without looking for Vulkan.
- The installed core-package consumer configured, built and ran with Vulkan
  package discovery disabled. Core-only configuration also succeeded with
  `NEXAUI_BUILD_VULKAN=OFF` and Vulkan discovery disabled.

These are local results; hosted CI and hardware-GPU behavior are not yet
verified by this report. The initial clean parallel Make build exposed the
shared Wayland-header race; the common generator target fixes that build
dependency rather than relying on files left by a previous build.

V02 local validation on the same environment: both X11/Xwayland and WSLg
Wayland clear/present and byte-exact readback passed under Khronos validation.
The native tests completed 46 clear/readback frames per backend before their
separate driver checks. The full static Debug suite, including both example
smokes, passed 77/77 tests. The shared Debug build also passed 77/77 tests;
its installed-package consumer configured, built and ran, and its core shared
library has no Vulkan loader link. The four Vulkan device/presentation tests also
passed with Clang ASan/UBSan and leak detection enabled. Physical mixed-DPI
transitions, hardware drivers and physical device removal remain unverified.

## Specification references

- [Instance creation](https://docs.vulkan.org/refpages/latest/refpages/source/vkCreateInstance.html)
- [Physical-device enumeration](https://docs.vulkan.org/refpages/latest/refpages/source/vkEnumeratePhysicalDevices.html)
- [Graphics queue families](https://docs.vulkan.org/refpages/latest/refpages/source/vkGetPhysicalDeviceQueueFamilyProperties.html)
- [Logical-device creation](https://docs.vulkan.org/refpages/latest/refpages/source/vkCreateDevice.html)
- [Swapchain extension dependencies](https://docs.vulkan.org/refpages/latest/refpages/source/VK_KHR_swapchain.html)
- [Wayland surfaces](https://docs.vulkan.org/refpages/latest/refpages/source/vkCreateWaylandSurfaceKHR.html)
- [Xlib surfaces](https://docs.vulkan.org/refpages/latest/refpages/source/vkCreateXlibSurfaceKHR.html)
- [Swapchain synchronization and retirement](https://docs.vulkan.org/guide/latest/swapchain_semaphore_reuse.html)

NexaUI command rendering and hardware GPU acceptance remain open. CPU-driver
presentation is evidence of the tested WSI path, not GPU performance.
