# First draw-record batching comparison at c31c92e

Source: [run 38786929455](https://github.com/Lulus792/NexaUI/actions/runs/38786929455),
revision `c31c92ead6e2afedf6caa8dc50a8fbb2fcebac2d`. All six desktop variants and
Linux sanitizers pass. Each native macOS suite passes 81/81 tests, including
packed-record clipping, storage growth rollback/reuse, 10000 ordered opaque
draws in one GPU call, releasing source commands during pending work and all
full-scene A/B comparisons.

Environment: macOS 26 ARM64, AppleClang 21, Apple Paravirtual device, Debug,
Metal debug layer requested, Arial, 1× backing. Per-pass state caching is enabled
in both modes. Three workloads each run unbatched/batched/batched/unbatched,
with four warmup and 32 measured frames per block. The six CSVs concatenate
original rows with one header, preserving mode/block names and numeric bytes.
There are 64 samples per mode/workload/variant and 768 measured frames overall.

All four mode/block images match headless. All 24 captures also match across
variants and the corresponding pre-batching `f870b29` image byte-for-byte.
Input commands, visible records, render-pass count, history-copy boundaries
and bitmap upload bytes remain unchanged. GPU API draw calls are reduced:

| Workload | Visible records | Unbatched calls | Batched calls | Record upload bytes |
| --- | --- | --- | --- | --- |
| Overview 100% (1024×642) | 343 | 343 | 28 | 39788 |
| Files 100% (1024×642) | 396 | 396 | 30 | 45936 |
| Files 200% (820×645) | 366 | 366 | 9 | 42456 |

The new record buffer carries 116 bytes per visible quad; legacy `set*Bytes`
arguments carry 100 bytes per quad. Both atlas and record capacities remain
stable after each block's warmup. The native storage test compares all pixels
for 10000 overlapping records, exercises an injected allocation failure and
keeps the owned record storage alive across retryable retirement timeouts.

The first measurement also reveals preparation overhead. The initial path walks
the geometry repeatedly to count, validate mappings and fill the buffer, and
validates groups twice. CPU encode alone improves, but is insufficient to judge
the change. These medians sum preparation and encoding **per sample before**
computing the pooled median (milliseconds):

| Variant | Workload | Unbatched prepare+encode | Batched prepare+encode |
| --- | --- | --- | --- |
| Static | Overview 100% | 4.481 | 3.175 |
| Static | Files 100% | 4.977 | 2.511 |
| Static | Files 200% | 3.815 | 1.982 |
| Shared | Overview 100% | 3.715 | 3.777 |
| Shared | Files 100% | 3.977 | 3.207 |
| Shared | Files 200% | 3.782 | 2.006 |

The shared overview shows no net median gain in this first run. Wall-time tails
are noisy: shared files at 100% changes from 22.521/47.361 ms median/p95 to
25.350/93.211 ms, and static files at 200% from 16.163/30.048 to 16.659/48.901 ms.
These observations do not establish an overall latency or FPS improvement.
They motivate a follow-up to eliminate repeated preparation. CPU intervals
include scheduling; GPU timestamps, concurrent upload rings, physical hardware
and Release acceptance remain open.

Artifact ZIP SHA-256, checked before extracting named members:

- Static: `e4c70f8893799a5ed6283bad73d98311f8e75496ff3137a22fdcb7eb96f06ad8`
- Shared: `db80217533678ad1ec71489fe457e7eb02062446ffb406949aa04b8d55d48612`
