# Aetheri desktop model: initial dock view

This separate C17 consumer builds against an installed NexaUI SDK and an
explicit Aetheri source snapshot. It borrows the original desktop model and
font bytes; it returns Search, Settings, Terminal and Files intents to the caller.
The view does not start processes, mutate the Aetheri model, create a Wayland
connection or select a privileged role. A pending app request disables its
corresponding button. The host remains responsible for validating and acting
on every intent and observing the real operation outcome. The caller supplies a
nonzero increasing revision, incremented on every model mutation including
remove/re-add with the same ID. Events queued for an older revision produce no
intent. The host must revalidate the revision and target again before acting;
recreate the view before revision exhaustion.

This current consumer requires an Aetheri desktop model with Files and
`ae_desktop_app_selector`/`ae_desktop_app_from_selector`; older snapshots are
rejected during CMake configuration. The tested immutable model is from Aetheri
`7374f381719397abc100dcb784becf59c86d1462`. This requirement applies to the
example consumer; it does not change the SDK's ABI or Aetheri's source pin.

The private layer test host now matches the desktop protocol's initial output
names to `wl_output.name`, rather than assuming registry order is the desktop
index. It rejects missing, duplicate or ambiguous names and an output removed
before mapping; the same registry IDs are retained for later retirement. The
layer host requires Wayland client headers 1.20 or newer and a compositor
advertising `wl_output` version 4. The regular view and borrowed-window targets
do not inherit that layer-host requirement. The Linux static consumer also
declares its math-library dependency.

The mapping callback test covers reordered outputs, missing/duplicate names,
initial removal, repeated mapping and retirement after reordering. An isolated
Fedora Aetheri test-compositor run on three virtual outputs additionally checked
the actual initial mapping, search, input, window observations and activation.
It used the matching-order outputs advertised by that compositor; the reordered
case is a controlled callback test, not a physical hotplug. New outputs after
the initial snapshot remain deliberately unmapped.

On the candidate based on `c3dc49068f54a969edf78473341591fabf4894c6`,
the test layer host defers a known output's registry removal or layer close
until Wayland dispatch returns. It cancels a pending pointer release and
activation, drops focus and IME association, retires the corresponding AT-SPI
tree, presenter, view and Wayland objects, and leaves the other slots alive.
Unknown removed globals are ignored. Callback and controlled-wire tests cover
these paths with a real surface and a pending sync; an independent AT-SPI client
checks that the removed tree becomes defunct while another remains operable.
They simulate removal rather than hot-unplugging an Aetheri output. This test
host still assumes three initial outputs and does not support output re-addition;
actual compositor ordering, hotplug and production shell ownership remain open.
On base `70a6a91b4f8edfb176f9f6147ab2f63a4ef8f44c`, later `wl_output`
globals are reported as `OUTPUT_UNMAPPED` instead of aborting or guessing an
index. They are not bound or displayed; existing views continue. A controlled
server announces an extra global and the independent AT-SPI fixture exercises
the same fallback after simulated removal. Re-adding a usable output requires
a reliable current mapping to the private desktop's output identity.

```sh
cmake -S /path/to/copied-example -B build \
  -DCMAKE_PREFIX_PATH=/path/to/nexaui-sdk \
  -DAETHERI_SOURCE_DIR=/path/to/aetheri-snapshot
cmake --build build
./build/aetheri_desktop_view_test /path/to/font.ttf
./build/aetheri_desktop_ime_transaction_test
./build/aetheri_desktop_preview /path/to/font.ttf
```

This is an initial dock and model-result view, not a complete shell adapter.
The view can opt into the external-host tagged commit/deletion seam, expose the
search-field freshness token and, since 0.1.13, render host-provided preedit and
report the drawn caret. Its synthetic test exercises preedit and one committed
scalar. It does not connect a native input method: protocol state, serials,
candidate placement and cancellation remain the host's responsibility. The
ordinary preview continues to use its window IME.
The private layer test host forwards native keyboard enter, leave and keyboard
removal to the view's surface-focus queue. Its callback-level focus test can be
run as `./build/aetheri_desktop_layer_focus_test /path/to/font.ttf` when the
layer-shell and Aetheri test protocol headers are available. It invokes host
callbacks without a compositor and is not native device-removal acceptance.
`NEXAUI_TEST_ATSPI=1` enables the 0.1.14 host-owned accessibility bridge in
the private layer test host. It publishes one semantic snapshot per configured
view and polls the bridge alongside the existing connection. Native focus is
reported only when the host has observed it; accessibility focus requests do
not grant a private Wayland role or establish compositor focus. Screen
coordinates remain unavailable. The separate `aetheri_desktop_ax_host_test`
is a headless fixture with simulated visibility and focus for independent
AT-SPI testing, not a running Aetheri shell.
The private `ime_transaction` helper stages bounded UTF-8 preedit, commit and
deletion events until a protocol `done`. The layer test host can opt into its
text-input-v3 binding with `NEXAUI_TEST_IME=1` when the compositor offers the
protocol. It admits an older serial only while it still refers to
the same field revision; for such a `done`, the host defers protocol state
requests until a matching serial arrives. It validates focus, surrounding bytes,
selection and UTF-8 boundaries before applying the batch. This is a test host,
not a production input method or admission authority. If no provider is
advertised it retains its ordinary keyboard path.
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
revision. Tab behavior outside the search field, real window activation,
full private-surface input and IME, production Aetheri shell integration,
multi-output ownership and the original process owner integration remain open.
The Linux preview uses an ordinary NexaUI window, the software presenter,
native input/clipboard/IME binding and optional window AT-SPI publication; it
prints launch/activation intents without performing them. It is not the
private-role shell adapter. Native input-method and human assistive-technology
acceptance remain open. The core view test uses synthetic context events and
semantic requests; neither test establishes physical or human acceptance.

In the preview and layer test host, Ctrl-Shift-Tab moves focus from Search to
the preceding dock control (Files when enabled); Ctrl-Tab moves forward back
to Search. Enter on the focused Files button requests Files. Plain Tab and
Shift-Tab retain the existing search-result navigation. On base
`30e05549b17329d2d8007e57069026cc938fbecb`, the scene and layer keyboard
callback checks and a private Weston virtual-keyboard preview passed with GCC
static/shared and Clang ASan/UBSan. This does not establish the shortcut in
the actual Aetheri private shell, a physical keyboard or a human screenreader.

On base `dcb3f4ddb20222120f1f557131a573b0c996fe10`, search results expose a
single-selection owner and native selection actions. Selecting a result emits
`AETHERI_DESKTOP_SELECT`; it does not launch or activate it. The semantic
selected state reflects the last host-confirmed model until the caller applies
the intent and advances the model revision. Disabled entries reject selection,
and requests queued for an older revision produce no intent. An independent
AT-SPI client tests the list's Selection interface, then edits the search field
and activates Terminal; the current Aetheri snapshot also tests selecting Files.
The test host uses simulated geometry and focus. Real private-role routing and
human assistive acceptance remain open.

On base `96e8e06e67e5d95a47758276615c05b660ea30ff`, the four-button dock
exposes Files/Dateien as a stable focusable action, disabled while the
application-owned request is pending. It reports an intent; the test layer host
uses the model's selector helpers to exchange `L2F` and A/E/X replies with its
injected owner socket and fans confirmed state to its three models. Its
socketpair test is not an actual process launcher or production admission.
On base `101507414e0e7c3806e0c1e93ce08dee83b20e9a`, the independent AT-SPI
client also checks Files actions, selection and a retired accessible through
the layer host's accessibility loop with three simulated views. This does not
establish native output removal or production shell ownership.

On base `6ced584aa23aec468bf6ed607228d3b0a5ac2045`, button widths follow
the shaped labels, with at least 44 logical pixels per hit target and clipping
to each button. The 320-pixel test checks full DE/EN label fit with three
DejaVu fonts. If a different font's labels cannot all fit, text can be clipped;
larger text scaling, other fonts and human visual approval remain open. Set
`NEXAUI_DOCK_EN_CAPTURE` and/or `NEXAUI_DOCK_DE_CAPTURE` to paths when running
`aetheri_desktop_view_test` to save the corresponding 320x84 headless PPMs.

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

Source `41fb0b20f0d1df0afa9d9368b1a67ac034be7101` connects the test
host's existing Aetheri desktop-observation callbacks to each view's original
model. Each observation or close advances that view's revision; painting is
deferred outside the callback. The driver accepts an optional
`--app-client /path/to/test_compositor_client`: without it, the result
explicitly records `window_observation: not-run`.

On the same isolated Aetheri headless test compositor, the independently
started native test client created two real xdg-toplevels. Their titles and
IDs appeared in the model for all three outputs and in the first view's
semantics. The test clicked the first window's row, observed its exact ID in
the proposed intent, confirmed that direct activation is denied while search
is exclusive, and separately activated it through the desktop-control test
command after collapsing search. A Unicode title change propagated to all
three models and the first view; closing the second window removed it.
Reopening it produced a new ID and replaced the old row. This fixture observed
IDs 1, 2 and 3; the app, desktop and compositor all exited 0. The app was
started by the test driver; the clicked intent was not itself wired to the
activation command or a process owner.

The desktop test binary SHA-256 was
`3c493eec7771d60487398d2b323daa146933baa77021d7520ae60f682bd4e938`;
the independent app client was
`d979f958d7471b3e6231908f09f8ba99384e7568e193f1bec8f546d13909d5fc`.
Suppressing model updates in a test-only binary made the same check fail.
Positive and negative logs are archived at
`D:/NexaUI/out/aetheri-layer-observation-20261105.tar.gz`, SHA-256
`e60bdba6d8626d05c95373c1855b3cdbcbc0236de304c75c40a6493e96badfb8`.
Actual process-owner launching, private-surface IME/AT-SPI, dynamic output
removal, physical operation and the held-device removal failure remain open.

Source `96382bedae8fb1cfb959875a6433e40ae49a90c8` adds an opt-in
**test-owner** path for activating an already observed window. After a row
click, it validates the originating model revision and ID, collapses the
exclusive search surface, waits for its configure and verifies the target
again before sending the original Aetheri desktop-control activation request.
An unrelated focus observation during collapse may change the overall model
revision; the second check therefore uses the target's continued presence and
an explicit close invalidation. The compositor retains authority and reports
the result. Aetheri's development protocol specifies that window IDs are not
reused during a compositor lifetime.

The isolated test now observes the clicked real test window being activated.
Two additional runs hold the owner at explicit test barriers and close the
target respectively before validation and after search collapse; both cancel
without sending its activation request. The first implementation rejected a
valid click when collapse changed an unrelated window's active state; that
failing log is retained. A later capture run also failed before the input
case, with a compositor capture/write error after temporary captures had
accumulated. After compressing this task's captures, all three final runs
passed; the driver now compresses its own captures after each run.

The final test desktop binary SHA-256 was
`255ee73f650cb0edab5bc4cae1455bcded236465fc48dfbb8bf183a3cd3369be`.
Results and logs are archived at
`D:/NexaUI/out/aetheri-layer-activation-20261105.tar.gz`, SHA-256
`0c534fd5610e79f97bfb0709ca5a8fdb1ff5fd56c14fc14daba38280d872b02c`.
This is an opt-in test host, not Aetheri's actual process owner or a production
desktop. It does not launch Settings or Terminal. In particular, the
held-pointer removal issue documented above remains unresolved and makes this
test path unsuitable as a production activation policy.

Source `cadaf8a3c4a6abd54b22851da64434c107858977` adds an optional,
inherited Unix `SOCK_SEQPACKET` launch channel to the test host. It uses
the existing Aetheri model's per-app pending/reply functions and the original
three-byte `L2S`/`L2T` and `A2*`/`E2*`/`X2*` messages. The test host
does not choose executable paths or launch a process. A nonblocking send
failure terminates the test instead of retrying an uncertain effect; malformed
or uncorrelated replies and connection loss also fail it.

With `--test-launch-channel`, an independent test peer observed Terminal and
Settings requests. A second Terminal Enter while its first response was
pending produced no second packet. The test applied a definite error,
retried, applied process acknowledgement and later process-end messages, and
verified a Settings error separately. Four isolated negative cases rejected
an uncorrelated acknowledgement, oversized packet, passed file descriptor
and peer loss; their host and test compositor exits were intentionally
nonzero. Each case verifies the expected failure site. The positive run also
passed the observed-window activation and input regressions. Its host binary
SHA-256 was `5e6d4b328a0b064727ee3f410fd743cf2d2468be26e4c5141b8863b1f450e2ea`;
logs and result files are in
`D:/NexaUI/out/aetheri-layer-launch-channel-20261105.tar.gz`, SHA-256
`508702c12db0f16df797a7190f320cce3e0bdcec5c21043ab0b06ce182baaf07`.

The peer is a test driver, not `development_session.py` or its actual
process owner. No Settings/Terminal process or ordered owner shutdown is
proved by this check; the `A` test message is not an application readiness
claim. Real owner integration, private-surface IME/AT-SPI and the pressed-device
removal failure remain open.

Sources `e58e718ff39900ece9ac0f5027528a28a8c39a54` and
`ac3915ac5b609e63d29784c13734a9a3e63b1cde` exercise this test host
through Aetheri's original development supervisor and its existing isolated
headless fixture. The fixture substitutes a test compositor, temporary paths
and notification handling; it does not modify the running carrier session or
installation. The host accepts the supervisor's existing desktop arguments,
uses its inherited launch and private Wayland connections, handles the
compositor's search toggle and retires on owner-requested signal shutdown.

Virtual keyboard search launched the separately built NexaUI Settings
**model consumer** through the real supervisor. A second request reused its
existing process; after Ctrl+Q and the supervisor's process/pipe drain, a
third request started a new Settings process. The same test then launched the
separately built original Aetheri SDL Terminal and its Aetheri shell; another
request reused that Terminal process. On supervisor shutdown, the terminal
reported its shell status as 129 and clean terminal cleanup, followed by
Settings signal shutdown, desktop retirement and zero exit statuses for the
four supervised children. The test waits for observed focus restoration before
sending the next input. It does not prove the shell or its jobs survived
shutdown, or that a repeated launch activated the existing window.

The final two runs passed on the isolated Fedora carrier with the historical
Aetheri source `d621c9bba1df8ffd1083999300bcff742022b9dc` and an installed
Wayland-only NexaUI SDK from
`a7a3df4b8f803cc15f876b4623c9008834c590e9`. The Settings model consumer
was the previously verified binary from NexaUI source
`41042e56d629c8532bd365212d97385b41eaa476`, not the complete Settings
application. SHA-256 values are recorded in the test results. Logs, including
an initial wrong-app-ID run and a later focus-timing failure, are in
`D:/NexaUI/out/aetheri-layer-real-owner-final-v2-20261105.tar.gz`, SHA-256
`8e732e02eb8f3e599d05baadd316778c35c95496d593fb2d33e73fec89b0941f`.
The test host is not yet the production desktop: private-surface IME/AT-SPI,
the held-pointer removal failure, physical and human acceptance, the full
Settings integration and broader app migration remain open.

Source `7065ee9c0bdd46c9d333dc7fd4dd52dbfae3b999` extends the owner
fixture so a successful fixed-app acknowledgement can select an already
observed matching window and use the guarded activation path after search
collapses. This mirrors the existing Aetheri development desktop's grouping
behavior: app IDs are descriptive metadata, not proof of identity or
authorization, and the process owner still chooses its fixed executable.
If no matching window has been observed, acknowledgement does not fabricate
one. A target closed before activation is cancelled.

Two isolated original-supervisor runs started both the NexaUI Settings model
consumer and original Aetheri Terminal, switched from Terminal to the
already-running Settings window and back, observed successful compositor
activation responses and verified no additional processes were spawned.
Settings close/reopen and ordered shutdown checks also passed. A separate
test-only binary whose app-ID match was deliberately disabled failed while
waiting for Settings activation. The final desktop binary SHA-256 was
`788109c5ea3a1a2c37587c77b8977fbac2d7ab0b752c2720f5ba7dd6de9c551c`;
positive and negative logs are in
`D:/NexaUI/out/aetheri-layer-owner-reuse-20261105.tar.gz`, SHA-256
`8e7eaed9cc59281cdc8a09e8be64fa915991da3519faef122581a3cd63ed27b5`.
This supersedes the earlier fixture's lack of observed-window activation, not
the remaining production, IME, AT-SPI, held-pointer or human/physical gates.

The optional text-input-v3 candidate based on
`340ac8b58c364bc479471e05292b313e9367f857` uses the existing host connection,
field revision, preedit and caret APIs. On Alpine/WSL2, a controlled Wayland
server exchanged actual text-input-v3 messages with the host callbacks: it
observed surrounding text and caret requests, sent preedit, committed `é`,
then deleted it and committed `B` in the original Aetheri search model. GCC
and Clang ASan/UBSan with leak detection passed that wire test and callback,
view and transaction tests. A separate mutant replacing committed scalars
failed the expected model assertion. Focus loss, malformed text, invalid
deletion and a hidden composition cursor were checked at callback level.
The server and keyboard-focus callback are fixtures: this is not a run of the
Aetheri compositor or its Compose provider, physical input, native AT-SPI,
multi-output acceptance or human assistive review. It does not resolve the
held-pointer removal failure; the private host remains test-only.

The subsequent wire-test candidate based on
`f005c93eac9d28674e9f25dc46b2a792e6bb1053` additionally delays a commit
across an intervening local model edit. The server then sends the old `done`
and a new commit; only the new commit reaches the model. It also sends a
preedit followed by a pending commit, native text-input leave and late `done`;
the draft clears and the late commit has no effect. The controlled wire test
passed with GCC and Clang ASan/UBSan with leak detection on Alpine/WSL2. A
test-only mutant removing both stale-revision guards inserted the old `X` and
failed the `ABDC` assertion. The compositor and keyboard focus remain fixtures.
The available Alpine environment had no wlroots-0.20 development package or
built Aetheri test compositor; the optional Aetheri Compose provider was not
executed here. No packages or Aetheri files were changed to manufacture a pass.

The same candidate bounds the complete resulting search query before queueing
any IME deletion or committed scalar. A 128-emoji commit that cannot fit the
Aetheri model is rejected without changing the field revision or model. A
test-only copy without that guard failed the unchanged-revision assertion; the
callback case passed with GCC and Clang ASan/UBSan and leak detection. This is
the test host's query policy, not terminal paste or production IME acceptance.

The pointer-release candidate based on
`bf1080d889a6d82471b28887b0321f6a3bccc9b6` delays a captured left-button
release until a nonblocking `wl_display.sync` callback. Pointer loss, leave,
intervening movement/scroll, view retirement or a changed model revision
cancels that capture. In the controlled server test an ordinary click reached
the intended window after the sync; a changed model or a simulated removal
before the callback did not activate it. The server exchange and callback test
passed with GCC and Clang ASan/UBSan with leak detection on Alpine/WSL2. A
test-only copy restoring immediate release produced the unwanted activation in
the removal scenario. The test invokes pointer and capability callbacks
directly; it does not send a real device removal from the Aetheri compositor.

The sync orders events already emitted on this connection; it cannot establish
whether a release was physical or synthesized, nor predict removal reported
after the callback. Fast pointer input before the callback conservatively
cancels the pending click. The earlier `--test-held-removal` failure remains
open until rerun against that compositor, and a later removal is not covered by
this mitigation. This private host is still not a production activation policy.

The accessibility-host candidate based on
`3c5eb737e0432ff4dff4800b295f40365b110dbb` was built with an installed 0.1.14
Wayland-only, AT-SPI-enabled, Vulkan-off SDK on Alpine/WSL2. A separate
libatspi client found the headless Aetheri model view, replaced its search text
with `Terminal` and activated the Terminal result. GCC static/shared and Clang
ASan/UBSan with leak detection passed; a test-only consumer that stopped applying routed
requests caused the independent editable-text request to fail. The host API
also passed absent-transport, invalid geometry, wrong-thread and teardown tests
with AT-SPI enabled and disabled. The existing independent multi-selection test
for ordinary windows passed in a private Weston/Xvfb session. An initial run
outside the required Weston fixture skipped, and two Weston setup attempts
failed due to missing process-local library/module paths; neither counts as a
pass. The private-layer poll integration was built but has not been exercised
on the Aetheri compositor. Human screenreader, native focus, three outputs and
production acceptance remain open.

In a second independent-client run, the view's context allocator attempted to
clear and destroy its accessibility host while a text request was being routed.
Both calls returned `NX_ERROR_STATE`, and the original text edit and Terminal
action completed. GCC static/shared and Clang ASan/UBSan with leak detection
passed. An isolated SDK mutant that removed the operation guard failed the same
client request and reported the guard assertion. A separate detach/republish
test confirmed that an old accessible object fails and the new tree is
discoverable; a mutant omitting detach failed that old-object assertion.
Unfinished-frame publication and routing to a different context were rejected.
To repeat using an installed 0.1.14 SDK and the separately built client and
fixture:

```sh
NEXAUI_AX_REENTER=1 NEXAUI_AX_REBIND=1 NEXAUI_DESKTOP_FONT=/path/to/font.ttf \
  python3 /path/to/NexaUI/tests/run_atspi_native.py /path/to/at-spi2-registryd \
  ./build/aetheri_desktop_atspi_client ./build/aetheri_desktop_ax_host_test
```

The follow-up candidate based on
`dd455a82b6306940d28823c89b9d8d7d8fbca625` exercises the private layer host's
actual accessibility publication and poll/routing functions with three
simulated configured views and a real independent libatspi client. The client
observed three separately published surfaces, checked that a retained logical
focus on another view did not become native focus, removed that view, verified
its old object was defunct, and then edited and activated the first view. GCC
static/shared and Clang ASan/UBSan with leak detection passed. Separate mutants
that fabricated focus or skipped removal failed the corresponding independent
client checks. One initial sanitizer CMake invocation failed because its flags
were quoted incorrectly; the corrected build and run passed. These are simulated
views and focus, without a presenter or Aetheri compositor. Actual output removal,
native focus, compositor integration and human assistive acceptance stay open.

GitHub Actions run `42054679539` for source
`dd455a82b6306940d28823c89b9d8d7d8fbca625` failed to build the static Linux
and sanitizer jobs: the internal multi-selection budget test still referenced
the adapter's removed `window` member. This candidate updates that test to
identify the same window owner through the new owner contract. On Alpine/WSL2,
complete static, shared and Clang sanitizer builds passed; a separate full
dual-backend Debug build included Vulkan, examples and benchmarks. The actual
65,533-member budget test passed on Xvfb, private Weston and private Weston
with ASan/UBSan and leak detection. The originally failing GitHub run remains
historical evidence, not a pass for this candidate.

```sh
NEXAUI_AX_LAYER=1 NEXAUI_AX_LAYER_MULTI=1 NEXAUI_DESKTOP_FONT=/path/to/font.ttf \
  python3 /path/to/NexaUI/tests/run_atspi_native.py /path/to/at-spi2-registryd \
  ./build/aetheri_desktop_atspi_client ./build/aetheri_desktop_layer_ax_host_test
```

The focus-forwarding candidate based on
`b097deac0894fed6573a2f39bfbedeece7182867` was built against the installed
0.1.13 static SDK on Alpine/WSL2. Its callback test and the desktop view test
passed with GCC and with Clang ASan/UBSan and leak detection. The callback test
checks enter, an unrelated leave, actual leave and re-entry; an old tagged
commit is rejected and a fresh commit changes the Aetheri model. A separate
test-only source copy with focus-loss forwarding removed failed the expected
loss assertion. This does not exercise a real Wayland keyboard, device
removal, compositor, native preedit or the held-pointer removal issue.

The transaction-helper candidate based on
`062c23b6dbccab38671ddf978110308b99d67b60` passed its isolated unit test on
Alpine/WSL2 with GCC and with Clang ASan/UBSan and leak detection. The test
checks older same-revision serials, revision changes, focus retirement,
malformed and oversized input, a multi-scalar commit, deletion metadata and
serial exhaustion. A test-only copy without the lower serial bound failed the
old-revision assertion. This is a state-machine test with no Wayland objects or
compositor, not native text-input evidence.
