# Text engine status

NexaUI has begun its own font stack with a bounded SFNT directory parser. `nx_font_open` accepts a single TrueType or OpenType SFNT face in an immutable caller-owned byte buffer. It checks the container signature, directory size, table bounds and duplicate tags, caps the directory at `NX_SFNT_MAX_TABLES`, and stores a sorted lookup table. `nx_font_table_at` and `nx_font_find_table` return borrowed byte spans. The original font bytes must remain unchanged and readable until `nx_font_close`.

`nx_font_get_metrics` validates `head`, `maxp`, `hhea` and `hmtx` before returning units per em, glyph count, ascender, descender, line gap and horizontal metric count. `nx_font_glyph_hmetrics` reads a glyph's advance width and left side bearing. Glyphs beyond `number_of_hmetrics` reuse the final advance width and take their own bearing from the trailing `hmtx` entries.

`nx_font_glyph_index` selects one valid Unicode `cmap` subtable when the font opens. It supports format 4 for BMP mappings and format 12 for the full Unicode range, preferring format 12 when present. Platform 3 encoding 10 and platform 0 Unicode records are considered for format 12; platform 3 encoding 1 and platform 0 Unicode records are considered for format 4. Segment and group ranges, relative glyph-array offsets and subtable lengths are checked before the lookup is cached. Glyph zero denotes a missing mapping. A malformed preferred subtable can fall back to a valid lower-priority Unicode subtable. The implementation follows the [OpenType `cmap` specification](https://learn.microsoft.com/en-us/typography/opentype/spec/cmap).

`nx_font_get_glyph_outline` decodes simple and composite TrueType outlines from `loca` and `glyf` into caller-owned point and contour arrays. It handles short and long `loca` offsets, packed coordinate deltas, repeated flags, zero-contour glyphs, component translations, outline-point alignment, F2DOT14 transforms and nested components. Composite traversal detects cycles and limits recursion to 32 glyphs and total decoded points to 65,536. A count-only call requires no caller point allocation; the parser uses temporary buffers from the font's allocator to validate transformed coordinates. A fill call copies to caller arrays only after successful validation. The parser follows the [OpenType `loca`](https://learn.microsoft.com/en-us/typography/opentype/spec/loca), [`glyf`](https://learn.microsoft.com/en-us/typography/opentype/spec/glyf) and [`maxp`](https://learn.microsoft.com/en-us/typography/opentype/spec/maxp) tables. It returns unscaled design-unit points; fractional transformed coordinates round to the nearest design unit. It does not apply hinting, variation deltas or phantom-point alignment.

`nx_rasterize_glyph_a8` now turns decoded outlines into caller-owned A8 coverage masks. It inserts implied on-curve midpoints between consecutive off-curve points, flattens quadratic BÃ©zier spans within a pixel-space tolerance, and fills using the nonzero winding rule. Each pixel uses a deterministic 4Ã—4 sample grid. The implementation is guided by the [TrueType fundamentals description of outlines and scan conversion](https://learn.microsoft.com/en-us/typography/opentype/spec/ttch01). It does not interpret TrueType instructions, perform grid fitting or apply dropout control, so small-size results can differ from native font rasterizers. Dimensions, edge count, recursion and scanline work are bounded. The resulting mask can be drawn by the headless renderer as `NX_BITMAP_A8`.

`nx_glyph_cache` reuses these A8 masks for a single font. The integer getter remains compatible; `nx_glyph_cache_get_fractional` accepts exact finite double sizes in [.125,65535], with no size quantization. A bounded resident-byte budget includes cache entry headers and mask pixels. New masks evict least recently used entries when needed; hit, miss, eviction, resident-byte and entry counters are available for diagnostics. Mask views expose baseline offsets and pixel advance. The font and its source bytes outlive the cache, and a borrowed view is invalidated by the next miss, clear or destroy. Temporary outline and raster buffers can briefly exceed the resident cache budget. The cache currently stores individual masks; shared atlas packing and GPU upload remain future work.

`nx_font_collection` holds an ordered list of borrowed font faces and selects the first nonzero glyph for a Unicode scalar. It rejects duplicate faces, requires a supported Unicode `cmap`, and is limited to 64 faces. A missing scalar is represented by a null font and glyph zero, allowing the caller to choose a replacement policy. The collection does not own fonts or source bytes. Selection currently considers coverage only; language, style, shaping, variation selectors and color-glyph capability are not yet part of the decision. A selected face may have a CFF outline that the current rasterizer cannot draw.

`nx_shape_text` produces logical glyphs and runs with bounded GSUB single, ligature, contextual and chained contextual substitutions, GPOS single/pair positioning and mark-to-base/ligature/mark attachment, and classic `kern` fallback. `nx_text_layout_create` adds Unicode line wrapping, hard breaks, line-local reshaping, bidi positions, grapheme carets, hit testing and selection rectangles. `nx_draw_text_layout` converts those positioned glyphs to A8 bitmap commands. It copies each mask into context-owned memory that remains valid until the next `nx_begin_frame` or context destruction, including after temporary glyph caches are released. Drawing accepts finite fractional pixels-per-em layouts in [.125,16384] and requires a positive per-font cache budget. A failed draw call rolls back its commands and owned masks.

The supported OpenType subset includes GSUB types 1/4/5/6 and extension type 7, and GPOS types 1/2/4/5/6 and extension type 9. Selected `mark`, `mkmk`, `abvm` and `blwm` features participate in positioning. GDEF class definitions and mark sets implement lookup filtering; `IgnoreMarks` takes precedence over mark filters, and a mark filtering set takes precedence over a mark attachment class. A selected lookup that requires missing class/filter data returns an error. This tightens the previous behavior that skipped such flags. Unsupported lookup kinds still remain outside the subset. Public output arrays and counts remain unchanged on malformed-font failure.

Mark anchors preserve advances, including spacing-mark advances. Private GSUB provenance tracks the logical component of retained marks through nested ligatures. Attachments resolve after positioning and legacy kerning, so changes to a parent propagate to its attached marks in both LTR and RTL runs. Each attachment points to an earlier logical glyph and the final resolution uses one forward pass. Ligature compaction streams the output instead of moving the complete suffix for every match. Parsing and lookup work retain a per-table budget of 4,000,000 operations; this is a bounded subset rather than a complete script shaper.

Anchor formats 1/2/3 use design coordinates. Format 2 falls back to those coordinates because the rasterizer does not apply TrueType hinting. Format 3 applies signed 2/4/8-bit Device deltas at integer ppem; fractional sizes keep design coordinates. VariationIndex deltas are ignored for the current default-instance-only engine. Device records in ordinary single/pair ValueRecords remain unsupported. These contracts follow the primary [GPOS](https://learn.microsoft.com/en-us/typography/opentype/spec/gpos), [GDEF](https://learn.microsoft.com/en-us/typography/opentype/spec/gdef) and [lookup flags](https://learn.microsoft.com/en-us/typography/opentype/spec/chapter2) specifications. See the [mark-positioning report](../quality/gpos-marks-report.md) for tests and limits.

The first rich-text path reuses this layout. `nx_draw_rich_text_layout` accepts ordered, nonoverlapping `nx_text_style_span` byte ranges for foreground color, background, underline and strike; uncovered text uses a base color. `nx_rich_label` adds the same drawing with one semantic text node whose accessible name is the complete unformatted source. Span boundaries must be extended-grapheme boundaries (`nx_text_layout_is_grapheme_boundary`) and cannot cut through one shaped glyph, including a ligature. A style boundary inside a ligature is rejected because the current layout API cannot reshape around it. Background and decoration rectangles follow the existing bidi and wrapped-line selection geometry. The drawing call rolls back all commands and owned masks on failure. Font family, size, weight and inline objects still require style-aware shaping/layout rather than paint-only spans.

`nx_text_buffer` owns mutable UTF-8 with selection offsets at extended-grapheme boundaries. Replacement, forward/backward grapheme deletion, default Unicode word-boundary navigation, line/document movement and a byte-budgeted undo/redo operation log are available. `nx_text_buffer_delete_surrounding` applies UTF-8 byte counts around the selection from a v3 IME; `nx_text_buffer_delete_relative` applies a v1 byte range relative to the cursor. Both reject invalid bounds and partial scalars. Edits rebuild text and boundary indexes before committing, so invalid UTF-8 or allocation failure leaves the prior state intact. New edits clear redo; an edit larger than the history budget remains applied but clears history. `nx_text_buffer_create_sensitive` instead creates a no-history buffer whose owned text allocations are overwritten before release. `nx_text_buffer_apply_event` maps normalized key, Unicode text and logical text-command events to editing actions for a focused buffer, while `nx_text_buffer_selected_span` exposes selected bytes for clipboard use. `nx_numeric_parse` and `nx_numeric_input_apply_event` add locale-independent decimal validation and commit-on-Enter behavior for a focused numeric editor buffer. Incomplete drafts do not change the committed number. Rendered numeric and general text fields combine buffers with font layout, clipping, selection/caret drawing and semantic validation state; the password constructor uses a sensitive buffer and masked presentation. This implementation uses contiguous text and linear scans rather than a piece table or rope. Preedit from Windows, X11 callback-style XIM, and Wayland v3/v1 is displayed transiently in focused text fields; AppKit also provides bounded composition and native editor commands. Full-document AppKit replacement/reconversion, installed-IME acceptance, secure control of external copies and interactive Wayland clipboard acceptance remain future work.

Rendered general text fields use layout caret positions for visual Left/Right,
word movement, line edges and vertical/page movement, including bidi and soft-wrap
affinity. Plain `PAGE_UP/DOWN` commands scroll without changing the selection;
`MOVE_PAGE_UP/DOWN` move the caret, and selection variants preserve the anchor.
Clipboard commands use the field's optional callbacks and suppress copy/cut for
passwords. The standalone text model has no viewport or clipboard and leaves
such commands unhandled. Numeric Enter commands commit a valid draft, but the
numeric widget does not yet share the complete general-field native input path.
See the [native command report](../quality/macos-input-commands-report.md).

General and numeric text fields now support left-button drag selection and
Shift-click extension. The moving endpoint follows layout hit testing while the
source anchor stays fixed. General fields preserve bidi/wrap affinity and map
password-mask positions back to complete source graphemes. Capture permits moving
and releasing outside the field; cancellation, focus loss, disabling or omission
ends the drag. A held pointer beyond the visible edge scrolls the field once per
frame, with bounded speed and catch-up. Touch taps still place the caret; touch
selection handles, double/triple-click units and text drag/drop remain open.

Selection uses both `NX_COLOR_SELECTION` and `NX_COLOR_SELECTION_TEXT`. Boundary
glyph masks split into disjoint clips, including glyph overhang, so foreground
colors do not require painting the antialiased glyph twice. See the
[executed selection and contrast checks](../quality/text-pointer-report.md).

This stage does not validate SFNT table checksums or parse font collections, Unicode variation sequences, other `cmap` formats or CFF outlines. Contextual substitutions/positioning, cursive attachment, advanced script preprocessing/reordering, variable-font deltas, atlas packing and native text rendering remain pending. Invalid supported table structures return an error rather than exposing out-of-bounds spans.

The `nexaui_font_inspect` example prints directory entries, metrics, representative Unicode mappings and simple/composite sample outlines from a local font. `nexaui_glyph_raster` writes a cached PGM mask for one Unicode scalar. `nexaui_font_fallback` reports which ordered face covers four representative scalars. `tests/test_font.c` exercises malformed offsets, duplicate tags, allocator failures, metric bounds and deterministic byte mutations; `tests/test_cmap.c` exercises format 4 and 12 mappings and malformed ranges; `tests/test_glyf.c` exercises short and long location tables, packed points, repeated flags, nested components, transforms, cycles, malformed references and allocator failures; `tests/test_raster.c` covers holes, antialiasing, implicit curve points, atomic failures and A8 headless compositing; `tests/test_glyph_cache.c` covers reuse, LRU eviction, resident budget, empty glyphs and allocator failures; `tests/test_font_collection.c` covers ordered fallback, missing glyphs, duplicate faces, capacity and allocation failures. For deeper corpus testing, configure with `-DNEXAUI_BUILD_FUZZERS=ON` under a non-MSVC Clang toolchain with libFuzzer and run `nexaui_sfnt_fuzz` on a font corpus. This fuzzer target has not yet been run in the current Windows environment.

On 2026-09-28, the local Windows builds mapped U+0041, U+20AC, U+65E5 and U+1F600 in Arial, Source Han Sans JP and Segoe UI Emoji. All twelve resulting glyph IDs matched an independent FontTools `getBestCmap`/`getGlyphID` check. This is a smoke check of those installed font versions, not a Unicode conformance claim.

The same local run decoded the `A` outlines in Arial (2 contours, 15 points, coordinate sums `10055,10740`, 13 on-curve points) and Bahnschrift (2 contours, 11 points, sums `7319,5745`, 11 on-curve points). Counts, coordinate sums and on-curve counts matched FontTools. Source Han Sans JP uses CFF outlines, which this API currently reports as unsupported.

The local Arial U+00E9 composite resolves to 3 contours and 34 points; its first point `(862,342)`, coordinate sums `21126,21093` and 17 on-curve points match FontTools `glyf.getCoordinates`. Bahnschrift U+00E9 also matches: 2 contours, 38 points, first point `(585,-15)`, sums `22149,22005` and 22 on-curve points. These checks cover two installed font versions and do not establish corpus-wide composite coverage.

The local direct raster example produced a 48Ã—50 A8 mask for Arial U+0041 and a 35Ã—52 mask for composite U+00E9 at 64 pixels per em. The current cached example uses a one-pixel border and produced 46Ã—48 and 33Ã—50 masks, respectively. After accounting for the border, cached and direct mask pixels match exactly. Both masks were opened and visually checked; the `Ã©` accent and counter render in the expected positions. This is a smoke check, not a cross-font visual regression suite.

The local fallback example selected Arial for U+0041 and U+00E9, Source Han Sans JP for U+65E5, and Segoe UI Emoji for U+1F600 in that order. This confirms collection order and mapping with those installed font versions; it does not imply those CFF or color glyphs can already be rasterized by NexaUI.

## Contextual substitution subset

GSUB types 5/6 support formats 1 (glyph), 2 (class) and 3 (coverage), including
implicit Class 0 and separate backtrack/input/lookahead classes. Default selected
features include `calt`. Nested actions support types 1/4/5/6/7. Matching precedes
actions; records run in order and their sequence indices address the sequence
after earlier contractions. Lookup flags filter following/context glyphs, not the
current first glyph. Each nested lookup retains its own GDEF flags. Scanning
resumes after matched input; lookahead remains eligible for subsequent matches.

Stable linked slots retain mark provenance through nested ligatures and forward
resume fences across removed nodes. Each contextual root compacts once. Limits
are 64 positions per input/backtrack/lookahead, 64 actions/subtables, depth 32,
and the shared 4,000,000-operation table budget. Unsupported nested kinds skip
the whole contextual root before mutation; malformed data and exhausted budgets
return errors with public outputs unchanged. GSUB 2/3/8, contextual/cursive GPOS,
script-specific joining/reordering and variable-font shaping remain open.
See the [context report](../quality/gsub-context-report.md).

## Raster density without changing layout

Set `nx_context_set_text_raster_scale(ui, window_density)` between frames to
choose the default for `nx_draw_text_layout`, rich text and font-backed widgets.
New contexts use 1×. `nx_context_get_text_raster_scale` reports the current value
in any frame state. The setter accepts finite [.125,8], does not allocate, and
rejects changes during frame building with STATE. It neither alters layout/input
coordinates nor invalidates the previous finished command stream.

The default reaches layout labels/actions, menus/tabs/tables, property text,
text/numeric editors including selection and preedit, search queries/results/
empty labels, plot ticks and dashboard text. Their existing calls converge on
the ordinary or selected/rich layout draw paths. Explicit `_scaled` drawing and
stamp creation use exactly the caller's scale, without multiplying the default.
Cached stamps/images, bitmap sources, icons, layout measurement, hit tests,
caret/selection/decorations and IME placement remain independently controlled.
The NexaCloud reference runner applies window density before each begin-frame.

Drawing still returns ARGUMENT for physical ppem outside [.125,16384], LIMIT
for exhausted resource budgets and MEMORY for failed allocations. Failed draws
retain the existing widget/command rollback contracts; there is no automatic
old-mask cache or lower-density retry. A field can accept an edit before painting
fails; its existing same-frame retry does not repeat that edit. Callers can retry
with a smaller density after ending the frame, or use their own retained output.
Keep the context alive and do not begin another frame until renderers finish
consuming its copied direct masks. Changing the default alone does not expire
them. Raster-padding clips/ink bounds can vary; logical selection fills and caret
positions do not. The [widget comparison](../../examples/17_widget_text_density/README.md)
demonstrates the distinction with a user-supplied font.

`nx_draw_text_layout_scaled` rasterizes at `layout_ppem * raster_scale` while
recording logical destination geometry. Scale must be finite in [.125,8] and
the resulting physical ppem must remain in [.125,16384]. It retains the supplied
layout's shaping, line breaks, carets and selection coordinates. It does not
reshape at physical ppem or reinterpret its existing GPOS Device adjustments.

`nx_text_stamp_create_scaled` builds an owned A8 stamp from the same physical
glyph masks. Its bitmap view has physical dimensions; `nx_text_stamp_bounds`
provides the logical rectangle relative to the original layout origin. The
ordinary stamp draw/label/action paths use that rectangle automatically. The
legacy constructor uses density 1. Constructors clear the output on error;
keep the previous handle in a separate variable until a replacement succeeds.

```c
nx_text_stamp *candidate = NULL;
nx_result result = nx_text_stamp_create_scaled(layout, window_density,
    2 * 1024 * 1024, NULL, &candidate);
/* If successful, use candidate for subsequent frames. Release replaced stamps
   after their renderer consumption; on failure retain the old handle. */
```

Construction's own live allocations cap at 64 MiB. Final masks retain the
16384-per-axis and 4,194,304-pixel limits. Borrowed-font outline decoding uses
the font's separate allocator; its existing parser bounds still apply. Invalid
scales/physical ppem return ARGUMENT, resource limits return LIMIT, allocation
failure returns MEMORY. Failed direct draws roll back appended commands/masks.
Layouts/fonts/source bytes can die after stamp creation or direct recording;
stamps must survive renderer consumption, and direct masks survive until the
context's next begin-frame/destruction. Allocator user state outlives its stamp.

Higher density produces new outline samples rather than enlarging the old mask.
Raster phase remains fixed per glyph and bitmap placement remains nearest-neighbor.
There is no phase-aware AA, TrueType hinting, LCD rendering, color-glyph support
or new atlas here. Physical-pixel alignment helps preserve the baked phase;
logical ink bounds include raster padding and can differ slightly by density,
while the original layout measurements remain unchanged. The
[validation report](../quality/text-density-report.md) and
[comparison example](../../examples/16_text_density/README.md) record the scope.

## Complete single-line construction

A layout whose single line covers the complete input reuses its original
paragraph shaping. New line boundaries retain reshaping. Font borrowing,
measurements, selection/caret behavior and frame-owned draw masks keep their
existing contracts. The [text-cost report](../quality/text-cost-report.md)
records paired real-font timings, full old/current comparisons and the
remaining wrapped-text, application and CI limits.
