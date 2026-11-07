# Hierarchy and property inspectors

An inspector can compose `nx_tree_flatten`, `nx_tree`, `nx_tree_item`, and
`nx_property_row`. The application owns the hierarchy, expansion flags,
selected ID and property values. A tree node array is in preorder;
`nx_tree_flatten` returns the visible source indices for the current expansion
state. Only register visible rows. Stable IDs let selection and focus survive
changes to their visible positions. The expander has its own hit region, so
clicking it changes expansion while clicking the rest of an item selects it.
Left/Right expansion applies to the row or expander that held logical focus
when that key event was routed. A later Tab, pointer click or application focus
change in the same frame does not transfer an earlier key to another branch.

Within the nearest tree or table, Up/Down move focus between registered item
rows and Home/End choose the first/last eligible row. The expander buttons do
not add stops to directional navigation; Tab still reaches them. Navigation
stops at the ends, skips disabled/inactive rows and their subtrees, and leaves
the application-owned selection unchanged. Enter/Space retain their existing
selection/expander behavior.

Right expands a closed branch; on an already expanded branch it focuses its
first eligible registered child. Left collapses an expanded branch, otherwise
it focuses its eligible parent item. These relationships follow semantic item
ancestry, not indentation or row indices. Items in separate table cells share
table-scoped row navigation; their logical hierarchy is not inferred from cell
geometry. Arrow-induced collapse also hides descendants from later navigation
in that same event batch. Consumed key events do not navigate.

The previous completed frame defines available targets. A newly expanded or
asynchronously loaded child becomes navigable after the application registers
it. Applications own loading/error/retry state, virtual-row realization and
scrolling to reveal focus, and choose focus restoration when a row disappears.
No navigation step requests data or changes selection/operation authority.

Between completed frames, `nx_semantics_request_action` accepts the branch's
currently offered `NX_ACTION_EXPAND` or `NX_ACTION_COLLAPSE`, on either the row
or its expander. These are state-setting requests: they neither toggle an
already updated expansion value nor click, select or focus the row. Tree branches
and collapsible-section headers have this binding; arbitrary custom widgets do
not. The next frame revalidates ID, role, parent, overlay and enabled ancestry.
Omitted, replaced, disabled or reparented targets drop their old requests. Painting
failure leaves the caller's expansion unchanged and permits retry in that frame.
This requests presentation expansion only; applications still own async loading,
errors, retry correlation, cancellation and stale-result rejection.

`nx_property_row` accepts a boolean, finite ranged number, or UTF-8 text
value. It copies the accessible name and current value into the frame's
semantic tree and paints an optional font-backed label. The minimum row
height is 16 logical pixels. With a font, supply a finite `pixels_per_em`
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
