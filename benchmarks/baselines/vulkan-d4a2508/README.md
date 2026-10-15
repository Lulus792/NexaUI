# Vulkan instancing and ordered batching at d4a2508

Source revision: `d4a2508660f29a414cbaf99851e391f0f524470d`.
The twelve original CSVs preserve 1536 measured frames: six local X11/Wayland
workloads and six Ubuntu CI X11 static/shared workloads. Each configuration runs
unbatched/batched/batched/unbatched, with four warmup and 32 measured frames per
block. Both modes use the same 84-byte instance layout. Each mode therefore has
64 measured samples per workload/configuration.

The native runner holds normalized input constant, supplies an off-canvas pointer,
rejects geometry/counter drift and compares all four mode images byte-for-byte.
Native resize/close polling continues; interactive input routing is unchanged.
All 48 final mode/block captures are byte-identical for their corresponding
workload across modes, local X11/Wayland and CI static/shared builds. Each capture
also passes its independent full GPU/headless pixel comparison. The earlier
unheld-pointer comparison was rejected after hover differences were observed.

The [pre-instancing baseline](../vulkan-19c07ba/README.md) retains the old payload
and record counts. Its input protocol differs: its screenshots are not claimed
byte-identical to this normalized run, and subtracting its elapsed timings does
not isolate an instancing speedup. The controlled comparison below isolates
ordered batching within the new instance layout.

## Work and payload

Each visible quad now uploads one 84-byte record instead of six 60-byte vertices:
76.7% fewer authored geometry bytes. These are one-way payload counters, excluding
allocation overhead and physical bus traffic. History/bitmap payloads are unchanged.

| Workload | Canvas | Input / visible records | Draw calls off → on | Reduction | Geometry bytes old → new | History copies / bytes | Bitmap bytes |
| --- | --- | --- | --- | --- | --- | --- | --- |
| Overview 100% | 1440×1040 | 1337 / 1309 | 1309 → 470 | 64.1% | 471240 → 109956 | 469 / 745024 | 684032 |
| Files 100% | 1440×1040 | 666 / 610 | 610 → 98 | 83.9% | 219600 → 51240 | 97 / 1159804 | 3579904 |
| Files 200% | 820×700 | 390 / 372 | 372 → 11 | 97.0% | 133920 → 31248 | 10 / 535764 | 2064384 |

Batched API draws equal render passes (470/98/11). A group contains an optional
destination-reading first instance followed by opaque instances; the next reader
retains its original clipped history copy. Each fragment enforces its instance's
exact clip, and primitive order is preserved. GPU allocation counts and warmed
capacities remain stable in all measured blocks.

## CPU measurements

Local environment: Alpine 3.24 under WSLg, GCC 15.2, Debug, Vulkan 1.4.347,
Mesa 26.1.6 Lavapipe (`llvmpipe`, LLVM 22.1.3), DejaVu Sans, 1× backing.
Local executable SHA-256:
`e329232c2b90e88628c837c55ce8f72464ebd4b5f5b0e662847036454697bf0e`.
X11 and Wayland ran sequentially without injected validation layers or a
concurrent local GPU/test process. CI runs use Ubuntu/Xvfb and software Vulkan
on separate hosted machines; absolute static/shared differences do not isolate
linkage cost. Profiling is a separate step from correctness validation.

All intervals below are elapsed CPU milliseconds. Wall time includes UI/driver
work and scheduling; most is inside `vkQueuePresentKHR` here. These are not GPU
timestamps or display latency. Readback, file I/O and sorting are outside the
measured frame interval. The runner checks finite clocks, committed submission
identity, stable counters and phase sum ≤ wall + 0.01 ms. P95 is nearest rank,
index 60 of 64 sorted samples; median prepare+encode uses each sample's sum.

| Local backend | Workload | Encode median off → on ms | Prepare+encode median off → on ms | Wall off median / p95 ms | Wall on median / p95 ms |
| --- | --- | --- | --- | --- | --- |
| x11 | overview-100 | 1.119 → 1.188 | 1.944 → 2.033 | 317.047 / 364.978 | 305.263 / 622.616 |
| x11 | files-100 | 0.356 → 0.306 | 1.492 → 1.498 | 107.157 / 125.648 | 100.579 / 118.133 |
| x11 | files-200 | 0.128 → 0.113 | 0.679 → 0.773 | 31.678 / 35.732 | 26.653 / 35.663 |
| wayland | overview-100 | 0.980 → 0.911 | 1.650 → 1.560 | 297.807 / 343.378 | 263.664 / 277.112 |
| wayland | files-100 | 0.287 → 0.234 | 1.206 → 1.092 | 104.591 / 126.446 | 85.978 / 100.244 |
| wayland | files-200 | 0.107 → 0.078 | 0.553 → 0.513 | 26.785 / 31.995 | 19.452 / 23.756 |

The local X11 overview CPU encode and prepare+encode medians increase; its wall
p95 worsens from 364.978 to 622.616 ms. Files 100% has no useful net CPU interval
gain there, and files 200% prepare+encode is worse despite fewer API calls.
Other medians improve, but mixed tails, debug software rendering and VM scheduling
prevent a general frame-rate or physical-GPU speedup claim.

| CI variant (X11) | Workload | Encode median off → on ms | Prepare+encode median off → on ms | Wall off median / p95 ms | Wall on median / p95 ms |
| --- | --- | --- | --- | --- | --- |
| shared | files-100 | 0.466 → 0.371 | 1.157 → 1.069 | 61.238 / 62.273 | 60.032 / 60.892 |
| shared | files-200 | 0.108 → 0.059 | 0.453 → 0.403 | 20.656 / 21.211 | 19.970 / 20.679 |
| shared | overview-100 | 2.136 → 1.997 | 2.765 → 2.628 | 95.455 / 96.943 | 93.529 / 96.235 |
| static | files-100 | 0.343 → 0.308 | 0.949 → 0.921 | 60.188 / 61.193 | 59.264 / 60.318 |
| static | files-200 | 0.076 → 0.039 | 0.391 → 0.348 | 20.406 / 21.167 | 19.632 / 20.169 |
| static | overview-100 | 1.506 → 1.478 | 2.122 → 2.094 | 93.792 / 94.743 | 92.050 / 94.001 |

## Correctness and portability

Local X11/Wayland core and synchronization validation passed the static drawing
cases (2/2, 13.69/14.07 seconds) and full NexaCloud scene smokes (2/2,
31.35/31.92 seconds). Shared Debug passed all six selected presentation, drawing
and native scene cases in 115.44 seconds. Clang 22.1.3 ASan/UBSan/LeakSanitizer
plus core/synchronization validation passed those same six cases in 116.61 seconds.
These are the executed native subsets, not claims of a complete local suite.

The Release configuration with `NEXAUI_BUILD_TESTS=OFF` builds both native
examples. Its native renderer smoke presents nine X11 and twelve Wayland frames,
exercising the production path without batching test hooks. This checks build
and presentation, not Release performance.

New tests cover exact per-instance clipping/history boundaries at 1/1.25/1.5/2×,
unbatched/batched full-pixel equality, rollback/retry/reuse, and 10000 overlapping
opaque records in one API draw. Existing shape/alpha/bitmap/fault/lifetime cases
run through the instanced path. Embedded shaders reproduce with glslang 16.2.0
and pass `spirv-val --target-env vulkan1.0`.

[CI run 38797120465](https://github.com/Lulus792/NexaUI/actions/runs/38797120465)
passes all six Windows/Linux/macOS static/shared core jobs and Linux sanitizers.
Each Linux core job registers 96 tests: 91 execute successfully and five Wayland
display cases explicitly skip under Xvfb. Local WSLg runs above execute Wayland.
The static CI job also passes twenty untraced X11 lifecycle repetitions.
Both Linux profiling targets succeed, enforcing the mode/geometry/image gates.

Both artifact ZIP digests were checked before reading named CSV/PPM members:

| Artifact | ID | SHA-256 |
| --- | --- | --- |
| linux-gpu-captures-shared-OFF | 12002104063 | `d124c097c2e71913f02b925dd02485a0d741d7e1512d23df451bac77301eb86b` |
| linux-gpu-captures-shared-ON | 12003040804 | `e3a11360a8a0ab430350050c90cee86955767dab30e780f1e59c8506a60a75ec` |

The CSVs remain in this directory; full mode captures and native logs are in
those CI artifacts. This closes the initial instancing/batching measurement,
not V07 as a whole. Concurrent upload rings, V08 GPU timestamps, Release/hardware
performance, physical HiDPI/input/IME and native assistive acceptance remain open.
