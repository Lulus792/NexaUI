# Aetheri W04: Wayland scaling assessment

## Exact toplevel extents (2026-11-05)

Candidate based on `ac8178abbf0b8121d3b4002c6d42c5f87e20bf8c`. W04 requires
correct geometry at fractional scales; the native Wayland adapter currently
discovers only integer `wl_output` scales. The [fractional-scale-v1 protocol](https://github.com/wayland-mirror/wayland-protocols/blob/1.48/staging/fractional-scale/fractional-scale-v1.xml)
represents the preferred scale in 120ths and specifies nearest-pixel rounding
with half ties away from zero for toplevel buffers. It also requires a viewport
destination in logical coordinates and buffer scale 1 for fractional scaling.

`nx_wayland_scale_extent` now computes those extents in integer arithmetic and
rejects zero-sized or overflowing shared-memory buffers. The existing integer
output-scale and background-buffer paths use the same checked calculation.
The controlled arithmetic test includes odd logical dimensions, 1.25, 1.5 and
2.0 scales, a half-pixel case at 123/120, invalid input and size limits. This
step does **not** enable fractional scaling or alter the public window API.

On Alpine-Dev/WSL2, the complete private-Weston suites passed 191/191 with GCC
shared and 192/192 with Clang ASan/UBSan static. The fresh Wayland-only package
probe built and ran static and shared installed C17 consumers with CMake X11
discovery disabled. The dual-backend build and its X11 window test passed.
These checks do not prove that the host has no X11 development files or that
a fractional-scale compositor/physical monitor has been exercised.

Remaining work: negotiate fractional-scale-v1 and viewporter only when both
are available, keep integer fallback, propagate exact pixel extents to external
renderers without relying on binary `float` round-trips, and verify resize,
input, cursor, IME and accessibility geometry. The Settings and NexaCloud
Linux examples currently use `ceil(logical * scale)` for render targets;
that must be reconciled with the native pixel extent before fractional scaling
is exposed; the subsequent API step below addresses that consumer mismatch.
Real compositor/monitor and visual acceptance remain open.

## Native render target extent API (2026-11-05)

Candidate based on `48225d4b9a96e2cd41296460e703c529ef09e56a` adds
`nx_window_get_pixel_size` and `nx_pixel_size`. It supplies a native backing
extent without converting the public floating-point scale back into pixels.
Wayland uses checked extent arithmetic; X11 uses its client dimensions;
Windows uses `GetClientRect` in the window's DPI-awareness context; AppKit
converts view bounds to backing coordinates via
[NSView's backing conversion](https://developer.apple.com/documentation/appkit/nsview/converttobacking%28_%3A%29-3zors?language=objc).
The Settings, NexaCloud and native
clear Linux examples now use the API for their render targets. Existing
`nx_window_info` remains ABI-compatible. This does not yet enable fractional
Wayland scaling, and actual compositor, mixed-monitor, AppKit and physical-DPI
acceptance remain open.
Existing binaries are unchanged; consumers that call the new symbol require a
library built from this candidate or a later compatible revision.
The complete private-Weston tests passed 191/191 in GCC shared and 192/192 in
Clang ASan/UBSan static. An installed C17 consumer called the new API in fresh
Wayland-only static and shared packages. A Windows MSVC Debug multi-window test
compared the API against `GetClientRect`, and the dual-backend X11 window test
compared it against the native resized window. The AppKit implementation and
its native oracle test were not built or run on this host.

## Fractional-scale and viewporter negotiation (2026-11-05)

Candidate based on `2f271a6cc1784c0ce9380508e769342148f89ea1`. When both
protocol XML files are installed, NexaUI builds support for
`wp_fractional_scale_v1` and `wp_viewporter`. It enables the fractional path
only when the compositor advertises both and sends a preferred scale. Until
then, and when either protocol is absent, the largest entered integer output
scale remains in use. Older protocol data without the staging XML still builds
and uses the integer path.

The adapter keeps the received numerator in 120ths, reports the corresponding
logical scale, calculates the pixel extent with integer arithmetic and uses a
logical viewport with surface buffer scale 1. Cursor buffers use the next
integer scale. Settings and NexaCloud obtain render dimensions through
`nx_window_get_pixel_size`; no float-to-pixel reconstruction is required.

A controlled server checks 1.0, 1.25, 1.5, 2.0 and 123/120 scales, odd-size
rounding, resize and output changes, late preferred-scale delivery, logical
pointer and IME coordinates, accessibility geometry, external-renderer lease,
invalid scale, and each missing-protocol combination. It verifies the actual
Wayland viewport, buffer scale and shared-memory buffer dimensions. These tests
do not prove Vulkan presentation or visual sharpness at fractional scale on a
real compositor. A private Weston trace advertised viewporter but not
fractional-scale-v1; the Settings Vulkan smoke passed there using integer
fallback. Mixed-monitor transitions, physical devices, actual screenreader and
installed-IME behavior, and real fractional-scale compositor/Vulkan visual
acceptance remain open.
As a negative control, temporarily suppressing the preferred-scale callback
made the controlled fractional and integer-preference cases fail; after
restoring it, all seven passed again with sanitizers.

The private Weston trace is `%TEMP%/nexaui-fractional-weston-trace.txt`
(SHA-256 `50bc23da6e2fed6a032e195a94683973d5a9c0a29c03ef01355afe35f70eb907`).
Wayland-only static/shared installed C17 consumers passed, as did a fresh build
and controlled clipboard test with the staging fractional XML deliberately
absent. The dual-backend X11 window test and all seven controlled fractional
cases passed. The full Clang ASan/UBSan run passed 199/199 before the final
test-only 2.0-scale extension; all seven fractional cases subsequently passed
again under sanitizers. The first GCC full run passed 197/198: the unrelated
`nexaui_settings_ax_async_short` asserted its request was still pending but
observed completion after 0.125 seconds with a 100 ms deadline while other
builds were active. Ten isolated repetitions passed; scheduler contention is a
plausible explanation, not a proven cause. No deadline or assertion changed.
A subsequent GCC full run without the parallel sanitizer run passed 198/198.

## Headless Sway and software Vulkan (2026-11-05)

The tested source revision was `7519aa83dca26de23aac7f897d5528400dabc6db`.
On Alpine 3.24.2 in WSL2, Sway 1.12-rc3 with wlroots 0.20.0 ran as an
unprivileged user with a headless 1280×720 output and Pixman. Packages were
extracted into a temporary directory; the system installation and Aetheri were
not modified. The existing Wayland-only shared Settings example used Mesa
lavapipe through its explicitly selected Vulkan ICD. Its `--smoke` run passed
at 1.25×. A second instance remained open through these Sway output-scale
changes; its Wayland trace recorded Mesa Vulkan presentation buffers:

| Preferred scale | Logical viewport | Presented buffer |
| --- | --- | --- |
| 1.25× | 1020×549 | 1275×686 |
| 1.5× | 849×453 | 1274×680 |
| 2× | 636×333 | 1272×666 |
| 1× | 1276×693 | 1276×693 |
| 1.25× again | 1020×549 | 1275×686 |

These extents include the protocol's nearest-pixel rounding, including the
half-pixel cases at 1.5×. Sway's close command caused the Settings process to
exit successfully, with the viewport and fractional-scale objects destroyed.
Screenshots at each distinct scale and the dynamic Wayland trace are retained
locally under `out/fractional-sway-20261105/`; this ignored directory is not
part of the repository. The trace SHA-256 is
`afce10fc5d2069727edd682ecbf03ef0d9d183ab9f5dd402f34a790cf1dba16d`.

Sway also logged an invalid effective window geometry of 0×0 at the initial
bufferless commit, warning that future versions may disconnect such clients.
The trace shows the client requested a 640×360 xdg window geometry before
that commit, and attached its first buffer after the configure. The precise
cause and compatibility impact still need investigation; this run is not a
clean W01 acceptance. It does not establish physical monitor sharpness,
hardware Vulkan, mixed-output transitions, or real screenreader and installed
IME behavior. Those and full visual/input acceptance remain open.
