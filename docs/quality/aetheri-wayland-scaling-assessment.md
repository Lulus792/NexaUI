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
