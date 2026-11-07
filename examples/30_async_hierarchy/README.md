# Asynchronous hierarchy presentation

This installed public-C17 consumer composes the existing tree, status, button,
scroll, font/theme, command/completion and native poll APIs. The view owns only
presentation state and a bounded copied response. Its test producer returns
declared `Child A`/`Child B` observations; it is not another Files model,
component catalogue, picker, privileged client or production hierarchy service.
Aetheri's original sources, models and SDK pin are not imported or modified.

The example requires the native expansion fix from SDK source
`ac0fb3b3b3aa53b2a4de927ec52a06deb33d5c36` or a compatible later source. The package
version remains 0.1.23; a version-only lookup cannot distinguish earlier 0.1.23
artifacts without that fix. Record and verify the selected SDK source/artifact.

```sh
cmake -S examples/30_async_hierarchy -B out/async-hierarchy \
  -DCMAKE_PREFIX_PATH=/absolute/installed/sdk -DCMAKE_BUILD_TYPE=Release
cmake --build out/async-hierarchy
ctest --test-dir out/async-hierarchy --output-on-failure
```

The portable test is also registered as `nexaui_async_hierarchy` in the normal
root test build. Linux additionally builds the ordinary native test host. Set
`AH_BUILD_ATSPI_CLIENT=ON` when OS `atspi-2`/`gobject-2.0` development files are
available to build the independent peer. That peer links no NexaUI code.

## Presentation and owner contract

The owner creates `ah_view` with a live borrowed `nx_sim_channel`, immutable
borrowed font collection and nonzero unique view epoch. Font bytes/collection
and channel outlive the view. The view uses the documented IDs 10–17; the host
must keep them distinct from its other controls. Child IDs are owner-supplied,
distinct and greater than 17, with nonempty bounded valid UTF-8 names. There is
no conversion from these UI IDs to filesystem paths or operation authority.

Build the view inside a context frame. Only after successful `nx_end_frame`
does the owner accept a returned Load/Retry/Refresh intent with `ah_request`.
An accepted channel command reserves completion capacity and changes the view
to Loading. Queue-full/allocation failures show a retryable failure; they never
look like successful loading or completion. A failed widget/frame construction
does not by itself post a request. Normal expansion of already loaded children
uses its cached observation; a failed load needs an explicit Retry. Repeated
Right while Loading does not submit more work.

The command contains only a copied `{view epoch, request generation, branch}`
key. A producer takes/disposes that command exactly once and publishes one
completion using its ticket. A successful payload contains the same copied key
and at most 16 child records; this response bound belongs to this example, not
to NexaUI's tree API. The owner takes the completion, calls `ah_observe`, then
disposes the message exactly once even when the view ignores it. Publication
requires its current ticket, generation, epoch and expanded/loading state.
An unrelated/retired ticket is ignored. A malformed active reply, including a
wrong key for the active ticket, becomes a failure rather than leaving the
view permanently busy. An empty successful observation is distinct from an
unqueried or failed observation.

Collapsing a pending branch retires its publication target. It does not assert
that a backend cancelled an already taken command or release that command's
channel reservation. The owner must still drain/dispose its later completion.
Explicit refresh supersedes older work. A replacement view gets a new epoch;
the host retires native semantics before destroying the old context. Generation
exhaustion does not wrap: the UI asks the owner to reopen the view and disables
Retry. The real data owner must also invalidate/supersede requests when its
source identity, revision or service session changes. The view cannot discover
those domain changes or reconcile a service on the owner's behalf.

Loading is a Busy status, errors have an explicit Retry, and empty/ready views
offer Refresh. Scrolling keeps controls and a focused bounded child fully
reachable, including at the 360×260 native test size with 200% text. Removing a
focused child or Retry/Refresh restores branch focus; the owner builds a second
frame when requested before publishing, so the focus cue matches that frame's
native state. Application selection survives cached collapse/re-expansion and
is reconciled when an accepted new observation removes its selected child.

The view uses theme colors, an opaque software surface and no animation.
English/German and 16/32-pixel text are exercised. Its font layouts are cached
by content revision, width, text size, locale and status. Large or paginated
hierarchies should compose the existing virtual-tree/list contracts with their
actual owner model; the bounded example is not a 100,000-row adoption proof.

## Native controlled-worker workflow

```sh
python3 examples/30_async_hierarchy/run_native.py \
  --server /absolute/private/test_compositor_server \
  --registry /absolute/at-spi2-registryd \
  --host out/async-hierarchy/async_hierarchy_native \
  --client out/async-hierarchy/async_hierarchy_atspi \
  --font /absolute/LiberationSans-Regular.ttf \
  --work-dir /absolute/fresh-short-directory --capture
```

The caller supplies reviewed existing assets and private OS runtime libraries;
the test downloads or installs nothing. Optional `--german --large --small
--contrast` and `--light` exercise the other presentation cases. This is an
explicit test invocation, not an automatically passing native CTest.

The driver creates its own 0700 directories, compositor/seat, bus, registry,
control pipe and processes. Two actual pthread workers read only copied request
keys, block on a condition until explicitly released, post through the existing
bounded channel and wake the owner through the application FD. The UI sleeps
with `nx_window_poll_with_fd(..., UINT32_MAX, ...)`; it needs no frame timer or
polling worker. The same test pipe carries producer permits and wake bytes.
Permits are test controls, not trusted-user or production service messages.

Eighteen independent AT-SPI/native-key stages cover loading without duplicate
requests, a failed observation and keyboard Retry, loaded selection/focus, cached
collapse/re-expansion, explicit refresh, out-of-order completion, empty state,
context/view replacement, an old-view reply, and a native Refresh action. They
check Busy status, actual rows and selected/focused state, full native viewport
containment, defunct references and fresh identities. Acknowledging an action
alone is insufficient. F4 replaces the test view, F5 refreshes and Escape closes;
these are fixture commands, not new toolkit shortcuts.

Finally the host tears down semantics, view, context, renderer and window before
releasing/joining the last held producer. That producer can complete only to
the still-live channel/pipe. The owner then drains/disposes remaining results
and destroys the channel. Each successful workflow requires seven submissions,
seven completions, four adopted observations, two ignored stale replies and one
post-UI-teardown result drained, with both workers joined and normal exit 0.

Physical input/screens/readers, installed IMEs, real hierarchy services, durable
recovery, complete pickers, cross-platform native hosts and performance budgets
remain separate gates. See the [quality report](../../docs/quality/async-hierarchy-report.md)
for exact sources, actual profiles, counterexamples and retained evidence.
