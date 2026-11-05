# Application-owned cell grid

This small Linux Wayland consumer keeps a separate visible-cell model. It maps
model Unicode scalars to existing NexaUI font glyphs, supplies a two-cell entry,
an explicitly positioned combining glyph, selection colors, underline and a
cursor outline, then paints with `nx_draw_cell_grid`. Right-arrow movement and
typed text change only the example's model. Left/Right move between leading
cells, including across rows; Shift+Left/Right extends or reverses an
application-owned selection. Ctrl+Shift+C copies the selected visible text;
with no selection it leaves the clipboard unchanged. Ctrl+C is not treated as
copy by this example. Ctrl+Shift+V asynchronously receives up to 4096 bytes
into an application-owned demonstration buffer; the status row reports waiting,
success, cancellation or failure without displaying its contents. A newer model
change, focus loss or close cancels a pending read. The buffer is not a PTY,
command queue or terminal screen: bytes, including controls and line breaks,
are never interpreted or executed. Copy failures are reported on standard error; retention
after the owner exits depends on the compositor. The example colors selected cells and publishes
the corresponding UTF-8 byte range in its read-only accessibility snapshot.
It is not a terminal emulator: PTY,
escape parsing, scrollback, PTY paste integration, IME, full selection policy and
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
Left/Right move the example cursor, Shift extends selection and Escape closes. A shared SDK may need its
library directory in the loader path.

`run_keyboard.py` uses the private Weston/Xvfb fixture to verify native text,
right-arrow traversal across a wide-cell continuation and normal close. It is
virtual-input evidence, not a production terminal or human accessibility test.
The separate libatspi client reads the published text, checks forward/reverse
selection across a wide character and a combining-mark cell, and checks that
native caret and selection setters fail. When `wl-paste` is available, a
separate Wayland client also checks copied Unicode, line breaks, empty
selection, and the Ctrl+C distinction. When both `wl-copy` and `wl-paste` are
available, another private test sends multiline Unicode and control bytes and
freezes its own clipboard owner to verify model-change and focus-loss
cancellation. `--verify-paste` exposes test-only length/checksum output and is
not intended for sensitive clipboard contents. This is not human Orca acceptance.
