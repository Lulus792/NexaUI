# Ordered batching with two-pass preparation at bbf3474

Source: [run 38788168764](https://github.com/Lulus792/NexaUI/actions/runs/38788168764),
revision `bbf3474862ab176ecb28ee4f11ef2e8a8cef9cdc`. All six desktop core
variants and Linux sanitizers pass. Both native macOS variants pass 81/81 tests,
including actual MSL compilation, complete GPU/reference images, 10000 ordered
overlapping records in one draw, storage allocation rollback/reuse and source
release during pending GPU work.

The follow-up removes repeated source traversal and batch validation from the
[first batching implementation](../metal-c31c92e/README.md). Preparation counts
visible records and validates atlas mappings together, then fills owned storage
in a second traversal. The already validated group iterator is reused. This
changes CPU preparation; the 116-byte GPU record layout and draw order remain
unchanged. Capacity failure still leaves caller output untouched, and failed
growth retains the renderer's previous buffer.

Environment: macOS 26 ARM64, AppleClang 21, Apple Paravirtual device, Debug,
Metal debug layer requested, Arial, 1× backing. Each workload runs
unbatched/batched/batched/unbatched on the same device and scene, with four
warmup frames and 32 measured frames per block. Both modes retain invariant
bindings per pass. Each CSV concatenates the four original blocks with one
header, preserving mode names, submission serials and numeric bytes. There are
64 samples per mode/workload/variant and 768 measured frames overall.

All 24 mode/block GPU captures match headless, each other across static/shared
variants and the corresponding pre-batching `f870b29` capture byte-for-byte.
Input commands, visible records, render passes, history-copy boundaries and
bitmap upload bytes remain constant between modes. Capacities remain stable
after warmup. Draw-call counts are unchanged from the first implementation:

| Workload | Visible records | Unbatched calls | Batched calls | Record upload bytes |
| --- | --- | --- | --- | --- |
| Overview 100% (1024×642) | 343 | 343 | 28 | 39788 |
| Files 100% (1024×642) | 396 | 396 | 30 | 45936 |
| Files 200% (820×645) | 366 | 366 | 9 | 42456 |

The table below sums CPU preparation and encoding **per sample before**
computing the pooled median. Values are milliseconds, rounded to three decimal
places. The improvement is relative to the unbatched mode in this same run;
separate CI runs have different scheduling and cannot isolate the preparation
change by subtracting their timings.

| Variant | Workload | Unbatched prepare+encode | Batched prepare+encode | Reduction |
| --- | --- | --- | --- | --- |
| Static | Overview 100% | 2.847 | 2.643 | 7.1% |
| Static | Files 100% | 2.395 | 1.959 | 18.2% |
| Static | Files 200% | 1.748 | 0.975 | 44.3% |
| Shared | Overview 100% | 3.759 | 2.555 | 32.0% |
| Shared | Files 100% | 4.338 | 2.483 | 42.8% |
| Shared | Files 200% | 3.983 | 1.601 | 59.8% |

These CPU medians improve in all six comparisons, but block-to-block drift is
substantial. For example, static overview unbatched block medians are 3.187
and 2.659 ms; batched medians are 2.727 and 2.643 ms. The small pooled static
overview gain is not strong evidence of a repeatable hardware improvement.

Whole-call wall time includes retirement, drawable pacing, driver work and
scheduling. It does not improve uniformly. P95 is nearest rank, sorted index
60 of the 64 pooled samples:

| Variant | Workload | Unbatched wall median / p95 | Batched wall median / p95 |
| --- | --- | --- | --- |
| Static | Overview 100% | 18.627 / 41.907 | 18.739 / 43.052 |
| Static | Files 100% | 18.309 / 24.246 | 17.085 / 21.120 |
| Static | Files 200% | 16.616 / 19.649 | 16.588 / 29.649 |
| Shared | Overview 100% | 21.304 / 47.967 | 19.728 / 31.214 |
| Shared | Files 100% | 23.273 / 36.630 | 19.084 / 27.553 |
| Shared | Files 200% | 16.958 / 32.819 | 16.690 / 40.149 |

This is a bounded virtual-device CPU comparison, not an FPS or physical Release
acceptance result. CPU intervals include descheduling. Payload counters measure
one-way authored copies, not physical GPU traffic. GPU timestamps, concurrent
upload rings, physical HiDPI/input and assistive acceptance remain open.

Artifact ZIP SHA-256, checked before reading named members:

- Static: `7169f3f29bc7701cf9a6b5b4a44120995382051ad0a97778a38e637b06b75bc1`
- Shared: `a3edbbcfe6ef7ae8ec95d68c70a455ada489a4bded6e2fdc193103a7fc8e62b7`
