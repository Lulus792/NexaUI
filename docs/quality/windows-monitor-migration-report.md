# Owned NexaUI window migration across available Windows monitors

Framework source: `d06fc382732e10d586db71f417e3a9f79867b748`, 2026-10-20.
An automated local probe moved its own NexaUI/NexaCloud example window across
the monitors actually returned by `EnumDisplayMonitors`. It used the normal
Win32 window procedure, D3D12 renderer and shared example scene, with a test
observer forwarding window messages to the original procedure. No global
display, DPI, cache or security setting was changed.

The actual run enumerated two monitors in a local session, at work rectangles
(-1920,0)–(0,1200) and (0,0)–(1920,1080). Both reported96 DPI. An earlier
`GetSystemMetrics` availability query had reported three active monitors;
that preliminary count was not used as the executed coverage count.

The fixed route0→1→1→0 passed all four stations. At each station the probe
checked `MonitorFromWindow`, `GetDpiForWindow`, logical window information and
actual client pixel size, then rendered the current scene. Each800×600 frame
contained1,420 commands. All1,920,000 RGBA bytes matched the independently
scaled headless rendering at every station; there were zero differing bytes.
The renderer used hardware (`using_warp=0`). GPU captures and source/build/
binary/library/log hashes are retained in the [receipt](windows-monitor-migration-local.json).

This proves the automated same-DPI monitor route and bytewise scene rendering
in that environment. It does not prove a mixed-DPI transition: no
`WM_DPICHANGED` event occurred. It also does not provide human visual,
physical keyboard/pointer, IME, contrast, screenreader or complete NexaCloud
application acceptance. The separate NexaCloud source and dependency pin
were not changed. R01 startup and its lower-layer trace gap remain open.
