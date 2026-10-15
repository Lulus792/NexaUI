# NexaCloud component validation — 2026-10-14

Local environments: Windows 11 Pro build 26200 with Visual Studio 2022/MSVC;
Alpine 3.24 under WSL/WSLg with GCC 15.2, and Clang 22 sanitizers. Example tests
use local Arial on Windows and a locally supplied TrueType font on Linux. Font
files are not bundled. No macOS host or native screenreader was available locally.

| Check | Result |
| --- | --- |
| Windows static Debug full suite | 85 passed, one clipboard test skipped; 86 total |
| Linux static Debug full suite | 88 passed, including existing X11/Wayland Vulkan checks under core and synchronization validation |
| Windows shared Debug new APIs/themes/example | Five targeted tests passed; final scene changes additionally reran both example tests |
| Linux shared Debug new APIs/themes/example | Four targeted tests passed |
| Clang ASan/UBSan/leak checks | Theme, cloud-component and text-stamp tests passed |
| Final Windows example workflow | Portable and native workflows passed after final scene changes |
| Final Linux example workflow | Shared build workflow passed after final scene changes |

The new portable checks verify keyboard activation and disabled state, named
selection/progress semantics, rounded border pixels, and rollback/retry of every
injected action allocation failure. Text-stamp creation is exercised at every
allocation failure with balanced allocations and exact deallocation sizes. Its
white-ink pixels match direct layout rendering in the fixture, and remain usable
after layout/fonts/source bytes are released. Blank output and frame-state
validation are covered.

The example workflow verifies page navigation, keyboard file selection, retained
description drafts longer than 128 bytes, reversible demo deletion, local folder
creation, empty search, narrow layout, compact inspector, modal dismissal/focus
return and runtime theme changes. The native Windows workflow routes actual
Win32 pointer/key messages, checks selection semantics, resizes the native window,
and captures real D3D12 back buffers. The checked-in [overview](images/nexacloud-overview.png)
and [file browser](images/nexacloud-files.png) captures were visually reviewed.
Software previews of compact details, empty search and Light settings were also
reviewed. These are renderer images from hidden test windows, not photographs of
physical monitors.

Commands used for the principal checks:

```powershell
cmake --build build --config Debug --parallel 4
ctest --test-dir build -C Debug --output-on-failure --timeout 120
ctest --test-dir build -C Debug -R nexaui_nexacloud -V
ctest --test-dir build-shared -C Debug -R 'nexaui_(text_stamp|cloud_components|nexacloud.*|theme)$' --output-on-failure
```

```sh
NEXAUI_REQUIRE_VULKAN=1 NEXAUI_REQUIRE_VULKAN_WAYLAND=1 \
VK_INSTANCE_LAYERS=VK_LAYER_KHRONOS_validation VK_VALIDATION_VALIDATE_SYNC=1 \
NEXAUI_TEST_PRIVATE_X11=1 xvfb-run -a \
ctest --test-dir out/vulkan-clean --output-on-failure --timeout 120

ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 \
ctest --test-dir out/vulkan-sanitize \
  -R 'nexaui_(text_stamp|cloud_components|theme)$' --output-on-failure
```

One isolated Debug smoke run measured 30 warm compact frames at 15.392 ms mean
wall time including event polling and presentation; its last measured GPU frame
was 4.131 ms with 387 draw calls and hardware rendering (`WARP=0`). A later run
with other tests active reported 11.009 ms and 2.981 ms respectively. These hidden
window samples are development observations, not a visible-window frame-rate,
latency or cross-hardware performance acceptance. The runner avoids an extra
16 ms event wait before a presenting frame and waits while minimized.

Open checks remain explicit: full native Linux rendering of this scene, Cocoa/
Metal, large text at 200%, physical HiDPI/4K and monitor changes, native assistive
technologies, visible-window performance distributions and release packaging.
The [delivery plan](nexacloud.md) retains these items. No complete three-OS native
support is claimed by these results.

## CI compatibility follow-up

The [previous published CI run](https://github.com/Lulus792/NexaUI/actions/runs/38615047096)
revealed three portability failures that local builds did not expose:

- A Windows checkout with `core.autocrlf=true` changed pinned Unicode bytes.
  A reproduced `BidiBrackets.txt` checkout hashed to `5002128b…`; preserving the
  data with `.gitattributes` restores the expected `4b3b62e4…`. The local full
  Unicode generator check passes without changing pinned data or hashes.
- The older Ubuntu protocol headers omit text-input-v3's version-2 listener
  fields. Their no-op callbacks and initializer entries now compile only when
  the generated event macros exist. The current local protocol build and eight
  Wayland/Vulkan-Wayland tests pass; the older Ubuntu build is checked by CI.
- The CI Unix build had no `CMAKE_BUILD_TYPE` while installation explicitly
  requested `Debug`, omitting its imported target location. The core matrix
  now configures Debug consistently. A fresh local Ninja Debug install and
  installed-package consumer configure/build/run pass. The corresponding
  macOS CI rerun remains the macOS package verification.

These are build/check compatibility fixes and do not add a native macOS window
or full Linux scene renderer.

The follow-up run completed both macOS static/shared jobs, including the portable
NexaCloud workflow and installed-package checks, and the Ubuntu static job.
It exposed three further checks: direct math calls in shared-build test/example
executables needed their own `libm` links; the Forms example compared a null,
empty semantic span with `memcmp`; and WARP crashed during controlled removal
with prior work outstanding. Math users now declare the system library and
the empty span bypasses `memcmp`. The Forms reproducer passes Clang ASan/UBSan.

The WARP failure was reproduced locally in `d3d10warp.dll` with MSVC ASan. The
test-only removal hook now drains this renderer's previous work before faulting
the device. A removed device signals all fences to `UINT64_MAX`, so waiting on
those values after removal cannot distinguish normally completed work; see
[Microsoft's RemoveDevice contract](https://learn.microsoft.com/en-us/windows/win32/api/d3d12/nf-d3d12-id3d12device5-removedevice).
The controlled WARP reproducer passes with MSVC ASan, and the default/WARP/widget
tests pass in static and shared Windows Debug builds. An explicit WARP smoke
case now preserves this coverage on hardware-equipped hosts too. Unexpected
hardware removal remains separate acceptance work.

The [next published CI run](https://github.com/Lulus792/NexaUI/actions/runs/38640428162)
passed both Windows and Ubuntu static/shared jobs and the sanitizer job. At the
time of this report, both macOS jobs are still queued. The preceding run's
successful macOS jobs remain the available package/core evidence; neither run
implements or validates a Cocoa/Metal NexaCloud window. The subsequent Linux
clip/alpha increment has its own [local validation report](concepts/vulkan.md#v05-local-validation--2026-10-15).

## Native Linux Vulkan scene — 2026-10-15

Alpine 3.24 under WSLg, GCC 15.2 / Clang 22.1.3, Vulkan 1.4.347 and Mesa 26.1.6
Lavapipe (LLVM 22.1.3) render the common NexaCloud scene through the native X11
and Wayland adapters. No full-frame CPU upload substitutes for GPU rendering.
The screenshot capture compares the GPU canvas against the software reference
after the same frame is recorded; all RGBA8 bytes must match before writing it.

| View | Native client size | Commands | Distinct bitmaps | Atlas |
| --- | --- | --- | --- | --- |
| Overview | 1440×1040 | 1490 | 73 | 1024×256 |
| Files and inspector | 1440×1040 | 662 | 96 | 1024×1024 |
| Compact files | 820×700 | 437 | 31 | 1024×1024 |

Both backends passed every comparison. The captured
[overview](images/nexacloud-linux-overview.png), [file browser](images/nexacloud-linux-files.png)
and [compact view](images/nexacloud-linux-compact.png) were visually inspected for
legible labels, thumbnails, ordering, selected files and compact navigation.
They are captures of NexaUI's implementation, not copies of the supplied mockups.

The complete static Debug suite passed 90/90 tests in 138.23 seconds. The shared
build passed all eight Vulkan tests and both native NexaCloud cases in 128.99
seconds (10/10). Clang ASan/UBSan/LeakSanitizer plus core and synchronization
validation passed those ten cases and the queued Wayland offer regression in
122.42 seconds (11/11). The source/header shader check and Vulkan 1.0 SPIR-V
validation pass. The Windows MSVC native NexaCloud target also reconfigured,
built and passed its existing D3D12 smoke after the CMake extension.

The final row-buffered PPM capture writer was rebuilt and rechecked in both
native cases, statically (2/2, 63.86 seconds) and with sanitizers (2/2, 68.65
seconds). Its size check also avoids an always-false comparison on 64-bit hosts.
The test-disabled Release configuration built both native Linux examples; its
basic Vulkan sample presented RGBA8/A8 frames on X11 (four) and Wayland (six).

Upload tests cover padded RGBA8/A8 rows, generated glyph coverage, mutable source
pixels, exact tint/alpha rounding, clipping and fractional sampling at
1×/1.5×/2×. Allocation faults hit initial/growing host tables, texture images and
staging storage; prior streams remain usable. Atlas budget errors return before
image acquisition. Borrowed source storage is released while submitted work is
retained, with no subsequent CPU access; submission fences prevent overwriting
its owned upload. Rebuild/recovery, reuse and retryable close are checked.

The native scene smoke uses normalized pointer/key events and checks selection
semantics. It creates a separate compact native window; it does not prove a
physical Wayland resize or installed compositor input/IME behavior. Actual
hardware GPU performance, native keyboard/IME walkthroughs, physical HiDPI/4K,
200% large-text reflow, screenreaders and macOS remain open. Readback agreement
also does not prove compositor color management or a visible-frame latency
budget. Vulkan's final canvas-to-swapchain copy is separately covered by X11
displayed-pixel tests.

```sh
NEXAUI_REQUIRE_VULKAN=1 NEXAUI_REQUIRE_VULKAN_WAYLAND=1 \
VK_INSTANCE_LAYERS=VK_LAYER_KHRONOS_validation VK_VALIDATION_VALIDATE_SYNC=1 \
NEXAUI_TEST_PRIVATE_X11=1 xvfb-run -a \
ctest --test-dir out/vulkan-clean --output-on-failure --timeout 180
```

## Initial macOS native path — 2026-10-15

At `74aa8ec`, both macOS 26 ARM64 / AppleClang 21 CI jobs passed all 77 tests,
including AppKit callback/lifecycle, Metal shapes/clips/straight alpha, exact
readbacks, recovery/retirement and the native clear example
([static](https://github.com/Lulus792/NexaUI/actions/runs/38768396552/job/116537519079),
[shared](https://github.com/Lulus792/NexaUI/actions/runs/38768396552/job/116537519309)).
These Metal tests ran rather than skipping; the runner uses Apple's virtual
GPU. Both consumer variants built, and the installed C consumer ran.

The subsequent A05 change implements RGBA8/A8 atlas uploads and a native
AppKit/Metal NexaCloud runner. Its authored smoke captures the shared overview,
file browser and a second compact window only after each GPU frame equals its
headless reference. The new image/full-scene GPU execution and visual review
remain pending. CPU atlas/preparation tests pass in Windows static/shared Debug
and Linux Clang ASan/UBSan/LeakSanitizer; the new entry point compiles as plain
C17 on Linux. Neither result establishes native GPU image rendering. The CI
workflow saves macOS captures and CTest logs for subsequent inspection.

## Native macOS scene — 2026-10-15

After correcting packed-vector access, `fb80bd0` passed all 79 tests in both
[static](https://github.com/Lulus792/NexaUI/actions/runs/38773070623/job/116553624423)
and [shared](https://github.com/Lulus792/NexaUI/actions/runs/38773070623/job/116553624411)
macOS 26 ARM64 / AppleClang 21 CI jobs. The AppKit/Metal NexaCloud test executed
with Metal validation enabled; it did not skip. Every RGBA8 canvas byte matched
the headless reference before capture.

| View | Actual GPU canvas | Commands | Distinct bitmaps | Atlas |
| --- | --- | --- | --- | --- |
| Overview | 1024×642 | 1306 | 64 | 1024×128 |
| Files | 1024×642 | 501 | 46 | 1024×1024 |
| Compact files | 820×645 | 409 | 23 | 1024×1024 |

The virtual desktop constrained the requested 1440×1040 and 820×700 windows to
these actual client sizes. The narrower layout scrolls content and exposes file
details through its dialog rather than the wide side inspector. An additional
native compact-details capture and Escape/focus check is authored but not yet
executed. These captures do not establish a 1440-pixel-wide macOS inspector test.

The [overview](images/nexacloud-macos-overview.png), [files](images/nexacloud-macos-files.png)
and [compact view](images/nexacloud-macos-compact.png) were inspected for readable
labels, selection, thumbnails, chart/transfer ordering and compact navigation.
The static/shared PPM files are byte-identical. These PNGs are direct lossless
conversions of the GPU captures, with no visual retouching. Artifact ZIP SHA-256:
static `9ee7e57d7f5d9022bd0b4541bdafc11e004e2259e29c0343498e1b5ea90896d3`;
shared `5434bc0d16140e95f95d51cbebc5251c04b088427a2d00ca34dfba8455f5f2c6`.
They include the full native test logs and belong to run `38773070623`.

This is Apple Paravirtual GPU evidence. Actual hardware performance, physical
input/IME, 200% text, Retina/4K/monitor transitions and VoiceOver acceptance
remain separate open requirements.

### Compact details and complete CI matrix

At `2f70d68`, both macOS jobs again pass 79/79 tests, now including opening the
compact details dialog, exact GPU/reference comparison, Escape dismissal and
focus restoration to the details action. The [dialog capture](images/nexacloud-macos-compact-details.png)
was reviewed for readable title/file labels, preview, scrollable content and
the fixed close action. Its canvas is 820×645, with 537 commands, 44 distinct
bitmaps and the reusable 1024×1024 atlas. All four static/shared PPM files are
byte-identical. The scrollable dialog capture does not prove access to every
offscreen field; a full physical keyboard/IME/assistive walkthrough remains open.

Artifact SHA-256 for run `38775104288`: static
`ea4fe4c419ed14b38a6629890ac92aea837627729482f9cd1d3c5e454f4465f5`, shared
`ecf278b96ad1fd175ff25df92e1807b8220254960c7262fd8b287043d7b04bc8`.

The [complete run](https://github.com/Lulus792/NexaUI/actions/runs/38775104288)
passes all six Windows/macOS/Ubuntu static/shared core jobs and the Linux
sanitizer job. Optional manual Windows-ASan/fuzz jobs were not selected. Ubuntu
uses the corrected, checksum-pinned validation layer and passes the active
counterexample plus 20 untraced X11 lifecycle repetitions; five Wayland-display
cases skip on its Xvfb-only desktop. See the [Vulkan investigation and resolution](concepts/vulkan.md#validation-tool-compatibility).
