# R01: private shader-cache event timing

2026-10-18. [Four complete trace receipts and helper sources](d3d12-private-cache-trace-local.json).
These measurements precede the R02 icon change: core base2460116,
startup diagnostic0bbe833 and renderer998a12c. All four first frames contain
1,326 commands. They extend the [cache-file investigation](d3d12-device-cache-report.md).

**Two cache-creation intervals account for2,213.107ms inside the first
2,705.315ms device call in this experiment.** This is a new, directly correlated
case; the old381/838ms and4GiB application findings remain open. No production
startup improvement or R01 acceptance is claimed.

| Fixed order | Parent launch→READY, ms | Device call, ms | Cache creations inside that call, ms |
| --- | ---: | ---: | --- |
| Hardware, first | 3076.621 | 2705.315 | 1114.023 +1099.083 |
| WARP | 572.089 | 15.295 | 5.125 |
| Hardware again | 752.709 | 343.863 | 4.081 +4.021 |
| WARP again | 697.505 | 19.061 | 7.120 |

The hardware events identify cache GUIDs6f75932f-7dfc-4fb0-b4b8-12de1ac415da
and f4eb2d6c-ed2b-4bdd-ad9d-f913287e6768. WARP emits the second GUID.
All creation start/stop pairs are on the device-calling thread, return success,
and lie inside its exact QPC span. This establishes temporal containment for
these calls, not the internal storage/filter cause or a general first-run cost.
The fixed adapter order can reuse shared cache identity; it does not establish
an independent cold-cache condition for WARP. The first slow case is retained.

## Collection boundary

The local helper wraps the unchanged startup diagnostic with a private
in-process ETW session. It pre-enables only the installed
`Microsoft-Windows-Direct3DShaderCache` provider
(2d4ebca6-ea64-453f-a292-ae2ea0ee513b), records QPC timestamps and stops it after
the owned application has completed. The parent still measures process launch
through actual Present/DWM composition and acknowledged owned-HWND input,
including trace setup and all device preparation. No thread is suspended.

Start, enable, disable and stop all return0 in every case. Event-loss and lost-log-
buffer counts are0. Each trace uses two64KiB buffers on disk; the configured
memory maximum is eight buffers and file limit16MiB. Every decoded cache event
belongs to the launched process; header metadata records are excluded separately.
Raw ETLs, decoded property bytes, provider metadata and successful stop receipts
are retained. The decoder uses `PROCESS_TRACE_MODE_RAW_TIMESTAMP`; provider
timestamps and renderer QPC spans therefore share the same clock.

Microsoft documents [private in-process sessions and low-privilege support](https://learn.microsoft.com/en-us/windows/win32/etw/configuring-and-starting-a-private-logger-session).
This helper successfully executes with the current unelevated token. It creates
no global or kernel session. Provider event names/fields are the installed
Windows10.0.26200/D3DSCache10.0.26100.8117 schema; they are not a stable public
framework API. The exact helper, decoder, build scripts and four-case runner
are included in the JSON as source text with hashes.
Rebuild against the recorded pre-R02 core base when reproducing this workload.
R02 adds three line commands per Transfer icon; its newer sample is a different
command stream. Future renderer comparisons must use the same core/scene on
both sides and retain the entire process-start boundary.

The provider also emits `Clear` during normal runtime initialization. The helper
requests no shader-cache control, purge, disable, relocation or developer-mode
change; the trace preserves those runtime events without assigning their cause.
No settings, NexaCloud sources/pin or driveE: were changed.

**R01 remains open.** The private user-mode trace narrows cache initialization
without collecting kernel disk/filter events. It does not establish why those
file operations take this long, explain every earlier PSO outlier, or make the
full first usable frame reliably faster. Physical/IME/assistive and broader
project acceptance remain separate. The independent R02 geometry correction
is recorded in the [transfer-icon report](transfer-icon-report.md).
