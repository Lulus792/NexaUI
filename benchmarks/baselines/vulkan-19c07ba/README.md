# Vulkan CPU baseline before instancing at 19c07ba

Source revision: `19c07baa309624d0580250b759c983ef26f246fa`. These six original
CSVs retain 32 measured rows each, following four warmup frames per workload:
192 measured frames across X11 and Wayland. Each workload ends with a full
GPU/software-reference comparison. Captures match between backends.

Environment: Alpine 3.24 under WSLg, GCC 15.2, Debug, Vulkan 1.4.347,
Mesa 26.1.6 Lavapipe (`llvmpipe`, LLVM 22.1.3), DejaVu Sans, 1× backing.
The X11 and Wayland performance processes ran sequentially, without injected
validation layers or a concurrent test run. The earlier overlapping exploratory
run is excluded. The executable SHA-256 was
`000f9ee33e5943b3ff4ad0498e6fb7ba0ded1a71e5ba685899c829310c5648b5`.

The independent native drawing cases passed on X11/Wayland with core and
synchronization validation (13.99 / 14.16 seconds). The full native scene
smokes also passed with synchronization validation (35.56 / 34.48 seconds).
Profiling is explicitly opt-in with `--profile`; ordinary `--smoke` keeps the
correctness comparisons without repeating the performance workload. Repeated
profiling under synchronization validation exceeded the original 180-second
smoke budget, with live stacks showing validation-layer barrier processing.
Performance measurements and validation acceptance therefore remain separate.

Before instancing, each visible quad uploads six 60-byte vertices. One draw is
encoded per visible record. Each destination reader retains its clipped-area
history copy. Both backends have the same stable workload counts:

| Workload | Canvas | Input commands | Records / draws | History copies / bytes | Vertex / bitmap upload bytes |
| --- | --- | --- | --- | --- | --- |
| Overview 100% | 1440×1040 | 1337 | 1309 / 1309 | 469 / 745024 | 471240 / 684032 |
| Files 100% | 1440×1040 | 666 | 610 / 610 | 97 / 1159804 | 219600 / 3579904 |
| Files 200% | 820×700 | 390 | 372 / 372 | 10 / 535764 | 133920 / 2064384 |

Monotonic CPU intervals separate submission retirement, preparation, acquisition
and fence waits, encoding, queue submission and presentation. Profiles publish
only for successfully queued work, including queued frames whose presentation
subsequently fails. Failed/suspended attempts before queueing preserve the last
committed profile. CPU wall includes UI construction and driver preflight too.
No readback, file I/O or sorting occurs inside a measured frame interval.
Allocations must remain stable after warmup; clocks/counters are checked without
a portable timing threshold. P95 is nearest rank, index 30 of 32 sorted samples.

| Backend | Workload | Wall median / p95 ms | CPU encode median / p95 ms |
| --- | --- | --- | --- |
| X11 | Overview 100% | 265.490 / 308.350 | 0.988 / 1.612 |
| X11 | Files 100% | 86.767 / 95.509 | 0.309 / 0.413 |
| X11 | Files 200% | 23.688 / 25.667 | 0.126 / 0.279 |
| Wayland | Overview 100% | 270.797 / 287.799 | 0.937 / 1.358 |
| Wayland | Files 100% | 81.414 / 88.104 | 0.263 / 0.375 |
| Wayland | Files 200% | 21.574 / 24.318 | 0.100 / 0.128 |

Most elapsed CPU time is inside `vkQueuePresentKHR` in this CPU-driver setup.
That interval can include waiting, driver execution and scheduling; it is not a
GPU timestamp or pure display latency. Upload counters are authored one-way
payloads, excluding allocation overhead and physical traffic. Results do not
establish hardware-GPU or Release performance. Later instancing/batching needs
its own in-process controlled comparison; subtracting different runs does not
isolate a speedup.
