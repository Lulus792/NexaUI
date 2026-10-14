# Benchmarks

`nexaui_text_stress_bench` measures five fresh-buffer runs of 32 mixed-script
insert/delete pairs at 4 KiB, 32 KiB and 128 KiB and verifies byte-for-byte
restoration. Its dated result and limits are in
`docs/quality/text-fuzz-report.md`.

Build with `-DNEXAUI_BUILD_BENCHMARKS=ON`. `nexaui_layout_bench` measures repeat measure/arrange of one row with 1k, 10k and 100k logical leaves after warmup. `nexaui_interaction_bench` measures frame registration of 1k, 10k and 100k focusable hit regions. Both report wall-clock time, allocator calls during the measured loop and peak owned bytes. `nexaui_series_bench` decimates a one-million-point array into 1920 columns for a full view and a 10k-point viewport. It reports min/median/max of 21 wall-clock calls after three warmups and the number of sample-callback calls.

On Windows, `nexaui_d3d12_bench` renders opaque rectangles, translucent rectangles, repeated images and distinct images through a hidden 640×480 window. Its 21 measured wall-clock samples include command preparation, GPU work and `Present(1)`; translucent and bitmap frames also wait for the frame fence. Private renderer diagnostics print draw/upload counts, copied texels and one latest-completed GPU timestamp sample split into upload and render phases. That one GPU sample is not a distribution. Record CPU model, memory, OS version, build configuration, compiler, adapter and presentation mode alongside any published result. These are focused baselines, not universal frame-time guarantees.
