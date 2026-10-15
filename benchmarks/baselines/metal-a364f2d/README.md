# Per-encoder state A/B comparison at a364f2d

Source: [run 38783657045](https://github.com/Lulus792/NexaUI/actions/runs/38783657045),
revision `a364f2d35f080d00c44ef6b5cb13950a070b8b1b`. All six static/shared desktop
jobs and Linux sanitizers pass. Both macOS jobs pass 80/80 tests, including
the new counter checks and all A/B GPU/reference comparisons.

Environment: macOS 26 ARM64, AppleClang 21, Apple Paravirtual device, Debug,
Metal debug layer requested, Arial, backing scale 1. This is virtual-device
evidence. See the [initial baseline](../metal-f870b29/README.md) for the cost
that motivated this change.

The production path sets pipeline, culling, viewport and two fragment textures
once per nonempty render encoder. The comparison switch is test-only. Scissors,
vertex/shape arguments, draw calls, render-pass breaks and GPU resources do not
change. A new encoder after a history copy starts with no cached binding state.

Each workload runs A/B/B/A: per-draw block 0, per-pass blocks 1 and 2, per-draw
block 3. Every block has four warmup and 32 measured frames. The six CSV files
concatenate the four original CSVs in that order, retaining the original row
bytes and one common header. Workload names identify the mode and block.
There are 64 measured samples per mode/workload/variant, 768 samples overall.
Within each comparison the scene, device, build and retained capacities match.
Static/shared variants run on different VMs and are not compared causally.

Pooled median averages samples 31/32; pooled p95 is nearest-rank index 60 of 64.
Times below are milliseconds of CPU elapsed encoding, including descheduling.

| Variant | Workload | Per-draw median / p95 | Per-pass median / p95 | Median reduction |
| --- | --- | --- | --- | --- |
| Static | Overview 100% | 5.487 / 12.871 | 2.385 / 6.045 | 56.5% |
| Static | Files 100% | 6.101 / 13.130 | 1.999 / 3.235 | 67.2% |
| Static | Files 200% | 4.718 / 5.742 | 1.400 / 2.669 | 70.3% |
| Shared | Overview 100% | 9.347 / 15.420 | 4.186 / 6.194 | 55.2% |
| Shared | Files 100% | 9.532 / 16.756 | 3.210 / 6.973 | 66.3% |
| Shared | Files 200% | 6.900 / 11.752 | 1.862 / 3.625 | 73.0% |

Each workload has unchanged geometry, uploads and draw/pass counts in all four
blocks and both variants. Only the five invariant state calls are reduced:

| Workload | Draw calls | Nonempty passes | Per-draw bindings | Per-pass bindings |
| --- | --- | --- | --- | --- |
| Overview 100% | 343 | 28 | 1715 | 140 |
| Files 100% | 396 | 30 | 1980 | 150 |
| Files 200% | 366 | 9 | 1830 | 45 |

After every block, readback matches headless. All four mode/block PPMs are also
byte-identical within each workload, across both variants, and with that
workload's preceding `f870b29` GPU capture. The native counter test additionally
checks five draws in three nonempty passes (25 versus 15 invariant calls),
pass restarts, empty clear passes and discarded-submission diagnostics.

Overall frame latency is **not** reduced by the same proportion. Static files
at 200% has wall medians 16.608 versus 16.646 ms; shared files at 200% has wall
p95 24.495 versus 28.764 ms. Presentation pacing, retirement and scheduler noise
remain visible. This result supports the specific CPU encode change, not a
universal FPS/latency claim or a physical Release-performance acceptance.
Longer runs, real hardware, upload rings, draw batching and GPU timestamps
remain required work. There are no wall-time pass/fail thresholds in CI.

Artifact ZIP SHA-256, verified before extracting named members:

- Static: `0c9a63eb4a7ca811bdbbf69fa179d8af8815d60afa49c3b25b0dd2765954f083`
- Shared: `5ab292df100da798e927a467036fa6d23b7e36999fac9c465be08942790c48f8`
