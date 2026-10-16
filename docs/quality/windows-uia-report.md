# Initial Windows UIA bridge validation — 2026-10-15

NexaUI now publishes a completed semantic tree to Windows UI Automation and forwards native focus, Invoke and Toggle requests to the context owner thread. The Windows NexaCloud runner publishes its frames through the new API. This report covers the initial transport; it does not accept full screenreader operation or complete UIA pattern coverage.

## Local environment and results

Windows 11 build 26200, Visual Studio 2022, MSVC C17 compiler 19.38.33145.0, Windows SDK 10.0.26100.0, CMake 4.3.2. Debug builds use the existing static and shared build trees. MSVC AddressSanitizer builds use RelWithDebInfo and the runtime beside the configured compiler. Linux checks use Alpine 3.24 under WSLg, GCC 15.2 and Clang 22.1.3; native X11 and Wayland tests run against WSLg/Mesa Lavapipe.

| Executed check | Result |
| --- | --- |
| Windows static Debug full suite | 95 passed; clipboard test skipped; 96 registered |
| Windows shared Debug full suite | 95 passed; clipboard test skipped; 96 registered |
| Windows static MSVC ASan: three UIA hosts plus portable semantics | 4 passed; no sanitizer finding |
| Windows shared MSVC ASan: three UIA hosts plus portable semantics | 4 passed; no sanitizer finding |
| Linux static Debug full suite | 101 passed, including X11/Wayland native Vulkan tests |
| Linux Clang ASan/UBSan/LeakSanitizer: semantics, widgets, controls, overlay | 4 passed; no sanitizer finding |
| Installed Windows static C consumer | Installed, configured, built and ran successfully |
| C17 SDK duplicate-definition counterexample | Expected LNK2005 failures reproduced for UIA_NamePropertyId and UIA_ScrollPatternNoScroll |

The full Windows suites above were executed on `188043a`. Its [six-platform-build matrix and Linux sanitizer job](https://github.com/Lulus792/NexaUI/actions/runs/38812796259) also succeeded. A subsequent native host-disablement counterexample and its final targeted regression results are recorded below; Linux and the portable core were unchanged by that follow-up.

The clipboard fixture declined to use the current clipboard; it was not forced to clear or replace the user's contents. Windows ASan does not establish general COM/OS allocation leak freedom. Local results do not establish macOS AX, Linux AT-SPI, physical GPU performance or physical monitor/DPI acceptance.

## Independent native client

`tests/uia_client.c` links Windows UIA/COM services and no NexaUI code. A separate host process owns the window, context and model. Its command/ack pipe advances application frames and reports their actual result. The client locates elements through the HWND, UIA fragment traversal and stable AutomationIds, queries native properties/patterns, and confirms model effects after the UI thread handles the requests.

The same client runs against a caller-owned STA (`nexaui_uia`), a caller-owned MTA (`nexaui_uia_mta`), and NexaUI-managed COM (`nexaui_uia_owned_com`). The fixture briefly displays/focuses its own window. The checks cover:

- UTF-8 to UTF-16 names, a surrogate-pair folder symbol and a 1600-character native title; source strings are changed after widget construction.
- Button/checkbox types, disabled-control and native-disabled-host invocation rejection, protected empty values and an offscreen control; physical client bounds track a 40-by-30-pixel window move.
- Real Invoke and Toggle patterns, ordinary model changes on the next frame, the final requested focus target and truthfully reported native focus.
- Simultaneous owner-thread publication/routing and client queries/actions; the host compares accepted requests with actual model clicks.
- A full 64-entry context queue and a full native queue, preserving every accepted press until dispatch. UIA's own focus requests share that native bound; repeated focus requests coalesce to the latest target.
- Allocation failure before first publication and during snapshot growth; the previous snapshot remains readable and a retry succeeds.
- Removed/recreated controls, role changes, context replacement, routing to the wrong context, context destruction, explicit detach, reattachment to the same HWND and window destruction while clients retain old handles.
- Native structure-change callbacks and native focus-change callbacks when the OS reports the test window focused. Event handlers run in the independent MTA client and query the sender through UIA.

The HWND can disappear while its COM apartment is still alive. The fixture continues pumping that owner thread until the client has finished; merely destroying its visible window is not a reason to stop servicing apartment calls.

## Native host disablement follow-up

An added independent-client case first calls Win32 `EnableWindow(FALSE)` on the owner thread. Against `188043a`, querying the native root's IsEnabled property still returned true, so the probe failed before attempting another activation. A semantic control's own disabled flag did not account for a disabled HWND.

The adapter now captures `IsWindowEnabled` with its native geometry/state and refreshes that copy on `WM_ENABLE`. Root and descendant queries report disabled state, new actions are rejected, and buffered native requests are discarded before forwarding to the core. The client queues a press before disablement, confirms both disabled properties and rejection of another Invoke, advances a frame, re-enables the HWND and confirms that no canceled click is replayed.

The follow-up passed all three UIA host variants and the native NexaCloud smoke in static and shared Debug builds (four targeted cases per configuration). The updated MTA client also passed MSVC ASan in both static and shared builds with no finding. The failing counterexample and final results are retained in local `out/uia-disabled-*.log` files. This narrows an actual native-state gap without adding a window-state dependency to portable contexts.

## C17 SDK linkage boundary

The SDK's `UIAutomationClient.h` defines UIA ID constants as external `const long` objects in C. `UIAutomationCore.h` also defines `UIA_ScrollPatternNoScroll`. Including the client header in both a static provider archive and its C consumer reproduced duplicate definitions under MSVC 19.38/SDK 10.0.26100.0. This was a linker failure, not a missing UIA library.

The provider now includes the SDK's provider interfaces directly, uses private integer enumerators for the stable UIA identifiers it needs, and privately namespaces the provider header's scroll constant. The test host deliberately includes the SDK's C client header alongside NexaUI and checks every private ID against that SDK. Both Debug and ASan static consumers link successfully; the shared variants also pass. This is scoped evidence for the checked SDK/compiler, not a promise that every future SDK emits the same declarations.

The local negative fixture is retained under `out/uia-sdk-linkage`; its two translation units both include `windows.h`, `objbase.h`, `oleauto.h` and `uiautomation.h`. One defines `int provider(void) { return (int)UIA_NamePropertyId; }` in a static library; the other calls it from `main`. The expected failing build names both duplicated SDK constants. Production builds and all registered tests pass.

## Reproduction

```powershell
cmake --build build --config Debug --parallel 4
ctest --test-dir build -C Debug --output-on-failure --parallel 4
cmake --build build-shared --config Debug --parallel 4
ctest --test-dir build-shared -C Debug --output-on-failure --parallel 4

cmake --build build-msvc-asan-static --config RelWithDebInfo --target nexaui_uia_tests nexaui_semantics_tests --parallel 4
ctest --test-dir build-msvc-asan-static -C RelWithDebInfo -R '^nexaui_(uia.*|semantics)$' --output-on-failure
# Repeat with build-msvc-asan-shared; put the configured compiler's ASan DLL
# directory on PATH, as in the Windows ASan workflow.
```

```sh
cmake --build out/vulkan-debug -j4
DISPLAY=:0 WAYLAND_DISPLAY=wayland-0 XDG_RUNTIME_DIR=/mnt/wslg/runtime-dir \
  NEXAUI_REQUIRE_VULKAN_TIMING=1 ctest --test-dir out/vulkan-debug --output-on-failure -j4
cmake --build out/vulkan-sanitize --target nexaui_semantics_tests nexaui_widget_tests nexaui_control_tests nexaui_overlay_tests -j4
ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 \
  ctest --test-dir out/vulkan-sanitize --output-on-failure -R '^nexaui_(semantics|widget|controls|overlay)$'
```

Local logs are retained as `out/uia-static-tests.log`, `out/uia-shared-tests.log`, `out/uia-build-msvc-asan-{static,shared}-tests.log`, `out/uia-linux-tests.log` and `out/uia-linux-asan-tests.log`. They are local development evidence, not bundled release artifacts. GitHub's regular CI matrix runs the newly registered Windows tests in both static and shared builds and keeps the existing macOS/Linux builds.

## Open acceptance

The [2026-10-16 selection follow-up](windows-uia-selection-report.md) adds explicit single-selection containers, SelectionItem and an automated native NexaCloud navigation/file workflow to the initial implementation recorded above. Editable Value/Text/TextRange, RangeValue, ExpandCollapse, Grid/Table and scrolling patterns remain unimplemented. So do richer property/text/live-region events, complete ancestor clipping and safe arbitrary DLL unloading while external clients retain providers. Real Narrator tasks, macOS AX, Linux AT-SPI and cross-platform screenreader scenarios remain open. W07/X05 have a partial implementation; X08 is not complete. See [the accessibility contract](../concepts/accessibility.md).
