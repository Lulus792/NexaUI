# Native renderer examples

## Linux Vulkan shapes

The Linux `nexaui_native_clear` opens a native window for up to three seconds
and draws a rounded opaque panel plus a translucent moving rectangle and triangle
inside two nested clips, over a changing clear color through the SPIR-V
pipeline, followed by an inside stroke and round-capped line. Resize it or close it
early to exercise the renderer driver. It uses the chosen Linux window
backend and requires a Vulkan driver supporting presentation to that surface
and KHR or EXT swapchain-maintenance present fences. The default F11 adapter
uses strict retirement; see [the Vulkan contract](../../docs/concepts/vulkan.md)
for explicit compatibility modes on older drivers.

```sh
NEXAUI_PLATFORM=wayland ./build/examples/10_native_clear/nexaui_native_clear
NEXAUI_PLATFORM=x11 ./build/examples/10_native_clear/nexaui_native_clear
```

`--smoke` limits the loop to a quarter second and checks that at least one
frame was presented. CTest registers both backend variants; an unavailable
display skips the corresponding smoke test unless explicitly required.
The V04/V05 pipeline accepts filled/stroked rectangles, rounded fills, triangles
and lines with nested clips and straight-alpha composition. Images and text remain Vulkan work;
see [the Vulkan contract](../../docs/concepts/vulkan.md).

The example checks `nx_renderer_close` and retries a retirement timeout while
keeping the native window alive. If retirement still fails, it exits with an
error and leaves cleanup to process teardown. Applications that remain running
must keep the renderer and window and retry close after pending work completes.

## Windows D3D12 shapes

Run `nexaui_native_clear` on Windows. It opens a visible Win32 window for up to
three seconds, clears its D3D12 swap chain, draws rectangles and a
translucent triangle inside nested clips, then draws an RGBA8 icon, an
A8-tinted letter mask, a stroked rectangle, a line and a rounded rectangle.
One rectangle moves
over time. Resizing the window
during that interval exercises the private
renderer driver's target resize path. The example reads the current native
backing scale each frame and attempts renderer recovery if the device is lost.
The window can be closed early.

This is the D01–D08 Windows surface example. It submits all current drawing
commands: filled and stroked rectangles, rounded rectangles, triangles,
lines, bitmaps and clips. The
renderer target is currently a build-tree component and is not installed as
a public package API.
