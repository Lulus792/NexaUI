# Three-slot Vulkan upload ring at 1cdf550

Source revision: `1cdf550993c8eb95d1929809bbea5a547f2af88a`.
Six original CSVs preserve 768 measured frames from the static/shared Linux CI
jobs in [run 38802136844](https://github.com/Lulus792/NexaUI/actions/runs/38802136844).
Each workload runs serial/ring/ring/serial, four warmup and 32 measured frames per
block. Both modes keep instancing and ordered batching enabled, so this compares
one active upload slot with three. Each mode has 64 samples per workload/variant.

All 24 mode/block GPU images are byte-identical for the corresponding workload
across modes and variants. They also match the previous normalized
[instancing/batching captures](../vulkan-d4a2508/README.md), and the native runner
compares each full image with its independent headless reference. Input is held
constant during warmup/measurement; geometry drift, allocation growth and changed
mode images fail the workload. No readback, sorting or file I/O is timed.

## Resources and invariants

Three fixed slots independently own geometry, bitmap staging, atlas, descriptor,
command buffer, acquisition semaphore and submission fence. CPU planning storage
is reused after encoding. Preparing a slot retires that slot's previous work;
shared canvas/history remain GPU-only with inter-submission dependencies. Resize,
recovery and close wait for all pending submissions/acquisitions/presentations in
one aggregate finite budget. Timeout retains storage and the window lease.
Production needs Vulkan 1.0 plus its existing presentation-maintenance support;
the timeline gate is test-only.

The CSV verifier checks sequential submission IDs, slot cycling modulo 1/3,
`retired + pending == submitted`, finite nonnegative CPU phases,
`phase sum ≤ wall + 0.01 ms`, and stable capacities/counters in each block. Draw and payload counts
remain unchanged between pacing modes:

| Workload | Canvas | Input / records | API draws / history copies | Geometry / bitmap payload bytes |
| --- | --- | --- | --- | --- |
| Overview 100% | 1440×1040 | 1337 / 1309 | 470 / 469 | 109956 / 684032 |
| Files 100% | 1440×1040 | 666 / 610 | 98 / 97 | 51240 / 3579904 |
| Files 200% | 820×700 | 390 / 372 | 11 / 10 | 31248 / 2064384 |

The diagnostic serial mode changes pacing, retaining previously allocated peer
slots. This is not a one-third-memory comparison. Both variants retain aggregate
geometry/staging capacity of 393216/3145728 bytes for overview,
393216/12582912 for files 100%, and 196608/12582912 for files 200%, in every block.
Per-slot geometry, bitmap staging and atlas texel payloads are each capped at
64 MiB: at most 576 MiB across three slots, plus shared canvas/history, driver
alignment and temporary growth candidates. Payload counters are not bus traffic.

## Timing results

Ubuntu hosted CI, GCC 13.3.0, Debug, X11/Xvfb, software Vulkan
`llvmpipe (LLVM 20.1.2, 256 bits)`, DejaVu Sans, 1× backing. Static and shared
jobs run on different machines; their absolute differences do not isolate linkage
cost. Profiling runs separately without injected validation layers. Local WSLg
results below are correctness/sanitizer evidence, not another timing baseline.

All timings are elapsed CPU milliseconds, including scheduling and driver waits.
They are not GPU execution timestamps, display latency or Release/hardware FPS.
P95 is nearest rank (index 60 of 64 sorted values). Prepare+encode statistics use
the per-frame sum before sorting, rather than adding separate medians.

| Variant | Workload | Wall serial median / p95 ms | Wall ring median / p95 ms | Prepare+encode serial median / p95 ms | Prepare+encode ring median / p95 ms |
| --- | --- | --- | --- | --- | --- |
| static | overview-100 | 50.393 / 52.866 | 50.921 / 51.828 | 0.789 / 0.852 | 1.237 / 1.393 |
| static | files-100 | 31.094 / 31.816 | 31.513 / 32.453 | 0.429 / 0.485 | 0.598 / 0.637 |
| static | files-200 | 10.288 / 10.637 | 10.449 / 10.729 | 0.151 / 0.163 | 0.158 / 0.181 |
| shared | overview-100 | 94.694 / 96.355 | 95.188 / 97.389 | 2.091 / 2.322 | 2.071 / 2.518 |
| shared | files-100 | 58.146 / 59.390 | 58.397 / 59.567 | 1.143 / 1.283 | 1.182 / 1.282 |
| shared | files-200 | 19.175 / 19.636 | 19.322 / 20.545 | 0.427 / 0.537 | 0.489 / 0.544 |

| Variant | Workload | Retirement median serial → ring ms | Acquisition median serial → ring ms | Presentation median serial → ring ms | Encode median serial → ring ms |
| --- | --- | --- | --- | --- | --- |
| static | overview-100 | 0.001 → 0.001 | 0.002 → 0.002 | 47.979 → 47.984 | 0.508 → 0.957 |
| static | files-100 | 0.001 → 0.001 | 0.003 → 0.003 | 28.935 → 29.136 | 0.144 → 0.275 |
| static | files-200 | 0.000 → 0.001 | 0.001 → 0.001 | 9.131 → 9.211 | 0.018 → 0.023 |
| shared | overview-100 | 0.003 → 0.003 | 0.005 → 0.005 | 90.051 → 90.253 | 1.444 → 1.437 |
| shared | files-100 | 0.002 → 0.003 | 0.005 → 0.005 | 54.261 → 54.455 | 0.345 → 0.379 |
| shared | files-200 | 0.002 → 0.002 | 0.004 → 0.003 | 17.081 → 17.184 | 0.045 → 0.058 |

These software-driver measurements show no frame-time gain from three-slot pacing:
all six wall medians are slightly higher, while CPU preparation/encoding and tails
are mixed or worse. Static overview encoding rises from 0.508 to 0.957 ms.
Presentation dominates both modes and retirement is already short. The ring's
verified benefit here is preserving independent in-flight storage and bounded
reuse; the measurements do not establish a performance improvement. Physical
GPU/Release testing and V08 GPU timestamps remain required.

## Native correctness

The native ring test queues three actual offscreen submissions through the same
production encoder behind a host-signaled timeline semaphore. Distinct padded
RGBA8/A8 sources and command arrays are freed/overwritten before release. It
checks real one-second timeouts on full-ring reuse, readback, dirty-swapchain
retirement, recovery and close. Caller output, committed identity, all three slots
and pending captures survive those timeouts. Signaling permits reverse-ID
collection, 24 independent full-frame/padding comparisons, wrap/reuse, busy-peer
allocation failure and successful growth/retry. Close releases the window lease.
Ordinary native NexaCloud smokes exercise presentation with the default ring;
legacy single-slot failure tests explicitly select serial pacing.

Two invalid initial test approaches were rejected: creating a new atlas view
while Lavapipe GPU work was deliberately held blocked in `vkCreateImageView`, so
the gate test first warms all atlas capacities; and host-setting `VkEvent` after
submission violates VUID 09543, so the test now explicitly enables/uses the KHR
timeline feature. Production introduces no timeline dependency or host event.

Local Alpine 3.24/WSLg, Mesa 26.1.6 Lavapipe, GCC 15.2/Clang 22.1.3 evidence:

- Static full run: 97/98 passed in 150.09 s, including both native drawing,
  gated-ring and NexaCloud cases. The remaining X11 clipboard assertion was run
  with an incorrect private-display flag on the shared WSLg desktop. Its separate
  private-Xvfb case passed; the ordinary X11 case passes after removing that flag
  (0.16 s). The final expanded ring/growth cases pass again (6.91/7.34 s).
- Shared Debug: all eight selected X11/Wayland presentation/drawing/ring/native
  scene cases pass with core and synchronization validation in 137.09 s.
- Clang ASan/UBSan/LeakSanitizer: those same eight native cases pass with core and
  synchronization validation in 134.45 s, including both gated-ring cases.
- Release with tests disabled builds both native examples and presents eight X11
  and seven Wayland frames. Changed production/test C17 units also pass Clang
  `-Wall -Wextra -Wpedantic -Werror` syntax checks. This is not Release performance.

Both Linux CI core jobs pass all executed tests: 98 registered, 92 executed and
six explicit Wayland display skips under Xvfb (present, draw, ring, window,
NexaCloud and native example). Local WSLg runs execute those native paths. Static
CI also passes twenty untraced X11 presentation lifecycle repetitions; Linux CI
sanitizers and both profiling targets pass. Windows static/shared and macOS static
jobs also pass. The macOS shared job is still queued at this report's publication;
it is not counted as executed acceptance. No macOS backend code changed in this
increment; the conditional build changes still require that queued job's result.

## Preserved artifacts

ZIP SHA-256 was verified before reading only expected CSV/PPM/log members:

| Artifact | ID | SHA-256 |
| --- | --- | --- |
| linux-gpu-captures-shared-OFF | 12004083557 | `8cc52a0a46518a9df91907739e5dbf2b0d6d4113f1476a9727ae0db35630f407` |
| linux-gpu-captures-shared-ON | 12004218395 | `960aba741dd79ffbc814eb2466add1a69aa97213a00ea12af362bddb67bd285e` |

The six raw CSVs are retained here. CI artifacts contain the full native captures
and logs; the static `LastTest.log` holds the final lifecycle repetitions, while
the shared log holds the full suite and gated-ring evidence. CPU profiles and
software-driver comparisons do not close V08/V09 or physical HiDPI/input/IME,
native assistive bridges and release acceptance.
