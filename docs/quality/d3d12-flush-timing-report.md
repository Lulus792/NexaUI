# R01: actual cache-file flush boundaries

2026-10-18, source base `1ff63d6b000f65b5b29a0105e4e97e2637cd8c46`
(production code f7ec75b; renderer998a12c unchanged).
[Eight complete process receipts, storage controls and helper sources](d3d12-flush-timing-local.json).
This extends the [private cache-event trace](d3d12-private-cache-trace-report.md).

**The first new hardware process spends1,867.716ms in28 successful file-flush
calls inside its2,491.052ms Device call, about75% of that span.** Four further
flushes cost205.530ms **after READY** and are excluded from startup attribution.
These are contemporaneous API boundaries, not stack-sampling percentages.
R01 overall startup acceptance and the original381/838ms/4GiB findings remain open.

| Fixed order, one EXE | Parent launch→READY, ms | Device, ms | Cache-creation spans, ms | Observed flushes inside Device |
| --- | ---: | ---: | --- | --- |
| Hardware, hook | 2851.817 | 2491.052 | 1409.262 /579.519 | 14+14;1311.594+556.122ms |
| Hardware, control | 650.902 | 338.541 | 3.429 /2.353 | Not observed in control mode |
| Hardware, control | 681.604 | 342.867 | 3.044 /3.163 | Not observed in control mode |
| Hardware, hook | 597.771 | 306.427 | 3.159 /2.406 | 0 |
| WARP, hook | 633.417 | 18.548 | 7.169 | 0 |
| WARP, control | 629.425 | 15.495 | 6.234 | Not observed in control mode |
| WARP, control | 656.931 | 16.562 | 6.109 | Not observed in control mode |
| WARP, hook | 672.663 | 16.270 | 5.696 | 0 |

All first frames contain1,329 commands at1280×720/1×, including R02's three
additional Transfer lines. The earlier pre-R02 trace had1,326 commands; these
are different workloads. Every task presents, waits for DWM, acknowledges its
own HWND input, submits three frames by READY and completes five normal frames.
The same-executable plan preserves its first slow case. It does not supply
independent cold caches or a causal hook/control speed comparison.

## Measured calls and limits

The local helper preloads the system D3DSCache module in both modes. In hook
mode it temporarily replaces only that owned process module's single imported
`FlushFileBuffers` pointer with a diagnostic wrapper. Its target must equal
the genuine Kernel32 or KernelBase export. No system DLL file, other process,
production renderer, cache policy or security setting is changed.

The wrapper calls the real API exactly once with the original handle, returns
its BOOL/last-error, and records QPC boundaries. It queries the same still-open
handle for its filename and storage geometry after the real call, outside the
narrow API timer. This metadata costs4.843ms over the28 Device calls, plus
counter/clock overhead. Parent startup includes preload, probe and trace setup.
The diagnostic is not a production interposition mechanism or a speed fix.

All32 captured calls succeed on the Device-calling thread. The two cache GUIDs
match the private ETW creation events: each has12 WAL, one journal and one cache
file flush during creation. Four further journal/cache flushes occur after READY.
The captured paths lie under `%LOCALAPPDATA%/D3DSCache/`; the prior single sampled
journal handle did not reveal this complete WAL/cache-file distribution.
Each handle reports512-byte logical and4096-byte physical/filesystem atomicity
sectors. Those fields do not identify the source of the delay.

Private ETW uses the same QPC clock. All eight start/enable/disable/stop calls,
event-loss and lost-buffer counts are0; all decoded cache events have the owned
PID and successful TDH metadata. Every import slot is restored, with zero
inflight/incomplete/overflow records. Each process also checks that the wrapper
preserves the real invalid-handle failure/error6. The buffer is fixed at256 records.

Thread/process CPU and WriteTransferCount deltas report zero during these calls.
CPU accounting is coarse; [process I/O counters](https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-getprocessiocounters)
are not physical disk timings. These zero values do not prove no storage work or
measure exact wait time. Microsoft's [FlushFileBuffers contract](https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-flushfilebuffers)
describes flushing a specified file; this helper never flushes a whole volume.

## Separate owned-file control

One fixed C,D,D,C,C,D,D,C sequence writes1KiB and flushes two newly created
regular files, with full final readback verification. C: flush times are
23.044 /31.283 /23.580 /16.909ms; D:0.824 /0.916 /0.732 /0.729ms.
The fresh mapping confirms C: on disk1/HGST HDD and D: on disk0/MTFDDAK SSD.
This supports a difference in local file-flush paths. It does not reproduce the
cache transaction workload or isolate hardware, firmware, filesystem, queueing
and filter contributions. No unbuffered or write-through policy is introduced.
Both owned1KiB files are removed only after their exact expected-byte readback
and hashes are recorded; no pre-existing file or cache entry is deleted.

## Reproduction and next work

The receipt embeds the helper, decoder, build scripts and fixed runners with
source hashes. Build against this exact source base and its Release core/scene
archives. Local commands were:

```powershell
cmake --build build --config Release --target nexaui nexacloud_scene --parallel 4
cmd /c out\build-r01-cache-flush-probe.cmd
python out/run-r01-cache-flush-probe.py
python out/analyze-r01-cache-flush.py
cmd /c out\build-r01-storage-flush.cmd
python out/run-r01-storage-flush.py
```

Restore the embedded sources into an isolated checkout and choose new output
directories. The initial failed setup guard, its EXE, and two overrestrictive
analysis/validation assumptions remain frozen. The initial guard expected the
Kernel32 export only; the API-set import actually resolves to KernelBase. A later
zero-flush process and the four post-READY calls are valid data, not renderer
failures. Corrected validators reuse existing terminal results without rerunning
the four completed processes. No warm-up exclusion or favorable rerun is applied.

All eight final processes and eight owned-file controls complete; this is a local
diagnostic result, not a newly run cross-platform CI or pixel regression. There is
no production code change. The latest source CI remains the
[R02 run](transfer-icon-ci.json). Two unused manual-link objects are removed
(149,611 bytes); all distinct programs, helper sources, ETLs, raw observations,
historical negatives and reference images remain.

**R01 remains open.** The current unelevated token has no SeSystemProfilePrivilege;
no kernel disk/filter trace was collected. Such a trace is the remaining direct
way to separate those lower-layer contributions; this receipt does not substitute
for it. Synchronous OS-cache durability calls should not be skipped or replaced
based on these measurements. NexaCloud sources/pin and driveE: are unchanged.
Physical/IME/assistive and whole-application acceptance remain separate work.
