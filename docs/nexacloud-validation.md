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
