# Original Physim model with installed NexaUI

This standalone C17 reference consumer executes Physim's **original documented
pendulum experiment**, its original core/numerics/scene implementation and its
original app-owned plot zoom/pan model. It does not reimplement the equations
or replace Physim's approved application UI. It is an initial packet7 model/
presentation seam, with native application adoption still open.

Physim source is caller-supplied and never bundled into the SDK. The fixed
[input manifest](source-inputs.json) selects 27 original files from Physim
`01ccc27a94ca611c9e9cdd8ccde0da426aee2378`, including its MIT license and
unchanged plot test. `prepare.py` reads those exact Git objects, verifies their
hashes and writes a separate fresh export. It neither changes nor builds the
Physim checkout, and does not touch its unrelated working changes. CMake checks
every exported input again before building. No network download occurs.

```sh
python3 examples/39_physim_consumer/prepare.py \
  --source-repo /path/to/Physim --output /tmp/new-physim-export
cmake -S examples/39_physim_consumer -B /tmp/new-physim-view \
  -DCMAKE_PREFIX_PATH=/path/to/nexaui-prefix \
  -DPHYSIM_SOURCE_DIR=/tmp/new-physim-export \
  -DPHYSIM_TEST_FONT=/path/to/local-font.ttf -DCMAKE_BUILD_TYPE=Release
cmake --build /tmp/new-physim-view
ctest --test-dir /tmp/new-physim-view --output-on-failure
```

The initial consumer targets Linux. It requires installed NexaUI0.1.33, the
original Physim C17 subset, libc and libm. It uses function/data sections and
linker garbage collection to exclude original file/run-persistence functions
unused by this bounded model. No persistence operation is called. There is no
SDL/Nuklear/OpenGL UI dependency, replacement original function, source patch,
installed SDK API or root-build dependency. The original Physim inputs compile
with strict basic warnings; the new consumer/test sources additionally use
conversion/shadow warnings as errors. Original source and license travel with
any redistributed consumer binary; the local font remains a borrowed input.

## Model, state and lifetime

`physim_model_run` invokes the original experiment ABI, supplies original
parameter overrides, creates/finalizes the model and performs 240 original RK4
steps at1/120s. The host advances `time_s` after each successful step according
to Physim's API contract. It copies all nine SI-valued channels at241 sample
times, original parameter bounds/unit metadata and the original hierarchical
scene into one owned snapshot. It uses no synthetic trajectory. The full output
is unchanged on validation/allocation/model failure, and the original model
user state is destroyed even when its create fails.

This is a bounded synchronous reference workload. A production Physim runner
must retain its existing scheduling, simulation ownership, job admission,
sampling/storage and asynchronous snapshot publication; rendering may borrow a
finished immutable snapshot. No GUI worker may mutate a live Physim context.
The model's current generation is app-owned, separate from sample time.

`physim_view` borrows the snapshot, original `ps_plot_view` and font collection.
It presents numeric drafts using `nx_numeric_field`, an original angle channel
using `nx_series_view`/`nx_plot`, and the original scene using `nx_viewport`.
Apply confirms both drafts against their original model bounds, runs into
separate temporary state and publishes one new generation only on success.
Invalid drafts leave the model untouched. Zoom/Pan/Fit dispatch to the original
app plot functions; UI-owned state never becomes a second physics model.

Tab from the viewport to **Inspect next** and press Enter, or activate the
button through accessibility. The read-only label above the viewport shows
the selected object's position in the list, original name or shape, original
ID and parent ID. It follows the published scene order and wraps to the first
object. An invalid Apply preserves the selection. A newly published generation
or a replacement view starts at the object count. Inspection changes only
view-local state after the render lease is complete.

The app-owned CPU renderer projects the original scene's line, sphere, arrow,
label and group objects into a bounded16MiB image. Original object order,
positions, colors, labels and group identities remain in the snapshot. This is
a labelled orthographic reference projection, not Physim's production3D
renderer, picking, camera model or full object-tree accessibility. Unsupported
scene shapes return an explicit error. Original scene text/line sizes scale
with viewport backing. No GPU texture import is implemented.

One owner thread builds the UI. The returned commands and source pixels remain
borrowed through rendering. Call `physim_view_complete` only when rendering is
finished or the frame has deliberately been discarded. Apply, another frame
and destroy return STATE while the viewport lease is live. Inspection follows
the same dispatch rule. Destroy preserves
the object on that refusal. Keep borrowed model/plot/fonts alive throughout.
Snapshot replacement must happen through the owner after lease completion.
View recreation retains the model and original plot state; widget drafts are
view-local and the next view starts from published parameter values.

The frame helper owns begin/end_frame and uses a bounded reference layout at
640..2048 by400..1200 logical pixels and0.5..2x backing. An unsuccessful frame
must be discarded; complete any acquired lease and retire/recreate its caller
context before retry. Internal scene-command failure retires its private canvas
so a later acquire can retry. There are no hidden threads, files, services or
window ownership in this initial view/model layer. The test writes only its
explicitly supplied create-only PPM output.

## Exporting a published view

Use `physim_export_render` with a successful model snapshot, the current
original plot range, the font collection, and explicit logical dimensions
and scale. Pass a zero-initialized `physim_export_image`. The result owns
packed RGBA8 pixels with each scaled extent rounded up. For example,
641×401 at 1.25× produces 802×502 pixels. Release the result with
`physim_export_release`.

The export builds the same scene and plot in a separate context from the
published parameters. It can run on the UI owner thread while an interactive
frame has a live render lease. It borrows no window, live input, focus,
unsaved field drafts or presentation state. Caller-owned image encoding,
color management and printing remain outside this adapter.

## Checked and open scope

The original plot test checks anchored zoom/pan, extremes and clipping. The
consumer model test independently runs the original experiment and compares
every sample time, all nine channels, geometry/parent IDs, units and parameter
limits. It checks whole-output preservation on rejected inputs. The view case
uses normalized keyboard input for Zoom and invalid/valid numeric entry,
requires explicit Apply, checks original plot-state equivalence, scene pixels,
whole-frame equality after view recreation, model preservation and2x backing.
It checks all twelve inspector entries, wraparound, lease refusal and selection
reset after model publication.
The failure case checks live leases, bounded geometry and twelve actual
context-allocation failures, then retries with a fresh context.

These five CTests are installed/headless C17 consumer checks, not native-window, IME,
screenreader, real-runner/worker, filesystem/storage, approved-design or GPU/
physical adoption evidence. Native keyboard/IME/accessibility composition,
full scene/camera/object tree, production dataset/plot varieties and original
application integration remain explicit gates. The optional fixture below adds
scoped native window/IME/AT-SPI evidence. Existing NexaUI native
widget evidence remains regression context, not proof that this app passed it.
Physim and NexaCloud can keep developing independently with their current UI.

## Optional ordinary Wayland window and independent native peer

Configure `-DNEXAUI_PHYSIM_BUILD_NATIVE_FIXTURE=ON` to additionally build
`physim_native_host` and `physim_atspi_peer`. The host uses the existing public
window, software presenter, event routing and Numeric IME APIs. It links no X11
presentation library. The peer links only original Physim, libatspi/X11 and
their OS dependencies, without NexaUI. X11 provides the private Xvfb carrier and
independent screen capture; presentation remains ordinary Wayland. Default OFF
builds neither native executable nor discover X11/libatspi.

```sh
python3 examples/39_physim_consumer/run_native.py \
  --build-dir /tmp/new-physim-view --font /path/to/local-font.ttf \
  --registry /path/to/at-spi2-registryd --work-dir /tmp/new-native-physim \
  --case full
```

The runner reuses the repository's private Weston/AT-SPI infrastructure and its
existing optional private tool-prefix variables. Each run owns its Xvfb seat,
compositor work directory, bus, host and peer. No existing desktop is controlled.
`full` exercises actual keyboard validation/Apply, native editable-text setting,
original-model changes, Zoom/Pan/Fit, context/view replacement, 2x source backing,
plot/viewport/inspector Tab navigation and Close. The independent peer checks
original object metadata after real keyboard and AT-SPI button activation,
including retention on invalid Apply and reset on publication. It runs the original
experiment separately and requires substantial actual screen-pixel coverage of
its original bob at the expected position. Whole client PPMs are retained.

The visible Replace view control is handled by the host after lease completion;
F6 is a fixture shortcut and F7 toggles 1x/2x backing. Shortcuts use fresh,
unmodified, unconsumed native downs. IMEs may own function keys, so the peer uses
the independent Replace action during composition. Escape remains available to
the editor/text system. Close is keyboard/assistive operable. Source2x backing
on this1x virtual output is not physical mixed-output DPI acceptance.

`ime-cancel`, `ime-replace` and `ime-close` select real private Weston Simple-IME
with compose:ralt. They create preedit through actual seat keys, then move focus,
replace the view or close while composition exists. Cancel/replacement cases
select the fresh field through native AT-SPI Text and type `2.5` through real
IME commits before Apply. Protocol traces retain preedit, commit, surrounding
selection, candidate rectangles and deactivation. Model parameters stay retained
until explicit Apply. Closing during preedit retains generation1 and retires IME.
The test numeric parameters are nonsensitive; trace files are fixture evidence.

For replacement, the host builds a fresh view/context, retires old IME, discards
the staging frame after completing its lease, publishes the replacement's native
semantics, and only then destroys the old view/context. The last ordinary window
keeps its assistive connection. Old native field objects become Defunct and refuse
focus/edit, even when the replacement reuses their numeric IDs. The published
physics and original plot range remain in their app owner. Native semantics detach
and IME withdraws before final view/context/window/font teardown. BUSY/SUSPENDED
software streams are completed/discarded and rebuilt, never reused after release.

These are controlled native consumer tasks, not the production Physim runner,
approved final UI, full scene/object accessibility, chosen user IME, actual
screenreader speech, Windows/macOS/GPU/physical or performance acceptance.
