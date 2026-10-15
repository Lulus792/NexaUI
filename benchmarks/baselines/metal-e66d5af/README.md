# Submission-keyed Metal GPU timing at e66d5af

Source: [run 38792441550](https://github.com/Lulus792/NexaUI/actions/runs/38792441550),
revision `e66d5af5c146d610f5395f00c767d5c4187b100c`. All six desktop core
variants and Linux sanitizers pass. Both macOS variants execute 82/82 tests.
CI requires a Metal device and valid GPU timing, rather than accepting a
timing-unavailable path as native timing evidence.

The private renderer collects `GPUStartTime`/`GPUEndTime` only at successful
terminal retirement, before releasing the command buffer. A fixed journal
retains 64 committed submission IDs with slot/canvas identity and explicit
pending, ready, unavailable or failed states. Invalid/absent pairs are not
reported as measured zero. Equal positive timestamps can represent valid zero
elapsed time. Kernel start/end timestamps are validated independently: their
duration describes CPU driver scheduling, not GPU work.

The native journal test submits 67 frames, checks expiration/wrap and exact IDs,
preserves pending state across retryable idle-collection timeouts, collects the
three-slot tail, and checks recovery reset and wrong-thread rejection. Each
variant reports 64/64 valid retained timings. The gated ring test additionally
proves no GPU timing is published while a shared event blocks actual GPU work.
Both final native logs record 168 submitted / 168 retired frames before the
separate driver test. Portable C tests cover failed work, missing/nonfinite/
reversed/overflowing timestamps, valid zero, stale completions and bounded reuse;
Windows static/shared and Linux ASan/UBSan/leak checks pass.

Environment: macOS 26 ARM64, AppleClang 21, Apple Paravirtual device, Debug,
Metal debug layer requested, Arial, 1× backing. Three workloads run
serial/ring/ring/serial with ordered batching and pass-state reuse enabled in
both modes. Each block warms four frames and measures 32. These CSVs concatenate
the original four blocks with one header, preserving numeric bytes and mode
names. There are 64 pooled samples per mode/workload/variant, 768 overall.

All 768 rows have valid GPU **and** kernel timestamps. Every GPU submission ID
and slot matches its CPU row. All 24 mode/block GPU captures match headless,
both variants and the corresponding pre-batching `f870b29` capture byte-for-byte.
Geometry, upload payloads and draw/history counts remain unchanged; allocations
stay stable after warmup. The workloads are overview 100% (1024×642, 343 records /
28 draws), files 100% (1024×642, 396 / 30), and files 200% (820×645, 366 / 9).

GPU command-buffer elapsed time in milliseconds, rounded to three decimals:

| Variant | Workload | Serial median / p95 | Ring median / p95 |
| --- | --- | --- | --- |
| Static | Overview 100% | 10.296 / 15.166 | 11.920 / 26.234 |
| Static | Files 100% | 12.730 / 28.715 | 12.534 / 20.820 |
| Static | Files 200% | 5.198 / 10.443 | 4.693 / 7.639 |
| Shared | Overview 100% | 5.454 / 7.288 | 5.756 / 8.141 |
| Shared | Files 100% | 5.697 / 7.462 | 6.045 / 7.171 |
| Shared | Files 200% | 3.229 / 4.331 | 3.104 / 6.405 |

P95 is nearest rank, sorted index 60 of 64 pooled samples; each median averages
the two middle samples. CPU driver-scheduling medians across these mode/workload
pairs range from 0.135 to 0.499 ms, separately retained in the CSVs. The static
and shared jobs run on different VMs: their substantial timing differences do
not establish a linkage-performance effect. Block drift and mixed serial/ring
outcomes also preclude a uniform speedup claim.

This is a whole-command-buffer GPU interval covering atlas upload, canvas clear,
composition/history copies and drawable blit. It is not a per-pass breakdown,
busy-only counter or input-to-display latency. The workload has no pixel readback
inside measured command buffers. A timestamp pair uses Metal's system-mach-time
domain; do not subtract it from CPU `CLOCK_MONOTONIC` samples or sum overlapping
CPU/GPU durations as end-to-end latency. GPU results from pending frames can be
unavailable even while the latest CPU submission profile exists.

After each timed block, bounded idle collection drains its remaining frames and
the code joins GPU results by immutable submission ID and slot. This drain,
lookup, sorting, image comparison and file I/O occur outside timed CPU intervals.
Tail drain times are logged separately. Whole-call wall time remains strongly
influenced by scheduling and drawable pacing. For example, static files 100%
changes from 17.636 / 36.687 ms median/p95 to 16.648 / 20.692 ms, while static
files 200% changes from 16.621 / 18.797 to 16.638 / 18.890 ms. Physical GPU,
Release, HiDPI/input and display-latency acceptance remain open.

Artifact ZIP SHA-256, checked before reading named members:

- Static: `eff1c76a092dbb0e2bbd644a30c652467a0fc64faa2383698b7837189a341b30`
- Shared: `7f1bc9afeece599dac4f26cdef50d0e1e07c0239fba969313dc5d86ecd79186c`
