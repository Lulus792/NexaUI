# Native Wayland checks on a private compositor

Fixture source: `670181b07a80faa39b77a20e06aa11811a3495f3`, 2026-10-20.
Compatibility correction: `d6945798a43006b2724ffff12f78101b2e4917d9`.
Diagnostic follow-up: `d06fc382732e10d586db71f417e3a9f79867b748`.
The new test infrastructure exposed an empty keyboard-array defect on older
Wayland SDKs; the follow-up guards that valid empty input in the C17 callback.
Public APIs, rendering policy and the separate NexaCloud application/pin are
unchanged.
The [earlier X11 response report](vulkan-canvas-response-report.md) retains the
WSLg presentation timeouts that prompted this independent check.

## Execution boundary

`tests/run_weston_native.py` starts Weston on an existing private Xvfb display.
Its X11 backend supplies a virtual input seat, while NexaUI's Wayland tests use
actual Wayland surfaces, buffers, configuration and presentation. The fixture
does not substitute the X11 NexaUI backend for a Wayland case. The existing test
names, backend assertions and complete GPU/headless pixel oracles are retained.

Runtime, socket, configuration and XDG directories are private. Existing session
and accessibility bus addresses and inherited Wayland sockets are removed.
X11 remains available for X11 tests in the same CTest invocation. Each owned
compositor/test process group is terminated and waited after normal completion,
observed child failure or the fixture's internal timeout, including descendants
after a failed child exits. External cancellation by SIGTERM is not covered by
the fixture guards. Test failure, compositor failure and
timeout cannot be reported as success. No compositor is downloaded by the
fixture and no existing desktop configuration is changed.

The CI installs Weston only as a test dependency on its ephemeral Linux runners.
Both Linux linkage jobs and Linux sanitizers run the full CTest suite under the
fixture, with Vulkan and the Wayland path required, Khronos validation and
synchronization validation. `tests/check_wayland_results.py` then requires one
actually successful JUnit result for each of the seven native Wayland cases;
missing, duplicate, skipped and failed results are rejected. Compositor logs,
exit metadata, CTest JUnit and failure logs are retained as CI artifacts.

## Actual local checks

Local platform: Alpine-Dev/WSL2, Alpine 3.24.2, Clang 22.1.3 ASan/UBSan with
leak detection enabled, Mesa Lavapipe and native Alpine Khronos validation
1.4.343. Weston 14.0.2 runs with its Pixman renderer on private Xvfb
1600×1200×24. No physical input/display or human screenreader acceptance is
implied by these software-backed checks.

| Run | Actual result |
| --- | --- |
| Initial independent Weston headless clear/present | Passed |
| Weston headless seven-case group | 6/7 passed; window case failed because there was no keyboard/pointer seat |
| Weston X11-backend seven-case group, virtual seat | 7/7 passed, 86.96 s, no skips |
| Fixture source, full ASan/UBSan/LSan + sync-validation suite on local SDK1.25 | 116/116 passed, 218.76 s, no skips |
| Corrected callback, local native Wayland window test | 1/1 passed, 0.44 s |
| Fixture lifecycle/isolation cases | Success, child exit 13, startup failure and timeout all correctly handled; owned roots/descendant stopped |
| Fixture deliberately ignoring child exit | Rejected at `expected exit 13, got 0` |
| Result gate on the actual seven-case success | Passed |
| Result gate on actual headless failure | Rejected |
| Synthetic skipped or duplicate result derived from passing XML | Both rejected |

The headless failure's original readiness flags were `hidden=0 visible=1`;
the existing assertion required the keyboard and cursor bits too. Failed CHECK
exit skipped teardown and produced a 128-byte leak report, retained with the
failed run. No assertion was removed or weakened. The X11 backend adds the
required virtual seat through the compositor, and the unchanged native window
case passes there. This does not demonstrate physical keyboard, IME or pointer
operation.

The passing native scene includes complete overview, files and compact
GPU/reference image equality, plus the existing text/density/reflow checks.
Native draw/ring/present/timing tests retain allocation failures, source lifetime,
bounded retirement/recovery and bytewise RGBA/A8 comparisons. Passing this
environment does not repair or explain WSLg's separate FIFO callback wait.

## Compatibility defect exposed by CI

The first fixture CI run exposed real undefined behavior that the earlier seven
Wayland skips had not exercised. Its Ubuntu Clang18/Wayland1.22 sanitizer job
passed115/116, failing `nexaui_wayland_window` at `window.c:1520` with
`applying zero offset to null pointer`. Ordinary linkage jobs passed116/116.

The affected code is the keyboard-enter callback, not clipboard transfer. A
valid empty pressed-key snapshot can have `data=NULL,size=0`.
Wayland1.22's `wl_array_for_each` computes `data + size` before deciding the
loop is empty. Local Wayland1.25 already short-circuits on zero size. The
callback now skips that iteration macro for an empty snapshot, while still
resetting old key/repeat state, establishing focus and queuing its focus event.
The nonempty held-key loop is retained. No event or sanitizer assertion is
removed, and no installed SDK header is modified.

The pinned [Wayland1.22 header](https://gitlab.freedesktop.org/wayland/wayland/-/raw/1.22.0/src/wayland-util.h)
and the actual CI diagnostic are retained with hashes. A local control compiled
the actual old/new callback translation units with that exact iteration macro
and the same native test object. Both passed with local Clang22; this local
control did **not** reproduce the failing sanitizer diagnostic and is not used
as a negative regression claim. The original Clang18 CI failure is the actual
before case. The corrected local native window case and current-macro control
both pass with normal cleanup/leak checks; the final CI below checks the older
SDK directly.

## CI results and unresolved leak observation

Later documentation/source runs reproduce the Vulkan leak reports. Their
same-process driver mapping and package-side allocation symbols are retained
in the [module follow-up](vulkan-lsan-module-report.md). The historical green
run below remains its own result and is not treated as a leak repair.

[Run39466687698](https://github.com/Lulus792/NexaUI/actions/runs/39466687698),
source `d06fc382732e10d586db71f417e3a9f79867b748`, passes all seven regular
jobs: Windows static/shared114/114 each, macOS95/95 each, Ubuntu static/shared
and Linux ASan/UBSan/LSan116/116 each. All seven native Wayland cases execute
successfully in each Linux job, with zero skips. Scheduled/manual-only Windows
ASan and fuzz campaign jobs are skipped on this push; they are not counted as
successes. [CI receipt](wayland-private-compositor-ci.json) retains exact jobs,
test names and raw-log hashes.

The preceding [run39464199758](https://github.com/Lulus792/NexaUI/actions/runs/39464199758)
already passes the corrected keyboard case, but its sanitizer job fails10/116
tests on LeakSanitizer reports. The reports contain indirect `calloc`
allocations of56 bytes with stack frames in an unloaded `<unknown module>`;
both X11 and Wayland native Vulkan paths are affected. The largest reported
case is1,904 bytes in34 allocations. Their owning module and cause remain
unassigned. The renderer teardown was reviewed, but that is not proof of a
driver, validation-layer or framework cause.

`tools/trace_vulkan_modules.py` adds two fixed native-clear smoke controls with
explicit Khronos/synchronization validation enabled and disabled. Leak
detection remains enabled. On glibc it records loader module addresses with
`LD_DEBUG=files`, uses slow allocation unwinding, and checks whether the
validation library was actually loaded. Both controls pass in the final CI,
with the expected library presence and no leak report. The separate full
CTest suite also passes without a Vulkan production change. This is a
non-reproduction, **not a demonstrated leak repair**; the earlier red run is
retained as an open intermittent observation. The diagnostic remains in CI
to preserve same-process module addresses if it recurs.

On local musl, the first slow-unwind diagnostic deadlocked recursively inside
libgcc unwinding during allocation; its debugger stack and failed run are
retained. The helper now keeps fast allocation unwinding on musl and reports
that glibc loader logging is unavailable. Its two local controls pass, but
cannot attribute the Ubuntu report. No leak suppression, sanitizer disabling,
OS/cache change or identical rerun-until-green campaign was used.

## Private local tool provenance

Weston was absent from the shared Alpine installation. Eight pinned Alpine
v3.24 community packages were downloaded from the official repository, checked
with `apk verify` and extracted under `out/private-weston-14/root`; none was
installed into the OS. The packages are Weston/libweston, desktop shell and
headless/X11 backends 14.0.2-r5, libinput-libs 1.31.3-r0, libevdev 1.13.6-r0 and
mtdev 1.1.7-r0. The retained inventory records URLs, hashes, licenses and
dependencies. Shared installed libraries supply their existing dependencies.

The process-local `LD_LIBRARY_PATH`, `WESTON_MODULE_MAP`, shell and helper paths
select this private prefix. The module-map format was checked in Weston's
14.0.2 `libweston/compositor.c`. The downloaded tools are not redistributed in
this repository. Local compositor logs include missing optional cursor/theme
assets and a layer-finalization warning after compositor termination. They are
retained; framework test exit, sanitizer and validation results are reported
separately rather than treating compositor exit 0 as a clean whole-desktop audit.

The first APK `fetch --recursive --url` inventory attempt also downloaded 104
archives into the project root. They were moved into the ignored tool evidence
directory, with a full hash inventory. Seven byte-identical duplicate archives
were removed only after matching the retained copies, freeing 965,571 bytes;
97 unique archives remain available offline. No pre-existing dependency, cache,
source, frozen failure record or WSL disk was deleted.

## Reproduction

With the ordinary developer test dependencies plus Weston, Xvfb and xauth:

```sh
xvfb-run -a -s '-screen 0 1600x1200x24' \
  python3 tests/check_weston_fixture.py --work-dir out/new-fixture-checks
NEXAUI_REQUIRE_VULKAN=1 NEXAUI_REQUIRE_VULKAN_WAYLAND=1 \
VK_INSTANCE_LAYERS=VK_LAYER_KHRONOS_validation VK_LAYER_VALIDATE_SYNC=1 \
NEXAUI_TEST_PRIVATE_X11=1 \
xvfb-run -a -s '-screen 0 1600x1200x24' \
  python3 tests/run_weston_native.py --work-dir out/new-wayland-session -- \
  ctest --test-dir build --output-on-failure \
    --output-junit "$PWD/out/new-wayland-session/junit.xml"
python3 tests/check_wayland_results.py out/new-wayland-session/junit.xml
```

Work directories must be new so previous evidence is preserved. The local
receipt also retains the private-prefix command and exact executable, package,
fixture, log and JUnit hashes. CI uses its installed test tool instead.

## Remaining acceptance

This supplies an independent compositor execution and removes an avoidable CI
coverage gap. It does not close WSLg presentation liveness, Wayland canvas-reuse
rollout, physical monitor/DPI/input/IME/file-drop, human assistive use, R01 total
Windows startup or complete NexaCloud Design-A acceptance. NexaCloud adopts the
framework separately after its own builds/tests. R01's missing lower-layer
trace/privilege is not replaced with another identical startup campaign.
