# Vulkan GPU timestamp baseline at fa58bc1

Source revision: `fa58bc1ac6e86d8a856aea8377dd881860537249`.
[CI run 38804963874](https://github.com/Lulus792/NexaUI/actions/runs/38804963874)
passes all six Windows/Linux/macOS static/shared core variants and Linux
sanitizers, including installed consumers. The six original CSVs preserve 768
measured CPU frames with **768/768 ready GPU timestamp pairs**, joined by committed
submission ID and frame slot. Each workload runs serial/ring/ring/serial with
batching enabled, four warmup and 32 measured frames per block: 64 samples per
mode/workload/variant.

All 24 mode/block images are byte-identical for the corresponding workload
across pacing modes and variants, and match the previous
[ring captures](../vulkan-1cdf550/README.md). Each native capture also matches its
independent full headless reference. Input, geometry, payload counts and warmed
allocation capacities remain fixed during measurement. Readback, tail draining,
ID joining, sorting and file output occur outside measured CPU frame intervals.

## Measurement contract

Each upload slot owns two timestamp queries. The command buffer resets that
slot's pool after its prior fence has retired, then records top-of-pipe start
and bottom-of-pipe end around all normal commands. The CPU reads 64-bit
result/availability pairs only after the same submission fence succeeds, without
query `WAIT_BIT` or partial results. No pool is overwritten or destroyed while
pending. Unsupported queues continue rendering with unavailable timing entries.

A fixed 64-entry journal distinguishes pending/ready/unavailable/failed and keeps
the highest completed ID separate from the latest submitted CPU frame. Cached
lookup neither polls nor waits. Successful recovery clears old-device timings
but keeps submission IDs monotonic. A later presentation failure can coexist with
valid GPU timing for the successfully queued commands. Test-only readback is
flagged explicitly; none of the measured 768 command buffers includes readback.

Both CI Vulkan devices report 64 meaningful bits and a 1 ns timestamp period.
The verifier checks finite nonnegative CPU/GPU intervals, sequential IDs and slot
cycling, `retired + pending == submitted`, phase sum within CPU wall, explicit
ready/readback state, raw GPU-tick arithmetic and a plausible observation bound.
For narrower counters, unsigned modular differences are accepted only when the
CPU interval from before Submit to after fence observation is shorter than one
whole device-counter wrap; otherwise the wrap count is ambiguous. The exported
`gpu_observation_ns` preserves that upper bound. A one-tick quantization tolerance
is allowed. CPU intervals are never subtracted from a GPU clock epoch.

The GPU interval is whole-command-buffer elapsed time: it includes GPU-side
dependencies, clear, bitmap uploads, history snapshots, rendering and final
copies/transitions. It is not per-pass time or GPU busy-only time. An initial
top-of-pipe timestamp can precede a later-stage acquisition wait; implementations
can also write at a later stage than requested. This does not measure visible
presentation latency. CPU/GPU intervals overlap and must not be added to claim
end-to-end latency. No calibrated clock-domain correlation is implemented.

## Distributions

Ubuntu hosted CI, GCC 13.3.0, Debug, X11/Xvfb, software Vulkan
`llvmpipe (LLVM 20.1.2, 256 bits)`, DejaVu Sans, 1× backing. Profiling runs
separately from injected validation. Static/shared jobs use different machines;
their absolute differences do not establish linkage cost. This is a new timing
baseline, not an isolated query-instrumentation overhead comparison with the old
run. P95 is nearest rank (index 60 of 64 sorted samples).

| Variant | Workload | GPU serial median / p95 ms | GPU ring median / p95 ms | CPU wall serial median / p95 ms | CPU wall ring median / p95 ms |
| --- | --- | --- | --- | --- | --- |
| static | overview-100 | 90.405 / 91.940 | 89.901 / 91.260 | 95.487 / 97.328 | 94.963 / 96.550 |
| static | files-100 | 54.635 / 55.825 | 54.552 / 55.605 | 58.944 / 60.300 | 58.620 / 59.991 |
| static | files-200 | 17.096 / 17.597 | 17.150 / 17.702 | 19.417 / 19.963 | 19.382 / 19.986 |
| shared | overview-100 | 89.040 / 92.140 | 88.944 / 90.705 | 93.558 / 96.596 | 93.443 / 95.633 |
| shared | files-100 | 54.155 / 55.078 | 54.305 / 55.320 | 58.034 / 58.899 | 58.162 / 59.040 |
| shared | files-200 | 17.069 / 17.359 | 17.104 / 17.968 | 19.152 / 19.486 | 19.277 / 20.300 |

| Variant | Workload | CPU retirement median serial → ring ms | CPU encode median serial → ring ms | CPU presentation median serial → ring ms |
| --- | --- | --- | --- | --- |
| static | overview-100 | 0.043 → 0.042 | 1.504 → 1.425 | 90.515 → 90.014 |
| static | files-100 | 0.041 → 0.041 | 0.373 → 0.377 | 54.734 → 54.656 |
| static | files-200 | 0.038 → 0.036 | 0.046 → 0.052 | 17.212 → 17.265 |
| shared | overview-100 | 0.040 → 0.040 | 1.251 → 1.270 | 89.179 → 89.093 |
| shared | files-100 | 0.040 → 0.038 | 0.331 → 0.353 | 54.271 → 54.418 |
| shared | files-200 | 0.036 → 0.036 | 0.036 → 0.050 | 17.149 → 17.200 |

GPU and CPU results vary by workload/mode, with both improving and worsening
tails. For example, shared files 200% GPU p95 rises from 17.359 to 17.968 ms,
while shared overview GPU p95 falls from 92.140 to 90.705 ms. CPU presentation
dominates these software-driver frames and includes substantial waiting. The
results establish valid Vulkan timing collection, not a uniform ring speedup,
physical-GPU throughput, Release FPS or display latency.

Scene counts remain the same as the previous instanced/batched path:

| Workload | Canvas | Input / visible records | API draws / history copies | Geometry / bitmap payload bytes |
| --- | --- | --- | --- | --- |
| Overview 100% | 1440×1040 | 1337 / 1309 | 470 / 469 | 109956 / 684032 |
| Files 100% | 1440×1040 | 666 / 610 | 98 / 97 | 51240 / 3579904 |
| Files 200% | 820×700 | 390 / 372 | 11 / 10 | 31248 / 2064384 |

Serial diagnostic pacing retains peer allocations; it is not a one-third-memory
comparison. Query pools add six timestamp queries across three slots. Payload
budgets and shared-canvas/presentation retirement remain unchanged.

## Native and portable validation

Portable C17 tests cover available zero/equal ticks, 36-/64-bit wrap, invalid/high
bits, nonfinite/reversed/impossible values, ambiguous full wraps, unavailable and
failed states, out-of-order completion, stale IDs and two journal wraps. They run
on all desktop CI variants. The 36-bit cases are mathematical simulations; native
timestamp execution here uses the reported 64-bit software devices.

Native X11/Wayland tests retain the last 64 of 67 submissions, check cached
pending lookup and bounded timeout/retry, disabled future sampling, unavailable/
failed query-result injection, readback scope, recovery reset and the distinction
between GPU completion and presentation failure. The final native test also
fails creation of the third query pool after two real pool/command/fence sets
exist; recovery retry cleans partial resources and preserves the window lease.
The timeline-gated ring test inspects pending timings while GPU work is actually
held, then validates capture timings after release and full-frame comparisons.

Local Alpine 3.24/WSLg, Mesa 26.1.6 Lavapipe, GCC 15.2 and Clang 22.1.3:

- Full static Debug run: 101/101 pass with required Vulkan X11/Wayland execution,
  required GPU timing, core and synchronization validation in 188.27 s. The final
  extra partial-pool-recovery/observation-field changes pass the focused timing
  follow-up (3/3, 3.65 s). Both native journals report 64/64 retained IDs with
  64-bit counters and a 1 ns period.
- Shared Debug: all 12 selected device, math and X11/Wayland presentation/drawing/
  timing/ring/NexaCloud cases pass with core and synchronization validation in
  221.17 s. These scenes use DejaVu Sans. Clang ASan/UBSan/LeakSanitizer passes the
  same 12 cases with core/synchronization validation in 246.14 s; its explicitly
  configured native scene font is the local Windows Segoe UI file. These are
  correctness runs with different fonts, not comparable performance baselines.
- Release with tests disabled builds both native examples and presents five X11
  and four Wayland frames. Changed production/test units pass Clang C17
  `-Wall -Wextra -Wpedantic -Werror` syntax checks. This is not Release performance.

Both Linux CI core jobs register 101 tests: 94 execute successfully and seven
Wayland display cases explicitly skip under Xvfb (present, draw, timing, ring,
window, NexaCloud and native example). Local WSLg executes the native Wayland
paths. Static CI additionally passes twenty untraced X11 lifecycle repetitions.
Both native profiling targets require valid GPU timing and complete successfully.

## Artifact provenance

ZIP SHA-256 is checked before reading only expected CSV/PPM/log members:

| Artifact | ID | SHA-256 |
| --- | --- | --- |
| linux-gpu-captures-shared-OFF | 12006530994 | `eefd601cc07a0e209831dc01dc9c9716af3ee29d70f559f3430bfd6778e1beb8` |
| linux-gpu-captures-shared-ON | 12005658741 | `9da83ed5fbede6fcf10dcb4a63fdf0dffa66445d33f9b68c5d86827631cbe32d` |

The six raw CSVs are retained in this directory. CI artifacts hold full captures
and native logs; the static final `LastTest.log` contains lifecycle repetitions,
while the shared log holds the full suite and 64-ID GPU journal evidence.
V08 now has native software-device timing evidence. Physical GPU timing and
device-loss behavior, V09 display/scale integration, assistive bridges and release
acceptance remain separate open work.
