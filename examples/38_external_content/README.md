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

These four cases are installed consumer/headless-render and native-thread checks.
The optional fixture below adds ordinary native composition and host focus.
The input adapter below adds a bounded normalized-key/absolute-pointer channel.
IME/candidates, engine accessibility subtrees, selected-engine input/crash
recovery and reviewed GPU interop still need integration.
A selected maintained engine and its real browser/document/
guest workflows remain external engineering gates. Guest pixels cannot serve
as host admission or permission UI. No application pin or 1.0 release changes.

The [source-bound report](../../docs/quality/external-content-report.md) records
the executed Linux profile matrix, complete-image checks and rejected generation,
snapshot-borrow and retired-pixel variants. Native/engine and Windows/macOS
consumer acceptance remain separate.

## Optional native fixture

On Linux, `-DNEXAUI_CONTENT_BUILD_NATIVE_FIXTURE=ON` additionally builds an
ordinary Wayland/software-window host and a separate libatspi/X11 test peer.
The peer's X11 dependency reads only the owned Xvfb/Weston output; the host and
SDK retain their Wayland-only presentation dependencies. Default builds do not
discover or link X11 for this example.

```sh
python3 examples/38_external_content/run_native.py \
  --build-dir /tmp/content-native --font /path/to/font.ttf \
  --registry /path/to/at-spi2-registryd --work-dir /tmp/new-content-run
```

Use the repository's existing private Weston tool configuration when required.
The runner owns its Xvfb seat, compositor work directory and assistive bus.
`--case pending-close` closes while a prepared producer allocation is held;
`--case rejected-close` closes after a late old-generation frame is rejected.
The default `full` case replaces the tab, releases that late frame, admits fresh
pixels, rejects an unsupported format, recovers and requests 2x content backing.
This is still a deterministic producer, not a selected browser or guest engine.

Native controls remain reachable by keyboard. The peer independently checks
stale object/focus rejection and entering/leaving the viewport with Tab. It reads
every pixel in the 448x208 content rectangle from the private X11 carrier window
and retains PPM captures; fresh/recovered/scaled content must match distinct
fixture colors, while rejected states show the placeholder. A successful SDK
software submit alone is not treated as displayed output. The worker uses the
existing public fd-wait API for completion wakeups, carries only copied request
metadata, and is stopped/joined before exchange destruction. Closing a tab does
not stop or replay its producer. The fixture closes its window with a held job
by releasing only its own worker gate during cleanup.

These frame cases cover ordinary native composition and host focus, not engine input
forwarding, IME/candidates, engine
accessibility subtrees, GPU import, physical output or final product design.
The [native source-bound report](../../docs/quality/external-content-native-report.md)
records all profiles, actual pixel captures, close/disposal counts and rejected
generation/display variants, including the earlier ineffective control.

## Explicit content input and host release

`input.h`/`input.c` are an app-owned example adapter over existing `nx_event`
values, separate from pixel staging. They add no installed SDK API, window
grab, worker or engine dependency. Create one gate with a nonzero session
generation, a configured escape key/modifier mask and an atomic batch sink.
The fixture uses **Ctrl+Shift+F12**, displayed before activation; product and
platform shortcut selection remains the application's responsibility.

Feed every normalized native event once, in order, including while inactive.
Queue only the returned host-event copy when `route_host` is true. This copy
removes modifiers still draining from a released content session. Do not route
the original event again. The native fixture drains `nx_window_next_event`,
queues host events with `nx_context_queue_event`, then uses
`nx_window_route_events` for pending native accessibility actions. This retains
the ordinary native-events-before-AX ordering. After each host frame/action,
synchronize actual content focus, availability and geometry with `set_target`
before polling more input. A failed enqueue is terminal for this fixture;
there is no fabricated successful route or retry of an already consumed event.

Activation requires explicit host intent, content and native-window focus,
an available target and all observed keys/buttons released. It sends FocusIn
with source-pixel dimensions. A queued Enter down cannot activate content and
leak its later up into the receiver. Focus regain or a newly arrived frame
never activates automatically. Dimensions are capped at 2^24 so integer pixel
coordinates remain exact in `nx_event` floats. Pointer coordinates are scaled
from logical half-open bounds, floored and clamped to source pixels. Wheel
deltas keep NexaUI's abstract scroll units. Leaving bounds releases input;
this absolute-pointer example does not promise relative mouse/OS capture.

Only fresh normalized key/button edges, pointer movement and wheel events
reach the sink. Duplicate downs, repeats, consumed-key downs and unpaired ups
do not create receiver actions. Native text/IME events are consumed while
active, so a physical key and its native text do not enter the guest twice.
This fixture does not enable native IME. It is **not** a text-input or full
guest-keyboard implementation: engine scancodes/keymaps, left/right modifiers,
IME/preedit/candidate geometry and unsupported keys/touch need a reviewed engine
contract. Unsupported physical keys/touch revoke with an explicit error.

Escape, host-focus loss, native focus loss, pointer cancellation, target
unavailability, geometry change and explicit release/close send one batch of
all locally admitted key/button ups followed by FocusOut under the old
generation. Later real ups/repeats drain without becoming host activations or
duplicate receiver releases. Native focus loss clears physical observations
because ups may never arrive; post-focus repeats cannot invent guest presses.
Historical pixels may remain visible after input disconnect, with a truthful
label and a disabled Enter control. They do not imply a live source.

The callback must admit the entire batch or none, copy/apply it before return,
and never retain or reenter a mutating call. **NX_OK proves only local admission,
not remote application or remote reset.** A callback failure deactivates and
fences the gate, preserving its uncertain accepted-hold ledger and error. No
automatic reset retry, replay or recapture occurs. Close frees even on release
failure and returns the uncertainty to the owner. Rebind requires an inactive
gate, a strictly newer generation and the owner's independent retirement/reset
of the old backend; it is an explicit assertion, never a remote acknowledgement.
Rebind preserves draining physical input and requires a new host target/intent.
The object uses one bounded allocation and one owner thread throughout.

The five input CTests use a separate atomic receiver model and check actual
admitted events, stuck holds, scaling, escape isolation, host draining, all
revocation causes, identities, invalid/unsupported inputs, reentrancy and sink
failure. The optional `--case input` additionally runs the complete frame case,
then real seat keys/buttons, 2x coordinates, the configured escape, native focus
loss and accessible Disconnect. Its receiver is explicitly a local test model,
not a VM/backend. Native Windows/macOS, real engines, guest applied/reset
acknowledgements, text/IME, sharing/reset authorities, engine accessibility and
physical/human acceptance remain open.
