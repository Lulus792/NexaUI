# Hierarchy and property inspector

Run `nexaui_inspector_headless FONT_FILE OUTPUT.ppm` with a TrueType font. The
example draws a scene hierarchy, selects **Light**, proposes a keyboard change
to its **Intensity** property, accepts that proposal between frames, and writes
the final inspector view to `OUTPUT.ppm`.

The hierarchy's expansion and selection are UI state. Property values come
from the application snapshot. `nx_property_row` returns a proposal; the
application accepts it between frames here. A simulation tool can instead
submit a corresponding S02 command and use the next S01 snapshot as the
authoritative value. Text and number activation can open an external editor.
