# Fixed-cell drawing

`<nexaui/cell_grid.h>` paints a bounded application-supplied visible grid. It
does not own or inspect a terminal session or editor document. The application
chooses the visible rows, resolves scalar values to fonts and glyph IDs, and
supplies cell spans, glyph offsets, foreground/background colors, selection
colors, underline and cursor-outline state. Combining marks can be provided as
additional positioned glyphs in their leading cell. The API does not perform
Unicode width decisions, shaping, bidi reordering, ligatures, parsing or reflow.
The caller is responsible for missing-glyph/fallback policy and for distinguishing
bytes, scalars, graphemes, glyphs, terminal cells and accessible offsets.

Spans of two use a zero-span continuation. A leading wide cell at the right edge
and a continuation at the left edge are permitted for clipped visible ranges.
The cell array has an explicit capacity and row stride; at most 32,768 visible
cells, four glyphs per leading cell and 16 distinct fonts are admitted. The
per-font glyph cache and per-call copied-mask budget have explicit maxima.
Glyph masks are copied into context-owned frame storage, so the source model and
fonts need only survive the draw call. A failure rolls back commands, clips and
owned masks to the state before the call. Each draw currently creates per-font
caches and copies visible glyph masks; retained atlas reuse and performance
budgets remain open.

`nx_cell_grid_hit_test` uses logical coordinates and maps a visible wide-cell
continuation back to its visible leading column. A continuation clipped at the
left edge remains column zero; the application resolves its offscreen model
identity. Hit testing validates the entire visible descriptor and is O(visible
cells), not O(history). Input routing, focus, UTF-8/IME editing and an accessible
text representation are not provided by this drawing primitive.

The [example](../../examples/20_cell_grid/README.md) has an independent model
and native virtual-keyboard check. It is not a PTY, complete terminal/editor,
human screenreader check or Aetheri integration. The targeted test checks fixed
geometry, wide-cell hit behavior, explicitly positioned glyphs, 240×100 visible
cells, allocation failure rollback and copied-mask lifetime.
