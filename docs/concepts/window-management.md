# Native window management (D01)

The public C17 contract and platform adapters are implemented. Executed checks
and remaining native/physical acceptance are tracked in the
[D01 report](../quality/window-management-report.md). An accepted request does
not establish a completed transition or full desktop acceptance.

The existing descriptor/info/event layouts remain unchanged. Two descriptor
flags are opt-in: `NX_WINDOW_CLIENT_CHROME` for a client-drawn title area and
`NX_WINDOW_STATE_EVENTS` for normalized native state notifications. Zero flags
retain existing window creation and event behavior. An event's new union alias
does not change its storage layout or earlier event enum values.

`nx_window_get_status` separates observed state, observable state bits and native
request capabilities. A successful asynchronous request is not a completed
transition. Unknown state bits must not be interpreted as a confirmed false
state. In particular Wayland has no minimized-state notification or direct
unminimize request; restore only unmaximizes/unfullscreens there. Windows/X11/
AppKit capabilities and confirmed behavior need separate native evidence.

Move/resize uses the last still-held native left-button press belonging to the
window. Protocol serials and retained native event ownership stay inside the
adapter. Releasing/cancelling the press invalidates it. The accepted gesture
cancels client capture. The native event loop can be nested; window teardown
must not occur reentrantly during an active native interaction.

## Adapter decisions

- Windows: retain overlapped-window management styles and system-menu behavior;
  suppress the standard nonclient caption only in client mode. Preserve resize
  hit testing and maximize work-area constraints. Delegate interactive movement
  and resizing to the native nonclient loop.
- Wayland: use xdg-toplevel move/resize with the cached seat/button serial,
  observe state at configure boundaries, and negotiate client-side decoration
  when the compositor exposes xdg-decoration. A rejected requested mode fails
  creation; protocol requests do not fabricate confirmed state.
- X11: use WM/EWMH messages and observed properties. A bare X server is not a
  window manager; unavailable management must report unsupported. Motif hints
  request client decoration; the actual WM remains responsible for honoring it.
- AppKit: retain native titled/resizable/minimizable window machinery with a
  full-size client title area and hidden standard buttons in client mode. Use
  public native drag APIs; resize gestures update the actual NSWindow frame from
  native pointer events with native frame limits, not just the rendered content.

Compared alternatives: exporting native handles would expose platform lifetime
and input provenance to applications; a generic always-successful shim would
hide compositor refusals. Both are rejected. Capability reporting plus native
adapters preserve the common C17 boundary while keeping actual limitations
visible.

## Acceptance still required

Default-window regression, invalid arguments, owner-thread/lifetime guards,
stale and valid gestures, actual native state/configure messages, decoration
negotiation and failure cleanup. Windows additionally needs native drag/resize,
Snap, Alt+Space/system menu, focus, DPI/monitor behavior and accessible controls.
macOS and both Linux backends need their own executed evidence. Physical
keyboard/IME/screenreader/full-design checks remain separate open requirements.

## Application integration

Create the window with `NX_WINDOW_CLIENT_CHROME | NX_WINDOW_STATE_EVENTS`.
After polling, route its events to the UI context as usual. Query
`nx_window_get_status` to update a maximize/restore icon; use `known` before
interpreting a bit and disable unavailable actions using `capabilities`.
`NX_EVENT_WINDOW_STATE` has no input side effect and appears in the frame's
event list only when opted in. It reports changed state bits; capability or
observability changes alone are not a state-change notification. Query status
when handling actions rather than keeping capability information indefinitely.

Register the title controls with `nx_button` and stable semantic IDs/names such
as “Minimize”, “Maximize”, “Restore”, and “Close”. Publish the completed semantic
tree with `nx_window_publish_semantics`. A keyboard or assistive activation uses
the same minimize/maximize/restore function as pointer activation. Changing a
maximize button to restore must update its name as well as its icon. Close must
still enter the application's existing worker/renderer shutdown sequence before
destroying the window. Hiding native buttons does not automatically create
accessible client controls.

Invoke `nx_window_begin_move` while processing a left press inside a title drag
region, excluding its buttons and text fields. Begin resize similarly from the
corresponding client edge. A frame can contain a press and its release together;
the later release deliberately invalidates the gesture. Handle
`NX_ERROR_STATE` without inventing another press or retaining an application
serial. The cancellation event after acceptance prevents the initiating client
control from later clicking. Native physical dragging, snap behavior and focus
need separate acceptance of the composed application.

Windows retains overlapped styles and native resize hit testing in client mode.
Alt+Space is forwarded to its system menu; legacy descriptor flags retain the
previous text/event route. Drag-edge snapping uses the native move loop. A custom
maximize control does not acquire the Windows 11 native hover Snap-layout flyout
through this API. Mixed-DPI/monitor and physical snap acceptance remain open.

Wayland uses compositor-side interactive operations. Without xdg-decoration,
the protocol's client-decoration default applies; with it, server-side refusal
returns `NX_ERROR_UNSUPPORTED`, including a later renegotiation. The application
must handle that error. Minimize cannot be observed or undone directly through
xdg-shell; applications must not claim successful unminimization from restore.
X11 client chrome is a Motif hint and can be ignored by the WM. AppKit resize
uses actual native event coordinates and NSWindow frames/limits; its move uses
the public Window Server drag method.

## Primary references

- [Microsoft WM_NCCALCSIZE](https://learn.microsoft.com/en-us/windows/win32/winmsg/wm-nccalcsize)
  and [WM_NCLBUTTONDOWN](https://learn.microsoft.com/en-us/windows/win32/inputdev/wm-nclbuttondown).
- The installed `wayland-protocols` xdg-shell and xdg-decoration XML files are
  the build's protocol authority; generated sources stay in the build directory.
  [Upstream protocols](https://gitlab.freedesktop.org/wayland/wayland-protocols).
- [EWMH moveresize and state](https://specifications.freedesktop.org/wm-spec/latest/ar01s05.html)
  and [ICCCM manager selections](https://www.x.org/releases/current/doc/xorg-docs/icccm/icccm.html).
- [AppKit performWindowDragWithEvent](https://developer.apple.com/documentation/appkit/nswindow/performdrag(with:)?language=objc)
  and [NSWindow](https://developer.apple.com/documentation/appkit/nswindow).
