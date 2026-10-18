# D3D12 device startup — executable identity and journal flushes

Diagnostic source: `0bbe8338b8a99f3f61f789cb7b29cbc294c14f97`, 2026-10-18.
The production renderer is byte-for-byte unchanged from `998a12c`.
[Local receipt, source snapshots and all 42 additional native tasks](d3d12-device-cache-local.json).
[Exact-source native CI](d3d12-device-cache-ci.json).
The earlier [58-start comparison](d3d12-startup-report.md) remains unchanged.

**Long device-creation samples now have a concrete cache-file observation.**
In a fixed local experiment, byte-identical executable copies at new paths
block inside `D3D12CreateDevice` while flushing OS shader-cache journal files.
This is additional evidence, not a production latency fix, a universal cold-start
measurement or a retrospective explanation of NexaCloud's 381/838-ms findings.
R01's overall startup acceptance remains open.

## Device work and rejected adapter change

Initial external samples of the owned main thread locate certificate-related
`imagehlp`/`CryptQueryObject` paths and locale enumeration below `nvldumdx`
inside device creation. Names without private PDBs are nearest exports plus
offsets; large displacements do not identify private functions. Some stacks
are truncated or unresolved. No symbol downloads were enabled.

A separate one-executable ABBAABBA experiment passes either NULL or an explicit
`EnumAdapters(0)` result to the original device call. Both modes use the same
binary, actual adapter LUID and four PSO bytecode keys. Enumeration stays inside
the full startup observation. All four values per variant/adapter are retained.

| Median, ms | Hardware NULL → explicit | WARP NULL → explicit |
| --- | ---: | ---: |
| Device creation, including enumeration | 304.307 → 311.953 | 15.363 → 16.105 |
| Process launch → composed input acknowledgment | 603.964 → 586.393 | 591.467 → 595.827 |

The first NULL hardware call took 1,839.678 ms, with a prototype main-thread CPU
counter of359.375 ms. It remains in the distribution. The experiment supplies
no justified device-time improvement, so the adapter prototype was **rejected**.
The production adapter/fallback/ownership behavior stays unchanged.
Microsoft documents [NULL as the first enumerated adapter](https://learn.microsoft.com/en-us/windows/win32/api/d3d12/nf-d3d12-d3d12createdevice);
this experiment does not change adapter-selection policy.

## Captured journal flushing

A lower-overhead instruction-pointer sample of the new CPU diagnostic records
a 1,730.925-ms hardware device call and 484.375-ms main-thread CPU accounting.
Of 109 complete samples inside that call, 70 have the leaf
`ntdll!NtFlushBuffersFile+20`. The sum of its sampling suspension intervals is
24.432 ms. Sample counts are not exact syscall durations, and wall time minus
CPU time is not an exact wait measurement.

The next fixed experiment copies exactly the same PE bytes to three new paths.
Its six-process order is written before execution. On the first observed flush,
the sampler takes a stack and duplicates the file handle from the owned child's
captured x64 syscall context. It queries that duplicate's filename after resuming
the child, then closes it. No cache contents are read or modified by the sampler.
All captured leaves resolve to the expected NtFlush export plus20; the caller is
`KERNELBASE!FlushFileBuffers`. The next frame identifies the cache module.

| Fixed order | Device wall, ms | Main-thread CPU, ms | Captured flush file/module |
| --- | ---: | ---: | --- |
| New copy A, hardware | 2095.287 | 328.125 | D3DSCache journal / D3DSCache.dll |
| Existing original path, hardware | 314.740 | 296.875 | NVIDIA DXCache `.nvph` / NvMemMapStoragex.dll |
| New copy B, hardware | 2193.538 | 421.875 | D3DSCache journal / D3DSCache.dll |
| Existing original path, hardware | 379.670 | 296.875 | NVIDIA DXCache `.nvph` / NvMemMapStoragex.dll |
| New copy C, WARP | 1105.169 | 15.625 | D3DSCache journal / D3DSCache.dll |
| Same copy C, WARP again | 17.737 | 15.625 | No flush captured |

The journals are under `%LOCALAPPDATA%\D3DSCache\<identity>\*.dxcache-journal`.
Hardware A and B have different identity directories; WARP C has a third.
The subsequent NVIDIA file is under `%LOCALAPPDATA%\NVIDIA\DXCache`.
The public receipt replaces the local user path with that environment placeholder;
original filename snapshots and raw logs remain locally hash-bound. The first
hardware journal stacks stop after three frames; the WARP stack reaches
`ShaderCache_Create` and D3D12Core. No complete hardware unwind is claimed.

Microsoft's [shader-cache specification](https://microsoft.github.io/DirectX-Specs/d3d/ShaderCache.html)
describes caches tied to the creating executable. The observed new-path/cached-path
difference is consistent with that design. The experiment does not identify the
complete internal identity key, nor claim that a fresh pathname creates globally
cold driver/OS caches. All copies retain the same binary hash and shader bytes.

This host's system C: volume is on an HGST HTS725050A7E630 HDD; project D: is on
an MTFDDAK256TDL SSD, both reported through RAID. These are read-only storage
queries, not proof of the lower-layer cause of the long flush. D3DSCache.dll is
10.0.26100.8117, D3D12Core.dll10.0.26100.9549, Windows10.0.26200, SDK10.0.26100,
MSVC19.38.33145 Release/O2/MD/C17, XeonW-2123, RTX2080Ti driver32.0.16.1088.
Hardware and WARP use1280×720/1× and1,326 first-frame commands.

## Delivered diagnostic and actual validation

`nexaui_d3d12_startup_bench` now adds `thread_cpu_ms`, `process_cpu_ms` and
`cpu_error` to every captured API span. The CPU bracket includes diagnostic
bookkeeping and is separate from the existing QPC wall/API clocks. Parent
launch→READY still includes all preparation and counter-reading overhead.
[Thread accounting](https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-getthreadtimes)
and [process accounting](https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-getprocesstimes)
use100-ns units, which do not promise that measurement precision. Locally the
values advance in coarse increments; a short call can report zero. Process CPU
sums all threads and can exceed elapsed wall time.

A failed counter query or a backwards counter emits a nonzero `cpu_error` and
both CPU values as-1. Rendering and the original HRESULT remain independent.
No production renderer, public API, cache policy, allocation or worker changed.

Eight native metadata tasks pass: static and shared Release on hardware/WARP,
the historical runtime-HLSL renderer on both adapters, and two independent
thread/process-counter failures. Every task presents, waits for DWM, routes
input only to its owned HWND and acknowledges the resulting composed Files page.
The failure controls still complete rendering and emit error5/-1 for every
span; unavailable counters never masquerade as zero CPU work. The old renderer
records14 spans including its eight source compilations; the embedded renderer
records six. Both are accepted by the existing profiling tool.

[CI run39095248146](https://github.com/Lulus792/NexaUI/actions/runs/39095248146)
passes all seven required jobs on0bbe833: Windows static/shared each110/110,
macOS each93/93, Ubuntu static/shared and sanitizer each107 passed +7 Wayland
skips. Both Windows jobs build the extended startup diagnostic and compare all
eight embedded blobs with the system compiler. Optional Windows-ASan/fuzz jobs
are skipped. Both package-consumer forms build/link; only installed consumers
execute. CI is functional evidence, not startup-speed or physical acceptance.

The42 additional tasks include the four initial samples whose pre-loader
symbol invasion fails with0x8000000D. Those keep valid instruction-pointer
contexts but unresolved symbols; they do not count as successful stack unwinds.
The corrected sampler registers without invasion and refreshes modules after
resuming. All first/negative tasks, source snapshots, protocols, normal frames,
private-byte observations and raw hashes remain in the receipt. Sampling is
intrusive and does not establish a production speedup. External application
builds were observed later in this session; no global idle-host condition is
asserted for the entire investigation.
Four obsolete manual-link objects and three redundant executable copies were
removed (1,854,461 bytes). Distinct hash-verified frozen executables, sources,
raw measurements, historical images and the shared WSL disk remain protected.

## Reproduction and remaining acceptance

Build the diagnostic and use the existing process-boundary profiler:

```powershell
cmake -S . -B build -DNEXAUI_BUILD_BENCHMARKS=ON
cmake --build build --config Release --target nexaui_d3d12_startup_bench --parallel 4
python tools/profile_d3d12_startup.py --before C:/frozen/before.exe --after build/Release/nexaui_d3d12_startup_bench.exe --font C:/Windows/Fonts/segoeui.ttf --output out/new-startup-run --blocks 2
```

The JSON's `reproduction_source_blobs` preserve the local sampler, adapter
control, build scripts, fault controls and runners as UTF-8 text. Both original
file hashes and normalized UTF-8/LF text hashes are recorded. They use the
recorded `D:/NexaUI` checkout and local MSVC environment. Restore the sources in
a separate checkout; select a **new** output directory in each runner before
executing it. Historical sampler variants must be copied to the build script's
`out/r01-device-stack.c` input when reproducing that variant. The old renderer
for the baseline control must first be extracted byte-preservingly from55e8f1b
to its recorded snapshot path. A rebuild can change executable/cache identity;
retain every first result instead of treating it as pre-warmed. The sampler can launch
only a new owned child; it does not attach to existing applications. Raw logs
remain under `out/r01-device-*`, `out/r01-adapter-control` and `out/r01-cpu-metadata`.

**R01 remains open.** This locates synchronous journal flushing in new local
device outliers; it does not identify the kernel/storage/filter contribution or
explain every earlier application/PSO outlier. No kernel storage trace was
collected with the current unelevated token. Microsoft limits
[ShaderCacheControl](https://learn.microsoft.com/en-us/windows/win32/api/d3d12/nf-d3d12-id3d12device9-shadercachecontrol)
to developer mode; it is not a delivered runtime fix. Caches were not purged,
disabled or relocated, and no OS/security setting was changed.

The next investigation should correlate the journal flush with storage/filter
events and distinguish fresh-executable cache initialization from later PSO work,
while preserving the full first usable-frame boundary. The original381/838-ms,
4-GiB interaction, physical display/IME and assistive acceptance remain open.
NexaCloud reviews pin adoption separately; its sources/pin and driveE: are unchanged.
