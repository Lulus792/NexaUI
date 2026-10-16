# Initial macOS AppKit AX validation

The AppKit bridge and its native probes pass on macOS 26.6.2 (25G83), ARM64, AppleClang 21.0.0.21000101 with Xcode 26.6, runner image macos-26-arm64/20261013.0424.1. Both static and shared Debug configurations executed all 85 registered cases successfully. The independent OS AX client ran and passed in both configurations; it was not skipped. This is native transport/model evidence, not a VoiceOver acceptance result.

The verified implementation is [b21d59c](https://github.com/Lulus792/NexaUI/commit/b21d59c9d943a63fe89c1e43fbee6c0e2cb5f411). Its [CI run](https://github.com/Lulus792/NexaUI/actions/runs/38887064537) also passes all six Windows/macOS/Linux static/shared builds and Linux sanitizers. Optional Windows ASan and fuzz jobs were not requested in that run; this report makes no macOS sanitizer claim.

The public C API is unchanged. AppKit objects and handles remain private. Two reusable C snapshots own completed semantic data; cached native elements retire on removal/retyping/context change/detach/teardown. Actions return to the owner thread. The native NexaCloud Metal runner publishes completed frames.

## Executed native checks

| Check | Static | Shared |
| --- | --- | --- |
| Full native/core suite | 85/85 passed | 85/85 passed |
| AppKit server-method fixture | Passed, 0.28 s | Passed, 0.19 s |
| Separate OS AX client | Passed, 0.66 s | Passed, 0.57 s |
| Installed C consumer | Configured, built and ran | Configured, built and ran |
| add_subdirectory C consumer | Configured and built | Configured and built |

The OS client reads the actual AX hierarchy and checks names, roles, protected empty values and application-model changes after Press, checkbox Toggle and tab selection. The server-method fixture additionally checks required/optional selection, row/tab relationships, no synthetic selection clicks, absolute flipped coordinates, hit testing, two-window isolation, retained strings, queue backpressure, allocation rollback and stale objects. Native NexaCloud Metal/GPU-reference cases remain green with publication enabled.

The new AX adapter/window changes and probe files emit no warnings in these builds. Pre-existing warnings elsewhere remain: core shape initializers, signed enum comparisons, a headless size comparison, deprecated Metal fastMathEnabled and duplicate static library linkage. This is not a warning-free-project claim.

## Fixture scope

- `nexaui_macos_accessibility`: real AppKit property/action methods on a native view, application-model effects, Unicode and 1600-character names, protected values, absolute flipped screen coordinates, window movement/hit testing, two-window isolation, retained native strings, required/optional selection and nested membership, legacy radio press, group focus, wrong-thread rejection, full native/core queues, allocation failure/retry, context replacement/destruction and held old objects.
- `nexaui_macos_ax_external`: separate C executable with no NexaUI linkage, using system AXUIElement APIs to inspect the hierarchy and observe actual press/toggle/tab model effects. A missing accessibility trust grant returns skip 77, without requesting a privacy prompt. A skipped OS-client case does not validate interprocess AX transport.
- Existing native macOS window, Metal and NexaCloud checks remain required; Windows/Linux regressions and static/shared builds must remain green.

## Retained evidence

The uploaded LastTest.log files were downloaded and read for both configurations. They contain the explicit messages `Independent OS AX client passed names/roles/protection/press/toggle/selection model checks` and `Native AppKit AX properties/actions/selection/lifetime/allocation checks passed`, followed by Test Passed. The artifact hashes match GitHub's published SHA-256 digests:

| Artifact | ID | SHA-256 |
| --- | --- | --- |
| macos-gpu-captures-shared-OFF | 12040048716 | a4082dcd1acabfee156f826e545e8d94b7b4c976529617148177b3d380c5fc58 |
| macos-gpu-captures-shared-ON | 12039834589 | 3bd5265c0b561c359a9ed98f19a338155c344e28e112054bc7d216fd3919592a |

Local development copies are `out/ax-b21d59c-{static,shared}.zip` and `out/ax-b21d59c-{static,shared}-LastTest.log`. They are not bundled release artifacts. Permission-aware skip behavior remains in the fixture for hosts where AXIsProcessTrusted is false; such a future skip must not be reported as a successful OS-client run.

## Local C regression evidence

Windows static Debug and Linux static GCC builds succeed after the native context identity extension. Each passes four targeted widget/control/overlay/semantics cases. Local logs are `out/ax-windows-{build,tests}.log` and `out/ax-linux-{build,tests}.log`.

## Remaining acceptance

Full role-specific protocol conformance, typed numeric ranges, editable Value/Text/TextRange, complete table/outline relationships, richer announcements, native notification observation, VoiceOver tasks, physical display/DPI and assistive end-to-end NexaCloud workflows remain open. macOS UI calls and native callbacks require the main thread. Arbitrary library unloading with external native references is not accepted. Linux AT-SPI remains separate work.

Primary references read: Apple [NSAccessibilityElement](https://developer.apple.com/documentation/appkit/nsaccessibilityelement-swift.class), [NSAccessibilityProtocol](https://developer.apple.com/documentation/appkit/nsaccessibilityprotocol), [parent](https://developer.apple.com/documentation/appkit/nsaccessibility-c.protocol/accessibilityparent), [radio values](https://developer.apple.com/documentation/appkit/nsaccessibilityradiobutton/accessibilityvalue%28%29), [selected children](https://developer.apple.com/documentation/appkit/nsaccessibility-c.protocol/accessibilityselectedchildren), [AX trust](https://developer.apple.com/documentation/applicationservices/1460720-axisprocesstrusted) and [OS AX queries](https://developer.apple.com/documentation/applicationservices/1462085-axuielementcopyattributevalue).

## Review before native execution

Value notifications compare the value actually returned by the native getter; focus/enabled-only changes are not reported as value changes. Selection notifications compare member incarnations as well as IDs, and list/outline selection posts selected-row changes too. A protected text subrole change invalidates the layout. Notifications retain native targets before call-outs so reentrant detach cannot invalidate C snapshot reads. The native getter/action checks above pass with these rules in place. External notification observation remains an open runtime gate.
