# Ordered numeric semantic steps — 2026-11-09

SDK **0.1.32**, source **`f1c96419a33ac00e14ab14ec8c7f93cfec2aea93`**.
The [original Settings audio consumer](settings-audio-report.md) exposed a real
gap in 0.1.31: AT-SPI advertised slider increment, but the core admitted numeric
steps only for scrollbars. The original volume control therefore submitted no
command. The old native transcript and the final unchanged regression linked
against installed 0.1.31 retain that failure.

## Contract and implementation

Existing `nx_semantics_request_action` now admits increment/decrement for the
built-in `nx_slider` and `nx_drag_value`. No public function, structure or enum is
added. The [widget contract](../concepts/widgets.md) and public header describe
the extension; generic custom widgets remain unsupported.

The existing bounded request queue holds at most 64 admitted actions. At the next
frame, typed requests preserve their admission order and original ID, built-in
kind, role, parent and overlay scope. Only the matching, enabled control applies
them, using its current validated range/step after routed keyboard/pointer work.
Each action clamps/snaps separately: increment then decrement at a non-grid
endpoint must not be collapsed into a zero net delta. Steps do not focus or
click the control. Disabled ancestors reject admission/application. Omitted,
reparented, rebound or moved-overlay targets cannot redirect stale steps.

Successful painting/value assignment consumes matching steps. A painting failure
leaves them available for a same-frame retry, without double application. A new
frame expires unused steps. Existing scrollbar behavior is retained.

The private kind marker fills existing 64-bit region padding; private bounded
storage belongs to its context. Public declarations/layouts, after excluding
comments and version macros, match 0.1.31. The shared ELF retains exactly all
**543 dynamic definitions** of the compared 0.1.31 library; this is an ELF
definition count, not a claim of 543 public APIs. No GLib/GObject/GIO, systemd,
PipeWire, SDL or X11 runtime dependency enters this Wayland-only SDK build.

## Executed evidence

Alpine-Dev WSL, Clang22.1.3, C17/extensions OFF, Wayland-only, X11/Vulkan OFF;
UID65534 executes the checks. Release/static, Release/shared and Debug-O1
ASan/UBSan/LSan each pass these **13 selected CTests**: core, value controls,
scroll/virtual, expansion actions, semantics, widget, overlay, docking, text
value, text field, numeric field, interaction and layout. Total **39**, with
**three installed external package consumers** run using `--expect-wayland-only`.
Original finite timeouts and active leak/error detection are retained.

All **1,091 exact Git inputs** and **213 retained objects** are source/flag/hash
bound: 70 SDK objects and one installed-consumer object per profile. All 36
built Wayland generated C/header files reproduce byte-for-byte from the recorded
system scanner/XML. See the [complete local receipt](value-steps-local.json).

The regression covers unfocused actions, no click/focus, duplicate/queue limits,
all 64 admitted requests and the rejected 65th, ordered endpoints, current
range/step, keyboard-before-step order, disabled controls/ancestors, omission,
parent/kind/custom/overlay rebinding, retained modal scope, and actual allocation
failure followed by same-frame retry. It exercises both built-in value controls.

Two independent negative controls are retained:

- The final committed regression compiles against installed Release 0.1.31 and
  fails at unsupported slider increment admission (exit1).
- A separate copy of the final instrumented core omits only the parent-identity
  check. The unchanged public-API reparent probe passes with the actual library
  (value5, exit0) and rejects the compiled defect (value6, exit1). Both clean up
  normally, with no ASan/UBSan/LSan report. Its three objects and the old-SDK
  negative's one object are separate from the 213 positive objects.

The consumer source `a8ef31de8f6b0afb476221d6840255e553930321` leaves SDK
production files unchanged from this SDK revision. Exact-source desktop CI is
recorded with the [Settings audio report](settings-audio-report.md). Complete
raw sources, binaries, commands, negative controls and logs are retained in the
[verified common archive](settings-audio-archive.json).

## Remaining acceptance

This extends existing step actions only. Standard native numeric Value/SetValue,
splitter/color-picker/custom step support and broader cross-platform native
numeric interaction remain open. No physical input, human assistive review,
canonical font or end-to-end performance acceptance follows from these tests.
L03/I01/A01 gain scoped evidence; no catalogue status is promoted to complete.
