# Native Windows D3D12 shapes

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
