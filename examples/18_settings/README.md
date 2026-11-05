# Settings example (in progress)

This small C17 example keeps an application-owned display name and reduced-motion
choice. Apply submits a snapshot to a deterministic in-memory example backend;
the host completes it after a short timer, and only then does the example update
its model. It does not change OS or Aetheri settings. The scene uses public text-field,
checkbox, button, focus, semantic and theme APIs. The Linux host passes its
window to the text field for native IME and clipboard routing, publishes the
semantic tree, and retires the renderer before destroying the scene and window.

With Linux Vulkan enabled, run `nexaui_settings /path/to/font.ttf`; add `--de`
to select the German example catalog. The application-owned scene can switch
between `en` and `de` with `settings_set_language` without changing semantic
IDs, the focused field, draft or in-flight submission. Unsupported codes are
rejected. This example catalog does not implement general plural rules,
locale-sensitive formatting or system language detection. The runner
currently uses the private build-tree Vulkan adapter and is not an installed-SDK
consumer. `--smoke` closes after a bounded native run. The headless scene test
checks Tab and Shift-Tab traversal, text editing, checkbox and button activation,
validation, applied model values and status semantics. It uses synthetic events.
Run `nexaui_settings /path/to/font.ttf --simulate-error` to make the first
submission fail; the status describes the example failure and Apply retries.
The option can be combined with `--de`.
While a submission is pending Apply is disabled, edits remain a separate draft,
and a late or duplicate completion cannot apply another submission. A newer
draft remains unsaved after the submitted snapshot succeeds. Closing destroys
the local pending operation. This timer is a demonstration, not an OS service,
worker-thread wakeup implementation or proof of external operation completion.

An independent libatspi test discovers this actual native example, activates
Reduce motion and Apply, and requires the application to observe the applied
model and exit through normal cleanup. It runs only in the private Weston and
accessibility-bus test fixture.
An additional native-client case observes the simulated failure in the status,
retries Apply, and waits for successful model confirmation and normal exit.
The client checks the nonstandard `nexaui-value` attribute and the supported
read-only AT-SPI Text queries using a Unicode draft and selection. Text range,
character, caret and selection reads and change notifications are available
for valid, non-protected built-in text fields. The Text interface is partial:
text geometry and non-character granularity remain open. A single selection and
caret can be changed at grapheme boundaries through AT-SPI, with notifications.
EditableText supports full replacement, bounded insert/delete and clipboard
operations. The independent test sets the name through
AT-SPI and checks that Apply receives it.

The private-Weston AT-SPI tests verify that copy/cut fail without a Wayland
input serial and that copy/cut/paste work after virtual keyboard input. If
`wl-copy` and `wl-paste` are installed at configure time, an additional test
transfers Unicode to and from those separate Wayland processes. The test uses
an isolated compositor and virtual input; physical interaction remains open.

`nexaui_settings_ime_v3` connects the same scene and native window adapter to a
private Wayland text-input-v3 protocol fixture. It checks preedit, a UTF-8
commit, no duplicate commit, separation from the applied model, and clearing
preedit after focus moves. It also exercises a queued external text replacement
against an already queued IME commit: the stale commit is rejected, the old
preedit is cleared and the focused input is reactivated. An equal-value request
does not interrupt composition. The fixture invokes the shared semantic request
directly; the separate native AT-SPI test verifies transport. It does not
exercise an installed input method or both transports in the same process.

This is not yet the complete first-view acceptance. Native IME/clipboard input,
accessible text editing, human screenreader and keyboard checks, visual/contrast
review, broader resize and text-scaling behavior,
worker wakeup, hidden-window behavior, and portable public renderer packaging remain open. Windows
and macOS native runners are not supplied. The options use a scroll area; a
synthetic test checks focus visibility at 240x150. Below 128x120 the runner
continues polling without drawing a new frame. Long-label reflow and native
small-window accessibility remain open.

The example wraps its English or German labels and status messages to the available
column. If an individual word cannot wrap, it reduces that label's size enough
to fit. At narrow widths the motion label moves below its checkbox; Apply fits
the content width. While focus is on the checkbox or Apply, Page Down and Page
Up scroll by half a viewport to inspect long status text. The scene and private
Weston tests exercise the 128x150 and 240x150 cases, including keyboard focus
and status scrolling. These geometry checks do not replace visual review, do not
establish 200% text-scale or complete translation acceptance, and at the minimum
height a whole multiline label may not fit in one viewport.

The native runner redraws after routed input or size/scale changes rather than
continuously after a successful presentation. A changed focus schedules one
follow-up frame so that its control can be scrolled into view; a viewport-height
change also rechecks the existing focus. It still polls at one-second
intervals to retry unavailable accessibility registration and retries a
suspended renderer more frequently. This is not a complete idle or recovery
contract. Private Weston virtual-keyboard tests using the default screen keyboard
exercise text, Tab, checkbox, Apply, model confirmation and clean exit. The
small-window test waits for each focused control's published bounds to fit the
viewport before releasing Tab and continuing. A separate test with
`weston-simple-im` still fails the keyboard workflow; the controlled v3 test
does not establish real installed-IME acceptance. See the adoption assessment
for preserved traces and the limits of these checks.

An opt-in `nexaui_settings_wayland_simple_ime` test separately exercises an
actual `weston-simple-im` compose sequence: a private Xvfb seat sends Compose,
apostrophe and `a`; Settings observes the preedit, then the committed `á`, with
the applied model unchanged, and closes through its normal cleanup. It checks
the Wayland wire trace as well as the scene's state. This does not resolve the
keyboard-workflow failures above, test a production IME, or establish human
accessibility and display acceptance. CTest skips it unless explicitly enabled.

To run it, provision Weston with `weston-simple-im` and its desktop shell,
`xvfb-run`, `setxkbmap`, `xdotool`, D-Bus and a Vulkan implementation. On Linux,
from the repository root (adjust the build and helper paths):

```sh
NEXAUI_TEST_SIMPLE_IME=1 NEXAUI_WESTON_INPUT_METHOD=weston-simple-im \
NEXAUI_WESTON_LIBEXEC=/path/to/weston/libexec \
xvfb-run -a -s '-screen 0 1280x1024x24 -noreset' sh -c \
  'setxkbmap -option compose:ralt && python3 tests/run_weston_native.py -- dbus-run-session -- ctest --test-dir build -R nexaui_settings_wayland_simple_ime --output-on-failure'
```

`-noreset` keeps the private X server's Compose mapping after `setxkbmap`
exits. The test uses evdev X11 keycode 108 for right Alt; a different XKB map
must be adapted rather than counted as a pass. Set `NEXAUI_TEST_IME_TRACE` to
an output path to retain the test process's Wayland trace; it includes the
example's fixed draft text and is not intended for real private data.

The same opt-in selection also runs
`nexaui_settings_wayland_simple_ime_focus_cancel`. Once the preedit is visible
to the application, a second X11 window on that private Xvfb takes keyboard
focus. The test requires Wayland keyboard leave and text-input deactivation,
checks that the draft and applied model stayed unchanged, restores focus, and
verifies that typing `a` inserts only `a` rather than the abandoned composition.
The peer is destroyed before the private X server exits. This checks window
focus loss in the virtual fixture, not focus changes between individual controls
or physical/human interaction.

When the independent libatspi test dependencies are available, a third opt-in
test moves focus from the field to the checkbox and back using real AT-SPI
`GrabFocus` requests while the same native IME is active. It verifies preedit
cancellation without a window-level keyboard leave and that subsequent text
reaches the original draft. The helper links to libatspi, not NexaUI.
