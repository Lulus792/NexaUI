# R01: system-I/O trace capability

2026-10-20, source base09484af. One new native capability preflight returns
**StartTraceW=5 (`ERROR_ACCESS_DENIED`), process exit77 (unavailable)**.
It launches no rendering workload and produces no ETL. This is not a new
startup-latency measurement or a performance improvement.

The [receipt](d3d12-system-io-capability-local.json) embeds the complete helper
and build script, exact command, build output, return values and file hashes.
MSVC19.38 builds it in C17/Release with `/W4`; the local run is unelevated.
Token queries succeed and show `SeSystemProfilePrivilege` absent, rather than
merely disabled. Session creation itself is denied; that status does not
identify the precise missing session/provider ACL.

## New candidate path and actual boundary

The candidate uses the SDK20348+ System I/O and System I/O Filter providers to
extend the existing same-QPC shader-cache/flush observations into kernel I/O
and filter events. Microsoft specifies that these providers require
`EVENT_TRACE_SYSTEM_LOGGER_MODE`.
An in-process private user-mode session, which successfully captured D3DSCache,
does not replace this lower-layer session: its enabled providers must be in
the same process. These are different trace scopes. See
[System providers](https://learn.microsoft.com/en-us/windows/win32/etw/system-providers),
[system-session configuration](https://learn.microsoft.com/en-us/windows/win32/etw/configuring-and-starting-a-systemtraceprovider-session)
and [private sessions](https://learn.microsoft.com/en-us/windows/win32/etw/configuring-and-starting-a-private-logger-session).

The capability helper uses a fresh GUID and owned name/output path, QPC clock,
system-logger mode and a small bounded file limit. It enables **no provider
or profiling flags**. If creation succeeds, it immediately stops only its own
valid session. On the observed access failure it does not stop any logger.
No token privilege, account group, ACL, cache or OS/security setting is changed.
No existing logger is touched. The zero-initialized loss field is not evidence
of a successful zero-loss trace, because no session was started.

This preflight gives a concrete capability failure for the new path without
repeating the frozen hardware/WARP startup campaign or generating warmed-cache
timings that cannot answer the lower-layer question. It does not prove a
storage-device, filesystem or filter driver as the latency cause.

## Measurement contract when the path is available

Retain the existing parent launch→Device/cache/flush→first Present→DWM wait→
owned-window input acknowledgement→READY boundary. Correlate successful system
I/O/filter events with the same QPC clock, owned PID/TID and exact cache-file
identity; preserve event-loss and API failures. The original first slow process
and the post-READY flush distinction remain part of the evidence.

No such kernel trace or new end-to-end measurement was collected here.
The earlier [28 flush calls costing1,867.716ms inside2,491.052ms Device](d3d12-flush-timing-report.md)
remain the bounded observation. Lower-layer attribution and acceptance of a
reliably fast visible, operable window remain open. Embedded shaders, R02/R03,
NexaCloud sources/pin and shared system settings are unchanged.
