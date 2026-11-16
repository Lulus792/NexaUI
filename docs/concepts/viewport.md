# Custom viewport surface

`nx_viewport` embeds an application-rendered RGBA8 image in the ordinary
paint-order command stream. The application supplies a `nx_viewport_source`
with `resize`, `acquire` and `release` callbacks and owns its pixels. The
source can render a software 3D scene, an external engine's CPU preview, or
another image producer. It is independent of simulation data: a producer can
read an acquired immutable `nx_snapshot_view` while building the frame and
release that view after its own rendering and command generation are done.

The descriptor bounds are logical pixels. NexaUI computes physical width and
height as `ceil(logical extent × scale)`, with each dimension limited to
16,384 pixels. It calls `resize` only when those pixel dimensions change.
Zero width or height suspends the surface with `resize(0, 0)` and registers no
widget or bitmap. A failed resize must leave the producer's previous surface
usable; NexaUI keeps its previous size so the operation can be retried.

For a nonempty viewport, the call registers one focusable semantic widget and
hit region, then calls `acquire`. The returned bitmap must be valid straight
alpha RGBA8 at exactly the requested pixel size. NexaUI draws it with a
clip and a 1:1 color tint, scaled into the logical bounds by the renderer.
The returned `nx_interaction_state` exposes the same pointer and focus signals
as other custom widgets. The application interprets camera or tool gestures;
NexaUI does not mutate scene state.

An acquired bitmap is a **lease**. Pixels must stay readable and unchanged
through all rendering of the frame commands, including after `nx_end_frame`.
The caller then invokes `nx_viewport_surface_complete`, which calls `release`
once. Reusing or destroying that surface while a lease is in flight returns
`NX_ERROR_STATE`. If registration, validation or painting fails after a
successful acquire, NexaUI releases immediately and rolls back the widget's
semantic node, hit region, clip and commands. Callbacks are invoked on the
thread that calls each API; the application must serialize source access and
choose a thread appropriate for its renderer.

During resize, acquire or release, drawing, completing or destroying that same
surface returns `NX_ERROR_STATE`. This includes resize to zero and release on
drawing failure. Keep the active context, frame and callback data intact during
the call. Defer a close requested from a callback until the call returns and
any successful frame lease is complete. A separate surface and context can
still run on the same owner thread.

For asynchronously published CPU frames, the
[installed external-content consumer](../../examples/38_external_content/README.md)
reuses `nx_snapshot_exchange` and this viewport API. Snapshot borrows must be
released before UI frame end; viewport pixels must survive later rendering.
The example stages each new accepted frame into a bounded app-owned buffer,
then releases its snapshot inside acquire. Its generation/resize tuple rejects
late producer results after tab replacement. This is an example consumer
policy, not a new SDK lifetime rule or a selected browser/guest engine.

The [installed Physim consumer](../../examples/39_physim_consumer/README.md)
uses the original documented pendulum experiment, its real measurement channels,
scene objects and app-owned plot model. Numeric Apply computes an original-model
replacement before publication; a live viewport lease blocks replacement and
view destruction. The [source-bound model/presentation report](../quality/physim-model-report.md)
separates those installed/headless checks from native application, production
runner, camera/picking, IME/accessibility and physical adoption gates.

`nx_viewport_surface_destroy` frees the NexaUI wrapper but does not destroy
the application-owned source or pixels. The application may release those
resources after all leases are complete. The
[headless 3D shell](../../examples/15_3d_viewport_shell/README.md) demonstrates
both a backing-scale resize and completion after rendering.

This contract transports a CPU RGBA8 image through ordinary bitmap commands.
Native renderers can consume that bitmap path; direct native GPU texture import
still needs a separate ownership/fence contract. These CPU examples do not
establish driver, physical-display or color-management acceptance.
