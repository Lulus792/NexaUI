# Initial macOS AppKit AX validation

The development host is Windows/Linux. The initial AppKit bridge and native fixtures are implemented, but their first macOS CI execution is pending at this revision. Local C regression checks do not compile or validate Objective-C. This report must be updated with actual macOS build/test evidence before claiming native acceptance.

The public C API is unchanged. AppKit objects and handles remain private. Two reusable C snapshots own completed semantic data; cached native elements retire on removal/retyping/context change/detach/teardown. Actions return to the owner thread. The native NexaCloud Metal runner publishes completed frames.

## Registered checks

- `nexaui_macos_accessibility`: real AppKit property/action methods on a native view, application-model effects, Unicode and 1600-character names, protected values, screen movement/hit testing, required/optional selection and nested membership, legacy radio press, group focus, wrong-thread rejection, full native/core queues, allocation failure/retry, context replacement/destruction and held old objects.
- `nexaui_macos_ax_external`: separate C executable with no NexaUI linkage, using system AXUIElement APIs to inspect the hierarchy and observe actual press/toggle/tab model effects. A missing accessibility trust grant returns skip 77, without requesting a privacy prompt. A skipped OS-client case does not validate interprocess AX transport.
- Existing native macOS window, Metal and NexaCloud checks remain required; Windows/Linux regressions and static/shared builds must remain green.

## Initial local evidence

Windows static Debug and Linux static GCC builds succeed after the native context identity extension. Each passes four targeted widget/control/overlay/semantics cases. Local logs are `out/ax-windows-{build,tests}.log` and `out/ax-linux-{build,tests}.log`.

## Remaining acceptance

Full role-specific protocol conformance, typed numeric ranges, editable Value/Text/TextRange, complete table/outline relationships, richer announcements, native notification observation, VoiceOver tasks, physical display/DPI and assistive end-to-end NexaCloud workflows remain open. macOS UI calls and native callbacks require the main thread. Arbitrary library unloading with external native references is not accepted. Linux AT-SPI remains separate work.

Primary references read: Apple [NSAccessibilityElement](https://developer.apple.com/documentation/appkit/nsaccessibilityelement-swift.class), [NSAccessibilityProtocol](https://developer.apple.com/documentation/appkit/nsaccessibilityprotocol), [parent](https://developer.apple.com/documentation/appkit/nsaccessibility-c.protocol/accessibilityparent), [radio values](https://developer.apple.com/documentation/appkit/nsaccessibilityradiobutton/accessibilityvalue%28%29), [selected children](https://developer.apple.com/documentation/appkit/nsaccessibility-c.protocol/accessibilityselectedchildren), [AX trust](https://developer.apple.com/documentation/applicationservices/1460720-axisprocesstrusted) and [OS AX queries](https://developer.apple.com/documentation/applicationservices/1462085-axuielementcopyattributevalue).
