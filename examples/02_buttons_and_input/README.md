# 02: Buttons and input

On Windows, run `nexaui_02_buttons_and_input FONT_FILE` with a local TrueType
font. The native D3D12 window contains a button, a checkbox, and a count
display. Click **Add one** or focus it with Tab and press Space to increment
the count. Click **Enable output** or focus it and press Space to show the
count in the progress bar.
Both controls draw focus feedback and expose their state in the semantic tree.

`nexaui_02_buttons_and_input --smoke FONT_FILE OUTPUT.ppm` uses a hidden native
window. It sends Win32 mouse and keyboard messages for both controls, checks
the resulting UI model and semantic nodes, renders six D3D12 frames, and writes the final
command stream as a PPM preview. CTest runs this mode when a suitable font is
available. The font path uses the C runtime's filename encoding.

This example follows `01_hello_ui`. The D3D12 driver is private to the build
tree; it is not an installed package API.
