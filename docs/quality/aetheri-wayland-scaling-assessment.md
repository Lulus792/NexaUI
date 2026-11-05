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
is exposed. Real compositor/monitor and visual acceptance remain open.
