# Wayland repaint suspension and Settings resume

SDK and Settings source `1d8ba46b53f5030ce420da3eb28498fe330bbeb5`.
Wayland negotiates xdg-shell up to v6 when the build's protocol headers
support it. The compositor's suspended value is applied at
`xdg_surface.configure` and exposed in the existing status and opt-in state
event. V5 clients still report the previous observable state mask. Windows,
X11 and AppKit continue to report their existing state masks.

Settings retains pending work while the surface is suspended. The loop can
still dispatch native close and consume completed application work. A
same-size configure that clears suspension wakes the renderer owner through
the existing event contract. Logical and backing extents remain valid.

The controlled Wayland server sends a toplevel configure followed separately
by its surface configure. The SDK test reads status before and after that
boundary, receives the state event through the public context queue, checks
logical and pixel extents and renders after the resume wake. The v5
management tests continue to cover the older protocol.

A separate fixture runs the actual Settings executable on a v6 connection.
The server observes the first ARGB buffer, requests suspension without a
resize and waits 400 ms from the client's acknowledgement before requesting
resume. It observes one buffer before suspension, zero during suspension and
one after resume, then requests close and checks the child's exit. Disabling
the Settings suspension guard in a separately compiled consumer produces a
third buffer during the pause and fails the unchanged fixture.

Thirty selected CTests pass in Wayland-only static Release, shared Release
and ASan/UBSan/LSan. They include software rendering, native management,
the resume fixture, visible and hidden idle, and native keyboard operation.
A separate Release Settings build against the installed SDK also passes
the resume fixture.

[Evidence manifest](wayland-suspended-settings-evidence.json). The local
archive `nexaui-settings-resume343.tar.gz` contains the frozen source,
protocol XML, commands, logs and JUnit results. It has 16,601,870 bytes and
SHA-256 `ed25bdd66efbfd036d141e5ca202d9818956e72c308f19001df49315c77960b7`.
The [GitHub CI](https://github.com/Lulus792/NexaUI/actions/runs/43969685351)
tracks the exact implementation revision.

This advances W03 and P01. The flag observes repaint suspension while
Wayland minimized state remains unknown. Physical compositor minimization,
production Vulkan idle, scheduler wake counts and Aetheri integration remain
open.
