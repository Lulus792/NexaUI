# Native tree navigation fixture

This external Linux C17 consumer presents the same small test hierarchy as
`tests/test_tree_navigation.c` through an ordinary public Wayland window and
software presenter. Both tests share `tests/tree_navigation_fixture.h`. It is
test data, not a Files/component model or an asynchronous hierarchy service.
Font bytes and installed SDKs are independently supplied; no download is implied.

The separate libatspi peer includes no NexaUI header and links no NexaUI code.
It locates the exact host PID and tree-item roles, requests focus through native
Component, and then observes 16 native keyboard steps. It checks focus and
expanded/selected states, focus-event identities, expansion events for the exact
root/child paths and values, both selection and deselection notifications, and
retired references after child/parent collapse. Re-expanded rows must have fresh
native paths while retaining application selection.

```sh
cmake -S tests/tree_navigation_native -B out/native-tree \
  -DCMAKE_PREFIX_PATH=/absolute/installed/sdk -DCMAKE_BUILD_TYPE=Release
cmake --build out/native-tree
ctest --test-dir out/native-tree --output-on-failure
python3 tests/tree_navigation_native/run.py \
  --server /absolute/private/test_compositor_server \
  --registry /absolute/at-spi2-registryd \
  --host out/native-tree/tree_native_host \
  --client out/native-tree/tree_native_client \
  --font /absolute/LiberationSans-Regular.ttf \
  --work-dir /absolute/new-short-work-dir --capture
```

The build needs the normal `atspi-2` pkg-config package to build the independent
client. `NEXAUI_TREE_NATIVE_CLIENT=OFF` builds only the installed-SDK host and
portable core test, allowing a separately compiled libatspi peer. The tested
private-prefix client command and dependency paths are retained in the evidence
archive. Private registry/runtime libraries must already be available through
the caller's environment. The CTest runs the portable test only; the native
driver is a separate explicit invocation, not an automatically passing CI case.

The Python 3.11+ driver requires Aetheri's explicitly supplied test compositor
with `--headless SOCKET CAPTURE_DIR_OR_- CONTROL_FD`, a retained SOCK_SEQPACKET
control socket and acknowledged `K EVDEV_CODE STATE` commands. It creates its
own 0700 runtime/capture directories, D-Bus, registry, seat and processes. After
each key batch it waits for a published host frame containing those native key
events before asking the independent peer to verify native state/events.
Optional captures preserve complete P6 frames for all three synthetic outputs.
The peer ends normally, then Escape closes the host normally. Failure cleanup
only targets retained private process groups and records escalation separately.

`--expect-unavailable` requires a separate AT-SPI-disabled SDK/host. The rendered
host must remain alive while the peer rejects discovery with exit 77, then close
normally with Escape. That expected negative is not native assistive acceptance.
The separate older-library counterexample must fail at the first Down navigation
step after successful discovery/focus; an arbitrary setup failure is insufficient.

`result.json`, input hashes, process maps, logs and optional raw captures retain
the actual result. Source/binary/SDK revisions, static/shared/sanitizer profiles,
initial harness failures and native acceptance limits are recorded in the
[quality report](../../docs/quality/tree-native-report.md).

This checks a private test seat and controlled hierarchy. Real child-loading/error
UI, virtual focus reveal, breadcrumbs, production services/registry, full native
tree/table selection, installed IMEs and human/physical acceptance remain open.
