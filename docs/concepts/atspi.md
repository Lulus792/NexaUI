# Initial Linux AT-SPI transport

`nx_window_publish_semantics` publishes completed semantic frames on the Linux
accessibility bus. The NexaCloud Vulkan runner calls it after building each frame.
The transport implements its own object hierarchy, role/state mapping, request
queue and AT-SPI D-Bus messages. It uses no GTK/ATK provider objects.

## Build and runtime

The private adapter uses the system `dbus-1` development headers when available.
`NEXAUI_ENABLE_ATSPI=OFF` disables it explicitly. The installed public header and
package have no D-Bus, libatspi or GLib types/dependencies. Linux links its system
dynamic loader; `libdbus-1.so.3` is loaded only for accessibility, with all required
entry points checked before use. libatspi and GLib are used only by the separate
test client.

`AT_SPI_BUS_ADDRESS` selects an existing accessibility bus. Otherwise, with an
explicit `DBUS_SESSION_BUS_ADDRESS`, the adapter asks `org.a11y.Bus.GetAddress`.
It never creates an arbitrary session bus when no address is configured.
`NO_AT_BRIDGE=1`, a missing SDK/runtime ABI or an unavailable bus returns
`NX_ERROR_UNSUPPORTED`; applications can continue normal input and rendering.
Unavailable connections are retried on a later publication, at most once per
second. Runtime ABI loading is checked once per process; installing a missing
runtime requires restarting the process. Passing a null context detaches the bridge, including an unavailable one.

Connection/authentication uses libdbus's synchronous system entry points.
`GetAddress` has a 250 ms reply timeout. Registry `Socket.Embed` is asynchronous
with a two-second timeout, allowing the registry to call application properties
without deadlocking the UI thread. After connection, I/O is nonblocking and each
pump dispatches at most 64 messages. X11 and Wayland poll the bus descriptor
alongside their native display/transfer descriptors, including write readiness.

## Ownership and protocol

Each UI thread owns one application endpoint at the required
`/org/a11y/atspi/accessible/root`, with its published native windows beneath it.
Independent UI threads have separate endpoints. The hub uses thread-local storage;
it does not retain the first window's allocator after that window is destroyed.
Its stable application label copies the first root name when it fits the fixed
1024-byte metadata field, otherwise using `NexaUI`; complete window/node names
remain in the owned snapshots.
Every window's bridge and two reusable snapshots use its allocator. Each snapshot
is limited to 32 MiB and 65,536 nodes. Query replies own their D-Bus data; no query
dereferences a published context or borrowed string span.

Accessible, Application, Component, Action, explicit single/multiple Selection
and bulk Cache interfaces are implemented. Queries expose hierarchy, names,
descriptions, roles, states, enabled/focus/selection state, bounds and hit testing.
Protected values are empty. Ordinary scalar semantic values are available as the
`nexaui-value` attribute; this is not a Text, EditableText or numeric Value
implementation. Unsupported interfaces are absent from `GetInterfaces`.

Object paths contain window and node incarnations. Consecutive unchanged
context/ID/role nodes retain their identities; removal, retyping, protection-type
changes and context replacement retire them. Reconnection uses a new unique bus
name and new window paths. Requests to retired paths return a native Defunct error.
Malformed arguments and unavailable interfaces have explicit D-Bus errors.
`GetSelectedChild` returns the AT-SPI null accessible reference for an absent
selected ordinal, including negative ordinals; selection mutators return false
for unavailable members.

The bulk cache uses `a((so)(so)(so)iiassusau)`. Replies have a conservative 4 MiB
budget; oversized trees retain lazy Accessible queries and return LimitsExceeded
for bulk requests. Incoming messages/queued incoming data are bounded through the
SDK. A queued outgoing backlog of 8 MiB causes disconnection before another send;
a single already-built reply can exceed that threshold, bounded by its snapshot.
A stalled bus does not grow an unbounded application request/event queue.

## Actions and selection

Press, increment/decrement, expand/collapse, focus and selection operations are
advertised only where the completed semantic node supports them. Accepted requests
enter a queue bounded to 64 pending plus in-flight requests and are routed through `nx_window_route_events` on the
window/context owner thread. Core backpressure preserves pending native work.
Acceptance does not mutate the application model or publish speculative state.

Explicit single-selection groups serialize their operations until a newer
completed frame acknowledges forwarding. Repeating a pending identical operation
is idempotent; a conflicting operation is rejected. Required selections cannot be
cleared. Selection updates the application-owned ID without synthesizing a click.
Context/frame and owner-incarnation checks reject stale requests. A real
connection-loss fixture requires re-registration, new native identities and
successful model actions after reconnection.

`SelectChild`, `DeselectChild` and `IsChildSelected` use direct child indices.
`GetSelectedChild` and `DeselectSelectedChild` instead use ordinals within the
complete published selected set. `NSelectedChildren` counts that complete set.
SELECT-capable members of an explicit
group are therefore projected directly beneath that group, bypassing intermediate
layout panels. Those panels remain in the tree, and original ancestry still
governs disabled inheritance and scroll clipping. Nonselectable direct children
retain their actual indices. Nested explicit groups retain independent ownership.

Bound multiple-selection owners expose MULTISELECTABLE. Only model-bound members
belong to their selection catalog; an unbound selectable sorting header remains
a separate child. `SelectChild` adds one member. `SelectAll` and `ClearSelection`
admit one atomic complete-set request rather than a sequence of item actions.
Logical offscreen members participate, while active ID and keyboard focus remain
independent. Disabled members must keep their existing selected state, and a
required owner cannot become empty. Conflicting operations on the same owner
reject until publication acknowledges forwarding; equivalent pending operations
are idempotent.

Native complete-set catalogs and desired IDs own at most 8 MiB altogether,
including requests temporarily removed from the queue during core allocation.
Retained C leases contain scalar identities and owned native snapshot access;
they never retain the application context or model. Context/model/member/owner,
raw ancestor, policy and Multi-to-Single changes revoke old requests. Once a
disconnect is dispatched, the previous connection's leases are revoked before
re-registration. The application must continue publishing to retry an unavailable
transport. Window and bridge retirement revoke immediately; physical release
waits for routing/publication operations that crossed application allocator
callbacks. Single-to-Single held-node compatibility is preserved.

Cache add/remove, child changes, accessible name/description/parent changes,
selected/checked/focus/enabled/showing state and selection changes are emitted from
committed publications. Old object references are encoded before their snapshot
storage is reused. X11 also emits screen-space BoundsChanged notifications.
Newly published focused incarnations additionally emit a focused state event
after insertion; cache state alone is insufficient for the tested Orca modal.
See the [focus regression and real Orca task](../quality/orca-focus-report.md).

## Coordinates and validation limits

Component queries support window and parent coordinates. X11 obtains actual root
coordinates for screen queries. Wayland does not provide a global surface origin:
screen queries return NotSupported, and screen-space BoundsChanged is omitted.
GrabFocus on Wayland can focus a control in an already keyboard-focused surface;
it does not claim to activate an arbitrary application. Actual native focus is
required for the exposed focused state.

The [native report](../quality/linux-atspi-report.md) distinguishes independent
libatspi/registry tests from manual screenreader acceptance. Human Orca acceptance, complete
text/range/table/collection/relations contracts, full event coverage, broader
desktop/distribution/architecture and physical scaling acceptance remain open.
The [multi-selection report](../quality/linux-multiselection-report.md) records
the new local isolated X11/Wayland evidence, negative providers and the separate
maximum-catalog C component. External complete-set coverage is 67 members;
65,533-member payload accounting is component evidence, not external IPC proof.

Protocol references: GNOME's [interface overview](https://gnome.pages.gitlab.gnome.org/at-spi2-core/devel-docs/xml-interfaces.html)
and primary [XML declarations](https://github.com/GNOME/at-spi2-core/tree/main/xml).
The native client checks the installed SDK behavior as well as direct wire errors;
an empty/false libatspi convenience result alone is not proof of a correct reply.
