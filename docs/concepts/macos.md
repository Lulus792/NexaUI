# Initial macOS AppKit adapter

`platform/macos/window.m` implements the C window API using AppKit. Objective-C
and native handles stay private. C17 consumers need no Apple headers. The Apple
target links system AppKit, the Objective-C runtime and POSIX synchronization.

## Ownership and input

All window calls require the macOS main thread. Each window owns its NSWindow,
custom NSView, delegate, event queue and file-drop queue. Close sets `should_close`;
the application retires its renderer before destroying the window. A private
renderer lease prevents competing renderers and early destruction. Callback owners
are cleared before C storage is freed, including for externally retained views.

AppKit has one process-wide NSApplication. If NexaUI creates it, a minimal menu
defers Quit until the caller closes its windows. An existing NSApplication and
delegate remain under host control. Polling one window dispatches AppKit events
for all windows; route each independent queue to its own context. Polling bounds
the event batch and uses an autorelease pool. C storage uses the caller allocator;
native objects use Apple's allocation.

Client dimensions and input positions are points, with backing scale reported
separately. Resize, backing-property and miniaturization notifications enter the
normalized stream. A flipped view uses top-left coordinates. Mouse buttons,
dragging, wheel units, key transitions, repeat and Command modifiers are separate
from committed Unicode. Diagonal resize cursors fall back to a crosshair and busy
to an arrow; dedicated AppKit assets for them are not yet provided.

## Text and transfers

The custom view implements NSTextInputClient with bounded uncommitted text.
UTF-16 cursor positions become UTF-8 byte offsets; surrogate-pair interiors are
rejected. Ordinary unmark commits marked text; disabling IME or losing focus
discards it. The logical caret is converted into native screen coordinates.

This is an initial composition slice. Full-document surrounding text and
replacement, revision-bound reconversion, native key-binding actions, complete
Command-based editor navigation and OS secure-input purpose remain M04 work.
Only default/full-mark replacement is accepted; partial/document requests are
ignored. Password purpose returns unsupported and blocks attributed-substring
exposure. It does not claim OS secure keyboard capture.

The common text model accepts Command+A/Z/Shift+Z on macOS, and text-field
clipboard callbacks accept Command+C/X/V. Ctrl remains an explicit compatible
binding; Command identity stays in the normalized event instead of being
rewritten as Ctrl. Mixed Control+Command and Alt chords are not primary actions.
Native word/line/paragraph key bindings remain separate work.

The pasteboard transfers validated UTF-8 strings with a 1 MiB cap, preserving
line endings. File URL drops copy complete UTF-8 path batches with logical
release coordinates before success. Promised/virtual files and other payloads
are unsupported. Receiving a path performs no filesystem operation.

## Validation status

The development host is Windows/Linux. Both configurations still build, and
shared event/drop/input/dependency tests pass. Host results do **not** compile
or validate Objective-C.

The authored `nexaui_macos_window` test creates real hidden AppKit windows and
checks callbacks, Unicode/preedit, resize, two-window isolation, wrong-thread
calls, allocator sizes, renderer leases, clipboard and file URL drops. Its
clipboard case snapshots/restores the original items. The C17 hello-window
example is also enabled on Apple builds. In-process callbacks do not prove
physical input/IME, monitor transitions or VoiceOver behavior.

On 2026-10-15, commit `addc4a6` compiled AppKit with AppleClang 21.0.0 and
passed `nexaui_macos_window` on both static and shared macOS CI builds
([static job](https://github.com/Lulus792/NexaUI/actions/runs/38654334451/job/116160404445),
[shared job](https://github.com/Lulus792/NexaUI/actions/runs/38654334451/job/116160404563)).
Each ran 74 tests with 73 passing. The sole failure was an outdated core-test
conditional that still expected macOS window creation to be unsupported; it
now exercises the same actual lifecycle checks as Windows/Linux. The follow-up
full matrix must pass before claiming a green integration run.

The dependency audit permits AppKit/objc only on Apple targets and checks
Objective-C `#import` as well as C `#include`. Its positive/negative fixtures pass
on Windows/Linux. Common text-buffer/field regressions pass on both hosts and
under Linux ASan/UBSan. The Apple Command policy branch was also compiled and
executed as portable C under Linux ASan/UBSan (two passing model/field probes).
That probe is a Linux executable with the policy branch selected; it is not an
Apple SDK build or macOS runtime result. The actual native CI jobs above also
passed text-buffer and text-field checks; physical Command/IME acceptance remains open.

An initial [Metal target](metal.md) implements clear/present and opaque geometry
with nested clips; native validation, alpha and image commands remain pending. A native NexaCloud
macOS runner is not yet available. A blank window or headless scene is not
complete macOS UI acceptance.

## Primary references

- [AppKit key handling](https://developer.apple.com/library/archive/documentation/Cocoa/Conceptual/EventOverview/HandlingKeyEvents/HandlingKeyEvents.html)
- [NSTextInputClient](https://developer.apple.com/documentation/appkit/nstextinputclient)
- [Unmark commits pending text](https://developer.apple.com/documentation/appkit/nstextinputclient/unmarktext%28%29?language=objc)
- [High-resolution coordinates](https://developer.apple.com/library/archive/documentation/GraphicsAnimation/Conceptual/HighResolutionOSX/APIs/APIs.html)
- [Dragging destinations](https://developer.apple.com/documentation/AppKit/NSDraggingDestination)
