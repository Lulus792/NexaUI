# External CPU content: snapshot to viewport

This standalone installed-C17 consumer connects the existing snapshot exchange
and viewport APIs. It demonstrates CPU frame ownership for an app-owned content
slot. Its producer is a labelled deterministic test fixture. It selects no
browser/PDF/VM engine and grants no service, guest or permission authority.

```sh
cmake -S examples/38_external_content -B /tmp/content-view \
  -DCMAKE_PREFIX_PATH=/path/to/nexaui-prefix -DCMAKE_BUILD_TYPE=Release
cmake --build /tmp/content-view
ctest --test-dir /tmp/content-view --output-on-failure
```

NexaUI 0.1.33 is required. `content_surface` links only `NexaUI::NexaUI`;
the test executable additionally uses native OS threads. The example is not
added to the root SDK build or installed API.

## Integrating the frame boundary

The app owns an `nx_snapshot_exchange`, producer lifetime, wakeup channel and a
nonzero content-session generation. Create a `content_surface` with that borrowed
exchange, generation and explicit payload/staging byte budget. Use a fresh
generation after tab/session replacement, even at the same dimensions. Keep the
exchange alive until all surfaces and producer jobs using it are finished.

Inside an ordinary UI frame, draw host controls and call `content_surface_draw`
with an `nx_viewport_desc`. Logical bounds and scale use the existing viewport
rules. A successful resize records a new `(generation, resize, width, height)`
request; read it with `content_surface_get_status` and send a copy to the app's
producer once per new request. The adapter starts no thread or engine command.
Missing or stale frames return `NX_ERROR_STATE`; the viewport rolls back while
host controls remain paintable. The app can show a waiting/error placeholder.

The producer publishes one allocation containing `content_frame_header` followed
by exactly `height * stride` bytes. This is an in-process C object contract,
not a wire or file format. Fields must echo the request, specify straight-alpha
`NX_BITMAP_RGBA8`, and use a nonzero sequence that increases for each changed
frame within that request. Equal sequences denote the same immutable pixels
and reuse the last staged content. Padded rows are supported within the declared
budget. Finish writing before `nx_snapshot_exchange_publish`; ownership transfers
only on `NX_OK`. A thread-safe disposer releases the allocation. Producer memory
and outstanding work need their own app bounds; the surface budget does not
limit arbitrary engine allocations before publication.

The acquire callback validates identity, sequence, dimensions, stride, format,
payload length and budget. It copies a new accepted frame into bounded packed
staging, then releases its snapshot borrow **before UI `nx_end_frame`**. The
renderer subsequently borrows staging through the ordinary viewport lease.
After ending and rendering the frame, call `content_surface_complete` before
reusing/destroying the surface. Do not retain the frame commands afterwards.
Completing a deliberately discarded frame is also allowed if no renderer will
use it. Repeated unchanged frames allocate and copy no new staging payload.

Resize allocation/budget failure preserves the previous request, pixels and
sequence. Transition to zero-size suspension retires staging and advances the resize ID.
Destroy returns `NX_ERROR_STATE` while a viewport lease remains active and
preserves the surface for later completion. Before destroying a published native
context, switch/detach its native semantics using the existing window contract.
Closing a view neither stops its producer nor destroys the borrowed exchange.
Late producer completion can publish safely; a replacement's fresh generation
rejects those old frames. Stop/join the producer before destroying the exchange.
Use the app's existing wakeup integration when new content arrives.

## Executed test scope

The four cases compose an ordinary host button and the content slot, comparing
complete 64x64 RGBA output against a separate reference composition. They cover
new/unchanged frames, padded stride, scale resize, suspension, byte-budget and
allocation failures, malformed metadata, decreasing sequence, active-render
close refusal and a real worker completing after view/context replacement.
They also acquire the exchange again before end_frame and retire the producer
allocation before rendering, checking the two distinct lifetime contracts.

These are installed consumer/headless-render and native-thread checks. Native
window presentation, content input/escape routing, IME/candidates, engine
accessibility subtrees, engine crash recovery and reviewed GPU interop still
need integration. A selected maintained engine and its real browser/document/
guest workflows remain external engineering gates. Guest pixels cannot serve
as host admission or permission UI. No application pin or 1.0 release changes.

The [source-bound report](../../docs/quality/external-content-report.md) records
the executed Linux profile matrix, complete-image checks and rejected generation,
snapshot-borrow and retired-pixel variants. Native/engine and Windows/macOS
consumer acceptance remain separate.
