# Initial Linux AT-SPI native evidence

Date: 2026-10-16. This report covers the initial private Linux transport, not
complete Linux accessibility or an Orca acceptance result. The public repository
remains [Lulus792/NexaUI](https://github.com/Lulus792/NexaUI).

## Executed environment

- Alpine 3.24 x86_64 under WSL2, kernel 6.18.33.2-microsoft-standard-WSL2.
- GCC 15.2 and Clang 22.1.3, C17; static Debug, shared Debug and Clang
  AddressSanitizer/UndefinedBehaviorSanitizer with leak detection.
- System D-Bus 1.16.2 and libatspi 2.60.5, with the actual `at-spi2-registryd`.
- X11 uses a private Xvfb display and authentication. Wayland uses WSLg 1.0.73.2,
  Weston revision 04d436c7d9a0cd55fa64b6612c7fa678d6fcd077; this is not GNOME/KDE
  or physical desktop coverage.

## Native checks

`nexaui_linux_atspi_client` is a separate C process linked to the system libatspi,
GObject and D-Bus APIs, with no NexaUI linkage. It discovers its host PID through
the real registry and uses the default libatspi cache. Direct D-Bus error checks
distinguish invalid/retired objects from convenience-wrapper false/empty results.
One allocation-rollback read deliberately clears that object's cache to force a
fresh native query.

The fixture verifies UTF-8/long names, protected empty values, native roles and
disabled state; window/parent coordinates, hit testing and actual X11 movement;
native focus acceptance; model-changing press/toggle; required/optional selection
with intermediate layout panels and actual direct-child indices; committed
name/state/selection/children events; 64 accepted queued presses plus rejection
of the 65th, with all accepted model effects eventually observed; two-window
identity/model isolation and peer teardown; allocation rollback/retry; node
removal/retyping/context replacement and retained references; and real connection
loss followed by registry rediscovery, new identities and a further successful
model action. Fractional child/parent positions must preserve the same integer
rectangle across coordinate-system translations.
An uncached RPC also has to complete within a one-second client timeout while
the host's native display poll waits two seconds, exercising the bus wakeup.

The initial reconnection probe failed because the consumed failure flag kept
resetting the retry deadline on every publication. Clearing it on disconnection
restores progress after the one-second backoff. The independent client now
observes a new connection/window identity and the existing model count advancing
from 65 to 66; the disconnected reference cannot retarget.

Five CTest cases cover a direct private accessibility bus, real `org.a11y.Bus`
session discovery, `NO_AT_BRIDGE=1`, an intentionally incomplete runtime ABI with
a live bus, and absent bus configuration. The latter three require ordinary
window/frame operation and complete caller-allocator cleanup. The private fixture
owns its process groups and temporary runtime/settings state.

Local evidence files are retained under `out/atspi-*`; they are development logs,
not packaged release artifacts. The latest static, shared and Clang sanitizer
runs each pass all five cases (13.26 s, 14.65 s and 15.89 s respectively), including
reconnection and fractional-coordinate checks. The new desktop CI matrix is
being verified separately; no pending run is counted as successful here.

An explicit `NEXAUI_ENABLE_ATSPI=OFF` configuration also builds the native host
and passes the optional-operation probe. `ldd` on shared NexaUI shows X11,
Wayland/XKB and their system dependencies, with no libdbus, libatspi or GLib link.
See the [transport contract](../concepts/atspi.md) for ownership and wire limits.

## Broader regression observations

Exploratory local full suites each ran 106 Linux cases. The shared run passed
105 and failed the NexaCloud X11 native smoke; its isolated follow-up passes.
The sanitizer run passed 105 and failed a Wayland cursor/scale assertion, then
reported 128 bytes from the assertion's early exit; its isolated follow-up passes.
These exploratory suites overlapped and used the same WSLg compositor. They do
not establish an isolated all-green full-suite result or a resolved root cause.
The new AT-SPI cases passed in both of those runs without sanitizer findings.

The local Windows static build succeeds. Its 98-case exploratory suite has UIA
fixture and compact NexaCloud UIA failures; the compact follow-up passes, while
subsequent UIA fixture runs encounter selection/structure event timeouts at
different stages. Production Windows adapter code is unchanged by this work.
Additional failure diagnostics preserve the member/result/click/focus values.
These results remain visible; they are not reclassified as successful checks.

## Remaining acceptance

Full Text/EditableText/Value/Table/Collection/relations and role/event contracts,
Orca tasks, compositor/desktop/distribution/architecture diversity, physical
scaling/input and release/hardware acceptance remain open (L11/X07/X08 and wider
MASTER gates). Wayland screen coordinates intentionally return NotSupported;
screen-space BoundsChanged is not fabricated. A new connection is tested, not
every possible accessibility-daemon restart sequence or hostile IPC peer.

Primary protocol evidence: GNOME's [XML interface documentation](https://gnome.pages.gitlab.gnome.org/at-spi2-core/devel-docs/xml-interfaces.html),
[public interface declarations](https://github.com/GNOME/at-spi2-core/tree/main/xml)
and [native event parser](https://github.com/GNOME/at-spi2-core/blob/main/atspi/atspi-event-listener.c).
The latter accepts both legacy and dictionary event tails; this transport emits
`siiva{sv}` and verifies it through the installed native library.
