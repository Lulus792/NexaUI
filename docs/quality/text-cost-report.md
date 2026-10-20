# Text construction and repeated draw costs (R03)

Framework source: `2f4199e86ccd4bee7dcd7cdc04383343998163f5`, 2026-10-20.
Baseline: `0739c4959430ec55fc336ae78f554f3857ad5c37`.
The production change adds 15 lines in `src/text_layout.c`; public headers and
the separate NexaCloud source/pin are unchanged.

## Finding and bounded change

The first executable profile separates layout construction from drawing an
already constructed layout. Shaping dominates the six measured constructor
cases; it includes the more expensive GSUB and GPOS work. A paragraph that
occupies exactly one line covering the complete input was shaped twice with
the same bytes, direction, font collection and ppem.

`nx_tl_reshape_lines` now retains the first shaped glyphs for that complete
single-line case. Their resolved attachments and font identities remain in
the existing layout. Trailing-space advances were already zeroed by
`nx_tl_add_line`; width is recomputed in the same summation order as the old
reshape path to preserve exact measurements. New soft/hard line boundaries
retain their original reshaping. The later positioning/caret work still runs.
There is no additional retained allocation or public API.

The measured work count falls from 24 to 12 shaper calls per 12 constructors
for the four unwrapped cases. The two wrapped controls retain 36 and 48 calls
respectively. The new work-count regression fails against the actual old
implementation and passes against the change, without a time threshold.

## Reproducer and timing boundary

The committed [benchmark instructions](../../benchmarks/README.md) build
`nexaui_text_cost_bench` with a private instrumented copy of the real core draw
function. The preparation tool checks each insertion boundary, retains source
hashes, rejects an existing output directory and checks the baseline's private
layout structure before direct paired rendering. Installed builds receive no
profiling calls. Real fonts remain local borrowed inputs.

Each string uses the fixed current/baseline/baseline/current order twice in
one process, with 12 constructor calls per batch and destruction outside the
constructor timer. One context then records eight repeated frames per method,
density and clip condition. Normal and explicit scaled draws use densities
1, 1.25, 1.5 and 2, at a fractional origin, both visible and fully clipped.
Complete RGBA comparisons check normal/scaled equality and equality to the
old layout. Visible samples must contain ink; fully clipped ones must be clear.

Every final run contains 48 constructor rows and 96 draw rows: 576 constructors,
768 timed draw calls, 48 additional old-layout comparison frames and 96 complete
image comparisons. File loading, image composition and comparison are outside
the timed operations. All rows and earlier exploratory runs remain retained.

Host: Xeon W-2123, 4 cores/8 threads, Windows 10.0.26200, about 31.7 GiB usable
RAM; Linux uses the shared Alpine-Dev/Alpine3.24.2 WSL environment on that host.
The final Windows runs are sequential, after owned builds/tests completed.
Unrelated host work can still affect scheduling. An earlier contended pilot is
preserved separately and does not supply the table below.

- Windows Release: MSVC19.38, `/O2 /Ob2 /MD /std:c17 /W4 /utf-8`.
- Windows Debug-ASan: `/Od /Ob0 /Zi /MD /fsanitize=address /std:c17 /W4 /utf-8`,
  `/DEBUG /INCREMENTAL:NO`; AddressSanitizer actually enabled for the library
  and diagnostic. Windows ASan supplies no LeakSanitizer claim.
- Linux Debug: Clang22.1.3, C17, `-g -Wall -Wextra -Wpedantic`,
  `-fsanitize=address,undefined -fno-omit-frame-pointer`, leak detection enabled.
  The generated core reports three pre-existing aggregate-initializer warnings;
  there are no new benchmark warnings or suppressed diagnostics.

The [local receipt](text-cost-local.json) records exact commands, flags,
source/binary/font/log hashes, full CSV rows, distributions, counters and scope.

## Actual paired constructor results

Values are milliseconds per constructor: the median of four 12-call batches
per variant. Min/max and every raw batch remain in the receipt. These are
component costs with instrumentation, not complete application frame/start times.

| String/case | Windows Release before→after | Windows Debug-ASan before→after | Linux Debug ASan/UBSan before→after |
| --- | ---: | ---: | ---: |
| `Bergsee.jpg` | 0.390→0.210 | 11.178→6.141 | 19.328→11.345 |
| Search prompt | 0.636→0.265 | 12.440→6.184 | 20.679→10.118 |
| German transfer/size label | 0.481→0.297 | 14.591→8.345 | 21.034→11.030 |
| Wrapped file/activity text | 0.909→0.792 | 22.914→23.075 | 32.302→34.455 |
| Mixed German/Arabic/Hebrew | 1.896→0.926 | 92.852→47.602 | 40.035→22.314 |
| Longer wrapped paragraph | 1.201→1.267 | 39.027→36.258 | 32.638→39.064 |

The complete single-line cases show reduced work and lower medians. The wrapped
controls move in both directions, including a slower Linux median; they supply
no general acceleration claim. Their shaping work is unchanged. These samples
do not establish a cause for that variation or an end-to-end NexaCloud gain.

## Repeated drawing and memory

The draw implementation still creates per-font caches for each call, gets/rasterizes
masks, copies them into frame-owned buffers and destroys those local caches.
Switching between normal and explicit scaled drawing is not this optimization.
At 2× density for visible `Bergsee.jpg`, the actual per-call means are:

| Configuration | Total draw ms | Cache get ms | Raster ms, nested in get | Mask allocation/copy ms | Placement/command ms |
| --- | ---: | ---: | ---: | ---: | ---: |
| Windows Release | 0.271 | 0.257 | 0.221 | 0.007 | 0.001 |
| Windows Debug-ASan | 2.866 | 2.717 | 2.161 | 0.064 | 0.027 |
| Linux Debug ASan/UBSan | 3.832 | 3.733 | 3.436 | 0.035 | 0.020 |

Each call copies 3,165 mask bytes, with 8 cache misses and 3 hits. The long
paragraph copies 27,180 bytes per 2× call; all cache-create/destroy costs,
allocation counts and density/clip variants are retained. A fully clipped
call still does this glyph work in the current implementation. The benchmark
keeps that behavior and verifies its empty output.

Cache-get time includes raster time; shaping time includes GSUB/GPOS time.
Do not add those nested measurements. Mask allocation/copy and placement/command
are disjoint draw intervals. Their clock/accounting overhead remains in the
measured program. The context allocator's maximum observed live request is
84,603 bytes, with zero tracked live bytes after context destruction. This
counter excludes font/layout allocations, font-owned scratch, allocator metadata
and runtime memory. Per-font cache budget is 65,536 bytes in the draw matrix.
The production change adds no cache or changed bitmap lifetime/budget/rollback.

## Correctness, native input and negative evidence

The retained old/current real-font differential uses Segoe UI, Arial and Calibri,
three directions, three sizes, narrow/wide wraps, hard breaks, spaces, combining
marks, bidi controls and emoji sequences. In both Windows Debug-ASan and Linux
ASan/UBSan/LSan it passes 891 status comparisons: 693 successful layouts have
identical line/glyph/font/position/caret fields, grapheme boundaries, hit tests
and suffix selections; 198 cases return the same error. Another 100 complete
RGBA pairs match at the four densities. The earlier Release run also passed.
The 198 error pairs are not counted as successful layouts or script-conformance
evidence.

A separate actual three-font fallback chain (Calibri→Arial→Segoe UI) selects
U+061D from font1 and U+07C0 from font2, asserting nonzero glyphs and both font
indices. Windows and Linux sanitized runs each pass all 27 direction/size/wrap
cases, all layout/caret/hit/selection comparisons, and 12 complete RGBA pairs.
The first exploratory mixed `AV/ffi/combining-mark` chain returned an error in
both versions across its 27 cases and failed the positive-coverage assertion;
that source and failure remain preserved. The valid simpler chain adds actual
fallback coverage without claiming that earlier error was repaired.

Windows Release passes the full115/115 CTest suite, including native input,
UIA, editor/selection, shaping and GPU examples. DLL layout/work/context-density
checks pass3/3 after correcting a test-only link dependency: the in-test layout
TU needs its private UCD classifiers, which the DLL does not export. No public
export or assertion was changed. Linux's full private-Weston native suite
passes117/117 with ASan/UBSan/LSan and Vulkan synchronization validation, with
all seven required native Wayland cases executed and no skips. Existing tests
retain their time limits and error assertions. These are automated native/input
checks, not physical-device or human assistive acceptance.

## CI and remaining limits

[CI39473369268](https://github.com/Lulus792/NexaUI/actions/runs/39473369268),
exact source2f4199e, passes all six linkage jobs: Windows115/115 each,
macOS96/96 each and Ubuntu117/117 each. The new work-count test passes in every
variant and in the sanitizer job. The **whole CI run is not green**: Linux
sanitizers pass103/117 and fail14 Vulkan cases on LeakSanitizer reports.
The same class was already present in the unchanged-source documentation run
before the text change. Optional Windows-ASan/fuzz jobs are skipped on this
push and are not counted as successful checks. Full results and raw-log hashes
are in the [CI receipt](text-cost-ci.json).

The [separate module investigation](vulkan-lsan-module-report.md) retains those
red runs, standalone Vulkan control and explicit-validation on/off diagnostics.
Neither leak detection nor validation was disabled to obtain a passing matrix.
The local Linux117/117 result and positive text checks do not resolve that
Ubuntu driver/lifetime observation.

R03 supplies a measured, bounded constructor improvement and its regression
handoff. NexaCloud's canonical425fb036 pin, source and app timeout findings
remain its separate adoption responsibility. No claim is made for full app
navigation speed, general wrapped-text acceleration, complete font/script
support, physical/mixed-DPI or human screenreader acceptance. R01's full visible,
operable READY startup target and missing lower-layer profiling privilege stay
open; no duplicate startup campaign or OS/cache/security change was performed.
