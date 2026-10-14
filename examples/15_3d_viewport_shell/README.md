# Software-rendered 3D viewport shell

Run `nexaui_3d_viewport_shell OUTPUT.ppm`. The example projects a wireframe
cube into an application-owned RGBA8 surface and displays it through
`nx_viewport`. It draws once at a 1× backing scale and again at 2×, then writes
the focused second frame to a PPM image.
CTest runs this command in both static and shared builds when examples and
tests are enabled.

The source callback keeps its pixels unchanged until
`nx_viewport_surface_complete` runs after `nx_headless_render`. A production 3D
engine can replace the sample projection with its own software output. Direct
GPU texture interop awaits the native renderer backends and a separate surface
import path.
