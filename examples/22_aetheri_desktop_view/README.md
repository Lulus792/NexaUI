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
full private-surface input and IME, production Aetheri shell integration,
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

Source `06b8f2800462d4bb7ff282cfdee1cc344e08c45a` adds a test-only
development-shell host, adapted from Aetheri's MIT-licensed
`tests/test_desktop_client.c` (blob `e21c47875668ef677271dfda0bc54ad5aab000e3`).
The host uses the original Aetheri protocol XML supplied in
`AETHERI_SOURCE_DIR/protocols`; their Git blob IDs were
`e9f27e4fdbe605f0a7014ec6194cf5dbc50d2b65` for layer shell and
`d8ae3955153413299071ac0ec20c56870fbf4f75` for desktop control. Aetheri
HEAD was `2790db68b6e6c4057f4d69b38cc39573df7de319` and the desktop model
blobs matched those recorded above. The external checkout was not modified.

The driver `tests/run_aetheri_layer_view.py` starts a separate Aetheri
headless test compositor and passes a private socket to the host. The host
creates the original layer roles and a separate NexaUI view and software
presenter for each of three virtual outputs. It opens and collapses search
through the test command channel, retires the shell connection and waits for
the compositor to exit. The driver checks dock pixels on all three outputs,
search-field pixels on the first output, transparent background there and
both process exit codes. It does not inject native input into the view.

The final run used Fedora GCC 16.2.1, Wayland 1.26.0, the installed static
Wayland-only SDK built from `a7a3df4b8f803cc15f876b4623c9008834c590e9`
with AT-SPI and Vulkan disabled, and the Aetheri test compositor built from
`d621c9bba1df8ffd1083999300bcff742022b9dc`. Compositor SHA-256 was
`da09d434747dafc3400fb8da4f426b3cc8713452515d61e2dd9d3dbf693ed164`;
desktop test binary SHA-256 was
`a198535479ef6c12388b5f92fd3e6ccf86559d5a1b4a36c2327b89c9e8532e56`.
Logs, result and selected captures are retained in
`D:/NexaUI/out/aetheri-layer-desktop-evidence-final-20261105.tar.gz`,
SHA-256 `0793ba897f5d14b93f01325d8bccce2ded9a6d4f39a5bc9e928ea5090f12477f`.
This is a private headless fixture, not the production shell adapter. Input,
IME, AT-SPI, actual launcher ownership, hotplug, physical outputs and human
acceptance remain open.

Source `facbc3e4eea8f021b1e0c8a5ac76e96dd76c638a` extends the test-only
layer host with a bounded XKB keymap, surface focus tracking and a subset of
key/text translation. Model updates are acknowledged before processing the
next input; the host retries a busy presenter after further Wayland dispatch.
On the same isolated Aetheri test compositor, its virtual keyboard typed
`terminal`, removed and re-entered the last character, then pressed Enter.
The driver observed both corrected query revisions and the Terminal launch
intent; all three dock pixel checks and orderly retirement also passed.
The final desktop binary SHA-256 was
`ffc5893e57b3ed9ebb717c42db955da86bb8dc4d14b9e2d146d08eb2a6f727a2`.
The final result and logs are in
`D:/NexaUI/out/aetheri-layer-input-final-20261105.tar.gz`, SHA-256
`41b51b21a7674c4de93c4642f8ba41ac0e70db215f87384ab6597a98d9a6b604`.
This fixture does not start the app and is not a complete native input
adapter: IME/composition, key repeat, clipboard, pointer, full key mapping,
selection and focus restoration, accessibility and human/physical validation
remain open. The test diagnostics intentionally include the non-secret query.

Source `08eba45e7446f69c7e8e0380402ca52329655020` extends the test host
with left-button pointer routing, cancellation on known pointer loss and a
configure gate. With the same test compositor and SDK, a result click emitted
the expected intent; pressing outside the result and releasing inside emitted
none. Removing and replacing idle input devices restored search keyboard focus
and a subsequent new click worked. This does not implement wheel scrolling,
other buttons, full capture behavior or actual application launch.

An explicit `--test-held-removal` case currently **fails**: when the virtual
pointer is removed while its button is held on a result, this compositor sends
a button release before it reports pointer removal, and the test host emits the
launch intent. The later cancellation cannot undo that intent. The normal
passing case removes an idle pointer and must not be read as evidence that
held-device removal is safe. The flagged failing case is retained for further
host/compositor contract work; this test host is not a production shell.
The final test binary SHA-256 was
`e7c729ba6b3b3d16d6ffbcab50c940b4d36daf87d5e456fe620c8db2949ec937`.
Positive and negative logs plus two failed screenshot runs are archived in
`D:/NexaUI/out/aetheri-layer-pointer-final-20261105.tar.gz`, SHA-256
`e0caa7e34e39bfd4021567bd526f1403cddcd6f5ca53ca4915446355ee7685de`.
The screenshot failures reported a compositor capture/write failure; after
compressing this task's accumulated temporary captures, the final run passed.
No specific underlying write error was recorded.

Source `57b0273b248286f07bf8edc34db36cd0222bc899` invalidates a
captured pointer press and an Enter/Space activation when the model revision
changes, while retaining the current focus. The regression holds a press on
window 42, removes and recreates that ID, presents the new revision and then
releases: neither the old pointer press nor the old Enter press may activate
the replacement. Fresh pointer and Enter presses on the replacement still
activate it. The pointer assertion failed before the change; after adding the
pointer cancellation, the separate Enter assertion also failed until the
keyboard capture was retired.

The core view regression passed with GCC 15.2 and Clang 22.1.3 ASan/UBSan
with leak detection, and with GCC 16.2.1 on the Fedora carrier. The separate
ordinary-window Wayland keyboard and independent AT-SPI regressions passed
under private Weston. The full private Aetheri layer fixture also passed; its
desktop binary SHA-256 was
`556c6a30481283a7c61c10877e4432e2d639a807055a4cacc03ea1d4f55e84cd`.
Its logs are in `D:/NexaUI/out/aetheri-layer-revision-guard-20261105.tar.gz`,
SHA-256 `7735efdabb14dea4a3c09a725e5b1094fe42fa3dc1ddf076011c2dfcfcf13323`.
The host must still advance and recheck its revision before acting. This fix
does not resolve the separate held-device removal case above, which can
produce a native release before any model revision or device-loss event.

Source `fc34f2b1b4e70110a75c33d1a05ddec631a61a5b` routes Wayland axis
frames in the test layer host to the existing NexaUI scroll area, preferring
discrete steps when supplied and clearing accumulated axes on pointer loss.
The private Aetheri test compositor exercised a synthetic 30-window model:
ten virtual wheel steps moved window 20 from logical y=1160 to y=760, three
reverse steps moved it back to y=880, and scrolling outside the results left
it there. A click on the scrolled row emitted window ID 20. The preceding
keyboard, pointer and idle-device-replacement checks also passed. A separate
test-only binary with wheel deltas suppressed failed with the row unchanged at
y=1160. Desktop binary SHA-256:
`e43bcc67b21609d2b25e3ff58cfed31ebb5823787afc1452e4a22df1e6e9607f`.
Positive and negative logs are archived at
`D:/NexaUI/out/aetheri-layer-scroll-20261105.tar.gz`, SHA-256
`6c6cd136b39ef91ebbc9ecb7d6cf0135ec6e0ee372fb660d3542651b618fd992`.
This tests virtual discrete wheel input only; continuous touchpad behavior,
real window observations and the held-device removal failure remain open.
