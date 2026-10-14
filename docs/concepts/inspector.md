# Hierarchy and property inspectors

An inspector can compose `nx_tree_flatten`, `nx_tree`, `nx_tree_item`, and
`nx_property_row`. The application owns the hierarchy, expansion flags,
selected ID and property values. A tree node array is in preorder;
`nx_tree_flatten` returns the visible source indices for the current expansion
state. Only register visible rows. Stable IDs let selection and focus survive
changes to their visible positions. The expander has its own hit region, so
clicking it changes expansion while clicking the rest of an item selects it.

`nx_property_row` accepts a boolean, finite ranged number, or UTF-8 text
value. It copies the accessible name and current value into the frame's
semantic tree and paints an optional font-backed label. The minimum row
height is 16 logical pixels. With a font, supply an integer `pixels_per_em`
from 1 to 16384 and a positive glyph-cache budget. A number needs a finite
`minimum < maximum`, an in-range value, and a positive finite `step`. A unit
can contain at most 64 UTF-8 bytes.

The row does not mutate the supplied value. Boolean activation returns a
toggle proposal. Focused Left/Right on a number returns a clamped step
proposal. Activating a number or text row returns `edit_requested` so the
application can open its own editor and validate the completed draft.
Read-only rows expose their value without a proposal. A successful proposal
appears in the semantic value and painted row for the current frame. The
application decides whether to accept it; the next frame reflects whatever
value it supplies. On a failed row call, no action is returned and its
semantic, hit-region, command, clip, and owned glyph-mask changes are rolled
back so the caller can retry.

For a simulation, acquire an `nx_snapshot_view`, build the inspector from its
immutable payload, and release the view after frame construction. Translate
accepted proposals to `nx_sim_channel` commands. The simulation applies and
validates commands, then publishes a new snapshot. The next UI frame uses
that snapshot as the authoritative value. Keep the selected ID and tree
expansion in UI-owned state, separate from the simulation payload.

The [headless tool example](../../examples/09_inspector_headless/README.md)
selects an entity, proposes an intensity change, accepts it between frames,
and renders the resulting inspector.
