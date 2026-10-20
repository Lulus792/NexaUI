# WSLg host-window observation

2026-10-20, source `8cdb487`. This extends the
[missing frame-callback diagnosis](wslg-frame-callback-report.md) with actual
Windows observations. The result is a narrower diagnostic boundary, **not** a
WSLg liveness, physical input/display or full Design-A acceptance.

## Native observer and independent clients

Windows11 Pro build26200, WSLg1.0.73.2 and the WSL-supplied
`msrdc.exe`1.2.7214.0. The unchanged actual SHM client retains binary SHA-256
`97a43e003fd93192649689e2f493fac6851c183fedc7f2080f5f54675cc635ac`.
ASan/UBSan/LSan remain enabled. No global compositor, driver, display, cache,
security or event-log configuration changes are made.

The Windows observer uses `EnumWindows`, process identity, native placement,
client/frame bounds, DPI, DWM cloak and nine client-point hit checks. Only
matching control captions are retained; nonmatching captions in the selected
RDP process are redacted. There are no screenshots, input injection, clipboard
changes or queries for other applications' contents. A per-thread DPI override
is restored afterward. Each launcher waits for its owned bounded client.

The strengthened observer verifies `WinSta0/Default` is the current input
desktop. Its own temporary nonactivating Win32 reference window is found with
real bounds,96DPI, visible/not minimized, DWM cloak0 and successful nine-point
ownership checks, then destroyed. This is an observer positive control, not a
physical acceptance of the WSLg client.

| New observation | Actual result |
| --- | --- |
| SHM, caption-prefix search | No target in98 samples,78 within the client's interval; callback absent after5.027312s; clean child exit2 |
| SHM, caption-substring search plus native reference/input-desktop guard | No target in52 samples,48 during the client; callback absent after5.018880s; clean child exit2 |
| SHM, RDP-process handle comparison | Same21 HWNDs before, throughout54 samples and afterward; no new handle; callback absent after5.032496s; clean child exit2 |
| Independent Xlib-only control, same process observation | One MapNotify, one Expose, `IsViewable`,128×96, clean exit0 after5.000916s; same21 RDP HWNDs throughout50 samples and afterward |

Each row answers a new observer/backend question; no workload is retried until
green. The Xlib client creates/maps/clears/destroys only its own window and links
neither NexaUI nor Vulkan. Its successful logical X11 map/expose result does
**not** establish a displayed Windows host window. No sanitizer diagnostic is
observed in these completed clients. The observer launcher's exit0 is separate
from the SHM child's exit2 and cannot count as a callback success.

The process-filtered snapshots initially contain a one-element nested PowerShell
array. The retained analysis flattens it and counts actual HWND records:21,
not the console container count1. This is a read-only artifact normalization;
the workload and original JSON are not rerun or overwritten.

## Why a missing caption is insufficient

The compositor version file identifies Microsoft Weston's source commit
`04d436c7d9a0cd55fa64b6612c7fa678d6fcd077`. The inspected
[RDP window-creation code](https://github.com/microsoft/weston/blob/04d436c7d9a0cd55fa64b6612c7fa678d6fcd077/libweston/backend-rdp/rdprail.c#L1674)
creates initially hidden, untitled windows; later updates assign titles and
show state. Therefore caption absence alone cannot prove that no host object
exists. The process-filtered handle comparison also covers untitled top-level
windows and finds no new object in the selected WSL RDP client.

That still does not establish the live server's WindowCreate/update/activation
branch. Sampling can miss short-lived objects; child windows, another host
process or other internal representations are outside this recorded comparison.
No WSLg candidate is positively identified, so its minimize/cloak/occlusion or
physical client pixels cannot be reported as measured. Values from the native
reference or RDP helper windows are not substituted for that missing candidate.

[IsWindowVisible](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-iswindowvisible)
describes a style-based state and can remain true under occlusion;
[EnumWindows](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-enumwindows)
covers desktop top-level windows. Neither API alone is a physical-display oracle.

## Output-origin hypothesis checked, not attributed

The current Windows monitor query reports one1920×1080 monitor at `(0,0)`.
A surface-free independent Wayland client queries actual `wl_output` and
`zxdg_output_v1` events: one1920×1080 output at `(1920,290)`, scale1, logical
position/size consistent with those values. Both queries complete normally
under sanitizers. The earlier two-monitor same-DPI evidence is preserved as
historical coverage; it is not the current session's monitor count.

The numeric origin difference is **not a wrong-layout proof**. The pinned
[RDP coordinate conversion](https://github.com/microsoft/weston/blob/04d436c7d9a0cd55fa64b6612c7fa678d6fcd077/libweston/backend-rdp/rdpdisp.c#L367)
subtracts the Weston output origin, applies scale and adds the client monitor
origin. Actual live placement/conversion of the blocked surface is not traced.
No display origin, scaling or monitor arrangement is changed to force a pass.

## Remaining observability boundary

The inspected version's `weston_rdprail_shell` protocol exposes a focus proxy,
not a surface-to-Windows-HWND query. The existing Windows
`Microsoft-Windows-TerminalServices-ClientActiveXCore` provider metadata describes
generic connection/graphics events, without a usable per-surface RAIL WindowCreate
correlation in the inspected manifest. Its already enabled operational log is
queried for the known RDP process and graphics/state event IDs over the control
interval: `NoMatchingEventsFound`. That absence neither proves RDP health nor
rules out other providers or additional instrumentation. No channel is enabled
or provider trace started.

The next useful WSLg evidence must correlate a specific surface with a server/
RAIL creation/update/activation or matching client event. Identical frame waits,
caption searches or tests repeated for a green exit would not close that gap.
The current observations show an issue beyond the NexaUI/Vulkan drawing path;
they do not identify a corrective application patch or prove a compositor defect.

The [machine receipt](wslg-host-visibility-local.json) records the exact native
reference, query results, client/source/binary hashes, verified upstream Git blobs
and all raw paths under `out/wslg-host-visibility-20261020`. Sources/executables
and original negative results remain frozen there. R01's separate profiling
permission gap, mixed-DPI, physical keyboard/IME, human assistive acceptance and
the public D01 window-management contract remain open. SDK production sources
and NexaCloud's canonical pin are unchanged by this diagnostic work.
