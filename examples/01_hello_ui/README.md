# 01: Hello UI

On Windows, run `nexaui_01_hello_ui [FONT_FILE]`. It opens a native window and
renders one UI context's command stream through the build-tree D3D12 driver.
The centered card contains an icon and an accessible greeting. Supply a local
TrueType font to display the heading and subtitle as text. Resize or close
the window using normal Windows controls.

`nexaui_01_hello_ui --smoke` creates a hidden window and submits two frames.
`nexaui_01_hello_ui --smoke FONT_FILE OUTPUT.ppm` also saves a headless preview
of the same commands. CTest runs both variants when a suitable font is
available. The smoke run verifies the semantic text node and its parent.

This example introduces context drawing after `00_hello_window`. The D3D12
driver is private to the build tree; it is not an installed package API.
Native rendering on macOS and Linux remains planned.
