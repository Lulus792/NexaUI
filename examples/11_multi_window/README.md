# Two native windows

Build on Windows with `NEXAUI_BUILD_EXAMPLES=ON`, then run
`nexaui_multi_window_example`. It opens two Win32 windows with different
colors. Click or type in either window: the yellow bar counts pointer presses,
the green bar counts text input, and the accent outline marks focus. Resize
either window independently. Closing one leaves the other running; close both
to exit.

Each window owns an `nx_context` and a D3D12 renderer driver. A single
`nx_window_poll` dispatches the thread's messages; `nx_window_route_events`
then sends each window's queued events only to its matching context. Each
context produces its own command stream, rendered to that window's current
physical client size and backing scale. Drivers are destroyed before their
windows.

`nexaui_multi_window_example --smoke` uses hidden windows and exits after
checking independent A/B key states, rendering both windows, closing the
first, then delivering text and rendering the survivor. CTest runs this mode.
The example uses the build-tree D3D12 driver, which is currently private and
is not part of the installed public package API.
