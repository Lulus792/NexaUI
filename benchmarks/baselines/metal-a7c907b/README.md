# Three-slot Metal upload ring at a7c907b

Source: [run 38790279333](https://github.com/Lulus792/NexaUI/actions/runs/38790279333),
revision `a7c907b8c567f9ba17a75ab0436e8dfa0ed05254`. All six desktop core
variants and Linux sanitizers pass. Both macOS suites pass 81/81 tests. Native
ring tests queue mixed alpha/RGBA/A8 frames, erase/free their sources after
submit, compare 27 retained full-frame GPU snapshots with headless including
row padding, and exercise slot wrap, allocation rollback with two busy peers,
full-ring/resize/recovery timeout injection, allocation reuse and three-slot
retryable driver close.

The subsequent test-only gate at `29eb9c141c6f12797deda42c410c5c0c19a7aa23`
also passes 81/81 in both [macOS variants](https://github.com/Lulus792/NexaUI/actions/runs/38790885087).
An unsignaled `MTLSharedEvent` stops all three command buffers before upload and
render passes. Sources are erased/freed while GPU work is demonstrably pending.
A real one-second readback polling timeout leaves caller bytes unchanged and
all three slots retained; CPU signaling then permits every captured frame to
complete and compare byte-for-byte. The final native log records 100 submitted /
100 retired frames. Its 24 workload captures also match the preceding baseline.
This gate is absent from production builds and installs no asynchronous callback.
The CSVs below remain the original `a7c907b` measurement, not a mixture of runs.

Environment: macOS 26 ARM64, AppleClang 21, Apple Paravirtual device, Debug,
Metal debug layer requested, Arial, 1× backing. Both modes use ordered draw
batching and per-pass state reuse. The one-slot serial baseline and three-slot
ring run serial/ring/ring/serial, with four warmup and 32 measured frames per
block, three workloads and static/shared variants. Each CSV concatenates the
four original blocks with one header, preserving submission, mode and numeric
bytes. There are 64 pooled samples per mode/workload/variant, 768 overall.

The slot index advances modulo its configured limit. After warmup the pending
count is one or three respectively, and all participating slot capacities and
global allocation counters stay stable. Pending counts retained submissions
awaiting CPU retirement; a retained command buffer may already be GPU-terminal.
Counts do not imply that three frames execute simultaneously on the GPU.

All 24 mode/block GPU captures match headless, both static/shared variants and
the pre-batching `f870b29` captures byte-for-byte. Geometry, draws, passes, history
copies and upload payloads remain unchanged between modes:

| Workload | Canvas | Input commands | Records / draws | History copies / bytes | Bitmap / record upload bytes |
| --- | --- | --- | --- | --- | --- |
| Overview 100% | 1024×642 | 1284 | 343 / 28 | 27 / 178176 | 524288 / 39788 |
| Files 100% | 1024×642 | 435 | 396 / 30 | 29 / 851272 | 2097152 / 45936 |
| Files 200% | 820×645 | 376 | 366 / 9 | 8 / 442880 | 4194304 / 42456 |

CPU polling of the oldest slot becomes short, but the wait largely moves into
drawable acquisition. Medians below are milliseconds:

| Variant | Workload | Serial retirement | Ring retirement | Serial acquire | Ring acquire |
| --- | --- | --- | --- | --- | --- |
| Static | Overview 100% | 14.725 | 0.013 | 0.176 | 12.374 |
| Static | Files 100% | 15.262 | 0.008 | 0.163 | 13.036 |
| Static | Files 200% | 8.454 | 0.005 | 5.500 | 14.464 |
| Shared | Overview 100% | 11.679 | 0.007 | 0.208 | 13.666 |
| Shared | Files 100% | 14.679 | 0.007 | 0.185 | 13.347 |
| Shared | Files 200% | 5.113 | 0.005 | 9.343 | 15.026 |

The ring allows CPU preparation while older work remains pending, but does not
establish a uniform speed improvement. CPU prepare+encode medians are higher in
five comparisons, and wall-time p95 is worse in two. The CPU sums are calculated
per sample before taking their median; p95 is nearest rank, sorted index 60 of
64 pooled samples. Values below are milliseconds rounded to three decimals:

| Variant | Workload | Serial prepare+encode | Ring prepare+encode | Serial wall median / p95 | Ring wall median / p95 |
| --- | --- | --- | --- | --- | --- |
| Static | Overview 100% | 2.728 | 3.262 | 18.041 / 25.799 | 16.701 / 17.911 |
| Static | Files 100% | 2.243 | 2.886 | 18.079 / 23.911 | 16.682 / 34.085 |
| Static | Files 200% | 1.489 | 1.405 | 16.611 / 20.958 | 16.616 / 18.678 |
| Shared | Overview 100% | 1.693 | 2.088 | 16.526 / 20.440 | 16.604 / 19.421 |
| Shared | Files 100% | 1.614 | 2.500 | 17.287 / 22.981 | 16.586 / 17.654 |
| Shared | Files 200% | 1.128 | 1.158 | 16.646 / 17.047 | 16.643 / 24.615 |

These are CPU submission-call intervals, including event routing/UI construction
in wall time, not end-to-end visibility or GPU durations. A ring may leave two
additional submissions pending at the end of a block. Its final drain and image
comparison occur outside measured intervals. Scheduling, debug validation and
drawable pacing influence results; no physical Release/FPS or lower input-to-
display latency claim follows. A07 GPU timestamps and hardware acceptance remain
open. Upload payload/capacity counters exclude driver overhead and physical
memory traffic. The ring trades additional bounded storage for overlap.

Artifact ZIP SHA-256, checked before reading named members:

- Static: `fd326484e2833a0226aee5414da1dcd25eb18a0ec5d348eb6d9c6b22e840c4e9`
- Shared: `b8bf64d825a993094eb9e0213981809d66e0a6fa470758f13e3f3ef203cfa89c`

Follow-up gated-test artifact SHA-256, also checked before named-member reads:

- Static: `d429563e880b71ca31a6ecc76ba751788b4e75282631f714d7c8af4b0018ef0e`
- Shared: `35834e8403f8ee7ec524bb53110bfd10d95edb2db5771532ebf7c1b2556e110c`
