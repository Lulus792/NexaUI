# Host-owned Wayland surface

This standalone C17 example opens and owns its Wayland connection, event queue,
`wl_surface` and ordinary xdg-toplevel role. It waits for initial configure,
acknowledges it, then uses `nx_wayland_shm_host_*` to commit a transparent
NexaUI frame. A short-lived worker writes a bounded reply to a pipe; the owner
thread waits for Wayland events and that descriptor together, reads the reply,
joins the worker and closes. The SDK does not create the role or a second
connection. The example exits after one submitted frame; it is not a dock,
input router, accessibility provider or proof of physical presentation.

Against a Linux NexaUI SDK and system Wayland client/protocol development files:

```sh
cmake -S /path/to/copied-example -B host-build \
  -DCMAKE_PREFIX_PATH=/path/to/nexaui-sdk
cmake --build host-build
LD_LIBRARY_PATH=/path/to/nexaui-sdk/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH} \
  ./host-build/nexaui_wayland_host_example
```

Copy this directory to build completely outside the SDK source tree. The shared
library path is needed only when the installed SDK is not in the system loader
configuration. `wayland-scanner` and the xdg-shell protocol XML generate the
host's protocol bindings; the SDK does not implement or choose this role.

A private shell role must be created and configured by its own authorized host,
which also owns scale/viewport state, input, semantics and dispatch. See the
[host contract](../../docs/concepts/window-management.md). The example does not
bind Aetheri's private protocols or claim its compositor has accepted the SDK.
