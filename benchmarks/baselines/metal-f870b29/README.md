# Metal submission baseline at f870b29

These unmodified CSV files come from the native NexaCloud smoke in
[run 38782640460](https://github.com/Lulus792/NexaUI/actions/runs/38782640460),
revision `f870b29497c2c9a35e358817eb46f68b1647c751`.
Both macOS variants passed 80/80 tests, including GPU/reference comparisons
after each workload and exact profile counters for clipped draws/copies.
All ten GPU captures were byte-identical across static/shared variants.

Environment: macOS 26 ARM64, AppleClang 21, Apple Paravirtual device, Debug,
Metal debug layer requested, Arial, backing scale 1. Each workload has four
warmup frames and 32 measured frames; no screenshot readback or file I/O occurs
inside the measurement. The 100% workloads use a 1024×642 canvas; 200% files
uses 820×645. The virtual display constrains the requested window size.

All values below are milliseconds. The median averages sorted samples 15/16;
p95 is nearest-rank sample 30 (zero-based) of 32.

| Variant | Workload | Encode median / p95 | Retirement median | Wall median / p95 |
| --- | --- | --- | --- | --- |
| Static | Overview 100% | 7.027 / 11.698 | 13.161 | 22.812 / 28.982 |
| Shared | Overview 100% | 5.246 / 6.087 | 10.245 | 16.889 / 19.996 |
| Static | Files 100% | 6.862 / 10.684 | 14.766 | 22.642 / 27.907 |
| Shared | Files 100% | 6.230 / 13.142 | 14.235 | 22.218 / 48.668 |
| Static | Files 200% | 4.949 / 7.776 | 5.271 | 16.658 / 19.635 |
| Shared | Files 200% | 4.685 / 5.428 | 5.147 | 16.669 / 18.069 |

Counts were identical in all samples and across both variants:

| Workload | Commands | Draws | Render passes | History copies / bytes | Atlas upload bytes | Draw argument bytes |
| --- | --- | --- | --- | --- | --- | --- |
| Overview 100% | 1284 | 343 | 28 | 27 / 178176 | 524288 | 34300 |
| Files 100% | 435 | 396 | 30 | 29 / 851272 | 2097152 | 39600 |
| Files 200% | 376 | 366 | 9 | 8 / 442880 | 4194304 | 36600 |

These are CPU intervals and one-way copy payloads. Retirement includes CPU
polling and descheduling; it is not a GPU timestamp. Wall time includes the
application frame, event polling, driver preflight, retirement and presentation
throttling. Virtual GPU, debug-layer and scheduler effects prevent extrapolating
to physical hardware or Release performance. Static/shared runs used different
runner instances and are observations, not an A/B comparison. No release frame
budget is accepted by this baseline.

Artifact ZIP SHA-256, verified before extracting the named CSV/log/capture files:

- Static: `65c4c300cf8286fb7b3003919600bb591983cf79f46b08167ba9c10466415f1c`
- Shared: `233e342d7f9db59ac5809b51c153dfcb6ac63515740f1459a4eb7e43fcee32d1`

The first bounded hypothesis is to retain five invariant state bindings inside
each render encoder. The old path issues them before every draw. A follow-up
A/B/B/A workload on each runner compares the original and per-pass paths with
identical scene, build, device and allocations, and requires post-block pixel
equivalence. Draw batching and concurrent upload rings remain further A06 work.
