# Aetheri desktop model: initial dock view

This separate C17 consumer builds against an installed NexaUI SDK and an
explicit Aetheri source snapshot. It borrows the original desktop model and
font bytes; it returns Search, Settings and Terminal intents to the caller.
The view does not start processes, mutate the Aetheri model, create a Wayland
connection or select a privileged role. A pending app request disables its
corresponding button. The host remains responsible for validating and acting
on every intent and observing the real operation outcome. The caller supplies a
nonzero increasing revision, incremented on every model mutation including
remove/re-add with the same ID. Events queued for an older revision produce no
intent. The host must revalidate the revision and target again before acting;
recreate the view before revision exhaustion.

```sh
cmake -S /path/to/copied-example -B build \
  -DCMAKE_PREFIX_PATH=/path/to/nexaui-sdk \
  -DAETHERI_SOURCE_DIR=/path/to/aetheri-snapshot
cmake --build build
./build/aetheri_desktop_view_test /path/to/font.ttf
./build/aetheri_desktop_preview /path/to/font.ttf
```

This is an initial dock and model-result view, not a complete shell adapter.
Search edits return bounded whole-query proposals; the host applies or rejects
them and advances its model revision. On a changed revision the query field is
read-only for that frame, so queued older edits do not alter its published text
or produce an intent; it becomes editable again for the next frame. That frame's
semantic snapshot also reports read-only. Up/Down in the search field proposes
a selected result to the host; Enter proposes activation. When the host queues
events through `aetheri_desktop_view_queue_event`, Tab and Shift-Tab in the
search field also propose next/previous selection without moving the text focus.
Native-consumed and otherwise modified keys are left unchanged. The model-confirmed row
is highlighted, exposed as a selected list item and revealed after a model
revision. Native selection-owner actions, Tab behavior outside the search field, real window activation,
private-surface input and IME, actual Aetheri shell-surface presentation,
multi-output ownership and the original process owner integration remain open.
The Linux preview uses an ordinary NexaUI window, the software presenter,
native input/clipboard/IME binding and optional window AT-SPI publication; it
prints launch/activation intents without performing them. It is not the
private-role shell adapter. Native input-method and human assistive-technology
acceptance remain open. The core view test uses synthetic context events and
semantic requests; neither test establishes physical or human acceptance.

On 5 November 2026 the initial source `3c0dfd20cb7217cbbe81d0af7965533c19b0f506`
was built on Alpine/WSL2 with GCC 15.2 against an installed static, Wayland-only
SDK built from `a7a3df4b8f803cc15f876b4623c9008834c590e9`; the test passed with
DejaVu Sans. A separate static Clang 22.1.3 ASan/UBSan build, including the
consumer and model, also passed with leak detection. The model snapshot's Git
blob IDs were `be57398f32c1e16315510bd95c743559b60c7b52` for `model.c` and
`5a7ed35e92744e5a4b08254b26151efc2025cb34` for `model.h`, identical to those
files in Aetheri commit `78932584c30f4364814d227e434feeb2b74078ed` at inspection.
The older installed SDK encountered first was version 0.1.3 and was correctly
rejected by the example's 0.1.5 minimum; it was not counted as a pass.

Source `b645fee6cd0e5743bb88e22b8eb24ed9dc2922b2` adds scrollable model
results and distinguishes fixed app tags from observed window IDs. The same
GCC and Clang ASan/UBSan consumer builds passed a window-activation test and
a remove/re-add case where a queued request for the old ID is dropped after
the model revision changes. A separate test-only copy with the revision check
removed failed that case. This does not protect a caller that fails to advance
or recheck its revision, and it does not establish real compositor activation.

Source `6860251c49f3ba1782c06ba73411d2e58de44b23` adds the ordinary-window
preview and a private-Weston/Xvfb native input test. The test types `Terminal`,
observes that complete query and its fixed Terminal launch intent, sends Escape
and requires a clean child exit. On Alpine/WSL2 it passed once with GCC and three
times with Clang 22.1.3 ASan/UBSan and leak detection; the latter used 30 ms
between virtual key events. The initial ASan run before immediate model-frame
reconciliation lost characters and failed (for example `Trminl`). A later
reconciled run passed; the earlier failure is not a pass. Fixture records are
`/tmp/nexaui-desktop-preview-gcc-final-20261105/result.json` and
`/tmp/nexaui-dock-fast-asan-{1,2,3}-20261105/result.json`. This uses virtual
input and a private software compositor, not the Aetheri shell connection or
an installed input method.

Source `99dc27a0fc2537fb42475d4995eb56a3336d732a` adds an independent
AT-SPI client, built when the AT-SPI and GObject development packages are
available. Under Alpine/WSL2, GCC 15.2 and private Weston 14/Xvfb, it located
the ordinary preview by PID, set the search to `Terminal`, waited for the
model acknowledgement, reacquired the native accessible, verified its text
and editable state, changed it to `Termina`, and invoked the Terminal list
item. It observed the launch intent and clean SIGTERM exit. Two final runs
passed, as did the separate virtual-keyboard preview regression. The SDK was
the installed Wayland-only build from `a7a3df4b8f803cc15f876b4623c9008834c590e9`;
the Aetheri model blobs are those listed above. Fixture records include
`/tmp/nexaui-desktop-atspi-final-b-20261105/result.json` and
`/tmp/nexaui-desktop-input-after-atspi-20261105/result.json`.

The host explicitly schedules an editable frame after model reconciliation.
A test-only run without that scheduling also passed in this compositor setup,
so the native test does not prove that the extra frame is necessary in every
run. Reacquiring the accessible matters to the client after its interfaces
change. This evidence covers ordinary-window AT-SPI query and press only: it
does not prove a human screenreader workflow, native selection-owner actions,
actual app launch, or semantics on an Aetheri-owned private shell surface.

Source `08ba9becf3c6177f450df3047c061f06829caa71` adds
`aetheri_desktop_borrowed`, built when the Wayland client and protocols
development packages, `wayland-scanner` and threads are available. Run it
with `./build/aetheri_desktop_borrowed /path/to/font.ttf` inside a Wayland
session. The example owns one connection, queue, surface and ordinary
xdg-toplevel role; after configure/ack it borrows them into the existing
software host presenter. It submits a desktop-model frame, waits for an
app-owned worker reply on the same owner loop, then submits a second,
host-updated model revision. It destroys the presenter and view before the
host-owned Wayland objects. It creates no Aetheri private role or second
connection, and does not route native input or publish AT-SPI semantics.

On Alpine/WSL2 and private Weston 14/Xvfb the example exited successfully with
GCC 15.2 and with Clang 22.1.3 ASan/UBSan and leak detection, using the installed
Wayland-only SDK and original model snapshot identified above. Fixture records:
`/tmp/nexaui-desktop-borrowed-model-20261105/result.json` and
`/tmp/nexaui-desktop-borrowed-asan-20261105/result.json`. These results
establish configure, submission, worker wakeup and teardown in this ordinary
host fixture; they do not establish display pixels, a private layer role,
multi-output operation, real Aetheri compositor integration or human acceptance.
