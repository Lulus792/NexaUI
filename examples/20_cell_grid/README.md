# Application-owned cell grid

This small Linux Wayland consumer keeps a separate visible-cell model. It maps
model Unicode scalars to existing NexaUI font glyphs, supplies a two-cell entry,
an explicitly positioned combining glyph, selection colors, underline and a
cursor outline, then paints with `nx_draw_cell_grid`. Right-arrow movement and
typed text change only the example's model. It is not a terminal emulator: PTY,
escape parsing, scrollback, clipboard, IME, selection policy and
process ownership are outside the example and remain separate integration work.
Fonts without a glyph draw their own `.notdef` glyph; no font fallback is added.
The application independently maps the visible cells to a copied read-only
UTF-8 accessibility snapshot. On Linux the partial AT-SPI Text interface uses
Unicode scalar offsets; its caret and selection setters are rejected. This is
not a scrollback, editable-text or full screenreader implementation.

Copy this directory outside the SDK source tree, then build against a Linux
Wayland SDK:

```sh
cmake -S /path/to/copied-example -B grid-build -DCMAKE_PREFIX_PATH=/path/to/nexaui-sdk
cmake --build grid-build
```

Run `grid-build/nexaui_cell_grid_example /path/to/font.ttf` inside a Wayland session;
Right moves the example cursor and Escape closes. A shared SDK may need its
library directory in the loader path.

`run_keyboard.py` uses the private Weston/Xvfb fixture to verify native text,
right-arrow traversal across a wide-cell continuation and normal close. It is
virtual-input evidence, not a production terminal or human accessibility test.
The separate libatspi client reads the published text and checks that native
caret and selection setters fail; it does not constitute human Orca acceptance.
