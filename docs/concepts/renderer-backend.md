# Internal renderer backend contract

`src/renderer_backend.h` is a private C17 boundary between the frame command
stream and a renderer. It contains no Win32, Cocoa, Wayland, X11, D3D12,
Metal, or Vulkan types. The initial Windows D3D12 backend plugs into this
boundary for clear/present frames. An `nx_renderer_driver` owns one copied backend
callback table and tracks its current target size, backing scale and device
loss state. The caller zero-initializes it, opens it once, serializes all calls
on one renderer thread, and closes it after backend work has completed.

`nx_renderer_run` receives a complete command array in paint order, physical
target width and height, backing scale, and clear color. Commands retain
logical-pixel coordinates; a native backend applies the target scale when
mapping them to physical pixels. The driver validates the full stream before
calling a backend. Validation checks each command's geometry, color, bitmap
view and balanced clip stack. Thus a malformed stream cannot start a resize
or submit. The headless renderer uses the same preflight, then paints its
physical RGBA8 output. Headless callers that want a scaled image currently
need to supply appropriately scaled commands; it does not yet apply a backing
scale itself.

The backend receives `resize` only when physical dimensions or scale change.
If either dimension becomes zero, it receives `(0,0)` and no submit. A
temporary surface suspension returns `NX_RENDER_SUSPENDED`; the driver retries
resize on a later run. If submit reports suspension, the cached target is
invalidated so the next run also resizes. `NX_RENDER_DEVICE_LOST` latches the
driver in a lost state. No more backend work is submitted until an explicit
`nx_renderer_recover` succeeds, after which the next run resizes again.
`NX_RENDER_READY` means submit finished consuming borrowed command data, not
that a display has presented. An `nx_result` error represents a failed call;
callbacks must leave borrowed data unretained on every return and preserve a
retryable backend state or report device loss.

The backend owns device, swap chain, uploaded textures, descriptor pools,
staging buffers and GPU fences. The core command stream carries borrowed
RGBA8/A8 bitmap views rather than reusable native resource handles. A native
submit must copy or upload their bytes into backend-owned storage before it
returns `NX_RENDER_READY`; it may let GPU execution continue afterward.
`destroy` releases the backend's resources after outstanding GPU work is
retired. Its callback and `nx_renderer_close` return `nx_result`: an error
retains the callback table, backend pointer and open state for another close
attempt. Once close starts, render and recovery calls return `NX_ERROR_STATE`.
Only successful close clears the driver; closing NULL or an already closed
driver returns `NX_OK`. Applications must preserve the backend's borrowed
window until that success. Native handles stay private to each renderer instance and cannot be
reused after its teardown. Upload caching, explicit resource handles and
backend-specific fences remain future work.

The mock backend test exercises command preflight, content forwarding,
unchanged-size reuse, scale changes, zero-size suspension, temporary
suspension, device loss from resize and submit, failed and successful recovery,
and failed-close ownership retention followed by successful, idempotent close.
The test calls the headless renderer inside submit to
check the forwarded command stream against pixels. It does not validate
asynchronous presentation. The separate D3D12 smoke test checks device,
queue, swap-chain and F11 adapter lifecycle on Windows, then forces real
device removal where supported and submits again after recovery. A test-only
GPU readback compares filled rectangle and triangle pixels with the headless
reference at 1× and 2×, including layered alpha over an opaque clear.

The Linux Vulkan adapter accepts empty streams and initial opaque filled
rectangle/triangle streams, and uses explicit per-image
presentation fences by default. Its native tests exercise close/recovery/resize
timeouts after real submissions and verify that a pending close retains the
window lease. Drawing tests compare exact headless pixels across scales,
fractional/shared edges, failed upload growth and renderer rebuild. See [the Vulkan contract](vulkan.md) for extension requirements
and explicit compatibility modes.
