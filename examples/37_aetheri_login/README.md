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
placement, native IME, human screen-reader interaction, credential byte lifetime
across native event queues, backend admission and real session readiness remain open.
