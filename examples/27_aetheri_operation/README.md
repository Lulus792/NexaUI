# Original Aetheri operation observer

This Linux C17 experiment presents the original `ae_catalogue_fixture_get`
observation through an installed NexaUI 0.1.23 SDK. It builds Aetheri's original
catalogue/core libraries and schema generator from a separately selected immutable
snapshot. No original source is copied into the toolkit core. The checked
snapshot is Aetheri `1fb45c7c21f914a2893457132b609be50ef4280d`.

The view copies its exact request identity and shows operation, install, scope,
generation, storage slot, artifact, component and observed revision. Visual hex
groups can wrap; semantic values retain all original unseparated digits.
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

Keyboard events here use the core event queue; native input and composed AT-SPI
acceptance have not been executed for this view. Real services, durable recovery,
other platforms, locale changes, physical/human accessibility and final visual
acceptance remain open. The separate activity-control AT-SPI evidence does not
close those composition gates. Aetheri's live source, ongoing changes and SDK pin
are untouched. No 1.0.0 release is implied.
