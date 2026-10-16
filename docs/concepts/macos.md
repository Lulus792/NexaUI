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
replacement, revision-bound reconversion, complete native key bindings and OS
secure-input purpose remain M04 work.
Only default/full-mark replacement is accepted; partial/document requests are
ignored. Password purpose returns unsupported and blocks attributed-substring
exposure. It does not claim OS secure keyboard capture.

For an active editor, AppKit's `interpretKeyEvents:` produces committed text or
`doCommandBySelector:` actions. Supported word, visual/logical caret, line,
paragraph, document, selection, deletion, scrolling and focus selectors become
`NX_EVENT_TEXT_COMMAND`. Physical presses already delegated to the interpreter
use `NX_EVENT_KEY_DOWN_CONSUMED`, so widgets do not apply them a second time.
Command+A/C/X/V/Z/Shift+Z use the same action path; original modifiers remain
unchanged. Unrelated Command shortcuts remain available to the host application.

Unknown text selectors are ignored locally. The specialized
[NSTextInputClient contract](https://developer.apple.com/documentation/appkit/nstextinputclient/docommand%28by%3A%29)
explicitly prohibits forwarding them to `super`, unlike the general NSResponder
implementation. Supported ordinary commands commit pending marked text first;
Cancel and Undo discard pending composition. Plain Page Up/Down scroll the
viewport without changing the selection. Their selection variants also move the
caret; explicit `MOVE_PAGE` commands provide caret page movement for other input
policies. See the [command validation report](../quality/macos-input-commands-report.md)
for executed native tests and the remaining limits.

The pasteboard transfers validated UTF-8 strings with a 1 MiB cap, preserving
line endings. File URL drops copy complete UTF-8 path batches with logical
release coordinates before success. Promised/virtual files and other payloads
are unsupported. Receiving a path performs no filesystem operation.

## Validation status

The development host is Windows/Linux; local C regression results do **not**
compile or validate Objective-C. Native static/shared macOS CI results are
recorded separately below and in the AX validation report.

The native `nexaui_macos_window` test creates real hidden AppKit windows and
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
now exercises the same actual lifecycle checks as Windows/Linux. Subsequent
native integration runs below and in the command report provide passing evidence.

The dependency audit permits AppKit/objc only on Apple targets and checks
Objective-C `#import` as well as C `#include`. Its positive/negative fixtures pass
on Windows/Linux. Common text-buffer/field regressions pass on both hosts and
under Linux ASan/UBSan. The Apple Command policy branch was also compiled and
executed as portable C under Linux ASan/UBSan (two passing model/field probes).
That probe is a Linux executable with the policy branch selected; it is not an
Apple SDK build or macOS runtime result. The actual native CI jobs above also
passed text-buffer and text-field checks; physical Command/IME acceptance remains open.

The [Metal target](metal.md) now passes all native shape/clip/alpha/lifecycle
checks in static/shared macOS CI at `74aa8ec` (77/77 tests in each job). Both
consumer variants build, and the installed consumer runs. At `fb80bd0`, both
variants pass 79/79 tests, including RGBA8/A8 uploads and native NexaCloud GPU
comparisons. The three static/shared scene captures are byte-identical and
reviewed; [actual client sizes and limits](../nexacloud-validation.md#native-macos-scene--2026-10-15)
are recorded. Physical input, IME, DPI and VoiceOver remain open.

The initial native editor-command path at `03bb8cf` also passes 86/86 tests in
each macOS variant, including actual AppKit interpretation and selector-to-model
checks. See [the command report](../quality/macos-input-commands-report.md) for
the tested subset, counterexamples and outstanding physical/IME acceptance.

## Primary references

- [AppKit key handling](https://developer.apple.com/library/archive/documentation/Cocoa/Conceptual/EventOverview/HandlingKeyEvents/HandlingKeyEvents.html)
- [NSTextInputClient](https://developer.apple.com/documentation/appkit/nstextinputclient)
- [Unmark commits pending text](https://developer.apple.com/documentation/appkit/nstextinputclient/unmarktext%28%29?language=objc)
- [High-resolution coordinates](https://developer.apple.com/library/archive/documentation/GraphicsAnimation/Conceptual/HighResolutionOSX/APIs/APIs.html)
- [Dragging destinations](https://developer.apple.com/documentation/AppKit/NSDraggingDestination)

## Initial accessibility transport

Completed frames can be published through `nx_window_publish_semantics`; the native NexaCloud Metal runner does this before rendering. The private AppKit bridge provides owned semantic copies and native `NSAccessibilityElement` objects with queued owner-thread actions. See [the transport contract](accessibility.md#initial-macos-appkit-ax-bridge) and [validation report](../quality/macos-ax-report.md). Native protocol checks and a separate OS AX client are registered. Accessibility trust is checked without prompting; a missing trust grant is an explicit skip of the external case. Full VoiceOver, text/range/table and physical display acceptance remain open.

The initial AX implementation passes all 85 native/core cases in each static/shared Debug build on macOS 26.6.2 ARM64 with AppleClang 21. Both the AppKit server-method fixture and independent OS AX client ran successfully, including actual action-model changes. See the [executed AX evidence](../quality/macos-ax-report.md). This does not establish VoiceOver, Intel, physical input/display or full role-specific protocol acceptance.
