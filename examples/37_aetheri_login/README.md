# Aetheri login conversation view

This standalone C17 consumer composes Aetheri's original login conversation
through NexaUI's public text field, password, focus and semantic control APIs.
The host owns the conversation and decides when to bind an independently
admitted service. Passing a null conversation shows an unavailable message.
The test uses a synthetic in-process conversation and the existing fixture font.

Export `src/login/protocol.{c,h}` and
`src/text/{grapheme.c,grapheme.h,grapheme_data.h}` from Aetheri revision
`6eda6feb98b37048ca84d73b65e24f919297987c` along with its license. Use NexaUI
0.1.33 built from `529d5a9d102d975f4fa0664df4936d343bc63b58` for the recorded
Linux runs.

```sh
cmake -S examples/37_aetheri_login -B /tmp/aetheri-login-view \
  -DCMAKE_PREFIX_PATH=/path/to/nexaui-installed \
  -DAETHERI_SOURCE_DIR=/path/to/aetheri-export
cmake --build /tmp/aetheri-login-view
ctest --test-dir /tmp/aetheri-login-view --output-on-failure
```

Create a view for one conversation on the UI owner thread. Queue input into its
context and build a frame. The host rebuilds when `AETHERI_LOGIN_REPAINT` is set
before rendering or publishing semantics. `AETHERI_LOGIN_REQUEST_QUEUED` signals
one explicitly requested model operation. The original owner handles pending
bytes and reports backend observations separately. Pass its native window only
when the host is ready to route native IME events. Destroy the view before
destroying the window, fonts or conversation.

The original prompt type controls whether the view accepts a visible draft, a
masked draft or an acknowledgement. Empty answers and information acknowledgements
retain distinct wire forms defined by Aetheri. State changes retire the previous
control IDs and draft. The view discards the sensitive field on accepted answer,
cancel, observed loss or destruction. It wipes context-owned allocations on
release. The host owns native event copies and must manage their retention,
the transport and the lifetime of the original conversation. The model's start
acknowledgement is shown as acceptance of a session request.

Long messages use a separate scroll area above the response and action controls.
If a message overflows, the scrollbar gets initial focus. Use arrows, Page Up,
Page Down, Home or End to read it and Tab to reach the controls. If the message
fits after a resize, focus returns to the response or first available action.
The semantic label retains the complete message. Changing prompt resets the
scroll position.

The headless tests cover a maximum-size message at the minimum view size,
including a rendered final glyph and Tab navigation.

An optional Linux build can borrow Aetheri's original admitted connection with
`-DNEXAUI_LOGIN_WITH_CONNECTION_MODEL=ON`. Add `src/login/connection.{c,h}` to
the frozen source export; the connection checks use original revision
`7d5fa4723c13460e9d61dc7db33b6a521409053f` and installed SDK source
`368cd38292498bd538964ba514ff02e0e9924358`. The default build keeps the minimal
conversation-only inputs above. The target publishes the feature macro for
`aetheri_login_view_create_connection` in `view.h`.

The host independently constructs and admits that connection before binding
it. The view borrows it and routes explicit commands through its original
methods, including their current admission checks. Status is the connection's
local observation, not a fresh admission check. The host owns watch/pump,
readiness/deadlines, interruption and close; it must keep the connection alive
until after destroying the view. Replacing or closing a view does not cancel,
restart or replay a conversation. A missing connection displays unavailable.
Native service/greeter/policy admission and trusted placement remain external.

On Linux libraries lacking `sys/pidfd.h`, the example uses a private header
adapter over the installed kernel UAPI and the Linux socket ABI constants.
It supplies no emulated process pins or checks. The imported connection source
remains byte-identical; only its musl `CMSG_NXTHDR` mixed-sign diagnostic is
excluded from `-Wsign-compare`. The view and tests retain their strict warnings.
The runtime still requires actual peer pidfds, pidfs and `PIDFD_GET_INFO`.

The optional native fixture uses an ordinary Wayland window, the software
presenter and a separate libatspi client. Build it with
`-DNEXAUI_LOGIN_BUILD_NATIVE_FIXTURE=ON` and the Linux AT-SPI development files.
Run the pair in the repository's private Weston and AT-SPI environment with
a font that covers the sample strings. Both executables require the private
environment provided by these runners.

```sh
NEXAUI_PLATFORM=wayland NEXAUI_LOGIN_TEST_FONT=/path/to/font.ttf \
NEXAUI_LOGIN_TEST_XDOTOOL=/usr/bin/xdotool \
xvfb-run -a python3 tests/run_weston_native.py --timeout 60 -- \
  python3 tests/run_atspi_native.py /path/to/at-spi2-registryd \
    /tmp/aetheri-login-view/aetheri_login_atspi_peer \
    /tmp/aetheri-login-view/aetheri_login_native_host
```

The peer observes password role and interface suppression, traverses Answer
and Cancel with Tab and Shift+Tab, types fixture answers with the private seat,
observes retired objects, and closes with Escape. The fixture consumes two
exact synthetic responses and requires a clean exit. Trusted compositor
placement, human screen-reader interaction, backend admission and real session
readiness remain open.

With both optional build flags enabled, add `--connection` to `run_native.py`
to exercise the borrowed-connection view through real local socket I/O and
kernel-observed self-process pins. The synthetic fixture's callback admits only
its own test setup; this is no claim of production service, greeter or policy
admission. The independent native peer keeps its original assertions and limits.

The native runner can also configure Compose on its owned Xvfb seat and start
Weston's `weston-simple-im`. It uses `setxkbmap` or an existing `xkbcomp` to
configure that private keyboard. Set `NEXAUI_WESTON_LIBEXEC` to the directory
containing the Weston helpers.

```sh
python3 examples/37_aetheri_login/run_native.py \
  --build-dir /tmp/aetheri-login-view --font /path/to/font.ttf \
  --registry /path/to/at-spi2-registryd --work-dir /tmp/login-ime-run --ime
```

Add `--cancel-preedit` with a fresh work directory to cancel secret preedit
through an AT-SPI focus change and then type an unaccented response. Both IME
tasks also compose a visible response. They require observed native preedit,
its removal, an empty committed visible field during preedit, exact original
model responses, protected secret interfaces and retired field objects.

The keyboard task uses Tab and Return to navigate and submit. The IME tasks
use AT-SPI focus and activation for the first field exit and submissions.
Weston's simple provider was observed sending Tab as a text commit containing
`"\t"`. The retained failed attempt and protocol trace document this limitation.
Keyboard navigation and submission through another installed input-method
provider, candidate placement, other native IME storage and broader language
coverage still require acceptance.

The [password-storage check](../../docs/quality/wayland-password-storage-report.md)
overwrites retired owned Wayland password preedit copies. The later
[key-handoff check](../../docs/quality/wayland-ime-key-handoff-report.md) fixes a
consumed Tab release across composition, preserving this peer's physical
Shift+Tab return after cancellation. These checks do not close the broader
provider, other native-storage or host/service gates above.
