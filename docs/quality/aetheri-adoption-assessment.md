# Aetheri adoption: initial source assessment

Assessment baseline: `05e28bb16c1e60ac339cd9428ff2d3afbafc21f6` (published main, checked 2026-11-04). This is an inventory, not acceptance of the catalogue. The requirements and acceptance criteria remain in the separately supplied `NEXT_AETHERI_UI.md`; this file records the first dependency stage. Later catalogue rows remain to be individually assessed.

The [preliminary full-catalogue source map](aetheri-requirement-map.md) lists all
102 requirement IDs, candidate source families and original acceptance targets.
It is a navigation and triage aid, not a substitute for individual row review or
evidence that the listed acceptance targets have passed.

| IDs | Existing source or original evidence | Disposition and remaining acceptance |
| --- | --- | --- |
| B01 | `CMakeLists.txt` Linux source/link block; `platform/linux/window.c`; `cmake/NexaUIConfig.cmake.in`; `renderers/vulkan/` | Missing implementation: X11 is mandatory in configure, dispatch, renderer and installed package. Tests also link X11. Separate X11 dependencies without dropping Wayland tests. |
| B02-B03 | `CMakeLists.txt`, `cmake/NexaUIConfig.cmake.in`, `include/nexaui/nexaui.h` | Missing profile-specific acceptance: prove static/shared and installed consumers with actual selected dependencies, including AT-SPI and IME. |
| B04-B05 | public `nexaui.h`, `docs/concepts/` | Missing acceptance evidence: inventory version/capability and ownership/error rules against each first-view operation. |
| W01-W02, W04, W10 | `nx_window_create`, `nx_window_poll`, `nx_window_get_info`, `nx_window_route_events`; `docs/concepts/window-management.md` | Existing API; missing composed Settings and selected-compositor evidence, fractional scale and idle/wakeup measurements. |
| R01-R03, R05, R10 | `docs/concepts/rendering.md`, `docs/concepts/renderer-backend.md`, `renderers/vulkan/`, `src/headless.c` | Existing source families; missing consumer-specific renderer/failure/lifetime acceptance. |
| T01-T04, T06 | `nx_text_buffer_*`, `nx_text_field_*`, `nx_window_set_ime_*`, `nx_text_clipboard_for_window`; `docs/concepts/text.md` | Existing API; real installed IME, clipboard and protected-field acceptance for the composed view remain open. Numeric complete native path is separately restricted (T05). |
| L01-L05, L08-L09, L11 | `nx_layout_*`, widget descriptors in `nexaui.h`, `docs/concepts/widgets.md`, `docs/concepts/layout.md` | Existing API families; responsive translated Settings composition and interactions still require a real consumer. |
| I01-I03, I05 | `nx_window_route_events`, `nx_interact`, text clipboard and widget APIs; `docs/concepts/input.md` | Existing paths; keyboard-only, native layout/IME and clipboard behavior in the composed view require acceptance. |
| A01-A02, A06-A09 | `nx_semantics_*`, `nx_window_publish_semantics`, `platform/linux/accessibility.c`, `docs/concepts/atspi.md` | Existing semantic/provider paths; independent native client and screenreader, actual composed controls, contrast and reduced-motion acceptance remain open. |
| D01-D02 | `nx_theme_*`, `docs/concepts/widgets.md` | Existing theme APIs; locale and variable-length translated view acceptance remain open. |
| P01, P04-P05, P08 | `nx_window_poll`, public allocators and lifecycle APIs; `docs/concepts/window-management.md` | Missing composed-view idle measurements and negative lifecycle/diagnostic acceptance. |
| Q01-Q03 | `docs/quality/wayland-private-compositor-report.md`, `tests/check_wayland_results.py` | Prior private Weston evidence is for the revisions named in that report, not this new profile or Settings view. Exact build, runtime library and non-skipped native checks remain required. |

## B01 dependency boundaries

`platform/linux/window.c` includes X11 headers and calls X11 functions throughout the shared dispatch, including geometry and accessibility. The Vulkan adapter and Vulkan/native test targets contain additional X11 references. The exported CMake package currently always calls `find_dependency(X11)`. Merely making `find_package(X11)` optional would leave compilation, linking, installed consumers and fallback semantics incorrect.

The private Weston report uses Weston's X11 backend as a test fixture to provide a virtual seat. This is a test-host dependency and must not be confused with an X11 dependency of the compiled NexaUI profile. A no-X11 SDK build/consumer still needs a separately provisioned real Wayland compositor and cannot claim native IME/AT-SPI acceptance from configure alone.

## First consumer boundary

`examples/04_forms` exercises model and semantic assertions but is not a complete native Wayland Settings application. `examples/10_native_clear/main_linux.c` exercises a native renderer lifecycle but is not a forms/IME/accessibility consumer. Combine existing public APIs in a new consumer; do not treat either existing example as proof of the requested complete view. Aetheri retains its current SDL3 foundation and its own application models while this work proceeds.

## Initial B01 negative control (2026-11-04)

On Alpine-Dev/WSL2, GCC 15.2.0, CMake and Ninja, `CMAKE_DISABLE_FIND_PACKAGE_X11=ON` with Vulkan, tests and examples disabled failed configuration at the mandatory `find_package(X11 REQUIRED)` in `CMakeLists.txt:29` (exit 1). Wayland client 1.25.0, xkbcommon 1.13.1 and wayland-protocols 1.48 were found. `tests/check_wayland_only_package.py` independently reaches the same failure using the proposed `NEXAUI_ENABLE_X11=OFF`; the option is not implemented yet. It is a deliberately failing pre-implementation probe, not a registered default test. Later it checks both static and shared installed consumers, but does not substitute for a build machine without X11 headers, Vulkan-on, native tests, or an actual compositor.

## Vulkan isolation in progress (2026-11-04)

Working tree based on `05e28bb16c1e60ac339cd9428ff2d3afbafc21f6`: `renderers/vulkan/renderer.c` SHA-256 `a4719f64313fff6aa9918ef17dbb43cf2d3ccbe7593bf08388c08f3dd1b2fb9f`. The internal `NEXAUI_VULKAN_WAYLAND_ONLY` compile switch excludes Xlib declarations, surface creation and the X11-only canvas-cache query. It rejects a non-Wayland window before selecting the surface extension. The switch is not yet wired to a public CMake profile.

Alpine-Dev/WSL2, GCC 15.2.0, Release, Vulkan 1.4.347: the existing dual-backend configuration and a configuration with `CMAKE_C_FLAGS=-DNEXAUI_VULKAN_WAYLAND_ONLY` both built `nexaui_renderer_vulkan` (exit 0), and each ran `nexaui_vulkan_device` 1/1. This device test does not cover the modified window-surface path. A separate GCC `-H -fsyntax-only` include trace for the switched renderer contained no X11 header; `nm -u` showed no `nx_window_x11_*` reference in its object, while the default object retained both references. Both whole builds still configured and built the X11 core, so they are not B01 acceptance. Native Wayland presentation and default X11 runtime regression remain open.

## B01 candidate progress (2026-11-04)

Working tree on `05e28bb16c1e60ac339cd9428ff2d3afbafc21f6`, uncommitted. `NEXAUI_ENABLE_X11=OFF` now selects a Wayland-only Linux dispatcher and Vulkan adapter, omits X11 from the installed CMake dependency graph, retains the Wayland native tests, and returns `NX_ERROR_UNSUPPORTED` for an explicit X11 selection. The default is still dual-backend. The package probe passed static and shared Release installed C17 consumers with CMake X11 discovery forbidden (Vulkan/tests/examples off for that probe). This machine has X11 development files, so it is not yet an independent no-X11-filesystem acceptance.

The separate Release Wayland-only build with Vulkan, tests and examples enabled configured and built successfully, including Wayland v1/v3 protocol, Vulkan and accessibility test targets. Its first full build failed because `test_vulkan_draw.c` retained X11-only symbols; that link failure was corrected by isolating its X11 displayed-pixel check, which the previous Wayland run already bypassed, while retaining the Wayland GPU/readback tests. The subsequent full build succeeded. Seven focused tests passed: backend selection, cursor, repeat, wire management, text-input v1/v3 and Vulkan device. They do not establish native compositor presentation or installed IME acceptance. The default dual-backend build passed the two text-input wire tests and the private Xvfb X11 window test (1/1). Runtime X11 Vulkan regression, real Wayland/Vulkan/AT-SPI clients, no-X11-header environment, sanitizer/shared native matrix and independent review are still open. X11-only AT-SPI fixtures are not registered in this profile; Wayland multiselection remains registered, but needs actual execution. No native-test skip counts as success.

The package run preceded later changes limited to test/example CMake and test sources; repeat against the frozen final candidate when the profile is complete. The first composed Settings consumer, the per-ID catalogue assessment beyond the first stage and the remaining delivery stages have not yet been completed.

## Isolated dependencies and private native run (2026-11-04)

The candidate was built on Alpine-Dev/WSL2 with a bubblewrap mount namespace hiding `/usr/include/X11` and masking `libX11.so.6.4.0` and `libX11-xcb.so.1.0.0`. A normal C compiler control built and ran; a separate Xlib header compile and `-lX11` link failed for the expected missing header/library. The static/shared installed C17 package probe passed in this namespace. SDK, Vulkan renderer, native examples and seven selected protocol/device test targets also built. Seven focused CTest cases passed with the same X11 masking after the test namespace made `/tmp` writable. An initial run with read-only `/tmp` failed three `mkdtemp` calls; its original output is retained. A later standalone invocation confirmed both text-input cases with process exit 0. This is a selective filesystem isolation, not a complete independent distribution image.

The full test build in the namespace failed when linking the independent AT-SPI clients: the installed Alpine `libatspi` itself depends on X11. These are test-client dependencies, not NexaUI SDK linkage. No AT-SPI test is counted as passing in the masked environment; the failure is preserved in `out/aetheri-b01-isolation-v1/build.log`. A first setup attempt failed before compilation because `/dev/null` was read-only; its header control was invalid and is not counted. The corrected attempt and all outputs above are separate. The seven-case retry's CTest output says 7/7 passed, but its outer shell returned 1 due to a trailing carriage return from the Windows-to-WSL script pipe; that wrapper error is not hidden.

Separately, the normal Wayland-only Release build ran against private Weston 14.0.2 with its X11 backend on private Xvfb and software rendering. Nine distinct cases executed without skips: four Vulkan presentation/draw/timing/ring cases, native window and window management, independent AT-SPI multiselection, NexaCloud native example and native Vulkan example. CTest returned 0; the fixture recorded child exit 0 and Weston exit 0. JUnit SHA-256 `18648bf87dd50c825022f58f784e2ae6a7b1ec0a9eccd4b8113062e27fe7147e`; fixture result SHA-256 `905e50a30cefa7b87660e2d62c374575040759bf06a7fafc05b29a1cf3250f80`. Evidence is in `out/aetheri-b01-native-v1`; the original JUnit filename contains a carriage return from the shell pipe and the byte-identical `junit-verified.xml` is provided for ordinary access. This is not physical, human, real Aetheri compositor or installed input-method acceptance. The test host intentionally retained X11 for Weston and its libatspi client, even though the SDK was built Wayland-only.

Open: shared/sanitized native runs, default X11 Vulkan regression, review and a final source-frozen complete gate. The Settings application and later catalogue stages remain open.

The default dual-backend Vulkan X11 tests were also run on private Xvfb with software rendering: presentation and draw passed; timing and ring were initially not run because those two executables had not been built. After building only those targets, each passed individually. The initial four-case CTest invocation therefore exited 1 and is not represented as a single 4/4 run; four actual passes were observed across the original and targeted invocations. No physical GPU acceptance follows.

## Settings consumer started (2026-11-04)

`examples/18_settings` adds an application-owned name/reduced-motion draft and
applied model using existing NexaUI scene, text, keyboard and semantic APIs. The
Linux Vulkan runner routes native events and IME/clipboard through the window,
publishes semantics and closes the renderer before scene/window teardown. It is
currently a build-tree example because the Vulkan adapter is private. The
headless `nexaui_settings_scene` test passed on Alpine/WSL2 in the Wayland-only
Release build, checking synthetic Tab/Shift-Tab, text edit, checkbox, Apply,
model/status and invalid-name disabled state. The native Weston/Xvfb software
smoke passed 1/1 with process exit 0 and actual ready frames; after a status-label
correction it was rebuilt and passed again in the separate v2 fixture. These are
not evidence of actual native IME insertion or independent accessibility
operation. The README lists missing resizing, visual, IME and consumer work.

## Settings: independent AT-SPI action (2026-11-04)

An independent libatspi client without NexaUI linkage starts the actual Settings
runner in `--verify-accessibility` mode on private Weston/Xvfb and a private
registry/bus, finds the application by PID and the Display name, Reduce motion
and Apply controls, verifies the checkbox role and unchecked/checked transition,
then invokes Apply. The host checks its applied model contains `Aetheri` and
reduced motion enabled, and exits through renderer/scene/window cleanup. Test
`nexaui_settings_wayland_atspi` passed 1/1, no skip; fixture child and Weston
exit 0. Raw CTest output SHA-256
`657791f166cb84059f41c9274df4fb3c827bad53b8741f1873febdbeafb32ab` and
fixture result SHA-256
`83c714aa079a222a33086bfb065cb26680be1febbd773828a64575a088a6ad3f`
 Original logs are
in `out/aetheri-settings-atspi-v1`. Source baseline remains `05e28bb` with local
changes; scene SHA-256 `43a2a14ca2aca4bec741cd00e2c2d8e15ed5e9e1755c41566526915a50004672`,
native runner `420ef0319a2550b98ca7e83c36b7cd89ce3d46bbaf64dcd37ba2c9beae03a73a`
independent client
`38f042db1ad54ec8a04c164491999d191e9b397502761c18d015ebf713695264`.
This proves these two accessible actions and model acknowledgement, not
accessible text editing, installed IME, a screenreader or human acceptance.

A follow-up tightened the test client's child cleanup: once `waitpid` has reaped
the host, it clears the PID before any failing assertion, avoiding a signal to a
potentially reused PID. The client was rebuilt and the same private native test
passed again (1/1, no skip, exit 0), with scene and host sources unchanged. V2
client SHA-256 `393793c7433f8861d55d70abc85e20fbd29a2b672302df22aeb2682b06756f2f`;
raw CTest log `49d92822c4f8b1851f7c4b18c8dfc3b84e1032dfaff359fb789b02e7110bd763`;
fixture result `bb6bc4533d077304d48755e09985afbed2f712bc60f6988ccc3aa2de0630fc48`.
The v1 result and sources are historical; v2 evidence is in
`out/aetheri-settings-atspi-v2`.

## Settings: Wayland text-input-v3 protocol (2026-11-04)

The existing private Wayland v3 server was extracted to a reusable fixture;
its default 240x120 behavior remains covered by the original unchanged test
body. The new Settings test requests 640x360, focuses the actual display-name
widget and receives native v3 preedit `naï` and commit `café` from that server.
It checks the draft stays unchanged before `done`, the commit appears exactly
once afterwards, the applied model remains `Aetheri`, and preedit clears when
focus moves to the checkbox. The first execution failed at the fixture's fixed
240x120 geometry assertion, before the IME sequence; the server was explicitly
parameterized and the old default preserved. This was a fixture mismatch, not
an asserted SDK defect.

On Alpine-Dev/WSL2, both the original v3 regression and Settings v3 case passed
2/2 in the Release Wayland-only build and 2/2 in Clang 22.1.3 Debug with
ASan/UBSan and leak detection; the separate Settings scene test also passed
under the sanitizers. JUnit files are in `out/aetheri-settings-ime-v1`:
Release SHA-256 `52918fef475479093f553a6677af352b05bb373727e4d7e4a65479549a02f3a6`;
sanitizer SHA-256 `c92de33f62768b1897bdeb4a177ed602dd163f5f75de0f45637b227089b5f8be`.
Settings test source SHA-256 `a16a681437a6bce519c45bad4276ee55df1e27a093692c73af0a929180f38a39`;
shared fixture `36f811219597adcba0f68f5ef556c57b4a0818ea2d5eec594dbaa445a44e4ba5`.
These are real Wayland protocol events against a controlled test server, not an
installed input-method engine, a complete user typing workflow or actual Aetheri
compositor acceptance. Build source remains local changes on `05e28bb`.

## Settings: small view and text capability (2026-11-04)

The settings options now use the existing `nx_scroll_area` and preserve their
IDs while moving focused controls into view. At 240x150 logical pixels a
headless synthetic test exercises Tab/Shift-Tab and verifies the resulting
checkbox/button semantic bounds lie inside the 64-pixel-high viewport after a
following frame. The scene and v3 IME tests passed in Release and in Clang
ASan/UBSan with leak detection. These checks are not a native resize,
long-label, 200% text-scale, visual or human keyboard acceptance. Below 128x120
the native runner still polls without producing a new frame.

On private Weston/Xvfb and private AT-SPI bus, the independent client again
passed the checkbox/Apply/model/clean-exit test. It now additionally reads
`Aetheri` via the provider's `nexaui-value` attribute and verifies Text and
EditableText interfaces are absent. This is positive evidence of a missing
standard text contract, not text-editing acceptance. Raw CTest log SHA-256
`1282c5b0f25536fdeee84355272506fa37310d5029f9350b11960b66c057a522` is in
`out/aetheri-settings-atspi-scroll-v2`. Scene source SHA-256
`5858b9c55bf7fb96f10e2e691e82a3a4c49c8345b50c46a88fdce2987ef2e9bc`;
scene test `496abff1f74974c9be5c2239814890eea5fe922870f740b96c80b16c5ac0c73b`;
AT-SPI client `dd35a42fec6b608c30f5f2c6d921cd7c5c26eaa6f5a86d5a60390f88af030979`.
These working sources remain uncommitted on `05e28bb`.

## Shared Wayland-only native run (2026-11-04)

On Alpine-Dev/WSL2, GCC 15.2.0 Release with `BUILD_SHARED_LIBS=ON`,
`NEXAUI_ENABLE_X11=OFF`, Vulkan, tests and examples enabled, the complete build
succeeded. A private Weston 14.0.2/Xvfb software run selected by `-R wayland`
executed 17 distinct cases, with no skips or failures, including Vulkan, both
Wayland text-input wire versions, AT-SPI multi-selection, Settings native and
its independent AT-SPI client. Two further shared tests, the Settings scene
and its private v3 IME fixture, passed separately. This is not the full SDK test
suite or physical/Aetheri compositor acceptance.

The JUnit evidence is `out/aetheri-b01-shared-v1/results.xml` (SHA-256
`224ba4b0e38089fb6a34f64d17115a59edda2e2c0fd4cfd59a5dcddee985de71`).
A separate actual Settings smoke invocation recorded `/proc/<pid>/maps` while
running, showing the executable mapping from
`/tmp/nexaui-b01-shared-v1/libnexaui.so.0.1.0` (SHA-256
`2ed329fc8e0d2afbc2d6457a8b3467286927d4c814724cf0da7de5a298d99edd`), and
then exited normally; retained `loaded-maps.txt` and fixture metadata are in
the same directory. The Settings executable hash was
`4db609f05bb11bdf83ad1fb286e2dee3d325b88e4ba4f69f6b450537a8fbf915`.
Sources are uncommitted changes on `05e28bb`. The shared run used a normal test
host with X11 libraries for the compositor and libatspi, not the X11-masked
build environment. Independent review, full SDK matrix and native sanitized
Wayland/Vulkan coverage remain open.

## Native Wayland-only ASan/UBSan run (2026-11-04)

Alpine-Dev/WSL2, Clang 22.1.3 Debug, static SDK,
`NEXAUI_ENABLE_X11=OFF`, Vulkan, tests and examples enabled, with
`-fsanitize=address,undefined`, `ASAN_OPTIONS=detect_leaks=1` and
`UBSAN_OPTIONS=halt_on_error=1`: the complete build succeeded. The private
Weston 14.0.2/Xvfb software fixture ran the 17 `wayland`-named tests; all 17
unique JUnit cases passed without skips, including Vulkan present/draw/timing/
ring, v1/v3 input, AT-SPI multiselection and Settings native/AT-SPI. The
fixture and child exited 0. The complete CTest log contains no ASan/LSan,
UBSan or Vulkan validation error marker; validation-layer activation was not
established by this run. It is not the full SDK test suite,
physical input/GPU, actual Aetheri compositor, or proof of installed-IME and
standard AT-SPI text support. The separate instrumented Settings v3/scene
checks are recorded above.

Evidence: `out/aetheri-b01-asan-native-v1/results.xml` SHA-256
`a19677172e4c5669ee578dbb16734c42be2dcb0df1de362960fee3578aab4f18`;
`ctest.log` SHA-256
`12078cd2aab37b740dd54c3527f3c9c424d27b4db2c36d619fc3cbcae820647e`;
fixture `result.json` SHA-256
`a6e66a1fa0459061d08cdb7cd31214767911e67336f7c7a7d4c9639d0256c1c9`.
Source files matched all 24 entries of
`out/aetheri-shared-source-v1/manifest.json` (manifest SHA-256
`ed61c3c8f866fb9314212cbd2d6682fd3c6a482789b899dd36e70c8e85d69bbf`)
before the run; this is a local dirty candidate based on `05e28bb`, not a
published revision. Full SDK matrix, candidate review, no-X11 test-client
dependency isolation and final delivery gate remain open.

## Full locally registered shared suite (2026-11-04)

The Wayland-only GCC 15.2.0 Release shared build ran its complete locally
registered CTest set inside private Weston 14.0.2/Xvfb with Vulkan required.
All 120 registered names appeared exactly once in JUnit; 120 passed, no skips
or failures. The fixture, child and CTest exited 0. This is one local Linux
profile on a virtual software-backed test host, not the complete cross-platform
CI, physical hardware, actual Aetheri compositor or human accessibility
acceptance. The available test-client `libatspi` and Weston themselves retained
X11 dependencies; the SDK under test did not select its X11 adapter.

Evidence under `out/aetheri-b01-shared-full-v1`: JUnit SHA-256
`34cc07705c06129fb759fd892bb6e1285011880054a02ecbcde109c005a51a97`,
registered CTest JSON `41e8c8c53ff32213e414b12fa28c7f215644d33719255367a0510f03b92361da`,
and raw CTest log `581867398539f13754dcdbfc5cbee06a4c0c432cce09d4611236dffbb680e476`.
The 24 source entries in `out/aetheri-shared-source-v1/manifest.json` matched
the current files before the run; baseline commit `05e28bb`, uncommitted local
changes. Previously recorded separate loaded-library evidence remains applicable
to the same build. Independent candidate review and remaining adoption gates
are not implied by this suite.

## Full locally registered sanitizer suite (2026-11-04)

The Clang 22.1.3 Debug static Wayland-only build ran its complete locally
registered set in private Weston/Xvfb with Vulkan, ASan, UBSan and leak
checking enabled. All 121 registered names appeared exactly once in JUnit;
121 passed, no skips or failures, and the fixture/child exited 0. The raw log
contained no AddressSanitizer, LeakSanitizer or UBSan error marker. The one
case registered here but absent from the 120-case shared suite was
`nexaui_selection_set`; this difference is not a missing shared test result.
Neither run represents the whole cross-platform CI or physical, real Aetheri,
installed-IME and human assistive acceptance.

Evidence under `out/aetheri-b01-asan-full-v1`: JUnit SHA-256
`89dae8b016579ec16792a3dc62561c30a0ea4c1329eed2530bfb8d4b78bf9055`,
registered JSON `de31dc0e1c9750adc8528d0d42c4babee8303e410642029ad7b52c46e77e8007`,
and raw CTest log `1ca1598ac190f5319e688f6bc24b4b07b8cca06d27dc7e53ae6cf0eeb16f6a68`.
Source matches the 24-entry candidate manifest noted above, on uncommitted
`05e28bb`-based work. A full independent review and the other platform/build
and consumer gates remain outstanding.

## Settings idle scheduling and native keyboard finding (2026-11-04)

The Linux Settings runner now redraws on routed events or size/scale changes,
keeps retrying a suspended renderer, and polls at up to one second when idle.
If AT-SPI is unavailable it republishes at most once per second to permit the
existing connection retry without redrawing. This does not implement worker
wakeup, a general animation deadline contract, or an idle guarantee for a
permanently suspended renderer. It is an example-level change, not a new public
SDK event-loop API.

Three sequential 3-second private Weston/Xvfb software runs of each version
used the same font and workload. The retained instrumented baseline produced
67/78/74 actual presentations, 68/79/75 poll calls and 4.805/5.304/5.169 s of
process CPU (includes software-renderer worker threads). The measured
candidate produced 1/2/1 presentations, 5/6/5 polls and 0.396/0.201/0.128 s CPU.
These small unpaired samples demonstrate this fixture's behavior only, not a
hardware-wide performance claim. Baseline and measured candidate source and
binary copies, three raw samples and fixture results are retained under
`out/aetheri-settings-idle-v1`. Subsequent current-source changes add only test
modes and their diagnostics; the exact measured candidate is preserved.

An independent AT-SPI client successfully changed and applied the model after
1.2 s of idle. A separate xdotool/Xvfb-to-Weston virtual-keyboard test then
passed Tab traversal, checkbox, Apply, model confirmation and clean close; the
same keyboard-controls, native smoke and AT-SPI tests passed 3/3 under
ASan/UBSan with leak checking. These are not physical-keyboard or human
screenreader acceptance.

The stronger registered `nexaui_settings_wayland_keyboard` text test remains
**failing** on this private Weston v1 setup: it times out waiting for the model
to contain the keyboard-entered `Ada`. Wayland debug records keyboard focus and
all key events, while the settings diagnostic records an unchanged 7-byte draft;
the checkbox and Apply action do commit. The protocol trace shows text-input-v1
entered and language/direction replies but no commit-string for the typed keys.
The adapter currently suppresses ordinary xkb text while IME is entered; the
trace alone does not establish whether the compositor/input-method configuration
or SDK policy is the ultimate cause. A diagnostic run forcing the pre-existing
continuous redraw behavior also failed with the same unchanged draft, so the
new idle strategy is not necessary for reproducing the observed failure. Earlier
fixture attempts failed to locate an untitled Weston X11 window; those did not
send input. The test now uses the private compositor's recorded window ID.

Original logs include `keyboard-diagnostic-v1.log` SHA-256
`86ddb08415ad83a67ab4c3c5822bdc92b9d43065c1a20de4a57c5a3c989a8790`,
`keyboard-continuous-v1.log`
`691fc6462c6daac101aae916fd48b9a5051e541dc569ef6974b20a56bb2f2bcb`,
`keyboard-text-final.log`
`fd50b94bf90fc2cf5189b128bca7d0f332b5fa42b2c02acafae37768d8c1452a`
and `keyboard-controls-v1.log`
`656c3894fe8d7686503c5c0d124284e0bd6d168f4774381a062851d0d523cba1`, and
`asan-controls-v1.log`
`9aec8131f94d3e12649a00170ccea8279eba2111f451fc5bd443c4ba2bf46de4`.
The 120/121 full-suite results above are historical for the previous source;
they do not include this new failing regression or certify the current tree.
Current work remains uncommitted on `05e28bb`.

## Forwarded hardware input and installed input method (2026-11-04)

The earlier default-Weston keyboard-text failure above is now fixed: entering
text-input does not imply that the input method grabbed the hardware keyboard.
The adapter emits ordinary xkb text for delivered `wl_keyboard` keypresses,
while independently handling input-method commits. A regression verifies the
delivered key/text pair and a separate composed commit. The native Settings
keyboard text and controls tests pass with the private Weston screen keyboard;
the v1 and v3 protocol fixtures also pass. This is not evidence for all IMEs.
The input-method-v1 protocol describes the input method's optional keyboard
grab and forwarding of keys it does not process:
https://raw.githubusercontent.com/wayland-mirror/wayland-protocols/1.48/unstable/input-method/input-method-unstable-v1.xml

An additional private Weston 14.0.2 run launched `weston-simple-im` and failed
both Settings keyboard checks. Its recorded protocol trace shows the initial
Tab as `wl_keyboard`, but while the field is active the input method sends
`commit_string("a")` for Ctrl+A and `commit_string` containing Tab and space
for later navigation. The example consequently cannot perform the expected
selection and focus traversal in that setup. This observation is specific to
the test input method and configuration; it does not establish behavior of
production IMEs or the Aetheri compositor. No events were synthesized to hide
the failure. The fixture supports selecting this input method explicitly; its
default remains the Weston screen keyboard. Raw results and trace are in
`out/aetheri-ime-forwarded-v1-simple-im.xml` and
`out/aetheri-ime-forwarded-v1-simple-im-trace.xml`, with compositor logs in
the corresponding `out/` directories. Real IME, physical input and human
assistive-technology acceptance remain open.

On the revised candidate, the complete locally registered shared Wayland-only
suite passed 122/122 in the default private Weston/Xvfb fixture, including the
new native keyboard cases (fixture and child exit 0). This supersedes the
historical 120-case shared result above for this candidate, but does not include
the separately failing `weston-simple-im` variant or the remaining acceptance
work. Evidence: `out/aetheri-ime-forwarded-v1-shared-full.log`, its adjacent
JUnit XML and the fixture's `result.json`.

The corresponding Clang Debug static Wayland-only run with ASan, UBSan and leak
checking passed 123/123, with child and compositor exit 0. The additional case
relative to shared is `nexaui_selection_set`. Its log contains no sanitizer or
UBSan error marker. Evidence: `out/aetheri-ime-forwarded-v1-asan-full.log`, its
adjacent JUnit XML and the fixture's `result.json`. Both full suites used
virtual input and the default Weston input method; the separately documented
`weston-simple-im` failure is not part of their pass count.

## Current Linux consumer profiles (2026-11-04)

On the same uncommitted `05e28bb`-based candidate, an external project copied
the existing public C17 package consumer into a temporary directory, used
`add_subdirectory` and linked only `NexaUI::NexaUI`. All four combinations of
static/shared core and Vulkan off/on configured, built and ran with
`NEXAUI_ENABLE_X11=OFF` and CMake X11 discovery forbidden. The two shared runs
also used `ldd` with `LD_LIBRARY_PATH` cleared and resolved the library under
their own build directories. The Vulkan-enabled builds built the private Vulkan
target; this headless consumer does not link it or test presentation. The
reusable probe is `tests/check_wayland_only_add_subdirectory.py`, SHA-256
`ef84a0204b2d14271ca814198398e0647db44d01f86cc525773abcf52afa31fd`;
`out/aetheri-b03-add-subdirectory-v2.log` SHA-256
`2fe896cd6afbb752b3907ea79fef54e5dff935f87fab34735a9ce6f25bd08815`.

The separate existing installed-package probe passed static and shared C17
consumers again with Vulkan off. It also passed in a bubblewrap namespace
hiding the X11 headers and masking the installed X11/X11-xcb shared library
files: a normal C executable compiled and ran, while dedicated X11 header and
link controls failed as expected. The namespace is selective isolation on this
host, not a separate minimal distribution. Its probe log is
`out/aetheri-b03-isolated-current-v1.log`, SHA-256
`0fe05e46645adae223a37401d44ffa06662751f3c680020aa302449a017e49ac`;
the ordinary-host log is `out/aetheri-b03-installed-current-v1.log`, SHA-256
`d010c671c326fc68f886d1c7ddd07db00408a83440f295215c6c89c2f901dcc8`.
All three processes exited 0. These builds used Alpine-Dev/WSL2, GCC 15.2.0,
CMake 4.2.3 and Python 3.14.8; Vulkan-enabled builds found 1.4.347. They do not
prove native compositor, installed IME, AT-SPI, independent-distribution,
default-X11 or other-platform acceptance, and do not close B03/Q05 in full.

## Settings focus visibility after resize (2026-11-04)

The scene previously revealed the focused control only when its focus ID
changed. A deterministic scene regression focused Apply at 640x360, rendered
another frame at that size, and then resized to 240x150 without input. The
original code failed the focused semantic-bounds check in
`out/aetheri-settings-resize-before-v1.log` (exit 8; SHA-256
`ba857a0e7b6001936e42b078be7368878e6f065fcd35e0f4c75ac120dc0610d5`).
The scene now reevaluates the same focus when viewport height changes. The
native host also schedules a bounded additional frame after a focus change so
that the scene can reveal the new target without waiting for another input.

The new private-Weston keyboard case requires the actual client dimensions to
be 240x150, holds each Tab key down until the focused control's published
semantic bounds fit the viewport, then completes text edit, checkbox and Apply.
All seven Settings scene, controlled v3, virtual keyboard, native and AT-SPI
tests passed without skips under both the Wayland-only GCC build and the Clang
ASan/UBSan build with leak checking. The raw logs are
`out/aetheri-settings-resize-final-v2.log` (SHA-256
`29514baa0c24e00c511bacf41ce9373b573f472aa593c63c306a2e944d294167`) and
`out/aetheri-settings-resize-asan-v2.log` (SHA-256
`708175772c3fb5557ba69d6eab4326f390d79777c543d3f464919ff20d571190`).
The final focused reruns record visible-focus IDs 3, 4 and 5 in
`out/aetheri-settings-visible-final-v1.log` and
`out/aetheri-settings-visible-asan-v1.log`; both fixtures and child processes
exited 0. These check published geometry and virtual input, not rendered pixels,
physical keyboard or human accessibility acceptance.

The first native test required a frame with zero routed events; that assumption
failed because the fixture delivered additional events. Its failure is retained
in `out/aetheri-settings-small-v1.log`. The replacement waits for observed
visible geometry before proceeding. A further negative control removing only
the host's focus-change follow-up also passed in this fixture
(`out/aetheri-settings-small-no-followup-v1.log`): native events can supply
another frame, so this test does not prove the follow-up independently. The
headless resize regression above does reject the original scene behavior.
The full 122/123-test suite results earlier in this document precede this
change; only the focused checks above were rerun on it.

## Idle accessibility recovery and waiting (2026-11-04)

Candidate based on `cebf6ce2201da564a75f1555149d7afc44bf037a`. A disconnected
AT-SPI bridge previously reconnected only when the application published a new
semantic frame. An independent libatspi client now pauses its host's frame
production, disconnects the transport, and requires rediscovery of the same
snapshot under a fresh bus identity before resuming. The base failed that
rediscovery. The candidate retries during window polling and reduces the
owner's two-second poll wait to the existing one-second retry deadline; removing
that timeout reduction made the bounded recovery test fail. The independent
client also verifies that the old reference is defunct and the fresh target
works after the host resumes.

The Settings host now requests a long event-driven wait when idle and AT-SPI is
available. A three-second private-Weston test observes at least one actual
one-second-or-longer wait; the previous fixed one-second poll failed this check
with 6 polls, one presentation and 0.137 s process CPU in that run. The
candidate produced 4 polls, one presentation and 0.163 s CPU in the GCC run,
and 4 polls, one presentation and 0.208 s CPU under ASan/UBSan. These single
samples demonstrate this fixture only. A disconnected or unavailable provider
retains bounded retries; synchronous AT-SPI address discovery can still take
up to 250 ms per attempt. General worker wakeup, animation scheduling,
minimized-window measurements and actual screenreader acceptance remain open.

The complete private-Weston suites on this candidate passed 172/173 with GCC
and 173/174 with ASan/UBSan. Both failures were the previously documented
intermittent `nexaui_settings_wayland_atspi_clipboard_once`; a GCC isolated
rerun also failed. A comparison restoring only the former one-second Settings
poll interval failed that same case in three more attempts. This comparison
does not prove the ultimate cause or turn the failures into passes. The native
X11 AT-SPI recovery and X11 window tests passed on a separate dual-backend
build. The one-shot failure remains open.

## Text-input-v1 keysym modifiers (2026-11-05)

Candidate based on `a74f206b855e0587d291110a74d243f98789e0f6`: the v1
adapter ignored both `modifiers_map` and the keysym's modifier mask, instead
using the unrelated physical-keyboard state. A controlled server sent
Control+A using a reordered map while the physical state was unmodified; the
new test failed against the previous source and passed after translating the
protocol map. It checks Shift, Control, Mod1/Alt, Mod4/Super, changed maps,
bit 31, unmapped bits, stale serial, invalid key state, malformed map and
recovery. Events referring to unmapped bits and all keysyms after a malformed
map are discarded; unknown names do not map to NexaUI modifiers. The
[text-input-v1 protocol](https://gitlab.freedesktop.org/wayland/wayland-protocols/-/blob/1.48/unstable/text-input/text-input-unstable-v1.xml)
defines map positions as keysym-mask indices and separates key events from
ordinary committed text. The fixture also routes Control+A into a focused
text field and verifies selection of the whole draft. Its first attempt sent
the key in reply to an older queued state commit and was correctly rejected as
stale; the final fixture first completes those pending requests.

Wayland v1/v3 controlled tests and three Settings v3 scene tests passed in the
GCC shared build and Clang ASan/UBSan static build. Four Settings keyboard
cases passed on private Weston with its default input method. An intermediate
candidate before the final unmapped-bit guard was also run with
`weston-simple-im` and failed all four: two timed out waiting for applied
state, one did not reach the next visible focus, and one did not reach the
expected clipboard draft. Its output is retained locally in
`/tmp/nexaui-ime-modifiers-simple.log`, SHA-256
`095122871a7207831a87fafb3c6a3ad1d585644f5f3276ce9027ac5baa2c5462`.
The earlier trace above shows that this input method commits control keys as
text in that setup; this keysym correction does not solve that separate
behavior. Real installed-IME and human acceptance remain open.
