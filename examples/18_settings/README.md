# Settings example (in progress)

This small C17 example keeps an application-owned display name and reduced-motion
choice. Apply copies a valid draft into the example's in-memory model only; it
does not change OS or Aetheri settings. The scene uses public text-field,
checkbox, button, focus, semantic and theme APIs. The Linux host passes its
window to the text field for native IME and clipboard routing, publishes the
semantic tree, and retires the renderer before destroying the scene and window.

With Linux Vulkan enabled, run `nexaui_settings /path/to/font.ttf`. The runner
currently uses the private build-tree Vulkan adapter and is not an installed-SDK
consumer. `--smoke` closes after a bounded native run. The headless scene test
checks Tab and Shift-Tab traversal, text editing, checkbox and button activation,
validation, applied model values and status semantics. It uses synthetic events.

An independent libatspi test discovers this actual native example, activates
Reduce motion and Apply, and requires the application to observe the applied
model and exit through normal cleanup. It runs only in the private Weston and
accessibility-bus test fixture.
The client checks the nonstandard `nexaui-value` attribute and the supported
read-only AT-SPI Text queries using a Unicode draft and selection. Text range,
character, caret and selection reads and change notifications are available
for valid, non-protected built-in text fields. The Text interface is partial:
caret/selection movement, text geometry and non-character granularity remain open.
EditableText currently supports replacing the full text; insert, delete and
clipboard operations remain open. The independent test sets the name through
AT-SPI and checks that Apply receives it.

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
