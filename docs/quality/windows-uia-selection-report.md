# Windows UIA single-selection validation — 2026-10-16

This follow-up adds an application-owned single-selection model to the portable semantic API and native Selection/SelectionItem transport to the [initial Windows UIA bridge](windows-uia-report.md). It also tests the real NexaCloud application loop through an independent UIA client. It does not establish full UIA control-type conformance or screenreader acceptance.

## Executed environment and checks

Local Windows checks use Windows 11 build 26200, MSVC C17 19.38.33145.0, SDK 10.0.26100.0 and CMake 4.3.2. AddressSanitizer uses RelWithDebInfo and the runtime beside the configured compiler. Linux checks use Alpine 3.24 under WSLg, GCC 15.2, Clang 22.1.3 and Mesa Lavapipe; native X11 and Wayland tests run against WSLg.

| Executed check | Result |
| --- | --- |
| Windows static Debug full suite | 96 passed; clipboard skipped; 97 registered |
| Windows shared Debug full suite | 96 passed; clipboard skipped; 97 registered |
| Additional group-focus case, static Debug | STA, MTA and library-owned COM hosts passed |
| Windows static MSVC ASan: three UIA hosts, semantics, real NexaCloud UIA | 5 passed; no sanitizer finding |
| Windows shared MSVC ASan: same five cases | 5 passed; no sanitizer finding |
| Linux static Debug full suite | 101 passed, including native X11/Wayland Vulkan and NexaCloud examples |
| Linux Clang ASan/UBSan/LeakSanitizer: semantics, widgets, controls, overlay | 4 passed; no sanitizer finding |

The full-suite counts above record the initial selection implementation (`ebd8081`); the compact-window follow-up below adds a 98th registered Windows test. The additional focus case was added after the full static suite and passed on all three static hosts; it is included in the full shared suite and sanitizer cases. The clipboard fixture skipped without modifying the user's contents. These finite sanitizer tests do not establish general COM/OS leak freedom or race freedom.

## Portable model and native behavior

Named GROUP/LIST/TREE/TAB_LIST containers bind a selected widget ID without retaining its application pointer. Optional groups allow zero or one member; required groups require one. TAB_LIST requires selection. The nearest explicit ancestor owns membership across intermediate panels, and nested groups remain independent. Radio CHECKED state participates in this contract. End-frame rejects invalid cardinality. Failed group registration preserves the caller's selected ID and queued request; early apply supports selection-dependent layout.

The portable fixture checks replacement, conditional add, optional removal, required removal rejection, ordered requests, no synthetic clicks, nested groups, invalid trees and allocation failure/retry. Windows exposes current zero/one selection arrays, required/multiple flags, selected status, actual selection owner and Select/AddToSelection/RemoveFromSelection. Group focus delegates to an eligible selected member. The native fixture checks disabled members, independent groups, committed selection/removed/invalidation events and model effects on the owner thread.

One pending operation per native group stays pending through core forwarding until a genuinely newer completed frame is published. Identical pending operations are idempotent; conflicting ones report busy. Republishing the same frame does not acknowledge work. Target and owner incarnations prevent a queued selection from moving to another group when an item is reparented. The COM interface set is static for an object's lifetime; pattern advertisement and methods validate current capabilities separately.

## Counterexamples and retained handles

A negative build deliberately removed the published-frame guard. The context advanced and moved an item to another selection container before publication, while the native request still referred to its previously displayed owner. Without the guard, the independent client observed selection in the wrong container and failed. Restoring the guard makes routing wait for publication and then revalidate the captured owner identity. The final probe passes. This is evidence for the exercised ordering, rather than a proof of every possible interleaving.

On the checked Windows runtime, retained pattern read access after context replacement can normalize unavailable data to S_OK with false/empty selection. The independent client requires old element names to be unavailable, rejects mutation through the old pattern, and verifies that the replacement model remains unchanged. Successful false/empty access alone is not treated as evidence that the old element is live. This runtime observation is scoped to this environment.

## Real NexaCloud workflow

`nexaui_nexacloud_uia` launches the ordinary native D3D12 NexaCloud loop and a separate MTA UIA client. The client links Windows UIA/COM services and no NexaUI implementation. It selects the Files navigation item, verifies Bergsee.jpg selection and its preview name, selects Wald.png and verifies its preview, clears selection, leaves the page, checks that the removed file handle is unavailable, returns and selects Wald.png again. Bounded UIA polling observes the actual published UI; this workflow has no private model/ack pipe.

Navigation and file selection use the same portable model APIs on Windows, macOS and Linux. Native AX and AT-SPI transport are still pending. Existing headless and Linux native example checks pass; no claim of macOS AX, Linux AT-SPI or real screenreader operation follows from the Windows client.

## Responsive-window client follow-up

The first CI run of `ebd8081` passed both macOS builds, both Linux builds, Linux sanitizers and the three Windows UIA fixtures, but both Windows Server 2025 jobs failed the new NexaCloud preview expectation. A local 800 × 700 native window reproduced the same timeout: the client assumed the side inspector would always exist, whereas narrow NexaCloud layouts present details in a modal opened through the details command.

The independent client now uses that exposed command when the side preview is absent, checks the preview, invokes Close and waits for the file's interaction scope to become enabled before continuing. It neither bypasses modality nor reaches into the application model. A mandatory `nexaui_nexacloud_uia_compact` case runs the same workflow with an 800 × 700 requested client size. The host logs its actual logical client size and scale so runner-dependent geometry remains visible. The corrected wide/compact pair passes in static and shared Debug and in both MSVC ASan configurations (two cases per configuration, no sanitizer finding). The original compact negative log is retained as `out/selection-compact-negative-test.log`; the passing static pair is in `out/selection-responsive-tests.log`; the shared and ASan pairs are in `out/selection-responsive-shared-tests.log` and `out/selection-responsive-build-msvc-asan-{static,shared}-tests.log`. Windows Release and Linux production builds with tests disabled also succeed.

## Reproduction and local evidence

```powershell
cmake --build build --config Debug --parallel 4
ctest --test-dir build -C Debug --output-on-failure --parallel 4
cmake --build build-shared --config Debug --parallel 4
ctest --test-dir build-shared -C Debug --output-on-failure --parallel 4

# Put the configured compiler's ASan runtime directory on PATH first.
cmake --build build-msvc-asan-static --config RelWithDebInfo --parallel 4
ctest --test-dir build-msvc-asan-static -C RelWithDebInfo -R '^nexaui_(uia.*|semantics|nexacloud_uia.*)$' --output-on-failure
# Repeat for build-msvc-asan-shared.
```

Local logs: `out/selection-static-final-tests.log`, `out/selection-focus-tests.log`, `out/selection-focus-other-hosts.log`, `out/selection-shared-final-tests.log`, `out/selection-build-msvc-asan-static-final-tests.log`, `out/selection-build-msvc-asan-shared-final-tests.log`, `out/selection-linux-tests.log` and `out/selection-linux-asan-tests.log`. The intentionally failing guard counterexample is in `out/selection-no-frame-guard.log`; the restored positive probe is in `out/selection-frame-gate-tests.log`. Local development logs are not bundled release artifacts. The normal [CI workflow](../../.github/workflows/ci.yml) covers static/shared Windows, macOS and Linux builds plus Linux sanitizers.

## Remaining acceptance

Multi-selection, editable Value/Text/TextRange, RangeValue, ExpandCollapse, Grid/Table and scrolling patterns, complete control-type properties, richer property/text/live announcements, ancestor clipping, physical DPI/large-text checks and safe arbitrary DLL unloading with retained providers remain open. Narrator tasks, macOS AX, Linux AT-SPI and cross-platform screenreader scenarios remain open. See [the accessibility contract](../concepts/accessibility.md).

Primary references: Microsoft [Selection provider](https://learn.microsoft.com/en-us/windows/win32/winauto/uiauto-implementingselection), [SelectionItem provider](https://learn.microsoft.com/en-us/windows/win32/winauto/uiauto-implementingselectionitem), [Tab control type](https://learn.microsoft.com/en-us/windows/win32/winauto/uiauto-supporttabcontroltype), [SAFEARRAY interface insertion](https://learn.microsoft.com/en-us/windows/win32/api/oleauto/nf-oleauto-safearrayputelement), [QueryInterface contract](https://learn.microsoft.com/en-us/windows/win32/api/unknwn/nf-unknwn-iunknown-queryinterface(refiid_void)) and [client property defaults](https://learn.microsoft.com/en-us/windows/win32/winauto/uiauto-propertiesforclients). Runtime observations above are local evidence, not general promises derived from those references.
