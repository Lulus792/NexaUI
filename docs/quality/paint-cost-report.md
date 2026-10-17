# Paint construction and filled-plot CPU costs — 2026-10-18

The Design-A primitives now avoid repeated row-color evaluation and unnecessary
rounded-coverage calculations. Filled plots retain column spans and shade only
their occupied rectangle. Public APIs, output pixels, allocation counts and
resource lifetimes are unchanged. This is a CPU measurement of the framework;
application adoption and the final NexaCloud visual match remain separate work.

## Paired Release measurements

Baseline: `876b006e24ffbe7a3da926b48a68c6f885228167`. The old `paint.c` and
`nexaui.c` are compiled beside the current sources with isolated symbols, in one
executable per platform. All other linked helpers are shared. Each case alternates
AB/BA order. Constructors have two warmups and 16 measured pairs; plots have four
warmups and 32 measured pairs. Data generation, hashing, snapshots and comparisons
are outside the timed region. Plot timing includes begin/plot/end and therefore
freeing previous frame masks, allocation, decimation, rasterization and recording.

The same Xeon W-2123 host ran Windows and then Alpine-Dev WSL2, without overlapping
local compilation or benchmarks. Windows uses MSVC 19.38 `/O2 /Ob2 /DNDEBUG` and
QPC; Alpine 3.24.2 uses Clang 22.1.3 `-O3 -DNDEBUG` and `CLOCK_MONOTONIC`.
These are separate platform/compiler observations, not an OS ranking.

| Workload | Windows before → current, median ms | Alpine/WSL before → current, median ms |
| --- | ---: | ---: |
| Create vertical gradient, 320×160, 1× | 5.461 → 0.955 | 4.388 → 0.887 |
| Create vertical gradient, 320×160, 2× | 21.025 → 3.888 | 13.714 → 2.662 |
| Create radial disk, 128×128, 2× | 7.304 → 5.537 | 6.079 → 5.614 |
| Create soft card, 640×360, 2×, softness 4 | 121.369 → 62.657 | 75.087 → 49.778 |
| Create small glow, 48×48, 2×, softness 6 | 2.158 → 1.428 | 1.588 → 1.185 |
| Record two filled rate series, 640×280, 1× | 2.250 → 0.827 | 4.004 → 1.104 |
| Same, 2× | 8.506 → 2.404 | 15.537 → 4.084 |
| Disjoint segments with negative values, 2× | 2.197 → 1.731 | 4.293 → 3.430 |
| Empty histories, 2× | 1.260 → 1.276 | 2.610 → 2.646 |
| Baseline-only histories, 2× | 1.407 → 1.396 | 2.917 → 2.922 |
| Sparse histories, 2× | 1.276 → 1.261 | 2.711 → 2.753 |

Empty and sparse cases do not establish an improvement; small median regressions
and noisy tails remain visible in the raw data. There is no universal speedup or
frame-time guarantee. Thirty-two cached paint records cost a median 0.001701 ms
on Windows and 0.000848 ms on Alpine, with zero allocations after warmup. That
measurement excludes rendering and does not mean cached bitmap uploads are free.

[Complete receipt](paint-cost-local.json), [Windows samples](paint-cost-windows.csv),
[Alpine samples](paint-cost-linux.csv): 1,120 measurements, min/median/nearest-rank
p95/max, exact source/executable hashes, allocations, retained and peak bytes.
Host scheduling and filesystem caches were retained; these are warmed costs,
not process startup, cold construction, GPU, presentation or physical-display
measurements. No macOS performance result is claimed.

## Pixel and lifetime evidence

Each platform compares 171 complete constructor RGBA buffers and relative bounds
over solid/linear/radial paints, degenerate vectors, fractional geometry, radii,
shadow offsets and scales .125–8. All plot command geometry/color fields and every
mask byte are compared after each old/new frame pair: 216 pairs per platform.
The 17 before/current timed-case snapshot pairs per platform are also bytewise
equal. Raw RGBA proofs remain in the receipt's local output directories; their
sizes and SHA-256 hashes are published. Checksums alone are not the comparison.

The regular plot regression additionally uses a scalar pixel-center oracle over
20 empty/baseline/positive/negative narrow-gap cases at .125/1/1.5/2/8×. It checks
all RGBA channels, including transparent-gap RGB, and releases all tracked bytes.
Existing allocation-failure, rollback/retry, clipping and native bitmap parity
tests remain required. Exact-source CI is recorded separately after completion.

Local Windows Debug static passes 104/104; shared paint/plot/hardware-D3D12/WARP
passes 4/4. Alpine ASan/UBSan/LSan paint/plot/components/native-Vulkan passes 4/4
with no skips. [Commands and actual outcomes](paint-cost-checks.json).

Construction still allocates twice for surfaces and four times for shadows.
The measured large shadow retains 4,096,480 bytes and peaks at 12,289,280 bytes;
the small glow retains 115,680 and peaks at 346,880 bytes. None of these totals
changed. Each measured two-filled-layer frame still allocates two masks; no
cross-frame mask cache was added. The span tables add 16 KiB of bounded stack
storage per plot painting call, rather than a new heap allocation. Full-image
zeroing remains necessary, so even an empty filled layer has area-dependent cost.

Paint images own their RGBA until destroy. Draw commands borrow it: the caller
must retain resources through renderer consumption. Plot masks belong to the
context until the next begin-frame or context destruction; input samples are
borrowed only during the call. The benchmark keeps both command streams alive
through their full comparison, destroys every resource/context and checks zero
tracked live bytes. The native private drivers stage/copy input pixels before
submit returns; this is not permission to free data before submit. See the
[paint contract](../concepts/paints.md) and [plot contract](../concepts/plotting.md).

## Reproduction

From a compatible checkout, export into a **new** ignored directory:

```powershell
python tools/prepare_paint_baseline.py --revision 876b006 --output-dir out/paint-source-review
cmake -S . -B out/paint-release-review -DNEXAUI_BUILD_BENCHMARKS=ON -DNEXAUI_BUILD_TESTS=OFF -DNEXAUI_BUILD_EXAMPLES=OFF -DNEXAUI_PAINT_BASELINE=D:/NexaUI/out/paint-source-review/paint.c -DNEXAUI_CORE_BASELINE=D:/NexaUI/out/paint-source-review
cmake --build out/paint-release-review --config Release --target nexaui_paint_bench --parallel 4
out/paint-release-review/Release/nexaui_paint_bench.exe out/paint-review.csv out/paint-review-snapshot
```

Adjust absolute paths for another checkout. On Linux use Ninja/Clang with
`-DCMAKE_BUILD_TYPE=Release`, quote baseline paths, and run the binary directly
from the build directory. Baseline extraction records full revision and source
hashes and refuses an existing destination. Omitting baseline options produces
a current-only benchmark; it does not produce a before/after comparison.

An exploratory full-row shading implementation regressed empty/sparse frames
substantially. Its raw `out/paint-profile-span-probe` evidence is retained. The
occupied-rectangle fix restored the zeroing-only path for empty/baseline data.
Earlier runs overlapping Linux compilation are exploratory and excluded from
the published table. The sanitizer all-target build also exposed a missing
`gsub_context.c` dependency in the layout fuzzer; that source list is repaired.
Its 10,000-input campaign completes all inputs but exits 1 on a 56-byte LSan
report in Alpine's libFuzzer runtime. An empty callback linked without NexaUI
reproduces the same direct/indirect allocation stacks in `FuzzerDriver`.
Both negative logs and source are retained in the check receipt. This is not
counted as a successful sanitizer campaign; no detection was disabled or
suppressed. The runtime issue remains outside this patch.

## NexaCloud integration handoff

Prepare replacement gradients/shadows outside frame building; cache by size,
density and style with a bounded aggregate budget. Keep the previous resource
or use a plain theme surface if construction fails. Release replaced resources
only after their recorded frames are consumed. A roughly 50–63 ms warmed large
shadow constructor is unsuitable as per-frame work on this host. Prefer cached
small glows and a bounded resize/rebuild policy; do not infer a first-use budget
from the warmed numbers.

Supply real data in NexaCloud, reuse plot scratch/state, choose a bounded update
cadence, and use `hide_fills` plus plain surfaces for contrast/reduced effects.
The existing limits still cap filled layers at 4,194,304 total pixels and eight
layers; these are resource ceilings, not performance targets. Actual 1:1 app
composition, legend/time labels, physical DPI/contrast/screenreader tasks and
final platform performance acceptance remain open. The NexaCloud dependency
pin is unchanged by this framework work.
