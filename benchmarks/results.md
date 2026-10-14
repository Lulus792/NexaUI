# Recorded local layout baseline

Date: 2026-09-28. Hardware: Intel Xeon W-2123 at 3.60 GHz, 4 cores/8 logical processors, 32 GiB RAM. Software: Windows 11 Pro 10.0.26200, MSVC 19.38, CMake 4.3.2, x64 Release (`/O2 /Ob2 /DNDEBUG`). Scenario: one row with 1k/10k/100k leaf nodes, repeated measure and arrange of a 1920 × 1080 root, 20 iterations after warmup. Timings are wall-clock measurements from one run; they are not GPU or complete UI-frame timings.

| Nodes | Initial implementation | After redistribution fix | Steady-state allocations | Peak layout-owned bytes |
| ---: | ---: | ---: | ---: | ---: |
| 1,000 | 0.057 ms | 0.058 ms | 0 | 176,248 |
| 10,000 | 6.565 ms | 0.642 ms | 0 | 1,760,248 |
| 100,000 | 113.306 ms | 9.685 ms | 0 | 17,600,248 |

The initial implementation repeated full sibling passes to consume floating-point summation residue even when no child reached a size cap. The fix stops after an uncapped pass. This baseline describes only this uniform layout case. Nested layouts, capped weights, text, command generation, GPU submission and virtualization require separate benchmarks.

## Interaction registry baseline

Same hardware and Release configuration, one local run on 2026-09-28. Each measured frame registers 1k/10k/100k focusable regions with stable numeric IDs after two warmup frames. No pointer event or native rendering is included.

| Regions | Frame registration | Steady-state allocations | Peak context-owned bytes |
| ---: | ---: | ---: | ---: |
| 1,000 | 0.037 ms | 0 | 87,808 |
| 10,000 | 0.358 ms | 0 | 971,008 |
| 100,000 | 11.654 ms | 0 | 9,024,768 |

The 100k case is a stress baseline for the hash table and frame swap, not a claim that 100k on-screen widgets would meet a target frame budget. Virtualization and full widget/render costs remain to be measured.

## Time-series decimation baseline

Date: 2026-10-13. Hardware: Intel Xeon W-2123 at 3.60 GHz, 4 cores/8 logical processors, 32 GiB RAM. Software: Windows 11 Pro 10.0.26200, MSVC 19.38 (`_MSC_VER=1938`), CMake 4.3.2, x64 Release (`/O2 /Ob2 /DNDEBUG`). Source: a one-million-point, 16 MiB array with integer x coordinates, periodic y values and one high spike. The callback copies one point by index. Both cases produce 1920 occupied columns. Each result is 21 individual wall-clock calls after three warmups, sorted to report min/median/max. This is one local run and excludes data generation, widget drawing, GPU work and presentation.

| Viewport | Source callback calls per decimation | Min | Median | Max |
| --- | ---: | ---: | ---: | ---: |
| Full million points | 1,000,039 | 8.563 ms | 8.824 ms | 9.367 ms |
| 10,000 visible points | 10,039 | 0.091 ms | 0.134 ms | 0.311 ms |

The narrow view reads its 10,000 visible points plus 39 points used by the two binary searches. It does not copy or scan the remaining 990,000 points. The full view remains a linear scan; these timings are not full plot-frame times or universal latency guarantees.

## D3D12 local renderer baseline

Date: 2026-10-14. Hardware: Intel Xeon W-2123 at 3.60 GHz, 4 cores/8 logical processors, 32 GiB RAM. Software: Windows 11 Pro 10.0.26200, MSVC 19.38, x64 Release. The hidden Win32 client is 640×480 and uses the default D3D12 adapter with `Present(1)`. Each scene has 5 warmups and 21 measured frames. Numbers are min/median/max end-to-end wall times from one local run, including command staging, GPU submission, vsync presentation and the serial fence wait. They are not GPU timestamps.

| Scene | Commands | Draw calls | Bitmap uploads | Min | Median | Max |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| Opaque rectangles | 1,000 | 16 | 0 | 3.595 ms | 4.155 ms | 4.684 ms |
| Half-alpha rectangles | 128 | 128 | 0 | 2.966 ms | 4.189 ms | 5.104 ms |
| Repeated RGBA8 view | 128 | 128 | 1 | 3.208 ms | 4.121 ms | 5.035 ms |
| Distinct RGBA8 views | 128 | 128 | 128 | 3.155 ms | 4.153 ms | 5.148 ms |

Before shared staging and texture reuse, one run of the repeated-view scene measured 162.963 ms median (157.384–195.899 ms), with a new texture, upload buffer and descriptor heap for every command. The newer scene stages the view once and reuses allocated storage. These runs do not isolate individual changes or establish a speedup on other hardware. The roughly 4 ms medians are strongly affected by vsync and the fence wait; high-load translucent scenes and GPU-only time require separate measurement. The benchmark source is [`d3d12_bench.c`](d3d12_bench.c).

After adding three frame slots and allowing two opaque or empty frames in flight, a new run on the same system measured 4.162 ms median for 1,000 opaque rectangles (16 draw calls), 4.167 ms for 128 half-alpha rectangles, 4.119 ms for 128 repeated-view bitmaps, and 4.197 ms for 128 distinct-view bitmaps. Opaque/empty frames no longer wait for their own fence after Present; all scenes still include VSync and the other scenes still wait for completion. These wall times do not demonstrate a throughput or latency improvement from the frame-slot change; GPU timestamps were not yet available at that stage.

## D3D12 GPU timestamp and clipped-copy probe

Date: 2026-10-14, same local 640×480 Release environment. Three GPU timestamps per frame measure texture upload, rendering and their total on the direct queue. Each entry below is **one latest-completed GPU-frame sample**, not a median. Opaque frames can report an earlier completed frame from the same warmed-up scene. Wall medians use 21 samples after five warmups and include `Present(1)`; the GPU samples exclude presentation and CPU work.

| Scene | Full-buffer-copy GPU sample | Clipped-copy GPU sample | Clipped texels | Clipped-copy wall median |
| --- | ---: | ---: | ---: | ---: |
| 1,000 opaque rectangles | 0.017 ms | 0.018 ms | 0 | 4.147 ms |
| 128 half-alpha rectangles | 1.626 ms | 1.124 ms | 79,389 | 4.167 ms |
| 128 repeated-view RGBA8 images | 1.664 ms | 1.182 ms | 61,909 | 4.153 ms |
| 128 distinct-view RGBA8 images | 1.667 ms | 1.223 ms | 61,909 | 4.163 ms |

The half-alpha scene previously copied 39,321,600 texels with 128 full-window copies. Clipped copies reduce transferred area, while each element still has its own copy, barriers and draw. One-frame GPU samples show the expected direction but are too weak to establish a stable speedup; a distribution of GPU samples is needed before choosing the next compositing optimization.

## D3D12 non-overlapping alpha batch probe

Date: 2026-10-14, same 640×480 x64 Release machine and scenes. The benchmark now collects one GPU total per distinct completed frame observed during 21 measured submissions after five warmups. An asynchronous opaque submission may report a late warmup frame. It reports each distribution's min/median/max; `Present(1)` is included only in the wall measurements. This is one local run, and the earlier clipped-copy column above contains single samples rather than a comparable distribution.

| Scene | Draws | Copied texels | GPU samples | GPU min | GPU median | GPU max | Wall median |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| 1,000 opaque rectangles | 16 | 0 | 21 | 0.015 ms | 0.017 ms | 0.018 ms | 4.155 ms |
| 128 half-alpha rectangles | 11 | 2,682,326 | 21 | 0.141 ms | 0.143 ms | 0.152 ms | 4.152 ms |
| 128 repeated-view RGBA8 images | 128 | 61,909 | 21 | 1.165 ms | 1.172 ms | 2.002 ms | 4.153 ms |
| 128 distinct-view RGBA8 images | 128 | 61,909 | 21 | 1.174 ms | 1.181 ms | 1.218 ms | 4.156 ms |

The alpha batch reduces 128 shape draws to 11, while copying the union of scattered bounds increases copied texels from 79,389 with separate clipped copies to 2,682,326. Its measured GPU time is well below the earlier 1.124 ms single sample, but the runs are not paired distributions and do not establish a general speedup. Wall medians remain near the vsync cadence. Bitmap scenes retain a copy, barriers and draw per image.
