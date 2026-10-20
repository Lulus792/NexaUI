# Benchmarks

## Text construction and repeated drawing

`nexaui_text_cost_bench` profiles six real-font strings, including mixed scripts
and wrapped text, through one long-lived context. It separates layout
construction, Shaping/GSUB/GPOS, cache creation/get/destruction, frame-owned mask
allocation/copy and placement/command work. Raster time is nested inside cache
get; GSUB/GPOS are nested inside shaping. Do not add these nested totals.
Normal and explicitly scaled drawing are compared at1×/1.25×/1.5×/2×, both
visible and fully clipped. All image comparisons use complete RGBA buffers.

The instrumentation is generated into a new private directory and built only
as a static developer executable. It does not add hooks to the installed
library. From the repository root, using a new build/output directory:

```sh
python tools/prepare_text_cost_profile.py out/new-text-profile \
  --baseline-commit 0739c4959430ec55fc336ae78f554f3857ad5c37
cmake -S . -B out/new-text-build -DBUILD_SHARED_LIBS=OFF \
  -DNEXAUI_BUILD_EXAMPLES=OFF -DNEXAUI_BUILD_TESTS=OFF \
  -DNEXAUI_BUILD_BENCHMARKS=ON -DCMAKE_BUILD_TYPE=Release \
  -DNEXAUI_TEXT_PROFILE_CORE="$PWD/out/new-text-profile/text-cost-core.c" \
  -DNEXAUI_TEXT_LAYOUT_BASELINE_SOURCE="$PWD/out/new-text-profile/text-layout-baseline.c"
cmake --build out/new-text-build --config Release --target nexaui_text_cost_bench
out/new-text-build/nexaui_text_cost_bench /path/to/font.ttf > out/new-text-profile/results.csv
```

On Visual Studio generators, the executable is in `out/new-text-build/Release/`.
The shell example uses POSIX line continuations; pass the same CMake arguments
on one line or use PowerShell continuations on Windows. Fonts are borrowed
local inputs and are not redistributed. For Debug-ASan use matching sanitized
compiler/linker flags for the whole build; Windows needs `/RTC1` removed and
incremental linking disabled.

An optional baseline must have the identical private layout structure, checked
by the preparation tool before direct paired rendering. Omit `--baseline-commit`
and the baseline CMake option for a current-only run. Paired constructors follow
the fixed current/baseline/baseline/current order twice per string,12 calls per
batch. Destruction, file loading and image composition are outside construction
timers. Context allocation/peak counters exclude font/layout allocations and
external runtime memory. Keep complete CSV, exit status, source/binary/font
hashes, flags and host load with any timing claim.

## Other focused workloads

`nexaui_text_stress_bench` measures five fresh-buffer runs of 32 mixed-script
insert/delete pairs at 4 KiB, 32 KiB and 128 KiB and verifies byte-for-byte
restoration. Its dated result and limits are in
`docs/quality/text-fuzz-report.md`.

Build with `-DNEXAUI_BUILD_BENCHMARKS=ON`. `nexaui_layout_bench` measures repeat measure/arrange of one row with 1k, 10k and 100k logical leaves after warmup. `nexaui_interaction_bench` measures frame registration of 1k, 10k and 100k focusable hit regions. Both report wall-clock time, allocator calls during the measured loop and peak owned bytes. `nexaui_series_bench` decimates a one-million-point array into 1920 columns for a full view and a 10k-point viewport. It reports min/median/max of 21 wall-clock calls after three warmups and the number of sample-callback calls.

On Windows, `nexaui_d3d12_bench` renders opaque rectangles, translucent rectangles, repeated images and distinct images through a hidden 640×480 window. Its 21 measured wall-clock samples include command preparation, GPU work and `Present(1)`; translucent and bitmap frames also wait for the frame fence. Private renderer diagnostics print draw/upload counts, copied texels and one latest-completed GPU timestamp sample split into upload and render phases. That one GPU sample is not a distribution. Record CPU model, memory, OS version, build configuration, compiler, adapter and presentation mode alongside any published result. These are focused baselines, not universal frame-time guarantees.
