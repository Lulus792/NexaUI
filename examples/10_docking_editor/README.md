# Docking editor

Build the sample and pass a TrueType font, image path and snapshot path:

```sh
cmake --build build --config Debug --target nexaui_docking_editor
build/examples/10_docking_editor/Debug/nexaui_docking_editor C:/Windows/Fonts/arial.ttf docking.ppm docking.nxdock
```

On Windows, add `.exe` to the executable path. Single-config generators
usually place it directly in `build/examples/10_docking_editor/`.

The example starts with Scene and Assets tabs on the left and Inspector on the
right. A synthetic click on **Move Assets right** moves the Assets panel to the
right stack and selects it. A pointer drag changes the split ratio; the next
frame arranges both panes at the new ratio. The program checks the dock tree,
writes the final 800 × 500 PPM frame, then encodes a versioned `.nxdock`
snapshot, decodes it into fresh buffers and verifies a byte-for-byte roundtrip.

The docking model owns no memory: the application supplies node and tab
buffers. The layout tree arranges panes, while tabs and the splitter provide
input and semantics. Drag-and-drop docking targets and previews remain future
work. `NEXAUI_EXAMPLE_FONT` can select a local TrueType font for CTest.
