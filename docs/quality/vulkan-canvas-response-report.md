# Unchanged X11 canvas and Orca response

Source: `5d7642dbfb2792a551f9d87de0a3b37dee80accb`, 2026-10-20.
The later [private compositor report](wayland-private-compositor-report.md)
adds executed Wayland CI coverage. The historical failures and skips below
remain evidence of this earlier source/run.
Baseline: `5e47e35104797aa3485ef44b22aecdf68e14adac`.
This is a framework/native-example follow-up to the
[Orca focus correction](orca-focus-report.md). NexaCloud's application code and
tested framework pin were not changed. R01 remains open.

## Diagnosis and change

The baseline rendered the same overview continuously even while its native tree
was being queried. A fresh actual Orca run completed all nine control/dialog
announcements in 7.469–30.056 seconds. Its trace contained 1,117 frames; median
render time was 78.253 ms across both overview and files, compared with 0.471 ms
for semantic publication and 5.068 ms for native polling. These are observations
at CPU API boundaries, not an attribution to exclusive GPU execution time.

A gated independent D-Bus client received a GetRole reply after exactly one
provider pump, before a deliberately delayed second pump (5.636 ms observed).
The proposed deferred-reply explanation was rejected; the AT-SPI transport was
not changed.

An overview-only trace recorded 470 draws/render passes per frame, with most
CPU time inside `vkQueuePresentKHR`. A fixed full/clear-only/full workload
control had median present-boundary costs 261.953/2.461/259.987 ms. Clear-only
deliberately changes pixels and is diagnostic, not a valid product optimization.

The X11 renderer now retains an optional exact CPU proof of the last submitted
canvas. Canonical command metadata and all actual RGBA8/A8 image-row bytes must
match. It owns the old bytes; it never dereferences a retired source pointer.
There are no hash collisions, pointer-identity matches or skipped mutable image
checks. Unused command fields, padding and borrowed addresses do not affect the
proof. Scale, clear color, clipping and all relevant geometry/tint fields do.

A hit skips preparation, clear, drawing and bitmap/vertex uploads while retaining
acquisition, copying, presentation, timestamp and retirement paths. Queue-owned
canvas/history images keep their existing ordering; the retained canvas remains
in transfer-source layout. No new GPU allocation, thread, file or driver-cache
policy is introduced. The [contract](../concepts/vulkan.md#unchanged-x11-canvas)
describes lifetime and private diagnostic fields.

Proof storage and replacement allocation peak are at most **32 MiB per renderer**.
Overflow, unsupported input or optional allocation failure falls back to drawing.
Preparation invalidates the prior proof; only successful submission establishes a
new one. Resize, recovery and teardown release it. Changing frames still draw
normally and pay the bounded CPU comparison/copy cost. No universal hardware or
animation-speed claim follows from this software-driver measurement.

## Native correctness and scope

The new portable test covers exact image ownership, in-place RGBA8/A8 mutation,
address/padding independence, all command kinds, scale/clear/clip/tint changes,
allocation failure and the retained/replacement memory bound. Deliberately
ignoring image bytes or clip position makes it fail at the corresponding check.
The normal successful sanitizer runs retain leak detection; negative controls
that deliberately stop at the failed CHECK omit leak reporting after that abort.

The native fixture keeps complete bytewise GPU/headless comparisons, including
row padding, fractional clips/alpha and 1/1.25/1.5/2× scale. Added cases cover
image mutation, a freed old source, resize/recovery, three pending upload slots
and over-budget fallback. Existing batching/ring profiles explicitly disable
canvas reuse when measuring drawing work, preserving their original assertions.

Actual local results:

| Check | Result |
| --- | --- |
| Windows Debug static portable cache test | 1/1 passed |
| Windows Debug DLL portable cache test | 1/1 passed |
| Windows RelWithDebInfo, actual MSVC `/fsanitize=address` | 1/1 passed |
| Linux Clang ASan/UBSan/LSan full exploratory suite | 114/116 passed; validation setup failed and Wayland draw timed out |
| Installed Alpine Khronos 1.4.343, X11 Vulkan validation/native/draw/present/ring/timing | 9/9 passed |
| Final scoped ASan/UBSan/LSan + native synchronization validation | 14/15 passed; Wayland clear/present timed out |
| Final scoped native draw | X11 13.38 s and Wayland redraw fallback 17.45 s, both passed |
| Two deliberately incorrect cache variants | Both rejected, exit 1 |

The exploratory layer setup first referenced a Windows absolute path, then a
glibc-only downloaded library that cannot load on Alpine/musl. Neither run proves
validation coverage. Final validation uses the already installed native Alpine
1.4.343 layer; no packages or OS settings were changed. Its independent reserved
fence-flag counterexample passed, confirming an actually active validation layer.

Wayland retains the existing redraw path. Exploratory reuse runs timed out in
draw (90 s) and timing (45 s). A captured main-thread stack was waiting through
`wl_display_dispatch_queue` inside Mesa/Lavapipe `vkQueuePresentKHR`, during the
first draw fixture's readback/present. An instrumented full run and bounded
reuse-on/off first-frame controls passed; a cache-disabled full drawing control
also passed. These do not establish the lower-level cause. After limiting reuse
to X11, native Wayland drawing/timing/ring passed, but a **clear-only presentation
test**, which has no reusable command stream, timed out. WSLg presentation
liveness therefore remains an explicit P2 gap; no repeated-until-green campaign,
present-mode workaround or compositor/OS change was applied.

Two earlier draft native failures came from stale test expectations about
submitted vertex counts after reuse. Failed CHECK exits skipped teardown and
produced leak reports; those raw failures are retained. The final targeted and
native runs use their normal cleanup and leak checks.

## Desktop CI

[Run 39457045119](https://github.com/Lulus792/NexaUI/actions/runs/39457045119)
completed successfully at the exact source commit above. The
[CI receipt](vulkan-canvas-response-ci.json) retains job IDs, decoded log hashes,
individual test results and explicit skips:

| Required job | Passed | Skipped |
| --- | ---: | ---: |
| Windows static / DLL | 114 / 114 | 0 / 0 |
| macOS static / shared | 95 / 95 | 0 / 0 |
| Ubuntu static / shared | 109 / 109 | 7 / 7 |
| Linux ASan/UBSan | 109 | 7 |

All seven required jobs passed, including the new portable cache case. Linux
native X11 image comparisons and the existing drawing/ring profiles passed;
the static job's 20 presentation-lifecycle repetitions also passed. All seven
Linux skips are Wayland cases, so CI does not close the local WSLg gap. Install
and add_subdirectory consumers built; the installed consumer ran. Optional
Windows-ASan and fuzz-campaign jobs were skipped; the local MSVC-ASan cache test
above is a separate, actually executed check.

## Actual Orca comparison

The fixed task uses the unchanged `tests/check_nexacloud_orca.py` and its original
deadlines. It verifies native focused controls and actual Orca `SPEECH OUTPUT`
for seven navigation controls plus dialog opening/closing, opens the actual file
page and verifies Bergsee.jpg. A real Speech Dispatcher BEGIN/END preflight and
private PulseAudio null sink precede the task. This is not human listening or
per-control synthesis-completion acceptance.

Environment: Alpine-Dev/WSL 2, Alpine 3.24.2, Orca 50.2, private Xvfb
1600×1200×24, Openbox, session/accessibility buses, runtime/settings directories,
Mesa Lavapipe and the same local Segoe UI font. The old/new static executables
use the same trace wrapper and unchanged core/scene archives. The font is not
redistributed. The comparison order was fixed as after/before/before/after before
launch, with no concurrent local builds or tests. Every run is retained.

The first exploratory after-run, concurrent with builds, failed before application
launch because the private Speech Dispatcher socket was not ready within its
unchanged startup deadline. It is excluded from task latency statistics and
retained as an infrastructure failure.

All four runs passed all nine checkpoints. The [local receipt](vulkan-canvas-response-local.json)
retains the 36 individual times, native focus paths, actual speech entries,
preflight completion, binary/font hashes and raw log hashes:

| Variant | Runs/checkpoints | Minimum | Median | Maximum |
| --- | --- | --- | --- | --- |
| Before | 2 / 18 | 6.687 s | 11.832 s | 33.350 s |
| After | 2 / 18 | 1.347 s | 1.893 s | 3.278 s |

Both baseline stderr logs contain registry warnings about an unresponsive app;
neither after-run stderr log contains that warning. These are two runs per
variant in one software-rendering environment, without CPU affinity or hardware
performance acceptance. Remaining 1–3-second responses are not a claim of
instant or fully accepted assistive operation.

The separate overview-only final trace retains its first full frame (502.921 ms
inside present); subsequent group medians were 2.159/2.084/2.109 ms with no draws
after the first frame. These stage-only costs are not control-announcement
latencies and do not establish total startup acceptance.

## Reproduction and remaining acceptance

Build the Linux native example using the commands in the
[Orca task report](orca-focus-report.md#real-orca-task-and-limits). Run the same
task with a new `--work` directory for each old/new executable. The local receipt
retains executable/archive/font hashes, exact commands, raw result paths and
all comparison checkpoints. Temporary trace wrappers only time existing calls;
they use the normal native example and real Orca, with no simulated speech log.

The final native check used:

```sh
ASAN_OPTIONS=halt_on_error=1:detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 \
NEXAUI_REQUIRE_VULKAN=1 NEXAUI_REQUIRE_VULKAN_WAYLAND=1 \
NEXAUI_TEST_PRIVATE_X11=1 \
VK_ICD_FILENAMES=/usr/share/vulkan/icd.d/lvp_icd.x86_64.json \
VK_LAYER_PATH=/usr/share/vulkan/explicit_layer.d \
VK_INSTANCE_LAYERS=VK_LAYER_KHRONOS_validation VK_LAYER_VALIDATE_SYNC=1 \
xvfb-run -a ctest --test-dir out/vulkan-sanitize \
  -R 'nexaui_vulkan_|nexaui_canvas_cache' --output-on-failure
```

Broader hardware performance, Wayland/compositor liveness, physical monitor/DPI,
IME/file-drop, full text/range/table accessibility, Narrator/VoiceOver and human
screenreader use remain open. This does not close P2, R01 or full NexaCloud
Design-A acceptance. NexaCloud adopts the framework separately after its own
builds and tests.
