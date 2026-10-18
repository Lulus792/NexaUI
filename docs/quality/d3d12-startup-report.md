# D3D12 first-frame preparation — R01 partial result

Source: `998a12c15fe07ca1159b79309e6d8f12f7f3ad67`, 2026-10-18.
[Local commands, all observations and hashes](d3d12-startup-local.json),
[exact-source CI](d3d12-startup-ci.json).

**Eight runtime HLSL-to-DXBC compilations have been removed with unchanged
bytecode. Overall startup latency acceptance remains open.** The final comparison
of the actual two executables does not establish a faster total start. The
historical NexaCloud 381/838-ms and 4-GiB interaction findings remain open.
No NexaCloud sources, dependency pin or drive E: were changed.

## Delivered component

The four solid/composite/shape/bitmap shader sources now live in
`renderers/d3d12/shaders/*.hlsl`. The generator calls the same
`D3DCompile(flags1=0, flags2=0)`, source names, entries and Shader Model 5.0 profiles
as the prior renderer, and embeds all eight resulting DXBC blobs. The verifier
compares every byte with the system compiler on Windows; portable checks validate
the authored-source hashes, DXBC structure and embedded hashes. LF checkout rules
preserve the authored input bytes. Controlled stale-source, changed-pixel-shader
and corrupted-DXBC cases are rejected.

The renderer directly borrows the immutable arrays for `CreateGraphicsPipelineState`.
Ordinary builds require no HLSL source files or source compiler at runtime.
The native Release executable's PE imports no longer include D3DCompiler_47.
Generation, compiler comparison and historical startup diagnostics still use it
as a development tool. Root signatures and GPU-specific PSOs remain lazy and
fallible; the driver may still compile machine code during PSO creation/use.

The eight arrays total **9,980 bytes of static readonly data per linked backend
module**, replacing 3,918 bytes of authored source strings including terminators. The observed
PE readonly section grows by 8 KiB. There is no new frame allocation, production
thread, disk cache or cache callback. Existing window-thread ownership, GPU
fences, PSO/root release, bitmap staging and texture-pool limits remain unchanged.
Driver/device allocation sizes are not bounded by the static-bytecode figure.

## Measurement boundary and first observations

The committed startup diagnostic measures entry, font load, window creation,
renderer creation, first UI build, first render/Present and `DwmFlush`. It then
posts input only to its owned HWND, requires the actual Files selection, presents
the resulting page and waits for DWM again before emitting READY. The parent
measures process launch through that marker, including loading and all preparation.
Phase and API-span timings use QPC wall clocks; they are not thread CPU accounting.
Windows documents [DwmFlush](https://learn.microsoft.com/en-us/windows/win32/api/dwmapi/nf-dwmapi-dwmflush)
as waiting for the calling application's queued DirectX changes to reach the screen.
This is an OS/compositor and input-acknowledgment boundary, not a human reaction or
physical scanout clock.

All observations use the framework example, Segoe UI, 1280×720 at 1× and 1,326
first-frame commands; three frames are submitted by READY. The original NexaCloud
application had 496 commands and different application state. Its old timings
are retained separately, not inferred from this sample.

Local host: Windows (10.0.26200) x64, SDK10.0.26100, MSVC19.38 Release/O2/MD/C17,
Xeon W-2123, RTX2080Ti driver32.0.16.1088. Hardware and forced WARP are separate.
The source compiler is D3DCompiler_47 10.0.26100.9711. System/driver caches were
not purged or disabled; no universal cold-cache condition is claimed.

| First campaign process | Parent launch→READY | Renderer creation | First render | Observed PSO calls |
| --- | ---: | ---: | ---: | --- |
| Original source, hardware | 2879.546 ms | 2512.069 ms | 172.000 ms | 9.410 /25.773 /27.072 /17.289 ms |
| Original source, WARP | 978.908 ms | 81.284 ms | 578.540 ms | 0.826 /1.441 /1.600 /1.753 ms |
| Embedded source, hardware | 675.890 ms | 406.154 ms | 55.631 ms | 0.952 /0.928 /0.795 /0.703 ms |
| Embedded source, WARP | 688.429 ms | 58.733 ms | 270.312 ms | 1.312 /1.260 /1.510 /1.364 ms |

PSO times are listed in solid/composite/shape/bitmap order.
These are first observations, **not a causal before/after speedup**. In particular,
the initial 2.512-second creation phase had no individual creation-API spans;
its full cause remains unproven. Later spans locate substantial variable time
inside `D3D12CreateDevice`, without proving its internal cause. Original-source
processes spend a measured 43.639–92.580 ms in eight source-compiler calls in the
initial/pilot observations. Embedded processes make zero such calls.

## All planned comparisons retained

Every ABBAABBA campaign keeps four observations per variant/adapter, including
the first slow processes. No favorable rerun or warm-up exclusion changes a
campaign. There are 58 native startup tasks in the local receipt: the initial
and cache-pilot observations, an early paired probe, the final public diagnostic
and an additional controlled comparison.

| Campaign, median ms | Hardware launch→READY | WARP launch→READY | Hardware first render | WARP first render |
| --- | --- | --- | --- | --- |
| Early diagnostic: runtime→embedded | 649.163→583.814 | 655.802→608.504 | 99.191→51.171 | 319.126→264.440 |
| Final actual executables: before→after | **912.770→1257.501** | **748.621→753.905** | 117.142→85.333 | 347.124→320.808 |
| Same-image diagnostic: runtime→DXBC replay | 975.801→841.913 | 920.955→769.013 | 133.785→64.497 | 425.348→337.561 |

The final actual-executable comparison contradicts a general startup-win claim.
Its hardware `CreateDevice` spans range from 359.798 to1704.338 ms; font/window/UI
phases also vary. Both binaries remain in the frozen evidence, with their exact
source, compiler/build configuration and complete startup phases. A green task
status in the profiling JSON means the input/composition workflow completed,
not that a speed threshold passed.

The extra control uses one executable and identical imports for both modes.
Its diagnostic wrapper either calls the original compiler or copies the verified
DXBC into ordinary SDK blobs. All four PSO bytecode keys match across the
variants. This supports the benefit of eliminating compiler work while controlling
executable identity; it is a separate experiment, not the shipped readonly-array
path, and does not override the final production-pair counterexample.

A preliminary explicit `GetCachedBlob` pilot successfully reused all four PSOs
on both adapters. It did not justify a startup-speed claim: later no-cache controls
were faster than the reuse runs. Export also incurred work. No production PSO
cache was introduced. Microsoft describes [cached PSOs](https://learn.microsoft.com/en-us/windows/win32/api/d3d12/ns-d3d12-d3d12_cached_pipeline_state)
as specific to the originating hardware, driver and machine; a future cache needs
its own compatibility, error and lifetime proof.

## Normal frames and memory

The final actual-executable campaign records five subsequent frames per process
(20 samples per group): hardware median8.838→9.442 ms, WARP75.094→75.221 ms.
The ranges overlap; no steady-state speedup is claimed. All first-use preparation
is included in launch→READY. Source-compiler work was removed from runtime,
not moved before its observation timer.

The READY private-byte medians are123,428,864→122,755,072 on hardware and
116,082,688→114,837,504 on WARP. These are process observations, not a memory cap
or general savings guarantee. Complete ranges, peak working sets and warm frames
remain in the receipt. The existing texture pool still retains at most256
textures/64MiB; that is not the total process/GPU allocation limit.

## Correctness and failure execution

Unchanged exact pixel, clipping, straight-alpha, text-density and bitmap checks
pass on local hardware and forced WARP. The new independent call-injection test
compiles the exact backend against real D3D12 objects. For each of the four
constructors it exercises root creation failure, PSO creation failure and actual
SDK rejection of truncated shader bytecode, followed by successful retry and
idempotent reuse. Backend-owned root/PSO references balance on failure/destruction.
Actual `ID3D12Device5::RemoveDevice` during bitmap PSO creation is followed by
the lost-state response, a partial recovery failure, another recovery and all
four pipelines recreated. This is controlled API removal, not physical GPU removal.

| Executed local configuration | Result |
| --- | --- |
| Windows static Debug full suite | 109 passed +1 clipboard skip;110 registered |
| Windows shared Debug D3D12 cases | 6/6 passed |
| MSVC ASan RelWithDebInfo D3D12 cases | 6/6 passed with actual `/fsanitize=address` and matching runtime |
| Generator counterexamples | Windows4/4; Linux3 passed +1 Windows-compiler skip |

The local clipboard skip remains separate from the earlier unresolved local
clipboard failure. MSVC ASan is not a leak sanitizer or proof about every driver
allocation. Native reference counters cover the backend's explicit root/PSO
references. Historical images, outliers, frozen executables and source remain.
Two obsolete manually linked object files were removed (245,902 bytes); their
executables, sources and raw evidence remain, and build scripts regenerate them.

[CI run39092120287](https://github.com/Lulus792/NexaUI/actions/runs/39092120287)
passes all seven required jobs on998a12c: Windows static/shared each110/110,
macOS each93/93, Ubuntu static/shared and sanitizer each107 passed +7 Wayland
skips (114 registered). Windows also compares all eight blobs with its system
compiler. Other core jobs execute portable integrity/counterexample checks;
the compiler-only negative case is explicitly skipped there. Optional Windows
ASan/fuzz jobs are skipped. Both consumer forms build/link; only the installed
consumer is executed by the workflow. CI supplies no startup-latency acceptance.

## Reproduction and next owner

Use the current probe/core/scene in both builds, changing only the diagnostic's
renderer source for the baseline. `NEXAUI_D3D12_STARTUP_RENDERER_SOURCE` affects
only this diagnostic target, not the normal renderer. Extract the55e8f1b renderer
with a byte-preserving `git show` into an absolute snapshot path, then:

```powershell
cmake -S . -B build-before -DNEXAUI_BUILD_BENCHMARKS=ON "-DNEXAUI_D3D12_STARTUP_RENDERER_SOURCE=C:/your/path/before-renderer.c"
cmake --build build-before --config Release --target nexaui_d3d12_startup_bench --parallel 4
cmake -S . -B build-after -DNEXAUI_BUILD_BENCHMARKS=ON
cmake --build build-after --config Release --target nexaui_d3d12_startup_bench --parallel 4
python tools/profile_d3d12_startup.py --before build-before/Release/nexaui_d3d12_startup_bench.exe --after build-after/Release/nexaui_d3d12_startup_bench.exe --font C:/Windows/Fonts/segoeui.ttf --output out/new-startup-run --blocks 2
```

`--output` must be new. The tool writes its order before starting children,
retains errors, enforces requested hardware/WARP identity and never equates
successful workflow execution with faster startup. Avoid concurrent builds or
profilers when collecting timings. The original full evidence remains under
`out/r01-startup/`, including the negative production-pair comparison and the
diagnostic-only PSO-cache/replay sources.

**R01 stays open:** isolate the remaining device/driver/startup variability and
the original PSO outliers before claiming reliably faster first usable frames.
NexaCloud separately reviews candidate998a12c and its application workload/pin.
Physical monitor/IME/assistive and whole-design acceptance remain P2 work; this
component and its green CI do not complete the broader project goal.
