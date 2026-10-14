# Linux Vulkan backend

The private `nexaui_renderer_vulkan` target implements V01 device selection
and an initial V02 Wayland/X11 surface, swapchain and clear/present path, with
V03 presentation-fence retirement and retryable teardown/recovery. It
accepts empty command streams through the private renderer driver. Drawing
NexaUI shapes, images and text remains V04–V06 work; batching, profiling,
hardware device-loss acceptance and broader integration remain V07–V09 work.

## Build boundary

Linux builds enable `NEXAUI_BUILD_VULKAN` by default and require Vulkan SDK
headers 1.3.250 or newer and the system Vulkan loader. The runtime still uses
Vulkan 1.0 plus explicitly negotiated extensions. Debian/Ubuntu packages are
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

`nx_vk_device_create` requests Vulkan 1.0. Its default descriptor enables no
optional device features. A requested present-fence policy negotiates
`VK_KHR_swapchain_maintenance1` or `VK_EXT_swapchain_maintenance1`, preferring
KHR when both are available. The selected device must advertise both the
extension and the `swapchainMaintenance1` feature. Instance creation enables
the matching surface-maintenance extension, `VK_KHR_get_surface_capabilities2`
and `VK_KHR_get_physical_device_properties2`; missing dependencies or a false
feature reject a required request. An optional request can select an adapter
without present fences. Diagnostics report the enabled variant.
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
including after device loss. The renderer uses a separate private destroy
entry only after retiring all of its tracked work. Adapter diagnostics report the advertised API and driver version,
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
and strict mode adds a present fence per image. Image barriers preserve acquisition,
transfer and presentation order, including separate graphics/present families.
Acquisition, acquisition-fence, submission-fence and present-fence waits use
one-second timeouts. A timed-out image fence retains the acquired image and
its acquisition signal for the next call; it does not acquire another image
or rebuild the swapchain first. A timeout suspends a frame without reusing
in-flight storage; an out-of-date swapchain
is rebuilt on retry. Suboptimal surfaces also trigger a rebuild even when the
application's target dimensions have not changed.

Strict resize first retires acquisition, submission and all pending presentations
with one aggregate, one-second fence wait. A timeout retains the old chain
and returns a suspended frame. Successful retirement destroys the old chain
before creating its replacement. A failure after teardown leaves an empty target for retry.
Fatal device/surface failures latch an unusable renderer until explicit
recovery recreates the device and surface. Native tests exercise explicit
rebuild and injected driver state; actual hardware removal is not yet forced.
Empty zero-size driver targets suspend, and drawing commands are rejected
before image acquisition. The visible [native clear example](../../examples/10_native_clear/README.md)
runs this path on either Linux backend.

### Presentation retirement and close

`nx_vulkan_renderer_create` and the F11 adapter use strict retirement by
default. `nx_vulkan_renderer_create_config` accepts these explicit policies:

| Policy | Present fences | Driver without maintenance support |
| --- | --- | --- |
| `NX_VULKAN_RETIRE_STRICT` | Required; KHR preferred, EXT accepted | Creation returns `NX_ERROR_UNSUPPORTED` |
| `NX_VULKAN_RETIRE_COMPATIBLE` | Used when supported | Conventional device-idle fallback |
| `NX_VULKAN_RETIRE_LEGACY` | Disabled | Conventional device-idle path |

A presentation fence proves when the image's present-wait semaphore may be
reused or destroyed. All pending present fences must complete before swapchain
destruction. A submission fence alone proves only submission completion.
Strict cleanup does not substitute `vkDeviceWaitIdle` for presentation fences.
As the [Khronos semaphore guide](https://docs.vulkan.org/guide/latest/swapchain_semaphore_reuse.html#vk_ext_swapchain_maintenance1_extension)
explains, device-idle waits in unextended Vulkan lack that formal presentation
retirement guarantee. The explicit fallback modes retain this limitation.

`nx_vulkan_renderer_destroy` now returns `nx_result`. A timeout returns
`NX_ERROR_LIMIT` and preserves the renderer, its resources and its window
lease. Keep the window alive, process events and retry destruction. Once close
starts, only diagnostics and another close are allowed. The private F11
`nx_renderer_close` propagates the error and retains its backend pointer until
close succeeds; applications must check the result. Recovery follows the same
retire-before-release rule, but remains retryable as recovery until close starts.

Presentation OOM leaves its wait operation unenqueued; cleanup waits for the
already submitted graphics work without waiting on an unsignaled present
fence. An enqueued out-of-date presentation still contributes a fence to
retirement. Readback buffers remain owned by the renderer after a submission
timeout and are freed only after a subsequent wait retires that work.
`VK_ERROR_DEVICE_LOST` returned by a synchronization wait counts as completion
for determining resource use, as specified by Vulkan; merely latching a loss
flag is insufficient to release resources.

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

Maintenance negotiation tests cover EXT, KHR, preference, missing dependencies,
false feature bits, optional fallback and all bookkeeping allocation failures.
Presentation fault tests retain old resources across resize/recovery/close
timeouts, resume a held acquisition before rebuilding, clean a partial rebuild,
distinguish unenqueued OOM from enqueued out-of-date, and retain readback storage
across submission timeouts. The F11 mock and real adapters verify failed close
preserves ownership and blocks new work until a successful retry.

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

V03 local validation on the same environment:

- Final static and shared Ninja Debug builds each passed all 77 tests under
  Khronos validation, with required X11 and Wayland Vulkan execution. The
  complete suites took 14.29 and 14.37 seconds respectively.
- Clang 22.1.3 ASan/UBSan and leak detection passed all four Vulkan tests in
  3.37 seconds, including the new retirement and retained-readback faults.
  No sanitizer, leak or validation finding was reported.
- Windows MSVC Debug built successfully; 81 tests passed and the clipboard
  test skipped. The shared Windows build also succeeded and passed all three
  focused F11/D3D12 tests. These check the changed private destroy callback;
  they do not execute Vulkan on Windows.
- The updated shared core installed successfully. Its package consumer
  configured, linked and ran with Vulkan discovery disabled; `ldd` still
  showed no Vulkan loader dependency in the core library.
- The README's complete C example built and ran against the installed core;
  all 96 local links/anchors in the README and updated renderer guides were
  checked. The installed public header has no changes in this slice.

EXT/KHR selection and missing-feature paths use injected dispatch. Native
execution uses Mesa Lavapipe; it does not establish physical GPU removal,
hardware performance or compatibility with every driver. Legacy fallback
keeps its documented specification limitation.

## Specification references

- [Instance creation](https://docs.vulkan.org/refpages/latest/refpages/source/vkCreateInstance.html)
- [Physical-device enumeration](https://docs.vulkan.org/refpages/latest/refpages/source/vkEnumeratePhysicalDevices.html)
- [Graphics queue families](https://docs.vulkan.org/refpages/latest/refpages/source/vkGetPhysicalDeviceQueueFamilyProperties.html)
- [Logical-device creation](https://docs.vulkan.org/refpages/latest/refpages/source/vkCreateDevice.html)
- [Swapchain extension dependencies](https://docs.vulkan.org/refpages/latest/refpages/source/VK_KHR_swapchain.html)
- [Wayland surfaces](https://docs.vulkan.org/refpages/latest/refpages/source/vkCreateWaylandSurfaceKHR.html)
- [Xlib surfaces](https://docs.vulkan.org/refpages/latest/refpages/source/vkCreateXlibSurfaceKHR.html)
- [Swapchain synchronization and retirement](https://docs.vulkan.org/guide/latest/swapchain_semaphore_reuse.html)
- [KHR maintenance dependencies](https://docs.vulkan.org/refpages/latest/refpages/source/VK_KHR_swapchain_maintenance1.html)
- [EXT maintenance dependencies](https://docs.vulkan.org/refpages/latest/refpages/source/VK_EXT_swapchain_maintenance1.html)
- [Presentation-fence lifetime](https://docs.vulkan.org/refpages/latest/refpages/source/VkSwapchainPresentFenceInfoKHR.html)
- [Present enqueue errors](https://docs.vulkan.org/refpages/latest/refpages/source/vkQueuePresentKHR.html)
- [Device loss and synchronization](https://docs.vulkan.org/spec/latest/chapters/devsandqueues.html#devsandqueues-devicelost)

NexaUI command rendering and hardware GPU acceptance remain open. CPU-driver
presentation is evidence of the tested WSI path, not GPU performance.
