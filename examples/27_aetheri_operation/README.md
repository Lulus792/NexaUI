# Original Aetheri operation observer

This Linux C17 experiment presents the original `ae_catalogue_fixture_get`
observation through an installed NexaUI 0.1.23 SDK. It builds Aetheri's original
catalogue/core libraries and schema generator from a separately selected immutable
snapshot. No original source is copied into the toolkit core. The checked
snapshot is Aetheri `1fb45c7c21f914a2893457132b609be50ef4280d`.

The view copies its exact request identity and shows operation, install, scope,
generation, storage slot, artifact, component and observed revision. Visual hex
groups can wrap; semantic values retain all original unseparated digits.
Metadata rows use the existing read-only text-snapshot API without a caret.
On Linux their complete values are readable through standard AT-SPI Text, as
well as the existing diagnostic attribute. This grants no editing or selection
authority; native cursor/selection changes are rejected. Held Text references
are retired when the view is replaced.
Refresh calls the original read API explicitly. Painting, opening and closing
never accept a request, publish an artifact or activate an installation. An
unavailable query supersedes old busy information. An uncertain outcome retains
the original identity without asserting either activity or completion.

The `operation_observer_test` executable is deliberately a test owner: it creates
a fresh private catalogue, uses the original acceptance/transition APIs to seed
metadata, and exercises the view. It performs **no payload installation or
privileged operation**. This is not a production Management1 client, installer,
removal UI, trusted permission host or completed recovery workflow. An internal
catalogue handle must stay with its authorized original owner; an ordinary app
must not acquire one just to render this view.

Use a fresh build directory and immutable Aetheri source snapshot:

```sh
cmake -S examples/27_aetheri_operation -B out/operation-observer \
  -DCMAKE_PREFIX_PATH=/absolute/path/to/installed/nexaui \
  -DAETHERI_SOURCE_DIR=/absolute/path/to/aetheri-snapshot \
  -DCMAKE_BUILD_TYPE=Release
cmake --build out/operation-observer --target operation_observer_test
python3 examples/27_aetheri_operation/run.py \
  --aetheri-source /absolute/path/to/aetheri-snapshot \
  --executable out/operation-observer/operation_observer_test \
  --font /absolute/path/to/licensed-font.ttf \
  --work-dir out/operation-observer/result
```

The result directory must not already exist. It contains the test manifest,
intent digest, private SQLite catalogue/lock, PPM images, log and JSON result.
Aetheri's original intent encoder and frozen manifest vector provide the seed;
NexaUI does not implement another intent language. Original catalogue build
dependencies include Aetheri's reviewed OpenSSL 3.5 LTS family, SQLite and Python
schema tooling. They are dependencies of this external consumer, not new NexaUI
core dependencies. No display, compositor, GPU, service or network is needed.

The caller keeps the original catalogue, immutable manifest and font collection
alive until view destruction. `observer_refresh` runs explicitly on their owner
thread; no worker or timer is created. A future asynchronous/service host must
supply its own correlated observations and wakeup handling rather than calling
this internal owner API from an arbitrary UI process. The activity phase is fixed
for reduced motion and has no completion meaning. Current installation activity
is displayed separately from historical completion evidence.

Checked cases include an unqueried view, absent/unaccepted operation, accepted/executing metadata,
staging-ready without installation completion,
injected read failure after an active observation, uncertainty through view
replacement, changed caller draft, a different unaccepted operation, keyboard
Refresh/Tab/Close and End on the scroll thumb. The test checks exactly one owner
acceptance, unchanged operation count and catalogue revision across view-only
work. Static Release, shared Release and static ASan/UBSan produce matching images
at 640×480 and 420×320, with dark/light themes and doubled theme font size. At
large text and the minimum height the content viewport is small and scrollable;
final density/minimum-size choices remain open.

The tests above use the core event queue. The separate native host below covers
selected actual Wayland input and AT-SPI cases for the composed view.

## Native Wayland and independent AT-SPI

`operation_observer_native` uses public software presentation, semantic
publication and `nx_window_poll_with_fd` with its borrowed test-control pipe.
It draws when input, model/view changes or pending presentation require work;
the fixed activity band creates no animation timer. Its test owner seeds an
executing operation once. An explicit `uncertain` fixture command updates that
same operation, retires the old semantics and recreates the view. It never
reaccepts the request. The original owner/manifest/fonts outlive both views.

Build against an installed AT-SPI-enabled SDK. With the optional libatspi/GObject
development packages discoverable by pkg-config, CMake also builds the independent
client; that executable links neither NexaUI nor Aetheri:

```sh
cmake --build out/operation-observer --target \
  operation_observer_native operation_observer_atspi_client
python3 tests/run_aetheri_operation.py \
  --server /absolute/path/to/aetheri/test_compositor_server \
  --host out/operation-observer/operation_observer_native \
  --client out/operation-observer/operation_observer_atspi_client \
  --registry /absolute/path/to/at-spi2-registryd \
  --runner tests/run_atspi_native.py \
  --font /absolute/path/to/licensed-font.ttf \
  --aetheri-source /absolute/path/to/aetheri-snapshot \
  --work-dir out/operation-observer/native-result --keyboard --capture
```

The driver owns a new private compositor, D-Bus/registry and catalogue. Its
compositor is supplied independently from the reviewed Aetheri test build; it
does not start or change a live desktop. PATH must provide dbus-daemon/dbus-send
and any private runtime libraries must already be available. Omitting
`--keyboard` uses native AT-SPI actions instead. `--capture` requires the keyboard
mode and captures the private outputs before Refresh. `--expect-unavailable`
checks an AT-SPI-disabled host's intended status-discovery failure; its forced
negative teardown is not clean-close evidence.

Release and ASan/UBSan native action and virtual-keyboard cases passed. The client
checks complete operation-ID/component values, busy/read-only status without a numeric
value/action, standard Text reads without a caret or editable interface, rejected
cursor/selection changes, Refresh, rediscovery after view replacement, cleared
busy state, retired accessible/Text references, restored focus and clean Close. The host verifies one
acceptance, one operation and unchanged catalogue revision during view-only work;
the driver independently reads the final original operation metadata. The
headless software cases remain separate regressions.

Real asynchronous services, durable recovery, other platforms, locale changes,
physical/human accessibility, broader native resize/scale/loss cases and final
visual acceptance remain open. Read-only controls here do not exercise IME.
Aetheri's live source, ongoing changes and SDK pin are untouched. No 1.0.0 release
is implied.
