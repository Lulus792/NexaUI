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

## Automated coverage and remaining acceptance

Executed tests cover default-window regression, invalid arguments, owner-thread/
lifetime guards, stale and accepted gesture requests, native state/configure
messages, decoration refusal/cleanup and restored dimensions. The report lists
independent Windows, AppKit, X11 and Wayland evidence and source revisions.
Windows system-menu forwarding and resize/move loop entry are checked through
native messages. Physical drag/Snap, mixed DPI/monitor behavior, real keyboard/
IME, human screenreader use, composed accessible controls and full-design
acceptance remain open.

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
Restore retains the last normal client dimensions when a configure leaves an
axis to the client (size zero), while honoring explicit compositor dimensions.
Fullscreen restore waits for the exit configure before unmaximizing an
underlying maximized state; state notifications continue to report observations.
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

## Linux application-owned wake descriptor

On Linux, `nx_window_poll_with_fd` combines the ordinary owner-thread native
window and accessibility dispatch with one borrowed application-owned readable
descriptor. It returns `NX_WINDOW_FD_READABLE`, `NX_WINDOW_FD_HANGUP` and
`NX_WINDOW_FD_ERROR` flags without reading, closing or retaining the descriptor.
`NX_BUILD_LINUX_POLL_FD` in `nx_get_build_features` identifies the compiled
Linux capability; it does not establish that a native display is available.
The caller keeps it open for the entire call, drains and interprets its data,
and must handle data and hangup together. A closed/invalid descriptor produces
the error flag; it is not silently consumed. A successful call can return no
flags when native input or an accessibility deadline wakes it first. This API
does not make windows or contexts thread-safe: workers should communicate
through application-owned bounded data, and the owner thread alone updates UI.
The Linux Settings example demonstrates one correlated reply and joins its
short-lived producer before closing the descriptor and scene. Other platforms,
multiple descriptors, application service lifetimes and cross-process
authorization require their own contracts.

## Host-owned Wayland surface

`<nexaui/wayland_host.h>` exposes `nx_wayland_shm_host_*` on Linux through
`NexaUI::NexaUI`. The host supplies borrowed `wl_display`, `wl_shm`, `wl_surface` and a
chosen event queue on one existing connection. It alone selects and creates the
surface role, handles initial configure/acknowledgement, input, semantics,
viewport and buffer-scale state, event dispatch, flushing and final teardown.
The adapter opens no connection and does not confer authority. Only one
presenter may attach buffers to that surface at a time.

After configure, the host passes physical dimensions and the matching scale;
the adapter rasterizes borrowed commands, converts straight RGBA into
premultiplied ARGB8888, attaches and commits a buffer together with pending host
surface state. The host must ensure its own viewport/scale and dimensions are
compatible. READY means queued, not flushed, displayed or accepted as a role.
Three buffers of at most 128 MiB each may await release. On BUSY, dispatch the
chosen queue and retry after events, without a permanent polling thread. The
presentation call does not dispatch or flush the host connection. An optional
`nx_wayland_shm_host_poll` dispatches the selected queue, flushes the connection
and waits for it plus one app-owned readable descriptor. The host must serialize
all readers of that connection; other event queues are not dispatched. Already
queued events return promptly, and callbacks cannot reenter that adapter's
present, poll or destroy calls. The descriptor is neither consumed nor closed;
readable/hangup/error are reported independently using the `NX_WINDOW_FD_*`
flags. The host chooses redraw, worker and accessibility deadlines; this poll
does not integrate an AT-SPI provider by itself. The adapter uses unlinked
temporary files in `/tmp`; destruction releases adapter-owned proxies and
mappings without destroying borrowed objects. The owner thread must stop all
adapter use before destroying those objects or its connection.
Once a display error has been observed, presentation returns a platform error;
an error detected after commit cannot establish whether that frame was received.

The [standalone host example](../../examples/19_wayland_host/README.md) uses an
ordinary host-owned xdg role. A controlled test uses a distinct fake private
role and checks initial acknowledgement, queue isolation, transparency,
backpressure and lifetime. Neither proves actual Aetheri private-role policy,
input/AT-SPI integration, three-output behavior, physical presentation or
display-loss recovery or re-establishment of a role after a disconnect.
